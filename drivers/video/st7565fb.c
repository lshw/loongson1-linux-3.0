/*
 * Driver for the Sitronix ST7565 Dot Matrix LCD controler
 *
 * Copyright (c) 2013 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 *
 * Licensed under the GPLv2 or later.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/fb.h>
#include <linux/uaccess.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/st7565.h>

#define X_OFFSET		0
#define ST7565FB_WIDTH			128
#define ST7565FB_HEIGHT		64

#define ST7565FB_DATA			1
#define ST7565FB_COMMAND		0

#define ST7565FB_CONTRAST		0x81
#define SSD1307FB_ADC_NOR		0xa0
#define SSD1307FB_ADC_REV		0xa1
#define SSD1307FB_RESET			0xe2
#define ST7565FB_LCD_19BIAS		0xa2
#define ST7565FB_LCD_17BIAS		0xa3
#define ST7565FB_DISPLAY_ALL_NOR		0xa4
#define ST7565FB_DISPLAY_ALL_REV		0xa5
#define ST7565FB_DISPLAY_NOR		0xa6
#define ST7565FB_DISPLAY_REV		0xa7
#define ST7565FB_DISPLAY_OFF		0xae
#define ST7565FB_DISPLAY_ON		0xaf
#define ST7565FB_SCAN_NOR		0xc0
#define ST7565FB_SCAN_REV		0xc8
#define ST7565FB_START_PAGE_ADDRESS	0xb0

struct st7565fb_par {
	struct st7565_platform_data *pdata;
	struct fb_info *info;
	void __iomem *reg_addr;
};

static struct fb_fix_screeninfo st7565fb_fix = {
	.id		= "Sitronix ST7565",
	.type		= FB_TYPE_PACKED_PIXELS,
	.visual		= FB_VISUAL_MONO10,
	.xpanstep	= 0,
	.ypanstep	= 0,
	.ywrapstep	= 0,
	.line_length	= ST7565FB_WIDTH / 8,
	.accel		= FB_ACCEL_NONE,
};

static struct fb_var_screeninfo st7565fb_var = {
	.xres		= ST7565FB_WIDTH,
	.yres		= ST7565FB_HEIGHT,
	.xres_virtual	= ST7565FB_WIDTH,
	.yres_virtual	= ST7565FB_HEIGHT,
	.bits_per_pixel	= 1,
};

static int st7565fb_write_array(struct st7565fb_par *par, u8 type, u8 *cmd, u32 len)
{
	struct st7565_platform_data *pdata = par->pdata;
#ifdef CONFIG_ST7565_8080
	u32 ret;
	
	gpio_set_value(pdata->gpios_cs, 0);
	gpio_set_value(pdata->gpios_dc, type);
	gpio_set_value(pdata->gpios_rd, 1);
	gpio_set_value(pdata->gpios_wr, 0);

	ret = readl(par->reg_addr);
	ret &= ~(0xFF << pdata->datas_offset);
	writel(ret | ((*cmd) << pdata->datas_offset), par->reg_addr);

	gpio_set_value(pdata->gpios_cs, 1);
	gpio_set_value(pdata->gpios_dc, 1);
	gpio_set_value(pdata->gpios_wr, 1);
#elif CONFIG_ST7565_6800
	u32 ret;
	
	gpio_set_value(pdata->gpios_cs, 0);
	gpio_set_value(pdata->gpios_dc, 0);
	gpio_set_value(pdata->gpios_wr, 0);	//RW
	gpio_set_value(pdata->gpios_rd, 1);	//E

	ret = readl(par->reg_addr);
	ret &= ~(0xFF << pdata->datas_offset);
	writel(ret | ((*cmd) << pdata->datas_offset), par->reg_addr);

	gpio_set_value(pdata->gpios_rd, 0);	//E
	gpio_set_value(pdata->gpios_cs, 1);
	gpio_set_value(pdata->gpios_dc, 1);
	gpio_set_value(pdata->gpios_wr, 1);	//RW
#elif CONFIG_ST7565_SPI3
	unsigned char i;
	unsigned char data = *cmd;

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
#elif CONFIG_ST7565_SPI4
	unsigned char i;
	unsigned char data = *cmd;

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
#elif CONFIG_ST7565_I2C

#endif
	return 0;
}

static inline int st7565fb_write_cmd_array(struct st7565fb_par *par, u8 *cmd, u32 len)
{
	return st7565fb_write_array(par, ST7565FB_COMMAND, cmd, len);
}

static inline int st7565fb_write_cmd(struct st7565fb_par *par, u8 cmd)
{
	return st7565fb_write_cmd_array(par, &cmd, 1);
}

static inline int st7565fb_write_data_array(struct st7565fb_par *par, u8 *cmd, u32 len)
{
	return st7565fb_write_array(par, ST7565FB_DATA, cmd, len);
}

static inline int st7565fb_write_data(struct st7565fb_par *par, u8 data)
{
	return st7565fb_write_data_array(par, &data, 1);
}

static void st7565fb_update_display(struct st7565fb_par *par)
{
	u8 *vmem = par->info->screen_base;
	int i, j, k;

	/*
	 * The screen is divided in pages, each having a height of 8
	 * pixels, and the width of the screen. When sending a byte of
	 * data to the controller, it gives the 8 bits for the current
	 * column. I.e, the first byte are the 8 bits of the first
	 * column, then the 8 bits for the second column, etc.
	 *
	 *
	 * Representation of the screen, assuming it is 5 bits
	 * wide. Each letter-number combination is a bit that controls
	 * one pixel.
	 *
	 * A0 A1 A2 A3 A4
	 * B0 B1 B2 B3 B4
	 * C0 C1 C2 C3 C4
	 * D0 D1 D2 D3 D4
	 * E0 E1 E2 E3 E4
	 * F0 F1 F2 F3 F4
	 * G0 G1 G2 G3 G4
	 * H0 H1 H2 H3 H4
	 *
	 * If you want to update this screen, you need to send 5 bytes:
	 *  (1) A0 B0 C0 D0 E0 F0 G0 H0
	 *  (2) A1 B1 C1 D1 E1 F1 G1 H1
	 *  (3) A2 B2 C2 D2 E2 F2 G2 H2
	 *  (4) A3 B3 C3 D3 E3 F3 G3 H3
	 *  (5) A4 B4 C4 D4 E4 F4 G4 H4
	 */

	for (i = 0; i < (ST7565FB_HEIGHT / 8); i++) {
		st7565fb_write_cmd(par, ST7565FB_START_PAGE_ADDRESS + i);
		st7565fb_write_cmd(par, 0x00+X_OFFSET%16);
		st7565fb_write_cmd(par, 0x10+X_OFFSET/16);

		for (j = 0; j < ST7565FB_WIDTH; j++) {
			u8 buf = 0;
			for (k = 0; k < 8; k++) {
				u32 page_length = ST7565FB_WIDTH * i;
				u32 index = page_length + (ST7565FB_WIDTH * k + j) / 8;
				u8 byte = *(vmem + index);
				u8 bit = byte & (1 << (j % 8));
				bit = bit >> (j % 8);
				buf |= bit << k;
			}
			st7565fb_write_data(par, buf);
		}
	}
}


static ssize_t st7565fb_write(struct fb_info *info, const char __user *buf,
		size_t count, loff_t *ppos)
{
	struct st7565fb_par *par = info->par;
	unsigned long total_size;
	unsigned long p = *ppos;
	u8 __iomem *dst;

	total_size = info->fix.smem_len;

	if (p > total_size)
		return -EINVAL;

	if (count + p > total_size)
		count = total_size - p;

	if (!count)
		return -EINVAL;

	dst = (void __force *) (info->screen_base + p);

	if (copy_from_user(dst, buf, count))
		return -EFAULT;

	st7565fb_update_display(par);

	*ppos += count;

	return count;
}

static void st7565fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	struct st7565fb_par *par = info->par;
	sys_fillrect(info, rect);
	st7565fb_update_display(par);
}

static void st7565fb_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
	struct st7565fb_par *par = info->par;
	sys_copyarea(info, area);
	st7565fb_update_display(par);
}

static void st7565fb_imageblit(struct fb_info *info, const struct fb_image *image)
{
	struct st7565fb_par *par = info->par;
	sys_imageblit(info, image);
	st7565fb_update_display(par);
}

static struct fb_ops st7565fb_ops = {
	.owner		= THIS_MODULE,
	.fb_read	= fb_sys_read,
	.fb_write	= st7565fb_write,
	.fb_fillrect	= st7565fb_fillrect,
	.fb_copyarea	= st7565fb_copyarea,
	.fb_imageblit	= st7565fb_imageblit,
};

static void st7565fb_deferred_io(struct fb_info *info,
				struct list_head *pagelist)
{
	st7565fb_update_display(info->par);
}

static struct fb_deferred_io st7565fb_defio = {
	.delay		= HZ,
	.deferred_io	= st7565fb_deferred_io,
};


static int st7565fb_gpio_init(struct st7565_platform_data *pdata)
{
	int err = 0;

	err = gpio_request(pdata->gpios_res, "st7565fb_gpio");
#if defined(CONFIG_ST7565_8080) || defined(CONFIG_ST7565_6800)
	err += gpio_request(pdata->gpios_cs, "st7565fb_gpio");
	err += gpio_request(pdata->gpios_dc, "st7565fb_gpio");
	err += gpio_request(pdata->gpios_rd, "st7565fb_gpio");
	err += gpio_request(pdata->gpios_wr, "st7565fb_gpio");
	
	err += gpio_request(pdata->gpios_d0, "st7565fb_gpio");
	err += gpio_request(pdata->gpios_d1, "st7565fb_gpio");
	err += gpio_request(pdata->gpios_d2, "st7565fb_gpio");
	err += gpio_request(pdata->gpios_d3, "st7565fb_gpio");
	err += gpio_request(pdata->gpios_d4, "st7565fb_gpio");
	err += gpio_request(pdata->gpios_d5, "st7565fb_gpio");
	err += gpio_request(pdata->gpios_d6, "st7565fb_gpio");
	err += gpio_request(pdata->gpios_d7, "st7565fb_gpio");
#elif CONFIG_ST7565_SPI3
	err += gpio_request(pdata->gpios_cs, "st7565fb_gpio");
	
	err += gpio_request(pdata->gpios_d0, "st7565fb_gpio");
	err += gpio_request(pdata->gpios_d1, "st7565fb_gpio");
#elif CONFIG_ST7565_SPI4
	err += gpio_request(pdata->gpios_cs, "st7565fb_gpio");
	err += gpio_request(pdata->gpios_dc, "st7565fb_gpio");
	
	err += gpio_request(pdata->gpios_d0, "st7565fb_gpio");
	err += gpio_request(pdata->gpios_d1, "st7565fb_gpio");
#elif CONFIG_ST7565_I2C
	err += gpio_request(pdata->gpios_dc, "st7565fb_gpio");
	
	err += gpio_request(pdata->gpios_d0, "st7565fb_gpio");
	err += gpio_request(pdata->gpios_d1, "st7565fb_gpio");
	err += gpio_request(pdata->gpios_d2, "st7565fb_gpio");
#endif
	if (err) {
		printk(KERN_ERR "failed to request GPIO for st7565\n");
		return -EINVAL;
	}

	gpio_direction_output(pdata->gpios_res, 1);
#if defined(CONFIG_ST7565_8080) || defined(CONFIG_ST7565_6800)
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
#elif CONFIG_ST7565_SPI3
	gpio_direction_output(pdata->gpios_cs, 1);
	gpio_direction_output(pdata->gpios_d0, 1);
	gpio_direction_output(pdata->gpios_d1, 1);
#elif CONFIG_ST7565_SPI4
	gpio_direction_output(pdata->gpios_cs, 1);
	gpio_direction_output(pdata->gpios_dc, 1);
	gpio_direction_output(pdata->gpios_d0, 1);
	gpio_direction_output(pdata->gpios_d1, 1);
#elif CONFIG_ST7565_I2C
	gpio_direction_output(pdata->gpios_dc, 1);
	gpio_direction_output(pdata->gpios_d0, 1);
	gpio_direction_output(pdata->gpios_d1, 1);
	gpio_direction_output(pdata->gpios_d2, 1);
#endif	
	return 0;
}

static void st7565fb_hw_init(struct st7565fb_par *par)
{
	struct st7565_platform_data *pdata = par->pdata;

	st7565fb_gpio_init(pdata);
	/* Reset the screen */
	gpio_set_value(pdata->gpios_res, 0);
	mdelay(2);
	gpio_set_value(pdata->gpios_res, 1);

	st7565fb_write_cmd(par, SSD1307FB_RESET);	//寄存器复位
	udelay(1000);

	/* 设置ADC和SCAN的模式可以对显示进行旋转 */
	/* Sets the display RAM address SEG output correspondence */
	st7565fb_write_cmd(par, SSD1307FB_ADC_NOR);
	/* Common output mode select */
	st7565fb_write_cmd(par, ST7565FB_SCAN_REV);

	/* Sets the LCD drive voltage bias ratio */
	st7565fb_write_cmd(par, ST7565FB_LCD_19BIAS);
	/* Select internal resistor ratio(Rb/Ra) mode */
	st7565fb_write_cmd(par, 0x24);
	/* Select internal power supply operating mode */
	st7565fb_write_cmd(par, 0x2f);
	/* Set the V0 output voltage electronic volume register */
	st7565fb_write_cmd(par, 0x81);
	st7565fb_write_cmd(par, 0x3f);
	/* Display start line set st7565r */
	st7565fb_write_cmd(par, 0x60);
	/* Booster ratio set */
	st7565fb_write_cmd(par, 0xf8);
	st7565fb_write_cmd(par, 0x03);
	/* Sets the LCD display normal/ reverse */
	st7565fb_write_cmd(par, ST7565FB_DISPLAY_NOR);
	st7565fb_write_cmd(par, ST7565FB_DISPLAY_ALL_NOR);
	st7565fb_write_cmd(par, ST7565FB_DISPLAY_ON);
}

static void st7565fb_hw_exit(struct st7565fb_par *par)
{
	st7565fb_write_cmd(par, ST7565FB_DISPLAY_OFF);
	gpio_free(par->pdata->gpios_res);
	gpio_free(par->pdata->gpios_cs);
	gpio_free(par->pdata->gpios_dc);
	gpio_free(par->pdata->gpios_rd);
	gpio_free(par->pdata->gpios_wr);
	gpio_free(par->pdata->gpios_d0);
	gpio_free(par->pdata->gpios_d1);
	gpio_free(par->pdata->gpios_d2);
	gpio_free(par->pdata->gpios_d3);
	gpio_free(par->pdata->gpios_d4);
	gpio_free(par->pdata->gpios_d5);
	gpio_free(par->pdata->gpios_d6);
	gpio_free(par->pdata->gpios_d7);
}

static int st7565fb_probe(struct platform_device *device)
{
	struct fb_info *info;
	u32 vmem_size = roundup((ST7565FB_WIDTH * ST7565FB_HEIGHT / 8), PAGE_SIZE);
	struct st7565fb_par *par;
	u8 *vmem;
	int ret = 0;

	info = framebuffer_alloc(sizeof(struct st7565fb_par), &device->dev);
	if (!info) {
		dev_err(&device->dev, "Couldn't allocate framebuffer.\n");
		return -ENOMEM;
	}

	vmem = vzalloc(vmem_size);
	if (!vmem) {
		dev_err(&device->dev, "Couldn't allocate graphical memory.\n");
		ret = -ENOMEM;
		goto fb_alloc_error;
	}

	info->fbops = &st7565fb_ops;
	info->fix = st7565fb_fix;
	info->fbdefio = &st7565fb_defio;

	info->var = st7565fb_var;
	info->var.red.length = 1;
	info->var.red.offset = 0;
	info->var.green.length = 1;
	info->var.green.offset = 0;
	info->var.blue.length = 1;
	info->var.blue.offset = 0;

	info->screen_base = (u8 __force __iomem *)vmem;
	info->fix.smem_start = (unsigned long)vmem;
	info->fix.smem_len = vmem_size;

	fb_deferred_io_init(info);

	par = info->par;
	par->info = info;

	par->pdata = device->dev.platform_data;
	if (!par->pdata) {
		dev_err(&device->dev, "no platform data defined\n");
		goto reset_oled_error;
	}
	par->reg_addr = ioremap(par->pdata->gpio_outpu, 0x4);

	st7565fb_hw_init(par);

	ret = register_framebuffer(info);
	if (ret) {
		dev_err(&device->dev, "Couldn't register the framebuffer\n");
		goto reset_oled_error;
	}

	platform_set_drvdata(device, info);

	dev_info(&device->dev, "fb%d: %s framebuffer device registered, using %d bytes of video memory\n", info->node, info->fix.id, vmem_size);

	return 0;

reset_oled_error:
	iounmap(par->reg_addr);
	fb_deferred_io_cleanup(info);
fb_alloc_error:
	framebuffer_release(info);
	return ret;
}

static int st7565fb_remove(struct platform_device *device)
{
	struct fb_info *info = platform_get_drvdata(device);
	struct st7565fb_par *par = info->par;

	st7565fb_hw_exit(par);
	unregister_framebuffer(info);
	iounmap(par->reg_addr);
	fb_deferred_io_cleanup(info);
	framebuffer_release(info);

	return 0;
}

static struct platform_driver st7565fb_driver = {
	.probe = st7565fb_probe,
	.remove = st7565fb_remove,
	.driver = {
		.name = "st7565fb",
		.owner = THIS_MODULE,
	},
};

static int __init st7565fb_init(void)
{
	return platform_driver_register(&st7565fb_driver);
}

static void __exit st7565fb_exit(void)
{
	platform_driver_unregister(&st7565fb_driver);
}

module_init(st7565fb_init);
module_exit(st7565fb_exit);

MODULE_DESCRIPTION("FB driver for the Sitronix ST7565 Dot Matrix LCD controler");
MODULE_AUTHOR("Haifeng,Tang <tanghaifeng-gz@loongson.cn>");
MODULE_LICENSE("GPL");
