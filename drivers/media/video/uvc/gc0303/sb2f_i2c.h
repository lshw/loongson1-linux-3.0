
#ifndef __SB2F_I2C_H__
#define __SB2F_I2C_H__

#include <asm/types.h>

/*register*/
#define REG_I2C_PRER_LO	0x0
#define REG_I2C_PRER_HI	0x1
#define REG_I2C_CTR		0x2
#define REG_I2C_TXR		0x3
#define REG_I2C_RXR		0x3
#define REG_I2C_CR		0X4
#define	REG_I2C_SR		0X4

/*control*/
#define I2C_C_START		0x80
#define I2C_C_STOP		0x40
#define	I2C_C_READ		0x20
#define I2C_C_WRITE		0x10
#define I2C_C_WACK		0x8
#define I2C_C_IACK		0x1

/*status*/
#define	I2C_S_RNOACK	0x80
#define I2C_S_BUSY		0x40
#define I2C_S_RUN		0x2
#define	I2C_S_IF		0x1

int i2c_read(int dev_addr, unsigned char *data, unsigned char addr, int size);

int i2c_init(void);

int i2c_write(int dev_addr, unsigned char *data, unsigned char addr, int size);

void i2c_test(void);

void print_gc0303_reg(void);


#define I2C0_BASE		0xbfe58000

struct gs2fsb_i2c {
	void *base;
	unsigned int interrupt;
	int irq;
	unsigned int flags;
};

/*
 * I2C Message - used for pure i2c transaction, also from /dev interface
 */
struct i2c_msg {
	unsigned short addr;	/* slave address			*/
	unsigned short flags;	/* 1: write, 0: read */
#define I2C_M_TEN               0x0010  /* this is a ten bit chip address */
#define I2C_M_RD                0x0001  /* read data, from slave to master */
#define I2C_M_NOSTART           0x4000  /* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_REV_DIR_ADDR      0x2000  /* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_IGNORE_NAK        0x1000  /* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NO_RD_ACK         0x0800  /* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_RECV_LEN          0x0400  /* length will be first received byte */
	unsigned short len;		/* msg length				*/
	unsigned char *buf;		/* pointer to msg data			*/
	int sub_addr;			/* if sub address = -1, mean no sub address */
};

extern struct gs2fsb_i2c	g_i2c0;
extern struct i2c_msg 	g_msg;

#endif
