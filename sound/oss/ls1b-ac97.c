
/*******************************************************************
 *
 * ac97 for fcr soc
 *
 ******************************************************************/

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
#include <linux/ac97_codec.h>
#include <asm/delay.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
//#include <asm/semaphore.h>
#include <linux/ac97_codec.h>
#include <asm/system.h>
#include <asm/loongson-sb2f/sb2f_board.h>
#include <asm/loongson-sb2f/sb2f_board_int.h>
#include <linux/interrupt.h>
//#include <asm/fcr-soc/hardware.h>
#include "ls1b-ac97.h"
#include "ls1b-private.h"
#include <asm/delay.h>
#include <linux/delay.h>
#include <linux/platform_device.h>


#define DEBUG 1
#undef DEBUG
#define REC_DEBUG 1
#undef REC_DEBUG
#define DMA_DESC		0x5
#define AUDIO_NBFRAGS_DEFAULT   ((DMA_DESC) * 3) 
#define REC_NBFRAGS		DMA_DESC
#define PLAY_NBFRAGS		AUDIO_NBFRAGS_DEFAULT
#define AUDIO_FRAGSIZE_DEFAULT  0x2000
#define BUF_MARK		0x12345678 
#define PLACEHOLDER		*((volatile int *)0xbfe74058)
//#error "FCR_SOC_IRQ_AC97 irq not define"

//#define SA_SHIRQ 0x00000080
/*  Debug macros*/
#if DEBUG
#define DPRINTK(fmt, args...) printk(KERN_ALERT "<%s>: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

//static struct fcr_ac97_reg *regs;
static void dma_enable_trans(audio_stream_t * s);
static int ac97_dma_write_intr(int irq, void *private);
static int ac97_dma_read_intr(int irq, void *private);
static int get_buf_depth(audio_stream_t *s);
static void inline sb2f_mark_one_buffer(audio_stream_t *s, const int i);

static audio_stream_t stream_in = {
       output: 0,
       fragsize:0x1000000,
};

static audio_stream_t output_stream = {
       output: 1,
       fragsize:0x1000000,
};

static audio_state_t sb2f_audio_state = {
input_stream:&stream_in,
	     output_stream:&output_stream,
	     //      sem:__MUTEX_INITIALIZER(sb2f_audio_state.sem),
	     sem: __SEMAPHORE_INITIALIZER(sb2f_audio_state.sem, 1),
};

static inline u32 read_reg(volatile u32 * reg)
{
	return (*reg);
}
static inline void write_reg(volatile u32 * reg, u32 val)
{
	*(reg) = (val);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
//
// codec reg ops 
//
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#define TIME_OUT 10		//HZ

//busy wait 
int codec_wait_complete(void)
{
	int timeout = 10000;

	while ((!(read_reg(INTRAW) & INTS_CRAC_MASK)) && (timeout-- > 0))
		udelay(1000);
	//printk(KERN_ALERT "intr_val = 0x%x\n", read_reg(INTRAW));
	if (timeout>0)
	{
		read_reg(CLR_CDC_RD);
		read_reg(CLR_CDC_WR);
	}
	return timeout > 0 ? 0 : 1;
}


static u16 sb2f_codec_read(struct ac97_codec *codec, volatile u8 reg)
{
	/*write crac */
	volatile u16 val;

	write_reg(CRAC,(CRAR_READ|CRAR_CODEC_REG(reg)));
	if (codec_wait_complete() != 0)
	{
		DPRINTK("ERROR\n");
		return -1;
	}
	val = read_reg(CRAC) & 0xffff;
	//DPRINTK("REG=0x%02x,val=0x%04x\n",reg,val);
	return val;
}

//static void ac97_write(struct ac97_codec *codec, u8 reg, u16 val)
static void sb2f_codec_write(struct ac97_codec *codec, u8 reg, u16 val)
{

	u32 tmp;
	tmp=(CRAR_WRITE | CRAR_CODEC_REG(reg) | val);
	//printk("REG=0x%02x,val=0x%04x,tmp=0x%08x\n",reg,val,tmp);
	udelay(1000);



	write_reg(CRAC, (CRAR_WRITE | CRAR_CODEC_REG(reg) | val));

	codec_wait_complete();
	//DPRINTK("REG=0x%02x,val=0x%04x\n",reg,val);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
//
// dma ops 
//
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
 *if buffer not EMpty entry dma is running
 */
void audio_clear_buf(audio_stream_t * s)
{
	//DECLARE_WAITQUEUE(wait, current);
	int frag;
	audio_buf_t *b;

	DPRINTK("Enter.\n");
	if (!s->buffers)
		return;
	
#ifdef REC_DEBUG
	s->buffers = NULL;	
	return;
#endif
	//printk(KERN_ALERT "%s: Free all buffer.\n", __func__);
	for (frag = 0; frag < s->nbfrags; frag++) {
		b = &s->buffers[frag];
		if (b->master)
		{
			DPRINTK("free b->master, size = 0x%x\n", b->master_size);
			free_pages((unsigned long)b->master, get_order(b->master_size));
		}
		if (b->dma_desc){
			DPRINTK("free s->buffers[%d].dma_desc, addr = 0x%x\n", frag, b->dma_desc);
			dma_free_coherent(NULL, sizeof(audio_dmadesc_t),
				b->dma_desc,0);
		}
	}
	kfree(s->buffers);
	if(s->ask_dma){
		DPRINTK("free s->ask_dma\n");
		dma_free_coherent(NULL, sizeof(audio_dmadesc_t),
			s->ask_dma, 0);
	}
	s->buffers = NULL;
}

static void display_dmadesc(audio_dmadesc_t *dmadesc)
{
	DPRINTK("\ndmadesc->ordered = 0x%x\n", dmadesc->ordered);	
	DPRINTK("dmadesc->saddr = 0x%x\n", dmadesc->saddr);
	DPRINTK("dmadesc->daddr = 0x%x\n", dmadesc->daddr);
	DPRINTK("dmadesc->length = 0x%x\n", dmadesc->length);
	DPRINTK("dmadesc->step_length = 0x%x\n", dmadesc->step_length);
	DPRINTK("dmadesc->step_times = 0x%x\n", dmadesc->step_times);
	DPRINTK("dmadesc->cmd = 0x%x\n", dmadesc->cmd);
	DPRINTK("dmadesc->dma_handle = 0x%x\n", dmadesc->dma_handle);
}

static void inline link_dma_desc(audio_stream_t *s, u32 i)
{
	if(i != 0)
	{
		s->buffers[i-1].dma_desc->ordered = 
			((u32)(s->buffers[i].dma_desc->dma_handle) | 0x1);

		dma_map_single(NULL, (void *)s->buffers[i - 1].dma_desc, 
					sizeof(audio_dmadesc_t), DMA_TO_DEVICE);
		//printk(KERN_ALERT "%s: s->buffers[%d].dma_desc->ordered = 0x%x\n",
		//		__func__, i -1 , s->buffers[i-1].dma_desc->ordered);
	} 
	else
	{
		if(!s->buffers[0].dma_desc->init)
		{
			s->buffers[s->nbfrags - 1].dma_desc->ordered = 
				((u32)(s->buffers[0].dma_desc->dma_handle) | 0x1);

			dma_map_single(NULL, (void *)s->buffers[s->nbfrags - 1].dma_desc, 
					sizeof(audio_dmadesc_t), DMA_TO_DEVICE);
			//printk(KERN_ALERT "%s: s->buffers[%d].dma_desc->ordered = 0x%x\n",
			//		__func__, 
			//		s->nbfrags - 1 , 
			//		s->buffers[s->nbfrags-1].dma_desc->ordered);
		}
		else
		{
			s->buffers[0].dma_desc->init = 0;
		}
	}
	dma_map_single(NULL, (void *)s->buffers[i].data, 
					s->fragsize, DMA_TO_DEVICE);
}

static void inline link_play_desc(audio_stream_t *s, u32 i)
{
	s->buffers[i + 2].dma_desc->ordered = 
		((u32)(s->buffers[i + 2].dma_desc->dma_handle) | 0x1);
	link_dma_desc(s, i);

}

static void inline link_rec_desc(audio_stream_t *s, u32 i)
{

	s->buffers[i].dma_desc->ordered = 
		((u32)(s->buffers[i].dma_desc->dma_handle) | 0x1);
	//dma_map_single(NULL, (void *)s->buffers[i].dma_desc, 
	//	sizeof(audio_dmadesc_t), DMA_TO_DEVICE);
	link_dma_desc(s, i);
}

static void dma_stop(audio_stream_t *s)
{
	u32 val;

	val = s->output ? 0x11 : 0x12;
	write_reg((volatile u32*)(CONFREG_BASE + 0x1160),val); 
	udelay(1000);

}

static int inline get_buf_depth(audio_stream_t *s)
{
	u32 depth;
	
	depth = (s->cur_buf >= s->free_buf) ? 
		(s->cur_buf - s->free_buf) : 
		(s->nbfrags - s->free_buf + s->cur_buf);	

	return depth/3;
}

#define AC97_INTM	0xafe74058
static void sb2f_init_placeholder(audio_stream_t *s, int index)
{
	audio_dmadesc_t *dma_desc;
	
	dma_desc = s->buffers[index].dma_desc;
	dma_desc->index[0] = (0xf00000ff | ((s->placeholder) << 8));
	dma_desc->index[1] = ((s->placeholder) << 8);
	dma_desc->ordered = 0;
	dma_desc->saddr = (char *)dma_desc->dma_handle + ((char *)&dma_desc->index - (char *)dma_desc);
	dma_desc->daddr = AC97_INTM;
	dma_desc->length = 1;
	dma_desc->step_length = 0;
	dma_desc->step_times = 1;
	dma_desc->cmd = s->output ? 0x00001001 : 0x00000001; //enable the interrupt
	s->placeholder++;
	//printk(KERN_ALERT "%s: index = 0x%x\n", __func__, dma_desc->index);
}

static void sb2f_init_dmadesc(audio_stream_t *s, int index, u32 count)
{
	audio_dmadesc_t *dma_desc;

	dma_desc = s->buffers[index].dma_desc;
	/* modify dma daddr */
	if(s->single)
	{
		/*
		 * [30]-1 two channels
		 * [30]-0 one channel
		 */
		dma_desc->daddr &= ~(1 << 30);
	}
	if(!s->single)
	{
		dma_desc->daddr &= ~(1 << 30);
		dma_desc->daddr |= (1 << 30);	
	}
	if(s->fmt == 8)
	{
		/*
		 * [29:28]-00 --> byte
		 * [29:28]-01 --> half word(two bytes)
		 * [29:28]-10 --> word (four bytes)
		 */
		dma_desc->daddr &= ~(3 << 28);
	}
	if(s->fmt == 16)
	{
		dma_desc->daddr &= ~(3 << 28);
		dma_desc->daddr |= (1 << 28);	
	}
	//dma_desc->ordered = 0;
	dma_desc->length = 8;
	dma_desc->step_length = 0;
	dma_desc->step_times = count >> 5;
	dma_desc->cmd = s->output ? 0x00001001 : 0x00000001; //enable the interrupt
}

#define AC97_CODEC	0xbfe74018
static void sb2f_init_detect_dmadesc(audio_stream_t *s, int  index)
{
	audio_dmadesc_t *dma_desc;
	
	dma_desc = s->buffers[index].dma_desc;
	s->buffers[index].dma_desc->ordered = 
		((u32)(s->buffers[index].dma_desc->dma_handle) | 0x1);
	dma_desc->saddr = virt_to_phys(s->zero);
	dma_desc->daddr = virt_to_phys(s->zero);
	//dma_desc->daddr = AC97_CODEC;
	dma_desc->length = 1;
	dma_desc->step_length = 0;
	dma_desc->step_times = Z_LEN >> 2;
	dma_desc->cmd = s->output ? 0x00001001 : 0x00000001; //enable the interrupt
	//printk(KERN_ALERT "%s: index = 0x%x\n", __func__, index);
	
}

static void rec_timer_handler(unsigned long data)
{
	audio_stream_t *s = (audio_stream_t *)data;
	volatile int *ptr;
	static int prev = -1;
	u32 val, i;

	mod_timer(&s->buf_timer, jiffies+1);
	val = ((u32)s->ask_dma->dma_handle | (s->output ? 0x5 : 0x6));	
	write_reg((volatile u32*)(CONFREG_BASE + 0x1160),val); 
	udelay(1000);

	for(i = 0; i < s->nbfrags; i++)
	{
		if(s->ask_dma->saddr >= s->buffers[i].dma_desc->saddr && 
			s->ask_dma->saddr <= s->buffers[i].dma_desc->saddr + s->fragsize)
			break;
	}
	dma_map_single(NULL, (void *)s->buffers[s->rec_buf].data,
			s->fragsize, DMA_FROM_DEVICE);
	ptr = (int *)(s->buffers[s->rec_buf].data + s->fragsize);
	if((*(ptr - 1)) != BUF_MARK)
	{
		wake_up_interruptible(&s->frag_wq);

		if(prev != s->rec_buf)
		{
			//printk(KERN_ALERT "========= %s: work on %d =========\n", __func__, i);
			for(i = 0; i < s->nbfrags; i++)
			{
			//printk(KERN_ALERT "dma_handle = 0x%x, dma_desc[%d]->ordered = 0x%x\n", 
			//	s->buffers[i].dma_desc->dma_handle, i, s->buffers[i].dma_desc->ordered);
			}
			//printk(KERN_ALERT "------%s: wake up s->rec_buf = %d\n", __func__, s->rec_buf);
			prev = s->rec_buf;
		}
	}
}

static void play_timer_handler(unsigned long data)
{
	audio_stream_t *s = (audio_stream_t *)data;
	
	mod_timer(&s->buf_timer, jiffies+HZ);
	printk(KERN_ALERT "Enter %s.\n", __func__);
}

static void inline sb2f_start_timer(audio_stream_t *s)
{
	if(s->start_timer){
		init_timer(&s->buf_timer);
		s->buf_timer.function = s->output ? play_timer_handler : rec_timer_handler;
		s->buf_timer.expires = jiffies + 1;
		s->buf_timer.data = (unsigned long)s;
		add_timer(&s->buf_timer);
		s->start_timer = 0;
	}
}



static void sb2f_add_one_dmadesc(audio_stream_t *s)
{
	//printk(KERN_ALERT "--Enter %s: s->cur_buf = %d\n", __func__, s->cur_buf);
	//sb2f_init_dmadesc(s, s->cur_buf, s->fragsize);
	s->buffers[s->cur_buf].bytecount = s->fragsize;
	sb2f_mark_one_buffer(s, s->cur_buf);
	link_rec_desc(s, s->cur_buf);
	s->cur_buf++;
	if(s->cur_buf == s->nbfrags)
		s->cur_buf = 0;
	//printk(KERN_ALERT "--Leave %s: s->cur_buf = %d\n", __func__, s->cur_buf);
}

static void sb2f_prepare_dmadesc(audio_stream_t *s, int num)
{
	int i;
	
	for(i = 0; i < s->nbfrags; i++)
		sb2f_init_dmadesc(s, i, s->fragsize);
	for(i = 0; i < num; i++)
		sb2f_add_one_dmadesc(s);
	dma_stop(s);
	dma_enable_trans(s);
	sb2f_start_timer(s);
}

/*
 * Fill data to dma buffer.
 * when buf_depth exceed the lowest threshold, start DMA.
 * Befor opening the DMA, we should reset DMA(stop).
 */
static u32 fill_dma_buffer(audio_stream_t *s, const char *buf, u32 count)
{
	u32 bytes, i;
	u32 tmp;
	u32 buf_depth;

	i = s->cur_buf;
	if(s->buffers[i].bytecount >= s->fragsize)
	{
		sb2f_init_dmadesc(s, i, s->fragsize);
		link_play_desc(s, i);

		s->cur_buf+=3;
		if(s->cur_buf == s->nbfrags)
			s->cur_buf = 0;
		
		buf_depth = get_buf_depth(s); 
		if(s->dma_status == DMA_STOP)
		{
			dma_enable_trans(s);
		}
		return 0;
	}
	else
	{
		bytes = s->buffers[i].bytecount;
		tmp = (s->fragsize - bytes) >= count ? 
			count : 
			(s->fragsize - bytes);
		copy_from_user((void *)(s->buffers[i].data + bytes), buf, tmp);
		s->buffers[i].bytecount += tmp;
	}
	return tmp;
}

/*
 * Clear dmadesc->ordered, so that when we wont link
 * dma descriptors, DMA will stop automatically. 
 */
static void inline sb2f_clear_dmadesc(audio_stream_t *s)
{
	int index;
	
	index = s->free_buf;
	//printk(" >> %s: clear dma_desc %d, addr = 0x%x\n", 
	//	__func__, index + 2, s->buffers[index + 2].dma_desc->ordered);
	if(index >= s->nbfrags)
		return;
	if(s->output)
	{
		index += 2;
		s->buffers[index].dma_desc->ordered = 
			((u32)(s->buffers[index].dma_desc->dma_handle) | 0x1);
	}
	else
	{
		s->buffers[index].dma_desc->ordered = 0;

	}
	//printk("  <<%s: clear dma_desc %d, addr = 0x%x\n", 
	//	__func__, index, s->buffers[index].dma_desc->ordered);
}




static void inline sb2f_mark_one_buffer(audio_stream_t *s, const int i)
{
	int *ptr,data;

	if(s == NULL || (i < 0 || i >= s->nbfrags))
	{
		printk("%s: Error dma_desc index.\n", __func__);
		return;
	}
	ptr = (int *)(s->buffers[i].data + s->fragsize);
	*(ptr - 1) = BUF_MARK;
	data = *(ptr - 1);
}

static void sb2f_mark_dma_buffer(audio_stream_t *s)
{
	int i;
	
	for(i = 0; i  < s->nbfrags; i++)
	{
		sb2f_mark_one_buffer(s, i);
	}
}

#ifdef REC_DEBUG
static void sb2f_setup_play_dmadesc(audio_stream_t *s)
{
	int i;
	audio_dmadesc_t **dma_desc;
	dma_addr_t *dma_phyaddr;
	u32 val;
	
	dma_desc = kmalloc(sizeof(*dma_desc) * s->nbfrags, GFP_KERNEL);
	if (!dma_desc){
		DPRINTK("2.alloc dma desc err.");
	}
	memset(dma_desc, 0, sizeof(*dma_desc) * s->nbfrags);

	for (i = 0; i < s->nbfrags; i++) {
		dma_desc[i] = dma_alloc_coherent(NULL, sizeof(audio_dmadesc_t),
					(dma_addr_t *)&dma_phyaddr, GFP_KERNEL);
		if (!dma_desc[i]) {
			DPRINTK("2.alloc dma desc err.");
		}
		memset(dma_desc[i], 0, sizeof(audio_dmadesc_t));
		dma_desc[i]->dma_handle = dma_phyaddr;
		DPRINTK("dma_desc[%d] = 0x%x, dma_handle = 0x%x\n", 
				i, (u32)dma_desc[i], (u32)dma_desc[i]->dma_handle);

		dma_desc[i]->saddr = s->buffers[i].dma_desc->saddr;
		dma_desc[i]->daddr = 0x9fe72420; 
		dma_desc[i]->length = 8;
		dma_desc[i]->step_length = 0;
		dma_desc[i]->step_times = s->fragsize >> 5;
		dma_desc[i]->cmd = 0x00001001; //enable the interrupt
	}
	for(i = 0; i < s->nbfrags - 1; i++){
		dma_desc[i]->ordered = 
			((u32)(dma_desc[i+1]->dma_handle) | 0x1);
	}
	dma_desc[s->nbfrags - 1]->ordered = 
		((u32)(dma_desc[0]->dma_handle) | 0x1);


	for(i = 0; i < s->nbfrags; i++)
	{
		display_dmadesc(dma_desc[i]);
	}
	val = 0x11;
	write_reg((volatile u32*)(CONFREG_BASE + 0x1160),val); 
	while(((*(volatile u32 *)(CONFREG_BASE + 0x1160)) & (0x8)));
	val = (u32)(dma_desc[0]->dma_handle);
	val |= 0x9;
	write_reg((volatile u32*)(CONFREG_BASE + 0x1160),val); 
	udelay(1000);

	kfree(dma_desc);
}
#endif 

/*
 *setup buffers,dma descs,buffer.
 */
int sb2f_setup_buf(audio_stream_t * s)
{
	int ret, i, step;
	int frag, nb_desc, buf_size;
	//void *dma_buf, *tmp;
	void *dma_buf;
	dma_addr_t *dma_phyaddr;
	audio_dmadesc_t **dma_desc = 0;
	//int szshift;

	DPRINTK("Enter.\n");

	if (s->buffers)
		return -EBUSY;

	//1.alloc audio_buf_t array
	s->buffers = kmalloc(sizeof(audio_buf_t) * s->nbfrags, GFP_KERNEL);

	if (!s->buffers) {
		DPRINTK("1.alloc buffer desc err!\n");
		ret = -ENOMEM;
		goto err;
	}
	memset(s->buffers, 0, sizeof(audio_buf_t) * s->nbfrags);

	//2.alloc dma desc;
	nb_desc = s->nbfrags;
	dma_desc = kmalloc(sizeof(*dma_desc) * nb_desc, GFP_KERNEL);
	if (!dma_desc){
		DPRINTK("2.alloc dma desc err.");
		goto err;
	}
	memset(dma_desc, 0, sizeof(*dma_desc) * nb_desc);

	s->descs_per_frag = nb_desc / s->nbfrags;
	for (i = 0; i < nb_desc; i++) {
		dma_desc[i] = dma_alloc_coherent(NULL, sizeof(audio_dmadesc_t),
					(dma_addr_t *)&dma_phyaddr, GFP_KERNEL);
		if (!dma_desc[i]) {
			DPRINTK("2.alloc dma desc err.");
			goto err;
		}
		memset(dma_desc[i], 0, sizeof(audio_dmadesc_t));
		dma_desc[i]->dma_handle = dma_phyaddr;
		s->buffers[i].dma_desc = dma_desc[i];
		if(!s->output)
			continue;

		/* only for playing */
		if(i%3 == 1)			/* trans index to INTM register */
		{
			sb2f_init_placeholder(s, i);
			s->buffers[i-1].dma_desc->ordered = 
				((u32)(s->buffers[i].dma_desc->dma_handle) | 0x1);
		}
		if(i%3 == 2)			/* loopback */
		{
			sb2f_init_detect_dmadesc(s, i);
			s->buffers[i-1].dma_desc->ordered = 
				((u32)(s->buffers[i].dma_desc->dma_handle) | 0x1);
		}
		if(i == 0) s->buffers[i].dma_desc->init = 1;
		//printk(KERN_ALERT "dma_desc[%d] = 0x%x, dma_handle = 0x%x\n", 
		//		i, (u32)dma_desc[i], (u32)dma_desc[i]->dma_handle);
	}
	/* dma desc for ask_valid one per audio_stream_t */
	s->ask_dma = dma_alloc_coherent(NULL, sizeof(audio_dmadesc_t), 
			(dma_addr_t *)&dma_phyaddr, GFP_KERNEL);
	if(s->ask_dma != NULL)
	{
		memset(s->ask_dma, 0, sizeof(audio_dmadesc_t));
		s->ask_dma->dma_handle = dma_phyaddr;
	}
	else
	{
		DPRINTK("3. alloc dma desc err.");
		goto err;
	}

	step = (s->output) ? (3) : (1);
	/* actual DMA data buffers */
	for (frag = 0; frag < s->nbfrags; frag+=step) 
	{
		dma_buf = 0;
		buf_size = (s->fragsize/* + BLANK_SIZE*/);
		dma_buf = (void *)__get_free_pages(GFP_ATOMIC|GFP_DMA, get_order(buf_size));

		if (!dma_buf) {
			DPRINTK("4.alloc dma buffer err\n");
			goto err;
		}
	
		//dma_desc[frag]->ordered = 0;
		dma_desc[frag]->saddr = virt_to_phys(dma_buf);
		dma_desc[frag]->daddr = s->output ? AUD_WRITE_ADDR : AUD_READ_ADDR; 
		
		s->buffers[frag].data =  dma_buf;//KSEG1ADDR(dma_buf);
		s->buffers[frag].bytecount = 0;	
		memset(dma_buf, 0, buf_size);
		s->buffers[frag].master = dma_buf;
		s->buffers[frag].master_size=buf_size;
	}

	s->cur_buf = 0;
	s->free_buf = 0;

	sema_init(&s->sem, 1);

	if(!s->output)
		sb2f_mark_dma_buffer(s);
#ifdef REC_DEBUG
	printk(KERN_ALERT "play for debuging ac97 recording.\n");
	if(!s->output)
		sb2f_setup_play_dmadesc(s);
#endif
	kfree(dma_desc);

#if 0
	audio_clear_buf(s);
	return -ENOMEM;
#endif
	DPRINTK("OK.\n");
	return 0;

err:
	printk("soc-audio: unable to allocate audio memory\n ");
	audio_clear_buf(s);
	if(dma_desc)
		kfree(dma_desc);
	return -ENOMEM;
}



static void dma_enable_trans(audio_stream_t * s)
{
	u32 val;

	if(s->first_time){
		printk("Start DMA(%s).  cur_buf = %d, free_buf = %d\n\n", 
				s->output ? ("play") : ("record"),
				s->cur_buf, s->free_buf);
		val = (u32)(s->buffers[s->free_buf].dma_desc->dma_handle);
		val |= s->output ? 0x9 : 0xa;
		write_reg((volatile u32*)(CONFREG_BASE + 0x1160),val); 
		udelay(1000);
		s->dma_status = DMA_START;
		s->dma_break = 0;	
		write_reg(INTM,0x0); //INTM ;disable all interrupt
		s->first_time = 0;
	}
}



/**********************************************************************
 *
 * ac97  dma intr.
 *
 **********************************************************************
 */
static int ac97_dma_read_intr(int irq, void *private)
{
	//audio_state_t *state = (audio_state_t *) private;
	audio_stream_t *s;
	
	DPRINTK("Enter %s\n", __FUNCTION__);
	
	//s = state->input_stream;
	s = (audio_stream_t *)private;

	DPRINTK("Exit %s\n", __FUNCTION__);
	return IRQ_HANDLED;
}


/* handle the ac97 write interrupt
 * s->free_buf point to the dma desc currently used
 * s->cur_buf point to the dma which will be filled
 */
static int ac97_dma_write_intr(int irq, void *private)
{
	audio_state_t *state = (audio_state_t *) private;
	audio_stream_t *s;
	u32 val;

	s = state->output_stream;
	val = PLACEHOLDER;
	write_reg(INTM, val & (~(0xff)) & (~(0xf0000000)));

	if(s->dma_status == DMA_STOP)
		return IRQ_HANDLED;

	//s = (audio_stream_t *)private;
	//printk(KERN_ALERT "\n--->%s: placeholder = 0x%x, s->free_buf = %d\n", 
	//		__func__, val, s->free_buf);
	dma_map_single(NULL, (void *)s->buffers[s->free_buf].data,
			s->fragsize, DMA_FROM_DEVICE);
	//sb2f_clear_dmadesc(s);	
	s->buffers[s->free_buf].bytecount = 0;	/* This had no data to play */

	s->free_buf = 3*(0xff & (val >> 8));
	if(s->free_buf == s->nbfrags)
		s->free_buf = 0;
	wake_up(&s->frag_wq);		
	//printk(KERN_ALERT "<---%s: s->free_buf = 0x%x, cur_buf= %d\n\n", 
	//		__func__, s->free_buf, s->cur_buf);
	return IRQ_HANDLED;
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
//
// mixer fops
//

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

struct ac97_codec sb2f_ac97_codec = {
codec_read:sb2f_codec_read,
	   codec_write:sb2f_codec_write,
};

	static int
mixer_ioctl( struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int ret;

	DPRINTK("Enter.\n");
	ret = sb2f_ac97_codec.mixer_ioctl(&sb2f_ac97_codec, cmd, arg);

	if (ret) {
		DPRINTK("Exit.\n");
		return ret;
	}

	/* We must snoop for some commands to provide our own extra processing */
	switch (cmd) {
		case SOUND_MIXER_WRITE_RECSRC:
			break;
		default:
			break;
	}
	DPRINTK("Exit.\n");
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
	static unsigned short sample_rate=0xac44;
	//int dma_mask=read_reg(DMA_INT_MASK)|0xfffffff6;

	//write_reg(INTM,0xFFFFFFFF); //INTM ;disable all interrupt
	write_reg(INTM,0x0); //INTM ;disable all interrupt

	if(mode & FMODE_READ)
	{
		write_reg(ICCR,0x6b0000);//ICCR
		//write_reg(DMA_SET_FRMDEV,0x6); //  6 entry
		sb2f_codec_write(NULL,0x32,sample_rate);//|(0x32<<16)|(0<<31));     //pcm input rate .
		sb2f_codec_write(NULL,0x34,sample_rate);//|(0x34<<16)|(0<<31));     //MIC rate.
		//dma_mask &= ~0x8;
	}

	if(mode & FMODE_WRITE)
	{
		write_reg(OCCR,0x6969|0x202); //OCCR0   L&& R enable ; 3/4 empty; dma enabled;8 bits;var rate(0x202);
		//write_reg(DMA_SET_TODEV,0x10001); // 16bits X 6 entry 
		sb2f_codec_write(NULL,0x2c,sample_rate);//|(0x2c<<16)|(0<<31));     //PCM Out rate
		//dma_mask &= ~0x1;
	}
	//write_reg(DMA_INT_MASK,dma_mask);        // one dma desc completed.
}

static void sb2f_def_set(void)
{
	static unsigned short sample_rate=0xac44;
	int i;

	printk("Enter %s.\n", __func__);
	/*ac97 config*/
	write_reg(OCCR,0x6969|0x202); //OCCR0   L&& R enable ; 3/4 empty; dma enabled;8 bits;var rate(0x202);
	write_reg(ICCR,0x630000);//ICCR
	//write_reg(INTM,0xFFFFFFFF); //INTM ;disable all interrupt
	write_reg(INTM, 0x0);

	sb2f_codec_write(NULL,0,0);//codec reset

	for(i = 0; i < 5; i++)
	{
		printk("======== A/D status = 0x%x\n", sb2f_codec_read(NULL, 0x26) & 0xffff);
	}

	sb2f_codec_write(NULL,0x2,0x0808);//|(0x2<<16)|(0<<31));      //Master Vol.
	sb2f_codec_write(NULL,0x4,0x0808);//|(0x2<<16)|(0<<31));      //headphone Vol.
	sb2f_codec_write(NULL,0x6,0x0008);//|(0x2<<16)|(0<<31));      //mono Vol.

	sb2f_codec_write(NULL,0xc,0x0008);//|(0x2<<16)|(0<<31));      //phone Vol.
	sb2f_codec_write(NULL,0x18,0x0808);//|(0x18<<16)|(0<<31));     //PCM Out Vol.
	sb2f_codec_write(NULL,0x2a,1);//0x1|(0x2A<<16)|(0<<31));        //Extended Audio Status  and control
	sb2f_codec_write(NULL,0x2c,sample_rate);//|(0x2c<<16)|(0<<31));     //PCM Out rate
	sb2f_codec_write(NULL,0x32,sample_rate);//|(0x32<<16)|(0<<31));     //pcm input rate .
	sb2f_codec_write(NULL,0x34,sample_rate);//|(0x34<<16)|(0<<31));     //MIC rate.
	sb2f_codec_write(NULL,0x0E,0x35f);//|(0x0E<<16)|(0<<31));     //Mic vol .
	sb2f_codec_write(NULL,0x1E,0x0808);//|(0x1E<<16)|(0<<31));     //MIC Gain ADC.
	//#endif 
	printk("Exit %s.\n", __func__);
}

/*write */
	static int
sb2f_audio_write(struct file *file, const char *buffer,
		size_t count, loff_t * ppos)
{
	const char *buffer0 = buffer;
	audio_state_t *state = (audio_state_t *) file->private_data;
	audio_stream_t *s = state->output_stream;
	u32 buf_depth;
	int  ret = 0;
	
	if (*ppos != file->f_pos)
	{
		DPRINTK("espipe\n");
		return -ESPIPE;
	}

	if (!s->buffers && sb2f_setup_buf(s)){
		DPRINTK("ERR 3\n");
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
		buf_depth = get_buf_depth(s); 
		if(buf_depth == DMA_BUF_UPPER)
		{
			if(file->f_flags & O_NONBLOCK)
			{
				ret = -EAGAIN;
				up(&s->sem);
				return ret;
			}
			else
			{
				//printk("====SLEEP: cur=%d,free=%d\n", s->cur_buf, s->free_buf);
				wait_event_interruptible(s->frag_wq, get_buf_depth(s) != DMA_BUF_UPPER);
				//printk("====WAKE : cur=%d,free=%d\n", s->cur_buf, s->free_buf);
			}
			buf_depth = get_buf_depth(s);
			/* if dma chain is break, no longer wait. 
			 * in fill_dma_buffer, DMA will be re-opened.
			 */
		}
		/* Fill data to the buffer, if necessary open the DMA */
		ret = fill_dma_buffer(s, buffer, count);
		count -= ret;
		buffer += ret;
	}
	if ((buffer - buffer0))
		ret = buffer - buffer0;
	up(&s->sem);
	return ret;
}

static int
sb2f_audio_read(struct file *file, char *buffer, size_t count,
		loff_t * ppos)
{
	char *buffer0 = buffer;
	audio_state_t *state = file->private_data;
	audio_stream_t *s = state->input_stream;
	u32 bytes = 0, len = 0;
	int ret = 0;
	volatile int *ptr;

	if (*ppos != file->f_pos)
		return -ESPIPE;

	if (!s->buffers && sb2f_setup_buf(s))
		return -ENOMEM;

	/*3. sem:get resource .if 0 ,wait */
	if (file->f_flags & O_NONBLOCK) {
		if (down_trylock(&s->sem))
			return -EAGAIN;
	} else {
		//wait sem
		if (down_interruptible(&s->sem))
			return -ERESTARTSYS;
	}
	if(s->first_time)
	{
		sb2f_prepare_dmadesc(s, 8);
		s->first_time = 0;
	}
	while(count > 0)
	{	
		ptr = (int *)(s->buffers[s->rec_buf].data + s->fragsize);
		if((*(ptr - 1)) == BUF_MARK)
		{
			if (file->f_flags & O_NONBLOCK) 
			{
				ret = -EAGAIN;
				goto output;
			}
			else
			{
				wait_event_interruptible(s->frag_wq, ((*(ptr - 1)) != BUF_MARK));
			}
		}

		len = s->fragsize - s->buffers[s->rec_buf].bytecount;
		bytes = count > s->buffers[s->rec_buf].bytecount ? 
			s->buffers[s->rec_buf].bytecount : count;
		if (copy_to_user((void *)buffer,(const void *)s->buffers[s->rec_buf].data + len, bytes)) 
		{
			up(&s->sem);
			return -EFAULT;
		}
		buffer += bytes;
		count -= bytes;
		s->buffers[s->rec_buf].bytecount -= bytes;
		if(s->buffers[s->rec_buf].bytecount == 0){
			dma_map_single(NULL, (void *)s->buffers[s->rec_buf].data, 
					s->fragsize, DMA_TO_DEVICE);
			s->rec_buf++;
			s->rec_buf = (s->rec_buf==s->nbfrags) ? 0 : s->rec_buf;
			sb2f_add_one_dmadesc(s);
		}
	}

output:
	if ((buffer - buffer0))
		ret = buffer - buffer0;
	up(&s->sem);
	return ret;
}



static int sb2f_audio_sync(struct file *file)
{
	audio_state_t *state = file->private_data;
	audio_stream_t *s = (file->f_mode & FMODE_WRITE)  ? 
		state->output_stream : state->input_stream;


	if (!s->buffers)
		return 0;
	
	if(s->sync_only_once)
	{
		s->sync_only_once = 0;
	}
	else 
	{
		return 0;
	}

	if(file->f_mode & FMODE_WRITE)
	{
		dma_stop(s);
		s->dma_status = DMA_STOP;
	}
	else
	{
		dma_stop(s);
		del_timer_sync(&s->buf_timer);
	}
	return 0;
}

	static unsigned int
sb2f_audio_poll(struct file *file, struct poll_table_struct *wait)
{
	audio_state_t *state = file->private_data;
	audio_stream_t *is = state->input_stream;
	audio_stream_t *os = state->output_stream;
	//audio_buf_t *b;
	u32 buf_depth;
	unsigned int mask = 0;

	poll_wait(file, &is->frag_wq, wait);
	poll_wait(file, &os->frag_wq, wait);
	
	DPRINTK("Enter.\n");
	if (file->f_mode & FMODE_READ) 
	{
		if (!is->buffers && sb2f_setup_buf(is))
			return -ENOMEM;
		poll_wait(file, &is->frag_wq, wait);
		if(is->rec_buf < is->free_buf)
		{
			mask |= POLLIN | POLLRDNORM;
		}
	}

	if (file->f_mode & FMODE_WRITE) 
	{
		if (!os->buffers && sb2f_setup_buf(os))
			return -ENOMEM;
		poll_wait(file, &os->frag_wq, wait);
		buf_depth = get_buf_depth(os); 
		if(buf_depth < DMA_BUF_UPPER)
		{
			mask |= POLLOUT | POLLWRNORM;
		}
	}
#if 0
	if (file->f_mode & FMODE_READ)
		mask |= POLLIN | POLLRDNORM;

	if (file->f_mode & FMODE_WRITE)
		mask |= POLLOUT | POLLWRNORM;
#endif
	DPRINTK("Exit.\n");
	return mask;
}

	static int
sb2f_audio_ioctl(struct file *file,
		uint cmd, ulong arg)
{
	audio_state_t *state = file->private_data;
	audio_stream_t *os = state->output_stream;
	audio_stream_t *is = state->input_stream;
	int rd=0, wr=0, val=0;
	if (file->f_mode & FMODE_WRITE)
		wr = 1;
	if (file->f_mode & FMODE_READ)
		rd = 1;

	DPRINTK("Enter.\n");

	switch (cmd) {
		case SNDCTL_DSP_SETFRAGMENT:
			if (get_user(val, (int *)arg)) {
				return -EFAULT;
			}
			DPRINTK ("DSP_SETFRAGMENT, val==%d\n", val);

			if (rd)
			{
				DPRINTK ("old fragsize=%d,nbfrags=%d\n",is->fragsize,is->nbfrags);
				is->fragsize = 1 << (val & 0xFFFF);
				if(is->fragsize < 1024) is->fragsize = 1024;
				is->nbfrags = (val >> 16) & 0xFFFF;
				if(is->nbfrags < 4) is->nbfrags = 4;
				DPRINTK ("new fragsize=%d,nbfrags=%d\n",is->fragsize,is->nbfrags);
				printk("%s: fragsize = 0x%x\n", __func__, is->fragsize);
			}

			if (wr)
			{
				DPRINTK ("old fragsize=%d,nbfrags=%d\n",os->fragsize,os->nbfrags);
				os->fragsize = 1 << (val & 0xFFFF);
				if(os->fragsize < 1024) os->fragsize = 1024;
				os->nbfrags = (val >> 16) & 0xFFFF;
				if(os->nbfrags < 4) os->nbfrags = 4;
				if(os->single)
				{
					//os->fragsize >>= 1;
					os->nbfrags >>=2;
					if(os->nbfrags <2) os->nbfrags=2;
				}
				DPRINTK ("new fragsize=%d,nbfrags=%d\n",os->fragsize,os->nbfrags);
				printk("%s: fragsize = 0x%x, frags = %d\n", __func__, 
						is->fragsize, os->nbfrags);
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
			printk("%s:%d SNDCTL_DSP_SPEED = %d\n", __func__, __LINE__, val);
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
			if (val<=0)
			{
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
			if (val!=0)
			{
				if (wr)
				{
					int single=os->single;
					os->single=(val==1)?1:0;
					if(!single && os->single)
					{
						//os->fragsize >>= 1;
						//os->nbfrags >>=2;
						//if(os->nbfrags <2) os->nbfrags=2;
					}
				}
				else if (rd) 
				{
					if(val!=1)
						return -EFAULT;
				}
			}
			else
			{
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

			if (wr)
			{
				int single=os->single;
				os->single=(val==1)?0:1;
				if(!single && os->single)
				{
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

				if (rd)
				{
					if(val==AFMT_S16_LE) write_reg(ICCR,0x6b0000);//ICCR
					else write_reg(ICCR,0x630000);//ICCR
					is->fmt = val;
				}

				if (wr)
				{
					if(val==AFMT_S16_LE) {
						write_reg(OCCR,0x6b6b); //OCCR0   L&& R enable ; 3/4 empty; dma enabled;16 bits;var rate(0x202);
						//write_reg(DMA_SET_TODEV,0x10001); // 16bits X 6 entry 
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
			if (file->f_mode & FMODE_WRITE)
			{
				sb2f_audio_sync(file);
				audio_clear_buf(os);
			}
			if (file->f_mode & FMODE_READ)
			{
				sb2f_audio_sync(file);
				audio_clear_buf(is);
			}
			return 0;

		default:
			DPRINTK("NO surpport.\n");
			return -EINVAL;
			//      return state->client_ioctl ?
			//        state->client_ioctl (inode, file, cmd, arg) : -EINVAL;
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
		//free dma
		state->rd_ref = 0;
	}

	if (file->f_mode & FMODE_WRITE) {
		//audio_stream_t *os = state->output_stream;
		sb2f_audio_sync(file);
		audio_clear_buf(state->output_stream);
		state->wr_ref = 0;
	}

	if ((file->f_mode & FMODE_WRITE)) {
		//write_reg(DMA_INT_MASK,read_reg(DMA_INT_MASK)|0x1);        //mask all dma interrupt  
	}

	if ((file->f_mode & FMODE_READ))
	{
		//write_reg(DMA_INT_MASK,read_reg(DMA_INT_MASK)|0x8);        //mask all dma interrupt  
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

	DPRINTK("Enter.\n");

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

	DPRINTK("*********1*************\n");
	file->private_data = state;

	if ((file->f_mode & FMODE_WRITE)) {
		state->wr_ref = 1;
		//os->fragsize = 0x89d0;
		os->fragsize = AUDIO_FRAGSIZE_DEFAULT;
		os->nbfrags = PLAY_NBFRAGS;
		os->output = 1;
		os->single = 0;
		os->dma_break = 0;
		os->start_timer = 1;
		os->dma_status = DMA_STOP;
		os->rec_buf = os->free_buf = os->cur_buf = 0;
		os->fmt=AFMT_S16_LE;
		os->first_time = 1;
		os->sync_only_once = 1;
		os->placeholder = 1;
		os->zero[0] = os->zero[1] = 0x807c0000;
		init_waitqueue_head(&os->frag_wq);
	}

	if (file->f_mode & FMODE_READ) {
		state->rd_ref = 1;
		//is->fragsize = 0x1000;
		is->fragsize = AUDIO_FRAGSIZE_DEFAULT;
		is->nbfrags = REC_NBFRAGS;
		is->output = 0;
		is->single = 0;
		is->dma_break = 0;
		is->start_timer = 1;
		is->dma_status = DMA_STOP;
		is->rec_buf = is->free_buf = is->cur_buf = 0;
		is->fmt=AFMT_S16_LE;
		is->first_time = 1;
		is->sync_only_once = 1;
		init_waitqueue_head(&is->frag_wq);
	}
	sb2f_def_set();
	sb2f_open_set(file->f_mode);
	err = 0;
	
	if(file->f_mode & FMODE_READ)
	{
		err = sanity_check_codec_status(os);
	}
	else
	{
		err = sanity_check_codec_status(is);
	}

out:
	up(&state->sem);
	DPRINTK("exit.\n");
	return err;
}


static struct file_operations sb2f_dsp_fops = {
read:sb2f_audio_read,
     write:sb2f_audio_write,
     unlocked_ioctl:sb2f_audio_ioctl,
     //fsync:sb2f_audio_sync,
     poll:sb2f_audio_poll,
     open:sb2f_audio_open,
     release:sb2f_audio_release,
};

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
//
// module ops
//
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#if 0
static void dsp_irq(int irq, void *dev_data, struct pt_regs *regs)
{
	/*nothing,but clean */
	DPRINTK("unliked interrupt,irq status =0x%x\n",*INTS);

}
#endif 



/*********************************************************/
//
//reset hw and set regs as default setting
//
/*********************************************************/
static void sb2f_audio_init_hw(void)
{
	u32 flags, i;

	local_irq_save(flags);
	for(i=0;i<100000;i++) *(volatile unsigned char *)0xbfe74000 |=1;
	local_irq_restore(flags);
}

static DEFINE_SEMAPHORE(sb2f_ac97_mutex);
static int sb2f_ac97_refcount=0;

static int sb2f_audio_probe(struct platform_device *pdev)
{

	printk("in the sb2f_audio_probe !!!!!!!!!!!!!\n\n\n");
	DPRINTK("Enter.\n");
	down(&sb2f_ac97_mutex);

	if (!sb2f_ac97_refcount) {

		sb2f_audio_init_hw();

		sb2f_def_set();
		//init codec
		if (0==ac97_probe_codec(&sb2f_ac97_codec))
		{
			DPRINTK("probe codec err.\n");
			return -ENODEV;
		}
		else
		{
			DPRINTK("probe codec OK.\n");
		}

		//2.register dsp  && mixer
		sb2f_audio_state.dev_dsp =
			register_sound_dsp(&sb2f_dsp_fops, -1);
		sb2f_ac97_codec.dev_mixer =
			register_sound_mixer(&sb2f_mixer_fops, -1);
#if 0
		request_irq(SB2F_BOARD_DMA1_IRQ, ac97_dma_read_intr, SA_SHIRQ,
				"ac97dma-read", &sb2f_audio_state);
		request_irq(SB2F_BOARD_DMA2_IRQ, ac97_dma_write_intr, SA_SHIRQ,
				"ac97dma-write", &sb2f_audio_state);
#endif
		request_irq(SB2F_BOARD_AC97_IRQ, ac97_dma_write_intr, IRQF_SHARED,
				"ac97dma-write", &sb2f_audio_state);
	}


	sb2f_ac97_refcount++;
	up(&sb2f_ac97_mutex);
	return 0;
}

static int sb2f_audio_remove(struct platform_device *pdev)
{
	unregister_sound_dsp(sb2f_audio_state.dev_dsp);
	unregister_sound_mixer(sb2f_ac97_codec.dev_mixer);

	down(&sb2f_ac97_mutex);
	sb2f_ac97_refcount--;
	if (!sb2f_ac97_refcount) {
		free_irq(SB2F_BOARD_AC97_IRQ, &sb2f_audio_state);
	}
	up(&sb2f_ac97_mutex);

	return 0;
}

static struct platform_driver sb2f_audio_driver = {
	.driver = {
		.name	= "ls1b-audio",
                .owner	= THIS_MODULE,
	},
	.probe		= sb2f_audio_probe,
	.remove		= sb2f_audio_remove,
};

static int __devinit sb2f_audio_init(void)
{
    int ret = 0;

	printk("in the sb2f_audio_init ***********&&&&&&&&\n\n\n");
    ret = platform_driver_register(&sb2f_audio_driver);
    if(ret) {
		printk("ddddddddddddddddddddddddddddd!!\n\n\n");
        printk(KERN_ERR "failed to register sb2f-audio drover");
}
    return ret;
}

static void __devexit sb2f_audio_exit(void)
{
    platform_driver_unregister(&sb2f_audio_driver);
}

module_init(sb2f_audio_init);
module_exit(sb2f_audio_exit);

MODULE_LICENSE("GPL");

