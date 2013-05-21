/*
 *  Copyright (c) 2012 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/gpio.h>

#include <asm/types.h>
#include <ls1b_board.h>
#include <irq.h>

#if defined(CONFIG_LS1A_MACH)
#define LS1X_N_GPIO		88
#elif defined(CONFIG_LS1B_MACH)
#define LS1X_N_GPIO		64
#elif defined(CONFIG_LS1C_MACH)
#define LS1X_N_GPIO		128
#endif

static DEFINE_SPINLOCK(gpio_lock);

static int ls_gpio_get_value(unsigned gpio)
{
	u32 val;
	u32 mask;

//	if (gpio >= LS1X_N_GPIO)
//		return __gpio_get_value(gpio);

	spin_lock(&gpio_lock);
#ifdef CONFIG_LS1A_MACH
	if (gpio >= 64) {
		mask = 1 << (gpio - 64);
		val = __raw_readl(LS1X_GPIO_IN2);
	} else
#endif
	{
		if (gpio >= 32) {
			mask = 1 << (gpio - 32);
			val = __raw_readl(LS1X_GPIO_IN1);
		} else {
			mask = 1 << gpio;
			val = __raw_readl(LS1X_GPIO_IN0);
		}
	}
	spin_unlock(&gpio_lock);

	return ((val & mask) != 0);
}

static void ls_gpio_set_value(unsigned gpio, int state)
{
	u32 val;
	u32 mask;

//	if (gpio >= LS1X_N_GPIO) {
//		__gpio_set_value(gpio, state);
//		return ;
//	}

	spin_lock(&gpio_lock);
#ifdef CONFIG_LS1A_MACH
	if (gpio >= 64) {
		mask = 1 << (gpio - 64);
		val = __raw_readl(LS1X_GPIO_OUT2);
		if (state)
			val |= mask;
		else
			val &= (~mask);
		__raw_writel(val, LS1X_GPIO_OUT2);
	} else
#endif
	{
		if (gpio >= 32) {
			mask = 1 << (gpio - 32);
			val = __raw_readl(LS1X_GPIO_OUT1);
			if (state)
				val |= mask;
			else
				val &= (~mask);
			__raw_writel(val, LS1X_GPIO_OUT1);
		} else {
			mask = 1 << gpio;
			val = __raw_readl(LS1X_GPIO_OUT0);
			if(state)	
				val |= mask;
			else
				val &= ~mask;
			__raw_writel(val, LS1X_GPIO_OUT0);
		}
	}
	spin_unlock(&gpio_lock);
}

int gpio_to_irq(unsigned gpio)
{
	return LS1X_GPIO_FIRST_IRQ + gpio;
}
EXPORT_SYMBOL_GPL(gpio_to_irq);

int irq_to_gpio(unsigned irq)
{
	return irq - LS1X_GPIO_FIRST_IRQ;
}
EXPORT_SYMBOL_GPL(irq_to_gpio);

int ls1x_gpio_direction_input(struct gpio_chip *chip, unsigned gpio)
{
	u32 temp;
	u32 mask;

//	if (gpio >= LS1X_N_GPIO)
//		return -EINVAL;

	spin_lock(&gpio_lock);
#ifdef CONFIG_LS1A_MACH
	if (gpio >= 64) {
		mask = 1 << (gpio - 64);
		temp = __raw_readl(LS1X_GPIO_CFG2);
		temp |= mask;
		__raw_writel(temp, LS1X_GPIO_CFG2);
		temp = __raw_readl(LS1X_GPIO_OE2);
		temp |= mask;
		__raw_writel(temp, LS1X_GPIO_OE2);
	} else
#endif
	{
		if (gpio >= 32) {
			mask = 1 << (gpio - 32);
			temp = __raw_readl(LS1X_GPIO_CFG1);
			temp |= mask;
			__raw_writel(temp, LS1X_GPIO_CFG1);
			temp = __raw_readl(LS1X_GPIO_OE1);
			temp |= mask;
			__raw_writel(temp, LS1X_GPIO_OE1);
		} else {
			mask = 1 << gpio;
			temp = __raw_readl(LS1X_GPIO_CFG0);
			temp |= mask;
			__raw_writel(temp, LS1X_GPIO_CFG0);
			temp = __raw_readl(LS1X_GPIO_OE0);
			temp |= mask;
			__raw_writel(temp, LS1X_GPIO_OE0);
		}
	}
	spin_unlock(&gpio_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(ls1x_gpio_direction_input);

int ls1x_gpio_direction_output(struct gpio_chip *chip,
		unsigned gpio, int level)
{
	u32 temp;
	u32 mask;

//	if (gpio >= LS1X_N_GPIO)
//		return -EINVAL;

	ls_gpio_set_value(gpio, level);
	spin_lock(&gpio_lock);
#ifdef CONFIG_LS1A_MACH
	if (gpio >= 64) {
		mask = 1 << (gpio - 64);
		temp = __raw_readl(LS1X_GPIO_CFG2);
		temp |= mask;
		__raw_writel(temp, LS1X_GPIO_CFG2);
		temp = __raw_readl(LS1X_GPIO_OE2);
		temp &= (~mask);
		__raw_writel(temp, LS1X_GPIO_OE2);
	} else
#endif
	{
		if (gpio >= 32) {
			mask = 1 << (gpio - 32);
			temp = __raw_readl(LS1X_GPIO_CFG1);
			temp |= mask;
			__raw_writel(temp, LS1X_GPIO_CFG1);
			temp = __raw_readl(LS1X_GPIO_OE1);
			temp &= (~mask);
			__raw_writel(temp, LS1X_GPIO_OE1);
		} else {
			mask = 1 << gpio;
			temp = __raw_readl(LS1X_GPIO_CFG0);
			temp |= mask;
			__raw_writel(temp, LS1X_GPIO_CFG0);
			temp = __raw_readl(LS1X_GPIO_OE0);
			temp &= (~mask);
			__raw_writel(temp, LS1X_GPIO_OE0);
		}
	}
	spin_unlock(&gpio_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(ls1x_gpio_direction_output);

void ls1x_gpio_free(struct gpio_chip *chip, unsigned gpio)
{
	u32 temp;
	u32 mask;

#ifdef CONFIG_LS1A_MACH
	if(gpio >= 64){
		mask = 1 << (gpio - 64);
		temp = __raw_readl(LS1X_GPIO_CFG2);
		temp &= ~mask;
		__raw_writel(temp, LS1X_GPIO_CFG2);
	} else
#endif
	{
		if (gpio >= 32) {
			mask = 1 << (gpio - 32);
			temp = __raw_readl(LS1X_GPIO_CFG1);
			temp &= ~mask;
			__raw_writel(temp, LS1X_GPIO_CFG1);
		} else {
			mask = 1 << gpio;
			temp = __raw_readl(LS1X_GPIO_CFG0);
			temp &= ~mask;
			__raw_writel(temp, LS1X_GPIO_CFG0);
		}
	}
}
EXPORT_SYMBOL(ls1x_gpio_free);

static int ls1x_gpio_get_value(struct gpio_chip *chip, unsigned gpio)
{
	return ls_gpio_get_value(gpio);
}

static void ls1x_gpio_set_value(struct gpio_chip *chip,
		unsigned gpio, int value)
{
	ls_gpio_set_value(gpio, value);
}

static struct gpio_chip ls1x_chip = {
	.label                  = "ls1x",
	.direction_input        = ls1x_gpio_direction_input,
	.direction_output       = ls1x_gpio_direction_output,
	.get                    = ls1x_gpio_get_value,
	.set                    = ls1x_gpio_set_value,
	.free					= ls1x_gpio_free,
	.base                   = 0,
	.ngpio                  = LS1X_N_GPIO,
};

static int __init ls1x_gpio_setup(void)
{
	return gpiochip_add(&ls1x_chip);
}
arch_initcall(ls1x_gpio_setup);
