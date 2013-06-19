/*
 *  Copyright (c) 2013 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 *
 *  Based partly on i2c-at91.c
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#define I2C_CLOCK		400000		/* Hz. max 400 Kbits/sec */

/* registers */
#define OCI2C_PRELOW		0
#define OCI2C_PREHIGH		1
#define OCI2C_CONTROL		2
#define OCI2C_DATA		3
#define OCI2C_CMD		4 /* write only */
#define OCI2C_STATUS		4 /* read only, same address as OCI2C_CMD */

#define OCI2C_CTRL_IEN		0x40
#define OCI2C_CTRL_EN		0x80

#define OCI2C_CMD_START		0x90
#define OCI2C_CMD_STOP		0x40
#define OCI2C_CMD_READ		0x20
#define OCI2C_CMD_WRITE		0x10
#define OCI2C_CMD_READ_ACK	0x20
#define OCI2C_CMD_READ_NACK	0x28
#define OCI2C_CMD_IACK		0x00

#define OCI2C_STAT_IF		0x01
#define OCI2C_STAT_TIP		0x02
#define OCI2C_STAT_ARBLOST	0x20
#define OCI2C_STAT_BUSY		0x40
#define OCI2C_STAT_NACK		0x80


struct ls1x_i2c {
	void __iomem *base;
	struct i2c_adapter adap;
};

static inline void i2c_writeb(struct ls1x_i2c *i2c, int reg, u8 value)
{
	writeb(value, i2c->base + reg);
}

static inline u8 i2c_readb(struct ls1x_i2c *i2c, int reg)
{
	return readb(i2c->base + reg);
}

/*
 * Poll the i2c status register until the specified bit is set.
 * Returns 0 if timed out (100 msec).
 */
static short ls1x_poll_status(struct ls1x_i2c *i2c, unsigned long bit)
{
	int loop_cntr = 10000;

	do {
		udelay(10);
	} while ((i2c_readb(i2c, OCI2C_STATUS) & bit) && (--loop_cntr > 0));

	return (loop_cntr > 0);
}

static int ls1x_xfer_read(struct ls1x_i2c *i2c, unsigned char *buf, int length) 
{
	int x;

	for (x=0; x<length; x++) {
		/* send ACK last not send ACK */
		if (x != (length -1)) 
			i2c_writeb(i2c, OCI2C_CMD, OCI2C_CMD_READ_ACK);
		else
			i2c_writeb(i2c, OCI2C_CMD, OCI2C_CMD_READ_NACK);

		if (!ls1x_poll_status(i2c, OCI2C_STAT_TIP)) {
			dev_dbg(&i2c->adap.dev, "READ timeout\n");
			return -ETIMEDOUT;
		}
		*buf++ = i2c_readb(i2c, OCI2C_DATA);
	}
	i2c_writeb(i2c,OCI2C_CMD, OCI2C_CMD_STOP);
		
	return 0;
}

static int ls1x_xfer_write(struct ls1x_i2c *i2c, unsigned char *buf, int length)
{
	int x;

	for (x=0; x<length; x++) {
		i2c_writeb(i2c, OCI2C_DATA, *buf++);
		i2c_writeb(i2c, OCI2C_CMD, OCI2C_CMD_WRITE);
		if (!ls1x_poll_status(i2c, OCI2C_STAT_TIP)) {
			dev_dbg(&i2c->adap.dev, "WRITE timeout\n");
			return -ETIMEDOUT;
		}
		if (i2c_readb(i2c, OCI2C_STATUS) & OCI2C_STAT_NACK) {
			i2c_writeb(i2c, OCI2C_CMD, OCI2C_CMD_STOP);
			return length;
		}
	}
	i2c_writeb(i2c, OCI2C_CMD, OCI2C_CMD_STOP);

	return 0;
}

static int ls1x_xfer(struct i2c_adapter *adap, struct i2c_msg *pmsg, int num)
{
	struct ls1x_i2c *i2c = (struct ls1x_i2c *)adap->algo_data;
	int i, ret;

	dev_dbg(&adap->dev, "ls1x_xfer: processing %d messages:\n", num);

	for (i = 0; i < num; i++) {
		dev_dbg(&adap->dev, " #%d: %sing %d byte%s %s 0x%02x\n", i,
			pmsg->flags & I2C_M_RD ? "read" : "writ",
			pmsg->len, pmsg->len > 1 ? "s" : "",
			pmsg->flags & I2C_M_RD ? "from" : "to",	pmsg->addr);

		if (!ls1x_poll_status(i2c, OCI2C_STAT_BUSY)) {
			return -ETIMEDOUT;
		}

		i2c_writeb(i2c, OCI2C_DATA, (pmsg->addr << 1)
			| ((pmsg->flags & I2C_M_RD) ? 1 : 0));
		i2c_writeb(i2c, OCI2C_CMD, OCI2C_CMD_START);

		/* Wait until transfer is finished */
		if (!ls1x_poll_status(i2c, OCI2C_STAT_TIP)) {
			dev_dbg(&adap->dev, "TXCOMP timeout\n");
			return -ETIMEDOUT;
		}

		if (i2c_readb(i2c, OCI2C_STATUS) & OCI2C_STAT_NACK) {
			dev_err(&adap->dev, "slave addr no ack !!\n");
			i2c_writeb(i2c, OCI2C_CMD, OCI2C_CMD_STOP);
			return 0;
		}

 		if (pmsg->flags & I2C_M_RD)
			ret = ls1x_xfer_read(i2c, pmsg->buf, pmsg->len);
  		else
			ret = ls1x_xfer_write(i2c, pmsg->buf, pmsg->len);

		if (ret)
			return ret;
		dev_dbg(&adap->dev, "transfer complete\n");
		pmsg++;
	}
	return i;
}

static void ls1x_i2c_hwinit(struct ls1x_i2c *i2c)
{
	struct clk *clk;
	int prescale;
	u8 ctrl = i2c_readb(i2c, OCI2C_CONTROL);

	/* make sure the device is disabled */
	i2c_writeb(i2c, OCI2C_CONTROL, ctrl & ~(OCI2C_CTRL_EN|OCI2C_CTRL_IEN));

	clk = clk_get(NULL, "apb");
	prescale = clk_get_rate(clk);
	prescale = (prescale / (5*I2C_CLOCK)) - 1;
	i2c_writeb(i2c, OCI2C_PRELOW, prescale & 0xff);
	i2c_writeb(i2c, OCI2C_PREHIGH, prescale >> 8);

	/* Init the device */
	i2c_writeb(i2c, OCI2C_CMD, OCI2C_CMD_IACK);
	i2c_writeb(i2c, OCI2C_CONTROL, ctrl | OCI2C_CTRL_IEN | OCI2C_CTRL_EN);
}

static u32 ls1x_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm ls1x_algorithm = {
	.master_xfer	= ls1x_xfer,
	.functionality	= ls1x_functionality,
};

static int __devinit ls1x_i2c_probe(struct platform_device *pdev)
{
	struct ls1x_i2c *i2c;
	struct resource *res;
	int rc;

	res =  platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "cannot find IO resource\n");
		return -ENOENT;
	}

	if (!(i2c = kzalloc(sizeof(struct ls1x_i2c), GFP_KERNEL))) {
		dev_err(&pdev->dev, "can't allocate inteface!\n");
		rc = -ENOMEM;
		goto fail2;
	}
	
	if (!request_mem_region(res->start, resource_size(res), "ls1x-i2c"))
		return -EBUSY;

	i2c->base = ioremap(res->start, resource_size(res));
	if (!i2c->base) {
		dev_err(&pdev->dev, "i2c-ls1x - failed to map controller\n");
		rc = -ENOMEM;
		goto fail1;
	}

	strlcpy(i2c->adap.name, "loongson1", sizeof(i2c->adap.name));
	i2c->adap.owner   = THIS_MODULE;
	i2c->adap.algo    = &ls1x_algorithm;
	i2c->adap.class   = I2C_CLASS_HWMON;
	i2c->adap.algo_data = i2c;
	i2c->adap.dev.parent = &pdev->dev;
	i2c->adap.nr = pdev->id;

	ls1x_i2c_hwinit(i2c);

	platform_set_drvdata(pdev, i2c);

	rc = i2c_add_numbered_adapter(&i2c->adap);
	if (rc) {
		dev_err(&pdev->dev, "Adapter %s registration failed\n",
				i2c->adap.name);
		goto fail0;
	}

	dev_info(&pdev->dev, "Loongson1 i2c bus driver.\n");
	return 0;

fail0:
	platform_set_drvdata(pdev, NULL);
fail1:
	iounmap(i2c->base);
	release_mem_region(res->start, resource_size(res));
fail2:
	kfree(i2c);

	return rc;
}

static int __devexit ls1x_i2c_remove(struct platform_device *pdev)
{
	struct ls1x_i2c *i2c = platform_get_drvdata(pdev);
	struct resource *res;
	int rc;

	rc = i2c_del_adapter(&i2c->adap);
	platform_set_drvdata(pdev, NULL);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	iounmap(i2c->base);
	release_mem_region(res->start, resource_size(res));

	return rc;
}

#ifdef CONFIG_PM
static int ls1x_i2c_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int ls1x_i2c_resume(struct platform_device *pdev)
{
	return 0;
}

#else
#define ls1x_i2c_suspend	NULL
#define ls1x_i2c_resume	NULL
#endif

static struct platform_driver ls1x_i2c_driver = {
	.probe		= ls1x_i2c_probe,
	.remove		= __devexit_p(ls1x_i2c_remove),
	.suspend	= ls1x_i2c_suspend,
	.resume		= ls1x_i2c_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "ls1x-i2c",
	},
};

static int __init ls1x_i2c_init(void)
{
	return platform_driver_register(&ls1x_i2c_driver);
}

static void __exit ls1x_i2c_exit(void)
{
	platform_driver_unregister(&ls1x_i2c_driver);
}

module_init(ls1x_i2c_init);
module_exit(ls1x_i2c_exit);

MODULE_AUTHOR("TangHaifeng <tanghaifeng-gz@loongson.cn>");
MODULE_DESCRIPTION("I2C driver for Loongson 1A/1B");
MODULE_LICENSE("GPL");

