/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/ls1x-lib.h>


#include <linux/interrupt.h>

#include "ls1x-pcm.h"//For DMA Reasons and AC97 Registers
//#include "../soc/loongson1/ls1x-pcm.h"//For sth shared with ls1a-ac97.c

extern void dma_free_writecombine(struct device *dev, size_t size, void *cpu_addr, dma_addr_t handle);

static const struct snd_pcm_hardware ls1x_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_INTERLEAVED |
				 //SNDRV_PCM_INFO_BLOCK_TRANSFER |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  /* No full-resume yet implemented */
				  SNDRV_PCM_INFO_RESUME |
				  SNDRV_PCM_INFO_PAUSE,

	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.rates			= SNDRV_PCM_RATE_8000_48000,
	.channels_min		= 1,
	.channels_max		= 2,
	.period_bytes_min	= 128,
	.period_bytes_max	= 128*1024,
	.periods_min		= 1,
	.periods_max		= PAGE_SIZE/sizeof(ls1x_dma_desc),
	.buffer_bytes_max	= 1024 * 1024,
//	.fifo_size		= 0,
};

int __ls1x_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ls1x_runtime_data *rtd = runtime->private_data;
	size_t totsize = params_buffer_bytes(params);
	size_t period = params_period_bytes(params);
	ls1x_dma_desc *dma_desc;
	dma_addr_t dma_buff_phys, next_desc_phys;
	
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = totsize;

	dma_desc = rtd->dma_desc_array;
	next_desc_phys = rtd->dma_desc_array_phys;
	dma_buff_phys = runtime->dma_addr;
	do {
		next_desc_phys += sizeof(ls1x_dma_desc);
		dma_desc->ordered = ((u32)(next_desc_phys) | 0x1);
		dma_desc->saddr = dma_buff_phys;
		dma_desc->daddr = rtd->params->dev_addr;
		dma_desc->dcmd = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
				0x00001001 : 0x00000001;
		dma_desc->length = 8;
		dma_desc->step_length = 0;
		if (period > totsize)
			period = totsize;
		dma_desc->step_times = period >> 5;

		dma_desc++;
		dma_buff_phys += period;
	} while (totsize -= period);
	dma_desc[-1].ordered = ((u32)(rtd->dma_desc_array_phys) | 0x1);

	return 0;
}
EXPORT_SYMBOL(__ls1x_pcm_hw_params);

int __ls1x_pcm_hw_free(struct snd_pcm_substream *substream)
{
//	struct ls1x_runtime_data *rtd = substream->runtime->private_data;

	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}
EXPORT_SYMBOL(__ls1x_pcm_hw_free);

int ls1x_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct ls1x_runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;
	u32 val;
	
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		val = prtd->dma_desc_array_phys;
		val |= (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? 0x9 : 0xa;
		writel(val, CONFREG_BASE);
		while (readl(CONFREG_BASE) & 0x8);
		break;

/*	case SNDRV_PCM_TRIGGER_STOP:
		//停止DMA
		val = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? 0x11 : 0x12;
		writel(val, CONFREG_BASE);
//		udelay(1000);
		break;*/
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		//写回控制器寄存器的值,暂停播放
		val = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? 0x5 : 0x6;
		writel(prtd->dma_desc_ready_phys | val, CONFREG_BASE);
		while (readl(CONFREG_BASE) & 0x4);

		val = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? 0x11 : 0x12;
		writel(val, CONFREG_BASE);
//		udelay(1000);
		break;

	case SNDRV_PCM_TRIGGER_RESUME:
		val = readl(CONFREG_BASE);
		val |=  (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? 0x9 : 0xa;
		writel(val, CONFREG_BASE);
		while (readl(CONFREG_BASE) & 0x8);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		val = prtd->dma_desc_ready_phys;
		val |=  (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? 0x9 : 0xa;
		writel(val, CONFREG_BASE);
		while (readl(CONFREG_BASE) & 0x8);
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}
EXPORT_SYMBOL(ls1x_pcm_trigger);

snd_pcm_uframes_t
ls1x_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ls1x_runtime_data *prtd = runtime->private_data;
	snd_pcm_uframes_t x;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		writel(prtd->dma_position_desc_phys | 0x5, CONFREG_BASE);
		while (readl(CONFREG_BASE) & 0x4);
	} else {
		writel(prtd->dma_position_desc_phys | 0x6, CONFREG_BASE);
		while (readl(CONFREG_BASE) & 0x4);
	}
	x = bytes_to_frames(runtime, prtd->dma_position_desc->saddr - runtime->dma_addr);
	if (x == runtime->buffer_size)
		x = 0;
	return x;
}
EXPORT_SYMBOL(ls1x_pcm_pointer);

int __ls1x_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct ls1x_runtime_data *prtd = substream->runtime->private_data;
//	u32 val;

	if (!prtd || !prtd->params)
		return 0;

	return 0;
}
EXPORT_SYMBOL(__ls1x_pcm_prepare);

inline void ls1x_pcm_dma_irq(int dma_ch, void *dev_id)
{
	struct snd_pcm_substream *substream = dev_id;
//	struct ls1x_runtime_data *rtd = substream->runtime->private_data;

	snd_pcm_period_elapsed(substream);
//	return IRQ_HANDLED;
}
EXPORT_SYMBOL(ls1x_pcm_dma_irq);

int __ls1x_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ls1x_runtime_data *rtd;
	int ret;

	runtime->hw = ls1x_pcm_hardware;
#if 1
	/*from dummy.c*/
	if (substream->pcm->device & 1) {
		runtime->hw.info &= ~SNDRV_PCM_INFO_INTERLEAVED;
		runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;
	}
	if (substream->pcm->device & 2)
		runtime->hw.info &= ~(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID);
	/*
	 * For mysterious reasons (and despite what the manual says)
	 * playback samples are lost if the DMA count is not a multiple
	 * of the DMA burst size.  Let's add a rule to enforce that.
	 */
	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 128);
	if (ret)
		goto out;

	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 128);
	if (ret)
		goto out;

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		goto out;
#endif
	ret = -ENOMEM;
	rtd = kzalloc(sizeof(*rtd), GFP_KERNEL);
	if (!rtd)
		goto out;
	//DMA desc内存在此处分配
	rtd->dma_desc_array =
		dma_alloc_coherent(substream->pcm->card->dev, PAGE_SIZE,
				       &rtd->dma_desc_array_phys, GFP_KERNEL);
	if (!rtd->dma_desc_array)
		goto err1;
	//分配记录当前DMA执行位置
	rtd->dma_desc_ready =
		dma_alloc_coherent(substream->pcm->card->dev, 32,
					&rtd->dma_desc_ready_phys, GFP_KERNEL);
	if (!rtd->dma_desc_ready)
		goto err1;
	//记录当前硬件缓冲区的使用程度
	rtd->dma_position_desc =
		dma_alloc_coherent(substream->pcm->card->dev, 32,
					&rtd->dma_position_desc_phys, GFP_KERNEL);
	if (!rtd->dma_position_desc)
		goto err1;

	runtime->private_data = rtd;
	return 0;

 err1:
	kfree(rtd);
 out:
	return ret;
}
EXPORT_SYMBOL(__ls1x_pcm_open);

int __ls1x_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ls1x_runtime_data *rtd = runtime->private_data;

	dma_free_coherent(substream->pcm->card->dev, PAGE_SIZE,	
			      rtd->dma_desc_array, rtd->dma_desc_array_phys);
	kfree(rtd);
	return 0;
}
EXPORT_SYMBOL(__ls1x_pcm_close);

/*
int ls1x_pcm_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	return dma_mmap_coherent(substream->pcm->card->dev, vma,
				     runtime->dma_area,
				     runtime->dma_addr,
				     runtime->dma_bytes);
				     
	return 0;
}
EXPORT_SYMBOL(ls1x_pcm_mmap);
*/

int ls1x_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = ls1x_pcm_hardware.buffer_bytes_max;
	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_coherent(pcm->card->dev, size,
					  &buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;
	return 0;
}
EXPORT_SYMBOL(ls1x_pcm_preallocate_dma_buffer);

void ls1x_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;
		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;
		dma_free_coherent(pcm->card->dev, buf->bytes,
				      buf->area, buf->addr);
		buf->area = NULL;
	}
}
EXPORT_SYMBOL(ls1x_pcm_free_dma_buffers);

MODULE_AUTHOR("Nicolas Pitre");
MODULE_DESCRIPTION("Intel PXA2xx sound library");
MODULE_LICENSE("GPL");
