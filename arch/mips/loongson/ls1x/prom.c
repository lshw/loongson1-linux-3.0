/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/serial_reg.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <video/ls1xfb.h>

#include <asm/bootinfo.h>
#include <asm/mach-loongson/ls1x/ls1b_board.h>
#include <prom.h>

#define DEFAULT_MEMSIZE			64	/* If no memsize provided */
#define DEFAULT_BUSCLOCK		133000000
#define DEFAULT_CPUCLOCK		266000000

#ifdef CONFIG_STMMAC_ETH
extern unsigned char *hwaddr;
char *tmp;
#endif

unsigned long cpu_clock_freq;
unsigned long ls1x_bus_clock;
EXPORT_SYMBOL(cpu_clock_freq);
EXPORT_SYMBOL(ls1x_bus_clock);

int prom_argc;
char **prom_argv, **prom_envp;
unsigned long memsize, highmemsize;

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

void __init prom_init(void)
{
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

	/* 需要复位一次USB控制器，且复位时间要足够长，否则启动时莫名其妙的死机 */
	#ifdef CONFIG_LS1A_MACH
	#define MUX_CTRL0 LS1X_MUX_CTRL0
	#define MUX_CTRL1 LS1X_MUX_CTRL1
	#elif CONFIG_LS1B_MACH
	#define MUX_CTRL0 LS1X_MUX_CTRL1
	#define MUX_CTRL1 LS1X_MUX_CTRL1
	#endif
	/* USB controller enable and reset */
	__raw_writel(__raw_readl(MUX_CTRL0) & (~USB_SHUT), MUX_CTRL0);
	__raw_writel(__raw_readl(MUX_CTRL1) & (~USB_RESET), MUX_CTRL1);
	mdelay(60);
	/* reset stop */
	__raw_writel(__raw_readl(MUX_CTRL1) | USB_RESET, MUX_CTRL1);

#ifdef CONFIG_STMMAC_ETH
	tmp = prom_getenv("ethaddr");
	if (tmp) {
		sscanf(tmp, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", 
			&hwaddr[0], &hwaddr[1], &hwaddr[2], &hwaddr[3], &hwaddr[4], &hwaddr[5]);
	}
#endif

#if defined(CONFIG_LS1B_MACH) && defined(CONFIG_FB_LOONGSON1)
	/* 只用于ls1b的lcd接口传vga接口时使用，如云终端，
	 因为使用vga时，pll通过计算难以得到合适的分频;给LS1X_CLK_PLL_FREQ和LS1X_CLK_PLL_DIV
	 寄存器一个固定的值 */
	{
	int i;
	int default_xres = 800;
	int default_yres = 600;
	int default_refresh = 75;
	struct ls1b_vga *input_vga;
	extern struct ls1b_vga ls1b_vga_modes[];
	char *strp, *options, *end;

	options = strstr(arcs_cmdline, "video=ls1bvga:");
	if (options) {
		
		options += 14;
		/* ls1bvga:1920x1080-16@60 */
		for (i=0; i<strlen(options); i++)
			if (isdigit(*(options+i)))
				break;	/* 查找options字串中第一个数字时i的值 */
		if (i < 4) {
			default_xres = simple_strtoul(options+i, &end, 10);
			default_yres = simple_strtoul(end+1, NULL, 10);
			/* refresh */
			strp = strchr((const char *)options, '@');
			if (strp) {
				default_refresh = simple_strtoul(strp+1, NULL, 0);
			}
			if ((default_xres<=0 || default_xres>1920) || 
				(default_yres<=0 || default_yres>1080)) {
				pr_info("Warning: Resolution is out of range."
					"MAX resolution is 1920x1080@60Hz\n");
				default_xres = 800;
				default_yres = 600;
			}
		}
		for (input_vga=ls1b_vga_modes; input_vga->ls1b_pll_freq !=0; ++input_vga) {
//			if((input_vga->xres == default_xres) && (input_vga->yres == default_yres) && 
//				(input_vga->refresh == default_refresh)) {
			if ((input_vga->xres == default_xres) && (input_vga->yres == default_yres)) {
				break;
			}
		}
		if (input_vga->ls1b_pll_freq) {
			u32 pll, ctrl;
			u32 x, divisor;

			writel(input_vga->ls1b_pll_freq, LS1X_CLK_PLL_FREQ);
			writel(input_vga->ls1b_pll_div, LS1X_CLK_PLL_DIV);
			/* 计算ddr频率，更新串口分频 */
			pll = input_vga->ls1b_pll_freq;
			ctrl = input_vga->ls1b_pll_div & DIV_DDR;
			divisor = (12 + (pll & 0x3f)) * APB_CLK / 2
					+ ((pll >> 8) & 0x3ff) * APB_CLK / 1024 / 2;
			divisor = divisor / (ctrl >> DIV_DDR_SHIFT);
			divisor = divisor / 2 / (16*115200);
			x = readb(PORT(UART_LCR));
			writeb(x | UART_LCR_DLAB, PORT(UART_LCR));
			writeb(divisor & 0xff, PORT(UART_DLL));
			writeb((divisor>>8) & 0xff, PORT(UART_DLM));
			writeb(x & ~UART_LCR_DLAB, PORT(UART_LCR));
		}
	}
	}
#endif
	
	pr_info("memsize=%ldMB, highmemsize=%ldMB\n", memsize, highmemsize);
}

void __init prom_free_prom_memory(void)
{
}

void __init prom_putchar(char c)
{
	int timeout;

	timeout = 1024;

	while (((readb(PORT(UART_LSR)) & UART_LSR_THRE) == 0)
			&& (timeout-- > 0))
		;

	writeb(c, PORT(UART_TX));
}

