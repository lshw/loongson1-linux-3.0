/*
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/ioport.h>
#include <linux/module.h>

#include <asm/bootinfo.h>
#include <asm/wbflush.h>
#include <prom.h>

#ifdef CONFIG_VT
#include <linux/console.h>
#include <linux/screen_info.h>
#endif

#include <asm/mach-loongson/ls1x/ls1b_board.h>

extern void prom_printf(char *fmt, ...);

#ifdef CONFIG_64BIT
#define PTR_PAD(p) ((0xffffffff00000000)|((unsigned long long)(p)))
#else
#define PTR_PAD(p) (p)
#endif

void (*__wbflush)(void);
EXPORT_SYMBOL(__wbflush);

static void wbflush_sb2f(void)
{
      *(volatile unsigned long *)CKSEG1;
	  asm(".set\tpush\n\t"
	  ".set\tnoreorder\n\t"
	  ".set mips3\n\t"
	  "sync\n\t"
	  "nop\n\t"
	  ".set\tpop\n\t"
	  ".set mips0\n\t");
}

void  __init plat_mem_setup(void)
{
	set_io_port_base(PTR_PAD(0xbc000000));
	
	ioport_resource.start = 0;
	ioport_resource.end = 0xffffffff;
	iomem_resource.start = 0;
	iomem_resource.end = 0xffffffff;

	__wbflush = wbflush_sb2f;

	add_memory_region(0, memsize<<20, BOOT_MEM_RAM);  

#ifdef CONFIG_VT
#if defined(CONFIG_VGA_CONSOLE)
	 conswitchp = &vga_con;

	 screen_info = (struct screen_info) {
	         0, 25,                  /* orig-x, orig-y */
	         0,                      /* unused */
	         0,                      /* orig-video-page */
	         0,                      /* orig-video-mode */
	         80,                     /* orig-video-cols */
	         0,0,0,                  /* ega_ax, ega_bx, ega_cx */
	         25,                     /* orig-video-lines */
	         VIDEO_TYPE_VGAC,        /* orig-video-isVGA */
	         16                      /* orig-video-points */
	 };      
#elif defined(CONFIG_DUMMY_CONSOLE)
	 	conswitchp = &dummy_con;
#endif
#endif


	/* Shut down all device DMA */
	/* LCD Controller DMA */
//	*(unsigned int *)KSEG1ADDR(FCR_SOC_LCD_BASE+REG_LCD_CTRL) = 0x000201a0;
	/* Reset MAC to shut down MAC DMA */
//	*(unsigned int *)KSEG1ADDR(FCR_SOC_MAC1_BASE+REG_MAC_CTRLMODER) = 0x00000000;
//	*(unsigned int *)KSEG1ADDR(FCR_SOC_MAC1_BASE+REG_MAC_MODER) = 0x00800000;
	
//	prom_printf("system_state: %d\n", system_state);
	
/*
	status = inb(FCR_SOC_APB_MISC_BASE+REG_APB_MISC_CTL);
	status |= 0xc0; //SET LED
	outb(FCR_SOC_APB_MISC_BASE+REG_APB_MISC_CTL, status);
*/
}

