/*
 *  Copyright (c) 2012 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef __ASM_MACH_LS1XFB_H
#define __ASM_MACH_LS1XFB_H

#include <ls1b_board.h>
#include <linux/fb.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

/*
 * Buffer pixel format
 */
#define PIX_FMT_RGB444		1
#define PIX_FMT_RGB1555		2
#define PIX_FMT_RGB565		3
#define PIX_FMT_RGB888PACK	4
#define PIX_FMT_RGB888UNPACK	5
#define PIX_FMT_RGBA888		6
#define PIX_FMT_PSEUDOCOLOR	20

#define DEFAULT_FB_SIZE	(800 * 600 * 4)

/*
 * LS1X LCD controller private state.
 */
struct ls1xfb_info;

struct ls1xfb_i2c_chan {
	struct ls1xfb_info *par;
	struct i2c_adapter adapter;
	struct i2c_algo_bit_data algo;
	unsigned int sda_pin;
	unsigned int scl_pin;
};

struct ls1xfb_info {
	struct device		*dev;
	struct clk		*clk;
	struct fb_info		*info;

	struct ls1xfb_i2c_chan chan;
	unsigned char *edid;

	void __iomem		*reg_base;
	dma_addr_t		fb_start_dma;
	u32			pseudo_palette[16];

	int			pix_fmt;
	unsigned		active:1;
};

/*
 * LS1X fb machine information
 */
struct ls1xfb_mach_info {
	char	id[16];

	int		num_modes;
	struct fb_videomode *modes;

	/*
	 * Pix_fmt
	 */
	unsigned	pix_fmt;

	/*
	 * Dumb panel -- configurable output signal polarity.
	 */
	unsigned	invert_pixclock:1;
	unsigned	invert_pixde:1;
	unsigned	active:1;
	unsigned	enable_lcd:1;
};

#ifdef CONFIG_LS1B_MACH
struct ls1b_vga {
	unsigned int xres;
	unsigned int yres;
	unsigned int refresh;
	unsigned int ls1b_pll_freq;
	unsigned int ls1b_pll_div;
};
#endif

#if defined(CONFIG_FB_LS1X_I2C)
extern int ls1xfb_probe_i2c_connector(struct fb_info *info, u8 **out_edid);
extern void ls1xfb_create_i2c_busses(struct fb_info *info);
extern void ls1xfb_delete_i2c_busses(struct fb_info *info);
#endif

#endif /* __ASM_MACH_LS1XFB_H */
