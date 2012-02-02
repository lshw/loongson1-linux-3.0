/*
 * linux/drivers/video/s3c2410fb.h
 * Copyright (c) Arnaud Patard
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	    S3C2410 LCD Controller Frame Buffer Driver
 *	    based on skeletonfb.c, sa1100fb.h
 *
 * ChangeLog
 *
 * 2004-12-04: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - Moved dprintk to s3c2410fb.c
 *
 * 2004-09-07: Arnaud Patard <arnaud.patard@rtp-net.org>
 * 	- Renamed from h1940fb.h to s3c2410fb.h
 * 	- Chenged h1940 to s3c2410
 *
 * 2004-07-15: Arnaud Patard <arnaud.patard@rtp-net.org>
 *	- First version
 */

#ifndef __LS1BFB_H
#define __LS1BFB_H

/* LCD Register define */
#define LS1B_LCD_ADDR KSEG1ADDR(LS1B_LCD_BASE)	//0xbc301240

#define SB_FB_BUF_CONFIG_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+n*16)				//0xbc301240
#define SB_FB_BUF_ADDRESS_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32+n*16)			//0xbc301260
#define SB_FB_BUF_STRIDE_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*2+n*16)			//0xbc301280
#define SB_FB_BUF_ORIGIN_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*3+n*16)			//0xbc3012a0

#define SB_FB_OVLY_CONFIG_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*4+n*16)		//0xbc3012c0
#define SB_FB_OVLY_ADDRESS_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*5++n*16)		//0xbc3012e0
#define SB_FB_OVLY_STRIDE_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*6++n*16)		//0xbc301300
#define SB_FB_OVLY_TOPLEFT_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*7+n*16)		//0xbc301320
#define SB_FB_OVLY_BOTRIGHT_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*8+n*16)		//0xbc301340

#define SB_FB_DITHER_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*9+n*16)				//0xbc301360
#define SB_FB_DITHER_TABLE_REG_LOW(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*10+n*16)	//0xbc301380
#define SB_FB_DITHER_TABLE_REG_HIG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*11+n*16)	//0xbc3013a0

#define SB_FB_PANEL_CONFIG_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*12+n*16)		//0xbc3013c0
#define SB_FB_PANEL_TIMING_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*13+n*16)		//0xbc3013e0

#define SB_FB_HDISP_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*14+n*16)				//0xbc301400
#define SB_FB_HSYNC_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*15+n*16)				//0xbc301420
#define SB_FB_HCNT1_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*16+n*16)				//0xbc301440
#define SB_FB_HCNT2_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*17+n*16)				//0xbc301460

#define SB_FB_VDISP_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*18+n*16)				//0xbc301480
#define SB_FB_VSYNC_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*19+n*16)				//0xbc3014a0
#define SB_FB_xxxxx_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*20+n*16)				//0xbc3014c0
#define SB_FB_GAMMA_INDEX_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*21+n*16)				//0xbc3014e0
#define SB_FB_GAMMA_DATA_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*22+n*16)				//0xbc301500

#define SB_FB_BUF_ADDRESS1_REG(n) *(volatile unsigned int *)(LS1B_LCD_ADDR+32*26+n*16)			//0xbc301580

#define PLL_FREQ_REG(x) *(volatile unsigned int *)(0xbfe78030+x)

enum{
	OF_BUF_CONFIG=0,
	OF_BUF_ADDR=0x20,
	OF_BUF_STRIDE=0x40,
	OF_BUF_ORIG=0x60,
	OF_DITHER_CONFIG=0x120,
	OF_DITHER_TABLE_LOW=0x140,
	OF_DITHER_TABLE_HIGH=0x160,
	OF_PAN_CONFIG=0x180,
	OF_PAN_TIMING=0x1a0,
	OF_HDISPLAY=0x1c0,
	OF_HSYNC=0x1e0,
	OF_VDISPLAY=0x240,
	OF_VSYNC=0x260,
	OF_DBLBUF=0x340,
};

#define	write_reg(addr, val)			*(volatile unsigned int *)(KSEG1ADDR(addr)) = (val)
#define	write_gpio_reg(addr, val)	*(volatile unsigned int *)(KSEG1ADDR(addr)) = (val)
#define	read_gpio_reg(addr)			*(volatile unsigned int *)(KSEG1ADDR(addr))

struct ls1bfb_info {
	struct fb_info		*fb;
	struct device		*dev;
	struct clk		*clk;

	struct ls1bfb_mach_info *mach_info;

	/* raw memory addresses */
	dma_addr_t		map_dma;	/* physical */
	u_char *		map_cpu;	/* virtual */
	u_int			map_size;

	struct ls1bfb_hw	regs;
	void __iomem		*regbase;

	/* addresses of pieces placed in raw buffer */
	u_char *		screen_cpu;	/* virtual address of buffer */
	dma_addr_t		screen_dma;	/* physical address of buffer */
	unsigned int	palette_ready;

	/* keep these registers in case we need to re-write palette */
	u32			palette_buffer[256];
	u32			pseudo_pal[16];
};

#define PALETTE_BUFF_CLEAR (0x80000000)	/* entry is clear/invalid */

int ls1bfb_init(void);

#endif
