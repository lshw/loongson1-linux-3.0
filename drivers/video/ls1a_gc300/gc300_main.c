/*
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/fb.h>
#include <linux/module.h>

#include <video/ls1xfb.h>

#include "gcSdk.h"

gcSURFACEINFO gcDisplaySurface;

void gc300_hw_init(struct fb_info *info)
{
	struct ls1xfb_info *fbi = info->par;
	struct fb_var_screeninfo *var = &info->var;

	*((volatile unsigned int*)0xbfd00420) &= ~0x00100000;	/* 使能GPU */

	// Set register base address.
	gcREG_BASE = 0xBC200000;	/* gc300寄存器基地址 */

//	gcVIDEOBASE = 0xA2000000;
//	gcVIDEOSIZE = 0x2000000;
	gcVIDEOBASE = fbi->fb_start_dma;	/* fb显示基地址 没有？ */
	gcVIDEOSIZE = info->fix.smem_len;	/* fb显存大小 */

	// Init memory.
	gcMemReset();

	// Init target surface.
	gcDisplaySurface.address = fbi->fb_start_dma;	/* 固定 */
	/* 可变 */
	gcDisplaySurface.stride  = var->xres_virtual * var->bits_per_pixel / 8;
	switch(fbi->pix_fmt) {
		case PIX_FMT_RGB444:
			gcDisplaySurface.format  = AQDE_SRC_CONFIG_FORMAT_A4R4G4B4;
		break;
		case PIX_FMT_RGB1555:
			gcDisplaySurface.format  = AQDE_SRC_CONFIG_FORMAT_A1R5G5B5;
		break;
		case PIX_FMT_RGB565:
			gcDisplaySurface.format  = AQDE_SRC_CONFIG_FORMAT_R5G6B5;
		break;
		case PIX_FMT_RGBA888:
			gcDisplaySurface.format  = AQDE_SRC_CONFIG_FORMAT_A8R8G8B8;
		break;
		default:
			gcDisplaySurface.format  = AQDE_SRC_CONFIG_FORMAT_R5G6B5;
		break;
	}

	// Init coordinates.
	gcDisplaySurface.rect.left   = 0;
	gcDisplaySurface.rect.top    = 0;
	gcDisplaySurface.rect.right  = var->xres;
	gcDisplaySurface.rect.bottom = var->yres;

	// Init clipping.
	gcDisplaySurface.clip.left   = 0;
	gcDisplaySurface.clip.top    = 0;
	gcDisplaySurface.clip.right  = var->xres; 
	gcDisplaySurface.clip.bottom = var->yres;
}

void gcFlushDisplay(void)
{
}

/* acceleration operations */
int gc300fb_sync(struct fb_info *info)
{
	return 0;
}

void gc300fb_copyarea(struct fb_info *info, const struct fb_copyarea *region)
{
}

void gc300fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	gcSURFACEINFO* Target = &gcDisplaySurface;
	int width = rect->width, height = rect->height;

	if ((rect->dx >= info->var.xres_virtual) ||
	    (rect->dy >= info->var.yres_virtual))
		/* Rectangle not within virtual screen, skipping */
		return;
	if ((rect->dx + width) >= info->var.xres_virtual)
		width = info->var.xres_virtual - rect->dx - 1;
	if ((rect->dy + height) >= info->var.yres_virtual)
		height = info->var.yres_virtual - rect->dy - 1;

	if (gc300fb_sync(info))
		return;

	/* set data format */
	switch (info->var.bits_per_pixel) {
		case 1:
		case 2:
		case 4:
		case 8:
		case 12:
			Target->format  = AQDE_SRC_CONFIG_FORMAT_A4R4G4B4;
			break;
		case 15:
			Target->format  = AQDE_SRC_CONFIG_FORMAT_A1R5G5B5;
			break;
		case 16:
			Target->format  = AQDE_SRC_CONFIG_FORMAT_R5G6B5;
			break;
		case 24:
		case 32:
			Target->format  = AQDE_SRC_CONFIG_FORMAT_A8R8G8B8;
			break;
	}

	Target->rect.left   =  rect->dx;
	Target->rect.right  =  rect->dx + width;
	Target->rect.top    =  rect->dy;
	Target->rect.bottom =  rect->dy + height;

	gcClear(Target, &Target->rect, rect->color);

	gcFlush2DAndStall();
	gcStart();
	gcFlushDisplay();
}

void gc300fb_imageblit(struct fb_info *info, const struct fb_image *image)
{
}
/*
EXPORT_SYMBOL(gc300_hw_init);
EXPORT_SYMBOL(gc300fb_copyarea);
EXPORT_SYMBOL(gc300fb_fillrect);
EXPORT_SYMBOL(gc300fb_imageblit);
*/
