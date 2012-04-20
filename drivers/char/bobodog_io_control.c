#include <linux/fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h> 
#include <linux/gpio_keys.h>
#include <asm/gpio.h>

#define DEVNAME "bobodog_gpio"
#define BOBODOG_MINOR 0	

static int major;

static struct gpio_keys_platform_data *pkb = NULL; 

#define DEBUG_BOBODOG
#ifdef DEBUG_BOBODOG
static int bobodog_write(struct file *filp, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	char code[2];
	int port,value;
	copy_from_user(code, buffer, (count < 2)?count:2);

	value = code[0] - '0';
printk("before\n");
printk("value: %d\n", value);
	if (value >= pkb->nbuttons)
		return -ENOMEM;
	port = pkb->buttons[value].gpio;
	value = code[1] - '0';

printk("port: %d\n", port);
printk("value: %d\n", value);
	gpio_set_value_cansleep(port, value);

	return count;	
}
#else 
static int bobodog_write(struct file *filp, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	char code[2];
	int ledth;
	int value;
	int port;
	copy_from_user(code, buffer, (count < 2)?count:2);
	ledth = code[0];
	value = code[1];

	if(ledth >= pkb->nbuttons)
		return -ENOMEM;

	port = pkb->buttons[ledth].gpio;

	gpio_set_value_cansleep(port, value);

	return count;	
}
#endif

static int __devinit bobodog_gpio_probe(struct platform_device *pdev)
{
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	int i;

	pkb = pdata;
	for(i = 0;i < pkb->nbuttons;i++) {
		gpio_request(pkb->buttons[i].gpio,"bobodog_gpio");
		gpio_direction_output(pkb->buttons[i].gpio, 0);
	}

	return 0;
}

static const struct file_operations bobodog_fops = {
	.owner		= THIS_MODULE,
	.write		= bobodog_write,
};

static struct miscdevice bobodog_misc_device = {
	BOBODOG_MINOR,
	"bobodog_io_control",
	&bobodog_fops,
};

struct platform_driver bobodog_device_driver = {
	.probe		= bobodog_gpio_probe,
	.driver		= {
		.name 	= "bobodog_io_control",
	}
};

static int __init bobodog_init(void)
{
	if(misc_register(&bobodog_misc_device)){
		printk(KERN_WARNING "bobodog:Couldn't register device %d.\n", BOBODOG_MINOR);
		return -EBUSY;
	}

	return platform_driver_register(&bobodog_device_driver);
}

static void __exit bobodog_exit(void)
{
	int i;
	misc_deregister(&bobodog_misc_device);
	for(i = 0;i < pkb->nbuttons;i++){
		gpio_free(pkb->buttons[i].gpio);
	}
}

module_init(bobodog_init);
module_exit(bobodog_exit);


module_param(major, int, 0);
MODULE_AUTHOR("xuhongmeng");
MODULE_DESCRIPTION("bobodog io control");
MODULE_LICENSE("GPL");

