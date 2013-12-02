/*
 * linux/drivers/misc/pwm_rfid.c
 *
 * simple PWM based rfid control, board code has to setup
 * 1) pin configuration so PWM waveforms can output
 * 2) platform_data being correctly configured
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/err.h>
#include <linux/miscdevice.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/gpio.h>

#include <linux/pwm_rfid.h>

#define LS1X_PWM_RFID_DEV 251

//p=126, 1/2p=53; PWM intrrupt T=1/(2*123k)  p=512us/T=126
#define 	RFID_TIME0		3   //1180 (128M)   //1000 (108M)   //750 (80M)   
#define 	RFID_TIME5		7   //2370          //2000		   //1500
#define 	RFID_TIME10		13   //4740          //4000		   //3000
#define 	RFID_TIME_WAIT	18  //5330          //4500		   //3300

#define BUF_LEN 12
static unsigned char rbuf[BUF_LEN];

struct pwm_rfid_data {
	struct pwm_device	*pwm;
	struct device		*dev;
	unsigned int		period;
	unsigned gpio;
};

struct pwm_rfid_data *pr;

static int read_bit(void)
{
	struct timeval rctime;
	unsigned int tmp = 0;
	int count, try = 1;
	int bit_val;

	bit_val = gpio_get_value(pr->gpio);
r_try:

	do_gettimeofday(&rctime);	

	for (count = 0; count < RFID_TIME_WAIT;) {
		if (bit_val != gpio_get_value(pr->gpio))
			break;  

		do_gettimeofday(&rctime); 
		if (rctime.tv_usec - tmp >= 50) {
			tmp = rctime.tv_usec;
			count++;
		}
	}
	if (count == RFID_TIME_WAIT)
		return -1; // Error

	bit_val = gpio_get_value(pr->gpio);

	if ((count > RFID_TIME5) && (count <= RFID_TIME10)) // One Period
		return ((~bit_val) & 0x01);
	if ((count >= RFID_TIME0) && (count <= RFID_TIME5)) { // Half Period
		if (try) {
			try--;
			goto r_try;
		}
		return ((~bit_val) & 0x01);
	}
	return -1; // Error
}

static int check_data(unsigned char *data)
{
	int i, j;
	int ret;

	/* 行校验 */
	for (i = 0; i < 10; i++) {
		ret = (data[i] >> 4) ^ (data[i] >> 3) ^ (data[i] >> 2) \
			^ (data[i] >> 1) ^ data[i];
		if (ret & 0x08)
			return -1;
	}

	/* 列校验 */
	for (i = 0x80; i >= 0x10; i >>= 1) {
		ret = 0;
		for (j = 0; j < 11; j += 1) {
			ret = ret ^ (data[j] & i);
		}
		if (ret) {
			return -1;
		}
	}

	return 0;
}

/* 适用于EM4100 RFID卡 */
static int read_data(unsigned char *data)
{	
	int i, j;
	int ret;

	/* 判断数据头 */
	for (i = 0; i < 9; i++) {
		if (read_bit() != 1) {
			return -1;
		}
	}

	/* 读取11行 读取到的数据移到字节的高位 */
	for (i = 0; i<11; i++) {
		data[i] = 0x00;
		/* 读取5bit */
		for (j = 4; j >= 0; j--) {
			ret = read_bit();
			if (ret) {
				data[i] |= (0x08 << j);	/* bit = 1 */
			} else if (ret < 0) {
				return -1;	/* err */
			}
		}
	}

	/* 判断结束标记 */
	if (data[10] & 0x08)
		return -1;

	if (check_data(data))
		return -1;

	return 0;
}

static int decode(void)
{
	int i;

	for (i = 0; i < 32; i += 1) {
		if (!read_data(rbuf))
			break;
	}
	
	if (i == 32) {
		return -1;
	}

	rbuf[0] = (rbuf[0] & 0xF0) | (rbuf[1]>>4 & 0x0F);
	rbuf[1] = (rbuf[2] & 0xF0) | (rbuf[3]>>4 & 0x0F);
	rbuf[2] = (rbuf[4] & 0xF0) | (rbuf[5]>>4 & 0x0F);
	rbuf[3] = (rbuf[6] & 0xF0) | (rbuf[7]>>4 & 0x0F);
	rbuf[4] = (rbuf[8] & 0xF0) | (rbuf[9]>>4 & 0x0F);

	return 0;
}


static int pwm_rfid_open(struct inode *inode, struct file *filep)
{
	pwm_config(pr->pwm, pr->period/2, pr->period);
	pwm_enable(pr->pwm);
	return 0;
}

static int pwm_rfid_close(struct inode *inode, struct file *filp)
{
	pwm_config(pr->pwm, 0, pr->period);
	pwm_disable(pr->pwm);
	return 0;
}

static ssize_t pwm_rfid_read(struct file *filp, char __user *buf, size_t count, loff_t *offp)
{
	int len = count;

	memset(rbuf, 0, BUF_LEN);

	if (len > BUF_LEN) {
		len = BUF_LEN;
	}

	if(!decode()) {
	    if(copy_to_user(buf, rbuf, len))
			return -EFAULT;
	    return len;
	}
	
	return -EFAULT;
}

static const struct file_operations pwm_rfid_ops = {
	.owner = THIS_MODULE,
	.open = pwm_rfid_open,
	.release = pwm_rfid_close,
	.read = pwm_rfid_read,
//	.write = pwm_rfid_write,
};

static struct miscdevice pwm_rfid_miscdev = {
	LS1X_PWM_RFID_DEV,
	"pwm_rfid",
	&pwm_rfid_ops,
};

static int pwm_rfid_probe(struct platform_device *pdev)
{
	struct platform_pwm_rfid_data *data = pdev->dev.platform_data;
	int ret;

	if (!data) {
		dev_err(&pdev->dev, "failed to find platform data\n");
		return -EINVAL;
	}

	if (data->init) {
		ret = data->init(&pdev->dev);
		if (ret < 0)
			return ret;
	}

	pr = kzalloc(sizeof(*pr), GFP_KERNEL);
	if (!pr) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	pr->period = data->pwm_period_ns;

	pr->pwm = pwm_request(data->pwm_id, "rfid");
	if (IS_ERR(pr->pwm)) {
		dev_err(&pdev->dev, "unable to request PWM for rfid\n");
		ret = PTR_ERR(pr->pwm);
		goto err_pwm;
	} else
		dev_dbg(&pdev->dev, "got pwm for rfid\n");

	pwm_config(pr->pwm, 0, pr->period);
	pwm_disable(pr->pwm);

	pr->gpio = data->gpio;
	ret = gpio_request(pr->gpio, "pwm_rfid");
	if (ret < 0)
		goto err_rfid;
	gpio_direction_input(pr->gpio);

	platform_set_drvdata(pdev, pr);

	return 0;

err_rfid:
	pwm_free(pr->pwm);
err_pwm:
	kfree(pr);
err_alloc:
	if (data->exit)
		data->exit(&pdev->dev);
	return ret;
}

static int pwm_rfid_remove(struct platform_device *pdev)
{
	struct platform_pwm_rfid_data *data = pdev->dev.platform_data;
	struct pwm_rfid_data *pr = platform_get_drvdata(pdev);;

	gpio_free(pr->gpio);
	pwm_config(pr->pwm, 0, pr->period);
	pwm_disable(pr->pwm);
	pwm_free(pr->pwm);
	kfree(pr);
	if (data->exit)
		data->exit(&pdev->dev);
	return 0;
}

#ifdef CONFIG_PM
static int pwm_rfid_suspend(struct platform_device *pdev,
				 pm_message_t state)
{
	struct pwm_rfid_data *pr = platform_get_drvdata(pdev);

	pwm_config(pr->pwm, 0, pr->period);
	pwm_disable(pr->pwm);
	return 0;
}

static int pwm_rfid_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define pwm_rfid_suspend	NULL
#define pwm_rfid_resume	NULL
#endif

static struct platform_driver pwm_rfid_driver = {
	.driver = {
		.name = "pwm-rfid",
		.owner	= THIS_MODULE,
	},
	.probe		= pwm_rfid_probe,
	.remove		= pwm_rfid_remove,
	.suspend	= pwm_rfid_suspend,
	.resume		= pwm_rfid_resume,
};

static int __init pwm_rfid_init(void)
{
	int ret;

	ret = platform_driver_register(&pwm_rfid_driver);

	if (misc_register(&pwm_rfid_miscdev)) {
		printk(KERN_WARNING "pwm_rfid: Couldn't register device!\n ");
		return -EBUSY;
	}

	return ret;
}

static void __exit pwm_rfid_exit(void)
{
	misc_deregister(&pwm_rfid_miscdev);
	platform_driver_unregister(&pwm_rfid_driver);	
}
module_init(pwm_rfid_init);
module_exit(pwm_rfid_exit);

MODULE_AUTHOR("www.loongson.cn fantianbao");
MODULE_DESCRIPTION("PWM audio for loongson1");
MODULE_LICENSE("GPL");
