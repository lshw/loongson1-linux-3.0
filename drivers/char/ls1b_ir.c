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
#include <linux/gpio.h>

#include <ls1b_board.h>
#include <irq.h>

#define LS1B_DEBUG 0

#define GPIO_IR 61

#define SYSTEMCODE_BIT_NUM 16

#define LS1B_IR_STATE_IDLE					0
#define LS1B_IR_STATE_RECEIVESTARTCODE		1
#define LS1B_IR_STATE_RECEIVESYSTEMCODE		2
#define LS1B_IR_STATE_RECEIVEDATACODE		3

static unsigned int ls1b_ir_irq = 0;
static unsigned int ls1b_ir_state = LS1B_IR_STATE_IDLE;
static unsigned int ls1b_ir_interval = 0;
static unsigned int ls1b_ir_systembit_count = 0;
static unsigned int ls1b_ir_databit_count = 0;
static unsigned int ls1b_ir_key_code_tmp = 0;
static unsigned int ls1b_ir_key_code = 0;

static struct timeval ls1b_ir_current_tv = {0, 0};
static struct timeval ls1b_ir_last_tv = {0, 0};

DECLARE_WAIT_QUEUE_HEAD(ls1b_wate_queue);

static irqreturn_t ls1b_ir_irq_handler(int i, void *blah)
{
//	ls1b_ir_key_code = 0;
	udelay(50);
	if (gpio_get_value(GPIO_IR))
		return IRQ_HANDLED;
	
	do_gettimeofday(&ls1b_ir_current_tv);
	if (ls1b_ir_current_tv.tv_sec == ls1b_ir_last_tv.tv_sec) {
		ls1b_ir_interval = ls1b_ir_current_tv.tv_usec - ls1b_ir_last_tv.tv_usec;
	} else {
		ls1b_ir_interval = 1000000 - ls1b_ir_last_tv.tv_usec + ls1b_ir_current_tv.tv_usec;
	}
	ls1b_ir_last_tv = ls1b_ir_current_tv;
	
	if (ls1b_ir_interval > 800 && ls1b_ir_interval < 15000) {
		if (ls1b_ir_interval > 11000) {
				ls1b_ir_state = LS1B_IR_STATE_RECEIVESTARTCODE;
				ls1b_ir_key_code_tmp = 0;
				ls1b_ir_databit_count = 0;
				ls1b_ir_systembit_count =0;				
		}
		else if (ls1b_ir_state == LS1B_IR_STATE_RECEIVESTARTCODE) {
			if (ls1b_ir_systembit_count >= SYSTEMCODE_BIT_NUM - 1) {
				ls1b_ir_state = LS1B_IR_STATE_RECEIVESYSTEMCODE;
				ls1b_ir_systembit_count = 0;
			}
			else if ((ls1b_ir_interval > 800 && ls1b_ir_interval < 1300) || (ls1b_ir_interval > 1900 && ls1b_ir_interval < 2400)) {
				ls1b_ir_systembit_count ++;
			}
			else
				goto receive_errerbit;
		}
		else if (ls1b_ir_state == LS1B_IR_STATE_RECEIVESYSTEMCODE) {
			if (ls1b_ir_databit_count < 8) {
				if (ls1b_ir_interval > 1900 && ls1b_ir_interval < 2400) {
					ls1b_ir_key_code_tmp |= (1 << ls1b_ir_databit_count);
					ls1b_ir_databit_count++;
				}
				else if (ls1b_ir_interval > 800 && ls1b_ir_interval < 1300) {
					ls1b_ir_databit_count++;
				}
				else
					goto receive_errerbit;
			}
			else if ((ls1b_ir_interval > 800 && ls1b_ir_interval < 1300) || (ls1b_ir_interval > 1900 && ls1b_ir_interval < 2400)) {
				ls1b_ir_state = LS1B_IR_STATE_IDLE;
				ls1b_ir_key_code = ls1b_ir_key_code_tmp;
				ls1b_ir_key_code_tmp = 0;
				ls1b_ir_databit_count = 0;
				ls1b_ir_systembit_count =0;	
				wake_up_interruptible(&ls1b_wate_queue);
#ifdef LS1B_DEBUG
				printk("IR:Receive key code:%d.\n",ls1b_ir_key_code);
#endif
			}
			else
				goto receive_errerbit;
		}
		ls1b_ir_interval = 0;
		return IRQ_HANDLED;	
	}

receive_errerbit:
	ls1b_ir_state = LS1B_IR_STATE_IDLE;
	ls1b_ir_key_code_tmp = 0;
	ls1b_ir_databit_count = 0;
	ls1b_ir_systembit_count =0;
	ls1b_ir_interval = 0;
	return IRQ_HANDLED;
}

static ssize_t ls1b_ir_read(struct file *filp, char __user *buf, size_t count, loff_t *offp)
{
	ls1b_ir_key_code = 0;

	if (filp->f_flags & O_NONBLOCK) {
		return -EAGAIN;
	}

	wait_event_interruptible(ls1b_wate_queue, ls1b_ir_key_code);

	if (copy_to_user(buf, &ls1b_ir_key_code, sizeof(unsigned int))) {
		printk("IR:ls1b_ir_read error!\n");
		return -EFAULT;
	}
	return count;
}

static int ls1b_ir_open(struct inode *inode, struct file *filep)
{
	int ret;
	ret = gpio_request(GPIO_IR, "ls1x_ir");
	if (ret < 0)
		return ret;
	gpio_direction_input(GPIO_IR);
	ret = request_irq(gpio_to_irq(GPIO_IR), ls1b_ir_irq_handler, IRQF_TRIGGER_FALLING, "ls1b_ir", NULL);
	if (ret) {
		printk("IR:ir_irq_handler resigered Error:%d\n", ret);
		return ret;
	}
	return 0;
}

static int ls1b_ir_close(struct inode *inode, struct file *filp)
{
	free_irq(gpio_to_irq(GPIO_IR), NULL);
	gpio_free(GPIO_IR);
	return 0;
}

static const struct file_operations ls1b_ir_ops = {
	.owner = THIS_MODULE,
	.open = ls1b_ir_open,
	.release = ls1b_ir_close,
	.read = ls1b_ir_read,
};

static struct miscdevice ls1b_ir_miscdev = {
	MISC_DYNAMIC_MINOR,
	"ls1b_ir",
	&ls1b_ir_ops,
};

static int __devinit ls1b_ir_probe(struct platform_device *pdev)
{
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
//module_init(ls1b_ir_init);
//module_exit(ls1b_ir_exit);

MODULE_AUTHOR("zhuangweilin-gz@loongson.cn");
MODULE_DESCRIPTION("Infrared remote receiver driver for the ls1b.");
MODULE_LICENSE("GPL");

__initcall(ls1b_ir_init);
