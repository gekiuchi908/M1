// Konami System GX audio (68000 + 2xK054539 + TMS57002)
// Konami Hornet audio (68000 + RF5c400, very similar board otherwise)
// Konami GTI Club audio (68000 + RF5c400, a minor evolution from Hornet)

#include "m1snd.h"

#define M68K_CLOCK (8000000)
#define K539_CLOCK (48000)
#define GX_TIMER_RATE (1.0/485.0)
#define HORNET_TIMER_RATE (1000.0/(44100.0/128.0))

static int h_irq1_enable = 0;
static int h_irq2_enable = 0;
static int h_irq2_pending = 0;
static unsigned char k056800_host_to_snd[4];
static unsigned char k056800_snd_to_host[2];

static void GX_Init(long srate);
static void GX_SendCmd(int cmda, int cmdb);

static void Hornet_Init(long srate);
static void Hornet_SendCmd(int cmda, int cmdb);

static unsigned int gx_read_memory_8(unsigned int address);
static unsigned int gx_read_memory_16(unsigned int address);
static unsigned int gx_read_memory_32(unsigned int address);
static void gx_write_memory_8(unsigned int address, unsigned int data);
static void gx_write_memory_16(unsigned int address, unsigned int data);
static void gx_write_memory_32(unsigned int address, unsigned int data);

static void hornet_timer(int refcon); // forward decl

static unsigned int hornet_read_memory_8(unsigned int address);
static unsigned int hornet_read_memory_16(unsigned int address);
static unsigned int hornet_read_memory_32(unsigned int address);
static void hornet_write_memory_8(unsigned int address, unsigned int data);
static void hornet_write_memory_16(unsigned int address, unsigned int data);
static void hornet_write_memory_32(unsigned int address, unsigned int data);

static unsigned int gti_read_memory_8(unsigned int address);
static unsigned int gti_read_memory_16(unsigned int address);
static unsigned int gti_read_memory_32(unsigned int address);
static void gti_write_memory_8(unsigned int address, unsigned int data);
static void gti_write_memory_16(unsigned int address, unsigned int data);
static void gti_write_memory_32(unsigned int address, unsigned int data);

static unsigned char commram[64];

static struct K054539interface k054539_interface =
{
	2,
	48000,
	{ RGN_SAMP1, RGN_SAMP1 },
	{ { 100, 100 }, { 100, 100 } },
};

static struct RF5C400interface rf5c400_interface =
{
	RGN_SAMP1
};

static M168KT gx_readwritemem =
{
	gx_read_memory_8,
	gx_read_memory_16,
	gx_read_memory_32,
	gx_write_memory_8,
	gx_write_memory_16,
	gx_write_memory_32,
};

static M168KT hornet_readwritemem =
{
	hornet_read_memory_8,
	hornet_read_memory_16,
	hornet_read_memory_32,
	hornet_write_memory_8,
	hornet_write_memory_16,
	hornet_write_memory_32,
};

static M168KT gti_readwritemem =
{
	gti_read_memory_8,
	gti_read_memory_16,
	gti_read_memory_32,
	gti_write_memory_8,
	gti_write_memory_16,
	gti_write_memory_32,
};

M1_BOARD_START( gx )
	MDRV_NAME( "System GX" )
	MDRV_HWDESC( "68000, K054539(x2), TMS57002" )
	MDRV_DELAYS( 5000, 15 )
	MDRV_INIT( GX_Init )
	MDRV_SEND( GX_SendCmd )

	MDRV_CPU_ADD(MC68000, M68K_CLOCK)
	MDRV_CPUMEMHAND(&gx_readwritemem)

	MDRV_SOUND_ADD(K054539, &k054539_interface)
M1_BOARD_END

M1_BOARD_START( hornet )
	MDRV_NAME( "Konami Hornet" )
	MDRV_HWDESC( "68000, RF5c400" )
	MDRV_DELAYS( 5500, 35 )
	MDRV_INIT( Hornet_Init )
	MDRV_SEND( Hornet_SendCmd )

	MDRV_CPU_ADD(MC68000, 16000000)
	MDRV_CPUMEMHAND(&hornet_readwritemem)

	MDRV_SOUND_ADD(RF5C400, &rf5c400_interface)
M1_BOARD_END

M1_BOARD_START( gticlub )
	MDRV_NAME( "GTI Club" )
	MDRV_HWDESC( "68000, RF5c400" )
	MDRV_DELAYS( 5500, 35 )
	MDRV_INIT( Hornet_Init )
	MDRV_SEND( Hornet_SendCmd )

	MDRV_CPU_ADD(MC68000, 16000000)
	MDRV_CPUMEMHAND(&gti_readwritemem)

	MDRV_SOUND_ADD(RF5C400, &rf5c400_interface)
M1_BOARD_END

static unsigned int gx_read_memory_8(unsigned int address)
{
	address &= 0xffffff;

	if (address < 0x40000)
	{
		return prgrom[address];
	}

	if (address >= 0x100000 && address <= 0x10ffff)
	{
		address &= 0xffff;
		return workram[address];
	}

	if (address >= 0x200000 && address <= 0x2004ff)
	{
		if (address & 1)
		{
			address &= 0xfff;
			address >>= 1;
			return K054539_1_r(address);
		}
		else
		{
			address &= 0xfff;
			address >>= 1;
			return K054539_0_r(address);
		}
	}

	if (address == 0x300001)
	{
		return tms57002_data_r(0);
	}

	if (address >= 0x400000 && address <= 0x40001f)
	{
		return commram[(address&0x1f)>>1];
	}

	if (address == 0x500001)
	{
		return tms57002_status_r(0);
	}

	return 0;
}

static unsigned int gx_read_memory_16(unsigned int address)
{
	address &= 0xffffff;

	if (address < 0x40000)
	{
		return mem_readword_swap((unsigned short *)(prgrom+address));
	}

	if ((address >= 0x100000) && (address <= 0x10ffff))
	{
		address &= 0xffff;
		return mem_readword_swap((unsigned short *)(workram+address));
	}

	if (address >= 0x200000 && address <= 0x200460)
	{
		int reg = (address >> 1) & 0xfff;

		return (K054539_0_r(reg)<<8) | K054539_1_r(reg);
	}

	return 0;
}

static unsigned int gx_read_memory_32(unsigned int address)
{
	address &= 0xffffff;

	if (address < 0x40000)
	{
		return mem_readlong_swap((unsigned int *)(prgrom+address));
	}

	if ((address >= 0x100000) && (address <= 0x10ffff))
	{
		address &= 0xffff;
		return mem_readlong_swap((unsigned int *)(workram+address));
	}

	return 0;
}

static void gx_write_memory_8(unsigned int address, unsigned int data)
{
	address &= 0xffffff;

	if (address >= 0x100000 && address <= 0x10ffff)
	{
		address &= 0xffff;
		workram[address] = data;
		return;
	}

	if (address >= 0x200000 && address < 0x200460)
	{
		if (address & 1)
		{
			address -= 0x200000;
			address >>= 1;
			K054539_1_w(address, data);
			return;
		}
		else
		{
			address -= 0x200000;
			address >>= 1;
			K054539_0_w(address, data);
			return;
		}
	}

	if (address == 0x300001)
	{
		tms57002_data_w(0, data);
		return;
	}

	if (address >= 0x400000 && address < 0x400020)
	{
//		if ((address != 0x400001) && (address != 0x400009))
//			printf("write %x to commram at %x\n", data, address);
		commram[(address&0x1f)>>1] = data;
		return;
	}
		 
	if (address == 0x500001)
	{
		tms57002_control_w(0, data);
		return;
	}
}

static void gx_write_memory_16(unsigned int address, unsigned int data)
{
	address &= 0xffffff;

	if (address >= 0x100000 && address <= 0x10ffff)
	{
		address &= 0xffff;
		mem_writeword_swap((unsigned short *)(workram+address), data);
		return;
	}

	if (address >= 0x200000 && address < 0x200460)
	{
		int reg;

		reg = address - 0x200000;
		reg >>= 1;
		
		K054539_0_w(reg, data>>8);
		K054539_1_w(reg, data&0xff);
		return;
	}
}

static void gx_write_memory_32(unsigned int address, unsigned int data)
{
	address &= 0xffffff;

	if (address >= 0x100000 && address <= 0x10ffff)
	{
		address &= 0xffff;
		mem_writelong_swap((unsigned int *)(workram+address), data);
		return;
	}
}

static void gx_timer(int refcon)
{
	m68k_set_irq(M68K_IRQ_2);	// IRQ2 on timer

	timer_set(GX_TIMER_RATE, 0, gx_timer);
}

static void GX_Init(long srate)
{
#if 0
	FILE *f;

	f = fopen("gx.bin", "wb");
	fwrite(prgrom, 512*1024, 1, f);
	fclose(f);
#endif
	tms57002_init();

	timer_set(GX_TIMER_RATE, 0, gx_timer);
}
static void GX_SendCmd(int cmda, int cmdb)
{
	commram[0xb] = cmda>>24;
	commram[0xa] = cmda>>16;
	commram[0x9] = cmda>>8;
	commram[0x8] = cmda&0xff;

	m68k_set_irq(M68K_IRQ_1);
}

// ============================================================================================================

static unsigned int hornet_k056800_r(unsigned int address)
{
	if (address & 1)
	{
		int reg = (address >> 1) & 7;
		if (reg < 4)
		{
			return k056800_host_to_snd[reg];
		}
	}
	return 0;
}

static void hornet_k056800_w(unsigned int address, unsigned int data)
{
	if (address & 1)
	{
		int reg = (address >> 1) & 7;
		switch (reg)
		{
			case 0:
			case 1:
				k056800_snd_to_host[reg] = data;
				break;
			case 4:
				h_irq2_enable = (data & 1);
				if (h_irq2_enable)
				{
					if (h_irq2_pending)
					{
						m68k_set_irq(M68K_IRQ_2);
					}
				}
				else
				{
					h_irq2_pending = 0;
					m68k_set_irq(M68K_IRQ_NONE);
				}
				break;
		}
	}
}

static unsigned int hornet_read_memory_8(unsigned int address)
{
	address &= 0xffffff;

	if (address < 0x80000)
	{
		return prgrom[address];
	}

	if (address >= 0x100000 && address <= 0x10ffff)
	{
		address &= 0xffff;
		return workram[address];
	}

	if (address >= 0x300000 && address <= 0x30001f)
	{
		return hornet_k056800_r(address);
	}

	return 0;
}

static unsigned int hornet_read_memory_16(unsigned int address)
{
	address &= 0xffffff;

	if (address < 0x80000)
	{
		return mem_readword_swap((unsigned short *)(prgrom+address));
	}

	if ((address >= 0x100000) && (address <= 0x10ffff))
	{
		address &= 0xffff;
		return mem_readword_swap((unsigned short *)(workram+address));
	}

	if (address >= 0x200000 && address <= 0x200fff)
	{
		address &= 0xfff;
		return RF5C400_0_r(address>>1, 0);
	}

	if (address >= 0x300000 && address <= 0x30001f)
	{
		return hornet_k056800_r(address | 1);
	}

	return 0;
}

static unsigned int hornet_read_memory_32(unsigned int address)
{
	address &= 0xffffff;

	if (address < 0x80000)
	{
		return mem_readlong_swap((unsigned int *)(prgrom+address));
	}

	if ((address >= 0x100000) && (address <= 0x10ffff))
	{
		address &= 0xffff;
		return mem_readlong_swap((unsigned int *)(workram+address));
	}

	return 0;
}

static void hornet_write_memory_8(unsigned int address, unsigned int data)
{
	address &= 0xffffff;

	if (address >= 0x100000 && address <= 0x10ffff)
	{
		address &= 0xffff;
		workram[address] = data;
		return;
	}

	if (address >= 0x300000 && address <= 0x30001f)
	{
		hornet_k056800_w(address, data);
		return;
	}

	if (address >= 0x480000 && address <= 0x480001) return;
	if (address >= 0x4c0000 && address <= 0x4c0001) return;

	if (address == 0x500000 || address == 0x500001)
	{
		if (address & 1)
		{
			h_irq1_enable = !(data & 1);
		}
		return;
	}

	if (address == 0x600000 || address == 0x600001)
	{
		m68k_set_irq(M68K_IRQ_NONE);
		return;
	}
}

static void hornet_write_memory_16(unsigned int address, unsigned int data)
{
	address &= 0xffffff;

	if (address >= 0x100000 && address <= 0x10ffff)
	{
		address &= 0xffff;
		mem_writeword_swap((unsigned short *)(workram+address), data);
		return;
	}

	if (address >= 0x200000 && address <= 0x200fff)
	{
		address &= 0xfff;
		RF5C400_0_w(address>>1, data, 0);
		return;
	}

	if (address >= 0x300000 && address <= 0x30001f)
	{
		hornet_k056800_w(address | 1, data & 0xff);
		return;
	}

	if (address == 0x480000) return;
	if (address == 0x4c0000) return;

	if (address == 0x500000)
	{
		h_irq1_enable = !(data & 1);
		return;
	}

	if (address == 0x600000)
	{
		m68k_set_irq(M68K_IRQ_NONE);
		return;
	}
}

static void hornet_write_memory_32(unsigned int address, unsigned int data)
{
	address &= 0xffffff;

	if (address >= 0x100000 && address <= 0x10ffff)
	{
		address &= 0xffff;
		mem_writelong_swap((unsigned int *)(workram+address), data);
		return;
	}
}

// ======================================================================================

static unsigned int gti_read_memory_8(unsigned int address)
{
	address &= 0xffffff;

	if (address < 0x80000)
	{
		return prgrom[address];
	}

	if (address >= 0x200000 && address <= 0x20ffff)
	{
		address &= 0xffff;
		return workram[address];
	}

	if (address >= 0x300000 && address <= 0x30001f)
	{
		return hornet_k056800_r(address);
	}

	return 0;
}

static unsigned int gti_read_memory_16(unsigned int address)
{
	address &= 0xffffff;

	if (address < 0x80000)
	{
		return mem_readword_swap((unsigned short *)(prgrom+address));
	}

	if ((address >= 0x200000) && (address <= 0x20ffff))
	{
		address &= 0xffff;
		return mem_readword_swap((unsigned short *)(workram+address));
	}

	if (address >= 0x400000 && address <= 0x400fff)
	{
		address &= 0xfff;
		return RF5C400_0_r(address>>1, 0);
	}

	if (address >= 0x300000 && address <= 0x30001f)
	{
		return hornet_k056800_r(address | 1);
	}

	return 0;
}

static unsigned int gti_read_memory_32(unsigned int address)
{
	address &= 0xffffff;

	if (address < 0x80000)
	{
		return mem_readlong_swap((unsigned int *)(prgrom+address));
	}

	if ((address >= 0x200000) && (address <= 0x20ffff))
	{
		address &= 0xffff;
		return mem_readlong_swap((unsigned int *)(workram+address));
	}

	return 0;
}

static void gti_write_memory_8(unsigned int address, unsigned int data)
{
	address &= 0xffffff;

	if (address >= 0x200000 && address <= 0x20ffff)
	{
		address &= 0xffff;
		workram[address] = data;
		return;
	}

	if (address >= 0x300000 && address <= 0x30001f)
	{
		hornet_k056800_w(address, data);
		return;
	}

	if (address >= 0x480000 && address <= 0x480001) return;
	if (address >= 0x4c0000 && address <= 0x4c0001) return;

	if (address == 0x500000 || address == 0x500001)
	{
		if (address & 1)
		{
			h_irq1_enable = !(data & 1);
		}
		return;
	}

	if (address == 0x600000 || address == 0x600001)
	{
		m68k_set_irq(M68K_IRQ_NONE);
		return;
	}
}

static void gti_write_memory_16(unsigned int address, unsigned int data)
{
	address &= 0xffffff;

	if (address >= 0x200000 && address <= 0x20ffff)
	{
		address &= 0xffff;
		mem_writeword_swap((unsigned short *)(workram+address), data);
		return;
	}

	if (address >= 0x400000 && address <= 0x400fff)
	{
		address &= 0xfff;
		RF5C400_0_w(address>>1, data, 0);
		return;
	}

	if (address >= 0x300000 && address <= 0x30001f)
	{
		hornet_k056800_w(address | 1, data & 0xff);
		return;
	}

	if (address == 0x480000) return;
	if (address == 0x4c0000) return;

	if (address == 0x500000)
	{
		h_irq1_enable = !(data & 1);
		return;
	}

	if (address == 0x600000)
	{
		m68k_set_irq(M68K_IRQ_NONE);
		return;
	}
}

static void gti_write_memory_32(unsigned int address, unsigned int data)
{
	address &= 0xffffff;

	if (address >= 0x200000 && address <= 0x20ffff)
	{
		address &= 0xffff;
		mem_writelong_swap((unsigned int *)(workram+address), data);
		return;
	}
}

static void hornet_timer(int refcon)
{
	if (h_irq1_enable)
	{
		m68k_set_irq(M68K_IRQ_1);
	}

	timer_set(TIME_IN_MSEC(HORNET_TIMER_RATE), 0, hornet_timer);
}

static void Hornet_Init(long srate)
{
	h_irq1_enable = 0;
	h_irq2_enable = 0;
	h_irq2_pending = 0;
	memset(k056800_host_to_snd, 0, sizeof(k056800_host_to_snd));
	memset(k056800_snd_to_host, 0, sizeof(k056800_snd_to_host));
	timer_set(TIME_IN_MSEC(HORNET_TIMER_RATE), 0, hornet_timer);
}

static void Hornet_SendCmd(int cmda, int cmdb)
{
	if (!cmda)
	{
		k056800_host_to_snd[0] = 0xf7;
		k056800_host_to_snd[1] = 0;
		k056800_host_to_snd[2] = 0;
		k056800_host_to_snd[3] = 0;
	}
	else
	{
		k056800_host_to_snd[0] = cmda >> 8;
		k056800_host_to_snd[1] = cmda & 0xff;
		k056800_host_to_snd[2] = 0;
		k056800_host_to_snd[3] = 0;
	}

	h_irq2_pending = 1;
	if (h_irq2_enable)
	{
		m68k_set_irq(M68K_IRQ_2);
	}
}

