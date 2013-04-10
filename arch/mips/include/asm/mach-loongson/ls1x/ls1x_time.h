/*
 * Copyright (c) 2012 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _LS1X_TIME_H
#define _LS1X_TIME_H

#ifdef CONFIG_LS1X_TIMER
extern void setup_ls1x_timer(void);
extern void disable_ls1x_counter(void);
extern void enable_ls1x_counter(void);
#else
static inline void __maybe_unused setup_ls1x_timer(void)
{
}
static inline void __maybe_unused disable_ls1x_counter(void)
{
}
static inline void __maybe_unused enable_ls1x_counter(void)
{
}
#endif

#endif /*!_LS1X_TIME_H */
