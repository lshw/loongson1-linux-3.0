/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/mutex.h>
#include <linux/mod_devicetable.h>
#include <linux/spi/spi.h>

#define DRVNAME "mcp3201"
#define REFERENCE	5000

struct mcp3201 {
	struct device *hwmon_dev;
	struct mutex lock;
	u32 channels;
	u32 reference; /* in millivolts */
	const char *name;
};

/* sysfs hook function */
static ssize_t mcp3201_read(struct device *dev,
		struct device_attribute *devattr, char *buf, int differential)
{
	struct spi_device *spi = to_spi_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct mcp3201 *adc = spi_get_drvdata(spi);
	u8 tx_buf[1];
	u8 rx_buf[2];
	int status = -1;
	u32 value = 0;

	if (mutex_lock_interruptible(&adc->lock))
		return -ERESTARTSYS;

	switch (adc->channels) {
		case 1:	/* mcp3201 */
			status = spi_read(spi, rx_buf, sizeof(rx_buf));
			break;
		case 2:	/* mcp3202 */
			if (differential)
				tx_buf[0] = 0x04 | attr->index;
			else
				tx_buf[0] = 0x06 | attr->index;
			status = spi_write_then_read(spi, tx_buf, sizeof(tx_buf),
								rx_buf, sizeof(rx_buf));
			break;
		case 4:	/* mcp3204 */
		case 8:	/* mcp3208 */
			if (differential)
				tx_buf[0] = 0x10 | attr->index;
			else
				tx_buf[0] = 0x18 | attr->index;
			status = spi_write_then_read(spi, tx_buf, sizeof(tx_buf),
							rx_buf, sizeof(rx_buf));
			break;
	}

	if (status < 0) {
		dev_warn(dev, "SPI synch. transfer failed with status %d\n",
				status);
		goto out;
	}

	switch (adc->channels) {
		case 1:	/* mcp3201 */
			value = (rx_buf[0] << 8);
			value = value & 0x1f00;
			value += rx_buf[1] ;
			value >>= 1;
			break;
		case 2:	/* mcp3202 */
		case 4:	/* mcp3204 */
		case 8:	/* mcp3208 */
			value = (rx_buf[0] & 0x3f) << 6 | (rx_buf[1] >> 2);
			break;
	}

	dev_dbg(dev, "raw value = 0x%x\n", value);

	value = value * adc->reference >> 12;
	status = sprintf(buf, "%d\n", value);
out:
	mutex_unlock(&adc->lock);
	return status;
}

static ssize_t mcp3201_read_single(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	return mcp3201_read(dev, devattr, buf, 0);
}

static ssize_t mcp3201_read_diff(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	return mcp3201_read(dev, devattr, buf, 1);
}

static ssize_t mcp3201_show_min(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	/* The minimum reference is 0 for this chip family */
	return sprintf(buf, "0\n");
}

static ssize_t mcp3201_show_max(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct mcp3201 *adc = spi_get_drvdata(spi);
	u32 reference;

	if (mutex_lock_interruptible(&adc->lock))
		return -ERESTARTSYS;

	reference = adc->reference;

	mutex_unlock(&adc->lock);

	return sprintf(buf, "%d\n", reference);
}

static ssize_t mcp3201_set_max(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct spi_device *spi = to_spi_device(dev);
	struct mcp3201 *adc = spi_get_drvdata(spi);
	unsigned long value;

	if (strict_strtoul(buf, 10, &value))
		return -EINVAL;

	if (mutex_lock_interruptible(&adc->lock))
		return -ERESTARTSYS;

	adc->reference = value;

	mutex_unlock(&adc->lock);

	return count;
}

static ssize_t mcp3201_show_name(struct device *dev, struct device_attribute
			      *devattr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct mcp3201 *adc = spi_get_drvdata(spi);

	return sprintf(buf, "mcp320%d\n", adc->channels);
}

static struct sensor_device_attribute ad_input[] = {
	SENSOR_ATTR(name, S_IRUGO, mcp3201_show_name, NULL, 0),
	SENSOR_ATTR(Vin_min, S_IRUGO, mcp3201_show_min, NULL, 0),
	SENSOR_ATTR(Vin_max, S_IWUSR | S_IRUGO, mcp3201_show_max,
					mcp3201_set_max, 0),
	SENSOR_ATTR(single_ch0, S_IRUGO, mcp3201_read_single, NULL, 0),
	SENSOR_ATTR(diff_ch0+ch1-, S_IRUGO, mcp3201_read_diff, NULL, 0),
	SENSOR_ATTR(single_ch1, S_IRUGO, mcp3201_read_single, NULL, 1),
	SENSOR_ATTR(diff_ch1+ch0-, S_IRUGO, mcp3201_read_diff, NULL, 1),
	SENSOR_ATTR(single_ch2, S_IRUGO, mcp3201_read_single, NULL, 2),
	SENSOR_ATTR(diff_ch2+ch3-, S_IRUGO, mcp3201_read_diff, NULL, 2),
	SENSOR_ATTR(single_ch3, S_IRUGO, mcp3201_read_single, NULL, 3),
	SENSOR_ATTR(diff_ch3+ch2-, S_IRUGO, mcp3201_read_diff, NULL, 3),
	SENSOR_ATTR(single_ch4, S_IRUGO, mcp3201_read_single, NULL, 4),
	SENSOR_ATTR(diff_ch4+ch5-, S_IRUGO, mcp3201_read_diff, NULL, 4),
	SENSOR_ATTR(single_ch5, S_IRUGO, mcp3201_read_single, NULL, 5),
	SENSOR_ATTR(diff_ch5+ch4-, S_IRUGO, mcp3201_read_diff, NULL, 5),
	SENSOR_ATTR(single_ch6, S_IRUGO, mcp3201_read_single, NULL, 6),
	SENSOR_ATTR(diff_ch6+ch7-, S_IRUGO, mcp3201_read_diff, NULL, 6),
	SENSOR_ATTR(single_ch7, S_IRUGO, mcp3201_read_single, NULL, 7),
	SENSOR_ATTR(diff_ch7+ch6-, S_IRUGO, mcp3201_read_diff, NULL, 7),
};

/*----------------------------------------------------------------------*/

static int __devinit mcp3201_probe(struct spi_device *spi)
{
	int channels = spi_get_device_id(spi)->driver_data;
	struct mcp3201 *adc;
	int status;
	int i;

	adc = kzalloc(sizeof *adc, GFP_KERNEL);
	if (!adc)
		return -ENOMEM;

	/* set a default value for the reference */
	adc->reference = REFERENCE;
	adc->channels = channels;
	adc->name = spi_get_device_id(spi)->name;
	mutex_init(&adc->lock);

	mutex_lock(&adc->lock);

	spi_set_drvdata(spi, adc);

	channels = 3 + (adc->channels << 1);
	for (i = 0; i < channels; i++) {
		status = device_create_file(&spi->dev, &ad_input[i].dev_attr);
		if (status) {
			dev_err(&spi->dev, "device_create_file failed.\n");
			goto out_err;
		}
	}

	adc->hwmon_dev = hwmon_device_register(&spi->dev);
	if (IS_ERR(adc->hwmon_dev)) {
		dev_err(&spi->dev, "hwmon_device_register failed.\n");
		status = PTR_ERR(adc->hwmon_dev);
		goto out_err;
	}

	mutex_unlock(&adc->lock);
	return 0;

out_err:
	for (i--; i >= 0; i--)
		device_remove_file(&spi->dev, &ad_input[i].dev_attr);

	spi_set_drvdata(spi, NULL);
	mutex_unlock(&adc->lock);
	kfree(adc);
	return status;
}

static int __devexit mcp3201_remove(struct spi_device *spi)
{
	int channels = spi_get_device_id(spi)->driver_data;
	struct mcp3201 *adc = spi_get_drvdata(spi);
	int i;

	mutex_lock(&adc->lock);
	hwmon_device_unregister(adc->hwmon_dev);
	channels = 3 + (adc->channels << 1);
	for (i = 0; i < channels; i++)
		device_remove_file(&spi->dev, &ad_input[i].dev_attr);

	spi_set_drvdata(spi, NULL);
	mutex_unlock(&adc->lock);
	kfree(adc);

	return 0;
}

static const struct spi_device_id mcp3201_ids[] = {
	{ "mcp3201", 1 },
	{ "mcp3202", 2 },
	{ "mcp3204", 4 },
	{ "mcp3208", 8 },
	{ },
};
MODULE_DEVICE_TABLE(spi, mcp3201_ids);

static struct spi_driver mcp3201_driver = {
	.driver = {
		.name	= "mcp3201",
		.owner	= THIS_MODULE,
	},
	.id_table = mcp3201_ids,
	.probe	= mcp3201_probe,
	.remove	= __devexit_p(mcp3201_remove),
};

static int __init init_mcp3201(void)
{
	return spi_register_driver(&mcp3201_driver);
}

static void __exit exit_mcp3201(void)
{
	spi_unregister_driver(&mcp3201_driver);
}

module_init(init_mcp3201);
module_exit(exit_mcp3201);

MODULE_AUTHOR("loongson");
MODULE_DESCRIPTION("mcp3201 Linux driver");
MODULE_LICENSE("GPL");
