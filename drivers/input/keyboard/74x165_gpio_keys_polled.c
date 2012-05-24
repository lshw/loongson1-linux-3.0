/*
 *  Driver for buttons on GPIO lines not capable of generating interrupts
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
#include <linux/input/74x165_gpio_keys_polled.h>

#define DRV_NAME	"gen74x165_gpio-keys-polled"

struct gpio_keys_button_data {
	int last_state;
	int count;
	int threshold;
	int can_sleep;
};

struct gpio_keys_polled_dev {
	struct input_polled_dev *poll_dev;
	struct device *dev;
	struct gen_74x165_platform_data *pdata;
	struct gpio_keys_button_data data[0];
};

static void gpio_keys_polled_poll(struct input_polled_dev *dev)
{
	struct gpio_keys_polled_dev *bdev = dev->private;
	struct gen_74x165_platform_data *pdata = bdev->pdata;
	struct gpio_keys_button_data *bdata = bdev->data;
	struct input_dev *input = dev->input;
	int i;
	int state = 0;
	int key;

	if (bdata->count < bdata->threshold)
		bdata->count++;
	else {
		gpio_set_value(pdata->cp, 0);
		gpio_set_value(pdata->pl, 0);
		gpio_set_value(pdata->pl, 1);
		for (i = 0; i < bdev->pdata->nbuttons; i++) {
				key = gpio_get_value(pdata->q7);
				state |= key << i;
				gpio_set_value(pdata->cp, 0);
				gpio_set_value(pdata->cp, 1);
		}
		if (state != bdata->last_state) {
			key = state ^ bdata->last_state;
			key = find_first_bit((unsigned long *)&key, 16);

			input_event(input, pdata->buttons[key].type ?: EV_KEY, 
						pdata->buttons[key].code, 
						!!(((state>>key)&0x1) ^ pdata->buttons[key].active_low));
			input_sync(input);

			bdata->count = 0;
			bdata->last_state = state;
		}
	}
}

static void gpio_keys_polled_open(struct input_polled_dev *dev)
{
	struct gpio_keys_polled_dev *bdev = dev->private;
	struct gen_74x165_platform_data *pdata = bdev->pdata;

	if (pdata->enable)
		pdata->enable(bdev->dev);
}

static void gpio_keys_polled_close(struct input_polled_dev *dev)
{
	struct gpio_keys_polled_dev *bdev = dev->private;
	struct gen_74x165_platform_data *pdata = bdev->pdata;

	if (pdata->disable)
		pdata->disable(bdev->dev);
}

static int __devinit gpio_keys_polled_probe(struct platform_device *pdev)
{
	struct gen_74x165_platform_data *pdata = pdev->dev.platform_data;
	struct device *dev = &pdev->dev;
	struct gpio_keys_polled_dev *bdev;
	struct input_polled_dev *poll_dev;
	struct input_dev *input;
	int error;
	int i;

	if (!pdata || !pdata->poll_interval)
		return -EINVAL;

	bdev = kzalloc(sizeof(struct gpio_keys_polled_dev), GFP_KERNEL);
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
	poll_dev->poll = gpio_keys_polled_poll;
	poll_dev->poll_interval = pdata->poll_interval;
	poll_dev->open = gpio_keys_polled_open;
	poll_dev->close = gpio_keys_polled_close;

	input = poll_dev->input;

	input->evbit[0] = BIT(EV_KEY);
	input->name = pdev->name;
	input->phys = DRV_NAME"/input0";
	input->dev.parent = &pdev->dev;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	/* q7 input */
	error = gpio_request(pdata->q7, DRV_NAME);
	if (error) {
		dev_err(dev, "unable to claim q7 %u, err=%d\n",
			pdata->q7, error);
		goto err_poll_dev;
	}
	
	error = gpio_direction_input(pdata->q7);
	if (error) {
		dev_err(dev,
			"unable to set direction on q7 %u, err=%d\n",
			pdata->q7, error);
		goto err_poll_dev;
	}
	/* cp output */
	error = gpio_request(pdata->cp, DRV_NAME);
	if (error) {
		dev_err(dev, "unable to claim cp %u, err=%d\n",
			pdata->cp, error);
		goto err_free_gpio_q7;
	}
	
	error = gpio_direction_output(pdata->cp, 1);
	if (error) {
		dev_err(dev,
			"unable to set direction on cp %u, err=%d\n",
			pdata->cp, error);
		goto err_free_gpio_q7;
	}
	/* pl output */
	error = gpio_request(pdata->pl, DRV_NAME);
	if (error) {
		dev_err(dev, "unable to claim pl %u, err=%d\n",
			pdata->pl, error);
		goto err_free_gpio_cp;
	}
	
	error = gpio_direction_output(pdata->pl, 1);
	if (error) {
		dev_err(dev,
			"unable to set direction on pl %u, err=%d\n",
			pdata->pl, error);
		goto err_free_gpio_cp;
	}

	bdev->data->can_sleep = gpio_cansleep(pdata->q7);
	bdev->data->can_sleep += gpio_cansleep(pdata->cp);
	bdev->data->can_sleep += gpio_cansleep(pdata->pl);
	bdev->data->last_state = -1;
	bdev->data->threshold = DIV_ROUND_UP(pdata->debounce_interval,
					pdata->poll_interval);
	
	for (i = 0; i < pdata->nbuttons; i++) {
		struct gpio_keys_button *button = &pdata->buttons[i];
		unsigned int type = button->type ?: EV_KEY;

		if (button->wakeup) {
			dev_err(dev, DRV_NAME " does not support wakeup\n");
			error = -EINVAL;
			goto err_free_gpio_pl;
		}

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
		goto err_free_gpio_pl;
	}

	return 0;

err_free_gpio_pl:
	gpio_free(pdata->pl);
err_free_gpio_cp:
	gpio_free(pdata->cp);
err_free_gpio_q7:
	gpio_free(pdata->q7);
err_poll_dev:
	input_free_polled_device(poll_dev);

err_free_bdev:
	kfree(bdev);

	platform_set_drvdata(pdev, NULL);
	return error;
}

static int __devexit gpio_keys_polled_remove(struct platform_device *pdev)
{
	struct gpio_keys_polled_dev *bdev = platform_get_drvdata(pdev);
	struct gen_74x165_platform_data *pdata = bdev->pdata;

	input_unregister_polled_device(bdev->poll_dev);

	gpio_free(pdata->q7);
	gpio_free(pdata->cp);
	gpio_free(pdata->pl);

	input_free_polled_device(bdev->poll_dev);

	kfree(bdev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver gpio_keys_polled_driver = {
	.probe	= gpio_keys_polled_probe,
	.remove	= __devexit_p(gpio_keys_polled_remove),
	.driver	= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init gpio_keys_polled_init(void)
{
	return platform_driver_register(&gpio_keys_polled_driver);
}

static void __exit gpio_keys_polled_exit(void)
{
	platform_driver_unregister(&gpio_keys_polled_driver);
}

module_init(gpio_keys_polled_init);
module_exit(gpio_keys_polled_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tanghaifeng <rou.ru.cai.hong@gmail.com>");
MODULE_DESCRIPTION("74x165 Polled GPIO Buttons driver");
MODULE_ALIAS("platform:" DRV_NAME);
