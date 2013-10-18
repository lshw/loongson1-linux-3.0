/*
 *  Driver for buttons on ADC lines not capable of generating interrupts
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input/adc_keys_polled.h>
#include <linux/delay.h>

#include <loongson1.h>

#define DRV_NAME	"adc-keys-polled"

/* ADC register */
#define ADC_CNT			0x00
#define ADC_S_CTRL		0x04
#define ADC_C_CTRL		0x08
#define X_RANGE			0x10
#define Y_RANGE			0x14
#define AWATCHDOG_RANGE	0x18
#define AXIS			0x1c
#define ADC_S_DOUT0		0x20
#define ADC_S_DOUT1		0x24
#define ADC_C_DOUT		0x28
#define ADC_DEBOUNCE_CNT	0x2c
#define ADC_INT			0x30


#define ADC_MAX_NUM	9
#define ADC_MAX_VAL	1023
#define ADC_MIN_VAL	0
//#define ADC_STEP	(ADC_MAX_VAL / ADC_MAX_NUM)
#define ADC_STEP	114

#define KEY_NULL	0x0

void __iomem *adc_base;

const unsigned int key_num[32] = 
{
	0x00000001, 0x00000002, 0x00000004, 0x00000008, 
	0x00000010, 0x00000020, 0x00000040, 0x00000080, 
	0x00000100, 0x00000200, 0x00000400, 0x00000800,
	0x00001000, 0x00002000, 0x00004000, 0x00008000,
	0x00010000, 0x00020000, 0x00040000, 0x00080000,
	0x00100000, 0x00200000, 0x00400000, 0x00800000,
	0x01000000, 0x02000000, 0x04000000, 0x08000000,
	0x10000000, 0x20000000, 0x40000000, 0x80000000,
};

const int adc_val_table0[ADC_MAX_NUM] =
{
//	0,  100, 205, 310, 420, 525, 625, 725, 925,
//	0, 122, 244, 360, 470, 571, 680, 780, 950,
	16, 113, 220, 324, 430, 535, 640, 740, 930,
};

const int adc_val_table1[ADC_MAX_NUM] =
{
	20,  150, 272, 382, 488, 586, 686, 780, 948,
};

const int adc_val_table2[ADC_MAX_NUM] =
{
	20,  150, 268, 378, 484, 583, 683, 780, 948,
};

static int ls1c_adc_init(void)
{
	adc_base = ioremap(LS1X_ADC_BASE, 0x4000);

//	writel(0x82, adc_base + ADC_C_CTRL);

//	writel(0x60, adc_base + ADC_S_CTRL);
//	writel(0x00, adc_base + ADC_C_CTRL);
//	writel(0x02, adc_base + ADC_S_CTRL);
	return 0;
}

static int adc_read(void)
{
	unsigned int i;
	int temp0 = 0, temp1 = 0, temp2 = 0;

	writel(0x81, adc_base + ADC_C_CTRL);
	for (i = 0; i < 10000; i++) {
		temp0 += readl(adc_base + ADC_C_DOUT) & 0x3ff;
	}
	writel(0x01, adc_base + ADC_C_CTRL);
	writel(0x20, adc_base + ADC_S_CTRL);
	while (readl(adc_base + ADC_S_CTRL) & 0x20) {
	}
	temp0 = temp0 / 10000;

	writel(0x82, adc_base + ADC_C_CTRL);
	for (i = 0; i < 10000; i++) {
		temp1 += readl(adc_base + ADC_C_DOUT) & 0x3ff;
	}
	writel(0x02, adc_base + ADC_C_CTRL);
	writel(0x20, adc_base + ADC_S_CTRL);
	while (readl(adc_base + ADC_S_CTRL) & 0x20) {
	}
	temp1 = temp1 / 10000;

	writel(0x88, adc_base + ADC_C_CTRL);
	for (i = 0; i < 10000; i++) {
		temp2 += readl(adc_base + ADC_C_DOUT) & 0x3ff;
	}
	writel(0x08, adc_base + ADC_C_CTRL);
	writel(0x20, adc_base + ADC_S_CTRL);
	while (readl(adc_base + ADC_S_CTRL) & 0x20) {
	}
	temp2 = temp2 / 10000;

	if ((temp0 > (1023 - 56)) && (temp1 > (1023 - 56)) && (temp2 > (1023 - 56))){
		return KEY_NULL;
	}

	for (i = 0; i < ADC_MAX_NUM; i++) {
		if((temp0 > (adc_val_table0[i] - 25)) && (temp0 < (adc_val_table0[i] + 25)))
			return key_num[i];
	}
	for (i = 0; i < ADC_MAX_NUM; i++) {
		if((temp1 > (adc_val_table1[i] - 25)) && (temp1 < (adc_val_table1[i] + 25)))
			return key_num[i + ADC_MAX_NUM];
	}
	for (i = 0; i < ADC_MAX_NUM; i++) {
		if((temp2 > (adc_val_table2[i] - 25)) && (temp2 < (adc_val_table2[i] + 25)))
			return key_num[i + ADC_MAX_NUM * 2];
	}

	return KEY_NULL;
}


struct adc_keys_button_data {
	int last_state;
	int count;
	int threshold;
	int can_sleep;
};

struct adc_keys_polled_dev {
	struct input_polled_dev *poll_dev;
	struct device *dev;
	struct adc_keys_platform_data *pdata;
	struct adc_keys_button_data data[0];
};


static void adc_keys_polled_poll(struct input_polled_dev *dev)
{
	struct adc_keys_polled_dev *bdev = dev->private;
	struct adc_keys_platform_data *pdata = bdev->pdata;
	struct adc_keys_button_data *bdata = bdev->data;
	struct input_dev *input = dev->input;
	int state = 0;
	int key;

	if (bdata->count < bdata->threshold)
		bdata->count++;
	else {
		state = adc_read();
		if (state != bdata->last_state) {
			key = state ^ bdata->last_state;
			key = find_first_bit((unsigned long *)&key, 32);

			input_event(input, pdata->buttons[key].type ?: EV_KEY, 
						pdata->buttons[key].code, 
						!!(((state>>key)&0x1) ^ pdata->buttons[key].active_low));
			input_sync(input);

			bdata->count = 0;
			bdata->last_state = state;
		}
	}
}

static void adc_keys_polled_open(struct input_polled_dev *dev)
{
	struct adc_keys_polled_dev *bdev = dev->private;
	struct adc_keys_platform_data *pdata = bdev->pdata;

	if (pdata->enable)
		pdata->enable(bdev->dev);
}

static void adc_keys_polled_close(struct input_polled_dev *dev)
{
	struct adc_keys_polled_dev *bdev = dev->private;
	struct adc_keys_platform_data *pdata = bdev->pdata;

	if (pdata->disable)
		pdata->disable(bdev->dev);
}

static int __devinit adc_keys_polled_probe(struct platform_device *pdev)
{
	struct adc_keys_platform_data *pdata = pdev->dev.platform_data;
	struct device *dev = &pdev->dev;
	struct adc_keys_polled_dev *bdev;
	struct input_polled_dev *poll_dev;
	struct input_dev *input;
	int error;
	int i;

	if (!pdata || !pdata->poll_interval)
		return -EINVAL;

	bdev = kzalloc(sizeof(struct adc_keys_polled_dev) +
		       pdata->nbuttons * sizeof(struct adc_keys_button_data),
		       GFP_KERNEL);
	if (!bdev) {
		dev_err(dev, "no memory for private data\n");
		return -ENOMEM;
	}

	poll_dev = input_allocate_polled_device();
	if (!poll_dev) {
		dev_err(dev, "no memory for polled device\n");
		error = -ENOMEM;
		goto err_free_bdev;
	}

	poll_dev->private = bdev;
	poll_dev->poll = adc_keys_polled_poll;
	poll_dev->poll_interval = pdata->poll_interval;
	poll_dev->open = adc_keys_polled_open;
	poll_dev->close = adc_keys_polled_close;

	input = poll_dev->input;

	input->evbit[0] = BIT(EV_KEY);
	input->name = pdev->name;
	input->phys = DRV_NAME"/input0";
	input->dev.parent = &pdev->dev;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	ls1c_adc_init();

	for (i = 0; i < pdata->nbuttons; i++) {
		struct gpio_keys_button *button = &pdata->buttons[i];
		struct adc_keys_button_data *bdata = &bdev->data[i];
		unsigned int type = button->type ?: EV_KEY;

		if (button->wakeup) {
			dev_err(dev, DRV_NAME " does not support wakeup\n");
			error = -EINVAL;
			goto err_free_gpio;
		}

//		bdata->can_sleep = gpio_cansleep(gpio);
		bdata->last_state = -1;
		bdata->threshold = DIV_ROUND_UP(button->debounce_interval,
						pdata->poll_interval);

		input_set_capability(input, type, button->code);
	}

	bdev->poll_dev = poll_dev;
	bdev->dev = dev;
	bdev->pdata = pdata;
	platform_set_drvdata(pdev, bdev);

	error = input_register_polled_device(poll_dev);
	if (error) {
		dev_err(dev, "unable to register polled device, err=%d\n",
			error);
		goto err_free_gpio;
	}

	/* report initial state of the buttons */
//	for (i = 0; i < pdata->nbuttons; i++)
//		gpio_keys_polled_check_state(input, &pdata->buttons[i],
//					 &bdev->data[i]);

	return 0;

err_free_gpio:
	input_free_polled_device(poll_dev);

err_free_bdev:
	kfree(bdev);

	platform_set_drvdata(pdev, NULL);
	return error;
}

static int __devexit adc_keys_polled_remove(struct platform_device *pdev)
{
	struct adc_keys_polled_dev *bdev = platform_get_drvdata(pdev);

	input_unregister_polled_device(bdev->poll_dev);
	input_free_polled_device(bdev->poll_dev);

	kfree(bdev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver adc_keys_polled_driver = {
	.probe	= adc_keys_polled_probe,
	.remove	= __devexit_p(adc_keys_polled_remove),
	.driver	= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init adc_keys_polled_init(void)
{
	return platform_driver_register(&adc_keys_polled_driver);
}

static void __exit adc_keys_polled_exit(void)
{
	platform_driver_unregister(&adc_keys_polled_driver);
}

module_init(adc_keys_polled_init);
module_exit(adc_keys_polled_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Gabor Juhos <juhosg@openwrt.org>");
MODULE_DESCRIPTION("Polled GPIO Buttons driver");
MODULE_ALIAS("platform:" DRV_NAME);
