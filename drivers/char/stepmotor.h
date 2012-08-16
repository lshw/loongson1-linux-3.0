
#ifndef __STEPMOTOR_H__
#define __STEPMOTOR_H__

/*
 * Copyright (C) 2012 Loongson Guangzhou inc.
 * All rights reserved.
 *
 * stepmotor.h
 * Original Author: nizhiquan(nizhiquan-gz@loongson.cn), 2012-05-29
 *
 * Description:
 *
 * Stepping motor header for 24BYJ48A 
 *
 * History:
 *	V1.0 nizhiquan(nizhiquan-gz@loongson.cn), 2012-05-29
 *
 */

/* 驱动电机的数量 */
#define MAX_MOTOR_NO	2
/* 电机最快转速 */
#define MIN_TICK	1

#define MOTOR_NO0	(1 << 0)
#define MOTOR_NO1	(1 << 1)
#define MOTOR_MAX_FLAGS	(MOTOR_NO0 | MOTOR_NO1)

enum motor_dir_t {
	dir_cw = 0,	/* 顺时针 */
	dir_ccw		/* 逆时针 */
};

/* 
 * 电机的位置，判断是否在到底部
 * pos_cw_end  ：顺时针底部
 * pos_ccw_end ：逆时针底部
 * 到达某一方向底部以后驱动不再驱动电机继续转动
 * 应用程序可以利用这个状态判断电机位置
 */
enum motor_pos_t{
	pos_not_end = 0,
	pos_cw_end,
	pos_ccw_end
};


typedef struct motor_ctrl_t {
	enum motor_pos_t pos[MAX_MOTOR_NO];
	enum motor_dir_t dir[MAX_MOTOR_NO];
	int flags;	/* bit0是第一电机开关，bit1是第二个电机开关 */
	int times;	/* 齿轮转动圈数，目前测试一圈约2度 */
	int tick;	/* 控制电机转速,4-最快，8-正常 */
}motor_ctrl_t;

#endif	/* __STEPMOTOR_H__*/

