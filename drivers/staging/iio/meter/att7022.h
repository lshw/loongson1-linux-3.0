#ifndef _ATT7022_H
#define _ATT7022_H

#define ATT7022_DEV_ID	0x00

#define ATT7022_PA	0x01
#define ATT7022_PB	0x02
#define ATT7022_PC	0x03
#define ATT7022_PT	0x04

#define ATT7022_QA	0x05
#define ATT7022_QB	0x06
#define ATT7022_QC	0x07
#define ATT7022_QT	0x08

#define ATT7022_SA	0x09
#define ATT7022_SB	0x0A
#define ATT7022_SC	0x0B
#define ATT7022_ST	0x0C

#define ATT7022_UARMS	0x0D
#define ATT7022_UBRMS	0x0E
#define ATT7022_UCRMS	0x0F
#define ATT7022_IARMS	0x10
#define ATT7022_IBRMS	0x11
#define ATT7022_ICRMS	0x12
#define ATT7022_ITRMS	0x13

#define ATT7022_PFA	0x14
#define ATT7022_PFB	0x15
#define ATT7022_PFC	0x16
#define ATT7022_PFT	0x17

#define ATT7022_PGA	0x18
#define ATT7022_PGB	0x19
#define ATT7022_PGC	0x1A

#define ATT7022_INTFLAG	0x1B
#define ATT7022_FREQ	0x1C
#define ATT7022_EFLAG	0x1D

#define ATT7022_EPA	0x1E
#define ATT7022_EPB	0x1F
#define ATT7022_EPC	0x20
#define ATT7022_EPT	0x21

#define ATT7022_EQA	0x22
#define ATT7022_EQB	0x23
#define ATT7022_EQC	0x24
#define ATT7022_EQT	0x25

#define ATT7022_YUAUB	0x26
#define ATT7022_YUAUC	0x27
#define ATT7022_YUBUC	0x28

#define ATT7022_I0RMS	0x29
#define ATT7022_TPSD0	0x2A
#define ATT7022_UTRMS	0x2B

#define ATT7022_SFLAG	0x2C
#define ATT7022_BCKREG	0x2D
#define ATT7022_CMDCHKSUM	0x2E

#define ATT7022_SAMPLE_IA	0x2F
#define ATT7022_SAMPLE_IB	0x30
#define ATT7022_SAMPLE_IC	0x31
#define ATT7022_SAMPLE_UA	0x32
#define ATT7022_SAMPLE_UB	0x33
#define ATT7022_SAMPLE_UC	0x34

#define ATT7022_ESA	0x35
#define ATT7022_ESB	0x36
#define ATT7022_ESC	0x37
#define ATT7022_EST	0x38

#define ATT7022_FSTCNTA	0x39
#define ATT7022_FSTCNTB	0x3A
#define ATT7022_FSTCNTC	0x3B
#define ATT7022_FSTCNTT	0x3C

#define ATT7022_PFLAG	0x3D
#define ATT7022_CHKSUM	0x3E
#define ATT7022_SAMPLE_I0	0x3F

#define ATT7022_LINEPA	0x40
#define ATT7022_LINEPB	0x41
#define ATT7022_LINEPC	0x42
#define ATT7022_LINEPT	0x43

#define ATT7022_LINEEPA	0x44
#define ATT7022_LINEEPB	0x45
#define ATT7022_LINEEPC	0x46
#define ATT7022_LINEEPT	0x47

#define ATT7022_LINEUARRMS	0x48
#define ATT7022_LINEUBRRMS	0x49
#define ATT7022_LINEUCRRMS	0x4A
#define ATT7022_LINEIARRMS	0x4B
#define ATT7022_LINEIBRRMS	0x4C
#define ATT7022_LINEICRRMS	0x4D
#define ATT7022_LEFLAG	0x4E
#define ATT7022_PTRWAVEBUFF	0x7E
#define ATT7022_WAVEBUFF	0x7F

#define ATT7022_READ_REG   0x00
#define ATT7022_WRITE_REG  0x80

/* 特殊寄命令 */
#define SAMPLE_BUFF		0xC0
#define BUFF_READ_P		0xC1
#define CLEAR_TABLE		0xC3
#define SYNC_FACTOR		0xC4
#define SYNC_START		0xC5
#define CHECK_DATA_WRITE		0xC6
#define CHECK_DATA_READ		0xC9
#define ATT7022_RESET	0xD3

#define ATT7022_MAX_TX    7
#define ATT7022_MAX_RX    7
#define ATT7022_STARTUP_DELAY 30

#define DRIVER_NAME		"att7022"

/**
 * struct att7022_state - device instance specific data
 * @spi:			actual spi_device
 * @indio_dev:		industrial I/O device structure
 * @tx:			transmit buffer
 * @rx:			receive buffer
 * @buf_lock:		mutex to protect tx and rx
 **/
struct att7022_state {
	struct spi_device		*spi;
	struct iio_dev			*indio_dev;
	u8				*tx;
	u8				*rx;
	int				(*read_reg_8) (struct device *, u16, u8 *);
	int				(*read_reg_16) (struct device *, u16, u16 *);
	int				(*read_reg_24) (struct device *, u16, u32 *);
	int				(*read_reg_32) (struct device *, u16, u32 *);
	int				(*write_reg_8) (struct device *, u16, u8);
	int				(*write_reg_16) (struct device *, u16, u16);
	int				(*write_reg_24) (struct device *, u16, u32);
	int				(*write_reg_32) (struct device *, u16, u32);
	int                             irq;
	struct mutex			buf_lock;
};

extern int att7022_probe(struct att7022_state *st, struct device *dev);
extern int att7022_remove(struct att7022_state *st);

#endif
