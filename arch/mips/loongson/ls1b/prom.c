/*
 * Copyright 2002 Momentum Computer Inc.
 * Author: Matthew Dharm <mdharm@momenco.com>
 *
 * Based on Ocelot Linux port, which is
 * Copyright 2001 MontaVista Software Inc.
 * Author: jsun@mvista.com or jsun@junsun.net
 *
 * Copyright 2003 ICT CAS
 * Author: Michael Guo <guoyi@ict.ac.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
//#include <linux/config.h>
//#include <linux/autoconf.h>
#include <generated/autoconf.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/bootmem.h>

#include <asm/addrspace.h>
#include <asm/bootinfo.h>

#define	CL_SIZE COMMAND_LINE_SIZE
extern char arcs_cmdline[CL_SIZE];

extern unsigned long bus_clock;
extern unsigned long ls1b_cpu_clock;
extern unsigned int  memsize,highmemsize;
extern int putDebugChar(unsigned char byte);
extern void prom_printf(char *fmt, ...);

static int  	argc;
/* ls232 pmon passes arguments in 32bit pointers */
static int *	arg;
static int *	env;

const char *get_system_type(void)
{
	return "LS232 Evaluation board-V1.0";
}

void __init prom_init_cmdline(void)
{
	int i;
     /* some 64bit toolchain can't convert int to a pointer correctly */
	long l;
	
	/* arg[0] is "g", the rest is boot parameters */
	arcs_cmdline[0] = '\0';
	for (i = 1; i < argc; i++) {
		l = (long)arg[i];
		if (strlen(arcs_cmdline) + strlen(((char*)l) + 1)
		    >= sizeof(arcs_cmdline))
			break;
		strcat(arcs_cmdline, ((char*)l));
		strcat(arcs_cmdline, " ");
	}
}

void __init prom_init(void)
{
	long l;
  	argc=fw_arg0;
	arg=(int *)fw_arg1;
	env=(int *)fw_arg2;

//	mips_machgroup = MACH_GROUP_LST;
	mips_machtype = MACH_LS232;

#ifndef CONFIG_PMON
#define CONFIG_PMON
#endif

#ifdef CONFIG_PMON
	prom_init_cmdline();

	if((strstr(arcs_cmdline, "console=")) == NULL)	
		strcat(arcs_cmdline, " console=ttyS0,57600");
	if((strstr(arcs_cmdline, "root=")) == NULL)	
		strcat(arcs_cmdline, " root=/dev/ram1");
//		strcat(arcs_cmdline, " root=/dev/hda1");

	l = (long)*env;
	while (l!=0) {
		if (strncmp("busclock", (char*)l, strlen("busclock")) == 0) {
			bus_clock = simple_strtol((char*)l + strlen("busclock="),
							NULL, 10);
		}
		if (strncmp("cpuclock", (char*)l, strlen("cpuclock")) == 0) {
			ls1b_cpu_clock = simple_strtol((char*)l + strlen("cpuclock="),
							NULL, 10);
		}
		if (strncmp("memsize", (char*)l, strlen("memsize")) == 0) {
			memsize = simple_strtol((char*)l + strlen("memsize="),
							NULL, 10);
		}
		if (strncmp("highmemsize", (char*)l, strlen("highmemsize")) == 0) {
			highmemsize = simple_strtol((char*)l + strlen("highmemsize="),
							NULL, 10);
		}
		env++;
		l=(long)*env;
	}
#else
	strcat(arcs_cmdline, " root=/dev/hda3 console=tty");
	bus_clock = 66000000;
	ls1b_cpu_clock = 666000000;
#endif
	if (memsize == 0) 
		memsize = 256;
//		memsize = 128;

	prom_printf("Booting Linux kernel...\n");
	system_state = SYSTEM_BOOTING;
	prom_printf("system_state: %d\taddr:%08x\n", system_state, &system_state);
	printk("busclock=%ld, cpuclock=%ld,memsize=%d,highmemsize=%d\n", bus_clock, ls1b_cpu_clock,memsize,highmemsize);
}

void __init prom_free_prom_memory(void)
{
}

void prom_putchar(char c)
{
	putDebugChar(c);
}

