/*
 * ATT7022 Polyphase Multifunction Energy Metering IC Driver (SPI Bus)
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>

#include "../iio.h"
#include "att7022.h"

static int att7022_spi_write_reg_24(struct device *dev,
		u16 reg_address,
		u32 value)
{
	int ret;
	struct spi_message msg;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct att7022_state *st = iio_dev_get_devdata(indio_dev);
	struct spi_transfer xfer = {
		.tx_buf = st->tx,
		.bits_per_word = 8,
		.len = 4,
//		.delay_usecs = 0,
	};

	mutex_lock(&st->buf_lock);
	st->tx[0] = ATT7022_WRITE_REG | reg_address;
	st->tx[1] = (value >> 16) & 0xFF;
	st->tx[2] = (value >> 8) & 0xFF;
	st->tx[3] = value & 0xFF;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(st->spi, &msg);
	mutex_unlock(&st->buf_lock);

	return ret;
}

static int att7022_spi_read_reg_24(struct device *dev,
		u16 reg_address,
		u32 *val)
{
	struct spi_message msg;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct att7022_state *st = iio_dev_get_devdata(indio_dev);
	int ret;
	struct spi_transfer xfers[] = {
		{
			.tx_buf = st->tx,
			.bits_per_word = 8,
			.len = 1,
//			.delay_usecs = 0,
		}, {
			.rx_buf = st->rx,
			.bits_per_word = 8,
			.len = 3,
//			.delay_usecs = 0,
		}
	};

	mutex_lock(&st->buf_lock);

	st->tx[0] = reg_address;

	spi_message_init(&msg);
	spi_message_add_tail(&xfers[0], &msg);
	spi_message_add_tail(&xfers[1], &msg);
	ret = spi_sync(st->spi, &msg);
	if (ret) {
		dev_err(&st->spi->dev, "problem when reading 24 bit register 0x%02X",
				reg_address);
		goto error_ret;
	}
	*val = (st->rx[0] << 16) | (st->rx[1] << 8) | st->rx[2];

error_ret:
	mutex_unlock(&st->buf_lock);
	return ret;
}

static int __devinit att7022_spi_probe(struct spi_device *spi)
{
	int ret;
	struct att7022_state *st = kzalloc(sizeof *st, GFP_KERNEL);
	if (!st) {
		ret =  -ENOMEM;
		return ret;
	}

	spi_set_drvdata(spi, st);
	st->read_reg_24 = att7022_spi_read_reg_24;
	st->write_reg_24 = att7022_spi_write_reg_24;
	st->irq = spi->irq;
	st->spi = spi;

	ret = att7022_probe(st, &spi->dev);
	if (ret) {
		kfree(st);
		return ret;
	}

	return 0;
}

static int att7022_spi_remove(struct spi_device *spi)
{
	att7022_remove(spi_get_drvdata(spi));

	return 0;
}
static const struct spi_device_id att7022_id[] = {
	{ "att7022", 0 },
	{ }
};

static struct spi_driver att7022_driver = {
	.driver = {
		.name = "att7022",
		.owner = THIS_MODULE,
	},
	.probe = att7022_spi_probe,
	.remove = __devexit_p(att7022_spi_remove),
	.id_table = att7022_id,
};

static __init int att7022_init(void)
{
	return spi_register_driver(&att7022_driver);
}
module_init(att7022_init);

static __exit void att7022_exit(void)
{
	spi_unregister_driver(&att7022_driver);
}
module_exit(att7022_exit);

MODULE_AUTHOR("Barry Song <21cnbao@gmail.com>");
MODULE_DESCRIPTION("Analog Devices ATT7022/58/68/78 SPI Driver");
MODULE_LICENSE("GPL v2");
