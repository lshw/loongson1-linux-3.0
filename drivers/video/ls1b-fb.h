#ifndef _LS1B_FB_H
#define _LS1B_FB_H

struct fb_var_screeninfo LS1B_480x272_16 __initdata = {
	.xres =		480,
	.yres =		272,
	.xres_virtual =	480,
	.yres_virtual =	272,
	.bits_per_pixel = 16,
	.red =		{ 11, 5 ,0},
	.green =	{ 5, 6, 0 },
	.blue =	{ 0, 5, 0 },
	.activate =	FB_ACTIVATE_NOW,
	.height =	-1,
	.width =	-1,
	.pixclock =	10000,
	.left_margin =	64,
	.right_margin =	64,
	.upper_margin =	32,
	.lower_margin =	32,
	.hsync_len =	64,
	.vsync_len =	2,
	.vmode =	FB_VMODE_NONINTERLACED,
};

struct fb_var_screeninfo LS1B_800x480_16 __initdata = {
	.xres =		800,
	.yres =		480,
	.xres_virtual =	800,
	.yres_virtual =	480,
	.bits_per_pixel = 16,
	.red =		{ 11, 5 ,0},
	.green =	{ 5, 6, 0 },
	.blue =	{ 0, 5, 0 },
	.activate =	FB_ACTIVATE_NOW,
	.height =	-1,
	.width =	-1,
	.pixclock =	20000,
	.left_margin =	64,
	.right_margin =	64,
	.upper_margin =	32,
	.lower_margin =	32,
	.hsync_len =	64,
	.vsync_len =	2,
	.vmode =	FB_VMODE_NONINTERLACED,
};

#endif /* _VIDEO_MACMODES_H */
