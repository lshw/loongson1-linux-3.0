
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

#include "ls1b-ac97.h"
#include <irq.h>
#include <ls1b_board.h>

#define TIME_OUT 10		//HZ

#define MYDBG printk("<0>%s:%d\n",__FUNCTION__,__LINE__);

/*  Debug macros*/
#if 0
#define DPRINTK(fmt, args...) printk(KERN_ALERT "<%s>: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif


static audio_stream_t stream_in = {
	output: 0,
	fragsize:0x10000,
};

static audio_stream_t output_stream = {
	output: 1,
	fragsize:0x10000,
};

static audio_state_t sb2f_audio_state = {
	input_stream:&stream_in,
	output_stream:&output_stream,
//	sem:__MUTEX_INITIALIZER(sb2f_audio_state.sem),
	sem: __SEMAPHORE_INITIALIZER(sb2f_audio_state.sem, 1),
};

/*
 * capmode: iccr bit0,bit1, control dma capture mircophone from ac link slots.
 * 0: slot 5
 * 1: slot 3
 * 2: slot 4
 * 3: slot 5
 *
 *
 */

static int capmode;

static inline u32 read_reg(volatile u32 * reg)
{
	return (*reg);
}

static inline void write_reg(volatile u32 * reg, u32 val)
{
	*(reg) = (val);
}

/* busy wait */
int codec_wait_complete(void)
{
	int timeout = 40000;

	/* 中断状态/中断掩膜寄存器 读写完成？ */
	while ((!(read_reg(INTRAW) & INTS_CRAC_MASK)) && (timeout-- > 0))
		udelay(100);
	if (timeout > 0) {
		read_reg(CLR_CDC_RD);	/* CODEC READ 中断清除 */
		read_reg(CLR_CDC_WR);	/* CODEC WRITE 中断清除 */
	}
	return timeout > 0 ? 0 : 1;
}

static u16 sb2f_codec_read(struct ac97_codec *codec, volatile u8 reg)
{
	volatile u16 val;

	/*write crac */
	write_reg(CRAC, (CRAR_READ|CRAR_CODEC_REG(reg)));	/* Codec寄存器访问命令 */
	/* 等待完成 */
	if (codec_wait_complete() != 0) {
		DPRINTK("ERROR\n");
		return -1;
	}
	/* 返回code寄存器中读取到的数据 */
	val = read_reg(CRAC) & 0xffff;
	return val;
}

static void sb2f_codec_write(struct ac97_codec *codec, u8 reg, u16 val)
{
	write_reg(CRAC, (CRAR_WRITE | CRAR_CODEC_REG(reg) | val));
	codec_wait_complete();
}

/* dma ops */
static void dma_enable_trans(audio_stream_t * s, audio_dmadesc_t *desc)
{
	u32 val;
//	int timeout = 20000;
	unsigned long flags;

	val = desc->snd_dma_handle;
	val |= s->output ? 0x9 : 0xa;
	local_irq_save(flags);
	write_reg((volatile u32*)(CONFREG_BASE + 0x1160), val);
	while(read_reg((volatile u32*)(CONFREG_BASE + 0x1160)) & 0x8);
//	while ((read_reg((volatile u32*)(CONFREG_BASE + 0x1160)) & 0x8) && (timeout-- > 0)) {
//		udelay(5);
//	}
	local_irq_restore(flags);
	write_reg(INTM, 0x0); //INTM ;disable all interrupt
#ifdef CONFIG_SND_SB2F_TIMER
	mod_timer(&s->timer, jiffies + HZ/20);
#endif
}

/* if buffer not EMpty entry dma is running */
void audio_clear_buf(audio_stream_t *s)
{
	audio_dmadesc_t *desc;

	while (!list_empty(&s->all_list)) {
		desc = list_entry(s->all_list.next, audio_dmadesc_t, all);
		list_del(&desc->all);
		list_del(&desc->link);

		if (desc->snd_buffer)
			free_pages((unsigned long)desc->snd_buffer, get_order(s->fragsize));
		dma_free_coherent(NULL, sizeof(audio_dmadesc_t), desc, 0);
	}

	if (s->ask_dma)
		dma_free_coherent(NULL, sizeof(audio_dmadesc_t), s->ask_dma, 0);

	if (s->null_buffer)
		dma_free_coherent(NULL, ZERO_BUF_SIZE, s->null_buffer, 0);

	s->ask_dma = NULL;
	s->null_buffer = NULL;
}

static void inline link_dma_desc(audio_stream_t *s, audio_dmadesc_t *desc)
{
	spin_lock_irq(&s->lock);

	if(!list_empty(&s->run_list)) {
		audio_dmadesc_t *desc0;
		desc0 = list_entry(s->run_list.prev, audio_dmadesc_t, link);//desc0指向结构体audio_dmadesc_t的起始地址，
		desc0->snd.ordered = desc->snd_dma_handle | DESC_VALID;
		desc0->null.ordered = desc->snd_dma_handle | DESC_VALID;
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


static void dma_stop(audio_stream_t *s)
{
	u32 val;

	val = s->output ? 0x11 : 0x12;
	write_reg((volatile u32*)(CONFREG_BASE + 0x1160), val);
//	udelay(1000);
}

static void sb2f_init_dmadesc(audio_stream_t *s, audio_dmadesc_t *desc, u32 count)
{
	_audio_dma_desc *_desc;
	u32 control;

	control = s->output ? 0x0fe72420 : 0x0fe74c4c;//所要读写的apb设备的地址

	control |= (1<<31);
	/*
	* [30]-1 two channels
	* [30]-0 one channel
	*/

	if(!s->single)
		control |= (1 << 30);

	/*
	* [29:28]-00 --> byte
	* [29:28]-01 --> half word(two bytes)
	* [29:28]-10 --> word (four bytes)
	*/

	if(s->fmt == AFMT_S16_LE) {
		control |= (1 << 28);
	}

	desc->snd.daddr = desc->null.daddr = control;

	_desc = &desc->snd;
	_desc->ordered = (desc->snd_dma_handle + sizeof(_audio_dma_desc)) |DESC_VALID;
	_desc->saddr = desc->snd_dma;//指定了DMA搬运数据时的内存地址
	_desc->length = 8;
	_desc->step_length = 0;
	_desc->step_times = count >> 5;
	_desc->cmd = s->output ? 0x00001001 : 0x00000001; //enable the interrupt

	_desc = &desc->null;
	_desc->ordered =  (desc->snd_dma_handle + sizeof(_audio_dma_desc))|DESC_VALID;
	_desc->saddr = s->null_dma;
	_desc->length = 8;
	_desc->step_length = 0;
	_desc->step_times = 1;
	_desc->cmd = s->output ? 0x00001000 : 0x00000000; //enable the interrupt

}

/*
 * Fill data to dma buffer.
 * when buf_depth exceed the lowest threshold, start DMA.
 * Befor opening the DMA, we should reset DMA(stop).
 */
static u32 fill_play_buffer(audio_stream_t *s, const char *buf, u32 count)
{
	u32 copy_bytes;
	audio_dmadesc_t *desc;

	desc = list_entry(s->free_list.next, audio_dmadesc_t, link);
	copy_bytes = min((s->fragsize - desc->pos), count);
	copy_from_user((void *)(desc->snd_buffer + desc->pos), buf, copy_bytes);
	desc->pos += copy_bytes;

	if(desc->pos == s->fragsize) {
		dma_map_single(NULL, desc->snd_buffer, s->fragsize, DMA_TO_DEVICE);
		spin_lock_irq(&s->lock);
		list_del(&desc->link);
		spin_unlock_irq(&s->lock);
		sb2f_init_dmadesc(s, desc, s->fragsize);
		link_dma_desc(s, desc);
	}

	return copy_bytes;
}


/* setup buffers,dma descs,buffer. */
int sb2f_setup_buf(audio_stream_t * s)
{	
	int i;
	int nb_desc;
	void *dma_buf;
	dma_addr_t dma_phyaddr;
	audio_dmadesc_t **dma_desc = 0;

	if (s->ask_dma)
		return -EBUSY;

	nb_desc = s->nbfrags;

	for (i = 0; i < nb_desc; i++) {
		audio_dmadesc_t *desc;//每个audio_dmadesc_t用来描述一片dma buffer，其结构中的snd_dma_handle --> audio_dmadesc_t总线地址

		desc = dma_alloc_coherent(NULL, sizeof(audio_dmadesc_t),
					(dma_addr_t *)&dma_phyaddr, GFP_KERNEL);
		//建立一致性DMA映射，arg 2 为所需缓冲区的大小，函数的返回值是缓冲区的内核虚拟地址，可以被驱动程序使用，
		//而与其相关的总线地址，保存在arg 3中
		if (!desc) {
			DPRINTK("2.alloc dma desc err.");
			goto err;
		}

		memset(desc, 0, sizeof(audio_dmadesc_t));
		desc->snd_dma_handle = dma_phyaddr;
		desc->null.ordered = desc->snd.ordered = (dma_phyaddr + sizeof(_audio_dma_desc))|DESC_VALID;
		list_add_tail(&desc->link,&s->free_list);
		list_add_tail(&desc->all,&s->all_list);

		//__get_free_pages 分配若干物理连续的页面，并返回指向该内存区域第一个字节的指针
		dma_buf = (void *)__get_free_pages(GFP_ATOMIC|GFP_DMA, get_order(s->fragsize));
		desc->snd_buffer = dma_buf;//分配的用于音频数据传输的每片buffer 8192bytes
		//当有一个缓冲区要被传输的时候，使用dma_map_single函数映射它，返回值是总线地址，可以把它传递给设备
		desc->snd_dma = dma_map_single(NULL, desc->snd_buffer, s->fragsize, DMA_FROM_DEVICE);
		if (!dma_buf) {
			DPRINTK("4.alloc dma buffer err\n");
			goto err;
		}
	}

	/* dma desc for ask_valid one per audio_stream_t */
	s->ask_dma = dma_alloc_coherent(NULL, sizeof(_audio_dma_desc),
			&dma_phyaddr, GFP_KERNEL);


	if(s->ask_dma != NULL) {
		memset(s->ask_dma, 0, sizeof(_audio_dma_desc));
		s->ask_dma_handle = dma_phyaddr;
	}
	else {
		DPRINTK("3. alloc dma desc err.");
		goto err;
	}

	s->null_buffer = dma_alloc_coherent(NULL, ZERO_BUF_SIZE,
			&dma_phyaddr, GFP_KERNEL);

	memset(s->null_buffer,ZERO_BUF_SIZE,0);
	s->null_dma = dma_phyaddr;

	sema_init(&s->sem, 1);

	return 0;

err:
	printk("soc-audio: unable to allocate audio memory\n ");
	audio_clear_buf(s);
	if(dma_desc)
		kfree(dma_desc);
	return -ENOMEM;
}

#ifdef CONFIG_SND_SB2F_TIMER
static void sb2f_audio_timeout(unsigned long data)
{
	audio_stream_t *s = (audio_stream_t *)data;
	if (s->output)
		ac97_dma_write_intr(0, s);
	else
		ac97_dma_read_intr(0, s);
	mod_timer(&s->timer, jiffies + HZ/20);
}
#endif


/* ac97  dma intr. */
static irqreturn_t ac97_dma_read_intr(int irq, void *private)
{
	audio_stream_t *s = (audio_stream_t *) private;
	audio_dmadesc_t *desc;
	unsigned long flags;

	if(list_empty(&s->run_list)) {
		return IRQ_HANDLED;
	}

	local_irq_save(flags);
	write_reg((volatile u32*)(CONFREG_BASE + 0x1160), s->ask_dma_handle|0x6);
	while (read_reg((volatile u32*)(CONFREG_BASE + 0x1160)) & 4);
	local_irq_restore(flags);

	do {
		desc = list_entry(s->run_list.next, audio_dmadesc_t, link);
		if(s->ask_dma->ordered == desc->snd.ordered)
			break;

		spin_lock(&s->lock);
		list_del(&desc->link);
		list_add_tail(&desc->link, &s->done_list);
		spin_unlock(&s->lock);
		dma_map_single(NULL, desc->snd_buffer, s->fragsize, DMA_FROM_DEVICE);
	}
	while(!list_empty(&s->run_list));// 一直运行直到s->run_list为空为止

	if(!list_empty(&s->done_list))
		wake_up(&s->frag_wq);
	return IRQ_HANDLED;
}


/* handle the ac97 write interrupt
 * s->tail point to the dma desc currently used
 * s->head point to the dma which will be filled
 */
static irqreturn_t ac97_dma_write_intr(int irq, void *private)
{
	audio_stream_t *s = (audio_stream_t *) private;
	audio_dmadesc_t *desc;
	unsigned long flags;

	if(list_empty(&s->run_list)) {
		return IRQ_HANDLED;
	}

	local_irq_save(flags);
	write_reg((volatile u32*)(CONFREG_BASE + 0x1160), s->ask_dma_handle|0x5);
	while(read_reg((volatile u32*)(CONFREG_BASE + 0x1160)) & 4);
	local_irq_restore(flags);

	do {
		desc = list_entry(s->run_list.next, audio_dmadesc_t, link);

		/*first desc's ordered may be null*/
		if(s->ask_dma->ordered == desc->snd.ordered || s->ask_dma->ordered == 
			((desc->snd_dma_handle + sizeof(_audio_dma_desc)) |DESC_VALID))
			break;

		desc = list_entry(s->run_list.next, audio_dmadesc_t, link);
		spin_lock(&s->lock);
		list_del(&desc->link);
		desc->pos = 0;
		list_add_tail(&desc->link, &s->free_list);
		spin_unlock(&s->lock);
	}
	while(!list_empty(&s->run_list));

	if(!list_empty(&s->free_list))
		wake_up(&s->frag_wq);

	return IRQ_HANDLED;
}

/* 外部code寄存器读写函数 */
struct ac97_codec sb2f_ac97_codec = {
	codec_read:sb2f_codec_read,
	codec_write:sb2f_codec_write,
};

static long mixer_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	int val = 0;

	ret = sb2f_ac97_codec.mixer_ioctl(&sb2f_ac97_codec, cmd, arg);

	/* We must snoop for some commands to provide our own extra processing */
	switch (cmd) {
		case SOUND_MIXER_WRITE_VOLUME:
			if (get_user(val, (int*)arg)) {
				return -EFAULT;
			}
			sb2f_codec_write(NULL, 0x2, val);		
			break;
		case SOUND_MIXER_READ_VOLUME:
			val = sb2f_codec_read(NULL, 0x2);
			return put_user(val, (long*)arg);		
			break;
		case SOUND_MIXER_WRITE_MUTE:
			if (get_user(val, (int*)arg)) {
				return -EFAULT;
			}
			sb2f_codec_write(NULL, 0x18, (sb2f_codec_read(NULL,0x18) & 0<<15)| val);		
			break;
		case SOUND_MIXER_WRITE_IGAIN:
			if (get_user(val, (int*)arg)) {
				return -EFAULT;
			}
			sb2f_codec_write(NULL, 0x1c, val);		
			break;
		case SOUND_MIXER_WRITE_PCM:
			if (get_user(val, (int*)arg)) {
				return -EFAULT;
			}
			sb2f_codec_write(NULL, 0x18, val);		
			break;
		case SOUND_MIXER_WRITE_RECSRC:
			printk("record source\n");
			if (get_user(val, (int*)arg)) {
				return -EFAULT;
			}
			sb2f_codec_write(NULL, 0x1a, val);		
			break;
		default:
			break;
	}
	return 0;
}

static struct file_operations sb2f_mixer_fops = {
	unlocked_ioctl:mixer_ioctl,
	llseek:no_llseek,
	owner:THIS_MODULE
};

static int inline sanity_check_codec_status(audio_stream_t *s)
{
	u16 val;

	val = sb2f_codec_read(NULL, 0x26);
	if(val == 0xf)
		return 0;
	else 
		return -ENODEV;
}

static void sb2f_open_set(int mode)
{
	audio_state_t *state = &sb2f_audio_state;
	audio_stream_t *is = state->input_stream;
	audio_stream_t *os = state->output_stream;
	unsigned short sample_rate=0xac44;

	write_reg(INTM, 0x0); //INTM ;disable all interrupt

	if (mode & FMODE_READ) {
		write_reg(ICCR, ((is->fmt == AFMT_S16_LE)?0x690000:0x610000)|capmode);//ICCR
//		sb2f_codec_write(NULL, 0x32, sample_rate);//|(0x32<<16)|(0<<31));     //pcm input rate .
		sb2f_codec_write(NULL, 0x34, sample_rate);//|(0x34<<16)|(0<<31));     //MIC rate.
	}

	if (mode & FMODE_WRITE) {
//		write_reg(OCCR, (os->fmt == AFMT_S16_LE)?0x6b6b:0x6363);
		write_reg(OCCR, (os->fmt == AFMT_S16_LE)?0x6969:0x6161);
		sb2f_codec_write(NULL, 0x2c, sample_rate);
		if (sb2f_ac97_codec.model == 0x414c4760)
			sb2f_codec_write(NULL, 0x6a, sb2f_codec_read(NULL,0x6a)|0x201);
	}
	sb2f_codec_write(NULL,0x02,0x8000); 
}

static void sb2f_def_set(void)
{
	static unsigned short sample_rate = 0xac44;

	/*ac97 config*/
//	write_reg(OCCR,0x6b6b); //OCCR0   L&& R enable ; 3/4 empty; dma enabled;8 bits;var rate(0x202);
	write_reg(OCCR, 0x6969);
	write_reg(ICCR, 0x690000 | capmode);//ICCR
//	write_reg(INTM,0xFFFFFFFF); //INTM ;disable all interrupt
	write_reg(INTM, 0x0);

	sb2f_codec_write(NULL, 0, 0);//codec reset
	sb2f_codec_write(NULL, 0x2, 0x0808);//|(0x2<<16)|(0<<31));      //Master Vol.
	sb2f_codec_write(NULL, 0x4, 0x0808);//|(0x2<<16)|(0<<31));      //headphone Vol.
	sb2f_codec_write(NULL, 0x6, 0x0008);//|(0x2<<16)|(0<<31));      //mono Vol.
	sb2f_codec_write(NULL, 0xc, 0x0008);//|(0x2<<16)|(0<<31));      //phone Vol.
	sb2f_codec_write(NULL, 0x18, 0x0808);//|(0x18<<16)|(0<<31));     //PCM Out Vol.
//	sb2f_codec_write(NULL, 0x18, sample_rate|(0x2c<<16)|(0<<31));
//	sb2f_codec_write(NULL, 0x18, sample_rate|(0x34<<16)|(0<<31));
//	sb2f_codec_write(NULL, 0x2a, 1);//0x1|(0x2A<<16)|(0<<31));        //Extended Audio Status  and control
	sb2f_codec_write(NULL, 0x2a, 0x3df2);//0x1|(0x2A<<16)|(0<<31));        //Extended Audio Status  and control
	sb2f_codec_write(NULL, 0x1a, 0x0);//        //select record
	sb2f_codec_write(NULL, 0x1c, 0x0f0f);//cys
	sb2f_codec_write(NULL, 0x2c, sample_rate);//|(0x2c<<16)|(0<<31));     //PCM Out rate
	sb2f_codec_write(NULL, 0x32, sample_rate);//|(0x32<<16)|(0<<31));     //pcm input rate .
	sb2f_codec_write(NULL, 0x34, sample_rate);//|(0x34<<16)|(0<<31));     //MIC rate.
	sb2f_codec_write(NULL, 0x0E, 0x0);//|(0x0E<<16)|(0<<31));     //Mic vol .
	sb2f_codec_write(NULL, 0x1c, 0x0f0f);//     //adc record gain
	sb2f_codec_write(NULL, 0x10, 0x0101);//     //line in gain
	sb2f_codec_write(NULL, 0x1E, 0x0808);//|(0x1E<<16)|(0<<31));     //MIC Gain ADC.
	sb2f_codec_write(NULL, 0x6a, 0x201);
//	sb2f_codec_write(NULL, 0x38, 0x8008);
//	sb2f_codec_write(NULL, 0x64, 0x800e);
//	sb2f_codec_write(NULL, 0x66, 0x800e);
}

static int sb2f_audio_write(struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
	const char *buffer0 = buffer;
	audio_state_t *state = (audio_state_t *) file->private_data;
	audio_stream_t *s = state->output_stream;
	unsigned int ret = 0;

	sb2f_codec_write(NULL, 0x2, 0x0);
	if (*ppos != file->f_pos) {
		return -ESPIPE;
	}

	if (!s->ask_dma && sb2f_setup_buf(s)) {
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

static int sb2f_copy_to_user(audio_stream_t *s, char *buffer, u32 count)
{
	audio_dmadesc_t *desc;
	int ret = 0;

	while(!list_empty(&s->done_list) && count) {
		u32 left;
		desc = list_entry(s->done_list.next, audio_dmadesc_t, link);
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


static int sb2f_audio_read(struct file *file, char *buffer, size_t count, loff_t * ppos)
{
	char *buffer0 = buffer;
	audio_state_t *state = file->private_data;
	audio_stream_t *s = state->input_stream;
	audio_dmadesc_t *desc;

	if (*ppos != file->f_pos)
		return -ESPIPE;

	if (!s->ask_dma && sb2f_setup_buf(s))
		return -ENOMEM;

	/*3. sem:get resource .if 0 ,wait */
	if (file->f_flags & O_NONBLOCK) {
		if (down_trylock(&s->sem))
			return -EAGAIN;
	} else {
		if (down_interruptible(&s->sem))
			return -ERESTARTSYS;
	}

	while(count > 0) {
		int ret;

		while(!list_empty(&s->free_list)) {
			desc = list_entry(s->free_list.next, audio_dmadesc_t, link);
			spin_lock_irq(&s->lock);
			list_del(&desc->link);
			spin_unlock_irq(&s->lock);
			sb2f_init_dmadesc(s, desc, s->fragsize);
			link_dma_desc(s, desc);
		}

		/* record's buffer is empty */
		while(list_empty(&s->done_list)) {
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
		ret = sb2f_copy_to_user(s, buffer, count);
		count -= ret;
		buffer += ret;
	}

	while(!list_empty(&s->free_list)) {
		desc = list_entry(s->free_list.next, audio_dmadesc_t, link);
		list_del(&desc->link);
		sb2f_init_dmadesc(s, desc, s->fragsize);
		link_dma_desc(s, desc);
	}

	up(&s->sem);
	return (buffer - buffer0);
}

static int sb2f_audio_sync(struct file *file)
{
	audio_state_t *state = file->private_data;
	audio_stream_t *is = state->input_stream;    
        audio_stream_t *os = state->output_stream; 

	if (file->f_mode & FMODE_READ) {
		if(is->state == STOP && !list_empty(&is->run_list)) {
			audio_dmadesc_t *desc;
			desc = list_entry(is->run_list.next, audio_dmadesc_t, link);
			dma_enable_trans(is, desc);
			is->state = RUN;
		}

		if(!list_empty(&is->run_list))
	 	schedule_timeout(CONFIG_HZ*2);

		dma_stop(is);
	}
	if (file->f_mode & FMODE_WRITE) {
		if(os->state == STOP && !list_empty(&os->run_list)) {
			audio_dmadesc_t *desc;
			desc = list_entry(os->run_list.next, audio_dmadesc_t, link);
			dma_enable_trans(os, desc);
			os->state = RUN;
		}

		if(!list_empty(&os->run_list))
		 schedule_timeout(CONFIG_HZ*2);

		dma_stop(os);
	}

	return 0;
}

static unsigned int sb2f_audio_poll(struct file *file, struct poll_table_struct *wait)
{
	audio_state_t *state = file->private_data;
	audio_stream_t *is = state->input_stream;
	audio_stream_t *os = state->output_stream;
	unsigned int mask = 0;

	poll_wait(file, &is->frag_wq, wait);
	poll_wait(file, &os->frag_wq, wait);

	if (file->f_mode & FMODE_READ) {
		if (!is->ask_dma && sb2f_setup_buf(is))
			return -ENOMEM;
		poll_wait(file, &is->frag_wq, wait);
		if(!list_empty(&is->done_list)) {
			mask |= POLLIN | POLLRDNORM;
		}
	}

	if (file->f_mode & FMODE_WRITE) {
		if (!os->ask_dma && sb2f_setup_buf(os))
			return -ENOMEM;
		poll_wait(file, &os->frag_wq, wait);
		if(!list_empty(&os->free_list)) {
			mask |= POLLOUT | POLLWRNORM;
		}
	}
	return mask;
}

static long sb2f_audio_ioctl(struct file *file, uint cmd, ulong arg)
{
	audio_state_t *state = file->private_data;
	audio_stream_t *os = state->output_stream;
	audio_stream_t *is = state->input_stream;
	int rd=0, wr=0, val=0;
	if (file->f_mode & FMODE_WRITE)
		wr = 1;
	if (file->f_mode & FMODE_READ)
		rd = 1;

	switch (cmd) {
		case SNDCTL_DSP_SETFRAGMENT:
			if (get_user(val, (int *)arg)) {
				return -EFAULT;
			}
			DPRINTK ("DSP_SETFRAGMENT, val==%d\n", val);

			if (rd) {
				DPRINTK ("old fragsize=%d,nbfrags=%d\n",is->fragsize,is->nbfrags);
				is->fragsize = 1 << (val & 0xFFFF);
				if(is->fragsize < 1024) is->fragsize = 1024;
				is->nbfrags = (val >> 16) & 0xFFFF;
				if(is->nbfrags < 4) is->nbfrags = 4;
				DPRINTK ("new fragsize=%d,nbfrags=%d\n",is->fragsize,is->nbfrags);
			}

			if (wr) {
				DPRINTK ("old fragsize=%d,nbfrags=%d\n",os->fragsize,os->nbfrags);
				os->fragsize = 1 << (val & 0xFFFF);
				if(os->fragsize < 1024) os->fragsize = 1024;
				os->nbfrags = (val >> 16) & 0xFFFF;
				if(os->nbfrags < 4) os->nbfrags = 4;
				if(os->single) {
					//os->fragsize >>= 1;
					os->nbfrags >>=2;
					if(os->nbfrags <2) os->nbfrags=2;
				}
				DPRINTK ("new fragsize=%d,nbfrags=%d\n",os->fragsize,os->nbfrags);
			}

			return 0;
		case OSS_GETVERSION:
			return put_user(SOUND_VERSION, (int *) arg);

		case SNDCTL_DSP_GETBLKSIZE:
			if (file->f_mode & FMODE_WRITE)
				return put_user(os->fragsize, (int *) arg);
			else
				return put_user(is->fragsize, (int *) arg);
		case SNDCTL_DSP_SPEED:
			if (get_user(val, (int *) arg))
				return -EFAULT;
			if (rd) {
				if (val >= 0) {
					if (val > 48000)
						val = 48000;
					if (val <8000)
						val = 8000;
					sb2f_codec_write(NULL,0x32,val); //ADC rate .
					sb2f_codec_write(NULL,0x34, val); //MIC rate.
				}
			}
			if (wr) {
				if (val >= 0) {
					if (val > 48000)
						val = 48000;
					if (val < 8000)
						val = 8000;
					sb2f_codec_write(NULL,0x2c,val);//DAC rate .
				}
			}
			//FIXME: no recode the speed;
			if (val<=0) {
				if (file->f_mode & FMODE_WRITE)
					val = 8000;
				else if (file->f_mode & FMODE_READ)
					val = 8000;
			}
			if(wr) os->rate = val;
			if(rd) is->rate = val;
			return put_user(val, (int *) arg);


		case  SNDCTL_DSP_CHANNELS:
			if (get_user(val, (int *) arg))
				return -EFAULT;
			DPRINTK("channel=%d.\n",val);
			if (val!=0) {
				if (wr) {
					int single=os->single;
					os->single=(val==1)?1:0;
					if(!single && os->single) {
						//os->fragsize >>= 1;
						//os->nbfrags >>=2;
						//if(os->nbfrags <2) os->nbfrags=2;
					}
				}
				else if (rd) {
					if(val!=1)
						return -EFAULT;
				}
			}
			else {
				if (wr)
					val=os->single?1:2;	
				else if (rd) 
					val=1;
			}
			return put_user(val,(int * )arg);	

		case SNDCTL_DSP_STEREO: 
			if (get_user(val, (int *)arg)) {
				return -EFAULT;
				break;
			}
			DPRINTK ("DSP_STEREO, val==%d\n", val);

			if (wr) {
				int single=os->single;
				os->single=(val==1)?0:1;
				if(!single && os->single) {
					//os->fragsize >>= 1;
					//os->nbfrags >>=2;
					//if(os->nbfrags <2) os->nbfrags=2;
				}
			}
			else val=0;

			DPRINTK ("STEREO EXIT, returning %d\n", val);
			put_user(val, (int *) arg);
			return 0;
			break;

		case SNDCTL_DSP_GETFMTS:
			DPRINTK("DSP_GETFMTS, EXIT, returning S16_LE\n");
			return put_user(AFMT_S16_LE|AFMT_U8, (int *) arg);
		case SNDCTL_DSP_SETFMT:
			if (get_user(val, (int *)arg)) {
				return -EFAULT;
			}
			DPRINTK ("DSP_SETFMT, val==%d\n", val);
			if (val != AFMT_QUERY) {
				if(val!=AFMT_U8)val=AFMT_S16_LE;

				if (rd) {
					if(val==AFMT_S16_LE) write_reg(ICCR,0x690000|capmode);//ICCR
					else write_reg(ICCR,0x610000|capmode);//ICCR
					is->fmt = val;
				}

				if (wr) {
					if(val==AFMT_S16_LE) {
						//write_reg(OCCR,0x6b6b); //OCCR0   L&& R enable ; 3/4 empty; dma enabled;16 bits;var rate(0x202);
						//write_reg(DMA_SET_TODEV,0x10001); // 16bits X 6 entry 
						write_reg(OCCR,0x6969);
					}
					else {
						write_reg(OCCR,0x6363); //OCCR0   L&& R enable ; 3/4 empty; dma enabled;8 bits;var rate(0x202);
						//write_reg(DMA_SET_TODEV,0x00001); // 16bits X 6 entry 
					}
					os->fmt = val;
				}
				
			} else {
				if ((rd && (read_reg(ICCR)&0x040000)) ||
						(wr && (read_reg(OCCR)&0x0404)))
					val = AFMT_S16_LE;
				else
					val = AFMT_U8;
			}
			if(wr)os->fmt=val;
			if(rd)is->fmt=val;
			DPRINTK ("SETFMT EXIT, returning %d\n", val);
			return   put_user (val, (int *)arg);
			break;
		case SNDCTL_DSP_GETCAPS:
			val = DSP_CAP_REALTIME | DSP_CAP_TRIGGER | DSP_CAP_MMAP;
			if (is && os)
				val |= DSP_CAP_DUPLEX;
			return put_user(val, (int *) arg);


		case SNDCTL_DSP_SYNC:
			return sb2f_audio_sync(file);

		case SNDCTL_DSP_SETDUPLEX:
			return 0;

		case SNDCTL_DSP_POST:
			return 0;


		case SNDCTL_DSP_NONBLOCK:
			file->f_flags |= O_NONBLOCK;
			return 0;

		case SNDCTL_DSP_RESET:
			if (file->f_mode & FMODE_WRITE) {
				sb2f_audio_sync(file);
				audio_clear_buf(os);
			}
			if (file->f_mode & FMODE_READ) {
				sb2f_audio_sync(file);
				audio_clear_buf(is);
			}
			return 0;

		default:
			DPRINTK("NO surpport.\n");
			return -EINVAL;
	}

	return -EINVAL;
}


static int sb2f_audio_release(struct inode *inode, struct file *file)
{
	audio_state_t *state = file->private_data;

	down(&state->sem);

	if (file->f_mode & FMODE_READ) {

		sb2f_audio_sync(file);
		audio_clear_buf(state->input_stream);
		state->rd_ref = 0;
		free_irq(LS1B_BOARD_DMA2_IRQ, state->input_stream);
#ifdef CONFIG_SND_SB2F_TIMER
		del_timer(&state->input_stream->timer);
#endif
	}

	if (file->f_mode & FMODE_WRITE) {
		sb2f_audio_sync(file);
		audio_clear_buf(state->output_stream);
		state->wr_ref = 0;
		free_irq(LS1B_BOARD_DMA1_IRQ, state->output_stream);
#ifdef CONFIG_SND_SB2F_TIMER
		del_timer(&state->output_stream->timer);
#endif
	}

	if ((file->f_mode & FMODE_WRITE)) {
		//write_reg(DMA_INT_MASK,read_reg(DMA_INT_MASK)|0x1);	//mask all dma interrupt  
		sb2f_codec_write(NULL,0,0);//codec reset
	}

	if ((file->f_mode & FMODE_READ)) {
		sb2f_codec_write(NULL,0,0);//codec reset
		//write_reg(DMA_INT_MASK,read_reg(DMA_INT_MASK)|0x8);	//mask all dma interrupt  
	}

	up(&state->sem);
	return 0;
}

static int sb2f_audio_open(struct inode *inode, struct file *file)
{
	audio_state_t *state = &sb2f_audio_state;
	audio_stream_t *is = state->input_stream;
	audio_stream_t *os = state->output_stream;
	int err;

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
		os->single = 0;
		os->fmt=AFMT_S16_LE;
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
		os->timer.function = sb2f_audio_timeout;
#endif
	}

	if (file->f_mode & FMODE_READ) {
		state->rd_ref = 1;
		is->fragsize = AUDIO_FRAGSIZE_DEFAULT;
		is->nbfrags = REC_NBFRAGS;
		is->output = 0;
		is->single = 1;
		is->fmt=AFMT_S16_LE;
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
		is->timer.function = sb2f_audio_timeout;
#endif
	}

	sb2f_def_set();
	sb2f_open_set(file->f_mode);
	err = 0;
	
	if(file->f_mode & FMODE_READ) {
		err = sanity_check_codec_status(os);
	}
	else {
		err = sanity_check_codec_status(is);
	}

out:
	up(&state->sem);
	return err;
}

static struct file_operations sb2f_dsp_fops = {
	read:sb2f_audio_read,
	write:sb2f_audio_write,
	unlocked_ioctl:sb2f_audio_ioctl,
//	fsync:sb2f_audio_sync,
	poll:sb2f_audio_poll,
	open:sb2f_audio_open,
	release:sb2f_audio_release,
};

static DEFINE_SEMAPHORE(sb2f_ac97_mutex);
static int sb2f_ac97_refcount = 0;

static int sb2f_audio_probe(struct platform_device *pdev)
{
	down(&sb2f_ac97_mutex);

	if (!sb2f_ac97_refcount) {
		int i;
		for (i=0; i<100000; i++)
			*CSR |= 1;	/* 冷启 */
		sb2f_def_set();
		//init codec
		if (0 == ac97_probe_codec(&sb2f_ac97_codec)) {
			DPRINTK("probe codec err.\n");
			return -ENODEV;
		}
		else {
			if (sb2f_ac97_codec.model == 0x414c4760) {
				capmode = 0x00001;
			}
			else {
				capmode =0x20000;
			}
			DPRINTK("probe codec OK.\n");
		}

		//2.register dsp  && mixer
		sb2f_audio_state.dev_dsp =
			register_sound_dsp(&sb2f_dsp_fops, -1);
		sb2f_ac97_codec.dev_mixer =
			register_sound_mixer(&sb2f_mixer_fops, -1);
	}

	sb2f_ac97_refcount = 1;
	up(&sb2f_ac97_mutex);
	return 0;
}

static int sb2f_audio_remove(struct platform_device *pdev)
{
	unregister_sound_dsp(sb2f_audio_state.dev_dsp);
	unregister_sound_mixer(sb2f_ac97_codec.dev_mixer);

	down(&sb2f_ac97_mutex);
	sb2f_ac97_refcount = 0;
	if (!sb2f_ac97_refcount) {
	}
	up(&sb2f_ac97_mutex);

	return 0;
}

#ifdef CONFIG_PM
static int ls1x_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int ls1x_resume(struct platform_device *pdev)
{
	u32 flags, i;
//	mdelay(100);
	for (i=0; i<100000; i++)
		*CSR |= 1;
//	sb2f_def_set();
	return 0;
}
#else
#define ls1x_suspend	NULL
#define ls1x_resume	NULL
#endif

static struct platform_driver sb2f_audio_driver = {
	.driver = {
		.name	= "ls1b-audio",
		.owner	= THIS_MODULE,
	},
	.probe		= sb2f_audio_probe,
	.remove		= sb2f_audio_remove,
	.suspend		= ls1x_suspend,
	.resume		= ls1x_resume,
};

static int __devinit sb2f_audio_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&sb2f_audio_driver);
	if (ret)
		printk(KERN_ERR "failed to register sb2f-audio drover");
	return ret;
}

static void __devexit sb2f_audio_exit(void)
{
    platform_driver_unregister(&sb2f_audio_driver);
}

module_init(sb2f_audio_init);
module_exit(sb2f_audio_exit);

MODULE_LICENSE("GPL");

