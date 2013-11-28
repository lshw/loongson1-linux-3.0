/*
 *  Copyright (c) 2013 www.loongson.cn fantianbao
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/signal.h>
#include <linux/sched.h>

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/gpio.h>

#include <loongson1.h>
#include <irq.h>

#include "wavpcm.h"

#define REG_PWM_CNTR	0x00
#define REG_PWM_HRC		0x04
#define REG_PWM_LRC		0x08
#define REG_PWM_CTRL	0x0c

#define LS1X_PWM_AUDIO_DEV 250

static void __iomem *ls1x_pwm_base;
static struct clk *ls1x_pwm_clk;
static int processed = 0;
DECLARE_WAIT_QUEUE_HEAD(ls1x_wate_queue);

struct ls1x_pwm_audio_device {
	unsigned long dat_offset;
	unsigned char *pBuf;
	unsigned long buffsize;
	unsigned short samplerate;
	unsigned int wChannels;
}audio_device;

static irqreturn_t ls1x_pwm_audio_irq_handler(int i, void *data)
{
	unsigned int tmp;

	if (audio_device.dat_offset >= audio_device.buffsize) {
		wake_up_interruptible(&ls1x_wate_queue);
		processed += 1;
		return IRQ_HANDLED;
	}

	tmp = (*(unsigned short *)&audio_device.pBuf[audio_device.dat_offset]) * audio_device.samplerate / 0xffff;
	writel(tmp, ls1x_pwm_base + REG_PWM_HRC);
	writel(0x421, ls1x_pwm_base + REG_PWM_CTRL);

	if (audio_device.wChannels == 1)
		audio_device.dat_offset += 2;	
	else
		audio_device.dat_offset += 4;

	return IRQ_HANDLED;
}

static int ls1x_pwm_audio_open(struct inode *inode, struct file *filep)
{
	memset((unsigned char *)&audio_device, 0, sizeof(struct ls1x_pwm_audio_device));
	return 0;
}

static ssize_t ls1x_pwm_audio_write(struct file *file, const char __user *buf, size_t count, loff_t *ptr)
{
	S_WAV_FMT * pWav_Fmt;
	unsigned long period;

	processed = 0;

	audio_device.dat_offset = sizeof(S_WAV_FMT);
	pWav_Fmt = (S_WAV_FMT *)buf;

	audio_device.buffsize = pWav_Fmt->v_head.dwSize + 4;
	audio_device.samplerate = pWav_Fmt->v_fmt.dwSamplesPerSec;
	audio_device.wChannels = pWav_Fmt->v_fmt.wChannels;

	audio_device.pBuf =  kmalloc(audio_device.buffsize,  GFP_KERNEL);
	if (audio_device.pBuf == NULL)
		return -EFAULT;
	if (copy_from_user((void *)(audio_device.pBuf), buf, count))
		return -EFAULT;

	period = (unsigned long)clk_get_rate(ls1x_pwm_clk) / audio_device.samplerate;
	audio_device.samplerate = period;
	writel(period, ls1x_pwm_base + REG_PWM_LRC);
	writel(0x000, ls1x_pwm_base + REG_PWM_CNTR);
	writel(0x421, ls1x_pwm_base + REG_PWM_CTRL);

	if (!processed && (file->f_flags & O_NONBLOCK))
		return -EAGAIN;

	if (wait_event_interruptible(ls1x_wate_queue, processed)) {
		writel(0x000, ls1x_pwm_base + REG_PWM_CTRL);
		return -EAGAIN;
	}
	writel(0x000, ls1x_pwm_base + REG_PWM_CTRL);

	return count;
}

static int ls1x_pwm_audio_close(struct inode *inode, struct file *filp)
{
	writel(0x00, ls1x_pwm_base + REG_PWM_CTRL);
	kfree(audio_device.pBuf);
	return 0;
}

static const struct file_operations ls1x_pwm_audio_ops = {
	.owner = THIS_MODULE,
	.open = ls1x_pwm_audio_open,
	.release = ls1x_pwm_audio_close,
	.write = ls1x_pwm_audio_write,
};

static struct miscdevice ls1x_pwm_audio_miscdev = {
	LS1X_PWM_AUDIO_DEV,
	"ls1x_pwm_audio",
	&ls1x_pwm_audio_ops,
};

static int __devinit ls1x_pwm_audio_probe(struct platform_device *pdev)
{
	int ret;

	ls1x_pwm_base = ioremap(LS1X_PWM1_BASE, 0xf);
	if (!ls1x_pwm_base) {
		printk(KERN_ERR "Failed to ioremap audio registers");
		return -1;
	}

	ls1x_pwm_clk = clk_get(NULL, "apb");

	if (IS_ERR(ls1x_pwm_clk)) {
		ret = PTR_ERR(ls1x_pwm_clk);
		ls1x_pwm_clk = NULL;
		return -1;
	}

	writel(0x00, ls1x_pwm_base + REG_PWM_CNTR);
	writel(0x00, ls1x_pwm_base + REG_PWM_CTRL);

	ret = request_irq(LS1X_PWM1_IRQ, ls1x_pwm_audio_irq_handler, IRQF_TRIGGER_RISING | IRQF_DISABLED, "ls1x_pwm_audio", NULL);
	if (ret) {
		printk("ls1x_pwm_audio:irq handler resigered error:%d\n", ret);
		return ret;
	}

	return 0;
}

/*
static struct platform_driver ls1x_pwm_audio_driver = {
	.probe = ls1x_pwm_audio_probe,
	.driver = {
		.name = "ls1x_pwm_audio",
	},
};
*/

static int __init ls1x_pwm_audio_init(void)
{
	if (misc_register(&ls1x_pwm_audio_miscdev)) {
		printk(KERN_WARNING "ls1x_pwm_audio: Couldn't register device!\n ");
		return -EBUSY;
	}

//	return platform_driver_register(&ls1x_pwm_audio_driver);
	return ls1x_pwm_audio_probe(NULL);
}

static void __exit ls1x_pwm_audio_exit(void)
{
	free_irq(LS1X_PWM1_IRQ, NULL);
	misc_deregister(&ls1x_pwm_audio_miscdev);
	iounmap(ls1x_pwm_base);
//	platform_driver_unregister(&ls1x_pwm_audio_driver);	
}
module_init(ls1x_pwm_audio_init);
module_exit(ls1x_pwm_audio_exit);

MODULE_AUTHOR("www.loongson.cn fantianbao");
MODULE_DESCRIPTION("PWM audio for loongson1");
MODULE_LICENSE("GPL");

//__initcall(ls1x_pwm_audio_init);
