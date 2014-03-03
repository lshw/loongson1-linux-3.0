/*
 * Copyright (c) 2013 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/poll.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/ac97_codec.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include "ls1x_ac97.h"
#include <irq.h>
#include <loongson1.h>

static void __iomem *order_addr_in;

struct dma_desc {
	u32 ordered;
	u32 saddr;
	u32 daddr;
	u32 length;
	u32 step_length;
	u32 step_times;
	u32 cmd;
	u32 stats;
};

struct audio_dma_desc {
	struct dma_desc snd;
	struct dma_desc null;
	struct list_head link;
	struct list_head all;
	dma_addr_t snd_dma_handle;
	dma_addr_t snd_dma;
	u32 pos;
	char *snd_buffer;
};

struct audio_stream {
	struct dma_desc *ask_dma;
	struct list_head free_list;
	struct list_head run_list;
	struct list_head done_list;
	struct list_head all_list;
	spinlock_t lock;
#ifdef CONFIG_SND_SB2F_TIMER
	struct timer_list	timer;		/* "no irq" timer */
#endif
	u32 nbfrags;
	u32 fragsize;
	struct semaphore sem;
	wait_queue_head_t frag_wq;
	dma_addr_t ask_dma_handle;

	int num_channels;
	u32 output;
	u32 sample_rate;
	u32 sample_size;
	u32 rate;
	u32 state;
};

static struct audio_stream input_stream = {
	output: 0,
	fragsize:0x10000,
};

static struct audio_stream output_stream = {
	output: 1,
	fragsize:0x10000,
};

enum {
	STOP = 0,
	RUN = 1
};

/* Boot options
 * 0 = no VRA, 1 = use VRA if codec supports it
 */
static int      vra = 1;
module_param(vra, bool, 0);
MODULE_PARM_DESC(vra, "if 1 use VRA if codec supports it");

static struct ls1x_audio_state {
	void __iomem *base;
	/* soundcore stuff */
	int dev_audio;

	struct ac97_codec *codec;
	unsigned codec_base_caps; /* AC'97 reg 00h, "Reset Register" */
	unsigned codec_ext_caps;  /* AC'97 reg 28h, "Extended Audio ID" */
	int no_vra;		/* do not use VRA */

	spinlock_t lock;
	struct mutex open_mutex;
	struct mutex mutex;
	fmode_t open_mode;
	wait_queue_head_t open_wait;

	struct audio_stream	 *input_stream;
	struct audio_stream	 *output_stream;

	u32 rd_ref:1;
	u32 wr_ref:1;
	struct semaphore sem;
} ls1x_audio_state;

static void dma_enable_trans(struct audio_stream * s, struct audio_dma_desc *desc)
{
	struct ls1x_audio_state *state = &ls1x_audio_state;
	u32 val;
	int timeout = 20000;
//	unsigned long flags;

	val = desc->snd_dma_handle;
	val |= s->output ? 0x9 : 0xa;
	if (s->output) {
		writel(readl(state->base + LS1X_IIS_CONTROL) | 0x1080, state->base + LS1X_IIS_CONTROL);
	} else {
		writel(readl(state->base + LS1X_IIS_CONTROL) | 0x2800, state->base + LS1X_IIS_CONTROL);
	}
//	local_irq_save(flags);
	writel(val, order_addr_in);
//	while(readl(order_addr_in) & 0x8);
	while ((readl(order_addr_in) & 0x8) && (timeout-- > 0)) {
//		udelay(5);
	}
//	local_irq_restore(flags);
//	writel(0xffffffff, state->base + INTM);
#ifdef CONFIG_SND_SB2F_TIMER
	mod_timer(&s->timer, jiffies + HZ/20);
#endif
}

void audio_clear_buf(struct audio_stream *s)
{
	struct audio_dma_desc *desc;

	while (!list_empty(&s->all_list)) {
		desc = list_entry(s->all_list.next, struct audio_dma_desc, all);
		list_del(&desc->all);
		list_del(&desc->link);

		if (desc->snd_buffer)
			free_pages((unsigned long)desc->snd_buffer, get_order(s->fragsize));
		dma_free_coherent(NULL, sizeof(struct audio_dma_desc), desc, 0);
	}

	if (s->ask_dma)
		dma_free_coherent(NULL, sizeof(struct audio_dma_desc), s->ask_dma, 0);

	s->ask_dma = NULL;
}

static void inline link_dma_desc(struct audio_stream *s, struct audio_dma_desc *desc)
{
	spin_lock_irq(&s->lock);

	if(!list_empty(&s->run_list)) {
		struct audio_dma_desc *desc0;
		desc0 = list_entry(s->run_list.prev, struct audio_dma_desc, link);
		desc0->snd.ordered = desc->snd_dma_handle | DMA_ORDERED_EN;
		desc0->null.ordered = desc->snd_dma_handle | DMA_ORDERED_EN;
		list_add_tail(&desc->link, &s->run_list);
		if(s->state == STOP) {
			s->state = RUN;
			dma_enable_trans(s, desc0);
		}
	}
	else {
		list_add_tail(&desc->link,&s->run_list);
		dma_enable_trans(s, desc);
	}

	spin_unlock_irq(&s->lock);
}

static void ls1x_init_dmadesc(struct audio_stream *s, struct audio_dma_desc *desc, u32 count)
{
	struct dma_desc *_desc;
	u32 control;

	control = s->output ? DMA_OUT_ADDR : DMA_IN_ADDR;

	desc->snd.daddr = desc->null.daddr = control;

	_desc = &desc->snd;
	_desc->ordered = (desc->snd_dma_handle + sizeof(struct dma_desc)) | DMA_ORDERED_EN;
	_desc->saddr = desc->snd_dma;
	_desc->length = 8;
	_desc->step_length = 0;
	_desc->step_times = count >> 5;
	_desc->cmd = s->output ? 0x00001001 : 0x00000001; //enable the interrupt

	_desc = &desc->null;
	_desc->ordered =  (desc->snd_dma_handle + sizeof(struct dma_desc)) | DMA_ORDERED_EN;
	_desc->saddr = desc->snd_dma;
	_desc->length = 8;
	_desc->step_length = 0;
	_desc->step_times = 1;
	_desc->cmd = s->output ? 0x00001000 : 0x00000000; //disable the interrupt
}

/* setup buffers,dma descs,buffer. */
int ls1x_setup_buf(struct audio_stream * s)
{	
	int i;
	dma_addr_t dma_phyaddr;

	if (s->ask_dma)
		return -EBUSY;

	for (i=0; i <s->nbfrags; i++) {
		struct audio_dma_desc *desc;

		desc = dma_alloc_coherent(NULL, sizeof(struct audio_dma_desc),
					(dma_addr_t *)&dma_phyaddr, GFP_KERNEL);
		if (!desc) {
			printk(KERN_ERR "2.alloc dma desc err.\n");
			goto err;
		}
		memset(desc, 0, sizeof(struct audio_dma_desc));

		desc->snd_dma_handle = dma_phyaddr;
		desc->null.ordered = (dma_phyaddr + sizeof(struct dma_desc)) | DMA_ORDERED_EN;
		desc->snd.ordered = (dma_phyaddr + sizeof(struct dma_desc)) | DMA_ORDERED_EN;
		list_add_tail(&desc->link,&s->free_list);
		list_add_tail(&desc->all,&s->all_list);

		desc->snd_buffer = (void *)__get_free_pages(GFP_ATOMIC|GFP_DMA, get_order(s->fragsize));
		if (!desc->snd_buffer) {
			printk(KERN_ERR "4.alloc dma buffer err\n");
			goto err;
		}
		desc->snd_dma = dma_map_single(NULL, desc->snd_buffer, s->fragsize, DMA_FROM_DEVICE);
	}

	/* dma desc for ask_valid one per struct audio_stream */
	s->ask_dma = dma_alloc_coherent(NULL, sizeof(struct dma_desc),
			&dma_phyaddr, GFP_KERNEL);
	if (!s->ask_dma) {
		printk(KERN_ERR "3. alloc dma desc err.\n");
		goto err;
	}
	memset(s->ask_dma, 0, sizeof(struct dma_desc));
	s->ask_dma_handle = dma_phyaddr;

	sema_init(&s->sem, 1);

	return 0;

err:
	audio_clear_buf(s);
	printk(KERN_ERR "unable to allocate audio memory\n");
	return -ENOMEM;
}

#ifdef CONFIG_SND_SB2F_TIMER
static void ls1x_audio_timeout(unsigned long data)
{
	struct audio_stream *s = (struct audio_stream *)data;
	if (s->output)
		ac97_dma_write_intr(0, s);
	else
		ac97_dma_read_intr(0, s);
	mod_timer(&s->timer, jiffies + HZ/20);
}
#endif

static irqreturn_t ac97_dma_read_intr(int irq, void *private)
{
	struct audio_stream *s = (struct audio_stream *)private;
	struct audio_dma_desc *desc;
//	unsigned long flags;

	if (list_empty(&s->run_list))
		return IRQ_HANDLED;

//	local_irq_save(flags);
	writel(s->ask_dma_handle | 0x6, order_addr_in);
	while (readl(order_addr_in) & 4) {
	}
//	local_irq_restore(flags);

	do {
		desc = list_entry(s->run_list.next, struct audio_dma_desc, link);
		if (s->ask_dma->ordered == desc->snd.ordered)
			break;

//		spin_lock(&s->lock);
		list_del(&desc->link);
		list_add_tail(&desc->link, &s->done_list);
//		spin_unlock(&s->lock);
	} while (!list_empty(&s->run_list));

	if (!list_empty(&s->done_list))
		wake_up(&s->frag_wq);
	return IRQ_HANDLED;
}

static irqreturn_t ac97_dma_write_intr(int irq, void *private)
{
	struct audio_stream *s = (struct audio_stream *)private;
	struct audio_dma_desc *desc;
//	unsigned long flags;
	
	if (list_empty(&s->run_list))
		return IRQ_HANDLED;

//	local_irq_save(flags);
	writel(s->ask_dma_handle | 0x5, order_addr_in);
	while (readl(order_addr_in) & 4) {
	}
//	local_irq_restore(flags);

	do {
		desc = list_entry(s->run_list.next, struct audio_dma_desc, link);
		/*first desc's ordered may be null*/
		if (s->ask_dma->ordered == desc->snd.ordered || s->ask_dma->ordered == 
			((desc->snd_dma_handle + sizeof(struct dma_desc)) | DMA_ORDERED_EN))
			break;

//		spin_lock(&s->lock);
		list_del(&desc->link);
		desc->pos = 0;
		list_add_tail(&desc->link, &s->free_list);
//		spin_unlock(&s->lock);
	} while (!list_empty(&s->run_list));

	if (!list_empty(&s->free_list))
		wake_up(&s->frag_wq);

	return IRQ_HANDLED;
}

static u32 fill_play_buffer(struct audio_stream *s, const char *buf, u32 count)
{
	struct audio_dma_desc *desc;
	u32 copy_bytes;

	desc = list_entry(s->free_list.next, struct audio_dma_desc, link);
	copy_bytes = min((s->fragsize - desc->pos), count);
	copy_from_user((void *)(desc->snd_buffer + desc->pos), buf, copy_bytes);
	desc->pos += copy_bytes;

	if (desc->pos == s->fragsize) {
//		spin_lock_irq(&s->lock);
		list_del(&desc->link);
//		spin_unlock_irq(&s->lock);
		ls1x_init_dmadesc(s, desc, s->fragsize);
		link_dma_desc(s, desc);
	}

	return copy_bytes;
}

static int ls1x_audio_write(struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
	struct ls1x_audio_state *state = (struct ls1x_audio_state *)file->private_data;
	struct audio_stream *s = state->output_stream;
	const char *buffer0 = buffer;
	unsigned int ret = 0;

	if (*ppos != file->f_pos) {
		return -ESPIPE;
	}

	if (!s->ask_dma && ls1x_setup_buf(s)) {
		return -ENOMEM;
	}

	if (file->f_flags & O_NONBLOCK) {
		if (down_trylock(&s->sem))
			return -EAGAIN;
	} else {
		if (down_interruptible(&s->sem))
			return -ERESTARTSYS;
	}

	while (count > 0) {
		if (list_empty(&s->free_list)) {
			if (file->f_flags & O_NONBLOCK)
				return -EAGAIN;

			if (wait_event_interruptible(s->frag_wq, !list_empty(&s->free_list))) {
				up(&s->sem);
				return -ERESTARTSYS;
			}
		}

		/* Fill data , if the ring is not full */
		ret = fill_play_buffer(s, buffer, count);
		count -= ret;
		buffer += ret;
	}

	up(&s->sem);
	return (buffer - buffer0);
}

static int ls1x_copy_to_user(struct audio_stream *s, char *buffer, u32 count)
{
	struct audio_dma_desc *desc;
	int ret = 0;

	while (!list_empty(&s->done_list) && count) {
		u32 left;
		desc = list_entry(s->done_list.next, struct audio_dma_desc, link);
		left = min(s->fragsize - desc->pos,count);
		copy_to_user(buffer, (void *)(desc->snd_buffer + desc->pos), left);
		desc->pos += left;
		count -= left;
		buffer += left;
		ret += left;
		if (desc->pos == s->fragsize) {
			list_del(&desc->link);
			desc->pos = 0;
			list_add_tail(&desc->link, &s->free_list);
		}
	}

	return ret;
}

static int ls1x_audio_read(struct file *file, char *buffer, size_t count, loff_t * ppos)
{
	struct ls1x_audio_state *state = file->private_data;
	struct audio_stream *s = state->input_stream;
	struct audio_dma_desc *desc;
	char *buffer0 = buffer;

	if (*ppos != file->f_pos)
		return -ESPIPE;

	if (!s->ask_dma && ls1x_setup_buf(s))
		return -ENOMEM;

	/*3. sem:get resource .if 0 ,wait */
	if (file->f_flags & O_NONBLOCK) {
		if (down_trylock(&s->sem))
			return -EAGAIN;
	} else {
		if (down_interruptible(&s->sem))
			return -ERESTARTSYS;
	}

	while (count > 0) {
		int ret;

		while (!list_empty(&s->free_list)) {
			desc = list_entry(s->free_list.next, struct audio_dma_desc, link);
//			spin_lock_irq(&s->lock);
			list_del(&desc->link);
//			spin_unlock_irq(&s->lock);
			ls1x_init_dmadesc(s, desc, s->fragsize);
			link_dma_desc(s, desc);
		}

		/* record's buffer is empty */
		while (list_empty(&s->done_list)) {
			if (file->f_flags & O_NONBLOCK) {
				up(&s->sem);
				return -EAGAIN;
			}
			if (wait_event_interruptible(s->frag_wq, !list_empty(&s->done_list))) {
				up(&s->sem);
				return -ERESTARTSYS;
			}
		}

		/* data is ready now , so copy it */
		ret = ls1x_copy_to_user(s, buffer, count);
		count -= ret;
		buffer += ret;
	}

	while (!list_empty(&s->free_list)) {
		desc = list_entry(s->free_list.next, struct audio_dma_desc, link);
		list_del(&desc->link);
		ls1x_init_dmadesc(s, desc, s->fragsize);
		link_dma_desc(s, desc);
	}

	up(&s->sem);
	return (buffer - buffer0);
}

static long ls1x_audio_ioctl(struct file *file, uint cmd, ulong arg)
{
	struct ls1x_audio_state *state = file->private_data;
	struct audio_stream *os = state->output_stream;
	struct audio_stream *is = state->input_stream;
	int rd=0, wr=0, val=0;

	if (file->f_mode & FMODE_WRITE)
		wr = 1;
	if (file->f_mode & FMODE_READ)
		rd = 1;

	switch (cmd) {
	case OSS_GETVERSION:
		return put_user(SOUND_VERSION, (int *) arg);

	case SNDCTL_DSP_SETDUPLEX:
		return 0;

	case SNDCTL_DSP_GETCAPS:
		val = DSP_CAP_REALTIME | DSP_CAP_TRIGGER | DSP_CAP_MMAP;
		if (is && os)
			val |= DSP_CAP_DUPLEX;
		return put_user(val, (int *) arg);

	case SNDCTL_DSP_SPEED:
		return 0;

	case SNDCTL_DSP_STEREO:
		return 0;

	case SNDCTL_DSP_CHANNELS:
		if (get_user(val, (int *) arg))
			return -EFAULT;
		if (val != 0) {
			if (file->f_mode & FMODE_READ) {
				if (val < 0 || val > 2)
					return -EINVAL;
				is->num_channels = val;
			}
			if (file->f_mode & FMODE_WRITE) {
				switch (val) {
				case 1:
				case 2:
					break;
				case 3:
				case 5:
					return -EINVAL;
				case 4:
					if (!(state->codec_ext_caps &
					      AC97_EXTID_SDAC))
						return -EINVAL;
					break;
				case 6:
					if ((state->codec_ext_caps &
					     AC97_EXT_DACS) != AC97_EXT_DACS)
						return -EINVAL;
					break;
				default:
					return -EINVAL;
				}

				if (val <= 2 &&
				    (state->codec_ext_caps & AC97_EXT_DACS)) {
					/* disable surround and center/lfe
					 * channels in AC'97
					 */
					//u16 ext_stat = ls1x_codec_read(state->codec, AC97_EXTENDED_STATUS);
					//ls1x_codec_write(state->codec, AC97_EXTENDED_STATUS,
					//	ext_stat | (AC97_EXTSTAT_PRI | AC97_EXTSTAT_PRJ | AC97_EXTSTAT_PRK));
				} else if (val >= 4) {
					/* enable surround, center/lfe
					 * channels in AC'97
					 */
					//u16 ext_stat = ls1x_codec_read(state->codec, AC97_EXTENDED_STATUS);
			//		ext_stat &= ~AC97_EXTSTAT_PRJ;
	///				if (val == 6)
				///	ext_stat &= ~(AC97_EXTSTAT_PRI | AC97_EXTSTAT_PRK);
			///		ls1x_codec_write(state->codec, AC97_EXTENDED_STATUS, ext_stat);
				}

				os->num_channels = val;
			}
		}
		return put_user(val, (int *) arg);

	case SNDCTL_DSP_GETFMTS:	/* Returns a mask */
		return put_user(AFMT_S16_LE | AFMT_U8, (int *) arg);

	case SNDCTL_DSP_SETFMT:	/* Selects ONE fmt */
		if (get_user(val, (int *) arg))
			return -EFAULT;
		if (val != AFMT_QUERY) {
			if (file->f_mode & FMODE_READ) {
				if (val == AFMT_S16_LE)
					state->input_stream->sample_size = 16;
				else {
					val = AFMT_U8;
					state->input_stream->sample_size = 8;
				}
			}
			if (file->f_mode & FMODE_WRITE) {
				if (val == AFMT_S16_LE)
					state->output_stream->sample_size = 16;
				else {
					val = AFMT_U8;
					state->output_stream->sample_size = 8;
				}
			}
		} else {
			if (file->f_mode & FMODE_READ)
				val = (state->input_stream->sample_size == 16) ?
					AFMT_S16_LE : AFMT_U8;
			else
				val = (state->output_stream->sample_size == 16) ?
					AFMT_S16_LE : AFMT_U8;
		}
		return put_user(val, (int *) arg);

	case SNDCTL_DSP_POST:
		return 0;

	case SNDCTL_DSP_SETFRAGMENT:
		if (get_user(val, (int *)arg)) {
			return -EFAULT;
		}

		if (rd) {
			is->fragsize = 1 << (val & 0xFFFF);
			if (is->fragsize < 1024) is->fragsize = 1024;
			is->nbfrags = (val >> 16) & 0xFFFF;
			if (is->nbfrags < 4) is->nbfrags = 4;
		}

		if (wr) {
			os->fragsize = 1 << (val & 0xFFFF);
			if (os->fragsize < 1024) os->fragsize = 1024;
			os->nbfrags = (val >> 16) & 0xFFFF;
			if (os->nbfrags < 4) os->nbfrags = 4;
			if (os->num_channels) {
				//os->fragsize >>= 1;
				os->nbfrags >>= 2;
				if (os->nbfrags < 2) os->nbfrags = 2;
			}
		}

		return 0;

	case SNDCTL_DSP_GETBLKSIZE:
		if (file->f_mode & FMODE_WRITE)
			return put_user(os->fragsize, (int *) arg);
		else
			return put_user(is->fragsize, (int *) arg);

	case SNDCTL_DSP_NONBLOCK:
		spin_lock(&file->f_lock);
		file->f_flags |= O_NONBLOCK;
		spin_unlock(&file->f_lock);
		return 0;

	case SOUND_PCM_READ_RATE:
		return put_user((file->f_mode & FMODE_READ) ?
				state->input_stream->sample_rate :
				state->output_stream->sample_rate,
				(int *)arg);

	case SOUND_PCM_READ_CHANNELS:
		if (file->f_mode & FMODE_READ)
			return put_user(state->input_stream->num_channels, (int *)arg);
		else
			return put_user(state->output_stream->num_channels, (int *)arg);

	case SOUND_PCM_READ_BITS:
		if (file->f_mode & FMODE_READ)
			return put_user(state->input_stream->sample_size, (int *)arg);
		else
			return put_user(state->output_stream->sample_size, (int *)arg);

	case SOUND_PCM_WRITE_FILTER:
	case SNDCTL_DSP_SETSYNCRO:
	case SOUND_PCM_READ_FILTER:
		return -EINVAL;
	default:
		return -EINVAL;
	}

	return 0;	//ls1x_ioctl_mixdev(file, cmd, arg);
}

static int ls1x_audio_sync(struct file *file)
{
        struct ls1x_audio_state *state = file->private_data;
        struct audio_stream *is = state->input_stream;
        struct audio_stream *os = state->output_stream;

        if (file->f_mode & FMODE_READ) {
                if (is->state == STOP && !list_empty(&is->run_list)) {
                        struct audio_dma_desc *desc;
                        desc = list_entry(is->run_list.next, struct audio_dma_desc, link);
                        dma_enable_trans(is, desc);
                        is->state = RUN;
                }

                if (!list_empty(&is->run_list))
                        schedule_timeout(CONFIG_HZ*2);

                /* stop write ac97 dma */
                writel(0x12, order_addr_in);
        }
        if (file->f_mode & FMODE_WRITE) {
                if (os->state == STOP && !list_empty(&os->run_list)) {
                        struct audio_dma_desc *desc;
                        desc = list_entry(os->run_list.next, struct audio_dma_desc, link);
                        dma_enable_trans(os, desc);
                        os->state = RUN;
                }

                if (!list_empty(&os->run_list))
                        schedule_timeout(CONFIG_HZ*2);

                /* stop read ac97 dma */
                writel(0x11, order_addr_in);
        }

        return 0;
}

static int ls1x_audio_release(struct inode *inode, struct file *file)
{
	struct ls1x_audio_state *state = file->private_data;

	down(&state->sem);

	if (file->f_mode & FMODE_READ) {
		ls1x_audio_sync(file);
		audio_clear_buf(state->input_stream);
		state->rd_ref = 0;
		free_irq(LS1X_DMA2_IRQ, state->input_stream);
#ifdef CONFIG_SND_SB2F_TIMER
		del_timer(&state->input_stream->timer);
#endif
	}

	if (file->f_mode & FMODE_WRITE) {
		ls1x_audio_sync(file);
		audio_clear_buf(state->output_stream);
		state->wr_ref = 0;
		free_irq(LS1X_DMA1_IRQ, state->output_stream);
#ifdef CONFIG_SND_SB2F_TIMER
		del_timer(&state->output_stream->timer);
#endif
	}

	up(&state->sem);
	return 0;
}

static int ls1x_audio_open(struct inode *inode, struct file *file)
{
	struct ls1x_audio_state *state = &ls1x_audio_state;
	struct audio_stream *is = state->input_stream;
	struct audio_stream *os = state->output_stream;
	int minor = MINOR(inode->i_rdev);
	int err;
	u32 x, conf;

	down(&state->sem);

	/* access control */
	err = -ENODEV;
	if ((file->f_mode & FMODE_WRITE) && !os)
		goto out;
	if ((file->f_mode & FMODE_READ) && !is)
		goto out;
	err = -EBUSY;

	if ((file->f_mode & FMODE_WRITE) && state->wr_ref)
		goto out;
	if ((file->f_mode & FMODE_READ) && state->rd_ref)
		goto out;

	file->private_data = state;

	if ((file->f_mode & FMODE_WRITE)) {
		state->wr_ref = 1;
		os->fragsize = AUDIO_FRAGSIZE_DEFAULT;
		os->nbfrags = PLAY_NBFRAGS;
		os->output = 1;
		os->num_channels = 2;
		os->sample_size = 16;
		if ((minor & 0xf) == SND_DEV_DSP16)
			os->sample_size = 16;
		init_waitqueue_head(&os->frag_wq);
		os->ask_dma = NULL;
		INIT_LIST_HEAD(&os->free_list);
		INIT_LIST_HEAD(&os->run_list);
		INIT_LIST_HEAD(&os->done_list);
		INIT_LIST_HEAD(&os->all_list);
		spin_lock_init(&os->lock);
		request_irq(LS1X_DMA1_IRQ, ac97_dma_write_intr, IRQF_SHARED,
				"ac97dma-write", os);
#ifdef CONFIG_SND_SB2F_TIMER
		init_timer(&os->timer);
		os->timer.data = (unsigned long)os;
		os->timer.function = ls1x_audio_timeout;
#endif
	}

	if (file->f_mode & FMODE_READ) {
		state->rd_ref = 1;
		is->fragsize = AUDIO_FRAGSIZE_DEFAULT;
		is->nbfrags = REC_NBFRAGS;
		is->output = 0;
		is->num_channels = 2;
		is->sample_size = 16;
		if ((minor & 0xf) == SND_DEV_DSP16)
			is->sample_size = 16;
		init_waitqueue_head(&is->frag_wq);
		is->ask_dma = NULL;
		INIT_LIST_HEAD(&is->free_list);
		INIT_LIST_HEAD(&is->run_list);
		INIT_LIST_HEAD(&is->done_list);
		INIT_LIST_HEAD(&is->all_list);
		spin_lock_init(&is->lock);
		request_irq(LS1X_DMA2_IRQ, ac97_dma_read_intr, IRQF_SHARED,
				"ac97dma-read", is);
#ifdef CONFIG_SND_SB2F_TIMER
		init_timer(&is->timer);
		is->timer.data = (unsigned long)is;
		is->timer.function = ls1x_audio_timeout;
#endif
	}

	if (file->f_mode & FMODE_WRITE) {
		x = readl(state->base + OCC0);
		conf = x & ~(FIFO_THRES_MASK << FIFO_THRES_OFFSET) & ~(SS_MASK << SS_OFFSET);
		if (state->no_vra)
			conf = (conf | (2 << FIFO_THRES_OFFSET) | DMA_EN | CH_EN) & (~SR);
		else
			conf = conf | (2 << FIFO_THRES_OFFSET) | DMA_EN | CH_EN | SR;
		if (os->sample_size == 16)
			conf = conf | (2 << SS_OFFSET);
		conf |= (conf << OCH1_CFG_R_OFFSET) | (conf << OCH0_CFG_L_OFFSET);
		//writel(conf, state->base + OCC0);cys
		//writel(0x6969, state->base + OCC0);
	}
	if (file->f_mode & FMODE_READ) {
		x = readl(state->base + ICC);
		conf = x & ~(FIFO_THRES_MASK << FIFO_THRES_OFFSET) & ~(SS_MASK << SS_OFFSET);
		if (state->no_vra)
			conf = (conf | (2 << FIFO_THRES_OFFSET) | DMA_EN | CH_EN) & (~SR);
		else
			conf = conf | (2 << FIFO_THRES_OFFSET) | DMA_EN | CH_EN | SR;
		if (is->sample_size == 16)
			conf = conf | (2 << SS_OFFSET);
		conf |= (conf << ICH2_CFG_MIC_OFFSET) | (conf << ICH1_CFG_R_OFFSET) | (conf << ICH0_CFG_L_OFFSET);
		writel(conf, state->base + ICC);
	}

	err = 0;

out:
	up(&state->sem);
	printk("err is %d\n",err);
	return err;
}

static struct file_operations ls1x_dsp_fops = {
	owner:			THIS_MODULE,
	read:			ls1x_audio_read,
	write:			ls1x_audio_write,
	unlocked_ioctl:	ls1x_audio_ioctl,
	open:			ls1x_audio_open,
	release:		ls1x_audio_release,
};

static DEFINE_SEMAPHORE(ls1x_ac97_mutex);

void ls1x_iis_config(struct ls1x_audio_state *state)
{
	unsigned char sck_ratio;
	unsigned char bck_ratio;
	struct clk *clk;

	clk = clk_get(NULL, "apb");
	if (IS_ERR(clk))
		panic("unable to get apb clock, err=%ld", PTR_ERR(clk));

	#define SAMP_RATE 44100
	sck_ratio = (clk_get_rate(clk)/(SAMP_RATE*2*2*16)) - 1;
	bck_ratio = (clk_get_rate(clk)/(SAMP_RATE*2*2*512)) - 1;
//	sck_ratio = 0x2d;
//	bck_ratio = 0x04;

	writel((16<<24) | (16<<16) | (sck_ratio<<8) | (bck_ratio<<0), state->base + LS1X_IIS_CONFIG);
	writel(0xc220, state->base + LS1X_IIS_CONTROL);
}

static int ls1x_audio_probe(struct platform_device *pdev)
{
	struct ls1x_audio_state *state = &ls1x_audio_state;
	struct resource *res;
	int rc;

	order_addr_in = ioremap(ORDER_ADDR_IN, 0x4);
	memset(state, 0, sizeof(struct ls1x_audio_state));

	state->input_stream = &input_stream;
	state->output_stream = &output_stream;
	sema_init(&state->sem, 1);

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
		rc = -ENOMEM;
		goto err_dev0;
	}

	if ((state->dev_audio = register_sound_dsp(&ls1x_dsp_fops, -1)) < 0){
		printk("error!\n");
		goto err_dev1;
	}

	ls1x_iis_config(state);

	return 0;

err_dev1:
	iounmap(state->base);
	release_mem_region(res->start, resource_size(res));
err_dev0:

	return rc;
}

static int __devexit ls1x_audio_remove(struct platform_device *pdev)
{
	struct ls1x_audio_state *state = &ls1x_audio_state;
	struct resource *res;

	unregister_sound_dsp(state->dev_audio);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	iounmap(state->base);
	release_mem_region(res->start, resource_size(res));

	return 0;
}

#ifdef CONFIG_PM
static int ls1x_audio_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int ls1x_audio_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define ls1x_audio_suspend NULL
#define ls1x_audio_resume NULL
#endif

static struct platform_driver ls1x_audio_driver = {
	.driver = {
		.name	= "ls1x-audio",
		.owner	= THIS_MODULE,
	},
	.probe		= ls1x_audio_probe,
	.remove		= ls1x_audio_remove,
	.suspend	= ls1x_audio_suspend,
	.resume		= ls1x_audio_resume,
};


#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
#include <linux/i2c.h>
#define UDA1342 0
#define ES8388 1

static ssize_t show_volume(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	u8 volume_l, volume_r;

	volume_l = i2c_smbus_read_byte_data(client, 0x1a);
	volume_r = i2c_smbus_read_byte_data(client, 0x1b);

	return sprintf(buf, "%d\n", (u8)(~volume_r));	/* 把值取反，es8388值愈大音量越小 */
}

static ssize_t set_volume(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	u8 volume_l, volume_r;

	volume_l = (u8)simple_strtol(buf, NULL, 10);
	if (volume_l > 255) {
		volume_l = 255;
	}
	volume_l = (u8)(~volume_l);/* 把值取反，es8388值愈大音量越小 */
	volume_r = volume_l;

	i2c_smbus_write_byte_data(client, 0x1a, volume_r);
	i2c_smbus_write_byte_data(client, 0x1b, volume_r);

	return count;
}

static DEVICE_ATTR(volume, S_IRUGO | S_IWUSR, show_volume, set_volume);

static int write_reg(struct i2c_client *client, int reg, int value)
{
	/* UDA1342 wants MSB first, but SMBus sends LSB first */
	i2c_smbus_write_word_data(client, reg, swab16(value));
	return 0;
}

static __devinit int i2c_codecs_probe(struct i2c_client *client,
				      const struct i2c_device_id *id)
{
	int err;

	if (id->driver_data == UDA1342) {
		pr_info("audio codec:uda1342\n");
		write_reg(client, 0x00, 0x8000); /* reset registers */
		udelay(500);
		write_reg(client, 0x00, 0x1241); /* select input 1 */

		write_reg(client, 0x00, 0x5c42);
		write_reg(client, 0x01, 0x0004);
		write_reg(client, 0x10, 0x0002);
		write_reg(client, 0x11, 0x0000);
		write_reg(client, 0x12, 0x0000);
		write_reg(client, 0x20, 0x0f30);
		write_reg(client, 0x21, 0x0f30);
	} else if (id->driver_data == ES8388) {	/* 需要i2c总线时钟设置为100KHz */
		pr_info("audio codec:es8388\n");
		i2c_smbus_write_byte_data(client, 0x08, 0x00);
		i2c_smbus_write_byte_data(client, 0x2b, 0x80);
		i2c_smbus_write_byte_data(client, 0x00, 0x32);
		i2c_smbus_write_byte_data(client, 0x01, 0x72);
		i2c_smbus_write_byte_data(client, 0x03, 0x00);
		i2c_smbus_write_byte_data(client, 0x04, 0x3c);
		i2c_smbus_write_byte_data(client, 0x09, 0x00);
		i2c_smbus_write_byte_data(client, 0x0a, 0x00);
		i2c_smbus_write_byte_data(client, 0x0c, 0x0c);
		i2c_smbus_write_byte_data(client, 0x0d, 0x02);
		i2c_smbus_write_byte_data(client, 0x0f, 0x70);
		i2c_smbus_write_byte_data(client, 0x10, 0x00);
		i2c_smbus_write_byte_data(client, 0x11, 0x00);
		i2c_smbus_write_byte_data(client, 0x17, 0x18);
		i2c_smbus_write_byte_data(client, 0x18, 0x02);
		i2c_smbus_write_byte_data(client, 0x19, 0x72);
		i2c_smbus_write_byte_data(client, 0x1a, 0x00);
		i2c_smbus_write_byte_data(client, 0x1b, 0x00);
		i2c_smbus_write_byte_data(client, 0x26, 0x00);
		i2c_smbus_write_byte_data(client, 0x27, 0xb8);
		i2c_smbus_write_byte_data(client, 0x28, 0x38);
		i2c_smbus_write_byte_data(client, 0x29, 0x38);
		i2c_smbus_write_byte_data(client, 0x2a, 0xd0);
		i2c_smbus_write_byte_data(client, 0x2e, 0x21);
		i2c_smbus_write_byte_data(client, 0x2f, 0x21);
		i2c_smbus_write_byte_data(client, 0x30, 0x21);
		i2c_smbus_write_byte_data(client, 0x31, 0x21);
		i2c_smbus_write_byte_data(client, 0x02, 0x00);
	} else {
		pr_info("no audio codec\n");
	}

	err = sysfs_create_file(&client->dev.kobj, &dev_attr_volume.attr);
	if (err)
		return err;

	return 0;
}

static int __devexit i2c_codecs_remove(struct i2c_client *i2c)
{
	return 0;
}

static const struct i2c_device_id i2c_codecs_id[] = {
	{ "uda1342", UDA1342, },
	{ "es8388", ES8388, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, i2c_codecs_id);

static struct i2c_driver i2c_codecs_driver = {
	.driver = {
		.name =  "i2c-codec",
		.owner = THIS_MODULE,
	},
	.probe =    i2c_codecs_probe,
	.remove =   __devexit_p(i2c_codecs_remove),
	.id_table = i2c_codecs_id,
};
#endif


static int __init ls1x_audio_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&ls1x_audio_driver);
	if (ret)
		printk(KERN_ERR "failed to register ls1x-audio\n");

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	ret = i2c_add_driver(&i2c_codecs_driver);
	if (ret != 0)
		pr_err("Failed to register I2C codecs driver: %d\n", ret);		
#endif

	return ret;
}

static void __exit ls1x_audio_exit(void)
{
	platform_driver_unregister(&ls1x_audio_driver);
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&i2c_codecs_driver);
#endif
}

module_init(ls1x_audio_init);
module_exit(ls1x_audio_exit);

#ifndef MODULE

static int __init ls1x_setup(char *options)
{
	char           *this_opt;

	if (!options || !*options)
		return 0;

	while ((this_opt = strsep(&options, ","))) {
		if (!*this_opt)
			continue;
		if (!strncmp(this_opt, "vra", 3)) {
			vra = 1;
		}
	}

	return 1;
}

__setup("ls1x_audio=", ls1x_setup);

#endif /* MODULE */

MODULE_LICENSE("GPL");

