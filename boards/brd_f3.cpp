// Taito F3 audio (68000 + ES5505 + ES5510)

/*
 All about shared RAM and the "link-level" protocol
 
 The 68020 sees commram as a contiguous byte array.  We also handle it that
 way here since we're emulating the 68020's driver program.  The 68000 sees it
 mapped every other byte, using the 680x0 MOVEP instruction as with 8-bit 
 peripherals.  This comes into play for the command read/write addresses, which
 are always stored from the perspective of the 68000 rather than the 68020.

 Map (addresses in hex):
 000-3FF: 1K  : command strings are written here according to the protocol  

 480-481: WORD: current 68020 command write address (*2 from the 020 perspective)
 482-483: WORD: current 68000 command read address  (*2 from the 020 perspective)
 7F8-7FF: BYTE: 4 bytes of master volume settings

 The command protocol works like this:
 1) The 68020 reads the word at 0x480.  Dividing by 2 gives a byte address in
    the first 4k of commram to start writing commands to.
 2) The '020 then writes bytes starting at that address.  Command strings can
    be anywhere from 3 to 1024 bytes, at least in theory.
 3) If the '020 runs out of the first 1k of commram, writing begins again at
    address zero.  The 68000 automatically knows to wrap at that point.
 4) The byte address then should be multiplied by 2 again and written to the
    word at commram address 0x480.  The 68000 will see that the 020 has written
    past where it last read from (which is stored at 0x482) and process the 
    command string.
*/


#include "m1snd.h"
#include "sound/es5506.h"
#include "sound/es5510.h"
#include <cmath>

#define M68K_CLOCK (16000000)

static void F3_Init(long srate);
static void F3_SendCmd(int cmda, int cmdb);

static unsigned int f3_read_memory_8(unsigned int address);
static unsigned int f3_read_memory_16(unsigned int address);
static unsigned int f3_read_memory_32(unsigned int address);
static void f3_write_memory_8(unsigned int address, unsigned int data);
static void f3_write_memory_16(unsigned int address, unsigned int data);
static void f3_write_memory_32(unsigned int address, unsigned int data);

static int f3_irq_callback(int int_level);

static unsigned char f3work[0x20000];
static unsigned char commram[0x2000];
static unsigned char bExtCodeBuf[128];

static int lastcmd = 1, lastcode;

static TimerT *timer_68681=(TimerT *)NULL;
static int timer_mode,m68681_imr;
static int counter,imr_status;
static int vector_reg;

static ES5510 es5510;
static bool esp_halted = true;
static uint8_t duart_opr = 0;

static uint8_t mb87078_control = 0;
static uint8_t mb87078_data = 0;
static uint16_t mb87078_channel_latch[4] = { 0x7f, 0x7f, 0x7f, 0x7f };
static float mb87078_gain[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

static void mb87078_recalc()
{
	for (int i = 0; i < 4; i++)
	{
		if (~mb87078_channel_latch[i] & 0x40)
		{
			mb87078_gain[i] = 0.0f;
		}
		else if (mb87078_channel_latch[i] & 0x100)
		{
			mb87078_gain[i] = 0.02511886f; // -32dB
		}
		else if (mb87078_channel_latch[i] & 0x80)
		{
			mb87078_gain[i] = 1.0f; // 0dB
		}
		else
		{
			int index = ~mb87078_channel_latch[i] & 0x3f;
			mb87078_gain[i] = (float)pow(10.0, -(index * 0.5) / 20.0);
		}
	}
}

static void mb87078_write(uint32_t offset, uint8_t data)
{
	if (offset & 1)
	{
		mb87078_control = data & 0x1f;
	}
	else
	{
		mb87078_data = data & 0x3f;
		uint16_t new_latch = ((mb87078_control << 4) & 0x1c0) | mb87078_data;
		if (new_latch != mb87078_channel_latch[mb87078_control & 3])
		{
			mb87078_channel_latch[mb87078_control & 3] = new_latch;
			mb87078_recalc();
		}
	}
}

static void f3_update_esphalt()
{
	bool halted = ((duart_opr ^ 0xff) & 0x40) != 0;
	if (halted != esp_halted)
	{
		esp_halted = halted;
		es5510.set_halt(esp_halted);
	}
}

static void f3_pump_callback(int chip,
	INT32 ch0_l, INT32 ch0_r,
	INT32 ch1_l, INT32 ch1_r,
	INT32 ch2_l, INT32 ch2_r,
	INT32 ch3_l, INT32 ch3_r,
	INT32 *out_l, INT32 *out_r)
{
	float vol_l = (float)(mb87078_channel_latch[2] & 0x3f) / 32.0f;
	float vol_r = (float)(mb87078_channel_latch[3] & 0x3f) / 32.0f;

	// In MAME taito_en, ensoniq routes to pump with gain 0.18 * (vol / 32.0)
	float in_gain_l = 0.18f * vol_l;
	float in_gain_r = 0.18f * vol_r;

	auto clamp16_l = [in_gain_l](INT32 val) -> int16_t {
		INT32 v = (INT32)(val * in_gain_l) >> 4;
		if (v > 32767) return 32767;
		if (v < -32768) return -32768;
		return (int16_t)v;
	};
	auto clamp16_r = [in_gain_r](INT32 val) -> int16_t {
		INT32 v = (INT32)(val * in_gain_r) >> 4;
		if (v > 32767) return 32767;
		if (v < -32768) return -32768;
		return (int16_t)v;
	};

	es5510.ser_w(0, clamp16_l(ch1_l)); // FX1 L
	es5510.ser_w(1, clamp16_r(ch1_r)); // FX1 R
	es5510.ser_w(2, clamp16_l(ch2_l)); // FX2 L
	es5510.ser_w(3, clamp16_r(ch2_r)); // FX2 R
	es5510.ser_w(4, clamp16_l(ch3_l)); // DRY L
	es5510.ser_w(5, clamp16_r(ch3_r)); // DRY R

	if (!esp_halted)
	{
		es5510.run_once();
	}

	int16_t wet_l = es5510.ser_r(6);
	int16_t wet_r = es5510.ser_r(7);

	// In MAME taito_en, pump routes to speaker with gain 0.5 * (vol / 32.0).
	// In M1 standalone output, 1.0 * (vol / 32.0) brings peak audio level to ~7200,
	// exactly matching the PCB reference recording without clipping.
	float out_gain_l = 1.0f * vol_l;
	float out_gain_r = 1.0f * vol_r;

	INT32 aux_l = (INT32)((ch0_l >> 4) * in_gain_l);
	INT32 aux_r = (INT32)((ch0_r >> 4) * in_gain_r);

	// es5506_update() shifts right by 4 (*lsrc++ >> 4) when writing to 16-bit sound buffer.
	// We scale by << 4 so that the effective volume matches 16-bit DAC amplitude.
	*out_l = (INT32)((wet_l + aux_l) * out_gain_l) << 4;
	*out_r = (INT32)((wet_r + aux_r) * out_gain_r) << 4;
}

// MC68681 DUART clock:
// The sequencer calculates ticks using Bresenham DDA: accum += tempo; accum -= 625;
// For standard 24 PPQN (96 ticks/quarter note), 625 * 96 = 60,000, so a 1000 Hz timer yields BPM = tempo.
// However, F3 sequencer songs use 100 ticks/quarter note (625 * 100 = 62,500),
// requiring a timer frequency of 62500 / 60 = 1041.6667 Hz (25/24 ratio) for BPM to equal the tempo setting.
// With timer count = 2000 ($7d0), the effective timer clock is 2000 * (62500 / 60) = 25000000 / 12 = 2083333.33 Hz.
#define M68681_CLOCK	(25000000.0 / 12.0) /* Actually X1 / 2 for timer mode */

enum { TIMER_SINGLESHOT, TIMER_PULSE };

static struct ES5505interface es5505_interface =
{
	1,					/* total number of chips */
	{ (30476100/2) },		/* freq */
	{ RGN_SAMP2 },	/* Bank 0: Unused by F3 games? */
	{ RGN_SAMP1 },	/* Bank 1: All games seem to use this */
	{ YM3012_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) },		/* master volume */
};

static M168KT f3_readwritemem =
{
	f3_read_memory_8,
	f3_read_memory_16,
	f3_read_memory_32,
	f3_write_memory_8,
	f3_write_memory_16,
	f3_write_memory_32,
};

void es5506_update(int num, INT16 **buffer, int length);

static double TIME_IN_CYCLES(double cycles, int cpu)
{
	return (cycles / (double)M68K_CLOCK);
}

static void timer_callback(int param)
{
	if (timer_mode==TIMER_SINGLESHOT) timer_68681=NULL;

	/* Only cause IRQ if the mask is set to allow it */
	if (m68681_imr&8) 
	{
		m68k_set_int_ack_callback(f3_irq_callback);	
		m68k_set_irq(M68K_IRQ_6);
		imr_status|=0x8;
	}
}

static void update_68681_timer(void)
{
	int count = (counter == 0) ? 65536 : counter;
	if (timer_68681) timer_remove(timer_68681);
	if (timer_mode == TIMER_SINGLESHOT)
	{
		timer_68681 = timer_set(TIME_IN_CYCLES(((double)M68K_CLOCK / M68681_CLOCK) * count * 16, 1), 0, timer_callback);
	}
	else if (timer_mode == TIMER_PULSE)
	{
		timer_68681 = timer_pulse(TIME_IN_CYCLES(((double)M68K_CLOCK / M68681_CLOCK) * count, 1), 0, timer_callback);
	}
}

static READ16_HANDLER(f3_68681_r)
{
	if (offset==0x5) {
		int ret=imr_status;
//		printf("rd IMR (PC=%x)\n", m68k_get_reg(NULL, M68K_REG_PC));
		imr_status=0;
		return ret;
	}

	/* Start counter command */
	if (offset==0xe) {
		update_68681_timer();
		return 1;
	}

	/* IRQ ack */
	if (offset==0xf) {
//		printf("ACK (PC=%x)\n", m68k_get_reg(NULL, M68K_REG_PC));
		m68k_set_irq(M68K_IRQ_NONE);
		return 0;
	}

	return 0xff;
}

static WRITE16_HANDLER(f3_68681_w)
{
	switch (offset) {
		case 0x04: /* ACR */
			switch ((data>>4)&7) {
				case 0:
//					logerror("Counter:  Unimplemented external IP2\n");
					break;
				case 1:
//					logerror("Counter:  Unimplemented TxCA - 1X clock of channel A\n");
					break;
				case 2:
//					logerror("Counter:  Unimplemented TxCB - 1X clock of channel B\n");
					break;
				case 3:
//					logerror("Counter:  X1/Clk - divided by 16, counter is %04x, so interrupt every %d cycles\n",counter,(M68K_CLOCK/M68681_CLOCK)*counter*16);
					timer_mode=TIMER_SINGLESHOT;
					update_68681_timer();
					break;
				case 4:
//					logerror("Timer:  Unimplemented external IP2\n");
					break;
				case 5:
//					logerror("Timer:  Unimplemented external IP2/16\n");
					break;
				case 6:
//					logerror("Timer:  X1/Clk, counter is %04x, so interrupt every %d cycles\n",counter,(M68K_CLOCK/M68681_CLOCK)*counter);
					timer_mode=TIMER_PULSE;
					update_68681_timer();
					break;
				case 7:
//					logerror("Timer:  Unimplemented X1/Clk - divided by 16\n");
					break;
			}
			break;

		case 0x05: /* IMR */
//			logerror("68681:  %02x %02x\n",offset,data&0xff);
			m68681_imr=data&0xff;
			break;

		case 0x06: /* CTUR */
			counter=((data&0xff)<<8)|(counter&0xff);
			if (timer_68681) update_68681_timer();
			break;
		case 0x07: /* CTLR */
			counter=(counter&0xff00)|(data&0xff);
			if (timer_68681) update_68681_timer();
			break;
		case 0x08: break; /* MR1B (Mode register B) */
		case 0x09: break; /* CSRB (Clock select register B) */
		case 0x0a: break; /* CRB (Command register B) */
		case 0x0b: break; /* TBB (Transmit buffer B) */
		case 0x0c: /* IVR (Interrupt vector) */
			vector_reg=data&0xff;
			break;
		case 0x0e: /* Set Output Port Bits */
			duart_opr |= (data & 0xff);
			f3_update_esphalt();
			break;
		case 0x0f: /* Reset Output Port Bits */
			duart_opr &= ~(data & 0xff);
			f3_update_esphalt();
			break;
		default:
//			logerror("68681:  %02x %02x\n",offset,data&0xff);
			break;
	}
}

READ16_HANDLER(es5510_dsp_r)
{
	return es5510.host_r(offset & 0xff);
}

WRITE16_HANDLER(es5510_dsp_w)
{
	es5510.host_w(offset & 0xff, data & 0xff);
}

static unsigned int f3_read_memory_8(unsigned int address)
{
	if ((address < 0x40000))
	{
		return f3work[address&0xffff];
	}

	if ((address >= 0xff0000))
	{
		return f3work[(address&0xffff)];
	}

	if (address >= 0x140000 && address <= 0x140fff)
	{
/*		if (commram[(address&0xfff)/2] != 0)
		{
			if (((address & 0xfff)/2) < 0x7f0)
				printf("read %x from commram at %x\n", commram[(address&0xfff)/2], (address&0xfff)/2);
		} */
		return commram[(address&0xfff)/2];
	}

	if (address == 0x200001)
	{
		return 0;
	}

	if (address >= 0x260000 && address <= 0x26ffff)
	{
		if (address & 1)
		{
			return es5510.host_r(((address & 0x1ff) / 2) & 0xff);
		}
		else
		{
			return 0;
		}
	}

	if (address >= 0x280000 && address <= 0x28ffff)
	{
		if (address & 1)
		{
			return (f3_68681_r((address&0xffff)/2, 0xff00));
		}
		else
		{
			return (f3_68681_r((address&0xffff)/2, 0x00ff))>>8;
		}
	}

	if (address >= 0xc00000 && address <= 0xc7ffff)
	{
		return prgrom[address&0x7ffff];
	}

	printf("Unmapped read 8 at %x\n", address);
	return 0;
}

static unsigned int f3_read_memory_16(unsigned int address)
{
	if ((address < 0x40000))
	{
		address &= 0xffff;
		return mem_readword_swap((unsigned short *)(f3work+address));
	}

	if ((address >= 0xff0000))
	{
		address &= 0xffff;
		return mem_readword_swap((unsigned short *)(f3work+address));
	}

	if (address >= 0x200000 && address <= 0x200460)
	{
		return ES5505_data_0_r((address&0xfff)/2, 0);
	}

	if (address >= 0x260000 && address <= 0x26ffff)
	{
		return es5510.host_r(((address & 0x1ff) / 2) & 0xff);
	}

	if (address >= 0x280000 && address <= 0x28ffff)
	{
		return f3_68681_r((address&0xffff)/2, 0);
	}

	if (address >= 0xc00000 && address <= 0xc7ffff)
	{
		address &= 0x7ffff;
		return mem_readword_swap((unsigned short *)(prgrom+address));
	}

	printf("Unmapped read 16 at %x (PC=%x)\n", address, m68k_get_reg(NULL, M68K_REG_PC));
//		exit(-1);
	return 0;
}

static unsigned int f3_read_memory_32(unsigned int address)
{
	if ((address < 0x40000))
	{
		address &= 0xffff;
		return mem_readlong_swap((unsigned int *)(f3work+address));
	}

	if ((address >= 0xff0000))
	{
		address &= 0xffff;
		return mem_readlong_swap((unsigned int *)(f3work+address));
	}

	return (f3_read_memory_16(address)<<16) | (f3_read_memory_16(address+2));
}

static void f3_write_memory_8(unsigned int address, unsigned int data)
{
	if ((address < 0x40000))
	{
		f3work[address&0xffff] = data;
		return;
	}

	if ((address >= 0xff0000))
	{
		f3work[(address&0xffff)] = data;
		return;
	}

	if (address >= 0x140000 && address <= 0x140fff)
	{
//		printf("write %x to commram at %x\n", data, (address&0xfff)/2);
//		if (((address&0xfff)/2)&0xff0 == 0x480)
//			printf("write %x to commram at %x\n", data, (address&0xfff)/2);

		commram[(address&0xfff)/2] = data;
		return;
	}

	if (address >= 0x260000 && address <= 0x26ffff)
	{
		if (address & 1)
		{
			es5510.host_w(((address & 0x1ff) / 2) & 0xff, data & 0xff);
		}
		return;
	}

	if (address >= 0x280000 && address <= 0x28ffff)
	{
		if (address & 1)
		{
			f3_68681_w((address&0xffff)/2, data, 0xff00);
		}
		else
		{
			f3_68681_w((address&0xffff)/2, data<<8, 0x00ff);
		}
		return;
	}

	if (address >= 0x300000 && address <= 0x300080)
	{
		unsigned int max_banks_this_game=(rom_getregionsize(RGN_SAMP1)/0x200000)-1;
		int offset = (address&0xff)/2;

//		printf("ES550x bankswitch: voice %d to %x\n", (address&0xff)/2, data);
		if ((data&0x7)>max_banks_this_game)
			ES5506_voice_bank_0_w(offset, max_banks_this_game<<20);
		else
			ES5506_voice_bank_0_w(offset, (data&0x7)<<20);
		return;
	}

	if (address >= 0x340000 && address <= 0x340003)
	{
		if (!(address & 1))
		{
			mb87078_write(((address - 0x340000) / 2) ^ 1, data & 0xff);
		}
		return;
	}

	printf("Unmapped write 8 %x to %x\n", data, address);
}

static void f3_write_memory_16(unsigned int address, unsigned int data)
{
	if ((address < 0x40000))
	{
		address &= 0xffff;
		mem_writeword_swap((unsigned short *)(f3work+address), data);
		return;
	}

	if ((address >= 0xff0000))
	{
		address &= 0xffff;
		mem_writeword_swap((unsigned short *)(f3work+address), data);
		return;
	}

	if (address >= 0x200000 && address <= 0x20ffff)
	{
		ES5505_data_0_w((address&0xfff)/2, data, 0);
		return;
	}

	if (address >= 0x260000 && address <= 0x26ffff)
	{
		es5510.host_w(((address & 0x1ff) / 2) & 0xff, data & 0xff);
		return;
	}

	if (address >= 0x280000 && address <= 0x28ffff)
	{
		f3_68681_w((address&0xffff)/2, data, 0);
		return;
	}

	if (address >= 0x340000 && address <= 0x340003)
	{
		mb87078_write(((address - 0x340000) / 2) ^ 1, (data >> 8) & 0xff);
		return;
	}

	printf("Unmapped write 16 %x to %x\n", data, address);
}

static void f3_write_memory_32(unsigned int address, unsigned int data)
{
	if ((address < 0x40000))
	{
		address &= 0xffff;
		mem_writelong_swap((unsigned int *)(f3work+address), data);
		return;
	}

	if (address >= 0xff0000)
	{
		address &= 0xffff;
		mem_writelong_swap((unsigned int *)(f3work+address), data);
		return;
	}

	f3_write_memory_16(address, data >> 16);
	f3_write_memory_16(address + 2, data & 0xffff);
}

static int f3_irq_callback(int int_level)
{
	return vector_reg;
}

static int addr;

static void F3_Init(long srate)
{
	memset(f3work, 0, 64*1024);

	// copy down the 68k vector table
	memcpy(f3work, prgrom, 256);

	memset(bExtCodeBuf, 0, sizeof(bExtCodeBuf));
	lastcode = 0;

	if (timer_68681) { timer_remove(timer_68681); timer_68681 = NULL; }
	timer_mode = TIMER_PULSE;
	m68681_imr = 0;
	counter = 0;
	imr_status = 0;
	vector_reg = 0;

	memset(commram, 0, 0x1000);
	commram[0x7f8] = 0x30;
	commram[0x7f9] = 0x30;
	commram[0x7fa] = 0x30;
	commram[0x7fb] = 0x30;
							  
	addr = 0;

	es5510.reset();
	esp_halted = true;
	es5510.set_halt(true);
	duart_opr = 0;

	mb87078_control = 0;
	mb87078_data = 0;
	for (int i = 0; i < 4; i++)
		mb87078_channel_latch[i] = 0x7f;
	mb87078_recalc();

	ES5505_set_pump_callback(0, f3_pump_callback);
}

static void f3_write_byte(int byte)
{
	commram[addr] = byte;
	addr++;

//	printf("%x\n", byte);

	// the 68020 and 68000 agree to wrap at this point
	// and this point only!
	if (addr >= 0x400)
	{
		addr = 0;
	}
}

void ExtCodeCase0(UINT8 *s)
{
	UINT8 ch = *s;

	if (ch < 0x80) {
		// $00 - $7F
		bExtCodeBuf[ch] = ch;

		f3_write_byte(0x03);
		f3_write_byte(0x81);
		f3_write_byte(ch);
		f3_write_byte(0x04);
		f3_write_byte(0x86);
		f3_write_byte(*s++);
		f3_write_byte(*s);
	}
	else {
		// $80 - $FF
		UINT8 ch2 = ch & 0x7f;

		bExtCodeBuf[ch2] = ch;

		f3_write_byte(0x03);
		f3_write_byte(0x81);
		f3_write_byte(*s);
		f3_write_byte(0x04);
		f3_write_byte(0x86);
		f3_write_byte(*s);
		f3_write_byte(*(s + 1));
		f3_write_byte(0x04);
		f3_write_byte(0x86);
		f3_write_byte(ch2);
		f3_write_byte(*(s + 1));
	}
}

void ExtCodeCase1(UINT8 *s)
{
	UINT8 ch = *s & 0x7f;

	bExtCodeBuf[ch] = *s;

	f3_write_byte(0x03);
	f3_write_byte(0x81);
	f3_write_byte(*s);
	f3_write_byte(0x04);
	f3_write_byte(0x86);
	f3_write_byte(*s);
	f3_write_byte(*(s + 1));
	f3_write_byte(0x04);
	f3_write_byte(0x88);
	f3_write_byte(*(s + 2));
	f3_write_byte(*s);
}

void ExtCodeCase2(UINT8 *s)
{
	UINT8 ch = *s;
	INT16 offs = ((ch - 1) << 3) + *(s + 2);

	if (bExtCodeBuf[offs]) {
		f3_write_byte(0x05);
		f3_write_byte(0x8f);
		f3_write_byte(ch);
		f3_write_byte(*(s + 2));
		f3_write_byte(bExtCodeBuf[offs]);
	}

	f3_write_byte(0x06);
	f3_write_byte(0x8d);
	f3_write_byte(*s);
	f3_write_byte(*(s + 2));
	f3_write_byte(*(s + 3));
	f3_write_byte(*(s + 4));
	f3_write_byte(0x06);
	f3_write_byte(0x8e);
	f3_write_byte(*s);
	f3_write_byte(*(s + 2));
	f3_write_byte(*(s + 5));
	f3_write_byte(*(s + 1));

	bExtCodeBuf[offs] = *(s + 5);
}

void ExtCodeCase3(UINT8 *s)
{
	int size = *s++;

	while (size > 0) {
		f3_write_byte(*s++);
		size--;
	}
}

void ExtCodeCase4(UINT8 *s)
{
	UINT8 ch = *s & 0x7f, ch2;

	ch2 = bExtCodeBuf[ch];
	if (ch2) {
		bExtCodeBuf[ch] = 0;
		f3_write_byte(0x04);
		f3_write_byte(0x86);
		f3_write_byte(ch2);
		f3_write_byte(0x00);
		f3_write_byte(0x03);
		f3_write_byte(0x82);
		f3_write_byte(ch2);
	}
}

void ExtCodeCase5(UINT8 *s)
{
	UINT8 ch = *s, ch2;
	INT16 offs = ((ch - 1) << 3) + *(s + 2);

	ch2 = bExtCodeBuf[offs];
	if (ch2) {
		bExtCodeBuf[offs] = 0;

		f3_write_byte(0x05);
		f3_write_byte(0x8f);
		f3_write_byte(*s);
		f3_write_byte(*(s + 2));
		f3_write_byte(ch2);
	}
}

void FetchCode(int type, int _code)
{
	UINT8 *src, job;
	UINT16 code;
	int tbladr, adrofs;

	code = _code & 0xffff;
	adrofs = Machine->refcon + (code << 2);
	tbladr = prgrom2[adrofs]<<24 | prgrom2[adrofs+1]<<16 | prgrom2[adrofs+2]<<8 | prgrom2[adrofs+3];

	if (tbladr < 0) return;
	if (tbladr > 2*1024*1024) return;

//	printf("refcon %x, tbladr %x\n", Machine->refcon, tbladr);
	src = &prgrom2[tbladr];
	job = *src++;

//	printf("job %x\n", job);

	if (type == 0)	// stop
	{
		switch (job)
		{
			case 0:
			case 1:
				ExtCodeCase4(src);
				break;
			case 2:
				ExtCodeCase5(src);
				break;
			default:
				break;
		}
	}
	else
	{
		switch (job)
		{
			case 0:
				ExtCodeCase0(src);
				break;
			case 1:
				ExtCodeCase1(src);
				break;
			case 2:
				ExtCodeCase2(src);
				break;
			case 3:
				ExtCodeCase3(src);
				break;
			case 4:
			default:
				break;
		}
	}
}

void Play(int code)
{
	FetchCode(0, lastcode);
	FetchCode(1, code);

	lastcode = code;
}

static void F3_SendCmd(int cmda, int cmdb)
{
	ES5505_set_pump_callback(0, f3_pump_callback);
	addr = commram[0x480]<<8 | commram[0x481];
//	printf("addr in = %x, 68k last=%x\n", addr, commram[0x0482]<<8|commram[0x0483]);
	addr /= 2;

//	printf("cmda %x\n", cmda);

	if (Machine->refcon != 0)
	{
		if (cmda != 0)
		{
			Play(cmda);
		}
	}
	else
	{
		if (cmda)
		{
			if (cmda < 0x100)
			{
				// play music command
				f3_write_byte(0x03);
				f3_write_byte(0x81);     
				f3_write_byte(cmda);

				// set volume (to 7f)
				f3_write_byte(0x04);     
				f3_write_byte(0x86);     
				f3_write_byte(cmda);
				f3_write_byte(0x7f);
			}
			else	// effect play
			{
	#if 0
				f3_write_byte(0x06);
				f3_write_byte(0x8e);
				f3_write_byte(cmda>>8);
				f3_write_byte(0x07);
				f3_write_byte(cmda&0xff);
				f3_write_byte(0x7f);	// volume
	#endif
			}

			lastcmd = cmda;
		}
		else
		{
			if (lastcmd < 0x100)
			{
				// set volume to 0
				f3_write_byte(0x04);
				f3_write_byte(0x86);
				f3_write_byte(lastcmd);
				f3_write_byte(0x00);

				// stop music
				f3_write_byte(0x03);
				f3_write_byte(0x82);
				f3_write_byte(lastcmd);
			}
			else
			{	// effect stop
	#if 0
				f3_write_byte(0x06);
				f3_write_byte(0x8d);
				f3_write_byte(0x01);
				f3_write_byte(0x07);
				f3_write_byte(0x40);
				f3_write_byte(0x00);
	#endif
			}
		}
	}

	addr *= 2;
//	printf("addr out = %x\n", addr);

	commram[0x480] = addr>>8;
	commram[0x481] = addr&0xff;
}

M1_BOARD_START( f3 )
	MDRV_NAME( "F3/Super Z/JC Systems" )
	MDRV_HWDESC( "68000, ES5505, ES5510 ESP" )
	MDRV_DELAYS( 2000, 200 )
	MDRV_INIT( F3_Init )
	MDRV_SEND( F3_SendCmd )

	MDRV_CPU_ADD(MC68000, M68K_CLOCK)
	MDRV_CPUMEMHAND(&f3_readwritemem)

	MDRV_SOUND_ADD(ES5505, &es5505_interface)
M1_BOARD_END
