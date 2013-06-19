/*
 * Copyright (c) 2011 Zhang, Keguang <keguang.zhang@gmail.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <asm/time.h>

#include <asm/mach-loongson/ls1x/clock.h>
#include <asm/mach-loongson/ls1x/ls1b_board.h>
#include <asm/mach-loongson/ls1x/ls1x_time.h>

static LIST_HEAD(clocks);
static DEFINE_MUTEX(clocks_mutex);
static DEFINE_SPINLOCK(clock_lock);

#ifdef	CONFIG_LS1A_MACH
extern unsigned long cpu_clock_freq;
extern unsigned long ls1x_bus_clock;
#endif

/* Minimum CLK support */
enum {
	DC_ZERO, DC_25PT = 2, DC_37PT, DC_50PT, DC_62PT, DC_75PT,
	DC_87PT, DC_DISABLE, DC_RESV
};

struct cpufreq_frequency_table loongson1_clockmod_table[] = {
	{DC_RESV, CPUFREQ_ENTRY_INVALID},
	{DC_ZERO, CPUFREQ_ENTRY_INVALID},
	{DC_25PT, 0},
	{DC_37PT, 0},
	{DC_50PT, 0},
	{DC_62PT, 0},
	{DC_75PT, 0},
	{DC_87PT, 0},
	{DC_DISABLE, 0},
	{DC_RESV, CPUFREQ_TABLE_END},
};
EXPORT_SYMBOL_GPL(loongson1_clockmod_table);

struct clk *clk_get(struct device *dev, const char *name)
{
	struct clk *c;
	struct clk *ret = NULL;

	mutex_lock(&clocks_mutex);
	list_for_each_entry(c, &clocks, node) {
		if (!strcmp(c->name, name)) {
			ret = c;
			break;
		}
	}
	mutex_unlock(&clocks_mutex);

	return ret;
}
EXPORT_SYMBOL(clk_get);

static void propagate_rate(struct clk *clk)
{
	struct clk *clkp;

	list_for_each_entry(clkp, &clocks, node) {
		if (likely(clkp->parent != clk))
			continue;
		if (likely(clkp->ops && clkp->ops->recalc))
			clkp->ops->recalc(clkp);
		if (unlikely(clkp->flags & CLK_RATE_PROPAGATES))
			propagate_rate(clkp);
	}
}

int clk_enable(struct clk *clk)
{
	return 0;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
}
EXPORT_SYMBOL(clk_disable);

unsigned long clk_get_rate(struct clk *clk)
{
	return clk->rate;
}
EXPORT_SYMBOL(clk_get_rate);

void clk_put(struct clk *clk)
{
}
EXPORT_SYMBOL(clk_put);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	return clk_set_rate_ex(clk, rate, 0);
}
EXPORT_SYMBOL_GPL(clk_set_rate);

int clk_set_rate_ex(struct clk *clk, unsigned long rate, int algo_id)
{
	int ret = 0;
	int regval __maybe_unused;
	int i;

	if (likely(clk->ops && clk->ops->set_rate)) {
		unsigned long flags;

		spin_lock_irqsave(&clock_lock, flags);
		ret = clk->ops->set_rate(clk, rate, algo_id);
		spin_unlock_irqrestore(&clock_lock, flags);
	}

	if (unlikely(clk->flags & CLK_RATE_PROPAGATES))
		propagate_rate(clk);

	for (i = 0; loongson1_clockmod_table[i].frequency != CPUFREQ_TABLE_END;
	     i++) {
		if (loongson1_clockmod_table[i].frequency ==
		    CPUFREQ_ENTRY_INVALID)
			continue;
		if (rate == loongson1_clockmod_table[i].frequency)
			break;
	}
	if (rate != loongson1_clockmod_table[i].frequency)
		return -ENOTSUPP;

	clk->rate = rate;

#if defined(CONFIG_LS1B_MACH)
	regval = __raw_readl(LS1X_CLK_PLL_DIV);
	regval |= 0x00000300;	//cpu_bypass 置1
	regval &= ~0x0000003;	//cpu_rst 置0
	regval &= ~(0x1f<<20);	//cpu_div 清零
	regval |= (loongson1_clockmod_table[i].index) << 20;
	__raw_writel(regval, LS1X_CLK_PLL_DIV);
	regval &= ~0x00000100;	//cpu_bypass 置0
	__raw_writel(regval, LS1X_CLK_PLL_DIV);
#elif defined(CONFIG_LS1A_MACH)
	regval = (ls1x_bus_clock / APB_CLK - 3) << 8;
	regval |= 0x8888;
	regval |= (loongson1_clockmod_table[i].index + 2);
	__raw_writel(regval, LS1X_CLK_PLL_DIV);
#endif

	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_rate_ex);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (likely(clk->ops && clk->ops->round_rate)) {
		unsigned long flags, rounded;

		spin_lock_irqsave(&clock_lock, flags);
		rounded = clk->ops->round_rate(clk, rate);
		spin_unlock_irqrestore(&clock_lock, flags);

		return rounded;
	}

	return rate;
}
EXPORT_SYMBOL_GPL(clk_round_rate);

/*
 * This is the simple version of Loongson-2 wait, Maybe we need do this in
 * interrupt disabled content
 */

DEFINE_SPINLOCK(loongson1_wait_lock);
void loongson1_cpu_wait(void)
{
//	u32 cpu_freq;
	unsigned long flags;

	spin_lock_irqsave(&loongson1_wait_lock, flags);
//	cpu_freq = LOONGSON_CHIPCFG0;
//	LOONGSON_CHIPCFG0 &= ~0x7;	/* Put CPU into wait mode */
//	LOONGSON_CHIPCFG0 = cpu_freq;	/* Restore CPU state */
	spin_unlock_irqrestore(&loongson1_wait_lock, flags);
}
EXPORT_SYMBOL_GPL(loongson1_cpu_wait);

static void pll_clk_init(struct clk *clk)
{
	u32 pll;

	pll = __raw_readl(LS1X_CLK_PLL_FREQ);
#if defined(CONFIG_LS1C_MACH)
	clk->rate = (((pll >> 8) & 0xff) + ((pll >> 16) & 0xff)) * APB_CLK / 4;
#else
	clk->rate = (12 + (pll & 0x3f)) * APB_CLK / 2
			+ ((pll >> 8) & 0x3ff) * APB_CLK / 1024 / 2;
#endif
}

static void cpu_clk_init(struct clk *clk)
{
	u32 pll, ctrl;

	pll = clk_get_rate(clk->parent);
	ctrl = __raw_readl(LS1X_CLK_PLL_DIV);
#if defined(CONFIG_LS1A_MACH)
	/* 由于目前loongson 1A CPU读取0xbfe78030 PLL寄存器有问题，
	   所以CPU的频率是通过PMON传进来的 */
	clk->rate = cpu_clock_freq;
#elif defined(CONFIG_LS1B_MACH)
	clk->rate = pll / ((ctrl & DIV_CPU) >> DIV_CPU_SHIFT);
#else
	if (ctrl & DIV_CPU_SEL) {
		if(ctrl & DIV_CPU_EN) {
			clk->rate = pll / ((ctrl & DIV_CPU) >> DIV_CPU_SHIFT);
		} else {
			clk->rate = pll / 2;
		}
	} else {
		clk->rate = APB_CLK;
	}
#endif
	pr_info("cpuclock=%ldHz\n", clk->rate);
}

static void ddr_clk_init(struct clk *clk)
{
	u32 pll, ctrl;

	pll = clk_get_rate(clk->parent);
	ctrl = __raw_readl(LS1X_CLK_PLL_DIV);
#if defined(CONFIG_LS1A_MACH)
	/* 由于目前loongson 1A CPU读取0xbfe78030 PLL寄存器有问题，
	   所以BUS(DDR)的频率是通过PMON传进来的 */
	clk->rate = ls1x_bus_clock;
#elif defined(CONFIG_LS1B_MACH)
	clk->rate = pll / ((ctrl & DIV_DDR) >> DIV_DDR_SHIFT);
#else
	ctrl = __raw_readl(LS1X_CLK_PLL_FREQ) & 0x3;
	switch	 (ctrl) {
		case 0:
			clk->rate = pll / 2;
		break;
		case 1:
			clk->rate = pll / 4;
		break;
		case 2:
		case 3:
			clk->rate = pll / 3;
		break;
	}
#endif
	pr_info("busclock=%ldHz\n", clk->rate);
}

static void apb_clk_init(struct clk *clk)
{
	u32 pll;

	pll = clk_get_rate(clk->parent);
#if defined(CONFIG_LS1C_MACH)
	clk->rate = pll;
#else
	clk->rate = pll / 2;
#endif
}

static void dc_clk_init(struct clk *clk)
{
	u32 pll, ctrl;

	pll = clk_get_rate(clk->parent);
	ctrl = __raw_readl(LS1X_CLK_PLL_DIV) & DIV_DC;
	clk->rate = pll / (ctrl >> DIV_DC_SHIFT);
}

static struct clk_ops pll_clk_ops = {
	.init	= pll_clk_init,
};

static struct clk_ops cpu_clk_ops = {
	.init	= cpu_clk_init,
};

static struct clk_ops ddr_clk_ops = {
	.init	= ddr_clk_init,
};

static struct clk_ops apb_clk_ops = {
	.init	= apb_clk_init,
};

static struct clk_ops dc_clk_ops = {
	.init	= dc_clk_init,
};

static struct clk pll_clk = {
	.name	= "pll",
	.ops	= &pll_clk_ops,
};

static struct clk cpu_clk = {
	.name	= "cpu",
	.flags	= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.parent = &pll_clk,
	.ops	= &cpu_clk_ops,
};

static struct clk ddr_clk = {
	.name	= "ddr",
#if defined(CONFIG_LS1C_MACH)
	.parent = &cpu_clk,
#else
	.parent = &pll_clk,
#endif
	.ops	= &ddr_clk_ops,
};

static struct clk apb_clk = {
	.name	= "apb",
	.parent = &ddr_clk,
	.ops	= &apb_clk_ops,
};

static struct clk dc_clk = {
	.name	= "dc",
	.parent = &pll_clk,
	.ops	= &dc_clk_ops,
};

int clk_register(struct clk *clk)
{
	mutex_lock(&clocks_mutex);
	list_add(&clk->node, &clocks);
	if (clk->ops->init)
		clk->ops->init(clk);
	mutex_unlock(&clocks_mutex);

	return 0;
}
EXPORT_SYMBOL(clk_register);

static struct clk *ls1x_clks[] = {
	&pll_clk,
	&cpu_clk,
	&ddr_clk,
	&apb_clk,
	&dc_clk,
};

int __init ls1x_clock_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ls1x_clks); i++)
		clk_register(ls1x_clks[i]);

	return 0;
}

void __init plat_time_init(void)
{
	struct clk *clk;

	/* Initialize LS1X clocks */
	ls1x_clock_init();

	/* setup mips r4k timer */
	clk = clk_get(NULL, "cpu");
	if (IS_ERR(clk))
		panic("unable to get dc clock, err=%ld", PTR_ERR(clk));

	mips_hpt_frequency = clk_get_rate(clk) / 2;
	setup_ls1x_timer();
}
