/*
 * SPCA505/506 chip based cameras
 *
 * V4L2 by Jean-François Moine <http://moinejf.free.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define MODULE_NAME "spca505"

#include "gspca.h"

MODULE_AUTHOR("Jean-François Moine <http://moinejf.free.fr>"
		"Michel Xhaard <mxhaard@users.sourceforge.net>");
MODULE_DESCRIPTION("GSPCA/SPCA505 USB Camera Driver");
MODULE_LICENSE("GPL");

enum controls {
	BRIGHTNESS,
	CONTRAST,
	COLORS,
	HUE,
	NCTRLS		/* number of controls */
};
/* specific webcam descriptor */
struct sd {
	struct gspca_dev gspca_dev;		/* !! must be the first item */

	struct gspca_ctrl ctrls[NCTRLS];

	u8 subtype;
};
enum subtypes {
	ST_GRABBER,		/* with spca506 and SAA7113 */
	IntelPCCameraPro,	/* or UsbGrabber PV321 */
	Nxultra,
};

static void setbrightness(struct gspca_dev *gspca_dev);
static void setcontrast(struct gspca_dev *gspca_dev);
static void setcolors(struct gspca_dev *gspca_dev);
static void sethue(struct gspca_dev *gspca_dev);

static const struct ctrl sd_ctrls[NCTRLS] = {
[BRIGHTNESS] = {
	    {
		.id      = V4L2_CID_BRIGHTNESS,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Brightness",
		.minimum = 0,
		.maximum = 255,
		.step    = 1,
		.default_value = 127,
	    },
	    .set_control = setbrightness
	},
[CONTRAST] = {
	    {
		.id      = V4L2_CID_CONTRAST,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Contrast",
		.minimum = 0,
		.maximum = 0xff,
		.step    = 1,
		.default_value = 0x47,
	    },
	    .set_control = setcontrast
	},
[COLORS] = {
	    {
		.id      = V4L2_CID_SATURATION,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Saturation",
		.minimum = 0,
		.maximum = 0xff,
		.step    = 1,
		.default_value = 0x40,
	    },
	    .set_control = setcolors
	},
[HUE] = {
	    {
		.id      = V4L2_CID_HUE,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Hue",
		.minimum = 0,
		.maximum = 0xff,
		.step    = 1,
		.default_value = 0,
	    },
	    .set_control = sethue
	},
};

static const struct v4l2_pix_format vga_mode[] = {
	{160, 120, V4L2_PIX_FMT_SPCA505, V4L2_FIELD_NONE,
		.bytesperline = 160,
		.sizeimage = 160 * 120 * 3 / 2,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 4},
	{176, 144, V4L2_PIX_FMT_SPCA505, V4L2_FIELD_NONE,
		.bytesperline = 176,
		.sizeimage = 176 * 144 * 3 / 2,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 3},
	{320, 240, V4L2_PIX_FMT_SPCA505, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240 * 3 / 2,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 2},
	{352, 288, V4L2_PIX_FMT_SPCA505, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimage = 352 * 288 * 3 / 2,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1},
	{640, 480, V4L2_PIX_FMT_SPCA505, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480 * 3 / 2,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
};

/* input table for ST_GRABBER devices */
static const struct v4l2_input sd_inputs[] = {
	{0, "Webcam", V4L2_INPUT_TYPE_CAMERA,
		.std = 0},
	{1, "Composite 0", V4L2_INPUT_TYPE_CAMERA,
		.std = V4L2_STD_PAL | V4L2_STD_NTSC | V4L2_STD_SECAM},
	{2, "Composite 1", V4L2_INPUT_TYPE_CAMERA,
		.std = V4L2_STD_PAL | V4L2_STD_NTSC | V4L2_STD_SECAM},
	{3, "Composite 2", V4L2_INPUT_TYPE_CAMERA,
		.std = V4L2_STD_PAL | V4L2_STD_NTSC | V4L2_STD_SECAM},
	{4, "Composite 3", V4L2_INPUT_TYPE_CAMERA,
		.std = V4L2_STD_PAL | V4L2_STD_NTSC | V4L2_STD_SECAM},
	{5, "S-Video 0", V4L2_INPUT_TYPE_CAMERA,
		.std = V4L2_STD_PAL | V4L2_STD_NTSC | V4L2_STD_SECAM},
	{6, "S-Video 1", V4L2_INPUT_TYPE_CAMERA,
		.std = V4L2_STD_PAL | V4L2_STD_NTSC | V4L2_STD_SECAM},
	{7, "S-Video 2", V4L2_INPUT_TYPE_CAMERA,
		.std = V4L2_STD_PAL | V4L2_STD_NTSC | V4L2_STD_SECAM},
	{8, "S-Video 3", V4L2_INPUT_TYPE_CAMERA,
		.std = V4L2_STD_PAL | V4L2_STD_NTSC | V4L2_STD_SECAM},
};

#define SPCA50X_OFFSET_DATA 10

/* USB control */
#define REQ_02_USB 0x02
#define IDX_00_USB_CTRL 0x00	/* USB control */
#define USB_CTRL_ISO_ENABLE 0x01 /* ISO enable */

/* global control */
#define REQ_03_GLOBAL 0x03
#define IDX_00_GLOBAL_MISC0 0x00 /* Global control miscellaneous 0 spca505 */
#define GLOBAL_MISC0_IDSEL 0x01 /* Global control device ID select spca505 */

#define IDX_01_GLOBAL_MISC1 0x01
#define IDX_03_GLOBAL_MISC3 0x03
#define GLOBAL_MISC3_SAA7113RST 0x20	/* Not sure about this one */

/* Image format and compression control */
#define REQ_04_COMPRESS 0x04

/* spca506 Synchronous Serial Control */
#define REQ_07_SSC 0x07
#define IDX_00_SSC_DATA 0x00
#define IDX_01_SSC_ADDR 0x01
#define IDX_02_SSC_PREFETCH 0x02
#define IDX_03_SSC_BUSY 0x03
#define IDX_04_SSC_SLAVE_ADDR 0x04
/* spca506 TV-IN */
#define REQ_08_TV_IN 0x08
#define IDX_00_TV_IN_PARM0 0x00

#define SAA7113_bright 0x0a
#define SAA7113_contrast 0x0b
#define SAA7113_saturation 0x0c
#define SAA7113_hue 0x0d
#define SAA7113_I2C_ADDR 0x4a

static const u8 grabber_bridge_init1[][3] = {
	{REQ_03_GLOBAL, 0x00, 0x04},
	{REQ_03_GLOBAL, 0xff, 0x03},
	{REQ_03_GLOBAL, 0x00, 0x00},
	{REQ_03_GLOBAL, 0x1c, 0x01},
	{REQ_03_GLOBAL, 0x18, 0x01},
	{}
};
static const u8 grabber_bridge_init2[][3] = {
	{REQ_03_GLOBAL, 0x1c, 0x01},
	{REQ_03_GLOBAL, 0x18, 0x01},
	{0x05, 0x00, 0x00},
	{0x05, 0xef, 0x01},
	{0x05, 0x00, 0xc1},
	{0x05, 0x00, 0xc2},
	{0x06, 0x18, 0x02},
	{0x06, 0xf5, 0x11},
	{0x06, 0x02, 0x12},
	{0x06, 0xfb, 0x13},
	{0x06, 0x00, 0x14},
	{0x06, 0xa4, 0x51},
	{0x06, 0x40, 0x52},
	{0x06, 0x71, 0x53},
	{0x06, 0x40, 0x54},
	/************************************************/
	{REQ_03_GLOBAL, 0x00, 0x04},
	{REQ_03_GLOBAL, 0x00, 0x03},
	{REQ_03_GLOBAL, 0x00, 0x04},
	{REQ_03_GLOBAL, 0xff, 0x03},
	{REQ_02_USB, 0x00, 0x00},
	{REQ_03_GLOBAL, 0x60, 0x00},
	{REQ_03_GLOBAL, 0x18, 0x01},
	{}
};
static const u8 grabber_saa7113_init[][2] = {
	{0x08, 0x01},
	{0xc0, 0x02},	/* input composite video */
	{0x33, 0x03},
	{0x00, 0x04},
	{0x00, 0x05},
	{0x0d, 0x06},
	{0xf0, 0x07},
	{0x98, 0x08},
	{0x03, 0x09},
	{0x80, 0x0a},
	{0x47, 0x0b},
	{0x48, 0x0c},
	{0x00, 0x0d},
	{0x03, 0x0e},	/* Chroma Pal adjust */
	{0x2a, 0x0f},
	{0x00, 0x10},
	{0x0c, 0x11},
	{0xb8, 0x12},
	{0x01, 0x13},
	{0x00, 0x14},
	{0x00, 0x15},
	{0x00, 0x16},
	{0x00, 0x17},
	{0x00, 0x18},
	{0x00, 0x19},
	{0x00, 0x1a},
	{0x00, 0x1b},
	{0x00, 0x1c},
	{0x00, 0x1d},
	{0x00, 0x1e},
	{0xa1, 0x1f},
	{0x02, 0x40},
	{0xff, 0x41},
	{0xff, 0x42},
	{0xff, 0x43},
	{0xff, 0x44},
	{0xff, 0x45},
	{0xff, 0x46},
	{0xff, 0x47},
	{0xff, 0x48},
	{0xff, 0x49},
	{0xff, 0x4a},
	{0xff, 0x4b},
	{0xff, 0x4c},
	{0xff, 0x4d},
	{0xff, 0x4e},
	{0xff, 0x4f},
	{0xff, 0x50},
	{0xff, 0x51},
	{0xff, 0x52},
	{0xff, 0x53},
	{0xff, 0x54},
	{0xff, 0x55},
	{0xff, 0x56},
	{0xff, 0x57},
	{0x00, 0x58},
	{0x54, 0x59},
	{0x07, 0x5a},
	{0x83, 0x5b},
	{0x00, 0x5c},
	{0x00, 0x5d},
	{0x00, 0x5e},
	{0x00, 0x5f},
	{0x00, 0x60},
	{0x05, 0x61},
	{0x9f, 0x62},
	{}
};
static const u8 grabber_bridge_start1[][3] = {
	{0x03, 0x00, 0x04},
	{0x03, 0x00, 0x03},
	{0x03, 0x00, 0x04},
	{0x03, 0xff, 0x03},
	{0x02, 0x00, 0x00},
	{0x03, 0x60, 0x00},
	{0x03, 0x18, 0x01},
	{}
};
static const u8 grabber_saa7113_start[][2] = {
	{0x08, 0x01},	/* Increment Delay */
/*	{0xc0, 0x02},	 * Analog Input Control 1 */
	{0x33, 0x03},	/* Analog Input Control 2 */
	{0x00, 0x04},	/* Analog Input Control 3 */
	{0x00, 0x05},	/* Analog Input Control 4 */
	{0x0d, 0x06},	/* Horizontal Sync Start 0xe9-0x0d */
	{0xf0, 0x07},	/* Horizontal Sync Stop  0x0d-0xf0 */

	{0x98, 0x08},	/* Sync Control */
/*		Default values			*/
	{0x03, 0x09},	/* Luminance Control */
	{0x80, 0x0a},	/* Luminance Brightness */
	{0x47, 0x0b},	/* Luminance Contrast */
	{0x48, 0x0c},	/* Chrominance Saturation */
	{0x00, 0x0d},	/* Chrominance Hue Control */
	{0x2a, 0x0f},	/* Chrominance Gain Control */
	/**************************************/
	{0x00, 0x10},	/* Format/Delay Control */
	{0x0c, 0x11},	/* Output Control 1 */
	{0xb8, 0x12},	/* Output Control 2 */
	{0x01, 0x13},	/* Output Control 3 */
	{0x00, 0x14},	/* reserved */
	{0x00, 0x15},	/* VGATE START */
	{0x00, 0x16},	/* VGATE STOP */
	{0x00, 0x17},	/* VGATE Control (MSB) */
	{0x00, 0x18},
	{0x00, 0x19},
	{0x00, 0x1a},
	{0x00, 0x1b},
	{0x00, 0x1c},
	{0x00, 0x1d},
	{0x00, 0x1e},
	{0xa1, 0x1f},
	{0x02, 0x40},
	{0xff, 0x41},
	{0xff, 0x42},
	{0xff, 0x43},
	{0xff, 0x44},
	{0xff, 0x45},
	{0xff, 0x46},
	{0xff, 0x47},
	{0xff, 0x48},
	{0xff, 0x49},
	{0xff, 0x4a},
	{0xff, 0x4b},
	{0xff, 0x4c},
	{0xff, 0x4d},
	{0xff, 0x4e},
	{0xff, 0x4f},
	{0xff, 0x50},
	{0xff, 0x51},
	{0xff, 0x52},
	{0xff, 0x53},
	{0xff, 0x54},
	{0xff, 0x55},
	{0xff, 0x56},
	{0xff, 0x57},
	{0x00, 0x58},
	{0x54, 0x59},
	{0x07, 0x5a},
	{0x83, 0x5b},
	{0x00, 0x5c},
	{0x00, 0x5d},
	{0x00, 0x5e},
	{0x00, 0x5f},
	{0x00, 0x60},
	{0x05, 0x61},
	{0x9f, 0x62},
	{}
};
static const u8 grabber_bridge_start2[][3] = {
	{0x05, 0x00, 0x0003},
	{0x05, 0x00, 0x0004},
	{0x03, 0x10, 0x0001},
	{0x03, 0x78, 0x0000},
	{}
};

/*
 * Data to initialize a SPCA505. Common to the CCD and external modes
 */
static const u8 spca505_init_data[][3] = {
	/* bmRequest,value,index */
	{REQ_03_GLOBAL, GLOBAL_MISC3_SAA7113RST, IDX_03_GLOBAL_MISC3},
	/* Sensor reset */
	{REQ_03_GLOBAL, 0x00, IDX_03_GLOBAL_MISC3},
	{REQ_03_GLOBAL, 0x00, IDX_01_GLOBAL_MISC1},
	/* Block USB reset */
	{REQ_03_GLOBAL, GLOBAL_MISC0_IDSEL, IDX_00_GLOBAL_MISC0},

	{0x05, 0x01, 0x10},
					/* Maybe power down some stuff */
	{0x05, 0x0f, 0x11},

	/* Setup internal CCD  ? */
	{0x06, 0x10, 0x08},
	{0x06, 0x00, 0x09},
	{0x06, 0x00, 0x0a},
	{0x06, 0x00, 0x0b},
	{0x06, 0x10, 0x0c},
	{0x06, 0x00, 0x0d},
	{0x06, 0x00, 0x0e},
	{0x06, 0x00, 0x0f},
	{0x06, 0x10, 0x10},
	{0x06, 0x02, 0x11},
	{0x06, 0x00, 0x12},
	{0x06, 0x04, 0x13},
	{0x06, 0x02, 0x14},
	{0x06, 0x8a, 0x51},
	{0x06, 0x40, 0x52},
	{0x06, 0xb6, 0x53},
	{0x06, 0x3d, 0x54},
	{}
};

/*
 * Data to initialize the camera using the internal CCD
 */
static const u8 spca505_open_data_ccd[][3] = {
	/* bmRequest,value,index */
	/* Internal CCD data set */
	{REQ_03_GLOBAL, 0x04, IDX_01_GLOBAL_MISC1},
	/* This could be a reset */
	{REQ_03_GLOBAL, 0x00, IDX_01_GLOBAL_MISC1},

	/* Setup compression and image registers. 0x6 and 0x7 seem to be
	   related to H&V hold, and are resolution mode specific */
	{REQ_04_COMPRESS, 0x10, 0x01},
		/* DIFF(0x50), was (0x10) */
	{REQ_04_COMPRESS, 0x00, 0x04},
	{REQ_04_COMPRESS, 0x00, 0x05},
	{REQ_04_COMPRESS, 0x20, 0x06},
	{REQ_04_COMPRESS, 0x20, 0x07},

	{0x08, 0x0a, 0x00},
	/* DIFF (0x4a), was (0xa) */

	{0x05, 0x00, 0x10},
	{0x05, 0x00, 0x11},
	{0x05, 0x00, 0x00},
	/* DIFF not written */
	{0x05, 0x00, 0x01},
	/* DIFF not written */
	{0x05, 0x00, 0x02},
	/* DIFF not written */
	{0x05, 0x00, 0x03},
	/* DIFF not written */
	{0x05, 0x00, 0x04},
	/* DIFF not written */
		{0x05, 0x80, 0x05},
		/* DIFF not written */
		{0x05, 0xe0, 0x06},
		/* DIFF not written */
		{0x05, 0x20, 0x07},
		/* DIFF not written */
		{0x05, 0xa0, 0x08},
		/* DIFF not written */
		{0x05, 0x00, 0x12},
		/* DIFF not written */
	{0x05, 0x02, 0x0f},
	/* DIFF not written */
		{0x05, 0x10, 0x46},
		/* DIFF not written */
		{0x05, 0x08, 0x4a},
		/* DIFF not written */

	{REQ_03_GLOBAL, 0x08, IDX_03_GLOBAL_MISC3},
	/* DIFF (0x3,0x28,0x3) */
	{REQ_03_GLOBAL, 0x08, IDX_01_GLOBAL_MISC1},
	{REQ_03_GLOBAL, 0x0c, IDX_03_GLOBAL_MISC3},
	/* DIFF not written */
	{REQ_03_GLOBAL, 0x21, IDX_00_GLOBAL_MISC0},
		/* DIFF (0x39) */

/* Extra block copied from init to hopefully ensure CCD is in a sane state */
	{0x06, 0x10, 0x08},
	{0x06, 0x00, 0x09},
	{0x06, 0x00, 0x0a},
	{0x06, 0x00, 0x0b},
	{0x06, 0x10, 0x0c},
	{0x06, 0x00, 0x0d},
	{0x06, 0x00, 0x0e},
	{0x06, 0x00, 0x0f},
	{0x06, 0x10, 0x10},
	{0x06, 0x02, 0x11},
	{0x06, 0x00, 0x12},
	{0x06, 0x04, 0x13},
	{0x06, 0x02, 0x14},
	{0x06, 0x8a, 0x51},
	{0x06, 0x40, 0x52},
	{0x06, 0xb6, 0x53},
	{0x06, 0x3d, 0x54},
	/* End of extra block */

		{0x06, 0x3f, 0x01},
		/* Block skipped */
	{0x06, 0x10, 0x02},
	{0x06, 0x64, 0x07},
	{0x06, 0x10, 0x08},
	{0x06, 0x00, 0x09},
	{0x06, 0x00, 0x0a},
	{0x06, 0x00, 0x0b},
	{0x06, 0x10, 0x0c},
	{0x06, 0x00, 0x0d},
	{0x06, 0x00, 0x0e},
	{0x06, 0x00, 0x0f},
	{0x06, 0x10, 0x10},
	{0x06, 0x02, 0x11},
	{0x06, 0x00, 0x12},
	{0x06, 0x04, 0x13},
	{0x06, 0x02, 0x14},
	{0x06, 0x8a, 0x51},
	{0x06, 0x40, 0x52},
	{0x06, 0xb6, 0x53},
	{0x06, 0x3d, 0x54},
	{0x06, 0x60, 0x57},
	{0x06, 0x20, 0x58},
	{0x06, 0x15, 0x59},
	{0x06, 0x05, 0x5a},

	{0x05, 0x01, 0xc0},
	{0x05, 0x10, 0xcb},
		{0x05, 0x80, 0xc1},
		/* */
		{0x05, 0x0, 0xc2},
		/* 4 was 0 */
	{0x05, 0x00, 0xca},
		{0x05, 0x80, 0xc1},
		/*  */
	{0x05, 0x04, 0xc2},
	{0x05, 0x00, 0xca},
		{0x05, 0x0, 0xc1},
		/*  */
	{0x05, 0x00, 0xc2},
	{0x05, 0x00, 0xca},
		{0x05, 0x40, 0xc1},
		/* */
	{0x05, 0x17, 0xc2},
	{0x05, 0x00, 0xca},
		{0x05, 0x80, 0xc1},
		/* */
	{0x05, 0x06, 0xc2},
	{0x05, 0x00, 0xca},
		{0x05, 0x80, 0xc1},
		/* */
	{0x05, 0x04, 0xc2},
	{0x05, 0x00, 0xca},

	{REQ_03_GLOBAL, 0x4c, IDX_03_GLOBAL_MISC3},
	{REQ_03_GLOBAL, 0x18, IDX_01_GLOBAL_MISC1},

	{0x06, 0x70, 0x51},
	{0x06, 0xbe, 0x53},
	{0x06, 0x71, 0x57},
	{0x06, 0x20, 0x58},
	{0x06, 0x05, 0x59},
	{0x06, 0x15, 0x5a},

	{REQ_04_COMPRESS, 0x00, 0x08},
	/* Compress = OFF (0x1 to turn on) */
	{REQ_04_COMPRESS, 0x12, 0x09},
	{REQ_04_COMPRESS, 0x21, 0x0a},
	{REQ_04_COMPRESS, 0x10, 0x0b},
	{REQ_04_COMPRESS, 0x21, 0x0c},
	{REQ_04_COMPRESS, 0x05, 0x00},
	/* was 5 (Image Type ? ) */
	{REQ_04_COMPRESS, 0x00, 0x01},

	{0x06, 0x3f, 0x01},

	{REQ_04_COMPRESS, 0x00, 0x04},
	{REQ_04_COMPRESS, 0x00, 0x05},
	{REQ_04_COMPRESS, 0x40, 0x06},
	{REQ_04_COMPRESS, 0x40, 0x07},

	{0x06, 0x1c, 0x17},
	{0x06, 0xe2, 0x19},
	{0x06, 0x1c, 0x1b},
	{0x06, 0xe2, 0x1d},
	{0x06, 0xaa, 0x1f},
	{0x06, 0x70, 0x20},

	{0x05, 0x01, 0x10},
	{0x05, 0x00, 0x11},
	{0x05, 0x01, 0x00},
	{0x05, 0x05, 0x01},
		{0x05, 0x00, 0xc1},
		/* */
	{0x05, 0x00, 0xc2},
	{0x05, 0x00, 0xca},

	{0x06, 0x70, 0x51},
	{0x06, 0xbe, 0x53},
	{}
};

/*
 * Made by Tomasz Zablocki (skalamandra@poczta.onet.pl)
 * SPCA505b chip based cameras initialization data
 */
/* jfm */
#define initial_brightness 0x7f	/* 0x0(white)-0xff(black) */
/* #define initial_brightness 0x0	//0x0(white)-0xff(black) */
/*
 * Data to initialize a SPCA505. Common to the CCD and external modes
 */
static const u8 spca505b_init_data[][3] = {
/* start */
	{REQ_02_USB, 0x00, 0x00},		/* init */
	{REQ_02_USB, 0x00, 0x01},
	{REQ_02_USB, 0x00, 0x02},
	{REQ_02_USB, 0x00, 0x03},
	{REQ_02_USB, 0x00, 0x04},
	{REQ_02_USB, 0x00, 0x05},
	{REQ_02_USB, 0x00, 0x06},
	{REQ_02_USB, 0x00, 0x07},
	{REQ_02_USB, 0x00, 0x08},
	{REQ_02_USB, 0x00, 0x09},
	{REQ_03_GLOBAL, 0x00, 0x00},
	{REQ_03_GLOBAL, 0x00, 0x01},
	{REQ_03_GLOBAL, 0x00, 0x02},
	{REQ_03_GLOBAL, 0x00, 0x03},
	{REQ_03_GLOBAL, 0x00, 0x04},
	{REQ_03_GLOBAL, 0x00, 0x05},
	{REQ_03_GLOBAL, 0x00, 0x06},
	{REQ_04_COMPRESS, 0x00, 0x00},
	{REQ_04_COMPRESS, 0x00, 0x02},
	{REQ_04_COMPRESS, 0x00, 0x04},
	{REQ_04_COMPRESS, 0x00, 0x05},
	{REQ_04_COMPRESS, 0x00, 0x06},
	{REQ_04_COMPRESS, 0x00, 0x07},
	{REQ_04_COMPRESS, 0x00, 0x08},
	{REQ_04_COMPRESS, 0x00, 0x09},
	{REQ_04_COMPRESS, 0x00, 0x0a},
	{REQ_04_COMPRESS, 0x00, 0x0b},
	{REQ_04_COMPRESS, 0x00, 0x0c},
	{0x07, 0x00, 0x00},
	{0x07, 0x00, 0x03},
	{0x08, 0x00, 0x00},
	{0x08, 0x00, 0x01},
	{0x08, 0x00, 0x02},
	{0x06, 0x18, 0x08},
	{0x06, 0xfc, 0x09},
	{0x06, 0xfc, 0x0a},
	{0x06, 0xfc, 0x0b},
	{0x06, 0x18, 0x0c},
	{0x06, 0xfc, 0x0d},
	{0x06, 0xfc, 0x0e},
	{0x06, 0xfc, 0x0f},
	{0x06, 0x18, 0x10},
	{0x06, 0xfe, 0x12},
	{0x06, 0x00, 0x11},
	{0x06, 0x00, 0x14},
	{0x06, 0x00, 0x13},
	{0x06, 0x28, 0x51},
	{0x06, 0xff, 0x53},
	{REQ_02_USB, 0x00, 0x08},

	{REQ_03_GLOBAL, 0x00, IDX_03_GLOBAL_MISC3},
	{REQ_03_GLOBAL, 0x10, IDX_03_GLOBAL_MISC3},
	{}
};

/*
 * Data to initialize the camera using the internal CCD
 */
static const u8 spca505b_open_data_ccd[][3] = {

/* {REQ_02_USB,0x00,0x00}, */
	{REQ_03_GLOBAL, 0x04, 0x01},		/* rst */
	{REQ_03_GLOBAL, 0x00, 0x01},
	{REQ_03_GLOBAL, 0x00, 0x00},
	{REQ_03_GLOBAL, 0x21, 0x00},
	{REQ_03_GLOBAL, 0x00, 0x04},
	{REQ_03_GLOBAL, 0x00, 0x03},
	{REQ_03_GLOBAL, 0x18, 0x03},
	{REQ_03_GLOBAL, 0x08, 0x01},
	{REQ_03_GLOBAL, 0x1c, 0x03},
	{REQ_03_GLOBAL, 0x5c, 0x03},
	{REQ_03_GLOBAL, 0x5c, 0x03},
	{REQ_03_GLOBAL, 0x18, 0x01},

/* same as 505 */
	{REQ_04_COMPRESS, 0x10, 0x01},
	{REQ_04_COMPRESS, 0x00, 0x04},
	{REQ_04_COMPRESS, 0x00, 0x05},
	{REQ_04_COMPRESS, 0x20, 0x06},
	{REQ_04_COMPRESS, 0x20, 0x07},

	{0x08, 0x0a, 0x00},

	{0x05, 0x00, 0x10},
	{0x05, 0x00, 0x11},
	{0x05, 0x00, 0x12},
	{0x05, 0x6f, 0x00},
	{0x05, initial_brightness >> 6, 0x00},
	{0x05, (initial_brightness << 2) & 0xff, 0x01},
	{0x05, 0x00, 0x02},
	{0x05, 0x01, 0x03},
	{0x05, 0x00, 0x04},
	{0x05, 0x03, 0x05},
	{0x05, 0xe0, 0x06},
	{0x05, 0x20, 0x07},
	{0x05, 0xa0, 0x08},
	{0x05, 0x00, 0x12},
	{0x05, 0x02, 0x0f},
	{0x05, 0x80, 0x14},		/* max exposure off (0=on) */
	{0x05, 0x01, 0xb0},
	{0x05, 0x01, 0xbf},
	{REQ_03_GLOBAL, 0x02, 0x06},
	{0x05, 0x10, 0x46},
	{0x05, 0x08, 0x4a},

	{0x06, 0x00, 0x01},
	{0x06, 0x10, 0x02},
	{0x06, 0x64, 0x07},
	{0x06, 0x18, 0x08},
	{0x06, 0xfc, 0x09},
	{0x06, 0xfc, 0x0a},
	{0x06, 0xfc, 0x0b},
	{REQ_04_COMPRESS, 0x00, 0x01},
	{0x06, 0x18, 0x0c},
	{0x06, 0xfc, 0x0d},
	{0x06, 0xfc, 0x0e},
	{0x06, 0xfc, 0x0f},
	{0x06, 0x11, 0x10},		/* contrast */
	{0x06, 0x00, 0x11},
	{0x06, 0xfe, 0x12},
	{0x06, 0x00, 0x13},
	{0x06, 0x00, 0x14},
	{0x06, 0x9d, 0x51},
	{0x06, 0x40, 0x52},
	{0x06, 0x7c, 0x53},
	{0x06, 0x40, 0x54},
	{0x06, 0x02, 0x57},
	{0x06, 0x03, 0x58},
	{0x06, 0x15, 0x59},
	{0x06, 0x05, 0x5a},
	{0x06, 0x03, 0x56},
	{0x06, 0x02, 0x3f},
	{0x06, 0x00, 0x40},
	{0x06, 0x39, 0x41},
	{0x06, 0x69, 0x42},
	{0x06, 0x87, 0x43},
	{0x06, 0x9e, 0x44},
	{0x06, 0xb1, 0x45},
	{0x06, 0xbf, 0x46},
	{0x06, 0xcc, 0x47},
	{0x06, 0xd5, 0x48},
	{0x06, 0xdd, 0x49},
	{0x06, 0xe3, 0x4a},
	{0x06, 0xe8, 0x4b},
	{0x06, 0xed, 0x4c},
	{0x06, 0xf2, 0x4d},
	{0x06, 0xf7, 0x4e},
	{0x06, 0xfc, 0x4f},
	{0x06, 0xff, 0x50},

	{0x05, 0x01, 0xc0},
	{0x05, 0x10, 0xcb},
	{0x05, 0x40, 0xc1},
	{0x05, 0x04, 0xc2},
	{0x05, 0x00, 0xca},
	{0x05, 0x40, 0xc1},
	{0x05, 0x09, 0xc2},
	{0x05, 0x00, 0xca},
	{0x05, 0xc0, 0xc1},
	{0x05, 0x09, 0xc2},
	{0x05, 0x00, 0xca},
	{0x05, 0x40, 0xc1},
	{0x05, 0x59, 0xc2},
	{0x05, 0x00, 0xca},
	{REQ_04_COMPRESS, 0x00, 0x01},
	{0x05, 0x80, 0xc1},
	{0x05, 0xec, 0xc2},
	{0x05, 0x0, 0xca},

	{0x06, 0x02, 0x57},
	{0x06, 0x01, 0x58},
	{0x06, 0x15, 0x59},
	{0x06, 0x0a, 0x5a},
	{0x06, 0x01, 0x57},
	{0x06, 0x8a, 0x03},
	{0x06, 0x0a, 0x6c},
	{0x06, 0x30, 0x01},
	{0x06, 0x20, 0x02},
	{0x06, 0x00, 0x03},

	{0x05, 0x8c, 0x25},

	{0x06, 0x4d, 0x51},		/* maybe saturation (4d) */
	{0x06, 0x84, 0x53},		/* making green (84) */
	{0x06, 0x00, 0x57},		/* sharpness (1) */
	{0x06, 0x18, 0x08},
	{0x06, 0xfc, 0x09},
	{0x06, 0xfc, 0x0a},
	{0x06, 0xfc, 0x0b},
	{0x06, 0x18, 0x0c},		/* maybe hue (18) */
	{0x06, 0xfc, 0x0d},
	{0x06, 0xfc, 0x0e},
	{0x06, 0xfc, 0x0f},
	{0x06, 0x18, 0x10},		/* maybe contrast (18) */

	{0x05, 0x01, 0x02},

	{REQ_04_COMPRESS, 0x00, 0x08},		/* compression */
	{REQ_04_COMPRESS, 0x12, 0x09},
	{REQ_04_COMPRESS, 0x21, 0x0a},
	{REQ_04_COMPRESS, 0x10, 0x0b},
	{REQ_04_COMPRESS, 0x21, 0x0c},
	{REQ_04_COMPRESS, 0x1d, 0x00},		/* imagetype (1d) */
	{REQ_04_COMPRESS, 0x41, 0x01},		/* hardware snapcontrol */

	{REQ_04_COMPRESS, 0x00, 0x04},
	{REQ_04_COMPRESS, 0x00, 0x05},
	{REQ_04_COMPRESS, 0x10, 0x06},
	{REQ_04_COMPRESS, 0x10, 0x07},
	{REQ_04_COMPRESS, 0x40, 0x06},
	{REQ_04_COMPRESS, 0x40, 0x07},
	{REQ_04_COMPRESS, 0x00, 0x04},
	{REQ_04_COMPRESS, 0x00, 0x05},

	{0x06, 0x1c, 0x17},
	{0x06, 0xe2, 0x19},
	{0x06, 0x1c, 0x1b},
	{0x06, 0xe2, 0x1d},
	{0x06, 0x5f, 0x1f},
	{0x06, 0x32, 0x20},

	{0x05, initial_brightness >> 6, 0x00},
	{0x05, (initial_brightness << 2) & 0xff, 0x01},
	{0x05, 0x06, 0xc1},
	{0x05, 0x58, 0xc2},
	{0x05, 0x00, 0xca},
	{0x05, 0x00, 0x11},
	{}
};

static void reg_w_riv(struct gspca_dev *gspca_dev,
		     u16 req, u16 index, u16 value)
{
	int ret;

	if (gspca_dev->usb_err < 0)
		return;
	ret = usb_control_msg(gspca_dev->dev,
			usb_sndctrlpipe(gspca_dev->dev, 0),
			req,
			USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			value, index, NULL, 0, 500);
	if (ret < 0) {
		pr_err("reg write: error %d\n", ret);
		gspca_dev->usb_err = ret;
	}
}
/*fixme: quick hack*/
#define reg_w(d, r, v, i) reg_w_riv(d, r, i, v)

/* returns: negative is error, pos or zero is data */
static int reg_read(struct gspca_dev *gspca_dev,
			u8 req,		/* bRequest */
			u16 index)	/* wIndex */
{
	int ret;

	if (gspca_dev->usb_err < 0)
		return -1;
	ret = usb_control_msg(gspca_dev->dev,
			usb_rcvctrlpipe(gspca_dev->dev, 0),
			req,
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0,			/* value */
			index,
			gspca_dev->usb_buf, 2,
			500);			/* timeout */
	if (ret < 0) {
		pr_err("reg_read err %d\n", ret);
		gspca_dev->usb_err = ret;
		return -1;
	}
	return (gspca_dev->usb_buf[1] << 8) + gspca_dev->usb_buf[0];
}

static void write_vector(struct gspca_dev *gspca_dev,
			const u8 (*data)[3])
{
	while ((*data)[0] != 0) {
		reg_w_riv(gspca_dev, (*data)[0], (*data)[2], (*data)[1]);
		data++;
	}
}

static void i2c_w(struct gspca_dev *gspca_dev,
			u8 valeur, u8 reg)
{
	int ret, retry = 60;

	reg_w_riv(gspca_dev, REQ_07_SSC, IDX_01_SSC_ADDR, reg);
	reg_w_riv(gspca_dev, REQ_07_SSC, IDX_00_SSC_DATA, valeur);
	while (retry--) {
		ret = reg_read(gspca_dev, REQ_07_SSC, IDX_03_SSC_BUSY);
		if (ret == 0)
			return;
	}
	err("i2c_w timeout");
}

static void i2c_w_buf(struct gspca_dev *gspca_dev,
			const u8 (*data)[2])
{
	while ((*data)[1] != 0) {
		i2c_w(gspca_dev, (*data)[0], (*data)[1]);
		data++;
	}
}

static void setinputstd(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int i, channel;
	u8 parm0;
	static const int channel_tb[ARRAY_SIZE(sd_inputs)] =
		{-1, 0, 1, 2, 3, 6, 7, 8, 9};

	i = gspca_dev->curr_input;
	if (sd->subtype == ST_GRABBER)
		i++;			/* (no webcam) */
	channel = channel_tb[i];

	/* NTSC bit0 -> 1 (525 l) - PAL SECAM bit0 -> 0 (625 l) */
	/* Composite channel bit1 -> 1 - S-video bit 1 -> 0 */
	parm0 = 0x48;
	if (gspca_dev->curr_std & V4L2_STD_NTSC)
		parm0 |= 0x01;
	if (channel < 4)
		parm0 |= 0x02;
	reg_w_riv(gspca_dev, REQ_08_TV_IN, IDX_00_TV_IN_PARM0, parm0);

	i2c_w(gspca_dev, 0xc0 | channel, 0x02);
	if (gspca_dev->curr_std & V4L2_STD_NTSC)
		i2c_w(gspca_dev, 0x33, 0x0e);
					/* Chrominance Control NTSC N */
	else if (gspca_dev->curr_std & V4L2_STD_SECAM)
		i2c_w(gspca_dev, 0x53, 0x0e);
					/* Chrominance Control SECAM */
	else
		i2c_w(gspca_dev, 0x03, 0x0e);
					/* Chrominance Control PAL BGHIV */
}

static void spca506_Setsize(struct gspca_dev *gspca_dev, u16 code,
					u16 xmult, u16 ymult)
{
	/* image type */
	reg_w(gspca_dev, REQ_04_COMPRESS, (0x18 | (code & 0x07)), 0x00);
	/* Soft snap 0x40 Hard 0x41 */
	reg_w(gspca_dev, REQ_04_COMPRESS, 0x41, 0x01);
	reg_w(gspca_dev, REQ_04_COMPRESS, 0x00, 0x02);
	/* reserved */
	reg_w(gspca_dev, REQ_04_COMPRESS, 0x00, 0x03);
	/* reserved */
	reg_w(gspca_dev, REQ_04_COMPRESS, 0x00, 0x04);
	/* reserved */
	reg_w(gspca_dev, REQ_04_COMPRESS, 0x01, 0x05);
	/* reserved */
	reg_w(gspca_dev, REQ_04_COMPRESS, xmult, 0x06);
	/* reserved */
	reg_w(gspca_dev, REQ_04_COMPRESS, ymult, 0x07);
	/* compression 1 */
	reg_w(gspca_dev, REQ_04_COMPRESS, 0x00, 0x08);
	/* T=64 -> 2 */
	reg_w(gspca_dev, REQ_04_COMPRESS, 0x00, 0x09);
	/* threshold2D */
	reg_w(gspca_dev, REQ_04_COMPRESS, 0x21, 0x0a);
	/* quantization */
	reg_w(gspca_dev, REQ_04_COMPRESS, 0x00, 0x0b);
}

/* this function is called at probe time */
static int sd_config(struct gspca_dev *gspca_dev,
			const struct usb_device_id *id)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct cam *cam;

	sd->subtype = id->driver_info;

	cam = &gspca_dev->cam;
	cam->cam_mode = vga_mode;
	cam->inputs = sd_inputs;
	cam->nmodes = ARRAY_SIZE(vga_mode);
	cam->ninputs = ARRAY_SIZE(sd_inputs);
	switch (sd->subtype) {
	case ST_GRABBER:
		cam->ninputs--;		/* no webcam */
		cam->inputs++;
		break;
	case IntelPCCameraPro:
		cam->nmodes--;		/* no 640x480 */
		break;
	default:
/*	case Nxultra: */
		cam->ninputs = 1;	/* webcam only */
		break;
	}
	gspca_dev->cam.ctrls = sd->ctrls;

	if (sd->subtype != ST_GRABBER)
		gspca_dev->ctrl_dis = (1 << CONTRAST)
					| (1 << COLORS)
					| (1 << HUE);
	return 0;
}

/* this function is called at probe and resume time */
static int sd_init(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	static const u8 (*init_tb[])[3] = {
		[ST_GRABBER] =		grabber_bridge_init1,
		[IntelPCCameraPro] =	spca505_init_data,
		[Nxultra] =		spca505b_init_data,
	};

	write_vector(gspca_dev, init_tb[sd->subtype]);

	reg_w(gspca_dev, REQ_07_SSC,
			SAA7113_I2C_ADDR, IDX_04_SSC_SLAVE_ADDR);

	if (sd->subtype == ST_GRABBER
	 || sd->subtype == IntelPCCameraPro) {
		reg_w(gspca_dev, REQ_07_SSC,
				SAA7113_I2C_ADDR, IDX_04_SSC_SLAVE_ADDR);
		setinputstd(gspca_dev);
		write_vector(gspca_dev, grabber_bridge_init2);
		i2c_w_buf(gspca_dev, grabber_saa7113_init);
	}
	return gspca_dev->usb_err;
}

static void setbrightness(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	u8 brightness = sd->ctrls[BRIGHTNESS].val;

	if (sd->subtype == ST_GRABBER) {
		i2c_w(gspca_dev, brightness, SAA7113_bright);
		i2c_w(gspca_dev, 0x01, 0x09);
	} else {
		reg_w_riv(gspca_dev, 0x05, 0x00, (255 - brightness) >> 6);
		reg_w_riv(gspca_dev, 0x05, 0x01, (255 - brightness) << 2);
	}
}

static void setcontrast(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	i2c_w(gspca_dev, sd->ctrls[CONTRAST].val, SAA7113_contrast);
	i2c_w(gspca_dev, 0x01, 0x09);
}

static void setcolors(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	i2c_w(gspca_dev, sd->ctrls[COLORS].val, SAA7113_saturation);
	i2c_w(gspca_dev, 0x01, 0x09);
}

static void sethue(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	i2c_w(gspca_dev, sd->ctrls[HUE].val, SAA7113_hue);
	i2c_w(gspca_dev, 0x01, 0x09);
}

static int sd_start(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int ret, mode;
	static const u8 mode_tb[][3] = {
	/*	  r00   r06   r07	*/
		{0x00, 0x10, 0x10},	/* 640x480 */
		{0x01, 0x1a, 0x1a},	/* 352x288 */
#if 1
		{0x02, 0x1c, 0x1c},	/* 320x240 */
#else
		{0x02, 0x1c, 0x1d},	/* 320x240 */
#endif
		{0x04, 0x34, 0x34},	/* 176x144 */
		{0x05, 0x40, 0x40}	/* 160x120 */
	};
	static const u8 (*start_tb[])[3] = {
		[ST_GRABBER] =		grabber_bridge_start1,
		[IntelPCCameraPro] =	spca505_open_data_ccd,
		[Nxultra] =		spca505b_open_data_ccd,
	};

	if (gspca_dev->curr_std != 0) {

		/* TV */
		write_vector(gspca_dev, start_tb[ST_GRABBER]);
		write_vector(gspca_dev, grabber_bridge_start1);
		i2c_w_buf(gspca_dev, grabber_saa7113_start);
		write_vector(gspca_dev, grabber_bridge_start2);
		mode = gspca_dev->cam.cam_mode[gspca_dev->curr_mode].priv;
		spca506_Setsize(gspca_dev, mode_tb[mode][0],
					mode_tb[mode][1],
					mode_tb[mode][2]);
		reg_w(gspca_dev, REQ_02_USB, USB_CTRL_ISO_ENABLE,
						IDX_00_USB_CTRL);
		reg_w(gspca_dev, REQ_03_GLOBAL, 0x12, IDX_01_GLOBAL_MISC1);
		reg_read(gspca_dev, REQ_04_COMPRESS, 0x0001);
		setinputstd(gspca_dev);
	} else {

		/* webcam */
		write_vector(gspca_dev, start_tb[sd->subtype]);
		ret = reg_read(gspca_dev, 0x06, 0x16);
		if (ret < 0)
			return gspca_dev->usb_err;
		if (ret != 0x0101) {
			pr_err("After vector read returns 0x%04x should be 0x0101\n",
				ret);
			gspca_dev->usb_err = -EIO;
			return gspca_dev->usb_err;
		}

		reg_w_riv(gspca_dev, 0x06, 0x16, 0x0a);
		reg_w_riv(gspca_dev, 0x05, 0xc2, 0x12);

		/* necessary because without it we can see stream
		 * only once after loading module */
		/* stopping usb registers Tomasz change */
		reg_w_riv(gspca_dev, REQ_02_USB, IDX_00_USB_CTRL, 0x00);

		mode = gspca_dev->cam.cam_mode[gspca_dev->curr_mode].priv;
		reg_w_riv(gspca_dev, REQ_04_COMPRESS, 0x00, mode_tb[mode][0]);
		reg_w_riv(gspca_dev, REQ_04_COMPRESS, 0x06, mode_tb[mode][1]);
		reg_w_riv(gspca_dev, REQ_04_COMPRESS, 0x07, mode_tb[mode][2]);
		reg_w_riv(gspca_dev, REQ_02_USB, IDX_00_USB_CTRL,
						USB_CTRL_ISO_ENABLE);
		setbrightness(gspca_dev);
	}
	return gspca_dev->usb_err;
}

static void sd_stopN(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	/* Disable ISO packet machine */
	reg_w_riv(gspca_dev, REQ_02_USB, IDX_00_USB_CTRL, 0x00);
	if (sd->subtype == ST_GRABBER) {
		reg_w(gspca_dev, REQ_03_GLOBAL, 0x00, 0x0004);
		reg_w(gspca_dev, REQ_03_GLOBAL, 0x00, IDX_03_GLOBAL_MISC3);
	}
}

/* called on streamoff with alt 0 and on disconnect */
static void sd_stop0(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (!gspca_dev->present)
		return;
	if (sd->subtype == ST_GRABBER)
		return;

	/* This maybe reset or power control */
	reg_w_riv(gspca_dev, REQ_03_GLOBAL, IDX_03_GLOBAL_MISC3,
						GLOBAL_MISC3_SAA7113RST);
	reg_w_riv(gspca_dev, REQ_03_GLOBAL, IDX_01_GLOBAL_MISC1,
						0x00);
	reg_w_riv(gspca_dev, REQ_03_GLOBAL, IDX_00_GLOBAL_MISC0,
						GLOBAL_MISC0_IDSEL);
	reg_w_riv(gspca_dev, 0x05, 0x10, 0x01);
	reg_w_riv(gspca_dev, 0x05, 0x11, 0x0f);
}

static void sd_pkt_scan(struct gspca_dev *gspca_dev,
			u8 *data,			/* isoc packet */
			int len)			/* iso packet length */
{
	switch (data[0]) {
	case 0:				/* start of frame */
		gspca_frame_add(gspca_dev, LAST_PACKET, NULL, 0);
		data += SPCA50X_OFFSET_DATA;
		len -= SPCA50X_OFFSET_DATA;
		gspca_frame_add(gspca_dev, FIRST_PACKET, data, len);
		break;
	case 0xff:			/* drop */
		break;
	default:
		data += 1;
		len -= 1;
		gspca_frame_add(gspca_dev, INTER_PACKET, data, len);
		break;
	}
}

static void sd_set_misc(struct gspca_dev *gspca_dev,
			int idx)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int i;

	switch (idx) {
	case MISC_INPUT:
		i = gspca_dev->curr_input;
		if (sd->subtype == ST_GRABBER)
			i++;		/* (no webcam) */
		gspca_dev->cam.std = sd_inputs[i].std;
		if (gspca_dev->cam.std == 0)
			gspca_dev->curr_std = 0;
		else if (gspca_dev->curr_std == 0)
			gspca_dev->curr_std = V4L2_STD_PAL;
		/* fall thru */
	case MISC_STD:

		/* don't change input or standard while streaming */
		if (gspca_dev->streaming)
			break;
		setinputstd(gspca_dev);
		break;
	default:
		gspca_dev->usb_err = -EINVAL;
		break;
	}
}

/* sub-driver description */
static const struct sd_desc sd_desc = {
	.name = MODULE_NAME,
	.ctrls = sd_ctrls,
	.nctrls = NCTRLS,
	.config = sd_config,
	.init = sd_init,
	.start = sd_start,
	.stopN = sd_stopN,
	.stop0 = sd_stop0,
	.pkt_scan = sd_pkt_scan,
	.set_misc = sd_set_misc,
};

/* -- module initialisation -- */
static const struct usb_device_id device_table[] = {
	{USB_DEVICE(0x041e, 0x401d), .driver_info = Nxultra},
	{USB_DEVICE(0x06e1, 0xa190), .driver_info = ST_GRABBER},
	{USB_DEVICE(0x0733, 0x0430), .driver_info = IntelPCCameraPro},
	{USB_DEVICE(0x0734, 0x043b), .driver_info = ST_GRABBER},
	{USB_DEVICE(0x99fa, 0x8988), .driver_info = ST_GRABBER},
	{}
};
MODULE_DEVICE_TABLE(usb, device_table);

/* -- device connect -- */
static int sd_probe(struct usb_interface *intf,
			const struct usb_device_id *id)
{
	return gspca_dev_probe(intf, id, &sd_desc, sizeof(struct sd),
				THIS_MODULE);
}

static struct usb_driver sd_driver = {
	.name = MODULE_NAME,
	.id_table = device_table,
	.probe = sd_probe,
	.disconnect = gspca_disconnect,
#ifdef CONFIG_PM
	.suspend = gspca_suspend,
	.resume = gspca_resume,
#endif
};

module_usb_driver(sd_driver);
