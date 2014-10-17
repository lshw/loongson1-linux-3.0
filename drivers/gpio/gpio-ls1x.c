/*
 *  Copyright (c) 2013 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>

#define GPIO_LS1X_INPUT_MINOR 252

#define LINE_DI_INPUT0	170
#define LINE_DI_INPUT1	171
#define LINE_DI_INPUT2	172
#define LINE_DI_INPUT3	173

static int gpio_ls1x_open(struct inode *inode, struct file *filep)
{
	return nonseekable_open(inode, filep);
}

static ssize_t
gpio_ls1x_read(struct file *filep, char __user *buf, size_t count, loff_t *ptr)
{
	u8 val;
	val = (gpio_get_value(LINE_DI_INPUT0)) << 0;
	val |= (gpio_get_value(LINE_DI_INPUT1)) << 1;
	val |= (gpio_get_value(LINE_DI_INPUT2)) << 2;
	val |= (gpio_get_value(LINE_DI_INPUT3)) << 3;

	if (copy_to_user(buf, &val, sizeof(val))) {
		printk("get gpio error!\n");
		return -EFAULT;
	}

	return count;
}

static long
gpio_ls1x_unlocked_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static ssize_t gpio_ls1x_write(struct file *filep, const char __user *buf,
			    size_t count, loff_t *ptr)
{
	return 0;
}

static const struct file_operations gpio_ls1x_fops = {
	.owner		= THIS_MODULE,
	.open		= gpio_ls1x_open,
	.read		= gpio_ls1x_read,
	.write		= gpio_ls1x_write,
	.unlocked_ioctl	= gpio_ls1x_unlocked_ioctl,
	.llseek		= no_llseek,
};

static struct miscdevice gpio_ls1x_miscdev = {
	.minor = GPIO_LS1X_INPUT_MINOR,
	.name = "gpio_ls1x",
	.fops = &gpio_ls1x_fops
};

static void gpio_input_init(void)
{
	gpio_request(LINE_DI_INPUT0, "gpio_inptu");
	gpio_request(LINE_DI_INPUT1, "gpio_inptu");
	gpio_request(LINE_DI_INPUT2, "gpio_inptu");
	gpio_request(LINE_DI_INPUT3, "gpio_inptu");

	gpio_direction_input(LINE_DI_INPUT0);
	gpio_direction_input(LINE_DI_INPUT1);
	gpio_direction_input(LINE_DI_INPUT2);
	gpio_direction_input(LINE_DI_INPUT3);
}

static int __init gpio_ls1x_init(void)
{
	gpio_input_init();
	return misc_register(&gpio_ls1x_miscdev);
}

module_init(gpio_ls1x_init);

static void __exit gpio_ls1x_cleanup(void)
{
	misc_deregister(&gpio_ls1x_miscdev);
}
module_exit(gpio_ls1x_cleanup);

MODULE_DESCRIPTION("loongson1 gpio input valu");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tang Hai <tanghaifeng-gz@loongson.cn>");
