/*
 * SoC audio driver for loongson1
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>

#include "ls1x-i2s.h"

/*-------------------------  I2S PART  ---------------------------*/

static int ls1x_i2s_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret;

	/* WM8731 has its own 12MHz crystal */
//	snd_soc_dai_set_sysclk(codec_dai, WM8731_SYSCLK_XTAL,
//				12000000, SND_SOC_CLOCK_IN);

	/* codec is bitclock and lrclk master */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		goto out;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		goto out;

	ret = 0;
out:
	return ret;
}

static struct snd_soc_ops ls1x_i2s_uda1342_ops = {
	.startup	= ls1x_i2s_startup,
};

static struct snd_soc_dai_link ls1x_dai[] = {
	{
		.name = "ls1x",
		.stream_name = "ls1x<->i2s",
		.cpu_dai_name = "ls1x-i2s",
		.codec_dai_name = "uda134x-hifi",
		.platform_name = "loongson1-pcm-audio",
		.codec_name = "uda1342-codec.0-001a",
//		.ops = &ls1a_ac97_hifi_dai_ops,
//		.init = uda134x_init,
	},
/*	{
		.name = "AC97 Aux",
		.stream_name = "AC97 Aux",
		.cpu_dai_name = "pxa2xx-ac97-aux",
		.codec_dai_name ="wm9712-aux",
		.platform_name = "pxa-pcm-audio",
		.codec_name = "wm9712-codec",
	},*/
};

static struct snd_soc_card ls1x = {
	.name = "LS1X",
	.dai_link = ls1x_dai,
	.num_links = ARRAY_SIZE(ls1x_dai),
};

/*-------------------------  COMMON PART  ---------------------------*/

static struct platform_device *ls1x_snd_device;

static int __init ls1x_init(void)
{
	int ret;

/*	if (!(machine_is_ls1x() || machine_is_exeda()
	      || machine_is_cm_x300()))
		return -ENODEV;*/

	ls1x_snd_device = platform_device_alloc("soc-audio", -1);
	if (!ls1x_snd_device)
		return -ENOMEM;

	platform_set_drvdata(ls1x_snd_device, &ls1x);
	ret = platform_device_add(ls1x_snd_device);

	if (ret)
		platform_device_put(ls1x_snd_device);

	return ret;
}

static void __exit ls1x_exit(void)
{
	platform_device_unregister(ls1x_snd_device);
}

module_init(ls1x_init);
module_exit(ls1x_exit);

/* Module information */
MODULE_AUTHOR("Tang Haifeng <tanghaifeng-gz@loongson.cn>");
MODULE_DESCRIPTION("ALSA SoC ls1x board Audio support");
MODULE_LICENSE("GPL");
