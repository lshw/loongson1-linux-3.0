/*
 * Based on sound/mips/ls1x-ac97.c and sound/soc/loongson1/ls1x-ac97.c
 * which contain:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/delay.h>

#include <sound/ac97_codec.h>
#include <sound/ls1x-lib.h>

#include "ls1x-ac97-lib.h"

static DEFINE_MUTEX(car_mutex);

static struct ls1x_audio_state {
	void __iomem *base;
	int irq;
} ls1x_audio_state;

unsigned short ls1x_ac97_read(struct snd_ac97 *ac97, unsigned short reg)
{
	struct ls1x_audio_state *state = &ls1x_audio_state;
	unsigned short val = -1;
	int i = 1000;
	u32 data = 0;

	mutex_lock(&car_mutex);

	data |= CODEC_WR;
	data |= ((u32)reg << CODEC_ADR_OFFSET);
	writel(data, state->base + CRAC);

	/* now wait for the data */
	while (i-- > 0) {
		if ((readl(state->base + INTRAW) & CR_DONE) != 0)
			break;
		udelay(500);
	}
	if (i > 0) {
		readl(state->base + INT_CRCLR);
		val = readl(state->base + CRAC) & 0xffff;
	}
	else {
		printk("AC97 command read timeout\n");
	}

	mutex_unlock(&car_mutex);

	return val;
}
EXPORT_SYMBOL_GPL(ls1x_ac97_read);

void ls1x_ac97_write(struct snd_ac97 *ac97, unsigned short reg,
			unsigned short val)
{
	struct ls1x_audio_state *state = &ls1x_audio_state;
	int i = 1000;
	u32 data = 0;

	mutex_lock(&car_mutex);

	data &= ~(CODEC_WR);
	data |= ((u32)reg << CODEC_ADR_OFFSET) | ((u32)val << CODEC_DAT_OFFSET);
	writel(data, state->base + CRAC);

	while (i-- > 0) {
		if ((readl(state->base + INTRAW) & CW_DONE) != 0)
			break;
		udelay(500);
	}
	if (i > 0) {
		readl(state->base + INT_CWCLR);
	}
	else {
		printk("AC97 command write timeout\n");
	}

	mutex_unlock(&car_mutex);
}
EXPORT_SYMBOL_GPL(ls1x_ac97_write);

bool ls1x_ac97_try_warm_reset(struct snd_ac97 *ac97)
{
	struct ls1x_audio_state *state = &ls1x_audio_state;
	u32 x;

	x = readl(state->base + CSR);

	writel(x | 0x01, state->base + CSR);
	mdelay(500);
	writel(x & 0xfe, state->base + CSR);

	return true;
}
EXPORT_SYMBOL_GPL(ls1x_ac97_try_warm_reset);

bool ls1x_ac97_try_cold_reset(struct snd_ac97 *ac97)
{
	struct ls1x_audio_state *state = &ls1x_audio_state;
	u32 x;

	x = readl(state->base + CSR);

	writel(x | 0x01, state->base + CSR);
	mdelay(500);
	writel(x & 0xfe, state->base + CSR);

	return true;
}
EXPORT_SYMBOL_GPL(ls1x_ac97_try_cold_reset);


void ls1x_ac97_finish_reset(struct snd_ac97 *ac97)
{
	struct ls1x_audio_state *state = &ls1x_audio_state;
	u32 x;

	do {
		x = readl(state->base + CSR);
	} while (x & 0x02);
}
EXPORT_SYMBOL_GPL(ls1x_ac97_finish_reset);

static irqreturn_t ls1x_ac97_irq(int irq, void *dev_id)
{
	return IRQ_NONE;
}

#ifdef CONFIG_PM
int ls1x_ac97_hw_suspend(void)
{
	u32 x;

	x = readl(state->base + CSR);
	writel(x | 0x02, state->base + CSR);
	return 0;
}
EXPORT_SYMBOL_GPL(ls1x_ac97_hw_suspend);

int ls1x_ac97_hw_resume(void)
{
	u32 x;

	x = readl(state->base + CSR);
	writel(x & (~0x02), state->base + CSR);
	return 0;
}
EXPORT_SYMBOL_GPL(ls1x_ac97_hw_resume);
#endif

int ls1x_ac97_hw_probe(struct platform_device *pdev)
{
	struct ls1x_audio_state *state = &ls1x_audio_state;
	struct resource *res;
	int ret;

	res =  platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "cannot find IO resource\n");
		return -ENOENT;
	}

	if (!request_mem_region(res->start, resource_size(res), "ls1x-audio"))
		return -EBUSY;

	state->base = ioremap(res->start, resource_size(res));
	if (!state->base) {
		dev_err(&pdev->dev, "ls1x-audio - failed to map controller\n");
		ret = -ENOMEM;
		goto out1;
	}

	state->irq = platform_get_irq(pdev, 0);
	if (!state->irq) {
		ret = -ENXIO;
		goto out2;
	}

	/* reset ls1x ac97 controller */
/*	writel(0x01, state->base + CSR);
	writel(0x02, state->base + CSR);
	udelay(300);
	writel(0x01, state->base + CSR);
	mdelay(500);*/
	/* config channels */
	writel(0x69696969, state->base + OCC0);
	writel(0x69696969, state->base + OCC1);
	writel(0x69696969, state->base + OCC2);
	writel(0x69696969, state->base + ICC);
	/* disable irqreturn */
	readl(state->base + INT_CLR);
	readl(state->base + INT_OCCLR);
	readl(state->base + INT_ICCLR);
	readl(state->base + INT_CWCLR);
	readl(state->base + INT_CRCLR);
//	writel(0xffffffff, state->base + INTM);
	writel(0x00000000, state->base + INTRAW);
	writel(0x00000000, state->base + INTM);

	ret = request_irq(state->irq, ls1x_ac97_irq, IRQF_DISABLED, pdev->name, NULL);
	if (ret < 0)
		goto out2;

	return 0;

out2:
	iounmap(state->base);
out1:
	release_mem_region(res->start, resource_size(res));
	return ret;
}
EXPORT_SYMBOL_GPL(ls1x_ac97_hw_probe);

void ls1x_ac97_hw_remove(struct platform_device *pdev)
{
	struct ls1x_audio_state *state = &ls1x_audio_state;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	free_irq(state->irq, NULL);
	iounmap(state->base);
	release_mem_region(res->start, resource_size(res));
}
EXPORT_SYMBOL_GPL(ls1x_ac97_hw_remove);

MODULE_AUTHOR("Tang Haifeng <tanghaifeng-gz@loongson.cn>");
MODULE_DESCRIPTION("loongson1 sound library");
MODULE_LICENSE("GPL");

