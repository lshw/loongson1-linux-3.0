/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */
#include <linux/init.h>
#include <linux/oprofile.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include "op_impl.h"

#ifndef LS232_PERFCONTER
#define LS232_PERFCONTER
#endif

#ifndef Z_DEBUG
#define Z_DEBUG
#endif

#define PERF_DEBUG_Z 
//prom_printf("ls232 %s : %d\n",__FUNCTION__,__LINE__);

#define LS232_COUNTER1_EVENT(event)	((event&0x1f) << 5)
#define LS232_COUNTER1_SUPERVISOR	(1UL    <<  2)
#define LS232_COUNTER1_KERNEL		(1UL    <<  1)
#define LS232_COUNTER1_USER		(1UL    <<  3)
#define LS232_COUNTER1_ENABLE		(1UL    << 4)
#define LS232_COUNTER1_OVERFLOW		(1ULL    << 31)
#define LS232_COUNTER1_EXL		(1UL	<< 0)

#define LS232_COUNTER2_EVENT(event)	((event&0x1f) << 5)
#define LS232_COUNTER2_SUPERVISOR	LS232_COUNTER1_SUPERVISOR
#define LS232_COUNTER2_KERNEL		LS232_COUNTER1_KERNEL
#define LS232_COUNTER2_USER		LS232_COUNTER1_USER
#define LS232_COUNTER2_ENABLE		LS232_COUNTER1_ENABLE
#define LS232_COUNTER2_OVERFLOW		(1ULL   << 31)
#define LS232_COUNTER2_EXL		(1UL	<< 0 )
#define LS232_COUNTER_EXL		(1UL << 0)	

extern unsigned int ls232_perfcount_irq;

static struct ls232_register_config {
	unsigned int control0;
	unsigned int control1;
	unsigned long reset_counter1;
	unsigned long reset_counter2;
	int ctr1_enable, ctr2_enable;
} reg;
unsigned long ctr3_enable, ctr3_count;

uint64_t PC0_HI=0;
uint64_t PC1_HI=0;

spinlock_t sample_lock;

static char *oprofid = "GodsonPerf";
irqreturn_t ls232_perfcount_handler(int irq, void * dev_id);
//	struct pt_regs *regs);
/* Compute all of the registers in preparation for enabling profiling.  */

static int myoprofileclock=0;

#ifdef Z_DEBUG
struct op_counter_config ctr[3];
#endif
static void ctr_init(void)
{
#ifdef Z_DEBUG
ctr[0].enabled=1;
ctr[1].enabled=1;
ctr[0].user=1;
ctr[1].user=1;
ctr[0].kernel=1;
ctr[1].kernel=1;
#endif

}

//#ifdef Z_DEBUG
//static void ls232_reg_setup(void)
//#else
static void ls232_reg_setup(struct op_counter_config *ctr)
//#endif
{
	unsigned int control0 = 0;
	unsigned int control1 = 0;
PERF_DEBUG_Z
	reg.reset_counter1 = 0;
	reg.reset_counter2 = 0;
//ctr_init();

#ifndef Z_DEBUG
ctr[0].enabled=1;
ctr[1].enabled=1;
ctr[0].user=1;
ctr[1].user=1;
ctr[0].kernel=0;
ctr[1].kernel=0;
#endif

	/* Compute the performance counter control word.  */
	/* For now count kernel and user mode */
	if (ctr[0].enabled){
PERF_DEBUG_Z
		control0 |= LS232_COUNTER1_EVENT(ctr[0].event) |
					LS232_COUNTER1_ENABLE;
		if(ctr[0].kernel)
			control0 |= LS232_COUNTER1_KERNEL;
		if(ctr[0].user)
			control0 |= LS232_COUNTER1_USER;
		control0 |= LS232_COUNTER_EXL;
		reg.reset_counter1 = 0x80000000UL - ctr[0].count;
		myoprofileclock=ctr[0].count-10000;
	}

	if (ctr[1].enabled){
PERF_DEBUG_Z
		control1 |= LS232_COUNTER2_EVENT(ctr[1].event) |
		           LS232_COUNTER2_ENABLE;
		if(ctr[1].kernel)
			control1 |= LS232_COUNTER2_KERNEL;
		if(ctr[1].user)
			control1 |= LS232_COUNTER2_USER;
		control1 |= LS232_COUNTER_EXL;
		reg.reset_counter2 = (0x80000000UL- ctr[1].count) ;
	}

//	if(ctr[0].enabled ||ctr[1].enabled)
//		control |= LS232_COUNTER_EXL;

	if(ctr[2].enabled){
PERF_DEBUG_Z
		ctr3_enable = 1;
		ctr3_count = ctr[2].count;
	}

	reg.control0 = control0;
	reg.control1 = control1;

	reg.ctr1_enable = ctr[0].enabled;
	reg.ctr2_enable = ctr[1].enabled;

}

/* Program all of the registers in preparation for enabling profiling.  */

//#ifdef Z_DEBUG
//static void ls232_cpu_setup (void)
//#else
static void ls232_cpu_setup (void *args)
//#endif
{
	//uint64_t perfcount;
	uint32_t perfcount0;
	uint32_t perfcount1;

PERF_DEBUG_Z
#if 1
	//perfcount = (reg.reset_counter2 << 32) |reg.reset_counter1; 
	perfcount0 = reg.reset_counter1; 
	perfcount1 = reg.reset_counter2; 
#ifdef LS232_PERFCONTER
	write_c0_perfcntr0(perfcount0);
	write_c0_perfcntr1(perfcount1);
#else
	write_c0_perflo(reg.control);
	write_c0_perfhi(perfcount);
#endif
#endif
}

static int myoprofilestart=0;

#if 0
int myoprofile_setup()
{
PERF_DEBUG_Z
return myoprofileclock;
}
#endif

int myoprofile(int irq, void *dev_id, struct pt_regs *regs)
{
PERF_DEBUG_Z
if(myoprofilestart)
			oprofile_add_sample(regs, 0);
return 0;
}

static void ls232_cpu_start(void *args)
{
#if 1
PERF_DEBUG_Z
#ifdef Z_DEBUG
reg.ctr1_enable=1;
reg.ctr2_enable=1;
#endif
	/* Start all counters on current CPU */
	if(reg.ctr1_enable || reg.ctr2_enable)
		request_irq(62, ls232_perfcount_handler,
	                   IRQF_DISABLED, "Perfcounter", oprofid);
	 write_c0_perfctrl0(reg.control0);
	 write_c0_perfctrl1(reg.control1);
#else
myoprofilestart=1;
#endif
}

static void ls232_cpu_stop(void *args)
{
#if 1
	/* Stop all counters on current CPU */
PERF_DEBUG_Z
#ifdef LS232_PERFCONTER
	PC0_HI=0;
     	PC1_HI=0;
        write_c0_perfctrl0(0);
	write_c0_perfctrl1(0);
        write_c0_perfcntr0(0);
	write_c0_perfcntr1(0);
#else
	write_c0_perflo(0);
#endif
	ctr3_enable = 0;
	if(reg.ctr1_enable || reg.ctr2_enable)
		free_irq(62, oprofid);
	memset(&reg, 0, sizeof(reg));
#else
myoprofilestart=0;
#endif
}

//uint64_t perfcounter;
unsigned long perfcounter;
irqreturn_t ls232_perfcount_handler(int irq, void * dev_id)
//	struct pt_regs *regs)
{
	 uint64_t counter1, counter2;
	 int enabled;
	 unsigned long flags;
 	 struct pt_regs *regs = get_irq_regs();

	/*
	 * LS2322 defines two 32-bit performance counters.
	 * To avoid a race updating the registers we need to stop the counters 
	 * while we're messing with
	 * them ...
	 */
	
	/* Check whether the irq belongs to me*/
	enabled = reg.ctr1_enable| reg.ctr2_enable;
	if(!enabled){
		return IRQ_HANDLED; //Ack the counter
	}

#ifdef LS232_PERFCONTER
	counter1 = read_c0_perfcntr0();
	counter2 = read_c0_perfcntr1();
#else
	counter = read_c0_perfhi();
	counter1 = counter & 0xffffffff;
	counter2 = counter >> 32;
#endif

	spin_lock_irqsave(&sample_lock, flags);

	if (counter1 & LS232_COUNTER1_OVERFLOW) {
		if(reg.ctr1_enable)
			PC0_HI++;
			oprofile_add_sample(regs, 0);
		counter1 = reg.reset_counter1;
	}
	if (counter2 & LS232_COUNTER2_OVERFLOW) {
		if(reg.ctr2_enable)
			PC1_HI++;
			oprofile_add_sample(regs, 1);
		counter2 = reg.reset_counter2;
	}

	spin_unlock_irqrestore(&sample_lock, flags);

#ifdef LS232_PERFCONTER
	write_c0_perfctrl0(counter1);
	write_c0_perfctrl1(counter2);
#else
	write_c0_perfhi((counter2 << 32) | counter1);
#endif

	return IRQ_HANDLED;
}

int perfcounter_proc_read(char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
    int len = 0;
    unsigned long counter0, counter1;
    unsigned int control0,control1;

PERF_DEBUG_Z
    local_irq_disable();

#ifdef LS232_PERFCONTER
    counter0 = read_c0_perfcntr0();
    counter1 = read_c0_perfcntr1();
    control0 = read_c0_perfctrl0();
    control1 = read_c0_perfctrl1();


#else
    counter0 = read_c0_perflo();
    counter1 = read_c0_perfhi();
#endif

    len += sprintf(page, "cl0:%x\n", control0);
    len += sprintf(page+len, "cr0:%lx\n", counter0);
    len += sprintf(page+len, "cl1:%x\n", control1);
    len += sprintf(page+len, "cr1:%lx\n", counter1);

    len += sprintf(page+len, "counter0_all: %016llx\n", PC0_HI<<32 | counter0);
    len += sprintf(page+len, "counter1_all: %016llx\n", PC1_HI<<32 | counter1);

    *eof = 1;
    len = len - off;
    if(len < 0)
        len = 0;
    *start += len;

    local_irq_enable();

    return len;

	}

int perfcounter_proc_write(struct file *file, const char __user *buffer,
			   unsigned long count, void *data)
{
	char buf[20];
	unsigned long value;
	//unsigned long long value;
	
	//copy_from_user(buf, buffer, min(20, count));
	copy_from_user(buf, buffer, count);
	value = simple_strtoull(buf, NULL, 0);

#ifdef LS232_PERFCONTER
	if(value&0x80000000)
		write_c0_perfctrl1(value);
	else
		write_c0_perfctrl0(value);
#else
	write_c0_perflo(value);
#endif

	return count;
}

static int __init ls232_init(void)
{
	struct proc_dir_entry *ent;
PERF_DEBUG_Z
	ent = create_proc_entry("perfcounter", 0, NULL);
	if(!ent) 
		return -ENOMEM;		
	ent->read_proc = perfcounter_proc_read;
	ent->write_proc = perfcounter_proc_write;

	spin_lock_init(&sample_lock);
	return 0;	
}

static void ls232_exit(void)
{
	remove_proc_entry("perfcounter", NULL);
}

struct op_mips_model op_model_ls232_ops = {
	.reg_setup	= ls232_reg_setup,
	.cpu_setup	= ls232_cpu_setup,
	.init		= ls232_init,	
	.exit		= ls232_exit,
	.cpu_start	= ls232_cpu_start,
	.cpu_stop	= ls232_cpu_stop,
#ifdef LS232_PERFCONTER
	.cpu_type	= "mips/ls232",
#else
	.cpu_type	= "mips/ls2322",
#endif
	.num_counters	= 3
};
