/*
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/irq.h>
#include <linux/ptrace.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/irq_cpu.h>
#include <asm/i8259.h>
#include <asm/mipsregs.h>
#include <asm/system.h>
#include <asm/mach-loongson/ls1x/ls1b_board.h>
#include <asm/mach-loongson/ls1x/ls1b_board_int.h>
#include <asm/mach-loongson/ls1x/ls1b_board_dbg.h>

static struct ls1b_board_intc_regs volatile *ls1b_board_hw0_icregs
	= (struct ls1b_board_intc_regs volatile *)(KSEG1ADDR(LS1B_BOARD_INTREG_BASE));

static void ack_ls1b_board_irq(struct irq_data *irq_data)
{
	(ls1b_board_hw0_icregs+(irq_data->irq>>5))->int_clr |= (1 << (irq_data->irq&0x1f));
}

static void disable_ls1b_board_irq(struct irq_data *irq_data)
{
	(ls1b_board_hw0_icregs+(irq_data->irq>>5))->int_en &= ~(1 << (irq_data->irq&0x1f));
}

static void enable_ls1b_board_irq(struct irq_data *irq_data)
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

static void ls1b_board_hw_irqdispatch(int n)
{
	int irq;
	int intstatus = 0;

	/* Receive interrupt signal, compute the irq */
	intstatus = (ls1b_board_hw0_icregs+n)->int_isr & (ls1b_board_hw0_icregs+n)->int_en;

	if (intstatus) {
		irq = ffs(intstatus);
		do_IRQ((n<<5) + irq - 1);
	}
}

asmlinkage void plat_irq_dispatch(struct pt_regs *regs)
{
	unsigned int pending;

	pending = read_c0_cause() & read_c0_status() & ST0_IM;

	if (pending & CAUSEF_IP7) {
		do_IRQ(MIPS_CPU_IRQ_BASE + cp0_compare_irq);
	}
	else if (pending & CAUSEF_IP2) {
		ls1b_board_hw_irqdispatch(0);
	}
	else if (pending & CAUSEF_IP3) {
		ls1b_board_hw_irqdispatch(1);
	}
	else if (pending & CAUSEF_IP4) {
		ls1b_board_hw_irqdispatch(2);
	}
	else if (pending & CAUSEF_IP5) {
		ls1b_board_hw_irqdispatch(3);
	}
	else if (pending & CAUSEF_IP6) {
		ls1b_board_hw_irqdispatch(4);
	} else {
		spurious_interrupt();
	}
}

static struct irqaction cascade_irqaction = {
	.handler = no_action,
	.name    = "cascade",
	.flags   = IRQF_NO_THREAD,
};

static void __init ls1b_board_irq_init(void)
{
	u32 i;
	for (i= 0; i<= LS1B_BOARD_LAST_IRQ; i++) {
		irq_set_chip_and_handler(i, &ls1b_board_irq_chip, handle_level_irq);
	}
}

void __init arch_init_irq(void)
{
	int i;

	clear_c0_status(ST0_IM | ST0_BEV);
	local_irq_disable();

	for (i=0; i<5; i++) {
		/* active level setting */
		/* uart, keyboard, and mouse are active high */
		if(i == 2)
			(ls1b_board_hw0_icregs+i)->int_pol = ~( (INT_PCI_INTA)|(INT_PCI_INTB)|(INT_PCI_INTC)|(INT_PCI_INTD));	//pci active low
		else
			(ls1b_board_hw0_icregs+i)->int_pol = -1;	//high level triggered.

		/* make all interrupts level triggered */ 
		if(i ==  0)
			(ls1b_board_hw0_icregs+i)->int_edge = 0x0000e000;
		else
			(ls1b_board_hw0_icregs+i)->int_edge = 0x00000000;
		/* mask all interrupts */
		(ls1b_board_hw0_icregs+i)->int_clr = 0xffffffff;
	}

	ls1b_board_irq_init();
	mips_cpu_irq_init();	
	setup_irq(MIPS_CPU_IRQ_BASE + 2, &cascade_irqaction);
	setup_irq(MIPS_CPU_IRQ_BASE + 3, &cascade_irqaction);
	setup_irq(MIPS_CPU_IRQ_BASE + 4, &cascade_irqaction);
	setup_irq(MIPS_CPU_IRQ_BASE + 5, &cascade_irqaction);
#ifdef CONFIG_LS1A_MACH
	setup_irq(MIPS_CPU_IRQ_BASE + 6, &cascade_irqaction);
#endif
}

