/*
 * CH372 UDC (USB gadget)
 *
 * Copyright (C) 2010 Faraday Technology Corp.
 *
 * Author : Yuan-hsin Chen <yhchen@faraday-tech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#ifndef __CH372_USB_DEV_H__
#define __CH372_USB_DEV_H__

#include <linux/kernel.h>

#define CH372_GET_IC_VER	0x01
#define CH372_ENTER_SLEEP	0x03
#define CH372_RESET_ALL	0x05
#define CH372_CHECK_EXIST	0x06
#define CH372_GET_TOGGLE	0x0A
#define CH372_CHK_SUSPEND	0x0B
#define CH372_SET_USB_ID	0x12
#define CH372_SET_USB_ADDR	0x13
#define CH372_SET_USB_MODE	0x15
#define CH372_SET_ENDP2	0x18
#define CH372_SET_ENDP3	0x19
#define CH372_SET_ENDP4	0x1A
#define CH372_SET_ENDP5	0x1B
#define CH372_SET_ENDP6	0x1C
#define CH372_SET_ENDP7	0x1D
#define CH372_GET_STATUS	0x22
#define CH372_UNLOCK_USB	0x23
#define CH372_RD_USB_DATA	0x28
#define CH372_WR_USB_DATA5	0x2A
#define CH372_WR_USB_DATA7	0x2B
#define CH372_RD_USB_DATA0	0x27
#define CH372_WR_USB_DATA3	0x29

/* 操作状态 */
#define CMD_RET_SUCCESS	0x51
#define CMD_RET_ABORT	0x5F

/* 中断状态值 */
#define USB_INT_BUS_RESET1	0x03
#define USB_INT_BUS_RESET2	0x07
#define USB_INT_BUS_RESET3	0x0B
#define USB_INT_BUS_RESET4	0x0F
#define USB_INT_EP0_SETUP	0x0C
#define USB_INT_EP0_OUT	0x00
#define USB_INT_EP0_IN	0x08
#define USB_INT_EP1_OUT	0x01
#define USB_INT_EP1_IN	0x09
#define USB_INT_EP2_OUT	0x02
#define USB_INT_EP2_IN	0x0A
#define USB_INT_USB_SUSPEND	0x05
#define USB_INT_WAKE_UP	0x06


/*----------------------------------------------------------------------*/
#define CH372_MAX_NUM_EP		5

struct ch372_ep_info {
	u8	epnum;
	u8	type;
	u8	interval;
	u8	dir_in;
	u16	maxpacket;
	u16	addrofs;
	u16	bw_num;
};

struct ch372 {
	spinlock_t		lock;
	void __iomem		*reg;

	unsigned long		irq_trigger;

	__le16			ep0_data;
	u32			ep0_length;	/* for internal request */
	u8			ep0_dir;	/* 0/0x80  out/in */

	u8			fifo_entry_num;	/* next start fifo entry */
	u32			addrofs;	/* next fifo address offset */
	u8			reenum;		/* if re-enumeration */
};

#endif
