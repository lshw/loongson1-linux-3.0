
#include "sb2f_i2c.h"
//#include "stub.h"

#define i2c_delay()		//udelay(100)

static char i2c_writeb(struct gs2fsb_i2c *i2c, unsigned int reg, unsigned char  data)
{
	
	(*(volatile unsigned char *)(i2c->base + reg)) = data;
}

static unsigned char i2c_readb (struct gs2fsb_i2c *i2c, unsigned char reg)
{
	unsigned char data;
	
	data = (*(volatile unsigned char *)(i2c->base + reg));

	return data;
}


static int gs2fsb_xfer_read(struct gs2fsb_i2c *i2c, unsigned char *buf, int len) 
{

	int x;

	for(x=0; x<len; x++) {

		//i2c_writeb(i2c, REG_I2C_CR, I2C_C_READ);
//send ACK last not send ACK
		if(x == (len -1)) 
			i2c_writeb(i2c, REG_I2C_CR,   I2C_C_READ);	 //the laset
		else
			i2c_writeb(i2c, REG_I2C_CR, I2C_C_WACK |I2C_C_READ);

		while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_RUN);

		buf[x] = i2c_readb(i2c,REG_I2C_TXR);

		i2c_delay();

	}
	
	i2c_writeb(i2c,REG_I2C_CR, I2C_C_STOP);
		
	return len;
}

static int gs2fsb_xfer_write(struct gs2fsb_i2c *i2c, unsigned char *buf, int len)
{

	int j;

	for(j=0; j< len; j++) {
			i2c_writeb(i2c, REG_I2C_TXR, buf[j]);
			i2c_writeb(i2c, REG_I2C_CR, I2C_C_WRITE);
			while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_RUN);
			if(i2c_readb(i2c, REG_I2C_SR) & I2C_S_RNOACK) {
				i2c_writeb(i2c, REG_I2C_CR, I2C_C_STOP);
				return len;
			}
 			i2c_delay();
		}

	i2c_writeb(i2c, REG_I2C_CR, I2C_C_STOP);

	return len;

}

#if 0
int gs2fsb_xfer(struct gs2fsb_i2c *i2c, struct i2c_msg *pmsg)
{
	int i,j, ret, ret0;


	char flags;

	//printk("gs2fsb_xfer().\n");

	//printk("pmsg->flags:0x%x, i2c base reg:0x%x.\n", pmsg->flags, i2c->base);
	
	while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_BUSY);

	/*set slave addr*/
	flags = (pmsg->flags & I2C_M_RD)?1:0;
	i2c_writeb(i2c, REG_I2C_TXR, ((pmsg->addr << 1 ) | flags));
	i2c_writeb(i2c, REG_I2C_CR, (I2C_C_WRITE | I2C_C_START | I2C_C_WACK));
	while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_RUN);
	if (i2c_readb(i2c, REG_I2C_SR) & I2C_S_RNOACK) {
		printk(" slave addr no ack !!\n");
		i2c_writeb(i2c, REG_I2C_CR, I2C_C_STOP); 
		return -1;
	}

	if (pmsg->sub_addr != -1) {
		i2c_writeb(i2c, REG_I2C_TXR, (unsigned char)pmsg->sub_addr);
		while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_RUN);
		if (i2c_readb(i2c, REG_I2C_SR) & I2C_S_RNOACK) {
			printk(" sub addr no ack !!\n");
			i2c_writeb(i2c, REG_I2C_CR, I2C_C_STOP); 
			return -1;
		}
	}

	printk("slave addr: 0x%x have ack. len: %d.\n", pmsg->addr, pmsg->len);
	
	if(flags )
	ret = gs2fsb_xfer_read(i2c, pmsg->buf, pmsg->len);
	else
	ret = gs2fsb_xfer_write(i2c, pmsg->buf, pmsg->len);

	return ret;
}
#endif

#define TSC2003_CMD(cn,pdn,m) (((cn) << 4) | ((pdn) << 2) | ((m) << 1))

enum tsc2003_pd {
  PD_POWERDOWN = 0, /* penirq */
  PD_IREFOFF_ADCON = 1, /* no penirq */
  PD_IREFON_ADCOFF = 2, /* penirq */
  PD_IREFON_ADCON = 3, /* no penirq */
  PD_PENIRQ_ARM = PD_IREFON_ADCOFF,
  PD_PENIRQ_DISARM = PD_IREFON_ADCON,
};

enum tsc2003_m {
  M_12BIT = 0,
  M_8BIT = 1
};

enum tsc2003_cmd {
  MEAS_TEMP0 = 0,
  MEAS_VBAT1 = 1,
  MEAS_IN1 = 2,
  MEAS_TEMP1 = 4,
  MEAS_VBAT2 = 5,
  MEAS_IN2 = 6,
  ACTIVATE_NX_DRIVERS = 8,
  ACTIVATE_NY_DRIVERS = 9,
  ACTIVATE_YNX_DRIVERS = 10,
  MEAS_XPOS = 12,
  MEAS_YPOS = 13,
  MEAS_Z1POS = 14,
  MEAS_Z2POS = 15
};

struct gs2fsb_i2c	g_i2c0;
struct i2c_msg 	g_msg;

int i2c_read(int dev_addr, unsigned char *data, unsigned char addr, int size)
{
	int ret;
	struct gs2fsb_i2c *i2c = &g_i2c0;
	/*
	printk("-->read dev address:0x%x, reg address:0x%x, data size:0x%x.\n",
		dev_addr, addr, size);
	*/
	
	
	while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_BUSY);

	//printk("send slave address.\n");
	/*set slave addr*/
	i2c_writeb(i2c, REG_I2C_TXR, ((dev_addr << 1 ) | 0));
	//i2c_writeb(i2c, REG_I2C_CR, (I2C_C_WRITE | I2C_C_START | I2C_C_WACK));
	i2c_writeb(i2c, REG_I2C_CR, (I2C_C_WRITE | I2C_C_START));
	while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_RUN);
	if (i2c_readb(i2c, REG_I2C_SR) & I2C_S_RNOACK) {
		//printk(" reg addr:0x%2x no ack !!\n", addr);
		i2c_writeb(i2c, REG_I2C_CR, I2C_C_STOP); 
		return -1;
	}

	i2c_delay();
	//printk("send reg address.\n");
	i2c_writeb(i2c, REG_I2C_TXR, addr);
	//i2c_writeb(i2c, REG_I2C_CR, I2C_C_WRITE);
	i2c_writeb(i2c, REG_I2C_CR, I2C_C_WRITE);
	while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_RUN);
	if (i2c_readb(i2c, REG_I2C_SR) & I2C_S_RNOACK) {
		//printk(" reg addr no ack !!\n");
		i2c_writeb(i2c, REG_I2C_CR, I2C_C_STOP); 
		return -1;
	}

	//i2c_writeb(i2c, REG_I2C_CR, I2C_C_STOP); 

	//printk("read reg data.\n");
	i2c_writeb(i2c, REG_I2C_TXR, ((dev_addr << 1 ) | 1));
	i2c_writeb(i2c, REG_I2C_CR, (I2C_C_WRITE | I2C_C_START));
	while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_RUN);
	if (i2c_readb(i2c, REG_I2C_SR) & I2C_S_RNOACK) {
		//printk(" reg addr:0x%2x no ack !!\n", addr);
		i2c_writeb(i2c, REG_I2C_CR, I2C_C_STOP); 
		return -1;
	}
	
	ret = gs2fsb_xfer_read(i2c,  data,  size);

	//here();
	return ret;
}

int i2c_init(void)
{
	struct gs2fsb_i2c *i2c;
	
	g_i2c0.base = (void *)I2C0_BASE;
	i2c_writeb(&g_i2c0, REG_I2C_CTR, 0x00); 
	i2c_writeb(&g_i2c0, REG_I2C_PRER_LO, 0x0/*0*/); 
	i2c_writeb(&g_i2c0, REG_I2C_PRER_HI, 0x64/*0x64*/); 
	i2c_writeb(&g_i2c0, REG_I2C_CTR, 0x80); 

	i2c = &g_i2c0;
	
	/* send 9 sclks to release bus */
	/*
	i2c_writeb(i2c, REG_I2C_CR,   I2C_C_READ);
	i2c_readb(i2c,REG_I2C_TXR);
	i2c_writeb(i2c, REG_I2C_CR,   I2C_C_READ);
	i2c_readb(i2c,REG_I2C_TXR);
	*/
	//here();
}

int i2c_write(int dev_addr, unsigned char *data, unsigned char addr, int size)
{
	int ret;
	struct gs2fsb_i2c *i2c = &g_i2c0;
	/*
	printk("-->write dev address:0x%x, reg address:0x%x, data size:0x%x.\n",
		dev_addr, addr, size);
	*/
	while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_BUSY);

	/*set slave addr*/
	i2c_writeb(i2c, REG_I2C_TXR, (dev_addr << 1 ) | 0);
	i2c_writeb(i2c, REG_I2C_CR, (I2C_C_WRITE | I2C_C_START));
	while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_RUN);
	if (i2c_readb(i2c, REG_I2C_SR) & I2C_S_RNOACK) {
		//printk(" slave addr no ack !!\n");
		i2c_writeb(i2c, REG_I2C_CR, I2C_C_STOP); 
		return -1;
	}

	i2c_writeb(i2c, REG_I2C_TXR, addr);
	i2c_writeb(i2c, REG_I2C_CR, I2C_C_WRITE);
	while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_RUN);
	if (i2c_readb(i2c, REG_I2C_SR) & I2C_S_RNOACK) {
		//printk(" reg addr:0x%2x no ack !!\n", addr);
		i2c_writeb(i2c, REG_I2C_CR, I2C_C_STOP); 
		return -1;
	}

	ret = gs2fsb_xfer_write(i2c, data,  size);

	//here();
	return ret;
}

static unsigned char t_buf[1024] = {0};

#define TEST_LEN 	32

void print_gc0303_reg(void)
{
/*
	int i;
	
	i2c_set_addr(0x30);
        sensor_read_reg(0x80);
		
	printk("\n------------- show gc0303 register ---------------\n");
	for (i = 0x80; i < 0xa1; i++) {
		printk("reg:0x%2x = 0x%2x.\n", i, sensor_read_reg(i));
	}
*/	
}

void i2c_test(void)
{
	unsigned char buf[10];
	int ret;
	unsigned char c;
	unsigned char temp0;
	unsigned char send, rec;
	int i;
	int j;


}

