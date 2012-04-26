/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Copyright (C) 1997, 2001 Ralf Baechle
 * Copyright 2001 MontaVista Software Inc.
 * Copyright 2003 ICT CAS
 * Author: jsun@mvista.com or jsun@junsun.net
 *         guoyi@ict.ac.cn
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
	printk(KERN_NOTICE "\n** You can safely turn off the power\n");
	while(1);
}

static void ls232_power_off(void)
{
	ls232_halt();
}

void mips_reboot_setup(void)
{
	_machine_restart = ls232_restart;
	_machine_halt = ls232_halt;
//	pm_power_off = ls232_power_off;
}
