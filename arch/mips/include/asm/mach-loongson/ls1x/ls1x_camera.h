/*
 * ls1x_camera.h - LS1X camera driver header file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_CAMERA_H_
#define __ASM_ARCH_CAMERA_H_

#define LS1X_CAMERA_DATA_HIGH	1
#define LS1X_CAMERA_PCLK_RISING	2
#define LS1X_CAMERA_VSYNC_HIGH	4
#define LS1X_CAMERA_HSYNC_HIGH	8

/**
 * struct ls1x_camera_pdata - LS1X camera platform data
 * @mclk_10khz:	master clock frequency in 10kHz units
 * @flags:	LS1X camera platform flags
 */
struct ls1x_camera_pdata {
	unsigned long mclk_10khz;
	unsigned long flags;
};

#endif /* __ASM_ARCH_CAMERA_H_ */
