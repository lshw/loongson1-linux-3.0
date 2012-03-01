/*
 * ST7565R driver
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
#include <asm/gpio.h>

#include <ls1b_board.h>

#include "st7565r.h"

static DEFINE_MUTEX(st7565r_lcd_mutex);

static void st7565r_rest(void);
static void write_dot_lcd(int x, int y, int i);

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

static unsigned char x_witch;					//字符写入时的宽度
static unsigned char y_witch;					//字符写入时的高度
static int font_wrod;							//字体的每个字模占用多少个存储单元数
static unsigned char *char_tab;					//字库指针

static int font_type = 0;						//标识字符类型
static int font_cover = 0;						//字符显示的模式
static int char_backcolor = 0;					//设置覆盖模式时,字符的背景色

//unsigned char Plot_Mode;						//绘图模式
static int bmp_color = 1;
static int char_color = 1;

#undef DEBUG
//#define	DEBUG
//#define	M68			// 8-bit 68XX Parallel
						//   BS1=0; BS2=1
#define	I80				// 8-bit 80XX Parallel
						//   BS1=1; BS2=1
//#define	SPI			// 4-wire SPI
						//   BS1=0; BS2=0
						//   The unused pins should be connected with VSS mostly or floating (D2).
/*
#define LCDCS	16	//片选信号
#define LCDRS	17	//复位信号
#define LCDA0	18	//命令信号
#define LCDSK	11	//时钟信号
#define LCDSI	12	//数据信号

//M68
#define	LCDRW	19
#define	LCDE	20

//I80
#define	LCDWR	19
#define	LCDRD	20
*/

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
static void write_command(unsigned char Data)
{
	
}


static void write_data(unsigned char Data)
{
	
}
#endif


#ifdef I80					// 8-bit 80XX Parallel
static void inline write_command(unsigned char dat)
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
	ret |= ((0x00120000) | (dat << 8));
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


static void inline write_data(unsigned char dat)
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
	ret |= ((0x00160000) | (dat << 8));
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

static unsigned char inline read_data(void)
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
static void write_command(unsigned char Data)
{
	unsigned char i;                                      //定义局部变量 
	unsigned char series,temp;                           //定义局部变量 

	gpio_set_value(LCDA0, 0);
	gpio_set_value(LCDSK, 0);	//SCL=0   
	series = dat;   
	for(i=0; i<8; i++){
		gpio_set_value(LCDSK, 0);	//SCL=0   
		temp = series & 0x80;                    //屏蔽低7位   
		//如果temp=1,则使SI=1.如果temp+0,则使SI=0 
		if(temp){   
			gpio_set_value(LCDSI, 1);	//SI=1   
		}   
		else{   
			gpio_set_value(LCDSI, 0);	//SI=0   
		} 
		gpio_set_value(LCDSK, 1);	//SCL=1上升沿写
		series = series << 1;               //左移1位    
	}
}


static void write_data(unsigned char Data)
{
	unsigned char i;                                      //定义局部变量 
	unsigned char series,temp;                           //定义局部变量 

	gpio_set_value(LCDA0, 1);
	gpio_set_value(LCDSK, 0);	//SCL=0   
	series = dat;   
	for(i=0; i<8; i++){
		gpio_set_value(LCDSK, 0);	//SCL=0   
		temp = series & 0x80;                    //屏蔽低7位   
		//如果temp=1,则使SI=1.如果temp+0,则使SI=0 
		if(temp){   
			gpio_set_value(LCDSI, 1);	//SI=1   
		}   
		else{   
			gpio_set_value(LCDSI, 0);	//SI=0   
		} 
		gpio_set_value(LCDSK, 1);	//SCL=1上升沿写
		series = series << 1;               //左移1位    
	}
}
#endif

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Instruction Setting
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
static void inline set_lcd_bias(unsigned char d)
{
	write_command(0xA2|d);
}

static void inline set_power_control(unsigned char d)
{
	write_command(0x28|d);
}

static void inline set_resistor_ratio(unsigned char d)
{
	write_command(0x20|d);
}

static void inline set_start_column(unsigned char d)
{
	write_command(0x10+d/16);	// Set Higher Column Start Address for Page Addressing Mode
									// Default => 0x10
	write_command(0x00+d%16);	// Set Lower Column Start Address for Page Addressing Mode
									// Default => 0x00
}

static void inline set_start_line(unsigned char d)
{
	write_command(0x40|d);	// Set Display Start Line
								// Default => 0x40 (0x00)
}

static void inline set_contrast_control(unsigned char d)
{
	write_command(0x81);		// Set Contrast Control for Bank 0
	write_command(d);			// Default => 0x80
}

static void inline set_segment_remap(unsigned char d)
{
	write_command(0xA0|d);	// Set Segment Re-Map
							// Default => 0xA0
							// 0xA0 (0x00) => Column Address 0 Mapped to SEG0
							// 0xA1 (0x01) => Column Address 0 Mapped to SEG131
}


static void inline set_entire_display(unsigned char d)
{
	write_command(0xA4|d);	// Set Entire Display On / Off
							// Default => 0xA4
							// 0xA4 (0x00) => Normal Display
							// 0xA5 (0x01) => Entire Display On
}


static void inline set_inverse_display(unsigned char d)
{
	write_command(0xA6|d);	// Set Inverse Display On/Off
							// Default => 0xA6
							// 0xA6 (0x00) => Normal Display
							// 0xA7 (0x01) => Inverse Display On
}

static void inline set_display_on_off(unsigned char d)	
{
	write_command(0xAE|d);	// Set Display On/Off
							// Default => 0xAE
							// 0xAE (0x00) => Display Off
							// 0xAF (0x01) => Display On
}


static void inline set_start_page(unsigned char d)
{
	write_command(0xB0|d);	// Set Page Start Address for Page Addressing Mode
							// Default => 0xB0 (0x00)
}


static void inline set_common_remap(unsigned char d)
{
	write_command(0xC0|d);	// Set COM Output Scan Direction
							// Default => 0xC0
							// 0xC0 (0x00) => Scan from COM0 to 63
							// 0xC8 (0x08) => Scan from COM63 to 0
}

static void inline set_read_modify_write(unsigned char d)
{
	write_command(0xE0|d);	// Set Read Modify Write Mode
							// Default => 0xE0
							// 0xE0 (0x00) => Enter Read Modify Write
							// 0xEE (0x0E) => Exit Read Modify Write
}


static void inline set_nop(void)
{
	write_command(0xE3);		// Command for No Operation
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Show Regular Pattern (Full Screen)
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
static void fill_ram(unsigned char Data)
{
	unsigned char i,j;

	for(i=0; i<8; i++){
		set_start_page(i);
		set_start_column(LCD_X_OFFSET);//这里注意不同的LCD的偏移

		for(j=0; j<128; j++){
			write_data(Data);
		}
	}
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Initialization GPIO
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

static void gpio_init(void)
{
	unsigned int ret;
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_CFG0)); //GPIO0	0xbfd010c0 使能GPIO
	ret |= 0x1FFF << 8;
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_CFG0)) = ret;
	
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OE0)); //输出 使能GPIO
	ret &= ~(0x1FFF << 8);
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OE0)) = ret;
}

static void st7565r_rest(void)
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
static void st7565r_init(void)
{
	gpio_init();
	st7565r_rest();
	
	write_command(0xe2);	//寄存器复位
	udelay(1000);
/*
	// 电源管理初始 
	write_command(0xaf);	//0xaf显示器开,0xae显示器关
	write_command(0x40);	//开始行地址
	write_command(0xa2);	//1/7 bias
	write_command(0x2f);	//0x2f升压电路,电压管理电路,
							//电压跟随电路均开(0x28-0x2f为设置上电控制模式) 
	write_command(0x26);	//0x20-0x27为V5电压内部电阻调整设置 对比度调节
	
	write_command(0x81);	//电量设置模式 
	write_command(0x0f);	//0x01-0x3f电量寄存器设置模式 

	// 根据屏的方向初始化安装方向(复位后默认下面配置)
	write_command(0xa0);	//0xa0为Segment正向,0xa1 为Segment反向
							//(0xa0-0xa1为ADC方向选择)
//	write_command(0xa1);
//	write_command(0xc0);	//0xc0正向扫描,0xc8反射扫描
	write_command(0xc8);	//com0-com63-com1
	// 显示管理初始(复位后默认下面配置)
	write_command(0xa6);	//0xa6正向显示,0xa7反向显示
	write_command(0xa4);	//0xa4正常显示,0xa5全屏点亮

	// 指示器设置
	write_command(0xac);	//Sleep mode set 0xac:Sleep mode, 0xad:Normal mode 
	write_command(0x01);
	//背压比设置
	write_command(0xf8);
	write_command(0x00);
	//Page address set
//	write_command(0xb0);
	//Column address set
//	write_command(0x10);
//	write_command(0x00);

//	write_command(0xa5);	//0xa4正常显示,0xa5全屏点亮
*/
	write_command(0xa2);
	write_command(0xa1);
	write_command(0xc0);
	
	write_command(0x24);
	write_command(0x81);
	write_command(0x30);
	
	write_command(0x2f);
	
	write_command(0x60);
	
	write_command(0xa6);
	write_command(0xa4);
	
	write_command(0xaf);
	
	fill_ram(0x00);
}


//========================================================================
// 函数: void Write_Dot_OLED(DOTBYTE x, DOTBYTE y, LCDBYTE i)
// 描述: 在LCD的真实坐标系上的X、Y点绘制填充色为i的点
// 参数: x 		X轴坐标
//		 y 		Y轴坐标
//		 i 		要填充的点的颜色 
// 返回: 无
//========================================================================
static void write_dot_lcd(int x, int y, int i)
{
	int x_low, x_hight;						//定义列地址的高低位指令
	int dot_mask_buf = 0x01;
	int y_page;								//用于存放要画点的位置所在的byte数据位置
	x = x + LCD_X_OFFSET;
	x_low = (x & 0x0F);							//定位列地址设置的低位指令
	x_hight = ((x >> 4) & 0x0F) + 0x10;		//定位列地址设置的高位指令
	switch (y & 0x07){
		case 0: dot_mask_buf = 0x01;break;
		case 1: dot_mask_buf = 0x02;break;
		case 2: dot_mask_buf = 0x04;break;
		case 3: dot_mask_buf = 0x08;break;
		case 4: dot_mask_buf = 0x10;break;
		case 5: dot_mask_buf = 0x20;break;
		case 6: dot_mask_buf = 0x40;break;
		case 7: dot_mask_buf = 0x80;break;
	}
	y_page = (y >> 3) + 0xB0;			//Get the page of the byte
	write_command(y_page);
	write_command(x_low);
	write_command(x_hight);
	write_command(0xE0);
	y_page = read_data();
	if (i){
		y_page |= dot_mask_buf;
	}
	else{
		y_page &= ~dot_mask_buf;
	}
	write_data(y_page);
	write_command(0xEE);
}

//========================================================================
// 函数: void line(unsigned char s_x,unsigned char s_y,unsigned char e_x,unsigned char e_y)
// 描述: 在s_x、s_y为起始坐标，e_x、e_y为结束坐标绘制一条直线
// 参数: x  X轴坐标     y  Y轴坐标
// 返回: 无
// 备注: 使用前景色
// 版本:
//      2006/10/15      First version
//========================================================================
static void line(int s_x, int s_y, int e_x, int e_y)
{  
	int offset_x, offset_y, offset_k = 0;
	int Err_d = 1;

	if(s_y > e_y){
		offset_x = s_x;
		s_x = e_x;
		e_x = offset_x;
		offset_x = s_y;
		s_y = e_y;
		e_y = offset_x;
	} 
	offset_x = e_x-s_x;
	offset_y = e_y-s_y;
	writ_dot(s_x, s_y, bmp_color);

	if(offset_x < 0){
		offset_x = s_x-e_x;
		// Err_d = s_x;
		// s_x = e_x;
		// e_x = Err_d;
		Err_d = -1;
	}
	if(offset_x == 0){
		while(s_y < e_y){
			s_y++;
			if(s_y > Dis_Y_MAX) return;
			writ_dot/*write_dot_lcd*/(s_x, s_y, bmp_color);
		}
		return;
	}
	else if(offset_y == 0){
		while(s_x != e_x){
			s_x += Err_d;
			if(s_x > Dis_X_MAX) return;
			writ_dot/*write_dot_lcd*/(s_x, s_y, bmp_color);
		}
		return;
	}
	if(offset_x > offset_y){
		offset_k += offset_y;
		while(s_x != e_x){
			if(offset_k > 0){
				s_y += 1;
				offset_k += (offset_y-offset_x);
			}
			else offset_k += offset_y;
			s_x += Err_d;
			if(s_x>Dis_X_MAX||s_y>Dis_Y_MAX) break;
			writ_dot/*write_dot_lcd*/(s_x, s_y, bmp_color);
		}
	}
	else{
		offset_k += offset_x;
		while(s_y != e_y){
			if(offset_k > 0){
				s_x += Err_d;
				offset_k += (offset_x - offset_y);
			}
			else offset_k += offset_x;
			s_y += 1;
			if(s_x>Dis_X_MAX||s_y>Dis_Y_MAX) break;
			writ_dot/*write_dot_lcd*/(s_x, s_y, bmp_color);
		}
	}
}

//========================================================================
// 函数: void rectangle(DOTBYTE left, DOTBYTE top, DOTBYTE right, 
//						DOTBYTE bottom, BYTE mode)
// 描述: 
// 参数: left - 矩形的左上角横坐标，范围0到118
//		 top - 矩形的左上角纵坐标，范围0到50
//		 right - 矩形的右下角横坐标，范围1到119
//		 bottom - 矩形的右下角纵坐标，范围1到51
//		 mode - 绘制模式，可以是下列数值之一：
//				0:	矩形框（空心矩形）
//				1:	矩形面（实心矩形）
// 返回: 无
// 备注: 画圆函数执行较慢，如果MCU有看门狗，请作好清狗的操作
// 版本:
//      2005/05/21      First version
//========================================================================
static void rectangle(int left, int top, int right, int bottom, int mode)
{
	int uitemp;
	
	if(mode == 0){
		line(left, top, left, bottom);
		line(left, top, right, top);
		line(right, bottom, left, bottom);
		line(right, bottom, right, top);
	}
	else{
//		pos_switch(&left,&top);						// 坐标变换
//		pos_switch(&right,&bottom);					//坐标变换
		if(left > right){
			uitemp = left;
			left = right;
			right = uitemp;
		}
		if(top > bottom){
			uitemp = top;
			top = bottom;
			bottom = uitemp;
		}
		for(uitemp=top; uitemp<=bottom; uitemp++){
			line(left, uitemp, right, uitemp);
		}
	}
}

//========================================================================
// 函数: void w_red_dot(unsigned char x,unsigned char y,char a,char b,unsigned char mode)
// 描述: 绘制圆的各个像限中的点和线
// 参数: 
// 返回: 无
// 备注: 该函数对用户不可见，使用前景色
// 版本:
//      2006/10/15      First version
//========================================================================
static void w_red_dot(int x, int y, int a, int b, int mode)
{
	if(mode > 0){
		line(x+a, y+b, x-a, y+b);
		line(x+a, y-b, x-a, y-b);
	}
	else{
		writ_dot(x+a, y+b, bmp_color);
		writ_dot(x-a, y+b, bmp_color);
		writ_dot(x+a, y-b, bmp_color);
		writ_dot(x-a, y-b, bmp_color);
	}
}
//========================================================================
// 函数: void w_red_err(int *a,int *b,int *r)
// 描述: 画圆误差计算
// 参数: 
// 返回: 无
// 备注: 该函数对用户不可见
// 版本:
//      2006/10/16      First version
//========================================================================
static void w_red_err(int *a, int *b, int *r)
{
	int r_error;
	int uitemp;
	
	r_error = (*a+1) * (*a+1);
	uitemp = (*b) * (*b);
	r_error += uitemp;
	uitemp = (*r) * (*r);
	r_error -= uitemp;
	if(r_error >= 0){
		r_error = r_error - *b;
		if(r_error>=0) *b = *b - 1;
	}
	*a = *a + 1;
}
//========================================================================
// 函数: void circle(unsigned char x,unsigned char y,unsigned char r,unsigned char mode)
// 描述: 以x,y为圆心R为半径画一个圆(mode = 0) or 圆面(mode = 1)
// 参数: 
// 返回: 无
// 备注: 画圆函数执行较慢，如果MCU有看门狗，请作好清狗的操作
// 版本:
//      2006/10/16      First version
//========================================================================
static void circle(int x, int y, int r, int mode)
{
	int arx1 = 0, ary1, arx2, ary2 = 0;
//	pos_switch(&x,&y);						//坐标变换
//	x += 4;
	ary1 = r;
	arx2 = r;
	while(1){
		w_red_dot(x, y, arx1, ary1, mode);
		w_red_err(&arx1,&ary1,&r);
		if(arx1 == arx2){
			w_red_dot(x, y, arx1, ary1, mode);
			break;
		}
		w_red_dot(x, y, arx2, ary2, mode);
		w_red_err(&ary2, &arx2, &r);
		if(arx1 == arx2) {
			w_red_dot(x, y, arx2, ary2, mode);
			break;
		}
	}
}

//========================================================================
// 函数: void FontSet(unsigned char Font_NUM, unsigned char color)
// 描述: 文本字体设置
// 参数: Font_NUM 字体选择,以驱动所带的字库为准
//		 color  文本颜色,仅作用于自带字库  
// 返回: 无
// 备注: 
// 版本:
//========================================================================
static void font_set(unsigned char font_num, unsigned char color)
{
	switch(font_num){
		case 0: font_wrod = 16;	//ASII字符A
				x_witch = 8;
				y_witch = 16;
				char_color = color;
				font_type = 1;
				char_tab = asii0816;
		break;
		case 1: font_wrod = 10;	//ASII字符B
				x_witch = 6;
				y_witch = 10;
				char_color = color;
				font_type = 1;
				char_tab = asii0610;
		break;		
/*		case 2: font_wrod = 48;	//汉字A
				x_witch = 17;
				y_witch = 16;
				char_color = color;
				font_type = 0;
				char_tab = GB1716;
		break;*/
/*		case 3: font_wrod = 16;	//汉字B
				x_witch = 16;
				y_witch = 2;
				char_color = color;
				font_type = 0;
				char_tab = GB16;
		break;*/
		default: break;
	}
}
//========================================================================
// 函数: void font_mode(BYTE Mode,LCDBYTE FontBackcolor) 
// 描述: 设置字符显示的模式,并设置背景色
// 参数: Mode	0: 字符显示时仅对字符的有效点进行显示操作,也就是不作背景覆盖
//					为叠加模式
//				1: 覆盖模式
//		FontBackcolor	设置覆盖模式时,字符的背景色 
// 返回: 无
// 备注: 仅对字符显示有效
// 版本:
//========================================================================
static void font_mode(unsigned char mode, unsigned char font_back_color)
{
	font_cover = mode;
	char_backcolor = font_back_color;
}
//========================================================================
// 函数: void put_char(unsigned char x,unsigned char y,char a)  
// 描述: 写入一个标准字符
// 参数: x  X轴坐标     y  Y轴坐标
//		 a  要显示的字符在字库中的偏移量  
// 返回: 无
// 备注: ASCII字符可直接输入ASCII码即可
// 版本:
//========================================================================
static void put_char(int x, int y, int a)
{
	int i,j;		//数据暂存
	unsigned char *p_data;
	unsigned char temp = 0;
	unsigned char index = 0;
	
	if(font_type == 1)
		p_data = char_tab + (a-32)*font_wrod;
	else
		p_data = char_tab + a*font_wrod;	//要写字符的首地址
	j = 0;
	while((j++) < y_witch){
		if(y > Dis_Y_MAX) break;
		i = 0;
		while(i < x_witch){
			if((i&0x07) == 0){
//				temp = *(p_data + (index>>1));
//				if((index&0x01)==0)temp = temp>>8; 
				temp = *(p_data+index);
				index++;
			}
			if((temp & 0x80) > 0) writ_dot(x+i,y,char_color);//write_dot_lcd
			else if(font_cover) writ_dot(x+i,y,char_backcolor);//*write_dot_lcd
			temp = temp << 1;
			if((x+i) >= Dis_X_MAX){
				index += (x_witch-i) >> 3;
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
static void put_string(unsigned char x, unsigned char y, const unsigned char *p)
{
	while(*p != 0){
		put_char(x, y, *p);
		x += x_witch;
		if((x + x_witch) > Dis_X_MAX){
			x = 0;
			if((Dis_Y_MAX - y) < y_witch) break;
			else y += y_witch;
		}
		p++;
	}
}


/******************************************************************************/
static ssize_t st7565r_lcd_read( struct file * file, char __user * buf, 
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

	mutex_lock(&st7565r_lcd_mutex);
	for ( i = *ppos; count > 0; ++i, ++p, --count ){
		c = read_data();
		__put_user(c, p);
	}
	mutex_unlock(&st7565r_lcd_mutex);
	*ppos = i;
	return p - buf;
}

static ssize_t st7565r_lcd_write( struct file * file, const char __user * buf, 
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

	mutex_lock(&st7565r_lcd_mutex);
	for ( i = *ppos; count > 0; ++i, ++p, --count ){
		__get_user(c, p);
		write_data(c);
	}
	mutex_unlock(&st7565r_lcd_mutex);
	*ppos = i;
	return p - buf;
}

static long st7565r_lcd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
#ifdef DEBUG
	printk(KERN_DEBUG "LCD: ioctl(%d,%d)\n",cmd,arg);
#endif
	mutex_lock(&st7565r_lcd_mutex);
//	if (_IOC_TYPE(cmd) != LSLCD_IOC_MAGIC) return -EINVAL;
//	if (_IOC_NR(cmd) > LSLCD_IOC_MAXNR) return -EINVAL;
	switch (cmd){
		case ST7565RLCD_CLEAR:
			fill_ram(arg);
		break;
		case ST7565RLCD_SENDCTRL:
			if (!capable(CAP_SYS_ADMIN))
				ret = -EACCES;
			else
				write_command(arg);
			break;
		case ST7565RLCD_REST:
			if (!capable(CAP_SYS_ADMIN))
				ret = -EACCES;
			else
				st7565r_rest();
			break;
		case ST7565RLCD_WRITE_DOT:
			{
			int dot_info[3];
			__copy_from_user(dot_info, (int __user *)arg, sizeof(dot_info));
			write_dot_lcd(dot_info[0], dot_info[1], dot_info[2]);
			}
			break;
		case ST7565RLCD_LINE:
			{
			int line_info[4];
			__copy_from_user(line_info, (int __user *)arg, sizeof(line_info));
			line(line_info[0], line_info[1], line_info[2], line_info[3]);
			}
		break;
		case ST7565RLCD_RECTANGLE:
			{
			int rectangle_info[5];
			__copy_from_user(rectangle_info, (int __user *)arg, sizeof(rectangle_info));
			rectangle(rectangle_info[0], rectangle_info[1], rectangle_info[2], rectangle_info[3], rectangle_info[4]);
			}
		break;
		case ST7565RLCD_CIRCLE:
			{
			int circle_info[4];
			__copy_from_user(circle_info, (int __user *)arg, sizeof(circle_info));
			circle(circle_info[0], circle_info[1], circle_info[2], circle_info[3]);
			}
		break;
		case SET_PRINT_MODE:
			bmp_color = arg;
		break;
		default:
			ret = -EINVAL;
	}
	mutex_unlock(&st7565r_lcd_mutex);
	return ret;
}

static int st7565r_lcd_open( struct inode * inode, struct file * file )
{
	return 0;
}

const struct file_operations st7565r_lcd_fops = {
	.owner			= THIS_MODULE,
	.write			= st7565r_lcd_write,
	.read			= st7565r_lcd_read,
	.unlocked_ioctl	= st7565r_lcd_ioctl,
	.open			= st7565r_lcd_open,
	.llseek			= default_llseek,
};

static struct miscdevice st7565r_lcd_dev = {
	.minor	= ST7565R_LCD_MINOR,
	.name	= "st7565r_lcd",
	.fops	= &st7565r_lcd_fops
};

const unsigned char st7565r_lcd_logo[] =	"*********************"  /* Line #1 */
											"*                   *"  /* Line #1 */
											"*     Welcome to    *"  /* Line #3 */
											"*                   *"  /* Line #1 */
				              				"*       LINUX!      *"  /* Line #2 */
				              				"*********************"; /* Line #4 */

static int __init st7565r_lcd_init(void)
{
	int retval;
	
	retval = misc_register(&st7565r_lcd_dev);
	if(retval < 0){
		printk(KERN_INFO "LCD: misc_register failed\n");
		return retval;
	}
#ifdef DEBUG
	printk(KERN_DEBUG "LCD: init\n");
#endif
	
	mutex_lock(&st7565r_lcd_mutex);
	
	st7565r_init();
	/*
	write_dot_lcd(0, 0, 1);
	write_dot_lcd(127, 0, 1);
	write_dot_lcd(0, 63, 1);
	write_dot_lcd(127, 63, 1);
	write_dot_lcd(0, 1, 1);
	write_dot_lcd(0, 2, 1);
	write_dot_lcd(0, 3, 1);
	write_dot_lcd(0, 4, 1);
	
	write_dot_lcd(2, 7, 1);
	write_dot_lcd(3, 7, 1);
	write_dot_lcd(4, 7, 1);
	write_dot_lcd(5, 7, 1);
	
	line(4,4,50,60);
	line(4,4,100,4);
	line(4,4,4,60);
	line(0,0,120,60);
	rectangle(12,12,42,42,1);
	rectangle(12,12,62,62,0);
	mdelay(8000);
	
	circle(10,10,10,1);
	circle(118,54,10,0);
	circle(800,30,16,0);
	mdelay(8000);
	
//	mdelay(5000);
*/
	font_set(1,1);
	put_string(0, 0, st7565r_lcd_logo);//在指定位置显示字符串
	
	mutex_unlock(&st7565r_lcd_mutex);
	printk(KERN_EMERG "st7565r_lcd\n");
	return 0;
}

static void __exit st7565r_lcd_exit(void)
{
	misc_deregister(&st7565r_lcd_dev);
}

module_init(st7565r_lcd_init);
module_exit(st7565r_lcd_exit);

MODULE_DESCRIPTION("ST7565R Driver");
MODULE_AUTHOR("loongson THF");
MODULE_LICENSE("GPL");
