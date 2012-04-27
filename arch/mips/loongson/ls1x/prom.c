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
#include <generated/autoconf.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/bootmem.h>

#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm/mach-loongson/ls1x/ls1b_board.h>
#include <asm/mach-loongson/ls1x/fb.h>

#define	CL_SIZE COMMAND_LINE_SIZE
extern char arcs_cmdline[CL_SIZE];

extern unsigned long bus_clock;
extern unsigned long ls1b_cpu_clock;
extern unsigned int  memsize, highmemsize;
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
	int pll,ctrl,clk;
	char *tmp, *end;
#define PLL_FREQ_REG(x) *(volatile unsigned int *)(0xbfe78030+x)

	
  	argc=fw_arg0;
	arg=(int *)fw_arg1;
	env=(int *)fw_arg2;

//	mips_machgroup = MACH_GROUP_LST;
	mips_machtype = MACH_LS232;

#ifndef CONFIG_PMON
#define CONFIG_PMON
#endif
//	prom_printf("Booting Linux kernel...\n");
	system_state = SYSTEM_BOOTING;
//	prom_printf("system_state: %d\taddr:%08x\n", system_state, &system_state);
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

	if (bus_clock == 0){
	#ifdef CONFIG_LS1A_MACH
		pll	= *(volatile unsigned int *)(0xbfe78030);
		ddr_clk = (((pll>>8)&7)+3)*APB_CLK;
	#else
		pll = PLL_FREQ_REG(0);
		ctrl = PLL_FREQ_REG(4);
		clk = (12+(pll&0x3f))*APB_CLK/2 + ((pll>>8)&0x3ff)*APB_CLK/2/1024;
		ls1b_cpu_clock = ((ctrl&0x300)==0x300) ? APB_CLK : (ctrl&(1<<25)) ? clk/((ctrl>>20)&0x1f) : clk/2;
		bus_clock = ((ctrl&0xc00)==0xc00) ? APB_CLK : (ctrl&(1<<19)) ? clk/((ctrl>>14)&0x1f) : clk/2;
	#endif
	}
	
	tmp = strstr(arcs_cmdline, "video=ls1bfb:vga");
	if(tmp){
		int i, xres, yres;
		
		tmp += 16;
		for (i = 0; i < strlen(tmp); i++){
			if (isdigit(*(tmp+i))){
				break;
			}
		}
		xres = simple_strtoul(tmp+i, &end, 10);
		yres = simple_strtoul(end+1, NULL, 10);
		if ((xres<=0 || xres>2000)||(yres<=0 || yres>2000)){
		}
		
		for(i=0; i<sizeof(vgamode)/sizeof(struct vga_struc); i++){
			if(vgamode[i].hr == xres && vgamode[i].vr == yres){
				break;
			}
		}
		if(i<0){
			i = 0;
		}
		PLL_FREQ_REG(4) = vgamode[i].pll_reg1;
		PLL_FREQ_REG(0) = vgamode[i].pll_reg0;
		
		pll = PLL_FREQ_REG(0);
		ctrl = PLL_FREQ_REG(4);
		clk = (12+(pll&0x3f))*APB_CLK/2 + ((pll>>8)&0x3ff)*APB_CLK/2/1024;
		ls1b_cpu_clock = ((ctrl&0x300)==0x300)?APB_CLK:(ctrl&(1<<25))?clk/((ctrl>>20)&0x1f):clk/2;
		bus_clock = ((ctrl&0xc00)==0xc00)?APB_CLK:(ctrl&(1<<19))?clk/((ctrl>>14)&0x1f):clk/2;
	}
	
#else
	strcat(arcs_cmdline, " root=/dev/hda3 console=tty");
	bus_clock = 66000000;
	ls1b_cpu_clock = 666000000;
#endif
	if (memsize == 0) 
		memsize = 64;

//	prom_printf("busclock=%ld, cpuclock=%ld,memsize=%d,highmemsize=%d\n", bus_clock, ls1b_cpu_clock,memsize,highmemsize);
//	printk("busclock=%ld, cpuclock=%ld,memsize=%d,highmemsize=%d\n", bus_clock, ls1b_cpu_clock,memsize,highmemsize);
}

void __init prom_free_prom_memory(void)
{
}

void prom_putchar(char c)
{
	putDebugChar(c);
}

