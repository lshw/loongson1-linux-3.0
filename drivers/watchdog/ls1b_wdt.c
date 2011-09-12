#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#define WDT_EN		0x00
#define WDT_SET		0x04
#define WDT_TIMER		0x08
#define TIMEOUT_MIN     1
#define TIMEOUT_MAX     5000
#define TIMEOUT_DEFAULT     TIMEOUT_MAX

static int timeout =  TIMEOUT_DEFAULT;
struct wdt_ls1b {
	void __iomem	*regs;
	int			timeout;
	struct	miscdevice	miscdev;
	
	spinlock_t		io_lock;
};	

static struct wdt_ls1b *wdt;
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

/*看门狗设备驱动的关闭接口函数*/
static int ls1b_wdt_release(struct inode *inode, struct file *file)
{
	/*
	 *	Shut off the timer.
	 *	Lock it in if it's a module and we set nowayout
	 */
	/*如果判断到当前操作状态是可以关闭看门狗定时器时就关闭，否则就是“喂狗”状态*/
	if (expect_close == 42) {
		ls1b_disable();/*关闭*/
	}else{
		printk(KERN_EMERG "Unexpected close, not stopping watchdog\n");
		ls1b_ping();
	}

	expect_close = 0;/*恢复看门狗定时器的当前操作状态为：无状态*/

	return 0;
}

/*看门狗设备驱动的打开接口函数*/
static int ls1b_wdt_open(struct inode *inode, struct file *file)
{
	if (nowayout)
		__module_get(THIS_MODULE);/*如果内核配置了CONFIG_WATCHDOG_NOWAYOUT项，则使模块使用计数加1*/

	expect_close = 0;/*开始记录看门狗定时器的当前操作状态为：无状态*/
	
	ls1b_ping();/*启动看门狗定时器*/
	
	/*表示返回的这个设备文件是不可以被seek操作的，nonseekable_open定义在fs.h中*/
	return nonseekable_open(inode, file);
}

/*看门狗设备驱动的写数据接口函数*/
static ssize_t ls1b_wdt_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	/*
	 *	Refresh the timer.
	 */
	if (count) {/*判断有数据写入*/
		if (!nowayout) {/*如果没有配置内核CONFIG_WATCHDOG_NOWAYOUT选项*/
			size_t i;
			
			/* In case it was set long ago */
			expect_close = 0;/*设看门狗定时器的当前操作状态为：无状态*/
		
			for (i = 0; i != count; i++) {
				char c;
				if (get_user(c, buf + i))
					return -EFAULT;
				if (c == 'V')/*判断写入的数据是"V"时，则设看门狗定时器的当前操作状态为关闭*/
					expect_close = 42;
			}
		}
		/*上面的意思是想要看门狗定时器可以被关闭，则内核不要配置CONFIG_WATCHDOG_NOWAYOUT选项，
         对于下面这里还要“喂狗”一次，我刚开始觉得不需要，因为在看门狗定时器中断里面不断的在
         “喂狗”。后来想想，这里还必须要“喂狗”一次，因为当上面我们判断到写入的数据是"V"时，
         看门狗定时器的当前操作状态马上被设置为关闭，再当驱动去调用看门狗设备驱动的关闭接口函数时，
         看门狗定时器中断被禁止，无法再实现“喂狗”，所以这里要手动“喂狗”一次，否则定时器溢出系统被复位*/
		ls1b_ping();
	}
	return count;
}

static int ls1b_wdt_settimeout(int time) 
{
	if ((time < TIMEOUT_MIN) || (time > TIMEOUT_MAX))
		return -EINVAL;
	
	wdt->timeout = time;

	return 0;
}

/*用于支持看门狗IO控制中获取看门狗信息的命令WDIOC_GETSUPPORT，
  下面的宏和看门狗信息结构体定义在watchdog.h中*/
static struct watchdog_info ls1b_wdt_info = {
	.identity	= "ls1b watchdog",
	.options	= WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
	.firmware_version = 0,
};

/*看门狗设备驱动的IO控制接口函数*/
static long ls1b_wdt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -ENOTTY;
	int time;
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	
	/*以下对看门狗定时器IO控制的命令定义在watchdog.h中*/
	switch (cmd) {
		case WDIOC_GETSUPPORT:/*获取看门狗的支持信息，wdt_ident定义在上面*/
			ret = copy_to_user(argp, &ls1b_wdt_info,
				sizeof(ls1b_wdt_info)) ? -EFAULT : 0;
			break;
		case WDIOC_GETSTATUS:/*获取看门够状态*/
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
		case WDIOC_KEEPALIVE:/*喂狗命令*/
			ls1b_ping();
			ret = 0;
			break;
		case WDIOC_SETTIMEOUT:/*设置定时器溢出时间值命令(时间单位为秒)*/
			ret = get_user(time, p);/*获取时间值*/
			if(ret)
				break;
			ret = ls1b_wdt_settimeout(time);/*设置到计数寄存器WTCNT中*/
			if(ret)
				break;
			ls1b_ping();/*喂狗*/
			break;
		case WDIOC_GETTIMEOUT:/*读取定时器默认溢出时间值命令(时间单位为秒)*/
			ret = put_user(wdt->timeout, p);
			break;
		default:
			return -ENOTTY;
	}
	
	return ret;
}

/*字符设备的相关操作实现*/
static const struct file_operations ls1b_wdt_fops = {
	.owner				= THIS_MODULE,
	.llseek			= no_llseek,
	.unlocked_ioctl	= ls1b_wdt_ioctl,
	.open				= ls1b_wdt_open,
	.release			= ls1b_wdt_release,
	.write				= ls1b_wdt_write,
};

static int __init ls1b_wdt_probe(struct platform_device *pdev)
{
	struct resource *regs;/*定义一个资源，用来保存获取的watchdog的IO资源*/
	int ret;

	/*获取watchdog平台设备所使用的IO端口资源，注意这个IORESOURCE_MEM标志和watchdog平台设备定义中的一致*/
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
	
	/*申请watchdog的IO端口资源所占用的IO空间(要注意理解IO空间和内存空间的区别),
	request_mem_region定义在ioport.h中*/
	if (request_mem_region(regs->start, regs->end - regs->start + 1, pdev->name) == NULL)
	{
		/*错误处理*/
		dev_err(&pdev->dev, "failed to reserve memory region\n");
		ret = -ENOENT;
		goto err_free;
	}
	
	/*将watchdog的IO端口占用的这段IO空间映射到内存的虚拟地址，ioremap定义在io.h中。
     注意：IO空间要映射后才能使用，以后对虚拟地址的操作就是对IO空间的操作，*/
	wdt->regs = ioremap(regs->start, regs->end - regs->start + 1);
	if (!wdt->regs) {
		ret = -ENOMEM;
		dev_dbg(&pdev->dev, "could not map i/o memory");
		goto err_free;
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
	
	/*把看门狗设备又注册成为misc设备，misc_register定义在miscdevice.h中*/
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

/*Watchdog平台驱动的设备关闭接口函数的实现*/
static void ls1b_wdt_shutdown(struct platform_device *pdev)
{
	/*停止看门狗定时器*/
	ls1b_wdt_stop();
}

/*对Watchdog平台设备驱动电源管理的支持。CONFIG_PM这个宏定义在内核中，
  当配置内核时选上电源管理，则Watchdog平台驱动的设备挂起和恢复功能均有效，
 */
#ifdef CONFIG_PM

/*Watchdog平台驱动的设备挂起接口函数的实现*/
static int ls1b_wdt_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

/*Watchdog平台驱动的设备恢复接口函数的实现*/
static int ls1b_wdt_resume(struct platform_device *dev)
{
	return 0;
}

#else /*配置内核时没选上电源管理,Watchdog平台驱动的设备挂起和恢复功能均无效,这两个函数也就无需实现了*/
#define ls1b_wdt_suspend NULL
#define ls1b_wdt_resume  NULL
#endif /* CONFIG_PM */

/*Watchdog平台驱动结构体，平台驱动结构体定义在platform_device.h中*/
static struct platform_driver ls1b_wdt_driver = {
	.probe			= ls1b_wdt_probe,
	.remove		= __devexit_p(ls1b_wdt_remove),	/*Watchdog移除函数*/
	.shutdown		= ls1b_wdt_shutdown,				/*Watchdog关闭函数*/
	.suspend		= ls1b_wdt_suspend,				/*Watchdog挂起函数*/
	.resume		= ls1b_wdt_resume,					/*Watchdog恢复函数*/
	.driver		= {
		/*注意这里的名称一定要和系统中定义平台设备的地方一致，这样才能把平台设备与该平台设备的驱动关联起来*/
		.name		= "ls1b-wdt",
		.owner		= THIS_MODULE,
	},
};

/*将Watchdog注册成平台设备驱动*/
static int __init ls1b_wdt_init(void)
{
	/*将Watchdog注册成平台设备驱动*/
	return platform_driver_register(&ls1b_wdt_driver);
}

static void __exit ls1b_wdt_exit(void)
{
	/*注销Watchdog平台设备驱动*/
	platform_driver_unregister(&ls1b_wdt_driver);
}

module_init(ls1b_wdt_init);
module_exit(ls1b_wdt_exit);

MODULE_AUTHOR("tanghaifeng <tanghaifeng-gz@loongson.cn>");
MODULE_DESCRIPTION("watchdog driver for loongson 1b");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
