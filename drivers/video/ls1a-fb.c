/*
 *loongson1B LCD driver
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <asm/uaccess.h>

#include <fb.h>
#include <ls1b_board.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#include "ls1b-fb.h"

#ifdef CONFIG_FB_LS1A
#define DC_FB 1
#else
#define DC_FB 0
#endif

static int mode_tmp = 0;
static int mode_type;

#define DEFAULT_VIDEO_MODE "480x272-16@60"
static char *mode_option = DEFAULT_VIDEO_MODE;
static struct ls1bfb_mach_info *mach_info;

/* Debugging stuff */
#ifdef CONFIG_FB_LS1B_DEBUG
static int debug	   = 1;
#else
static int debug	   = 0;
#endif

#define dprintk(msg...)	if (debug) { printk(KERN_DEBUG "ls1bfb: " msg); }

/* useful functions */

/* ls1bfb_set_lcdaddr
 *
 * initialise lcd controller address pointers
*/
static void ls1bfb_set_lcdaddr(struct ls1bfb_info *fbi)
{
	unsigned long saddr1, saddr2;

	saddr1	= fbi->fb->fix.smem_start;
	saddr2	= fbi->fb->fix.smem_start;

	dprintk("LCDSADDR1 = 0x%08lx\n", saddr1);
	dprintk("LCDSADDR2 = 0x%08lx\n", saddr2);

	SB_FB_BUF_ADDRESS_REG(DC_FB) = saddr1;
	SB_FB_BUF_ADDRESS1_REG(DC_FB) = saddr2;
//	*(volatile int *)0xbc301580 = saddr2;
}

int caclulatefreq(long XIN, long PCLK)
{
	long N=4, NO=4, OD=2, M, FRAC;
	int flag = 0;
	long  out;
	long MF;

	while (flag == 0) {
		flag = 1;
		if ((XIN/N) < 5000) {
			N--;
			flag = 0;
		}
		if ((XIN/N) > 50000) {
			N++;
			flag = 0;
		}
	}
	flag = 0;
	while (flag == 0) {
		flag = 1;
		if ((PCLK*NO) < 200000) {
			NO *= 2;
			OD++;
			flag = 0;
		}
		if ((PCLK*NO) > 700000) {
			NO /= 2;
			OD--;
			flag = 0;
		}
	}
	MF = PCLK * N * NO * 262144 / XIN;
	MF %= 262144;
	M = PCLK * N * NO / XIN;
	FRAC = (int)(MF);
	out = (FRAC<<14) + (OD<<12) + (N<<8) + M;
	return out;
}

/*
 *	ls1bfb_check_var():
 *	Get the video params out of 'var'. If a value doesn't fit, round it up,
 *	if it's too big, return -EINVAL.
 *
 */
static int ls1bfb_check_var(struct fb_var_screeninfo *var,
			       struct fb_info *info)
{
	/*��lcd_fb_probe̽�⺯�����õ�ƽ̨�������ٻ��LCD�����Ϣ������*/
	struct ls1bfb_info *fbi = info->par;

	dprintk("check_var(var=%p, info=%p)\n", var, info);

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
	if (var->bits_per_pixel <= 12){
		var->bits_per_pixel = 16;
		fbi->regs.fb_conf &= ~0x07;
		fbi->regs.fb_conf |= 0x1;
	}
	else if (var->bits_per_pixel <= 15){
		var->bits_per_pixel = 16;
		fbi->regs.fb_conf &= ~0x07;
		fbi->regs.fb_conf |= 0x2;
	}
	else if (var->bits_per_pixel <= 16){
		var->bits_per_pixel = 16;
		fbi->regs.fb_conf &= ~0x07;
		fbi->regs.fb_conf |= 0x3;
	}
	else if (var->bits_per_pixel <= 24){
		var->bits_per_pixel = 32;
		fbi->regs.fb_conf &= ~0x07;
		fbi->regs.fb_conf |= 0x4;
	}
	else if (var->bits_per_pixel <= 32){
		var->bits_per_pixel = 32;
		fbi->regs.fb_conf &= ~0x07;
		fbi->regs.fb_conf |= 0x4;
	}
	else
		return -EINVAL;

	if (var->xres_virtual < var->xoffset + var->xres)
		var->xres_virtual = var->xoffset + var->xres;
	if (var->yres_virtual < var->yoffset + var->yres)
		var->yres_virtual = var->yoffset + var->yres;

	/* validate x/y resolution */
	/*��֤X/Y������*/
//	if (var->yres > fbi->mach_info->yres.max)
//		var->yres = fbi->mach_info->yres.max;
//	else if (var->yres < fbi->mach_info->yres.min)
//		var->yres = fbi->mach_info->yres.min;

//	if (var->xres > fbi->mach_info->xres.max)
//		var->xres = fbi->mach_info->xres.max;
//	else if (var->xres < fbi->mach_info->xres.min)
//		var->xres = fbi->mach_info->xres.min;

	/* validate bpp */

//	if (var->bits_per_pixel > fbi->mach_info->bpp.max)
//		var->bits_per_pixel = fbi->mach_info->bpp.max;
//	else if (var->bits_per_pixel < fbi->mach_info->bpp.min)
//		var->bits_per_pixel = fbi->mach_info->bpp.min;

	/* set r/g/b positions */
	switch (var->bits_per_pixel) {
		case 1:
		case 2:
		case 4:
		case 8:
		case 12:
		case 15:
		default:
		case 16:
			if ((fbi->regs.fb_conf & 0x7) == 0x3) {
				/* 16 bpp, 565 format */
				dprintk("%s: var->bpp   = 565\n", __FUNCTION__);
				var->red.offset		= 11;
				var->green.offset	= 5;
				var->blue.offset	= 0;
				var->transp.offset   = 0;
				var->red.length		= 5;
				var->green.length	= 6;
				var->blue.length	= 5;
				var->transp.offset   = 0;
				var->transp.length	= 0;
			} 
			else if ((fbi->regs.fb_conf & 0x7) == 0x02) {
				/* 16 bpp, 5551 format */
				dprintk("%s: var->bpp   = 5551\n", __FUNCTION__);
				var->red.offset		= 10;
				var->green.offset	= 5;				
				var->blue.offset	= 0;
				var->red.length		= 5;
				var->green.length	= 5;
				var->blue.length	= 5;
				var->transp.offset   = 0;
				var->transp.length	= 0;
			}
			else if ((fbi->regs.fb_conf & 0x7) == 0x01) {
				/* 12 bpp 444 */
				dprintk("%s: var->bpp   = 444\n", __FUNCTION__);
				var->red.length		= 4;
				var->red.offset		= 8;
				var->green.length	= 4;
				var->green.offset	= 4;
				var->blue.length	= 4;
				var->blue.offset	= 0;
				var->transp.offset   = 0;
				var->transp.length	= 0;
			}
			break;
		case 24:
			/* 24 bpp 888 */
		case 32:
			/* 32 bpp 888 */
			var->red.length		= 8;
			var->red.offset		= 16;
			var->green.length	= 8;
			var->green.offset	= 8;
			var->blue.length	= 8;
			var->blue.offset	= 0;
			var->transp.offset   = 24;
			var->transp.length	= 8;
			break;

	}
	return 0;
}


/* ls1bfb_activate_var
 *
 * activate (set) the controller from the given framebuffer
 * information
*/
static void ls1bfb_activate_var(struct ls1bfb_info *fbi,
				   struct fb_var_screeninfo *var)
{
	int i, mode = -1;
	unsigned int out;

	SB_FB_BUF_CONFIG_REG(DC_FB)	&= ~(1<<8);

	dprintk("%s: var->xres  = %d\n", __FUNCTION__, var->xres);
	dprintk("%s: var->yres  = %d\n", __FUNCTION__, var->yres);
	dprintk("%s: var->bpp   = %d\n", __FUNCTION__, var->bits_per_pixel);
	
	/* check to see if we need to update sync/borders */
	/*
	if (!fbi->mach_info->fixed_syncs) {
		dprintk("setting vert: up=%d, low=%d, sync=%d\n",
			var->upper_margin, var->lower_margin,
			var->vsync_len);

		dprintk("setting horz: lft=%d, rt=%d, sync=%d\n",
			var->left_margin, var->right_margin,
			var->hsync_len);

		fbi->regs.hdisplay	= ((var->xres + var->right_margin + var->left_margin + var->hsync_len) << 16) | var->xres;
		fbi->regs.hsync		= 0x40000000 | ((var->xres + var->right_margin + var->hsync_len) << 16) | (var->xres + var->right_margin);
		fbi->regs.vdisplay	= ((var->yres + var->upper_margin + var->lower_margin + var->vsync_len) << 16) | var->yres;
		fbi->regs.vsync		= 0x40000000 | ((var->yres + var->lower_margin + var->vsync_len) << 16) | (var->yres + var->lower_margin);
	}
	*/

	for(i=0; i<sizeof(vgamode)/sizeof(struct vga_struc); i++){
		if(vgamode[i].hr == var->xres && vgamode[i].vr == var->yres){
			mode = i;
			break;
		}
	}
	if(mode<0){
		printk(KERN_DEBUG "\n\n\nunsupported framebuffer resolution\n\n\n");
		return;
	}

	fbi->regs.hdisplay	= (vgamode[mode].hfl<<16) | vgamode[mode].hr;
	fbi->regs.hsync		= 0x40000000 | (vgamode[mode].hse<<16) | vgamode[mode].hss;
	fbi->regs.vdisplay	= (vgamode[mode].vfl<<16) | vgamode[mode].vr;
	fbi->regs.vsync		= 0x40000000 | (vgamode[mode].vse<<16) | vgamode[mode].vss;

	/* �趨LCD����Ƶ�� */
	out = caclulatefreq(APB_CLK/1000, vgamode[mode].pclk);
	*(volatile int *)0xbfd00414 = out;
	/*output pix1 clock  pll ctrl*/
	*(volatile int *)0xbfd00410 = out;
	/*output pix2 clock pll ctrl */
	*(volatile int *)0xbfd00424 = out;

	/* write new registers */

	dprintk("new register set:\n");
	dprintk("hdisplay = 0x%08x\n", fbi->regs.hdisplay);
	dprintk("hsync = 0x%08x\n", fbi->regs.hsync);
	dprintk("vdisplay = 0x%08x\n", fbi->regs.vdisplay);
	dprintk("vsync = 0x%08x\n", fbi->regs.vsync);

	SB_FB_HDISP_REG(DC_FB)	= fbi->regs.hdisplay;
	SB_FB_HSYNC_REG(DC_FB)	= fbi->regs.hsync;
	SB_FB_VDISP_REG(DC_FB)	= fbi->regs.vdisplay;
	SB_FB_VSYNC_REG(DC_FB)	= fbi->regs.vsync;
	switch(fbi->regs.fb_conf & 0x7){
		case 1:	//12bpp R4G4B4
			dprintk("12bit\n");
			SB_FB_BUF_STRIDE_REG(DC_FB) = ((fbi->regs.hdisplay & 0xFFF) * 2 + 255) & ~255;
			break;
		case 2:	//15bpp R5G5B5
			dprintk("15bit\n");
			SB_FB_BUF_STRIDE_REG(DC_FB) = ((fbi->regs.hdisplay & 0xFFF) * 2 + 255) & ~255;
			break;
		case 3:	//16bpp R5G6B5
			dprintk("16bit\n");
			SB_FB_BUF_STRIDE_REG(DC_FB) = ((fbi->regs.hdisplay & 0xFFF) * 2 + 255) & ~255;
			break;
		case 4:	//24bpp R8G8B8
			dprintk("24bit\n");
			SB_FB_BUF_STRIDE_REG(DC_FB) = ((fbi->regs.hdisplay & 0xFFF) * 4 + 255) & ~255;
			break;
		default:
			SB_FB_BUF_STRIDE_REG(DC_FB) = ((fbi->regs.hdisplay & 0xFFF) * 2 + 255) & ~255;
			break;
	}
	SB_FB_BUF_ORIGIN_REG(DC_FB) = fbi->regs.fb_origin;

	/* set lcd address pointers */
	ls1bfb_set_lcdaddr(fbi);
	
	fbi->regs.fb_conf |= (1<<4);
	SB_FB_BUF_CONFIG_REG(DC_FB) = fbi->regs.fb_conf;
//	mdelay(1);
	fbi->regs.fb_conf &= ~(1<<4);
	SB_FB_BUF_CONFIG_REG(DC_FB) = fbi->regs.fb_conf;
	
	fbi->regs.fb_conf |= ((1<<20) | (1<<8));
	SB_FB_BUF_CONFIG_REG(DC_FB) = fbi->regs.fb_conf;
}


/*
 *      ls1bfb_set_par - Optional function. Alters the hardware state.
 *      @info: frame buffer structure that represents a single frame buffer
 *
 */
/*����fb_info�еĲ�������������û����õĿɱ����var�����̶�����fix*/
static int ls1bfb_set_par(struct fb_info *info)
{
	struct ls1bfb_info *fbi = info->par;
	/*���fb_info�еĿɱ����*/
	/* �ɱ���� ��ǰvar */
	struct fb_var_screeninfo *var = &info->var;

	/* bits_per_pixelÿ����λ�� BPP */
	/*�жϿɱ�����е�ɫλģʽ������ɫλģʽ������ɫ��ģʽ*/
	switch (var->bits_per_pixel)
	{
		/* fix.visual��¼��Ļʹ�õ�ɫ��ģʽ */
		case 32:
		case 24:
			fbi->fb->fix.line_length     = var->xres_virtual * 4;
			fbi->fb->fix.visual = FB_VISUAL_TRUECOLOR;		//���ɫ
			break;
		case 16:
		case 15:
		case 12:
			fbi->fb->fix.line_length     = var->xres_virtual * 2;
			fbi->fb->fix.visual = FB_VISUAL_TRUECOLOR;		//���ɫ
			break;
		case 1:
			 fbi->fb->fix.visual = FB_VISUAL_MONO01;		//��ɫ
			 break;
		default:/*Ĭ������Ϊα��ɫ������������ɫ��ʾ*/
			 fbi->fb->fix.visual = FB_VISUAL_PSEUDOCOLOR;	//α��ɫ
			 break;
	}
	/*����fb_info�й̶�������һ�е��ֽ�������ʽ��1���ֽ���=(1�����ظ���*ÿ����λ��BPP)/8 */
//	fbi->fb->fix.line_length     = (var->width * var->bits_per_pixel)/8;

	/* activate this new configuration */
	/*�޸����ϲ��������¼���fb_info�еĲ�������(����ʹ�޸ĺ�Ĳ�����Ӳ������Ч)*/
	ls1bfb_activate_var(fbi, var);
	return 0;
}

static void schedule_palette_update(struct ls1bfb_info *fbi,
				    unsigned int regno, unsigned int val)
{
	fbi->palette_buffer[regno] = val;

	if (!fbi->palette_ready) {
		fbi->palette_ready = 1;
	}
}

/* from pxafb.c */
static inline unsigned int chan_to_field(unsigned int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static int ls1bfb_setcolreg(unsigned regno,
			       unsigned red, unsigned green, unsigned blue,
			       unsigned transp, struct fb_info *info)
{
	struct ls1bfb_info *fbi = info->par;
	unsigned int val;

	/* dprintk("setcol: regno=%d, rgb=%d,%d,%d\n", regno, red, green, blue); */
	/* fix.visual��¼��Ļʹ�õ�ɫ��ģʽ */
	switch (fbi->fb->fix.visual) {
	case FB_VISUAL_TRUECOLOR:	//���ɫ
		/* true-colour, use pseuo-palette */

		if (regno < 16) {
			u32 *pal = fbi->fb->pseudo_palette;

			val  = chan_to_field(red,   &fbi->fb->var.red);
			val |= chan_to_field(green, &fbi->fb->var.green);
			val |= chan_to_field(blue,  &fbi->fb->var.blue);

			pal[regno] = val;
		}
		break;

	case FB_VISUAL_PSEUDOCOLOR:	//α��ɫ
		if (regno < 256) {
			/* currently assume RGB 5-6-5 mode */

			val  = ((red   >>  0) & 0xf800);
			val |= ((green >>  5) & 0x07e0);
			val |= ((blue  >> 11) & 0x001f);

//			writel(val, S3C2410_TFTPAL(regno));
			schedule_palette_update(fbi, regno, val);
		}

		break;

	default:
		return 1;   /* unknown type */
	}

	return 0;
}


/**
 *      ls1bfb_blank
 *	@blank_mode: the blank mode we want.
 *	@info: frame buffer structure that represents a single frame buffer
 *
 *	Blank the screen if blank_mode != 0, else unblank. Return 0 if
 *	blanking succeeded, != 0 if un-/blanking failed due to e.g. a
 *	video mode which doesn't support it. Implements VESA suspend
 *	and powerdown modes on hardware that supports disabling hsync/vsync:
 *	blank_mode == 2: suspend vsync
 *	blank_mode == 3: suspend hsync
 *	blank_mode == 4: powerdown
 *
 *	Returns negative errno on error, or zero on success.
 *
 */
static int ls1bfb_blank(int blank_mode, struct fb_info *info)
{
	dprintk("blank(mode=%d, info=%p)\n", blank_mode, info);

	return 0;
}

static int ls1bfb_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", debug ? "on" : "off");
}

static int ls1bfb_debug_store(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t len)
{
	if (mach_info == NULL)
		return -EINVAL;

	if (len < 1)
		return -EINVAL;

	if (strnicmp(buf, "on", 2) == 0 ||
	    strnicmp(buf, "1", 1) == 0) {
		debug = 1;
		printk(KERN_DEBUG "ls1bfb: Debug On");
	} else if (strnicmp(buf, "off", 3) == 0 ||
		   strnicmp(buf, "0", 1) == 0) {
		debug = 0;
		printk(KERN_DEBUG "ls1bfb: Debug Off");
	} else {
		return -EINVAL;
	}

	return len;
}


static DEVICE_ATTR(debug, 0666,
		   ls1bfb_debug_show,
		   ls1bfb_debug_store);

static struct fb_ops ls1bfb_ops = {
	.owner			= THIS_MODULE,
	.fb_check_var	= ls1bfb_check_var,
	.fb_set_par		= ls1bfb_set_par,
	.fb_blank		= ls1bfb_blank,
	.fb_setcolreg	= ls1bfb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};


/*
 * ls1bfb_map_video_memory():
 *	Allocates the DRAM memory for the frame buffer.  This buffer is
 *	remapped into a non-cached, non-buffered, memory region to
 *	allow palette and pixel writes to occur without flushing the
 *	cache.  Once this area is remapped, all virtual memory
 *	access to the video memory should occur at the new region.
 */
static int __init ls1bfb_map_video_memory(struct ls1bfb_info *fbi)
{
	dprintk("map_video_memory(fbi=%p)\n", fbi);

	fbi->map_size = PAGE_ALIGN(fbi->fb->fix.smem_len + PAGE_SIZE);
//	fbi->map_cpu  = dma_alloc_writecombine(fbi->dev, fbi->map_size,
//					       &fbi->map_dma, GFP_KERNEL);
	fbi->map_cpu  = dma_alloc_coherent(fbi->dev, fbi->map_size,
					       &fbi->map_dma, GFP_KERNEL);
	
	fbi->map_size = fbi->fb->fix.smem_len;

	if (fbi->map_cpu) {
		/* prevent initial garbage on screen */
		dprintk("map_video_memory: clear %p:%08x\n",
			fbi->map_cpu, fbi->map_size);
		memset(fbi->map_cpu, 0x00, fbi->map_size);

		fbi->screen_dma		= fbi->map_dma;
		fbi->fb->screen_base	= fbi->map_cpu;
		fbi->fb->fix.smem_start  = fbi->screen_dma;

		dprintk("map_video_memory: dma=%08x cpu=%p size=%08x\n",
			fbi->map_dma, fbi->map_cpu, fbi->fb->fix.smem_len);
	}

	return fbi->map_cpu ? 0 : -ENOMEM;
}

static inline void ls1bfb_unmap_video_memory(struct ls1bfb_info *fbi)
{
//	dma_free_writecombine(fbi->dev,fbi->map_size,fbi->map_cpu, fbi->map_dma);
	dma_free_coherent(fbi->dev,fbi->map_size,fbi->map_cpu, fbi->map_dma);
}

/*
 * ls1bfb_init_registers - Initialise all LCD-related registers
 */
static int ls1bfb_init_registers(struct ls1bfb_info *fbi)
{
	unsigned int out;
	
	/* �趨LCD����Ƶ�� */
	out = caclulatefreq(APB_CLK/1000, fbi->mach_info->pclk);
	*(volatile int *)0xbfd00414 = out;
	/*output pix1 clock  pll ctrl*/
	*(volatile int *)0xbfd00410 = out;
	/*output pix2 clock pll ctrl */
	*(volatile int *)0xbfd00424 = out;
	/* ����LCD�Ĵ��� */
	SB_FB_BUF_CONFIG_REG(DC_FB)		= fbi->regs.fb_conf;
	SB_FB_PANEL_CONFIG_REG(DC_FB)	= fbi->regs.panel_conf;
	SB_FB_HDISP_REG(DC_FB)			= fbi->regs.hdisplay;
	SB_FB_HSYNC_REG(DC_FB)			= fbi->regs.hsync;
	SB_FB_VDISP_REG(DC_FB)			= fbi->regs.vdisplay;
	SB_FB_VSYNC_REG(DC_FB)			= fbi->regs.vsync;
	switch(fbi->regs.fb_conf & 0x7){
		case 1:	//12bpp R4G4B4
			SB_FB_BUF_STRIDE_REG(DC_FB) = ((fbi->regs.hdisplay &0xFFF) * 2 + 255) & ~255;
			break;
		case 2:	//15bpp R5G5B5
			SB_FB_BUF_STRIDE_REG(DC_FB) = ((fbi->regs.hdisplay &0xFFF) * 2 + 255) & ~255;
			break;
		case 3:	//16bpp R5G6B5
			SB_FB_BUF_STRIDE_REG(DC_FB) = ((fbi->regs.hdisplay &0xFFF) * 2 + 255) & ~255;
			break;
		case 4:	//24bpp R8G8B8
			SB_FB_BUF_STRIDE_REG(DC_FB) = ((fbi->regs.hdisplay &0xFFF) * 4 + 255) & ~255;
			break;
		default:
			SB_FB_BUF_STRIDE_REG(DC_FB) = ((fbi->regs.hdisplay &0xFFF) * 2 + 255) & ~255;
			break;
	}
	SB_FB_BUF_ORIGIN_REG(DC_FB)		= fbi->regs.fb_origin;
	
 	ls1bfb_set_lcdaddr(fbi);
	/* Enable video by setting the ENVID bit to 1 */
	
	fbi->regs.fb_conf |= (1<<4);
	SB_FB_BUF_CONFIG_REG(DC_FB) = fbi->regs.fb_conf;
//	mdelay(1);
	fbi->regs.fb_conf &= ~(1<<4);
	SB_FB_BUF_CONFIG_REG(DC_FB) = fbi->regs.fb_conf;
	
	fbi->regs.fb_conf |= ((1<<20) | (1<<8));
	SB_FB_BUF_CONFIG_REG(DC_FB) = fbi->regs.fb_conf;
	return 0;
}

static int mach_mode_option(char *mode_option)
{
	struct ls1bfb_mach_info *pd = NULL;
	char *p, *end;
	unsigned int xres, yres;
	unsigned int frame_rate;
	int i, mode = -1;
	
	printk("%s  %d\n", mode_option, strlen(mode_option));
	pd = &LS1B_default_mach;

	mach_info = kmalloc(sizeof(*mach_info), GFP_KERNEL);
	if (mach_info) {
		memcpy(mach_info, pd, sizeof(*mach_info));
	} else {
		printk(KERN_ERR "no platform data for lcd, cannot attach\n");
		return -EINVAL;
	}
	
	for (i = 0; i < strlen(mode_option); i++){
		if (isdigit(*(mode_option+i))){
			break;
		}
	}
	if (i>=strlen(mode_option)){
		return -EINVAL;
	}
	xres = simple_strtoul(mode_option+i, &end, 10);
	yres = simple_strtoul(end+1, NULL, 10);
	if ((xres<=0 || xres>2000)||(yres<=0 || yres>2000)){
		return -EINVAL;
	}
	
	for(i=0; i<sizeof(vgamode)/sizeof(struct vga_struc); i++){
		if(vgamode[i].hr == xres && vgamode[i].vr == yres){
			mode = i;
			mode_tmp = i;
			mach_info->xres.defval = vgamode[i].hr;
			mach_info->xres.min = vgamode[i].hr;
			mach_info->xres.max = vgamode[i].hr;
			mach_info->yres.defval = vgamode[i].vr;
			mach_info->yres.min = vgamode[i].vr;
			mach_info->yres.max = vgamode[i].vr;
			mach_info->regs.hdisplay = (vgamode[mode].hfl<<16) | vgamode[mode].hr;
			mach_info->regs.hsync = 0x40000000 | (vgamode[mode].hse<<16) | vgamode[mode].hss;
			mach_info->regs.vdisplay = (vgamode[mode].vfl<<16) | vgamode[mode].vr;
			mach_info->regs.vsync = 0x40000000 | (vgamode[mode].vse<<16) | vgamode[mode].vss;
			mach_info->width = vgamode[i].hr;
			mach_info->height = vgamode[i].vr;
			mach_info->pclk = vgamode[i].pclk;
			break;
		}
	}
	if(mode<0){
		i = 0;
		mode_tmp = 0;
	}
	
	p = strstr(mode_option, "vga");
	if (p != NULL){
		mach_info->type = TYPE_VGA;
		mode_type = TYPE_VGA;
		mach_info->regs.panel_conf = 0x80001311;
	}
	else {
		mach_info->type = TYPE_LCD;
		mode_type = TYPE_LCD;
		mach_info->regs.panel_conf = vgamode[i].pan_config;
	}
	p = strchr(mode_option, '-');
	switch (simple_strtoul(p+1, NULL, 0)){
		case 12:
			mach_info->regs.fb_conf = 0x01;
			mach_info->bpp.defval = 12;
			break;
		case 15:
			mach_info->regs.fb_conf = 0x02;
			mach_info->bpp.defval = 15;
			break;
		case 16:
			mach_info->regs.fb_conf = 0x03;
			mach_info->bpp.defval = 16;
			break;
		case 24:
			mach_info->regs.fb_conf = 0x04;
			mach_info->bpp.defval = 24;
			break;
		case 32:
			mach_info->regs.fb_conf = 0x04;
			mach_info->bpp.defval = 32;
			break;
		default :
			mach_info->regs.fb_conf = 0x03;
			mach_info->bpp.defval = 16;
			break;
	}
/*
	p = strchr(mode_option, '@');
	frame_rate = simple_strtoul(p+1, NULL, 0);
	if ((frame_rate<10) || (frame_rate>300)){
		frame_rate = 60;	//Hz
	}
	mach_info->pclk = frame_rate * (mach_info->regs.vdisplay>>16&0xfff) * (mach_info->regs.hdisplay>>16&0xfff);*/
	return 0;
}

static char driver_name[]="ls1bfb";

static int __init ls1bfb_probe(struct platform_device *pdev)
{
	struct ls1bfb_info *info;
	struct fb_info	   *fbinfo;
	struct ls1bfb_hw *mregs;
	struct resource *res;
	int ret;
	int i;

	if (mach_mode_option(mode_option)) {
		return -EINVAL;
	} 

	mregs = &mach_info->regs;

	fbinfo = framebuffer_alloc(sizeof(struct ls1bfb_info), &pdev->dev);
	if (!fbinfo) {
		return -ENOMEM;
	}

	info = fbinfo->par;
	info->fb = fbinfo;
	info->dev = &pdev->dev;

	platform_set_drvdata(pdev, fbinfo);

	dprintk("devinit\n");

	strcpy(fbinfo->fix.id, driver_name);

	memcpy(&info->regs, &mach_info->regs, sizeof(info->regs));

//	info->mach_info = pdev->dev.platform_data;
	info->mach_info = mach_info;

	fbinfo->fix.type			= FB_TYPE_PACKED_PIXELS;
	fbinfo->fix.type_aux		= 0;
	fbinfo->fix.xpanstep		= 0;
	fbinfo->fix.ypanstep		= 0;
	fbinfo->fix.ywrapstep	= 0;
	fbinfo->fix.accel			= FB_ACCEL_NONE;
	

	fbinfo->var.nonstd		= 0;
//	fbinfo->var.nonstd		= FB_NONSTD_HAM;
	fbinfo->var.activate		= FB_ACTIVATE_NOW;
	fbinfo->var.height		= mach_info->height;
	fbinfo->var.width			= mach_info->width;
	fbinfo->var.accel_flags	= 0;
	fbinfo->var.vmode			= FB_VMODE_NONINTERLACED;

	fbinfo->fbops				= &ls1bfb_ops;
	fbinfo->flags				= FBINFO_FLAG_DEFAULT;
	fbinfo->pseudo_palette	= &info->pseudo_pal;

	fbinfo->var.xres				= mach_info->xres.defval;
	fbinfo->var.xres_virtual    = mach_info->xres.defval;
	fbinfo->var.yres				= mach_info->yres.defval;
	fbinfo->var.yres_virtual    = mach_info->yres.defval;
	fbinfo->var.bits_per_pixel  = mach_info->bpp.defval;

	{
	unsigned long long div;
	div = 1000000000000ULL;
	do_div(div, mach_info->pclk);
	}
	if (mode_type == TYPE_VGA){
		fbinfo->fix.smem_len = 0x500000;
	}
	else{
		fbinfo->fix.smem_len        =	mach_info->xres.max *
					mach_info->yres.max *
					mach_info->bpp.max / 8;
	}

	for (i = 0; i < 256; i++)
		info->palette_buffer[i] = PALETTE_BUFF_CLEAR;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "no I/O memory resource defined\n");
		ret = -ENODEV;
		goto dealloc_fb;
	}

	res = request_mem_region(res->start, (res->end-res->start)+1, pdev->name);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to request I/O memory\n");
		ret = -EBUSY;
		goto dealloc_fb;
	}

	info->regbase = ioremap(res->start, (res->end-res->start)+1);
	if (info->regbase == NULL) {
		dev_err(&pdev->dev, "failed to map I/O memory\n");
		ret = -EBUSY;
		goto failed_free_res;
	}

	dprintk("got LCD region\n");

	/* Initialize video memory */
	ret = ls1bfb_map_video_memory(info);
	if (ret) {
		printk( KERN_ERR "Failed to allocate video RAM: %d\n", ret);
		ret = -ENOMEM;
		goto failed_free_res;
	}
	dprintk("got video memory\n");

	ret = ls1bfb_init_registers(info);
	ret = ls1bfb_check_var(&fbinfo->var, fbinfo);

	ret = register_framebuffer(fbinfo);
	if (ret < 0) {
		printk(KERN_ERR "Failed to register framebuffer device: %d\n", ret);
		goto free_video_memory;
	}

	/* create device files */
	device_create_file(&pdev->dev, &dev_attr_debug);

	printk(KERN_INFO "fb%d: %s frame buffer device\n",
		fbinfo->node, fbinfo->fix.id);

	return 0;

free_video_memory:
	ls1bfb_unmap_video_memory(info);
failed_free_res:
	release_mem_region(res->start, (res->end-res->start)+1);
dealloc_fb:
	framebuffer_release(fbinfo);
	return ret;
}

/* ls1bfb_stop_lcd
 *
 * shutdown the lcd controller
*/
static void ls1bfb_stop_lcd(struct ls1bfb_info *fbi)
{
	SB_FB_BUF_CONFIG_REG(DC_FB) = fbi->regs.fb_conf & (~0x100);
}

/*
 *  Cleanup
 */
static int ls1bfb_remove(struct platform_device *pdev)
{
	struct fb_info	   *fbinfo = platform_get_drvdata(pdev);
	struct ls1bfb_info *info = fbinfo->par;
	struct resource *res;

	ls1bfb_stop_lcd(info);
	msleep(1);

	ls1bfb_unmap_video_memory(info);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, (res->end-res->start)+1);
	unregister_framebuffer(fbinfo);

	return 0;
}

#ifdef CONFIG_PM

/* suspend and resume support for the lcd controller */
static int ls1bfb_suspend(struct platform_device *dev, pm_message_t state)
{
	struct fb_info	   *fbinfo = platform_get_drvdata(dev);
	struct ls1bfb_info *info = fbinfo->par;

	ls1bfb_stop_lcd(info);

	return 0;
}

static int ls1bfb_resume(struct platform_device *dev)
{
	struct fb_info	   *fbinfo = platform_get_drvdata(dev);
	struct ls1bfb_info *info = fbinfo->par;

	ls1bfb_init_registers(info);

	return 0;
}

#else
#define ls1bfb_suspend NULL
#define ls1bfb_resume  NULL
#endif

static struct platform_driver ls1bfb_driver = {
	.probe		= ls1bfb_probe,
	.remove	= ls1bfb_remove,
	.suspend	= ls1bfb_suspend,
	.resume	= ls1bfb_resume,
	.driver	= {
		.name	= "ls1b-lcd",
		.owner	= THIS_MODULE,
	},
};


int __devinit ls1bfb_init(void)
{
#ifndef MODULE
	char *option = NULL;

	if (fb_get_options("ls1bfb", &option))
		return -ENODEV;

	if (option && *option)
		mode_option = option;
#endif
	return platform_driver_register(&ls1bfb_driver);
}

static void __exit ls1bfb_cleanup(void)
{
	platform_driver_unregister(&ls1bfb_driver);
}


module_init(ls1bfb_init);
module_exit(ls1bfb_cleanup);

MODULE_AUTHOR("loongson-gz tang <tanghaifeng-gz@loongson.cn>");
MODULE_DESCRIPTION("Framebuffer driver for the loongson1B");
MODULE_LICENSE("GPL");