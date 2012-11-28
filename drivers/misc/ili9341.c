/*  */
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
#include <asm/ioctl.h>
#include <asm/gpio.h>

#include <ls1b_board.h>

#include "ili9341.h"

#define dis_zero  0
#define ILI9341_LCD_MINOR			156

#define ILI9341LCD_CLEAR			0x01		//清屏？
#define ILI9341LCD_SENDCTRL			0x02		//发送Ctrl
#define ILI9341LCD_SETSHORTDELAY	0x03		//SET短暂的延迟
#define ILI9341LCD_SETLONGDELAY		0x04		//SET长延迟
#define ILI9341LCD_REST				0x05
#define ILI9341LCD_WRITE_DOT		0x06
#define ILI9341LCD_LINE				0x07
#define ILI9341LCD_RECTANGLE		0x08
#define ILI9341LCD_CIRCLE			0x09
#define SET_PRINT_MODE				0x0a
#define PUT_CHAR					0x0b

static DEFINE_MUTEX(ili9341_lcd_mutex);

//extern unsigned int Asii8[];		//6X8的ASII字符库
extern unsigned char Asii1529[];	//15X29的ASII字符库
extern unsigned char GB32[];		//32*32自定义的汉字库

int x_witch;				//字符写入时的宽度
int y_witch;				//字符写入时的高度
int font_wrod;				//字体的大
unsigned char *char_tab;	//字库指针
int plot_mode;				//绘图模式
unsigned int bmp_color;
unsigned int char_color;


#define STLS1B_N_GPIO		64
#define STLS1B_GPIO_IN_OFFSET	16

#define LOONGSON_REG(x)	\
	(*(volatile u32 *)((char *)CKSEG1ADDR(x)))

#define LOONGSON_GPIOCFG0	LOONGSON_REG(0xbfd010c0)
#define LOONGSON_GPIOCFG1	LOONGSON_REG(0xbfd010c4)
#define LOONGSON_GPIOIE0 	LOONGSON_REG(0xbfd010d0)
#define LOONGSON_GPIOIE1	LOONGSON_REG(0xbfd010d4)
#define LOONGSON_GPIOIN0	LOONGSON_REG(0xbfd010e0)
#define LOONGSON_GPIOIN1	LOONGSON_REG(0xbfd010e4)
#define LOONGSON_GPIOOUT0	LOONGSON_REG(0xbfd010f0)
#define LOONGSON_GPIOOUT1	LOONGSON_REG(0xbfd010f4)
static void gpio_set_value(unsigned int gpio, int state)
{
	u32 val;
	u32 mask;

	if (gpio >= STLS1B_N_GPIO) {
		return ;
	}

	if(gpio >= 32){
		mask = 1 << (gpio - 32);
		val = LOONGSON_GPIOOUT1;
		if (state)
			val |= mask;
		else
			val &= (~mask);
		LOONGSON_GPIOOUT1 = val;
	}else{
		mask = 1 << gpio;
		val = LOONGSON_GPIOOUT0;
		if(state)	
			val |= mask;
		else
			val &= ~mask;
		LOONGSON_GPIOOUT0 = val;
	}
}

static int ls1b_gpio_direction_output(struct gpio_chip *chip,
		unsigned int gpio, int level)
{
	u32 temp;
	u32 mask;

	if (gpio >= STLS1B_N_GPIO)
		return -EINVAL;

	gpio_set_value(gpio, level);
	
	if(gpio >= 32){
		mask = 1 << (gpio - 32);
		temp = LOONGSON_GPIOCFG1;
		temp |= mask;
		LOONGSON_GPIOCFG1 = temp;
		temp = LOONGSON_GPIOIE1;
		temp &= (~mask);
		LOONGSON_GPIOIE1 = temp;
	}else{
		mask = 1 << gpio;
		temp = LOONGSON_GPIOCFG0;
		temp |= mask;
		LOONGSON_GPIOCFG0 = temp;
		temp = LOONGSON_GPIOIE0;
		temp &= (~mask);
		LOONGSON_GPIOIE0 = temp;
	}

	return 0;
}

static void gpio_init(void)
{
	u32 ret;

	ls1b_gpio_direction_output(NULL, LCDCS, 0);
	ls1b_gpio_direction_output(NULL, LCDA0, 1);
	ls1b_gpio_direction_output(NULL, LCDWR, 1);
	ls1b_gpio_direction_output(NULL, LCDRD, 1);
	ls1b_gpio_direction_output(NULL, LCDRES, 1);
//	gpio_set_value(LCDCS, 0);
//	gpio_set_value(LCDA0, 1);
//	gpio_set_value(LCDWR, 1);
//	gpio_set_value(LCDRD, 1);
//	gpio_set_value(LCDRES, 1);

	ret = readl(0xbfd010c0);
	ret |= 0xFFFF << 8;
	writel(ret, 0xbfd010c0);
	
	ret = readl(0xbfd010d0);
	ret &= ~(0xFFFF << 8);
	writel(ret, 0xbfd010d0);

}

static void write_data16(unsigned int dat)
{
	u32 ret;
	ret = readl(0xbfd010f0);
	ret &= ~(0xFFFF << 8);
	writel(ret | (dat<<8), 0xbfd010f0);
	gpio_set_value(LCDWR, 0);
	gpio_set_value(LCDWR, 1);
}

static void write_data8(unsigned int dat)
{
	u32 ret;
	
	ret = readl(0xbfd010f0);
	ret &= ~(0xFFFF << 8);
//	tmp1 = (0x1F & data) << 19;	/* 低5bit */
//	tmp2 = ((0xE0 & data) >> 5) << 10;	/* 高3bit ((0xE0 & dat) >> 5) << 10; */
	writel(ret | ((0x1F & dat) << 19) | ((0xE0 & dat) << 5), 0xbfd010f0);
	gpio_set_value(LCDWR, 0);
	gpio_set_value(LCDWR, 1);
}

static void write_command(unsigned int command)
{
	gpio_set_value(LCDCS, 0);
	gpio_set_value(LCDA0, 0);
	write_data8(command);
	gpio_set_value(LCDA0, 1);
}

static unsigned int read_data16(void)
{
	u32 ret;
	u32 tmp1;

	ret = readl(0xbfd010d0);
	ret |= 0xFFFF << 8;
	writel(ret, 0xbfd010d0);
	
	gpio_set_value(LCDRD, 0);
	tmp1 = ((readl(0xbfd010e0) >> 8) & 0xFFFF);
	gpio_set_value(LCDRD, 1);

	ret = readl(0xbfd010d0);
	ret &= ~(0xFFFF << 8);
	writel(ret, 0xbfd010d0);
	
	return tmp1;
}

static unsigned int read_data8(void)
{
	u32 ret;
	u32 tmp1;
	u32 data1, data2;

	ret = readl(0xbfd010d0);
	ret |= 0xFFFF << 8;
	writel(ret, 0xbfd010d0);
	
	gpio_set_value(LCDRD, 0);
	tmp1 = readl(0xbfd010e0);
	data1 = (tmp1 >> 19) & 0x1F;
	data2 = (tmp1 >> 5) & 0xE0;
	tmp1 = data1 | data2;
	gpio_set_value(LCDRD, 1);

	ret = readl(0xbfd010d0);
	ret &= ~(0xFFFF << 8);
	writel(ret, 0xbfd010d0);
	
	return tmp1;
}

static void ili9341_rest(void)
{
	gpio_set_value(LCDRES, 0);	//LCD 复位有效(L) 
	mdelay(100); // 延时100ms , Datasheet 要求至少大于1us
	gpio_set_value(LCDRES, 1);	//LCD 复位无效(H)
	mdelay(100); //硬件复位
}

static void read_id4(void)
{
	u32 parameter1, parameter2, parameter3, parameter4;
	write_command(0xd3);
	parameter1 = read_data8();
	parameter2 = read_data8();
	parameter3 = read_data8();
	parameter4 = read_data8();
	printk("ID: %x %x %x %x\n", parameter1, parameter2, parameter3, parameter4);
}

//========================================================================
// 函数: void clear_dot_lcd(int x, int y)
// 描述: 清除在LCD的真实坐标系上的X、Y点（清除后该点为黑色）
// 参数: x 		X轴坐标
//		 y 		Y轴坐标
// 返回: 无
// 备注: 
// 版本:
//========================================================================
static void clear_dot_lcd(int x, int y)
{
	x = y;//无意义，仅为了不提警告
}

//========================================================================
// 函数: unsigned int get_dot_lcd(int x, int y)
// 描述: 获取在LCD的真实坐标系上的X、Y点上的当前填充色数据
// 参数: x 		X轴坐标
//		 y 		Y轴坐标
// 返回: 该点的颜色
// 备注: 
// 版本:
//========================================================================
static unsigned int get_dot_lcd(int x, int y)
{
/*
	unsigned int Read_Data;
	LCD_RegWrite(0x20,x);
	LCD_RegWrite(0x21,y);
	LCD_Reg22();
	Read_Data = LCD_DataRead();
	return Read_Data;
*/
	return 0;
}

//========================================================================
// 函数: void set_dot_addr_lcd(int x, int y)
// 描述: 设置在LCD的真实坐标系上的X、Y点对应的RAM地址
// 参数: x 		X轴坐标
//		 y 		Y轴坐标
// 返回: 无
// 备注: 仅设置当前操作地址，为后面的连续操作作好准备
// 版本:
//========================================================================
static void set_dot_addr_lcd(int x, int y)
{
	//列地址 x
	write_command(0x2A);
	write_data8((x>>8) & 0xFF);	//高8位
	write_data8(x & 0xFF);		//低8位
	write_data8(0x00);			//改变xy像素大小时需要修改这里.
	write_data8(0xEF);
	//页地址 y
	write_command(0x2B);
	write_data8((y>>8) & 0xFF);	//高8位
	write_data8(y & 0xFF);		//低8位
	write_data8(0x01);
	write_data8(0x3F);
	
	write_command(0x2c);
}

//========================================================================
// 函数: void fill_dot_lcd(unsigned int color)
// 描述: 填充一个点到LCD显示缓冲RAM当中，而不管当前要填充的地址如何
// 参数: color 		要填充的点的颜色 
// 返回: 无
// 备注: 
// 版本:
//========================================================================
static void fill_dot_lcd(unsigned int color)
{
//	write_command(0x2c);
	write_data16(color);
}
//========================================================================
// 函数: void write_dot_lcd(int x, int y, unsigned int i)
// 描述: 在LCD的真实坐标系上的X、Y点绘制填充色为i的点
// 参数: x 		X轴坐标
//		 y 		Y轴坐标
//		 i 		要填充的点的颜色 
// 返回: 无
// 备注: 
// 版本:
//========================================================================
static void write_dot_lcd(int x, int y, unsigned int i)
{
/*	//列地址
	write_command(0x2A);
	write_data8((x>>8) & 0xFF);	//高8位
	write_data8(x & 0xFF);		//低8位
	write_data8(0x01);
	write_data8(0x3F);
	//页地址
	write_command(0x2B);
	write_data8((y>>8) & 0xFF);	//高8位
	write_data8(y & 0xFF);		//低8位
	write_data8(0x00);
	write_data8(0xEF);
	
	write_command(0x2c);*/
	set_dot_addr_lcd(x, y);
	write_data16(i);
}

//========================================================================
// 函数: void lcd_fill(unsigned int dat)
// 描述: 会屏填充以dat的数据至各点中
// 参数: dat   要填充的颜色数据
// 返回: 无
// 备注: 仅在LCD初始化程序当中调用
// 版本:
//========================================================================
static void lcd_fill(unsigned int dat)
{
	int i;
	int j;
	set_dot_addr_lcd(0, 0);
	//改变xy像素大小时需要修改这里.
	for(i=0; i<240; i++){
		for(j=0; j<320; j++){
			fill_dot_lcd(dat);
		}
	}
}

//========================================================================
// 函数: void lcd_fill_s(unsigned int number, unsigned int color)
// 描述: 连续填充以color色调的number个点
// 参数: number 填充的数量    color  像素点的颜色  
// 返回:
// 备注:
// 版本:
//========================================================================
static void lcd_fill_s(unsigned int number, unsigned int color)
{
//	LCD_Reg22();
	while(number != 0){
		fill_dot_lcd(color);
		number--;
	}
}

//========================液晶显示模块初始化================================= 
static void ili9341_init(void)
{ 
	gpio_init();
	gpio_set_value(LCDRES, 0);       //LCD 复位有效(L) 
	mdelay(100); // 延时100ms , Datasheet 要求至少大于1us
	gpio_set_value(LCDRES, 1);    //LCD 复位无效(H)
	mdelay(100); //硬件复位
	read_id4();
	//************* Start Initial Sequence **********//
	write_command(0x11);	//Sleep OUT
	mdelay(100);
//	write_command(0xCB);
//	write_data8(0x01);

	write_command(0xC0);	//Power Control 1 
	write_data8(0x23);
	write_data8(0x08);


	write_command(0xC1);	//Power Control 2 
	write_data8(0x04);     

	write_command(0xC5);	//VCOM Control 1
	write_data8(0x25);
	write_data8(0x2b);

	write_command(0x36);	//Memory Access Control 内存访问控制
	write_data8(0x48);		//改变xy像素大小时需要修改这里.

	write_command(0xB1);	//Frame Control 帧频控制
	write_data8(0x00);
	write_data8(0x1b);		//OSC 0x16,0x18

	write_command(0xB6);	//显示功能控制
	write_data8(0x0A);
	write_data8(0x82);

	write_command(0xC7);	//VCOM Control 2 
	write_data8(0xBC);

	write_command(0xF2);	//??
	write_data8(0x00);

	write_command(0x26);	//伽玛曲线设置
	write_data8(0x01);

	write_command(0x3a);	//像素格式设置
	write_data8(0x55);		//16 bits / pixel 

	write_command(0x2a);	//列地址设置
	write_data8(0x00);
	write_data8(0x00);
	write_data8(0x00);
	write_data8(0xef);		//239
	write_command(0x2b);	//
	write_data8(0x00);
	write_data8(0x00);
	write_data8(0x01);
	write_data8(0x3f);		//319

	//=======================================
	write_command(0xE0);	//正伽玛校正
	write_data8(0x1f);
	write_data8(0x25);
	write_data8(0x25);
	write_data8(0x0c);
	write_data8(0x11);
	write_data8(0x0a);
	write_data8(0x4e);
	write_data8(0xcb);
	write_data8(0x37);
	write_data8(0x03);
	write_data8(0x00);
	write_data8(0x00);
	write_data8(0x00);
	write_data8(0x00);
	write_data8(0x00);

	write_command(0XE1);	//负伽玛校正
	write_data8(0x00);
	write_data8(0x1a);
	write_data8(0x1c);
	write_data8(0x02);
	write_data8(0x0e);
	write_data8(0x06);
	write_data8(0x31);
	write_data8(0x36);
	write_data8(0x48);
	write_data8(0x0c);
	write_data8(0x1f);
	write_data8(0x1f);
	write_data8(0x3f);
	write_data8(0x3f);
	write_data8(0x1F);

	write_command(0x29);	//DISPON (Display ON) 
	
	udelay(800000);
	lcd_fill(LCD_INITIAL_COLOR);
/*	
	lcd_fill(RED);
	delay(1000000);
	lcd_fill(GREEN);
	delay(1000000);
	lcd_fill(WHITE);
	delay(1000000);
	lcd_fill(BLACK);
	delay(1000000);
	lcd_fill(CYAN);
	delay(1000000);
	lcd_fill(YELLOW);
	delay(1000000);
	lcd_fill(PURPLE);
	delay(1000000);
	lcd_fill(OLIVE);
	delay(1000000);
	lcd_fill(MAROON);
	delay(1000000);
	lcd_fill(WHITE);
	delay(1000000);

	write_dot_lcd(0, 0, RED);
	write_dot_lcd(239, 0, RED);
	write_dot_lcd(0, 319, RED);
	write_dot_lcd(239, 319, RED);
	write_dot_lcd(1, 0, RED);
	write_dot_lcd(2, 0, RED);
	write_dot_lcd(100, 100, RED);
	write_dot_lcd(101, 100, RED);
	write_dot_lcd(102, 100, RED);
	write_dot_lcd(103, 100, RED);
*/
}

//========================================================================
// 函数: void font_set(int font_num, unsigned int color)
// 描述: 文本字体设置
// 参数: font_num 字体选择,以驱动所带的字库为准
//		 color  文本颜色,仅作用于自带字库  
// 返回: 无
// 备注: 
// 版本:
//========================================================================
static void font_set(int font_num, unsigned int color)
{
	switch(font_num)
	{
/*		case 0: font_wrod = 3;	//ASII字符A
				x_witch = 6;
				y_witch = 1;
				char_color = color;
//				char_tab = (Asii8 - 32*3);
		break;*/
		case 1: font_wrod = 58;	//ASII字符B
				x_witch = 15;
				y_witch = 29;
				char_color = color;
				char_tab = (unsigned char *)(Asii1529 - (32*58));
		break;
		case 2: font_wrod = 128;	//汉字A
				x_witch = 32;
				y_witch = 32;
				char_color = color;
				char_tab = (unsigned char *)GB32;
		break;
/*		case 3: font_wrod = 16;	//汉字B
				x_witch = 16;
				y_witch = 2;
				char_color = color;
				char_tab = GB16;
		break;*/
		default: break;
	}
}

//========================================================================
// 函数: void put_char(int x, int y, unsigned int a)
// 描述: 写入一个标准字符
// 参数: x  X轴坐标     y  Y轴坐标
//		 a  要显示的字符在字库中的偏移量  
// 返回: 无
// 备注: ASCII字符可直接输入ASCII码即可
// 版本:
//========================================================================
static void put_char(int x, int y, unsigned int a)
{
	int i,j;//,K;		//数据暂存
	unsigned char *p_data;
	unsigned char temp;
	p_data = char_tab + a*font_wrod;	//要写字符的首地址
	j = 0;
	while((j ++) < y_witch){
		if(y > DIS_Y_MAX) break;
		i = 0;
		while(i < x_witch){
			if((i&0x07) == 0){
				temp = *(p_data ++);
			}
//			K = 0;
			if((x+i) > DIS_X_MAX) break;
			if((temp & 0x80) > 0) writ_dot(x+i,y,char_color);
			temp = temp << 1;
			i++;
		}
		y ++;
	}
}

//========================================================================
// 函数: void put_string(int x, int y, char *p)
// 描述: 在x、y为起始坐标处写入一串标准字符
// 参数: x  X轴坐标     y  Y轴坐标
//		 p  要显示的字符串  
// 返回: 无
// 备注: 仅能用于自带的ASCII字符串显示
// 版本:
//========================================================================
static void put_string(int x, int y, char *p)
{
	while(*p!=0){
		put_char(x,y,*p);
		x += x_witch;
		if((x + x_witch) > DIS_X_MAX){
			x = dis_zero;
			if((DIS_Y_MAX - y) < y_witch) break;
			else y += y_witch;
		}
		p++;
	}
}
/*
//========================================================================
// 函数: void bit_map(unsigned int *p, int x, int y)
// 描述: 写入一个BMP图片,起点为(x,y)
// 参数:   
// 返回: 无
// 备注:
// 版本:
//========================================================================
static void bit_map(unsigned int *p, int x, int y)
{
	int temp_with,temp_High,i,j;	//数据暂存
	temp_High = *(p ++);
	temp_with = *(p ++);			//图片宽度
	i = 0;
	while((i ++) < temp_High){
		j = 0;
		while((j ++) < temp_with){
			if(i < DIS_X_MAX) writ_dot(x+j,y+i,*p);
			else break;
		}
	}
}
*/
//========================================================================
// 函数: void set_paint_mode(int mode, unsigned int color)
// 描述: 绘图模式设置
// 参数: mode 绘图模式    color  像素点的颜色,相当于前景色  
// 返回: 无
// 备注: mode无效
// 版本:
//========================================================================
static void set_paint_mode(int mode, unsigned int color)
{
	plot_mode = mode;
	bmp_color = color;
}
//========================================================================
// 函数: void put_pixel(int x, int y)
// 描述: 在x、y点上绘制一个前景色的点
// 参数: x  X轴坐标     y  Y轴坐标
// 返回: 无
// 备注: 使用前景色
// 版本:
//========================================================================
static void put_pixel(int x, int y)
{
	writ_dot(x, y, bmp_color);
}
//========================================================================
// 函数: void line_my(int s_x, int s_y, int e_x, int e_y)
// 描述: 在s_x、s_y为起始坐标，e_x、e_y为结束坐标绘制一条直线
// 参数: x  X轴坐标     y  Y轴坐标
// 返回: 无
// 备注: 使用前景色
// 版本:
//========================================================================
static void line_my(int s_x, int s_y, int e_x, int e_y)
{  
	int offset_x,offset_y,offset_k = 0;
	int err_d = 1;
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
	if(offset_x <= 0){
		offset_x = s_x-e_x;
		err_d = -1;
	}
	if(offset_x > offset_y){
		while(s_x != e_x){
			if(offset_k > 0){
				s_y += 1;
				offset_k += (offset_y-offset_x);
			}
			else offset_k += offset_y; 
			s_x += err_d;
			if(s_x>LCD_X_MAX || s_y>LCD_Y_MAX) break;
			writ_dot(s_x, s_y, bmp_color);
		}	
	}
	else{
		while(s_y != e_y){
			if(offset_k > 0){
				s_x += err_d;
				offset_k += (offset_x-offset_y);
			}
			else offset_k += offset_x;
			s_y += 1;
			if(s_x>=LCD_X_MAX || s_y>=LCD_Y_MAX) break;
			writ_dot(s_x, s_y, bmp_color);
		}
	}    
}
/*
//========================================================================
// 函数: void w_db_line(int *p)
// 描述: 画一个任意多边形
// 参数: p
// 返回: 该函数无效
// 备注: 使用前景色
// 版本:
//========================================================================
static void w_db_line(int *p)
{
     int dot_sun,i;
     dot_sun = *p++;
     i = 0;
     while((i ++) < dot_sun)
     {
        W_line((*p >> 8)&0xff,*(p + 1)&0xff,(*(p + 1)>>8)&0xff,*(p + 1)&0xff);
        p += 1;
     }
}*/

//========================================================================
// 函数: void w_red_dot(int x, int y, int a, int b, int mode)
// 描述: 绘制圆的各个像限中的点和线
// 参数: 
// 返回: 无
// 备注: 该函数对用户不可见，使用前景色
// 版本:
//========================================================================
static void line_f(int s_x, int s_y, int e_x);//,int e_y);
static void w_red_dot(int x, int y, int a, int b, int mode)
{
	if(mode > 0){
		line_f(x+a, y+b, x-a);//,y+b);
		line_f(x+a, y-b, x-a);//,y-b);
	}
	else{
		put_pixel(x+a, y+b);
		put_pixel(x-a, y+b);
		put_pixel(x+a, y-b);
		put_pixel(x-a, y-b);
	}
}
//========================================================================
// 函数: void w_red_err(int *a, int *b, int *r)
// 描述: 画圆误差计算
// 参数: 
// 返回: 无
// 备注: 该函数对用户不可见
// 版本:
//========================================================================
static void w_red_err(int *a, int *b, int *r)
{
	int r_error;
	unsigned int uitemp;
	
	r_error = (*a+1)*(*a+1);
	uitemp = (*b)*(*b);
	r_error += uitemp;
	uitemp = (*r)*(*r);
	r_error -= uitemp;
	if(r_error >= 0){
		r_error = r_error-*b;
		if(r_error >= 0) *b = *b-1;
	}
	*a = *a+1;
}

//========================================================================
// 函数: void circle(int x, int y, int r, int mode)
// 描述: 以x,y为圆心R为半径画一个圆(mode = 0) or 圆面(mode = 1)
// 参数: 
// 返回: 无
// 备注: 画圆函数执行较慢，如果MCU有看门狗，请作好清狗的操作
// 版本:
//      2006/10/16      First version
//========================================================================
static void circle(int x, int y, int r, int mode)
{
	int arx1=0, ary1, arx2, ary2=0;
	
	pos_switch(&x, &y);						//坐标变换
	x += 4;
	ary1 = r;
	arx2 = r;
	while(1){
        w_red_dot(x, y, arx1, ary1, mode);
        w_red_err(&arx1, &ary1, &r);
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
// 函数: void rectangle(left, top, right, 
//		 			 bottom, mode)
// 描述: 以x,y为圆心R为半径画一个圆(mode = 0) or 圆面(mode = 1)
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
//========================================================================
static void rectangle(int left, int top, int right, 
					int bottom, int mode)
{
	unsigned int uitemp;
	
	pos_switch(&left, &top);						//坐标变换
	pos_switch(&right, &bottom);					//坐标变换
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
	if(mode == 0){
		line_my(left, top, left, bottom);
		line_my(left, top, right, top);
		line_my(right, bottom, left+1, bottom);
		line_my(right, bottom, right, top+1);
	}
	else{
		for(uitemp=top; uitemp<=bottom; uitemp++){
			line_f(left, uitemp, right);//,uitemp);
		}
	}
}
//========================================================================
// 函数: void writ_dot(int x, int y, unsigned int color)
// 描述: 填充以x,y为坐标的象素
// 参数: x  X轴坐标     y  Y轴坐标      color  像素颜色 
// 返回: 无
// 备注: 这里以及之前的所有x和y坐标系都是用户层的，并不是实际LCD的坐标体系
//		 本函数提供可进行坐标变换的接口
// 版本:
//========================================================================
static void writ_dot(int x, int y, unsigned int color)
{
#if	LCD_XY_SWITCH == 0
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 0)
		write_dot_lcd(x,y,color);
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 0)
		write_dot_lcd(LCD_X_MAX - x,y,color);
	#endif
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 1)
		write_dot_lcd(x,LCD_Y_MAX - y,color);
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 1)
		write_dot_lcd(LCD_X_MAX - x,LCD_Y_MAX - y,color);
	#endif
#endif
#if	LCD_XY_SWITCH == 1
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 0)
		write_dot_lcd(y,x,color);
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 0)
		write_dot_lcd(y,LCD_Y_MAX - x,color);
	#endif
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 1)
		write_dot_lcd(LCD_X_MAX - y,x,color);
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 1)
		write_dot_lcd(LCD_X_MAX - y,LCD_Y_MAX - x,color);
	#endif
#endif	
}
//========================================================================
// 函数: unsigned int get_dot(int x, int y)
// 描述: 获取x,y为坐标的象素
// 参数: x  X轴坐标     y  Y轴坐标      
// 返回: color  像素颜色 
// 备注: 这里以及之前的所有x和y坐标系都是用户层的，并不是实际LCD的坐标体系
//		 本函数提供可进行坐标变换的接口
// 版本:
//========================================================================
static unsigned int get_dot(int x, int y)
{
#if	LCD_XY_SWITCH == 0
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 0)
		return get_dot_lcd(x,y);
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 0)
		return get_dot_lcd(LCD_X_MAX - x,y);
	#endif
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 1)
		return get_dot_lcd(x,LCD_Y_MAX - y);
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 1)
		return get_dot_lcd(LCD_X_MAX - x,LCD_Y_MAX - y);
	#endif
#endif
#if	LCD_XY_SWITCH == 1
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 0)
		return get_dot_lcd(y,x);
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 0)
		return get_dot_lcd(y,LCD_Y_MAX - x);
	#endif
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 1)
		return get_dot_lcd(LCD_X_MAX - y,x);
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 1)
		return get_dot_lcd(LCD_X_MAX - y,LCD_Y_MAX - x);
	#endif
#endif	
}

//========================================================================
// 函数: void clear_dot(int x, int y)
// 描述: 清除以x,y为坐标的象素
// 参数: x  X轴坐标     y  Y轴坐标      
// 返回: 无 
// 备注: 这里以及之前的所有x和y坐标系都是用户层的，并不是实际LCD的坐标体系
//		 本函数提供可进行坐标变换的接口
// 版本:
//========================================================================
static void clear_dot(int x, int y)
{
#if	LCD_XY_SWITCH == 0
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 0)
		clear_dot_lcd(x,y);
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 0)
		clear_dot_lcd(LCD_X_MAX - x,y);
	#endif
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 1)
		clear_dot_lcd(x,LCD_Y_MAX - y);
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 1)
		clear_dot_lcd(LCD_X_MAX - x,LCD_Y_MAX - y);
	#endif
#endif
#if	LCD_XY_SWITCH == 1
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 0)
		clear_dot_lcd(y,x);
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 0)
		clear_dot_lcd(y,LCD_Y_MAX - x);
	#endif
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 1)
		clear_dot_lcd(LCD_X_MAX - y,x);
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 1)
		clear_dot_lcd(LCD_X_MAX - y,LCD_Y_MAX - x);
	#endif
#endif	
}
//========================================================================
// 函数: void set_dot_addr(int x, int y)
// 描述: 设置当前需要操作的象素地址
// 参数: x  X轴坐标     y  Y轴坐标      
// 返回: 无 
// 备注: 这里以及之前的所有x和y坐标系都是用户层的，并不是实际LCD的坐标体系
//		 本函数提供可进行坐标变换的接口
// 版本:
//========================================================================
static void set_dot_addr(int x, int y)
{
#if	LCD_XY_SWITCH == 0
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 0)
		set_dot_addr_lcd(x,y);
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 0)
		set_dot_addr_lcd(LCD_X_MAX - x,y);
	#endif
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 1)
		set_dot_addr_lcd(x,LCD_Y_MAX - y);
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 1)
		set_dot_addr_lcd(LCD_X_MAX - x,LCD_Y_MAX - y);
	#endif
#endif
#if	LCD_XY_SWITCH == 1
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 0)
		set_dot_addr_lcd(y,x);
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 0)
		set_dot_addr_lcd(y,LCD_Y_MAX - x);
	#endif
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 1)
		set_dot_addr_lcd(LCD_X_MAX - y,x);
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 1)
		set_dot_addr_lcd(LCD_X_MAX - y,LCD_Y_MAX - x);
	#endif
#endif		
}
//========================================================================
// 函数: void pos_switch(unsigned int * x, unsigned int * y)
// 描述: 将画面的坐标变换为实际LCD的坐标体系，以便于快速画圆形以及矩形
// 参数: x  X轴坐标     y  Y轴坐标      
// 返回: 无 
// 备注: 这里以及之前的所有x和y坐标系都是用户层的，并不是实际LCD的坐标体系
//		 本函数提供可进行坐标变换的接口
// 版本:
//========================================================================
static void pos_switch(unsigned int * x, unsigned int * y)
{
	*x = *x;
	*y = *y;
#if	LCD_XY_SWITCH == 0
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 0)
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 0)
		*x = LCD_X_MAX-*x;
	#endif
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 1)
		*y = LCD_Y_MAX-*y;
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 1)
		*x = LCD_X_MAX-*x;
		*y = LCD_Y_MAX-*y;
	#endif
#endif
#if	LCD_XY_SWITCH == 1
	unsigned int uitemp;
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 0)
		uitemp = *x;
		*x = y;
		*y = uitemp;
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 0)
		uitemp = LCD_Y_MAX-*x;
		*x = *y;
		*y = uitemp;
	#endif
	#if (LCD_X_REV == 0)&&(LCD_Y_REV == 1)
		uitemp = *x;
		*x = LCD_X_MAX-*y;
		*y = uitemp;
	#endif
	#if (LCD_X_REV == 1)&&(LCD_Y_REV == 1)
		uitemp = LCD_Y_MAX - *x;
		*x = LCD_X_MAX-*y;
		*y = uitemp;
	#endif
#endif
}
//========================================================================
// 函数: void line_f(int s_x, int s_y, int e_x, int e_y, int mode)
// 描述: 以s_x,s_y为起点,e_x,e_y为终点连续填充一条直线上的点.用于画矩形、圆
// 参数: x  X轴坐标     y  Y轴坐标      
// 返回: 无 
// 备注: 以实际的LCD坐标体系为准
// 版本:
//========================================================================
static void line_f(int s_x, int s_y, int e_x)//,int e_y)
{  
	unsigned int uitemp;
	if(s_x>e_x) 
	{
		uitemp = s_x;
		s_x = e_x;
		e_x = uitemp;
	}
	write_dot_lcd(s_x++,s_y,bmp_color);
	for(;s_x<=e_x;s_x++)
		fill_dot_lcd(bmp_color);
}
//========================================================================
// 函数: void clr_screen(void)
// 描述: 全屏以初始化屏幕色的颜色进行清屏
// 参数: 无      
// 返回: 无 
// 备注: 无
// 版本:
//========================================================================
static void clr_screen(void)
{  
	set_dot_addr_lcd(0,0);
	lcd_fill(LCD_INITIAL_COLOR);
}

static void ili9341_test(void)
{
	ili9341_init();							//LCD初始化
	font_set(1,0xf800);
	put_string(10,10,"Mz Design!");
//	clr_screen();
	font_set(1,0x07e0);
	put_string(10,42,"Mz");
	font_set(2,0x07e0);
	put_char(42,40,0);
	put_char(74,40,1);

	set_paint_mode(0,0x001f);
	put_pixel(10,72);
	put_pixel(12,72);
	put_pixel(14,72);
	line_my(10, 75, 230, 75);
	rectangle(20,80,100,120,1);
	rectangle(18,78,102,122,0);
	circle(60,180,30,1);
	circle(60,180,32,0);
}

static ssize_t ili9341_lcd_write( struct file * file, const u16 __user * buf, 
				                  size_t count, loff_t *ppos )
{
	const u16 __user *p = (u16)buf;
	u16 c;
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

	mutex_lock(&ili9341_lcd_mutex);
	for ( i = *ppos; count > 0; ++i, ++p, --count ){
		__get_user(c, p);
		write_data16(c);
	}
	mutex_unlock(&ili9341_lcd_mutex);
	*ppos = i;
	return p-buf;
}

static ssize_t ili9341_lcd_read( struct file * file, char __user * buf, 
				                 size_t count, loff_t *ppos )
{}

static long ili9341_lcd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
#ifdef DEBUG
	printk(KERN_DEBUG "LCD: ioctl(%d,%d)\n",cmd,arg);
#endif
	mutex_lock(&ili9341_lcd_mutex);
//	if (_IOC_TYPE(cmd) != LSLCD_IOC_MAGIC) return -EINVAL;
//	if (_IOC_NR(cmd) > LSLCD_IOC_MAXNR) return -EINVAL;
	switch (cmd){
		case ILI9341LCD_CLEAR:
			clr_screen();
		break;
		case ILI9341LCD_SENDCTRL:
			if (!capable(CAP_SYS_ADMIN))
				ret = -EACCES;
			else
				write_command(arg);
			break;
		case ILI9341LCD_REST:
			if (!capable(CAP_SYS_ADMIN))
				ret = -EACCES;
			else
				ili9341_rest();
			break;
		case ILI9341LCD_WRITE_DOT:
			{
			int dot_info[3];
			__copy_from_user(dot_info, (int __user *)arg, sizeof(dot_info));
			write_dot_lcd(dot_info[0], dot_info[1], dot_info[2]);
			}
			break;
		case ILI9341LCD_LINE:
			{
			int line_info[4];
			__copy_from_user(line_info, (int __user *)arg, sizeof(line_info));
			line_my(line_info[0], line_info[1], line_info[2], line_info[3]);
			}
		break;
		case ILI9341LCD_RECTANGLE:
			{
			int rectangle_info[5];
			__copy_from_user(rectangle_info, (int __user *)arg, sizeof(rectangle_info));
			rectangle(rectangle_info[0], rectangle_info[1], rectangle_info[2], rectangle_info[3], rectangle_info[4]);
			}
		break;
		case ILI9341LCD_CIRCLE:
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
	mutex_unlock(&ili9341_lcd_mutex);
	return ret;
}

static int ili9341_lcd_open( struct inode * inode, struct file * file )
{
	return 0;
}

const struct file_operations ili9341_lcd_fops = {
	.owner			= THIS_MODULE,
	.write			= ili9341_lcd_write,
	.read			= ili9341_lcd_read,
	.unlocked_ioctl	= ili9341_lcd_ioctl,
	.open			= ili9341_lcd_open,
	.llseek			= default_llseek,
};

static struct miscdevice ili9341_lcd_dev = {
	.minor	= ILI9341_LCD_MINOR,
	.name	= "ili9341_lcd",
	.fops	= &ili9341_lcd_fops
};

const unsigned char ili9341_lcd_logo[] =	"*********************"  /* Line #1 */
											"*                   *"  /* Line #1 */
											"*     Welcome to    *"  /* Line #3 */
											"*                   *"  /* Line #1 */
				              				"*       LINUX!      *"  /* Line #2 */
				              				"*********************"; /* Line #4 */

static int __init ili9341_lcd_init(void)
{
	int retval;
	
	retval = misc_register(&ili9341_lcd_dev);
	if(retval < 0){
		printk(KERN_INFO "LCD: misc_register failed\n");
		return retval;
	}
#ifdef DEBUG
	printk(KERN_DEBUG "LCD: init\n");
#endif
	
	mutex_lock(&ili9341_lcd_mutex);
	
	ili9341_init();
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
//	font_set(1,1);
//	put_string(0, 0, ili9341_lcd_logo);//在指定位置显示字符串
	
	mutex_unlock(&ili9341_lcd_mutex);
	printk(KERN_EMERG "ili9341_lcd\n");
	return 0;
}

static void __exit ili9341_lcd_exit(void)
{
	misc_deregister(&ili9341_lcd_dev);
}

module_init(ili9341_lcd_init);
module_exit(ili9341_lcd_exit);

MODULE_DESCRIPTION("ILI9341 Driver");
MODULE_AUTHOR("loongson THF");
MODULE_LICENSE("GPL");
