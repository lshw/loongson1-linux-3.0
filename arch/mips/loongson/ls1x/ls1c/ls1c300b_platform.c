/*
 * Copyright (c) 2012 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 * Platform device support for loongson 1c v2.
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
#define	SZ_100M	(100*1024*1024)
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
//		.start          = LS1X_NAND_IRQ,
//		.end            = LS1X_NAND_IRQ,
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

	LS1X_UART(4),
	LS1X_UART(5),
	LS1X_UART(6),
	LS1X_UART(7),

	LS1X_UART(8),
	LS1X_UART(9),
	LS1X_UART(10),
	LS1X_UART(11),
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

	/* 如果要使用开发板的串口7 和串口8，则使能以下设置，开发板
	开发板的串口7 和串口8与IIS控制器复用(gpio87-90)  注意需要把
	内核的IIS控制器驱动选项关掉 。注意跳线设置，根据实际情况修改 */
#if 0
	__raw_writel(__raw_readl(LS1X_CBUS_FIRST2)   & 0xF87FFFFF, LS1X_CBUS_FIRST2);
	__raw_writel(__raw_readl(LS1X_CBUS_SECOND2)  & 0xF87FFFFF, LS1X_CBUS_SECOND2);
	__raw_writel(__raw_readl(LS1X_CBUS_THIRD2)   & 0xF87FFFFF, LS1X_CBUS_THIRD2);
	__raw_writel(__raw_readl(LS1X_CBUS_FOURTHT2) & 0xF87FFFFF, LS1X_CBUS_FOURTHT2);
	__raw_writel(__raw_readl(LS1X_CBUS_FIFTHT2)  | 0x07800000, LS1X_CBUS_FIFTHT2);
#endif

#if 0
	/* 串口0 */
	__raw_writel(__raw_readl(LS1X_CBUS_FIRST2)   & 0xFFFFF3FF, LS1X_CBUS_FIRST2);
	__raw_writel(__raw_readl(LS1X_CBUS_SECOND2)  | 0x00000C00, LS1X_CBUS_SECOND2);
	__raw_writel(__raw_readl(LS1X_CBUS_THIRD2)   & 0xFFFFF3FF, LS1X_CBUS_THIRD2);
	__raw_writel(__raw_readl(LS1X_CBUS_FOURTHT2) & 0xFFFFF3FF, LS1X_CBUS_FOURTHT2);
	__raw_writel(__raw_readl(LS1X_CBUS_FIFTHT2)  & 0xFFFFF3FF, LS1X_CBUS_FIFTHT2);
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
#ifdef CONFIG_RTC_DRV_TOY_LOONGSON1CV2
static struct platform_device ls1cv2_toy_device = {
	.name       = "ls1x-toy",
	.id         = 1,
};
#endif //#ifdef CONFIG_RTC_DRV_TOY_LOONGSON1CV2

/* I2C */
/* I2C devices fitted. */
#ifdef CONFIG_GPIO_PCA953X
#include <linux/i2c/pca953x.h>
#define PCA9555_GPIO_BASE 170
#define PCA9555_IRQ_BASE 170
#define PCA9555_GPIO_IRQ 31

#define LOCKER_TS	(PCA9555_GPIO_BASE+12)
#define LOCKER_BL	(PCA9555_GPIO_BASE+13)

static int pca9555_setup(struct i2c_client *client,
			       unsigned gpio_base, unsigned ngpio,
			       void *context)
{
	gpio_request(PCA9555_GPIO_IRQ, "pca9555 gpio irq");
	gpio_direction_input(PCA9555_GPIO_IRQ);
	return 0;
}

static struct pca953x_platform_data i2c_pca9555_platdata = {
	.gpio_base	= PCA9555_GPIO_BASE, /* Start directly after the CPU's GPIO */
	.irq_base = PCA9555_IRQ_BASE,
//	.invert		= 0, /* Do not invert */
	.setup		= pca9555_setup,
};

#if defined(CONFIG_LEDS_GPIO) || defined(CONFIG_LEDS_GPIO_MODULE)
#include <linux/leds.h>
struct gpio_led pca9555_gpio_leds[] = {
	{
		.name			= "locker_ts",
		.gpio			= LOCKER_TS,
		.active_low		= 1,
		.default_trigger	= NULL,
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
	{
		.name			= "locker_bl",
		.gpio			= LOCKER_BL,
		.active_low		= 0,
		.default_trigger	= NULL,
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

#ifdef CONFIG_I2C_LS1X
static struct i2c_board_info __initdata ls1x_i2c0_devs[] = {
#ifdef CONFIG_RTC_DRV_SD2068
	{
		I2C_BOARD_INFO("sd2068", 0x32),
	},
#endif
#ifdef CONFIG_GPIO_PCA953X
	{
		I2C_BOARD_INFO("pca9555", 0x20),
		.irq = LS1X_GPIO_FIRST_IRQ + PCA9555_GPIO_IRQ,
		.platform_data = &i2c_pca9555_platdata,
	},
#endif
//#ifdef CONFIG_SND_SOC_ES8388
#ifdef CONFIG_CODEC_ES8388
	{
		I2C_BOARD_INFO("es8388", 0x10),
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

/* ls1c300b的i2c控制器添加了中断(添加了中断号，原来1a 1b 1c的i2c控制器中断号都没有引出
        所以不能使用中断)，与ocores的i2c控制器相同，所以这里使用linux内核提供的ocores i2c控制器驱动
 */
#ifdef CONFIG_I2C_OCORES
#include <linux/i2c-ocores.h>
static struct i2c_board_info ocores_i2c0_board_info[] = {
#ifdef CONFIG_RTC_DRV_SD2068
	{
		I2C_BOARD_INFO("sd2068", 0x32),
	},
#endif
#ifdef CONFIG_GPIO_PCA953X
	{
		I2C_BOARD_INFO("pca9555", 0x20),
		.irq = LS1X_GPIO_FIRST_IRQ + PCA9555_GPIO_IRQ,
		.platform_data = &i2c_pca9555_platdata,
	},
#endif
//#ifdef CONFIG_SND_SOC_ES8388
#ifdef CONFIG_CODEC_ES8388
	{
		I2C_BOARD_INFO("es8388", 0x10),
	},
#endif
};

static struct resource ls1x_i2c0_resource[] = {
	[0]={
		.start	= LS1X_I2C0_BASE,
		.end	= LS1X_I2C0_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1]={
		.start	= LS1X_I2C0_IRQ,
		.end	= LS1X_I2C0_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct ocores_i2c_platform_data ocores_i2c0_data = {
	.regstep = 1,
//	.clock_khz	= 50000,	/* 输入时钟等于LPB时钟 */
	.devices	= ocores_i2c0_board_info, /* optional table of devices */
	.num_devices	= ARRAY_SIZE(ocores_i2c0_board_info), /* table size */
};

static struct platform_device ls1x_i2c0_device = {
	.name		= "ocores-i2c",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(ls1x_i2c0_resource),
	.resource	= ls1x_i2c0_resource,
	.dev = {
		.platform_data	= &ocores_i2c0_data,
	}
};

static void ls1x_i2c_setup(void)
{
	struct ocores_i2c_platform_data *ocores_i2c_data;
	struct clk *clk;

	clk = clk_get(NULL, "apb");
	if (IS_ERR(clk))
		panic("unable to get apb clock, err=%ld", PTR_ERR(clk));

	ocores_i2c_data = &ocores_i2c0_data;
	ocores_i2c_data->clock_khz = clk_get_rate(clk);
//	ocores_i2c_data = &ocores_i2c1_data;
//	ocores_i2c_data->clock_khz = clk_get_rate(clk);
}
#endif	//#ifdef CONFIG_I2C_OCORES

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
	.de_mode		= 1,	/* 注意：lcd是否使用DE模式 */
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

#ifdef CONFIG_BACKLIGHT_GENERIC
#define GPIO_BACKLIGHT_CTRL	180 //PCA9555_GPIO_BASE+10
#include <linux/backlight.h>
static void ls1x_bl_set_intensity(int intensity)
{
	if (intensity)
		gpio_direction_output(GPIO_BACKLIGHT_CTRL, 1);
	else
		gpio_direction_output(GPIO_BACKLIGHT_CTRL, 0);
}

static struct generic_bl_info ls1x_bl_info = {
	.name			= "ls1x-bl",
	.max_intensity		= 0xff,
	.default_intensity	= 0xff,
	.set_bl_intensity	= ls1x_bl_set_intensity,
};

static struct platform_device ls1x_bl_dev = {
	.name			= "generic-bl",
	.id			= 1,
	.dev = {
		.platform_data	= &ls1x_bl_info,
	},
};
#endif //#ifdef CONFIG_BACKLIGHT_GENERIC

#ifdef CONFIG_BACKLIGHT_PWM
#include <linux/pwm_backlight.h>
static struct platform_pwm_backlight_data ls1x_backlight_data = {
	.pwm_id		= 1,
	.max_brightness	= 255,
	.dft_brightness	= 100,
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

#if defined(CONFIG_SOUND_LS1X_IIS)
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
#define DETECT_GPIO  41
#if 1
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
	/* 中断方式方式探测card的插拔 */
//	.init = ls1b_mmc_spi_init,
//	.exit = ls1b_mmc_spi_exit,
//	.detect_delay = 1200,	/* msecs */
	/* 轮询方式方式探测card的插拔 */
	.get_cd = mmc_spi_get_cd,
	.caps = MMC_CAP_NEEDS_POLL,
};	
#endif  /* defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE) */

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

#ifdef CONFIG_SOC_CAMERA_GC0307
static struct i2c_board_info gc0307_i2c_camera = {
	I2C_BOARD_INFO("gc0307", 0x21),
};

static struct soc_camera_link gc0307_link = {
	.bus_id         = 0,
	.i2c_adapter_id = 0,
	.board_info     = &gc0307_i2c_camera,
};
#endif

/* soc-camera OV2640 */
#if defined(CONFIG_SOC_CAMERA_OV2640) || \
	defined(CONFIG_SOC_CAMERA_OV2640_MODULE)
#include <media/soc_camera.h>
#define OV2640_GPIO_CAMERA_PW		(PCA9555_GPIO_BASE + 14)
#define OV2640_GPIO_CAMERA_RST	(PCA9555_GPIO_BASE + 15)
static int gpio_ini = 1;
static int ov2640_camera_power(struct device *dev, int on)
{
	if (gpio_ini) {
		gpio_request(OV2640_GPIO_CAMERA_PW, "ov2640_pw");
		gpio_direction_output(OV2640_GPIO_CAMERA_PW, 1);
		gpio_request(OV2640_GPIO_CAMERA_RST, "ov2640_rst");
		gpio_direction_output(OV2640_GPIO_CAMERA_RST, 1);
		gpio_ini = 0;
		msleep(20);
	}

	/* enable or disable the camera */
	gpio_set_value(OV2640_GPIO_CAMERA_PW, on ? 0 : 1);
	msleep(20);

	if (!on)
		goto out;

	/* If enabled, give a reset impulse */
	gpio_set_value(OV2640_GPIO_CAMERA_RST, 0);
	msleep(20);
	gpio_set_value(OV2640_GPIO_CAMERA_RST, 1);
	msleep(100);

out:
	return 0;
}

static unsigned long isi_camera_query_bus_param(struct soc_camera_link *link)
{
	/* ISI board for ek using default 8-bits connection */
	return SOCAM_DATAWIDTH_8;
}

static struct i2c_board_info i2c_camera = {
	I2C_BOARD_INFO("ov2640", 0x30),
};

static struct soc_camera_link iclink_ov2640 = {
	.bus_id			= 0,
	.board_info		= &i2c_camera,
	.i2c_adapter_id		= 0,
	.power			= ov2640_camera_power,
	.query_bus_param	= isi_camera_query_bus_param,
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
#ifdef CONFIG_SOC_CAMERA_GC0307
		.platform_data = &gc0307_link,
#endif
#if defined(CONFIG_SOC_CAMERA_OV2640) || \
	defined(CONFIG_SOC_CAMERA_OV2640_MODULE)
	.platform_data = &iclink_ov2640,
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
	/* battery voltage (0-3.3V) */
	.in[0] = &(struct ls1x_hwmon_chcfg) {
		.name		= "battery-voltage",
		.mult		= 3300,	/* 3.3V参考电压 */
		.div		= 1024,
		.single		= 1,
	},
	.in[1] = &(struct ls1x_hwmon_chcfg) {
		.name		= "adc-d1",
		.mult		= 3300,	/* 3.3V参考电压 */
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

#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
#include <linux/gpio_keys.h>
static struct gpio_keys_button ls1x_gpio_keys_buttons[] = {
	 {
		.code		= KEY_0,
		.gpio		= 170,	/* PCA9555_GPIO_BASE+0 */
		.active_low	= 1,
		.desc		= "0",
		.wakeup		= 1,
		.debounce_interval	= 10, /* debounce ticks interval in msecs */
	},
	{
		.code		= KEY_1,
		.gpio		= 171,	/* PCA9555_GPIO_BASE+1 */
		.active_low	= 1,
		.desc		= "1",
		.wakeup		= 1,
		.debounce_interval	= 10, /* debounce ticks interval in msecs */
	},
	{
		.code		= KEY_2,
		.gpio		= 172,	/* PCA9555_GPIO_BASE+2 */
		.active_low	= 1,
		.desc		= "2",
		.wakeup		= 1,
		.debounce_interval	= 10, /* debounce ticks interval in msecs */
	},
	{
		.code		= KEY_3,
		.gpio		= 173,	/* PCA9555_GPIO_BASE+3 */
		.active_low	= 1,
		.desc		= "3",
		.wakeup		= 1,
		.debounce_interval	= 10, /* debounce ticks interval in msecs */
	},
	{
		.code		= KEY_4,
		.gpio		= 174,	/* PCA9555_GPIO_BASE+4 */
		.active_low	= 1,
		.desc		= "4",
		.wakeup		= 1,
		.debounce_interval	= 10, /* debounce ticks interval in msecs */
	},
	{
		.code		= KEY_5,
		.gpio		= 175,	/* PCA9555_GPIO_BASE+5 */
		.active_low	= 1,
		.desc		= "5",
		.wakeup		= 1,
		.debounce_interval	= 10, /* debounce ticks interval in msecs */
	},
	{
		.code		= KEY_6,
		.gpio		= 176,	/* PCA9555_GPIO_BASE+6 */
		.active_low	= 1,
		.desc		= "6",
		.wakeup		= 1,
		.debounce_interval	= 10, /* debounce ticks interval in msecs */
	},
	{
		.code		= KEY_ENTER,
		.gpio		= 177,	/* PCA9555_GPIO_BASE+7 */
		.active_low	= 1,
		.desc		= "ENTER",
		.wakeup		= 1,
		.debounce_interval	= 10, /* debounce ticks interval in msecs */
	},
};

static struct gpio_keys_platform_data ls1x_gpio_keys_data = {
	.nbuttons = ARRAY_SIZE(ls1x_gpio_keys_buttons),
	.buttons = ls1x_gpio_keys_buttons,
	.rep	= 1,	/* enable input subsystem auto repeat */
};

static struct platform_device ls1x_gpio_keys = {
	.name =	"gpio-keys",
	.id =	-1,
	.dev = {
		.platform_data = &ls1x_gpio_keys_data,
	}
};
#endif

#if defined(CONFIG_MMC_LS1X)
#include <mci.h>
#define SDIO_WP_GPIO  32
#define SDIO_DETECT_GPIO  84
/* 轮询方式探测card的插拔 */
static int ls1x_sdio_get_ro(struct device *dev)
{
	return gpio_get_value(SDIO_WP_GPIO);
}

static int ls1x_sdio_get_cd(struct device *dev)
{
	return !gpio_get_value(SDIO_DETECT_GPIO);
}

static struct ls1x_mci_pdata ls1x_sdio_parts = {
	/* 中断方式方式探测card的插拔 */
//	.init = ls1x_mmc_init,
//	.exit = ls1x_mmc_exit,
//	.detect_delay = 1200,	/* msecs */
	/* 轮询方式方式探测card的插拔 */
	.get_ro = ls1x_sdio_get_ro,
	.get_cd = ls1x_sdio_get_cd,
	.caps = MMC_CAP_NEEDS_POLL,

//	.max_clk = 17000000, /* 部分sd卡可能会出现读写错误，可以尝试降低提供给sd卡的频率 */
};

static struct resource ls1x_sdio_resources[] = {
	[0] = {
		.start          = LS1X_SDIO_BASE,
		.end            = LS1X_SDIO_BASE + SZ_16K - 1,
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = LS1X_SDIO_IRQ,
		.end            = LS1X_SDIO_IRQ,
		.flags          = IORESOURCE_IRQ,
	},
};

struct platform_device ls1x_sdio_device = {
	.name	= "ls1x-sdi",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(ls1x_sdio_resources),
	.resource		= ls1x_sdio_resources,
	.dev	= {
		.platform_data = &ls1x_sdio_parts,
	},
};
#endif //CONFIG_MTD_SDIO_LS1X


/***********************************************/
static struct platform_device *ls1b_platform_devices[] __initdata = {
	&ls1x_uart_device,

#ifdef CONFIG_USB_GADGET_SNPS_DWC_OTG
	&ls1c_otg_device, 
#endif

#ifdef CONFIG_LS1X_FB0
	&ls1x_fb0_device,
#endif
#ifdef CONFIG_BACKLIGHT_GENERIC
	&ls1x_bl_dev,
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

#if defined(CONFIG_SOUND_LS1X_IIS)
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

#ifdef CONFIG_RTC_DRV_TOY_LOONGSON1CV2
	&ls1cv2_toy_device,
#endif

#ifdef CONFIG_I2C_LS1X
	&ls1x_i2c0_device,
	&ls1x_i2c1_device,
	&ls1x_i2c2_device,
#endif
#ifdef CONFIG_I2C_OCORES
	&ls1x_i2c0_device,
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

#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
	&ls1x_gpio_keys,
#endif

#if defined(CONFIG_MMC_LS1X)
	&ls1x_sdio_device,
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

#ifdef CONFIG_I2C_OCORES
	ls1x_i2c_setup();
#endif

#ifdef CONFIG_I2C_LS1X
	i2c_register_board_info(0, ls1x_i2c0_devs, ARRAY_SIZE(ls1x_i2c0_devs));
#endif

#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
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

#ifdef CONFIG_BACKLIGHT_GENERIC
	gpio_request(GPIO_BACKLIGHT_CTRL, "backlight");
#endif

#if defined(CONFIG_SOC_CAMERA_OV2640) || \
	defined(CONFIG_SOC_CAMERA_OV2640_MODULE)
/*	gpio_request(OV2640_GPIO_CAMERA_PW, "ov2640_pw");
	gpio_direction_output(OV2640_GPIO_CAMERA_PW, 1);
	gpio_request(OV2640_GPIO_CAMERA_RST, "ov2640_rst");
	gpio_direction_output(OV2640_GPIO_CAMERA_RST, 1);*/
#endif

#if defined(CONFIG_MMC_LS1X)
	/* 轮询方式或中断方式探测card的插拔 */
	gpio_request(SDIO_WP_GPIO, "ls1x sdio wp");
	gpio_direction_input(SDIO_WP_GPIO);
	gpio_request(SDIO_DETECT_GPIO, "ls1x sdio detect");
	gpio_direction_input(SDIO_DETECT_GPIO);
#endif

	return platform_add_devices(ls1b_platform_devices, ARRAY_SIZE(ls1b_platform_devices));
}

arch_initcall(ls1b_platform_init);

