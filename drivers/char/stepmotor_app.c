
/*
 * Copyright (C) 2012 Loongson Guangzhou inc.
 * All rights reserved.
 *
 * motor.c
 * Original Author: nizhiquan(nizhiquan-gz@loongson.cn), 2012-05-29
 *
 * Description:
 *
 * Stepping motor application for 24BYJ48A 
 *
 * History:
 *	V1.0 nizhiquan(nizhiquan-gz@loongson.cn), 2012-05-29
 *
 */
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "stepmotor.h"

#ifndef u32
typedef unsigned int u32;
#endif

#define DEV_PATH	"/dev/stepmotor"

#define dp //printf

static void dump_ctrl(motor_ctrl_t *ctrl)
{
	dp("\n");
	dp("times = %d\n", ctrl->times);
	dp("tick = %d\n", ctrl->tick);
	dp("flags = %d\n", ctrl->flags);
	dp("dir0 = %d, dir1 = %d\n", ctrl->dir[0], ctrl->dir[1]);
	dp("\n");
}

int main(int argc, char **argv)
{
	int fd;
	int ret;
	motor_ctrl_t motor_ctrl;

	if (argc < 5) {
		printf("Please use motor times speed flags dir0/dir2.\n");
		//printf("Please use motor no dir times tick\n");
		return -1;
	}
	
	motor_ctrl.times = strtol(argv[1], NULL, 10);
	motor_ctrl.tick = strtol(argv[2], NULL, 10);
	motor_ctrl.flags = strtol(argv[3], NULL, 10);
	
	if (motor_ctrl.flags == MOTOR_MAX_FLAGS) {
		motor_ctrl.dir[0] = strtol(argv[4], NULL, 10);
		if (argc == 6) {
			motor_ctrl.dir[1] = strtol(argv[5], NULL, 10);
		}
	} else if (motor_ctrl.flags == MOTOR_NO0) {
		motor_ctrl.dir[0] = strtol(argv[4], NULL, 10);
	} else if (motor_ctrl.flags == MOTOR_NO1) {
		motor_ctrl.dir[1] = strtol(argv[4], NULL, 10);
	} else {
		printf("flags is invalid.\n");
		return -1;
	}

	dump_ctrl(&motor_ctrl);

	fd = open(DEV_PATH,O_RDWR);
	if (fd < 0) {
		printf("!Un...\n");
		return -1;
	}

	ret = write(fd, &motor_ctrl, sizeof(motor_ctrl));

	printf("Motor0 pos at %d.", motor_ctrl.pos[0]);	
	printf("Motor1 pos at %d.", motor_ctrl.pos[1]);	
exit:
	close(fd);
	
	return ret;
}


