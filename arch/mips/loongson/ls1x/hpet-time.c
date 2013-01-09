/*
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/clockchips.h>

#include <asm/time.h>

#include <ls1b_board.h>
#include <irq.h>
//#include <asm/mach-loongson/ls1x/ls1x_time.h>

#define TIMER_CLOCKEVENT 0
#define TIMER_CLOCKSOURCE 1

#if 0
#define TIMER_CNTR	0x00
#define TIMER_HRC	0x04
#define TIMER_LRC	0x08
#define TIMER_CTRL	0x0c
#endif

#define	GEN_CAP_L	0x0
#define	GEN_CAP_H	0x4
#define	GEN_CONF	0x10
#define	GEN_INTN_STATUS	0x20
#define	GEN_MAIN_COUNT	0xf0
#define	T0_CONF_CAP	0x100
#define	T0_COM_VAL	0x108
#define	T1_CONF_CAP	0x120
#define	T1_COM_VAL	0x128
#define	T2_CONF_CAP	0x140
#define	T2_COM_VAL	0x148

void __iomem *ls1x_timer_base;
extern unsigned long ls1x_bus_clock;
static uint32_t ls1x_jiffies_per_tick;

DEFINE_SPINLOCK(ls1x_lock);
EXPORT_SYMBOL(ls1x_lock);

static cycle_t ls1x_clocksource_read(struct clocksource *cs)
{
	unsigned long flags;
	int count;
	u32 jifs;
	static int old_count;
	static u32 old_jifs;

	spin_lock_irqsave(&ls1x_lock, flags);
	/*
	 * Although our caller may have the read side of xtime_lock,
	 * this is now a seqlock, and we are cheating in this routine
	 * by having side effects on state that we cannot undo if
	 * there is a collision on the seqlock and our caller has to
	 * retry.  (Namely, old_jifs and old_count.)  So we must treat
	 * jiffies as volatile despite the lock.  We read jiffies
	 * before latching the timer count to guarantee that although
	 * the jiffies value might be older than the count (that is,
	 * the counter may underflow between the last point where
	 * jiffies was incremented and the point where we latch the
	 * count), it cannot be newer.
	 */
	jifs = jiffies;
	/* read the count */
	count = readl(ls1x_timer_base + GEN_MAIN_COUNT);

	/*
	 * It's possible for count to appear to go the wrong way for this
	 * reason:
	 *
	 *  The timer counter underflows, but we haven't handled the resulting
	 *  interrupt and incremented jiffies yet.
	 *
	 * Previous attempts to handle these cases intelligently were buggy, so
	 * we just do the simple thing now.
	 */
	if (count < old_count && jifs == old_jifs)
		count = old_count;

	old_count = count;
	old_jifs = jifs;

	spin_unlock_irqrestore(&ls1x_lock, flags);

	return (cycle_t) (jifs * ls1x_jiffies_per_tick) + count;

//	return readl(ls1x_timer_base + TIMER_CNTR);
}

static struct clocksource ls1x_clocksource = {
	.name = "ls1x-timer",
	.rating = 200,
	.read = ls1x_clocksource_read,
//	.mask = CLOCKSOURCE_MASK(24),
	.mask = CLOCKSOURCE_MASK(32),
	.flags = CLOCK_SOURCE_IS_CONTINUOUS,
};

static irqreturn_t ls1x_clockevent_irq(int irq, void *devid)
{
	struct clock_event_device *cd = devid;

//	writel(readl(ls1x_timer_base+TIMER_CTRL) | 0x40, ls1x_timer_base + TIMER_CTRL);
//	if (cd->mode != CLOCK_EVT_MODE_PERIODIC)
//		writel(readl(ls1x_timer_base+TIMER_CTRL) & 0xfe, ls1x_timer_base + TIMER_CTRL);

	writel(0x00, ls1x_timer_base + GEN_CONF);
	writel(0x00, ls1x_timer_base + GEN_MAIN_COUNT);
	writel(0x01, ls1x_timer_base + GEN_CONF);

	cd->event_handler(cd);

	return IRQ_HANDLED;
}

static void ls1x_clockevent_set_mode(enum clock_event_mode mode,
	struct clock_event_device *cd)
{
	spin_lock(&ls1x_lock);
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		writel(0x00, ls1x_timer_base + GEN_CONF);
		writel(ls1x_jiffies_per_tick, ls1x_timer_base + T0_COM_VAL);
		writel(0x00, ls1x_timer_base + GEN_MAIN_COUNT);
		writel(0x01, ls1x_timer_base + GEN_CONF);
	case CLOCK_EVT_MODE_RESUME:
		writel(0x01, ls1x_timer_base + GEN_CONF);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		break;
	case CLOCK_EVT_MODE_SHUTDOWN:
		writel(0x00, ls1x_timer_base + GEN_CONF);
		break;
	default:
		break;
	}
	spin_unlock(&ls1x_lock);
}

static int ls1x_clockevent_set_next(unsigned long evt,
	struct clock_event_device *cd)
{
	writel(0x00, ls1x_timer_base + GEN_CONF);
	writel(evt, ls1x_timer_base + T0_COM_VAL);
	
	writel(0x00, ls1x_timer_base + GEN_MAIN_COUNT);
	writel(0x01, ls1x_timer_base + GEN_CONF);

	return 0;
}

static struct clock_event_device ls1x_clockevent = {
	.name = "ls1a-hpet",
	.features = CLOCK_EVT_FEAT_PERIODIC,	// | CLOCK_EVT_FEAT_ONESHOT,
	.set_next_event = ls1x_clockevent_set_next,
	.set_mode = ls1x_clockevent_set_mode,
	.rating = 200,
	.irq = LS1A_BOARD_HPET_IRQ,
};

static struct irqaction timer_irqaction = {
	.handler	= ls1x_clockevent_irq,
	.flags		= IRQF_PERCPU | IRQF_TIMER,
	.name		= "ls1a-hpetirq",
	.dev_id		= &ls1x_clockevent,
};

void __init ls1x_timer_init(void)
{
	ls1x_timer_base = ioremap(LS1A_HPET_BASE, 0x15f);
	if (!ls1x_timer_base)
		panic("Failed to ioremap timer registers");
}

void __init setup_ls1x_timer(void)
{
	int ret;
	uint32_t clk_rate;

	ls1x_timer_init();

//	clk_rate = ls1x_bus_clock / 2;
	clk_rate = 47683716;
	ls1x_jiffies_per_tick = DIV_ROUND_CLOSEST(clk_rate, HZ);

	clockevent_set_clock(&ls1x_clockevent, clk_rate);
	ls1x_clockevent.min_delta_ns = clockevent_delta2ns(0x4, &ls1x_clockevent);
	ls1x_clockevent.max_delta_ns = clockevent_delta2ns(0xffff, &ls1x_clockevent);
	ls1x_clockevent.cpumask = cpumask_of(0);

	clockevents_register_device(&ls1x_clockevent);

	ret = clocksource_register_hz(&ls1x_clocksource, clk_rate);

	if (ret)
		printk(KERN_ERR "Failed to register clocksource: %d\n", ret);

	setup_irq(LS1A_BOARD_HPET_IRQ, &timer_irqaction);

	writel(0x00, ls1x_timer_base + GEN_CONF);
	writel(0x00, ls1x_timer_base + GEN_MAIN_COUNT);

	writel(ls1x_jiffies_per_tick, ls1x_timer_base + T0_COM_VAL);
	writel(0x04, ls1x_timer_base + T0_CONF_CAP);
	writel(0x00, ls1x_timer_base + T1_CONF_CAP);
	writel(0x00, ls1x_timer_base + T2_CONF_CAP);

	writel(0x01, ls1x_timer_base + GEN_CONF);
}
