/*
 * linux/sound/ls1a-ac97.c -- AC97 support for the Loongson 1A chip.
 *
 * Author:	Nicolas Pitre
 * Created:	Dec 02, 2004
 * Copyright:	MontaVista Software Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <sound/core.h>
#include <sound/ac97_codec.h>
#include <sound/soc.h>

#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/ls1x-lib.h>

#include <linux/interrupt.h>
//#include "ls1x-pcm.h"
#include "ls1x-i2s.h"

struct ls1x_i2s_port {
	int master;
	u32 iis_control;
};
static struct ls1x_i2s_port ls1x_i2s;
static struct clk *clk_i2s;
static int clk_ena = 0;

static void ls1a_ac97_cold_reset(struct snd_ac97 *ac97)
{
	udelay(5000);

//	ls1a_i2c_write( 0, 0);
	udelay(5000);		//FIXME,This is very very necessary,Zhuo Qixiang!
	printk(KERN_ALERT "+++++++++ Here Enter into %s.+++++++++\n", __func__);
}


static struct ls1x_pcm_dma_params ls1x_i2s_pcm_stereo_out = {
	.name = "I2S PCM Stereo out",
	.dev_addr = 0x1fe60010,
};

static struct ls1x_pcm_dma_params ls1x_i2s_pcm_stereo_in = {
	.name = "I2S PCM Stereo in",
	.dev_addr = 0x1fe6000c,
};

static int ls1x_i2s_set_dai_fmt(struct snd_soc_dai *cpu_dai,
		unsigned int fmt)
{
	int ret;

	ret = -EINVAL;

	printk("++++fmt %x\n", fmt);

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
//		ct |= PSC_I2SCFG_XM;	/* enable I2S mode */
		break;
	case SND_SOC_DAIFMT_MSB:
		ls1x_i2s.iis_control |= CONTROL_MSB_LSB;
		break;
	case SND_SOC_DAIFMT_LSB:
		ls1x_i2s.iis_control &= ~CONTROL_MSB_LSB;	/* LSB (right-) justified */
		break;
	default:
		goto out;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:	/* CODEC master */
		ls1x_i2s.iis_control &= ~CONTROL_MASTER;	/* LS1X I2S slave mode */
		break;
	case SND_SOC_DAIFMT_CBS_CFS:	/* CODEC slave */
		ls1x_i2s.iis_control |= CONTROL_MASTER;	/* LS1X I2S Master mode */
		break;
	default:
		goto out;
	}

	ret = 0;
out:
	return ret;
}

static int ls1x_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct ls1x_pcm_dma_params *dma_data;
	unsigned char sck_ratio;
	unsigned char bck_ratio;

	//采样位数;设置声道对于I2S 如何设置???
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dma_data = &ls1x_i2s_pcm_stereo_out;
		writel(readl(LS1X_IIS_BASE(8)) | 0x1080, LS1X_IIS_BASE(8));
		sck_ratio = (clk_get_rate(clk_i2s)/(params_rate(params)*2*2*16)) - 1;
		bck_ratio = (clk_get_rate(clk_i2s)/(params_rate(params)*2*512)) - 1;
//		sck_ratio = 0xf;
//		bck_ratio = 0x1;
	}
	else {
		dma_data = &ls1x_i2s_pcm_stereo_in;
		writel(readl(LS1X_IIS_BASE(8)) | 0x2800, LS1X_IIS_BASE(8));
//		sck_ratio = (clk_get_rate(clk_i2s)/(params_rate(params)*2*2*16)) - 1;
		bck_ratio = (clk_get_rate(clk_i2s)/(params_rate(params)*2*512)) - 1;
		sck_ratio = 0xf;
//		bck_ratio = 0x1;
	}
	writel((16<<24) | (16<<16) | (sck_ratio<<8) | (bck_ratio<<0), LS1X_IIS_BASE(4));

	snd_soc_dai_set_dma_data(dai, substream, dma_data);

	switch (params_rate(params)) {
	case 8000:
		break;
	case 11025:
		break;
	case 16000:
		break;
	case 22050:
		break;
	case 44100:
		break;
	case 48000:
		break;
	case 96000: /* not in manual and possibly slightly inaccurate */
		break;
	}

//	#define SAMP_RATE 44100
//	sck_ratio = (clk_get_rate(clk_i2s)/(params_rate(params)*2*2*16)) - 1;
//	bck_ratio = (clk_get_rate(clk_i2s)/(params_rate(params)*2*512)) - 1;
//	sck_ratio = 0xf;
//	bck_ratio = 0x1;

//	writel((16<<24) | (16<<16) | (sck_ratio<<8) | (bck_ratio<<0), LS1X_IIS_BASE(4));

	printk("++++++++++clk_i2s=%ld\n", clk_get_rate(clk_i2s));
	printk("+++++++++++++ %d %x %x\n\n", params_rate(params), sck_ratio, bck_ratio);

	return 0;
}

static int ls1x_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
			      struct snd_soc_dai *dai)
{
//	struct au1xpsc_audio_data *pscdata = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
//		ret = au1xpsc_i2s_start(pscdata, stype);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
//		ret = au1xpsc_i2s_stop(pscdata, stype);
		break;
	default:
		ret = -EINVAL;
	}
	printk("++++trigger %x ret=%d\n\n", cmd, ret);
	return ret;
}


#ifdef CONFIG_PM
static int ls1x_i2s_suspend(struct snd_soc_dai *dai)
{
	return 0;
}

static int ls1x_i2s_resume(struct snd_soc_dai *dai)
{
	return 0;
}

#else
#define ls1x_i2s_suspend	NULL
#define ls1x_i2s_resume	NULL
#endif

static int ls1x_i2s_probe(struct snd_soc_dai *dai)
{
	clk_i2s = clk_get(dai->dev, "apb");
	if (IS_ERR(clk_i2s))
		return PTR_ERR(clk_i2s);

	writel(0xc220, LS1X_IIS_BASE(8));
	writel(0x0110, LS1X_IIS_BASE(0));

	return 0;
}

static int ls1x_i2s_remove(struct snd_soc_dai *dai)
{
	return 0;
}

#define LS1X_I2S_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | \
		SNDRV_PCM_RATE_48000)

#define LS1X_I2S_FMTS (SNDRV_PCM_FMTBIT_S8 |\
	SNDRV_PCM_FMTBIT_S16_LE |\
	SNDRV_PCM_FMTBIT_S16_BE |\
	SNDRV_PCM_FMTBIT_S20_3LE |\
	SNDRV_PCM_FMTBIT_S20_3BE |\
	SNDRV_PCM_FMTBIT_S24_3LE |\
	SNDRV_PCM_FMTBIT_S24_3BE |\
	SNDRV_PCM_FMTBIT_S32_LE |\
	SNDRV_PCM_FMTBIT_U16_LE	|\
	SNDRV_PCM_FMTBIT_S32_BE)

static struct snd_soc_dai_ops ls1x_i2s_dai_ops = {
//	.startup	= ls1x_i2s_startup,
//	.shutdown	= ls1x_i2s_shutdown,
	.trigger	= ls1x_i2s_trigger,
	.hw_params	= ls1x_i2s_hw_params,
	.set_fmt	= ls1x_i2s_set_dai_fmt,
//	.set_sysclk	= ls1x_i2s_set_dai_sysclk,
};

struct snd_soc_dai_driver ls1x_i2s_dai = {
	.probe = ls1x_i2s_probe,
	.remove = ls1x_i2s_remove,
	.suspend = ls1x_i2s_suspend,
	.resume = ls1x_i2s_resume,
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = LS1X_I2S_RATES,
		.formats = LS1X_I2S_FMTS,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = LS1X_I2S_RATES,
		.formats = LS1X_I2S_FMTS,
	},
	.ops = &ls1x_i2s_dai_ops,
};

static int ls1x_i2s_drv_probe(struct platform_device *pdev)
{
	return snd_soc_register_dai(&pdev->dev, &ls1x_i2s_dai);
}

static int __devexit ls1x_i2s_drv_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static struct platform_driver ls1x_i2s_driver = {
	.probe = ls1x_i2s_drv_probe,
	.remove = __devexit_p(ls1x_i2s_drv_remove),

	.driver = {
		.name = "ls1x-i2s",
		.owner = THIS_MODULE,
	},
};

static int __init ls1x_i2s_init(void)
{
	return platform_driver_register(&ls1x_i2s_driver);
}

static void __exit ls1x_i2s_exit(void)
{
	platform_driver_unregister(&ls1x_i2s_driver);
}

module_init(ls1x_i2s_init);
module_exit(ls1x_i2s_exit);

/* Module information */
MODULE_AUTHOR("Tang Haifeng <tanghaifeng-gz@loongson.cn>");
MODULE_DESCRIPTION("ls1x I2S SoC Interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ls1x-i2s");
