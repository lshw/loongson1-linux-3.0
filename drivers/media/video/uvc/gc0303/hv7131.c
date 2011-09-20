/*
 * HV7131 CMOS camera sensor initialization
 */

/*
#include "camera.h"
#include "jz4740.h"
#include "clock.h"
#include "i2c.h"
*/

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>

#include "../uvcvideo.h"

#include "stub.h"

#include "sb2f_i2c.h"

/* error code */
#define ETIMEDOUT       1
#define ENODEV          2

/*
 * hv7131 registers
 */
#define DEVID    0x00 /* Dev ID */
#define SCTRA    0x01 /* Sensor Control A */
#define SCTRB    0x02 /* Sensor Control B */
#define OUTIV    0x03 /* Output Inversion */

#define RSAU     0x10 /* Row Start Address Upper */
#define RSAL     0x11 /* Row Start Address Lower */
#define CSAU     0x12 /* Col Start Address Upper */
#define CSAL     0x13 /* Col Start Address Lower */
#define WIHU     0x14 /* Window Height Upper */
#define WIHL     0x15 /* Window Height Lower */
#define WIWU     0x16 /* Window Width Upper */
#define WIWL     0x17 /* Window Width Lower */

#define HBLU     0x20 /* HBLANK Time Upper */
#define HBLL     0x21 /* HBLANK Time Lower */
#define VBLU     0x22 /* VBLANK Time Upper */
#define VBLL     0x23 /* VBLANK Time Lower */
#define INTH     0x25 /* Integration Time High */
#define INTM     0x26 /* Integration Time Middle */
#define INTL     0x27 /* Integration Time Low */

#define PAG      0x30 /* Pre-amp Gain */
#define RCG      0x31 /* Red Color Gain */
#define GCG      0x32 /* Green Color Gain */
#define BCG      0x33 /* Blue Color Gain */
#define ACTRA    0x34 /* Analog Bias Control A */
#define ACTRB    0x35 /* Analog Bias Control B */

#define BLCTH    0x40 /* Black Level Threshod */
#define ORedI    0x41 /* Initial ADC Offset Red */
#define OGrnI    0x42 /* Initial ADC Offset Green */
#define OBluI    0x43 /* Initial ADC Offset Blue */


/*
 * Define the starting (x, y)
 */
/* cam2 */
#define XSTART 0x02
#define YSTART 0x02

#define VAL_INT_TIME_GC303  0x0050
#define VAL_INT_TIME  35
#define VAL_PAG       0x30
#define VAL_DIV       16	//16		//360Mhz
/* cam2 */

/* integration time */
//static unsigned int integration_time = 35; /* unit: ms */
static unsigned int integration_time = VAL_INT_TIME; /* unit: ms */
static unsigned int integration_time_GC303 = VAL_INT_TIME_GC303; /* unit: ms */
static int m_sensor_opt = 0;

/* master clock and video clock */
static unsigned int mclk_hz = 25000000;    /* 25 MHz */
static unsigned int vclk_div = VAL_DIV;          /* VCLK = MCLK/vclk_div: 2,4,8,16,32 */
static int i2c_ok = 0;
int DEV_ADDR = 0x22>>1;

int GC0303_NoCoat=FALSE;

void i2c_set_addr(unsigned int addr)
{
	DEV_ADDR=addr>>1;
	//DEV_ADDR=addr;
}

int sensor_write_reg(unsigned char reg, unsigned char val)
{
	if(-ENODEV == i2c_write(DEV_ADDR, &val, reg, 1)) 
	{
    	//	OSTaskDel(OS_PRIO_SELF);
		i2c_ok = -1;
		return -ENODEV;
  	}
	return 0;
}

unsigned char sensor_read_reg(unsigned char reg)
{
 	unsigned char val;
  	if(-ENODEV==i2c_read(DEV_ADDR, &val, reg, 1))
	{
		i2c_ok=-1;
		return -ENODEV;
	}
  	return val;
}

//303/305 support
#define GC_BIAS		0x9C
#define GC_TEST		0x9D
#define GC_MCRT		0x9E
#define GC_FRAME	0x9F
#define GC_EXP_H        0x83
#define GC_EXP_L        0x84

#define GC_ROW_H        0x92
#define GC_ROW_L        0x93
#define GC_COL_H        0x94
#define GC_COL_L        0x95
#define GC_WINH_H       0x96
#define GC_WINH_L       0x97
#define GC_WINW_H       0x98
#define GC_WINW_L       0x99
#define GC_VSYNC	0xA0

static void set_window_GC303(int l, int t, int w, int h)
{
	int ltemp=0;
	if(GC0303_NoCoat)
	{
		ltemp = 640 - w - l;
                l=(ltemp>=0?ltemp:(640-w))/2*2; //nocoatting sensor have a manual correcting param,that is 36
                t=(480 - h - t)/2*2;
                if(t<0)
                        t=0;
	}
	else
	{
		l=(640 - w - l)/2*2;
        	t=(480 - h - t)/2*2;
	}
        /* Set the row start address */
        sensor_write_reg(GC_ROW_H, (t >> 8) & 0xff);
        sensor_write_reg(GC_ROW_L, t & 0xff);

        /* Set the column start address */
        sensor_write_reg(GC_COL_H, (l >> 8) & 0xff);
        sensor_write_reg(GC_COL_L, l & 0xff);

        /* Set the image window width*/
        sensor_write_reg(GC_WINW_H, (w >> 8) & 0xff);
        sensor_write_reg(GC_WINW_L, w & 0xff);

        /* Set the image window height*/
        sensor_write_reg(GC_WINH_H, (h >> 8) & 0xff);
        sensor_write_reg(GC_WINH_L, h & 0xff);
}

void set_BLANK_GC303(int d3,int d4)
{
        sensor_write_reg(0x85, d3);
        sensor_write_reg(0x9a, d4);
//        val = sensor_read_reg(0x9a);
//        printk("0x9a:%x\n",val);
}

void set_BRGB_GC303(int d1,int d2,int d3,int d4)
{
        sensor_write_reg(0x86, d1);
        sensor_write_reg(0x87, d2);
        sensor_write_reg(0x88, d3);
        sensor_write_reg(0x89, d4);
}

void set_PRGB_GC303(int d1,int d2,int d3,int d4,int d5)
{
//        unsigned char val;

        sensor_write_reg(0x8a, d1/256);
        sensor_write_reg(0x8b, d1%256);
        sensor_write_reg(0x8c, d2/256);
        sensor_write_reg(0x8d, d2%256);
        sensor_write_reg(0x8e, d3/256);
        sensor_write_reg(0x8f, d3%256);
        sensor_write_reg(0x90, d4/256);
        sensor_write_reg(0x91, d4%256);

//        val = sensor_read_reg(0x9c);
//        printk("0x9c:%x\n",val);
}

void set_integration_time_GC303(void)
{
	if(GC0303_NoCoat)
	{
        	sensor_write_reg(GC_EXP_H, 0x00);
        	sensor_write_reg(GC_EXP_L, 0xe0);
	}
	else
	{
        	sensor_write_reg(GC_EXP_H, (integration_time_GC303>>8)&0xff);
        	sensor_write_reg(GC_EXP_L, integration_time_GC303&0xff);
	}
}

void sensor_power_down_GC303(void)
{
        sensor_write_reg(0x9b, 0x24);
}

void sensor_power_on_GC303(void)
{
        sensor_write_reg(0x9b, 0x20);
}

static void set_gc303_clock(int div)
{
#define ABLC_EN (1 << 3)
        /* ABLC enable */
        switch (div) {
        case 2:
                sensor_write_reg(GC_MCRT, ABLC_EN | 0x0);       // DCF=MCLK
                break;
        case 4:
                sensor_write_reg(GC_MCRT, ABLC_EN | 0x1);       // DCF=MCLK/2
                break;
        case 8:
                sensor_write_reg(GC_MCRT, ABLC_EN | 0x2);       // DCF=MCLK/4
                break;
        case 16:
                sensor_write_reg(GC_MCRT, ABLC_EN | 0x3);       // DCF=MCLK/8
                break;
        default:
                break;
        }
}

static int sensor_init_GC303(int left, int top, int width, int height)
{
        unsigned char timeout=5;
	unsigned char val;

try_again:
        i2c_ok = 0;

        sensor_write_reg(GC_BIAS, 0x17);
        sensor_write_reg(GC_TEST, 0x80);  
        sensor_write_reg(GC_FRAME, 0x89/*0x49*/); /* upside down, output pclk all the time, pclk inversion */
        sensor_write_reg(GC_VSYNC, 0x00);

        /* set clock */
        set_gc303_clock(vclk_div);

        set_BLANK_GC303(0x00,2);

        set_BRGB_GC303(0,0,0,0);
	if(GC0303_NoCoat)
        	set_PRGB_GC303(0x0230,0x0350,0x0350,0x0230,0x25);
	else
        	set_PRGB_GC303(0x180,0x180,0x180,0x130,0x25);
	
        set_window_GC303(left, top, width, height);
	set_integration_time_GC303();

        sensor_power_on_GC303();
	val = sensor_read_reg(GC_MCRT);
	//printk("read gc clock reg:%x\n",val);
	if( (val&0x3) != 0x3)
	{
		uvc_printk(KERN_ERR,"gc303 set clock failed ......\n");
		i2c_ok = -1;
	}

	val = sensor_read_reg(GC_FRAME);
        printk("read gc frame reg:%x\n",val);

        if( val!= 0x89)
        {
                printk("gc303 set frame failed ......\n");
                i2c_ok = -1;
        }

        if(i2c_ok==0)
        {
                printk("Initialized sensor GC303 register OK\n");
                return 0;
        }
        else
        {
                if(timeout--)
                        goto try_again;
                else
                {
                        printk("Initialize sensor GC303 register ERROR!\n");
                        return -1;
                }
        }

}

//303/305 end

void sensor_power_down(void)
{
        unsigned char val;

	i2c_set_addr(0x22);
        val = sensor_read_reg(SCTRB);
        val |= 0x08;
        sensor_write_reg(SCTRB, val);
}

void sensor_power_on(void)
{
        unsigned char val;

	i2c_set_addr(0x22);
        val = sensor_read_reg(SCTRB);
        val &= ~0x08;
        sensor_write_reg(SCTRB, val);
}

/* left, top, width, height */
static void set_window(int l, int t, int w, int h)
{
	l = (l/2)*2;
	t = (t/2)*2;

	/* Set the column start address */
	sensor_write_reg(CSAU, (l >> 8) & 0xff);
	sensor_write_reg(CSAL, l & 0xff);


	/* Set the row start address */
	sensor_write_reg(RSAU, (t >> 8) & 0xff);
	sensor_write_reg(RSAL, t & 0xff);

	/* Set the image window width*/
	sensor_write_reg(WIWU, (w >> 8) & 0xff);
	sensor_write_reg(WIWL, w & 0xff);

	/* Set the image window height*/
	sensor_write_reg(WIHU, (h >> 8) & 0xff);
	sensor_write_reg(WIHL, h & 0xff);
}

static void set_blanking_time(unsigned short hb_time, unsigned short vb_time)
{
	hb_time = (hb_time < 0xd0)? 0xd0 : hb_time;
	vb_time = (vb_time < 0x08)? 0x08 : vb_time;

	sensor_write_reg(HBLU, (hb_time >> 8) & 0xff);
	sensor_write_reg(HBLL, hb_time & 0xff);
	sensor_write_reg(VBLU, (vb_time >> 8) & 0xff);
	sensor_write_reg(VBLL, vb_time & 0xff);
}

static void set_integration_time(void)
{
        unsigned int regval;

	if (vclk_div == 0) vclk_div = 2;

	regval = (integration_time * mclk_hz)/ (1000*vclk_div); /* default: 0x065b9a */

	sensor_write_reg(INTH, (regval & 0xff0000) >> 16);
	sensor_write_reg(INTM, (regval & 0xff00) >> 8);
	sensor_write_reg(INTL, regval & 0xff);
}

/* VCLK = MCLK/div */
static void set_hv7131_clock(int div)
{
#define ABLC_EN (1 << 3)
	/* ABLC enable */
	switch (div) {
	case 2:
		sensor_write_reg(SCTRA, ABLC_EN | 0x01);       // DCF=MCLK
		break;
	case 4:
		sensor_write_reg(SCTRA, ABLC_EN | 0x11);       // DCF=MCLK/2
		break;
	case 8:
		sensor_write_reg(SCTRA, ABLC_EN | 0x21);       // DCF=MCLK/4
		break;
	case 16:
		sensor_write_reg(SCTRA, ABLC_EN | 0x31);       // DCF=MCLK/8
		break;
	case 32:
		sensor_write_reg(SCTRA, ABLC_EN | 0x41);       // DCF=MCLK/16
		break;
	case 64:
		sensor_write_reg(SCTRA, ABLC_EN | 0x51);       // DCF=MCLK/32
		break;
	default:
		break;
	}
}

static int CMOSGain=0x30;

int sensor_init_HV7131(int left, int top, int width, int height, int FPReaderOpt)
{
	unsigned char timeout=5;

try_again:
	i2c_ok = 0;
/*	sensor_power_down();
	mdelay(200); */
	set_hv7131_clock(vclk_div);

//	sensor_write_reg(SCTRB, 0x15);       // VsHsEn, HSYNC mode
	sensor_write_reg(SCTRB, 0x11);      // VsHsEn, HSYNC mode
        sensor_write_reg(OUTIV, 0x00); /*modified, Wolfwang */

	set_window(left, top, width, height);
	set_integration_time();
	set_blanking_time(0xff, 0x08); // ZK value

	if(FPReaderOpt&0x01)
        {
                CMOSGain=0x90;
                sensor_write_reg(PAG, CMOSGain);
        }
        else
        {
                CMOSGain=0x30;
                sensor_write_reg(PAG, CMOSGain);
        }

        sensor_write_reg(RCG, 0x08);
        sensor_write_reg(GCG, 0x08);
        sensor_write_reg(BCG, 0x08);

        sensor_write_reg(ACTRA, 0x17);
        sensor_write_reg(ACTRB, 0x7f);

	sensor_write_reg(BLCTH, 0xff);       // set black level threshold

        sensor_write_reg(ORedI, 0x7f);
        sensor_write_reg(OGrnI, 0x7f);
        sensor_write_reg(OBluI, 0x7f);

	sensor_power_on();
	if(i2c_ok==0)
	{
		printk("Initialized sensor hv7131 register OK\n");
		return 0;
	}
	else
	{
		if(timeout--)
			goto try_again;
		else
		{	
			printk("Initialize sensor hv7131 register ERROR!\n");
			return -1;
		}
	}
}


int sensor_init(int left, int top, int width, int height, int FPReaderOpt)
{
        int ret = 0;

        i2c_init();

        m_sensor_opt = FPReaderOpt;
        i2c_set_addr(0x30);
        ret = sensor_read_reg(0x80);
	printk("Device ID from CMOS GC0303: %2x\n",ret);
        if(ret == 0x11)
        {
        	m_sensor_opt |= 0x20;
                printk("Found GC303\n");
                return sensor_init_GC303(left, top ,width, height);
        }
        
        /*
	else if(ret==0x29)
	{
		printk("Found GC305\n");
		// support GC305 in the furture
		return -1;
	}

        i2c_set_addr(0x42);
        select_page_gc307(0);
        ret = sensor_read_reg(0x00);
        printk("Device ID from CMOS GC0307: %2x\n",ret);
        if(ret == 0x99)
        {
                printk("Found GC307\n");
                return sensor_init_GC307(left, top ,width, height);
        }

        i2c_set_addr(0x22);
        ret = sensor_read_reg(0x0);
	printk("Device ID from CMOS HV7131: %2x\n",ret);
	if(ret==0x02)
        {
                printk("Found HV7131\n");
                return sensor_init_HV7131(left,top,width,height,FPReaderOpt);
        }*/
	else
	{
		i2c_set_addr(0x42);
		sensor_write_reg(0xfe, 0x00);	//lxy
		ret = sensor_read_reg(0x00);
		printk("Device ID: 0x00 = %2x\n",ret);
		ret = sensor_read_reg(0x80);
		printk("Device ID: 0x80 = %2x\n",ret);
		if(ret == 0x9b)
		{
		        printk("Found GC307\n");
		        return ;
		}
		
		
		printk("No CMOS!!!\n");
		return -1;
	}
}

void sensor_close(void)
{
	if(m_sensor_opt & 0x20)
        	sensor_power_down_GC303();
        else
                sensor_power_down();
}

int SetCMOSGain(int Gain)
{
	if(m_sensor_opt & 0x20)
		i2c_set_addr(0x30);
	else
		i2c_set_addr(0x22);
        return sensor_write_reg(PAG, Gain);//Pre-amp Gain
}

int IncCMOSGain(void)
{
        if(CMOSGain>4)
                CMOSGain=(CMOSGain*5+2)/4;
        else
                CMOSGain=10;
        if(CMOSGain>255) CMOSGain=255;
        return SetCMOSGain(CMOSGain);
}

int DecCMOSGain(void)
{
        CMOSGain=CMOSGain*2/3;
        if(CMOSGain==0) CMOSGain=1;
        return SetCMOSGain(CMOSGain);
}

void FilterRGB(char *PixelsBuffer, int Width, int Height)
{
        int i,j;
        unsigned char p,*p1,*p2,*p3,*p4;
        p1 = (unsigned char *)PixelsBuffer; p2 = p1+1;
        p3 = p1+Width; p4 = p3+1;
        for(i = 0; i<Height-1; i++)
        {
                for(j = 0; j<Width-1; j++)
                {
                        p=(*p1+*p2+++*p3+++*p4+++2) / 4;
                        *p1++=p;
                }
                p1++; p2++; p3++; p4++;
        }
}

int Read24WC02(int Address, unsigned char *data, int size)
{
	i2c_set_addr(0xA0);
	if(-ENODEV==i2c_read(DEV_ADDR, data, Address, size))
		return 0;
	else
		return 1;
}

int Write24WC02(int Address, unsigned  char *data, int size)
{
	i2c_set_addr(0xA0);
	if(-ENODEV == i2c_write(DEV_ADDR, data, Address, size))
		return 0;
	else
		return 1;
}
