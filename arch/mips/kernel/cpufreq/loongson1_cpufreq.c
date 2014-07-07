/*
 * Copyright (c) 2012 Tang Haifeng <tanghaifeng-gz@loongson.cn> or <pengren.mcu@qq.com>
 * Based on loongson2_cpufreq.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/sched.h>	/* set_cpus_allowed() */
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <linux/io.h>
#include <linux/init.h>
#include <linux/irq.h>

#include <asm/clock.h>
#include <loongson1.h>

extern unsigned long cpu_clock_freq;
extern void loongson1_cpu_wait(void);
extern struct cpufreq_frequency_table loongson1_clockmod_table[];

static uint nowait;

static struct clk *cpuclk;

static void (*saved_cpu_wait) (void);

static int loongson1_cpu_freq_notifier(struct notifier_block *nb,
					unsigned long val, void *data);

static struct notifier_block loongson1_cpufreq_notifier_block = {
	.notifier_call = loongson1_cpu_freq_notifier
};

static int loongson1_cpu_freq_notifier(struct notifier_block *nb,
					unsigned long val, void *data)
{
	if (val == CPUFREQ_POSTCHANGE)
		current_cpu_data.udelay_val = loops_per_jiffy;

	return 0;
}

static unsigned int loongson1_cpufreq_get(unsigned int cpu)
{
	return clk_get_rate(cpuclk);
}

/*
 * Here we notify other drivers of the proposed change and the final change.
 */
static int loongson1_cpufreq_target(struct cpufreq_policy *policy,
				     unsigned int target_freq,
				     unsigned int relation)
{
	unsigned int cpu = policy->cpu;
	unsigned int newstate = 0;
	cpumask_t cpus_allowed;
	struct cpufreq_freqs freqs;
	unsigned int freq;

	if (!cpu_online(cpu))
		return -ENODEV;

	cpus_allowed = current->cpus_allowed;
	set_cpus_allowed_ptr(current, cpumask_of(cpu));

	if (cpufreq_frequency_table_target
	    (policy, &loongson1_clockmod_table[0], target_freq, relation,
	     &newstate))
		return -EINVAL;

#ifdef CONFIG_LS1B_MACH
/*	freq =
	    ((cpu_clock_freq / 1000) /
	     loongson1_clockmod_table[newstate].index) * 2;*/
	freq = loongson1_clockmod_table[newstate].frequency;
#else
	freq = APB_CLK * (loongson1_clockmod_table[newstate].index + 2);
#endif
	if (freq < policy->min || freq > policy->max)
		return -EINVAL;

	pr_debug("cpufreq: requested frequency %u Hz\n", target_freq);

	freqs.cpu = cpu;
	freqs.old = loongson1_cpufreq_get(cpu);
	freqs.new = freq;
	freqs.flags = 0;

	if (freqs.new == freqs.old)
		return 0;

	/* notifiers */
	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	set_cpus_allowed_ptr(current, &cpus_allowed);

	/* setting the cpu frequency */
	clk_set_rate(cpuclk, freq);

	/* notifiers */
	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	pr_debug("cpufreq: set frequency %u Hz\n", freq);

	return 0;
}

static int loongson1_cpufreq_cpu_init(struct cpufreq_policy *policy)
{
	int i;
	int cpu_div;
	struct clk *pllclk;

	if (!cpu_online(policy->cpu))
		return -ENODEV;

	pllclk = clk_get(NULL, "pll");
	if (IS_ERR(pllclk)) {
		printk(KERN_ERR "pllfreq: couldn't get PLL clk\n");
		return PTR_ERR(pllclk);
	}

	cpuclk = clk_get(NULL, "cpu");
	if (IS_ERR(cpuclk)) {
		printk(KERN_ERR "cpufreq: couldn't get CPU clk\n");
		return PTR_ERR(cpuclk);
	}

	cpu_div = (__raw_readl(LS1X_CLK_PLL_DIV) >> 20) & 0xf;

#if defined(CONFIG_LS1B_MACH)
	/* clock table init */
	for (i = 0;
	     (loongson1_clockmod_table[i].frequency != CPUFREQ_TABLE_END);
	     i++)
		loongson1_clockmod_table[i].frequency = clk_get_rate(pllclk) / (cpu_div + i);
#elif defined(CONFIG_LS1A_MACH)
	for (i = 2; (loongson1_clockmod_table[i].frequency != CPUFREQ_TABLE_END); i++)
		loongson1_clockmod_table[i].frequency = APB_CLK * (2 + i);
#endif

	policy->cur = loongson1_cpufreq_get(policy->cpu);

	cpufreq_frequency_table_get_attr(&loongson1_clockmod_table[0],
					 policy->cpu);

	return cpufreq_frequency_table_cpuinfo(policy,
					    &loongson1_clockmod_table[0]);
}

static int loongson1_cpufreq_verify(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy,
					      &loongson1_clockmod_table[0]);
}

static int loongson1_cpufreq_exit(struct cpufreq_policy *policy)
{
	clk_put(cpuclk);
	return 0;
}

static struct freq_attr *loongson1_table_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver loongson1_cpufreq_driver = {
	.owner = THIS_MODULE,
	.name = "loongson1",
	.init = loongson1_cpufreq_cpu_init,
	.verify = loongson1_cpufreq_verify,
	.target = loongson1_cpufreq_target,
	.get = loongson1_cpufreq_get,
	.exit = loongson1_cpufreq_exit,
	.attr = loongson1_table_attr,
};

static struct platform_device_id platform_device_ids[] = {
	{
		.name = "loongson1_cpufreq",
	},
	{}
};

MODULE_DEVICE_TABLE(platform, platform_device_ids);

static struct platform_driver platform_driver = {
	.driver = {
		.name = "loongson1_cpufreq",
		.owner = THIS_MODULE,
	},
	.id_table = platform_device_ids,
};

static int __init cpufreq_init(void)
{
	int ret;

	/* Register platform stuff */
	ret = platform_driver_register(&platform_driver);
	if (ret)
		return ret;

	pr_info("cpufreq: Loongson 1A/1B CPU frequency driver.\n");

	cpufreq_register_notifier(&loongson1_cpufreq_notifier_block,
				  CPUFREQ_TRANSITION_NOTIFIER);

	ret = cpufreq_register_driver(&loongson1_cpufreq_driver);

	if (!ret && !nowait) {
		saved_cpu_wait = cpu_wait;
		cpu_wait = loongson1_cpu_wait;
	}

	return ret;
}

static void __exit cpufreq_exit(void)
{
	if (!nowait && saved_cpu_wait)
		cpu_wait = saved_cpu_wait;
	cpufreq_unregister_driver(&loongson1_cpufreq_driver);
	cpufreq_unregister_notifier(&loongson1_cpufreq_notifier_block,
				    CPUFREQ_TRANSITION_NOTIFIER);

	platform_driver_unregister(&platform_driver);
}

module_init(cpufreq_init);
module_exit(cpufreq_exit);

module_param(nowait, uint, 0644);
MODULE_PARM_DESC(nowait, "Disable Loongson1 specific wait");

MODULE_AUTHOR("www.loongson-gz.cn");
MODULE_DESCRIPTION("cpufreq driver for loongson1F");
MODULE_LICENSE("GPL");
