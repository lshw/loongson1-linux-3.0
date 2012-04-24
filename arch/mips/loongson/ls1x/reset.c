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
#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/reboot.h>
#include <asm/system.h>
#include <linux/delay.h>

void ls232_restart(char *command)
{
	unsigned long *watchdogEn 		= (unsigned long *)0xbfe5c060;	//lxy
	unsigned long *watchdogSet		= (unsigned long *)0xbfe5c064;
	unsigned long *watchdogCount	= (unsigned long *)0xbfe5c068;
	
	//__asm__ __volatile__("jr\t%0"::"r"(0xbfc00000));
	*watchdogEn 	= 0x1;
	*watchdogCount 	= 0x1;
	*watchdogSet	= 0x1;
	printk (KERN_NOTICE "going to reboot.......\n");
}

void ls232_halt(void)
{
	printk(KERN_NOTICE "\n** You can safely turn off the power\n");
	while(1);
}

void ls232_power_off(void)
{
	ls232_halt();
}

void mips_reboot_setup(void)
{
	_machine_restart = ls232_restart;
	_machine_halt = ls232_halt;
//	pm_power_off = ls232_power_off;
}
