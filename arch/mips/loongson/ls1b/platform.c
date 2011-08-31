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
#include <linux/mtd/partitions.h>
#include <linux/spi/flash.h>
#include <asm/mach-loongson/ls1b/gpio_keys.h>	//lqx
#include <asm/mach-loongson/ls1b/ls1b_board_int.h>
#include <linux/i2c.h>
#include <linux/i2c/tsc2007.h>

static struct ls1b_board_intc_regs volatile *ls1b_board_hw0_icregs
	= (struct ls1b_board_intc_regs volatile *)(KSEG1ADDR(LS1B_BOARD_INTREG_BASE));


struct ls1g_nand_platform_data{
    int enable_arbiter;
    struct mtd_partition *parts;
    unsigned int nr_parts;
};

static struct mtd_partition ls1g_nand_partitions[]={
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

static struct ls1g_nand_platform_data ls1g_nand_parts = {
        .enable_arbiter =   1,
        .parts          =   ls1g_nand_partitions,
        .nr_parts       =   ARRAY_SIZE(ls1g_nand_partitions),
    
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
	.name       = "gs2fsb_wdt",
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

static struct platform_device ls1b_dc_device = {
 .name           = "ls1b-fb",
 .id             = -1,
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
static struct resource ls1g_nand_resources[] = {
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

struct platform_device ls1g_nand_device = {
    .name       ="ls1g-nand",
    .id         =-1,
    .dev        ={
        .platform_data = &ls1g_nand_parts,
    },
    .num_resources  =ARRAY_SIZE(ls1g_nand_resources),
    .resource       =ls1g_nand_resources,
};

static struct platform_device ls1b_audio_device = {
 .name           = "ls1b-audio",
 .id             = -1,
};


#if 1		//lxy
static struct mtd_partition partitions[] = { 
	{
		.name		= "pmon",
		.offset		= 0,
		.size		= 512 * 1024,	//512KB
	//	.mask_flags	= MTD_WRITEABLE,
	}, 
	{
		.name		= "kernel",	
		.offset		= 512 * 1024,
		.size		= 0x320000,
	},
	{
		.name		= "system",
		.offset		= 0x3a0000,
		.size		= 0x460000,
	}
};

static struct flash_platform_data flash = {
	.name		= "ls1b_norflash",
	.parts		= partitions,
	.nr_parts	= ARRAY_SIZE(partitions),
	.type		= "w25x64",
};
	static struct spi_board_info ls1b_spi_devices[] = {
		{	/* DataFlash chip */
			.modalias	= "m25p80",
			.chip_select	= 0,
			.max_speed_hz	= 80 * 1000 * 1000,
			.platform_data	= &flash,
		}
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
	
	static struct platform_device ls1b_spi0_device = {
		.name		= "ls1b-spi",
		.id 		= -1,
		.num_resources	= ARRAY_SIZE(ls1b_spi0_resource),
		.resource	= ls1b_spi0_resource,
		.dev		= {
			.platform_data	= &ls1b_spi_devices,
		},
	};
#endif

/************************************************/	//GPIO && buzzer && button
static struct gpio_keys_button ls1g_gpio_button[] = {
	[0] = {
		.keycode	= 'A',
		.gpio	 	= 37,
		.desc		= "SW1",
	},
	[1] = {
		.keycode	= 'B',
		.gpio	 	= 38,
		.desc		= "SW2",
	},
	[2] = {
		.keycode	= 'C',
		.gpio	 	= 39,
		.desc		= "SW3",
	},
	[3] = {
		.keycode	= 'D',
		.gpio	 	= 40,
		.desc		= "SW4",
	},
	[4] = {
		.keycode	= 'E',
		.gpio	 	= 41,
		.desc		= "SW5",
	},
};

static struct gpio_keys_platform_data ls1g_gpio_key_dat = {
	.buttons 	= ls1g_gpio_button,
	.nbuttons 	= 5, 
};

static struct platform_device ls1b_gpio_key_device = {
	.name 	= "gpio-keys",
	.id	= -1,
	.dev	= {
		.platform_data = &ls1g_gpio_key_dat,
	},
};

static struct gpio_keys_button ls1g_gpio_buzzer[] = {
	[0] = {
		.gpio	= 57,
	},	
};

static struct gpio_keys_platform_data ls1g_gpio_buzzer_data = {
	.buttons	= ls1g_gpio_buzzer,
	.nbuttons	= 1,
};

static struct platform_device ls1b_gpio_buzzer_device = {
	.name	= "buzzer_gpio",
	.id	= -1,
	.dev	= {
		.platform_data = &ls1g_gpio_buzzer_data,
	},
};

/***********************************************/



static struct platform_device *ls1b_platform_devices[] __initdata = {
    &ls1g_nand_device,
	&uart8250_device,
//	&ls1b_ahci_device,
	&ls1b_ohci_device,
	&ls1b_ehci_device,
	&ls1b_dc_device,
	&ls1b_gmac1_device,
//    &ls1b_gmac2_device,
	&ls1b_wat_device,
	&ls1b_rtc_device,
	&ls1b_i2c_device,
	&ls1b_audio_device,
	&ls1b_spi0_device,	//lxy
//	&ls1b_gpio_key_device,
//	&ls1b_gpio_buzzer_device,
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
	*(volatile int *)0xbfd00424 = 0;
	*(volatile int *)0xbfd00424;
	mdelay(1);
	/*ls1g usb reset stop*/
	*(volatile int *)0xbfd00424 = 0x80000000;
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

	i2c_register_board_info(0, ls1b_i2c_devs, ARRAY_SIZE(ls1b_i2c_devs));
	return platform_add_devices(ls1b_platform_devices, ARRAY_SIZE(ls1b_platform_devices));
}

arch_initcall(ls1b_platform_init);
