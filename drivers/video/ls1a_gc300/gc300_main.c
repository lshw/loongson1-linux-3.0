/*
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/fb.h>
#include <linux/module.h>
#include <linux/delay.h>

#include <video/ls1xfb.h>

#include "gcSdk.h"

gcSURFACEINFO gcDisplaySurface;

void gc300_set_par(struct fb_info *info)
{
	struct ls1xfb_info *fbi = info->par;
	struct fb_var_screeninfo *var = &info->var;
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

	gcSelect2DPipe();
}

void gc300_hw_init(struct fb_info *info)
{
	struct ls1xfb_info *fbi = info->par;

	*(volatile int *)0xbfd00420 |= 0x00100000;	/* 关闭GPU */
	mdelay(100);
	do {
		*(volatile int *)0xbfd00420 &= ~0x00100000;	/* 使能GPU */
	} while ((*(volatile int *)0xbfd00420 & 0x00100000) != 0);
	mdelay(100);

	// Set register base address.
//	gcREG_BASE = 0xbc200000;	/* gc300寄存器基地址 */

	gcVIDEOSIZE = 64 * 1024;
	gcVIDEOBASE = kmalloc(sizeof(UINT32) * gcVIDEOSIZE, GFP_KERNEL);

	// Init memory.
	gcMemReset();

	// Init target surface.
	gcDisplaySurface.address = fbi->fb_start_dma;	/* 固定 */

//	gc300_set_var(info);
}

void gc300_remove(void)
{
	kfree(gcVIDEOBASE);
}

void gcFlushDisplay(void)
{
}

/* acceleration operations */
int gc300fb_sync(struct fb_info *info)
{
	return 0;
}

void gc300fb_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
	gcSURFACEINFO* Target = &gcDisplaySurface;
	gcSURFACEINFO Src;
	int width = area->width;
	int height = area->height;
	int sx = area->sx;
	int sy = area->sy;
	int dx = area->dx;
	int dy = area->dy;
	unsigned long rtl = 0;

	/* source clip */
	if ((sx >= info->var.xres_virtual) ||
	    (sy >= info->var.yres_virtual))
		/* source Area not within virtual screen, skipping */
		return;
	if ((sx + width) >= info->var.xres_virtual)
		width = info->var.xres_virtual - sx - 1;
	if ((sy + height) >= info->var.yres_virtual)
		height = info->var.yres_virtual - sy - 1;

	/* dest clip */
	if ((dx >= info->var.xres_virtual) ||
	    (dy >= info->var.yres_virtual))
		/* Destination Area not within virtual screen, skipping */
		return;
	if ((dx + width) >= info->var.xres_virtual)
		width = info->var.xres_virtual - dx - 1;
	if ((dy + height) >= info->var.yres_virtual)
		height = info->var.yres_virtual - dy - 1;

	if ((sx < dx) || (sy < dy)) {
		rtl = 1 << 27;
		sx += width - 1;
		dx += width - 1;
		sy += height - 1;
		dy += height - 1;
	}

	// Init target surface.
	Src.address = Target->address;
	Src.stride  = Target->stride; // gcSCREENWIDTH * gcGetPixelSize(gcSCREENFORMAT) / 8;
	Src.format  = Target->format;

	// Init clipping.
	Src.clip.left   = sx;
	Src.clip.top    = sy;
	Src.clip.right  = sx + width;
	Src.clip.bottom = sy + height;

	// Compute initial rect.
	Src.rect.left   = sx;
	Src.rect.right  = sx + width;
	Target->rect.left = dx;
	Target->rect.right = dx + width;
	Src.rect.top = sy;
	Src.rect.bottom = sy + height;
	Target->rect.top = dy;
	Target->rect.bottom = dy + height;

	// Blit the image.
//	gcBitBlt_SC(Target, &Src, &Target->rect, &Src.rect);
	/* 把一块内存的数据传送到令一块 必须0xCC 0xCC 设置透明度 Relative或Absolute(会被拉伸或压缩) */
	gcBitBlt(Target, &Src, &Target->rect, &Src.rect, 0xCC, 0xCC, NULL, 
		AQDE_SRC_CONFIG_TRANSPARENCY_OPAQUE, ~0, NULL, ~0, 0);
//	1);		// Relative coordinates.
//	0);		// Absolute coordinates.

	// Start.
	gcFlush2DAndStall();
	gcStart();
//	gcFlushDisplay();

	// Free the image.
//	gcMemFree();
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
//	gcFlushDisplay();
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
