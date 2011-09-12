
//#ifndef __STUB_H__
#define __STUB_H__

#define CMOS_WIDTH	480
#define CMOS_HEIGHT 640

#define ZKFPV10		0
#define ZKFP_PARAM_CODE_DBMM 0
#define ZKFP_DBMM_EXTERNAL	0
#define IDENTIFYSPEED	0
#define ONLY_LOCAL		0

#define IMG_WIDTH	640
#define IMG_HEIGHT	480
#define IMG_BPP		16

#define IRQ_GPIO_0	15

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef u8
#define u8	unsigned char
#endif

#ifndef u16
#define u16	unsigned short
#endif

#ifndef u32
#define u32	unsigned int
#endif

#ifndef U32
#define U32	unsigned int
#endif

#ifndef BYTE
#define BYTE   unsigned char
#endif

#ifndef WORD
#define WORD unsigned short
#endif

#ifndef NULL
#define NULL	0
#endif

#ifndef BOOL
#define BOOL int
#endif

struct sensor_interface{
	unsigned char *buf;
	int img_width;
	int img_height;
	unsigned int frame_size;
}sensor_if;

#define here()	printk("-->%s, %dline.\n", __FILE__, __LINE__)
//int i2c_read(int dev_addr, unsigned char *data, unsigned char addr, int size);
//int i2c_write(int dev_addr, unsigned char *data, unsigned char addr, int size);
//

/**
	#define gOHeight gOptions.OImageHeight
	#define gOWidth gOptions.OImageWidth
	#define gOSize gOptions.OImageWidth*gOptions.OImageHeight
	gOptions.ZKFPVersion
	gOptions.NewFPReader
	gOptions.MThreshold
	gOptions.MaxNoiseThr
	gOptions.MinMinutiae
	gOptions.MaxTempLen
 */
 
 //#endif

