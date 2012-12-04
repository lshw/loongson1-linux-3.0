/*
 * Based partly on savagefb-i2c.c rivafb-i2c.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/gfp.h>
#include <linux/fb.h>
#include <video/ls1xfb.h>

#include <asm/gpio.h>

#include "edid.h"

#define LS1X_DDC 	0x50

#define VGA_CR_IX	0x3d4
#define VGA_CR_DATA	0x3d5

//#if defined(CONFIG_LS1A_CLOUD_TERMIAL)
#if defined(CONFIG_LS1A_MACH)
#define SCL_GPIO	64
#define SDA_GPIO	65
//#elif defined(CONFIG_LS1B_CORE_BOARD)
#elif defined(CONFIG_LS1B_MACH)
#define SCL_GPIO	32
#define SDA_GPIO	33
#endif

static void ls1x_gpio_setscl(void *data, int val)
{
	struct ls1xfb_i2c_chan *chan = data;

	if (val)
		gpio_direction_input(chan->scl_pin);
	else
		gpio_direction_output(chan->scl_pin, 0);
}

static void ls1x_gpio_setsda(void *data, int val)
{
	struct ls1xfb_i2c_chan *chan = data;

	if (val)
		gpio_direction_input(chan->sda_pin);
	else
		gpio_direction_output(chan->sda_pin, 0);
}

static int ls1x_gpio_getscl(void *data)
{
	struct ls1xfb_i2c_chan *chan = data;

	return gpio_get_value(chan->scl_pin);
}

static int ls1x_gpio_getsda(void *data)
{
	struct ls1xfb_i2c_chan *chan = data;

	return gpio_get_value(chan->sda_pin);
}

static int ls1x_setup_i2c_bus(struct ls1xfb_i2c_chan *chan,
				const char *name)
{
	int rc = 0;

	if (chan->par) {
		strcpy(chan->adapter.name, name);
		chan->adapter.owner		= THIS_MODULE;
		chan->adapter.algo_data		= &chan->algo;
		chan->adapter.dev.parent	= chan->par->dev;
		chan->algo.udelay		= 10;
		chan->algo.timeout		= 20;
		chan->algo.data 		= chan;

		i2c_set_adapdata(&chan->adapter, chan);

		/* Raise SCL and SDA */
		chan->algo.setsda(chan, 1);
		chan->algo.setscl(chan, 1);
		udelay(20);

		rc = i2c_bit_add_bus(&chan->adapter);

		if (rc == 0)
			dev_dbg(chan->par->dev,
				"I2C bus %s registered.\n", name);
		else
			dev_warn(chan->par->dev,
				 "Failed to register I2C bus %s.\n", name);
	}

	return rc;
}

void ls1xfb_create_i2c_busses(struct fb_info *info)
{
	struct ls1xfb_info *par = info->par;
	par->chan.par	= par;

//	switch (par->chip) {
//	case S3_SAVAGE2000:
		par->chan.sda_pin = SDA_GPIO;
		par->chan.scl_pin = SCL_GPIO;
		gpio_request(par->chan.sda_pin, "sda");
		gpio_request(par->chan.scl_pin, "scl");
		gpio_direction_input(par->chan.sda_pin);
		gpio_direction_input(par->chan.scl_pin);
//		par->chan.reg         = MM_SERIAL1;
//		par->chan.ioaddr      = par->mmio.vbase;
		par->chan.algo.setsda = ls1x_gpio_setsda;
		par->chan.algo.setscl = ls1x_gpio_setscl;
		par->chan.algo.getsda = ls1x_gpio_getsda;
		par->chan.algo.getscl = ls1x_gpio_getscl;
//		break;
//	default:
//		par->chan.par = NULL;
//	}

	ls1x_setup_i2c_bus(&par->chan, "LS1X DDC2");
}

void ls1xfb_delete_i2c_busses(struct fb_info *info)
{
	struct ls1xfb_info *par = info->par;

	gpio_free(par->chan.sda_pin);
	gpio_free(par->chan.scl_pin);
	if (par->chan.par)
		i2c_del_adapter(&par->chan.adapter);

	par->chan.par = NULL;
}

int ls1xfb_probe_i2c_connector(struct fb_info *info, u8 **out_edid)
{
	struct ls1xfb_info *par = info->par;
	u8 *edid;

	if (par->chan.par)
		edid = fb_ddc_read(&par->chan.adapter);
	else
		edid = NULL;

	if (!edid) {
		/* try to get from firmware */
		const u8 *e = fb_firmware_edid(info->device);

		if (e)
			edid = kmemdup(e, EDID_LENGTH, GFP_KERNEL);
	}

	*out_edid = edid;

	return (edid) ? 0 : 1;
}

MODULE_LICENSE("GPL");
