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
#include <linux/st7920.h>


#define ST7920FB_NAME "st7920fb"
#define ST7920_NAME "st7920"

/* st7920 chipset specific defines and code */
#define brightness	0x2F

#define X_OFFSET	(2)
#define ST7920_WIDTH	(128)
#define ST7920_HEIGHT	(64)
#define ST7920_CONTROLLERS	(1)
#define ST7920_PAGES	(8)
#define ST7920_ADDRESSES	(128)
#define ST7920_SIZE		((ST7920_CONTROLLERS) * \
				(ST7920_PAGES) * \
				(ST7920_ADDRESSES))
#define bit(n) (((unsigned char)1)<<(n))

static struct st7920_platform_data *pdata;
void __iomem *reg_addr;

static struct fb_fix_screeninfo st7920fb_fix __devinitdata = {
	.id = ST7920FB_NAME,
	.type = FB_TYPE_PACKED_PIXELS,
	.visual = FB_VISUAL_MONO10,
	.xpanstep = 0,
	.ypanstep = 0,
	.ywrapstep = 0,
	.line_length = ST7920_WIDTH / 8,
	.accel = FB_ACCEL_NONE,
};

static struct fb_var_screeninfo st7920fb_var __devinitdata = {
	.xres = ST7920_WIDTH,
	.yres = ST7920_HEIGHT,
	.xres_virtual = ST7920_WIDTH,
	.yres_virtual = ST7920_HEIGHT,
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

unsigned char reverse8(unsigned char c)
{
    c = (c&0x55)<<1 | (c&0xAA)>>1;
    c = (c&0x33)<<2 | (c&0xCC)>>2;
    c = (c&0x0F)<<4 | (c&0xF0)>>4;
    return c;
}

static void st7920_sent(unsigned char value)
{
	unsigned char i;
	unsigned char data = value;

//	gpio_set_value(pdata->gpios_cs, 1);

	for (i=0; i<8; i++) {
		gpio_set_value(pdata->gpios_sck, 0);//SCLK=0
		gpio_set_value(pdata->gpios_sid, (data&0x80)>>7);
		data = data << 1;
		gpio_set_value(pdata->gpios_sck, 1);//SCLK=1
	}
//	gpio_set_value(pdata->gpios_cs, 0);
}

static void st7920_write_ctrl(unsigned char ctrl)
{
	st7920_sent(0xF8);				//发送起始信号
	udelay(600);					//延时是必须的
	st7920_sent(ctrl & 0xF0);		//发送高四位
	st7920_sent((ctrl<<4) & 0xF0);	//发送低四位
}

static void st7920_write_data(unsigned char data)
{
	st7920_sent(0xFA);				//发送起始信号
	udelay(600);					//延时是必须的
	st7920_sent(data & 0xF0);		//发送高四位
	st7920_sent((data<<4) & 0xF0);	//发送低四位
}

/*
 * st7920 Internal Commands
 */

static void inline display_clear(void)
{
	st7920_write_ctrl(0x01);
}

static void inline return_home(void)
{
	st7920_write_ctrl(0x02);
//	st7920_write_ctrl(0x03);
}

static void inline entry_mode_set(unsigned char d)
{
	st7920_write_ctrl(0x04 | d);
}

static void inline display_ctrl(unsigned char d)
{
	st7920_write_ctrl(0x08 | d);
}

static void inline cursor_display_ctrl(unsigned char d)
{
	st7920_write_ctrl(0x10 | d);
}

static void inline function_set(unsigned char d)
{
	st7920_write_ctrl(0x20 | d);
}

static void inline set_cgram_addr(unsigned char d)
{
	st7920_write_ctrl(0x40 | d);
}

static void inline set_ddram_addr(unsigned char d)
{
	st7920_write_ctrl(0x80 | d);
}

/* 扩充命令 */
static void inline standby(void)
{
	st7920_write_ctrl(0x01);
}

static void inline scroll_or_ram_addr_select(unsigned char d)
{
	st7920_write_ctrl(0x02 | d);
}

static void inline reverse(unsigned char d)
{
	st7920_write_ctrl(0x04 | d);
}

static void inline extended_function_set(unsigned char d)
{
	st7920_write_ctrl(0x20 | d);
}

static void inline set_scroll_addr(unsigned char d)
{
	st7920_write_ctrl(0x40 | d);
}

static void inline set_graphic_display_ram_addr(unsigned char d)
{
	st7920_write_ctrl(0x80 | d);
}

static void st7920_clear(void)
{
	unsigned char i, j;

	function_set(0x34);		 //先关闭图形显示功能
	for (i = 0; i < 32; i++) {
		set_graphic_display_ram_addr(i);
		set_graphic_display_ram_addr(0x80);
		for (j = 0; j < 8; j++) {
			st7920_write_data(0xFF);
			st7920_write_data(0xFF);
		}
	}
	for (i = 0; i < 32; i++) {
		set_graphic_display_ram_addr(i);
		set_graphic_display_ram_addr(0x88);
		for (j = 0; j < 8; j++) {
			st7920_write_data(0xFF);
			st7920_write_data(0xFF);
		}
	}
	function_set(0x36);		//最后打开图形显示功能
}

static int st7920_gpio_init(void)
{
	int err = 0;

	err = gpio_request(pdata->gpios_res, "st7920_gpio");
	err += gpio_request(pdata->gpios_cs, "st7920_gpio");
	err += gpio_request(pdata->gpios_sid, "st7920_gpio");
	err += gpio_request(pdata->gpios_sck, "st7920_gpio");
	if (err) {
		printk(KERN_ERR ST7920_NAME "failed to request GPIO for st7920\n");
		return -EINVAL;
	}

	gpio_direction_output(pdata->gpios_res, 0);
	gpio_direction_output(pdata->gpios_cs, 0);
	gpio_direction_output(pdata->gpios_sid, 0);
	gpio_direction_output(pdata->gpios_sck, 0);
	return 0;
}

static void st7920_rest(void)
{
	gpio_set_value(pdata->gpios_res, 0);
	mdelay(42);
	gpio_set_value(pdata->gpios_res, 1);
}

static void st7920_hw_init(void)
{
	st7920_gpio_init();
	st7920_rest();

	function_set(0x30);
	display_ctrl(0x0C);
	display_clear();
	return_home();
	set_ddram_addr(0x80);
	st7920_clear();
//	function_set(0x36);	//打开图形显示功能
}

/*
 * Module Parameters
 */
static unsigned int st7920_rate = CONFIG_ST7920_RATE;
module_param(st7920_rate, uint, S_IRUGO);
MODULE_PARM_DESC(st7920_rate,
	"Refresh rate (hertz)");

static unsigned int st7920_getrate(void)
{
	return st7920_rate;
}

/*
 * Update work
 */
unsigned char *st7920_buffer;
static unsigned char *st7920_cache;
static DEFINE_MUTEX(st7920_mutex);
static unsigned char st7920_updating;
static void st7920_update(struct work_struct *delayed_work);
static struct workqueue_struct *st7920_workqueue;
static DECLARE_DELAYED_WORK(st7920_work, st7920_update);

static void st7920_queue(void)
{
	queue_delayed_work(st7920_workqueue, &st7920_work,
		HZ / st7920_rate);
}

static unsigned char st7920_enable(void)
{
	unsigned char ret;

	mutex_lock(&st7920_mutex);

	if (!st7920_updating) {
		st7920_updating = 1;
		st7920_queue();
		ret = 0;
	} else
		ret = 1;

	mutex_unlock(&st7920_mutex);

	return ret;
}

static void st7920_disable(void)
{
	mutex_lock(&st7920_mutex);

	if (st7920_updating) {
		st7920_updating = 0;
		cancel_delayed_work(&st7920_work);
		flush_workqueue(st7920_workqueue);
	}

	mutex_unlock(&st7920_mutex);
}

static unsigned char st7920_isenabled(void)
{
	return st7920_updating;
}

static void st7920_update(struct work_struct *work)
{
	unsigned short i, j;
	unsigned char *buffer = st7920_buffer;

	if (memcmp(st7920_cache, st7920_buffer, ST7920_SIZE)) {
//		function_set(0x34);		 //先关闭图形显示功能
		for (i = 0; i < 32; i++) {
			set_graphic_display_ram_addr(i);
			set_graphic_display_ram_addr(0x80);
			for (j = 0; j < 8; j++) {
				st7920_write_data(reverse8(*(buffer++)));
				st7920_write_data(reverse8(*(buffer++)));
			}
		}
		for (i = 0; i < 32; i++) {
			set_graphic_display_ram_addr(i);
			set_graphic_display_ram_addr(0x88);
			for (j = 0; j < 8; j++) {
				st7920_write_data(reverse8(*(buffer++)));
				st7920_write_data(reverse8(*(buffer++)));
			}
		}
//		function_set(0x36);		//最后打开图形显示功能

		memcpy(st7920_cache, st7920_buffer, ST7920_SIZE);
	}

	if (st7920_updating)
		st7920_queue();
}

/*
 * st7920 Exported Symbols
 */
EXPORT_SYMBOL_GPL(st7920_buffer);
EXPORT_SYMBOL_GPL(st7920_getrate);
EXPORT_SYMBOL_GPL(st7920_enable);
EXPORT_SYMBOL_GPL(st7920_disable);
EXPORT_SYMBOL_GPL(st7920_isenabled);

/*
 * Module Init & Exit
 */
static int st7920_init(void)
{
	int ret = -EINVAL;

	st7920_buffer = (unsigned char *) get_zeroed_page(GFP_KERNEL);
	if (st7920_buffer == NULL) {
		printk(KERN_ERR ST7920_NAME ": ERROR: "
			"can't get a free page\n");
		ret = -ENOMEM;
		goto none;
	}

	st7920_cache = kmalloc(sizeof(unsigned char) *
		ST7920_SIZE, GFP_KERNEL);
	if (st7920_cache == NULL) {
		printk(KERN_ERR ST7920_NAME ": ERROR: "
			"can't alloc cache buffer (%i bytes)\n",
			ST7920_SIZE);
		ret = -ENOMEM;
		goto bufferalloced;
	}

	st7920_workqueue = create_singlethread_workqueue(ST7920_NAME);
	if (st7920_workqueue == NULL)
		goto cachealloced;

	return 0;

cachealloced:
	kfree(st7920_cache);

bufferalloced:
	free_page((unsigned long) st7920_buffer);

none:
	return ret;
}

static void st7920_exit(void)
{
	st7920_disable();
	function_set(0x30);
	display_ctrl(0x08);
	destroy_workqueue(st7920_workqueue);
	kfree(st7920_cache);
	free_page((unsigned long) st7920_buffer);
}


static int st7920fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	return vm_insert_page(vma, vma->vm_start,
		virt_to_page(st7920_buffer));
}

static struct fb_ops st7920fb_ops = {
	.owner = THIS_MODULE,
	.fb_read = fb_sys_read,
	.fb_write = fb_sys_write,
	.fb_fillrect = sys_fillrect,
	.fb_copyarea = sys_copyarea,
	.fb_imageblit = sys_imageblit,
	.fb_mmap = st7920fb_mmap,
};

static int __devinit st7920fb_probe(struct platform_device *device)
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

	info->screen_base = (char __iomem *) st7920_buffer;
	info->screen_size = ST7920_SIZE;
	info->fbops = &st7920fb_ops;
	info->fix = st7920fb_fix;
	info->var = st7920fb_var;
	info->pseudo_palette = NULL;
	info->par = NULL;
	info->flags = FBINFO_FLAG_DEFAULT;

	if (register_framebuffer(info) < 0)
		goto fballoced;

	platform_set_drvdata(device, info);

	printk(KERN_INFO "fb%d: %s frame buffer device\n", info->node,
		info->fix.id);

	st7920_hw_init();

	return 0;

fballoced:
	framebuffer_release(info);

none:
	return ret;
}

static int __devexit st7920fb_remove(struct platform_device *device)
{
	struct fb_info *info = platform_get_drvdata(device);

	if (info) {
		unregister_framebuffer(info);
		framebuffer_release(info);
	}

	return 0;
}

static struct platform_driver st7920fb_driver = {
	.probe	= st7920fb_probe,
	.remove = __devexit_p(st7920fb_remove),
	.driver = {
		.name	= ST7920FB_NAME,
	},
};

static int __init st7920fb_init(void)
{
	int ret = -EINVAL;

	/* st7920_init() must be called first */
	if (st7920_init()) {
		printk(KERN_ERR ST7920FB_NAME ": ERROR: "
			"st7920 is not initialized\n");
		goto none;
	}

	if (st7920_enable()) {
		printk(KERN_ERR ST7920FB_NAME ": ERROR: "
			"can't enable st7920 refreshing (being used)\n");
		return -ENODEV;
	}

	ret = platform_driver_register(&st7920fb_driver);

none:
	return ret;
}

static void __exit st7920fb_exit(void)
{
	platform_driver_unregister(&st7920fb_driver);
	st7920_exit();
}

module_init(st7920fb_init);
module_exit(st7920fb_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tanghaifeng <rou.ru.cai.hong@gmail.com>");
MODULE_DESCRIPTION("st7920 LCD framebuffer driver");

