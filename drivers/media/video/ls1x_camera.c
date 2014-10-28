/*
 * V4L2 Driver for Loongson1 SoC camera host
 *
 * Copyright (C) 2013-05-08, linxiyuan <linxiyuan-gz@foxmail.com>
 * Copyright (C) 2014, tanghaifeng <pengren.mcu@qq.com>
 *
 * Based on mx1 SoC camera driver
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

#include <media/soc_camera.h>
#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/videobuf-dma-contig.h>
#include <media/soc_mediabus.h>

#include <loongson1.h>
#include <ls1x_camera.h>

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
#define CAM_CAMIF_CONFIG	0x38

#define CONFIG_PARA_CLR (0x1 << 31)
#define CONFIG_PARA_SINGLEFIELDEN (0x1 << 6)
#define CONFIG_PARA_BGR_EN (0x1 << 3)
#define CONFIG_PARA_RGB_FORMAT (0x1 << 2)
#define CONFIG_PARA_HS (0x1 << 1)
#define CONFIG_PARA_VS (0x1 << 0)

#define LS1X_CAM_DMA_CNT	4
#define LS1X_CAM_START		31

#define VERSION_CODE KERNEL_VERSION(0, 0, 1)
#define DRIVER_NAME "ls1x-camera"

#define CSI_BUS_FLAGS	(SOCAM_MASTER | SOCAM_HSYNC_ACTIVE_HIGH | \
			SOCAM_VSYNC_ACTIVE_HIGH | SOCAM_VSYNC_ACTIVE_LOW | \
			SOCAM_DATA_ACTIVE_HIGH | SOCAM_DATA_ACTIVE_LOW | \
			SOCAM_DATAWIDTH_8)

//#define MAX_VIDEO_MEM 4	/* Video memory limit in megabytes */
#define MAX_VIDEO_MEM 16	/* Video memory limit in megabytes */

#define POLL_CAMERA	1


/* buffer for one video frame */
struct ls1x_camera_buffer {
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
struct ls1x_camera_dev {
	struct soc_camera_host		soc_host;
	struct soc_camera_device	*icd;
	struct ls1x_camera_pdata	*pdata;
	struct ls1x_camera_buffer	*active;
	struct resource	*res;
	struct clk			*clk;
	struct list_head	capture;

	void __iomem	*base;
	int				dma_chan;
	unsigned int	irq;
	unsigned long	mclk;

	spinlock_t		lock;
};

#ifdef POLL_CAMERA
#include <linux/kthread.h>
static struct task_struct *tsk = NULL;
static struct ls1x_camera_dev *thread_data;
static int camera_thread(void *pthread_data);
#endif
static unsigned int flag = 0;
static unsigned int buf_init_cnt = 0;

/*
 *  Videobuf operations
 */
static int ls1x_videobuf_setup(struct videobuf_queue *vq, unsigned int *count,
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

static void free_buffer(struct videobuf_queue *vq, struct ls1x_camera_buffer *buf)
{
	struct soc_camera_device *icd = vq->priv_data;
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct ls1x_camera_dev *pcdev = ici->priv;
	struct videobuf_buffer *vb = &buf->vb;
	unsigned int config;

	flag = 1;

	BUG_ON(in_interrupt());

	dev_dbg(icd->dev.parent, "%s (vb=0x%p) 0x%08lx %d\n", __func__,
		vb, vb->baddr, vb->bsize);
	
	config = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
	config &= ~(1 << LS1X_CAM_START);
	__raw_writel(config, pcdev->base + CAM_CAMIF_CONFIG);

	config = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
	while (config & 0x40000000) { 
		config = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
		config |= (1 << 29);
		__raw_writel(config, pcdev->base + CAM_CAMIF_CONFIG);
	}

	/*
	 * This waits until this buffer is out of danger, i.e., until it is no
	 * longer in STATE_QUEUED or STATE_ACTIVE
	 */
	videobuf_waiton(vq, vb, 0, 0);
	videobuf_dma_contig_free(vq, vb);

	vb->state = VIDEOBUF_NEEDS_INIT;
}

static int ls1x_videobuf_prepare(struct videobuf_queue *vq,
		struct videobuf_buffer *vb, enum v4l2_field field)
{
	struct soc_camera_device *icd = vq->priv_data;
	struct ls1x_camera_buffer *buf = container_of(vb, struct ls1x_camera_buffer, vb);
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

static void ls1x_videobuf_queue(struct videobuf_queue *vq,
						struct videobuf_buffer *vb)
{
	struct soc_camera_device *icd = vq->priv_data;
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct ls1x_camera_dev *pcdev = ici->priv;
	struct ls1x_camera_buffer *buf = container_of(vb, struct ls1x_camera_buffer, vb);
	unsigned int config;
	struct videobuf_buffer *vbuf;

	config = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
	while (config & 0x40000000) {
		config = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
		config |= (1 << 29);
		__raw_writel(config, pcdev->base + CAM_CAMIF_CONFIG);
	}
	dev_dbg(icd->dev.parent, "%s (vb=0x%p) 0x%08lx %d\n", __func__,
			vb, vb->baddr, vb->bsize);

	list_add_tail(&vb->queue, &pcdev->capture);
	vb->state = VIDEOBUF_ACTIVE;

	if (buf_init_cnt < LS1X_CAM_DMA_CNT) {
		if (!pcdev->active) {
			pcdev->active = buf;
		}
		vbuf = &buf->vb;
		config = (unsigned int)videobuf_to_dma_contig(vbuf);
		__raw_writel(config, pcdev->base + (CAM_DMA0_CONFIG + buf_init_cnt * 8));
		buf_init_cnt++;

		config = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
		config |= (1 << LS1X_CAM_START) /*| (1 << 13) & (~(1<<11))*/;
		__raw_writel(config, pcdev->base + CAM_CAMIF_CONFIG);		//start camera
		
#ifdef POLL_CAMERA
		if (flag) {
			flag = 0;
			tsk = kthread_run(camera_thread, thread_data, "ls1x_camera_thread");
		}
#endif
		if (buf_init_cnt == LS1X_CAM_DMA_CNT) {
			buf_init_cnt = 0;
		}
	}
}

static void ls1x_videobuf_release(struct videobuf_queue *vq,
				 struct videobuf_buffer *vb)
{
	struct ls1x_camera_buffer *buf = container_of(vb, struct ls1x_camera_buffer, vb);
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

#ifdef POLL_CAMERA
static int camera_thread(void *pthread_data)
{
	struct ls1x_camera_dev *pcdev = (struct ls1x_camera_dev *)pthread_data;
	struct device *dev = pcdev->icd->dev.parent;
	struct ls1x_camera_buffer *buf;
	struct videobuf_buffer *vb;
	unsigned int config;
	struct videobuf_buffer *vbuf;

	while (1) {
		config = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
		if ((config & 0xf00000) == 0)	//dma buffer is empty
			goto schedu;

		if (list_empty(&pcdev->capture)) {
			pcdev->active = NULL;
			goto schedu;
		}
		
		if (unlikely(!pcdev->active)) {
			pcdev->active = list_entry(pcdev->capture.next,
					struct ls1x_camera_buffer, vb.queue);
		}

		vb = &pcdev->active->vb;
		buf = container_of(vb, struct ls1x_camera_buffer, vb);
		WARN_ON(buf->inwork || list_empty(&vb->queue));
		dev_dbg(dev, "%s (vb=0x%p) 0x%08lx %d\n", __func__,
				vb, vb->baddr, vb->bsize);

		/* _init is used to debug races, see comment in ls1x_camera_reqbufs() */
		list_del_init(&vb->queue);
		vb->state = VIDEOBUF_DONE;
		do_gettimeofday(&vb->ts);

		vb->field_count++;
		wake_up(&vb->done);

		msleep(20);

		if (list_empty(&pcdev->capture)) {
			pcdev->active = NULL;
			goto schedu;
		}

		pcdev->active = list_entry(pcdev->capture.next,
				struct ls1x_camera_buffer, vb.queue);
			
		vbuf = &pcdev->active->vb;
schedu:
		msleep(10);
		schedule();

		if (kthread_should_stop())
			break;
	}

	return 0;
}
#else
static void ls1x_camera_wakeup(struct ls1x_camera_dev *pcdev,
			      struct videobuf_buffer *vb,
			      struct ls1x_camera_buffer *buf)
{
	/* _init is used to debug races, see comment in ls1x_camera_reqbufs() */
	list_del_init(&vb->queue);
	vb->state = VIDEOBUF_DONE;
	do_gettimeofday(&vb->ts);
	vb->field_count++;
	wake_up(&vb->done);

	if (list_empty(&pcdev->capture)) {
		pcdev->active = NULL;
		return;
	}

	pcdev->active = list_entry(pcdev->capture.next,
				   struct ls1x_camera_buffer, vb.queue);

	/* setup sg list for future DMA */
}

static irqreturn_t ls1x_camera_irq(int irq, void *data)
{
	struct ls1x_camera_dev *pcdev = data;
	struct device *dev = pcdev->icd->dev.parent;
	struct ls1x_camera_buffer *buf;
	struct videobuf_buffer *vb;
//	unsigned long flags;
//	unsigned int config;

//	spin_lock_irqsave(&pcdev->lock, flags);

/*	config = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
	config &= ~(1 << LS1X_CAM_START);
	__raw_writel(config, pcdev->base + CAM_CAMIF_CONFIG);*/
//	ls1x_dma_disable(channel);

	if (unlikely(!pcdev->active)) {
		dev_err(dev, "DMA End IRQ with no active buffer\n");
		goto out;
	}

	vb = &pcdev->active->vb;
	buf = container_of(vb, struct ls1x_camera_buffer, vb);
	WARN_ON(buf->inwork || list_empty(&vb->queue));
	dev_dbg(dev, "%s (vb=0x%p) 0x%08lx %d\n", __func__,
		vb, vb->baddr, vb->bsize);

	ls1x_camera_wakeup(pcdev, vb, buf);
out:
//	spin_unlock_irqrestore(&pcdev->lock, flags);
	return IRQ_HANDLED;
}
#endif

static struct videobuf_queue_ops ls1x_videobuf_ops = {
	.buf_setup	= ls1x_videobuf_setup,
	.buf_prepare	= ls1x_videobuf_prepare,
	.buf_queue	= ls1x_videobuf_queue,
	.buf_release	= ls1x_videobuf_release,
};

static void ls1x_camera_init_videobuf(struct videobuf_queue *q,
				     struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct ls1x_camera_dev *pcdev = ici->priv;

	videobuf_queue_dma_contig_init(q, &ls1x_videobuf_ops, icd->dev.parent,
				&pcdev->lock, V4L2_BUF_TYPE_VIDEO_CAPTURE,
				V4L2_FIELD_NONE,
				sizeof(struct ls1x_camera_buffer), icd, &icd->video_lock);
}

static void ls1x_camera_activate(struct ls1x_camera_dev *pcdev)
{
	dev_dbg(pcdev->icd->dev.parent, "Activate device\n");

//	clk_enable(pcdev->clk);
}

static void ls1x_camera_deactivate(struct ls1x_camera_dev *pcdev)
{
	dev_dbg(pcdev->icd->dev.parent, "Deactivate device\n");

//	clk_disable(pcdev->clk);
}

/*
 * The following two functions absolutely depend on the fact, that
 * there can be only one camera on LS1X camera sensor interface
 */
static int ls1x_camera_add_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct ls1x_camera_dev *pcdev = ici->priv;

	if (pcdev->icd)
		return -EBUSY;

	dev_info(icd->dev.parent, "LS1X Camera driver attached to camera %d\n",
		 icd->devnum);

	enable_irq(pcdev->irq);

	ls1x_camera_activate(pcdev);

	pcdev->icd = icd;

	return 0;
}

static void ls1x_camera_remove_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct ls1x_camera_dev *pcdev = ici->priv;
	unsigned int config;
	unsigned long flags;

	BUG_ON(icd != pcdev->icd);

	buf_init_cnt = 0;
	/* disable camera */
	config = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);
	config &= ~(1 << LS1X_CAM_START);
	config &= ~(1 << 30);
	__raw_writel(config, pcdev->base + CAM_CAMIF_CONFIG);

	local_irq_save(flags);
	disable_irq(pcdev->irq);
	local_irq_restore(flags);

#ifdef POLL_CAMERA
	if (tsk != NULL) {
		kthread_stop(tsk);
		tsk = NULL;
	}
#endif

	dev_info(icd->dev.parent, "LS1X Camera driver detached from camera %d\n",
		 icd->devnum);

	ls1x_camera_deactivate(pcdev);

	pcdev->icd = NULL;
}

static int ls1x_camera_set_crop(struct soc_camera_device *icd,
			       struct v4l2_crop *a)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);

	return v4l2_subdev_call(sd, video, s_crop, a);
}

static int ls1x_camera_set_bus_param(struct soc_camera_device *icd, __u32 pixfmt)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct ls1x_camera_dev *pcdev = ici->priv;
	unsigned long camera_flags, common_flags;
	unsigned int config;
	int ret;

	camera_flags = icd->ops->query_bus_param(icd);

	/* LS1X supports only 8bit buswidth */
	common_flags = soc_camera_bus_param_compatible(camera_flags,
						       CSI_BUS_FLAGS);
	if (!common_flags)
		return -EINVAL;

	/* Make choises, based on platform choice */
	if ((common_flags & SOCAM_VSYNC_ACTIVE_HIGH) &&
		(common_flags & SOCAM_VSYNC_ACTIVE_LOW)) {
			if (!pcdev->pdata ||
			     pcdev->pdata->flags & LS1X_CAMERA_VSYNC_HIGH)
				common_flags &= ~SOCAM_VSYNC_ACTIVE_LOW;
			else
				common_flags &= ~SOCAM_VSYNC_ACTIVE_HIGH;
	}

	if ((common_flags & SOCAM_PCLK_SAMPLE_RISING) &&
		(common_flags & SOCAM_PCLK_SAMPLE_FALLING)) {
			if (!pcdev->pdata ||
			     pcdev->pdata->flags & LS1X_CAMERA_PCLK_RISING)
				common_flags &= ~SOCAM_PCLK_SAMPLE_FALLING;
			else
				common_flags &= ~SOCAM_PCLK_SAMPLE_RISING;
	}

	if ((common_flags & SOCAM_DATA_ACTIVE_HIGH) &&
		(common_flags & SOCAM_DATA_ACTIVE_LOW)) {
			if (!pcdev->pdata ||
			     pcdev->pdata->flags & LS1X_CAMERA_DATA_HIGH)
				common_flags &= ~SOCAM_DATA_ACTIVE_LOW;
			else
				common_flags &= ~SOCAM_DATA_ACTIVE_HIGH;
	}

	if ((common_flags & SOCAM_HSYNC_ACTIVE_HIGH) &&
		(common_flags & SOCAM_HSYNC_ACTIVE_LOW)) {
			if (!pcdev->pdata ||
			     pcdev->pdata->flags & LS1X_CAMERA_HSYNC_HIGH)
				common_flags &= ~SOCAM_HSYNC_ACTIVE_LOW;
			else
				common_flags &= ~SOCAM_HSYNC_ACTIVE_HIGH;
	}

	ret = icd->ops->set_bus_param(icd, common_flags);
	if (ret < 0)
		return ret;

	config = __raw_readl(pcdev->base + CAM_CAMIF_CONFIG);

	if (common_flags & SOCAM_VSYNC_ACTIVE_LOW)
		config |= CONFIG_PARA_VS;
	if (common_flags & SOCAM_HSYNC_ACTIVE_LOW)
		config |= CONFIG_PARA_HS;

	config &= ~(0x3 << 7);
	if ((icd->user_width == 320) && (icd->user_height == 240)) {
		config |= 0x0 << 7;
	}
	if ((icd->user_width == 640) && (icd->user_height == 480)) {
		config |= 0x1 << 7;
	}
	else {
		config |= 0x2 << 7;
		__raw_writel((icd->user_height << 12) | (icd->user_width), pcdev->base + CAM_PIX_CONFIG);
	}

	config &= ~(0x3 << 13);
	config &= ~(0x3 << 11);
	config &= ~(0x3 << 4);
	config &= ~(0x1 << 2);
	switch (pixfmt) {
	case V4L2_PIX_FMT_YUYV:
		config = config | (0x0 << 4) | (0x1 << 11);
		break;
	case V4L2_PIX_FMT_YVYU:
		config = config | (0x1 << 4) | (0x1 << 11);
		break;
	case V4L2_PIX_FMT_VYUY:
		config = config | (0x2 << 4) | (0x1 << 11);
		break;
	case V4L2_PIX_FMT_UYVY:
		config = config | (0x3 << 4) | (0x1 << 11);
		break;
	case V4L2_PIX_FMT_RGB565:
		config = config | (0x1 << 13) | (0x0 << 11) | (0x0 << 2);
		break;
	case V4L2_PIX_FMT_RGB24:
		config = config | (0x2 << 13) | (0x0 << 11) | (0x1 << 2);
		break;
	default :
		dev_warn(icd->dev.parent, "Format %x not found (used yuyv)\n", pixfmt);
		config = config | (0x0 << 4) | (0x1 << 11);
		break;
	}

//	config |= 0xf0040;
	__raw_writel(config, pcdev->base + CAM_CAMIF_CONFIG);

	return 0;
}

static int ls1x_camera_set_fmt(struct soc_camera_device *icd,
			      struct v4l2_format *f)
{
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
	pix->height		= mf.height;
	pix->field		= mf.field;
	pix->colorspace		= mf.colorspace;
	icd->current_fmt	= xlate;

	return ret;
}

static int ls1x_camera_try_fmt(struct soc_camera_device *icd,
			      struct v4l2_format *f)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	const struct soc_camera_format_xlate *xlate;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_mbus_framefmt mf;
	int ret;
	/* TODO: limit to ls1x hardware capabilities */

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

static int ls1x_camera_reqbufs(struct soc_camera_device *icd,
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
		struct ls1x_camera_buffer *buf = container_of(icd->vb_vidq.bufs[i],
						      struct ls1x_camera_buffer, vb);
		buf->inwork = 0;
		INIT_LIST_HEAD(&buf->vb.queue);
	}

	return 0;
}

static unsigned int ls1x_camera_poll(struct file *file, poll_table *pt)
{
	struct soc_camera_device *icd = file->private_data;
	struct ls1x_camera_buffer *buf;

	buf = list_entry(icd->vb_vidq.stream.next, struct ls1x_camera_buffer,
			 vb.stream);

	poll_wait(file, &buf->vb.done, pt);

	if (buf->vb.state == VIDEOBUF_DONE ||
	    buf->vb.state == VIDEOBUF_ERROR)
		return POLLIN | POLLRDNORM;

	return 0;
}

static int ls1x_camera_querycap(struct soc_camera_host *ici,
			       struct v4l2_capability *cap)
{
	/* cap->name is set by the friendly caller:-> */
	strlcpy(cap->card, "LS1X Camera", sizeof(cap->card));
	cap->version = VERSION_CODE;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;

	return 0;
}

static struct soc_camera_host_ops ls1x_soc_camera_host_ops = {
	.owner		= THIS_MODULE,
	.add		= ls1x_camera_add_device,
	.remove		= ls1x_camera_remove_device,
	.set_bus_param	= ls1x_camera_set_bus_param,
	.set_crop	= ls1x_camera_set_crop,
	.set_fmt	= ls1x_camera_set_fmt,
	.try_fmt	= ls1x_camera_try_fmt,
	.init_videobuf	= ls1x_camera_init_videobuf,
	.reqbufs	= ls1x_camera_reqbufs,
	.poll		= ls1x_camera_poll,
	.querycap	= ls1x_camera_querycap,
};

static int __init ls1x_camera_probe(struct platform_device *pdev)
{
	struct ls1x_camera_dev *pcdev;
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

	clk = clk_get(&pdev->dev, "cam");
	if (IS_ERR(clk)) {
		err = PTR_ERR(clk);
		goto exit;
	}

	pcdev = kzalloc(sizeof(*pcdev), GFP_KERNEL);
	if (!pcdev) {
		dev_err(&pdev->dev, "Could not allocate pcdev\n");
		err = -ENOMEM;
		goto exit_put_clk;
	}

	pcdev->res = res;
	pcdev->clk = clk;

	pcdev->pdata = pdev->dev.platform_data;

	if (pcdev->pdata)
		pcdev->mclk = pcdev->pdata->mclk_10khz * 10000;

	if (!pcdev->mclk) {
		dev_warn(&pdev->dev,
			 "mclk_10khz == 0! Please, fix your platform data. "
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

	clk_set_rate(pcdev->clk, 24000000);

#ifdef	POLL_CAMERA
	thread_data = pcdev;
#else
	/* request irq */
	err = request_irq(pcdev->irq, ls1x_camera_irq, IRQF_DISABLED |
			  IRQF_TRIGGER_RISING, DRIVER_NAME,
			  pcdev);
	if (err) {
		dev_err(&pdev->dev, "Camera interrupt register failed \n");
		goto exit_free_dma;
	}
#endif

	pcdev->soc_host.drv_name	= DRIVER_NAME;
	pcdev->soc_host.ops		= &ls1x_soc_camera_host_ops;
	pcdev->soc_host.priv		= pcdev;
	pcdev->soc_host.v4l2_dev.dev	= &pdev->dev;
	pcdev->soc_host.nr		= pdev->id;
	err = soc_camera_host_register(&pcdev->soc_host);
	if (err)
		goto exit_free_irq;

	dev_info(&pdev->dev, "LS1X Camera driver loaded !\n");

	return 0;

exit_free_irq:
	free_irq(pcdev->irq, pcdev);
exit_free_dma:

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

static int __exit ls1x_camera_remove(struct platform_device *pdev)
{
	struct soc_camera_host *soc_host = to_soc_camera_host(&pdev->dev);
	struct ls1x_camera_dev *pcdev = container_of(soc_host,
					struct ls1x_camera_dev, soc_host);
	struct resource *res;

	flag = 1;

	free_irq(pcdev->irq, pcdev);
	clk_put(pcdev->clk);

	soc_camera_host_unregister(soc_host);

	iounmap(pcdev->base);

	res = pcdev->res;
	release_mem_region(res->start, resource_size(res));

	kfree(pcdev);

	dev_info(&pdev->dev, "LS1X Camera driver unloaded\n");

	return 0;
}

static struct platform_driver ls1x_camera_driver = {
	.driver 	= {
		.name	= DRIVER_NAME,
	},
	.remove		= __exit_p(ls1x_camera_remove),
};

static int __init ls1x_camera_init(void)
{
	flag = 1;
	return platform_driver_probe(&ls1x_camera_driver, ls1x_camera_probe);
}

static void __exit ls1x_camera_exit(void)
{
	flag = 0;
	return platform_driver_unregister(&ls1x_camera_driver);
}

module_init(ls1x_camera_init);
module_exit(ls1x_camera_exit);

MODULE_DESCRIPTION("Loongson1 SoC Camera Host driver");
MODULE_AUTHOR("linxiyuan <linxiyuan-gz@foxmail.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRIVER_NAME);
