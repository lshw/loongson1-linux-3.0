
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

int main(int argc, char **argv)
{
	int fd;
	int ret;
	motor_ctrl_t motor_ctrl;

	if (argc < 5) {
		printf("Please use motor no dir times tick\n");
		return -1;
	}
	
	motor_ctrl.no = strtol(argv[1], NULL, 10);
	motor_ctrl.dir = strtol(argv[2], NULL, 10);
	motor_ctrl.times = strtol(argv[3], NULL, 10);
	motor_ctrl.tick = strtol(argv[4], NULL, 10);

	fd = open(DEV_PATH,O_RDWR);
	if (fd < 0) {
		printf("!Un...\n");
		return -1;
	}

	ret = write(fd, &motor_ctrl, sizeof(motor_ctrl));

	printf("Motor pos at %d.", motor_ctrl.pos);	
exit:
	close(fd);
	
	return ret;

}

