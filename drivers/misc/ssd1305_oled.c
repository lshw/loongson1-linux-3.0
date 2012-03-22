/*
 * SSD1305 OLED driver
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/capability.h>

#include <asm/uaccess.h>
#include <asm/sections.h>
#include <asm/io.h>

#include <ls1b_board.h>

#include "ssd1305_oled.h"

static DEFINE_MUTEX(SSD1305LCD_mutex);

static void SSD1305_rest(void);
static void Write_Dot_LCD(int x, int y, int i);

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Global Variables
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#define XLevelL		0x00
#define XLevelH		0x10
#define XLevel		((XLevelH&0x0F)*16+XLevelL)
#define Max_Column	128
#define Max_Row		64
//#define Brightness	0x6F
#define Brightness	0xFF

unsigned char X_Witch;					//字符写入时的宽度
unsigned char Y_Witch;					//字符写入时的高度
int Font_Wrod;							//字体的每个字模占用多少个存储单元数
unsigned char *Char_TAB;							//字库指针

int Font_type=0;						//标识字符类型
int Font_Cover=0;						//字符显示的模式
int Char_BackColor=0;					//设置覆盖模式时,字符的背景色

//unsigned char Plot_Mode;						//绘图模式
int BMP_Color;
int Char_Color;

#undef DEBUG
//#define	DEBUG
#undef SSD1305Z_TEST

//#define	M68			// 8-bit 68XX Parallel
						//   BS1=0; BS2=1
#define	I80			// 8-bit 80XX Parallel
						//   BS1=1; BS2=1
//#define	SPI			// 4-wire SPI
						//   BS1=0; BS2=0
						//   The unused pins should be connected with VSS mostly or floating (D2).
						//   Please refer to the SSD1305 specification for detail.

#define	RES		(1 << 17)
#define	CS		(1 << 16)
#define	DC		(1 << 18)

//M68
#define	E		(1 << 20)
#define	RW		(1 << 19)

//I80
#define	RD		(1 << 20)
#define	WR		(1 << 19)

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Read/Write Sequence
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#ifdef M68					// 8-bit 68XX Parallel
static void Write_Command(unsigned char Data)
{
	
}


static void Write_Data(unsigned char Data)
{
	
}
#endif


#ifdef I80					// 8-bit 80XX Parallel
static void inline Write_Command(unsigned char Data)
{
	unsigned int ret;
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)); //
	ret &= 0xFFE000FF;	//ret &= ~(0x1FFF << 8); 清零
	
	//1 0010 xxxx xxxx
//	ret &= ~CS;
//	ret |= RES;
//	ret &= ~DC;
//	ret &= ~WR;
//	ret |= RD;
//	ret &= ~(0xFF << 8);
	ret |= ((0x00120000) | (Data << 8));
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)) = ret;
//	udelay(5);
	
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)); //
	
	//1 1111
//	ret |= CS;
//	ret |= RES;
//	ret |= DC;
//	ret |= WR;
//	ret |= RD;
	ret |= 0x001F0000;
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)) = ret;
//	udelay(800);
//	printk(KERN_EMERG "RET = %x \n", ret);
}


static void inline Write_Data(unsigned char Data)
{
	unsigned int ret;
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)); //
	ret &= 0xFFE000FF;	//ret &= ~(0x1FFF << 8); 清零
	
	//1 0110 xxxx xxxx
//	ret &= ~CS;
//	ret |= RES;
//	ret |= DC;
//	ret &= ~WR;
//	ret |= RD;
//	ret &= ~(0xFF << 8);
	ret |= ((0x00160000) | (Data << 8));
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)) = ret;
//	udelay(5);
	
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)); //
	//1 1111
//	ret |= CS;
//	ret |= RES;
//	ret |= DC;
//	ret |= WR;
//	ret |= RD;
	ret |= 0x001F0000;
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)) = ret;
//	udelay(800);
//	printk(KERN_EMERG "RET = %x \n", ret);
}

static unsigned char inline Read_Data(void)
{
	unsigned int ret;
	unsigned char data;
	
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OE0)); //输入 使能GPIO
	ret |= (0xFF << 8);
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OE0)) = ret;
	
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)); //
	ret &= 0xFFE0FFFF;	//ret &= ~(0x1FFF << 8); 清零
	//0 1110
//	ret &= ~CS;
//	ret |= RES;
//	ret |= DC;
//	ret |= WR;
//	ret &= ~RD;
	ret |= 0x000E0000;
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)) = ret;
	
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)); //
	ret &= 0xFFE0FFFF;	//ret &= ~(0x1FFF << 8); 清零
	// 1 1110
//	ret &= ~CS;
//	ret |= RES;
//	ret |= DC;
//	ret |= WR;
//	ret |= RD;
	ret |= 0x001E0000;
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)) = ret;
	
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)); //
	ret &= 0xFFE0FFFF;	//ret &= ~(0x1FFF << 8); 清零
	//0 1110
//	ret &= ~CS;
//	ret |= RES;
//	ret |= DC;
//	ret |= WR;
//	ret &= ~RD;
	ret |= 0x000E0000;
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)) = ret;
	
//	udelay(5);
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_IN0)); //读回的数值是反码？
//	printk(KERN_EMERG "RET = %x \n", ret);
	data = (unsigned char)(ret >> 8);
//	printk(KERN_EMERG "Dat = %x \n", data);
	
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)); //
	ret &= 0xFFE0FFFF;	//ret &= ~(0x1FFF << 8); 清零
	//1 1111
//	ret |= CS;
//	ret |= RES;
//	ret |= DC;
//	ret |= WR;
//	ret |= RD;
	ret |= 0x001F0000;
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)) = ret;
	
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OE0)); //输出 使能GPIO
	ret &= ~(0xFF << 8);
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OE0)) = ret;
	
	return data;
//	udelay(800);
//	printk(KERN_EMERG "RET = %x \n", ret);
}
#endif


#ifdef SPI					// 4-wire SPI
static void Write_Command(unsigned char Data)
{
	
}


static void Write_Data(unsigned char Data)
{
	
}
#endif

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Instruction Setting
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
static void inline Set_Start_Column(unsigned char d)
{
	Write_Command(0x10+d/16);	// Set Higher Column Start Address for Page Addressing Mode
									// Default => 0x10
	Write_Command(0x00+d%16);	// Set Lower Column Start Address for Page Addressing Mode
									// Default => 0x00
}


static void inline Set_Addressing_Mode(unsigned char d)
{
	Write_Command(0x20);		// Set Memory Addressing Mode
	Write_Command(d);			// Default => 0x02
								// 0x00 => Horizontal Addressing Mode
								// 0x01 => Vertical Addressing Mode
								// 0x02 => Page Addressing Mode
}


static void inline Set_Column_Address(unsigned char a, unsigned char b)
{
	Write_Command(0x21);		// Set Column Address
	Write_Command(a);			// Default => 0x00 (Column Start Address)
	Write_Command(b);			// Default => 0x83 (Column End Address)
}


static void inline Set_Page_Address(unsigned char a, unsigned char b)
{
	Write_Command(0x22);		// Set Page Address
	Write_Command(a);			// Default => 0x00 (Page Start Address)
	Write_Command(b);			// Default => 0x07 (Page End Address)
}


static void inline Set_Start_Line(unsigned char d)
{
	Write_Command(0x40|d);	// Set Display Start Line
								// Default => 0x40 (0x00)
}


static void inline Set_Contrast_Control(unsigned char d)
{
	Write_Command(0x81);		// Set Contrast Control for Bank 0
	Write_Command(d);			// Default => 0x80
}


static void inline Set_Area_Brightness(unsigned char d)
{
	Write_Command(0x82);		// Set Brightness for Area Color Banks
	Write_Command(d);			// Default => 0x80
}


static void inline Set_Segment_Remap(unsigned char d)
{
	Write_Command(0xA0|d);	// Set Segment Re-Map
								// Default => 0xA0
								// 0xA0 (0x00) => Column Address 0 Mapped to SEG0
								// 0xA1 (0x01) => Column Address 0 Mapped to SEG131
}


static void inline Set_Entire_Display(unsigned char d)
{
	Write_Command(0xA4|d);	// Set Entire Display On / Off
								// Default => 0xA4
								// 0xA4 (0x00) => Normal Display
								// 0xA5 (0x01) => Entire Display On
}


static void inline Set_Inverse_Display(unsigned char d)
{
	Write_Command(0xA6|d);	// Set Inverse Display On/Off
								// Default => 0xA6
								// 0xA6 (0x00) => Normal Display
								// 0xA7 (0x01) => Inverse Display On
}


static void inline Set_Multiplex_Ratio(unsigned char d)
{
	Write_Command(0xA8);		// Set Multiplex Ratio
	Write_Command(d);			// Default => 0x3F (1/64 Duty)
}


static void inline Set_Dim_Mode(unsigned char a, unsigned char b)
{
	Write_Command(0xAB);		// Set Dim Mode Configuration
	Write_Command(0X00);		// => (Dummy Write for First Parameter)
	Write_Command(a);			// Default => 0x80 (Contrast Control for Bank 0)
	Write_Command(b);			// Default => 0x80 (Brightness for Area Color Banks)
	Write_Command(0xAC);		// Set Display On in Dim Mode
}


static void inline Set_Master_Config(unsigned char d)
{
	Write_Command(0xAD);		// Set Master Configuration
	Write_Command(0x8E|d);	// Default => 0x8E
								// 0x8E (0x00) => Select External VCC Supply
								// 0x8F (0x01) => Select Internal DC/DC Voltage Converter
}


static void inline Set_Display_On_Off(unsigned char d)	
{
	Write_Command(0xAE|d);	// Set Display On/Off
								// Default => 0xAE
								// 0xAE (0x00) => Display Off
								// 0xAF (0x01) => Display On
}


static void inline Set_Start_Page(unsigned char d)
{
	Write_Command(0xB0|d);	// Set Page Start Address for Page Addressing Mode
								// Default => 0xB0 (0x00)
}


static void inline Set_Common_Remap(unsigned char d)
{
	Write_Command(0xC0|d);	// Set COM Output Scan Direction
								// Default => 0xC0
								// 0xC0 (0x00) => Scan from COM0 to 63
								// 0xC8 (0x08) => Scan from COM63 to 0
}


static void inline Set_Display_Offset(unsigned char d)
{
	Write_Command(0xD3);		// Set Display Offset
	Write_Command(d);			// Default => 0x00
}


static void inline Set_Display_Clock(unsigned char d)
{
	Write_Command(0xD5);		// Set Display Clock Divide Ratio / Oscillator Frequency
	Write_Command(d);			// Default => 0x70
								// D[3:0] => Display Clock Divider
								// D[7:4] => Oscillator Frequency
}


static void inline Set_Area_Color(unsigned char d)
{
	Write_Command(0xD8);		// Set Area Color Mode On/Off & Low Power Display Mode
	Write_Command(d);			// Default => 0x00 (Monochrome Mode & Normal Power Display Mode)
}


static void inline Set_Precharge_Period(unsigned char d)
{
	Write_Command(0xD9);		// Set Pre-Charge Period
	Write_Command(d);			// Default => 0x22 (2 Display Clocks [Phase 2] / 2 Display Clocks [Phase 1])
								// D[3:0] => Phase 1 Period in 1~15 Display Clocks
								// D[7:4] => Phase 2 Period in 1~15 Display Clocks
}


static void inline Set_Common_Config(unsigned char d)
{
	Write_Command(0xDA);		// Set COM Pins Hardware Configuration
	Write_Command(0x02|d);	// Default => 0x12 (0x10)
								// Alternative COM Pin Configuration
								// Disable COM Left/Right Re-Map
}


static void inline Set_VCOMH(unsigned char d)
{
	Write_Command(0xDB);		// Set VCOMH Deselect Level
	Write_Command(d);			// Default => 0x34 (0.77*VCC)
}


static void inline Set_Read_Modify_Write(unsigned char d)
{
	Write_Command(0xE0|d);	// Set Read Modify Write Mode
								// Default => 0xE0
								// 0xE0 (0x00) => Enter Read Modify Write
								// 0xEE (0x0E) => Exit Read Modify Write
}


static void inline Set_NOP(void)
{
	Write_Command(0xE3);		// Command for No Operation
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Show Regular Pattern (Full Screen)
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
static void Fill_RAM(unsigned char Data)
{
unsigned char i,j;

	for(i=0;i<8;i++)
	{
		Set_Start_Page(i);
//		Set_Start_Column(0x0);	//这里注意不同的LCD的偏移
		Set_Start_Column(0x02);

		for(j=0;j<128;j++)
		{
			Write_Data(Data);
		}
	}
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Bank Color & Look Up Table Setting (Partial Screen)
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
static void inline Set_LUT(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
	Write_Command(0x91);		//   Define Look Up Table of Area Color
	Write_Command(a);			//   Define Bank 0 Pulse Width
	Write_Command(b);			//   Define Color A Pulse Width
	Write_Command(c);			//   Define Color B Pulse Width
	Write_Command(d);			//   Define Color C Pulse Width
}

static void inline Set_Bank_Color(void)
{
	Write_Command(0x92);			// Define Area Color for Bank 1~16 (Page 0)
	Write_Command(0x00);			//   Define Bank 1~4 as Color A
	Write_Command(0xEA);			//   Define Bank 5~7 as Color C
	Write_Command(0xAF);			//   Define Bank 8~10 as Color D
	Write_Command(0x56);			//   Define Bank 11~13 as Color C
						//   Define Bank 12~16 as Color B

	Write_Command(0x93);			// Define Area Color for Bank 17~32 (Page 1)
	Write_Command(0x00);			//   Define Bank 17~20 as Color A
	Write_Command(0xEA);			//   Define Bank 21~23 as Color C
	Write_Command(0xAF);			//   Define Bank 24~26 as Color D
	Write_Command(0x56);			//   Define Bank 27~29 as Color C
						//   Define Bank 30~32 as Color B
}

#if 0
//#ifdef SSD1305Z_TEST

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Show Regular Pattern (Partial or Full Screen)
//
//    a: Start Page
//    b: End Page
//    c: Start Column
//    d: Total Columns
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void Fill_Block(unsigned char Data, unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
unsigned char i,j;
	
	for(i=a;i<(b+1);i++)
	{
		Set_Start_Page(i);
		Set_Start_Column(c);

		for(j=0;j<d;j++)
		{
			Write_Data(Data);
		}
	}
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Show Checkboard (Full Screen) 显示棋盘
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void Checkerboard(void)
{
unsigned char i,j;
	
	for(i=0;i<8;i++)
	{
		Set_Start_Page(i);
		Set_Start_Column(0x00);

		for(j=0;j<66;j++)
		{
			Write_Data(0x55);
			Write_Data(0xaa);
		}
	}
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Show Frame (Full Screen)
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void Frame(void)
{
unsigned char i,j;
	
	Set_Start_Page(0x00);
	Set_Start_Column(XLevel);

	for(i=0;i<Max_Column;i++)
	{
		Write_Data(0x01);
	}

	Set_Start_Page(0x07);
	Set_Start_Column(XLevel);

	for(i=0;i<Max_Column;i++)
	{
		Write_Data(0x80);
	}

	for(i=0;i<8;i++)
	{
		Set_Start_Page(i);

		for(j=0;j<Max_Column;j+=(Max_Column-1))
		{
			Set_Start_Column(XLevel+j);

			Write_Data(0xFF);
		}
	}
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Show Character (5x7)
//
//    a: Database 数据库
//    b: Ascii ASCII码
//    c: Start Page 起始页
//    d: Start Column 起始行
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void Show_Font57(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
unsigned char *Src_Pointer = NULL;
unsigned char i = 0;

	switch(a)
	{
		case 1:
			Src_Pointer=&Ascii_1[(b)][i];
			break;
		case 2:
			Src_Pointer=&Ascii_2[(b)][i];
			break;
		default :
			break;
	}
	Set_Start_Page(c);
	Set_Start_Column(d);

	for(i=0;i<5;i++)
	{
		Write_Data(*Src_Pointer);
		Src_Pointer++;
	}
	Write_Data(0x00);
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Show String
//
//    a: Database
//    b: Start Page
//    c: Start Column
//    * Must write "0" in the end...
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void Show_String(unsigned char a, unsigned char *Data_Pointer, unsigned char b, unsigned char c)
{
	unsigned char *Src_Pointer;

	Src_Pointer=Data_Pointer;
	Show_Font57(1,96,b,c);			// No-Break Space
						//   Must be written first before the string start...

	while(1)
	{
		Show_Font57(a,*Src_Pointer,b,c);
		Src_Pointer++;
		c+=6;
		if(*Src_Pointer == 0) break;
	}
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Show Pattern (Partial or Full Screen)
//
//    a: Start Page
//    b: End Page
//    c: Start Column
//    d: Total Columns
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void Show_Pattern(unsigned char *Data_Pointer, unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
unsigned char *Src_Pointer;
unsigned char i,j;

	Src_Pointer=Data_Pointer;
	for(i=a;i<(b+1);i++)
	{
		Set_Start_Page(i);
		Set_Start_Column(c);

		for(j=0;j<d;j++)
		{
			Write_Data(*Src_Pointer);
			Src_Pointer++;
		}
	}
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Vertical垂直 / Fade渐渐 Scrolling (Partial or Full Screen)
//
//    a: Scrolling Direction 方向
//       "0x00" (Upward)
//       "0x01" (Downward)
//    b: Set Top Fixed Area
//    c: Set Vertical Scroll Area
//    d: Set Numbers of Row Scroll per Step
//    e: Set Time Interval时间间隔 between Each Scroll Step滚动步骤
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void Vertical_Scroll(unsigned char a, unsigned char b, unsigned char c, unsigned char d, unsigned char e)
{
unsigned int i,j;	

	Write_Command(0xA3);		//   Set Vertical Scroll Area
	Write_Command(b);			//   Default => 0x00 (Top Fixed Area)
	Write_Command(c);			//   Default => 0x40 (Vertical Scroll Area)

	switch(a)
	{
		case 0:
			for(i=0;i<c;i+=d)
			{
				Set_Start_Line(i);
				for(j=0;j<e;j++)
				{
					udelay(200);
				}
			}
			break;
		case 1:
			for(i=0;i<c;i+=d)
			{
				Set_Start_Line(c-i);
				for(j=0;j<e;j++)
				{
					udelay(200);
				}
			}
			break;
	}
	Set_Start_Line(0x00);
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Continuous连续 Horizontal水平 Scrolling (Partial or Full Screen)
//
//    a: Scrolling Direction 方向
//       "0x00" (Rightward)
//       "0x01" (Leftward)
//    b: Set Numbers of Column Scroll per Step
//    c: Define Start Page Address
//    d: Define End Page Address
//    e: Set Time Interval时间间隔 between Each Scroll Step in Terms of Frame帧 Frequency
//    f: delay Time
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void Horizontal_Scroll(unsigned char a, unsigned char b, unsigned char c, unsigned char d, unsigned char e, unsigned char f)
{
	Write_Command(0x26|a);			// Horizontal Scroll Setup
	Write_Command(b);
	Write_Command(c);
	Write_Command(e);
	Write_Command(d);
	Write_Command(0x2F);			// Activate Scrolling
	mdelay(1000*f);
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Continuous连续 Vertical / Horizontal / Diagonal对角 Scrolling (Partial or Full Screen)
//
//    a: Scrolling Direction
//       "0x00" (Vertical & Rightward)
//       "0x01" (Vertical & Leftward)
//    b: Set Numbers of Column Scroll per Step (Horizontal / Diagonal Scrolling)
//    c: Define Start Row Address (Horizontal / Diagonal Scrolling)
//    d: Define End Page Address (Horizontal / Diagonal Scrolling)
//    e: Set Top Fixed Area (Vertical Scrolling)
//    f: Set Vertical Scroll Area (Vertical Scrolling)
//    g: Set Numbers of Row Scroll per Step (Vertical / Diagonal Scrolling)
//    h: Set Time Interval between Each Scroll Step in Terms of Frame Frequency
//    i: delay Time
//    * e+f must be less than or equal to the Multiplex Ratio...
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void Continuous_Scroll(unsigned char a, unsigned char b, unsigned char c, unsigned char d, unsigned char e, unsigned char f, unsigned char g, unsigned char h, unsigned char i)
{
	Write_Command(0xA3);			// Set Vertical Scroll Area
	Write_Command(e);			//   Default => 0x00 (Top Fixed Area)
	Write_Command(f);			//   Default => 0x40 (Vertical Scroll Area)

	Write_Command(0x29+a);			// Continuous Vertical & Horizontal Scroll Setup
	Write_Command(b);
	Write_Command(c);
	Write_Command(h);
	Write_Command(d);
	Write_Command(g);
	Write_Command(0x2F);			// Activate Scrolling
	mdelay(1000*i);
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Deactivate停止 Scrolling (Full Screen)
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void Deactivate_Scroll(void)
{
	Write_Command(0x2E);			// Deactivate Scrolling
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Fade In (Full Screen) 渐渐进入
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void Fade_In(void)
{
unsigned int i;	

//	Set_Display_On_Off(0x01);
	for(i=0;i<(Brightness+1);i++)
	{
		Set_Contrast_Control(i);
		mdelay(1);
	}
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Fade Out (Full Screen) 褪色  渐渐退出
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void Fade_Out(void)
{
unsigned int i;	

	for(i=(Brightness+1);i>0;i--)
	{
		Set_Contrast_Control(i-1);
		mdelay(1);
	}
//	Set_Display_On_Off(0x00);
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Sleep Mode
//
//    "0x00" Enter Sleep Mode
//    "0x01" Exit Sleep Mode
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void Sleep(unsigned char a)
{
	switch(a)
	{
		case 0:
			Set_Display_On_Off(0x00);
			Set_Entire_Display(0x01);
			break;
		case 1:
			Set_Entire_Display(0x00);
			Set_Display_On_Off(0x01);
			break;
	}
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Connection Test 连续测试
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void Test(void)
{
	SSD1305_rest();

	Set_Entire_Display(0x01);		// Enable Entire总体 Display On (0x00/0x01)

	while(1)
	{
		Set_Display_On_Off(0x01);	// Display On (0x00/0x01)
		mdelay(2000);
		Set_Display_On_Off(0x00);	// Display Off (0x00/0x01)
		mdelay(2000);
	}
}

#endif

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Initialization GPIO
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

static void LS1B_GPIO_Init(void)
{
	unsigned int ret;
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_CFG0)); //GPIO0	0xbfd010c0 使能GPIO
	ret |= 0x1FFF << 8;
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_CFG0)) = ret;
	
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OE0)); //输出 使能GPIO
	ret &= ~(0x1FFF << 8);
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OE0)) = ret;
}

static void SSD1305_rest(void)
{
	unsigned int ret;
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)); //GPIO0	0xbfd010c0 使能GPIO
	ret &= ~RES;
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)) = ret;
	mdelay(2);
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)); //GPIO0	0xbfd010c0 使能GPIO
	ret |= RES;
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OUT0)) = ret;
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Initialization
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
static void SSD1305_Init(void)
{
	LS1B_GPIO_Init();
	SSD1305_rest();
	
	Set_Display_On_Off(0x00);		// Display Off (0x00/0x01)
	Set_Display_Clock(0xA0);			// Set Clock as 80 Frames/Sec
	Set_Multiplex_Ratio(0x3F);		// 1/64 Duty (0x0F~0x3F)
	Set_Display_Offset(0x00);		// Shift Mapping RAM Counter (0x00~0x3F)
	Set_Start_Line(0x00);			// Set Mapping RAM Display Start Line (0x00~0x3F)
	Set_Master_Config(0x00);			// Disable Embedded DC/DC Converter (0x00/0x01)
	Set_Area_Color(0x05);			// Set Monochrome & Low Power Save Mode
	Set_Addressing_Mode(0x02);		// Set Page Addressing Mode (0x00/0x01/0x02)
	Set_Segment_Remap(0x01);			// Set SEG/Column Mapping (0x00/0x01)
	Set_Common_Remap(0x08);			// Set COM/Row Scan Direction (0x00/0x08)
	Set_Common_Config(0x10);			// Set Alternative Configuration (0x00/0x10)
	Set_LUT(0x3F,0x3F,0x3F,0x3F);		// Define All Banks Pulse Width as 64 Clocks
	Set_Contrast_Control(Brightness);	// Set SEG Output Current
	Set_Area_Brightness(Brightness);	// Set Brightness for Area Color Banks
	Set_Precharge_Period(0xD2);			// Set Pre-Charge as 13 Clocks & Discharge as 2 Clock
	Set_VCOMH(0x34);					// Set VCOM Deselect Level
	Set_Entire_Display(0x00);			// Disable Entire Display On (0x00/0x01)
	Set_Inverse_Display(0x00);			// Disable Inverse Display On (0x00/0x01)

	Fill_RAM(0x00);						// Clear Screen

	Set_Display_On_Off(0x01);			// Display On (0x00/0x01)

/*
	Set_Multiplex_Ratio(0x3F);			// 1/64 Duty (0x0F~0x3F)
	Set_Addressing_Mode(0x00);			// Set Page Addressing Mode (0x00/0x01/0x02)
	Set_Contrast_Control(0x88);			// Set SEG Output Current
	Set_Display_On_Off(0x01);			// Display On (0x00/0x01)
	Set_Start_Line(0x00);				// Set Mapping RAM Display Start Line (0x00~0x3F)
	Fill_RAM(0x00);						// Clear Screen
*/
#if 0
	Write_Command(0xAE);/*display off*/
	
	Write_Command(0x02);/*set lower column address*/
	Write_Command(0x10);/*set higher column address*/
	Write_Command(0x40);/*set display start line*/
		   
	Write_Command(0xB0);/*set page address*/
		   
	Write_Command(0x81);/*contract control*/
	Write_Command(0x80);/*128*/
		   
	Write_Command(0xA1);/*set segment remap*/
	Write_Command(0xA4);/*normal display*/
	Write_Command(0xA6);/*normal / reverse*/
		   
	Write_Command(0xA8);/*multiplex ratio*/
	Write_Command(0x3F);/*duty = 1/64*/
		   
	Write_Command(0xC8);/*Com scan direction*/
		   
	Write_Command(0xD3);/*set display offset*/
	Write_Command(0x00);
		   
	Write_Command(0xD5);/*set osc division*/
	Write_Command(0x00);
		   
	Write_Command(0xD8);/*set area color mode off*/
	Write_Command(0x00);
		   
	Write_Command(0xD9);/*set pre-charge period*/
	Write_Command(0x11);
		   
	Write_Command(0xDA);/*set COM pins*/
	Write_Command(0x12);
		   
	Write_Command(0xAF);/*display ON*/
	Fill_RAM(0x00);/*一定要*/
#endif
}

#if 0
//#ifdef SSD1305Z_TEST
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  main test Program
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void main_test(void)
{
unsigned char Name[]={53,78,73,86,73,83,73,79,78,0};
						// Univision
unsigned char Tel[]={11,24,24,22,13,19,23,13,21,24,22,21,24,22,0};
						// +886-37-586586
unsigned char i;

	SSD1305_Init();
	Write_Dot_OLED(0, 0, 1);
	mdelay(1000);
	Fill_RAM(0x00);
	
//	while(1)
	{
	// Show Pattern - UniV OLED
		Show_Pattern(UniV, 0x02, 0x05, XLevel+0x28, 0x30);
		mdelay(1000);

	// Fade In/Out (Full Screen)
		Fade_Out();
		Fade_In();
		Fade_Out();
		Fade_In();
		mdelay(1000);

	// Scrolling滚动 (Partial部分 or Full Screen)
		Vertical_Scroll(0x00,0x00,Max_Row,0x01,0x60);
						// Upward - Full Screen
		mdelay(1000);
		Vertical_Scroll(0x01,0x00,Max_Row,0x01,0x60);
						// Downward - Full Screen
		mdelay(1000);
		Deactivate_Scroll();
		Continuous_Scroll(0x00,0x00,0x00,0x00,0x00,0x20,0x01,0x00,0x01);
						// Upward - Top Area
		Continuous_Scroll(0x00,0x00,0x00,0x00,0x00,0x20,0x1F,0x00,0x01);
						// Downward - Top Area
		Continuous_Scroll(0x00,0x01,0x00,0x03,0x00,0x20,0x01,0x00,0x02);
						// Up & Rightward - Top Area
		Continuous_Scroll(0x01,0x01,0x00,0x03,0x00,0x20,0x1F,0x00,0x02);
						// Down & Leftward - Top Area
		Continuous_Scroll(0x01,0x01,0x04,0x07,0x00,0x20,0x01,0x00,0x02);
						// Upward - Top Area
						// Leftward - Bottom Area
		Continuous_Scroll(0x00,0x01,0x04,0x07,0x00,0x20,0x1F,0x00,0x02);
						// Downward - Top Area
						// Rightward - Bottom Area
		Deactivate_Scroll();

	// All Pixels像数 On (Test Pattern)
		Fill_RAM(0xFF);
		mdelay(1000);

	// Partial Brightness Control 部分亮度控制
		Set_Area_Color(0x35);		// Set Area Color & Low Power Save Mode
		Set_Bank_Color();
		Set_LUT(0x3F,0x0F,0x1F,0x2F);
		mdelay(1000);

		for(i=16;i>0;i--)
		{
			Set_LUT(0x3F,i-1,0x1F,0x2F);
			mdelay(5);
		}
		for(i=0;i<64;i++)
		{
			Set_LUT(0x3F,i,0x1F,0x2F);
			mdelay(5);
		}
		for(i=64;i>0;i--)
		{
			Set_LUT(0x3F,i-1,0x1F,0x2F);
			mdelay(5);
		}
		for(i=0;i<64;i++)
		{
			Set_LUT(0x3F,i,0x1F,0x2F);
			mdelay(5);
		}
		mdelay(1000);
		Set_Area_Color(0x05);		// Set Monochrome单色 & Low Power Save Mode

	// Checkerboard棋盘 (Test Pattern)
		Checkerboard();
		mdelay(1000);
		Fill_RAM(0x00);			// Clear Screen

	// Frame (Test Pattern) 帧
		Frame();
		mdelay(1000);
		printk(KERN_EMERG "Frame\n");
		
	// Show String 显示字符串 - Univision +886-37-586586
		Show_String(1, Name, 0x03, XLevel+0x25);
		Show_String(1, Tel, 0x04, XLevel+0x16);
//		mdelay(3000);
//		Fill_RAM(0x00);			// Clear Screen
	}

}

#endif

//========================================================================
// 函数: void Write_Dot_OLED(DOTBYTE x, DOTBYTE y, LCDBYTE i)
// 描述: 在LCD的真实坐标系上的X、Y点绘制填充色为i的点
// 参数: x 		X轴坐标
//		 y 		Y轴坐标
//		 i 		要填充的点的颜色 
// 返回: 无
//========================================================================
static void Write_Dot_LCD(int x, int y, int i)
{
	int x_low,x_hight;						//定义列地址的高低位指令
	int Dot_Mask_Buf = 0x01;
	int y_Page;								//用于存放要画点的位置所在的byte数据位置
	x = x + 2;
	x_low = (x & 0x0F);							//定位列地址设置的低位指令
	x_hight = ((x >> 4) & 0x0F) + 0x10;		//定位列地址设置的高位指令
	switch (y & 0x07)
	{
		case 0: Dot_Mask_Buf = 0x01;break;
		case 1: Dot_Mask_Buf = 0x02;break;
		case 2: Dot_Mask_Buf = 0x04;break;
		case 3: Dot_Mask_Buf = 0x08;break;
		case 4: Dot_Mask_Buf = 0x10;break;
		case 5: Dot_Mask_Buf = 0x20;break;
		case 6: Dot_Mask_Buf = 0x40;break;
		case 7: Dot_Mask_Buf = 0x80;break;
	}
	y_Page = (y >> 3) + 0xB0;			//Get the page of the byte
	Write_Command(y_Page);
	Write_Command(x_low);
	Write_Command(x_hight);
	Write_Command(0xE0);
	y_Page = Read_Data();
	if (i){
		y_Page |= Dot_Mask_Buf;
	}
	else{
		y_Page &= ~Dot_Mask_Buf;
	}
	Write_Data(y_Page);
	Write_Command(0xEE);
}

//========================================================================
// 函数: void Line(unsigned char s_x,unsigned char s_y,unsigned char e_x,unsigned char e_y)
// 描述: 在s_x、s_y为起始坐标，e_x、e_y为结束坐标绘制一条直线
// 参数: x  X轴坐标     y  Y轴坐标
// 返回: 无
// 备注: 使用前景色
// 版本:
//      2006/10/15      First version
//========================================================================
static void Line(int s_x, int s_y, int e_x, int e_y)
{  
	int Offset_x,Offset_y,Offset_k = 0;
	int Err_d = 1;

	if(s_y > e_y){
		Offset_x = s_x;
		s_x = e_x;
		e_x = Offset_x;
		Offset_x = s_y;
		s_y = e_y;
		e_y = Offset_x;
	} 
	Offset_x = e_x-s_x;
	Offset_y = e_y-s_y;
	Writ_Dot(s_x, s_y, BMP_Color);

	if(Offset_x < 0){
		Offset_x = s_x-e_x;
		// Err_d = s_x;
		// s_x = e_x;
		// e_x = Err_d;
		Err_d = -1;
	}
	if(Offset_x==0){
		while(s_y<e_y){
			s_y++;
			if(s_y>Dis_Y_MAX) return;
			Writ_Dot/*Write_Dot_LCD*/(s_x,s_y,BMP_Color);
		}
		return;
	}
	else if(Offset_y==0){
		while(s_x!=e_x){
			s_x+=Err_d;
			if(s_x>Dis_X_MAX) return;
			Writ_Dot/*Write_Dot_LCD*/(s_x,s_y,BMP_Color);
		}
		return;
	}
	if(Offset_x>Offset_y){
		Offset_k += Offset_y;
		while(s_x!=e_x){
			if(Offset_k>0){
				s_y+=1;
				Offset_k += (Offset_y-Offset_x);
			}
			else Offset_k += Offset_y;
			s_x+=Err_d;
			if(s_x>Dis_X_MAX||s_y>Dis_Y_MAX) break;
			Writ_Dot/*Write_Dot_LCD*/(s_x,s_y,BMP_Color);
		}
	}
	else{
		Offset_k += Offset_x;
		while(s_y!=e_y){
			if(Offset_k>0){
				s_x+=Err_d;
				Offset_k += (Offset_x-Offset_y);
			}
			else Offset_k += Offset_x;
			s_y+=1;
			if(s_x>Dis_X_MAX||s_y>Dis_Y_MAX) break;
			Writ_Dot/*Write_Dot_LCD*/(s_x,s_y,BMP_Color);
		}
	}
}

//========================================================================
// 函数: void Rectangle(DOTBYTE left, DOTBYTE top, DOTBYTE right, 
//						DOTBYTE bottom, BYTE Mode)
// 描述: 
// 参数: left - 矩形的左上角横坐标，范围0到118
//		 top - 矩形的左上角纵坐标，范围0到50
//		 right - 矩形的右下角横坐标，范围1到119
//		 bottom - 矩形的右下角纵坐标，范围1到51
//		 Mode - 绘制模式，可以是下列数值之一：
//				0:	矩形框（空心矩形）
//				1:	矩形面（实心矩形）
// 返回: 无
// 备注: 画圆函数执行较慢，如果MCU有看门狗，请作好清狗的操作
// 版本:
//      2005/05/21      First version
//========================================================================
static void Rectangle(int left, int top, int right, int bottom, int Mode)
{
	int uiTemp;
	
	if(Mode==0)
	{
		Line(left,top,left,bottom);
		Line(left,top,right,top);
		Line(right,bottom,left,bottom);
		Line(right,bottom,right,top);
	}
	else
	{
//		Pos_Switch(&left,&top);						// 坐标变换
//		Pos_Switch(&right,&bottom);					//坐标变换
		if(left>right)
		{
			uiTemp = left;
			left = right;
			right = uiTemp;
		}
		if(top>bottom)
		{
			uiTemp = top;
			top = bottom;
			bottom = uiTemp;
		}
		for(uiTemp=top; uiTemp<=bottom; uiTemp++)
		{
			Line(left, uiTemp, right, uiTemp);
		}
	}
}

//========================================================================
// 函数: void W_Red_Dot(unsigned char x,unsigned char y,char a,char b,unsigned char mode)
// 描述: 绘制圆的各个像限中的点和线
// 参数: 
// 返回: 无
// 备注: 该函数对用户不可见，使用前景色
// 版本:
//      2006/10/15      First version
//========================================================================
static void W_Red_Dot(int x, int y, int a, int b, int mode)
{
    if(mode > 0)
    {
       Line(x+a,y+b,x-a,y+b);
       Line(x+a,y-b,x-a,y-b);
    }
    else
    {
       Writ_Dot(x+a, y+b, BMP_Color);
       Writ_Dot(x-a, y+b, BMP_Color);
       Writ_Dot(x+a, y-b, BMP_Color);
       Writ_Dot(x-a, y-b, BMP_Color);
    }
}
//========================================================================
// 函数: void W_Red_Err(int *a,int *b,int *r)
// 描述: 画圆误差计算
// 参数: 
// 返回: 无
// 备注: 该函数对用户不可见
// 版本:
//      2006/10/16      First version
//========================================================================
static void W_Red_Err(int *a, int *b, int *r)
{
	int R_Error;
	int uiTemp;
	R_Error = (*a+1)*(*a+1);
	uiTemp = (*b)*(*b);
	R_Error += uiTemp;
	uiTemp = (*r)*(*r);
	R_Error -= uiTemp;
	if(R_Error>=0)
	{
		R_Error = R_Error-*b;
		if(R_Error>=0) *b = *b-1;
	}
	*a = *a+1;
}
//========================================================================
// 函数: void Circle(unsigned char x,unsigned char y,unsigned char r,unsigned char mode)
// 描述: 以x,y为圆心R为半径画一个圆(mode = 0) or 圆面(mode = 1)
// 参数: 
// 返回: 无
// 备注: 画圆函数执行较慢，如果MCU有看门狗，请作好清狗的操作
// 版本:
//      2006/10/16      First version
//========================================================================
static void Circle(int x, int y, int r, int mode)
{
	int arx1 = 0, ary1, arx2, ary2 = 0;
//	Pos_Switch(&x,&y);						//坐标变换
//	x += 4;
	ary1 = r;
	arx2 = r;
	while(1){
		W_Red_Dot(x,y,arx1,ary1,mode);
		W_Red_Err(&arx1,&ary1,&r);
		if(arx1 == arx2){
			W_Red_Dot(x,y,arx1,ary1,mode);
			break;
		}
		W_Red_Dot(x,y,arx2,ary2,mode);
		W_Red_Err(&ary2,&arx2,&r);
		if(arx1 == arx2) {
			W_Red_Dot(x,y,arx2,ary2,mode);
			break;
		}
	}
}

//========================================================================
// 函数: void FontSet(unsigned char Font_NUM, unsigned char Color)
// 描述: 文本字体设置
// 参数: Font_NUM 字体选择,以驱动所带的字库为准
//		 Color  文本颜色,仅作用于自带字库  
// 返回: 无
// 备注: 
// 版本:
//========================================================================
static void FontSet(unsigned char Font_NUM, unsigned char Color)
{
	switch(Font_NUM)
	{
		case 0: Font_Wrod = 16;	//ASII字符A
				X_Witch = 8;
				Y_Witch = 16;
				Char_Color = Color;
				Font_type = 1;
				Char_TAB = Asii0816;
		break;
		case 1: Font_Wrod = 10;	//ASII字符B
				X_Witch = 6;
				Y_Witch = 10;
				Char_Color = Color;
				Font_type = 1;
				Char_TAB = Asii0610;
		break;		
/*		case 2: Font_Wrod = 48;	//汉字A
				X_Witch = 17;
				Y_Witch = 16;
				Char_Color = Color;
				Font_type = 0;
				Char_TAB = GB1716;
		break;*/
/*		case 3: Font_Wrod = 16;	//汉字B
				X_Witch = 16;
				Y_Witch = 2;
				Char_Color = Color;
				Font_type = 0;
				Char_TAB = GB16;
		break;*/
		default: break;
	}
}
//========================================================================
// 函数: void FontMode(BYTE Mode,LCDBYTE FontBackColor) 
// 描述: 设置字符显示的模式,并设置背景色
// 参数: Mode	0: 字符显示时仅对字符的有效点进行显示操作,也就是不作背景覆盖
//					为叠加模式
//				1: 覆盖模式
//		FontBackColor	设置覆盖模式时,字符的背景色 
// 返回: 无
// 备注: 仅对字符显示有效
// 版本:
//========================================================================
static void FontMode(int Mode, int FontBackColor)
{
	Font_Cover = Mode;
	Char_BackColor = FontBackColor;
}
//========================================================================
// 函数: void PutChar(unsigned char x,unsigned char y,char a)  
// 描述: 写入一个标准字符
// 参数: x  X轴坐标     y  Y轴坐标
//		 a  要显示的字符在字库中的偏移量  
// 返回: 无
// 备注: ASCII字符可直接输入ASCII码即可
// 版本:
//========================================================================
static void PutChar(int x, int y, int a)
{
	int i,j;		//数据暂存
	unsigned char *p_data;
	unsigned char Temp = 0;
	int Index = 0;
	
	if(Font_type==1)
		p_data = Char_TAB + (a-32)*Font_Wrod;
	else
		p_data = Char_TAB + a*Font_Wrod;	//要写字符的首地址
	j = 0;
	while((j ++) < Y_Witch)
	{
		if(y > Dis_Y_MAX) break;
		i = 0;
		while(i < X_Witch)
		{
			if((i&0x07)==0)
			{
//				Temp = *(p_data + (Index>>1));
//				if((Index&0x01)==0)Temp = Temp>>8; 
				Temp = *(p_data+Index);
				Index++;
			}
			if((Temp & 0x80) > 0) Writ_Dot(x+i,y,Char_Color);//Write_Dot_LCD
			else if(Font_Cover) Writ_Dot(x+i,y,Char_BackColor);//*Write_Dot_LCD
			Temp = Temp << 1;
			if((x+i) >= Dis_X_MAX)
			{
				Index += (X_Witch-i)>>3;
				break;
			}
			i++;
		}
		y ++;
	}
}

//========================================================================
// 函数: void PutString(unsigned char x,unsigned char y, unsigned char *p)
// 描述: 在x、y为起始坐标处写入一串标准字符
// 参数: x  X轴坐标     y  Y轴坐标
//		 p  要显示的字符串  
// 返回: 无
// 备注: 仅能用于自带的ASCII字符串显示
// 版本:
//========================================================================
static void PutString(int x, int y, const unsigned char *p)
{
	while(*p!=0)
	{
		PutChar(x,y,*p);
		x += X_Witch;
		if((x + X_Witch) > Dis_X_MAX)
		{
			x = 0;
			if((Dis_Y_MAX - y) < Y_Witch) break;
			else y += Y_Witch;
		}
		p++;
	}
}

static ssize_t
SSD1305LCD_read( struct file * file, char __user * buf, 
				size_t count, loff_t *ppos )
{
	const char __user *p = buf;
	char c;
	int i;

#ifdef DEBUG
	printk(KERN_DEBUG "LCD: read\n");
#endif
	//access_ok()函数 检查用户空间指针是否可用
	//buf  :   用户空间的指针变量，其指向一个要检查的内存块开始处。
	//count   :   要检查内存块的大小。
	//此函数检查用户空间中的内存块是否可用。如果可用，则返回真(非0值)，否则返回假(0)。
	if (!access_ok(VERIFY_READ, buf, count))
		return -EFAULT;

	mutex_lock(&SSD1305LCD_mutex);
	for ( i = *ppos; count > 0; ++i, ++p, --count )
	{
		c = Read_Data();
		__put_user(c, p);
	}
	mutex_unlock(&SSD1305LCD_mutex);
	*ppos = i;
	return p - buf;
}

static ssize_t
SSD1305LCD_write( struct file * file, const char __user * buf, 
				size_t count, loff_t *ppos )
{
	const char __user *p = buf;
	char c;
	int i;

#ifdef DEBUG
	printk(KERN_DEBUG "LCD: write\n");
#endif
	//access_ok()函数 检查用户空间指针是否可用
	//buf  :   用户空间的指针变量，其指向一个要检查的内存块开始处。
	//count   :   要检查内存块的大小。
	//此函数检查用户空间中的内存块是否可用。如果可用，则返回真(非0值)，否则返回假(0)。
	if (!access_ok(VERIFY_READ, buf, count))
		return -EFAULT;

	mutex_lock(&SSD1305LCD_mutex);
	for ( i = *ppos; count > 0; ++i, ++p, --count )
	{
		__get_user(c, p);
		Write_Data(c);
	}
	mutex_unlock(&SSD1305LCD_mutex);
	*ppos = i;
	return p - buf;
}

static long
SSD1305LCD_ioctl(struct file * file, unsigned int cmd, unsigned long arg )
{
	long ret = 0;
#ifdef DEBUG
	printk(KERN_DEBUG "LCD: ioctl(%d,%d)\n",cmd,arg);
#endif
	mutex_lock(&SSD1305LCD_mutex);
//	if (_IOC_TYPE(cmd) != LSLCD_IOC_MAGIC) return -EINVAL;
//	if (_IOC_NR(cmd) > LSLCD_IOC_MAXNR) return -EINVAL;
	switch (cmd)
	{
	case SSD1305LCD_CLEAR:
		Fill_RAM(arg);
	break;
	case SSD1305LCD_SENDCTRL:
		if (!capable(CAP_SYS_ADMIN))
			ret = -EACCES;
		else
			Write_Command(arg);
		break;
	case SSD1305LCD_REST:
		if (!capable(CAP_SYS_ADMIN))
			ret = -EACCES;
		else
			SSD1305_rest();
		break;
	case SSD1305LCD_WRITE_DOT:
		{
		int dot_info[3];
		__copy_from_user(dot_info, (int __user *)arg, sizeof(dot_info));
		Write_Dot_LCD(dot_info[0], dot_info[1], dot_info[2]);
		}
		break;
	case SSD1305LCD_LINE:
		{
		int line_info[4];
		__copy_from_user(line_info, (int __user *)arg, sizeof(line_info));
		Line(line_info[0], line_info[1], line_info[2], line_info[3]);
		}
	break;
	case SSD1305LCD_RECTANGLE:
		{
		int rectangle_info[5];
		__copy_from_user(rectangle_info, (int __user *)arg, sizeof(rectangle_info));
		Rectangle(rectangle_info[0], rectangle_info[1], rectangle_info[2], rectangle_info[3], rectangle_info[4]);
		}
	break;
	case SSD1305LCD_CIRCLE:
		{
		int circle_info[4];
		__copy_from_user(circle_info, (int __user *)arg, sizeof(circle_info));
		Circle(circle_info[0], circle_info[1], circle_info[2], circle_info[3]);
		}
	break;
	case SET_PRINT_MODE:
		BMP_Color = arg;
	break;
	default:
		ret = -EINVAL;
	}
	mutex_unlock(&SSD1305LCD_mutex);
	return ret;
}

static int
SSD1305LCD_open( struct inode * inode, struct file * file )
{
	return 0;
}

const struct file_operations SSD1305LCD_fops = {
	.owner				= THIS_MODULE,
	.write				= SSD1305LCD_write,
	.read				= SSD1305LCD_read,
	.unlocked_ioctl		= SSD1305LCD_ioctl,
	.open				= SSD1305LCD_open,
	.llseek			= default_llseek,
};

static struct miscdevice SSD1305LCD_dev = {
	.minor	= SSD1305LCD_MINOR,
	.name	= "SSD1305LCD",
	.fops	= &SSD1305LCD_fops
};

const unsigned char SSD1305LCD_logo[] =	"*********************"  /* Line #1 */
										"*                   *"  /* Line #1 */
										"*     Welcome to    *"  /* Line #3 */
										"*                   *"  /* Line #1 */
				              			"*       LINUX!      *"  /* Line #2 */
				              			"*********************"; /* Line #4 */

static int __init
SSD1305LCD_init(void)
{
	int retval;
	
	retval = misc_register(&SSD1305LCD_dev);
	if(retval < 0){
		printk(KERN_INFO "LCD: misc_register failed\n");
		return retval;
	}
#ifdef DEBUG
	printk(KERN_DEBUG "LCD: init\n");
#endif
	
	mutex_lock(&SSD1305LCD_mutex);
	
	SSD1305_Init();
//	main_test();
	FontSet(1,1);
	PutString(0, 0, SSD1305LCD_logo);//在指定位置显示字符串
	
	mutex_unlock(&SSD1305LCD_mutex);
	printk(KERN_EMERG "SSD1305Z OLED\n");
	return 0;
}

static void __exit
SSD1305LCD_exit(void)
{
	misc_deregister(&SSD1305LCD_dev);
}

module_init(SSD1305LCD_init);
module_exit(SSD1305LCD_exit);

MODULE_DESCRIPTION("SSD1305 Driver");
MODULE_AUTHOR("loongson THF");
MODULE_LICENSE("GPL");
