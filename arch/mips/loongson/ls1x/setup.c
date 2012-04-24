/*
 * BRIEF MODULE DESCRIPTION
 * GodsonEV2e - board dependent boot routines
 *
 * Copyright 2006 Lemote Inc.
 * Author: zhangfx@lemote.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
//#include <linux/config.h>
//#include <linux/autoconf.h>
#include <generated/autoconf.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/pci.h>

#include <asm/mc146818-time.h>
#include <asm/time.h>
#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/pci.h>

#include <linux/bootmem.h>
#include <linux/tty.h>
#include <linux/mc146818rtc.h>

#ifdef CONFIG_VT
#include <linux/console.h>
#include <linux/screen_info.h>
#endif
// #include <asm/ict/tc-godson.h>
#include <asm/mach-loongson/ls1x/ls1b_board.h>

extern void prom_printf(char *fmt, ...);
extern void mips_reboot_setup(void);

#ifdef CONFIG_64BIT
#define PTR_PAD(p) ((0xffffffff00000000)|((unsigned long long)(p)))
#else
#define PTR_PAD(p) (p)
#endif


unsigned long ls1b_cpu_clock;
unsigned long bus_clock;
unsigned int  memsize;
unsigned int  highmemsize = 0;

static unsigned long ls232_rtc_get_time(void)
{
	struct rtc_time *rtc_tm;
	static unsigned int epoch= 1900;
	rtc_tm->tm_year +=  ((rtc_tm->tm_year >=70) ? 1900 : 2000);
	return mktime(rtc_tm->tm_year,rtc_tm->tm_mon, rtc_tm->tm_mday,rtc_tm->tm_hour, rtc_tm->tm_min, rtc_tm->tm_sec);
}

#if 0
void __init plat_timer_setup(struct irqaction *irq)
{
      setup_irq(MIPS_CPU_IRQ_BASE+7, irq);  
}
#endif

void __init plat_time_init(void)
{
	/* setup mips r4k timer */
	mips_hpt_frequency = ls1b_cpu_clock / 2;
}

static unsigned long __init mips_rtc_get_time(void)
{
	//return mc146818_get_cmos_time();
	return mktime(2009,11,28,12,30,20);
}

void (*__wbflush)(void);
EXPORT_SYMBOL(__wbflush);	//lxy
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
	unsigned char status;
	
//  prom_printf("LS232 CPU board\n");

	set_io_port_base(PTR_PAD(0xbc000000));
	
	ioport_resource.start = 0;
	ioport_resource.end = 0xffffffff;
	iomem_resource.start = 0;
	iomem_resource.end = 0xffffffff;

	mips_reboot_setup();

//	board_time_init = sb2f_time_init;
//	rtc_mips_get_time = mips_rtc_get_time;

	__wbflush = wbflush_sb2f;

//	prom_printf("memsize=%d,highmemsize=%d\n",memsize,highmemsize);
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

