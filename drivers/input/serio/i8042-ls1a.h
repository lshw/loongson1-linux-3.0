#ifndef _I8042_SB2FIO_H
#define _I8042_SB2FIO_H

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

/*
 * Names.
 */

#define I8042_KBD_PHYS_DESC "isa0060/serio0"
#define I8042_AUX_PHYS_DESC "isa0060/serio1"
#define I8042_MUX_PHYS_DESC "isa0060/serio%d"

/*
 * IRQs.
 */

#include <asm/irq.h>
# define I8042_KBD_IRQ	12
# define I8042_AUX_IRQ	11


/*
 * Register numbers.
 */

#define I8042_COMMAND_REG	0xbfe60004
#define I8042_STATUS_REG	0xbfe60004
#define I8042_DATA_REG		0xbfe60000
static inline int i8042_read_data(void)
{
	return readb(I8042_DATA_REG);

}

static inline int i8042_read_status(void)
{
	int status;
        status=readb(I8042_STATUS_REG);
	return status;
}

static inline void i8042_write_data(int val)
{ 
	writeb(val, I8042_DATA_REG);

}

static inline void i8042_write_command(int val)
{
	writeb(val, I8042_COMMAND_REG);
}

static inline int i8042_platform_init(void)
{
/*
 * On some platforms touching the i8042 data register region can do really
 * bad things. Because of this the region is always reserved on such boxes.
 */
	i8042_reset = 1;
	return 0;
}

static inline void i8042_platform_exit(void)
{
}

#endif /* _I8042_IO_H */
