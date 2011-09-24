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

#define DEVNAME "buzzer_gpio"

#define BUZZER_MINOR	123

MODULE_AUTHOR("liangqx <liangqingxin-gz@loongson.cn>");
MODULE_DESCRIPTION("Drive the buzzer through the gpio");
MODULE_LICENSE("GPL");

static int major;
module_param(major, int, 0);

static struct gpio_keys_platform_data *pkb = NULL; 


static int buzzer_set_value(struct file *filp, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	char code[2];
	int port,value;

	copy_from_user(code, buffer, (count < 2)?count:2);

	value = code[0] - '0';

	if(value >= pkb->nbuttons) return -ENOMEM;

	port = pkb->buttons[value].gpio;
	value = code[1] - '0';
	printk("prot is %d, value is %d\n",port,value);
	gpio_set_value_cansleep(port, value);

	return count;	
}

static int __devinit buzzer_gpio_probe(struct platform_device *pdev)
{
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	int i;

	pkb = pdata;

	for(i = 0;i < pkb->nbuttons;i++)
	{
		gpio_request(pkb->buttons[i].gpio,"buzzer");
		gpio_direction_output(pkb->buttons[i].gpio, 0);
	}

	return 0;
}

static const struct file_operations buzzer_fops = {
	.owner		= THIS_MODULE,
	.write		= buzzer_set_value,
};

static struct miscdevice buzzer_misc_device = {
	BUZZER_MINOR,
	"buzzer_gpio",
	&buzzer_fops,
};

struct platform_driver buzzer_device_driver = {
	.probe		= buzzer_gpio_probe,
	.driver		= {
		.name 	= "buzzer_gpio",
	}
};

static int __init buzzer_init(void)
{
	if(misc_register(&buzzer_misc_device)){
		printk(KERN_WARNING "buzzer:Couldn't register device 10, %d.\n", BUZZER_MINOR);
		return -EBUSY;
	}

	return platform_driver_register(&buzzer_device_driver);
}

static void __exit buzzer_exit(void)
{
	int i;
	misc_deregister(&buzzer_misc_device);
	for(i = 0;i < pkb->nbuttons;i++){
		gpio_free(pkb->buttons[i].gpio);
	}
}

module_init(buzzer_init);
module_exit(buzzer_exit);