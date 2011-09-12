#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/slab.h>

/*register*/
#define REG_I2C_PRER_LO	0x0	//分频锁存器低字节寄存器
#define REG_I2C_PRER_HI	0x1	//分频锁存器高字节寄存器
#define REG_I2C_CTR		0x2	//控制寄存器
#define REG_I2C_TXR		0x3	//发送寄存器
#define REG_I2C_RXR		0x3	//接收寄存器
#define REG_I2C_CR		0X4	//命令寄存器（写时）
#define REG_I2C_SR		0X4	//状态寄存器（读时）

/*control 命令寄存器 位描述*/
#define I2C_C_START		0x80	//产生 START 信号
#define I2C_C_STOP		0x40	//产生 STOP 信号
#define I2C_C_READ		0x20	//产生读信号
#define I2C_C_WRITE		0x10	//产生写信号
#define I2C_C_WACK		0x8		//产生应答信号
#define I2C_C_IACK		0x1		//产生中断应答信号

/*status*/
#define	I2C_S_RNOACK	0x80	//1 没收到应答位  0 收到应答位
#define	I2C_S_BUSY		0x40	//1 总线在忙  0 总线空闲
#define	I2C_S_RUN		0x2		//1 表示正在传输数据  0 表示数据传输完毕
#define	I2C_S_IF		0x1		//中断标志位，一个数据传输完，或另外一个器件发起数据传输，该位置1


struct ls1b_i2c {
	void __iomem *base;
	u32 interrupt;
	wait_queue_head_t queue;
	struct i2c_adapter adap;
	int irq;
	u32 flags;
};

static void i2c_writeb(struct ls1b_i2c *i2c, unsigned int reg, unsigned char  data)
{
	(*(volatile unsigned char *)(i2c->base + reg)) = data;
}

static unsigned char i2c_readb (struct ls1b_i2c *i2c, unsigned char reg)
{
	unsigned char data;
	data = (*(volatile unsigned char *)(i2c->base + reg));
	return data;
}

static int ls1b_xfer_read(struct ls1b_i2c *i2c, unsigned char *buf, int len) 
{
	int x;

	for(x=0; x<len; x++) {
		i2c_writeb(i2c, REG_I2C_CR, I2C_C_READ);
		//send ACK last not send ACK
		if(x != (len -1)) 
			i2c_writeb(i2c, REG_I2C_CR,   I2C_C_READ);
		else
			i2c_writeb(i2c, REG_I2C_CR, I2C_C_WACK |I2C_C_READ);

		while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_RUN);

		buf[x] = i2c_readb(i2c,REG_I2C_TXR);
	}
	i2c_writeb(i2c,REG_I2C_CR, I2C_C_STOP);
		
	return 0;
}

static int ls1b_xfer_write(struct ls1b_i2c *i2c, unsigned char *buf, int len)
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
	}
	i2c_writeb(i2c, REG_I2C_CR, I2C_C_STOP);

	return 0;
}

static int ls1b_xfer(struct i2c_adapter *adap, struct i2c_msg *pmsg, int num)
{
	int i,ret;
	struct ls1b_i2c *i2c = adap->algo_data;

	dev_dbg(&adap->dev, "ls1b_xfer: processing %d messages:\n", num);

/*set slave addr*/
	for (i=0; i<num; i++) {

		char flags;
		
		while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_BUSY);
		
		flags = (pmsg->flags & I2C_M_RD)?1:0;
		i2c_writeb(i2c, REG_I2C_TXR, ((pmsg->addr << 1 ) | flags));
//		i2c_writeb(i2c, REG_I2C_CR, (I2C_C_WRITE | I2C_C_START | I2C_C_WACK));
		i2c_writeb(i2c, REG_I2C_CR, (I2C_C_WRITE | I2C_C_START));


		while(i2c_readb(i2c, REG_I2C_SR) & I2C_S_RUN);

		if (i2c_readb(i2c, REG_I2C_SR) & I2C_S_RNOACK) {
			printk(" slave addr no ack !!\n");
			i2c_writeb(i2c, REG_I2C_CR, I2C_C_STOP);
			return 0;
		}

 		if(flags )
			ret = ls1b_xfer_read(i2c, pmsg->buf, pmsg->len);
  		else
			ret = ls1b_xfer_write(i2c, pmsg->buf, pmsg->len);
		if (ret)
			return ret;
			
		dev_dbg(&adap->dev, "transfer complete\n");
		++pmsg;
	}
	return num;
}


static u32 ls1b_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm ls1b_algo = {
	.master_xfer = ls1b_xfer,
	.functionality = ls1b_functionality,
};

static struct i2c_adapter ls1b_ops = {
	.owner	= THIS_MODULE,
	.name	= "ls1b_adapter",
	.algo	= &ls1b_algo,
	.class = I2C_CLASS_HWMON,
	.timeout= 1,
	.retries= 1,
};

#if 0
//中断处理函数
static irqreturn_t ls1b_i2c_isr(int irqno, void *dev_id)
{
	struct ls1b_i2c *i2c = dev_id;

	i2c_writeb(i2c, REG_I2C_CR, 1);
	
	return IRQ_HANDLED;
}
#endif

static int ls1b_i2c_probe(struct platform_device *pdev)
{
	struct ls1b_i2c *i2c;
	struct resource *res;
	int result;
	
	printk(KERN_EMERG "i2c-ls1b probe\n");
	
	/* map the registers */
	res =  platform_get_resource(pdev,	IORESOURCE_MEM, 0);  //通过platform_device 获取i2c IO资源
	if (res == NULL) {
		dev_err(&pdev->dev, "cannot find IO resource\n");
		return -ENOENT;
	}
	
	//分配struct ls1b_i2c结构体内存
	if (!(i2c = kzalloc(sizeof(struct ls1b_i2c), GFP_KERNEL))) {
		dev_err(&pdev->dev, "can't allocate inteface!\n");
		result = -ENOMEM;
		goto fail2;
	}
	
	if (!request_mem_region(res->start, res->end - res->start + 1, "sb2f-i2c"))
		return -EBUSY;
	
	//ioremap把一个物理地址范围重新映射到处理器的虚拟地址空间，以供内核使用。
	//龙芯 在/include/asm-mips/io.h中定义
	i2c->base = ioremap(res->start, res->end - res->start + 1);
	if (!i2c->base) {
		printk(KERN_ERR "i2c-ls1b - failed to map controller\n");
		result = -ENOMEM;
		goto fail1;
	}

#if 0	
	i2c->irq = platform_get_irq(pdev, 0);
	if (i2c->irq < 0) {
		result = -ENXIO;
		goto fail_get_irq;
	}
	//初始化等待队列头
	init_waitqueue_head(&i2c->queue);

	
	if (i2c->irq != 0){
		if ((result = request_irq(i2c->irq, ls1b_i2c_isr, IRQF_SHARED, "i2c-ls1b", i2c)) < 0) {
			printk(KERN_ERR "i2c-ls1b - failed to attach interrupt\n");
			goto fail_irq;
		}
	}
#endif

	i2c->adap = ls1b_ops;
	i2c_set_adapdata(&i2c->adap, i2c);
	i2c->adap.algo_data = i2c;
	i2c->adap.dev.parent = &pdev->dev;
	
	//设置分频锁存器高低字节
	i2c_writeb(i2c, REG_I2C_PRER_LO, 0x64);
	i2c_writeb(i2c, REG_I2C_PRER_HI, 0x00);
	//设置控制寄存器值[7:6]EN:IEN 模块工作使能 中断使能
	i2c_writeb(i2c, REG_I2C_CTR, 0xc0);
	
	platform_set_drvdata(pdev, i2c);
	
	if ((result = i2c_add_numbered_adapter(&i2c->adap) < 0)) {
		printk(KERN_ERR "i2c-ls1b - failed to add adapter\n");
		goto fail0;
	}
	printk(KERN_EMERG "i2c-ls1b probe ok\n");
	return 0;

#if 0
fail_irq:
fail_get_irq:
		if (i2c->irq != 0)
			free_irq(i2c->irq, NULL);
#endif

fail0:
	platform_set_drvdata(pdev, NULL);
fail1:
	iounmap(i2c->base);
	release_mem_region(res->start, res->end - res->start + 1);
fail2:
	kfree(i2c);

	return result;
}

static int __devexit ls1b_i2c_remove(struct platform_device *pdev)
{
	struct ls1b_i2c *i2c = platform_get_drvdata(pdev);
	struct resource *res;
	int rc;

	rc = i2c_del_adapter(&i2c->adap);
	platform_set_drvdata(pdev, NULL);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	iounmap(i2c->base);
	release_mem_region(res->start, res->end - res->start + 1);

	return rc;
}

#ifdef CONFIG_PM


static int ls1b_i2c_suspend(struct platform_device *pdev, pm_message_t mesg)
{
//	clk_disable(twi_clk);
	return 0;
}

static int ls1b_i2c_resume(struct platform_device *pdev)
{
//	return clk_enable(twi_clk);
	return 0;
}

#else
#define ls1b_i2c_suspend	NULL
#define ls1b_i2c_resume	NULL
#endif

static struct platform_driver ls1b_i2c_driver = {
	.probe		= ls1b_i2c_probe,
	.remove	= __devexit_p(ls1b_i2c_remove),
	.suspend	= ls1b_i2c_suspend,
	.resume	= ls1b_i2c_resume,
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "ls1b-i2c",
	},
};

static int __init i2c_ls1b_init (void)
{
	printk(KERN_EMERG "i2c-ls1b init\n");
	return platform_driver_register(&ls1b_i2c_driver);
}

static void __exit i2c_ls1b_exit(void)
{
	return platform_driver_unregister(&ls1b_i2c_driver);
}

module_init(i2c_ls1b_init);
module_exit(i2c_ls1b_exit);

MODULE_AUTHOR("tanghaifeng <tanghaifeng-gz@loongson.cn>");
MODULE_DESCRIPTION("loongson 1b i2c bus driver");
MODULE_LICENSE("GPL");
