/*
 * Platform device support for GS232 SoCs.
 *
 * Copyright 2009, Su Wen <suwen@ict.ac.cn>
 *	
 * base on Au1xxx Socs drivers by Matt Porter <mporter@kernel.crashing.org>
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
#include <ls1b_board.h>
#include <ls1b_board_int.h>
#include <linux/serial_8250.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>		//lxy
#include <linux/spi/mmc_spi.h>		//lqx
#include <linux/mmc/host.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/flash.h>
#include <asm/mach-loongson/ls1x/ls1b_board_int.h>
#include <linux/i2c.h>
#include <linux/i2c/tsc2007.h>
#include <linux/i2c/ft5x06_ts.h>
#include <media/gc0308_platform.h>		//lxy
#include <linux/videodev2.h>
#include <linux/spi/ads7846.h>
#include <asm/mach-loongson/ls1x/spi.h>
#include <asm/mach-loongson/ls1x/fb.h>
#include <linux/gpio_keys.h>
#include <asm/gpio.h>

#include <linux/phy.h> //lv
#include <linux/stmmac.h> //lv
#include <asm-generic/sizes.h> //lv

extern unsigned long bus_clock;
extern unsigned long ls1b_cpu_clock;
extern unsigned int  memsize, highmemsize;

static struct ls1b_board_intc_regs volatile *ls1b_board_hw0_icregs
	= (struct ls1b_board_intc_regs volatile *)(KSEG1ADDR(LS1B_BOARD_INTREG_BASE));

#ifdef CONFIG_MTD_NAND_LS1B
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
	        .mask_flags =   MTD_WRITEABLE,
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
#if 0
	[0] = {
        .name   ="kernel",
        .offset =0,
        .size   =0xe00000,
//        .mask_flags =   MTD_WRITEABLE,
    },
    [1] = {
        .name   ="os",
        .offset = 0xe00000,
        .size   = 0x6700000,
    
    },
    [2] = {
        .name   ="data",
        .offset = 0x7500000,
        .size   = 0xb00000,
    
    },
#endif
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
#endif //CONFIG_MTD_NAND_LS1B

static struct plat_serial8250_port uart8250_data[] = {
{
	.mapbase = LS1B_BOARD_UART0_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1B_BOARD_UART0_BASE),
	.irq = LS1B_BOARD_UART0_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1B_BOARD_UART1_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1B_BOARD_UART1_BASE),
	.irq = LS1B_BOARD_UART1_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1B_BOARD_UART2_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1B_BOARD_UART2_BASE),
	.irq = LS1B_BOARD_UART2_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1B_BOARD_UART3_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1B_BOARD_UART3_BASE),
	.irq = LS1B_BOARD_UART3_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1B_BOARD_UART4_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1B_BOARD_UART4_BASE),
	.irq = LS1B_BOARD_UART4_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1B_BOARD_UART5_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1B_BOARD_UART5_BASE),
	.irq = LS1B_BOARD_UART5_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
	.iotype = UPIO_MEM,
	.regshift = 0,
},
#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL0
{
	.mapbase = LS1B_BOARD_UART6_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1B_BOARD_UART6_BASE),
	.irq = LS1B_BOARD_UART0_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_SHARE_IRQ,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1B_BOARD_UART7_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1B_BOARD_UART7_BASE),
	.irq = LS1B_BOARD_UART0_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_SHARE_IRQ,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1B_BOARD_UART8_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1B_BOARD_UART8_BASE),
	.irq = LS1B_BOARD_UART0_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_SHARE_IRQ,
	.iotype = UPIO_MEM,
	.regshift = 0,
},
#endif
#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL1
{
	.mapbase = LS1B_BOARD_UART9_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1B_BOARD_UART9_BASE),
	.irq = LS1B_BOARD_UART1_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_SHARE_IRQ,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1B_BOARD_UART10_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1B_BOARD_UART10_BASE),
	.irq = LS1B_BOARD_UART1_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_SHARE_IRQ,
	.iotype = UPIO_MEM,
	.regshift = 0,
},{
	.mapbase = LS1B_BOARD_UART11_BASE,
	.membase = (void __iomem*)KSEG1ADDR(LS1B_BOARD_UART11_BASE),
	.irq = LS1B_BOARD_UART1_IRQ,
	.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_SHARE_IRQ,
	.iotype = UPIO_MEM,
	.regshift = 0,
},
#endif
{.flags = 0,}
};

static struct platform_device uart8250_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = uart8250_data,
	}
};

#ifdef LOONGSON_AHCI
static struct resource ls1b_ahci_resources[] = {
	[0] = {
		.start          = 0x1fe30000,
		.end            = 0x1fe30000+0x1ff,
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = 36,
		.end            = 36,
		.flags          = IORESOURCE_IRQ,
	},
};

static void __iomem *ls1b_ahci_map_table[6];

static struct platform_device ls1b_ahci_device = {
	.name           = "ls1b-ahci",
	.id             = -1,
	.dev = {
		.platform_data = ls1b_ahci_map_table,
	},
	.num_resources  = ARRAY_SIZE(ls1b_ahci_resources),
	.resource       = ls1b_ahci_resources,
};
#endif //#ifdef LOONGSON_AHCI

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
#ifdef CONFIG_LS1B_WDT
static struct resource ls1b_wat_resource[] = {
	[0]={
		.start      = LS1B_BOARD_WAT_BASE,
		.end        = (LS1B_BOARD_WAT_BASE + 0x8),
		.flags      = IORESOURCE_MEM,
	},
};

static struct platform_device ls1b_wat_device = {
	.name       = "ls1b-wdt",
	.id         = -1,
	.num_resources  = ARRAY_SIZE(ls1b_wat_resource),
	.resource   = ls1b_wat_resource,
};
#endif //#ifdef CONFIG_LS1B_WDT

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
	(ls1b_board_hw0_icregs + 3) -> int_edge	&= ~(1 << (TSC2007_GPIO_IRQ & 0x1f));
	(ls1b_board_hw0_icregs + 3) -> int_pol	&= ~(1 << (TSC2007_GPIO_IRQ & 0x1f));
	(ls1b_board_hw0_icregs + 3) -> int_clr	|= (1 << (TSC2007_GPIO_IRQ & 0x1f));
	(ls1b_board_hw0_icregs + 3) -> int_set	&= ~(1 << (TSC2007_GPIO_IRQ & 0x1f));
	(ls1b_board_hw0_icregs + 3) -> int_en	|= (1 << (TSC2007_GPIO_IRQ & 0x1f));
	return 0;
}

void ts_clear_penirq(void)
{
	(ls1b_board_hw0_icregs + 3) -> int_en &= ~(1 << (TSC2007_GPIO_IRQ & 0x1f));
}

static struct tsc2007_platform_data tsc2007_info = {
	.model				= 2007,
	.x_plate_ohms		= 180,
	.get_pendown_state	= ts_get_pendown_state,
	.init_platform_hw	= ts_init,
//	.clear_penirq		= ts_clear_penirq,
};
#endif //#ifdef CONFIG_TOUCHSCREEN_TSC2007

#ifdef CONFIG_TOUCHSCREEN_FT5X0X
#define FT5X0X_GPIO_IRQ		38
#define FT5X0X_GPIO_WAUP	39
int ft5x0x_irq_init(void)
{
	ls1b_gpio_direction_input(NULL, FT5X0X_GPIO_IRQ);		/* 输入使能 */

	(ls1b_board_hw0_icregs + 3) -> int_edge	&= ~(1 << (FT5X0X_GPIO_IRQ & 0x1f));
	(ls1b_board_hw0_icregs + 3) -> int_pol	&= ~(1 << (FT5X0X_GPIO_IRQ & 0x1f));
	(ls1b_board_hw0_icregs + 3) -> int_clr	|= (1 << (FT5X0X_GPIO_IRQ & 0x1f));
	(ls1b_board_hw0_icregs + 3) -> int_set	&= ~(1 << (FT5X0X_GPIO_IRQ & 0x1f));
	(ls1b_board_hw0_icregs + 3) -> int_en	|= (1 << (FT5X0X_GPIO_IRQ & 0x1f));
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
		.irq = LS1B_BOARD_GPIO_FIRST_IRQ + TSC2007_GPIO_IRQ,
		.platform_data	= &tsc2007_info,
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_FT5X0X
	{
		I2C_BOARD_INFO(FT5X0X_NAME, 0x38),
		.irq = LS1B_BOARD_GPIO_FIRST_IRQ + FT5X0X_GPIO_IRQ,
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
 * dc
 */
#ifdef CONFIG_FB_LS1B
static struct resource ls1b_lcd_resource[] = {
	[0] = {
		.start = LS1B_LCD_BASE,
		.end   = LS1B_LCD_BASE + 0x00100000 - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device ls1b_dc_device = {
	.name			= "ls1b-lcd",
	.id				= -1,
	.num_resources	= ARRAY_SIZE(ls1b_lcd_resource),
	.resource		= ls1b_lcd_resource,
	.dev			= {
	}
};
#endif //#ifdef CONFIG_FB_LS1B

/*
 * gmac
 */
/*
static struct resource ls1b_gmac1_resources[] = { 
 [0] = {
   .start          = LS1B_BOARD_GMAC1_BASE,
   .end            = (LS1B_BOARD_GMAC1_BASE + 0x6b),
   .flags          = IORESOURCE_MEM,
 },
 [1] = {
   .start          = LS1B_BOARD_GMAC1_IRQ,
   .end            = LS1B_BOARD_GMAC1_IRQ,
   .flags          = IORESOURCE_IRQ,
 },
};

static struct platform_device ls1b_gmac1_device = {
 .name           = "ls1b-gmac",
 .id             = 1,
 .dev = {
   .dma_mask=&dma_mask,
 },
 .num_resources  = ARRAY_SIZE(ls1b_gmac1_resources),
 .resource       = ls1b_gmac1_resources,
};


static struct resource ls1b_gmac2_resources[] = { 
 [0] = {
   .start          = LS1B_BOARD_GMAC2_BASE,
   .end            = (LS1B_BOARD_GMAC2_BASE + 0x6b),
   .flags          = IORESOURCE_MEM,
 },
 [1] = {
   .start          = LS1B_BOARD_GMAC2_IRQ,
   .end            = LS1B_BOARD_GMAC2_IRQ,
   .flags          = IORESOURCE_IRQ,
 },
};

static struct platform_device ls1b_gmac2_device = {
 .name           = "ls1b-gmac",
 .id             = 2,
 .dev = {
   .dma_mask=&dma_mask,
 },
 .num_resources  = ARRAY_SIZE(ls1b_gmac2_resources),
 .resource       = ls1b_gmac2_resources,
};
*/

//gmac0
static struct resource ls1b_mac0_resources[] = {
        [0] = {
                .start  = LS1B_BOARD_GMAC1_BASE,
                .end    = LS1B_BOARD_GMAC1_BASE + SZ_64K - 1,
                .flags  = IORESOURCE_MEM,
        },
        [1] = {
                .name   = "macirq",
                .start  = LS1B_BOARD_GMAC1_IRQ,
                .flags  = IORESOURCE_IRQ,
        },
};

static struct plat_stmmacenet_data ls1b_mac0_data = {
        .bus_id         = 0,
//      .pbl            = 32,
        .has_gmac       = 1,
	.enh_desc	= 0,
        /*.tx_coe               = 1,*/
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
	.bus_id = 0,
	.phy_addr = 1,
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
			.start = 1,
			.end = 1,
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy0_private_data,
	},
};

//gmac1
static struct resource ls1b_mac1_resources[] = {
        [0] = {
                .start  = LS1B_BOARD_GMAC2_BASE,
                .end    = LS1B_BOARD_GMAC2_BASE + SZ_64K - 1,
                .flags  = IORESOURCE_MEM,
        },
        [1] = {
                .name   = "macirq",
                .start  = LS1B_BOARD_GMAC2_IRQ,
                .flags  = IORESOURCE_IRQ,
        },
};


static struct plat_stmmacenet_data ls1b_mac1_data = {
        .bus_id         = 1,
//      .pbl            = 32,
        .has_gmac       = 1,
	.enh_desc	= 0,
        /*.tx_coe               = 1,*/
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
			.start = 1,
			.end = -1,
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy1_private_data,
	},
};

#ifdef CONFIG_SND_LS1B
static struct platform_device ls1b_audio_device = {
	.name           = "ls1b-audio",
	.id             = -1,
};
#endif

#ifdef CONFIG_MTD_M25P80
static struct mtd_partition partitions[] = {
#if 1	//for bobodog program
	[0] = {
		.name		= "pmon",
		.offset		= 0,
		.size		= 512 * 1024,	//512KB
	//	.mask_flags	= MTD_WRITEABLE,
	}, 
	[1] = {
		.name		= "kernel",	
		.offset		= 512 * 1024,
		.size		= 0x210000,
	},
	[2] = {
		.name		= "fs",
		.offset		= 0x290000,
		.size		= 0x500000,
	},
	[3] = {
		.name		= "data",
		.offset		= 0x790000,
		.size		= 0x800000 - 0x790000,
	},
#if 0	//for finger program
	[0] = {
		.name		= "pmon",
		.offset		= 0,
		.size		= 512 * 1024,	//512KB
	//	.mask_flags	= MTD_WRITEABLE,
	}, 
	[1] = {
		.name		= "kernel",	
		.offset		= 512 * 1024,
		.size		= 0x4a0000,
	},
	[2] = {
		.name		= "data",
		.offset		= 0x520000,
		.size		= 0x800000 - 0x520000,
	},
#endif
#else
	[0] = {
		.name		= "pmon",
		.offset		= 0,
		.size		= 512 * 1024,	//512KB
	//	.mask_flags	= MTD_WRITEABLE,
	}, 
	[1] = {
		.name		= "kernel",	
		.offset		= 512 * 1024,
		.size		= 0x2c0000,
	},
	[2] = {
		.name		= "system",
		.offset		= 0x340000,
		.size		= 0x180000,
	},
	[3] = {
		.name		= "data",
		.offset		= 0x520000,
		.size		= 0x800000 - 0x520000,
	},
#endif
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

#if 1
#define MMC_SPI_CARD_DETECT_INT  (LS1B_BOARD_GPIO_FIRST_IRQ + DETECT_GPIO)
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
	.init = ls1b_mmc_spi_init,
	.exit = ls1b_mmc_spi_exit,
	.detect_delay = 1200,	/* msecs */
	/* 轮询方式方式探测card的插拔 */
	.get_cd = mmc_spi_get_cd,
//	.caps = MMC_CAP_NEEDS_POLL,
};	
#endif  /* defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE) */

#ifdef CONFIG_TOUCHSCREEN_ADS7846
#define ADS7846_GPIO_IRQ 60 /* 开发板触摸屏使用的外部中断 */
int ads7846_pendown_state(unsigned int pin)
{
	return !gpio_get_value(ADS7846_GPIO_IRQ);
}
	
int ads7846_detect_penirq(void)
{
	//配置GPIO0
	ls1b_gpio_direction_input(NULL, ADS7846_GPIO_IRQ);		/* 输入使能 */
		
	(ls1b_board_hw0_icregs + 3) -> int_edge &= ~(1 << (ADS7846_GPIO_IRQ & 0x1f));
	(ls1b_board_hw0_icregs + 3) -> int_pol	&= ~(1 << (ADS7846_GPIO_IRQ & 0x1f));
	(ls1b_board_hw0_icregs + 3) -> int_clr	|= (1 << (ADS7846_GPIO_IRQ & 0x1f));
	(ls1b_board_hw0_icregs + 3) -> int_set	&= ~(1 << (ADS7846_GPIO_IRQ & 0x1f));
	(ls1b_board_hw0_icregs + 3) -> int_en		|= (1 << (ADS7846_GPIO_IRQ & 0x1f));
	
	return (LS1B_BOARD_GPIO_FIRST_IRQ + ADS7846_GPIO_IRQ);
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

#ifdef CONFIG_LS1B_SPI0
static struct spi_board_info ls1b_spi0_devices[] = {
#ifdef CONFIG_MTD_M25P80
	{	/* DataFlash chip */
		.modalias	= "w25q64",		//"m25p80",
		.bus_num 		= 0,
		.chip_select	= 0,
		.max_speed_hz	= 80000000,
		.platform_data	= &flash,
	},
#endif
#ifdef CONFIG_SPI_MCP3201
	{	/* ADC chip */
		.modalias	= "mcp3201",
		.bus_num 		= 0,
		.chip_select	= 0,
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
		.irq				= LS1B_BOARD_GPIO_FIRST_IRQ + ADS7846_GPIO_IRQ,
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
#if defined(CONFIG_EASY_DAB_AUDIO)
	{	
		.modalias	= "easy_dab",
		.bus_num 	= 0,
		.chip_select	= SPI0_CS1,
		.max_speed_hz	= 10000000,
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
		.start	= (LS1B_BOARD_GPIO_FIRST_IRQ + GPIO_IR),
		.end	= (LS1B_BOARD_GPIO_FIRST_IRQ + GPIO_IR),
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
	[0]  = { .gpio = 4, },	
	[1]  = { .gpio = 5, },
	[2]  = { .gpio = 6, },
	[3]  = { .gpio = 7, },
	[4]  = { .gpio = 8, },
	[5]  = { .gpio = 9, },
	[6]  = { .gpio = 10, },
	[7]  = { .gpio = 11, },
	[8]  = { .gpio = 12, },
	[9]  = { .gpio = 13, },
	[10] = { .gpio = 14, },
	[11] = { .gpio = 15, },
	[12] = { .gpio = 16, },
	[13] = { .gpio = 17, },
	[14] = { .gpio = 18, },
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

static struct rotary_encoder_platform_data raumfeld_rotary_encoder_info = {
	.steps		= 30,
	.axis		= REL_X,
	.relative_axis	= 1,
	.rollover	= false,
	.gpio_a		= GPIO_ROTARY_A,
	.gpio_b		= GPIO_ROTARY_B,
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

/***********************************************/
static struct platform_device *ls1b_platform_devices[] __initdata = {
#ifdef CONFIG_MTD_NAND_LS1B
	&ls1b_nand_device,
#endif

	&uart8250_device,

#ifdef LOONGSON_AHCI
	&ls1b_ahci_device,
#endif

#ifdef CONFIG_USB_OHCI_HCD_LS1B
	&ls1b_ohci_device,
#endif
#ifdef CONFIG_USB_EHCI_HCD_LS1B
	&ls1b_ehci_device,
#endif

#ifdef CONFIG_FB_LS1B
	&ls1b_dc_device,
#endif

#ifdef CONFIG_LS1B_GMAC0_OPEN   //lv
	&ls1b_gmac0_mac,
	&ls1b_gmac0_phy,
#endif
#ifdef CONFIG_LS1B_GMAC1_OPEN  //lv
	&ls1b_gmac1_mac,
	&ls1b_gmac1_phy,
#endif

#ifdef CONFIG_LS1B_WDT
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
};

#define AHCI_PCI_BAR  5

int ls1b_platform_init(void)
{
	int i;
	unsigned int apb_clk;

#ifdef LOONGSON_AHCI
	ls1b_ahci_map_table[AHCI_PCI_BAR]=ioremap_nocache(ls1b_ahci_resources[0].start,0x200);
#endif
	
#ifdef CONFIG_LS1A_MACH
	*(volatile int *)0xbfd00420 &= ~0x200000;/* enable USB */
	*(volatile int *)0xbff10204 = 0;
	mdelay(5);
	*(volatile int *)0xbff10204 |= 0x40000000;/*ls1a usb reset stop*/
#else
#if defined(CONFIG_USB_EHCI_HCD_LS1B) || defined(CONFIG_USB_OHCI_HCD_LS1B)
	*(volatile int *)0xbfd00424 &= ~0x800;/* enable USB */
	*(volatile int *)0xbfd00424 &= ~0x80000000;/*ls1g usb reset*/
	mdelay(5);
	*(volatile int *)0xbfd00424 |= 0x80000000;/*ls1g usb reset stop*/
#endif
#endif	//CONFIG_LS1A_MACH

#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL0
	(*(volatile unsigned char *)(LS1B_UART_SPLIT)) = 0x1;
#endif
#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL1
	(*(volatile unsigned char *)(LS1B_UART_SPLIT)) |= 0x2;

	(*(volatile unsigned int *)(LS1B_GPIO_MUX_CTRL1)) |= 0x30;
#endif

	/* uart clock */
	apb_clk = bus_clock/2;
	for(i=0; i<CONFIG_SERIAL_8250_NR_UARTS; i++) {
		uart8250_data[i].uartclk = apb_clk;
	}

#if 0
//	clk = *(volatile unsigned int *)(0xbfd010c0);
//	*(volatile unsigned int *)(0xbfd010c0) = clk | x01;

	*(volatile unsigned int *)(0xbfe5c000 + 0xc) = 0x80;
	*(volatile unsigned int *)(0xbfe5c000 + 0x0) = 0x0; 
	*(volatile unsigned int *)(0xbfe5c000 + 0x4) = 0x1;
	*(volatile unsigned int *)(0xbfe5c000 + 0x8) = 0x3;
	*(volatile unsigned int *)(0xbfe5c000 + 0xc) = 0x1;
	
#if 0
	*(volatile unsigned int *)(0xbfe5c010 + 0xc) = 0x80;
	*(volatile unsigned int *)(0xbfe5c010 + 0x0) = 0; 
	*(volatile unsigned int *)(0xbfe5c010 + 0x4) = 19;
	*(volatile unsigned int *)(0xbfe5c010 + 0x8) = 19 + 320 * 8 *6;
	*(volatile unsigned int *)(0xbfe5c010 + 0xc) = 0x1;

	*(volatile unsigned int *)(0xbfe5c020 + 0xc) = 0x80;
	*(volatile unsigned int *)(0xbfe5c020 + 0x0) = 0; 
	*(volatile unsigned int *)(0xbfe5c020 + 0x4) = (1 + 8) * 240 * (19 + 320 * 8 *6) -1;
	*(volatile unsigned int *)(0xbfe5c020 + 0x8) = (1 + 8 + 1) * 240 * (19 + 320 * 8 *6) -1;
	*(volatile unsigned int *)(0xbfe5c020 + 0xc) = 0x1;
#endif
#endif

#ifdef CONFIG_LS1B_I2C
	i2c_register_board_info(0, ls1b_i2c_devs, ARRAY_SIZE(ls1b_i2c_devs));
#endif

#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
	/* 轮询方式探测card的插拔 */
	ls1b_gpio_direction_input(NULL, DETECT_GPIO);		/* 输入使能 */
	/* 中断方式探测card的插拔 */
	(ls1b_board_hw0_icregs + 3) -> int_edge |= (1 << (DETECT_GPIO & 0x1f));		/* 边沿触发方式寄存器 */
	(ls1b_board_hw0_icregs + 3) -> int_pol	&= ~(1 << (DETECT_GPIO & 0x1f));	/* 电平触发方式寄存器 */
	(ls1b_board_hw0_icregs + 3) -> int_clr	|= (1 << (DETECT_GPIO & 0x1f));		/* 中断清空寄存器 */
	(ls1b_board_hw0_icregs + 3) -> int_set	&= ~(1 << (DETECT_GPIO & 0x1f));	/* 中断置位寄存器 */
	(ls1b_board_hw0_icregs + 3) -> int_en	|= (1 << (DETECT_GPIO & 0x1f));		/* 中断使能寄存器 */
#endif

#ifdef CONFIG_INPUT_GPIO_ROTARY_ENCODER
	(ls1b_board_hw0_icregs + 3) -> int_edge |= (1 << (GPIO_ROTARY_A & 0x1f));	/* 边沿触发方式寄存器 */
	(ls1b_board_hw0_icregs + 3) -> int_pol	&= ~(1 << (GPIO_ROTARY_A & 0x1f));	/* 电平触发方式寄存器 */
	(ls1b_board_hw0_icregs + 3) -> int_clr	|= (1 << (GPIO_ROTARY_A & 0x1f));	/* 中断清空寄存器 */
	(ls1b_board_hw0_icregs + 3) -> int_set	&= ~(1 << (GPIO_ROTARY_A & 0x1f));	/* 中断置位寄存器 */
	(ls1b_board_hw0_icregs + 3) -> int_en	|= (1 << (GPIO_ROTARY_A & 0x1f));	/* 中断使能寄存器 */

	(ls1b_board_hw0_icregs + 3) -> int_edge |= (1 << (GPIO_ROTARY_B & 0x1f));	/* 边沿触发方式寄存器 */
	(ls1b_board_hw0_icregs + 3) -> int_pol	&= ~(1 << (GPIO_ROTARY_B & 0x1f));	/* 电平触发方式寄存器 */
	(ls1b_board_hw0_icregs + 3) -> int_clr	|= (1 << (GPIO_ROTARY_B & 0x1f));	/* 中断清空寄存器 */
	(ls1b_board_hw0_icregs + 3) -> int_set	&= ~(1 << (GPIO_ROTARY_B & 0x1f));	/* 中断置位寄存器 */
	(ls1b_board_hw0_icregs + 3) -> int_en	|= (1 << (GPIO_ROTARY_B & 0x1f));	/* 中断使能寄存器 */
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

//modify by lvling
#if CONFIG_LS1B_GMAC0_OPEN && CONFIG_LS1B_GMAC1_OPEN//open gmac0 and gmac1  
	printk("open gmac0 and gmac1.\n");
	/* 寄存器0xbfd00424有GMAC的使能开关 */
	(*(volatile unsigned int *)0xbfd00424) &= ~((1<<13) | (1<<12));	/* 使能GMAC0 GMAC1 */
	(*(volatile unsigned int *)0xbfd00420) |= (1 << 4 | 1 << 3);
	(*(volatile unsigned int *)0xbfd00424) |= (0xf);

#elif (CONFIG_LS1B_GMAC0_OPEN) && (~CONFIG_LS1B_GMAC1_OPEN)//open gmac0,close gmac1
	printk("open gmac0 close gmac1.\n");
	(*(volatile unsigned int *)0xbfd00424) &= ~(1 << 12);	//使能GMAC0
	(*(volatile unsigned int *)0xbfd00424) |= (1 << 0 | 1 << 2); //open gmac0

	(*(volatile unsigned int *)0xbfd00424) |= (1 << 13);	//禁止GMAC1
	(*(volatile unsigned int *)0xbfd00424) &= ~(1 << 1 | 1 << 3); //close gmac1
	(*(volatile unsigned int *)0xbfd00420) &= ~(1 << 3 | 1 << 4); //open uart0/1

#elif (~CONFIG_LS1BGMAC0_OPEN) && (CONFIG_LS1B_GMAC1_OPEN) //close gmac0,open gmac 1
	printk("close gmac0 open gmac1.\n");
	(*(volatile unsigned int *)0xbfd00424) |= (1 << 12);	//禁止GMAC0
	(*(volatile unsigned int *)0xbfd00424) &= ~(1 << 0 | 1 << 2); //close gmac0

	(*(volatile unsigned int *)0xbfd00424) &= ~(1 << 13);	//使能GMAC1
	(*(volatile unsigned int *)0xbfd00424) |= (1 << 1 | 1 << 3); //open gmac1
	(*(volatile unsigned int *)0xbfd00420) |= (1 << 3 | 1 <<4); //close uart0/1
  
#else
	(*(volatile unsigned int *)0xbfd00424) |= ((1<<13) | (1<<12));	/* 禁止GMAC0 GMAC1 */
	(*(volatile unsigned int *)0xbfd00420) &= ~(1 << 3 | 1 << 4);	//open uart0/1
#endif

	return platform_add_devices(ls1b_platform_devices, ARRAY_SIZE(ls1b_platform_devices));
}

arch_initcall(ls1b_platform_init);
