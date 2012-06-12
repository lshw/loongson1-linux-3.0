/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/ctype.h>

#include <asm/bootinfo.h>
#include <asm/mach-loongson/ls1x/ls1b_board.h>
#include <asm/mach-loongson/ls1x/fb.h>
#include <prom.h>

#define DEFAULT_MEMSIZE			64	/* If no memsize provided */
#define DEFAULT_BUSCLOCK		133000000
#define DEFAULT_CPUCLOCK		266000000

#ifdef CONFIG_STMMAC_ETH
extern char *hwaddr;
#endif

unsigned long cpu_clock_freq;
unsigned long ls1x_bus_clock;
EXPORT_SYMBOL(cpu_clock_freq);
EXPORT_SYMBOL(ls1x_bus_clock);

unsigned long memsize, highmemsize;
int prom_argc;
char **prom_argv, **prom_envp;

extern int putDebugChar(unsigned char byte);

const char *get_system_type(void)
{
	return "LS232 Evaluation board-V1.0";
}

char *prom_getenv(char *envname)
{
	char **env = prom_envp;
	int i;

	i = strlen(envname);

	while (*env) {
		if (strncmp(envname, *env, i) == 0 && *(*env+i) == '=')
			return *env + i + 1;
		env++;
	}

	return 0;
}

static inline unsigned long env_or_default(char *env, unsigned long dfl)
{
	char *str = prom_getenv(env);
	return str ? simple_strtol(str, 0, 0) : dfl;
}

void __init prom_init_cmdline(void)
{
	char *c = &(arcs_cmdline[0]);
	int i;

	for (i = 1; i < prom_argc; i++) {
		strcpy(c, prom_argv[i]);
		c += strlen(prom_argv[i]);
		if (i < prom_argc-1)
			*c++ = ' ';
	}
	*c = 0;
}

#define PLL_FREQ_REG(x) *(volatile unsigned int *)(0xbfe78030+x)
void __init prom_init(void)
{
	char *tmp, *end;

	prom_argc = fw_arg0;
	prom_argv = (char **)fw_arg1;
	prom_envp = (char **)fw_arg2;

	mips_machtype = MACH_LS232;
	system_state = SYSTEM_BOOTING;

	prom_init_cmdline();

	ls1x_bus_clock = env_or_default("busclock", DEFAULT_BUSCLOCK);
	cpu_clock_freq = env_or_default("cpuclock", DEFAULT_CPUCLOCK);
	memsize = env_or_default("memsize", DEFAULT_MEMSIZE);
	highmemsize = env_or_default("highmemsize", 0x0);

#ifdef	CONFIG_LS1A_MACH
	if (ls1x_bus_clock == 0)
		ls1x_bus_clock = 100  * 1000000;
	if (cpu_clock_freq == 0)
		cpu_clock_freq = 200 * 1000000;
#endif

#ifdef CONFIG_STMMAC_ETH
	hwaddr = prom_getenv("ethaddr");
#endif
	
	tmp = strstr(arcs_cmdline, "video=ls1bfb:vga");
	if (tmp) {
		int i, xres, yres;
		
		tmp += 16;
		for (i = 0; i < strlen(tmp); i++) {
			if (isdigit(*(tmp+i))) {
				break;
			}
		}
		xres = simple_strtoul(tmp+i, &end, 10);
		yres = simple_strtoul(end+1, NULL, 10);
		if ((xres<=0 || xres>2000)||(yres<=0 || yres>2000)) {
			xres = 1024;
			yres = 768;
		}

		for (i=0; i<sizeof(vgamode)/sizeof(struct vga_struc); i++) {
			if(vgamode[i].hr == xres && vgamode[i].vr == yres) {
				break;
			}
		}
		if (i<0) {
			i = 0;
		}
		PLL_FREQ_REG(0) = vgamode[i].pll_reg0;
		PLL_FREQ_REG(4) = vgamode[i].pll_reg1;
	}
	
	pr_info("busclock=%ld, cpuclock=%ld, memsize=%ld, highmemsize=%ld\n",
		ls1x_bus_clock, cpu_clock_freq, memsize, highmemsize);
}

void __init prom_free_prom_memory(void)
{
}

void __init prom_putchar(char c)
{
	putDebugChar(c);
}

