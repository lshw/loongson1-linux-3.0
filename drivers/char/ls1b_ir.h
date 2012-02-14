#ifndef _LS1B_IR_H_
#define _LS1B_IR_H_

#define GPIO_IR 61

#define SYSTEMCODE_BIT_NUM 16

#define LS1B_IR_STATE_IDLE					0
#define LS1B_IR_STATE_RECEIVESTARTCODE		1
#define LS1B_IR_STATE_RECEIVESYSTEMCODE		2
#define LS1B_IR_STATE_RECEIVEDATACODE		3

static struct resource *ls1b_ir_res = NULL;
static unsigned int ls1b_ir_irq = 0;
static unsigned int ls1b_ir_state = LS1B_IR_STATE_IDLE;
//static spinlock_t ls1b_ir_lock = SPIN_LOCK_UNLOCKED;
static DEFINE_SPINLOCK(ls1b_ir_lock);
static struct timeval ls1b_ir_current_tv = {0, 0};
static struct timeval ls1b_ir_last_tv = {0, 0};
static unsigned int ls1b_ir_interval = 0;
static unsigned int ls1b_ir_systembit_count = 0;
static unsigned int ls1b_ir_databit_count = 0;
static unsigned int ls1b_ir_key_code_tmp = 0;
static unsigned int ls1b_ir_key_code = 0;
static struct ls1b_board_intc_regs volatile *ls1b_board_int0_regs
	= (struct ls1b_board_intc_regs volatile *)(KSEG1ADDR(LS1B_BOARD_INTREG_BASE));

DECLARE_WAIT_QUEUE_HEAD(ls1b_wate_queue);

#endif
