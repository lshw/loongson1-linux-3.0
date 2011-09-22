#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <asm/gpio.h>
//#include <asm/semaphore.h>
#include <ls1b_board.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/fs.h>

#define	LS1B_BOARD_SPI0_BASE	0xbfe80000

struct spi_device *adc;

static int mcp3201_open(struct inode *inode, struct file *file) {
	//取消片选CS3
	writeb(0xff, LS1B_BOARD_SPI0_BASE + REG_SPCSR);

	//复位SPI控制寄存器工作
	writeb(0x10, LS1B_BOARD_SPI0_BASE + REG_SPCR);

	//重置状态寄存器SPSR
	writeb(0xc0, LS1B_BOARD_SPI0_BASE + REG_SPSR);

	//设置外部寄存器
	writeb(0x02, LS1B_BOARD_SPI0_BASE + REG_SPER);

	//禁用SPI FLASH读
	writeb(0x30, LS1B_BOARD_SPI0_BASE + REG_SPPR);

	//配置SPI时序
	writeb(0xd3, LS1B_BOARD_SPI0_BASE + REG_SPCR);

	//设置片选CS3
	writeb(0x7f, LS1B_BOARD_SPI0_BASE + REG_SPCSR);

	return 0;
}

static int mcp3201_close(struct inode *inode, struct file *file) {
	//取消对CS3的片选
	writeb(0xff, LS1B_BOARD_SPI0_BASE + REG_SPCSR);

	return 0;
}

static ssize_t mcp3201_read(struct file *file, char __user *buf, size_t count, loff_t *ptr)
{
	ssize_t retval;
	unsigned char val[2] = {0x0};
	unsigned char tx_buf[1] = {0x0};
	unsigned char rx_buf[2] = {0};

	retval = spi_write_then_read(adc, tx_buf, 0, rx_buf, 2);

	if (retval < 0) {
		  dev_err(&adc->dev, "error %d reading SR\n", (int)retval);
		  return retval;
		}

	//对从mcp3201读来的数据，按照其datasheet的要求取出编码
	val[0] = rx_buf[1] >> 1;
	val[0] |= (rx_buf[0] & 0x01) << 7;
	val[1] = (rx_buf[0] >> 1) & 0xf;

	if (copy_to_user(buf, val, 2))
	{
		printk("Copy data to userspace error!\n");
		return -EFAULT;
	}

	return 0;
}

static int __devinit mcp3201_probe(struct spi_device *spi)
{
	struct spi_device *spi0;
	spi0 = spi;
	adc = spi0;

	return 0;
}

static int __devexit mcp3201_remove(struct spi_device *spi)
{
	writeb(0xff, LS1B_BOARD_SPI0_BASE + REG_SPCSR);

	return 0;
}

static struct spi_driver mcp3201_driver = {
	.driver = {
	  .name	= "mcp3201",
	  .bus	= &spi_bus_type,
	  .owner	= THIS_MODULE,
	},
	.probe	= mcp3201_probe,
	.remove	= __devexit_p(mcp3201_remove),
};

static const struct file_operations mcp3201_ops = {
		.owner = THIS_MODULE,
		.open = mcp3201_open,
		.release = mcp3201_close,
		.read = mcp3201_read,
};

static struct miscdevice mcp3201_miscdev = {
		MISC_DYNAMIC_MINOR,
		"mcp3201",
		&mcp3201_ops,
};

static int mcp3201_init(void)
{
	if (misc_register(&mcp3201_miscdev)) {
		printk(KERN_WARNING "buzzer:Couldn't register device 10, %d.\n", 255);
		return -EBUSY;
	}

	return spi_register_driver(&mcp3201_driver);
}

static void mcp3201_exit(void)
{
	spi_unregister_driver(&mcp3201_driver);
}

module_init(mcp3201_init);
module_exit(mcp3201_exit);

MODULE_LICENSE("GPL");
