/*
 *  Copyright (c) 2013 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/am2301.h>

struct am2301 {
	struct am2301_platform_data *pdata;
	struct device *hwmon_dev;
	struct mutex lock;
	char valid;
	unsigned long last_update;
	int temperature;
	int humidity;
};

static unsigned char inline am2301_read_byte(struct am2301 *am2301)
{
	unsigned char am2301_byte = 0;
	int i, temp;

	for (i = 0; i < 8; i++) {
		temp = 0;
		while (!(gpio_get_value(am2301->pdata->pin))) {
			temp++;
			if (temp > 12) {
				return 1;
			}
			udelay(5);
		}
		temp = 0;
		while (gpio_get_value(am2301->pdata->pin)) {
			temp++;
			if (temp > 20) {
				return 1;
			}
			udelay (5);
		}
		if (temp > 8) {
			am2301_byte <<= 1;
			am2301_byte |= 1;
		} 
		else {
			am2301_byte <<= 1;
			am2301_byte |= 0;
		}
	}
	return am2301_byte;
}

static int am2301_update_measurements(struct am2301 *am2301)
{
	int ret = 0, err = 0, i;
	unsigned char data_temp;
	unsigned char temp_buf[5];
	unsigned long irq_flags;

	mutex_lock(&am2301->lock);
	/* 两次测量间隔为2秒 */
	if (time_after(jiffies, am2301->last_update + 2 * HZ) || !am2301->valid) {
		local_irq_save(irq_flags);

		gpio_direction_output(am2301->pdata->pin, 0);
		udelay(900);
		gpio_set_value(am2301->pdata->pin, 1);
		udelay(35);
		gpio_direction_input(am2301->pdata->pin);

		if (!err) {
			data_temp = 10;
			while (!(gpio_get_value(am2301->pdata->pin)) && data_temp) {
				data_temp--;
				udelay(10);
			}
			if (!data_temp) {
				err = 1;
				ret = -EFAULT;
			}
		}
		if (!err) {
			data_temp = 10;
			while (gpio_get_value(am2301->pdata->pin) && data_temp) {
				data_temp--;
				udelay(10);
			}
			if (!data_temp) {
				err = 1;
				ret = -EFAULT;
			}
		}
		if (!err) {
			for (i = 0; i < 5; i++) {
				temp_buf[i] = am2301_read_byte(am2301);
			}
			local_irq_restore(irq_flags);

			data_temp = 0;
			for ( i = 0; i < 4; i ++ ) {
				data_temp += temp_buf[i];
			}
			if (data_temp != temp_buf[4]) {
				ret = 1;
			}

			am2301->temperature = ((temp_buf[2] & 0x7F) * 256) + 
								(((temp_buf[3] >> 4) & 0x0F) * 16) + 
								(temp_buf[3] & 0x0F);
			if (temp_buf[2] & 0x80)
				am2301->temperature *= -1;
			am2301->humidity = ((temp_buf[0] & 0xFF) * 256) + 
							(((temp_buf[1] >> 4) & 0x0F) * 16) + 
							(temp_buf[1] & 0x0F);

			am2301->last_update = jiffies;
			am2301->valid = 1;
		}
		gpio_direction_output(am2301->pdata->pin, 1);
	}

	mutex_unlock(&am2301->lock);

	return ret;
}

static ssize_t am2301_show_temperature(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct am2301 *am2301 = platform_get_drvdata(pdev);

	int ret = am2301_update_measurements(am2301);
	if (ret < 0) {
		dev_err(&pdev->dev, "get temperature err! %d\n", ret);
		return ret;
	}
	return sprintf(buf, "%d\n", am2301->temperature);
}

static ssize_t am2301_show_humidity(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct am2301 *am2301 = platform_get_drvdata(pdev);

	int ret = am2301_update_measurements(am2301);
	if (ret < 0) {
		dev_err(&pdev->dev, "get humidity err! %d\n", ret);
		return ret;
	}
	return sprintf(buf, "%d\n", am2301->humidity);
}

/* sysfs attributes */
static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, am2301_show_temperature,
	NULL, 0);
static SENSOR_DEVICE_ATTR(humidity1_input, S_IRUGO, am2301_show_humidity,
	NULL, 0);

static struct attribute *am2301_attributes[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_humidity1_input.dev_attr.attr,
	NULL
};

static const struct attribute_group am2301_attr_group = {
	.attrs = am2301_attributes,
};

static int __devinit am2301_probe(struct platform_device *pdev)
{
	struct am2301_platform_data *pdata;
	struct am2301 *am2301;
	int err;
	int ret;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data defined\n");
		return -EINVAL;
	}

	ret = gpio_request(pdata->pin, "am2301");
	if (ret < 0)
		return ret;
	gpio_direction_output(pdata->pin, 1);

	am2301 = kzalloc(sizeof(*am2301), GFP_KERNEL);
	if (!am2301) {
		dev_dbg(&pdev->dev, "kzalloc failed\n");
		return -ENOMEM;
	}

	am2301->pdata = pdata;
	platform_set_drvdata(pdev, am2301);
	mutex_init(&am2301->lock);

	err = sysfs_create_group(&pdev->dev.kobj, &am2301_attr_group);
	if (err) {
		dev_dbg(&pdev->dev, "could not create sysfs files\n");
		goto fail_free;
	}
	am2301->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(am2301->hwmon_dev)) {
		dev_dbg(&pdev->dev, "unable to register hwmon device\n");
		err = PTR_ERR(am2301->hwmon_dev);
		goto fail_remove_sysfs;
	}

	dev_info(&pdev->dev, "initialized\n");
	return 0;

fail_remove_sysfs:
	sysfs_remove_group(&pdev->dev.kobj, &am2301_attr_group);
fail_free:
	kfree(am2301);
	gpio_free(pdata->pin);

	return err;
}

static int __devexit am2301_remove(struct platform_device *pdev)
{
	struct am2301 *am2301 = platform_get_drvdata(pdev);

	hwmon_device_unregister(am2301->hwmon_dev);
	sysfs_remove_group(&pdev->dev.kobj, &am2301_attr_group);
	kfree(am2301);
	gpio_free(am2301->pdata->pin);

	return 0;
}

static struct platform_driver am2301_driver = {
	.probe = am2301_probe,
	.remove = __devexit_p(am2301_remove),
	.driver = {
		.name = "am2301",
		.owner = THIS_MODULE,
	},
};

static int __init am2301_init(void)
{
	return platform_driver_register(&am2301_driver);
}
module_init(am2301_init);

static void __exit am2301_cleanup(void)
{
	platform_driver_unregister(&am2301_driver);
}
module_exit(am2301_cleanup);

MODULE_DESCRIPTION("Aosong am2301 humidity and temperature sensor driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tang Hai <tanghaifeng-gz@loongson.cn>");
