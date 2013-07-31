/*
 * am2301 interface to platform code
 *
 * Copyright (c) 2013 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */
#ifndef _LINUX_AM2301_H
#define _LINUX_AM2301_H

/**
 * struct am2301_platform_data - Platform-dependent data for am2301
 * @pin: GPIO pin to use
 * @is_open_drain: GPIO pin is configured as open drain
 */
struct am2301_platform_data {
	unsigned int pin;
	unsigned int is_open_drain:1;
	void (*enable_external_pullup)(int enable);
};

#endif /* _LINUX_AM2301_H */
