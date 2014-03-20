/*
 *  Copyright (c) 2012 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/io.h>

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/gpio.h>

#include <loongson1.h>

#define REG_PWM_CNTR	0x00
#define REG_PWM_HRC		0x04
#define REG_PWM_LRC		0x08
#define REG_PWM_CTRL	0x0c

#if defined(CONFIG_LS1A_MACH)
#define LS_GPIO_PWM0	84
#define LS_GPIO_PWM1	85
#define LS_GPIO_PWM2	86
#define LS_GPIO_PWM3	87
#elif defined(CONFIG_LS1B_MACH)
#define LS_GPIO_PWM0	0
#define LS_GPIO_PWM1	1
#define LS_GPIO_PWM2	2
#define LS_GPIO_PWM3	3
#elif defined(CONFIG_LS1C_MACH)
#define LS_GPIO_PWM0	06
#define LS_GPIO_PWM1	92
#define LS_GPIO_PWM2	93
#define LS_GPIO_PWM3	94
#endif

void __iomem *ls1x_pwm_base;
static struct clk *ls1x_pwm_clk;

DEFINE_MUTEX(ls1x_pwm_mutex);

#if defined(CONFIG_LS1A_MACH) || defined(CONFIG_LS1B_MACH)
static struct pwm_device ls1x_pwm_list[] = {
	{ 0, LS_GPIO_PWM0, false },
	{ 1, LS_GPIO_PWM1, false },
	{ 2, LS_GPIO_PWM2, false },
	{ 3, LS_GPIO_PWM3, false },
};
#elif defined(CONFIG_LS1C_MACH)
extern struct pwm_device ls1x_pwm_list[];
#endif

struct pwm_device *pwm_request(int id, const char *label)
{
	int ret = 0;
	u32 x;
	struct pwm_device *pwm;

	if (id > 3 || !ls1x_pwm_clk)
		return ERR_PTR(-ENODEV);

	mutex_lock(&ls1x_pwm_mutex);

	pwm = &ls1x_pwm_list[id];
	if (pwm->used)
		ret = -EBUSY;
	else
		pwm->used = true;

	mutex_unlock(&ls1x_pwm_mutex);

	if (ret)
		return ERR_PTR(ret);

	/* 如果该引脚被设置为gpio需要释放该引脚 */
	gpio_request(pwm->gpio, "pwm");
	gpio_free(pwm->gpio);

	x = __raw_readl(LS1X_MUX_CTRL0);
	/* 设备复用模式为pwm */
#if defined(CONFIG_LS1A_MACH)
	if (id == 0 || id == 1) {
		x = x & (~NAND1_USE_PWM01) & (~GMAC0_USE_PWM01);
		__raw_writel(x, LS1X_MUX_CTRL0);
	} else {
		x = x & (~NAND_D45_USE_PWM23) & (~GMAC1_USE_PWM23);
		__raw_writel(x, LS1X_MUX_CTRL0);
	}
#elif defined(CONFIG_LS1B_MACH)
	if (id == 0 || id == 1) {
		x = x & (~UART0_USE_PWM01) & (~NAND3_USE_PWM01) & (~NAND2_USE_PWM01) & (~NAND1_USE_PWM01);
		__raw_writel(x, LS1X_MUX_CTRL0);
		x = __raw_readl(LS1X_MUX_CTRL1);
		x = x & (~SPI1_CS_USE_PWM01) & (~GMAC0_USE_PWM01);
		__raw_writel(x, LS1X_MUX_CTRL1);
	} else {
		x = x & (~UART0_USE_PWM23) & (~NAND3_USE_PWM23) & (~NAND2_USE_PWM23) & (~NAND1_USE_PWM23);
		__raw_writel(x, LS1X_MUX_CTRL0);
		x = __raw_readl(LS1X_MUX_CTRL1);
		x = x & (~GMAC1_USE_PWM23);
		__raw_writel(x, LS1X_MUX_CTRL1);
	}
#elif defined(CONFIG_LS1C_MACH)

#endif

	return pwm;
}
EXPORT_SYMBOL(pwm_request);

void pwm_free(struct pwm_device *pwm)
{
	pwm_disable(pwm);
	gpio_free(pwm->gpio);
	pwm->used = false;
}
EXPORT_SYMBOL(pwm_free);

int pwm_config(struct pwm_device *pwm, int duty_ns, int period_ns)
{
	unsigned long long tmp;
	unsigned long period, duty;
	unsigned int id = pwm->id;

	if (duty_ns < 0 || duty_ns > period_ns)
		return -EINVAL;

	tmp = (unsigned long long)clk_get_rate(ls1x_pwm_clk) * period_ns;
	do_div(tmp, 1000000000);
	period = tmp;

	tmp = (unsigned long long)period * duty_ns;
	do_div(tmp, period_ns);
	duty = period - tmp;

	/* 设置占空比 */
	writel(duty, ls1x_pwm_base + (id << 4) + REG_PWM_HRC);
	writel(period, ls1x_pwm_base + (id << 4) + REG_PWM_LRC);

	pwm_enable(pwm);

	return 0;
}
EXPORT_SYMBOL(pwm_config);

int pwm_enable(struct pwm_device *pwm)
{
	unsigned int id = pwm->id;

	writel(0x00, ls1x_pwm_base + (id << 4) + REG_PWM_CNTR);
	writel(0x01, ls1x_pwm_base + (id << 4) + REG_PWM_CTRL);
	return 0;
}
EXPORT_SYMBOL(pwm_enable);

void pwm_disable(struct pwm_device *pwm)
{
	unsigned int id = pwm->id;

	writel(0x09, ls1x_pwm_base + (id << 4) + REG_PWM_CTRL);
}
EXPORT_SYMBOL(pwm_disable);

static int __init ls1x_pwm_init(void)
{
	int ret = 0;

	ls1x_pwm_base = ioremap(LS1X_PWM0_BASE, 0xf);
	if (!ls1x_pwm_base)
		panic("Failed to ioremap timer registers");

	ls1x_pwm_clk = clk_get(NULL, "apb");

	if (IS_ERR(ls1x_pwm_clk)) {
		ret = PTR_ERR(ls1x_pwm_clk);
		ls1x_pwm_clk = NULL;
	}

	return ret;
}
subsys_initcall(ls1x_pwm_init);

