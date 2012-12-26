/*
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
#include <asm/mach-loongson/ls1x/ls1b_board.h>

#ifdef CONFIG_LS1A_MACH
#define STLS1B_N_GPIO		88
#elif CONFIG_LS1B_MACH
#define STLS1B_N_GPIO		64
#endif

static DEFINE_SPINLOCK(gpio_lock);

int gpio_get_value(unsigned gpio)
{
	u32 val;
	u32 mask;

//	if (gpio >= STLS1B_N_GPIO)
//		return __gpio_get_value(gpio);

	spin_lock(&gpio_lock);
#ifdef CONFIG_LS1A_MACH
	if (gpio >= 64) {
		mask = 1 << (gpio - 64);
		val = LOONGSON_GPIOIN2;
	} else
#endif
	{
		if (gpio >= 32) {
			mask = 1 << (gpio - 32);
			val = LOONGSON_GPIOIN1;
		} else {
			mask = 1 << gpio;
			val = LOONGSON_GPIOIN0;
		}
	}
	spin_unlock(&gpio_lock);

	return ((val & mask) != 0);
}
EXPORT_SYMBOL(gpio_get_value);

void gpio_set_value(unsigned gpio, int state)
{
	u32 val;
	u32 mask;

//	if (gpio >= STLS1B_N_GPIO) {
//		__gpio_set_value(gpio, state);
//		return ;
//	}

	spin_lock(&gpio_lock);
#ifdef CONFIG_LS1A_MACH
	if (gpio >= 64) {
		mask = 1 << (gpio - 64);
		val = LOONGSON_GPIOOUT2;
		if (state)
			val |= mask;
		else
			val &= (~mask);
		LOONGSON_GPIOOUT2 = val;
	} else
#endif
	{
		if (gpio >= 32) {
			mask = 1 << (gpio - 32);
			val = LOONGSON_GPIOOUT1;
			if (state)
				val |= mask;
			else
				val &= (~mask);
			LOONGSON_GPIOOUT1 = val;
		} else {
			mask = 1 << gpio;
			val = LOONGSON_GPIOOUT0;
			if(state)	
				val |= mask;
			else
				val &= ~mask;
			LOONGSON_GPIOOUT0 = val;
		}
	}
	spin_unlock(&gpio_lock);
}
EXPORT_SYMBOL(gpio_set_value);

int gpio_cansleep(unsigned gpio)
{
	if (gpio < STLS1B_N_GPIO)
		return 0;
	else
		return __gpio_cansleep(gpio);
}
EXPORT_SYMBOL(gpio_cansleep);

int ls1b_gpio_direction_input(struct gpio_chip *chip, unsigned gpio)
{
	u32 temp;
	u32 mask;

//	if (gpio >= STLS1B_N_GPIO)
//		return -EINVAL;

	spin_lock(&gpio_lock);
#ifdef CONFIG_LS1A_MACH
	if (gpio >= 64) {
		mask = 1 << (gpio - 64);
		temp = LOONGSON_GPIOCFG2;
		temp |= mask;
		LOONGSON_GPIOCFG2 = temp;
		temp = LOONGSON_GPIOIE2;
		temp |= mask;
		LOONGSON_GPIOIE2 = temp;
	} else
#endif
	{
		if (gpio >= 32) {
			mask = 1 << (gpio - 32);
			temp = LOONGSON_GPIOCFG1;
			temp |= mask;
			LOONGSON_GPIOCFG1 = temp;
			temp = LOONGSON_GPIOIE1;
			temp |= mask;
			LOONGSON_GPIOIE1 = temp;
		} else {
			mask = 1 << gpio;
			temp = LOONGSON_GPIOCFG0;
			temp |= mask;
			LOONGSON_GPIOCFG0 = temp;
			temp = LOONGSON_GPIOIE0;
			temp |= mask;
			LOONGSON_GPIOIE0 = temp;
		}
	}
	spin_unlock(&gpio_lock);
	return 0;
}
EXPORT_SYMBOL(ls1b_gpio_direction_input);

int ls1b_gpio_direction_output(struct gpio_chip *chip,
		unsigned gpio, int level)
{
	u32 temp;
	u32 mask;

//	if (gpio >= STLS1B_N_GPIO)
//		return -EINVAL;

	gpio_set_value(gpio, level);
	spin_lock(&gpio_lock);
#ifdef CONFIG_LS1A_MACH
	if (gpio >= 64) {
		mask = 1 << (gpio - 64);
		temp = LOONGSON_GPIOCFG2;
		temp |= mask;
		LOONGSON_GPIOCFG2 = temp;
		temp = LOONGSON_GPIOIE2;
		temp &= (~mask);
		LOONGSON_GPIOIE2 = temp;
	} else
#endif
	{
		if (gpio >= 32) {
			mask = 1 << (gpio - 32);
			temp = LOONGSON_GPIOCFG1;
			temp |= mask;
			LOONGSON_GPIOCFG1 = temp;
			temp = LOONGSON_GPIOIE1;
			temp &= (~mask);
			LOONGSON_GPIOIE1 = temp;
		} else {
			mask = 1 << gpio;
			temp = LOONGSON_GPIOCFG0;
			temp |= mask;
			LOONGSON_GPIOCFG0 = temp;
			temp = LOONGSON_GPIOIE0;
			temp &= (~mask);
			LOONGSON_GPIOIE0 = temp;
		}
	}
	spin_unlock(&gpio_lock);
	return 0;
}
EXPORT_SYMBOL(ls1b_gpio_direction_output);

void ls1b_gpio_free(struct gpio_chip *chip, unsigned gpio)
{
	u32 temp;
	u32 mask;

#ifdef CONFIG_LS1A_MACH
	if(gpio >= 64){
		mask = 1 << (gpio - 64);
		temp = LOONGSON_GPIOCFG2;
		temp &= ~mask;
		LOONGSON_GPIOCFG2 = temp;
	} else
#endif
	{
		if (gpio >= 32) {
			mask = 1 << (gpio - 32);
			temp = LOONGSON_GPIOCFG1;
			temp &= ~mask;
			LOONGSON_GPIOCFG1 = temp;
		} else {
			mask = 1 << gpio;
			temp = LOONGSON_GPIOCFG0;
			temp &= ~mask;
			LOONGSON_GPIOCFG0 = temp;
		}
	}
}
EXPORT_SYMBOL(ls1b_gpio_free);

static int ls1b_gpio_get_value(struct gpio_chip *chip, unsigned gpio)
{
	return gpio_get_value(gpio);
}

static void ls1b_gpio_set_value(struct gpio_chip *chip,
		unsigned gpio, int value)
{
	gpio_set_value(gpio, value);
}

static struct gpio_chip ls1b_chip = {
	.label                  = "ls1b",
	.direction_input        = ls1b_gpio_direction_input,
	.get                    = ls1b_gpio_get_value,
	.direction_output       = ls1b_gpio_direction_output,
	.set                    = ls1b_gpio_set_value,
	.free					= ls1b_gpio_free,
	.base                   = 0,
	.ngpio                  = STLS1B_N_GPIO,
};

static int __init ls1b_gpio_setup(void)
{
	return gpiochip_add(&ls1b_chip);
}
arch_initcall(ls1b_gpio_setup);
