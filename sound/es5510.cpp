// license:BSD-3-Clause
// copyright-holders:Christian Brunschen
/***************************************************************************************
 *
 *   es5510.cpp - Ensoniq ES5510 (ESP) emulation for M1
 *   based on MAME's es5510.cpp by Christian Brunschen
 *
 ***************************************************************************************/

#include "es5510.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

#define BIT(val, bit) (((val) >> (bit)) & 1)

constexpr uint8_t FLAG_N = 1 << 7;
constexpr uint8_t FLAG_C = 1 << 6;
constexpr uint8_t FLAG_V = 1 << 5;
constexpr uint8_t FLAG_LT = 1 << 4;
constexpr uint8_t FLAG_Z = 1 << 3;
constexpr uint8_t FLAG_NOT = 1 << 2;

constexpr uint8_t FLAG_MASK = FLAG_N | FLAG_C | FLAG_V | FLAG_LT | FLAG_Z;

#define OP_CMP (4)

constexpr uint8_t setFlag(uint8_t ccr, uint8_t flag) {
	return ccr | flag;
}

constexpr uint8_t clearFlag(uint8_t ccr, uint8_t flag) {
	return ccr & ~flag;
}

constexpr uint8_t setFlagTo(uint8_t ccr, uint8_t flag, bool set) {
	return set ? setFlag(ccr, flag) : clearFlag(ccr, flag);
}

inline bool isFlagSet(uint8_t ccr, uint8_t flag) {
	return (ccr & flag) != 0;
}

template <typename T>
constexpr inline int32_t sext(T val, int bits) {
	int32_t sval = (int32_t)val;
	return (sval << (32 - bits)) >> (32 - bits);
}

constexpr inline int64_t sext64(uint64_t val, int bits) {
	int64_t sval = (int64_t)val;
	return (sval << (64 - bits)) >> (64 - bits);
}

constexpr inline int64_t mul_32x32(int32_t a, int32_t b) {
	return (int64_t)a * (int64_t)b;
}

inline int32_t add(int32_t a, int32_t b, uint8_t &flags) {
	int32_t const aSign = a & 0x00800000;
	int32_t const bSign = b & 0x00800000;
	int32_t const result = a + b;
	int32_t const resultSign = result & 0x00800000;
	bool const overflow = (aSign == bSign) && (aSign != resultSign);
	bool const carry = (result & 0x01000000) != 0;
	bool const negative = resultSign != 0;
	bool const lessThan = (overflow && !negative) || (!overflow && negative);
	flags = setFlagTo(flags, FLAG_C, carry);
	flags = setFlagTo(flags, FLAG_N, negative);
	flags = setFlagTo(flags, FLAG_Z, (result & 0x00ffffff) == 0);
	flags = setFlagTo(flags, FLAG_V, overflow);
	flags = setFlagTo(flags, FLAG_LT, lessThan);
	return result & 0x00ffffff;
}

inline int16_t round_24_to_16(int32_t dol24) {
	return int16_t(dol24 >> 8);
}

inline int32_t saturate(int32_t value, uint8_t &flags, bool negative) {
	if (isFlagSet(flags, FLAG_V)) {
		flags = setFlagTo(flags, FLAG_N, negative);
		flags = clearFlag(flags, FLAG_Z);
		return negative ? 0x00800000 : 0x007fffff;
	} else {
		return value;
	}
}

constexpr int32_t negate(int32_t value) {
	return ((value ^ 0x00ffffff) + 1) & 0x00ffffff;
}

inline int32_t asl(int32_t value, int shift, uint8_t &flags) {
	const int32_t src24 = value & 0x00ffffff;
	const bool carry = BIT(src24, 24 - shift);
	const int64_t shifted = int64_t(sext(src24, 24)) << shift;
	const bool overflow = (shifted > 0x007fffffLL) || (shifted < -0x00800000LL);
	const int32_t result = overflow
		? ((shifted < 0) ? 0x00800000 : 0x007fffff)
		: (int32_t(shifted) & 0x00ffffff);

	flags = setFlagTo(flags, FLAG_C, carry);
	flags = setFlagTo(flags, FLAG_V, overflow);
	flags = setFlagTo(flags, FLAG_N, (result & 0x00800000) != 0);
	flags = setFlagTo(flags, FLAG_Z, (result & 0x00ffffff) == 0);
	return result;
}

int8_t countLowOnes(int32_t x) {
	int8_t n = 0;
	while ((x & 1) == 1) {
		++n;
		x >>= 1;
	}
	return n;
}

const ES5510::alu_op_t ES5510::ALU_OPS[16] = {
	{ 2, "ADD" },
	{ 2, "SUB" },
	{ 2, "ADDU" },
	{ 2, "SUBU" },
	{ 2, "CMP" },
	{ 2, "AND" },
	{ 2, "OR" },
	{ 2, "XOR" },
	{ 1, "ABS" },
	{ 1, "MOV" },
	{ 1, "ASL2" },
	{ 1, "ASL8" },
	{ 1, "LS15" },
	{ 1, "DIFF" },
	{ 1, "ASR" },
	{ 0, "END" },
};

const ES5510::op_select_t ES5510::OPERAND_SELECT[16] = {
	{ ES5510::SRC_DST_REG, ES5510::SRC_DST_REG, ES5510::SRC_DST_REG, ES5510::SRC_DST_REG },
	{ ES5510::SRC_DST_REG, ES5510::SRC_DST_REG, ES5510::SRC_DST_REG, ES5510::SRC_DST_DELAY },
	{ ES5510::SRC_DST_REG, ES5510::SRC_DST_REG, ES5510::SRC_DST_REG, ES5510::SRC_DST_BOTH },
	{ ES5510::SRC_DST_REG, ES5510::SRC_DST_REG, ES5510::SRC_DST_DELAY, ES5510::SRC_DST_REG },
	{ ES5510::SRC_DST_REG, ES5510::SRC_DST_REG, ES5510::SRC_DST_DELAY, ES5510::SRC_DST_BOTH },
	{ ES5510::SRC_DST_REG, ES5510::SRC_DST_DELAY, ES5510::SRC_DST_REG, ES5510::SRC_DST_REG },
	{ ES5510::SRC_DST_REG, ES5510::SRC_DST_DELAY, ES5510::SRC_DST_DELAY, ES5510::SRC_DST_REG },
	{ ES5510::SRC_DST_REG, ES5510::SRC_DST_BOTH, ES5510::SRC_DST_REG, ES5510::SRC_DST_REG },
	{ ES5510::SRC_DST_REG, ES5510::SRC_DST_BOTH, ES5510::SRC_DST_DELAY, ES5510::SRC_DST_REG },
	{ ES5510::SRC_DST_DELAY, ES5510::SRC_DST_REG, ES5510::SRC_DST_REG, ES5510::SRC_DST_REG },
	{ ES5510::SRC_DST_DELAY, ES5510::SRC_DST_REG, ES5510::SRC_DST_REG, ES5510::SRC_DST_DELAY },
	{ ES5510::SRC_DST_DELAY, ES5510::SRC_DST_REG, ES5510::SRC_DST_REG, ES5510::SRC_DST_BOTH },
	{ ES5510::SRC_DST_DELAY, ES5510::SRC_DST_REG, ES5510::SRC_DST_DELAY, ES5510::SRC_DST_REG },
	{ ES5510::SRC_DST_DELAY, ES5510::SRC_DST_REG, ES5510::SRC_DST_DELAY, ES5510::SRC_DST_BOTH },
	{ ES5510::SRC_DST_DELAY, ES5510::SRC_DST_BOTH, ES5510::SRC_DST_REG, ES5510::SRC_DST_REG },
	{ ES5510::SRC_DST_DELAY, ES5510::SRC_DST_BOTH, ES5510::SRC_DST_DELAY, ES5510::SRC_DST_REG },
};

const ES5510::ram_control_t ES5510::RAM_CONTROL[8] = {
	{ ES5510::RAM_CYCLE_READ,      ES5510::RAM_CONTROL_DELAY,   "Read Delay" },
	{ ES5510::RAM_CYCLE_WRITE,     ES5510::RAM_CONTROL_DELAY,   "Write Delay" },
	{ ES5510::RAM_CYCLE_READ,      ES5510::RAM_CONTROL_TABLE_A, "Read Table A" },
	{ ES5510::RAM_CYCLE_WRITE,     ES5510::RAM_CONTROL_TABLE_A, "Write Table A" },
	{ ES5510::RAM_CYCLE_READ,      ES5510::RAM_CONTROL_TABLE_B, "Read Table B" },
	{ ES5510::RAM_CYCLE_DUMP_FIFO, ES5510::RAM_CONTROL_DELAY,   "Read Delay and Dump FIFO" },
	{ ES5510::RAM_CYCLE_READ,      ES5510::RAM_CONTROL_IO,      "Read from I/O" },
	{ ES5510::RAM_CYCLE_WRITE,     ES5510::RAM_CONTROL_IO,      "Write to I/O" },
};

ES5510::ES5510()
	: icount(0)
	, halt_asserted(false)
	, pc(0)
	, state(STATE_HALTED)
	, gpr(nullptr)
	, ser0r(0), ser0l(0)
	, ser1r(0), ser1l(0)
	, ser2r(0), ser2l(0)
	, ser3r(0), ser3l(0)
	, machl(0)
	, mac_overflow(false)
	, dil(0)
	, memsiz(0x00ffffff)
	, memmask(0x00000000)
	, memincrement(0x01000000)
	, memshift(24)
	, dlength(0)
	, abase(0)
	, bbase(0)
	, dbase(0)
	, sigreg(0)
	, mulshift(2)
	, ccr(0)
	, cmr(0)
	, dol_count(0)
	, instr(nullptr)
	, dram(nullptr)
	, dol_latch(0)
	, dil_latch(0)
	, dadr_latch(0)
	, gpr_latch(0)
	, instr_latch(0)
	, ram_sel(0)
	, host_control(0)
	, host_serial(0)
{
	dol[0] = dol[1] = 0;
	memset(&alu, 0, sizeof(alu));
	memset(&mulacc, 0, sizeof(mulacc));
	memset(&ram, 0, sizeof(ram));
	memset(&ram_p, 0, sizeof(ram_p));
	memset(&ram_pp, 0, sizeof(ram_pp));

	gpr = std::make_unique<int32_t[]>(0xc0);
	instr = std::make_unique<uint64_t[]>(160);
	dram = std::make_unique<int16_t[]>(DRAM_SIZE);

	reset();
}

void ES5510::reset() {
	pc = 0x00;
	if (gpr) std::fill(&gpr[0], &gpr[0xc0], 0);
	if (instr) std::fill(&instr[0], &instr[160], 0);
	if (dram) std::fill(&dram[0], &dram[DRAM_SIZE], 0);
	state = STATE_HALTED;
	dil_latch = dol_latch = dadr_latch = gpr_latch = 0;
	instr_latch = 0;
	ram_sel = 0;
	host_control = 0x04; // Signal Host Access not OK
	host_serial = 0;
	sigreg = 0;
	mulshift = 2;
	ser0r = ser0l = 0;
	ser1r = ser1l = 0;
	ser2r = ser2l = 0;
	ser3r = ser3l = 0;
	machl = 0;
	mac_overflow = false;
	dil = 0;
	memsiz = 0x00ffffff;
	memmask = 0;
	memincrement = 0x01000000;
	memshift = 24;
	dlength = abase = bbase = dbase = 0;
	ccr = cmr = 0;
	dol[0] = dol[1] = 0;
	dol_count = 0;
	memset(&ram, 0, sizeof(ram_t));
	memset(&ram_p, 0, sizeof(ram_p));
	memset(&ram_pp, 0, sizeof(ram_pp));
	memset(&alu, 0, sizeof(alu));
	memset(&mulacc, 0, sizeof(mulacc));
}

uint8_t ES5510::host_r(uint32_t offset) {
	switch (offset) {
	case 0x00: return (gpr_latch >> 16) & 0xff;
	case 0x01: return (gpr_latch >>  8) & 0xff;
	case 0x02: return (gpr_latch >>  0) & 0xff;

	case 0x03: return uint8_t((instr_latch >> 40) & 0xff);
	case 0x04: return uint8_t((instr_latch >> 32) & 0xff);
	case 0x05: return uint8_t((instr_latch >> 24) & 0xff);
	case 0x06: return uint8_t((instr_latch >> 16) & 0xff);
	case 0x07: return uint8_t((instr_latch >>  8) & 0xff);
	case 0x08: return uint8_t((instr_latch >>  0) & 0xff);

	case 0x09: return (dil_latch >> 16) & 0xff;
	case 0x0a: return (dil_latch >>  8) & 0xff;
	case 0x0b: return 0;

	case 0x0c: return (dol_latch >> 16) & 0xff;
	case 0x0d: return (dol_latch >>  8) & 0xff;
	case 0x0e: return 0xff;

	case 0x0f: return (dadr_latch >> 16) & 0xff;
	case 0x10: return (dadr_latch >>  8) & 0xff;
	case 0x11: return (dadr_latch >>  0) & 0xff;

	case 0x12: return 0; // Host Control
	case 0x16: return 0x27; // PC test
	default:   return 0x00;
	}
}

void ES5510::host_w(uint32_t offset, uint8_t data) {
	switch (offset) {
	case 0x00:
		gpr_latch = (gpr_latch & 0x00ffff) | ((data & 0xff) << 16);
		break;
	case 0x01:
		gpr_latch = (gpr_latch & 0xff00ff) | ((data & 0xff) << 8);
		break;
	case 0x02:
		gpr_latch = (gpr_latch & 0xffff00) | ((data & 0xff) << 0);
		break;

	case 0x03: instr_latch = ((instr_latch & 0x00ffffffffffULL) | (uint64_t(data) & 0xff) << 40); break;
	case 0x04: instr_latch = ((instr_latch & 0xff00ffffffffULL) | (uint64_t(data) & 0xff) << 32); break;
	case 0x05: instr_latch = ((instr_latch & 0xffff00ffffffULL) | (uint64_t(data) & 0xff) << 24); break;
	case 0x06: instr_latch = ((instr_latch & 0xffffff00ffffULL) | (uint64_t(data) & 0xff) << 16); break;
	case 0x07: instr_latch = ((instr_latch & 0xffffffff00ffULL) | (uint64_t(data) & 0xff) << 8); break;
	case 0x08: instr_latch = ((instr_latch & 0xffffffffff00ULL) | (uint64_t(data) & 0xff) << 0); break;

	case 0x0c:
		dol_latch = (dol_latch & 0x00ffff) | ((data & 0xff) << 16);
		break;
	case 0x0d:
		dol_latch = (dol_latch & 0xff00ff) | ((data & 0xff) << 8);
		break;
	case 0x0e:
		dol_latch = (dol_latch & 0xffff00) | ((data & 0xff) << 0);
		break;

	case 0x0f:
		dadr_latch = (dadr_latch & 0x00ffff) | ((data & 0xff) << 16);
		if (ram_sel) {
			dil_latch = dram_r(dadr_latch) << 8;
		} else {
			dram_w(dadr_latch, dol_latch >> 8);
		}
		break;
	case 0x10: dadr_latch = (dadr_latch & 0xff00ff) | ((data & 0xff) << 8); break;
	case 0x11: dadr_latch = (dadr_latch & 0xffff00) | ((data & 0xff) << 0); break;

	case 0x12:
		host_control = (host_control & 0x4) | (data & 0x3);
		if (BIT(host_control, 1)) {
			if (state == STATE_HALTED) {
				for (uint32_t i = 0; i < DRAM_SIZE; i++)
					dram[i] = 0;
			}
			host_control &= ~0x2;
		}
		break;

	case 0x14: ram_sel = data & 0x80; break;
	case 0x18: host_serial = data; break;

	case 0x1f:
		if (halt_asserted) {
			state = STATE_HALTED;
		}
		break;

	case 0x80: /* Read select - GPR + INSTR */
		if (data < 0xa0) {
			instr_latch = instr[data];
		}
		if (data < 0xc0) {
			gpr_latch = gpr[data] & 0xffffff;
		} else if (data >= 0xea) {
			gpr_latch = read_reg(data);
		}
		break;

	case 0xa0: /* Write select - GPR */
		write_reg(data, gpr_latch);
		break;

	case 0xc0: /* Write select - INSTR */
		if (data < 0xa0) {
			instr[data] = instr_latch & 0xffffffffffffULL;
		}
		break;

	case 0xe0: /* Write select - GPR + INSTR */
		if (data < 0xa0) {
			instr[data] = instr_latch;
		}
		write_reg(data, gpr_latch);
		break;
	}
}

int16_t ES5510::ser_r(int offset) const {
	switch (offset) {
	case 0: return ser0l;
	case 1: return ser0r;
	case 2: return ser1l;
	case 3: return ser1r;
	case 4: return ser2l;
	case 5: return ser2r;
	case 6: return ser3l;
	case 7: return ser3r;
	default: return 0;
	}
}

void ES5510::ser_w(int offset, int16_t data) {
	switch (offset) {
	case 0: ser0l = data; break;
	case 1: ser0r = data; break;
	case 2: ser1l = data; break;
	case 3: ser1r = data; break;
	case 4: ser2l = data; break;
	case 5: ser2r = data; break;
	case 6: ser3l = data; break;
	case 7: ser3r = data; break;
	}
}

int32_t ES5510::read_reg(uint8_t reg) {
	if (reg < 0xc0) {
		return gpr[reg];
	} else {
		switch (reg) {
		case 234: return ser0r << 8;
		case 235: return ser0l << 8;
		case 236: return ser1r << 8;
		case 237: return ser1l << 8;
		case 238: return ser2r << 8;
		case 239: return ser2l << 8;
		case 240: return ser3r << 8;
		case 241: return ser3l << 8;
		case 242: return mac_overflow ? (machl < 0 ? 0x00000000 : 0x00ffffff) : (machl >>  0) & 0x00ffffff;
		case 243: return mac_overflow ? (machl < 0 ? 0x00800000 : 0x007fffff) : (machl >> 24) & 0x00ffffff;
		case 244: return dil << 8;
		case 245: return dlength;
		case 246: return abase;
		case 247: return bbase;
		case 248: return dbase;
		case 249: return sigreg;
		case 250: return ccr << 16;
		case 251: return cmr << 16;
		case 252: return 0x00ffffff;
		case 253: return 0x00800000;
		case 254: return 0x007fffff;
		case 255: return 0;
		default:  return 0;
		}
	}
}

void ES5510::write_reg(uint8_t reg, int32_t value) {
	value &= 0x00ffffff;
	if (reg < 0xc0) {
		gpr[reg] = value;
	} else {
		switch (reg) {
		case 234: ser0r = ((value >> 8) & 0xffff); break;
		case 235: ser0l = ((value >> 8) & 0xffff); break;
		case 236: ser1r = ((value >> 8) & 0xffff); break;
		case 237: ser1l = ((value >> 8) & 0xffff); break;
		case 238: ser2r = ((value >> 8) & 0xffff); break;
		case 239: ser2l = ((value >> 8) & 0xffff); break;
		case 240: ser3r = ((value >> 8) & 0xffff); break;
		case 241: ser3l = ((value >> 8) & 0xffff); break;
		case 242: {
			int64_t masked = machl & (int64_t(0x00ffffffU) << 24);
			int64_t shifted = (int64_t)(value & 0x00ffffff) << 0;
			machl = sext64(masked | shifted, 48);
			break;
		}
		case 243: {
			int64_t masked = machl & (int64_t(0x00ffffffU) << 0);
			int64_t shifted = (int64_t)(value & 0x00ffffff) << 24;
			machl = sext64(masked | shifted, 48);
			mac_overflow = false;
			break;
		}
		case 244:
			memshift = countLowOnes(value);
			memsiz = 0x00ffffff >> (24 - memshift);
			memmask = 0x00ffffff & ~memsiz;
			memincrement = 1 << memshift;
			dbase &= memmask;
			break;
		case 245: dlength = value; break;
		case 246: abase = value; break;
		case 247: bbase = value; break;
		case 248: dbase = value; break;
		case 249:
			sigreg = value;
			mulshift = BIT(sigreg, 22) ? 1 : 2;
			break;
		case 250: ccr = (value >> 16) & FLAG_MASK; break;
		case 251: cmr = (value >> 16) & (FLAG_MASK | FLAG_NOT); break;
		default: break;
		}
	}
}

void ES5510::write_to_dol(int32_t value) {
	uint16_t dol16 = round_24_to_16(value);
	if (dol_count >= 2) {
		dol[0] = dol[1];
		dol[1] = dol16;
	} else {
		dol[dol_count++] = dol16;
	}
}

void ES5510::run_once() {
	set_halt(false);
	icount = 1;
	execute_run();
	set_halt(true);
	while (state != STATE_HALTED) {
		icount = 1;
		execute_run();
	}
}

void ES5510::alu_operation_end() {
	if (halt_asserted) {
		state = STATE_HALTED;
		host_control &= ~0x04; // Signal Host Access OK
	}
	dbase -= memincrement;
	if (dbase < 0) {
		dbase = dlength;
	}
	if (state == STATE_RUNNING) {
		pc = 0;
	}
}

int32_t ES5510::alu_operation(uint8_t op, int32_t a, int32_t b, uint8_t &flags) {
	int32_t tmp;
	switch (op) {
	case 0x0: // ADD
		tmp = add(a, b, flags);
		return saturate(tmp, flags, (a & 0x00800000) != 0);

	case 0x1: // SUB
		tmp = add(a, negate(b), flags);
		return saturate(tmp, flags, (a & 0x00800000) != 0);

	case 0x2: // ADDU
		return add(a, b, flags);

	case 0x3: // SUBU
		return add(a, negate(b), flags);

	case 0x4: // CMP
		add(a, negate(b), flags);
		return a;

	case 0x5: // AND
		a &= b;
		flags = setFlagTo(flags, FLAG_N, (a & 0x00800000) != 0);
		flags = setFlagTo(flags, FLAG_Z, a == 0);
		return a;

	case 0x6: // OR
		a |= b;
		flags = setFlagTo(flags, FLAG_N, (a & 0x00800000) != 0);
		flags = setFlagTo(flags, FLAG_Z, a == 0);
		return a;

	case 0x7: // XOR
		a ^= b;
		flags = setFlagTo(flags, FLAG_N, (a & 0x00800000) != 0);
		flags = setFlagTo(flags, FLAG_Z, a == 0);
		return a;

	case 0x8: { // ABS
		flags = clearFlag(flags, FLAG_N);
		bool isNegative = (b & 0x00800000) != 0;
		flags = setFlagTo(flags, FLAG_C, isNegative);
		int32_t result = isNegative ? (0x00ffffff ^ b) : b;
		flags = setFlagTo(flags, FLAG_Z, (result & 0x00ffffff) == 0);
		return result;
	}

	case 0x9: // MOV
		return b;

	case 0xa: // ASL2
		return asl(b, 2, flags);

	case 0xb: // ASL8
		return asl(b, 8, flags);

	case 0xc: // LS15
		flags = clearFlag(flags, FLAG_N);
		flags = setFlagTo(flags, FLAG_C, (b & 0x00800000) != 0);
		return (b << 15) & 0x007fffff;

	case 0xd: // DIFF
		return add(0x007fffff, negate(b), flags);

	case 0xe: // ASR
		flags = setFlagTo(flags, FLAG_N, (b & 0x00800000) != 0);
		flags = setFlagTo(flags, FLAG_C, (b & 1) != 0);
		return (b >> 1) | (b & 0x00800000);

	case 0xf:
	default:
		return 0;
	}
}

void ES5510::execute_run() {
	while (icount > 0) {
		if (state == STATE_HALTED) {
			if (halt_asserted) {
				host_control &= ~0x04;
			} else {
				state = STATE_RUNNING;
				host_control |= 0x04;
				pc = 0;
			}
		} else {
			ram_pp = ram_p;
			ram_p = ram;

			uint64_t inst = this->instr[pc];

			if (ram_pp.cycle != RAM_CYCLE_WRITE) {
				if (ram_pp.io) {
					dil = 0;
				} else {
					dil = dram_r(ram_pp.address);
				}
			}

			ram_control_t ramControl = RAM_CONTROL[((inst >> 3) & 0x07)];
			ram.cycle = ramControl.cycle;
			ram.io = (ramControl.access == RAM_CONTROL_IO);

			int32_t offset = gpr[pc];
			switch (ramControl.access) {
			case RAM_CONTROL_DELAY:
				ram.address = (((dbase + offset) % (dlength + memincrement)) & memmask) >> memshift;
				break;
			case RAM_CONTROL_TABLE_A:
				ram.address = ((abase + offset) & memmask) >> memshift;
				break;
			case RAM_CONTROL_TABLE_B:
				ram.address = ((bbase + offset) & memmask) >> memshift;
				break;
			case RAM_CONTROL_IO:
				ram.address = offset & 0x00fffff0;
				break;
			}

			uint8_t operandSelect = (uint8_t)((inst >> 8) & 0x0f);
			const op_select_t &opSelect = OPERAND_SELECT[operandSelect];
			bool skip = false;
			bool skippable = (inst & (0x01 << 7)) != 0;
			if (skippable) {
				bool skipConditionSatisfied = (ccr & cmr & FLAG_MASK) != 0;
				if (isFlagSet(cmr, FLAG_NOT)) {
					skipConditionSatisfied = !skipConditionSatisfied;
				}
				skip = skipConditionSatisfied;
			}

			if (mulacc.write_result) {
				mulacc.product = mul_32x32(sext(mulacc.cValue, 24), sext(mulacc.dValue, 24)) << mulshift;
				if (mulacc.accumulate) {
					mulacc.result = mulacc.product + machl;
				} else {
					mulacc.result = mulacc.product;
				}

				if (mulacc.result < -(int64_t(1) << 47) || mulacc.result >= (int64_t(1) << 47)) {
					mac_overflow = true;
				} else {
					mac_overflow = false;
				}
				machl = mulacc.result;
				int32_t tmp = mac_overflow ? (machl < 0 ? 0x00800000 : 0x007fffff) : (mulacc.result & 0x0000ffffff000000ULL) >> 24;
				if (mulacc.dst & SRC_DST_REG) {
					write_reg(mulacc.cReg, tmp);
				}
				if (mulacc.dst & SRC_DST_DELAY) {
					write_to_dol(tmp);
				}
			}

			mulacc.cReg = (uint8_t)((inst >> 32) & 0xff);
			mulacc.dReg = (uint8_t)((inst >> 40) & 0xff);
			mulacc.src = opSelect.mac_src;
			mulacc.dst = opSelect.mac_dst;
			mulacc.accumulate = ((inst >> 6) & 0x01) != 0;
			mulacc.write_result = !skip;

			if (mulacc.src == SRC_DST_REG) {
				mulacc.cValue = read_reg(mulacc.cReg);
			} else {
				mulacc.cValue = int32_t(dil) << 8;
			}
			mulacc.dValue = read_reg(mulacc.dReg);

			if (alu.write_result) {
				uint8_t flags = ccr;
				alu.result = alu_operation(alu.op, alu.aValue, alu.bValue, flags);
				if (alu.op != OP_CMP) {
					if (alu.dst & SRC_DST_REG) {
						write_reg(alu.aReg, alu.result);
					}
					if (alu.dst & SRC_DST_DELAY) {
						write_to_dol(alu.result);
					}
				}
				if (alu.update_ccr) {
					ccr = flags;
				}
			}

			alu.aReg = (inst >> 16) & 0xff;
			alu.bReg = (inst >> 24) & 0xff;
			alu.op = (inst >> 12) & 0x0f;
			alu.src = opSelect.alu_src;
			alu.dst = opSelect.alu_dst;
			alu.write_result = !skip || (alu.op == OP_CMP);
			alu.update_ccr = !skippable || (alu.op == OP_CMP);

			if (alu.op == 0xf) {
				alu_operation_end();
			} else {
				alu_op_t aluOp = ALU_OPS[alu.op];
				if (aluOp.operands == 1) {
					if (alu.src == SRC_DST_REG) {
						alu.bValue = read_reg(alu.bReg);
					} else {
						alu.bValue = int32_t(dil) << 8;
					}
				} else {
					if (alu.src == SRC_DST_REG) {
						alu.aValue = read_reg(alu.aReg);
					} else {
						alu.aValue = int32_t(dil) << 8;
					}
					alu.bValue = read_reg(alu.bReg);
				}
			}

			if (ram_p.cycle != RAM_CYCLE_READ) {
				if (ram_p.cycle == RAM_CYCLE_WRITE) {
					if (!ram_p.io) {
						dram_w(ram_p.address, dol[0]);
					}
				}
				dol[0] = dol[1];
				if (dol_count > 0) {
					--dol_count;
				}
			}

			++pc;
		}
		--icount;
	}
}
