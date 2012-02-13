/*
 *
 * BRIEF MODULE DESCRIPTION
 *	LS1B BOARD interrupt/setup routines.
 *
 * Copyright 2000,2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
 *
 * Part of this file was derived from Carsten Langgaard's 
 * arch/mips/ite-boards/generic/init.c.
 *
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
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
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/serial_reg.h>

#include <asm/bitops.h>
#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/system.h>
#include <asm/mach-loongson/ls1b/ls1b_board.h>
#include <asm/mach-loongson/ls1b/ls1b_board_int.h>
#include <asm/mach-loongson/ls1b/ls1b_board_dbg.h>

#undef DEBUG_IRQ
//#define	DEBUG_IRQ
#ifdef DEBUG_IRQ
/* note: prints function name for you */
#define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#ifdef CONFIG_REMOTE_DEBUG
extern void breakpoint(void);
#endif

/* revisit */
#define EXT_IRQ0_TO_IP 2 /* IP 2 */
#define EXT_IRQ5_TO_IP 7 /* IP 7 */

#define ALLINTS_NOTIMER (IE_IRQ0 | IE_IRQ1 | IE_IRQ2 | IE_IRQ3 | IE_IRQ4)

void disable_ls1b_board_irq(struct irq_data *irq_data);
void enable_ls1b_board_irq(struct irq_data *irq_data);

extern void set_debug_traps(void);
// extern void mips_timer_interrupt(int irq, struct pt_regs *regs);
extern void mips_timer_interrupt(int irq);
extern asmlinkage void ls1b_board_IRQ(void);

static struct ls1b_board_intc_regs volatile *ls1b_board_hw0_icregs
	= (struct ls1b_board_intc_regs volatile *)(KSEG1ADDR(LS1B_BOARD_INTREG_BASE));


void ack_ls1b_board_irq(struct irq_data *irq_data)
{
	DPRINTK("ack_ls1b_board_irq %d\n", irq_data->irq);
    	(ls1b_board_hw0_icregs+(irq_data->irq>>5))->int_clr |= (1 << (irq_data->irq&0x1f));
}
void disable_ls1b_board_irq(struct irq_data *irq_data)
{
	DPRINTK("disable_ls1b_board_irq %d\n", irq_data->irq);
    	(ls1b_board_hw0_icregs+(irq_data->irq>>5))->int_en &= ~(1 << (irq_data->irq&0x1f));
}

void enable_ls1b_board_irq(struct irq_data *irq_data)
{
	DPRINTK("enable_ls1b_board_irq %d\n", irq_data->irq);
    	(ls1b_board_hw0_icregs+(irq_data->irq>>5))->int_en |= (1 << (irq_data->irq&0x1f));
}

static unsigned int startup_ls1b_board_irq(unsigned int irq)
{
	enable_ls1b_board_irq(irq);
	return 0; 
}

#define shutdown_ls1b_board_irq	disable_ls1b_board_irq
#define mask_and_ack_ls1b_board_irq    disable_ls1b_board_irq

#if 0
static void end_ls1b_board_irq(unsigned int irq)
{
	if (!(irq_desc[irq].istate & (IRQ_DISABLED|IRQ_INPROGRESS))){		//lxy
		(ls1b_board_hw0_icregs+(irq>>5))->int_clr |= 1 << (irq&0x1f); 
		//if(irq<LS1B_BOARD_GPIO_FIRST_IRQ) 
		enable_ls1b_board_irq(irq);
	}
}
#endif


static struct irq_chip ls1b_board_irq_chip = {
	.name = "LS1B BOARD",
	.irq_ack = ack_ls1b_board_irq,		//lxy
	.irq_mask = disable_ls1b_board_irq,
	.irq_unmask = enable_ls1b_board_irq,
//	.irq_eoi = enable_ls1b_board_irq,
//	.irq_end = end_ls1b_board_irq,
};

// void ls1b_board_hw0_irqdispatch(struct pt_regs *regs)
void ls1b_board_hw_irqdispatch(int n)
{
	int irq;
	int intstatus = 0;
   	int status;

	/* Receive interrupt signal, compute the irq */
	status = read_c0_cause();
	intstatus = (ls1b_board_hw0_icregs+n)->int_isr;
	
	irq=ffs(intstatus);
//	prom_printf("irq=%d,n=%d,realirq=%d\n",irq,n,n*32+irq-1);
	
	if(!irq){
//		printk("Unknow interrupt status %x intstatus %x \n" , status, intstatus);
		return; 
	}
	else do_IRQ(n*32+irq-1);
}

void ls1b_board_irq_init(u32 irq_base)
{
//	extern irq_desc_t irq_desc[];
	u32 i;
	for (i= 0; i<= LS1B_BOARD_LAST_IRQ; i++) {
		irq_set_chip_and_handler(i, &ls1b_board_irq_chip, handle_level_irq);
		//set_irq_chip_and_handler(i, &ls1b_board_irq_chip, handle_level_irq);	//lxy
	}

}
