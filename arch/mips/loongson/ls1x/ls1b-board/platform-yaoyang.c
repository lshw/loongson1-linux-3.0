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
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/clk.h>
#include <linux/leds.h>
#include <linux/leds_pwm.h>

#include <video/ls1xfb.h>

#include <loongson1.h>
#include <irq.h>
#include <asm/gpio.h>
#include <asm-generic/sizes.h>

#ifdef CONFIG_MTD_NAND_LS1X
#include <ls1x_nand.h>
static struct mtd_partition ls1x_nand_partitions[] = {
	{
		.name	= "kernel",
		.offset	= MTDPART_OFS_APPEND,
		.size	= 14*1024*1024,
	}, {
		.name	= "rootfs",
		.offset	= MTDPART_OFS_APPEND,
		.size	= 100*1024*1024,
	}, {
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

#define LS1X_UART_SHARE(_id, _irq)						\
	{							\
		.mapbase	= LS1X_UART ## _id ## _BASE,	\
		.irq		= LS1X_UART ## _irq ## _IRQ,	\
		.iotype		= UPIO_MEM,			\
		.flags		= UPF_IOREMAP | UPF_FIXED_TYPE | UPF_SHARE_IRQ,	\
		.type		= PORT_16550A,			\
	}

static struct plat_serial8250_port ls1x_serial8250_port[] = {
	LS1X_UART(0),
	LS1X_UART(1),
	LS1X_UART(2),
	LS1X_UART(3),
#ifdef CONFIG_LS1B_MACH
	LS1X_UART(4),
	LS1X_UART(5),
#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL0
	LS1X_UART_SHARE(6, 0),
	LS1X_UART_SHARE(7, 0),
	LS1X_UART_SHARE(8, 0),
#endif
#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL1
	LS1X_UART_SHARE(9, 1),
	LS1X_UART_SHARE(10, 1),
	LS1X_UART_SHARE(11, 1),
#endif
#endif
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

#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL0
	__raw_writeb(__raw_readb(UART_SPLIT) | 0x01, UART_SPLIT);
#endif
#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL1
	__raw_writeb(__raw_readb(UART_SPLIT) | 0x02, UART_SPLIT);
	__raw_writel(__raw_readl(LS1X_MUX_CTRL1) | UART1_3_USE_CAN1 | UART1_2_USE_CAN0, 
				LS1X_MUX_CTRL1);
#endif

	clk = clk_get(NULL, "apb");
	if (IS_ERR(clk))
		panic("unable to get apb clock, err=%ld", PTR_ERR(clk));

	for (p = ls1x_serial8250_port; p->flags != 0; ++p)
		p->uartclk = clk_get_rate(clk);
}

/* OHCI */
#ifdef CONFIG_USB_OHCI_HCD_LS1X
static u64 ls1x_ohci_dma_mask = DMA_BIT_MASK(32);
static struct resource ls1x_ohci_resources[] = {
	[0] = {
		.start          = LS1X_OHCI_BASE,
		.end            = LS1X_OHCI_BASE + SZ_32K - 1,
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = LS1X_OHCI_IRQ,
		.end            = LS1X_OHCI_IRQ,
		.flags          = IORESOURCE_IRQ,
	},
};

static struct platform_device ls1x_ohci_device = {
	.name           = "ls1x-ohci",
	.id             = 0,
	.dev = {
//		.platform_data = &ls1x_ohci_platform_data,
		.dma_mask = &ls1x_ohci_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.num_resources  = ARRAY_SIZE(ls1x_ohci_resources),
	.resource       = ls1x_ohci_resources,
};
#endif //#ifdef CONFIG_USB_OHCI_HCD_LS1X

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
//		.platform_data = &ls1x_ehci_platform_data,
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
#ifdef CONFIG_SOC_CAMERA_LS1B
#include <media/ls1b_camera.h>
static struct ls1b_camera_platform_data gc0307_plat = {
//	.bl = 57,
//	.ts = 56,
	.hsync = 58,
	.vsync = 59,
};
#endif

#if defined(CONFIG_GPIO_PCF857X) || defined(CONFIG_GPIO_PCF857X_MODULE)
#include <linux/i2c/pcf857x.h>
#define PCF8574_GPIO_BASE 188

#define PCF8574_DO0	(PCF8574_GPIO_BASE+0)
#define LOCKER_BL	(PCF8574_GPIO_BASE+1)
#define SHUTDOWN	(PCF8574_GPIO_BASE+2)
#define WIFI_RFEN	(PCF8574_GPIO_BASE+3)
#define USBRESET	(PCF8574_GPIO_BASE+4)
#define NAND_CAMERA	(PCF8574_GPIO_BASE+5)
#define PCF8574_DO6	(PCF8574_GPIO_BASE+6)
#define POWER_OFF	(PCF8574_GPIO_BASE+7)
static struct pcf857x_platform_data ls1x_pcf857x_pdata = {
	.gpio_base	= PCF8574_GPIO_BASE,
	.n_latch	= 0,
	.setup		= NULL,
	.teardown	= NULL,
	.context	= NULL,
};
#if defined(CONFIG_LEDS_GPIO) || defined(CONFIG_LEDS_GPIO_MODULE)
#include <linux/leds.h>
struct gpio_led pcf8574_gpio_leds[] = {
	{
		.name			= "LOCKER_BL",
		.gpio			= LOCKER_BL,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	}, {
		.name			= "SHUTDOWN",
		.gpio			= SHUTDOWN,
		.active_low		= 1,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	}, {
		.name			= "WIFI_RFEN",
		.gpio			= WIFI_RFEN,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	}, {
		.name			= "USB_RESET",
		.gpio			= USBRESET,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	}, {
		.name			= "NAND_CAMERA",
		.gpio			= NAND_CAMERA,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	}, {
		.name			= "POWER_OFF",
		.gpio			= POWER_OFF,
		.active_low		= 1,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	},
};

static struct gpio_led_platform_data pcf8574_gpio_led_info = {
	.leds		= pcf8574_gpio_leds,
	.num_leds	= ARRAY_SIZE(pcf8574_gpio_leds),
};

static struct platform_device pcf8574_leds = {
	.name	= "leds-gpio",
	.id	= 0,
	.dev	= {
		.platform_data	= &pcf8574_gpio_led_info,
	}
};
#endif
#endif

#ifdef CONFIG_GPIO_PCA953X
#include <linux/i2c/pca953x.h>
#define PCA9555_GPIO_BASE 170
#define PCA9555_IRQ_BASE 170 + LS1X_GPIO_FIRST_IRQ
#define PCA9555_GPIO_IRQ 2

static int ls1x_pca9555_setup(struct i2c_client *client,
			       unsigned gpio_base, unsigned ngpio,
			       void *context)
{
	gpio_request(PCA9555_GPIO_IRQ, "pca9555 gpio irq");
	gpio_direction_input(PCA9555_GPIO_IRQ);

	gpio_request(gpio_base + 12, "lcd reset");
	gpio_direction_output(gpio_base + 12, 1);
	gpio_request(gpio_base + 13, "mfrc531 irq");
	gpio_direction_input(gpio_base + 13);
	gpio_request(gpio_base + 14, "mfrc531 ncs");
//	gpio_direction_output(gpio_base + 14, 1);
	gpio_direction_input(gpio_base + 14);
//	gpio_request(gpio_base + 15, "mfrc531 rstpd");
//	gpio_direction_output(gpio_base + 15, 0);
	return 0;
}

static struct pca953x_platform_data ls1x_i2c_pca9555_platdata = {
	.gpio_base	= PCA9555_GPIO_BASE, /* Start directly after the CPU's GPIO */
	.irq_base = PCA9555_IRQ_BASE,
//	.invert		= 0, /* Do not invert */
	.setup		= ls1x_pca9555_setup,
};

#if defined(CONFIG_LEDS_GPIO) || defined(CONFIG_LEDS_GPIO_MODULE)
#include <linux/leds.h>
struct gpio_led pca9555_gpio_leds[] = {
	{
		.name			= "mfrc531_rstpd",
		.gpio			= 185,	/* PCA9555_GPIO_BASE 170 + 15 */
		.active_low		= 1,
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
	.id	= 1,
	.dev	= {
		.platform_data	= &pca9555_gpio_led_info,
	}
};
#endif //#if defined(CONFIG_LEDS_GPIO) || defined(CONFIG_LEDS_GPIO_MODULE)
#endif //#ifdef CONFIG_GPIO_PCA953X

#ifdef CONFIG_I2C_LS1X
static struct i2c_board_info __initdata ls1x_i2c0_devs[] = {
#ifdef CONFIG_SOC_CAMERA_LS1B
	{
		I2C_BOARD_INFO("gc0307", 0x21),
		.platform_data = &gc0307_plat,
	},
#endif
#if defined(CONFIG_GPIO_PCF857X) || defined(CONFIG_GPIO_PCF857X_MODULE)
	{
		I2C_BOARD_INFO("pcf8574a", 0x24),
		.platform_data	= &ls1x_pcf857x_pdata,
	},
#endif
#ifdef CONFIG_GPIO_PCA953X
	{
		I2C_BOARD_INFO("pca9555", 0x26),
		.irq = LS1X_GPIO_FIRST_IRQ + PCA9555_GPIO_IRQ,
		.platform_data = &ls1x_i2c_pca9555_platdata,
	},
#endif
#ifdef CONFIG_SENSORS_MCP3021
	{
		I2C_BOARD_INFO("mcp3021", 0x4d),
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

#define GPIO_BACKLIGHT_CTRL	181	/* 对应PCA9555的I/O1.3 */
#ifdef CONFIG_BACKLIGHT_GENERIC
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

//gmac
#ifdef CONFIG_STMMAC_ETH
void __init ls1x_gmac_setup(void)
{
#ifdef CONFIG_LS1X_GMAC0
{
	u32 x;
	x = __raw_readl(LS1X_MUX_CTRL1);
	#if defined(CONFIG_LS1X_GMAC0_100M)
	x = x | GMAC0_USE_TXCLK | GMAC0_USE_PWM01;
	#elif defined(CONFIG_LS1X_GMAC0_1000M)
	x = x & (~GMAC0_USE_TXCLK) & (~GMAC0_USE_PWM01);
	#endif
	__raw_writel(x & (~GMAC0_SHUT), LS1X_MUX_CTRL1);
}
#endif
#ifdef CONFIG_LS1X_GMAC1
{
	u32 x;
	x = __raw_readl(LS1X_MUX_CTRL0);
	x = x | GMAC1_USE_UART1 | GMAC1_USE_UART0;	/* 复用UART0&1 */
	__raw_writel(x, LS1X_MUX_CTRL0);
	x = __raw_readl(LS1X_MUX_CTRL1);
	#if defined(CONFIG_LS1X_GMAC1_100M)
	x = x | GMAC1_USE_TXCLK | GMAC1_USE_PWM23;
	#elif defined(CONFIG_LS1X_GMAC1_1000M)
	x = x & (~GMAC1_USE_TXCLK) & (~GMAC1_USE_PWM23);
	#endif
	__raw_writel(x & (~GMAC1_SHUT), LS1X_MUX_CTRL1);
}
#endif
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
	.phy_addr = 0,	/* -1自动检测 */
#endif
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	
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

#ifdef CONFIG_LS1X_GMAC1
static struct resource ls1x_mac1_resources[] = {
	[0] = {
		.start  = LS1X_GMAC1_BASE,
		.end    = LS1X_GMAC1_BASE + SZ_8K - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.name   = "macirq",
		.start  = LS1X_GMAC1_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct plat_stmmacenet_data ls1x_mac1_data = {
	.bus_id = 1,
	.pbl = 0,
	.has_gmac = 1,
	.enh_desc = 0,
};

struct platform_device ls1x_gmac1_mac = {
	.name           = "stmmaceth",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(ls1x_mac1_resources),
	.resource       = ls1x_mac1_resources,
	.dev            = {
		.platform_data = &ls1x_mac1_data,
	},
};

static struct plat_stmmacphy_data  phy1_private_data = {
	.bus_id = 1,
	.phy_addr = 0,	/* -1自动检测 */
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	
};

struct platform_device ls1x_gmac1_phy = {
	.name = "stmmacphy",
	.id = 1,
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
		.platform_data = &phy1_private_data,
	},
};
#endif //#ifdef CONFIG_LS1X_GMAC1
#endif //#ifdef CONFIG_STMMAC_ETH

#ifdef CONFIG_SOUND_LS1X_AC97
static struct resource ls1x_ac97_resource[] = {
	[0]={
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
#define DETECT_GPIO  29
#define WRITE_PROTECT_GPIO  194	/* 写保护探测 */ /* PCF8574_DO6 (PCF8574_GPIO_BASE+6) */
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

static struct mmc_spi_platform_data mmc_spi __maybe_unused = {
	/* 中断方式方式探测card的插拔 */
//	.init = ls1b_mmc_spi_init,
//	.exit = ls1b_mmc_spi_exit,
//	.detect_delay = 1200,	/* msecs */
	/* 轮询方式方式探测card的插拔 */
	.get_ro = mmc_spi_get_ro,
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
#ifdef CONFIG_SPI_MCP3201
	{
		.modalias	= "mcp3201",
		.bus_num 	= 0,
		.chip_select	= SPI0_CS3,
		.max_speed_hz	= 1000000,
	},
#endif
	{
		.modalias		= "spidev",
		.bus_num 		= 0,
		.chip_select	= SPI0_CS1,	/* SPI1_CS1 */
		.max_speed_hz	= 5000000,
		.mode = SPI_MODE_2,
	},
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
	{ 27, 28, 29, 30 };
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

#elif defined(CONFIG_SPI_GPIO)	/* 使用GPIO模拟SPI代替 */
struct spi_gpio_platform_data spi0_gpio_platform_data = {
	.sck = 24,	/*gpio24*/
	.mosi = 26,	/*gpio26*/
	.miso = 25,	/*gpio25*/
	.num_chipselect = 4,
};

static struct platform_device spi0_gpio_device = {
	.name = "spi_gpio",
	.id   = 2,	/* 用于区分spi0和spi1 */
	.dev = {
		.platform_data = &spi0_gpio_platform_data,
	},
};

static struct spi_board_info spi0_gpio_devices[] = {
#ifdef CONFIG_MTD_M25P80
	{
		.modalias	= "m25p80",
		.bus_num 		= 2,	/* 对应spigpio_device的.id=2 */
		.controller_data = (void *)27,	/*gpio27*/
		.chip_select	= 0,
		.max_speed_hz	= 60000000,
		.platform_data	= &flash,
	},
#endif
#ifdef CONFIG_SPI_MCP3201
	{
		.modalias	= "mcp3201",
		.bus_num 	= 2,
		.controller_data = (void *)30,	/*gpio30*/
		.chip_select	= 3, /*SPI0_CS2*/
		.max_speed_hz	= 1000000,
	},
#endif
	{
		.modalias		= "spidev",
		.bus_num 		= 2,
		.controller_data = (void *)28,	/*gpio38*/
		.chip_select	= 1,	/* SPI1_CS1 */
		.max_speed_hz	= 5000000,
		.mode = SPI_MODE_2,
	},
};
#endif //#ifdef CONFIG_SPI_LS1X_SPI0

#if defined(CONFIG_SPI_LS1X_SPI1) /* SPI1 控制器 */
#include <linux/spi/spi_ls1x.h>
static struct spi_board_info ls1x_spi1_devices[] = {
#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
	{
		.modalias		= "mmc_spi",
		.bus_num 		= 1,
		.chip_select	= SPI0_CS0,
		.max_speed_hz	= 25000000,
		.platform_data	= &mmc_spi,
		.mode = SPI_MODE_3,
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
#elif defined(CONFIG_SPI_GPIO)	/* 使用GPIO模拟SPI代替 */
//#elif 0
struct spi_gpio_platform_data spi1_gpio_platform_data = {
	.sck = 39,	/*gpio39*/
	.mosi = 40,	/*gpio40*/
	.miso = 41,	/*gpio41*/
	.num_chipselect = 1,
};

static struct platform_device spi1_gpio_device = {
	.name = "spi_gpio",
//	.name = "spi_gpio_delay",
	.id   = 3,
	.dev = {
		.platform_data = &spi1_gpio_platform_data,
	},
};

static struct spi_board_info spi1_gpio_devices[] = {
#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
	{
		.modalias		= "mmc_spi",
		.bus_num 		= 3,
		.controller_data = (void *)38,	/*gpio38*/
		.chip_select	= 0,
		.max_speed_hz	= 25000000,
		.platform_data	= &mmc_spi,
		.mode = SPI_MODE_3,
	},
#endif
};
#endif	//#ifdef CONFIG_SPI_LS1X_SPI1

/* matrix keypad */
#if defined(CONFIG_KEYBOARD_MATRIX) || defined(CONFIG_KEYBOARD_MATRIX_MODULE)
#include <linux/input/matrix_keypad.h>
static const uint32_t ls1bkbd_keymap[] = {
	KEY(0, 0, KEY_LEFT),
	KEY(0, 1, KEY_B),
	KEY(0, 2, KEY_F5),
	KEY(0, 3, KEY_F8),
	KEY(0, 4, KEY_F6),
	KEY(0, 5, KEY_F7),

	KEY(1, 0, KEY_DOWN),
	KEY(1, 1, KEY_F1),
	KEY(1, 2, KEY_1),
	KEY(1, 3, KEY_ESC),
	KEY(1, 4, KEY_2),
	KEY(1, 5, KEY_3),
	
	KEY(2, 0, KEY_RIGHT),
	KEY(2, 1, KEY_F2),
	KEY(2, 2, KEY_4),
	KEY(2, 3, KEY_M),
	KEY(2, 4, KEY_5),
	KEY(2, 5, KEY_6),
	
	KEY(3, 0, KEY_UP),
	KEY(3, 1, KEY_F3),
	KEY(3, 2, KEY_7),
	KEY(3, 3, KEY_BACKSPACE),
	KEY(3, 4, KEY_8),
	KEY(3, 5, KEY_9),

	KEY(4, 0, KEY_ENTER),
	KEY(4, 1, KEY_F4),
	KEY(4, 2, KEY_HOME),
	KEY(4, 3, KEY_Q),
	KEY(4, 4, KEY_0),
	KEY(4, 5, KEY_END),
};

static struct matrix_keymap_data ls1bkbd_keymap_data = {
	.keymap			= ls1bkbd_keymap,
	.keymap_size	= ARRAY_SIZE(ls1bkbd_keymap),
};

static const int ls1bkbd_row_gpios[] =
	{ 170, 171, 172, 173, 174 };
static const int ls1bkbd_col_gpios[] =
	{ 175, 176, 177, 178, 179, 180 };

static struct matrix_keypad_platform_data ls1bkbd_pdata = {
	.keymap_data		= &ls1bkbd_keymap_data,
	.row_gpios			= ls1bkbd_row_gpios,
	.col_gpios			= ls1bkbd_col_gpios,
	.num_row_gpios		= ARRAY_SIZE(ls1bkbd_row_gpios),
	.num_col_gpios		= ARRAY_SIZE(ls1bkbd_col_gpios),
	.col_scan_delay_us	= 2,
	.debounce_ms		= 2,
	.active_low			= 1,
	.wakeup				= 1,
	.no_autorepeat		= 1,
};

static struct platform_device ls1bkbd_device = {
	.name	= "matrix-keypad",
	.id		= -1,
	.dev	= {
		.platform_data = &ls1bkbd_pdata,
	},
};
#endif	//#if defined(CONFIG_KEYBOARD_MATRIX) || defined(CONFIG_KEYBOARD_MATRIX_MODULE)

#ifdef CONFIG_USB_CH372
#include <linux/ch372.h>
#define CH372_GPIO_IRQ 60
static void ch372_irq_init(void)
{
	gpio_request(CH372_GPIO_IRQ, "ch372 gpio irq");
	gpio_direction_input(CH372_GPIO_IRQ);
}

static struct ch372_platform_data ch372_pdata = {
	.gpio_outpu = (unsigned int)LS1X_GPIO_OUT0,
//	.gpios_res = 17,
	.gpios_cs = 50,
	.gpios_dc = 51,
	.gpios_rd = 52,
	.gpios_wr = 53,
	
	.gpios_d0 = 42,
	.gpios_d1 = 43,
	.gpios_d2 = 44,
	.gpios_d3 = 45,
	.gpios_d4 = 46,
	.gpios_d5 = 47,
	.gpios_d6 = 48,
	.gpios_d7 = 49,
	.datas_offset = 8,
	.irq = LS1X_GPIO_FIRST_IRQ + CH372_GPIO_IRQ,
};

struct platform_device ch372_usb_device = {
	.name	= "ch372_usb",
	.id		= -1,
	.dev	= {
		.platform_data = &ch372_pdata,
	},
};
#endif //#ifdef CONFIG_USB_CH372


/***********************************************/
static struct platform_device *ls1b_platform_devices[] __initdata = {
	&ls1x_uart_device,

#ifdef CONFIG_LS1X_FB0
	&ls1x_fb0_device,
#endif

#ifdef CONFIG_MTD_NAND_LS1X
	&ls1x_nand_device,
#endif

#ifdef CONFIG_USB_OHCI_HCD_LS1X
	&ls1x_ohci_device,
#endif
#ifdef CONFIG_USB_OHCI_HCD_PLATFORM
	&ls1x_ohci_device,
#endif
#ifdef CONFIG_USB_EHCI_HCD_LS1X
	&ls1x_ehci_device,
#endif

#ifdef CONFIG_STMMAC_ETH
#ifdef CONFIG_LS1X_GMAC0
	&ls1x_gmac0_mac,
	&ls1x_gmac0_phy,
#endif
#ifdef CONFIG_LS1X_GMAC1
	&ls1x_gmac1_mac,
	&ls1x_gmac1_phy,
#endif
#endif

#ifdef CONFIG_SOUND_LS1X_AC97
	&ls1x_audio_device,
#endif

#if defined(CONFIG_SPI_LS1X_SPI0)
	&ls1x_spi0_device,
#elif defined(CONFIG_SPI_GPIO)
	&spi0_gpio_device,
#endif

#if defined(CONFIG_SPI_LS1X_SPI1)
	&ls1x_spi1_device,
#elif defined(CONFIG_SPI_GPIO)
//#elif 0
	&spi1_gpio_device,
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

#if defined(CONFIG_GPIO_PCF857X) || defined(CONFIG_GPIO_PCF857X_MODULE)
#if defined(CONFIG_LEDS_GPIO) || defined(CONFIG_LEDS_GPIO_MODULE)
	&pcf8574_leds,
#endif
#endif

#ifdef CONFIG_GPIO_PCA953X
#if defined(CONFIG_LEDS_GPIO) || defined(CONFIG_LEDS_GPIO_MODULE)
	&pca9555_leds,
#endif
#endif

#ifdef CONFIG_USB_CH372
	&ch372_usb_device,
#endif
};

int __init ls1b_platform_init(void)
{
	ls1x_serial_setup();

#ifdef CONFIG_STMMAC_ETH
	ls1x_gmac_setup();
#endif

#ifdef CONFIG_I2C_LS1X
	i2c_register_board_info(0, ls1x_i2c0_devs, ARRAY_SIZE(ls1x_i2c0_devs));
#endif

#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
	/* 轮询方式或中断方式探测card的插拔 */
	gpio_request(DETECT_GPIO, "MMC_SPI GPIO detect");
	gpio_direction_input(DETECT_GPIO);		/* 输入使能 */
#endif

#if defined(CONFIG_SPI_LS1X_SPI0)
	/* disable gpio24-27 */
	*(volatile unsigned int *)0xbfd010c0 &= ~(0xf << 24);
	spi_register_board_info(ls1x_spi0_devices, ARRAY_SIZE(ls1x_spi0_devices));
#elif defined(CONFIG_SPI_GPIO)
	spi_register_board_info(spi0_gpio_devices, ARRAY_SIZE(spi0_gpio_devices));
#endif

#if defined(CONFIG_SPI_LS1X_SPI1)
	/* 使能SPI1控制器，与CAN0 CAN1 GPIO38-GPIO41复用,同时占用PWM0 PWM1用于片选. */
	/* 编程需要注意 */
	*(volatile unsigned int *)0xbfd00424 |= (0x3 << 23);
	/* disable gpio38-41 */
	*(volatile unsigned int *)0xbfd010c4 &= ~(0xf << 6);
	spi_register_board_info(ls1x_spi1_devices, ARRAY_SIZE(ls1x_spi1_devices));
#elif defined(CONFIG_SPI_GPIO)
//#elif 0
	spi_register_board_info(spi1_gpio_devices, ARRAY_SIZE(spi1_gpio_devices));
#endif

#ifdef CONFIG_USB_CH372
	ch372_irq_init();
#endif

	return platform_add_devices(ls1b_platform_devices, ARRAY_SIZE(ls1b_platform_devices));
}
arch_initcall(ls1b_platform_init);

static struct platform_device *lateinit_platform_devices[] __initdata = {
#ifdef CONFIG_BACKLIGHT_GENERIC
	&ls1x_bl_dev,
#endif
#if defined(CONFIG_KEYBOARD_MATRIX) || defined(CONFIG_KEYBOARD_MATRIX_MODULE)
	&ls1bkbd_device,
#endif
};

static int __init late_init(void)
{
	return platform_add_devices(lateinit_platform_devices, ARRAY_SIZE(lateinit_platform_devices));
}
late_initcall(late_init);
