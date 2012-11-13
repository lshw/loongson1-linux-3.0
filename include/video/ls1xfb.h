#ifndef __ASM_MACH_LS1XFB_H
#define __ASM_MACH_LS1XFB_H

#include <ls1b_board.h>
#include <linux/fb.h>

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
struct ls1xfb_info {
	struct device		*dev;
	struct clk		*clk;
	struct fb_info		*info;

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

#endif /* __ASM_MACH_LS1XFB_H */
