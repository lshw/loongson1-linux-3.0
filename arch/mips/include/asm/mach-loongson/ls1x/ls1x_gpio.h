#ifndef __ASM_ARCH_LS1X_GPIO_H
#define __ASM_ARCH_LS1X_GPIO_H

/* get everything through gpiolib */
#define gpio_to_irq	__gpio_to_irq
#define gpio_get_value	__gpio_get_value
#define gpio_set_value	__gpio_set_value
#define gpio_cansleep	__gpio_cansleep
#define irq_to_gpio	ls1x_irq_to_gpio

#include <asm-generic/gpio.h>

#endif /* __ASM_ARCH_LS1X_GPIO_H */
