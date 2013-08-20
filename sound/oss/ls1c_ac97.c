/*
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

#include "ls1x_ac97.h"
#include <irq.h>
#include <loongson1.h>
///#define	CONFIG_SND_SB2F_TIMER	1

void __iomem *order_addr_in;
static int codec_reset = 1;

static irqreturn_t ac97_dma_write_intr(int irq, void *private);
static irqreturn_t ac97_dma_read_intr(int irq, void *private);

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

unsigned short reg1[7] = {
        //    0x1c02,
        //    0x1a02,
        0x1402,
        //    0x1402,
        //    0x1002,
        //    0x1202,
        0x0014,
        //    0x0010,
        //    0xc003,
        //    0xff63,
        //    0xff00,
        0xff03,
        //    0xfc00,
        //    0xcf83,
        0x0000,
        0x0000,
        0x0030,
        0x0030,
        //    0x0230,
        //    0x0230,
        //    0x0830,
        //    0x0830,
};

#define uda_address     /*0x1fe70000	*/	0x34

#define uda_dev_addr    0x34

#define uda1342_base    0xbfe60000
#define DMA_BUF 0x00800000

#define IISVERSION  (volatile unsigned int *)(uda1342_base + 0x0)
#define IISCONFIG   (volatile unsigned int *)(uda1342_base + 0x4)
#define IISSTATE  (volatile unsigned int *)(uda1342_base + 0x8)

#define I2C_SINGLE 0 
#define I2C_BLOCK 1
#define I2C_SMB_BLOCK 2
#define I2C_HW_SINGLE 3 //lxy 

#define GS_SOC_I2C_BASE    0xbfe58000           //0x1fe58000
//#define GS_SOC_I2C_BASE    0xbfe70000 //IIC2's base.
#define GS_SOC_I2C_PRER_LO (volatile unsigned char *)(GS_SOC_I2C_BASE + 0x0)
#define GS_SOC_I2C_PRER_HI (volatile unsigned char *)(GS_SOC_I2C_BASE + 0x1)
#define GS_SOC_I2C_CTR     (volatile unsigned char *)(GS_SOC_I2C_BASE + 0x2)
#define GS_SOC_I2C_TXR     (volatile unsigned char *)(GS_SOC_I2C_BASE + 0x3)
#define GS_SOC_I2C_RXR     (volatile unsigned char *)(GS_SOC_I2C_BASE + 0x3)
#define GS_SOC_I2C_CR      (volatile unsigned char *)(GS_SOC_I2C_BASE + 0x4)
#define GS_SOC_I2C_SR      (volatile unsigned char *)(GS_SOC_I2C_BASE + 0x4)

#define CR_START 0x80
#define CR_WRITE 0x10
#define SR_NOACK 0x80
#define CR_STOP  0x40
#define SR_BUSY  0x40 
#define CR_START 0x80
#define SR_TIP   0x2

#define CR_READ  0x20
#define I2C_WACK 0x8

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
//	struct ls1x_audio_state *state = &ls1x_audio_state;
	u32 val;
	int timeout = 20000;
	unsigned long flags;

	val = desc->snd_dma_handle;
	val |= s->output ? 0x9 : 0xa;
	local_irq_save(flags);
	writel(val, order_addr_in);
//	while(readl(order_addr_in) & 0x8);
	while ((readl(order_addr_in) & 0x8) && (timeout-- > 0)) {
		udelay(5);
	}
	local_irq_restore(flags);
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

	control = s->output ? 0x1fe60010 : 0x1fe6000c ;

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
	if(!s->ask_dma) {
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
	unsigned long flags;

	if (list_empty(&s->run_list))
		return IRQ_HANDLED;

	local_irq_save(flags);
	writel(s->ask_dma_handle | 0x6, order_addr_in);
	while (readl(order_addr_in) & 4) {
	}
	local_irq_restore(flags);

	do {
		desc = list_entry(s->run_list.next, struct audio_dma_desc, link);
		if (s->ask_dma->ordered == desc->snd.ordered)
			break;

//		spin_lock(&s->lock);
		list_del(&desc->link);
		list_add_tail(&desc->link, &s->done_list);
//		spin_unlock(&s->lock);
	} while(!list_empty(&s->run_list));

	if (!list_empty(&s->done_list))
		wake_up(&s->frag_wq);
	return IRQ_HANDLED;
}

static irqreturn_t ac97_dma_write_intr(int irq, void *private)
{
	struct audio_stream *s = (struct audio_stream *)private;
	struct audio_dma_desc *desc;
	unsigned long flags;
	
	if (list_empty(&s->run_list))
		return IRQ_HANDLED;

	local_irq_save(flags);
	writel(s->ask_dma_handle | 0x5, order_addr_in);
	while (readl(order_addr_in) & 4) {
	}
	local_irq_restore(flags);

	do {
		desc = list_entry(s->run_list.next, struct audio_dma_desc, link);
		/*first desc's ordered may be null*/
		if(s->ask_dma->ordered == desc->snd.ordered || s->ask_dma->ordered == 
			((desc->snd_dma_handle + sizeof(struct dma_desc)) | DMA_ORDERED_EN))
			break;
//		spin_lock(&s->lock);
		list_del(&desc->link);
		desc->pos = 0;
		list_add_tail(&desc->link, &s->free_list);
//		spin_unlock(&s->lock);
	} while(!list_empty(&s->run_list));

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
		if(list_empty(&s->free_list)) {
			if(file->f_flags & O_NONBLOCK)
				return -EAGAIN;

			if(wait_event_interruptible(s->frag_wq, !list_empty(&s->free_list))) {
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
			if(wait_event_interruptible(s->frag_wq, !list_empty(&s->done_list))) {
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
	}

	return 0;	//ls1x_ioctl_mixdev(file, cmd, arg);
}


int i2c_rec_haf_word(unsigned char *addr,int addrlen,unsigned char reg,unsigned short* buf ,int count)
{
        int i;
        int j;
        unsigned char value;
        printk ("in %s !\n", __func__);
        for(i=0;i<count;i++)
        {
                for(j=0;j<addrlen;j++)
                {
                        /*write slave_addr*/
                        * GS_SOC_I2C_TXR = addr[j];
                        * GS_SOC_I2C_CR  = (j == 0)? (CR_START|CR_WRITE):CR_WRITE; /* start on first addr */
                        while(*GS_SOC_I2C_SR & SR_TIP);
                        //                    printk ("device's address = 0x%x ,sr = 0x%x !\n", addr[j] ,*GS_SOC_I2C_SR);

                        if((* GS_SOC_I2C_SR) & SR_NOACK) return -1;
                }

        //      printk ("------1---->\n");
                * GS_SOC_I2C_TXR = reg++;   //lxy reg++ after read once
                * GS_SOC_I2C_CR  = CR_WRITE;
                while(*GS_SOC_I2C_SR & SR_TIP);
                if((* GS_SOC_I2C_SR) & SR_NOACK) return -1;

        //      printk ("------2---->\n");

                /*write slave_addr*/
                * GS_SOC_I2C_TXR = addr[0]|1;
                * GS_SOC_I2C_CR  = CR_START|CR_WRITE; /* start on first addr */
                while(*GS_SOC_I2C_SR & SR_TIP);

                if((* GS_SOC_I2C_SR) & SR_NOACK) return -1;

        //              printk ("------3---->\n");

                * GS_SOC_I2C_CR  = CR_READ; /*read send ack*/
                while(*GS_SOC_I2C_SR & SR_TIP);
                value = * GS_SOC_I2C_TXR;
                buf[i] =  value << 8;

        //              printk ("------4---->\n");

                * GS_SOC_I2C_CR  = CR_READ|I2C_WACK; /*last read not send ack*/
                while(*GS_SOC_I2C_SR & SR_TIP);
                value = * GS_SOC_I2C_TXR;
                buf[i] |= value;

                * GS_SOC_I2C_CR  = CR_STOP;
                while(*GS_SOC_I2C_SR & SR_TIP);
                //        * GS_SOC_I2C_SR;


        }
        while(*GS_SOC_I2C_SR & SR_BUSY);

        return count;
}



unsigned char i2c_rec_b(unsigned char *addr,int addrlen,unsigned char reg,unsigned char* buf ,int count)
{
        int i;
        int j;


        unsigned char value;

        for(j=0;j<addrlen;j++)
        {
                /*write slave_addr*/
                * GS_SOC_I2C_TXR = addr[j];
                * GS_SOC_I2C_CR  = j == 0? (CR_START|CR_WRITE):CR_WRITE; /* start on first addr */
                while(*GS_SOC_I2C_SR & SR_TIP);

                if((* GS_SOC_I2C_SR) & SR_NOACK) return i;
        }


        * GS_SOC_I2C_TXR = reg;
        * GS_SOC_I2C_CR  = CR_WRITE;
        while(*GS_SOC_I2C_SR & SR_TIP);
        if((* GS_SOC_I2C_SR) & SR_NOACK) return i;

        * GS_SOC_I2C_TXR = addr[0]|1;
        * GS_SOC_I2C_CR  = CR_START|CR_WRITE;
        if((* GS_SOC_I2C_SR) & SR_NOACK) return i;

        for(i=0;i<count;i++)
        {
                * GS_SOC_I2C_CR  = CR_READ;
                while(*GS_SOC_I2C_SR & SR_TIP);

                buf[i] = * GS_SOC_I2C_TXR;
        }
        * GS_SOC_I2C_CR  = CR_STOP;

        return count;
}

unsigned char i2c_rec_s(unsigned char *addr,int addrlen,unsigned char reg,unsigned char* buf ,int count)
{
        int i;
        int j;
        unsigned char value;
        for(i=0;i<count;i++)
        {
                for(j=0;j<addrlen;j++)
                {
                        /*write slave_addr*/
                        * GS_SOC_I2C_TXR = addr[j];
                        * GS_SOC_I2C_CR  = (j == 0)? (CR_START|CR_WRITE):CR_WRITE; /* start on first addr */
                        while(*GS_SOC_I2C_SR & SR_TIP);

                        if((* GS_SOC_I2C_SR) & SR_NOACK) return i;
                }

                * GS_SOC_I2C_TXR = reg;
                * GS_SOC_I2C_CR  = CR_WRITE;
                while(*GS_SOC_I2C_SR & SR_TIP);
                if((* GS_SOC_I2C_SR) & SR_NOACK) return i;

                /*write slave_addr*/
                * GS_SOC_I2C_TXR = addr[0]|1;
                * GS_SOC_I2C_CR  = CR_START|CR_WRITE; /* start on first addr */
                while(*GS_SOC_I2C_SR & SR_TIP);

                if((* GS_SOC_I2C_SR) & SR_NOACK) return i;

                * GS_SOC_I2C_CR  = CR_READ|I2C_WACK; /*last read not send ack*/
                while(*GS_SOC_I2C_SR & SR_TIP);

                buf[i] = * GS_SOC_I2C_TXR;
                * GS_SOC_I2C_CR  = CR_STOP;
                * GS_SOC_I2C_SR;

        }
        return count;
}


int tgt_i2cinit()
{
        *(volatile unsigned int *)(0xbfd010c0) = 0x0;
        *(volatile unsigned int *)(0xbfd010c4) = 0x0;
        *(volatile unsigned int *)(0xbfd010c8) = 0x0;

        static int inited=0;
        if(inited)
                return 0;
        inited=1;
        * GS_SOC_I2C_PRER_LO = 0x64;
        * GS_SOC_I2C_PRER_HI = 0;
        * GS_SOC_I2C_CTR = 0x80;
}

/*
 * 0 single: Ã¿´Î¶ÁÒ»¸ö
 * 1 smb block
 */
int tgt_i2cread(int type,unsigned char *addr,int addrlen,unsigned char reg,unsigned char *buf,int count)
{
        int i;
        tgt_i2cinit();
        memset(buf,-1,count);
        //printk ("in %s !\n", __func__);
        switch(type)
        {
                case I2C_SINGLE:
                        return i2c_rec_s(addr,addrlen,reg,buf,count);
                        break;
                case I2C_BLOCK:
                        return i2c_rec_b(addr,addrlen,reg,buf,count);
                        break;
                case I2C_HW_SINGLE:
                        return i2c_rec_haf_word(addr,addrlen,reg,(unsigned short *)buf,count);
                        break;

                default: return 0;break;
        }
        return 0;
}

unsigned char i2c_send_s(unsigned char *addr,int addrlen,unsigned char reg,unsigned char * buf ,int count)
{
        int i;
        int j;
        for(i=0;i<count;i++)
        {
                for(j=0;j<addrlen;j++)
                {
                        /*write slave_addr*/
                        * GS_SOC_I2C_TXR = addr[j];
                        * GS_SOC_I2C_CR  = j == 0? (CR_START|CR_WRITE):CR_WRITE; /* start on first addr */
                        while(*GS_SOC_I2C_SR & SR_TIP);

                        if((* GS_SOC_I2C_SR) & SR_NOACK) return i;
                }

                * GS_SOC_I2C_TXR = reg;
                * GS_SOC_I2C_CR  = CR_WRITE;
                while(*GS_SOC_I2C_SR & SR_TIP);
                if((* GS_SOC_I2C_SR) & SR_NOACK) return i;

                * GS_SOC_I2C_TXR = buf[i];
                * GS_SOC_I2C_CR = CR_WRITE|CR_STOP;
                while(*GS_SOC_I2C_SR & SR_TIP);

                if((* GS_SOC_I2C_SR) & SR_NOACK) return i;
        }
        while(*GS_SOC_I2C_SR & SR_BUSY);
        return count;
}

unsigned char i2c_send_b(unsigned char *addr,int addrlen,unsigned char reg,unsigned char * buf ,int count)
{
        int i;
        int j;
        for(j=0;j<addrlen;j++)
        {
                /*write slave_addr*/
                * GS_SOC_I2C_TXR = addr[j];
                * GS_SOC_I2C_CR  = j == 0? (CR_START|CR_WRITE):CR_WRITE; /* start on first addr */
                while(*GS_SOC_I2C_SR & SR_TIP);

                if((* GS_SOC_I2C_SR) & SR_NOACK) return i;
        }


        * GS_SOC_I2C_TXR = reg;
        * GS_SOC_I2C_CR  = CR_WRITE;
        while(*GS_SOC_I2C_SR & SR_TIP);
        if((* GS_SOC_I2C_SR) & SR_NOACK) return i;

        for(i=0;i<count;i++)
        {
                * GS_SOC_I2C_TXR = buf[i];
                * GS_SOC_I2C_CR = CR_WRITE;
                while(*GS_SOC_I2C_SR & SR_TIP);

                if((* GS_SOC_I2C_SR) & SR_NOACK) return i;

        }
        * GS_SOC_I2C_CR  = CR_STOP;
        while(*GS_SOC_I2C_SR & SR_BUSY);
        return count;
}

int i2c_send_haf_word(unsigned char *addr,int addrlen,unsigned char reg,unsigned short * buf ,int count)
{
        int i;
        int j;
        unsigned char value;
        for(i=0;i<count;i++)
        {
                for(j=0;j<addrlen;j++)
                {
                        /*write slave_addr*/
                        * GS_SOC_I2C_TXR = addr[j];
                        * GS_SOC_I2C_CR  = j == 0? (CR_START|CR_WRITE):CR_WRITE; /* start on first addr */
                        while(*GS_SOC_I2C_SR & SR_TIP);

                        if((* GS_SOC_I2C_SR) & SR_NOACK) return -1;
                }

                * GS_SOC_I2C_TXR = reg++;   //lxy: reg++ after write once
                * GS_SOC_I2C_CR  = CR_WRITE;
                while(*GS_SOC_I2C_SR & SR_TIP);
                if((* GS_SOC_I2C_SR) & SR_NOACK) return -1;

                value = (buf[i] >> 8);
                * GS_SOC_I2C_TXR = value;
                * GS_SOC_I2C_CR  = CR_WRITE;
//                printk ("hi data = 0x%x ,", value);
                while(*GS_SOC_I2C_SR & SR_TIP);
                if((* GS_SOC_I2C_SR) & SR_NOACK) return -1;

                value = buf[i];
                * GS_SOC_I2C_TXR = value;
                * GS_SOC_I2C_CR  = CR_WRITE;
//                printk ("low data = 0x%x ,", value);
                while(*GS_SOC_I2C_SR & SR_TIP);
                if((* GS_SOC_I2C_SR) & SR_NOACK) return -1;

                * GS_SOC_I2C_CR = CR_WRITE|CR_STOP;
                while(*GS_SOC_I2C_SR & SR_TIP);
        }
        while(*GS_SOC_I2C_SR & SR_BUSY);
        return count;
}



int tgt_i2cwrite(int type,unsigned char *addr,int addrlen,unsigned char reg,unsigned char *buf,int count)
{
        tgt_i2cinit();
        switch(type&0xff)
        {
                case I2C_SINGLE:
                    i2c_send_s(addr,addrlen,reg,buf,count);
                    break;
                case I2C_BLOCK:
                    return i2c_send_b(addr,addrlen,reg,buf,count);
                    break;
                case I2C_SMB_BLOCK:
                    break;
                case I2C_HW_SINGLE:
                        return i2c_send_haf_word(addr,addrlen,reg,(unsigned short *)buf,count);
                        break;
                default:
                    return -1;break;
        }
        return -1;
}

void    uda1342_test()
{
        unsigned char *dev_add;
        unsigned short data;
        unsigned char reg;
        int ret = 0;

        dev_add = 0x8000bb90;
        *dev_add = uda_address;
        reg = 0x10;
        unsigned short data1[7];
        unsigned char i;

        tgt_i2cinit();

        tgt_i2cread(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&data), 1);
        printk ("data = 0x%x !\n", data);

        data = 0xfc00;
        ret = tgt_i2cwrite(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&data), 1);
        if (ret == -1)
                printk ("write error......\n");

        data = 0x0;
        ret = tgt_i2cread(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&data), 1);
        if (ret == -1)
                printk ("read error......\n");
        printk ("data = 0x%x !\n", data);

        reg = 0x0;
        ret = tgt_i2cwrite(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&reg1[0]), 1);
        reg = 0x1;
        ret = tgt_i2cwrite(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&reg1[1]), 1);
        reg = 0x10;
        ret = tgt_i2cwrite(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&reg1[2]), 1);
        reg = 0x11;
        ret = tgt_i2cwrite(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&reg1[3]), 1);
        reg = 0x12;
        ret = tgt_i2cwrite(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&reg1[4]), 1);
        reg = 0x20;
        ret = tgt_i2cwrite(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&reg1[5]), 1);
        reg = 0x21;
        ret = tgt_i2cwrite(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&reg1[6]), 1);

        reg = 0x0;
        ret = tgt_i2cread(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&data1[0]), 1);
        reg = 0x1;
        ret = tgt_i2cread(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&data1[1]), 1);
        reg = 0x10;
        ret = tgt_i2cread(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&data1[2]), 1);
        reg = 0x11;
        ret = tgt_i2cread(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&data1[3]), 1);
        reg = 0x12;
        ret = tgt_i2cread(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&data1[4]), 1);
        reg = 0x20;
        ret = tgt_i2cread(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&data1[5]), 1);
        reg = 0x21;
        ret = tgt_i2cread(I2C_HW_SINGLE, dev_add, 1, reg, (unsigned char *)(&data1[6]), 1);

        for (i=0; i<7; i++)
        {
                printk ("data1[%d] = 0x%x ---> 0x%x!\n", i, data1[i], reg1[i]);
        }
}



void    config_uda1342()
{
        unsigned char rat_cddiv;
        unsigned char rat_bitdiv;
        unsigned char dev_addr;
        unsigned short value;
#if 1
        dev_addr = uda_dev_addr;
        rat_bitdiv = 0xf;
        rat_cddiv = 0x1;

        * IISCONFIG = (16<<24) | (16<<16) | (rat_bitdiv<<8) | (rat_cddiv<<0) ;
        //    * IISSTATE = 0xc220;
        //* IISSTATE = 0xc200;
	//printk("* IISSTATE is %x\n",* IISSTATE);
        * IISSTATE = 0x0d280;	///0x0200;
        value =0x8000;
#endif
        uda1342_test();
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
                free_irq(LS1B_BOARD_DMA2_IRQ, state->input_stream);
#ifdef CONFIG_SND_SB2F_TIMER
                del_timer(&state->input_stream->timer);
#endif 
        }

        if (file->f_mode & FMODE_WRITE) {
                ls1x_audio_sync(file);
                audio_clear_buf(state->output_stream);
                state->wr_ref = 0;
                free_irq(LS1B_BOARD_DMA1_IRQ, state->output_stream);
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

	config_uda1342();

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
		request_irq(LS1B_BOARD_DMA1_IRQ, ac97_dma_write_intr, IRQF_SHARED,
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
		request_irq(LS1B_BOARD_DMA2_IRQ, ac97_dma_read_intr, IRQF_SHARED,
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
		writel(conf, state->base + OCC0);
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

static int ls1x_audio_probe(struct platform_device *pdev)
{
	struct ls1x_audio_state *state = &ls1x_audio_state;
	struct resource *res;
	int rc;

#ifdef CONFIG_LS1C_AC97_BUS
*(volatile unsigned int *)(0xbfd00420) |= (0x1 << 14);  //shut i2s
*(volatile unsigned int *)(0xbfd00424) |= (0x1 << 25);  //mux ac97/i2s
#elif CONFIG_LS1C_IIS_BUS
*(volatile unsigned int *)(0xbfd00420) |= (0x1 << 15);  //shut ac97
*(volatile unsigned int *)(0xbfd00424) &= ~(0x1 << 25);  //mux ac97/i2s

*(volatile unsigned int *)(0xbfd01044) |= (0x7 << 13); //enable dma interrupt
*(volatile unsigned int *)(0xbfd01050) &= ~(0x7 << 13); 
*(volatile unsigned int *)(0xbfd01054) |= (0x7 << 13); 

#endif

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

	if (!request_mem_region(res->start, resource_size(res), "ls1x-audio")){
		printk("request mem error\n");
		return -EBUSY;
	}

	state->base = ioremap(res->start, resource_size(res));
	if (!state->base) {
		dev_err(&pdev->dev, "ls1x-audio - failed to map controller\n");
		printk("error iomap\n");
		rc = -ENOMEM;
		goto err_dev0;
	}

	if ((state->dev_audio = register_sound_dsp(&ls1x_dsp_fops, -1)) < 0){
		printk("error!\n");
		goto err_dev1;
	}

	printk("register dsp success\n");
	return 0;

err_dev3:
	unregister_sound_mixer(state->codec->dev_mixer);
err_dev2:
	unregister_sound_dsp(state->dev_audio);
err_dev1:
	iounmap(state->base);
	release_mem_region(res->start, resource_size(res));
err_dev0:
	///ac97_release_codec(state->codec);

	return rc;
}

static int __devexit ls1x_audio_remove(struct platform_device *pdev)
{
	struct ls1x_audio_state *state = &ls1x_audio_state;
	struct resource *res;

	unregister_sound_dsp(state->dev_audio);
	unregister_sound_mixer(state->codec->dev_mixer);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	iounmap(state->base);
	release_mem_region(res->start, resource_size(res));

	///ac97_release_codec(state->codec);

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

static int __init ls1x_audio_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&ls1x_audio_driver);
	if (ret)
		printk(KERN_ERR "failed to register ls1x-audio\n");
	return ret;
}

static void __exit ls1x_audio_exit(void)
{
    platform_driver_unregister(&ls1x_audio_driver);
}

module_init(ls1x_audio_init);
module_exit(ls1x_audio_exit);

#ifndef MODULE

static int __init
ls1x_setup(char *options)
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


