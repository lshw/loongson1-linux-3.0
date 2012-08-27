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

/*
 * Configuration address and data registers
 */
#define CNF_ADDR        0x1e0
#define CNF_DATA        0x1e4

/* LS1B FPGA BOARD Memory control regs */
#define LS1B_BOARD_SD_BASE		0x1f000000
#define REG_SD_TIMING			0x0
#define REG_SD_MOD_SIZE			0x4

/* AHB BUS control regs */
#define LS1B_BOARD_AHB_MISC_BASE	 0x1f003200
#define AHB_MISC_CTRL		0x00
#ifdef	CONFIG_LS1A_MACH
#define AHB_CLK			25000000
#else
//#define AHB_CLK			25000000
#define AHB_CLK			33000000
#endif

/* Interrupt register */
#define REG_INT_EDGE		0x04
#define REG_INT_STEER		0x08
#define REG_INT_POL		0x0c
#define REG_INT_SET		0x10
#define REG_INT_CLR		0x14
#define REG_INT_EN		0x18
#define REG_INT_ISR		0x1c
#define LS1B_BOARD_INTC_BASE	LS1B_BOARD_AHB_MISC_BASE + REG_INT_EDGE

/* GPIO register */
#define REG_GPIO_OE_AHB		0x20
#define REG_GPIO_R_AHB		0x24
#define REG_GPIO_W_AHB		0x28

#define REG_GPIO_CFG0		0x1fd010c0		//GPIO \u914d\u7f6e\u5bc4\u5b58\u5668 0
#define REG_GPIO_CFG1		0x1fd010c4		//GPIO \u914d\u7f6e\u5bc4\u5b58\u5668 1
#define REG_GPIO_OE0		0x1fd010d0		//GPIO \u914d\u7f6e\u5bc4\u5b58\u5668\u8f93\u51fa\u4f7f\u80fd 0
#define REG_GPIO_OE1		0x1fd010d4		//GPIO \u914d\u7f6e\u5bc4\u5b58\u5668\u8f93\u51fa\u4f7f\u80fd 1
#define REG_GPIO_IN0		0x1fd010e0		//GPIO \u914d\u7f6e\u5bc4\u5b58\u5668\u8f93\u5165\u5bc4\u5b58\u5668 0
#define REG_GPIO_IN1		0x1fd010e4		//GPIO \u914d\u7f6e\u5bc4\u5b58\u5668\u8f93\u5165\u5bc4\u5b58\u5668 1
#define REG_GPIO_OUT0		0x1fd010f0		//GPIO \u914d\u7f6e\u5bc4\u5b58\u5668\u8f93\u51fa\u5bc4\u5b58\u5668 0
#define REG_GPIO_OUT1		0x1fd010f4		//GPIO \u914d\u7f6e\u5bc4\u5b58\u5668\u8f93\u51fa\u5bc4\u5b58\u5668 1


/* SPI regs */
#define LS1B_BOARD_SPI_BASE		 0x1f000000 
#define REG_SPCR			0x00
#define REG_SPSR			0x01
#define REG_SPDR			0x02
#define REG_SPER			0x03
#define	REG_SPPR			0x04
#define	REG_SPCSR			0x05
#define	REG_SPTR			0x06

#define LS1B_BOARD_SPI0_BASE		0x1fe80000
#define LS1B_BOARD_SPI1_BASE		0x1fec0000
#define REG_SPCR				0x00	//控制寄存器
#define REG_SPSR				0x01	//状态寄存器
#define REG_SPDR				0x02	//数据寄存器（TxFIFO）
#define REG_TXFIFO    		0x02	//数据传输寄存器 输出
#define REG_RXFIFO    		0x02	//数据传输寄存器 输入
#define REG_SPER				0x03	//外部寄存器
#define REG_PARAM     		0x04	//SPI Flash参数控制寄存器
#define REG_SOFTCS    		0x05	//SPI Flash片选控制寄存器
#define REG_PARAM2    		0x06	//SPI Flash时序控制寄存器

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
#define LS1B_BOARD_UART0_BASE		0x1fe40000
#define LS1B_BOARD_UART1_BASE		0x1fe44000
#define LS1B_BOARD_UART2_BASE		0x1fe48000
#define LS1B_BOARD_UART3_BASE		0x1fe4c000
#define LS1B_BOARD_UART4_BASE		0x1fe6c000
#define LS1B_BOARD_UART5_BASE		0x1fe7c000

#define LS1B_BOARD_UART6_BASE		0x1fe41000
#define LS1B_BOARD_UART7_BASE		0x1fe42000
#define LS1B_BOARD_UART8_BASE		0x1fe43000
#define LS1B_BOARD_UART9_BASE		0x1fe45000
#define LS1B_BOARD_UART10_BASE		0x1fe46000
#define LS1B_BOARD_UART11_BASE		0x1fe47000

#define LS1B_UART_SPLIT				0xbfe78038

/* watchdog */
#define LS1B_BOARD_WAT_BASE			0x1fe5c060
#define WDT_EN						0x00
#define WDT_TIMER					0x04
#define WDT_SET						0x08

/* RTC */
#define LS1X_RTC_BASE				0x1fe64000

/* I2C */
#define LS1B_BOARD_I2C_BASE			0x1fe58000

/* APB BUS control regs */
#define LS1X_CLK_BASE				0x1fe78030
#define LS1B_BOARD_APB_MISC_BASE	0x1f004100
#define REG_GPIO_OE_APB 	0x00
#define REG_GPIO_R_APB		0x10
#define REG_GPIO_W_APB		0x20
#define REG_APB_MISC_CTL	0x40
#define APB_CLK				AHB_CLK
#define LS1B_GPIO_MUX_CTRL1 0xbfd00424

/* AC97 */
#define LS1B_AC97_REGS_BASE 0x1fe74000

/* PCI */
#define LS1B_BOARD_PCI_REGS_BASE		 0x1f002000

#include <regs-clk.h>

struct ls1b_usbh_data {
    u8      ports;      /* number of ports on root hub */
    u8      vbus_pin[]; /* port power-control pin */
}; 

#define LS1B_USB_OHCI_BASE 0x1fe08000
#define LS1B_USB_EHCI_BASE 0x1fe00000
#define LS1B_LCD_BASE 0x1c301240

#define LS1A_PCICFG_BASE 0x1c100000
#define LS1A_PCIMAP (*(volatile int *)0xbfd01114)
#define LS1A_PCIMAP_CFG  (*(volatile int *)0xbfd01120)

#endif	/* __LS1B_BOARD__H__ */

