/*
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
#include <linux/spi/ads7846.h>
#include <linux/spi/mmc_spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/mmc/host.h>
#include <linux/phy.h>
#include <linux/stmmac.h>
#include <linux/i2c.h>
#include <linux/i2c/tsc2007.h>
#include <linux/i2c/ft5x06_ts.h>
#include <linux/videodev2.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/input/matrix_keypad.h>
#include <linux/input/74x165_gpio_keys_polled.h>
#include <linux/rotary_encoder.h>
#include <linux/ssd1305.h>
#include <linux/st7920.h>
#include <linux/clk.h>
#include <linux/jbt6k74.h>
#include <linux/leds.h>
#include <linux/leds_pwm.h>
#include <linux/can/platform/sja1000.h>

#include <video/ls1xfb.h>
#include <media/gc0308_platform.h>

#include <asm/mach-loongson/ls1x/ls1b_board.h>
#include <asm/mach-loongson/ls1x/ls1b_board_int.h>
#include <asm/mach-loongson/ls1x/spi.h>
#include <asm/gpio.h>
#include <asm-generic/sizes.h>

#ifdef CONFIG_MTD_NAND_LS1X
struct ls1b_nand_platform_data{
    int enable_arbiter;
    struct mtd_partition *parts;
    unsigned int nr_parts;
};

#define	SZ_100M	(100*1024*1024)

static struct mtd_partition ls1b_nand_partitions[]={
#if 1
	[0] = {
		.name   ="kernel",
		.offset =MTDPART_OFS_APPEND,
		.size   =0xe00000,
//	        .mask_flags =   MTD_WRITEABLE,
	},
	[1] = {
		.name   ="os",
		.offset = MTDPART_OFS_APPEND,
		.size   = SZ_100M,

	},
	[2] = {
		.name   ="data",
		.offset = MTDPART_OFS_APPEND,
		.size   = MTDPART_SIZ_FULL,
	},
#else
    [0] = {
        .name   ="kernel",
        .offset =0,
        .size   =0xe00000,
//        .mask_flags =   MTD_WRITEABLE,
    },
    [1] = {
        .name   ="os",
        .offset = 0xe00000,
        .size   = 0x7100000,
    
    },
#endif
};

static struct ls1b_nand_platform_data ls1b_nand_parts = {
	.enable_arbiter =   1,
	.parts          =   ls1b_nand_partitions,
	.nr_parts       =   ARRAY_SIZE(ls1b_nand_partitions),
};

static struct resource ls1b_nand_resources[] = {
    [0] = {
        .start      =0,
        .end        =0,
        .flags      =IORESOURCE_DMA,    
    },
    [1] = {
        .start      =0x1fe78000,
        .end        =0x1fe78020,
        .flags      =IORESOURCE_MEM,
    },
    [2] = {
        .start      =0x1fd01160,
        .end        =0x1fd01160,
        .flags      =IORESOURCE_MEM,
    },
    [3] = {
        .start      =LS1B_BOARD_DMA0_IRQ,
        .end        =LS1B_BOARD_DMA0_IRQ,
        .flags      =IORESOURCE_IRQ,
    },
};

struct platform_device ls1b_nand_device = {
    .name       ="ls1b-nand",
    .id         =-1,
    .dev        ={
        .platform_data = &ls1b_nand_parts,
    },
    .num_resources  =ARRAY_SIZE(ls1b_nand_resources),
    .resource       =ls1b_nand_resources,
};
#endif //CONFIG_MTD_NAND_LS1X

static struct plat_serial8250_port uart8250_data[] = {
{
	.mapbase = LS1X_UART0_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1X_UART0_BASE),
	.irq = LS1X_UART0_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1X_UART1_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1X_UART1_BASE),
	.irq = LS1X_UART1_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1X_UART2_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1X_UART2_BASE),
	.irq = LS1X_UART2_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1X_UART3_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1X_UART3_BASE),
	.irq = LS1X_UART3_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
	.iotype = UPIO_MEM,
	.regshift = 0,
},
#ifdef	CONFIG_LS1B_MACH
{
	.mapbase = LS1X_UART4_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1X_UART4_BASE),
	.irq = LS1X_UART4_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1X_UART5_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1X_UART5_BASE),
	.irq = LS1X_UART5_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
	.iotype = UPIO_MEM,
	.regshift = 0,
},
#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL0
{
	.mapbase = LS1X_UART6_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1X_UART6_BASE),
	.irq = LS1X_UART0_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_SHARE_IRQ,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1X_UART7_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1X_UART7_BASE),
	.irq = LS1X_UART0_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_SHARE_IRQ,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1X_UART8_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1X_UART8_BASE),
	.irq = LS1X_UART0_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_SHARE_IRQ,
	.iotype = UPIO_MEM,
	.regshift = 0,
},
#endif
#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL1
{
	.mapbase = LS1X_UART9_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1X_UART9_BASE),
	.irq = LS1X_UART1_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_SHARE_IRQ,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1X_UART10_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1X_UART10_BASE),
	.irq = LS1X_UART1_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_SHARE_IRQ,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1X_UART11_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1X_UART11_BASE),
	.irq = LS1X_UART1_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_SHARE_IRQ,
	.iotype = UPIO_MEM,
	.regshift = 0,
},
#endif
#endif //#ifdef	CONFIG_LS1B_MACH
{.flags = 0,}
};

static struct platform_device uart8250_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = uart8250_data,
	}
};

/*
 * ohci
 */
#ifdef CONFIG_USB_OHCI_HCD_LS1B
static u64 ls1b_ohci_dma_mask = DMA_BIT_MASK(32);
static struct resource ls1b_ohci_resources[] = {
	[0] = {
		.start          = LS1B_USB_OHCI_BASE,
		.end            = (LS1B_USB_OHCI_BASE + 0x100 - 1),
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = LS1B_BOARD_OHCI_IRQ,
		.end            = LS1B_BOARD_OHCI_IRQ,
		.flags          = IORESOURCE_IRQ,
	},
};
/*
static struct ls1b_usbh_data  ls1b_ohci_platform_data={
#ifdef CONFIG_LS1A_MACH
	.ports=4,
#else
	.ports=1,
#endif
};
*/
static struct platform_device ls1b_ohci_device = {
	.name           = "ls1b-ohci",
	.id             = 0,
	.dev = {
//		.platform_data = &ls1b_ohci_platform_data,
		.dma_mask = &ls1b_ohci_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.num_resources  = ARRAY_SIZE(ls1b_ohci_resources),
	.resource       = ls1b_ohci_resources,
};
#endif //#ifdef CONFIG_USB_OHCI_HCD_LS1B

/*
 * ehci
 */
#ifdef CONFIG_USB_EHCI_HCD_LS1B
static u64 ls1b_ehci_dma_mask = DMA_BIT_MASK(32);
static struct resource ls1b_ehci_resources[] = { 
	[0] = {
		.start          = LS1B_USB_EHCI_BASE,
		.end            = (LS1B_USB_EHCI_BASE + 0x100 - 1),
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = LS1B_BOARD_EHCI_IRQ,
		.end            = LS1B_BOARD_EHCI_IRQ,
		.flags          = IORESOURCE_IRQ,
	},
};
/*
static struct ls1b_usbh_data  ls1b_ehci_platform_data={
#ifdef CONFIG_LS1A_MACH
	.ports=4,
#else
	.ports=1,
#endif
};
*/
static struct platform_device ls1b_ehci_device = {
	.name           = "ls1b-ehci",
	.id             = 0,
	.dev = {
//		.platform_data = &ls1b_ehci_platform_data,
		.dma_mask = &ls1b_ehci_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.num_resources  = ARRAY_SIZE(ls1b_ehci_resources),
	.resource       = ls1b_ehci_resources,
};
#endif //#ifdef CONFIG_USB_EHCI_HCD_LS1B


/*
* watchdog
*/
#ifdef CONFIG_LS1X_WDT
static struct resource ls1b_wat_resource[] = {
	[0]={
		.start      = LS1X_WAT_BASE,
		.end        = (LS1X_WAT_BASE + 0x8),
		.flags      = IORESOURCE_MEM,
	},
};

static struct platform_device ls1b_wat_device = {
	.name       = "ls1b-wdt",
	.id         = -1,
	.num_resources  = ARRAY_SIZE(ls1b_wat_resource),
	.resource   = ls1b_wat_resource,
};
#endif //#ifdef CONFIG_LS1X_WDT

/*
*RTC
*/
#ifdef CONFIG_RTC_DRV_LOONGSON1
static struct platform_device ls1x_rtc_device = {
	.name       = "ls1x-rtc",
	.id         = -1,
};
#endif //#ifdef CONFIG_RTC_DRV_LOONGSON1

/*
*I2C
*/
/* I2C devices fitted. */
#ifdef CONFIG_TOUCHSCREEN_TSC2007
#define TSC2007_GPIO_IRQ	60
static int ts_get_pendown_state(void)
{
	return !gpio_get_value(TSC2007_GPIO_IRQ);
}

int ts_init(void)
{
	ls1b_gpio_direction_input(NULL, TSC2007_GPIO_IRQ);		/* 输入使能 */
	return 0;
}

static struct tsc2007_platform_data tsc2007_info = {
	.model				= 2007,
	.x_plate_ohms		= 180,
	.get_pendown_state	= ts_get_pendown_state,
	.init_platform_hw	= ts_init,
};
#endif //#ifdef CONFIG_TOUCHSCREEN_TSC2007

#ifdef CONFIG_TOUCHSCREEN_FT5X0X
#define FT5X0X_GPIO_IRQ		38
#define FT5X0X_GPIO_WAUP	39
int ft5x0x_irq_init(void)
{
	ls1b_gpio_direction_input(NULL, FT5X0X_GPIO_IRQ);		/* 输入使能 */
	return 0;
}

int ft5x0x_wake_up(void)
{
	ls1b_gpio_direction_output(NULL, FT5X0X_GPIO_WAUP, 0);		/* 输出使能 */
	msleep(10);
	gpio_set_value(FT5X0X_GPIO_WAUP, 1);
	msleep(10);
	return 0;
}

static struct ft5x0x_ts_platform_data ft5x0x_info = {
	.init_platform_hw	= ft5x0x_irq_init,
	.wake_platform_hw	= ft5x0x_wake_up,
};
#endif //#ifdef CONFIG_TOUCHSCREEN_FT5X0X

#ifdef CONFIG_VIDEO_GC0308
static struct gc0308_platform_data gc0308_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_YUYV,
	.freq = 24000000,
	.is_mipi = 0,
};
#endif //#ifdef CONFIG_VIDEO_GC0308

#ifdef CONFIG_LS1B_I2C
static struct i2c_board_info __initdata ls1b_i2c_devs[] = {
#ifdef CONFIG_TOUCHSCREEN_TSC2007
	{
		I2C_BOARD_INFO("tsc2007", 0x48),
		.irq = LS1X_GPIO_FIRST_IRQ + TSC2007_GPIO_IRQ,
		.platform_data	= &tsc2007_info,
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_FT5X0X
	{
		I2C_BOARD_INFO(FT5X0X_NAME, 0x38),
		.irq = LS1X_GPIO_FIRST_IRQ + FT5X0X_GPIO_IRQ,
		.platform_data	= &ft5x0x_info,
	},
#endif
#ifdef CONFIG_VIDEO_GC0308
	{
		I2C_BOARD_INFO("GC0308", 0x42 >> 1),
		.platform_data = &gc0308_plat,
	},
#endif
};

static struct resource ls1b_i2c_resource[] = {
	[0]={
		.start	= LS1B_BOARD_I2C_BASE,
		.end	= (LS1B_BOARD_I2C_BASE + 0x4),
		.flags	= IORESOURCE_MEM,
	},
/*	
	[1]={
		.start	= LS1B_BOARD_I2C0_IRQ,
		.end	= LS1B_BOARD_I2C0_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
*/
};

static struct platform_device ls1b_i2c_device = {
	.name		= "ls1b-i2c",
	.id			= 0,
	.num_resources	= ARRAY_SIZE(ls1b_i2c_resource),
	.resource	= ls1b_i2c_resource,
};
#endif //#ifdef CONFIG_LS1B_I2C

/*
 * lcd
 */
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
	.active			= 1,
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

#define GPIO_BACKLIGHT_CTRL	53
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

#ifdef CONFIG_LCD_PLATFORM
#include <video/platform_lcd.h>
static void ls1x_lcd_power_set(struct plat_lcd_data *pd,
				   unsigned int power)
{
	if (power)
		gpio_direction_output(GPIO_BACKLIGHT_CTRL, 1);
	else
		gpio_direction_output(GPIO_BACKLIGHT_CTRL, 0);
}

static struct plat_lcd_data ls1x_lcd_power_data = {
	.set_power		= ls1x_lcd_power_set,
};

static struct platform_device ls1x_lcd_powerdev = {
	.name			= "platform-lcd",
	.dev.parent		= &ls1x_fb0_device.dev,
	.dev.platform_data	= &ls1x_lcd_power_data,
};
#endif //#ifdef CONFIG_LCD_PLATFORM

//gmac0
#ifdef CONFIG_LS1B_GMAC0_OPEN
static struct resource ls1b_mac0_resources[] = {
	[0] = {
		.start  = LS1B_BOARD_GMAC1_BASE,
		.end    = LS1B_BOARD_GMAC1_BASE + SZ_8K - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.name   = "macirq",
		.start  = LS1B_BOARD_GMAC1_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct plat_stmmacenet_data ls1b_mac0_data = {
	.bus_id = 0,
	.has_gmac = 1,
	.enh_desc = 0,
};

struct platform_device ls1b_gmac0_mac = {
	.name           = "stmmaceth",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(ls1b_mac0_resources),
	.resource       = ls1b_mac0_resources,
	.dev            = {
		.platform_data = &ls1b_mac0_data,
	},
};

static struct plat_stmmacphy_data  phy0_private_data = {
#ifdef CONFIG_LS1A_MACH
#ifdef	CONFIG_LS1A_CORE_BOARD
	.bus_id = 0,
	.phy_addr = 0,
#else
	.bus_id = 0,
	.phy_addr = 1,
#endif
#else
	.bus_id = 0,
#ifdef CONFIG_RTL8305SC
	.phy_addr = 4,
#else
	.phy_addr = 0,
#endif
#endif
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	
};

struct platform_device ls1b_gmac0_phy = {
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
#endif //#ifdef CONFIG_LS1B_GMAC0_OPEN

//gmac1
#ifdef CONFIG_LS1B_GMAC1_OPEN
static struct resource ls1b_mac1_resources[] = {
	[0] = {
		.start  = LS1B_BOARD_GMAC2_BASE,
		.end    = LS1B_BOARD_GMAC2_BASE + SZ_8K - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.name   = "macirq",
		.start  = LS1B_BOARD_GMAC2_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};


static struct plat_stmmacenet_data ls1b_mac1_data = {
	.bus_id = 1,
	.has_gmac = 1,
	.enh_desc = 0,
};

struct platform_device ls1b_gmac1_mac = {
	.name           = "stmmaceth",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(ls1b_mac1_resources),
	.resource       = ls1b_mac1_resources,
	.dev            = {
		.platform_data = &ls1b_mac1_data,
	},
};

static struct plat_stmmacphy_data  phy1_private_data = {
	.bus_id = 1,
	.phy_addr = 1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	
};

struct platform_device ls1b_gmac1_phy = {
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
#endif //#ifdef CONFIG_LS1B_GMAC1_OPEN

#ifdef CONFIG_SND_LS1B
static struct platform_device ls1b_audio_device = {
	.name           = "ls1b-audio",
	.id             = -1,
};
#endif

#ifdef CONFIG_MTD_M25P80
static struct mtd_partition partitions[] = {
	[0] = {
		.name		= "pmon_spi",
		.offset		= 0,
		.size		= 512 * 1024,	//512KB
	}, 
};

static struct flash_platform_data flash = {
	.name		= "ls1b_norflash",
	.parts		= partitions,
	.nr_parts	= ARRAY_SIZE(partitions),
	.type		= "w25q64",
};
#endif /* CONFIG_MTD_M25P80 */

#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
/* 开发板使用GPIO40(CAN1_RX)引脚作为MMC/SD卡的插拔探测引脚 */
#define DETECT_GPIO  41
//#define DETECT_GPIO  56
/* 轮询方式探测card的插拔 */
static int mmc_spi_get_cd(struct device *dev)
{
	return !gpio_get_value(DETECT_GPIO);
}

#if 0
#define MMC_SPI_CARD_DETECT_INT  (LS1X_GPIO_FIRST_IRQ + DETECT_GPIO)
/* 中断方式方式探测card的插拔 */
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
	.detect_delay = 1200,	/* msecs */
	/* 轮询方式方式探测card的插拔 */
	.get_cd = mmc_spi_get_cd,
	.caps = MMC_CAP_NEEDS_POLL,
};	
#endif  /* defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE) */

#ifdef CONFIG_TOUCHSCREEN_ADS7846
#define ADS7846_GPIO_IRQ 60 /* 开发板触摸屏使用的外部中断 */
int ads7846_pendown_state(void)
{
	return !gpio_get_value(ADS7846_GPIO_IRQ);
}
	
int ads7846_detect_penirq(void)
{
	//配置GPIO0
	ls1b_gpio_direction_input(NULL, ADS7846_GPIO_IRQ);		/* 输入使能 */

	return (LS1X_GPIO_FIRST_IRQ + ADS7846_GPIO_IRQ);
}
	
static struct ads7846_platform_data ads_info = {
	.model				= 7846,
	.vref_delay_usecs	= 1,
	.keep_vref_on		= 0,
	.x_plate_ohms		= 800,
	.pressure_min		= 0,
	.pressure_max		= 15000,
	.debounce_rep		= 3,
	.debounce_max		= 10,
	.debounce_tol		= 50,
	.get_pendown_state	= ads7846_pendown_state,
	.filter_init		= NULL,
	.filter 			= NULL,
	.filter_cleanup 	= NULL,
};
#endif /* TOUCHSCREEN_ADS7846 */

#ifdef CONFIG_LCD_JBT6K74
/* JBT6k74 display controller */
static void gta02_jbt6k74_probe_completed(struct device *dev)
{
//	pcf50633_bl_set_brightness_limit(gta02_pcf, 0x3f);
}

const struct jbt6k74_platform_data jbt6k74_pdata = {
//	.gpio_reset = 41,
};
#endif

#ifdef CONFIG_LS1B_SPI0
static struct spi_board_info ls1b_spi0_devices[] = {
#ifdef CONFIG_MTD_M25P80
	{	/* DataFlash chip */
		.modalias	= "w25q64",		//"m25p80",
		.bus_num 		= 0,
		.chip_select	= SPI0_CS0,
		.max_speed_hz	= 80000000,
		.platform_data	= &flash,
	},
#endif
#ifdef CONFIG_SPI_MCP3201
	{	/* ADC chip */
		.modalias	= "mcp3201",
		.bus_num 		= 0,
		.chip_select	= SPI0_CS3,
		.max_speed_hz	= 80000000,
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_ADS7846
	{
		.modalias = "ads7846",
		.platform_data = &ads_info,
		.bus_num 		= 0,
		.chip_select 	= SPI0_CS1,
		.max_speed_hz 	= 2500000,
		.mode 			= SPI_MODE_1,
		.irq				= LS1X_GPIO_FIRST_IRQ + ADS7846_GPIO_IRQ,
	},
#endif
#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
	{	/* mmc/sd card */
		.modalias		= "mmc_spi",		//mmc spi,
		.bus_num 		= 0,
		.chip_select	= SPI0_CS2,
		.max_speed_hz	= 25000000,
		.platform_data	= &mmc_spi,
	},
#endif
};
	
static struct resource ls1b_spi0_resource[] = {
	[0]={
		.start	= LS1B_BOARD_SPI0_BASE,
		.end	= (LS1B_BOARD_SPI0_BASE + 0x6),
		.flags	= IORESOURCE_MEM,
	},
	[1]={
		.start	= LS1B_BOARD_SPI0_IRQ,
		.end	= LS1B_BOARD_SPI0_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct ls1b_spi_info ls1b_spi0_platdata = {
	.board_size = ARRAY_SIZE(ls1b_spi0_devices),
	.board_info = ls1b_spi0_devices,
	.bus_num	= 0,
	.num_cs		= SPI0_CS3 + 1,
};

static struct platform_device ls1b_spi0_device = {
	.name		= "ls1b-spi",
	.id 		= 0,
	.num_resources	= ARRAY_SIZE(ls1b_spi0_resource),
	.resource	= ls1b_spi0_resource,
	.dev		= {
		.platform_data	= &ls1b_spi0_platdata,//&ls1b_spi_devices,
	},
};
#endif //#ifdef CONFIG_LS1B_SPI0

#ifdef CONFIG_LS1B_SPI1 /* SPI1 控制器 */
static struct spi_board_info ls1b_spi1_devices[] = {
#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
	{	/* mmc/sd card */
		.modalias		= "mmc_spi",		//mmc spi,
		.bus_num 		= 1,
		.chip_select	= SPI1_CS0,
		.max_speed_hz	= 25000000,
		.platform_data	= &mmc_spi,
	},
#endif

#if defined(CONFIG_EASY_DAB_AUDIO)
	{	
		.modalias		= "easy_dab",
		.bus_num 		= 1,
		.chip_select	= SPI1_CS1,
		.max_speed_hz	= 10000000,
	},
#endif
};

static struct resource ls1b_spi1_resource[] = {
	[0]={
		.start	= LS1B_BOARD_SPI1_BASE,
		.end	= (LS1B_BOARD_SPI1_BASE + 0x6),
		.flags	= IORESOURCE_MEM,
	},
	[1]={
		.start	= LS1B_BOARD_SPI1_IRQ,
		.end	= LS1B_BOARD_SPI1_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct ls1b_spi_info ls1b_spi1_platdata = {
	.board_size = ARRAY_SIZE(ls1b_spi1_devices),
	.board_info = ls1b_spi1_devices,
	.bus_num	= 1,
	.num_cs		= 3,
};

static struct platform_device ls1b_spi1_device = {
	.name		= "ls1b-spi",
	.id 		= 1,
	.num_resources	= ARRAY_SIZE(ls1b_spi1_resource),
	.resource	= ls1b_spi1_resource,
	.dev		= {
		.platform_data	= &ls1b_spi1_platdata,//&ls1b_spi_devices,
	},
};
#endif	//#ifdef CONFIG_LS1B_SPI1

#ifdef CONFIG_SPI_GPIO
struct spi_gpio_platform_data spigpio_platform_data = {
	.sck = 24,	/*gpio24*/
	.mosi = 26,	/*gpio26*/
	.miso = 25,	/*gpio25*/
	.num_chipselect = 3,
};

static struct platform_device spigpio_device = {
	.name = "spi_gpio",
	.id   = 2,	/* 用于区分spi0和spi1 */
	.dev = {
		.platform_data = &spigpio_platform_data,
	},
};

static struct spi_board_info spi_gpio_devices[] = {
#ifdef CONFIG_MTD_M25P80
	{	/* DataFlash chip */
		.modalias	= "w25q64",		//"m25p80",
		.bus_num 		= 2,	/* 对应spigpio_device的.id=2 */
		.controller_data = 27,	/*gpio27*/
		.chip_select	= 0,
		.max_speed_hz	= 80000000,
		.platform_data	= &flash,
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_ADS7846
	{
		.modalias = "ads7846",
		.platform_data = &ads_info,
		.bus_num 		= 2,	/* 对应spigpio_device的.id=2 */
		.controller_data = 28,	/*gpio28*/
		.chip_select 	= 1,
		.max_speed_hz 	= 2500000,
		.mode 			= SPI_MODE_1,
		.irq			= LS1X_GPIO_FIRST_IRQ + ADS7846_GPIO_IRQ,
	},
#endif
#ifdef CONFIG_LCD_JBT6K74
	{
		.modalias	= "jbt6k74",
		.platform_data	= &jbt6k74_pdata,
		.bus_num	= 2,		/* 对应spigpio_device的.id=2 */
		.controller_data = 43,	/*gpio43*/
		.chip_select = 2,
		/* irq */
		.max_speed_hz	= 100 * 1000,
	},
#endif
};
#endif //#ifdef CONFIG_SPI_GPIO

/************************************************/	//GPIO && buzzer && button
#ifdef CONFIG_KEYBOARD_GPIO_POLLED
static struct gpio_keys_button ls1b_gpio_button[] = {
	[0] = {
		.code		= 'A',
		.gpio	 	= 37,
		.desc		= "SW1",
	},
	[1] = {
		.code		= 'B',
		.gpio	 	= 38,
		.desc		= "SW2",
	},
	[2] = {
		.code		= 'C',
		.gpio	 	= 39,
		.desc		= "SW3",
	},
	[3] = {
		.code		= 'D',
		.gpio	 	= 40,
		.desc		= "SW4",
	},
	[4] = {
		.code		= 'E',
		.gpio	 	= 41,
		.desc		= "SW5",
	},
};

static struct gpio_keys_platform_data ls1b_gpio_key_dat = {
	.buttons 		= ls1b_gpio_button,
	.nbuttons 		= 5, 
	.poll_interval	= 200,
};

static struct platform_device ls1b_gpio_key_device = {
	.name 	= "gpio-keys-polled",
	.id	= -1,
	.dev	= {
		.platform_data = &ls1b_gpio_key_dat,
	},
};
#endif //#ifdef CONFIG_KEYBOARD_GPIO_POLLED

#ifdef CONFIG_LS1B_BUZZER
static struct gpio_keys_button ls1b_gpio_buzzer[] = {
	[0] = {
		.gpio	= 40,	//57
		.type	= EV_LED,
	},	
/*
	[1] = {
		.gpio	= 34,
	},	
	[2] = {
		.gpio	= 35,
	},	*/
};

static struct gpio_keys_platform_data ls1b_gpio_buzzer_data = {
	.buttons	= ls1b_gpio_buzzer,
	.nbuttons	= 1,
};

static struct platform_device ls1b_gpio_buzzer_device = {
	.name	= "buzzer_gpio",
	.id	= -1,
	.dev	= {
		.platform_data = &ls1b_gpio_buzzer_data,
	},
};
#endif //#ifdef CONFIG_LS1B_BUZZER

#ifdef CONFIG_LS1B_IR
#define GPIO_IR 61
static struct resource ls1b_ir_resource[] = {
	[0] = {
		.start	= (LS1X_GPIO_FIRST_IRQ + GPIO_IR),
		.end	= (LS1X_GPIO_FIRST_IRQ + GPIO_IR),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device ls1b_ir_device = {
	.name	= "ls1b_ir",
	.id	= -1,
	.num_resources	= ARRAY_SIZE(ls1b_ir_resource),
	.resource	= ls1b_ir_resource,
};
#endif //#ifdef CONFIG_LS1B_IR

//xhm
#ifdef CONFIG_LS1B_BBDIO
static struct gpio_keys_button ls1b_bobodogio_button[] = {
#if 0
	[0]  = { .gpio = 4, .type = EV_LED},	
	[1]  = { .gpio = 5, .type = EV_LED},
	[2]  = { .gpio = 6, .type = EV_LED},
	[3]  = { .gpio = 7, .type = EV_LED},
	[4]  = { .gpio = 8, .type = EV_LED},
	[5]  = { .gpio = 9, .type = EV_LED},
	[6]  = { .gpio = 10, .type = EV_LED},
	[7]  = { .gpio = 11, .type = EV_LED},
	[8]  = { .gpio = 12, .type = EV_LED},
	[9]  = { .gpio = 13, .type = EV_LED},
	[10] = { .gpio = 14, .type = EV_LED},
	[11] = { .gpio = 15, .type = EV_LED},
	[12] = { .gpio = 16, .type = EV_LED},
	[13] = { .gpio = 17, .type = EV_LED},
	[14] = { .gpio = 18, .type = EV_LED},
#endif
	[0] = { .gpio = 48, .type = EV_KEY},
	[1] = { .gpio = 49, .type = EV_LED},
};

static struct gpio_keys_platform_data ls1b_bobodogio_dog_data = {
	.buttons	= ls1b_bobodogio_button,
	.nbuttons	= 15,
};

static struct platform_device ls1b_bobodogio_dog = {
	.name	= "bobodog_io_control",
	.id	= -1,
	.dev	= {
		.platform_data = &ls1b_bobodogio_dog_data,
	},
};
#endif //#ifdef CONFIG_LS1B_BBDIO

#ifdef CONFIG_LS1B_PWM_DRIVER
static struct resource ls1b_pwm0_resource[] = {
	[0]={
		.start	= LS1B_PWM0_BASE,
		.end	= (LS1B_PWM0_BASE + 0x0f),
		.flags	= IORESOURCE_MEM,
	},
	[1]={
		.start	= LS1B_PWM1_BASE,
		.end	= (LS1B_PWM1_BASE + 0x0f),
		.flags	= IORESOURCE_MEM,
	},
	[2]={
		.start	= LS1B_PWM2_BASE,
		.end	= (LS1B_PWM2_BASE + 0x0f),
		.flags	= IORESOURCE_MEM,
	},
	[3]={
		.start	= LS1B_PWM3_BASE,
		.end	= (LS1B_PWM3_BASE + 0x0f),
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ls1b_pwm_device = {
	.name	= "ls1b-pwm",
	.id	= -1,
	.num_resources	= ARRAY_SIZE(ls1b_pwm0_resource),
	.resource	= ls1b_pwm0_resource,
};
#endif //#ifdef CONFIG_LS1B_PWM_DRIVER

/**
 * Rotary encoder input device
 */
#ifdef CONFIG_INPUT_GPIO_ROTARY_ENCODER
#define GPIO_ROTARY_A 59
#define GPIO_ROTARY_B 51
#define GPIO_KEY_C 53

static struct rotary_encoder_platform_data raumfeld_rotary_encoder_info = {
	.steps		= 30,
	.axis		= REL_X,
	.relative_axis	= 1,
	.rollover	= false,
	.gpio_a		= GPIO_ROTARY_A,
	.gpio_b		= GPIO_ROTARY_B,
	.gpio_c		= GPIO_KEY_C,
	.debounce_ms	= 10,
	.active_low		= 1,
	.key		= KEY_ENTER,
	.inverted_a	= 0,
	.inverted_b	= 0,
	.half_period	= 1,
};

static struct platform_device rotary_encoder_device = {
	.name		= "rotary-encoder",
	.id		= 0,
	.dev		= {
		.platform_data = &raumfeld_rotary_encoder_info,
	}
};
#endif //#ifdef CONFIG_INPUT_GPIO_ROTARY_ENCODER

/* matrix keypad */
#if defined(CONFIG_KEYBOARD_MATRIX) || defined(CONFIG_KEYBOARD_MATRIX_MODULE)
/*
static const uint32_t ls1bkbd_keymap[] = {
	KEY(0, 0, KEY_A),
	KEY(0, 1, KEY_B),
	KEY(0, 2, KEY_C),
	KEY(0, 3, KEY_D),

	KEY(1, 0, KEY_E),
	KEY(1, 1, KEY_F),
	KEY(1, 2, KEY_G),
	KEY(1, 3, KEY_H),
};
*/
static const uint32_t ls1bkbd_keymap[] = {
	KEY(0, 0, KEY_1),
	KEY(0, 1, KEY_2),
	KEY(0, 2, KEY_3),
	KEY(0, 3, KEY_4),

	KEY(1, 0, KEY_5),
	KEY(1, 1, KEY_6),
	KEY(1, 2, KEY_7),
	KEY(1, 3, KEY_8),
	
	KEY(2, 0, KEY_9),
	KEY(2, 1, KEY_0),
	KEY(2, 2, KEY_A),
	KEY(2, 3, KEY_B),
	
	KEY(3, 0, KEY_C),
	KEY(3, 1, KEY_D),
	KEY(3, 2, KEY_E),
	KEY(3, 3, KEY_F),
};

static struct matrix_keymap_data ls1bkbd_keymap_data = {
	.keymap			= ls1bkbd_keymap,
	.keymap_size	= ARRAY_SIZE(ls1bkbd_keymap),
};
/*
static const int ls1bkbd_row_gpios[] =
	{ 30, 28 };
static const int ls1bkbd_col_gpios[] =
	{ 29, 58, 50, 52 };
*/
static const int ls1bkbd_row_gpios[] =
	{ 16, 17, 18, 19 };	//gpio 16 17 18 19
static const int ls1bkbd_col_gpios[] =
	{ 20, 21, 22, 23 };	//gpio 20 21 22 23

static struct matrix_keypad_platform_data ls1bkbd_pdata = {
	.keymap_data		= &ls1bkbd_keymap_data,
	.row_gpios			= ls1bkbd_row_gpios,
	.col_gpios			= ls1bkbd_col_gpios,
	.num_row_gpios		= ARRAY_SIZE(ls1bkbd_row_gpios),
	.num_col_gpios		= ARRAY_SIZE(ls1bkbd_col_gpios),
	.col_scan_delay_us	= 5,
	.debounce_ms		= 25,
	.active_low			= 1,
	.wakeup				= 1,
	.no_autorepeat		= 0,
};

static struct platform_device ls1bkbd_device = {
	.name	= "matrix-keypad",
	.id		= -1,
	.dev	= {
		.platform_data = &ls1bkbd_pdata,
	},
};
#endif	//#if defined(CONFIG_KEYBOARD_MATRIX) || defined(CONFIG_KEYBOARD_MATRIX_MODULE)

#ifdef CONFIG_FB_SSD1305
static struct ssd1305_platform_data ssd1305_pdata = {
	.gpio_outpu = REG_GPIO_OUT0,
	.gpios_res = 17,
	.gpios_cs = 16,
	.gpios_dc = 18,
	.gpios_rd = 20,
	.gpios_wr = 19,
	
	.gpios_d0 = 8,
	.gpios_d1 = 9,
	.gpios_d2 = 10,
	.gpios_d3 = 11,
	.gpios_d4 = 12,
	.gpios_d5 = 13,
	.gpios_d6 = 14,
	.gpios_d7 = 15,
	.datas_offset = 8,
};

struct platform_device ssd1305fb_device = {
	.name	= "ssd1305fb",
	.id		= -1,
	.dev	= {
		.platform_data = &ssd1305_pdata,
	},
};
#endif //#ifdef CONFIG_FB_SSD1305

#ifdef CONFIG_FB_ST7920
static struct st7920_platform_data st7920_pdata = {
	.gpio_outpu = REG_GPIO_OUT0,
	.gpios_res = 8,
	.gpios_cs = 11,
	.gpios_sid = 9,
	.gpios_sck = 10,

	.datas_offset = 8,
};

struct platform_device st7920fb_device = {
	.name	= "st7920fb",
	.id		= -1,
	.dev	= {
		.platform_data = &st7920_pdata,
	},
};
#endif //#ifdef CONFIG_FB_ST7920

#ifdef CONFIG_KEYBOARD_74X165_GPIO_POLLED
static struct gpio_keys_button gen74x165_gpio_keys_table[] = {
	{
		.code		= KEY_0,
		.active_low	= 1,
	}, {
		.code		= KEY_1,
		.active_low	= 1,
	}, {
		.code		= KEY_2,
		.active_low	= 1,
	}, {
		.code		= KEY_3,
		.active_low	= 1,
	}, {
		.code		= KEY_4,
		.active_low	= 1,
	}, {
		.code		= KEY_5,
		.active_low	= 1,
	}, {
		.code		= KEY_6,
		.active_low	= 1,
	}, {
		.code		= KEY_7,
		.active_low	= 1,
	}, {
		.code		= KEY_8,
		.active_low	= 1,
	}, {
		.code		= KEY_9,
		.active_low	= 1,
	}, {
		.code		= KEY_A,
		.active_low	= 1,
	}, {
		.code		= KEY_B,
		.active_low	= 1,
	}, {
		.code		= KEY_C,
		.active_low	= 1,
	}, {
		.code		= KEY_D,
		.active_low	= 1,
	}, {
		.code		= KEY_E,
		.active_low	= 1,
	}, {
		.code		= KEY_F,
		.active_low	= 1,
	},
};

static struct gen_74x165_platform_data gen74x165_gpio_keys_info = {
	.q7 = 41,
	.cp = 39,
	.pl = 38,
	.debounce_interval = 1,
	.buttons	= gen74x165_gpio_keys_table,
	.nbuttons	= ARRAY_SIZE(gen74x165_gpio_keys_table),
	.poll_interval	= 50, /* default to 50ms */
};

static struct platform_device gen74x165_gpio_keys_device = {
	.name		= "gen74x165_gpio-keys-polled",
	.dev		= {
		.platform_data	= &gen74x165_gpio_keys_info,
	},
};
#endif //#ifdef CONFIG_KEYBOARD_74X165_GPIO_POLLED

#ifdef CONFIG_LEDS_PWM
static struct led_pwm ls1x_pwm_leds[] = {
	{
		.name		= "ls1x_pwm_led1",
		.pwm_id		= 2,
		.max_brightness	= 255,
		.pwm_period_ns	= 7812500,
	},
	{
		.name		= "ls1x_pwm_led2",
		.pwm_id		= 3,
		.max_brightness	= 255,
		.pwm_period_ns	= 7812500,
	},
};

static struct led_pwm_platform_data ls1x_pwm_data = {
	.num_leds	= ARRAY_SIZE(ls1x_pwm_leds),
	.leds		= ls1x_pwm_leds,
};

static struct platform_device ls1x_leds_pwm = {
	.name	= "leds_pwm",
	.id		= -1,
	.dev	= {
		.platform_data = &ls1x_pwm_data,
	},
};
#endif //#ifdef CONFIG_LEDS_PWM

#ifdef CONFIG_CAN_SJA1000_PLATFORM
#ifdef CONFIG_LS1X_CAN0
static struct resource ls1x_sja1000_resources_0[] = {
	{
		.start   = LS1X_CAN0_BASE,
		.end     = LS1X_CAN0_BASE + 0x100 - 1,
		.flags   = IORESOURCE_MEM | IORESOURCE_MEM_8BIT,
	}, {
		.start   = LS1X_BOARD_CAN0_IRQ,
		.end     = LS1X_BOARD_CAN0_IRQ,
		.flags   = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE,
	},
};

static struct sja1000_platform_data ls1x_sja1000_platform_data_0 = {
//	.osc_freq	= 16000000,
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
		.end     = LS1X_CAN1_BASE + 0x100 - 1,
		.flags   = IORESOURCE_MEM | IORESOURCE_MEM_8BIT,
	}, {
		.start   = LS1X_BOARD_CAN1_IRQ,
		.end     = LS1X_BOARD_CAN1_IRQ,
		.flags   = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE,
	},
};

static struct sja1000_platform_data ls1x_sja1000_platform_data_1 = {
//	.osc_freq	= 16000000,
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
#endif //#ifdef CONFIG_CAN_SJA1000_PLATFORM


/***********************************************/
static struct platform_device *ls1b_platform_devices[] __initdata = {
	&uart8250_device,

#ifdef CONFIG_LS1X_FB0
	&ls1x_fb0_device,
#endif

#ifdef CONFIG_MTD_NAND_LS1X
	&ls1b_nand_device,
#endif

#ifdef CONFIG_USB_OHCI_HCD_LS1B
	&ls1b_ohci_device,
#endif
#ifdef CONFIG_USB_EHCI_HCD_LS1B
	&ls1b_ehci_device,
#endif

#ifdef CONFIG_LS1B_GMAC0_OPEN   //lv
	&ls1b_gmac0_mac,
	&ls1b_gmac0_phy,
#endif
#ifdef CONFIG_LS1B_GMAC1_OPEN  //lv
	&ls1b_gmac1_mac,
	&ls1b_gmac1_phy,
#endif

#ifdef CONFIG_LS1X_WDT
	&ls1b_wat_device,
#endif

#ifdef CONFIG_RTC_DRV_LOONGSON1
	&ls1x_rtc_device,
#endif

#ifdef CONFIG_LS1B_I2C
	&ls1b_i2c_device,
#endif

#ifdef CONFIG_SND_LS1B
	&ls1b_audio_device,
#endif

#ifdef CONFIG_LS1B_SPI0
	&ls1b_spi0_device,
#endif
#ifdef CONFIG_LS1B_SPI1
	&ls1b_spi1_device,
#endif

#ifdef CONFIG_SPI_GPIO
	&spigpio_device,
#endif

#ifdef CONFIG_KEYBOARD_GPIO_POLLED
	&ls1b_gpio_key_device,
#endif
	
#ifdef CONFIG_LS1B_BUZZER
	&ls1b_gpio_buzzer_device,
#endif

#ifdef CONFIG_LS1B_PWM_DRIVER
	&ls1b_pwm_device,
#endif

#ifdef CONFIG_LS1B_IR
	&ls1b_ir_device,//zwl
#endif

#ifdef CONFIG_LS1B_BBDIO
	&ls1b_bobodogio_dog,
#endif

#ifdef CONFIG_INPUT_GPIO_ROTARY_ENCODER
	&rotary_encoder_device,
#endif

#if defined(CONFIG_KEYBOARD_MATRIX) || defined(CONFIG_KEYBOARD_MATRIX_MODULE)
	&ls1bkbd_device,
#endif

#ifdef CONFIG_FB_SSD1305
	&ssd1305fb_device,
#endif

#ifdef CONFIG_FB_ST7920
	&st7920fb_device,
#endif

#ifdef CONFIG_KEYBOARD_74X165_GPIO_POLLED
	&gen74x165_gpio_keys_device,
#endif

#ifdef CONFIG_LEDS_PWM
	&ls1x_leds_pwm,
#endif

#ifdef CONFIG_CAN_SJA1000_PLATFORM
#ifdef CONFIG_LS1X_CAN0
	&ls1x_sja1000_0,
#endif
#ifdef CONFIG_LS1X_CAN1
	&ls1x_sja1000_1,
#endif
#endif

#ifdef CONFIG_BACKLIGHT_GENERIC
	&ls1x_bl_dev
#endif
#ifdef CONFIG_LCD_PLATFORM
	&ls1x_lcd_powerdev,
#endif
};

int __init ls1b_platform_init(void)
{
	struct clk *clk;
	struct plat_serial8250_port *p;

#ifdef CONFIG_LS1A_MACH
	*(volatile int *)0xbfd00420 |= 0x200000;/* disable USB */
	*(volatile int *)0xbff10204 &= ~0x40000000;/* ls1a usb reset */
#elif CONFIG_LS1B_MACH
	*(volatile int *)0xbfd00424 |= 0x800;/* disable USB */
	*(volatile int *)0xbfd00424 &= ~0x80000000;/* ls1g usb reset */
#endif

#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL0
	(*(volatile unsigned char *)(LS1B_UART_SPLIT)) = 0x1;
#endif
#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL1
	(*(volatile unsigned char *)(LS1B_UART_SPLIT)) |= 0x2;

	(*(volatile unsigned int *)(LS1B_GPIO_MUX_CTRL1)) |= 0x30;
#endif

	/* uart clock */
	clk = clk_get(NULL, "ddr");
	if (IS_ERR(clk))
		panic("unable to get dc clock, err=%ld", PTR_ERR(clk));

	for (p = uart8250_data; p->flags != 0; ++p)
		p->uartclk = clk_get_rate(clk) / 2;

#ifdef CONFIG_CAN_SJA1000_PLATFORM
	{
	#ifdef CONFIG_LS1X_CAN0
	struct sja1000_platform_data *sja1000_pdata = &ls1x_sja1000_platform_data_0;
	sja1000_pdata->osc_freq = clk_get_rate(clk);
	#endif
	#ifdef CONFIG_LS1X_CAN1
	sja1000_pdata = &ls1x_sja1000_platform_data_1;
	sja1000_pdata->osc_freq = clk_get_rate(clk);
	#endif

#ifdef	CONFIG_LS1A_MACH
	#ifdef CONFIG_LS1X_CAN0
	/* CAN0复用设置 */
	ls1b_gpio_free(NULL, 66);	/* 引脚设置为CAN模式 */
	ls1b_gpio_free(NULL, 67);
	*(volatile int *)0xbfd00420 &= ~(1<<17 | 3<<14 );	/* 与I2C3 SPI0复用 */
	#endif
	#ifdef CONFIG_LS1X_CAN1
	/* CAN1复用设置 */
	ls1b_gpio_free(NULL, 68);	/* 引脚设置为CAN模式 */
	ls1b_gpio_free(NULL, 69);
	*(volatile int *)0xbfd00420 &= ~(1<<31 | 1<<16 | 3<<12);	/* 与NAND I2C2 SPI1复用 */
	#endif
#elif	CONFIG_LS1B_MACH
	*(volatile int *)0xbfd00424 &= ~(1<<23);	/* 与SPI1复用 */
	#ifdef CONFIG_LS1X_CAN0
	/* CAN0复用设置 */
	ls1b_gpio_free(NULL, 38);	/* 引脚设置为CAN模式 */
	ls1b_gpio_free(NULL, 39);
	*(volatile int *)0xbfd00420 &= ~(1<<24);	/* 与I2C1复用 */
	*(volatile int *)0xbfd00424 &= ~(1<<4);	/* 与UART1_2复用 */
	#endif
	#ifdef CONFIG_LS1X_CAN1
	/* CAN1复用设置 */
	ls1b_gpio_free(NULL, 40);	/* 引脚设置为CAN模式 */
	ls1b_gpio_free(NULL, 41);
	*(volatile int *)0xbfd00420 &= ~(1<<25);	/* 与I2C2复用 */
	*(volatile int *)0xbfd00424 &= ~(1<<5);	/* 与UART1_3复用 */
	#endif
#endif
	}
#endif	//#ifdef CONFIG_CAN_SJA1000_PLATFORM

#ifdef CONFIG_LS1B_I2C
	i2c_register_board_info(0, ls1b_i2c_devs, ARRAY_SIZE(ls1b_i2c_devs));
#endif

#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
	/* 轮询方式探测card的插拔 */
	ls1b_gpio_direction_input(NULL, DETECT_GPIO);		/* 输入使能 */
	/* 中断方式探测card的插拔 */
#endif

#ifdef CONFIG_TOUCHSCREEN_ADS7846
	ads7846_detect_penirq();
#endif

#ifdef CONFIG_LS1B_SPI0
	/* disable gpio24-27 */
	*(volatile unsigned int *)0xbfd010c0 &= ~(0xf << 24);
	spi_register_board_info(ls1b_spi0_devices, ARRAY_SIZE(ls1b_spi0_devices));
#endif

#ifdef CONFIG_LS1B_SPI1
	/* 使能SPI1控制器，与CAN0 CAN1 GPIO38-GPIO41复用,同时占用PWM0 PWM1用于片选. */
	/* 编程需要注意 */
	*(volatile unsigned int *)0xbfd00424 |= (0x3 << 23);
	/* disable gpio38-41 */
	*(volatile unsigned int *)0xbfd010c4 &= ~(0xf << 6);
	spi_register_board_info(ls1b_spi1_devices, ARRAY_SIZE(ls1b_spi1_devices));
#endif

#ifdef CONFIG_SPI_GPIO
	spi_register_board_info(spi_gpio_devices, ARRAY_SIZE(spi_gpio_devices));
#endif

//modify by lvling
#if defined(CONFIG_LS1B_GMAC0_OPEN) && defined(CONFIG_LS1B_GMAC1_OPEN)//open gmac0 and gmac1  
	printk("open gmac0 and gmac1.\n");
	/* 寄存器0xbfd00424有GMAC的使能开关 */
	(*(volatile unsigned int *)0xbfd00424) &= ~((1<<13) | (1<<12));	/* 使能GMAC0 GMAC1 */
	(*(volatile unsigned int *)0xbfd00420) |= (1 << 4 | 1 << 3);
	(*(volatile unsigned int *)0xbfd00424) |= (0xf);

#elif defined(CONFIG_LS1B_GMAC0_OPEN) && !defined(CONFIG_LS1B_GMAC1_OPEN)//open gmac0,close gmac1
	printk("open gmac0 close gmac1.\n");
	(*(volatile unsigned int *)0xbfd00424) &= ~(1 << 12);	//使能GMAC0
	(*(volatile unsigned int *)0xbfd00424) |= (1 << 0 | 1 << 2); //open gmac0

//	(*(volatile unsigned int *)0xbfd00424) |= (1 << 13);	//禁止GMAC1
	(*(volatile unsigned int *)0xbfd00424) &= ~(1 << 1 | 1 << 3); //close gmac1
	(*(volatile unsigned int *)0xbfd00420) &= ~(1 << 3 | 1 << 4); //open uart0/1

#elif !defined(CONFIG_LS1BGMAC0_OPEN) && defined(CONFIG_LS1B_GMAC1_OPEN) //close gmac0,open gmac 1
	printk("close gmac0 open gmac1.\n");
//	(*(volatile unsigned int *)0xbfd00424) |= (1 << 12);	//禁止GMAC0
	(*(volatile unsigned int *)0xbfd00424) &= ~(1 << 0 | 1 << 2); //close gmac0

	(*(volatile unsigned int *)0xbfd00424) &= ~(1 << 13);	//使能GMAC1
	(*(volatile unsigned int *)0xbfd00424) |= (1 << 1 | 1 << 3); //open gmac1
	(*(volatile unsigned int *)0xbfd00420) |= (1 << 3 | 1 <<4); //close uart0/1
  
#else
//	(*(volatile unsigned int *)0xbfd00424) |= ((1<<13) | (1<<12));	/* 禁止GMAC0 GMAC1 */
	(*(volatile unsigned int *)0xbfd00420) &= ~(1 << 3 | 1 << 4);	//open uart0/1
#endif

#ifdef CONFIG_LS1A_MACH
	*(volatile int *)0xbfd00420 &= ~0x200000;/* enable USB */
	*(volatile int *)0xbff10204 = 0;
	mdelay(105);
	*(volatile int *)0xbff10204 |= 0x40000000;/* ls1a usb reset stop */
#elif defined(CONFIG_USB_EHCI_HCD_LS1B) || defined(CONFIG_USB_OHCI_HCD_LS1B)
	*(volatile int *)0xbfd00424 &= ~0x800;/* enable USB */
	*(volatile int *)0xbfd00424 &= ~0x80000000;/* ls1g usb reset */
	mdelay(105);
	*(volatile int *)0xbfd00424 |= 0x80000000;/* ls1g usb reset stop */
#endif	//CONFIG_LS1A_MACH

#ifdef CONFIG_BACKLIGHT_GENERIC
	gpio_request(GPIO_BACKLIGHT_CTRL, "backlight");
#endif
#ifdef CONFIG_LCD_PLATFORM
	gpio_request(GPIO_BACKLIGHT_CTRL, "lcd_enable");
#endif
	return platform_add_devices(ls1b_platform_devices, ARRAY_SIZE(ls1b_platform_devices));
}

arch_initcall(ls1b_platform_init);
