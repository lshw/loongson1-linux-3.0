/*
 * Copyright (c) 2012 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/ssd1305.h>


#define SSD1305FB_NAME "ssd1305fb"
#define SSD1305_NAME "ssd1305"

/* ssd1305 chipset specific defines and code */
#define brightness	0x2F

#define X_OFFSET	(2)
#define SSD1305_WIDTH	(128)
#define SSD1305_HEIGHT	(64)
#define SSD1305_CONTROLLERS	(1)
#define SSD1305_PAGES	(8)
#define SSD1305_ADDRESSES	(128)
#define SSD1305_SIZE		((SSD1305_CONTROLLERS) * \
				(SSD1305_PAGES) * \
				(SSD1305_ADDRESSES))
#define bit(n) (((unsigned char)1)<<(n))

static struct ssd1305_platform_data *pdata;
void __iomem *reg_addr;

static struct fb_fix_screeninfo ssd1305fb_fix __devinitdata = {
	.id = SSD1305FB_NAME,
	.type = FB_TYPE_PACKED_PIXELS,
	.visual = FB_VISUAL_MONO10,
	.xpanstep = 0,
	.ypanstep = 0,
	.ywrapstep = 0,
	.line_length = SSD1305_WIDTH / 8,
	.accel = FB_ACCEL_NONE,
};

static struct fb_var_screeninfo ssd1305fb_var __devinitdata = {
	.xres = SSD1305_WIDTH,
	.yres = SSD1305_HEIGHT,
	.xres_virtual = SSD1305_WIDTH,
	.yres_virtual = SSD1305_HEIGHT,
	.bits_per_pixel = 1,
	.red = { 0, 1, 0 },
	.green = { 0, 1, 0 },
	.blue = { 0, 1, 0 },
	.left_margin = 0,
	.right_margin = 0,
	.upper_margin = 0,
	.lower_margin = 0,
	.vmode = FB_VMODE_NONINTERLACED,
};

static void ssd1305_writeb_ctl(unsigned char value)
{
#ifdef CONFIG_SSD1305_8080
	u32 ret;
	
	gpio_set_value(pdata->gpios_cs, 0);
	gpio_set_value(pdata->gpios_dc, 0);
	gpio_set_value(pdata->gpios_rd, 1);
	gpio_set_value(pdata->gpios_wr, 0);

	ret = readl(reg_addr);
	ret &= ~(0xFF << pdata->datas_offset);
	writel(ret|(value<<pdata->datas_offset), reg_addr);

	gpio_set_value(pdata->gpios_cs, 1);
	gpio_set_value(pdata->gpios_dc, 1);
	gpio_set_value(pdata->gpios_wr, 1);
#elif CONFIG_SSD1305_6800
	u32 ret;
	
	gpio_set_value(pdata->gpios_cs, 0);
	gpio_set_value(pdata->gpios_dc, 0);
	gpio_set_value(pdata->gpios_wr, 0);	//RW
	gpio_set_value(pdata->gpios_rd, 1);	//E

	ret = readl(reg_addr);
	ret &= ~(0xFF << pdata->datas_offset);
	writel(ret|(value<<pdata->datas_offset), reg_addr);

	gpio_set_value(pdata->gpios_rd, 0);	//E
	gpio_set_value(pdata->gpios_cs, 1);
	gpio_set_value(pdata->gpios_dc, 1);
	gpio_set_value(pdata->gpios_wr, 1);	//RW
#elif CONFIG_SSD1305_SPI3
	unsigned char i;
	unsigned char data = value;

	gpio_set_value(pdata->gpios_cs, 0);
	gpio_set_value(pdata->gpios_d0, 0);//SCLK=0
	gpio_set_value(pdata->gpios_d1, 0);
	gpio_set_value(pdata->gpios_d0, 1);//SCLK=1
	for (i=0; i<8; i++) {
		gpio_set_value(pdata->gpios_d0, 0);//SCLK=0
		gpio_set_value(pdata->gpios_d1, (data&0x80)>>7);
		data = data << 1;
		gpio_set_value(pdata->gpios_d0, 1);//SCLK=1
	}
	gpio_set_value(pdata->gpios_cs, 1);
#elif CONFIG_SSD1305_SPI4
	unsigned char i;
	unsigned char data = value;

	gpio_set_value(pdata->gpios_cs, 0);
	gpio_set_value(pdata->gpios_dc, 0);
	for (i=0; i<8; i++) {
		gpio_set_value(pdata->gpios_d0, 0);//SCLK=0
		gpio_set_value(pdata->gpios_d1, (data&0x80)>>7);
		data = data << 1;
		gpio_set_value(pdata->gpios_d0, 1);//SCLK=1
	}
	gpio_set_value(pdata->gpios_dc, 1);
	gpio_set_value(pdata->gpios_cs, 1);
#elif CONFIG_SSD1305_I2C

#endif
}

static void ssd1305_writebyte(unsigned char byte)
{
#ifdef CONFIG_SSD1305_8080
	u32 ret;
	
	gpio_set_value(pdata->gpios_cs, 0);
	gpio_set_value(pdata->gpios_dc, 1);
	gpio_set_value(pdata->gpios_rd, 1);
	gpio_set_value(pdata->gpios_wr, 0);

	ret = readl(reg_addr);
	ret &= ~(0xFF << pdata->datas_offset);
	writel(ret|(byte<<pdata->datas_offset), reg_addr);

	gpio_set_value(pdata->gpios_cs, 1);
	gpio_set_value(pdata->gpios_wr, 1);
#elif CONFIG_SSD1305_6800
	u32 ret;
	
	gpio_set_value(pdata->gpios_cs, 0);
	gpio_set_value(pdata->gpios_dc, 1);
	gpio_set_value(pdata->gpios_wr, 0);	//RW
	gpio_set_value(pdata->gpios_rd, 1);	//E

	ret = readl(reg_addr);
	ret &= ~(0xFF << pdata->datas_offset);
	writel(ret|(byte<<pdata->datas_offset), reg_addr);

	gpio_set_value(pdata->gpios_rd, 0);	//E
	gpio_set_value(pdata->gpios_cs, 1);
	gpio_set_value(pdata->gpios_wr, 1);	//RW
#elif CONFIG_SSD1305_SPI3
	unsigned char i;
	unsigned char data = byte;

	gpio_set_value(pdata->gpios_cs, 0);

	gpio_set_value(pdata->gpios_d0, 0);//SCLK=0
	gpio_set_value(pdata->gpios_d1, 1);
	gpio_set_value(pdata->gpios_d0, 1);//SCLK=1

	for (i=0; i<8; i++) {
		gpio_set_value(pdata->gpios_d0, 0);//SCLK=0
		gpio_set_value(pdata->gpios_d1, (data&0x80)>>7);
		data = data << 1;
		gpio_set_value(pdata->gpios_d0, 1);//SCLK=1
	}
	gpio_set_value(pdata->gpios_cs, 1);
#elif CONFIG_SSD1305_SPI4
	unsigned char i;
	unsigned char data = byte;

	gpio_set_value(pdata->gpios_cs, 0);
	gpio_set_value(pdata->gpios_dc, 1);
	for (i=0; i<8; i++) {
		gpio_set_value(pdata->gpios_d0, 0);//SCLK=0
		gpio_set_value(pdata->gpios_d1, (data&0x80)>>7);
		data = data << 1;
		gpio_set_value(pdata->gpios_d0, 1);//SCLK=1
	}
	gpio_set_value(pdata->gpios_dc, 1);
	gpio_set_value(pdata->gpios_cs, 1);
#elif CONFIG_SSD1305_I2C

#endif
}


/*
 * ssd1305 Internal Commands
 */
static void inline ssd1305set_start_column(unsigned char d)
{
	ssd1305_writeb_ctl(0x10+d/16);	// Set Higher Column Start Address for Page Addressing Mode
									// Default => 0x10
	ssd1305_writeb_ctl(0x00+d%16);	// Set Lower Column Start Address for Page Addressing Mode
									// Default => 0x00
}

static void inline ssd1305set_addressing_mode(unsigned char d)
{
	ssd1305_writeb_ctl(0x20);	// Set Memory Addressing Mode
	ssd1305_writeb_ctl(d);		// Default => 0x02
								// 0x00 => Horizontal Addressing Mode
								// 0x01 => Vertical Addressing Mode
								// 0x02 => Page Addressing Mode
}

static void inline ssd1305set_column_address(unsigned char a, unsigned char b)
{
	ssd1305_writeb_ctl(0x21);	// Set Column Address
	ssd1305_writeb_ctl(a);		// Default => 0x00 (Column Start Address)
	ssd1305_writeb_ctl(b);		// Default => 0x83 (Column End Address)
}

static void inline ssd1305set_page_address(unsigned char a, unsigned char b)
{
	ssd1305_writeb_ctl(0x22);	// Set Page Address
	ssd1305_writeb_ctl(a);		// Default => 0x00 (Page Start Address)
	ssd1305_writeb_ctl(b);		// Default => 0x07 (Page End Address)
}

static void inline ssd1305set_start_line(unsigned char d)
{
	ssd1305_writeb_ctl(0x40|d);	// Set Display Start Line
								// Default => 0x40 (0x00)
}


static void inline ssd1305set_contrast_control(unsigned char d)
{
	ssd1305_writeb_ctl(0x81);	// Set Contrast Control for Bank 0
	ssd1305_writeb_ctl(d);		// Default => 0x80
}

static void inline ssd1305set_area_brightness(unsigned char d)
{
	ssd1305_writeb_ctl(0x82);	// Set Brightness for Area Color Banks
	ssd1305_writeb_ctl(d);		// Default => 0x80
}

static void inline ssd1305set_segment_remap(unsigned char d)
{
	ssd1305_writeb_ctl(0xA0|d);	// Set Segment Re-Map
								// Default => 0xA0
								// 0xA0 (0x00) => Column Address 0 Mapped to SEG0
								// 0xA1 (0x01) => Column Address 0 Mapped to SEG131
}

static void inline ssd1305set_entire_display(unsigned char d)
{
	ssd1305_writeb_ctl(0xA4|d);	// Set Entire Display On / Off
								// Default => 0xA4
								// 0xA4 (0x00) => Normal Display
								// 0xA5 (0x01) => Entire Display On
}

static void inline ssd1305set_inverse_display(unsigned char d)
{
	ssd1305_writeb_ctl(0xA6|d);	// Set Inverse Display On/Off
								// Default => 0xA6
								// 0xA6 (0x00) => Normal Display
								// 0xA7 (0x01) => Inverse Display On
}

static void inline ssd1305set_multiplex_ratio(unsigned char d)
{
	ssd1305_writeb_ctl(0xA8);	// Set Multiplex Ratio
	ssd1305_writeb_ctl(d);		// Default => 0x3F (1/64 Duty)
}

static void inline ssd1305set_dim_mode(unsigned char a, unsigned char b)
{
	ssd1305_writeb_ctl(0xAB);	// Set Dim Mode Configuration
	ssd1305_writeb_ctl(0X00);	// => (Dummy Write for First Parameter)
	ssd1305_writeb_ctl(a);		// Default => 0x80 (Contrast Control for Bank 0)
	ssd1305_writeb_ctl(b);		// Default => 0x80 (Brightness for Area Color Banks)
	ssd1305_writeb_ctl(0xAC);	// Set Display On in Dim Mode
}

static void inline ssd1305set_master_config(unsigned char d)
{
	ssd1305_writeb_ctl(0xAD);	// Set Master Configuration
	ssd1305_writeb_ctl(0x8E|d);	// Default => 0x8E
								// 0x8E (0x00) => Select External VCC Supply
								// 0x8F (0x01) => Select Internal DC/DC Voltage Converter
}

static void inline ssd1305set_display_on_off(unsigned char d)	
{
	ssd1305_writeb_ctl(0xAE|d);	// Set Display On/Off
								// Default => 0xAE
								// 0xAE (0x00) => Display Off
								// 0xAF (0x01) => Display On
}

static void inline ssd1305set_start_page(unsigned char d)
{
	ssd1305_writeb_ctl(0xB0|d);	// Set Page Start Address for Page Addressing Mode
								// Default => 0xB0 (0x00)
}

static void inline ssd1305set_common_remap(unsigned char d)
{
	ssd1305_writeb_ctl(0xC0|d);	// Set COM Output Scan Direction
								// Default => 0xC0
								// 0xC0 (0x00) => Scan from COM0 to 63
								// 0xC8 (0x08) => Scan from COM63 to 0
}

static void inline ssd1305set_display_offset(unsigned char d)
{
	ssd1305_writeb_ctl(0xD3);		// Set Display Offset
	ssd1305_writeb_ctl(d);			// Default => 0x00
}

static void inline ssd1305set_display_clock(unsigned char d)
{
	ssd1305_writeb_ctl(0xD5);	// Set Display Clock Divide Ratio / Oscillator Frequency
	ssd1305_writeb_ctl(d);		// Default => 0x70
								// D[3:0] => Display Clock Divider
								// D[7:4] => Oscillator Frequency
}

static void inline ssd1305set_area_color(unsigned char d)
{
	ssd1305_writeb_ctl(0xD8);	// Set Area Color Mode On/Off & Low Power Display Mode
	ssd1305_writeb_ctl(d);		// Default => 0x00 (Monochrome Mode & Normal Power Display Mode)
}

static void inline ssd1305set_precharge_period(unsigned char d)
{
	ssd1305_writeb_ctl(0xD9);	// Set Pre-Charge Period
	ssd1305_writeb_ctl(d);		// Default => 0x22 (2 Display Clocks [Phase 2] / 2 Display Clocks [Phase 1])
								// D[3:0] => Phase 1 Period in 1~15 Display Clocks
								// D[7:4] => Phase 2 Period in 1~15 Display Clocks
}

static void inline ssd1305set_common_config(unsigned char d)
{
	ssd1305_writeb_ctl(0xDA);	// Set COM Pins Hardware Configuration
	ssd1305_writeb_ctl(0x02|d);	// Default => 0x12 (0x10)
								// Alternative COM Pin Configuration
								// Disable COM Left/Right Re-Map
}

static void inline ssd1305set_vcomh(unsigned char d)
{
	ssd1305_writeb_ctl(0xDB);	// Set VCOMH Deselect Level
	ssd1305_writeb_ctl(d);		// Default => 0x34 (0.77*VCC)
}

static void inline ssd1305set_read_modify_write(unsigned char d)
{
	ssd1305_writeb_ctl(0xE0|d);	// Set Read Modify Write Mode
								// Default => 0xE0
								// 0xE0 (0x00) => Enter Read Modify Write
								// 0xEE (0x0E) => Exit Read Modify Write
}

static void inline ssd1305set_nop(void)
{
	ssd1305_writeb_ctl(0xE3);	// Command for No Operation
}

static void inline ssd1305set_lut(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
	ssd1305_writeb_ctl(0x91);	//   Define Look Up Table of Area Color
	ssd1305_writeb_ctl(a);		//   Define Bank 0 Pulse Width
	ssd1305_writeb_ctl(b);		//   Define Color A Pulse Width
	ssd1305_writeb_ctl(c);		//   Define Color B Pulse Width
	ssd1305_writeb_ctl(d);		//   Define Color C Pulse Width
}

static void inline ssd1305set_bank_color(void)
{
	ssd1305_writeb_ctl(0x92);	// Define Area Color for Bank 1~16 (Page 0)
	ssd1305_writeb_ctl(0x00);	//   Define Bank 1~4 as Color A
	ssd1305_writeb_ctl(0xEA);	//   Define Bank 5~7 as Color C
	ssd1305_writeb_ctl(0xAF);	//   Define Bank 8~10 as Color D
	ssd1305_writeb_ctl(0x56);	//   Define Bank 11~13 as Color C
								//   Define Bank 12~16 as Color B

	ssd1305_writeb_ctl(0x93);	// Define Area Color for Bank 17~32 (Page 1)
	ssd1305_writeb_ctl(0x00);	//   Define Bank 17~20 as Color A
	ssd1305_writeb_ctl(0xEA);	//   Define Bank 21~23 as Color C
	ssd1305_writeb_ctl(0xAF);	//   Define Bank 24~26 as Color D
	ssd1305_writeb_ctl(0x56);	//   Define Bank 27~29 as Color C
								//   Define Bank 30~32 as Color B
}

static void ssd1305_clear(void)
{
	unsigned char i, j;

	for (i = 0; i < SSD1305_PAGES; i++) {
		ssd1305set_start_page(i);
		ssd1305set_start_column(X_OFFSET);
		for (j = 0; j < SSD1305_ADDRESSES; j++)
			ssd1305_writebyte(0);
	}
}

static int ssd1305_gpio_init(void)
{
	int err = 0;

	err = gpio_request(pdata->gpios_res, "ssd1305_gpio");
#if defined(CONFIG_SSD1305_8080) || defined(CONFIG_SSD1305_6800)
	err += gpio_request(pdata->gpios_cs, "ssd1305_gpio");
	err += gpio_request(pdata->gpios_dc, "ssd1305_gpio");
	err += gpio_request(pdata->gpios_rd, "ssd1305_gpio");
	err += gpio_request(pdata->gpios_wr, "ssd1305_gpio");
	
	err += gpio_request(pdata->gpios_d0, "ssd1305_gpio");
	err += gpio_request(pdata->gpios_d1, "ssd1305_gpio");
	err += gpio_request(pdata->gpios_d2, "ssd1305_gpio");
	err += gpio_request(pdata->gpios_d3, "ssd1305_gpio");
	err += gpio_request(pdata->gpios_d4, "ssd1305_gpio");
	err += gpio_request(pdata->gpios_d5, "ssd1305_gpio");
	err += gpio_request(pdata->gpios_d6, "ssd1305_gpio");
	err += gpio_request(pdata->gpios_d7, "ssd1305_gpio");
#elif CONFIG_SSD1305_SPI3
	err += gpio_request(pdata->gpios_cs, "ssd1305_gpio");
	
	err += gpio_request(pdata->gpios_d0, "ssd1305_gpio");
	err += gpio_request(pdata->gpios_d1, "ssd1305_gpio");
#elif CONFIG_SSD1305_SPI4
	err += gpio_request(pdata->gpios_cs, "ssd1305_gpio");
	err += gpio_request(pdata->gpios_dc, "ssd1305_gpio");
	
	err += gpio_request(pdata->gpios_d0, "ssd1305_gpio");
	err += gpio_request(pdata->gpios_d1, "ssd1305_gpio");
#elif CONFIG_SSD1305_I2C
	err += gpio_request(pdata->gpios_dc, "ssd1305_gpio");
	
	err += gpio_request(pdata->gpios_d0, "ssd1305_gpio");
	err += gpio_request(pdata->gpios_d1, "ssd1305_gpio");
	err += gpio_request(pdata->gpios_d2, "ssd1305_gpio");
#endif
	if (err) {
		printk(KERN_ERR SSD1305_NAME "failed to request GPIO for ssd1305\n");
		return -EINVAL;
	}

	gpio_direction_output(pdata->gpios_res, 1);
#if defined(CONFIG_SSD1305_8080) || defined(CONFIG_SSD1305_6800)
	gpio_direction_output(pdata->gpios_cs, 1);
	gpio_direction_output(pdata->gpios_dc, 1);
	gpio_direction_output(pdata->gpios_rd, 1);
	gpio_direction_output(pdata->gpios_wr, 1);
	gpio_direction_output(pdata->gpios_d0, 1);
	gpio_direction_output(pdata->gpios_d1, 1);
	gpio_direction_output(pdata->gpios_d2, 1);
	gpio_direction_output(pdata->gpios_d3, 1);
	gpio_direction_output(pdata->gpios_d4, 1);
	gpio_direction_output(pdata->gpios_d5, 1);
	gpio_direction_output(pdata->gpios_d6, 1);
	gpio_direction_output(pdata->gpios_d7, 1);
#elif CONFIG_SSD1305_SPI3
	gpio_direction_output(pdata->gpios_cs, 1);
	gpio_direction_output(pdata->gpios_d0, 1);
	gpio_direction_output(pdata->gpios_d1, 1);
#elif CONFIG_SSD1305_SPI4
	gpio_direction_output(pdata->gpios_cs, 1);
	gpio_direction_output(pdata->gpios_dc, 1);
	gpio_direction_output(pdata->gpios_d0, 1);
	gpio_direction_output(pdata->gpios_d1, 1);
#elif CONFIG_SSD1305_I2C
	gpio_direction_output(pdata->gpios_dc, 1);
	gpio_direction_output(pdata->gpios_d0, 1);
	gpio_direction_output(pdata->gpios_d1, 1);
	gpio_direction_output(pdata->gpios_d2, 1);
#endif	
	return 0;
}

static void ssd1305_rest(void)
{
	gpio_set_value(pdata->gpios_res, 0);
	mdelay(2);
	gpio_set_value(pdata->gpios_res, 1);
}

static void ssd1305_hw_init(void)
{
	ssd1305_gpio_init();
	ssd1305_rest();
	
	ssd1305set_display_on_off(0x00);			// Display Off (0x00/0x01)
	ssd1305set_display_clock(0xA0);			// Set Clock as 80 Frames/Sec
	ssd1305set_multiplex_ratio(0x3F);			// 1/64 Duty (0x0F~0x3F)
	ssd1305set_display_offset(0x00);			// Shift Mapping RAM Counter (0x00~0x3F)
	ssd1305set_start_line(0x00);				// Set Mapping RAM Display Start Line (0x00~0x3F)
	ssd1305set_master_config(0x00);			// Disable Embedded DC/DC Converter (0x00/0x01)
	ssd1305set_area_color(0x05);				// Set Monochrome & Low Power Save Mode
	ssd1305set_addressing_mode(0x02);			// Set Page Addressing Mode (0x00/0x01/0x02)
	ssd1305set_segment_remap(0x01);			// Set SEG/Column Mapping (0x00/0x01)
	ssd1305set_common_remap(0x08);				// Set COM/Row Scan Direction (0x00/0x08)
	ssd1305set_common_config(0x10);			// Set Alternative Configuration (0x00/0x10)
	ssd1305set_lut(0x3F,0x3F,0x3F,0x3F);		// Define All Banks Pulse Width as 64 Clocks
	ssd1305set_contrast_control(brightness);	// Set SEG Output Current
	ssd1305set_area_brightness(brightness);	// Set Brightness for Area Color Banks
	ssd1305set_precharge_period(0xD2);			// Set Pre-Charge as 13 Clocks & Discharge as 2 Clock
	ssd1305set_vcomh(0x34);					// Set VCOMH Deselect Level
	ssd1305set_entire_display(0x00);			// Disable Entire Display On (0x00/0x01)
	ssd1305set_inverse_display(0x00);			// Disable Inverse Display On (0x00/0x01)

	ssd1305set_display_on_off(0x01);			// Display On (0x00/0x01)
}

/*
 * Module Parameters
 */
static unsigned int ssd1305_rate = CONFIG_SSD1305_RATE;
module_param(ssd1305_rate, uint, S_IRUGO);
MODULE_PARM_DESC(ssd1305_rate,
	"Refresh rate (hertz)");

static unsigned int ssd1305_getrate(void)
{
	return ssd1305_rate;
}

/*
 * Update work
 */
unsigned char *ssd1305_buffer;
static unsigned char *ssd1305_cache;
static DEFINE_MUTEX(ssd1305_mutex);
static unsigned char ssd1305_updating;
static void ssd1305_update(struct work_struct *delayed_work);
static struct workqueue_struct *ssd1305_workqueue;
static DECLARE_DELAYED_WORK(ssd1305_work, ssd1305_update);

static void ssd1305_queue(void)
{
	queue_delayed_work(ssd1305_workqueue, &ssd1305_work,
		HZ / ssd1305_rate);
}

static unsigned char ssd1305_enable(void)
{
	unsigned char ret;

	mutex_lock(&ssd1305_mutex);

	if (!ssd1305_updating) {
		ssd1305_updating = 1;
		ssd1305_queue();
		ret = 0;
	} else
		ret = 1;

	mutex_unlock(&ssd1305_mutex);

	return ret;
}

static void ssd1305_disable(void)
{
	mutex_lock(&ssd1305_mutex);

	if (ssd1305_updating) {
		ssd1305_updating = 0;
		cancel_delayed_work(&ssd1305_work);
		flush_workqueue(ssd1305_workqueue);
	}

	mutex_unlock(&ssd1305_mutex);
}

static unsigned char ssd1305_isenabled(void)
{
	return ssd1305_updating;
}

static void ssd1305_update(struct work_struct *work)
{
	unsigned char c;
	unsigned short i, j, k, b;

	if (memcmp(ssd1305_cache, ssd1305_buffer, SSD1305_SIZE)) {
		for (i = 0; i < SSD1305_CONTROLLERS; i++) {
			for (j = 0; j < SSD1305_PAGES; j++) {
				ssd1305set_start_page(j);
				ssd1305set_start_column(X_OFFSET);
				for (k = 0; k < SSD1305_ADDRESSES; k++) {
					for (c = 0, b = 0; b < 8; b++)
						if (ssd1305_buffer
							[i * SSD1305_ADDRESSES / 8
							+ k / 8 + (j * 8 + b) *
							SSD1305_WIDTH / 8]
							& bit(k % 8))
							c |= bit(b);
					ssd1305_writebyte(c);
				}
			}
		}

		memcpy(ssd1305_cache, ssd1305_buffer, SSD1305_SIZE);
	}

	if (ssd1305_updating)
		ssd1305_queue();
}

/*
 * ssd1305 Exported Symbols
 */
EXPORT_SYMBOL_GPL(ssd1305_buffer);
EXPORT_SYMBOL_GPL(ssd1305_getrate);
EXPORT_SYMBOL_GPL(ssd1305_enable);
EXPORT_SYMBOL_GPL(ssd1305_disable);
EXPORT_SYMBOL_GPL(ssd1305_isenabled);

/*
 * Module Init & Exit
 */
static int ssd1305_init(void)
{
	int ret = -EINVAL;

	ssd1305_buffer = (unsigned char *) get_zeroed_page(GFP_KERNEL);
	if (ssd1305_buffer == NULL) {
		printk(KERN_ERR SSD1305_NAME ": ERROR: "
			"can't get a free page\n");
		ret = -ENOMEM;
		goto none;
	}

	ssd1305_cache = kmalloc(sizeof(unsigned char) *
		SSD1305_SIZE, GFP_KERNEL);
	if (ssd1305_cache == NULL) {
		printk(KERN_ERR SSD1305_NAME ": ERROR: "
			"can't alloc cache buffer (%i bytes)\n",
			SSD1305_SIZE);
		ret = -ENOMEM;
		goto bufferalloced;
	}

	ssd1305_workqueue = create_singlethread_workqueue(SSD1305_NAME);
	if (ssd1305_workqueue == NULL)
		goto cachealloced;

	return 0;

cachealloced:
	kfree(ssd1305_cache);

bufferalloced:
	free_page((unsigned long) ssd1305_buffer);

none:
	return ret;
}

static void ssd1305_exit(void)
{
	ssd1305_disable();
	ssd1305set_display_on_off(0x00);
	destroy_workqueue(ssd1305_workqueue);
	kfree(ssd1305_cache);
	free_page((unsigned long) ssd1305_buffer);
}


static int ssd1305fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	return vm_insert_page(vma, vma->vm_start,
		virt_to_page(ssd1305_buffer));
}

static struct fb_ops ssd1305fb_ops = {
	.owner = THIS_MODULE,
	.fb_read = fb_sys_read,
	.fb_write = fb_sys_write,
	.fb_fillrect = sys_fillrect,
	.fb_copyarea = sys_copyarea,
	.fb_imageblit = sys_imageblit,
	.fb_mmap = ssd1305fb_mmap,
};

static int __devinit ssd1305fb_probe(struct platform_device *device)
{
	int ret = -EINVAL;
 	struct fb_info *info = framebuffer_alloc(0, &device->dev);

	if (!info)
		goto none;

	pdata = device->dev.platform_data;
	if (!pdata) {
		dev_err(&device->dev, "no platform data defined\n");
		goto fballoced;
	}
	reg_addr = ioremap(pdata->gpio_outpu, 0x4);

	info->screen_base = (char __iomem *) ssd1305_buffer;
	info->screen_size = SSD1305_SIZE;
	info->fbops = &ssd1305fb_ops;
	info->fix = ssd1305fb_fix;
	info->var = ssd1305fb_var;
	info->pseudo_palette = NULL;
	info->par = NULL;
	info->flags = FBINFO_FLAG_DEFAULT;

	if (register_framebuffer(info) < 0)
		goto fballoced;

	platform_set_drvdata(device, info);

	printk(KERN_INFO "fb%d: %s frame buffer device\n", info->node,
		info->fix.id);

	ssd1305_hw_init();
	ssd1305_clear();

	return 0;

fballoced:
	framebuffer_release(info);

none:
	return ret;
}

static int __devexit ssd1305fb_remove(struct platform_device *device)
{
	struct fb_info *info = platform_get_drvdata(device);

	if (info) {
		unregister_framebuffer(info);
		framebuffer_release(info);
	}

	return 0;
}

static struct platform_driver ssd1305fb_driver = {
	.probe	= ssd1305fb_probe,
	.remove = __devexit_p(ssd1305fb_remove),
	.driver = {
		.name	= SSD1305FB_NAME,
	},
};

static int __init ssd1305fb_init(void)
{
	int ret = -EINVAL;

	/* ssd1305_init() must be called first */
	if (ssd1305_init()) {
		printk(KERN_ERR SSD1305FB_NAME ": ERROR: "
			"ssd1305 is not initialized\n");
		goto none;
	}

	if (ssd1305_enable()) {
		printk(KERN_ERR SSD1305FB_NAME ": ERROR: "
			"can't enable ssd1305 refreshing (being used)\n");
		return -ENODEV;
	}

	ret = platform_driver_register(&ssd1305fb_driver);

none:
	return ret;
}

static void __exit ssd1305fb_exit(void)
{
	platform_driver_unregister(&ssd1305fb_driver);
	ssd1305_exit();
}

module_init(ssd1305fb_init);
module_exit(ssd1305fb_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tanghaifeng <rou.ru.cai.hong@gmail.com>");
MODULE_DESCRIPTION("ssd1305 LCD framebuffer driver");
