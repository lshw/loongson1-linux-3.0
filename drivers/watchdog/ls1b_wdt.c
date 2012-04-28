#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>

#include <asm/mach-loongson/ls1x/ls1b_board.h>

#define TIMEOUT_MIN		0
#define TIMEOUT_MAX		0xFFFFFFFF
#define TIMEOUT_DEFAULT	15	//second

static unsigned int timeout =  TIMEOUT_DEFAULT;
struct wdt_ls1b {
	void __iomem		*regs;
	unsigned int		timeout;
	struct	miscdevice	miscdev;
	
	spinlock_t		io_lock;
};	

static struct wdt_ls1b	*wdt;
static struct clk		*wdt_clock;
static char	expect_close;
static int nowayout = WATCHDOG_NOWAYOUT;

static void wdt_writel(struct wdt_ls1b *wdt, unsigned reg, int data)
{
	(*(volatile unsigned int *)(wdt->regs + reg)) = data;
}

static inline void ls1b_wdt_stop(void)
{
	spin_lock(&wdt->io_lock);
	wdt_writel(wdt, WDT_EN, 0x0);
	spin_unlock(&wdt->io_lock);
}

static inline void ls1b_disable(void)
{
	spin_lock(&wdt->io_lock);
	wdt_writel(wdt, WDT_EN, 0x0);
	spin_unlock(&wdt->io_lock);
}

static inline void ls1b_ping(void)
{
	unsigned long psel = wdt->timeout;
	spin_lock(&wdt->io_lock);
	wdt_writel(wdt, WDT_EN, 0x1); 
	wdt_writel(wdt, WDT_TIMER, psel);
	wdt_writel(wdt, WDT_SET, 0x1);
	spin_unlock(&wdt->io_lock);
}

static int ls1b_wdt_release(struct inode *inode, struct file *file)
{
	/*
	 *	Shut off the timer.
	 *	Lock it in if it's a module and we set nowayout
	 */
	if (expect_close == 42) {
		ls1b_disable();
	}else{
		printk(KERN_EMERG "Unexpected close, not stopping watchdog\n");
		ls1b_ping();
	}

	expect_close = 0;

	return 0;
}

static int ls1b_wdt_open(struct inode *inode, struct file *file)
{
	if (nowayout)
		__module_get(THIS_MODULE);

	expect_close = 0;
	
	ls1b_ping();

	return nonseekable_open(inode, file);
}

static ssize_t ls1b_wdt_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	/*
	 *	Refresh the timer.
	 */
	if (count) {
		if (!nowayout) {
			size_t i;
			
			/* In case it was set long ago */
			expect_close = 0;
			for (i = 0; i != count; i++) {
				char c;
				if (get_user(c, buf + i))
					return -EFAULT;
				if (c == 'V')
					expect_close = 42;
			}
		}
		ls1b_ping();
	}
	return count;
}

static int ls1b_wdt_settimeout(int time) 
{
	unsigned long freq = clk_get_rate(wdt_clock);
	if ((time < TIMEOUT_MIN) || (time > TIMEOUT_MAX))
		return -EINVAL;
	
	wdt->timeout = time * freq / 2;

	return 0;
}

static struct watchdog_info ls1b_wdt_info = {
	.identity	= "ls1b watchdog",
	.options	= WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
	.firmware_version = 0,
};

static long ls1b_wdt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -ENOTTY;
	int time;
	void __user *argp = (void __user *)arg;
	int __user *p = argp;

	switch (cmd) {
		case WDIOC_GETSUPPORT:
			ret = copy_to_user(argp, &ls1b_wdt_info,
				sizeof(ls1b_wdt_info)) ? -EFAULT : 0;
			break;
		case WDIOC_GETSTATUS:
		case WDIOC_GETBOOTSTATUS:
			ret = put_user(0, p);
			break;
		case WDIOC_SETOPTIONS:
			ret = get_user(time, p);
			if (ret)
				break;
			if (time & WDIOS_DISABLECARD)
				ls1b_wdt_stop();
			if (time & WDIOS_ENABLECARD)
				ls1b_ping();
			ret = 0;
			break;
		case WDIOC_KEEPALIVE:
			ls1b_ping();
			ret = 0;
			break;
		case WDIOC_SETTIMEOUT:
			ret = get_user(time, p);
			if(ret)
				break;
			ret = ls1b_wdt_settimeout(time);
			if(ret)
				break;
			ls1b_ping();
			break;
		case WDIOC_GETTIMEOUT:
			ret = put_user(wdt->timeout, p);
			break;
		default:
			return -ENOTTY;
	}
	
	return ret;
}

static const struct file_operations ls1b_wdt_fops = {
	.owner			= THIS_MODULE,
	.llseek			= no_llseek,
	.unlocked_ioctl	= ls1b_wdt_ioctl,
	.open			= ls1b_wdt_open,
	.release		= ls1b_wdt_release,
	.write			= ls1b_wdt_write,
};

static int __init ls1b_wdt_probe(struct platform_device *pdev)
{
	struct resource *regs;
	int ret;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_dbg(&pdev->dev, "missing mmio resource\n");
		return -ENXIO;
	}
	
	wdt = kzalloc(sizeof(struct wdt_ls1b), GFP_KERNEL);
	if (!wdt) {
		dev_dbg(&pdev->dev, "no memory for wdt structure");
		return -ENOMEM;
	}

	if (request_mem_region(regs->start, regs->end - regs->start + 1, pdev->name) == NULL) {
		dev_err(&pdev->dev, "failed to reserve memory region\n");
		ret = -ENOENT;
		goto err_free;
	}

	wdt->regs = ioremap(regs->start, regs->end - regs->start + 1);
	if (!wdt->regs) {
		ret = -ENOMEM;
		dev_dbg(&pdev->dev, "could not map i/o memory");
		goto err_free;
	}

	wdt_clock = clk_get(&pdev->dev, "ddr");
	if (IS_ERR(wdt_clock)) {
		dev_err(&pdev->dev, "failed to find watchdog clock source\n");
		ret = PTR_ERR(wdt_clock);
		goto err_iounmap;
	}

	spin_lock_init(&wdt->io_lock);
	wdt->miscdev.minor = WATCHDOG_MINOR;
	wdt->miscdev.name = "watchdog";
	wdt->miscdev.fops = &ls1b_wdt_fops;
	wdt->miscdev.parent = &pdev->dev;

	if (ls1b_wdt_settimeout(timeout)) {
		ls1b_wdt_settimeout(TIMEOUT_DEFAULT);
		dev_dbg(&pdev->dev, 
			"default timeout invalid set to %d sec.\n",
		TIMEOUT_DEFAULT);
	}

	ret = misc_register(&wdt->miscdev);
	if (ret) {
		dev_dbg(&pdev->dev, "failed to register wdt miscdev\n");
		goto err_iounmap;
	}
	
	platform_set_drvdata(pdev, wdt);
	
	dev_info(&pdev->dev,
		"ls1b WDT at 0x%p, timeout %d sec (no wayout= %d)\n", wdt->regs, wdt->timeout, nowayout);

	return 0;

err_iounmap:
	iounmap(wdt->regs);
err_free:
	kfree(wdt);
	wdt = NULL;
	return ret;
}

static int __devexit ls1b_wdt_remove(struct platform_device *pdev)
{
	if (wdt && platform_get_drvdata(pdev) == wdt) {
		if(!nowayout)
			ls1b_wdt_stop();

		misc_deregister(&wdt->miscdev);
		iounmap(wdt->regs);
		kfree(wdt);
		wdt = NULL;
		platform_set_drvdata(pdev, NULL);
	}
	
	return 0;
}

static void ls1b_wdt_shutdown(struct platform_device *pdev)
{
	ls1b_wdt_stop();
}

#ifdef CONFIG_PM

static int ls1b_wdt_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int ls1b_wdt_resume(struct platform_device *dev)
{
	return 0;
}

#else
#define ls1b_wdt_suspend NULL
#define ls1b_wdt_resume  NULL
#endif /* CONFIG_PM */

static struct platform_driver ls1b_wdt_driver = {
	.probe		= ls1b_wdt_probe,
	.remove		= __devexit_p(ls1b_wdt_remove),
	.shutdown	= ls1b_wdt_shutdown,
	.suspend	= ls1b_wdt_suspend,
	.resume		= ls1b_wdt_resume,
	.driver		= {
		.name	= "ls1b-wdt",
		.owner	= THIS_MODULE,
	},
};

static int __init ls1b_wdt_init(void)
{
	return platform_driver_register(&ls1b_wdt_driver);
}

static void __exit ls1b_wdt_exit(void)
{
	platform_driver_unregister(&ls1b_wdt_driver);
}

module_init(ls1b_wdt_init);
module_exit(ls1b_wdt_exit);

MODULE_AUTHOR("tanghaifeng <tanghaifeng-gz@loongson.cn>");
MODULE_DESCRIPTION("watchdog driver for loongson 1b");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
