#ifndef __ASM_LS1B_FB_H
#define __ASM_LS1B_FB_H

struct ls1bfb_val {
	unsigned int	defval;
	unsigned int	min;
	unsigned int	max;
};

struct ls1bfb_hw {
	unsigned int fb_conf;
	unsigned int fb_stride;
	unsigned int fb_origin;
	unsigned int panel_conf;
	unsigned int hdisplay;
	unsigned int hsync;
	unsigned int vdisplay;
	unsigned int vsync;
};

struct ls1bfb_mach_info {
	unsigned char	fixed_syncs;	/* do not update sync/border */

	/* LCD types */
	int		type;

	/* Screen size */
	int		width;
	int		height;

	/* Screen info */
	struct ls1bfb_val xres;
	struct ls1bfb_val yres;
	struct ls1bfb_val bpp;

	/* lcd configuration registers */
	struct ls1bfb_hw  regs;
	unsigned int pclk;	//频率hz
	
	/* GPIOs */

	unsigned long	gpcup;
	unsigned long	gpcup_mask;
	unsigned long	gpccon;
	unsigned long	gpccon_mask;
	unsigned long	gpdup;
	unsigned long	gpdup_mask;
	unsigned long	gpdcon;
	unsigned long	gpdcon_mask;

	/* lpc3600 control register */
	unsigned long	lpcsel;
};

extern void __init ls1bfb_set_platdata(struct ls1bfb_mach_info *);

static struct ls1bfb_mach_info LS1B_320x240 __initdata = {
	.regs	= {
		.fb_conf		= 0x00100003,
		.fb_stride		= 0x00000000,
		.fb_origin		= 0x00000000,
		.panel_conf	= 0x00000101,
		.hdisplay		= (432 << 16) | 320,
		.hsync			= 0x40000000 | (364 <<16) | 332,
		.vdisplay		= (276 << 16) | 240,
		.vsync			= 0x40000000 | (254 <<16) | 248,
	},
	.width		= 320,
	.height	= 240,
	.xres		= {
		.min	= 320,
		.max	= 320,
		.defval	= 320,
	},
	.yres		= {
		.min	= 240,
		.max	= 240,
		.defval = 240,
	},
	.bpp		= {
		.min	= 16,
		.max	= 32,
		.defval = 16,
	},
};

static struct ls1bfb_mach_info LS1B_480x272 __initdata = {
	.regs	= {
		.fb_conf		= 0x00100003,
		.fb_stride		= 0x00000000,
		.fb_origin		= 0x00000000,
		.panel_conf	= 0x00000101,
		.hdisplay		= (525 << 16) | 480,
		.hsync			= 0x40000000 | (482 <<16) | 481,
		.vdisplay		= (288 << 16) | 272,
		.vsync			= 0x40000000 | (274 <<16) | 273,
	},
	.width		= 480,
	.height	= 272,
	.xres		= {
		.min	= 480,
		.max	= 480,
		.defval	= 480,
	},
	.yres		= {
		.min	= 272,
		.max	= 272,
		.defval = 272,
	},
	.bpp		= {
		.min	= 16,
		.max	= 32,
		.defval = 16,
	},
};

static struct ls1bfb_mach_info LS1B_640x480 __initdata = {
	.regs	= {
		.fb_conf		= 0x00100003,
		.fb_stride		= 0x00000000,
		.fb_origin		= 0x00000000,
		.panel_conf	= 0x00000101,
		.hdisplay		= (816 << 16) | 640,
		.hsync			= 0x40000000 | (728 <<16) | 664,
		.vdisplay		= (500 << 16) | 480,
		.vsync			= 0x40000000 | (484 <<16) | 481,
	},
	.width		= 640,
	.height	= 480,
	.xres		= {
		.min	= 640,
		.max	= 640,
		.defval	= 640,
	},
	.yres		= {
		.min	= 480,
		.max	= 480,
		.defval = 480,
	},
	.bpp		= {
		.min	= 16,
		.max	= 32,
		.defval = 16,
	},
};

static struct ls1bfb_mach_info LS1B_640x768 __initdata = {
	.regs	= {
		.fb_conf		= 0x00100003,
		.fb_stride		= 0x00000000,
		.fb_origin		= 0x00000000,
		.panel_conf	= 0x00000101,
		.hdisplay		= (816 << 16) | 640,
		.hsync			= 0x40000000 | (728 <<16) | 664,
		.vdisplay		= (795 << 16) | 768,
		.vsync			= 0x40000000 | (772 <<16) | 769,
	},
	.width		= 640,
	.height	= 768,
	.xres		= {
		.min	= 640,
		.max	= 640,
		.defval	= 640,
	},
	.yres		= {
		.min	= 768,
		.max	= 768,
		.defval = 768,
	},
	.bpp		= {
		.min	= 16,
		.max	= 32,
		.defval = 16,
	},
};

static struct ls1bfb_mach_info LS1B_800x600 __initdata = {
	.regs	= {
		.fb_conf		= 0x00100003,
		.fb_stride		= 0x00000000,
		.fb_origin		= 0x00000000,
		.panel_conf	= 0x00000101,
		.hdisplay		= (1024 << 16) | 800,
		.hsync			= 0x40000000 | (912 <<16) | 832,
		.vdisplay		= (622 << 16) | 600,
		.vsync			= 0x40000000 | (604 <<16) | 601,
	},
	.width		= 800,
	.height	= 600,
	.xres		= {
		.min	= 800,
		.max	= 800,
		.defval	= 800,
	},
	.yres		= {
		.min	= 600,
		.max	= 600,
		.defval = 600,
	},
	.bpp		= {
		.min	= 16,
		.max	= 32,
		.defval = 16,
	},
};

static struct ls1bfb_mach_info LS1B_1024x768 __initdata = {
	.regs	= {
		.fb_conf		= 0x00100003,
		.fb_stride		= 0x00000000,
		.fb_origin		= 0x00000000,
		.panel_conf	= 0x00000101,
		.hdisplay		= (1344 << 16) | 1024,
		.hsync			= 0x40000000 | (1184 <<16) | 1080,
		.vdisplay		= (795 << 16) | 768,
		.vsync			= 0x40000000 | (772 <<16) | 769,
	},
	.width		= 1024,
	.height	= 768,
	.xres		= {
		.min	= 1024,
		.max	= 1024,
		.defval	= 1024,
	},
	.yres		= {
		.min	= 768,
		.max	= 768,
		.defval = 768,
	},
	.bpp		= {
		.min	= 16,
		.max	= 32,
		.defval = 16,
	},
};

static struct ls1bfb_mach_info LS1B_1440x900 __initdata = {
	.regs	= {
		.fb_conf		= 0x00100003,
		.fb_stride		= 0x00000000,
		.fb_origin		= 0x00000000,
		.panel_conf	= 0x00000101,
		.hdisplay		= (1920 << 16) | 1440,
		.hsync			= 0x40000000 | (1680 <<16) | 1528,
		.vdisplay		= (935 << 16) | 900,
		.vsync			= 0x40000000 | (904 <<16) | 901,
	},
	.width		= 1440,
	.height	= 900,
	.xres		= {
		.min	= 1440,
		.max	= 1440,
		.defval	= 1440,
	},
	.yres		= {
		.min	= 900,
		.max	= 900,
		.defval = 900,
	},
	.bpp		= {
		.min	= 16,
		.max	= 32,
		.defval = 16,
	},
};

#endif /* __ASM_LS1B_FB_H */
