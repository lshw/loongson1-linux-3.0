/*
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#ifndef __LS1B_BOARD__H__
#define __LS1B_BOARD__H__

#include <asm/addrspace.h>
#include <asm/types.h>

/* AHB BUS control regs */
#define LS1B_BOARD_AHB_MISC_BASE	 0x1f003200
#define AHB_MISC_CTRL		0x00

/* 定义晶振频率和Early printk UART地址 */
#ifdef CONFIG_LS1A_CORE_BOARD
	#define AHB_CLK			25000000
	#define PORT(offset)	(u8 *)(KSEG1ADDR(LS1X_UART2_BASE + offset))
#elif defined(CONFIG_LS1A_CLOUD_TERMIAL)
	#define AHB_CLK			25000000
	#define PORT(offset)	(u8 *)(KSEG1ADDR(LS1X_UART2_BASE + offset))
#elif defined(CONFIG_LS1B_BOARD)
	#define AHB_CLK			33000000
	#define PORT(offset)	(u8 *)(KSEG1ADDR(LS1X_UART2_BASE + offset))
#elif defined(CONFIG_LS1B_CORE_BOARD)
	#define AHB_CLK			25000000
	#define PORT(offset)	(u8 *)(KSEG1ADDR(LS1X_UART5_BASE + offset))
#endif	//#ifdef	CONFIG_LS1A_CORE_BOARD

#define APB_CLK			AHB_CLK

#define LS1X_MUX_BASE			0x1fd00420

/* Interrupt register */
#define REG_INT_EDGE	0x04
#define REG_INT_STEER	0x08
#define REG_INT_POL		0x0c
#define REG_INT_SET		0x10
#define REG_INT_CLR		0x14
#define REG_INT_EN		0x18
#define REG_INT_ISR		0x1c
#define LS1B_BOARD_INTC_BASE	LS1B_BOARD_AHB_MISC_BASE + REG_INT_EDGE

#define LS1X_GPIO_BASE	0x1fd010c0

/* SPI regs */
#define LS1B_BOARD_SPI_BASE	0x1f000000 
#define REG_SPCR			0x00
#define REG_SPSR			0x01
#define REG_SPDR			0x02
#define REG_SPER			0x03
#define	REG_SPPR			0x04
#define	REG_SPCSR			0x05
#define	REG_SPTR			0x06

#define LS1B_BOARD_SPI0_BASE	0x1fe80000
#define LS1B_BOARD_SPI1_BASE	0x1fec0000
#define REG_SPCR				0x00	//控制寄存器
#define REG_SPSR				0x01	//状态寄存器
#define REG_SPDR				0x02	//数据寄存器（TxFIFO）
#define REG_TXFIFO    			0x02	//数据传输寄存器 输出
#define REG_RXFIFO    			0x02	//数据传输寄存器 输入
#define REG_SPER				0x03	//外部寄存器
#define REG_PARAM     			0x04	//SPI Flash参数控制寄存器
#define REG_SOFTCS    			0x05	//SPI Flash片选控制寄存器
#define REG_PARAM2    			0x06	//SPI Flash时序控制寄存器

#define SPI0_CS0				0
#define SPI0_CS1				1
#define SPI0_CS2				2
#define SPI0_CS3				3

#define SPI1_CS0				0
#define SPI1_CS1				1
#define SPI1_CS2				2

/* pwm regs */
#define LS1B_PWM0_BASE	0x1fe5c000
#define LS1B_PWM1_BASE	0x1fe5c010
#define LS1B_PWM2_BASE	0x1fe5c020
#define LS1B_PWM3_BASE	0x1fe5c030

/* gamc regs */
#define LS1B_BOARD_GMAC1_BASE		0x1fe10000
#define LS1B_BOARD_GMAC2_BASE		0x1fe20000

/* UART regs */
#define LS1X_UART0_BASE		0x1fe40000
#define LS1X_UART1_BASE		0x1fe44000
#define LS1X_UART2_BASE		0x1fe48000
#define LS1X_UART3_BASE		0x1fe4c000
#ifdef	CONFIG_LS1B_MACH	/* Loongson 1B最多可扩展12个3线UART串口 1A只能有4个 */
#define LS1X_UART4_BASE		0x1fe6c000
#define LS1X_UART5_BASE		0x1fe7c000
#define LS1X_UART6_BASE		0x1fe41000
#define LS1X_UART7_BASE		0x1fe42000
#define LS1X_UART8_BASE		0x1fe43000
#define LS1X_UART9_BASE		0x1fe45000
#define LS1X_UART10_BASE		0x1fe46000
#define LS1X_UART11_BASE		0x1fe47000
#endif

#define LS1B_UART_SPLIT				0xbfe78038

/* watchdog */
#ifdef	CONFIG_LS1A_MACH
#define LS1X_WAT_BASE				0x1fe7c060
#elif	CONFIG_LS1B_MACH
#define LS1X_WAT_BASE				0x1fe5c060
#endif
#define WDT_EN						0x00
#define WDT_TIMER					0x04
#define WDT_SET						0x08

/* RTC */
#define LS1X_RTC_BASE				0x1fe64000

/* I2C */
#define LS1B_BOARD_I2C_BASE			0x1fe58000

/*	HPET timer	*/
#define	LS1A_HPET_BASE			0x1fe6c000

/* APB BUS control regs */
#define LS1X_CLK_BASE				0x1fe78030
#define LS1B_BOARD_APB_MISC_BASE	0x1f004100
#define REG_APB_MISC_CTL	0x40

/* CAN */
#define LS1X_CAN0_BASE	0x1fe50000
#define LS1X_CAN1_BASE	0x1fe54000

/* AC97 */
#define LS1B_AC97_REGS_BASE 0x1fe74000

/* PCI */
#define LS1B_BOARD_PCI_REGS_BASE		 0x1f002000

#include <regs-clk.h>
#include <regs-mux.h>
#include <regs-gpio.h>

struct ls1b_usbh_data {
    u8      ports;      /* number of ports on root hub */
    u8      vbus_pin[]; /* port power-control pin */
}; 

#define LS1B_USB_OHCI_BASE 	0x1fe08000
#define LS1B_USB_EHCI_BASE 	0x1fe00000

/* LCD */
#define LS1X_DC0_BASE	0x1c301240
#ifdef CONFIG_LS1A_MACH
#define LS1X_DC1_BASE	0x1c301250
#define LS1X_GPU_PLL_CTRL	0x1fd00414
#define LS1X_PIX1_PLL_CTRL	0x1fd00410
#define LS1X_PIX2_PLL_CTRL	0x1fd00424
#endif

#define LS1A_PCICFG_BASE 	0x1c100000
#define	LS1A_PCI_HEADER_CFG	0x1c180000
#define LS1A_PCIMAP (*(volatile int *)0xbfd01114)
#define LS1A_PCIMAP_CFG  (*(volatile int *)0xbfd01120)

#endif	/* __LS1B_BOARD__H__ */

