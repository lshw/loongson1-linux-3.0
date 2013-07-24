/*
 * V4L2 Driver for LS1C_SoC camera host
 *
 * Copyright (C) 2013-05-08, linxiyuan <linxiyuan-gz@foxmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#include <media/soc_camera.h>
#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/videobuf-dma-contig.h>
#include <media/soc_mediabus.h>

#include <irq.h>
#include <ls1b_board.h>
//#include <linux/types.h>

/*
 * camera registers
 */
#define CAM_DMA0_CONFIG		0x0
#define CAM_DMA1_CONFIG		0x8
#define CAM_DMA2_CONFIG		0x10
#define CAM_DMA3_CONFIG		0x18
#define CAM_PIX_CONFIG		0x20
#define CAM_UOFF_CONFIG		0x28
#define CAM_VOFF_CONFIG		0x30
#define CAM_CAMIF_CONFIG    0x38
#define	LS1C_CAM_DMA_CNT	4
#define	LS1C_CAM_START		31

#define LS1C_CAMERA_DATA_HIGH	1
#define LS1C_CAMERA_PCLK_RISING	2
#define LS1C_CAMERA_VSYNC_HIGH	4

#define	CONFIG_PARA_H_WD			16
#define	CONFIG_PARA_INPUT_DATA_MODE	11
#define	CONFIG_PARA_640X480			7
#define	CONFIG_PARA_YUV_ORDER		4
#define	CONFIG_PARA_HS			(1 << 1)
#define	CONFIG_PARA_VS			(1 << 0)

#define VERSION_CODE KERNEL_VERSION(0, 0, 1)
#define DRIVER_NAME "ls1c-camera"

#define CSI_BUS_FLAGS	(SOCAM_MASTER | SOCAM_HSYNC_ACTIVE_HIGH | \
			SOCAM_VSYNC_ACTIVE_HIGH | SOCAM_VSYNC_ACTIVE_LOW | \
			SOCAM_DATA_ACTIVE_HIGH | SOCAM_DATA_ACTIVE_LOW | \
			SOCAM_DATAWIDTH_8)

#define MAX_VIDEO_MEM 4	/* Video memory limit in megabytes */
//#define MAX_VIDEO_MEM 16	/* Video memory limit in megabytes */

#define	POLL_CAMERA	0

struct timeval t_start,t_end;

    long cost_time = 0; 
static unsigned int flag = 0;
static unsigned int buf_init_cnt = 0;
struct timer_list my_timer;
struct timex  txc;
struct rtc_time tm;

//static void ls1c_camera_irq(int irq, void *data);
static irqreturn_t ls1c_camera_irq(int irq, void *data);
/* buffer for one video frame */
struct ls1c_camera_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer		vb;
	enum v4l2_mbus_pixelcode	code;
	int				inwork;
};

/*
 * Only supposed to handle one camera on its Camera Sensor
 * Interface. If anyone ever builds hardware to enable more than
 * one camera, they will have to modify this driver too
 */
struct ls1c_camera_dev {
	struct soc_camera_host		soc_host;
	struct soc_camera_device	*icd;
	struct ls1c_camera_pdata		*pdata;
	struct ls1c_camera_buffer		*active;
	struct resource			*res;
	struct clk			*clk;
	struct list_head		capture;

	void __iomem			*base;
	int				dma_chan;
	unsigned int			irq;
	unsigned long			mclk;

	spinlock_t			lock;
};

#ifdef	POLL_CAMERA
#include <linux/kthread.h>
static struct task_struct *tsk = NULL;
static struct ls1c_camera_dev	*thread_data;
static int camera_thread(void *pthread_data);
#endif

/*
 *  Videobuf operations
 */
static int ls1c_videobuf_setup(struct videobuf_queue *vq, unsigned int *count,
			      unsigned int *size)
{
	struct soc_camera_device *icd = vq->priv_data;
	int bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
						icd->current_fmt->host_fmt);

	if (bytes_per_line < 0)
		return bytes_per_line;

	*size = bytes_per_line * icd->user_height;

	if (!*count)
		*count = 32;

	if (*size * *count > MAX_VIDEO_MEM * 1024 * 1024)
		*count = (MAX_VIDEO_MEM * 1024 * 1024) / *size;

	dev_dbg(icd->dev.parent, "count=%d, size=%d\n", *count, *size);

	return 0;
}

static void free_buffer(struct videobuf_queue *vq, struct ls1c_camera_buffer *buf)
{
	flag = 1;
	struct soc_camera_device *icd = vq->priv_data;
	struct videobuf_buffer *vb = &buf->vb;
	int temp;

#if 0
	if (tsk != NULL ) {
		printk("--------task error-------\n");
		kthread_stop(tsk);
		tsk = NULL;
	}
#endif
	BUG_ON(in_interrupt());

	dev_dbg(icd->dev.parent, "%s (vb=0x%p) 0x%08lx %d\n", __func__,
		vb, vb->baddr, vb->bsize);
	
	temp = __raw_readl(0xbc280038);
	temp &= ~(1 << LS1C_CAM_START);
	__raw_writel(temp, 0xbc280038);
	temp = __raw_readl(0xbc280038);
	printk("-----stop 0x%x-----\n", temp);
	while (temp & 0x40000000) { 
		temp = __raw_readl(0xbc280038);
		temp |= (1 << 29);
	//	printk("----29 bits--------\n");
		__raw_writel(temp, 0xbc280038);
	}

	printk("-----stop end-----\n");
	/*
	 * This waits until this buffer is out of danger, i.e., until it is no
	 * longer in STATE_QUEUED or STATE_ACTIVE
	 */
	videobuf_waiton(vq, vb, 0, 0);
	videobuf_dma_contig_free(vq, vb);

	printk("-------free-buf 0x%x-------\n", buf);
	vb->state = VIDEOBUF_NEEDS_INIT;
}

static int ls1c_videobuf_prepare(struct videobuf_queue *vq,
		struct videobuf_buffer *vb, enum v4l2_field field)
{
//	printk("-------ls1c_videobuf_prepare-------\n");
	struct soc_camera_device *icd = vq->priv_data;
	struct ls1c_camera_buffer *buf = container_of(vb, struct ls1c_camera_buffer, vb);
	int ret;
	int bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
						icd->current_fmt->host_fmt);

	if (bytes_per_line < 0)
		return bytes_per_line;

	dev_dbg(icd->dev.parent, "%s (vb=0x%p) 0x%08lx %d\n", __func__,
		vb, vb->baddr, vb->bsize);

	/* Added list head initialization on alloc */
	WARN_ON(!list_empty(&vb->queue));

	BUG_ON(NULL == icd->current_fmt);

	/*
	 * I think, in buf_prepare you only have to protect global data,
	 * the actual buffer is yours
	 */
	buf->inwork = 1;

	if (buf->code	!= icd->current_fmt->code ||
	    vb->width	!= icd->user_width ||
	    vb->height	!= icd->user_height ||
	    vb->field	!= field) {
		buf->code	= icd->current_fmt->code;
		vb->width	= icd->user_width;
		vb->height	= icd->user_height;
		vb->field	= field;
		vb->state	= VIDEOBUF_NEEDS_INIT;
	}

	vb->size = bytes_per_line * vb->height;
	if (0 != vb->baddr && vb->bsize < vb->size) {
		ret = -EINVAL;
		goto out;
	}

	if (vb->state == VIDEOBUF_NEEDS_INIT) {
		ret = videobuf_iolock(vq, vb, NULL);
		if (ret)
			goto fail;

		vb->state = VIDEOBUF_PREPARED;
	}

	buf->inwork = 0;

	return 0;

fail:
	free_buffer(vq, buf);
out:
	buf->inwork = 0;
	return ret;
}


/* Called under spinlock_irqsave(&pcdev->lock, ...) */
static int cnt = 0;
static void ls1c_videobuf_queue(struct videobuf_queue *vq,
						struct videobuf_buffer *vb)
{
	struct timex  txc;
	struct rtc_time tm;
	do_gettimeofday(&(txc.time));
	rtc_time_to_tm(txc.time.tv_sec,&tm);
	printk("UTC time :%d-%d-%d %d:%d:%d \n",tm.tm_year+1900,tm.tm_mon, tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);
//	printk("---------ls1c_videobuf_queue %d---------\n" ,flag);
	struct soc_camera_device *icd = vq->priv_data;
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct ls1c_camera_dev *pcdev = ici->priv;
	struct ls1c_camera_buffer *buf = container_of(vb, struct ls1c_camera_buffer, vb);
	unsigned int temp;
	unsigned int ret;
	struct videobuf_buffer *vbuf;
	unsigned long flags;
	//printk("-------pcdev 0x%x----------\n", pcdev);
	temp = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
	//printk("------config 0x%x-------\n", temp);
	while (temp & 0x40000000) { 
		temp = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
		temp |= (1 << 29);
	//	printk("----29 bits--------\n");
		__raw_writel(temp, pcdev->base + CAM_CAMIF_CONFIG);
	}
	dev_dbg(icd->dev.parent, "%s (vb=0x%p) 0x%08lx %d\n", __func__,
			vb, vb->baddr, vb->bsize);

	
	list_add_tail(&vb->queue, &pcdev->capture);
	vb->state = VIDEOBUF_ACTIVE;

#if 0
	if (!pcdev->active) {
		pcdev->active = buf;

		/* setup sg list for future DMA */
		if (!ls1c_camera_setup_dma(pcdev)) {
			unsigned int temp;
			/* enable SOF irq */
			temp = __raw_readl(pcdev->base + CSICR1) |
				CSICR1_SOF_INTEN;
			__raw_writel(temp, pcdev->base + CSICR1);
		}
	}
#else
	//printk("count = %d \n", cnt);
	cnt++;
	if (buf_init_cnt < LS1C_CAM_DMA_CNT) {
		if (!pcdev->active) {
			pcdev->active = buf;
			//printk("-----------pcdev->active = buf\n");
//			vbuf = &pcdev->active->vb;
		}
		//vbuf = &buf->vb;
		vbuf = &buf->vb;
		temp = (unsigned int)videobuf_to_dma_contig(vbuf);
		__raw_writel(temp, pcdev->base + (CAM_DMA0_CONFIG + buf_init_cnt * 8));
		buf_init_cnt++;

			temp = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
			temp |= (1 << LS1C_CAM_START) /*| (1 << 13) & (~(1<<11))*/;
			__raw_writel(temp, pcdev->base + CAM_CAMIF_CONFIG);
#ifdef	POLL_CAMERA
			if (flag) {
			//printk("---------ls1c_camera_thread--------\n");
				flag = 0 ;
				tsk = kthread_run(camera_thread, thread_data, "ls1c_camera_thread");
			}
#endif
			//temp = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
			//temp |= (1 << LS1C_CAM_START) /*| (1 << 13) & (~(1<<11))*/;
			//__raw_writel(temp, pcdev->base + CAM_CAMIF_CONFIG);
		if (buf_init_cnt == LS1C_CAM_DMA_CNT) {
#if 0
//#ifdef	POLL_CAMERA
			if (flag) {
			printk("---------ls1c_camera_thread--------\n");
			flag = 0 ;
			tsk = kthread_run(camera_thread, thread_data, "ls1c_camera_thread");
			}
//#endif
			temp = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
			temp |= (1 << LS1C_CAM_START) /*| (1 << 13) & (~(1<<11))*/;
			__raw_writel(temp, pcdev->base + CAM_CAMIF_CONFIG);
#endif
			buf_init_cnt = 0;
			//enable_irq(36);
		
			//tsk = kthread_run(camera_thread, thread_data, "ls1c_camera_thread");
			//request_irq(36, ls1c_camera_irq, /*IRQF_DISABLED*/ IRQF_SHARED | 0x1, /*pdev->name*/DRIVER_NAME, pcdev);
		}

	}
#endif
}

static void ls1c_videobuf_release(struct videobuf_queue *vq,
				 struct videobuf_buffer *vb)
{
	struct ls1c_camera_buffer *buf = container_of(vb, struct ls1c_camera_buffer, vb);
#define DEBUG
	//printk("-------ls1c_videobuf_release buf 0x%x-----\n", buf);
#ifdef DEBUG
	struct soc_camera_device *icd = vq->priv_data;
	struct device *dev = icd->dev.parent;

	dev_dbg(dev, "%s (vb=0x%p) 0x%08lx %d\n", __func__,
		vb, vb->baddr, vb->bsize);

	switch (vb->state) {
	case VIDEOBUF_ACTIVE:
		dev_dbg(dev, "%s (active)\n", __func__);
		break;
	case VIDEOBUF_QUEUED:
		dev_dbg(dev, "%s (queued)\n", __func__);
		break;
	case VIDEOBUF_PREPARED:
		dev_dbg(dev, "%s (prepared)\n", __func__);
		break;
	default:
		dev_dbg(dev, "%s (unknown)\n", __func__);
		break;
	}
#endif

	free_buffer(vq, buf);
}

#ifdef	POLL_CAMERA
static long passtime=0;
static long stime=0;
static long etime=0;
static int times=0;
static int chg = 0;
static long sums = 0;
static int sped = 0;
static int camera_thread(void *pthread_data)
{
	struct ls1c_camera_dev *pcdev = (struct ls1c_camera_dev *)pthread_data;
	struct device *dev = pcdev->icd->dev.parent;
	struct ls1c_camera_buffer *buf;
	struct videobuf_buffer *vb;
	unsigned int temp;
	struct videobuf_buffer *vbuf;
	unsigned long flags;

	while (1) {
		//printk ("lxy: in %s !\n", __FUNCTION__);
		//printk("tsped %d \n", sped);
		//sped++;
		temp = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
		//printk("temp 0x%x---------\n", temp);
		if ((temp & 0xf00000) == 0)
			goto schedu;


#if 0
		while (temp & 0x40000000) { 
		temp = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
		temp |= (1 << 29);
		printk("----29 bits--------\n");
		__raw_writel(temp, pcdev->base + CAM_CAMIF_CONFIG);
		}
#endif
#if 0
		if (unlikely(!pcdev->active)) {
//			dev_err(dev, "DMA End IRQ with no active buffer\n");
			goto schedu;
		}
#else
		if (list_empty(&pcdev->capture)) {
			pcdev->active = NULL;
			goto schedu;
		}
		
		if (unlikely(!pcdev->active)) {
			pcdev->active = list_entry(pcdev->capture.next,
					struct ls1c_camera_buffer, vb.queue);
		}
#endif

		//printk ("lxy: ------> put data to user space !\n");
		//spin_lock_irqsave(&pcdev->lock, flags); 
		vb = &pcdev->active->vb;
		buf = container_of(vb, struct ls1c_camera_buffer, vb);
		WARN_ON(buf->inwork || list_empty(&vb->queue));
		dev_dbg(dev, "%s (vb=0x%p) 0x%08lx %d\n", __func__,
				vb, vb->baddr, vb->bsize);

		/* _init is used to debug races, see comment in ls1c_camera_reqbufs() */
		list_del_init(&vb->queue);
		vb->state = VIDEOBUF_DONE;
		do_gettimeofday(&vb->ts);
		
#if 0
		if (chg) {
			stime = vb->ts.tv_usec ;
			chg = 0;
		} else {
			etime = vb->ts.tv_usec;
			chg = 1;
		}
		
		passtime = abs(etime - stime);
		sums += passtime;
		printk("counts %d two frames interval 0d%ld us\n", times,sums);
		times++;
#endif

		vb->field_count++;
		wake_up(&vb->done);

		if (list_empty(&pcdev->capture)) {
			pcdev->active = NULL;
			goto schedu;
		}

		pcdev->active = list_entry(pcdev->capture.next,
				struct ls1c_camera_buffer, vb.queue);
			
		vbuf = &pcdev->active->vb;
		
		//spin_unlock_irqrestore(&pcdev->lock, flags); 
#if 0
		temp = (unsigned int)videobuf_to_dma_contig(vbuf);
		__raw_writel(temp, pcdev->base + (CAM_DMA0_CONFIG + 0));
		temp = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
		temp |= (1 << LS1C_CAM_START) /*| (1 << 13) & (~(1<<11))*/;
		__raw_writel(temp, pcdev->base + CAM_CAMIF_CONFIG);
#endif
schedu:
		schedule();

		if (kthread_should_stop())
			break;
	}
out:
	return 0;
}
#else
static irqreturn_t ls1c_camera_irq(int irq, void *data)
//static void ls1c_camera_irq(int irq, void *data)
{
	disable_irq_nosync(36);
	//printk("--------ls1c_camera_irq-------\n");
	struct ls1c_camera_dev *pcdev = data;
	struct device *dev = pcdev->icd->dev.parent;
	struct ls1c_camera_buffer *buf;
	struct videobuf_buffer *vb;
	unsigned long flags;
	int temp;

		temp = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
	//printk("-------temp value 0x%x-------\n", temp);
	//spin_lock_irqsave(&pcdev->lock, flags);
	if (unlikely(!pcdev->active)) {
	//	dev_err(dev, "DMA End IRQ with no active buffer\n");
		goto out;
	}

	vb = &pcdev->active->vb;
	buf = container_of(vb, struct ls1c_camera_buffer, vb);
	WARN_ON(buf->inwork || list_empty(&vb->queue));
	dev_dbg(dev, "%s (vb=0x%p) 0x%08lx %d\n", __func__,
			vb, vb->baddr, vb->bsize);

	//printk("--------list_del_init_b-------\n");
	/* _init is used to debug races, see comment in ls1c_camera_reqbufs() */
	list_del_init(&vb->queue);
	vb->state = VIDEOBUF_DONE;
	do_gettimeofday(&vb->ts);
	vb->field_count++;
	wake_up(&vb->done);

	if (list_empty(&pcdev->capture)) {
		pcdev->active = NULL;
		printk("----------finish -------\n");
		enable_irq(36);
		return IRQ_HANDLED;
		//return;
	}
	pcdev->active = list_entry(pcdev->capture.next,
				   struct ls1c_camera_buffer, vb.queue);
	printk("----------finish process irq--------\n");
	//enable_irq(36);
	temp = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
	temp |= (1 << 29);
	__raw_writel(temp, pcdev->base + CAM_CAMIF_CONFIG);
	enable_irq(36);
out:
	//spin_unlock_irqrestore(&pcdev->lock, flags); 
	//printk("--------out-------\n");
	enable_irq(36);
	return IRQ_HANDLED;
}
#endif


static struct videobuf_queue_ops ls1c_videobuf_ops = {
	.buf_setup	= ls1c_videobuf_setup,
	.buf_prepare	= ls1c_videobuf_prepare,
	.buf_queue	= ls1c_videobuf_queue,
	.buf_release	= ls1c_videobuf_release,
};

static void ls1c_camera_init_videobuf(struct videobuf_queue *q,
				     struct soc_camera_device *icd)
{
	printk("---------init videobuf----------\n");
	do_gettimeofday(&t_start);
	printk("t_start %ld\n", t_start.tv_sec);
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct ls1c_camera_dev *pcdev = ici->priv;

	videobuf_queue_dma_contig_init(q, &ls1c_videobuf_ops, icd->dev.parent,
				&pcdev->lock, V4L2_BUF_TYPE_VIDEO_CAPTURE,
				V4L2_FIELD_NONE,
				sizeof(struct ls1c_camera_buffer), icd, &icd->video_lock);
}

#if 0
static int mclk_get_divisor(struct ls1c_camera_dev *pcdev)
{
	return 1;
#if 0
	unsigned int mclk = pcdev->mclk;
	unsigned long div;
	unsigned long lcdclk;

	lcdclk = clk_get_rate(pcdev->clk);

	/*
	 * We verify platform_mclk_10khz != 0, so if anyone breaks it, here
	 * they get a nice Oops
	 */
	div = (lcdclk + 2 * mclk - 1) / (2 * mclk) - 1;

	dev_dbg(pcdev->icd->dev.parent,
		"System clock %lukHz, target freq %dkHz, divisor %lu\n",
		lcdclk / 1000, mclk / 1000, div);
	return div;
#endif
}
#endif

static void ls1c_camera_activate(struct ls1c_camera_dev *pcdev)
{
	dev_dbg(pcdev->icd->dev.parent, "Activate device\n");

//	clk_enable(pcdev->clk);

	/* enable camera interface before doing anything else */
}

static void ls1c_camera_deactivate(struct ls1c_camera_dev *pcdev)
{
	dev_dbg(pcdev->icd->dev.parent, "Deactivate device\n");

	/* Disable all Camera interface */

//	clk_disable(pcdev->clk);
}

/*
 * The following two functions absolutely depend on the fact, that
 * there can be only one camera on LS1C camera sensor interface
 */
static int ls1c_camera_add_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct ls1c_camera_dev *pcdev = ici->priv;

	if (pcdev->icd)
		return -EBUSY;

	dev_info(icd->dev.parent, "LS1C Camera driver attached to camera %d\n",
		 icd->devnum);

	ls1c_camera_activate(pcdev);

	pcdev->icd = icd;

	return 0;
}

static void ls1c_camera_remove_device(struct soc_camera_device *icd)
{
	do_gettimeofday(&t_end);
	cost_time = t_end.tv_sec - t_start.tv_sec;

	printk("Cost time: %ld us", cost_time);
	printk("t_end %ld\n", t_end.tv_sec);
	printk("--------ls1c_cam_remove_dev--------\n");
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct ls1c_camera_dev *pcdev = ici->priv;
	unsigned int temp;
	unsigned long flags;

	BUG_ON(icd != pcdev->icd);

	buf_init_cnt = 0;
	//*(unsigned int *)(0xbc280038) &= ~(1 << 30);
	temp = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
	temp &= ~(1 << LS1C_CAM_START);
	temp &= ~(1 << 30);
	//temp |= (1 << 29);
	__raw_writel(temp, pcdev->base + CAM_CAMIF_CONFIG);

	local_irq_save(flags);
	disable_irq(LS1X_CAM_IRQ);
	local_irq_restore(flags);
#ifdef	POLL_CAMERA
	if (tsk != NULL) {
		printk("------stop one thread---------\n");
		kthread_stop(tsk);
		tsk = NULL;
	}
#endif
	dev_info(icd->dev.parent, "LS1C Camera driver detached from camera %d\n",
		 icd->devnum);

	ls1c_camera_deactivate(pcdev);

	pcdev->icd = NULL;
}

static int ls1c_camera_set_crop(struct soc_camera_device *icd,
			       struct v4l2_crop *a)
{
	
	//printk("-----------ls1c_camerea-set_crop------\n");
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);

	return v4l2_subdev_call(sd, video, s_crop, a);
}

static int ls1c_camera_set_bus_param(struct soc_camera_device *icd, __u32 pixfmt)
{
	//printk("-----------ls1c_camerea-set_bus_param------\n");
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct ls1c_camera_dev *pcdev = ici->priv;
	unsigned long camera_flags, common_flags;
	unsigned int csicr1;
	int ret;

	camera_flags = icd->ops->query_bus_param(icd);

	/* LS1C supports only 8bit buswidth */
	common_flags = soc_camera_bus_param_compatible(camera_flags,
						       CSI_BUS_FLAGS);
	if (!common_flags)
		return -EINVAL;

	/* Make choises, based on platform choice */
	if ((common_flags & SOCAM_VSYNC_ACTIVE_HIGH) &&
		(common_flags & SOCAM_VSYNC_ACTIVE_LOW)) {
			if (!pcdev->pdata ||
			     pcdev->pdata->flags & LS1C_CAMERA_VSYNC_HIGH)
				common_flags &= ~SOCAM_VSYNC_ACTIVE_LOW;
			else
				common_flags &= ~SOCAM_VSYNC_ACTIVE_HIGH;
	}

	if ((common_flags & SOCAM_PCLK_SAMPLE_RISING) &&
		(common_flags & SOCAM_PCLK_SAMPLE_FALLING)) {
			if (!pcdev->pdata ||
			     pcdev->pdata->flags & LS1C_CAMERA_PCLK_RISING)
				common_flags &= ~SOCAM_PCLK_SAMPLE_FALLING;
			else
				common_flags &= ~SOCAM_PCLK_SAMPLE_RISING;
	}

	if ((common_flags & SOCAM_DATA_ACTIVE_HIGH) &&
		(common_flags & SOCAM_DATA_ACTIVE_LOW)) {
			if (!pcdev->pdata ||
			     pcdev->pdata->flags & LS1C_CAMERA_DATA_HIGH)
				common_flags &= ~SOCAM_DATA_ACTIVE_LOW;
			else
				common_flags &= ~SOCAM_DATA_ACTIVE_HIGH;
	}

	ret = icd->ops->set_bus_param(icd, common_flags);
	if (ret < 0)
		return ret;

	csicr1 = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);

	if (common_flags & SOCAM_VSYNC_ACTIVE_LOW)
		csicr1 |= CONFIG_PARA_VS;
	if (common_flags & SOCAM_HSYNC_ACTIVE_LOW)
		csicr1 |= CONFIG_PARA_HS;

#if 0
	__raw_writel(csicr1, pcdev->base + CAM_CAMIF_CONFIG);
#else
	csicr1 |=  (1 << CONFIG_PARA_640X480) ;
	csicr1 |= (1 << 13) | (1 << CONFIG_PARA_640X480) ;//rgb
//	csicr1 |= (1 << CONFIG_PARA_INPUT_DATA_MODE) | (1 << CONFIG_PARA_640X480) ;//yuv
//	printk("------------csicr1 0x%x----------\n", csicr1);
	__raw_writel(csicr1, pcdev->base + CAM_CAMIF_CONFIG);
	//__raw_writel((640*480), pcdev->base + CAM_UOFF_CONFIG);
	//__raw_writel((640*480*3/2), pcdev->base + CAM_VOFF_CONFIG);
	*(volatile unsigned int *)(0xbfd00414) |= 0x200000;
#endif

	return 0;
}

static int ls1c_camera_set_fmt(struct soc_camera_device *icd,
			      struct v4l2_format *f)
{
	//printk("-----------ls1c_camerea-set_fmt------\n");
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	const struct soc_camera_format_xlate *xlate;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_mbus_framefmt mf;
	int ret, buswidth;

	xlate = soc_camera_xlate_by_fourcc(icd, pix->pixelformat);
	if (!xlate) {
		dev_warn(icd->dev.parent, "Format %x not found\n",
			 pix->pixelformat);
		return -EINVAL;
	}

	buswidth = xlate->host_fmt->bits_per_sample;
	if (buswidth > 8) {
		dev_warn(icd->dev.parent,
			 "bits-per-sample %d for format %x unsupported\n",
			 buswidth, pix->pixelformat);
		return -EINVAL;
	}

	mf.width	= pix->width;
	mf.height	= pix->height;
	mf.field	= pix->field;
	mf.colorspace	= pix->colorspace;
	mf.code		= xlate->code;

	ret = v4l2_subdev_call(sd, video, s_mbus_fmt, &mf);
	if (ret < 0)
		return ret;

	if (mf.code != xlate->code)
		return -EINVAL;

	pix->width		= mf.width;
	//printk("------pix width %s --------\n", pix->width);
	pix->height		= mf.height;
	pix->field		= mf.field;
	pix->colorspace		= mf.colorspace;
	icd->current_fmt	= xlate;

	int flags;
#if 0
	switch (pix->width) {
		case 640:
			*(volatile unsigned int *)(0xbc280038) |= (1 << CONFIG_PARA_INPUT_DATA_MODE) | (1 << CONFIG_PARA_640X480) ;//yuv
			break;
		case 320:
			*(volatile unsigned int *)(0xbc280038) |= (1 << CONFIG_PARA_INPUT_DATA_MODE) & (~(1 << CONFIG_PARA_640X480)) ;//yuv
			break;
	}
#endif
	return ret;
}

static int ls1c_camera_try_fmt(struct soc_camera_device *icd,
			      struct v4l2_format *f)
{
	//printk("-----------ls1c_camerea-try_fmt------\n");
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	const struct soc_camera_format_xlate *xlate;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_mbus_framefmt mf;
	int ret;
	/* TODO: limit to ls1c hardware capabilities */

	xlate = soc_camera_xlate_by_fourcc(icd, pix->pixelformat);
	if (!xlate) {
		dev_warn(icd->dev.parent, "Format %x not found\n",
			 pix->pixelformat);
		return -EINVAL;
	}

	mf.width	= pix->width;
	mf.height	= pix->height;
	mf.field	= pix->field;
	mf.colorspace	= pix->colorspace;
	mf.code		= xlate->code;

	/* limit to sensor capabilities */
	ret = v4l2_subdev_call(sd, video, try_mbus_fmt, &mf);
	if (ret < 0)
		return ret;

	pix->width	= mf.width;
	pix->height	= mf.height;
	pix->field	= mf.field;
	pix->colorspace	= mf.colorspace;

	return 0;
}

static int ls1c_camera_reqbufs(struct soc_camera_device *icd,
			      struct v4l2_requestbuffers *p)
{
	int i;

	/*
	 * This is for locking debugging only. I removed spinlocks and now I
	 * check whether .prepare is ever called on a linked buffer, or whether
	 * a dma IRQ can occur for an in-work or unlinked buffer. Until now
	 * it hadn't triggered
	 */
	for (i = 0; i < p->count; i++) {
		struct ls1c_camera_buffer *buf = container_of(icd->vb_vidq.bufs[i],
						      struct ls1c_camera_buffer, vb);
		buf->inwork = 0;
		INIT_LIST_HEAD(&buf->vb.queue);
	}

	return 0;
}

static unsigned int ls1c_camera_poll(struct file *file, poll_table *pt)
{
	struct soc_camera_device *icd = file->private_data;
	struct ls1c_camera_buffer *buf;

	buf = list_entry(icd->vb_vidq.stream.next, struct ls1c_camera_buffer,
			 vb.stream);

	poll_wait(file, &buf->vb.done, pt);

	if (buf->vb.state == VIDEOBUF_DONE ||
	    buf->vb.state == VIDEOBUF_ERROR)
		return POLLIN | POLLRDNORM;

	return 0;
}

static int ls1c_camera_querycap(struct soc_camera_host *ici,
			       struct v4l2_capability *cap)
{
	/* cap->name is set by the friendly caller:-> */
	strlcpy(cap->card, "LS1C Camera", sizeof(cap->card));
	cap->version = VERSION_CODE;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;

	return 0;
}

static struct soc_camera_host_ops ls1c_soc_camera_host_ops = {
	.owner		= THIS_MODULE,
	.add		= ls1c_camera_add_device,
	.remove		= ls1c_camera_remove_device,
	.set_bus_param	= ls1c_camera_set_bus_param,
	.set_crop	= ls1c_camera_set_crop,
	.set_fmt	= ls1c_camera_set_fmt,
	.try_fmt	= ls1c_camera_try_fmt,
	.init_videobuf	= ls1c_camera_init_videobuf,
	.reqbufs	= ls1c_camera_reqbufs,
	.poll		= ls1c_camera_poll,
	.querycap	= ls1c_camera_querycap,
};

static void four_secs_timer(unsigned long arg) 
{
	*(unsigned int*)(0xbfd0105c) |= (1<<4); 
	//*(unsigned int*)(0xbfd0106c) |= (1<<4);
	//*(unsigned int*)(0xbfd01068) &= ~(1<<4);
	printk("---------irq sr 0x%x-------\n", *(unsigned int *)(0xbfd01058));
	printk("---------irq en 0x%x-------\n", *(unsigned int *)(0xbfd0105c));
	printk("---------irq set 0x%x-------\n", *(unsigned int *)(0xbfd01060));
	printk("---------irq clr 0x%x-------\n", *(unsigned int *)(0xbfd01064));
	printk("---------irq pol 0x%x-------\n", *(unsigned int *)(0xbfd01068));
	printk("---------irq edge 0x%x-------\n", *(unsigned int *)(0xbfd0106c));
	my_timer.expires = jiffies + HZ * 5; 
	add_timer(&my_timer);
}

static int __init ls1c_camera_probe(struct platform_device *pdev)
{
#if 0
	init_timer(&my_timer); 
	my_timer.function = &four_secs_timer;
	my_timer.expires = jiffies + HZ * 10; 
	add_timer(&my_timer);
#endif

	struct ls1c_camera_dev *pcdev;
	struct resource *res;
	struct clk *clk;
	void __iomem *base;
	unsigned int irq;
	int err = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!res || (int)irq <= 0) {
		err = -ENODEV;
		goto exit;
	}

#if 0
	clk = clk_get(&pdev->dev, "camera_clk");
	if (IS_ERR(clk)) {
		err = PTR_ERR(clk);
		goto exit;
	}
#endif

	pcdev = kzalloc(sizeof(*pcdev), GFP_KERNEL);
	if (!pcdev) {
		dev_err(&pdev->dev, "Could not allocate pcdev\n");
		err = -ENOMEM;
		goto exit_put_clk;
	}

	pcdev->res = res;
//	pcdev->clk = clk;

	pcdev->pdata = pdev->dev.platform_data;

	if (pcdev->pdata)
		pcdev->mclk = pcdev->pdata->mclk_24MHz;

	if (!pcdev->mclk) {
		dev_warn(&pdev->dev,
			 "mclk_24MHz == 0! Please, fix your platform data. "
			 "Using default 24MHz\n");
		pcdev->mclk = 24000000;
	}

	INIT_LIST_HEAD(&pcdev->capture);
	spin_lock_init(&pcdev->lock);

	/*
	 * Request the regions.
	 */
	if (!request_mem_region(res->start, resource_size(res), DRIVER_NAME)) {
		err = -EBUSY;
		goto exit_kfree;
	}

	base = ioremap(res->start, resource_size(res));
	if (!base) {
		err = -ENOMEM;
		goto exit_release;
	}
	pcdev->irq = irq;
	pcdev->base = base;

#ifdef	POLL_CAMERA
	thread_data = pcdev;
#else
	/* request irq */
//	err = request_irq(irq, ls1c_camera_irq, /*IRQF_DISABLED*/ IRQF_SHARED | 0x1, /*pdev->name*/DRIVER_NAME, pcdev);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to request IRQ\n");
		goto exit_iounmap;
	}
#endif

	pcdev->soc_host.drv_name	= DRIVER_NAME;
	pcdev->soc_host.ops		= &ls1c_soc_camera_host_ops;
	pcdev->soc_host.priv		= pcdev;
	pcdev->soc_host.v4l2_dev.dev	= &pdev->dev;
	pcdev->soc_host.nr		= pdev->id;
	err = soc_camera_host_register(&pcdev->soc_host);
	if (err)
		goto exit_free_irq;

	dev_info(&pdev->dev, "LS1C Camera driver loaded !\n");

	return 0;

exit_free_irq:
	free_irq(LS1X_CAM_IRQ, pcdev);
exit_iounmap:
	iounmap(base);
exit_release:
	release_mem_region(res->start, resource_size(res));
exit_kfree:
	kfree(pcdev);
exit_put_clk:
	clk_put(clk);
exit:
	return err;
}

static int __exit ls1c_camera_remove(struct platform_device *pdev)
{
	flag = 1;
	struct soc_camera_host *soc_host = to_soc_camera_host(&pdev->dev);
	struct ls1c_camera_dev *pcdev = container_of(soc_host,
					struct ls1c_camera_dev, soc_host);
	struct resource *res;

	clk_put(pcdev->clk);

	soc_camera_host_unregister(soc_host);

	iounmap(pcdev->base);

	res = pcdev->res;
	release_mem_region(res->start, resource_size(res));

	kfree(pcdev);

	dev_info(&pdev->dev, "LS1C Camera driver unloaded\n");

	return 0;
}

static struct platform_driver ls1c_camera_driver = {
	.driver 	= {
		.name	= DRIVER_NAME,
	},
	.remove		= __exit_p(ls1c_camera_remove),
};

static int __init ls1c_camera_init(void)
{
	flag = 1;
	return platform_driver_probe(&ls1c_camera_driver, ls1c_camera_probe);
}

static void __exit ls1c_camera_exit(void)
{
	flag = 0;
	return platform_driver_unregister(&ls1c_camera_driver);
}

module_init(ls1c_camera_init);
module_exit(ls1c_camera_exit);

MODULE_DESCRIPTION("LS1C_SoC Camera Host driver");
MODULE_AUTHOR("linxiyuan <linxiyuan-gz@foxmail.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRIVER_NAME);
