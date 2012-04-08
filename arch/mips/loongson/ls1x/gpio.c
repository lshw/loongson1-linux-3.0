/*
 *  STLS1B GPIO Support
 *
 *  Copyright (c) 2008 Richard Liu,  STMicroelectronics  <richard.liu@st.com>
 *  Copyright (c) 2008-2010 Arnaud Patard <apatard@mandriva.com>
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
#include <asm/types.h>
//#include <loongson.h>
#include <linux/gpio.h>

#define STLS1B_N_GPIO		64
#define STLS1B_GPIO_IN_OFFSET	16

#define LOONGSON_REG(x)	\
	(*(volatile u32 *)((char *)CKSEG1ADDR(x)))

#define LOONGSON_GPIOCFG0	LOONGSON_REG(0xbfd010c0)
#define LOONGSON_GPIOCFG1	LOONGSON_REG(0xbfd010c4)
#define LOONGSON_GPIOIE0 	LOONGSON_REG(0xbfd010d0)
#define LOONGSON_GPIOIE1	LOONGSON_REG(0xbfd010d4)
#define LOONGSON_GPIOIN0	LOONGSON_REG(0xbfd010e0)
#define LOONGSON_GPIOIN1	LOONGSON_REG(0xbfd010e4)
#define LOONGSON_GPIOOUT0	LOONGSON_REG(0xbfd010f0)
#define LOONGSON_GPIOOUT1	LOONGSON_REG(0xbfd010f4)


static DEFINE_SPINLOCK(gpio_lock);

int gpio_get_value(unsigned gpio)
{
	u32 val;
	u32 mask;

	if (gpio >= STLS1B_N_GPIO)
		return __gpio_get_value(gpio);

	if(gpio >= 32){
		mask = 1 << (gpio - 32);
		spin_lock(&gpio_lock);
		val = LOONGSON_GPIOIN1;
		spin_unlock(&gpio_lock);
	}else{
		mask = 1 << gpio;
		spin_lock(&gpio_lock);
		val = LOONGSON_GPIOIN0;
		spin_unlock(&gpio_lock);
	}

	return ((val & mask) != 0);
}
EXPORT_SYMBOL(gpio_get_value);

void gpio_set_value(unsigned gpio, int state)
{
	u32 val;
	u32 mask;

	if (gpio >= STLS1B_N_GPIO) {
		__gpio_set_value(gpio, state);
		return ;
	}

	if(gpio >= 32){
		mask = 1 << (gpio - 32);
		spin_lock(&gpio_lock);
		val = LOONGSON_GPIOOUT1;
		if (state)
			val |= mask;
		else
			val &= (~mask);
		LOONGSON_GPIOOUT1 = val;
		spin_unlock(&gpio_lock);
	}else{
		mask = 1 << gpio;
		spin_lock(&gpio_lock);
		val = LOONGSON_GPIOOUT0;
		if(state)	
			val |= mask;
		else
			val &= ~mask;
		LOONGSON_GPIOOUT0 = val;
		spin_unlock(&gpio_lock);
	}
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

static int ls1b_gpio_direction_input(struct gpio_chip *chip, unsigned gpio)
{
	u32 temp;
	u32 mask;

	if (gpio >= STLS1B_N_GPIO)
		return -EINVAL;

	if(gpio >= 32){
		spin_lock(&gpio_lock);
		mask = 1 << (gpio - 32);
		temp = LOONGSON_GPIOCFG1;
		temp |= mask;
		LOONGSON_GPIOCFG1 = temp;
		temp = LOONGSON_GPIOIE1;
		temp |= mask;
		LOONGSON_GPIOIE1 = temp;
		spin_unlock(&gpio_lock);
	}else{
		spin_lock(&gpio_lock);
		mask = 1 << gpio;
		temp = LOONGSON_GPIOCFG0;
		temp |= mask;
		LOONGSON_GPIOCFG0 = temp;
		temp = LOONGSON_GPIOIE0;
		temp |= mask;
		LOONGSON_GPIOIE0 = temp;
		spin_unlock(&gpio_lock);
	}

	return 0;
}

static int ls1b_gpio_direction_output(struct gpio_chip *chip,
		unsigned gpio, int level)
{
	u32 temp;
	u32 mask;

	if (gpio >= STLS1B_N_GPIO)
		return -EINVAL;

	gpio_set_value(gpio, level);
	
	if(gpio >= 32){
		spin_lock(&gpio_lock);
		mask = 1 << (gpio - 32);
		temp = LOONGSON_GPIOCFG1;
		temp |= mask;
		LOONGSON_GPIOCFG1 = temp;
		temp = LOONGSON_GPIOIE1;
		temp &= (~mask);
		LOONGSON_GPIOIE1 = temp;
		spin_unlock(&gpio_lock);
	}else{	
		spin_lock(&gpio_lock);
		mask = 1 << gpio;
		temp = LOONGSON_GPIOCFG0;
		temp |= mask;
		LOONGSON_GPIOCFG0 = temp;
		temp = LOONGSON_GPIOIE0;
		temp &= (~mask);
		LOONGSON_GPIOIE0 = temp;
		spin_unlock(&gpio_lock);
	}

	return 0;
}

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
	.base                   = 0,
	.ngpio                  = STLS1B_N_GPIO,
};

static int __init ls1b_gpio_setup(void)
{
	return gpiochip_add(&ls1b_chip);
}
arch_initcall(ls1b_gpio_setup);
