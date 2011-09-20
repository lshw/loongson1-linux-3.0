/*
 * Driver for GC0308 from GC
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

struct gc0308_platform_data {
	unsigned int default_width;
	unsigned int default_height;
	unsigned int pixelformat;
	int freq;	/* MCLK in KHz */

	/* This SoC supports Parallel & CSI-2 */
	int is_mipi;
};


enum fimc_cam_type {
	CAM_TYPE_ITU = 0,
	CAM_TYPE_MIPI = 1,
};

enum fimc_cam_format {
	ITU_601_YCBCR422_8BIT = (1 << 31),
	ITU_656_YCBCR422_8BIT = (0 << 31),
	ITU_601_YCBCR422_16BIT = (1 << 29),
	MIPI_CSI_YCBCR422_8BIT = 0x1e,
	MIPI_CSI_RAW8 = 0x2a,
	MIPI_CSI_RAW10 = 0x2b,
	MIPI_CSI_RAW12 = 0x2c,
	MIPI_USER_DEF_PACKET_1 = 0x30, /* User defined Byte-based packet 1 */
};

enum fimc_cam_order422 {
	CAM_ORDER422_8BIT_YCBYCR = (0 << 14),
	CAM_ORDER422_8BIT_YCRYCB = (1 << 14),
	CAM_ORDER422_8BIT_CBYCRY = (2 << 14),
	CAM_ORDER422_8BIT_CRYCBY = (3 << 14),
	CAM_ORDER422_16BIT_Y4CBCRCBCR = (0 << 14),
	CAM_ORDER422_16BIT_Y4CRCBCRCB = (1 << 14),
};

enum fimc_cam_index {
	CAMERA_PAR_A = 0,
	CAMERA_PAR_B = 1,
	CAMERA_CSI_C = 2,
	CAMERA_PATTERN = 3, /* Not actual camera but test pattern */
	CAMERA_WB = 4, /* Not actual camera but write back */
};


#if 0
struct s3c_platform_camera {
	/*
	* ITU cam A,B: 0,1
	* CSI-2 cam C: 2
	*/
	enum fimc_cam_index id;

	enum fimc_cam_type type; /* ITU or MIPI */
	enum fimc_cam_format fmt; /* input format */
	enum fimc_cam_order422 order422; /* YCBCR422 order for ITU */

	u32 pixelformat; /* default fourcc */

	int i2c_busnum;
	struct i2c_board_info *info;
	struct v4l2_subdev *sd;

	const char srclk_name[16]; /* source of mclk name */
	const char clk_name[16]; /* mclk name */
	u32 clk_rate; /* mclk ratio */
	struct clk *clk; /* mclk */
	int line_length; /* max length */
	int width; /* default resol */
	int height; /* default resol */
	struct v4l2_rect window; /* real cut region from source */

	int mipi_lanes; /* MIPI data lanes */
	int mipi_settle; /* MIPI settle */
	int mipi_align; /* MIPI data align: 24/32 */

	/* Polarity: 1 if inverted polarity used */
	int inv_pclk;
	int inv_vsync;
	int inv_href;
	int inv_hsync;

	int initialized;

	/* Board specific power pin control */
	int (*cam_power)(int onoff);
};
#endif
