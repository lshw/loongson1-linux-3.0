/*
 * Driver for OV7675  from OV
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/soc_camera.h>
#include <media/soc_mediabus.h>
//#include <media/v4l2-i2c-drv.h>

#define CONFIG_CAMERA_CONTROLLED_BY_ARM


#include <media/gc0308_platform.h>

#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif

#include "gc0308.h"
#include "gc0307.h"



#define GC0308_DRIVER_NAME	"GC0308"

//*********** proc *********************
#include <linux/proc_fs.h>
#include "../../../fs/proc/internal.h"
#define GC0308_PROC_NAME	"gc0308"
static struct proc_dir_entry * s_proc = NULL;
static struct i2c_client * s_i2c_client = NULL;
//**************************************

void GC0308_night_mode(struct v4l2_subdev *sd, bool enable);
static int gc0308_init(struct v4l2_subdev *sd, u32 val);


/* Default resolution & pixelformat. plz ref gc0308_platform.h */
#define DEFAULT_RES		VGA	/* Index of resoultion */
#define DEFAULT_FMT		V4L2_PIX_FMT_UYVY	/* YUV422 */

static unsigned char regABVal = 0x06;
static unsigned char hueTable[21][3] = {{0x0, 0x80, 0},
				{0x14, 0x7E, 0},
				{0x27, 0x79, 0},
				{0x3A, 0x72, 0},
				{0x4B, 0x67, 0},
				{0x5A, 0x5A, 0},
				{0x67, 0x4B, 0},
				{0x72, 0x3A, 0},
				{0x79, 0x27, 0},
				{0x7E, 0x14, 0},
				{0x80, 0x00, 1},
				{0x7E, 0x14, 1},
				{0x79, 0x27, 1},
				{0x72, 0x3A, 1},
				{0x67, 0x4B, 1},
				{0x5A, 0x5A, 1},
				{0x4B, 0x67, 1},
				{0x3A, 0x72, 1},
				{0x27, 0x79, 1},
				{0x14, 0x7E, 1},
				{0x00, 0x80, 1}};

/*
 * Specification
 * Parallel : ITU-R. 656/601 YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Serial : MIPI CSI2 (single lane) YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Resolution : 1280 (H) x 1024 (V)
 * Image control : Brightness, Contrast, Saturation, Sharpness, Glamour
 * Effect : Mono, Negative, Sepia, Aqua, Sketch
 * FPS : 15fps @full resolution, 30fps @VGA, 24fps @720p
 * Max. pixel clock frequency : 48MHz(upto)
 * Internal PLL (6MHz to 27MHz input frequency)
 */

/* Camera functional setting values configured by user concept */
struct gc0308_userset {
	signed int exposure_bias;	/* V4L2_CID_EXPOSURE */
	unsigned int ae_lock;
	unsigned int awb_lock;
	unsigned int auto_wb;	/* V4L2_CID_AUTO_WHITE_BALANCE */
	unsigned int manual_wb;	/* V4L2_CID_WHITE_BALANCE_PRESET */
	unsigned int wb_temp;	/* V4L2_CID_WHITE_BALANCE_TEMPERATURE */
	unsigned int effect;	/* Color FX (AKA Color tone) */
	unsigned int contrast;	/* V4L2_CID_CONTRAST */
	unsigned int saturation;	/* V4L2_CID_SATURATION */
	unsigned int sharpness;		/* V4L2_CID_SHARPNESS */
	unsigned int glamour;
};

struct gc0308_state {
	struct gc0308_platform_data *pdata;
	struct v4l2_subdev sd;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	struct gc0308_userset userset;
	int freq;	/* MCLK in KHz */
	int is_mipi;
	int isize;
	int ver;
	int fps;
};

static inline struct gc0308_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gc0308_state, sd);
}

static int gc0308_i2c_write(struct v4l2_subdev *sd, unsigned char i2c_data[],
				unsigned char length);

#if 1
/*
 * OV7675 register structure : 2bytes address, 2bytes value
 * retry on write failure up-to 5 times
 */
static inline int gc0308_write(struct v4l2_subdev *sd, u8 addr, u8 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg[1];
	unsigned char reg[2];
	int err = 0;
	int retry = 0;


	if (!client->adapter)
		return -ENODEV;

again:
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = reg;

	reg[0] = addr & 0xff;
	reg[1] = val & 0xff;

	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
		return err;	/* Returns here on success */

	/* abnormal case: retry 5 times */
	if (retry < 5) {
		dev_err(&client->dev, "%s: address: 0x%02x%02x, " \
			"value: 0x%02x%02x\n", __func__, \
			reg[0], reg[1], reg[2], reg[3]);
		retry++;
		goto again;
	}

	return err;
}

static int gc0308_i2c_read(struct v4l2_subdev *sd, unsigned char reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int rc;

	unsigned char data[1]={0};
	data[0] = reg;
	return i2c_smbus_read_byte_data(client, reg);
	//i2c_master_recv(client, data ,1);
	//return data[0];
}

static int gc0308_write_regs(struct v4l2_subdev *sd, unsigned char regs[],
				int size)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i, err;

	for (i = 0; i < size; i++) {
		err = gc0308_i2c_write(sd, &regs[i], sizeof(regs[i]));
		if (err < 0)
			v4l_info(client, "%s: register set failed\n", \
			__func__);
	}

	return 0;	/* FIXME */
}
#endif

extern s32 i2c_smbus_write_byte_data(const struct i2c_client *client, u8 command, u8 value);


static int gc0308_i2c_write(struct v4l2_subdev *sd, unsigned char i2c_data[],
				unsigned char length)
{
#if 0 //FIXME: urbetter skip I2C
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg = {client->addr, 0, length, i2c_data};
//	struct i2c_msg msg = {client->addr, I2C_M_IGNORE_NAK, length, buf};

	return i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;  
#else

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return i2c_smbus_write_byte_data(client, i2c_data[0], i2c_data[1]);
#endif
}

static enum v4l2_mbus_pixelcode gc0308_codes[] = {
	V4L2_MBUS_FMT_YUYV8_2X8,
	V4L2_MBUS_FMT_YVYU8_2X8,
	V4L2_MBUS_FMT_UYVY8_2X8,
	V4L2_MBUS_FMT_RGB565_2X8_LE,
};


static struct v4l2_queryctrl gc0308_controls[] = {
	{
		/*
		 * For now, we just support in preset type
		 * to be close to generic WB system,
		 * we define color temp range for each preset
		 */
		.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "White balance in kelvin",
		.minimum = 0,
		.maximum = 10000,
		.step = 1,
		.default_value = 0,	/* FIXME */
	},
	{
		.id = V4L2_CID_AUTO_WHITE_BALANCE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Auto white balance",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_BRIGHTNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Brightness",
		.minimum = -80,
		.maximum = 80,
		.step = 1,
		.default_value = 0x08,
	},
	{
		.id = V4L2_CID_CONTRAST,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Contrast",
		.minimum = 0x10,
		.maximum = 0x30,
		.step = 1,
		.default_value = 0x20,
	},
	{
		.id = V4L2_CID_SATURATION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Saturation",
		.minimum = 0x00,
		.maximum = 0x80,
		.step = 1,
		.default_value = 0x40,
	},
	{
		.id = V4L2_CID_SHARPNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Sharpness",
		.minimum = 0x00,
		.maximum = 0x09,
		.step = 1,
		.default_value = 0x04,
	},
	{
		.id = V4L2_CID_HUE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hue",
		.minimum = 0,
		.maximum = 21,
		.step = 1,
		.default_value = 10,
	},
	{
		.id = V4L2_CID_POWER_LINE_FREQUENCY,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Power line frequency",
		.minimum = 0,
		.maximum = 2,
		.step = 1,
		.default_value = 0,
	},
};

static inline struct v4l2_queryctrl const *gc0308_find_qctrl(int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gc0308_controls); i++)
		if (gc0308_controls[i].id == id)
			return &gc0308_controls[i];

	return NULL;
}

static int gc0308_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gc0308_controls); i++) {
		if (gc0308_controls[i].id == qc->id) {
			memcpy(qc, &gc0308_controls[i], \
				sizeof(struct v4l2_queryctrl));
			return 0;
		}
	}

	return -EINVAL;
}


/*
 * Clock configuration
 * Configure expected MCLK from host and return EINVAL if not supported clock
 * frequency is expected
 * 	freq : in Hz
 * 	flag : not supported for now
 */
static int gc0308_s_crystal_freq(struct v4l2_subdev *sd, u32 freq, u32 flags)
{
	int err = -EINVAL;

	return err;
}

static int gc0308_g_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	int err = 0;

	return err;
}

static int gc0308_s_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	int err = 0;

//	gc0308_init(sd, 0);
	return err;
}
static int gc0308_enum_framesizes(struct v4l2_subdev *sd, \
					struct v4l2_frmsizeenum *fsize)
{
	int err = 0;

	return err;
}

static int gc0308_enum_frameintervals(struct v4l2_subdev *sd,
					struct v4l2_frmivalenum *fival)
{
	int err = 0;

	return err;
}

static int gc0308_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(gc0308_codes))
		return -EINVAL;

	*code = gc0308_codes[index];
	return 0;
}

static int gc0308_try_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *mf)
{
	int err = 0;

	return err;
}

static int gc0308_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;

	dev_dbg(&client->dev, "%s\n", __func__);

	return err;
}

static int gc0308_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;

	dev_dbg(&client->dev, "%s: numerator %d, denominator: %d\n", \
		__func__, param->parm.capture.timeperframe.numerator, \
		param->parm.capture.timeperframe.denominator);

	return err;
}

static int gc0308_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = 0;
	return err;
}

void gc0308_color_effect(struct v4l2_subdev *sd, int which)
{
	int err, i;
	int num;
	
	printk("gc0308 color mode!!!\n");
	// normal,grayscale,sepia,sepia_green,sepia_blue,color_inv,gray_inv,embossment,sketch

	switch (which) {
	case 1:		
		num = ((sizeof(gc0308_effect_normal)/sizeof(gc0308_effect_normal[0])));			
		for (i = 0; i < num ; i++) {
			err =gc0308_i2c_write(sd, gc0308_effect_normal[i], 2);
		}
	break;
	case 2:		
		num = ((sizeof(gc0308_effect_grayscale)/sizeof(gc0308_effect_grayscale[0])));			
		for (i = 0; i < num ; i++) {
			err =gc0308_i2c_write(sd, gc0308_effect_grayscale[i], 2);
		}
	break;
	case 3:		
		num = ((sizeof(gc0308_effect_sepia)/sizeof(gc0308_effect_sepia[0])));			
		for (i = 0; i < num ; i++) {
			err =gc0308_i2c_write(sd, gc0308_effect_sepia[i], 2);
		}
	break;
	case 4:		
		num = ((sizeof(gc0308_effect_sepia_green)/sizeof(gc0308_effect_sepia_green[0])));			
		for (i = 0; i < num ; i++) {
			err =gc0308_i2c_write(sd, gc0308_effect_sepia_green[i], 2);
		}
	break;
	case 5:	
		num = ((sizeof(gc0308_effect_sepia_blue)/sizeof(gc0308_effect_sepia_blue[0])));			
		for (i = 0; i < num ; i++) {
			err =gc0308_i2c_write(sd, gc0308_effect_sepia_blue[i], 2);
		}
	break;
	case 6:	
		num = ((sizeof(gc0308_effect_color_inv)/sizeof(gc0308_effect_color_inv[0])));			
		for (i = 0; i < num ; i++) {
			err =gc0308_i2c_write(sd, gc0308_effect_color_inv[i], 2);
		}
	break;
	case 7:		
		num = ((sizeof(gc0308_effect_gray_inv)/sizeof(gc0308_effect_gray_inv[0])));			
		for (i = 0; i < num ; i++) {
			err =gc0308_i2c_write(sd, gc0308_effect_gray_inv[i], 2);
		}
	break;
	case 8:		
		num = ((sizeof(gc0308_effect_embossment)/sizeof(gc0308_effect_embossment[0])));			
		for (i = 0; i < num ; i++) {
			err =gc0308_i2c_write(sd, gc0308_effect_embossment[i], 2);
		}
	break;
	case 9:		   
		num = ((sizeof(gc0308_effect_sketch)/sizeof(gc0308_effect_sketch[0])));			
		for (i = 0; i < num ; i++) {
			err =gc0308_i2c_write(sd, gc0308_effect_sketch[i], 2);
		}
	break;
	}
	
	mdelay(200);
}

void gc0308_white_balance_preset(struct v4l2_subdev *sd, int which)
{
	int err, i;
	int num;
	unsigned char ctrlreg[1][2];
	
	printk("  gc0308 color mode!!!\n");
	// normal,grayscale,sepia,sepia_green,sepia_blue,color_inv,gray_inv,embossment,sketch

	switch (which) {
	case 1:		
		num = ((sizeof(gc0308_white_balance_auto)/sizeof(gc0308_white_balance_auto[0])));			
		for (i = 0; i < num ; i++) {
			err=gc0308_i2c_write(sd, gc0308_white_balance_auto[i], 2);
		}
		// enable AWB
		ctrlreg[0][0]=0x22; 
		ctrlreg[0][1]=gc0308_i2c_read(sd, 0x22)|0x02;
		err=gc0308_i2c_write(sd, ctrlreg[0], 2);
		
	break;
	case 2:		
		// disable AWB
		ctrlreg[0][0]=0x22; 
		ctrlreg[0][1]=gc0308_i2c_read(sd, 0x22) & (~0x02);
		err=gc0308_i2c_write(sd, ctrlreg[0], 2);
		
		num = ((sizeof(gc0308_white_balance_incandescence)/
						sizeof(gc0308_white_balance_incandescence[0])));			
		for (i = 0; i < num ; i++) {
			err =gc0308_i2c_write(sd, gc0308_white_balance_incandescence[i], 2);
		}
	break;
	case 3:		
		// disable AWB
		ctrlreg[0][0]=0x22; 
		ctrlreg[0][1]=gc0308_i2c_read(sd, 0x22) & (~0x02);
		err=gc0308_i2c_write(sd, ctrlreg[0], 2);
		
		num = ((sizeof(gc0308_white_balance_daylight)/sizeof(gc0308_white_balance_daylight[0])));			
		for (i = 0; i < num ; i++) {
			err =gc0308_i2c_write(sd, gc0308_white_balance_daylight[i], 2);
		}
	break;
	case 4:		
		// disable AWB
		ctrlreg[0][0]=0x22; 
		ctrlreg[0][1]=gc0308_i2c_read(sd, 0x22) & (~0x02);
		err=gc0308_i2c_write(sd, ctrlreg[0], 2);
		
		num = ((sizeof(gc0308_white_balance_fluorescent)/sizeof(gc0308_white_balance_fluorescent[0])));			
		for (i = 0; i < num ; i++) {
			err =gc0308_i2c_write(sd, gc0308_white_balance_fluorescent[i], 2);
		}
	break;
	case 5:	
		num = ((sizeof(gc0308_white_balance_cloud)/sizeof(gc0308_white_balance_cloud[0])));			
		for (i = 0; i < num ; i++) {
			err =gc0308_i2c_write(sd, gc0308_white_balance_cloud[i], 2);
		}
	break;
	}
	
	mdelay(200);
}

static int gc0308_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i, cnt = 0, err = -EINVAL;
	unsigned char ctrlreg[3][2];

	printk("In %s() ctrl id is 0x%x value 0xis %x\n", 
			__func__,
			ctrl->id,
			ctrl->value);

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		dev_dbg(&client->dev, "%s: V4L2_CID_BRIGHTNESS\n", __func__);
		ctrlreg[0][0] = 0xAB;
		ctrlreg[1][0] = 0x9B;
		if (ctrl->value >= 0) {
			regABVal &= ~BIT(3);
			ctrlreg[1][1] = ctrl->value;
		} else {
			regABVal |= BIT(3);
			ctrlreg[1][1] = -ctrl->value;
		}
		ctrlreg[0][1] = regABVal;
		cnt = 2;
		break;

	case V4L2_CID_CONTRAST:
		dev_dbg(&client->dev, "%s: V4L2_CID_CONTRAST\n", __func__);
		ctrlreg[0][0] = 0x9C;
		ctrlreg[0][1] = ctrl->value;
		cnt = 1;
		break;

	case V4L2_CID_SATURATION:
		dev_dbg(&client->dev, "%s: V4L2_CID_SATURATION\n", __func__);
		ctrlreg[0][0] = 0xA7;
		ctrlreg[1][0] = 0xA8;
		ctrlreg[0][1] = ctrlreg[1][1] = ctrl->value;
		cnt = 2;
		break;

	case V4L2_CID_SHARPNESS:
		dev_dbg(&client->dev, "%s: V4L2_CID_SHARPNESS\n", __func__);
		ctrlreg[0][0] = 0x8F;
		ctrlreg[0][1] = ctrl->value;
		cnt = 1;
		break;

	case V4L2_CID_HUE:
		dev_dbg(&client->dev, "%s: V4L2_CID_HUE\n", __func__);
		if (hueTable[ctrl->value][2] == 0) {
			regABVal &= ~BIT(1);
			regABVal |= BIT(0);
		} else {
			regABVal |= BIT(1);
			regABVal &= ~BIT(0);
		}
		ctrlreg[0][0] = 0xA9;
		ctrlreg[0][1] = hueTable[ctrl->value][0];
		ctrlreg[1][0] = 0xAA;
		ctrlreg[1][1] = hueTable[ctrl->value][1];
		ctrlreg[2][0] = 0xAB;
		ctrlreg[2][1] = regABVal;
		cnt = 3;
		break;

	case V4L2_CID_POWER_LINE_FREQUENCY:
		dev_dbg(&client->dev, "%s: V4L2_CID_POWER_LINE_FREQUENCY\n", __func__);
		ctrlreg[0][0] = 0x22;
		ctrlreg[1][0] = 0x23;
		if ((ctrl->value == 0) || (ctrl->value == 2)) { // off or 60Hz
			ctrlreg[0][1] = 0x7F;
			ctrlreg[1][1] = 0x03;
			cnt = 2;
		} else if (ctrl->value == 1){ // 50Hz
			ctrlreg[0][1] = 0x99;
			ctrlreg[1][1] = 0x02;
			cnt = 2;
		}
		break;
#if 0	//lxy
	case V4L2_CID_SCENE_MODE:
		GC0308_night_mode(sd, ctrl->value);
		return;
		break;
#endif

	case V4L2_CID_COLORFX:
   		dev_err(&client->dev, "%s: in color effect %d\n", 
					__func__,   
					ctrl->value);
		gc0308_color_effect(sd, ctrl->value);
		return 0;
		break;

#if 0	//lxy
	case V4L2_CID_WHITE_BALANCE_PRESET:
		dev_err(&client->dev, "%s: in white balance preset %d\n", 
					__func__,   
					ctrl->value);
		gc0308_white_balance_preset(sd, ctrl->value);
		return 0;
		break;
#endif

	default:
		dev_err(&client->dev, "%s: no such control id 0x%x\n", __func__, ctrl->id);
		break;  
	}

	for (i = 0; i < cnt; ++i)
		err = gc0308_i2c_write(sd, ctrlreg[i], 2);

	if (err < 0) {
		dev_dbg(&client->dev, "%s: vidioc_s_ctrl failed: %d\n", __func__, ctrl->id);
	}

	return err;
}

static int gc0308_init(struct v4l2_subdev *sd, u32 val)
{
//printk("-----------gc0308_init------------\n");
#ifdef CONFIG_CAMERA_CONTROLLED_BY_ARM
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i, err = -EINVAL;

	/*
	unsigned char initreg[2] = {0x12, 0x80};

	v4l_info(client, "%s: camera initialization start\n", __func__);
	err = gc0308_i2c_write(sd, initreg, 2);
	if (err < 0)
		v4l_info(client, "%s: init register set failed\n", __func__);
	mdelay(150);
*/
	printk ("lxy: begin to init gc0308's reg.\n");

    printk ("GC0307 ID:0x%x\n", gc0308_i2c_read(sd, 0));
#if 1   

for (i = 0; i < GC0307_INIT_REGS; i++) {
	err = gc0308_i2c_write(sd, gc0307_YCbCr8bit[i],
			sizeof(gc0307_YCbCr8bit[i]));
	//mdelay(10);
	if (err < 0)
		v4l_info(client, "%s: %d register set failed\n",
				__func__, i);
}

gc0308_i2c_write(sd, "\x4d\x18", 2);

/*
for (i = 0; i < GC0307_INIT_REGS; i++) 
{
    unsigned int tmp;
	tmp = gc0308_i2c_read(sd, gc0307_YCbCr8bit[i][0]);
	printk("gc0307_YCbCr8bit{0x%x, 0x%x}\n", gc0307_YCbCr8bit[i][0], tmp);
    mdelay(10);
}*/


#else
	for (i = 0; i < GC0308_INIT_REGS; i++) {
		err = gc0308_i2c_write(sd, gc0308_init_reg[i],
				sizeof(gc0308_init_reg[i]));
		if (err < 0)
			v4l_info(client, "%s: %d register set failed\n",
					__func__, i);
	}

for (i = 0; i < GC0308_INIT_REGS; i++) 
{
    unsigned int tmp;
	tmp = gc0308_i2c_read(sd, gc0308_init_reg[i][0]);
	printk("gc0308_init_reg{0x%x, 0x%x}\n", gc0308_init_reg[i][0], tmp);
    mdelay(100);
}

#endif

#if 0
		gc0308_i2c_write(sd, gc0308_windows_2_page1[0],
				sizeof(gc0308_windows_2_page1[0]));
		
		gc0308_i2c_write(sd, gc0308_windows_2_vga[0],
				sizeof(gc0308_windows_2_vga[0]));

		gc0308_i2c_write(sd, gc0308_windows_2_page0[0],
				sizeof(gc0308_windows_2_page0[0]));

		for (i = 0; i < 8; i++) {
			gc0308_i2c_write(sd, gc0308_windows_640[i],
					sizeof(gc0308_windows_640[i]));
		}
	
		gc0308_i2c_write(sd, gc0308_output_RGB[0],
				sizeof(gc0308_output_RGB[0]));
		
		gc0308_i2c_write(sd, gc0308_paddrv[0],
				sizeof(gc0308_paddrv[0]));

		gc0308_i2c_write(sd, gc0308_syncmode1[0],
				sizeof(gc0308_syncmode1[0]));

		gc0308_i2c_write(sd, gc0308_output_en[0],
				sizeof(gc0308_output_en[0]));
#endif	

	if (err < 0) {
		v4l_err(client, "%s: camera initialization failed\n",
				__func__);
		return -EIO;	/* FIXME */
	}
#endif
	printk ("lxy: init gc0308's reg success.\n");
	return 0;
}

/*
 * s_config subdev ops
 * With camera device, we need to re-initialize every single opening time therefor,
 * it is not necessary to be initialized on probe time. except for version checking
 * NOTE: version checking is optional
 */
static int gc0308_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	printk("gc0308_s_cfg---------\n");
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct gc0308_state *state = to_state(sd);
	struct gc0308_platform_data *pdata;

	dev_info(&client->dev, "fetching platform data\n");

	pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "%s: no platform data\n", __func__);
		return -ENODEV;
	}

	/*
	 * Assign default format and resolution
	 * Use configured default information in platform data
	 * or without them, use default information in driver
	 */
	if (!(pdata->default_width && pdata->default_height)) {
		/* TODO: assign driver default resolution */
	} else {
		state->pix.width = pdata->default_width;
		state->pix.height = pdata->default_height;
	}

	if (!pdata->pixelformat)
		state->pix.pixelformat = DEFAULT_FMT;
	else
		state->pix.pixelformat = pdata->pixelformat;

	if (!pdata->freq)
		state->freq = 12000000;	/* 12MHz default */
	else
		state->freq = pdata->freq;

	if (!pdata->is_mipi) {
		state->is_mipi = 0;
		dev_info(&client->dev, "parallel mode\n");
	} else
		state->is_mipi = pdata->is_mipi;

	return 0;
}



/*************************************************************************
* FUNCTION
*	GC0308_night_mode
*
* DESCRIPTION
*	This function night mode of GC0308.
*
* PARAMETERS
*	none
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void GC0308_night_mode(struct v4l2_subdev *sd, bool enable)
{
	//kal_int32 temp_reg =0;
	int temp_reg =0;
	unsigned char ctrlreg[1][2];
	
#ifdef GC_DEBUG	
	_dbg_print("  GC0308_night_mode!!!\n");
#endif	

    //temp_reg = read_cmos_sensor(0x20);
    temp_reg = gc0308_i2c_read(sd, 0x20);
	
    if(enable){
		//MAX_EXPOSURE_LINES=2*(PIXEL_CLK/MIN_FRAME_RATE)/VGA_PERIOD_PIXEL_NUMS;

		//sensor_night_mode=KAL_TRUE;

		printk("%s(): line: %d: enable GC0308_night_mode !\n",__func__, __LINE__);

		ctrlreg[0][0] = 0xec;
		ctrlreg[0][1] = 0x30;		
		gc0308_i2c_write(sd, ctrlreg[0], 2);

		ctrlreg[0][0] = 0x20;
		ctrlreg[0][1] = temp_reg&0x5f;
		gc0308_i2c_write(sd, ctrlreg[0], 2);

		ctrlreg[0][0] = 0x3c;
		ctrlreg[0][1] = 0x08;
		gc0308_i2c_write(sd, ctrlreg[0], 2);
		
		ctrlreg[0][0] = 0x3d;
		ctrlreg[0][1] = 0x08;
		gc0308_i2c_write(sd, ctrlreg[0], 2);

		ctrlreg[0][0] = 0x3e;
		ctrlreg[0][1] = 0x08;
		gc0308_i2c_write(sd, ctrlreg[0], 2);

		
		ctrlreg[0][0] = 0x3f;
		ctrlreg[0][1] = 0x08;
		gc0308_i2c_write(sd, ctrlreg[0], 2);

     } else {
		//MAX_EXPOSURE_LINES=(PIXEL_CLK/MIN_FRAME_RATE)/VGA_PERIOD_PIXEL_NUMS;

		//sensor_night_mode=KAL_FALSE;

		printk("%s(): line: %d: disable GC0308_night_mode !\n", __func__, __LINE__);

		ctrlreg[0][0] = 0xec;
		ctrlreg[0][1] = 0x20;		
		gc0308_i2c_write(sd, ctrlreg[0], 2);

		ctrlreg[0][0] = 0x20;
		ctrlreg[0][1] = temp_reg|0x20;		
		gc0308_i2c_write(sd, ctrlreg[0], 2);

		ctrlreg[0][0] = 0x3c;
		ctrlreg[0][1] = 0x02;		
		gc0308_i2c_write(sd, ctrlreg[0], 2);
		
		ctrlreg[0][0] = 0x3d;
		ctrlreg[0][1] = 0x02;		
		gc0308_i2c_write(sd, ctrlreg[0], 2);
	
		ctrlreg[0][0] = 0x3e;
		ctrlreg[0][1] = 0x02;		
		gc0308_i2c_write(sd, ctrlreg[0], 2);
		
		ctrlreg[0][0] = 0x3f;
		ctrlreg[0][1] = 0x02;		
		gc0308_i2c_write(sd, ctrlreg[0], 2);
	}

	 //Delayms_GC(200);
	 mdelay(200);
}

static int gc0308_set_bus_param(struct soc_camera_device *icd,
				unsigned long flags)
{
	struct soc_camera_link *icl = to_soc_camera_link(icd);
	unsigned long width_flag = flags & SOCAM_DATAWIDTH_MASK;

	/* Only one width bit may be set */
	if (icl->set_bus_param)
		return icl->set_bus_param(icl, width_flag);

	/*
	 * Without board specific bus width settings we support only the
	 * sensors native bus width witch are tested working
	 */
	if (width_flag & (SOCAM_DATAWIDTH_10 | SOCAM_DATAWIDTH_8))
		return 0;

	return 0;
}

static unsigned long gc0308_query_bus_param(struct soc_camera_device *icd)
{
	struct soc_camera_link *icl = to_soc_camera_link(icd);
	unsigned long flags = SOCAM_PCLK_SAMPLE_RISING | SOCAM_MASTER |
		SOCAM_VSYNC_ACTIVE_HIGH | SOCAM_HSYNC_ACTIVE_HIGH |
		SOCAM_DATA_ACTIVE_HIGH;

	if (icl->query_bus_param)
		flags |= icl->query_bus_param(icl) & SOCAM_DATAWIDTH_MASK;
	else
		flags |= SOCAM_DATAWIDTH_8;

	return soc_camera_apply_sensor_flags(icl, flags);
}

static const struct v4l2_subdev_core_ops gc0308_core_ops = {
	.init = gc0308_init,	/* initializing API */
//	.s_config = gc0308_s_config,	/* Fetch platform data */
	.queryctrl = gc0308_queryctrl,
	.g_ctrl = gc0308_g_ctrl,
	.s_ctrl = gc0308_s_ctrl,
};

static const struct v4l2_subdev_video_ops gc0308_video_ops = {
	.s_crystal_freq = gc0308_s_crystal_freq,
	.g_mbus_fmt = gc0308_g_fmt,
	.s_mbus_fmt = gc0308_s_fmt,
	.enum_framesizes = gc0308_enum_framesizes,
	.enum_frameintervals = gc0308_enum_frameintervals,
	.enum_mbus_fmt = gc0308_enum_fmt,
	.try_mbus_fmt = gc0308_try_fmt,
	.g_parm = gc0308_g_parm,
	.s_parm = gc0308_s_parm,
};

static const struct v4l2_subdev_ops gc0308_ops = {
	.core = &gc0308_core_ops,
	.video = &gc0308_video_ops,
};

static struct soc_camera_ops gc0308_camera_ops = {
	.set_bus_param		= gc0308_set_bus_param,
	.query_bus_param	= gc0308_query_bus_param,
	.controls		= gc0308_controls,
	.num_controls		= ARRAY_SIZE(gc0308_controls),
};


//*********** proc ***************
static void dump_i2c_regs(void)
{
	int i;
	if(!s_i2c_client) {
		printk("s_i2c_client not ready\n");
		return;
	}

	for(i = 0; i <= 0xFF; i++) 
		printk("dump_i2c_regs: 0x%02X=0x%02X\n", i, i2c_smbus_read_byte_data(s_i2c_client, i) & 0xFF);
}

static int gc0308_writeproc(struct file *file,const char *buffer,
                           unsigned long count, void *data)
{
	return count;
}

static int gc0308_readproc(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	dump_i2c_regs();
	return 0;
}
//********************************


static int gc0308_init_reg1(struct i2c_client *client)
{
//printk("------gc0308_init_reg1------\n");
#ifdef CONFIG_CAMERA_CONTROLLED_BY_ARM
//printk("------gc0308_control_arm------\n");
	int i, err = -EINVAL;

	/*
	unsigned char initreg[2] = {0x12, 0x80};

	v4l_info(client, "%s: camera initialization start\n", __func__);
	err = gc0308_i2c_write(sd, initreg, 2);
	if (err < 0)
		v4l_info(client, "%s: init register set failed\n", __func__);
	mdelay(150);
*/
	printk ("lxy: gc0308_init......\n");
	for (i = 0; i < GC0308_INIT_REGS; i++) {
		err = i2c_smbus_write_byte_data(client,  gc0308_init_reg[i][0],  gc0308_init_reg[i][1]);
		if (err < 0)
			v4l_info(client, "%s: %d register set failed\n",
					__func__, i);
	}

#if 0
	i2c_smbus_write_byte_data(client,  gc0308_init_reg[i][0],  gc0308_init_reg[i][1]);
#endif
	
	if (err < 0) {
		v4l_err(client, "%s: camera initialization failed\n",
				__func__);
		return -EIO;	/* FIXME */
	}
#endif
	return 0;
}

/*
 * ov7675_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int gc0308_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	printk("------gc0308 probe--------\n");
	struct gc0308_state *state;
	struct v4l2_subdev *sd;
	struct soc_camera_device  *icd = client->dev.platform_data;
	struct i2c_adapter        *adapter = to_i2c_adapter(client->dev.parent);
	struct soc_camera_link    *icl;

	state = kzalloc(sizeof(struct gc0308_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	sd = &state->sd;
	strcpy(sd->name, GC0308_DRIVER_NAME);

	icl = to_soc_camera_link(icd);
	if (!icl) {
		dev_err(&adapter->dev,
			"GC0308: Missing platform_data for driver\n");
		return -EINVAL;
	}

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &gc0308_ops);

	icd->ops = &gc0308_camera_ops;

	gc0308_init(sd, 0);

	s_i2c_client = client;
	s_proc = create_proc_entry(GC0308_PROC_NAME, 0666, &proc_root);
	if (s_proc != NULL)
	{
		s_proc->write_proc = gc0308_writeproc;
		s_proc->read_proc = gc0308_readproc;
	}

	dev_info(&client->dev, "gc0308 has been probed\n");
/*
	// i2c_smbus_read_byte_data
	//int gc0308_id = gc0308_i2c_read(sd, 0x00);
	extern s32 i2c_smbus_read_byte_data(struct i2c_client *client, u8 command);	

	int time;
	char value;
	for (time=0; time<8; time++){
		value = i2c_smbus_read_byte_data(client, time);
		dev_info(&client->dev, "gc0308 Read out the reg 0x%x is 0x%x\n", 
					time,
					value);
	}
*/	
	return 0;
}


static int gc0308_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct soc_camera_device *icd = client->dev.platform_data;
	struct soc_camera_link *icl = to_soc_camera_link(icd);
	if (icl->free_bus)
		icl->free_bus(icl);
	icd->ops = NULL;
	if (s_proc != NULL)
		remove_proc_entry(GC0308_PROC_NAME, &proc_root);

	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id gc0308_id[] = {
	{ GC0308_DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, gc0308_id);

static struct i2c_driver gc0308_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= GC0308_DRIVER_NAME,
	},
	.probe		= gc0308_probe,
	.remove		= gc0308_remove,
	.id_table	= gc0308_id,
};

static __init int init_gc0308(void)
{
	return i2c_add_driver(&gc0308_driver);
}

static __exit void exit_gc0308(void)
{
	i2c_del_driver(&gc0308_driver);
}

module_init(init_gc0308);
module_exit(exit_gc0308);

#if 0
static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = GC0308_DRIVER_NAME,
	.probe = gc0308_probe,
	.remove = gc0308_remove,
	.id_table = gc0308_id,
};
#endif

MODULE_DESCRIPTION("GC0308 camera driver");
MODULE_AUTHOR("Ingjye Huang <Ingjye_Huang@asus.com>");
MODULE_LICENSE("GPL");
