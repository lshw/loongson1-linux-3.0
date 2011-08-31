/*
 *  linux/drivers/video/sb2f_fb.c -- Virtual frame buffer device
 *
 *      Copyright (C) 2002 James Simmons
 *
 *	Copyright (C) 1997 Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <ls1b_board.h>

#undef DEBUG
//#define DEBUG
//#define DEFAULT_VIDEO_MODE "1024x768-16@60"
#define DEFAULT_VIDEO_MODE "320x240-16@60"

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

static char *mode_option  = DEFAULT_VIDEO_MODE;
static void *videomemory;
static	dma_addr_t dma_A;
static u_long videomemorysize = 0;
module_param(videomemorysize, ulong, 0);

struct vga_struc{
	long pclk;
	int hr,hss,hse,hfl;
	int vr,vss,vse,vfl;
}

vgamode[] =
{
	{/*"640x480_70.00"*/    14280,  320,    332,    364,    432,    240,    248,    254,    276,},//THF
	{/*"640x480_70.00"*/    28560,  640,    664,    728,    816,    480,    481,    484,    500,},
	{/*"640x640_60.00"*/	33100,	640,	672,	736,	832,	640,	641,	644,	663,	},
	{/*"640x768_60.00"*/	39690,	640,	672,	736,	832,	768,	769,	772,	795,	},
	{/*"640x800_60.00"*/	42130,	640,	680,	744,	848,	800,	801,	804,	828,	},
	{/*"800x480_70.00"*/ 35840,  800,    832,    912,    1024,   480,    481,    484,    500,    },
	{/*"800x600_60.00"*/	38220,	800,	832,	912,	1024,	600,	601,	604,	622,	},
	{/*"800x640_60.00"*/	40730,	800,	832,	912,	1024,	640,	641,	644,	663,	},
	{/*"832x600_60.00"*/	40010,	832,	864,	952,	1072,	600,	601,	604,	622,	},
	{/*"832x608_60.00"*/	40520,	832,	864,	952,	1072,	608,	609,	612,	630,	},
	{/*"1024x480_60.00"*/	38170,	1024,	1048,	1152,	1280,	480,	481,	484,	497,	},
	{/*"1024x600_60.00"*/	48960,	1024,	1064,	1168,	1312,	600,	601,	604,	622,	},
	{/*"1024x640_60.00"*/	52830,	1024,	1072,	1176,	1328,	640,	641,	644,	663,	},
	{/*"1024x768_60.00"*/	64110,	1024,	1080,	1184,	1344,	768,	769,	772,	795,	},
	{/*"1152x764_60.00"*/   71380,  1152,   1208,   1328,   1504,   764,    765,    768,    791,    },
	{/*"1280x800_60.00"*/   83460,  1280,   1344,   1480,   1680,   800,    801,    804,    828,    },
	{/*"1280x1024_55.00"*/  98600,  1280,   1352,   1488,   1696,   1024,   1025,   1028,   1057,   },
	{/*"1440x800_60.00"*/   93800,  1440,   1512,   1664,   1888,   800,    801,    804,    828,    },
	{/*"1440x900_67.00"*/   120280, 1440,   1528,   1680,   1920,   900,    901,    904,    935,    },
};

//fb_var_screeninfo结构体主要记录用户可以修改的(var)控制器的参数，比如屏幕的分辨率和每个像素的比特数等
static struct fb_var_screeninfo ls1b_lcd_default __initdata = {
	.xres =		320,
	.yres =		240,
	.xres_virtual =	320,
	.yres_virtual =	240,
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

//fb_fix_screeninfo结构体又主要记录用户不可以修改的控制器的参数，比如屏幕缓冲区的物理地址和长度等
static struct fb_fix_screeninfo ls1b_lcd_fix __initdata = {
	.id			=	"Virtual FB",				//识别符
	.type		=	FB_TYPE_PACKED_PIXELS,	//显示类型
	.visual	=	FB_VISUAL_TRUECOLOR,		//显示的颜色类型
	.xpanstep	=	1,
	.ypanstep	=	1,
	.ywrapstep	=	1,
	.accel		=	FB_ACCEL_NONE,			//加速选项
};

static int ls1b_lcd_enable __initdata = 0;	/* disabled by default */
module_param(ls1b_lcd_enable, bool, 0);

static int ls1b_lcd_check_var(struct fb_var_screeninfo *var, struct fb_info *info);
static int ls1b_lcd_set_par(struct fb_info *info);
static int ls1b_lcd_setcolreg(u_int regno, u_int red, u_int green, u_int blue, u_int transp, struct fb_info *info);
static int ls1b_lcd_pan_display(struct fb_var_screeninfo *var, struct fb_info *info);

static struct fb_ops ls1b_lcd_ops = {
	.fb_check_var		= ls1b_lcd_check_var,		/*检测可变参数，并调整到支持的值*/
	.fb_set_par		= ls1b_lcd_set_par,		/*设置fb_info中的参数，主要是LCD的显示模式*/
	.fb_setcolreg		= ls1b_lcd_setcolreg,		/*设置颜色表*/
	.fb_pan_display	= ls1b_lcd_pan_display,
	/*以下三个函数是可选的，主要是提供fb_console的支持，在内核中已经实现，这里直接调用即可*/
	.fb_fillrect		= cfb_fillrect,
	.fb_copyarea		= cfb_copyarea,
	.fb_imageblit		= cfb_imageblit,
};

/*
*  Internal routines
*/
/*设置fb_info中固定参数中一行的字节数，公式：1行字节数=(1行像素个数*每像素位数BPP)/8 */
static u_long get_line_length(int xres_virtual, int bpp)
{
	u_long length;

	length = xres_virtual * bpp;
	length = (length + 31) & ~31;
	length >>= 3;
	return (length);
}

/*
 *  Setting the video mode has been split into two parts.
 *  First part, xxxfb_check_var, must not write anything
 *  to hardware, it should only verify and adjust var.
 *  This means it doesn't alter par but it does use hardware
 *  data from it to check this var. 
 */
/*检查fb_info中的可变参数*/
static int ls1b_lcd_check_var(struct fb_var_screeninfo *var,
			 struct fb_info *info)
{
	u_long line_length;

	/*
	 *  FB_VMODE_CONUPDATE and FB_VMODE_SMOOTH_XPAN are equal!
	 *  as FB_VMODE_SMOOTH_XPAN is only used internally
	 */

	if (var->vmode & FB_VMODE_CONUPDATE) {
		var->vmode |= FB_VMODE_YWRAP;
		var->xoffset = info->var.xoffset;
		var->yoffset = info->var.yoffset;
	}

	/*
	 *  Some very basic checks
	 */
	if (!var->xres)
		var->xres = 1;
	if (!var->yres)
		var->yres = 1;
	if (var->xres > var->xres_virtual)
		var->xres_virtual = var->xres;
	if (var->yres > var->yres_virtual)
		var->yres_virtual = var->yres;
	if (var->bits_per_pixel <= 1)
		var->bits_per_pixel = 1;
	else if (var->bits_per_pixel <= 8)
		var->bits_per_pixel = 8;
	else if (var->bits_per_pixel <= 16)
		var->bits_per_pixel = 16;
	else if (var->bits_per_pixel <= 24)
		var->bits_per_pixel = 24;
	else if (var->bits_per_pixel <= 32)
		var->bits_per_pixel = 32;
	else
		return -EINVAL;

	if (var->xres_virtual < var->xoffset + var->xres)
		var->xres_virtual = var->xoffset + var->xres;
	if (var->yres_virtual < var->yoffset + var->yres)
		var->yres_virtual = var->yoffset + var->yres;

	/*
	 *  Memory limit
	 */
	line_length =
	    get_line_length(var->xres_virtual, var->bits_per_pixel);
	if (videomemorysize &&  line_length * var->yres_virtual > videomemorysize)
		return -ENOMEM;

	/*
	 * Now that we checked it we alter var. The reason being is that the video
	 * mode passed in might not work but slight changes to it might make it 
	 * work. This way we let the user know what is acceptable.
	 */
	/*根据色位模式(BPP)来设置可变参数中R、G、B的颜色位域。对于这些参数值的设置请参考CPU数据
    手册中"显示缓冲区与显示点对应关系图"，例如在上一篇章中我就画出了8BPP和16BPP时的对应关系图*/
	switch (var->bits_per_pixel) {
	case 1:
	case 8:
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 0;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 16:		/* RGBA 5551 */
		if (var->transp.length) {
			var->red.offset = 0;
			var->red.length = 5;
			var->green.offset = 5;
			var->green.length = 5;
			var->blue.offset = 10;
			var->blue.length = 5;
			var->transp.offset = 15;
			var->transp.length = 1;
		} else {	/* BGR 565 */
			var->red.offset = 11;
			var->red.length = 5;
			var->green.offset = 5;
			var->green.length = 6;
			var->blue.offset = 0;
			var->blue.length = 5;
			var->transp.offset = 0;
			var->transp.length = 0;
		}
		break;
	case 24:		/* RGB 888 */
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 16;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 32:		/* RGBA 8888 */
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 16;
		var->blue.length = 8;
		var->transp.offset = 24;
		var->transp.length = 8;
		break;
	}
	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;

	return 0;
}

//如果需要调用implicit declaration of function()函数 则取消屏蔽
#if 0
static int config_ls1blcd_controller(void)
{
	int i,mode=-1;
	
	//根据定义的FB_XSIZE和FB_YSIZE 寻找合适的mode(分辨率)
	for(i=0; i<sizeof(vgamode)/sizeof(struct vga_struc); i++)
	{
		if(vgamode[i].hr == ls1b_lcd_default.xres && vgamode[i].vr == ls1b_lcd_default.yres){
			mode = i;
			break;
		}
	}
	if(mode<0)
	{
		printk(KERN_DEBUG "\n\n\nunsupported framebuffer resolution\n\n\n");
		return -EINVAL;
	}
	
	SB_FB_BUF_CONFIG_REG(0)			= 0x00000000;
	SB_FB_BUF_CONFIG_REG(0)			= 0x00000003;
//	SB_FB_BUF_ADDRESS_REG(0)			= dma_A;
//	SB_FB_BUF_ADDRESS_REG1(0)		= dma_A;
//	*(volatile int *)0xbc301580		= dma_A;
	SB_FB_DITHER_REG(0)				= 0x00000000;
	SB_FB_DITHER_TABLE_REG_LOW(0)	= 0x00000000;	//low
	SB_FB_DITHER_TABLE_REG_HIG(0)	= 0x00000000;	//high
	SB_FB_PANEL_CONFIG_REG(0)		= 0x80000102;
	SB_FB_PANEL_TIMING_REG(0)		= 0x00000000;
	
	SB_FB_HDISP_REG(0)				= (vgamode[mode].hfl<<16) | vgamode[mode].hr;
	SB_FB_HSYNC_REG(0)				= 0x40000000 | (vgamode[mode].hse<<16) | vgamode[mode].hss;
	SB_FB_VDISP_REG(0)				= (vgamode[mode].vfl<<16) | vgamode[mode].vr;
	SB_FB_VSYNC_REG(0)				= 0x40000000 | (vgamode[mode].vse<<16) | vgamode[mode].vss;
	
	SB_FB_BUF_CONFIG_REG(0)			= 0x00100103;
	SB_FB_BUF_STRIDE_REG(0)			= (ls1b_lcd_default.xres * 2 + 255) & ~255;
	SB_FB_BUF_ORIGIN_REG(0)			= 0x00000000;
	
	SB_FB_BUF_CONFIG_REG(0)			= 0x00100113;
	mdelay(20);
	SB_FB_BUF_CONFIG_REG(0)			= 0x00100103;
}
#endif

/* This routine actually sets the video mode. It's in here where we
 * the hardware state info->par and fix which can be affected by the 
 * change in par. For this driver it doesn't do much. 
 */
/*设置fb_info中的参数，这里根据用户设置的可变参数var调整固定参数fix*/
static int ls1b_lcd_set_par(struct fb_info *info)
{
	/*设置fb_info中固定参数中一行的字节数，公式：1行字节数=(1行像素个数*每像素位数BPP)/8 */
	info->fix.line_length = get_line_length(info->var.xres_virtual, info->var.bits_per_pixel);
	return 0;
}

/*
 *  Set a single color register. The values supplied are already
 *  rounded down to the hardware's capabilities (according to the
 *  entries in the var structure). Return != 0 for invalid regno.
 */
/*设置颜色表*/
static int ls1b_lcd_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			 u_int transp, struct fb_info *info)
{
	if (regno >= 256)	/* no. of hw registers */
		return 1;
	/*
	 * Program hardware... do anything you want with transp
	 */

	/* grayscale works only partially under directcolor */
	if (info->var.grayscale) {
		/* grayscale = 0.30*R + 0.59*G + 0.11*B */
		red = green = blue = (red * 77 + green * 151 + blue * 28) >> 8;
	}

	/* Directcolor:
	 *   var->{color}.offset contains start of bitfield
	 *   var->{color}.length contains length of bitfield
	 *   {hardwarespecific} contains width of RAMDAC
	 *   cmap[X] is programmed to (X << red.offset) | (X << green.offset) | (X << blue.offset)
	 *   RAMDAC[X] is programmed to (red, green, blue)
	 * 
	 * Pseudocolor:
	 *    uses offset = 0 && length = RAMDAC register width.
	 *    var->{color}.offset is 0
	 *    var->{color}.length contains widht of DAC
	 *    cmap is not used
	 *    RAMDAC[X] is programmed to (red, green, blue)
	 * Truecolor:
	 *    does not use DAC. Usually 3 are present.
	 *    var->{color}.offset contains start of bitfield
	 *    var->{color}.length contains length of bitfield
	 *    cmap is programmed to (red << red.offset) | (green << green.offset) |
	 *                      (blue << blue.offset) | (transp << transp.offset)
	 *    RAMDAC does not exist
	 */
#define CNVT_TOHW(val,width) ((((val)<<(width))+0x7FFF-(val))>>16)
	switch (info->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
	case FB_VISUAL_PSEUDOCOLOR:
		red = CNVT_TOHW(red, info->var.red.length);
		green = CNVT_TOHW(green, info->var.green.length);
		blue = CNVT_TOHW(blue, info->var.blue.length);
		transp = CNVT_TOHW(transp, info->var.transp.length);
		break;
	case FB_VISUAL_DIRECTCOLOR:
		red = CNVT_TOHW(red, 8);	/* expect 8 bit DAC */
		green = CNVT_TOHW(green, 8);
		blue = CNVT_TOHW(blue, 8);
		/* hey, there is bug in transp handling... */
		transp = CNVT_TOHW(transp, 8);
		break;
	}
#undef CNVT_TOHW
	/* Truecolor has hardware independent palette */
	if (info->fix.visual == FB_VISUAL_TRUECOLOR) {
		u32 v;

		if (regno >= 16)
			return 1;

		v = (red << info->var.red.offset) |
		    (green << info->var.green.offset) |
		    (blue << info->var.blue.offset) |
		    (transp << info->var.transp.offset);
		switch (info->var.bits_per_pixel) {
		case 8:
			break;
		case 16:
			((u32 *) (info->pseudo_palette))[regno] = v;
			break;
		case 24:
		case 32:
			((u32 *) (info->pseudo_palette))[regno] = v;
			break;
		}
		return 0;
	}
	return 0;
}

/*
 *  Pan or Wrap the Display
 *
 *  This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
 */
static int ls1b_lcd_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	if (var->vmode & FB_VMODE_YWRAP) {
		if (var->yoffset < 0
		    || var->yoffset >= info->var.yres_virtual
		    || var->xoffset)
			return -EINVAL;
	} else {
		if (var->xoffset + var->xres > info->var.xres_virtual ||
		    var->yoffset + var->yres > info->var.yres_virtual)
			return -EINVAL;
	}
	info->var.xoffset = var->xoffset;
	info->var.yoffset = var->yoffset;
	if (var->vmode & FB_VMODE_YWRAP)
		info->var.vmode |= FB_VMODE_YWRAP;
	else
		info->var.vmode &= ~FB_VMODE_YWRAP;
	return 0;
}

#ifndef MODULE
static int __init ls1b_lcd_setup(char *options)
{
	char *this_opt;

	ls1b_lcd_enable = 1;

	if (!options || !*options)
		return 1;

	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!*this_opt)
			continue;
		if (!strncmp(this_opt, "disable", 7))
			ls1b_lcd_enable = 0;
		else
			mode_option = this_opt;
	}
	return 1;
}
#endif  /*  MODULE  */



/*
*  Initialisation
*/
static int __init ls1b_lcd_probe(struct platform_device *dev)
{
	struct fb_info *info;		/*FrameBuffer驱动所对应的fb_info结构体*/
	int retval = -ENOMEM;
	struct fb_var_screeninfo var;

	var = ls1b_lcd_default;

	/*给fb_info分配空间，大小为256的内存，framebuffer_alloc定义在fb.h中在fbsysfs.c中实现*/
	info = framebuffer_alloc(sizeof(u32) * 256, &dev->dev);
	if (!info)
		return retval;

	info->fix = ls1b_lcd_fix;				//固定参数
	info->node = -1;
	info->fbops = &ls1b_lcd_ops;			//fb_ops帧缓冲操作
	info->pseudo_palette = info->par;	//伪16色颜色表
	info->par = NULL;
	info->flags = FBINFO_FLAG_DEFAULT;
	var = info->var = ls1b_lcd_default;	//可变参数结构体 当前var

	if(mode_option)
		retval = fb_find_mode(&info->var, info, mode_option, NULL, 0, NULL, 32);

	//计算视频缓冲区大小？
    if (!videomemorysize) {
    	videomemorysize = info->var.xres_virtual *
			  info->var.yres_virtual *
			  info->var.bits_per_pixel / 8;
    }

	/*
	 * For real video cards we use ioremap.
	 */
	videomemory = dma_alloc_coherent(&dev->dev,videomemorysize+PAGE_SIZE, &dma_A,GFP_ATOMIC); //use only bank A
	if (!videomemory)
		goto err;
	memset(videomemory, 0, videomemorysize);
#ifdef DEBUG
	printk(KERN_DEBUG "videomemorysize = %lx \n", videomemorysize);
//	prom_printf("%x::%x\n",videomemory,dma_A);
#endif

	info->screen_base = (char __iomem *)videomemory;
	info->fix.smem_start = dma_A;
	info->fix.smem_len = videomemorysize;

	retval = fb_alloc_cmap(&info->cmap, 32, 0);
	if (retval < 0)
		goto err1;
	
	//如果PMON 已对LCD控制器进行了初始化 则这里不需要执行config_ls1blcd_controller()函数
//	config_ls1blcd_controller();

	SB_FB_BUF_ADDRESS_REG(0)=dma_A;
//	SB_FB_BUF_ADDRESS_REG(1)=dma_A;		//loongson 1A LCD(VGA)控制器
	/*set double flip buffer address*/
	*(volatile int *)0xbc301580 = dma_A;
//	*(volatile int *)0xbc301590 = dma_A;	//loongson 1A LCD(VGA)控制器
	/*disable fb0,board only use fb1 now*/
#ifdef CONFIG_MACH_SB2F
	SB_FB_BUF_CONFIG_REG(0) &= ~0x100;
#endif
	
	//检查可变参数
	ls1b_lcd_check_var(&var,info);
	//注册fb_info
	retval = register_framebuffer(info);
	if (retval < 0)
		goto err2;
	platform_set_drvdata(dev, info);

#ifdef DEBUG
	printk(KERN_INFO
	       "fb%d: Virtual frame buffer device, using %ldK of video memory\n",
	       info->node, videomemorysize >> 10);
	printk(KERN_DEBUG "ls1b_lcd Initialization Complete\n");
#endif
	return 0;
err2:
	fb_dealloc_cmap(&info->cmap);
err1:
	dma_free_coherent(&dev->dev,videomemorysize+PAGE_SIZE,videomemory,dma_A);
err:
	framebuffer_release(info);
	return retval;
}

static int ls1b_lcd_remove(struct platform_device *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);

	if (info) {
		unregister_framebuffer(info);
		dma_free_coherent(&dev->dev,videomemorysize+PAGE_SIZE,videomemory,dma_A);
		framebuffer_release(info);
	}
	return 0;
}

static struct platform_driver ls1b_lcd_driver = {
	.probe		= ls1b_lcd_probe,
	.remove	= ls1b_lcd_remove,
	.driver	= {
		.name	= "ls1b-fb",
		.owner = THIS_MODULE,
	},
};

//static struct platform_device *ls1b_lcd_device;
static int __init ls1b_lcd_init(void)
{
	int ret = 0;
	char *option = NULL;

#ifdef DEBUG
	printk(KERN_DEBUG "ls1b_lcd Initialization\n");
#endif
#ifndef MODULE
	if (fb_get_options("ls1b_lcd", &option))
		return -ENODEV;
	ls1b_lcd_setup(option);
#endif

	if (!ls1b_lcd_enable)
		return -ENXIO;

	ret = platform_driver_register(&ls1b_lcd_driver);

	return ret;
}
module_init(ls1b_lcd_init);

#ifdef MODULE
static void __exit ls1b_lcd_exit(void)
{
	platform_driver_unregister(&ls1b_lcd_driver);
}
module_exit(ls1b_lcd_exit);

MODULE_DESCRIPTION("loongson 1B LCD Driver");
MODULE_AUTHOR("loongson");
MODULE_LICENSE("GPL");
#endif				/* MODULE */
