/*
 * STLS2F GPIO Support
 *
 * Copyright (c) 2008  Richard Liu, STMicroelectronics <richard.liu@st.com>
 * Copyright (c) 2008-2010  Arnaud Patard <apatard@mandriva.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef	__STLS2F_GPIO_H
#define	__STLS2F_GPIO_H

#include <irq.h>
#include <asm-generic/gpio.h>

extern void gpio_set_value(unsigned gpio, int value);
extern int gpio_get_value(unsigned gpio);
extern int gpio_cansleep(unsigned gpio);
extern int ls1b_gpio_direction_input(struct gpio_chip *chip, unsigned gpio);
extern int ls1b_gpio_direction_output(struct gpio_chip *chip,
		unsigned gpio, int level);
extern void ls1b_gpio_free(struct gpio_chip *chip, unsigned gpio);

/* The chip can do interrupt
 * but it has not been tested and doc not clear
 */
static inline int gpio_to_irq(int gpio)
{
//	return -EINVAL;
	return LS1X_GPIO_FIRST_IRQ + gpio;
}

static inline int irq_to_gpio(int irq)
{
//	return -EINVAL;
	return irq - LS1X_GPIO_FIRST_IRQ;
}

#endif				/* __STLS2F_GPIO_H */
