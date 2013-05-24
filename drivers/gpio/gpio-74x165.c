/*
 *	74Hx165 - Generic parallel-out/serial-in 8-bits shift register GPIO driver
 *
 *	Copyright (C) 2012 Erich Schroeter <eschroeter@magnetek.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <linux/moduleparam.h>
#include <linux/gpio.h>

#include <linux/spi/spi.h>
#include <linux/spi/74x165.h>

#include <asm/uaccess.h>

#define DEFAULT_DAISY_CHAINED 1

#define USER_BUFF_SIZE 128

static int registered_by_this_driver = 0;
const char this_driver_name[] = "74x165";

struct nxp_74x165_chip {
	struct spi_device	*spi_device;
	struct gpio_chip	gpio_chip;
	struct mutex		lock;
	u8			*rx_buff;
	u32			latch;
	u32			daisy_chained;
	dev_t			devt;
	struct cdev		cdev;
	struct class		*class;
	char			*user_buff;
};

static int nxp_74x165_read_spi(struct nxp_74x165_chip *chip)
{
	int status;

	/* toggle latch to capture inputs */
	gpio_set_value(chip->latch, 0);
	gpio_set_value(chip->latch, 1);

	mutex_lock(&chip->lock);

	if (chip->spi_device)
		status = spi_read(chip->spi_device, chip->rx_buff,
					chip->daisy_chained);
	else
		status = -ENODEV;

	mutex_unlock(&chip->lock);

	return status;
}

static ssize_t nxp_74x165_show_value(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct nxp_74x165_chip *chip = dev_get_drvdata(dev);
	int i;
	int j;

	nxp_74x165_read_spi(chip);
	for (i = j = 0; i < chip->daisy_chained; ++i) {
		j += sprintf(buf + j, "%x", chip->rx_buff[i]);
	}
	sprintf(buf + j, "\n");

	return strlen(buf);
}
static DEVICE_ATTR(value, S_IRUGO, nxp_74x165_show_value, NULL);

static struct device_attribute *dev_attrs[] = {
	&dev_attr_value,
};

static int nxp_74x165_get_value(struct gpio_chip *gc, unsigned offset)
{
	struct nxp_74x165_chip *chip = container_of(gc, struct nxp_74x165_chip, gpio_chip);
	int ret = 0;
	int reg;
	int off;

	nxp_74x165_read_spi(chip);
	/* figure out what shift register the bit is located, then the bit */
	reg = offset / 8;
	off = offset % 8;
	ret = (chip->rx_buff[reg] >> off) & 0x1;

	return ret;
}

static int nxp_74x165_direction_input(struct gpio_chip *gc,
		unsigned offset)
{
	nxp_74x165_get_value(gc, offset);
	return 0;
}

static int __devinit nxp_74x165_probe(struct spi_device *spi)
{
	struct nxp_74x165_chip_platform_data	*pdata;
	struct nxp_74x165_chip			*chip;
	int ret = 0;
	int ptr;

	pdata = spi->dev.platform_data;
	if (!pdata || !pdata->base || !pdata->latch) {
		dev_dbg(&spi->dev, "incorrect or missing platform data\n");
		return -EINVAL;
	}

	if (!pdata->daisy_chained) {
		pr_alert("%s : daisy_chained reset to %d\n",
			this_driver_name, DEFAULT_DAISY_CHAINED);
		pdata->daisy_chained = DEFAULT_DAISY_CHAINED;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		ret = -ENOMEM;
		goto fail_chip;
	}
	spi_set_drvdata(spi, chip);

	mutex_init(&chip->lock);

	chip->daisy_chained = pdata->daisy_chained;
	chip->latch = pdata->latch;
	chip->spi_device = spi;

	chip->rx_buff = kmalloc(chip->daisy_chained, GFP_KERNEL | GFP_DMA);
	if (!chip->rx_buff) {
		ret = -ENOMEM;
		goto fail_rx_buff;
	}

	ret = gpio_request(chip->latch, "74x165-latch");
	if (ret < 0 && !registered_by_this_driver) {
		dev_dbg(&spi->dev, "latch not available\n");
		goto fail_gpio_request;
	}
	registered_by_this_driver = 1;
	gpio_direction_output(chip->latch, 0);

	chip->gpio_chip.label = spi->modalias;
	chip->gpio_chip.direction_input = nxp_74x165_direction_input;
	chip->gpio_chip.get = nxp_74x165_get_value;
	chip->gpio_chip.base = pdata->base;
	chip->gpio_chip.ngpio = chip->daisy_chained * 8;
	chip->gpio_chip.can_sleep = 1;
	chip->gpio_chip.dev = &spi->dev;
	chip->gpio_chip.owner = THIS_MODULE;

	ret = gpiochip_add(&chip->gpio_chip);
	if (ret) {
		dev_dbg(&spi->dev, "Failed adding GPIO chip\n");
		goto fail_gpio_add;
	}

	ret = nxp_74x165_read_spi(chip);
	if (ret) {
		dev_err(&spi->dev, "Failed reading: %d\n", ret);
		goto fail_read;
	}

	for (ptr = 0; ptr < ARRAY_SIZE(dev_attrs); ptr++) {
		ret = device_create_file(&spi->dev, dev_attrs[ptr]);
		if (ret) {
			dev_err(&spi->dev, "cannot create device attribute\n");
			goto fail_attributes;
		}
	}

	return ret;

fail_attributes:
	while (--ptr >= 0)
		device_remove_file(&spi->dev, dev_attrs[ptr]);

	device_remove_file(&spi->dev, &dev_attr_value);
fail_read:
	if (gpiochip_remove(&chip->gpio_chip))
		dev_dbg(&spi->dev, "could not remove gpio chip\n");

fail_gpio_add:
	gpio_free(chip->latch);
fail_gpio_request:
	kfree(chip->rx_buff);
	chip->rx_buff = 0;
fail_rx_buff:
	spi_set_drvdata(spi, NULL);
	kfree(chip);
fail_chip:
	return ret;
}

static int __devexit nxp_74x165_remove(struct spi_device *spi)
{
	struct nxp_74x165_chip *chip;
	int ret;
	int ptr;

	chip = dev_get_drvdata(&spi->dev);
	if (chip == NULL)
		return -ENODEV;

	dev_set_drvdata(&spi->dev, NULL);

	chip->spi_device = NULL;

	for (ptr = 0; ptr < ARRAY_SIZE(dev_attrs); ptr++)
		device_remove_file(&spi->dev, dev_attrs[ptr]);

	device_remove_file(&spi->dev, &dev_attr_value);

	gpio_free(chip->latch);

	ret = gpiochip_remove(&chip->gpio_chip);
	if (!ret)
		kfree(chip);
	else
		dev_err(&spi->dev, "Failed to remove the GPIO controller: %d\n",
				ret);

	return ret;
}

static struct spi_driver nxp_74x165_spi_driver = {
	.driver = {
		.name		= this_driver_name,
		.owner		= THIS_MODULE,
	},
	.probe		= nxp_74x165_probe,
	.remove		= __devexit_p(nxp_74x165_remove),
};

static int __init nxp_74x165_init_spi(void)
{
	return spi_register_driver(&nxp_74x165_spi_driver);
}

static int __init nxp_74x165_init(void)
{
	int ret;

	ret = nxp_74x165_init_spi();
	if (ret)
		goto fail_spi_init;

	return 0;

fail_spi_init:
	return ret;
}
subsys_initcall(nxp_74x165_init);

static void __exit nxp_74x165_exit(void)
{
	spi_unregister_driver(&nxp_74x165_spi_driver);
}
module_exit(nxp_74x165_exit);

MODULE_AUTHOR("Erich Schroeter <eschroeter@magnetek.com>");
MODULE_DESCRIPTION("GPIO expander driver for 74X165 8-bit shift register");
MODULE_LICENSE("GPL");