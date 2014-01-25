/*
 * linux/sound/mips/ls1a-pcm.h -- ALSA PCM interface for the Intel PXA2xx chip
 *
 * Author:	Nicolas Pitre
 * Created:	Nov 30, 2004
 * Copyright:	MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <asm/delay.h>
#include "ls1x-pcm-private.h"

struct ls1x_runtime_data {
	int dma_ch;
	struct ls1x_pcm_dma_params *params;

	ls1x_dma_desc *dma_desc_array;
	dma_addr_t dma_desc_array_phys;

	/* 将要执行的dma描述符物理地址 */
	ls1x_dma_desc *dma_desc_ready;
	dma_addr_t dma_desc_ready_phys;

	ls1x_dma_desc *dma_position_desc;
	dma_addr_t dma_position_desc_phys;
};

struct ls1a_pcm_client {
	struct ls1a_pcm_dma_params *playback_params;
	struct ls1a_pcm_dma_params *capture_params;
	int (*startup)(struct snd_pcm_substream *);
	void (*shutdown)(struct snd_pcm_substream *);
	int (*prepare)(struct snd_pcm_substream *);
};
