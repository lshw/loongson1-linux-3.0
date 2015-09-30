/*
 * Copyright (c) 2012 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 * Platform device support for GS232 SoCs.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/resource.h>
#include <linux/serial_8250.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/spi/mmc_spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/mmc/host.h>
#include <linux/phy.h>
#include <linux/stmmac.h>
#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <linux/input.h>
#include <linux/clk.h>

#include <video/ls1xfb.h>

#include <loongson1.h>
#include <irq.h>
#include <asm/gpio.h>
#include <asm-generic/sizes.h>
#include <media/soc_camera.h>
#include <media/soc_camera_platform.h>

#if defined(CONFIG_HAVE_PWM)
struct pwm_device ls1x_pwm_list[] = {
	{ 0, 06, false },
	{ 1, 92, false },
	{ 2, 93, false },
	{ 3, 37, false },
};
#endif

#ifdef CONFIG_MTD_NAND_LS1X
#include <ls1x_nand.h>
static struct mtd_partition ls1x_nand_partitions[] = {
	{
		.name	= "bootloader",
		.offset	= MTDPART_OFS_APPEND,
		.size	= 1024*1024,
	},  {
		.name	= "kernel",
		.offset	= MTDPART_OFS_APPEND,
		.size	= 13*1024*1024,
	},  {
		.name	= "rootfs",
		.offset	= MTDPART_OFS_APPEND,
		.size	= 50*1024*1024,
	},  {
		.name	= "data",
		.offset	= MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL,
	},
};

static struct ls1x_nand_platform_data ls1x_nand_parts = {
//	.enable_arbiter	= 1,
//	.keep_config	= 1,
	.parts		= ls1x_nand_partitions,
	.nr_parts	= ARRAY_SIZE(ls1x_nand_partitions),
};

static struct resource ls1x_nand_resources[] = {
	[0] = {
		.start          = LS1X_NAND_BASE,
		.end            = LS1X_NAND_BASE + SZ_16K - 1,
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = LS1X_DMA0_IRQ,
        .end            = LS1X_DMA0_IRQ,
		.flags          = IORESOURCE_IRQ,
	},
};

struct platform_device ls1x_nand_device = {
	.name	= "ls1x-nand",
	.id		= -1,
	.dev	= {
		.platform_data = &ls1x_nand_parts,
	},
	.num_resources	= ARRAY_SIZE(ls1x_nand_resources),
	.resource		= ls1x_nand_resources,
};
#endif //CONFIG_MTD_NAND_LS1X

#define LS1X_UART(_id)						\
	{							\
		.mapbase	= LS1X_UART ## _id ## _BASE,	\
		.irq		= LS1X_UART ## _id ## _IRQ,	\
		.iotype		= UPIO_MEM,			\
		.flags		= UPF_IOREMAP | UPF_FIXED_TYPE,	\
		.type		= PORT_16550A,			\
	}

static struct plat_serial8250_port ls1x_serial8250_port[] = {
	LS1X_UART(0),
	LS1X_UART(1),
	LS1X_UART(2),
	LS1X_UART(3),
	{},
};

struct platform_device ls1x_uart_device = {
	.name		= "serial8250",
	.id		= PLAT8250_DEV_PLATFORM,
	.dev		= {
		.platform_data = ls1x_serial8250_port,
	},
};

void __init ls1x_serial_setup(void)
{
	struct clk *clk;
	struct plat_serial8250_port *p;

	clk = clk_get(NULL, "apb");
	if (IS_ERR(clk))
		panic("unable to get apb clock, err=%ld", PTR_ERR(clk));

	for (p = ls1x_serial8250_port; p->flags != 0; ++p)
		p->uartclk = clk_get_rate(clk);

#if 0
	/* 设置复用关系(EJTAG) */
	__raw_writel(__raw_readl(LS1X_CBUS_FIRST0) & (~0x0000003f), LS1X_CBUS_FIRST0);
	__raw_writel(__raw_readl(LS1X_CBUS_SECOND0) & (~0x0000003f), LS1X_CBUS_SECOND0);
	__raw_writel(__raw_readl(LS1X_CBUS_THIRD0) & (~0x0000003f), LS1X_CBUS_THIRD0);

	/* UART1 */
	__raw_writel(__raw_readl(LS1X_CBUS_FIRST0) & (~0x00060000), LS1X_CBUS_FIRST0);
	__raw_writel(__raw_readl(LS1X_CBUS_FIRST3) & (~0x00000060), LS1X_CBUS_FIRST3);
	__raw_writel(__raw_readl(LS1X_CBUS_SECOND1) & (~0x00000300), LS1X_CBUS_SECOND1);
	__raw_writel(__raw_readl(LS1X_CBUS_SECOND2) & (~0x00003000), LS1X_CBUS_SECOND2);
	__raw_writel(__raw_readl(LS1X_CBUS_FOURTHT0) | 0x0000000c, LS1X_CBUS_FOURTHT0);
	/* UART2 */
	__raw_writel(__raw_readl(LS1X_CBUS_SECOND1) & (~0x00000c30), LS1X_CBUS_SECOND1);
	__raw_writel(__raw_readl(LS1X_CBUS_THIRD0) & (~0x18000000), LS1X_CBUS_THIRD0);
	__raw_writel(__raw_readl(LS1X_CBUS_THIRD3) & (~0x00000180), LS1X_CBUS_THIRD3);
	__raw_writel(__raw_readl(LS1X_CBUS_FOURTHT0) | 0x00000030, LS1X_CBUS_FOURTHT0);
	/* UART3 */
	__raw_writel(__raw_readl(LS1X_CBUS_SECOND0) & (~0x00060000), LS1X_CBUS_SECOND0);
	__raw_writel(__raw_readl(LS1X_CBUS_SECOND1) & (~0x00003006), LS1X_CBUS_SECOND1);
	__raw_writel(__raw_readl(LS1X_CBUS_FOURTHT0) | 0x00000003, LS1X_CBUS_FOURTHT0);
#endif
}

/* USB OHCI */
#ifdef CONFIG_USB_OHCI_HCD_PLATFORM
#include <linux/usb/ohci_pdriver.h>
static u64 ls1x_ohci_dmamask = DMA_BIT_MASK(32);

static struct resource ls1x_ohci_resources[] = {
	[0] = {
		.start	= LS1X_OHCI_BASE,
		.end	= LS1X_OHCI_BASE + SZ_32K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= LS1X_OHCI_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct usb_ohci_pdata ls1x_ohci_pdata = {
};

struct platform_device ls1x_ohci_device = {
	.name		= "ohci-platform",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(ls1x_ohci_resources),
	.resource	= ls1x_ohci_resources,
	.dev		= {
		.dma_mask = &ls1x_ohci_dmamask,
		.platform_data = &ls1x_ohci_pdata,
	},
};
#endif

/* EHCI */
#ifdef CONFIG_USB_EHCI_HCD_LS1X
static u64 ls1x_ehci_dma_mask = DMA_BIT_MASK(32);
static struct resource ls1x_ehci_resources[] = { 
	[0] = {
		.start          = LS1X_EHCI_BASE,
		.end            = LS1X_EHCI_BASE + SZ_32K - 1,
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = LS1X_EHCI_IRQ,
		.end            = LS1X_EHCI_IRQ,
		.flags          = IORESOURCE_IRQ,
	},
};

static struct platform_device ls1x_ehci_device = {
	.name           = "ls1x-ehci",
	.id             = 0,
	.dev = {
		.dma_mask = &ls1x_ehci_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.num_resources  = ARRAY_SIZE(ls1x_ehci_resources),
	.resource       = ls1x_ehci_resources,
};
#endif //#ifdef CONFIG_USB_EHCI_HCD_LS1X


/* watchdog */
#ifdef CONFIG_LS1X_WDT
static struct resource ls1x_wdt_resource[] = {
	[0]={
		.start      = LS1X_WDT_BASE,
		.end        = (LS1X_WDT_BASE + 0x8),
		.flags      = IORESOURCE_MEM,
	},
};

static struct platform_device ls1x_wdt_device = {
	.name       = "ls1x-wdt",
	.id         = -1,
	.num_resources  = ARRAY_SIZE(ls1x_wdt_resource),
	.resource   = ls1x_wdt_resource,
};
#endif //#ifdef CONFIG_LS1X_WDT

/* RTC */
#ifdef CONFIG_RTC_DRV_RTC_LOONGSON1
static struct resource ls1x_rtc_resource[] = {
	[0]={
		.start      = LS1X_RTC_BASE,
		.end        = LS1X_RTC_BASE + SZ_16K - 1,
		.flags      = IORESOURCE_MEM,
	},
	[1] = {
		.start      = LS1X_RTC_INT0_IRQ,
		.end        = LS1X_RTC_INT0_IRQ,
		.flags      = IORESOURCE_IRQ,
	},
	[2] = {
		.start      = LS1X_RTC_INT1_IRQ,
		.end        = LS1X_RTC_INT1_IRQ,
		.flags      = IORESOURCE_IRQ,
	},
	[3] = {
		.start      = LS1X_RTC_INT2_IRQ,
		.end        = LS1X_RTC_INT2_IRQ,
		.flags      = IORESOURCE_IRQ,
	},
	[4] = {
		.start      = LS1X_RTC_TICK_IRQ,
		.end        = LS1X_RTC_TICK_IRQ,
		.flags      = IORESOURCE_IRQ,
	},
};

static struct platform_device ls1x_rtc_device = {
	.name       = "ls1x-rtc",
	.id         = 0,
	.num_resources  = ARRAY_SIZE(ls1x_rtc_resource),
	.resource   = ls1x_rtc_resource,
};
#endif //#ifdef CONFIG_RTC_DRV_RTC_LOONGSON1

#ifdef CONFIG_RTC_DRV_TOY_LOONGSON1
static struct platform_device ls1x_toy_device = {
	.name       = "ls1x-toy",
	.id         = 1,
};
#endif //#ifdef CONFIG_RTC_DRV_TOY_LOONGSON1

/* I2C */
/* I2C devices fitted. */
#ifdef CONFIG_GPIO_PCA953X
#include <linux/i2c/pca953x.h>
#define PCA9555_GPIO_BASE_0 170
#define PCA9555_IRQ_BASE_0 170
#define PCA9555_GPIO_IRQ_0 36

static int pca9555_setup_0(struct i2c_client *client,
			       unsigned gpio_base, unsigned ngpio,
			       void *context)
{
	gpio_request(PCA9555_GPIO_IRQ_0, "pca9555 gpio irq0");
	gpio_direction_input(PCA9555_GPIO_IRQ_0);

	gpio_request(gpio_base + 0, "mfrc531 irq");
	gpio_direction_input(gpio_base + 0);
	gpio_request(gpio_base + 1, "mfrc531 ncs");
	gpio_direction_output(gpio_base + 1, 1);
	gpio_request(gpio_base + 2, "mfrc531 rstpd");
	gpio_direction_output(gpio_base + 2, 0);
	return 0;
}

static struct pca953x_platform_data i2c_pca9555_platdata_0 = {
	.gpio_base	= PCA9555_GPIO_BASE_0, /* Start directly after the CPU's GPIO */
	.irq_base = PCA9555_IRQ_BASE_0,
//	.invert		= 0, /* Do not invert */
	.setup		= pca9555_setup_0,
};

#define PCA9555_GPIO_BASE_1 186
#define PCA9555_IRQ_BASE_1 186
#define PCA9555_GPIO_IRQ_1 31

static int pca9555_setup_1(struct i2c_client *client,
			       unsigned gpio_base, unsigned ngpio,
			       void *context)
{
	gpio_request(PCA9555_GPIO_IRQ_1, "pca9555 gpio irq1");
	gpio_direction_input(PCA9555_GPIO_IRQ_1);
	return 0;
}

static struct pca953x_platform_data i2c_pca9555_platdata_1 = {
	.gpio_base	= PCA9555_GPIO_BASE_1,
	.irq_base = PCA9555_IRQ_BASE_1,
	.setup		= pca9555_setup_1,
};

#define PCA9555_GPIO_BASE_2 202
#define PCA9555_IRQ_BASE_2 202
#define PCA9555_GPIO_IRQ_2 32

static int pca9555_setup_2(struct i2c_client *client,
			       unsigned gpio_base, unsigned ngpio,
			       void *context)
{
	gpio_request(PCA9555_GPIO_IRQ_2, "pca9555 gpio irq2");
	gpio_direction_input(PCA9555_GPIO_IRQ_2);
	return 0;
}

static struct pca953x_platform_data i2c_pca9555_platdata_2 = {
	.gpio_base	= PCA9555_GPIO_BASE_2,
	.irq_base = PCA9555_IRQ_BASE_2,
	.setup		= pca9555_setup_2,
};

#if defined(CONFIG_LEDS_GPIO) || defined(CONFIG_LEDS_GPIO_MODULE)
#include <linux/leds.h>
struct gpio_led pca9555_gpio_leds[] = {
	/* PCA9555 0 */
	{
		.name			= "soft_start_0",
		.gpio			= PCA9555_GPIO_BASE_0 + 3,
		.active_low		= 1,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
	{
		.name			= "rs485",
		.gpio			= PCA9555_GPIO_BASE_0 + 4,
		.active_low		= 1,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	},
	{
		.name			= "lock_key",
		.gpio			= PCA9555_GPIO_BASE_0 + 7,
		.active_low		= 1,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	},
	{
		.name			= "usb_reset",
		.gpio			= PCA9555_GPIO_BASE_0 + 8,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
	{
		.name			= "wifi_rfen",
		.gpio			= PCA9555_GPIO_BASE_0 + 9,
		.active_low		= 1,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	},
	{
		.name			= "gsm_emerg_off",
		.gpio			= PCA9555_GPIO_BASE_0 + 12,
		.active_low		= 1,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
	{
		.name			= "gsm_pwrkey",
		.gpio			= PCA9555_GPIO_BASE_0 + 13,
		.active_low		= 1,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
	/* PCA9555 1 */
	{
		.name			= "led_red",
		.gpio			= PCA9555_GPIO_BASE_1 + 8,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
	{
		.name			= "led_green",
		.gpio			= PCA9555_GPIO_BASE_1 + 9,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
	{
		.name			= "led_blue",
		.gpio			= PCA9555_GPIO_BASE_1 + 10,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
	{
		.name			= "lcd_backlight",
		.gpio			= PCA9555_GPIO_BASE_1 + 11,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
	{
		.name			= "soft_start_1",
		.gpio			= PCA9555_GPIO_BASE_1 + 13,
		.active_low		= 1,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	},
	/* PCA9555 2 */
	{
		.name			= "locker_bl",
		.gpio			= PCA9555_GPIO_BASE_2 + 1,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
	{
		.name			= "audio_en",
		.gpio			= PCA9555_GPIO_BASE_2 + 11,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	},
	{
		.name			= "otg_vbus",
		.gpio			= PCA9555_GPIO_BASE_2 + 13,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
	{
		.name			= "otg_id",
		.gpio			= PCA9555_GPIO_BASE_2 + 14,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
};

static struct gpio_led_platform_data pca9555_gpio_led_info = {
	.leds		= pca9555_gpio_leds,
	.num_leds	= ARRAY_SIZE(pca9555_gpio_leds),
};

static struct platform_device pca9555_leds = {
	.name	= "leds-gpio",
	.id	= 0,
	.dev	= {
		.platform_data	= &pca9555_gpio_led_info,
	}
};
#endif //#if defined(CONFIG_LEDS_GPIO) || defined(CONFIG_LEDS_GPIO_MODULE)
#endif //#ifdef CONFIG_GPIO_PCA953X

#ifdef CONFIG_SND_SOC_UDA1342
#include <sound/uda1342.h>
static struct uda1342_platform_data uda1342_info = {
//	.gpio_power	= H1940_LATCH_UDA_POWER,
//	.gpio_reset	= S3C2410_GPA(12),
//	.dac_clk	= UDA1380_DAC_CLK_SYSCLK,
};
#endif

#ifdef CONFIG_I2C_LS1X
static struct i2c_board_info __initdata ls1x_i2c0_devs[] = {
#ifdef CONFIG_RTC_DRV_SD2068
	{
		I2C_BOARD_INFO("sd2068", 0x32),
	},
#endif
#ifdef CONFIG_SND_SOC_UDA1342
	{
		I2C_BOARD_INFO("uda1342", 0x1a),
		.platform_data = &uda1342_info,
	},
#endif
#ifdef CONFIG_CODEC_UDA1342
	{
		I2C_BOARD_INFO("uda1342", 0x1a),
	},
#endif
//#ifdef CONFIG_SND_SOC_ES8388
#ifdef CONFIG_CODEC_ES8388
	{
		I2C_BOARD_INFO("es8388", 0x10),
	},
#endif
#ifdef CONFIG_RTC_DRV_ISL12022
	{
		I2C_BOARD_INFO("isl12022", 0x6f),
	},
#endif

#ifdef CONFIG_GPIO_PCA953X
	{
		I2C_BOARD_INFO("pca9555", 0x26),
		.irq = LS1X_GPIO_FIRST_IRQ + PCA9555_GPIO_IRQ_0,
		.platform_data = &i2c_pca9555_platdata_0,
	},
	{
		I2C_BOARD_INFO("pca9555", 0x22),
		.irq = LS1X_GPIO_FIRST_IRQ + PCA9555_GPIO_IRQ_1,
		.platform_data = &i2c_pca9555_platdata_1,
	},
	{
		I2C_BOARD_INFO("pca9555", 0x23),
		.irq = LS1X_GPIO_FIRST_IRQ + PCA9555_GPIO_IRQ_2,
		.platform_data = &i2c_pca9555_platdata_2,
	},
#endif
};

static struct resource ls1x_i2c0_resource[] = {
	[0]={
		.start	= LS1X_I2C0_BASE,
		.end	= LS1X_I2C0_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ls1x_i2c0_device = {
	.name		= "ls1x-i2c",
	.id			= 0,
	.num_resources	= ARRAY_SIZE(ls1x_i2c0_resource),
	.resource	= ls1x_i2c0_resource,
};

static struct resource ls1x_i2c1_resource[] = {
	[0]={
		.start	= LS1X_I2C1_BASE,
		.end	= LS1X_I2C1_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ls1x_i2c1_device = {
	.name		= "ls1x-i2c",
	.id			= 1,
	.num_resources	= ARRAY_SIZE(ls1x_i2c1_resource),
	.resource	= ls1x_i2c1_resource,
};

static struct resource ls1x_i2c2_resource[] = {
	[0]={
		.start	= LS1X_I2C2_BASE,
		.end	= LS1X_I2C2_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ls1x_i2c2_device = {
	.name		= "ls1x-i2c",
	.id			= 2,
	.num_resources	= ARRAY_SIZE(ls1x_i2c2_resource),
	.resource	= ls1x_i2c2_resource,
};
#endif //#ifdef CONFIG_I2C_LS1X

/* lcd */
#if defined(CONFIG_FB_LOONGSON1)
#include "../video_modes.c"
#ifdef CONFIG_LS1X_FB0
static struct resource ls1x_fb0_resource[] = {
	[0] = {
		.start = LS1X_DC0_BASE,
		.end   = LS1X_DC0_BASE + 0x0010 - 1,	/* 1M? */
		.flags = IORESOURCE_MEM,
	},
};

struct ls1xfb_mach_info ls1x_lcd0_info = {
	.id			= "Graphic lcd",
	.modes			= video_modes,
	.num_modes		= ARRAY_SIZE(video_modes),
	.pix_fmt		= PIX_FMT_RGB565,
	.de_mode		= 0,	/* 注意：lcd是否使用DE模式 */
	/* 根据lcd屏修改invert_pixclock和invert_pixde参数(0或1)，部分lcd可能显示不正常 */
	.invert_pixclock	= 0,
	.invert_pixde	= 0,
};

struct platform_device ls1x_fb0_device = {
	.name			= "ls1x-fb",
	.id				= 0,
	.num_resources	= ARRAY_SIZE(ls1x_fb0_resource),
	.resource		= ls1x_fb0_resource,
	.dev			= {
		.platform_data = &ls1x_lcd0_info,
	}
};
#endif	//#ifdef CONFIG_LS1X_FB0
#endif	//#if defined(CONFIG_FB_LOONGSON1)

#ifdef CONFIG_BACKLIGHT_PWM
#include <linux/pwm_backlight.h>
static struct platform_pwm_backlight_data ls1x_backlight_data = {
	.pwm_id		= 3,
	.max_brightness	= 255,
	.dft_brightness	= 200,
	.pwm_period_ns	= 7812500,
};

static struct platform_device ls1x_pwm_backlight = {
	.name = "pwm-backlight",
	.dev = {
		.platform_data = &ls1x_backlight_data,
	},
};
#endif //#ifdef CONFIG_BACKLIGHT_PWM

//gmac
#ifdef CONFIG_STMMAC_ETH
void __init ls1x_gmac_setup(void)
{
	u32 x;
	x = __raw_readl(LS1X_MUX_CTRL1);
	x &= ~PHY_INTF_SELI;
	#if defined(CONFIG_LS1X_GMAC0_RMII)
	x |= 0x4 << PHY_INTF_SELI_SHIFT;
	#endif
	__raw_writel(x, LS1X_MUX_CTRL1);

	x = __raw_readl(LS1X_MUX_CTRL0);
	__raw_writel(x & (~GMAC_SHUT), LS1X_MUX_CTRL0);

#if defined(CONFIG_LS1X_GMAC0_RMII)
	__raw_writel(0x400, (void __iomem *)KSEG1ADDR(LS1X_GMAC0_BASE + 0x14));
#endif
	__raw_writel(0xe4b, (void __iomem *)KSEG1ADDR(LS1X_GMAC0_BASE + 0x10));
}

#ifdef CONFIG_LS1X_GMAC0
static struct resource ls1x_mac0_resources[] = {
	[0] = {
		.start  = LS1X_GMAC0_BASE,
		.end    = LS1X_GMAC0_BASE + SZ_8K - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.name   = "macirq",
		.start  = LS1X_GMAC0_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct plat_stmmacenet_data ls1x_mac0_data = {
	.bus_id = 0,
	.pbl = 0,
	.has_gmac = 1,
	.enh_desc = 0,
};

struct platform_device ls1x_gmac0_mac = {
	.name           = "stmmaceth",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(ls1x_mac0_resources),
	.resource       = ls1x_mac0_resources,
	.dev            = {
		.platform_data = &ls1x_mac0_data,
	},
};

static struct plat_stmmacphy_data  phy0_private_data = {
	.bus_id = 0,
#ifdef CONFIG_RTL8305SC
	.phy_addr = 4,
#else
	.phy_addr = -1,	/* -1自动检测 */
#endif
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_RMII,
	
};

struct platform_device ls1x_gmac0_phy = {
	.name = "stmmacphy",
	.id = 0,
	.num_resources = 1,
	.resource = (struct resource[]){
		{
			.name = "phyirq",
			.start = PHY_POLL,
			.end = PHY_POLL,
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy0_private_data,
	},
};
#endif //#ifdef CONFIG_LS1X_GMAC0
#endif //#ifdef CONFIG_STMMAC_ETH

#if defined(CONFIG_SOUND_LS1X_AC97)
static struct resource ls1x_ac97_resource[] = {
	[0] = {
		.start	= LS1X_AC97_BASE,
		.end	= LS1X_AC97_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ls1x_audio_device = {
	.name           = "ls1x-audio",
	.id             = -1,
	.num_resources	= ARRAY_SIZE(ls1x_ac97_resource),
	.resource		= ls1x_ac97_resource,
};
#elif defined(CONFIG_SOUND_LS1X_IIS)
static struct resource ls1x_iis_resource[] = {
	[0] = {
		.start	= LS1C_I2S_BASE,
		.end	= LS1C_I2S_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ls1x_audio_device = {
	.name           = "ls1x-audio",
	.id             = -1,
	.num_resources	= ARRAY_SIZE(ls1x_iis_resource),
	.resource		= ls1x_iis_resource,
};
#endif

#ifdef CONFIG_SND_LS1X_SOC_I2S
static struct resource ls1x_i2s_resource[] = {
	[0] = {
		.start	= LS1C_I2S_BASE,
		.end	= LS1C_I2S_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ls1x_i2s_device = {
	.name           = "ls1x-i2s",
	.id             = -1,
	.num_resources	= ARRAY_SIZE(ls1x_i2s_resource),
	.resource		= ls1x_i2s_resource,
};
#endif

#ifdef CONFIG_SND_LS1X_SOC
static struct platform_device ls1x_pcm_device = {
	.name = "loongson1-pcm-audio",
	.id = -1,
};
#endif

#ifdef CONFIG_MTD_M25P80
static struct mtd_partition partitions[] = {
	[0] = {
		.name		= "pmon",
		.offset		= 0,
		.size		= 512 * 1024,	//512KB
	//	.mask_flags	= MTD_WRITEABLE,
	},
};

static struct flash_platform_data flash __maybe_unused = {
	.name		= "spi-flash",
	.parts		= partitions,
	.nr_parts	= ARRAY_SIZE(partitions),
	.type		= "w25x40",
};
#endif /* CONFIG_MTD_M25P80 */

#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
/* 开发板使用GPIO40(CAN1_RX)引脚作为MMC/SD卡的插拔探测引脚 */
#define DETECT_GPIO  184	/* PCA9555_GPIO_BASE_0 + 14 */
#define WRITE_PROTECT_GPIO  185	/* 写保护探测 */ /* PCA9555_GPIO_BASE_0 + 15 */
#if 1
static int mmc_spi_get_ro(struct device *dev)
{
	return gpio_get_value(WRITE_PROTECT_GPIO);
}
/* 轮询方式探测card的插拔 */
static int mmc_spi_get_cd(struct device *dev)
{
	return !gpio_get_value(DETECT_GPIO);
}
#else
#define MMC_SPI_CARD_DETECT_INT  (LS1X_GPIO_FIRST_IRQ + DETECT_GPIO)
/* 中断方式方式探测card的插拔，由于loongson1 CPU不支持双边沿触发 所以不建议使用中断方式 */
static int ls1b_mmc_spi_init(struct device *dev,
	irqreturn_t (*detect_int)(int, void *), void *data)
{
	return request_irq(MMC_SPI_CARD_DETECT_INT, detect_int,
		IRQF_TRIGGER_FALLING, "mmc-spi-detect", data);
}
/* 释放中断 */
static void ls1b_mmc_spi_exit(struct device *dev, void *data)
{
	free_irq(MMC_SPI_CARD_DETECT_INT, data);
}
#endif

static struct mmc_spi_platform_data mmc_spi = {
	.get_ro = mmc_spi_get_ro,
	/* 中断方式方式探测card的插拔 */
//	.init = ls1b_mmc_spi_init,
//	.exit = ls1b_mmc_spi_exit,
//	.detect_delay = 1200,	/* msecs */
	/* 轮询方式方式探测card的插拔 */
	.get_cd = mmc_spi_get_cd,
	.caps = MMC_CAP_NEEDS_POLL,
};	
#endif  /* defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE) */

#ifdef CONFIG_TOUCHSCREEN_ADS7846
#include <linux/spi/ads7846.h>
#define ADS7846_GPIO_IRQ 180 /* 开发板触摸屏使用的外部中断 */ /* PCA9555_GPIO_BASE_0 + 10 */
static struct ads7846_platform_data ads_info __maybe_unused = {
	.model				= 7846,
	.vref_delay_usecs	= 1,
	.keep_vref_on		= 0,
	.settle_delay_usecs	= 20,
//	.x_plate_ohms		= 800,
	.pressure_min		= 0,
	.pressure_max		= 2048,
	.debounce_rep		= 3,
	.debounce_max		= 10,
	.debounce_tol		= 50,
//	.get_pendown_state	= ads7846_pendown_state,
	.get_pendown_state	= NULL,
	.gpio_pendown		= ADS7846_GPIO_IRQ,
	.filter_init		= NULL,
	.filter 			= NULL,
	.filter_cleanup 	= NULL,
};
#endif /* TOUCHSCREEN_ADS7846 */

#ifdef CONFIG_SPI_LS1X_SPI0
#include <linux/spi/spi_ls1x.h>
static struct spi_board_info ls1x_spi0_devices[] = {
#ifdef CONFIG_MTD_M25P80
	{
		.modalias	= "m25p80",
		.bus_num 		= 0,
		.chip_select	= SPI0_CS0,
		.max_speed_hz	= 60000000,
		.platform_data	= &flash,
	},
#endif
	{
		.modalias		= "spidev",
		.bus_num 		= 0,
		.chip_select	= SPI1_CS1,
		.max_speed_hz	= 25000000,
		.mode = SPI_MODE_0,
	},
#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
	{
		.modalias		= "mmc_spi",
		.bus_num 		= 0,
		.chip_select	= SPI0_CS2,
		.max_speed_hz	= 25000000,
		.platform_data	= &mmc_spi,
		.mode = SPI_MODE_3,
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_ADS7846
	{
		.modalias = "ads7846",
		.platform_data = &ads_info,
		.bus_num 		= 0,
		.chip_select 	= SPI0_CS3,
		.max_speed_hz 	= 2500000,
		.mode 			= SPI_MODE_1,
		.irq			= ADS7846_GPIO_IRQ,
	},
#endif
};

static struct resource ls1x_spi0_resource[] = {
	[0]={
		.start	= LS1X_SPI0_BASE,
		.end	= LS1X_SPI0_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
#if defined(CONFIG_SPI_IRQ_MODE)
	[1]={
		.start	= LS1X_SPI0_IRQ,
		.end	= LS1X_SPI0_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

#ifdef CONFIG_SPI_CS_USED_GPIO
static int spi0_gpios_cs[] =
	{ 81, 82, 83, 84 };
#endif

static struct ls1x_spi_platform_data ls1x_spi0_platdata = {
#ifdef CONFIG_SPI_CS_USED_GPIO
	.gpio_cs_count = ARRAY_SIZE(spi0_gpios_cs),
	.gpio_cs = spi0_gpios_cs,
#elif CONFIG_SPI_CS
	.cs_count = SPI0_CS3 + 1,
#endif
};

static struct platform_device ls1x_spi0_device = {
	.name		= "spi_ls1x",
	.id 		= 0,
	.num_resources	= ARRAY_SIZE(ls1x_spi0_resource),
	.resource	= ls1x_spi0_resource,
	.dev		= {
		.platform_data	= &ls1x_spi0_platdata,//&ls1x_spi_devices,
	},
};
#endif //#ifdef CONFIG_SPI_LS1X_SPI0

#ifdef CONFIG_SPI_LS1X_SPI1 /* SPI1 控制器 */
#include <linux/spi/spi_ls1x.h>
static struct spi_board_info ls1x_spi1_devices[] = {
#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
	{
		.modalias		= "mmc_spi",
		.bus_num 		= 1,
		.chip_select	= SPI1_CS0,
		.max_speed_hz	= 25000000,
		.platform_data	= &mmc_spi,
	},
#endif
};

static struct resource ls1x_spi1_resource[] = {
	[0]={
		.start	= LS1X_SPI1_BASE,
		.end	= LS1X_SPI1_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
#if defined(CONFIG_SPI_IRQ_MODE)
	[1]={
		.start	= LS1X_SPI1_IRQ,
		.end	= LS1X_SPI1_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

#ifdef CONFIG_SPI_CS_USED_GPIO
static int spi1_gpios_cs[] =
	{ 38, 0, 1 };
#endif

static struct ls1x_spi_platform_data ls1x_spi1_platdata = {
#ifdef CONFIG_SPI_CS_USED_GPIO
	.gpio_cs_count = ARRAY_SIZE(spi1_gpios_cs),
	.gpio_cs = spi1_gpios_cs,
#elif CONFIG_SPI_CS
	.cs_count = SPI1_CS2 + 1,
#endif
};

static struct platform_device ls1x_spi1_device = {
	.name		= "spi_ls1x",
	.id 		= 1,
	.num_resources	= ARRAY_SIZE(ls1x_spi1_resource),
	.resource	= ls1x_spi1_resource,
	.dev		= {
		.platform_data	= &ls1x_spi1_platdata,//&ls1x_spi_devices,
	},
};
#endif	//#ifdef CONFIG_SPI_LS1X_SPI1

#ifdef CONFIG_CAN_SJA1000_PLATFORM
#include <linux/can/platform/sja1000.h>
#ifdef CONFIG_LS1X_CAN0
static struct resource ls1x_sja1000_resources_0[] = {
	{
		.start   = LS1X_CAN0_BASE,
		.end     = LS1X_CAN0_BASE + SZ_16K - 1,
		.flags   = IORESOURCE_MEM | IORESOURCE_MEM_8BIT,
	}, {
		.start   = LS1X_CAN0_IRQ,
		.end     = LS1X_CAN0_IRQ,
		.flags   = IORESOURCE_IRQ,
	},
};

static struct sja1000_platform_data ls1x_sja1000_platform_data_0 = {
	.ocr		= OCR_TX1_PULLDOWN | OCR_TX0_PUSHPULL,
	.cdr		= CDR_CBP,
};

static struct platform_device ls1x_sja1000_0 = {
	.name = "sja1000_platform",
	.id = 0,
	.dev = {
		.platform_data = &ls1x_sja1000_platform_data_0,
	},
	.resource = ls1x_sja1000_resources_0,
	.num_resources = ARRAY_SIZE(ls1x_sja1000_resources_0),
};
#endif	//#ifdef CONFIG_LS1X_CAN0
#ifdef CONFIG_LS1X_CAN1
static struct resource ls1x_sja1000_resources_1[] = {
	{
		.start   = LS1X_CAN1_BASE,
		.end     = LS1X_CAN1_BASE + SZ_16K - 1,
		.flags   = IORESOURCE_MEM | IORESOURCE_MEM_8BIT,
	}, {
		.start   = LS1X_CAN1_IRQ,
		.end     = LS1X_CAN1_IRQ,
		.flags   = IORESOURCE_IRQ,
	},
};

static struct sja1000_platform_data ls1x_sja1000_platform_data_1 = {
	.ocr		= OCR_TX1_PULLDOWN | OCR_TX0_PUSHPULL,
	.cdr		= CDR_CBP,
};

static struct platform_device ls1x_sja1000_1 = {
	.name = "sja1000_platform",
	.id = 1,
	.dev = {
		.platform_data = &ls1x_sja1000_platform_data_1,
	},
	.resource = ls1x_sja1000_resources_1,
	.num_resources = ARRAY_SIZE(ls1x_sja1000_resources_1),
};
#endif //#ifdef CONFIG_LS1X_CAN1

static void ls1x_can_setup(void)
{
	struct sja1000_platform_data *sja1000_pdata;
	struct clk *clk;

	clk = clk_get(NULL, "apb");
	if (IS_ERR(clk))
		panic("unable to get apb clock, err=%ld", PTR_ERR(clk));

	#ifdef CONFIG_LS1X_CAN0
	sja1000_pdata = &ls1x_sja1000_platform_data_0;
	sja1000_pdata->osc_freq = clk_get_rate(clk);
	#endif
	#ifdef CONFIG_LS1X_CAN1
	sja1000_pdata = &ls1x_sja1000_platform_data_1;
	sja1000_pdata->osc_freq = clk_get_rate(clk);
	#endif

	/* 设置复用关系 can0 gpio54/55 */
	__raw_writel(__raw_readl(LS1X_CBUS_FIRST1) & (~0x00c00000), LS1X_CBUS_FIRST1);
	__raw_writel(__raw_readl(LS1X_CBUS_SECOND1) & (~0x00c00000), LS1X_CBUS_SECOND1);
	__raw_writel(__raw_readl(LS1X_CBUS_THIRD1) | 0x00c00000, LS1X_CBUS_THIRD1);
	__raw_writel(__raw_readl(LS1X_CBUS_FOURTHT1) & (~0x00c00000), LS1X_CBUS_FOURTHT1);

	/* 使能can0控制器 */
	__raw_writel(__raw_readl(LS1X_MUX_CTRL0) & (~CAN0_SHUT), LS1X_MUX_CTRL0);
}

#endif //#ifdef CONFIG_CAN_SJA1000_PLATFORM

#ifdef CONFIG_USB_GADGET_SNPS_DWC_OTG
static u64 ls1c_otg_dma_mask = DMA_BIT_MASK(32);
static struct resource ls1c_otg_resources[] = {
	[0] = {
		.start = LS1X_OTG_BASE,
		.end   = LS1X_OTG_BASE + SZ_64K - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = LS1X_OTG_IRQ,
		.end   = LS1X_OTG_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device ls1c_otg_device = {
	.name           = "dwc_otg",
	.id             = -1,
	.dev = {
		.dma_mask = &ls1c_otg_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.num_resources  = ARRAY_SIZE(ls1c_otg_resources),
	.resource       = ls1c_otg_resources,
};
#endif

#ifdef CONFIG_SOC_CAMERA_GC0308
static struct i2c_board_info gc0308_i2c_camera = {
	I2C_BOARD_INFO("GC0308", 0x42 >> 1),
};

static struct soc_camera_link gc0308_link = {
	.bus_id         = 0,
	.i2c_adapter_id = 0,
	.board_info     = &gc0308_i2c_camera,
//	.power          = ls1c_camera_power,
//	.reset          = ls1c_camera_reset,
};
#endif

#ifdef CONFIG_SOC_CAMERA_S5K5CA
static struct i2c_board_info s5k5ca_i2c_camera = {
	I2C_BOARD_INFO("S5K5CA", 0x78 >> 1),
};

static struct soc_camera_link s5k5ca_link = {
	.bus_id         = 0,
	.i2c_adapter_id = 0,
	.board_info     = &s5k5ca_i2c_camera,
//	.power          = ls1c_camera_power,
//	.reset          = ls1c_camera_reset,
};
#endif

#ifdef CONFIG_SOC_CAMERA
static struct platform_device ls1x_camera_sensor = {
	.name	= "soc-camera-pdrv",
	.id	= 0,
	.dev	= {
#ifdef CONFIG_SOC_CAMERA_S5K5CA
		.platform_data = &s5k5ca_link,
#endif
#ifdef CONFIG_SOC_CAMERA_GC0308
		.platform_data = &gc0308_link,
#endif
	},
};
#endif

#ifdef CONFIG_SOC_CAMERA_LS1C
static struct resource ls1c_camera_resources[] = {
	{
		.start   = LS1X_CAMERA_BASE,
		.end     = LS1X_CAMERA_BASE + SZ_512K - 1,
		.flags   = IORESOURCE_MEM,
	}, {
		.start   = LS1X_CAM_IRQ,
		.end     = LS1X_CAM_IRQ,
		.flags   = IORESOURCE_IRQ,
	},
};

static struct ls1c_camera_pdata ls1c_camera_platform_data = {
	.mclk_24MHz = 24000000,
};

static struct platform_device ls1c_camera_host = {
	.name	= "ls1c-camera",
	.id		= 0,
	.dev	= {
		.platform_data = &ls1c_camera_platform_data,
	},
	.resource		= ls1c_camera_resources,
	.num_resources	= ARRAY_SIZE(ls1c_camera_resources),
};
#endif	//End of CONFIG_SOC_CAMERA_LS1C

#ifdef CONFIG_VIDEO_LS1X
#include <ls1x_camera.h>
static struct resource ls1x_camera_resources[] = {
	{
		.start   = LS1X_CAMERA_BASE,
		.end     = LS1X_CAMERA_BASE + SZ_512K - 1,
		.flags   = IORESOURCE_MEM,
	}, {
		.start   = LS1X_CAM_IRQ,
		.end     = LS1X_CAM_IRQ,
		.flags   = IORESOURCE_IRQ,
	},
};

static struct ls1x_camera_pdata ls1x_camera_platform_data = {
	.mclk_10khz = 2400,
};

struct platform_device ls1x_camera_device = {
	.name	= "ls1x-camera",
	.id		= 0,
	.dev	= {
		.platform_data = &ls1x_camera_platform_data,
	},
	.resource		= ls1x_camera_resources,
	.num_resources	= ARRAY_SIZE(ls1x_camera_resources),
};
#endif	//End of CONFIG_VIDEO_LS1X

#ifdef CONFIG_SENSORS_LS1X
#include <hwmon.h>
static struct ls1x_hwmon_pdata bast_hwmon_info = {
	/* battery voltage (0-8.4V) */
	.in[0] = &(struct ls1x_hwmon_chcfg) {
		.name		= "battery-voltage",
		.mult		= 3300 * 4,	/* 3.3V参考电压 乘以4转换为实际的测量电压(0-8.4V) */
		.div		= 1024,
		.single		= 1,
	},
};

static struct resource ls1x_hwmon_resources[] = {
	{
		.start   = LS1X_ADC_BASE,
		.end     = LS1X_ADC_BASE + SZ_16K - 1,
		.flags   = IORESOURCE_MEM,
	}, 
/*	{
		.start   = LS1X_ADC_IRQ,
		.end     = LS1X_ADC_IRQ,
		.flags   = IORESOURCE_IRQ,
	},*/
};

struct platform_device ls1x_device_hwmon = {
	.name		= "ls1x-hwmon",
	.id		= -1,
	.resource		= ls1x_hwmon_resources,
	.num_resources	= ARRAY_SIZE(ls1x_hwmon_resources),
//	.dev	= {
//		.platform_data = &bast_hwmon_info,
//	},
};

void __init ls1x_hwmon_set_platdata(struct ls1x_hwmon_pdata *pd)
{
	struct ls1x_hwmon_pdata *npd;

	if (!pd) {
		printk(KERN_ERR "%s: no platform data\n", __func__);
		return;
	}

	npd = kmemdup(pd, sizeof(struct ls1x_hwmon_pdata), GFP_KERNEL);
	if (!npd)
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);

	ls1x_device_hwmon.dev.platform_data = npd;
}
#endif

#ifdef CONFIG_KEYBOARD_LS1C_ADC_KEYS
#include <linux/gpio_keys.h>
#include <linux/input/adc_keys_polled.h>
static struct gpio_keys_button adc_keys_table[] = {
	{
		.code		= KEY_0,
		.active_low	= 0,
	}, {
		.code		= KEY_1,
		.active_low	= 0,
	}, {
		.code		= KEY_2,
		.active_low	= 0,
	}, {
		.code		= KEY_3,
		.active_low	= 0,
	}, {
		.code		= KEY_4,
		.active_low	= 0,
	}, {
		.code		= KEY_5,
		.active_low	= 0,
	}, {
		.code		= KEY_6,
		.active_low	= 0,
	}, {
		.code		= KEY_7,
		.active_low	= 0,
	}, {
		.code		= KEY_8,
		.active_low	= 0,
	}, 
	/*  */
	{
		.code		= KEY_9,
		.active_low	= 0,
	}, {
		.code		= KEY_A,
		.active_low	= 0,
	}, {
		.code		= KEY_B,
		.active_low	= 0,
	}, {
		.code		= KEY_C,
		.active_low	= 0,
	}, {
		.code		= KEY_D,
		.active_low	= 0,
	}, {
		.code		= KEY_E,
		.active_low	= 0,
	}, {
		.code		= KEY_F,
		.active_low	= 0,
	}, {
		.code		= KEY_G,
		.active_low	= 0,
	}, {
		.code		= KEY_H,
		.active_low	= 0,
	}, 
	/*  */
	{
		.code		= KEY_I,
		.active_low	= 0,
	}, {
		.code		= KEY_J,
		.active_low	= 0,
	}, {
		.code		= KEY_K,
		.active_low	= 0,
	}, {
		.code		= KEY_L,
		.active_low	= 0,
	}, {
		.code		= KEY_M,
		.active_low	= 0,
	}, {
		.code		= KEY_N,
		.active_low	= 0,
	}, {
		.code		= KEY_O,
		.active_low	= 0,
	}, {
		.code		= KEY_P,
		.active_low	= 0,
	}, {
		.code		= KEY_Q,
		.active_low	= 0,
	}, 
};

static struct adc_keys_platform_data adc_keys_info = {
	.debounce_interval = 1,
	.buttons	= adc_keys_table,
	.nbuttons	= ARRAY_SIZE(adc_keys_table),
	.poll_interval	= 50, /* default to 50ms */
};

static struct platform_device adc_keys_device = {
	.name		= "adc-keys-polled",
	.dev		= {
		.platform_data	= &adc_keys_info,
	},
};
#endif //#ifdef CONFIG_KEYBOARD_LS1C_ADC_KEYS


/***********************************************/
static struct platform_device *ls1b_platform_devices[] __initdata = {
	&ls1x_uart_device,

#ifdef CONFIG_USB_GADGET_SNPS_DWC_OTG
	&ls1c_otg_device, 
#endif

#ifdef CONFIG_LS1X_FB0
	&ls1x_fb0_device,
#endif

#ifdef CONFIG_MTD_NAND_LS1X
	&ls1x_nand_device,
#endif

#ifdef CONFIG_USB_EHCI_HCD_LS1X
	&ls1x_ehci_device,
#endif
#ifdef CONFIG_USB_OHCI_HCD_PLATFORM
	&ls1x_ohci_device,
#endif

#ifdef CONFIG_STMMAC_ETH
#ifdef CONFIG_LS1X_GMAC0
	&ls1x_gmac0_mac,
	&ls1x_gmac0_phy,
#endif
#endif

#if defined(CONFIG_SOUND_LS1X_AC97) || defined(CONFIG_SOUND_LS1X_IIS)
	&ls1x_audio_device,
#endif

#ifdef CONFIG_SND_LS1X_SOC
	&ls1x_pcm_device,
#endif
#ifdef CONFIG_SND_LS1X_SOC_I2S
	&ls1x_i2s_device,
#endif

#ifdef CONFIG_SPI_LS1X_SPI0
	&ls1x_spi0_device,
#endif
#ifdef CONFIG_SPI_LS1X_SPI1
	&ls1x_spi1_device,
#endif

#ifdef CONFIG_LS1X_WDT
	&ls1x_wdt_device,
#endif

#ifdef CONFIG_RTC_DRV_RTC_LOONGSON1
	&ls1x_rtc_device,
#endif
#ifdef CONFIG_RTC_DRV_TOY_LOONGSON1
	&ls1x_toy_device,
#endif

#ifdef CONFIG_I2C_LS1X
	&ls1x_i2c0_device,
	&ls1x_i2c1_device,
	&ls1x_i2c2_device,
#endif
#ifdef CONFIG_GPIO_PCA953X
#if defined(CONFIG_LEDS_GPIO) || defined(CONFIG_LEDS_GPIO_MODULE)
	&pca9555_leds,
#endif
#endif

#ifdef CONFIG_SOC_CAMERA_LS1C
	&ls1c_camera_host,
#endif
#ifdef CONFIG_SOC_CAMERA
	&ls1x_camera_sensor,
#endif
#ifdef CONFIG_VIDEO_LS1X
	&ls1x_camera_device,
#endif

#ifdef CONFIG_CAN_SJA1000_PLATFORM
#ifdef CONFIG_LS1X_CAN0
	&ls1x_sja1000_0,
#endif
#ifdef CONFIG_LS1X_CAN1
	&ls1x_sja1000_1,
#endif
#endif
#ifdef CONFIG_BACKLIGHT_PWM
	&ls1x_pwm_backlight,
#endif
#ifdef CONFIG_SENSORS_LS1X
	&ls1x_device_hwmon,
#endif
#ifdef CONFIG_KEYBOARD_LS1C_ADC_KEYS
	&adc_keys_device,
#endif
};

int __init ls1b_platform_init(void)
{
	ls1x_serial_setup();

#ifdef CONFIG_STMMAC_ETH
	ls1x_gmac_setup();
#endif

#ifdef CONFIG_CAN_SJA1000_PLATFORM
	ls1x_can_setup();
#endif	//#ifdef CONFIG_CAN_SJA1000_PLATFORM

#ifdef CONFIG_I2C_LS1X
	i2c_register_board_info(0, ls1x_i2c0_devs, ARRAY_SIZE(ls1x_i2c0_devs));
#endif

#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
	gpio_request(WRITE_PROTECT_GPIO, "MMC_SPI GPIO wp");
	gpio_direction_input(WRITE_PROTECT_GPIO);
	/* 轮询方式或中断方式探测card的插拔 */
	gpio_request(DETECT_GPIO, "MMC_SPI GPIO detect");
	gpio_direction_input(DETECT_GPIO);		/* 输入使能 */
#endif

#ifdef CONFIG_SPI_LS1X_SPI0
	spi_register_board_info(ls1x_spi0_devices, ARRAY_SIZE(ls1x_spi0_devices));
#endif

#ifdef CONFIG_SPI_LS1X_SPI1
	spi_register_board_info(ls1x_spi1_devices, ARRAY_SIZE(ls1x_spi1_devices));
#endif

#ifdef CONFIG_SENSORS_LS1X
	ls1x_hwmon_set_platdata(&bast_hwmon_info);
#endif

	return platform_add_devices(ls1b_platform_devices, ARRAY_SIZE(ls1b_platform_devices));
}
arch_initcall(ls1b_platform_init);

