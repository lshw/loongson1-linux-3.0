/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/pm.h>

#include <asm/reboot.h>

#include <asm/mach-loongson/ls1x/ls1b_board.h>

static void ls232_restart(char *command)
{
	void __iomem *wdt_base = ioremap(LS1B_BOARD_WAT_BASE, 0x0f);
	
	writel(1, wdt_base + WDT_EN);
	writel(1, wdt_base + WDT_TIMER);
	writel(1, wdt_base + WDT_SET);
	
	while (1) {
		__asm__(".set push;\n"
			".set mips3;\n"
			"wait;\n"
			".set pop;\n"
		);
	}
}

static void ls232_halt(void)
{
	pr_notice("\n\n** You can safely turn off the power now **\n\n");
	while (1) {
		if (cpu_wait)
			cpu_wait();
	}
}

/*
static void ls232_power_off(void)
{
	ls232_halt();
}
*/

void mips_reboot_setup(void)
{
	_machine_restart = ls232_restart;
	_machine_halt = ls232_halt;
//	pm_power_off = ls232_power_off;
}
