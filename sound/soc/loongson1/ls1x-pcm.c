/*
 * linux/sound/arm/ls1x-pcm.c -- ALSA PCM interface for the Loongson1 chip
 *
 * 
 * 
 * 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <sound/core.h>
#include <sound/soc.h>
#include <sound/ls1x-lib.h>

#include "../../mips/ls1x-pcm.h"

struct dma_channel {
//	char *name;
//	pxa_dma_prio prio;
//	void (*irq_handler)(int, void *);
	void *data;
//	spinlock_t lock;
};

static struct dma_channel dma_channels;
static int num_dma_channels;

static irqreturn_t ls1x_dmar_irq(int irq, void *dev_id)
{
	ls1x_pcm_dma_irq(irq, dma_channels.data);

	return IRQ_HANDLED;
}

static irqreturn_t ls1x_dmaw_irq(int irq, void *dev_id)
{
	ls1x_pcm_dma_irq(irq, dma_channels.data);

	return IRQ_HANDLED;
}

static int ls1x_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ls1x_runtime_data *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct ls1x_pcm_dma_params *dma;

	dma = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);

	/* return if this is a bufferless transfer e.g.
	 * codec <--> BT codec or GSM modem -- lg FIXME */
	if (!dma)
		return 0;

	/* this may get called several times by oss emulation
	 * with different params */
	if (prtd->params != dma || prtd->params == NULL) {
		prtd->params = dma;
		dma_channels.data = substream;
	}

	return __ls1x_pcm_hw_params(substream, params);
}

static int ls1x_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct ls1x_runtime_data *prtd = substream->runtime->private_data;

	__ls1x_pcm_hw_free(substream);

	return 0;
}

static struct snd_pcm_ops ls1x_pcm_ops = {
	.open		= __ls1x_pcm_open,
	.close		= __ls1x_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= ls1x_pcm_hw_params,
	.hw_free	= ls1x_pcm_hw_free,
	.prepare	= __ls1x_pcm_prepare,
	.trigger	= ls1x_pcm_trigger,
	.pointer	= ls1x_pcm_pointer,
//	.mmap		= ls1x_pcm_mmap,
};

static u64 ls1x_pcm_dmamask = DMA_BIT_MASK(32);

static int ls1x_soc_pcm_new(struct snd_card *card, struct snd_soc_dai *dai,
	struct snd_pcm *pcm)
{
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &ls1x_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = ls1x_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = ls1x_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}
 out:
	return ret;
}

static struct snd_soc_platform_driver ls1x_soc_platform = {
	.ops 	= &ls1x_pcm_ops,
	.pcm_new	= ls1x_soc_pcm_new,
	.pcm_free	= ls1x_pcm_free_dma_buffers,
};

static int __devinit ls1x_soc_platform_probe(struct platform_device *pdev)
{
	int ret;

	ret = request_irq(LS1X_DMA1_IRQ, ls1x_dmar_irq, IRQF_DISABLED, "pcm-dma-read", NULL);
	if (ret < 0)
		return ret;

	ret = request_irq(LS1X_DMA2_IRQ, ls1x_dmaw_irq, IRQF_DISABLED, "pcm-dma-write", NULL);
	if (ret < 0)
		return ret;

	return snd_soc_register_platform(&pdev->dev, &ls1x_soc_platform);
}

static int __devexit ls1x_soc_platform_remove(struct platform_device *pdev)
{
	free_irq(LS1X_DMA1_IRQ, NULL);
	free_irq(LS1X_DMA2_IRQ, NULL);

	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver loongson1_pcm_driver = {
	.driver = {
			.name = "loongson1-pcm-audio",
			.owner = THIS_MODULE,
	},

	.probe = ls1x_soc_platform_probe,
	.remove = __devexit_p(ls1x_soc_platform_remove),
};

static int __init snd_loongson1_pcm_init(void)
{
	return platform_driver_register(&loongson1_pcm_driver);
}
module_init(snd_loongson1_pcm_init);

static void __exit snd_loongson1_pcm_exit(void)
{
	platform_driver_unregister(&loongson1_pcm_driver);
}
module_exit(snd_loongson1_pcm_exit);

MODULE_AUTHOR("loongson.cn");
MODULE_DESCRIPTION("Loongson1 PCM DMA module");
MODULE_LICENSE("GPL");
