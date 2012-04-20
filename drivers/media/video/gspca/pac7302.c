/*
 * Pixart PAC7302 driver
 *
 * Copyright (C) 2008-2012 Jean-Francois Moine <http://moinejf.free.fr>
 * Copyright (C) 2005 Thomas Kaiser thomas@kaiser-linux.li
 *
 * Separated from Pixart PAC7311 library by Márton Németh
 * Camera button input handling by Márton Németh <nm127@freemail.hu>
 * Copyright (C) 2009-2010 Márton Németh <nm127@freemail.hu>
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
 */

/* Some documentation about various registers as determined by trial and error.

   Register page 1:

   Address	Description
   0x78		Global control, bit 6 controls the LED (inverted)

   Register page 3:

   Address	Description
   0x02		Clock divider 3-63, fps = 90 / val. Must be a multiple of 3 on
		the 7302, so one of 3, 6, 9, ..., except when between 6 and 12?
   0x03		Variable framerate ctrl reg2==3: 0 -> ~30 fps, 255 -> ~22fps
   0x04		Another var framerate ctrl reg2==3, reg3==0: 0 -> ~30 fps,
		63 -> ~27 fps, the 2 msb's must always be 1 !!
   0x05		Another var framerate ctrl reg2==3, reg3==0, reg4==0xc0:
		1 -> ~30 fps, 2 -> ~20 fps
   0x0e		Exposure bits 0-7, 0-448, 0 = use full frame time
   0x0f		Exposure bit 8, 0-448, 448 = no exposure at all
   0x10		Master gain 0-31
   0x21		Bitfield: 0-1 unused, 2-3 vflip/hflip, 4-5 unknown, 6-7 unused

   The registers are accessed in the following functions:

   Page | Register   | Function
   -----+------------+---------------------------------------------------
    0   | 0x0f..0x20 | setcolors()
    0   | 0xa2..0xab | setbrightcont()
    0   | 0xc4..0xc7 | setwhitebalance()
    0   | 0xdc       | setbrightcont(), setcolors()
    1	| 0x80       | setexposure()
    3   | 0x03/0x80  | setexposure()
    3   | 0x04/0x05  | setgain()
    3   | 0x11       | setcolors(), setgain(), setexposure(), sethvflip()
    3	| 0x12       | setgain()
    3   | 0x21       | sethvflip()

- (jfm) from ms-win traces:
	- the autogain/autoexposure uses the registers
		0-b6,
		1-80,
		3-03, 04, 05, 10, 12, 80
	- the values of these registers are correlated
*/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/input.h>
#include <media/v4l2-chip-ident.h>
#include "gspca.h"

//new autogain
#define NEWAGC 1
//+ new new agc
#define NEWAGC2 1
//new pkt_scan
#define NEWPKT

#ifndef NEWPKT
/* Include pac common sof detection functions */
#include "pac_common.h"
#endif

MODULE_AUTHOR("Jean-Francois Moine <http://moinejf.free.fr>, "
		"Thomas Kaiser thomas@kaiser-linux.li");
MODULE_DESCRIPTION("Pixart PAC7302");
MODULE_LICENSE("GPL");


enum e_ctrl {
	BRIGHTNESS,
	CONTRAST,
	COLORS,
	WHITE_BALANCE,
#ifndef NEWAGC
	GAIN,
#endif
	AUTOGAIN,
	EXPOSURE,
	VFLIP,
	HFLIP,
	NCTRLS		/* number of controls */
};

/* specific webcam descriptor for pac7302 */
struct sd {
	struct gspca_dev gspca_dev;		/* !! must be the first item */

	struct gspca_ctrl ctrls[NCTRLS];

#ifdef NEWAGC
	u8 old_expo;
#endif

	u8 flags;
#define FL_HFLIP 0x01		/* mirrored by default */
#define FL_VFLIP 0x02		/* vertical flipped by default */

#ifndef NEWPKT
	u8 sof_read;
#endif
	s8 autogain_ignore_frames;
#ifdef NEWAGC
#define AGC_CNT 5		/* counter of frames for autogain */
#endif

	atomic_t avg_lum;
};

/* V4L2 controls supported by the driver */
static void setbrightcont(struct gspca_dev *gspca_dev);
static void setcolors(struct gspca_dev *gspca_dev);
static void setwhitebalance(struct gspca_dev *gspca_dev);
#ifndef NEWAGC
static void setgain(struct gspca_dev *gspca_dev);
#endif
static void setexposure(struct gspca_dev *gspca_dev);
static void setautogain(struct gspca_dev *gspca_dev);
static void sethvflip(struct gspca_dev *gspca_dev);

static const struct ctrl sd_ctrls[] = {
[BRIGHTNESS] = {
	    {
		.id      = V4L2_CID_BRIGHTNESS,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Brightness",
		.minimum = 0,
#define BRIGHTNESS_MAX 0x20
		.maximum = BRIGHTNESS_MAX,
		.step    = 1,
		.default_value = 0x10,
	    },
	    .set_control = setbrightcont
	},
[CONTRAST] = {
	    {
		.id      = V4L2_CID_CONTRAST,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Contrast",
		.minimum = 0,
#define CONTRAST_MAX 255
		.maximum = CONTRAST_MAX,
		.step    = 1,
		.default_value = 127,
	    },
	    .set_control = setbrightcont
	},
[COLORS] = {
	    {
		.id      = V4L2_CID_SATURATION,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Saturation",
		.minimum = 0,
#define COLOR_MAX 255
		.maximum = COLOR_MAX,
		.step    = 1,
		.default_value = 127
	    },
	    .set_control = setcolors
	},
[WHITE_BALANCE] = {
	    {
		.id      = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "White Balance",
		.minimum = -80,
		.maximum = 80,
		.step    = 1,
		.default_value = 0,
	    },
	    .set_control = setwhitebalance
	},
#ifndef NEWAGC
[GAIN] = {
	    {
		.id      = V4L2_CID_GAIN,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Gain",
#define GAIN_DEF 127
#define GAIN_MAX 255
#define GAIN_KNEE 255 /* Gain seems to cause little noise on the pac73xx */
		.minimum = 0,
		.maximum = GAIN_MAX,
		.step    = 1,
		.default_value = GAIN_DEF,
	    },
	    .set_control = setgain
	},
#endif
[EXPOSURE] = {
	    {
		.id      = V4L2_CID_EXPOSURE,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Exposure",
		.minimum = 0,
#ifndef NEWAGC
		.maximum = 1023,
		.step    = 1,
#define EXPOSURE_DEF  66  /*  33 ms / 30 fps */
#define EXPOSURE_KNEE 133 /*  66 ms / 15 fps */
#else	/* NEWAGC */
#define EXPOSURE_MAX  0x3f
		.maximum = EXPOSURE_MAX,
		.step    = 1,
#define EXPOSURE_DEF  4
#endif
		.default_value = EXPOSURE_DEF,
	    },
	    .set_control = setexposure
	},
[AUTOGAIN] = {
	    {
		.id      = V4L2_CID_AUTOGAIN,
		.type    = V4L2_CTRL_TYPE_BOOLEAN,
		.name    = "Auto Gain",
		.minimum = 0,
		.maximum = 1,
		.step    = 1,
#define AUTOGAIN_DEF 1
		.default_value = AUTOGAIN_DEF,
	    },
	    .set_control = setautogain,
	},
[HFLIP] = {
	    {
		.id      = V4L2_CID_HFLIP,
		.type    = V4L2_CTRL_TYPE_BOOLEAN,
		.name    = "Mirror",
		.minimum = 0,
		.maximum = 1,
		.step    = 1,
		.default_value = 0,
	    },
	    .set_control = sethvflip,
	},
[VFLIP] = {
	    {
		.id      = V4L2_CID_VFLIP,
		.type    = V4L2_CTRL_TYPE_BOOLEAN,
		.name    = "Vflip",
		.minimum = 0,
		.maximum = 1,
		.step    = 1,
		.default_value = 0,
	    },
	    .set_control = sethvflip
	},
};

static const struct v4l2_pix_format vga_mode[] = {
	{640, 480, V4L2_PIX_FMT_PJPG, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
	},
};

#define LOAD_PAGE3		255
#define END_OF_SEQUENCE		0

/* pac 7302 */
static const u8 init_7302[] = {
/*	index,value */
	0xff, 0x01,		/* page 1 */
	0x78, 0x00,		/* deactivate */
	0xff, 0x01,
	0x78, 0x40,		/* led off */
};
static const u8 start_7302[] = {
/*	index, len, [value]* */
	0xff, 1,	0x00,		/* page 0 */
	0x00, 12,	0x01, 0x40, 0x40, 0x40, 0x01, 0xe0, 0x02, 0x80,
			0x00, 0x00, 0x00, 0x00,
//--fixme: setcolors (start 0f..20) without page nor dc 01
	0x0d, 24,	0x03, 0x01, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x11,
	0x26, 2,	0xaa, 0xaa,
	0x2e, 1,	0x31,
	0x38, 1,	0x01,
	0x3a, 3,	0x14, 0xff, 0x5a,
	0x43, 11,	0x00, 0x0a, 0x18, 0x11, 0x01, 0x2c, 0x88, 0x11,
			0x00, 0x54, 0x11,
	0x55, 1,	0x00,
	0x62, 4,	0x10, 0x1e, 0x1e, 0x18,
	0x6b, 1,	0x00,
	0x6e, 3,	0x08, 0x06, 0x00,
	0x72, 3,	0x00, 0xff, 0x00,
	0x7d, 23,	0x01, 0x01, 0x58, 0x46, 0x50, 0x3c, 0x50, 0x3c,
			0x54, 0x46, 0x54, 0x56, 0x52, 0x50, 0x52, 0x50,
			0x56, 0x64, 0xa4, 0x00, 0xda, 0x00, 0x00,
//--fixme: setbrightcont without page nor dc 01
	0xa2, 10,	0x08, 0x11, 0x1f, 0x37, 0x4a, 0x5d, 0x7d, 0x9a,
			0xb6, 0xcf,
	0xaf, 1,	0x02,
	0xb5, 2,	0x08, 0x08,
	0xb8, 2,	0x08, 0x88,
//--fixme: setwhitebalance
	0xc4, 4,	0xae, 0x01, 0x04, 0x01,
	0xcc, 1,	0x00,
	0xd1, 11,	0x01, 0x30, 0x49, 0x5e, 0x6f, 0x7f, 0x8e, 0xa9,
			0xc1, 0xd7, 0xec,
	0xdc, 1,	0x01,
	0xff, 1,	0x01,		/* page 1 */
	0x12, 3,	0x02, 0x00, 0x01,
	0x3e, 2,	0x00, 0x00,
	0x76, 5,	0x01, 0x20, 0x40, 0x00, 0xf2,
	0x7c, 1,	0x00,
	0x7f, 10,	0x4b, 0x0b, 0x01, 0x2c, 0x02, 0x58, 0x03, 0x20,
			0x02, 0x00,
	0x96, 5,	0x01, 0x10, 0x04, 0x01, 0x04,
	0xc8, 14,	0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00,
			0x07, 0x00, 0x01, 0x07, 0x04, 0x01,
	0xd8, 1,	0x01,
	0xdb, 2,	0x00, 0x01,
	0xde, 7,	0x00, 0x01, 0x04, 0x04, 0x00, 0x00, 0x00,
	0xe6, 4,	0x00, 0x00, 0x00, 0x01,
	0xeb, 1,	0x00,
	0xff, 1,	0x02,		/* page 2 */
	0x22, 1,	0x00,
	0xff, 1,	0x03,		/* page 3 */
	0, LOAD_PAGE3,			/* load the page 3 */
	0x11, 1,	0x01,
	0xff, 1,	0x02,		/* page 2 */
	0x13, 1,	0x00,
	0x22, 4,	0x1f, 0xa4, 0xf0, 0x96,
	0x27, 2,	0x14, 0x0c,
	0x2a, 5,	0xc8, 0x00, 0x18, 0x12, 0x22,
	0x64, 8,	0x00, 0x00, 0xf0, 0x01, 0x14, 0x44, 0x44, 0x44,
	0x6e, 1,	0x08,
	0xff, 1,	0x01,		/* page 1 */
	0x78, 1,	0x00,
	0, END_OF_SEQUENCE		/* end of sequence */
};

#define SKIP		0xaa
/* page 3 - the value SKIP says skip the index - see reg_w_page() */
static const u8 page3_7302[] = {
	0x90, 0x40, 0x03, 0xf4, 0xe7, 0x01, 0x14, 0x16,
	0x14, 0x12, 0x00, 0x00, 0x00, 0x02, 0x02, 0x00,
	0x0f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x47, 0x01, 0xb3, 0x01, 0x00,
//--fixme: 21 = sethvflip
	0x00, 0x08, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x21,
	0x00, 0x00, 0x00, 0x54, 0xf4, 0x02, 0x52, 0x54,
	0xa4, 0xb8, 0xe0, 0x2a, 0xf6, 0x00, 0x00, 0x00,
	0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xfc, 0x00, 0xf2, 0x1f, 0x04, 0x00, 0x00,
	SKIP, 0x00, 0x00, 0xc0, 0xc0, 0x10, 0x00, 0x00,
	0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x40, 0xff, 0x03, 0x19, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0xc8, 0xc8,
	0xc8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50,
	0x08, 0x10, 0x24, 0x40, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x02, 0x47, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x02, 0xfa, 0x00, 0x64, 0x5a, 0x28, 0x00,
	0x00
};

static void reg_w_buf(struct gspca_dev *gspca_dev,
		u8 index,
		  const u8 *buffer, int len)
{
	int ret;

	if (gspca_dev->usb_err < 0)
		return;
	memcpy(gspca_dev->usb_buf, buffer, len);
	ret = usb_control_msg(gspca_dev->dev,
			usb_sndctrlpipe(gspca_dev->dev, 0),
			0,		/* request */
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0,		/* value */
			index, gspca_dev->usb_buf, len,
			500);
	if (ret < 0) {
		pr_err("reg_w_buf failed i: %02x error %d\n",
		       index, ret);
		gspca_dev->usb_err = ret;
	}
}


static void reg_w(struct gspca_dev *gspca_dev,
		u8 index,
		u8 value)
{
	int ret;

	if (gspca_dev->usb_err < 0)
		return;
	gspca_dev->usb_buf[0] = value;
	ret = usb_control_msg(gspca_dev->dev,
			usb_sndctrlpipe(gspca_dev->dev, 0),
			0,			/* request */
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0, index, gspca_dev->usb_buf, 1,
			500);
	if (ret < 0) {
		pr_err("reg_w() failed i: %02x v: %02x error %d\n",
		       index, value, ret);
		gspca_dev->usb_err = ret;
	}
}

static void reg_w_seq(struct gspca_dev *gspca_dev,
		const u8 *seq, int len)
{
	while (--len >= 0) {
		reg_w(gspca_dev, seq[0], seq[1]);
		seq += 2;
	}
}

/* load the beginning of a page */
static void reg_w_page(struct gspca_dev *gspca_dev,
			const u8 *page, int len)
{
	int index;
	int ret = 0;

	if (gspca_dev->usb_err < 0)
		return;
	for (index = 0; index < len; index++) {
		if (page[index] == SKIP)		/* skip this index */
			continue;
		gspca_dev->usb_buf[0] = page[index];
		ret = usb_control_msg(gspca_dev->dev,
				usb_sndctrlpipe(gspca_dev->dev, 0),
				0,			/* request */
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
				0, index, gspca_dev->usb_buf, 1,
				500);
		if (ret < 0) {
			pr_err("reg_w_page() failed i: %02x v: %02x error %d\n",
			       index, page[index], ret);
			gspca_dev->usb_err = ret;
			break;
		}
	}
}

/* output a variable sequence */
static void reg_w_var(struct gspca_dev *gspca_dev,
			const u8 *seq,
			const u8 *page3, unsigned int page3_len)
{
	int index, len;

	for (;;) {
		index = *seq++;
		len = *seq++;
		switch (len) {
		case END_OF_SEQUENCE:
			return;
		case LOAD_PAGE3:
			reg_w_page(gspca_dev, page3, page3_len);
			break;
		default:
#ifdef GSPCA_DEBUG
			if (len > USB_BUF_SZ) {
				PDEBUG(D_ERR|D_STREAM,
					"Incorrect variable sequence");
				return;
			}
#endif
			while (len > 0) {
				if (len < 8) {
					reg_w_buf(gspca_dev,
						index, seq, len);
					seq += len;
					break;
				}
				reg_w_buf(gspca_dev, index, seq, 8);
				seq += 8;
				index += 8;
				len -= 8;
			}
		}
	}
	/* not reached */
}

/* this function is called at probe time for pac7302 */
static int sd_config(struct gspca_dev *gspca_dev,
			const struct usb_device_id *id)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct cam *cam;

	cam = &gspca_dev->cam;

	cam->cam_mode = vga_mode;	/* only 640x480 */
	cam->nmodes = ARRAY_SIZE(vga_mode);

	gspca_dev->cam.ctrls = sd->ctrls;

	sd->flags = id->driver_info;
	return 0;
}

/* This function is used by pac7302 only */
static void setbrightcont(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int i, v;
	static const u8 max[10] =
		{0x29, 0x33, 0x42, 0x5a, 0x6e, 0x80, 0x9f, 0xbb,
		 0xd4, 0xec};
	static const u8 delta[10] =
		{0x35, 0x33, 0x33, 0x2f, 0x2a, 0x25, 0x1e, 0x17,
		 0x11, 0x0b};

	reg_w(gspca_dev, 0xff, 0x00);		/* page 0 */
	for (i = 0; i < 10; i++) {
		v = max[i];
		v += (sd->ctrls[BRIGHTNESS].val - BRIGHTNESS_MAX)
			* 150 / BRIGHTNESS_MAX;		/* 200 ? */
		v -= delta[i] * sd->ctrls[CONTRAST].val / CONTRAST_MAX;
		if (v < 0)
			v = 0;
		else if (v > 0xff)
			v = 0xff;
		reg_w(gspca_dev, 0xa2 + i, v);
	}
	reg_w(gspca_dev, 0xdc, 0x01);
}

/* This function is used by pac7302 only */
static void setcolors(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int i, v;
	static const int a[9] =
		{217, -212, 0, -101, 170, -67, -38, -315, 355};
	static const int b[9] =
		{19, 106, 0, 19, 106, 1, 19, 106, 1};

	reg_w(gspca_dev, 0xff, 0x03);			/* page 3 */
	reg_w(gspca_dev, 0x11, 0x01);
	reg_w(gspca_dev, 0xff, 0x00);			/* page 0 */
	for (i = 0; i < 9; i++) {
		v = a[i] * sd->ctrls[COLORS].val / COLOR_MAX + b[i];
		reg_w(gspca_dev, 0x0f + 2 * i, (v >> 8) & 0x07);
		reg_w(gspca_dev, 0x0f + 2 * i + 1, v);
	}
	reg_w(gspca_dev, 0xdc, 0x01);
}

/*
 * The registers c4/c5 and c6/c7 of the page 0 (LE) permit
 * to adjust the red and blue colors.
 * They may vary from 0x101 to 0x1aa.
 */
static void setwhitebalance(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	u16 red, blue;

	red = 0x0158 + sd->ctrls[WHITE_BALANCE].val;
	blue = 0x0158 - sd->ctrls[WHITE_BALANCE].val;

	reg_w(gspca_dev, 0xff, 0x00);		/* page 0 */
	reg_w(gspca_dev, 0xc4, red);
	reg_w(gspca_dev, 0xc5, red >> 8);
	reg_w(gspca_dev, 0xc6, blue);
	reg_w(gspca_dev, 0xc7, blue >> 8);

	reg_w(gspca_dev, 0xdc, 0x01);
}

#ifndef NEWAGC
static void setgain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	reg_w(gspca_dev, 0xff, 0x03);			/* page 3 */
	reg_w(gspca_dev, 0x10, sd->ctrls[GAIN].val >> 3);

	/* load registers to sensor (Bit 0, auto clear) */
	reg_w(gspca_dev, 0x11, 0x01);
}

static void setexposure(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	u8 clockdiv;
	u16 exposure;

	/* register 2 of frame 3 contains the clock divider configuring the
	   no fps according to the formula: 90 / reg. sd->exposure is the
	   desired exposure time in 0.5 ms. */
	clockdiv = (90 * sd->ctrls[EXPOSURE].val + 1999) / 2000;

	/* Note clockdiv = 3 also works, but when running at 30 fps, depending
	   on the scene being recorded, the camera switches to another
	   quantization table for certain JPEG blocks, and we don't know how
	   to decompress these blocks. So we cap the framerate at 15 fps */
	if (clockdiv < 6)
		clockdiv = 6;
	else if (clockdiv > 63)
		clockdiv = 63;

	/* reg2 MUST be a multiple of 3, except when between 6 and 12?
	   Always round up, otherwise we cannot get the desired frametime
	   using the partial frame time exposure control */
	if (clockdiv < 6 || clockdiv > 12)
		clockdiv = ((clockdiv + 2) / 3) * 3;

	/* frame exposure time in ms = 1000 * clockdiv / 90    ->
	exposure = (sd->exposure / 2) * 448 / (1000 * clockdiv / 90) */
	exposure = (sd->ctrls[EXPOSURE].val * 45 * 448) / (1000 * clockdiv);
	/* 0 = use full frametime, 448 = no exposure, reverse it */
	exposure = 448 - exposure;

	reg_w(gspca_dev, 0xff, 0x03);			/* page 3 */
	reg_w(gspca_dev, 0x02, clockdiv);
	reg_w(gspca_dev, 0x0e, exposure & 0xff);
	reg_w(gspca_dev, 0x0f, exposure >> 8);

	/* load registers to sensor (Bit 0, auto clear) */
	reg_w(gspca_dev, 0x11, 0x01);
}

#else /* NEWAGC */
static void setexposure(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int expo, old_expo;
	u8 r12;
	static const struct {
		u16 r8003, r0504;
		u8 r1_80, r0_b6;
	} regs[(EXPOSURE_MAX + 1) >> 2] = {
		{0x00f4, 0x01e7, 0x0b, 0x08},
		{0x00f4, 0x01e7, 0x0b, 0x08},
		{0x00f4, 0x01e7, 0x0b, 0x08},
		{0x00f4, 0x01e7, 0x0b, 0x08},
		{0x00f5, 0x022d, 0x0d, 0x07},
		{0x0150, 0x0259, 0x0f, 0x07},
		{0x017c, 0x02a5, 0x11, 0x07},
		{0x017c, 0x0315, 0x11, 0x07},
		{0x017c, 0x0386, 0x11, 0x07},
		{0x017c, 0x03f6, 0x11, 0x07},
		{0x017c, 0x0467, 0x11, 0x07},
		{0x017c, 0x04d7, 0x11, 0x07},
		{0x017c, 0x0548, 0x11, 0x06},
		{0x017c, 0x05b8, 0x11, 0x06},
		{0x017c, 0x0629, 0x11, 0x06},
		{0x017c, 0x0699, 0x11, 0x06}
	};

	expo = sd->ctrls[EXPOSURE].val;
	old_expo = sd->old_expo;

	if (expo == old_expo)
		return;

	sd->old_expo = expo;

	reg_w(gspca_dev, 0xff, 0x03);			/* page 3 */

	if (expo < 16)
		r12 = expo;				/* 00 .. 0f */
	else
		r12 = (expo & 0x03) + 0x0c;		/* 0c .. 0f */

	expo >>= 2;					/* regs index */
	old_expo >>= 2;

	if (regs[expo].r8003 != regs[old_expo].r8003) {
		reg_w(gspca_dev, 0x03, regs[expo].r8003);
		reg_w(gspca_dev, 0x80, regs[expo].r8003 >> 8);
	}
	if (regs[expo].r0504 != regs[old_expo].r0504) {
		reg_w(gspca_dev, 0x04, regs[expo].r0504);
		reg_w(gspca_dev, 0x05, regs[expo].r0504 >> 8);
	}

	reg_w(gspca_dev, 0x12, r12);

	/* load registers to sensor (Bit 0, auto clear) */
	reg_w(gspca_dev, 0x11, 0x01);

	if (regs[expo].r1_80 != regs[old_expo].r1_80) {
		reg_w(gspca_dev, 0xff, 0x01);		/* page 1 */
		reg_w(gspca_dev, 0x80, regs[expo].r1_80);
	}
	if (regs[expo].r0_b6 != regs[old_expo].r0_b6) {
		reg_w(gspca_dev, 0xff, 0x00);		/* page 0 */
		reg_w(gspca_dev, 0xb6, regs[expo].r0_b6);
	}
}
#endif

static void setautogain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
#ifndef NEWAGC
	/* when switching to autogain set defaults to make sure
	   we are on a valid point of the autogain gain /
	   exposure knee graph, and give this change time to
	   take effect before doing autogain. */
	if (sd->ctrls[AUTOGAIN].val) {
		sd->ctrls[EXPOSURE].val = EXPOSURE_DEF;
		sd->ctrls[GAIN].val = GAIN_DEF;
		sd->autogain_ignore_frames =
				PAC_AUTOGAIN_IGNORE_FRAMES;
	} else {
		sd->autogain_ignore_frames = -1;
	}
	setexposure(gspca_dev);
	setgain(gspca_dev);
#else /* NEWAGC */
	if (sd->ctrls[AUTOGAIN].val) {
		sd->autogain_ignore_frames = AGC_CNT;
		gspca_dev->ctrl_inac |= (1 << EXPOSURE);
	} else {
		sd->autogain_ignore_frames = -1;
		gspca_dev->ctrl_inac &= ~(1 << EXPOSURE);
	}
#endif
}

static void sethvflip(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	u8 data, hflip, vflip;

	hflip = sd->ctrls[HFLIP].val;
	if (sd->flags & FL_HFLIP)
		hflip = !hflip;
	vflip = sd->ctrls[VFLIP].val;
	if (sd->flags & FL_VFLIP)
		vflip = !vflip;

	reg_w(gspca_dev, 0xff, 0x03);			/* page 3 */
	data = (hflip ? 0x08 : 0x00) | (vflip ? 0x04 : 0x00);
	reg_w(gspca_dev, 0x21, data);

	/* load registers to sensor (Bit 0, auto clear) */
	reg_w(gspca_dev, 0x11, 0x01);
}

/* this function is called at probe and resume time for pac7302 */
static int sd_init(struct gspca_dev *gspca_dev)
{
	reg_w_seq(gspca_dev, init_7302, sizeof(init_7302)/2);
	return gspca_dev->usb_err;
}

static int sd_start(struct gspca_dev *gspca_dev)
{
#ifndef NEWPKT
#ifndef NEWAGC
	struct sd *sd = (struct sd *) gspca_dev;
#endif
#endif

	reg_w_var(gspca_dev, start_7302,
		page3_7302, sizeof(page3_7302));
	setbrightcont(gspca_dev);
	setcolors(gspca_dev);
	setwhitebalance(gspca_dev);
	setautogain(gspca_dev);
	sethvflip(gspca_dev);

	/* only resolution 640x480 is supported for pac7302 */
#ifndef NEWPKT
	sd->sof_read = 0;
#endif
#ifndef NEWAGC
	atomic_set(&sd->avg_lum, 270 + sd->ctrls[BRIGHTNESS].val);
#endif

	/* start stream */
	reg_w(gspca_dev, 0xff, 0x01);
	reg_w(gspca_dev, 0x78, 0x01);

	return gspca_dev->usb_err;
}

static void sd_stopN(struct gspca_dev *gspca_dev)
{

	/* stop stream */
	reg_w(gspca_dev, 0xff, 0x01);
	reg_w(gspca_dev, 0x78, 0x00);
}

/* called on streamoff with alt 0 and on disconnect for pac7302 */
static void sd_stop0(struct gspca_dev *gspca_dev)
{
	if (!gspca_dev->present)
		return;
	reg_w(gspca_dev, 0xff, 0x01);
	reg_w(gspca_dev, 0x78, 0x40);
}

#ifndef NEWAGC
/* !! coarse_grained_expo_autogain is not used !! */
#define exp_too_low_cnt flags
#define exp_too_high_cnt sof_read
#include "autogain_functions.h"

static void do_autogain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int avg_lum = atomic_read(&sd->avg_lum);
	int desired_lum;
	const int deadzone = 30;

	if (sd->autogain_ignore_frames < 0)
		return;

	if (sd->autogain_ignore_frames > 0) {
		sd->autogain_ignore_frames--;
	} else {
		desired_lum = 270 + sd->ctrls[BRIGHTNESS].val;
		auto_gain_n_exposure(gspca_dev, avg_lum, desired_lum,
				deadzone, GAIN_KNEE, EXPOSURE_KNEE);
		sd->autogain_ignore_frames = PAC_AUTOGAIN_IGNORE_FRAMES;
	}
}
#else /* NEWAGC */
/* do the autogain stuff */
/* This function is called by the application when it gets an image */
static void do_autogain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int avg_lum, desired_lum, steps, expo;
#ifndef NEWAGC2
#define DEAD_ZONE 20
#else
#define DEAD_ZONE 200
#endif

	if (sd->autogain_ignore_frames < 0)
		return;
	if (sd->autogain_ignore_frames > 0) {
		sd->autogain_ignore_frames--;
		return;
	}
	sd->autogain_ignore_frames = AGC_CNT;

#ifndef NEWAGC2
	/* The avg_lum is the sum of 2 luminosity bytes.
	 * Good luminosity values seem to be around 190 (~ 0x60 * 2) */
	avg_lum = atomic_read(&sd->avg_lum);
	desired_lum = 190 + sd->ctrls[BRIGHTNESS].val;
#else
	/* The avg_lum is the sum of 25 luminosity bytes.
	 * Good luminosity values seem to be around 3200 (~ 0x80 * 25) */
	avg_lum = atomic_read(&sd->avg_lum);
	desired_lum = 3000 + sd->ctrls[BRIGHTNESS].val * 10;
#endif
	expo = sd->ctrls[EXPOSURE].val;
	if (desired_lum > avg_lum + DEAD_ZONE) {
		steps = (desired_lum - avg_lum) / DEAD_ZONE;
		expo += steps;
		if (expo > EXPOSURE_MAX)
			expo = EXPOSURE_MAX;
	} else if (desired_lum < avg_lum - DEAD_ZONE) {
		steps = (avg_lum - desired_lum) / DEAD_ZONE;
		expo -= steps;
		if (expo < 0)
			expo = 0;
	}
	if (expo != sd->ctrls[EXPOSURE].val) {
		sd->ctrls[EXPOSURE].val = expo;
		setexposure(gspca_dev);
	}
}
#endif

/* JPEG header */
static const u8 jpeg_header[] = {
	0xff, 0xd8,	/* SOI: Start of Image */

	0xff, 0xc0,	/* SOF0: Start of Frame (Baseline DCT) */
	0x00, 0x11,	/* length = 17 bytes (including this length field) */
	0x08,		/* Precision: 8 */
	0x02, 0x80,	/* height = 640 (image rotated) */
	0x01, 0xe0,	/* width = 480 */
	0x03,		/* Number of image components: 3 */
	0x01, 0x21, 0x00, /* ID=1, Subsampling 1x1, Quantization table: 0 */
	0x02, 0x11, 0x01, /* ID=2, Subsampling 2x1, Quantization table: 1 */
	0x03, 0x11, 0x01, /* ID=3, Subsampling 2x1, Quantization table: 1 */

	0xff, 0xda,	/* SOS: Start Of Scan */
	0x00, 0x0c,	/* length = 12 bytes (including this length field) */
	0x03,		/* number of components: 3 */
	0x01, 0x00,	/* selector 1, table 0x00 */
	0x02, 0x11,	/* selector 2, table 0x11 */
	0x03, 0x11,	/* selector 3, table 0x11 */
	0x00, 0x3f,	/* Spectral selection: 0 .. 63 */
	0x00		/* Successive approximation: 0 */
};

#ifdef NEWPKT
/* search the start of frame */
/*
 * A frame starts with 'ff ff 00 ff 96' (sof).
 * The sof is always in the first 0x100 bytes of a packet,
 * and it is aligned on a 2 bytes boundary.
 */
static unsigned char *find_sof(u8 *data, int len)
{
	static const unsigned char sof[5] =
		{ 0xff, 0xff, 0x00, 0xff, 0x96 };

	if (len > 0x100)
		len = 0x100;
	len -= sizeof sof;
	while (len >= 0) {
		if (memcmp(data, sof, sizeof sof) == 0)
			return data + sizeof sof;
		len -= 2;
		data += 2;
	}
	return NULL;
}
#endif

/* this function is run at interrupt level */
static void sd_pkt_scan(struct gspca_dev *gspca_dev,
			u8 *data,			/* isoc packet */
			int len)			/* iso packet length */
{
	struct sd *sd = (struct sd *) gspca_dev;
	u8 *image;
	u8 *sof;

#ifndef NEWPKT
	sof = pac_find_sof(&sd->sof_read, data, len);
#else
	sof = find_sof(data, len);
#endif
	if (sof) {
#ifndef NEWAGC2
		int n, lum_offset, footer_length;

		/* 6 bytes after the FF D9 EOF marker a number of lumination
		   bytes are send corresponding to different parts of the
		   image, the 14th and 15th byte after the EOF seem to
		   correspond to the center of the image */
		lum_offset = 61 + sizeof pac_sof_marker;
		footer_length = 74;

		/* Finish decoding current frame */
		n = (sof - data) - (footer_length + sizeof pac_sof_marker);

		if (n < 0)
			gspca_dev->image_len += n;
		else
			gspca_frame_add(gspca_dev, INTER_PACKET, data, n);

		image = gspca_dev->image;
		if (image != NULL
		 && image[gspca_dev->image_len - 2] == 0xff
		 && image[gspca_dev->image_len - 1] == 0xd9)
			gspca_frame_add(gspca_dev, LAST_PACKET, NULL, 0);
#else
		int n;

#define FOOTER_SZ (74 + 5)	/* footer + sof marker */

		/* update the end of the current frame */
		n = sof - data - FOOTER_SZ;
		if (n < 0) {
			gspca_dev->image_len += n;
			image = gspca_dev->image;
			if (image != NULL
			 && image[gspca_dev->image_len - 2] == 0xff
			 && image[gspca_dev->image_len - 1] == 0xd9)
				gspca_frame_add(gspca_dev, LAST_PACKET,
						NULL, 0);
		} else if (n > 0
			&& data[n - 2] == 0xff
			&& data[n - 1] == 0xd9) {
				gspca_frame_add(gspca_dev, LAST_PACKET,
						data, n);
		}
#endif

		n = sof - data;
		len -= n;
		data = sof;

#ifndef NEWAGC2
		/* Get average lumination */
		if (gspca_dev->last_packet_type == LAST_PACKET &&
				n >= lum_offset)
			atomic_set(&sd->avg_lum, data[-lum_offset] +
						data[-lum_offset + 1]);
#else
		/* get the whole lumination of the 25 areas */
		if (gspca_dev->last_packet_type == LAST_PACKET
		 && n >= FOOTER_SZ - 6) {
			u8 *p;
			int lum;

			p = data + 6 - FOOTER_SZ;
			n = 25;
			lum = 0;
			while (--n >= 0)
				lum += *p++;
			atomic_set(&sd->avg_lum, lum);
		}
#endif

		/* Start the new frame with the jpeg header */
		/* The PAC7302 has the image rotated 90 degrees */
		gspca_frame_add(gspca_dev, FIRST_PACKET,
				jpeg_header, sizeof jpeg_header);
	}
	gspca_frame_add(gspca_dev, INTER_PACKET, data, len);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int sd_dbg_s_register(struct gspca_dev *gspca_dev,
			struct v4l2_dbg_register *reg)
{
	u8 index;
	u8 value;

	/* reg->reg: bit0..15: reserved for register index (wIndex is 16bit
			       long on the USB bus)
	*/
	if (reg->match.type == V4L2_CHIP_MATCH_HOST &&
	    reg->match.addr == 0 &&
	    (reg->reg < 0x000000ff) &&
	    (reg->val <= 0x000000ff)
	) {
		/* Currently writing to page 0 is only supported. */
		/* reg_w() only supports 8bit index */
		index = reg->reg;
		value = reg->val;

		/* Note that there shall be no access to other page
		   by any other function between the page swith and
		   the actual register write */
		reg_w(gspca_dev, 0xff, 0x00);		/* page 0 */
		reg_w(gspca_dev, index, value);

		reg_w(gspca_dev, 0xdc, 0x01);
	}
	return gspca_dev->usb_err;
}

static int sd_chip_ident(struct gspca_dev *gspca_dev,
			struct v4l2_dbg_chip_ident *chip)
{
	int ret = -EINVAL;

	if (chip->match.type == V4L2_CHIP_MATCH_HOST &&
	    chip->match.addr == 0) {
		chip->revision = 0;
		chip->ident = V4L2_IDENT_UNKNOWN;
		ret = 0;
	}
	return ret;
}
#endif

#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
static int sd_int_pkt_scan(struct gspca_dev *gspca_dev,
			u8 *data,		/* interrupt packet data */
			int len)		/* interrput packet length */
{
	int ret = -EINVAL;
	u8 data0, data1;

	if (len == 2) {
		data0 = data[0];
		data1 = data[1];
		if ((data0 == 0x00 && data1 == 0x11) ||
		    (data0 == 0x22 && data1 == 0x33) ||
		    (data0 == 0x44 && data1 == 0x55) ||
		    (data0 == 0x66 && data1 == 0x77) ||
		    (data0 == 0x88 && data1 == 0x99) ||
		    (data0 == 0xaa && data1 == 0xbb) ||
		    (data0 == 0xcc && data1 == 0xdd) ||
		    (data0 == 0xee && data1 == 0xff)) {
			input_report_key(gspca_dev->input_dev, KEY_CAMERA, 1);
			input_sync(gspca_dev->input_dev);
			input_report_key(gspca_dev->input_dev, KEY_CAMERA, 0);
			input_sync(gspca_dev->input_dev);
			ret = 0;
		}
	}

	return ret;
}
#endif

/* sub-driver description for pac7302 */
static const struct sd_desc sd_desc = {
	.name = KBUILD_MODNAME,
	.ctrls = sd_ctrls,
	.nctrls = ARRAY_SIZE(sd_ctrls),
	.config = sd_config,
	.init = sd_init,
	.start = sd_start,
	.stopN = sd_stopN,
	.stop0 = sd_stop0,
	.pkt_scan = sd_pkt_scan,
	.dq_callback = do_autogain,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.set_register = sd_dbg_s_register,
	.get_chip_ident = sd_chip_ident,
#endif
#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
	.int_pkt_scan = sd_int_pkt_scan,
#endif
};

/* -- module initialisation -- */
static const struct usb_device_id device_table[] = {
	{USB_DEVICE(0x06f8, 0x3009)},
	{USB_DEVICE(0x06f8, 0x301b)},
	{USB_DEVICE(0x093a, 0x2620)},
	{USB_DEVICE(0x093a, 0x2621)},
	{USB_DEVICE(0x093a, 0x2622), .driver_info = FL_VFLIP},
	{USB_DEVICE(0x093a, 0x2624), .driver_info = FL_VFLIP},
	{USB_DEVICE(0x093a, 0x2625)},
	{USB_DEVICE(0x093a, 0x2626)},
	{USB_DEVICE(0x093a, 0x2628)},
	{USB_DEVICE(0x093a, 0x2629), .driver_info = FL_VFLIP},
	{USB_DEVICE(0x093a, 0x262a)},
	{USB_DEVICE(0x093a, 0x262c)},
	{USB_DEVICE(0x145f, 0x013c)},
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
	.name = KBUILD_MODNAME,
	.id_table = device_table,
	.probe = sd_probe,
	.disconnect = gspca_disconnect,
#ifdef CONFIG_PM
	.suspend = gspca_suspend,
	.resume = gspca_resume,
#endif
};

module_usb_driver(sd_driver);
