#ifndef __ASM_LS1B_FB_H
#define __ASM_LS1B_FB_H

#define TYPE_LCD	0
#define TYPE_VGA	1

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
	unsigned int pclk;
	
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

static struct vga_struc{
	unsigned int pclk;
	int hr,hss,hse,hfl;
	int vr,vss,vse,vfl;
	int pan_config;
	int pll_reg0, pll_reg1;
}

vgamode[] = {
	{/*"320x240_70.00"*/	4000,	320,	332,	364,	432,	240,	248,	254,	276,	0x00000103},/* HX8238-D控制器 */
//	{/*"320x240_70.00"*/	4000,	320,	336,	337,	408,	240,	250,	251,	263,	0x80001311},/* NT39016D控制器 */
	{/*"480x272_70.00"*/	6500,	480,	481,	482,	525,	272,	273,	274,	288,	0x00000101},/**/
	{/*"640x480_70.00"*/    14280,  320,    332,    364,    432,    240,    248,    254,    276,	0x00000101},/**/
	{/*"640x480_70.00"*/    28560,  640,    664,    728,    816,    480,    481,    484,    500,	0x00000101},
	{/*"640x640_60.00"*/	33100,	640,	672,	736,	832,	640,	641,	644,	663,	0x00000101},
	{/*"640x768_60.00"*/	39690,	640,	672,	736,	832,	768,	769,	772,	795,	0x00000101},
	{/*"640x800_60.00"*/	42130,	640,	680,	744,	848,	800,	801,	804,	828,	0x00000101},
	{/*"800x480_70.00"*/	35840,  800,    832,    912,    1024,   480,    481,    484,    500,	0x00000101},
	{/*"800x600_75.00"*/	49500,	800,	816,	896,	1056,	600,	601,	604,	625,	0x80001311, 0x0080c, 0x8a28ea00},/*******/
	{/*"800x640_60.00"*/	40730,	800,	832,	912,	1024,	640,	641,	644,	663,	0x00000101},
	{/*"832x600_60.00"*/	40010,	832,	864,	952,	1072,	600,	601,	604,	622,	0x00000101},
	{/*"832x608_60.00"*/	40520,	832,	864,	952,	1072,	608,	609,	612,	630,	0x00000101},
	{/*"1024x480_60.00"*/	38170,	1024,	1048,	1152,	1280,	480,	481,	484,	497,	0x00000101},
	{/*"1024x600_60.00"*/	48960,	1024,	1064,	1168,	1312,	600,	601,	604,	622,	0x00000101},
	{/*"1024x640_60.00"*/	52830,	1024,	1072,	1176,	1328,	640,	641,	644,	663,	0x00000101},
	{/*"1024x768_60.00"*/	65000,	1024,	1048,	1184,	1344,	768,	771,	777,	806,	0x80001311, 0x21813, 0x8a28ea00},/*******/
	{/*"1152x764_60.00"*/   71380,  1152,   1208,   1328,   1504,   764,    765,    768,    791,    0x00000101},
	{/*"1280x800_60.00"*/   83460,  1280,   1344,   1480,   1680,   800,    801,    804,    828,    0x00000101},
//	{/*"1280x1024_60.00"*/	10888,	1280,	1328,	1440,	1688,	1024,	1025,   1028,   1066,	0x80001311, 0x0080e, 0x8628ea00},/*******/
	{/*"1280x1024_75.00"*/	135000,	1280,	1296,	1440,	1688,	1024,	1025,   1028,   1066,	0x80001311, 0x3af14, 0x8628ea00},/*******/
	{/*"1440x800_60.00"*/   93800,  1440,   1512,   1664,   1888,   800,    801,    804,    828,    0x80001311},
//	{/*"1440x900_60.00"*/	120280,	1440,	1520,	1672,	1904,	900,    903,    909,    934,	0x80001311, 0x0080e, 0x8628ea00},/*******/
	{/*"1440x900_75.00"*/	136750,	1440,	1536,	1688,	1936,	900,    903,    909,    942,	0x80001311, 0x3af14, 0x8628ea00},/*******/
};

static struct ls1bfb_mach_info LS1B_default_mach __initdata = {
	.regs	= {
		.fb_conf		= 0x00100003,
		.fb_stride		= 0x00000000,
		.fb_origin		= 0x00000000,
		.panel_conf		= 0x00000101,
		.hdisplay		= (525 << 16) | 480,
		.hsync			= 0x40000000 | (482 <<16) | 481,
		.vdisplay		= (288 << 16) | 272,
		.vsync			= 0x40000000 | (274 <<16) | 273,
	},
	.width		= 480,
	.height		= 272,
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

#endif /* __ASM_LS1B_FB_H */
