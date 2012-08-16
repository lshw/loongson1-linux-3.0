
/*
 * Copyright (C) 2012 Loongson Guangzhou inc.
 * All rights reserved.
 *
 * driver/char/stepping_motor.c
 * Original Author: nizhiquan(nizhiquan-gz@loongson.cn), 2012-05-29
 *
 * Description:
 *
 * Stepping motor driver for 24BYJ48A 
 *
 * History:
 *	V1.0 nizhiquan(nizhiquan-gz@loongson.cn), 2012-05-29
 *
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h> 
#include <linux/delay.h>
#include <asm/addrspace.h>

#include "stepmotor.h"

#define DEVNAME "stepmotor"
#define STEPMOTOR_MINOR	1	

#define STEPMOTOR_DEBUG	1

#ifdef STEPMOTOR_DEBUG		
#define dp(x...)	printk(x)
#else
#define dp(x...)
#endif

#define GPIO_EN0	0x1fd010c0
#define GPIO_DIR0	0x1fd010d0
#define GPIO_IN0	0x1fd010e0
#define GPIO_OUT0	0x1fd010f0

#define KSEG1_ADDR(x) ((x)|0xa0000000)

#define REGWRITE(x, v)  (*(volatile u32 *)KSEG1_ADDR(x) = (u32)(v))                                                                                                  
#define REGREAD(x)      (*(volatile u32 *)KSEG1_ADDR(x))


#define here dp("\n\t%s - %s - %dline.\n", __FILE__, __func__, __LINE__);
//#define motor(phase)	REGWRITE(GPIO_OUT0, ((REGREAD(GPIO_OUT0) & (~0x00000f00))) | (phase))

static void motor0_do_phase(u32 phase);
static void motor1_do_phase(u32 phase);

/* stepping motor control gpio define */
/*
#define A	1
#define B	2
#define C	4
#define D	8
*/
/*
#define A	8
#define B	4
#define C	2
#define D	1

#define POS_OFF	8
*/

/*
#define A	(1 << 11)	
#define B	(1 << 10)
#define C	(1 << 9)
#define D	(1 << 8)

#define CCW	(1 << 3)
#define CW	(1 << 2)

#define A1	(1 << 13)	
#define B1	(1 << 15)
#define C1	(1 << 19)
#define D1	(1 << 21)

#define CCW1	(1 << 6)
#define CW1		(1 << 5)
*/

#define A	(1 << 11)	
#define B	(1 << 10)
#define C	(1 << 9)
#define D	(1 << 8)
#define CCW	(1 << 18)
#define CW	(1 << 19)

#define A1	(1 << 12)	
#define B1	(1 << 13)
#define C1	(1 << 14)
#define D1	(1 << 15)
#define CCW1	(1 << 16)
#define CW1		(1 << 17)

#define MOTOR_INPUT_IOS		((CW|CCW)|(CW1|CCW1))
#define MOTOR_OUTPUT_IOS	((A|B|C|D) | (A1|B1|C1|D1))
#define MOTOR_IOS			(MOTOR_INPUT_IOS | MOTOR_OUTPUT_IOS)
#define MOTOR_TICK			(8 * 1000)
#define MOTOR_TIME			16	

#define PHASE_NULL		0

static u32	ccw[8] = {A,A|B,B,B|C,C,C|D,D,D|A};
static u32	cw[8] = {D|A,D,D|C,C,C|B,B,B|A,A};
static u32	ccw1[8] = {A1,A1|B1,B1,B1|C1,C1,C1|D1,D1,D1|A1};
static u32	cw1[8] = {D1|A1,D1,D1|C1,C1,C1|B1,B1,B1|A1,A1};

typedef struct motor_info_t{
	u32	*ph_cw;
	u32	*ph_ccw;
	u32	pos_cw;
	u32	pos_ccw;
	void (*do_phase)(u32 motor_phase);
}motor_info_t;

motor_info_t motors_info[] = {
	{.ph_cw = cw, 
	.ph_ccw = ccw, 
	.pos_cw = CW, 
	.pos_ccw = CCW, 
	.do_phase = motor0_do_phase},
	{.ph_cw = cw1, 
	.ph_ccw = ccw1, 
	.pos_cw = CW1, 
	.pos_ccw = CCW1, 
	.do_phase = motor1_do_phase}
};

static int major;


static void motor0_do_phase(u32 phase)
{
	REGWRITE(GPIO_OUT0, ((REGREAD(GPIO_OUT0) & (~(A|B|C|D)))) | (phase));
}

static void motor1_do_phase(u32 phase)
{
	REGWRITE(GPIO_OUT0, ((REGREAD(GPIO_OUT0) & (~(A1|B1|C1|D1)))) | (phase));
}

void motor_init(void)
{
	u32	gpio_en, gpio_dir, gpio_out;

	gpio_en = REGREAD(GPIO_EN0) & ~(MOTOR_IOS);
	/* disable all motor gpio */
	REGWRITE(GPIO_EN0, gpio_en);
	//REGWRITE(REGREAD(GPIO_EN0) & ~(MOTOR_IOS))

	gpio_dir = REGREAD(GPIO_DIR0);
	gpio_dir |= MOTOR_INPUT_IOS;
	gpio_dir &= ~MOTOR_OUTPUT_IOS;
	REGWRITE(GPIO_DIR0, gpio_dir);

	gpio_out = REGREAD(GPIO_OUT0);
	gpio_out &= ~MOTOR_OUTPUT_IOS;
	REGWRITE(GPIO_OUT0, gpio_out);

	/* now enable all motor gpio */
	gpio_en |= MOTOR_IOS;
	REGWRITE(GPIO_EN0, gpio_en);
	
	return;
}



/**
 * return valule:
 * 0: not at the end, 
 * 1: at cw the end
 * 2: at ccw the end
 * */
static enum motor_pos_t go_end(motor_ctrl_t *ctrl, int no) {
	
	enum motor_pos_t pos;

	pos = pos_not_end;	
	if (ctrl->dir[no] == dir_cw) {
		/* if( (REGREAD(GPIO_IN0) & CW) == 0 ) { */
		if( (REGREAD(GPIO_IN0) & motors_info[no].pos_cw) == 0 ) {
			pos = pos_cw_end;	
			//dp("\n\tctrl->pos[%d] at %d.\n", no, pos);
		} 
	} else {
		/*if( (REGREAD(GPIO_IN0) & CCW) == 0 ) {*/
		if( (REGREAD(GPIO_IN0) & motors_info[no].pos_ccw) == 0 ) {
			pos = pos_ccw_end;	
			//dp("\n\tctrl->pos[%d] at %d.\n", no, pos);
		} 
	}

	return pos;
}

int stepmotor_do(motor_ctrl_t *ctrl)
{
	unsigned char i,j;
	int no;
	u32	*phase[MAX_MOTOR_NO];
	enum motor_pos_t pos[MAX_MOTOR_NO];

	if (ctrl->flags & MOTOR_NO0) {
		here
		if (ctrl->dir[0] == dir_cw) {
			phase[0] = motors_info[0].ph_cw;
		here
		} else {
		here
			phase[0] = motors_info[0].ph_ccw;
		}
	}
	if (ctrl->flags & MOTOR_NO1) {
		if (ctrl->dir[1] == dir_cw) {
		here
			phase[1] = motors_info[1].ph_cw;
		} else {
		here
			phase[1] = motors_info[1].ph_ccw;
		}
	}

	/* 电机齿轮旋转一周，不是外面所看到的一周，是里面的传动轮转了一周*/
	for(j=0;j<ctrl->times;j++){
		/* 每次旋转45度 */
		for(i=0;i<8;i++) {
			if (ctrl->flags & MOTOR_NO0) {
				ctrl->pos[0] = go_end(ctrl, 0);
				if (ctrl->pos[0] == pos_not_end) {
					motors_info[0].do_phase(phase[0][i]);
				}
			}
			if (ctrl->flags & MOTOR_NO1) {
				ctrl->pos[1] = go_end(ctrl, 1);
				if (ctrl->pos[1] == pos_not_end) {
					motors_info[1].do_phase(phase[1][i]);
				}
			}

			switch (ctrl->flags) {
				case MOTOR_NO0:
					if (ctrl->pos[0] != pos_not_end) {
						here
						goto exit_do_phase;
					}
					break;
				case MOTOR_NO1:
					if (ctrl->pos[1] != pos_not_end) {
						here
						goto exit_do_phase;
					}
					break;
				case MOTOR_MAX_FLAGS:
					if (ctrl->pos[0] != pos_not_end && ctrl->pos[1] != pos_not_end) {
						here
						goto exit_do_phase;
					}
					break;
				default:
					break;
			}
			msleep(ctrl->tick);   /* 调节转速 */
		}
	}

exit_do_phase:
	motors_info[0].do_phase(PHASE_NULL);
	motors_info[1].do_phase(PHASE_NULL);
	return 0;
}

static void dump_ctrl(motor_ctrl_t *ctrl)
{
	dp("\n");
	dp("times = %d\n", ctrl->times);
	dp("tick = %d\n", ctrl->tick);
	dp("flags = %d\n", ctrl->flags);
	dp("dir0 = %d, dir1 = %d\n", ctrl->dir[0], ctrl->dir[1]);
	dp("\n");
}

static int stepmotor_write(struct file *filp, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	motor_ctrl_t ctrl;
	
	if (count < sizeof(motor_ctrl_t)) {
		dp("!Argument count is invalid. Buffer size should be equal sizeof(motor_ctrl_t)\n");
		return -EINVAL;
	}

	copy_from_user(&ctrl, buffer, sizeof(motor_ctrl_t));
	if (ctrl.flags > MOTOR_MAX_FLAGS) {
		dp("!Argument count is invalid. Max motor number is %d\n", MAX_MOTOR_NO-1);
		return -EINVAL;
	}
	
	if (ctrl.tick < MIN_TICK) {
		ctrl.tick = MIN_TICK;
	}
	dump_ctrl(&ctrl);
	stepmotor_do(&ctrl);
	copy_to_user(buffer, &ctrl, sizeof(motor_ctrl_t));
	
	return 0;
}

static const struct file_operations stepmotor_fops = {
	.owner		= THIS_MODULE,
	.write		= stepmotor_write,
};

static struct miscdevice stepmotor_misc_device = {
	STEPMOTOR_MINOR,
	DEVNAME,
	&stepmotor_fops,
};

static int __init stepmotor_init(void)
{
	dp("\n\t%s - %s - %dline.\n", __FILE__, __func__, __LINE__);
	if(misc_register(&stepmotor_misc_device)){
		printk(KERN_WARNING "stepmotor:Couldn't register device %d.\n", STEPMOTOR_MINOR);
		return -EBUSY;
	}
	
	motor_init();

	return 0;
}

static void __exit stepmotor_exit(void)
{
	dp("\n\t%s - %s - %dline.\n", __FILE__, __func__, __LINE__);
	misc_deregister(&stepmotor_misc_device);
}

module_init(stepmotor_init);
module_exit(stepmotor_exit);

module_param(major, int, 0);
MODULE_AUTHOR("nizhiquan, nizhiquan-gz@loongson.cn");
MODULE_DESCRIPTION("stepping motor driver for 24BYJ48A");
MODULE_LICENSE("GPL");

