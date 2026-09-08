// license:BSD-3-Clause
// copyright-holders:Christian Brunschen
/**********************************************************************************************
 *
 *   es5510.h - Ensoniq ES5510 (ESP) standalone emulator for M1
 *   based on MAME's es5510.cpp/h by Christian Brunschen
 *
 **********************************************************************************************/

#ifndef M1_SOUND_ES5510_H
#define M1_SOUND_ES5510_H

#include <cstdint>
#include <memory>
#include <cstring>
#include <algorithm>

class ES5510 {
public:
	static constexpr uint32_t DRAM_SIZE = (1 << 20); // 1M words
	static constexpr uint32_t DRAM_MASK = (DRAM_SIZE - 1);

	enum line_t {
		ES5510_HALT = 0
	};

	enum state_t {
		STATE_RUNNING = 0,
		STATE_HALTED = 1
	};

	struct alu_op_t {
		int operands;
		const char *opcode;
	};

	enum op_src_dst_t {
		SRC_DST_REG   = 1 << 0,
		SRC_DST_DELAY = 1 << 1,
		SRC_DST_BOTH  = (1 << 0) | (1 << 1)
	};

	struct op_select_t {
		op_src_dst_t alu_src;
		op_src_dst_t alu_dst;
		op_src_dst_t mac_src;
		op_src_dst_t mac_dst;
	};

	enum ram_control_access_t {
		RAM_CONTROL_DELAY = 0,
		RAM_CONTROL_TABLE_A,
		RAM_CONTROL_TABLE_B,
		RAM_CONTROL_IO
	};

	enum ram_cycle_t {
		RAM_CYCLE_READ = 0,
		RAM_CYCLE_WRITE = 1,
		RAM_CYCLE_DUMP_FIFO = 2
	};

	struct ram_control_t {
		ram_cycle_t cycle;
		ram_control_access_t access;
		const char *description;
	};

	static const alu_op_t ALU_OPS[16];
	static const op_select_t OPERAND_SELECT[16];
	static const ram_control_t RAM_CONTROL[8];

	struct alu_t {
		uint8_t aReg;
		uint8_t bReg;
		op_src_dst_t src;
		op_src_dst_t dst;
		uint8_t op;
		int32_t aValue;
		int32_t bValue;
		int32_t result;
		bool update_ccr;
		bool write_result;
	};

	struct mulacc_t {
		uint8_t cReg;
		uint8_t dReg;
		op_src_dst_t src;
		op_src_dst_t dst;
		bool accumulate;
		int32_t cValue;
		int32_t dValue;
		int64_t product;
		int64_t result;
		bool write_result;
	};

	struct ram_t {
		int32_t address;
		bool io;
		ram_cycle_t cycle;
	};

	ES5510();
	~ES5510() = default;

	void reset();

	uint8_t host_r(uint32_t offset);
	void host_w(uint32_t offset, uint8_t data);

	int16_t ser_r(int offset) const;
	void ser_w(int offset, int16_t data);

	void set_halt(bool halt) { halt_asserted = halt; }
	bool get_halt() const { return halt_asserted; }
	bool is_halted() const { return state == STATE_HALTED; }

	void run_once();

	int32_t read_reg(uint8_t reg);
	void write_reg(uint8_t reg, int32_t value);
	void write_to_dol(int32_t value);

private:
	int32_t alu_operation(uint8_t op, int32_t aValue, int32_t bValue, uint8_t &flags);
	void alu_operation_end();
	void execute_run();

	int16_t dram_r(int addr) const { return dram[addr & DRAM_MASK]; }
	void dram_w(int addr, int16_t data) { dram[addr & DRAM_MASK] = data; }

	int icount;
	bool halt_asserted;
	uint8_t pc;
	state_t state;
	std::unique_ptr<int32_t[]> gpr;
	int16_t ser0r;
	int16_t ser0l;
	int16_t ser1r;
	int16_t ser1l;
	int16_t ser2r;
	int16_t ser2l;
	int16_t ser3r;
	int16_t ser3l;
	int64_t machl;
	bool mac_overflow;
	int16_t dil;
	int32_t memsiz;
	int32_t memmask;
	int32_t memincrement;
	int8_t memshift;
	int32_t dlength;
	int32_t abase;
	int32_t bbase;
	int32_t dbase;
	int32_t sigreg;
	int mulshift;
	int8_t ccr;
	int8_t cmr;
	int16_t dol[2];
	int dol_count;

	std::unique_ptr<uint64_t[]> instr;
	std::unique_ptr<int16_t[]> dram;

	int32_t dol_latch;
	int32_t dil_latch;
	uint32_t dadr_latch;
	int32_t gpr_latch;
	uint64_t instr_latch;
	uint8_t ram_sel;
	uint8_t host_control;
	uint8_t host_serial;

	alu_t alu;
	mulacc_t mulacc;
	ram_t ram, ram_p, ram_pp;
};

#endif // M1_SOUND_ES5510_H
