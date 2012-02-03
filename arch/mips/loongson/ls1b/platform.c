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
#include <linux/mtd/partitions.h>
#include <linux/spi/flash.h>
//#include <asm/mach-loongson/ls1b/gpio_keys.h>	//lqx
#include <asm/mach-loongson/ls1b/ls1b_board_int.h>
#include <linux/i2c.h>
#include <linux/i2c/tsc2007.h>
#include <media/gc0308_platform.h>		//lxy
#include <linux/videodev2.h>
#include <linux/spi/ads7846.h>
#include <asm/mach-loongson/ls1b/spi.h>
#include <asm/mach-loongson/ls1b/fb.h>
#include <linux/gpio_keys.h>



static struct ls1b_board_intc_regs volatile *ls1b_board_hw0_icregs
	= (struct ls1b_board_intc_regs volatile *)(KSEG1ADDR(LS1B_BOARD_INTREG_BASE));


struct ls1b_nand_platform_data{
    int enable_arbiter;
    struct mtd_partition *parts;
    unsigned int nr_parts;
};

static struct mtd_partition ls1b_nand_partitions[]={
#if 1
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

static struct plat_serial8250_port uart8250_data[12] = {
{ .mapbase=0xbfe40000,.membase=(void *)0xbfe40000,.irq=2,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
{ .mapbase=0xbfe44000,.membase=(void *)0xbfe44000,.irq=3,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
{ .mapbase=0xbfe48000,.membase=(void *)0xbfe48000,.irq=4,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
{ .mapbase=0xbfe4c000,.membase=(void *)0xbfe4c000,.irq=5,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
{ .mapbase=0xbfe6c000,.membase=(void *)0xbfe6c000,.irq=29,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
{ .mapbase=0xbfe7c000,.membase=(void *)0xbfe7c000,.irq=30,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL0
{ .mapbase=0xbfe41000,.membase=(void *)0xbfe41000,.irq=LS1B_BOARD_UART0_IRQ,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
{ .mapbase=0xbfe42000,.membase=(void *)0xbfe42000,.irq=LS1B_BOARD_UART0_IRQ,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
{ .mapbase=0xbfe43000,.membase=(void *)0xbfe43000,.irq=LS1B_BOARD_UART0_IRQ,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
#endif
#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL1
{ .mapbase=0xbfe45000,.membase=(void *)0xbfe45000,.irq=LS1B_BOARD_UART0_IRQ,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
{ .mapbase=0xbfe46000,.membase=(void *)0xbfe46000,.irq=LS1B_BOARD_UART0_IRQ,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
{ .mapbase=0xbfe47000,.membase=(void *)0xbfe47000,.irq=LS1B_BOARD_UART0_IRQ,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,},
#endif
//{ .uartclk = 1843200, .mapbase=0xbff003f8,.membase=(void *)0xbff003f8,.irq=0,.flags=UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,.iotype=UPIO_MEM,.regshift   = 0,}, 
{}
};

static struct platform_device uart8250_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
 .dev = {
   .platform_data = uart8250_data,
	}
};

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

/*
 * ohci
 */
static int dma_mask=-1;

static struct resource ls1b_ohci_resources[] = { 
 [0] = {
   .start          = LS1B_USB_OHCI_BASE,
   .end            = (LS1B_USB_OHCI_BASE + 0x1000 - 1),
   .flags          = IORESOURCE_MEM,
 },
 [1] = {
   .start          = LS1B_BOARD_OHCI_IRQ,
   .end            = LS1B_BOARD_OHCI_IRQ,
   .flags          = IORESOURCE_IRQ,
 },
};

static struct ls1b_usbh_data  ls1b_ohci_platform_data={
#ifdef CONFIG_MACH_SB2F
	.ports=4,
#else
	.ports=1,
#endif
};

static struct platform_device ls1b_ohci_device = {
 .name           = "ls1b-ohci",
 .id             = -1,
 .dev = {
   .platform_data = &ls1b_ohci_platform_data,
   .dma_mask=&dma_mask,
 },
 .num_resources  = ARRAY_SIZE(ls1b_ohci_resources),
 .resource       = ls1b_ohci_resources,
};

/*
 * ehci
 */

static struct resource ls1b_ehci_resources[] = { 
 [0] = {
   .start          = LS1B_USB_EHCI_BASE,
   .end            = (LS1B_USB_EHCI_BASE + 0x6b),
   .flags          = IORESOURCE_MEM,
 },
 [1] = {
   .start          = LS1B_BOARD_EHCI_IRQ,
   .end            = LS1B_BOARD_EHCI_IRQ,
   .flags          = IORESOURCE_IRQ,
 },
};

static struct ls1b_usbh_data  ls1b_ehci_platform_data={
#ifdef CONFIG_MACH_SB2F
	.ports=4,
#else
	.ports=1,
#endif
};

static struct platform_device ls1b_ehci_device = {
 .name           = "ls1b-ehci",
 .id             = -1,
 .dev = {
   .platform_data = &ls1b_ehci_platform_data,
   .dma_mask=&dma_mask,
 },
 .num_resources  = ARRAY_SIZE(ls1b_ehci_resources),
 .resource       = ls1b_ehci_resources,
};

/*
* watchdog
*/

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

/*
*RTC
*/

static struct resource ls1b_rtc_resource[] = {
	[0]={
		.start      = LS1B_BOARD_RTC_BASE,
		.end        = (LS1B_BOARD_RTC_BASE + 0x54),
		.flags      = IORESOURCE_MEM,
	},
	[1]={
		.start      = LS1B_BOARD_TOY0_IRQ,
		.end        = LS1B_BOARD_TOY0_IRQ,
		.flags      = IORESOURCE_IRQ,
	},
};

static struct platform_device ls1b_rtc_device = {
	.name       = "ls1b-rtc",
	.id         = -1,
	.num_resources  = ARRAY_SIZE(ls1b_rtc_resource),
	.resource   = ls1b_rtc_resource,
};

/*
*I2C
*/

/* I2C devices fitted. */
#define	tsc_irq	(LS1B_BOARD_GPIO_FIRST_IRQ + 60)

static int ts_get_pendown_state(void)
{
	int val = 0;
#if 0
	gpio_free(GPIO_FN_INTC_IRQ0);
	gpio_request(GPIO_PTZ0, NULL);
	gpio_direction_input(GPIO_PTZ0);

	val = gpio_get_value(GPIO_PTZ0);

	gpio_free(GPIO_PTZ0);
	gpio_request(GPIO_FN_INTC_IRQ0, NULL);
#endif
	val = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_IN1));
	
	return val & (1 <<(60&0x1f)) ? 0 : 1;
}

void ts_init()
{
	unsigned int ret;
	int gpio = tsc_irq - LS1B_BOARD_GPIO_FIRST_IRQ;
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_CFG1)); //GPIO0	0xbfd010c0
	ret |= (1 << (gpio & 0x1f)); //GPIO50
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_CFG1)) = ret;	
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OE1));//GPIO0 
	ret |= (1 << (gpio & 0x1f));
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OE1)) = ret;

	(ls1b_board_hw0_icregs + 3) -> int_edge	&= ~(1 << (gpio & 0x1f));
	(ls1b_board_hw0_icregs + 3) -> int_pol	&= ~(1 << (gpio & 0x1f));
	(ls1b_board_hw0_icregs + 3) -> int_clr	|= (1 << (gpio & 0x1f));
	(ls1b_board_hw0_icregs + 3) -> int_set	&= ~(1 << (gpio & 0x1f));
	(ls1b_board_hw0_icregs + 3) -> int_en	|= (1 << (gpio & 0x1f));	
}

void ts_clear_penirq()
{
	int gpio = tsc_irq - LS1B_BOARD_GPIO_FIRST_IRQ;
	(ls1b_board_hw0_icregs + 3) -> int_en &= ~(1 << (gpio & 0x1f));
}


static struct tsc2007_platform_data tsc2007_info = {
	.model			= 2007,
	.x_plate_ohms		= 180,
	.get_pendown_state	= ts_get_pendown_state,
	.init_platform_hw	= ts_init,
//	.clear_penirq		= ts_clear_penirq,
};


static struct i2c_board_info __initdata ls1b_i2c_devs[] = {
	{
	 I2C_BOARD_INFO("tsc2007", 0x48),
	 .irq = tsc_irq,
	 .platform_data	= &tsc2007_info,
	 },
};

#ifdef CONFIG_VIDEO_GC0308
#define GC0308_ENABLED
#endif

#ifdef GC0308_ENABLED
static struct gc0308_platform_data gc0308_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_YUYV,
	.freq = 24000000,
	.is_mipi = 0,
};

static struct i2c_board_info __initdata gc0308_i2c_info[] = {
	{
		I2C_BOARD_INFO("GC0308", 0x42 >> 1),
		.platform_data = &gc0308_plat,
	},
};
#if 0
static struct s3c_platform_camera gc0308 = {
//#ifdef CAM_ITU_CH_A
	.id		= CAMERA_PAR_A,
//#else
//	.id		= CAMERA_PAR_B,
//#endif
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 1,
	.info		= &gc0308_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_YUYV,
	.srclk_name	= "mout_mpll",
	.clk_name	= "sclk_cam1",
	.clk_rate	= 24000000,             /* 24MHz */
	.line_length	= 640,              /* 640*480 */
	/* default resol for preview kind of thing */
	.width		= 640,
	.height		= 480,
	.window		= {
		.left   = 16,
		.top    = 0,
		.width  = 640 - 16,
		.height = 480,
	},

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,
};
#endif
#endif




static struct resource ls1b_i2c_resource[] = {
	[0]={
		.start	= LS1B_BOARD_I2C_BASE,
		.end	= (LS1B_BOARD_I2C_BASE + 0x4),
		.flags	= IORESOURCE_MEM,
	},
	[1]={
		.start	= LS1B_BOARD_I2C0_IRQ,
		.end	= LS1B_BOARD_I2C0_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device ls1b_i2c_device = {
	.name		= "ls1b-i2c",
	.id			= 0,
	.num_resources	= ARRAY_SIZE(ls1b_i2c_resource),
	.resource	= ls1b_i2c_resource,

};

/*
 * dc
 */

//static struct platform_device ls1b_dc_device = {
// .name           = "ls1b-lcd",
// .id             = -1,
//};

static struct resource ls1b_lcd_resource[] = {
	[0] = {
		.start = LS1B_LCD_BASE,
		.end   = LS1B_LCD_BASE + 0x00100000 - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device ls1b_dc_device = {
	.name		  = "ls1b-lcd",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(ls1b_lcd_resource),
	.resource	  = ls1b_lcd_resource,
	.dev              = {
	}
};

/*
 * gmac
 */

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

static struct platform_device ls1b_audio_device = {
 .name           = "ls1b-audio",
 .id             = -1,
};


#if 1		//lxy
static struct mtd_partition partitions[] = {
#if 1
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


static struct mmc_spi_platform_data mmc_spi = {
	.detect_delay = 100,
};	

	//-----------------SPI-0---------------------------
#define GPIO_IRQ 60
//	static struct ls1b_board_intc_regs volatile *ls1b_board_hw0_icregs
//		= (struct ls1b_board_intc_regs volatile *)(KSEG1ADDR(LS1B_BOARD_INTREG_BASE));
	
	int ads7846_pendown_state(unsigned int pin)
	{
		unsigned int ret;
		ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_IN1)); //读回的数值是反码？
		ret = ((ret >> (GPIO_IRQ & 0x1f)) & 0x01);
	//	printk("ret = %x \n", !ret);
		return !ret;
	}
	
	int ads7846_detect_penirq(void)
	{
		unsigned int ret;
		//配置GPIO0
		ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_CFG1)); //GPIO0 0xbfd010c0 使能GPIO
		ret |= (1 << (GPIO_IRQ & 0x1f)); //GPIO50
		*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_CFG1)) = ret;
		
		ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OE1));//GPIO0 设置GPIO输入使能
		ret |= (1 << (GPIO_IRQ & 0x1f));
		*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OE1)) = ret;
		(ls1b_board_hw0_icregs + 3) -> int_edge &= ~(1 << (GPIO_IRQ & 0x1f));
	//	(ls1b_board_hw0_icregs + 3) -> int_edge |= (1 << (GPIO_IRQ & 0x1f));
		(ls1b_board_hw0_icregs + 3) -> int_pol	&= ~(1 << (GPIO_IRQ & 0x1f));
		(ls1b_board_hw0_icregs + 3) -> int_clr	|= (1 << (GPIO_IRQ & 0x1f));
		(ls1b_board_hw0_icregs + 3) -> int_set	&= ~(1 << (GPIO_IRQ & 0x1f));
		(ls1b_board_hw0_icregs + 3) -> int_en		|= (1 << (GPIO_IRQ & 0x1f));
		
		return (LS1B_BOARD_GPIO_FIRST_IRQ + GPIO_IRQ);
	}
	
	static struct ads7846_platform_data ads_info = {
		.model				= 7846,
		.vref_delay_usecs		= 1,
	//	.vref_mv			= 0,
		.keep_vref_on			= 0,
	//	.settle_delay_usecs 	= 150,
	//	.penirq_recheck_delay_usecs = 1,
		.x_plate_ohms			= 800,
		.pressure_min		 = 0,//需要定义采用0
		.pressure_max		 = 15000,//需要定义采用15000
		.debounce_rep			= 3,
		.debounce_max			= 10,
		.debounce_tol			= 50,
		.get_pendown_state		= ads7846_pendown_state,
		.filter_init			= NULL,
		.filter 			= NULL,
		.filter_cleanup 		= NULL,
	};


	static struct spi_board_info ls1b_spi0_devices[] = {
#ifdef CONFIG_MTD_M25P80
		{	/* DataFlash chip */
			.modalias	= "w25q64",		//"m25p80",
			.bus_num 		= 0,
			.chip_select	= 0,
			.max_speed_hz	= 80 * 1000 * 1000,
			.platform_data	= &flash,
		},
#endif
		{	/* ADC chip */
			.modalias	= "mcp3201",
			.bus_num 		= 0,
			.chip_select	= 0,
			.max_speed_hz	= 80 * 1000 * 1000,
		},
		{
			.modalias = "ads7846",
			.platform_data = &ads_info,
//			.irq = LS1B_BOARD_I2C0_IRQ, //LS1B_BOARD_PCI_INTA_IRQ,	//PCF8574A I2C扩展IO口中断
			.bus_num 		= 0,
			.chip_select 	= SPI0_CS1,
			.max_speed_hz 	= 500*1000,
			.mode 			= SPI_MODE_1,
			.irq				= LS1B_BOARD_GPIO_FIRST_IRQ + GPIO_IRQ,
		},
		{	/* mmc/sd card */
			.modalias	= "mmc_spi",		//mmc spi,
			.bus_num 		= 0,
			.chip_select	= SPI0_CS2,
			.max_speed_hz	= 25 * 1000 * 1000,
			.platform_data	= &mmc_spi,
		},
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
	//	.pin_cs = SPI0_CS0,// CS 片选
		.board_size = ARRAY_SIZE(ls1b_spi0_devices),
		.board_info = ls1b_spi0_devices,
		.num_cs		= SPI0_CS3 + 1,
	};
	
	static struct platform_device ls1b_spi0_device = {
		.name		= "ls1b-spi0",
		.id 		= 0,
		.num_resources	= ARRAY_SIZE(ls1b_spi0_resource),
		.resource	= ls1b_spi0_resource,
		.dev		= {
			.platform_data	= &ls1b_spi0_platdata,//&ls1b_spi_devices,
		},
	};
#endif

/************************************************/	//GPIO && buzzer && button
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

static struct gpio_keys_button ls1b_gpio_buzzer[] = {
	[0] = {
		.gpio	= 3,	//57
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

/***********************************************/




static struct platform_device *ls1b_platform_devices[] __initdata = {
    &ls1b_nand_device,
	&uart8250_device,
//	&ls1b_ahci_device,
	&ls1b_ohci_device,
	&ls1b_ehci_device,
	&ls1b_dc_device,
#ifdef CONFIG_LS1B_GMAC0_OPEN   //lv
        &ls1b_gmac1_device,
#endif
#ifdef CONFIG_LS1B_GMAC1_OPEN  //lv
        &ls1b_gmac2_device,
#endif
	&ls1b_wat_device,
	&ls1b_rtc_device,
	&ls1b_i2c_device,
	&ls1b_audio_device,
	&ls1b_spi0_device,	//lxy
	&ls1b_gpio_key_device,
	&ls1b_gpio_buzzer_device,
	&ls1b_pwm_device,
};

#define AHCI_PCI_BAR  5

int ls1b_platform_init(void)
{
	unsigned int data;
	int pll,ctrl,clk,i;
	unsigned int ddr_clk,uart_clk;

	ls1b_ahci_map_table[AHCI_PCI_BAR]=ioremap_nocache(ls1b_ahci_resources[0].start,0x200);
#ifdef CONFIG_MACH_SB2F
	*(volatile int *)0xbff10204 = 0;
	*(volatile int *)0xbff10204;
	mdelay(1);
	/*ls1f usb reset stop*/
	*(volatile int *)0xbff10204 = 0x40000000;
#else
	*(volatile int *)0xbfd00424 &= ~0x80000000;
	*(volatile int *)0xbfd00424;
	mdelay(1);
	/*ls1g usb reset stop*/
	*(volatile int *)0xbfd00424 |= 0x80000000;
#endif

#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL0
	(*(volatile unsigned char *)(LS1B_UART_SPLIT)) = 0x1;
#endif
#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL1
	data = (*(volatile unsigned char *)(LS1B_UART_SPLIT));
	(*(volatile unsigned char *)(LS1B_UART_SPLIT)) = data | 0x2;

	data = (*(volatile unsigned int *)(LS1B_GPIO_MUX_CTRL1));
	(*(volatile unsigned int *)(LS1B_GPIO_MUX_CTRL1))= data | 0x30;
#endif

#ifdef CONFIG_MACH_SB2F
	pll	= *(volatile unsigned int *)(0xbfe78030);
        ddr_clk  =  (((pll>>8)&7)+3)*33333333;
//	ddr_clk = 33333333*3/3;
#else
	
	pll	= *(volatile unsigned int *)(0xbfe78030);
	ctrl = *(volatile unsigned int *)(0xbfe78034);	 
	clk=(12+(pll&0x3f))*33333333/2 + ((pll>>8)&0x3ff)*33333333/2/1024;
	ddr_clk=(ctrl&(1<<19))?clk/((ctrl>>14)&0x1f):clk/2;
//	ddr_clk = 33333333*3/3;
#endif
	for(i=0; i<12; i++)
		uart8250_data[i].uartclk = ddr_clk/2;

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

	i2c_register_board_info(0, ls1b_i2c_devs, ARRAY_SIZE(ls1b_i2c_devs));
#ifdef GC0308_ENABLED
	i2c_register_board_info(0, gc0308_i2c_info, ARRAY_SIZE(gc0308_i2c_info));
#endif

	spi_register_board_info(ls1b_spi0_devices, ARRAY_SIZE(ls1b_spi0_devices));
	
//	ls1b_spi0_devices[2].irq = ads7846_detect_penirq();
	ads7846_detect_penirq();
	
//modify by lvling
#if CONFIG_LS1B_GMAC0_OPEN && CONFIG_LS1B_GMAC1_OPEN//open gmac0 and gmac1  
  printk("open gmac0 and gmac1.\n");
  (*(volatile unsigned int *)0xbfd00420) |= (1 << 4 | 1 << 3);
  (*(volatile unsigned int *)0xbfd00424) |= (0xf);

#elif (CONFIG_LS1B_GMAC0_OPEN) && (~CONFIG_LS1B_GMAC1_OPEN)//open gmac0,close gmac1
  printk("open gmac0 close gmac1.\n");
  (*(volatile unsigned int *)0xbfd00424) |= (1 << 0 | 1 << 2); //open gmac0
  (*(volatile unsigned int *)0xbfd00424) &= ~(1 << 1 | 1 << 3); //close gmac1
  (*(volatile unsigned int *)0xbfd00420) &= ~(1 << 3 | 1 << 4);  //open uart0/1

#elif (~CONFIG_LS1BGMAC0_OPEN) && (CONFIG_LS1B_GMAC1_OPEN) //close gmac0,open gmac 1
  printk("close gmac0 open gmac1.\n");
  (*(volatile unsigned int *)0xbfd00424) &= ~(1 << 0 | 1 << 2); //close gmac0
  (*(volatile unsigned int *)0xbfd00424) |= (1 << 1 | 1 << 3); //open gmac1
  (*(volatile unsigned int *)0xbfd00420) |= (1 << 3 | 1 <<4); //close uart0/1
#endif
	
	return platform_add_devices(ls1b_platform_devices, ARRAY_SIZE(ls1b_platform_devices));
}

arch_initcall(ls1b_platform_init);
