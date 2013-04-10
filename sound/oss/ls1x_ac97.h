/*
 *  Copyright (c) 2013 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef __LS1X_AC97_H__
#define __LS1X_AC97_H__

/* Control Status Register (CSR) */
#define CSR		0x00
#define AC97_CSR_RESUME		(1 << 1)
#define AC97_CSR_RST_FORCE	(1 << 0)
/* Output Channel Configuration Registers (OCCn) */
#define OCC0	0x04
#define OCC1	0x08
#define OCC2	0x0c
#define OCH1_CFG_R_OFFSET	(8)
#define OCH0_CFG_L_OFFSET	(0)
#define OCH1_CFG_R_MASK		(0xFF)
#define OCH0_CFG_L_MASK		(0xFF)

/* Input Channel Configuration (ICC) */
#define ICC		0x10
#define ICH2_CFG_MIC_OFFSET	(16)
#define ICH1_CFG_R_OFFSET	(8)
#define ICH0_CFG_L_OFFSET	(0)
#define ICH2_CFG_MIC_MASK	(0xFF)
#define ICH1_CFG_R_MASK		(0xFF)
#define ICH0_CFG_L_MASK		(0xFF)

/* Channel Configurations (Sub-field) */
#define DMA_EN	(1 << 6)
#define FIFO_THRES_OFFSET	(4)
#define FIFO_THRES_MASK		(0x3)
#define SS_OFFSET		(2)	/* Sample Size 00:8bit 10:16bit */
#define SS_MASK		(0x3)
#define SR		(1 << 1)	/* Sample Rate 1:Variable 0:Fixed */
#define CH_EN	(1 << 0)


#define CODECID	0x14

/* Codec Register Access Command (CRAC) */
#define CRAC	0x18
#define CODEC_WR			(1 << 31)
#define CODEC_ADR_OFFSET	(16)
#define CODEC_DAT_OFFSET	(0)
#define CODEC_ADR_MASK		(0xFF)
#define CODEC_DAT_MASK		(0xFF)

/* OCHn and ICHn Registers
   OCHn are the output fifos (data that will be send to the codec), ICHn are the 
   input fifos (data received from the codec).
 */
#define OCH0	0x20
#define OCH1	0x24
#define OCH2	0x28
#define OCH3	0x2c
#define OCH4	0x30
#define OCH5	0x34
#define OCH6	0x38
#define OCH7	0x3c
#define OCH8	0x40
#define ICH0	0x44
#define ICH1	0x48
#define ICH2	0x4c

/* Interrupt Status Register (INTS) */
#define INTRAW	0x54
#define ICH_FULL		(1 << 31)
#define ICH_TH_INT		(1 << 30)
#define OCH1_FULL		(1 << 7)
#define OCH1_EMPTY		(1 << 6)
#define OCH1_TH_INT		(1 << 5)
#define OCH0_FULL		(1 << 4)
#define OCH0_EMPTY		(1 << 3)
#define OCH0_TH_INT		(1 << 2)
#define CW_DONE		(1 << 1)
#define CR_DONE		(1 << 0)

#define INTM	0x58

/* 中断状态/清除寄存器 
   屏蔽后的中断状态寄存器，对本寄存器的读操作将清除寄存器0x54中的所有中断状态 */
#define INT_CLR	0x5c
/* OCH中断清除寄存器
   对本寄存器的读操作将清除寄存器0x54中的所有output channel的中断状态对应的 bit[7:2] */
#define INT_OCCLR	0x60
/* ICH中断清除寄存器 
   对本寄存器的读操作将清除寄存器0x54中的所有input channel的中断状态对应的 bit[31:30] */
#define INT_ICCLR	0x64
/* CODEC WRITE 中断清除寄存器 
   对本寄存器的读操作将清除寄存器0x54中的中bit[1] */
#define INT_CWCLR	0x68
/* CODEC READ 中断清除寄存器
   对本寄存器的读操作将清除寄存器0x54中的中bit[0] */
#define INT_CRCLR	0x6c

#define ORDER_ADDR_IN	0x1fd01160

/* dma regs */
#define DMA_ORDERED		0x0
#define DMA_SADDR		0x1
#define DMA_DADDR		0x2
#define DMA_LENGTH		0x3
#define DMA_STEP_LENGTH		0x4
#define DMA_STEP_TIMES		0x5
#define	DMA_CMD			0x6

#define DMA_ORDERED_EN	(1 << 0)

#define DMA_AC97_WRITE_EN	(0x1 << 31)
#define DMA_AC97_STERO	(0x1 << 30)
#define DMA_AC97_WRITE_MODE	(0x3 << 28)
#define DMA_AC97_WRITE_MODE_SHIFT 28
#define DMA_AC97_WRITE_1B	0
#define DMA_AC97_WRITE_2B	1
#define DMA_AC97_WRITE_4B	2

#define AC97_EXT_DACS (AC97_EXTID_SDAC | AC97_EXTID_CDAC | AC97_EXTID_LDAC)

#define DMA_DESC		0x5
#define REC_NBFRAGS		DMA_DESC
#define PLAY_NBFRAGS	DMA_DESC
#define AUDIO_FRAGSIZE_DEFAULT	0x2000

#endif /* __LS1X_AC97_H__ */
