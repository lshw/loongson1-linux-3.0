/* compilation option */
#define GSPCA_DEBUG 1

#include "gspca.orig.h"
#include <linux/version.h>

#undef GSPCA_VERSION
#define GSPCA_VERSION "2.15.14"

#ifndef V4L2_PIX_FMT_CPIA1
#define V4L2_PIX_FMT_SRGGB8  v4l2_fourcc('R', 'G', 'G', 'B')
#define V4L2_PIX_FMT_CPIA1 v4l2_fourcc('C', 'P', 'I', 'A')
#define V4L2_PIX_FMT_SN9C2028 v4l2_fourcc('S', 'O', 'N', 'X')
#define V4L2_PIX_FMT_STV0680 v4l2_fourcc('S', '6', '8', '0')
static inline const char *vdnn(struct video_device *vdev)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
	return vdev->dev.class_id;
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
	return vdev->dev.bus_id;
#else
	return dev_name(&vdev->dev);
#endif
#endif
}
#define video_device_node_name(vdev) vdnn(vdev)
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
#define usb_alloc_coherent(d, b, g, a) usb_buffer_alloc(d, b, g, a)
#define usb_free_coherent(d, l, b, a) usb_buffer_free(d, l, b, a)
#endif
#ifndef V4L2_PIX_FMT_CIT_YYVYUY
#define V4L2_PIX_FMT_CIT_YYVYUY v4l2_fourcc('C', 'I', 'T', 'V') /* one line of Y then 1 line of VYUY */
#define V4L2_PIX_FMT_KONICA420  v4l2_fourcc('K', 'O', 'N', 'I') /* YUV420 planar in blocks of 256 pixels */
#endif
#ifndef V4L2_PIX_FMT_JPGL
#define V4L2_PIX_FMT_JPGL	v4l2_fourcc('J', 'P', 'G', 'L') /* JPEG-Lite */
#endif
#ifndef V4L2_PIX_FMT_Y10BPACK
#define V4L2_PIX_FMT_Y10BPACK    v4l2_fourcc('Y', '1', '0', 'B') /* 10  Greyscale bit-packed */
#endif
#ifndef V4L2_PIX_FMT_SE401
#define V4L2_PIX_FMT_SE401      v4l2_fourcc('S', '4', '0', '1') /* se401 janggu compressed rgb */
#endif
#ifndef V4L2_PIX_FMT_JL2005BCD
#define V4L2_PIX_FMT_JL2005BCD v4l2_fourcc('J', 'L', '2', '0') /* compressed RGGB bayer */
#endif
#ifndef V4L2_CID_ILLUMINATORS_1
#define V4L2_CID_ILLUMINATORS_1			(V4L2_CID_BASE+37)
#define V4L2_CID_ILLUMINATORS_2			(V4L2_CID_BASE+38)
#endif
#ifndef V4L2_CID_JPEG_COMPRESSION_QUALITY
#define V4L2_CID_JPEG_COMPRESSION_QUALITY	(V4L2_CID_BASE+42)
#endif
#ifndef pr_warn
#define pr_err(fmt, ...) \
	printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warning(fmt, ...) \
	printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn pr_warning
#define pr_info(fmt, ...) \
	printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#endif
#ifndef module_driver
#define module_driver(__driver, __register, __unregister) \
static int __init __driver##_init(void) \
{ \
	return __register(&(__driver)); \
} \
module_init(__driver##_init); \
static void __exit __driver##_exit(void) \
{ \
	__unregister(&(__driver)); \
} \
module_exit(__driver##_exit);
#endif
#ifndef module_usb_driver
#define module_usb_driver(__usb_driver) \
	module_driver(__usb_driver, usb_register, \
		       usb_deregister)
#endif
