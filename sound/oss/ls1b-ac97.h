
/*
 * FileName:lson_ac97.h 
 *
 * data:
 * descriptor:
 * update:
 *
 */

#ifndef FCR_AC97_H__
#define FCR_AC97_H__
//typedef unsigned int U32;

#define _REG(x) 	((volatile u32 *)(x))
#define _REG2(b,o) 	((volatile u32 *)((b)+(o)))

#define LSON_AC_FMT  		AFM_S16_LE
#define LSON_AC_MAXCHNL 	2

/*
 * 2x4 个DMA DESC
 */

#define DMA_FROMAC_DESCs 4
#define DMA_TOAC_DESCs 4


/*
 * NOTE 3:
 * registers define
 */

//#define Lson_AC97_IOBASE 0x00000000L
#if 0
#else
//AC97_controler
#define AC97_REG(off)       _REG2(KSEG1ADDR(SB2F_AC97_REGS_BASE),(off))
#define CSR			AC97_REG(0x00)
#define OCCR0		AC97_REG(0x04)
#define OCCR1		AC97_REG(0x08)
#define OCCR2		AC97_REG(0x0C)
#define ICCR		AC97_REG(0x10)
#define CDC_ID      AC97_REG(0x14)
#define CRAC		AC97_REG(0x18)
                                       /*Reserved 0x1C Reserved*/
#define OC0_DATA    AC97_REG(0x20)     /*20 bits WO Output Channel0 Tx buffer*/
#define OC1_DATA    AC97_REG(0x24)     /*20 bits WO Output Channel1 Tx buffer*/
#define OC2_DATA    AC97_REG(0x28)     /*20 bits WO Output Channel2 Tx buffer*/
#define OC3_DATA    AC97_REG(0x2C)     /*20 bits WO Output Channel3 Tx buffer*/
#define OC4_DATA    AC97_REG(0x30)     /*20 bits WO Output Channel4 Tx buffer*/
#define OC5_DATA    AC97_REG(0x34)     /*20 bits WO Output Channel5 Tx buffer*/
#define OC6_DATA    AC97_REG(0x38)     /*20 bits WO Output Channel6 Tx buffer*/
#define OC7_DATA    AC97_REG(0x3C)     /*20 bits WO Output Channel7 Tx buffer*/
#define OC8_DATA    AC97_REG(0x40)     /*20 bits WO Output Channel8 Tx buffer*/
#define IC0_DATA    AC97_REG(0x44)     /*20 bits RO Input Channel0 Rx buffer*/
#define IC1_DATA    AC97_REG(0x48)     /*20 bits RO Input Channel1 Rx buffer*/
#define IC2_DATA    AC97_REG(0x4C)     /*20 bits RO Input Channel2 Rx buffer*/
                                       /*Reserved AC97_REG(0x50 Reserved*/
#define INTRAW      AC97_REG(0x54)     /*32 bits RO Interrupt RAW status*/
#define INTM        AC97_REG(0x58)     /*32 bits R/W Interrupt Mask*/
#define INTS        AC97_REG(0x5C)     /*32 bits RO Interrupt Masked Status*/
#define CLR_INT     AC97_REG(0x60)     /*1 bit RO Clear Combined and Individual Interrupt*/
#define CLR_OC_INT  AC97_REG(0x64)     /*1 bit RO Clear Output Channel Reference Interrupt*/
#define CLR_IC_INT  AC97_REG(0x68)      /*1 bit RO Clear Input Channel Reference Interrupt*/
#define CLR_CDC_WR  AC97_REG(0x68)      /*1 bit RO Clear Codec Write Done Interrupt*/
#define CLR_CDC_RD  AC97_REG(0x6c)      /*1 bit RO Clear Codec Read Done Interrupt*/

#define OC_DATA     OC0_DATA            /*DEFAULT */
#define IN_DATA     IC1_DATA            /*SETTING */

#endif

#define 	INPUT_CHANNELS	8
#define 	OUTPUT_CHANNELS	9

/*reg bits define*/ 
#define CRAR_READ  0x80000000
#define CRAR_WRITE 0x00000000
#define CRAR_CODEC_REG(x)  (((x)&(0x7F))<<16)

//default setting 
#define OCCR	    OCCR0
#define OUT_CHANNEL OC[0]
#define IN_CHANNEL  IC[0]
//interrupt state reg;
#define INTS_CRAC_MASK	0x3

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
//
// DMA regs
//
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

//#define FCR_ACDMA_REG_BASE	0x1f004280
#define CONFREG_BASE		0xbfd00000
#define FCR_ACDMA_REG_BASE	0x1fe74080
//#define AC97_REG(off)       _REG2(KSEG1ADDR(FCR_AC97_REGS_BASE),(off))
#define DMA_REG(off)  		_REG2(KSEG1ADDR(FCR_ACDMA_REG_BASE),(off))

/* dma regs */
#define DMA_ORDERED		0x0
#define DMA_SADDR		0x1
#define DMA_DADDR		0x2
#define DMA_LENGTH		0x3
#define DMA_STEP_LENGTH		0x4
#define DMA_STEP_TIMES		0x5
#define	DMA_CMD			0x6

/* bit offset in order_addr_reg */
#define DMA_STOP_OFF		0x4
#define DMA_START_OFF		0x3
#define DMA_ASKVALID_OFF	0x2
#define DMA_DEVNUM_OFF		0x0

/* define dma status */
#define DMA_START		0x0
#define DMA_STOP		0x1

#define DMA_BUF_UPPER		0x3
#define DMA_BUF_LOW		0x1

#define AUD_WRITE_ADDR		0xdfe72420	
#define AUD_READ_ADDR		0x9fe74c4c

#define BLANK_SIZE		0x20
#if 0
/*control and status regs*/
#define DMA_STATUS  		 DMA_REG(0x2C)
#define DMA_INT_STATUS	 	 DMA_REG(0x2C)
#define DMA_INT_CLEAN		 DMA_REG(0x2C)
//#define DMA_CONTROL 		 DMA_REG(0x0)   /*NOT this reg*/
#define DMA_INT_MASK		 DMA_REG(0x28)  /*INT_MASK*/
/*channel control regs*/
#define DMA_ADDR_TODEV	 	 DMA_REG(0x0)   /*hardware define as RD_FIFO*/
#define DMA_SIZE_TODEV   	 DMA_REG(0x4)   /*RD_FIFO_SIZE*/
#define DMA_TODEV_RDPT       DMA_REG(0x8)   /*RD_FIFO_RD_PT*/               
#define DMA_TODEV_WRPT       DMA_REG(0xc)   /*RD_FIFO_WR_PT*/               
#define DMA_ADDR_FROMDEV 	 DMA_REG(0x10)  /*WR_FIFO*/
#define DMA_SIZE_FROMDEV 	 DMA_REG(0x14)  /*WR_FIFO_SIZE*/
#define DMA_FROMDEV_RDPT     DMA_REG(0x18)  /*RD_FIFO_RD_PT*/               
#define DMA_FROMDEV_WRPT     DMA_REG(0x1c)  /*RD_FIFO_WR_PT*/               
#define DMA_SET_TODEV        DMA_REG(0x20)  /*RD_MODE*/
#define DMA_SET_FRMDEV       DMA_REG(0x24)   /*WR_MODE*/

//status
#define DESC_TODEV_COMPELE	(0x1)           /**/
#define DESC_TODEV_EMPTY	(0x2)
#define DESC_TODEV_FULL		(0x4)
#define DESC_FRMDEV_COMPELE	(0x8)
#define DESC_FRMDEV_EMPTY	(0x10)
#define DESC_FRMDEV_FULL	(0x20)
#define DESC_EMPTY          (DESC_TODEV_EMPTY|DESC_FRMDEV_EMPTY)
#define DESC_FULL           (DESC_TODEV_FULL|DESC_FRMDEV_FULL)
//IRQ status
#define IRQ_TODEV_STANS     DESC_TODEV_COMPELE 
#define IRQ_TODEV_EMPTY		DESC_TODEV_EMPTY    
#define IRQ_FRMDEV_STANS	DESC_FRMDEV_COMPELE	
#define IRQ_FRMDEV_EMPTY	DESC_FRMDEV_EMPTY	
#define IRQ_TODEV	        IRQ_TODEV_STANS     		
#define IRQ_FRMDEV	        IRQ_FRMDEV_STANS			

//IRQ clear
#define CLS_IN_STANS       	0x00000001
#define CLS_IN_EMPTY		0x00000002
#define CLS_OUT_STANS	 	0x00000004
#define CLS_OUT_EMPTY		0x00000008
#define CLEAN_ALL		    0x0000003F

/*
enable a dma channel after desc write;
*/
#define DMA_CHNN_IN 	0
#define DMA_CHNN_OUT	1

#define TO_DEV_DESC_FULL    (((*DMA_STATUS)&DESC_TODEV_FULL)==DESC_TODEV_FULL) 
#define TO_DEV_DESC_EMPTY   (((*DMA_STATUS)&DESC_TODEV_EMPTY)==DESC_TODEV_EMPTY)
#define FROM_DEV_DESC_FULL  (((*DMA_STATUS)&DESC_FRMDEV_FULL)==DESC_FRMDEV_FULL)
#define FROM_DEV_DESC_EMPTY (((*DMA_STATUS)&DESC_FRMDEV_EMPTY)==DESC_FRMDEV_EMPTY)

#define DMA_ENABLE(ch)	do{}while(0)

#if 1
#define DMA_DESC_ISFULL(channel)  ({int __ret;\
				if (channel==DMA_CHNN_OUT)\
				  __ret=TO_DEV_DESC_FULL;\
				else \
				  __ret=FROM_DEV_DESC_FULL;\
				__ret;\
				})

#define DMA_DESC_ISEMPTY(channel)  ({int __ret;\
				if (channel==DMA_CHNN_OUT)\
				  __ret=TO_DEV_DESC_EMPTY;\
				else \
				  __ret=FROM_DEV_DESC_EMPTY; \
				__ret; \
				})
#endif



#endif
#endif
