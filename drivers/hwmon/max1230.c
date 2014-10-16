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
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/mutex.h>
#include <linux/mod_devicetable.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#define DRVNAME "max1230"
#define REFERENCE	5000
#define GPIO_EOC 177

/* Reset Register */
#define RESET				0x10	//	reset max1230 to its default power - up state
#define CLEAR_FIFO		0x18	//	clear the FIFO register

/* Setup Register */
#define SETUP_DEFAULT	0x60	//	setup register location + default clock mode 10
										//	conversion clock = internal, acquisition/sampling = internal
										//	!CNVST = AIN15, REF- = AIN14, voltage reference = internal, internal
										//	reference off after scan, reference need wake-up delay
										//	channels mode = unipolar single ended

#define DIFFSEL0			0x01	//	unipolar/bipolar mode register configuration for differential mode
#define DIFFSEL1			0x02	//	unipolar/bipolar mode register configuration for differential mode
#define REFSEL0			0x04	//	reference mode configuration
#define REFSEL1			0x08	//	reference mode configuration
#define CKSEL0				0x10	//	clock mode and CNVST configuration
#define CKSEL1				0x20	//	clock mode and CNVST configuration, resets to 1 at power-up

/* Channel Mode Selection */
#define SINGLE_ENDED		0x00	//	No data follows the setup byte.
										//	Unipolar mode and bipolar mode registers remain unchanged.
										//	In single-ended mode, the MAX1228 always operate in unipolar mode.
#define DIFF_UNIPOLAR	0x02	//	One byte of data follows the setup byte
										//	and is written to the unipolar mode register.
#define DIFF_BIPOLAR		0x03	//	One byte of data follows the setup byte
										//	and is written to the bipolar mode register.

/* Reference Type Selection */
#define REF_INT_WKUP_DL	0x00	//	voltage reference = internal, internal reference off after scan,
										//	need wake-up delay, pin REF- = AIN14
#define REF_EXT_SIN_END	0x04	//	voltage reference = external single ended, internal reference off,
										//	no wake-up delay, pin REF- = AIN14
#define REF_INT_NO_WKUP	0x08	//	voltage reference = internal, reference always on, no wake-up delay
										//	pin REF- = AIN14
#define REF_EXT_DIFF		0x0C	//	voltage reference = external differential, internal reference off,
										//	no wake-up delay, pin REF- = REF-

/* Average Register */
#define AVERAGE_DEFAULT	0x20	//	average register location, averaging off

#define AVGON				0x10	//	averaging on/off bit
#define NAVG1				0x08	//	configures the number of conversions for single-channel scans
#define NAVG0				0x04	//	configures the number of conversions for single-channel scans
#define NSCAN1				0x02	//	single-channel scan count (scan mode 10 only)
#define NSCAN0				0x01	//	single-channel scan count (scan mode 10 only)

/* Conversion Averaging Control */
#define AVERAGE_OFF		0x00	//	turn averaging off (default value)
#define AVERAGE_ON		0x10	//	turn averaging on

/* Number of conversions for one averaged result per single channel */
#define AVG_4				0x00	//	performs 4 conversions, returns the average for each requested result
#define AVG_8				0x04	//	performs 8 conversions, returns the average for each requested result
#define AVG_16				0x08	//	performs 16 conversions, returns the average for each requested result
#define AVG_32				0x0C	//	performs 32 conversions, returns the average for each requested result

/* Number of scans per one single channel */
#define SCAN_4				0x00	//	scans channel N and returns 4 results
#define SCAN_8				0x01	//	scans channel N and returns 8 results
#define SCAN_12			0x02	//	scans channel N and returns 12 results
#define SCAN_16			0x03	//	scans channel N and returns 16 results

/* Conversion Register */
#define CONVERSION_DEFAULT	0x80	//	conversion register location, default channel = AIN0,
										//	scan mode = scans channels 0 through N
										//	temperature measurement off
#define CHSEL3				0x40	//	analog input channel select
#define CHSEL2				0x20	//	analog input channel select
#define CHSEL1				0x10	//	analog input channel select
#define CHSEL0				0x08	//	analog input channel select

#define SCAN1				0x04	//	scan mode select
#define SCAN0				0x02	//	scan mode select

#define TEMP				0x01	//	set to 1 to take a single temperature measurement, the first conversion
										//	result of a scan contains temperature information

/* Input Channel Selection */
#define CHANNEL_0			0x00		//	select channel AIN0
#define CHANNEL_1			0x08		//	select channel AIN1
#define CHANNEL_2			0x10		//	select channel AIN2
#define CHANNEL_3			0x18		//	select channel AIN3
#define CHANNEL_4			0x20		//	select channel AIN4
#define CHANNEL_5			0x28		//	select channel AIN5
#define CHANNEL_6			0x30		//	select channel AIN6
#define CHANNEL_7			0x38		//	select channel AIN7
#define CHANNEL_8			0x40		//	select channel AIN8
#define CHANNEL_9			0x48		//	select channel AIN9
#define CHANNEL_10		0x50		//	select channel AIN10
#define CHANNEL_11		0x58		//	select channel AIN11
#define CHANNEL_12		0x60		//	select channel AIN12
#define CHANNEL_13		0x68		//	select channel AIN13
#define CHANNEL_14		0x70		//	select channel AIN14
#define CHANNEL_15		0x78		//	select channel AIN15

/* Scan Mode Selection */
#define SCAN_00			0x00		//	scans channels 0 through N
#define SCAN_01			0x02		//	scans channels N through the highest numbered channel.
#define SCAN_10			0x04		//	scans channel N repeatedly, the averaging register sets the number
											//	of results
#define SCAN_11			0x06		//	no scan, converts channel N once only

/* Temperature Measurement */
#define TEMP_MEAS_ON		0x01	//	enable temperature measurement
#define TEMP_MEAS_OFF	0x00	//	disable temperature measurement (default)

struct max1230 {
	struct device *hwmon_dev;
	struct mutex lock;
	u32 channels;
	u32 reference; /* in millivolts */
	const char *name;
	struct completion	cmd_complete;
};

/* Function sets the MAX1230 to its default power - up state.
	The MAX1230 powers up with all blocks in shutdown, including the reference.
	All registers power up in state 00000000, except for the setup register,
	which powers up in clock mode 10 (CKSEL1 = 1). */
static void max1230_reset(struct spi_device *spi)
{
	u8 tx_buf[1];

	tx_buf[0] = RESET;
	spi_write(spi, tx_buf, sizeof(tx_buf));
}

/* Function clears only the FIFO register of MAX1230. */
static void max1230_clear_fifo(struct spi_device *spi)
{
	u8 tx_buf[1];

	tx_buf[0] = CLEAR_FIFO;
	spi_write(spi, tx_buf, sizeof(tx_buf));
}

static void max1230_config(struct spi_device *spi, unsigned char channels_mode, unsigned char reference, unsigned char input_channels)
{
	unsigned char tx_buf[1];
	unsigned char setup_reg = SETUP_DEFAULT;	//	power-up state of max1230 setup register
	unsigned char channels = 0;					//	power-up state of max1230 bipolar and unipolar
															//	mode registers
	
	if (channels_mode == DIFF_UNIPOLAR) {
		setup_reg |= DIFF_UNIPOLAR;
		channels = input_channels;
	}
	if (channels_mode == DIFF_BIPOLAR) {
		setup_reg |= DIFF_BIPOLAR;
		channels = input_channels;
	}
	setup_reg |= reference;

	tx_buf[0] = setup_reg;
	spi_write(spi, tx_buf, sizeof(tx_buf));

	if (channels_mode != SINGLE_ENDED) {
		tx_buf[0] = channels;
		spi_write(spi, tx_buf, sizeof(tx_buf));
	}
}

/* Function writes to the averaging register to configure the ADC to average up to 32 samples for each
	requested result, and to independently control the number of results requested for single-channel
	scans. For appropriate settings look at max1230.h file or max1230 datasheet. */
static void max1230_average(struct spi_device *spi, unsigned char avg_control, unsigned char avg_conversions, unsigned char scans_per_channel)
{
	unsigned char tx_buf[1];
	unsigned char average_register = AVERAGE_DEFAULT;

	average_register |= (avg_control | avg_conversions | scans_per_channel);

	tx_buf[0] = average_register;
	spi_write(spi, tx_buf, sizeof(tx_buf));
}

/* Function selects active analog input channels, scan modes, and a single temperature measurement per
	scan by writing to the conversion register.
	For more information look at max1230.h file or max1230 datasheet */
void max1230_conversion(struct spi_device *spi, unsigned char channel, unsigned char scan_mode, unsigned char temperature_meas)
{
	unsigned char tx_buf[1];
	unsigned char conversion_reg = CONVERSION_DEFAULT;

	conversion_reg |= (channel | scan_mode | temperature_meas);

	tx_buf[0] = conversion_reg;
	spi_write(spi, tx_buf, sizeof(tx_buf));
}

/* sysfs hook function */
static ssize_t max1230_read(struct device *dev,
		struct device_attribute *devattr, char *buf, int differential)
{
	struct spi_device *spi = to_spi_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct max1230 *adc = spi_get_drvdata(spi);
	u8 tx_buf[1];
	u8 rx_buf[2];
	int status = -1;
	u32 value = 0;
	int i;

	if (mutex_lock_interruptible(&adc->lock))
		return -ERESTARTSYS;

	init_completion(&adc->cmd_complete);
//	max1230_reset(spi);
	max1230_clear_fifo(spi);
//	max1230_reset(spi);

	switch (adc->channels) {
		case 8:	/* max1226 */
			max1230_config(spi, SINGLE_ENDED, REF_INT_NO_WKUP, 0xF0);
			break;
		case 12:	/* max1228 */
			max1230_config(spi, SINGLE_ENDED, REF_INT_NO_WKUP, 0xFC);
			break;
		case 16:	/* max1230 */
			max1230_config(spi, SINGLE_ENDED, REF_INT_NO_WKUP, 0xFF);
			break;
	}

	max1230_average(spi, AVERAGE_ON, AVG_8, SCAN_8);
	msleep(50);
	max1230_conversion(spi, attr->index << 3, SCAN_11, TEMP_MEAS_OFF);
	wait_for_completion(&adc->cmd_complete);
//	for (i=0; i<16; i++) {
	status = spi_read(spi, rx_buf, sizeof(rx_buf));
	value = rx_buf[0] << 8;
	value |= rx_buf[1];

	dev_dbg(dev, "raw value = 0x%x\n", value);
//	printk("raw value = 0x%x\n", value);
//	}
	value = value * adc->reference >> 12;
	status = sprintf(buf, "%d\n", value);
out:
	mutex_unlock(&adc->lock);
	return status;
}

static ssize_t max1230_read_single(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	return max1230_read(dev, devattr, buf, 0);
}

static ssize_t max1230_read_diff(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	return max1230_read(dev, devattr, buf, 1);
}

static ssize_t max1230_show_min(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	/* The minimum reference is 0 for this chip family */
	return sprintf(buf, "0\n");
}

static ssize_t max1230_show_max(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct max1230 *adc = spi_get_drvdata(spi);
	u32 reference;

	if (mutex_lock_interruptible(&adc->lock))
		return -ERESTARTSYS;

	reference = adc->reference;

	mutex_unlock(&adc->lock);

	return sprintf(buf, "%d\n", reference);
}

static ssize_t max1230_set_max(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct spi_device *spi = to_spi_device(dev);
	struct max1230 *adc = spi_get_drvdata(spi);
	unsigned long value;

	if (strict_strtoul(buf, 10, &value))
		return -EINVAL;

	if (mutex_lock_interruptible(&adc->lock))
		return -ERESTARTSYS;

	adc->reference = value;

	mutex_unlock(&adc->lock);

	return count;
}

static ssize_t max1230_show_name(struct device *dev, struct device_attribute
			      *devattr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct max1230 *adc = spi_get_drvdata(spi);

	switch (adc->channels) {
		case 8:	/* max1226 */
			return sprintf(buf, "max1226\n");
			break;
		case 12:	/* max1228 */
			return sprintf(buf, "max1228\n");
			break;
		case 16:	/* max1230 */
			return sprintf(buf, "max1230\n");
			break;
	}
	return 0;
}

static struct sensor_device_attribute ad_input[] = {
	SENSOR_ATTR(name, S_IRUGO, max1230_show_name, NULL, 0),
	SENSOR_ATTR(Vin_min, S_IRUGO, max1230_show_min, NULL, 0),
	SENSOR_ATTR(Vin_max, S_IWUSR | S_IRUGO, max1230_show_max,
					max1230_set_max, 0),
	SENSOR_ATTR(single_ch0, S_IRUGO, max1230_read_single, NULL, 0),
	SENSOR_ATTR(diff_ch0+ch1-, S_IRUGO, max1230_read_diff, NULL, 0),
	SENSOR_ATTR(single_ch1, S_IRUGO, max1230_read_single, NULL, 1),
	SENSOR_ATTR(diff_ch1+ch0-, S_IRUGO, max1230_read_diff, NULL, 1),
	SENSOR_ATTR(single_ch2, S_IRUGO, max1230_read_single, NULL, 2),
	SENSOR_ATTR(diff_ch2+ch3-, S_IRUGO, max1230_read_diff, NULL, 2),
	SENSOR_ATTR(single_ch3, S_IRUGO, max1230_read_single, NULL, 3),
	SENSOR_ATTR(diff_ch3+ch2-, S_IRUGO, max1230_read_diff, NULL, 3),
	SENSOR_ATTR(single_ch4, S_IRUGO, max1230_read_single, NULL, 4),
	SENSOR_ATTR(diff_ch4+ch5-, S_IRUGO, max1230_read_diff, NULL, 4),
	SENSOR_ATTR(single_ch5, S_IRUGO, max1230_read_single, NULL, 5),
	SENSOR_ATTR(diff_ch5+ch4-, S_IRUGO, max1230_read_diff, NULL, 5),
	SENSOR_ATTR(single_ch6, S_IRUGO, max1230_read_single, NULL, 6),
	SENSOR_ATTR(diff_ch6+ch7-, S_IRUGO, max1230_read_diff, NULL, 6),
	SENSOR_ATTR(single_ch7, S_IRUGO, max1230_read_single, NULL, 7),
	SENSOR_ATTR(diff_ch7+ch6-, S_IRUGO, max1230_read_diff, NULL, 7),

	SENSOR_ATTR(single_ch8, S_IRUGO, max1230_read_single, NULL, 8),
	SENSOR_ATTR(diff_ch8+ch7-, S_IRUGO, max1230_read_diff, NULL, 8),
	SENSOR_ATTR(single_ch9, S_IRUGO, max1230_read_single, NULL, 9),
	SENSOR_ATTR(diff_ch9+ch8-, S_IRUGO, max1230_read_diff, NULL, 9),
	SENSOR_ATTR(single_ch10, S_IRUGO, max1230_read_single, NULL, 10),
	SENSOR_ATTR(diff_ch10+ch9-, S_IRUGO, max1230_read_diff, NULL, 10),
	SENSOR_ATTR(single_ch11, S_IRUGO, max1230_read_single, NULL, 11),
	SENSOR_ATTR(diff_ch11+ch10-, S_IRUGO, max1230_read_diff, NULL, 11),
	SENSOR_ATTR(single_ch12, S_IRUGO, max1230_read_single, NULL, 12),
	SENSOR_ATTR(diff_ch12+ch11-, S_IRUGO, max1230_read_diff, NULL, 12),
	SENSOR_ATTR(single_ch13, S_IRUGO, max1230_read_single, NULL, 13),
	SENSOR_ATTR(diff_ch13+ch12-, S_IRUGO, max1230_read_diff, NULL, 13),
	SENSOR_ATTR(single_ch14, S_IRUGO, max1230_read_single, NULL, 14),
	SENSOR_ATTR(diff_ch14+ch13-, S_IRUGO, max1230_read_diff, NULL, 14),
	SENSOR_ATTR(single_ch15, S_IRUGO, max1230_read_single, NULL, 15),
	SENSOR_ATTR(diff_ch15+ch14-, S_IRUGO, max1230_read_diff, NULL, 15),
};

/*----------------------------------------------------------------------*/

static irqreturn_t max1230_eoc_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static irqreturn_t max1230_eoc_interrupt(int irq, void *id)
{
	struct max1230 *adc = id;
	complete(&adc->cmd_complete);
	return IRQ_HANDLED;
}

static int max1230_gpio(struct max1230 *adc)
{
	int ret;
	ret = gpio_request(GPIO_EOC, "eoc");
	if (ret < 0)
		return ret;
	gpio_direction_input(GPIO_EOC);

	ret = request_threaded_irq(GPIO_EOC, max1230_eoc_handler, max1230_eoc_interrupt, 
			  IRQF_DISABLED | IRQF_TRIGGER_FALLING, "max1230_eoc", adc);
	if (ret) {
		printk("max1230 eoc handler resigered Error:%d\n", ret);
		return ret;
	}
	disable_irq(GPIO_EOC);
	return 0;
}

static int __devinit max1230_probe(struct spi_device *spi)
{
	int channels = spi_get_device_id(spi)->driver_data;
	struct max1230 *adc;
	int status;
	int i;

	adc = kzalloc(sizeof *adc, GFP_KERNEL);
	if (!adc)
		return -ENOMEM;

	init_completion(&adc->cmd_complete);
	max1230_gpio(adc);

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

	max1230_reset(spi);
//	max1230_config(spi, DIFF_UNIPOLAR, REF_EXT_SIN_END, 0xFF);
	max1230_config(spi, SINGLE_ENDED, REF_INT_NO_WKUP, 0xFF);
	enable_irq(GPIO_EOC);

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

static int __devexit max1230_remove(struct spi_device *spi)
{
	int channels = spi_get_device_id(spi)->driver_data;
	struct max1230 *adc = spi_get_drvdata(spi);
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

static const struct spi_device_id max1230_ids[] = {
	{ "max1226", 8 },
	{ "max1228", 12 },
	{ "max1230", 16 },
	{ },
};
MODULE_DEVICE_TABLE(spi, max1230_ids);

static struct spi_driver max1230_driver = {
	.driver = {
		.name	= "max1230",
		.owner	= THIS_MODULE,
	},
	.id_table = max1230_ids,
	.probe	= max1230_probe,
	.remove	= __devexit_p(max1230_remove),
};

static int __init init_max1230(void)
{
	return spi_register_driver(&max1230_driver);
}

static void __exit exit_max1230(void)
{
	spi_unregister_driver(&max1230_driver);
}

module_init(init_max1230);
module_exit(exit_max1230);

MODULE_AUTHOR("loongson");
MODULE_DESCRIPTION("max1230 Linux driver");
MODULE_LICENSE("GPL");
