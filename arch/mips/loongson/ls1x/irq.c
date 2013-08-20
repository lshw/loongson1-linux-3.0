/*
 * Copyright (c) 2012 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/irq_cpu.h>

#include <loongson1.h>
#include <irq.h>

static struct ls1x_intc_regs volatile *ls1x_icregs
	= (struct ls1x_intc_regs volatile *)(KSEG1ADDR(LS1X_INTREG_BASE));

static int ls1x_irq_set_type(struct irq_data *irq_data, unsigned int flow_type);

static void ls1x_irq_ack(struct irq_data *irq_data)
{
	(ls1x_icregs + (irq_data->irq >> 5))->int_clr |= 
		(1 << (irq_data->irq & 0x1f));
}

static void ls1x_irq_mask(struct irq_data *irq_data)
{
	(ls1x_icregs + (irq_data->irq >> 5))->int_en &= 
		~(1 << (irq_data->irq & 0x1f));
}

static void ls1x_irq_mask_ack(struct irq_data *irq_data)
{
	(ls1x_icregs + (irq_data->irq >> 5))->int_clr |= 
		(1 << (irq_data->irq & 0x1f));
	(ls1x_icregs + (irq_data->irq >> 5))->int_en &= 
		~(1 << (irq_data->irq & 0x1f));
}

static void ls1x_irq_unmask(struct irq_data *irq_data)
{
	(ls1x_icregs + (irq_data->irq >> 5))->int_en |= 
		(1 << (irq_data->irq & 0x1f));
}
/*
static void ls1x_irq_disable(struct irq_data *irq_data)
{
	(ls1x_icregs + (irq_data->irq >> 5))->int_en &= 
		~(1 << (irq_data->irq & 0x1f));
}

static void ls1x_irq_enable(struct irq_data *irq_data)
{
	(ls1x_icregs + (irq_data->irq >> 5))->int_en |= 
		(1 << (irq_data->irq & 0x1f));
}
*/

static struct irq_chip ls1x_irq_chip = {
	.name 		= "LS1X-INTC",
	.irq_ack 	= ls1x_irq_ack,
	.irq_mask 	= ls1x_irq_mask,
	.irq_unmask 	= ls1x_irq_unmask,
	.irq_mask_ack	= ls1x_irq_mask_ack,
	.irq_set_type 	= ls1x_irq_set_type,
//	.irq_enable		= ls1x_irq_enable,
//	.irq_disable	= ls1x_irq_disable,
};

static int ls1x_irq_set_type(struct irq_data *irq_data, unsigned int flow_type)
{
	switch (flow_type) {
	case IRQ_TYPE_EDGE_RISING:
		(ls1x_icregs + (irq_data->irq >> 5))->int_pol |= (1 << (irq_data->irq & 0x1f));
		(ls1x_icregs + (irq_data->irq >> 5))->int_edge |= (1 << (irq_data->irq & 0x1f));
		break;
	case IRQ_TYPE_EDGE_FALLING:
		(ls1x_icregs + (irq_data->irq >> 5))->int_pol &= ~(1 << (irq_data->irq & 0x1f));
		(ls1x_icregs + (irq_data->irq >> 5))->int_edge |= (1 << (irq_data->irq & 0x1f));
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		(ls1x_icregs + (irq_data->irq >> 5))->int_pol |= (1 << (irq_data->irq & 0x1f));
		(ls1x_icregs + (irq_data->irq >> 5))->int_edge &= ~(1 << (irq_data->irq & 0x1f));
		break;
	case IRQ_TYPE_LEVEL_LOW:
		(ls1x_icregs + (irq_data->irq >> 5))->int_pol &= ~(1 << (irq_data->irq & 0x1f));
		(ls1x_icregs + (irq_data->irq >> 5))->int_edge &= ~(1 << (irq_data->irq & 0x1f));
		break;
	case IRQ_TYPE_EDGE_BOTH:
//		printk(KERN_WARNING "No edge_both irq type %d", flow_type);
		/* Loongson1 上升沿和下降沿都会触发？ */
		(ls1x_icregs + (irq_data->irq >> 5))->int_pol &= ~(1 << (irq_data->irq & 0x1f));
		(ls1x_icregs + (irq_data->irq >> 5))->int_edge |= (1 << (irq_data->irq & 0x1f));
		break;
	case IRQ_TYPE_NONE:
		printk(KERN_WARNING "No irq type setting!\n");
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void ls1x_irq_dispatch(int n)
{
	u32 intstatus, irq;

	/* Receive interrupt signal, compute the irq */
	intstatus = (ls1x_icregs+n)->int_isr & (ls1x_icregs+n)->int_en;
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
		do_IRQ(TIMER_IRQ);
	}
	else if (pending & CAUSEF_IP2) {
		ls1x_irq_dispatch(0);
	}
	else if (pending & CAUSEF_IP3) {
		ls1x_irq_dispatch(1);
	}
	else if (pending & CAUSEF_IP4) {
		ls1x_irq_dispatch(2);
	}
	else if (pending & CAUSEF_IP5) {
		ls1x_irq_dispatch(3);
	}
	else if (pending & CAUSEF_IP6) {
		ls1x_irq_dispatch(4);
	} else {
		spurious_interrupt();
	}
}

static struct irqaction cascade_irqaction = {
	.handler = no_action,
	.name    = "cascade",
	.flags   = IRQF_NO_THREAD,
};

void __init arch_init_irq(void)
{
	u32 i;

	mips_cpu_irq_init();

	/* Disable interrupts and clear pending,
	 * setup all IRQs as high level triggered
	 */
	for (i=0; i<INTN; i++) {
		(ls1x_icregs+i)->int_en = 0x0;
		(ls1x_icregs+i)->int_clr = 0xffffffff;
		if (i == 2)
			(ls1x_icregs+i)->int_pol = ~((INT_PCI_INTA)|(INT_PCI_INTB)|(INT_PCI_INTC)|(INT_PCI_INTD));
		else
			(ls1x_icregs+i)->int_pol = 0xffffffff;
		/* set DMA0, DMA1 and DMA2 to edge trigger */
		if (i == 0)
			(ls1x_icregs+i)->int_edge = 0x0000e000;
		else
			(ls1x_icregs+i)->int_edge = 0x00000000;
	}

	for (i=0; i<=LS1X_LAST_IRQ; i++) {
		irq_set_chip_and_handler(i, &ls1x_irq_chip, handle_level_irq);
	}

	setup_irq(INT0_IRQ, &cascade_irqaction);
	setup_irq(INT1_IRQ, &cascade_irqaction);
	setup_irq(INT2_IRQ, &cascade_irqaction);
	setup_irq(INT3_IRQ, &cascade_irqaction);
#if defined(CONFIG_LS1A_MACH) || defined(CONFIG_LS1C_MACH)
	setup_irq(INT4_IRQ, &cascade_irqaction);
#endif
}

