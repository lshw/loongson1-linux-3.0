/*
 * gc0307 Camera Driver
 *
 * Based on ov2640
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-subdev.h>
#include <media/soc_camera.h>
#include <media/soc_mediabus.h>

/* ID */
#define PID_GC0307	0x99

struct regval_list {
	u8 reg_num;
	u8 value;
};

/* Supported resolutions */
enum gc0307_width {
	W_QCIF	= 176,
	W_QVGA	= 320,
	W_CIF	= 352,
	W_VGA	= 640,
};

enum gc0307_height {
	H_QCIF	= 144,
	H_QVGA	= 240,
	H_CIF	= 288,
	H_VGA	= 480,
};

struct gc0307_win_size {
	char				*name;
	enum gc0307_width		width;
	enum gc0307_height		height;
	const struct regval_list	*regs;
};


struct gc0307_priv {
	struct v4l2_subdev		subdev;
	struct gc0307_camera_info	*info;
	enum v4l2_mbus_pixelcode	cfmt_code;
	const struct gc0307_win_size	*win;
	int				model;

	u8			contrast;
	u8			saturation;
};

#define ENDMARKER { 0xff, 0xff }

#include "ls1x_gc0307.h"

static const struct regval_list gc0307_qcif_regs[] = {
	{ 0x09, (H_QCIF >> 8) & 0xff },
	{ 0x0a, H_QCIF & 0xff },
	{ 0x0b, (H_QCIF >> 8) & 0xff },
	{ 0x0c, H_QCIF & 0xff },
	ENDMARKER,
};

static const struct regval_list gc0307_qvga_regs[] = {
	{ 0x09, (H_QVGA >> 8) & 0xff },
	{ 0x0a, H_QVGA & 0xff },
	{ 0x0b, (H_QVGA >> 8) & 0xff },
	{ 0x0c, H_QVGA & 0xff },
	ENDMARKER,
};

static const struct regval_list gc0307_cif_regs[] = {
	{ 0x09, (H_CIF >> 8) & 0xff },
	{ 0x0a, H_CIF & 0xff },
	{ 0x0b, (H_CIF >> 8) & 0xff },
	{ 0x0c, H_CIF & 0xff },
	ENDMARKER,
};

static const struct regval_list gc0307_vga_regs[] = {
	{ 0x09, (H_VGA >> 8) & 0xff },
	{ 0x0a, H_VGA & 0xff },
	{ 0x0b, (H_VGA >> 8) & 0xff },
	{ 0x0c, H_VGA & 0xff },
	ENDMARKER,
};

#define GC0307_SIZE(n, w, h, r) \
	{.name = n, .width = w , .height = h, .regs = r }

static const struct gc0307_win_size gc0307_supported_win_sizes[] = {
	GC0307_SIZE("QCIF", W_QCIF, H_QCIF, gc0307_qcif_regs),
	GC0307_SIZE("QVGA", W_QVGA, H_QVGA, gc0307_qvga_regs),
	GC0307_SIZE("CIF", W_CIF, H_CIF, gc0307_cif_regs),
	GC0307_SIZE("VGA", W_VGA, H_VGA, gc0307_vga_regs),
};

static const struct regval_list gc0307_yuyv_regs[] = {
	{ 0x44, 0xe2 },
	ENDMARKER,
};

static const struct regval_list gc0307_yvyu_regs[] = {
	{ 0x44, 0xe3 },
	ENDMARKER,
};

static const struct regval_list gc0307_uyvy_regs[] = {
	{ 0x44, 0xe0 },
	ENDMARKER,
};

static const struct regval_list gc0307_vyuy_regs[] = {
	{ 0x44, 0xe1 },
	ENDMARKER,
};

static const struct regval_list gc0307_rgb565_regs[] = {
	{ 0x44, 0xe6 },
	ENDMARKER,
};

static enum v4l2_mbus_pixelcode gc0307_codes[] = {
	V4L2_MBUS_FMT_RGB565_2X8_LE,
	V4L2_MBUS_FMT_YUYV8_2X8,
};

/*
 * Supported controls
 */
static const struct v4l2_queryctrl gc0307_controls[] = {
	{
		.id = V4L2_CID_CONTRAST,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Contrast",
		.minimum = 0x10,
		.maximum = 0x30,
		.step = 1,
		.default_value = 0x20,
	}, {
		.id = V4L2_CID_SATURATION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Saturation",
		.minimum = 0x00,
		.maximum = 0x80,
		.step = 1,
		.default_value = 0x40,
	},
};

/*
 * General functions
 */
static struct gc0307_priv *to_gc0307(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct gc0307_priv,
			    subdev);
}

static int gc0307_write_array(struct i2c_client *client,
			      const struct regval_list *vals)
{
	int ret;

	while ((vals->reg_num != 0xff) || (vals->value != 0xff)) {
		ret = i2c_smbus_write_byte_data(client,
						vals->reg_num, vals->value);
		dev_vdbg(&client->dev, "array: 0x%02x, 0x%02x",
			 vals->reg_num, vals->value);

		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

static int gc0307_reset(struct i2c_client *client)
{
//	int ret = i2c_smbus_write_byte_data(client, COM7, SCCB_RESET);
//	msleep(1);
//	return ret;
}

/*
 * soc_camera_ops functions
 */
static int gc0307_s_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static int gc0307_set_bus_param(struct soc_camera_device *icd,
				unsigned long flags)
{
	struct soc_camera_link *icl = to_soc_camera_link(icd);
	unsigned long width_flag = flags & SOCAM_DATAWIDTH_MASK;

	/* Only one width bit may be set */
	if (!is_power_of_2(width_flag))
		return -EINVAL;

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

static unsigned long gc0307_query_bus_param(struct soc_camera_device *icd)
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

static int gc0307_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client  *client = v4l2_get_subdevdata(sd);
	struct gc0307_priv *priv = to_gc0307(client);

	switch (ctrl->id) {
	case V4L2_CID_CONTRAST:
		ctrl->value = priv->contrast;
		break;
	case V4L2_CID_SATURATION:
		ctrl->value = priv->saturation;
		break;
	}
	return 0;
}

static int gc0307_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client  *client = v4l2_get_subdevdata(sd);
	struct gc0307_priv *priv = to_gc0307(client);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_CONTRAST:
		priv->contrast = ctrl->value;
		ret = i2c_smbus_write_byte_data(client,
						0x77, ctrl->value);
		break;
	case V4L2_CID_SATURATION:
		priv->saturation = ctrl->value;
		ret = i2c_smbus_write_byte_data(client,
						0xa0, ctrl->value);
		break;
	}

	return ret;
}

static int gc0307_g_chip_ident(struct v4l2_subdev *sd,
			       struct v4l2_dbg_chip_ident *id)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct gc0307_priv *priv = to_gc0307(client);

	id->ident    = priv->model;
	id->revision = 0;

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int gc0307_g_register(struct v4l2_subdev *sd,
			     struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	reg->size = 1;
	if (reg->reg > 0xff)
		return -EINVAL;

	ret = i2c_smbus_read_byte_data(client, reg->reg);
	if (ret < 0)
		return ret;

	reg->val = ret;

	return 0;
}

static int gc0307_s_register(struct v4l2_subdev *sd,
			     struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (reg->reg > 0xff ||
	    reg->val > 0xff)
		return -EINVAL;

	return i2c_smbus_write_byte_data(client, reg->reg, reg->val);
}
#endif

/* Select the nearest higher resolution for capture */
static const struct gc0307_win_size *gc0307_select_win(u32 *width, u32 *height)
{
	int i, default_size = ARRAY_SIZE(gc0307_supported_win_sizes) - 1;

	for (i = 0; i < ARRAY_SIZE(gc0307_supported_win_sizes); i++) {
		if (gc0307_supported_win_sizes[i].width  >= *width &&
		    gc0307_supported_win_sizes[i].height >= *height) {
			*width = gc0307_supported_win_sizes[i].width;
			*height = gc0307_supported_win_sizes[i].height;
			return &gc0307_supported_win_sizes[i];
		}
	}

	*width = gc0307_supported_win_sizes[default_size].width;
	*height = gc0307_supported_win_sizes[default_size].height;
	return &gc0307_supported_win_sizes[default_size];
}

static int gc0307_set_params(struct i2c_client *client, u32 *width, u32 *height,
			     enum v4l2_mbus_pixelcode code)
{
	struct gc0307_priv       *priv = to_gc0307(client);
	const struct regval_list *selected_cfmt_regs;
	int ret;

	/* select win */
	priv->win = gc0307_select_win(width, height);

	/* select format */
	priv->cfmt_code = 0;
	switch (code) {
	case V4L2_MBUS_FMT_YUYV8_2X8:
		dev_dbg(&client->dev, "%s: Selected cfmt YUYV", __func__);
		selected_cfmt_regs = gc0307_yuyv_regs;
		break;
	case V4L2_MBUS_FMT_YVYU8_2X8:
		dev_dbg(&client->dev, "%s: Selected cfmt YVYU", __func__);
		selected_cfmt_regs = gc0307_yvyu_regs;
		break;
	case V4L2_MBUS_FMT_VYUY8_2X8:
		dev_dbg(&client->dev, "%s: Selected cfmt VYUY", __func__);
		selected_cfmt_regs = gc0307_vyuy_regs;
		break;
	case V4L2_MBUS_FMT_UYVY8_2X8:
		dev_dbg(&client->dev, "%s: Selected cfmt UYVY", __func__);
		selected_cfmt_regs = gc0307_uyvy_regs;
		break;
	case V4L2_MBUS_FMT_RGB565_2X8_LE:
		dev_dbg(&client->dev, "%s: Selected cfmt RGB565", __func__);
		selected_cfmt_regs = gc0307_rgb565_regs;
		break;
	default:
		break;
	}

	/* reset hardware */
	gc0307_reset(client);

	/* initialize the sensor with default data */
	dev_dbg(&client->dev, "%s: Init default", __func__);
	ret = gc0307_write_array(client, gc0307_init_regs);
	if (ret < 0)
		goto err;

	/* set cfmt */
	ret = gc0307_write_array(client, selected_cfmt_regs);
	if (ret < 0)
		goto err;

	priv->cfmt_code = code;
	*width = priv->win->width;
	*height = priv->win->height;

	return 0;

err:
	dev_err(&client->dev, "%s: Error %d", __func__, ret);
	gc0307_reset(client);
	priv->win = NULL;

	return ret;
}

static int gc0307_g_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client  *client = v4l2_get_subdevdata(sd);
	struct gc0307_priv *priv = to_gc0307(client);

	if (!priv->win) {
		u32 width = W_VGA, height = H_VGA;
		int ret = gc0307_set_params(client, &width, &height,
					    V4L2_MBUS_FMT_YUYV8_2X8);
		if (ret < 0)
			return ret;
	}

	mf->width	= priv->win->width;
	mf->height	= priv->win->height;
	mf->code	= priv->cfmt_code;

	switch (mf->code) {
	case V4L2_MBUS_FMT_YVYU8_2X8:
	case V4L2_MBUS_FMT_YUYV8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
	case V4L2_MBUS_FMT_UYVY8_2X8:
		mf->colorspace = V4L2_COLORSPACE_JPEG;
		break;
	default:
		mf->code = V4L2_MBUS_FMT_RGB565_2X8_LE;
	case V4L2_MBUS_FMT_RGB565_2X8_LE:
		mf->colorspace = V4L2_COLORSPACE_SRGB;
	}
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int gc0307_s_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;


	switch (mf->code) {
	case V4L2_MBUS_FMT_YVYU8_2X8:
	case V4L2_MBUS_FMT_YUYV8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
	case V4L2_MBUS_FMT_UYVY8_2X8:
		mf->colorspace = V4L2_COLORSPACE_JPEG;
		break;
	default:
		mf->code = V4L2_MBUS_FMT_RGB565_2X8_LE;
	case V4L2_MBUS_FMT_RGB565_2X8_LE:
		mf->colorspace = V4L2_COLORSPACE_SRGB;
	}

	ret = gc0307_set_params(client, &mf->width, &mf->height, mf->code);

	return ret;
}

static int gc0307_try_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *mf)
{
	const struct gc0307_win_size *win;

	/*
	 * select suitable win
	 */
	win = gc0307_select_win(&mf->width, &mf->height);

	mf->field	= V4L2_FIELD_NONE;

	switch (mf->code) {
	case V4L2_MBUS_FMT_YVYU8_2X8:
	case V4L2_MBUS_FMT_YUYV8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
	case V4L2_MBUS_FMT_UYVY8_2X8:
		mf->colorspace = V4L2_COLORSPACE_JPEG;
		break;
	default:
		mf->code = V4L2_MBUS_FMT_RGB565_2X8_LE;
	case V4L2_MBUS_FMT_RGB565_2X8_LE:
		mf->colorspace = V4L2_COLORSPACE_SRGB;
	}

	return 0;
}

static int gc0307_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(gc0307_codes))
		return -EINVAL;

	*code = gc0307_codes[index];
	return 0;
}

static int gc0307_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= W_VGA;
	a->c.height	= H_VGA;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int gc0307_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->bounds.width			= W_VGA;
	a->bounds.height		= H_VGA;
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int gc0307_video_probe(struct soc_camera_device *icd,
			      struct i2c_client *client)
{
	struct gc0307_priv *priv = to_gc0307(client);
	u8 pid;
	const char *devname;
	int ret;

	/*
	 * we must have a parent by now. And it cannot be a wrong one.
	 * So this entire test is completely redundant.
	 */
	if (!icd->dev.parent ||
	    to_soc_camera_host(icd->dev.parent)->nr != icd->iface) {
		dev_err(&client->dev, "Parent missing or invalid!\n");
		ret = -ENODEV;
		goto err;
	}

	/*
	 * check and show product ID and manufacturer ID
	 */
	pid = i2c_smbus_read_byte_data(client, 0x00);

	switch (pid) {
	case PID_GC0307:
		devname     = "gc0307";
		priv->model = V4L2_IDENT_GC0307;
		break;
	default:
		dev_err(&client->dev,
			"Product ID error %x\n", pid);
		ret = -ENODEV;
		goto err;
	}

	dev_info(&client->dev,
		 "%s Product ID %0x\n", devname, pid);

	return 0;

err:
	return ret;
}

static struct soc_camera_ops gc0307_ops = {
	.set_bus_param		= gc0307_set_bus_param,
	.query_bus_param	= gc0307_query_bus_param,
	.controls		= gc0307_controls,
	.num_controls		= ARRAY_SIZE(gc0307_controls),
};

static struct v4l2_subdev_core_ops gc0307_subdev_core_ops = {
	.g_ctrl		= gc0307_g_ctrl,
	.s_ctrl		= gc0307_s_ctrl,
	.g_chip_ident	= gc0307_g_chip_ident,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= gc0307_g_register,
	.s_register	= gc0307_s_register,
#endif
};

static struct v4l2_subdev_video_ops gc0307_subdev_video_ops = {
	.s_stream	= gc0307_s_stream,
	.g_mbus_fmt	= gc0307_g_fmt,
	.s_mbus_fmt	= gc0307_s_fmt,
	.try_mbus_fmt	= gc0307_try_fmt,
	.cropcap	= gc0307_cropcap,
	.g_crop		= gc0307_g_crop,
	.enum_mbus_fmt	= gc0307_enum_fmt,
};

static struct v4l2_subdev_ops gc0307_subdev_ops = {
	.core	= &gc0307_subdev_core_ops,
	.video	= &gc0307_subdev_video_ops,
};

/*
 * i2c_driver functions
 */
static int gc0307_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct gc0307_priv        *priv;
	struct soc_camera_device  *icd = client->dev.platform_data;
	struct i2c_adapter        *adapter = to_i2c_adapter(client->dev.parent);
	struct soc_camera_link    *icl;
	int                        ret;

	if (!icd) {
		dev_err(&adapter->dev, "GC0307: missing soc-camera data!\n");
		return -EINVAL;
	}

	icl = to_soc_camera_link(icd);
	if (!icl) {
		dev_err(&adapter->dev,
			"GC0307: Missing platform_data for driver\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&adapter->dev,
			"GC0307: I2C-Adapter doesn't support SMBUS\n");
		return -EIO;
	}

	priv = kzalloc(sizeof(struct gc0307_priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&adapter->dev,
			"Failed to allocate memory for private data!\n");
		return -ENOMEM;
	}

	priv->info = icl->priv;

	v4l2_i2c_subdev_init(&priv->subdev, client, &gc0307_subdev_ops);

	icd->ops = &gc0307_ops;

	ret = gc0307_video_probe(icd, client);
	if (ret) {
		icd->ops = NULL;
		kfree(priv);
	} else {
		dev_info(&adapter->dev, "GC0307 Probed\n");
	}

	return ret;
}

static int gc0307_remove(struct i2c_client *client)
{
	struct gc0307_priv       *priv = to_gc0307(client);
	struct soc_camera_device *icd = client->dev.platform_data;

	icd->ops = NULL;
	kfree(priv);
	return 0;
}

static const struct i2c_device_id gc0307_id[] = {
	{ "gc0307", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc0307_id);

static struct i2c_driver gc0307_i2c_driver = {
	.driver = {
		.name = "gc0307",
	},
	.probe    = gc0307_probe,
	.remove   = gc0307_remove,
	.id_table = gc0307_id,
};

/*
 * Module functions
 */
static int __init gc0307_module_init(void)
{
	return i2c_add_driver(&gc0307_i2c_driver);
}

static void __exit gc0307_module_exit(void)
{
	i2c_del_driver(&gc0307_i2c_driver);
}

module_init(gc0307_module_init);
module_exit(gc0307_module_exit);

MODULE_DESCRIPTION("SoC Camera driver for gc0307 sensor");
MODULE_AUTHOR("Loongson");
MODULE_LICENSE("GPL v2");
