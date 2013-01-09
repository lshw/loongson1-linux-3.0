#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/errno.h>

#include <ls1b_board.h>
#include <irq.h>
#include <asm/segment.h>
#include <asm/ioctl.h>
#include <asm/uaccess.h>

#include "ls1b_ir.h"

#define LS1B_DEBUG

static inline unsigned int ls1b_ir_pinstate(void)
{
	unsigned int ret;
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_IN1)); 
	ret = ((ret >> (GPIO_IR - 32)) & 0x01);
	return ret;
}

static void ls1b_ir_irq_enable(void)
{
	unsigned int ret;
	unsigned long flag;
	
	spin_lock(&ls1b_ir_lock);
	local_irq_save(flag);
	
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_CFG1)); 
	ret |= (1 << (GPIO_IR - 32)); 
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_CFG1)) = ret;
	
	ret = *(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OE1));
	ret |= (1 << (GPIO_IR - 32));
	*(volatile unsigned int *)(KSEG1ADDR(REG_GPIO_OE1)) = ret;

	(ls1b_board_int0_regs + 3) -> int_edge	|= (1 << (GPIO_IR - 32));
	(ls1b_board_int0_regs + 3) -> int_pol	&= ~(1 << (GPIO_IR - 32));
	(ls1b_board_int0_regs + 3) -> int_clr	|= (1 << (GPIO_IR - 32));
	(ls1b_board_int0_regs + 3) -> int_set	&= ~(1 << (GPIO_IR - 32));
	(ls1b_board_int0_regs + 3) -> int_en	|= (1 << (GPIO_IR - 32));
	
	local_irq_restore(flag);
	spin_unlock(&ls1b_ir_lock);
}

static irqreturn_t ls1b_ir_irq_handler(int i, void *blah)
{	
	udelay(50);
	if (ls1b_ir_pinstate() != 0)		
		return IRQ_HANDLED;
	
	do_gettimeofday(&ls1b_ir_current_tv);
	if (ls1b_ir_current_tv.tv_sec == ls1b_ir_last_tv.tv_sec) {
		ls1b_ir_interval = ls1b_ir_current_tv.tv_usec - ls1b_ir_last_tv.tv_usec;
	} else  
	ls1b_ir_interval = 1000000 - ls1b_ir_last_tv.tv_usec + ls1b_ir_current_tv.tv_usec;
	ls1b_ir_last_tv = ls1b_ir_current_tv;
	
	if (ls1b_ir_interval > 800 && ls1b_ir_interval < 15000) {
		if (ls1b_ir_interval > 11000) {
				ls1b_ir_state = LS1B_IR_STATE_RECEIVESTARTCODE;
				ls1b_ir_key_code_tmp = 0;
				ls1b_ir_databit_count = 0;
				ls1b_ir_systembit_count =0;				
		} else if (ls1b_ir_state == LS1B_IR_STATE_RECEIVESTARTCODE) {
			if (ls1b_ir_systembit_count >= SYSTEMCODE_BIT_NUM - 1) {
				ls1b_ir_state = LS1B_IR_STATE_RECEIVESYSTEMCODE;
				ls1b_ir_systembit_count = 0;
			} else if ((ls1b_ir_interval > 800 && ls1b_ir_interval < 1300) || (ls1b_ir_interval > 1900 && ls1b_ir_interval < 2400)) {
				ls1b_ir_systembit_count ++;
			} else goto receive_errerbit;
		} else if (ls1b_ir_state == LS1B_IR_STATE_RECEIVESYSTEMCODE) {
			if (ls1b_ir_databit_count < 8) {
				if (ls1b_ir_interval > 1900 && ls1b_ir_interval < 2400) {
					ls1b_ir_key_code_tmp |= (1 << ls1b_ir_databit_count);
					ls1b_ir_databit_count++;
				} else if (ls1b_ir_interval > 800 && ls1b_ir_interval < 1300) {
					ls1b_ir_databit_count++;
				} else goto receive_errerbit;
			} else if ((ls1b_ir_interval > 800 && ls1b_ir_interval < 1300) || (ls1b_ir_interval > 1900 && ls1b_ir_interval < 2400)) {
				ls1b_ir_state = LS1B_IR_STATE_IDLE;
				ls1b_ir_key_code = ls1b_ir_key_code_tmp;
				ls1b_ir_key_code_tmp = 0;
				ls1b_ir_databit_count = 0;
				ls1b_ir_systembit_count =0;	
				wake_up_interruptible(&ls1b_wate_queue);
#ifdef LS1B_DEBUG
				printk("IR:Receive key code:%d.\n",ls1b_ir_key_code);
#endif
			} else goto receive_errerbit;	
		}
		ls1b_ir_interval = 0;
		(ls1b_board_int0_regs + 3) -> int_clr |= (1 << (GPIO_IR - 32));
		return IRQ_HANDLED;	
	}
	

receive_errerbit:
	ls1b_ir_state = LS1B_IR_STATE_IDLE;
	ls1b_ir_key_code_tmp = 0;
	ls1b_ir_databit_count = 0;
	ls1b_ir_systembit_count =0;
	ls1b_ir_interval = 0;
	(ls1b_board_int0_regs + 3) -> int_clr	|= (1 << (GPIO_IR - 32));
	return IRQ_HANDLED;
}

static ssize_t ls1b_ir_read(struct file *filp, char __user *buf, size_t count, loff_t *offp)
{
	if (copy_to_user(buf, &ls1b_ir_key_code, sizeof(unsigned int))) {
		printk("IR:ls1b_ir_read error!\n");
		return -EFAULT;
	}
	return sizeof(unsigned int);
}

static ssize_t ls1b_ir_write(struct file *filp, const char __user *buf, size_t count, loff_t *offp)
{
	if (copy_from_user(&ls1b_ir_key_code, buf, sizeof(unsigned int))) {
		printk("IR:ls1b_ir_write error!\n");
		return -EFAULT;
	}
	return sizeof(unsigned int);
}

static int ls1b_ir_open(struct inode *inode, struct file *filep)
{
	int ret;
	ls1b_ir_irq_enable();	
	ret = request_irq(ls1b_ir_irq, ls1b_ir_irq_handler, IRQF_DISABLED, "ls1b_ir", NULL);
	if (ret) {
		printk("IR:ir_irq_handler resigered Error:%d\n", ret);
		return ret;
	}
	return 0;
}

static int ls1b_ir_close(struct inode *inode, struct file *filp)
{
	free_irq(ls1b_ir_irq, NULL);
	return 0;
}

static int ls1b_ir_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static const struct file_operations ls1b_ir_ops = {
		.owner = THIS_MODULE,
		.open = ls1b_ir_open,
		.release = ls1b_ir_close,
		.read = ls1b_ir_read,
		.write = ls1b_ir_write,
//		.ioctl = ls1b_ir_ioctl,
};

static struct miscdevice ls1b_ir_miscdev = {
		MISC_DYNAMIC_MINOR,
		"ls1b_ir",
		&ls1b_ir_ops,
};

static int __devinit ls1b_ir_probe(struct platform_device *pdev)
{
	ls1b_ir_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	ls1b_ir_irq = ls1b_ir_res->start;
	return 0;
}

static struct platform_driver ls1b_ir_driver = {
		.probe = ls1b_ir_probe,
		.driver = {
			.name = "ls1b_ir",
		},
};

static int __init ls1b_ir_init(void)
{
	if (misc_register(&ls1b_ir_miscdev)) {
		printk(KERN_WARNING "IR: Couldn't register device!\n ");
		return -EBUSY;
	}

	return platform_driver_register(&ls1b_ir_driver);
}

static void __exit ls1b_ir_exit(void)
{
	free_irq(ls1b_ir_irq,NULL);
	misc_deregister(&ls1b_ir_miscdev);
	platform_driver_unregister(&ls1b_ir_driver);	
}

MODULE_AUTHOR("zhuangweilin-gz@loongson.cn");
MODULE_DESCRIPTION("Infrared remote receiver driver for the ls1b.");
module_init(ls1b_ir_init);
module_exit(ls1b_ir_exit);
MODULE_LICENSE("GPL");
