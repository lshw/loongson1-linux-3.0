/*
 * Copyright (c) 2012 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/io.h>
#include <linux/pm.h>

#include <asm/reboot.h>

#include <asm/gpio.h>
#include <loongson1.h>

static void ls1x_restart(char *command)
{
	void __iomem *wdt_base = ioremap(LS1X_WDT_BASE, 0x0f);
	
	writel(1, wdt_base + WDT_EN);
	writel(1, wdt_base + WDT_TIMER);
	writel(1, wdt_base + WDT_SET);

#ifdef CONFIG_LS1A_CLOUD_TERMIAL
	gpio_direction_output(0, 0);
#endif
	
	while (1) {
		__asm__(".set push;\n"
			".set mips3;\n"
			"wait;\n"
			".set pop;\n"
		);
	}
}

static void ls1x_halt(void)
{
	pr_notice("\n\n** You can safely turn off the power now **\n\n");
	while (1) {
		if (cpu_wait)
			cpu_wait();
	}
}

static void ls1x_power_off(void)
{
	ls1x_halt();
}

static int __init ls1x_reboot_setup(void)
{
	_machine_restart = ls1x_restart;
	_machine_halt = ls1x_halt;
	pm_power_off = ls1x_power_off;
	
	return 0;
}

arch_initcall(ls1x_reboot_setup);

