/*
 * linux/sound/ls1x-ac97.c -- AC97 support for the Loongson1 chip.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <sound/core.h>
#include <sound/ac97_codec.h>
#include <sound/soc.h>
#include <sound/ls1x-lib.h>

static void ls1x_ac97_warm_reset(struct snd_ac97 *ac97)
{
	ls1x_ac97_try_warm_reset(ac97);

	ls1x_ac97_finish_reset(ac97);
}

static void ls1x_ac97_cold_reset(struct snd_ac97 *ac97)
{
	ls1x_ac97_try_cold_reset(ac97);

	ls1x_ac97_finish_reset(ac97);
}

struct snd_ac97_bus_ops soc_ac97_ops = {
	.read	= ls1x_ac97_read,
	.write	= ls1x_ac97_write,
	.warm_reset	= ls1x_ac97_warm_reset,
	.reset	= ls1x_ac97_cold_reset,
};

static struct ls1x_pcm_dma_params ls1x_ac97_pcm_stereo_out = {
	.name			= "AC97 PCM Stereo out",
	.dev_addr		= 0xdfe72420,
};

static struct ls1x_pcm_dma_params ls1x_ac97_pcm_stereo_in = {
	.name			= "AC97 PCM Stereo in",
	.dev_addr		= 0x5fe74c4c,
};

#ifdef CONFIG_PM
static int ls1x_ac97_suspend(struct snd_soc_dai *dai)
{
	return ls1x_ac97_hw_suspend();
}

static int ls1x_ac97_resume(struct snd_soc_dai *dai)
{
	return ls1x_ac97_hw_resume();
}

#else
#define ls1x_ac97_suspend	NULL
#define ls1x_ac97_resume	NULL
#endif

static int ls1x_ac97_probe(struct snd_soc_dai *dai)
{
	return ls1x_ac97_hw_probe(to_platform_device(dai->dev));
}

static int ls1x_ac97_remove(struct snd_soc_dai *dai)
{
	ls1x_ac97_hw_remove(to_platform_device(dai->dev));
	return 0;
}

static int ls1x_ac97_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *cpu_dai)
{
	struct ls1x_pcm_dma_params *dma_data;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data = &ls1x_ac97_pcm_stereo_out;
	else
		dma_data = &ls1x_ac97_pcm_stereo_in;

	snd_soc_dai_set_dma_data(cpu_dai, substream, dma_data);

	return 0;
}


#define LS1X_AC97_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | \
		SNDRV_PCM_RATE_48000)

static struct snd_soc_dai_ops ls1x_ac97_hifi_dai_ops = {
	.hw_params	= ls1x_ac97_hw_params,
};

/*
 * There is only 1 physical AC97 interface for ls1x, but it
 * has extra fifo's that can be used for aux DACs and ADCs.
 */
static struct snd_soc_dai_driver ls1x_ac97_dai[] = {
{
	.name = "ls1x-ac97",
	.ac97_control = 1,
	.probe = ls1x_ac97_probe,
	.remove = ls1x_ac97_remove,
	.suspend = ls1x_ac97_suspend,
	.resume = ls1x_ac97_resume,
	.playback = {
		.stream_name = "AC97 Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = LS1X_AC97_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.capture = {
		.stream_name = "AC97 Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = LS1X_AC97_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.ops = &ls1x_ac97_hifi_dai_ops,
},
};

EXPORT_SYMBOL_GPL(soc_ac97_ops);

static __devinit int ls1x_ac97_dev_probe(struct platform_device *pdev)
{
	if (pdev->id != -1) {
		dev_err(&pdev->dev, "LS1x has only one AC97 port.\n");
		return -ENXIO;
	}

	/* Punt most of the init to the SoC probe; we may need the machine
	 * driver to do interesting things with the clocking to get us up
	 * and running.
	 */
	return snd_soc_register_dais(&pdev->dev, ls1x_ac97_dai,
			ARRAY_SIZE(ls1x_ac97_dai));
}

static int __devexit ls1x_ac97_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dais(&pdev->dev, ARRAY_SIZE(ls1x_ac97_dai));
	return 0;
}

static struct platform_driver ls1x_ac97_driver = {
	.probe		= ls1x_ac97_dev_probe,
	.remove		= __devexit_p(ls1x_ac97_dev_remove),
	.driver		= {
		.name	= "ls1x-ac97",
		.owner	= THIS_MODULE,
	},
};

static int __init ls1x_ac97_init(void)
{
	return platform_driver_register(&ls1x_ac97_driver);
}
module_init(ls1x_ac97_init);

static void __exit ls1x_ac97_exit(void)
{
	platform_driver_unregister(&ls1x_ac97_driver);
}
module_exit(ls1x_ac97_exit);

MODULE_AUTHOR("Tang Haifeng <tanghaifeng-gz@loongson.cn>");
MODULE_DESCRIPTION("AC97 driver for the Loongson1 chip");
MODULE_LICENSE("GPL");
