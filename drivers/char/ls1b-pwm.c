#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/ioctl.h>
#include <linux/platform_device.h>
#include <linux/timer.h>

#define CMD_PWM0_ON	_IO('c',0x01)
#define CMD_PWM1_ON _IO('c',0x02)
#define CMD_PWM2_ON _IO('c',0x03)
#define CMD_PWM3_ON _IO('c',0x04)

#define REG_PWM_CNTR	0x00
#define REG_PWM_HRC	0x04
#define REG_PWM_LRC	0x08
#define REG_PWM_CTRL	0x0c
#define REG_GPIO_CFG0	0xbfd010c0

static unsigned char __iomem *pwm_base = NULL;
struct resource *res;
static int ls1b_pwm_probe(struct platform_device *pdev);
struct platform_device *pwm_dev;
struct resource *res1 = NULL;


static struct platform_driver ls1b_pwm_driver = {
		.probe = ls1b_pwm_probe,
		.driver = {
				.name = "ls1b-pwm",
		},
};

static int ls1b_pwm_open(struct inode *inode, struct file *file) {
	unsigned int val = readl(REG_GPIO_CFG0);

	//配置GPIO复用为使用PWM工作的模式
	val &= 0xfffffff0;
	writel(val, REG_GPIO_CFG0);
	return 0;
}

static int ls1b_pwm_close(struct inode *inode, struct file *file)
{
	writeb(0x0, pwm_base + REG_PWM_CTRL);
	return 0;
}

static ssize_t ls1b_pwm_read(struct file *file, char __user *buf, size_t count, loff_t *ptr)
{
	unsigned int pwm_val;
	pwm_val = readl(pwm_base);

	if (copy_to_user(buf, &pwm_val, sizeof(unsigned int)))
		return -EFAULT;
	return 4;
}

static ssize_t ls1b_pwm_write(struct file *file, const char __user *buf, size_t count, loff_t *ptr)
{
	unsigned int hrc_val, lrc_val, val;
	unsigned int data[2] = {0x0};

	if (copy_from_user(data, buf, sizeof(data)))
	{
		printk("Write error!\n");
		return -EIO;
	}

	hrc_val = data[1] - 1;
	lrc_val = data[0] + data[1] -1;

	//设置占空比
	writel(hrc_val, pwm_base + REG_PWM_HRC);
	writel(lrc_val, pwm_base + REG_PWM_LRC);
	writel(0x0, pwm_base + REG_PWM_CNTR);
	writeb(0x01, pwm_base + REG_PWM_CTRL);
	return 0;
}


static int ls1b_pwm_getResourse(struct platform_device *pdev, unsigned int index)
{
	int ret = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, index);

	if (res == NULL)
	{
	  printk("Fail to get ls1b_pwm_resource!\n");
	  return -ENOENT;
	}
	printk("Resource start=0x%x, end = 0x%x\n", res->start, res->end);
	if (res1 != NULL)
	{
		release_mem_region(res->start, 0x0f);
	}
	res1 = request_mem_region(res->start, 0x0f, "ls1b-pwm");
	if (res1 == NULL)
	{
	  printk("Fail to request ls1b_pwm region!\n");
	  return -ENOENT;
	}
	pwm_base = ioremap(res->start, res->end - res->start + 1);
	if (pwm_base == NULL)
	{
	  printk("Fail to ioremap ls1b_pwm resource!\n");
	  return -EINVAL;
	}
	return 0;
}


static int ls1b_pwm_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case CMD_PWM0_ON:
		ls1b_pwm_getResourse(pwm_dev, 0);
		break;
	case CMD_PWM1_ON:
		ls1b_pwm_getResourse(pwm_dev, 1);
		break;
	case CMD_PWM2_ON:
		ls1b_pwm_getResourse(pwm_dev, 2);
		break;
	case CMD_PWM3_ON:
		ls1b_pwm_getResourse(pwm_dev, 3);
		break;
	default:
		ls1b_pwm_getResourse(pwm_dev, 0);
		break;
	}
	return 0;
}

static const struct file_operations ls1b_pwm_ops = {
		.owner = THIS_MODULE,
		.open = ls1b_pwm_open,
		.release = ls1b_pwm_close,
		.read = ls1b_pwm_read,
		.write = ls1b_pwm_write,
//		.ioctl = ls1b_pwm_ioctl,
		.unlocked_ioctl	= ls1b_pwm_ioctl,
};

static struct miscdevice ls1b_pwm_miscdev = {
		MISC_DYNAMIC_MINOR,
		"ls1b-pwm",
		&ls1b_pwm_ops,
};



static int __devinit ls1b_pwm_probe(struct platform_device *pdev)
{
	pwm_dev = pdev;
	return ls1b_pwm_getResourse(pdev, 1);
}

static int __init ls1b_pwm_init(void) {
	if (misc_register(&ls1b_pwm_miscdev))
	{
	  printk(KERN_WARNING "pwm: Couldn't register device 10, %d.\n", 255);
	  return -EBUSY;
	}
	return platform_driver_register(&ls1b_pwm_driver);
}

static int __exit ls1b_pwm_exit(void)
{
	misc_deregister(&ls1b_pwm_miscdev);
	release_mem_region(res->start, 0x20);
	platform_driver_unregister(&ls1b_pwm_driver);
	return 0;
}

module_init(ls1b_pwm_init);
module_exit(ls1b_pwm_exit);
MODULE_LICENSE("GPL");
