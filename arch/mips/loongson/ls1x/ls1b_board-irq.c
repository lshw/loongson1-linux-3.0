/*
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
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
#include <asm/mach-loongson/ls1x/ls1b_board.h>
#include <asm/mach-loongson/ls1x/ls1b_board_int.h>
#include <asm/mach-loongson/ls1x/ls1b_board_dbg.h>

static struct ls1b_board_intc_regs volatile *ls1b_board_hw0_icregs
	= (struct ls1b_board_intc_regs volatile *)(KSEG1ADDR(LS1B_BOARD_INTREG_BASE));

void ack_ls1b_board_irq(struct irq_data *irq_data)
{
	(ls1b_board_hw0_icregs+(irq_data->irq>>5))->int_clr |= (1 << (irq_data->irq&0x1f));
}

void disable_ls1b_board_irq(struct irq_data *irq_data)
{
	(ls1b_board_hw0_icregs+(irq_data->irq>>5))->int_en &= ~(1 << (irq_data->irq&0x1f));
}

void enable_ls1b_board_irq(struct irq_data *irq_data)
{
	(ls1b_board_hw0_icregs+(irq_data->irq>>5))->int_en |= (1 << (irq_data->irq&0x1f));
}

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

/*
static int ls1b_board_irq_set_type(struct irq_data *irq_data, unsigned int flow_type)
{
	int mode;

	if (flow_type & IRQF_TRIGGER_PROBE)
		return 0;
	switch (flow_type & IRQF_TRIGGER_MASK) {
		case IRQF_TRIGGER_RISING:	mode = 0;	break;
		case IRQF_TRIGGER_FALLING:	mode = 1;	break;
		case IRQF_TRIGGER_HIGH:	mode = 2;	break;
		case IRQF_TRIGGER_LOW:	mode = 3;	break;
		default:
		return -EINVAL;
	}

	if(mode & 1)
		(ls1b_board_hw0_icregs+(irq_data->irq>>5))->int_pol = (1 << (irq_data->irq&0x1f));
	else
		(ls1b_board_hw0_icregs+(irq_data->irq>>5))->int_pol &= ~(1 << (irq_data->irq&0x1f));

	if(mode & 2)
		(ls1b_board_hw0_icregs+(irq_data->irq>>5))->int_edge = (1 << (irq_data->irq&0x1f));
	else
		(ls1b_board_hw0_icregs+(irq_data->irq>>5))->int_edge &= ~(1 << (irq_data->irq&0x1f));

	return 0;
}
*/

static struct irq_chip ls1b_board_irq_chip = {
	.name = "LS1B BOARD",
	.irq_ack = ack_ls1b_board_irq,
	.irq_mask = disable_ls1b_board_irq,
	.irq_unmask = enable_ls1b_board_irq,
//	.irq_set_type = ls1b_board_irq_set_type,
};

void ls1b_board_hw_irqdispatch(int n)
{
	int irq;
	int intstatus = 0;
   	int status;

	/* Receive interrupt signal, compute the irq */
	status = read_c0_cause();
	intstatus = (ls1b_board_hw0_icregs+n)->int_isr;
	
	irq=ffs(intstatus);
	
	if(!irq){
		return; 
	}
	else do_IRQ(n*32+irq-1);
}

void ls1b_board_irq_init(void)
{
	u32 i;
	for (i= 0; i<= LS1B_BOARD_LAST_IRQ; i++) {
		irq_set_chip_and_handler(i, &ls1b_board_irq_chip, handle_level_irq);
	}
}
