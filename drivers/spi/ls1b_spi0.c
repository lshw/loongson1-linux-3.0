#define DEBUG

#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>

#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>

#include <asm/types.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/mach-loongson/ls1x/ls1b_board_int.h>
#include <asm/mach-loongson/ls1x/ls1b_board.h>
#include <asm/mach-loongson/ls1x/spi.h>


extern unsigned long bus_clock;

#define clk_disable(...)
#define clk_enable(...)
#define clk_put(...)
#define clk_get(...)
#define clk_get_rate(...) bus_clock
//#define USE_POLL

struct ls1b_spi {
	/* bitbang has to be first */
	struct spi_bitbang	 bitbang;
	struct completion	 done;

	void __iomem		*regs;
	int			 irq;
	int			 len;
	int			 count;
	
	/* data buffers */
	const unsigned char	*tx;
	unsigned char		*rx;

	struct clk		*clk;
	struct resource		*ioarea;
	struct spi_master	*master;
	struct spi_device	*curdev;
	struct device		*dev;
	struct ls1b_spi_info *pdata;
};


static inline struct ls1b_spi *to_hw(struct spi_device *sdev)
{
	return spi_master_get_devdata(sdev->master);
}

static void ls1b_spi_chipsel(struct spi_device *spi, int value)
{
	struct ls1b_spi *hw = to_hw(spi);
	unsigned char cspol = spi->mode & SPI_CS_HIGH ? 1 : 0;	//THF
	unsigned char ret;
	unsigned char chip_select = (0x01 << (spi->chip_select + 4));

	switch (value) {
	case BITBANG_CS_INACTIVE:
		if (cspol)
		{
			ret = 0x00;
			writeb(ret, hw->regs + REG_SOFTCS);	//THF
		}
		else
		{
			ret = 0xFF;
			writeb(ret, hw->regs + REG_SOFTCS);	//THF
		}
		break;

	case BITBANG_CS_ACTIVE:
		if (cspol)
		{
			ret = (chip_select | 0x0F);
			writeb(ret, hw->regs + REG_SOFTCS);	//THF
		}
		else
		{
			ret = ~(chip_select);
			writeb(ret, hw->regs + REG_SOFTCS);	//THF
		}
		break;
	}
}


static int ls1b_spi_setupxfer(struct spi_device *spi,
				 struct spi_transfer *t)
{
	struct ls1b_spi *hw = to_hw(spi);
	unsigned int bpw;
	unsigned int hz;
	unsigned int div;
	unsigned char val,bit;
	
	bpw = t ? t->bits_per_word : spi->bits_per_word;
	hz  = t ? t->speed_hz : spi->max_speed_hz;
	
	if (!bpw)
		bpw = 8;

	if (!hz)
		hz = spi->max_speed_hz;
	
	if (bpw != 8) {
		dev_err(&spi->dev, "invalid bits-per-word (%d)\n", bpw);
		return -EINVAL;
	}

	div = clk_get_rate(hw->clk) / hz;

	if (div < 2)
		div = 2;

	if (div > 4096)
		div = 4096;

	bit = fls(div) - 1;
	switch(2<<bit)
	{
		case 16: div=2;break;
		case 32: div=3;break;
		case 8: div=4;break;
		default: div=bit-1;break;
	}

	dev_dbg(&spi->dev, "setting pre-scaler to %d (hz %d)\n", div, hz);
	val=readb(hw->regs + REG_SPCR);
	val &= ~3;
	val |=div&3;
	writeb(val, hw->regs + REG_SPCR);
	div >>= 2;
	val=readb(hw->regs + REG_SPER);
	val &= ~3;
	val |=div&3;
	writeb(val, hw->regs + REG_SPER);

	spin_lock(&hw->bitbang.lock);
	if (!hw->bitbang.busy) {
		hw->bitbang.chipselect(spi, BITBANG_CS_INACTIVE);
		/* need to ndelay for 0.5 clocktick ? */
	}
	spin_unlock(&hw->bitbang.lock);

	return 0;
}

static int ls1b_spi_setup(struct spi_device *spi)
{
	int ret;
	
	if (!spi->bits_per_word)
		spi->bits_per_word = 8;

	if ((spi->mode & SPI_LSB_FIRST) != 0)
		return -EINVAL;

	ret = ls1b_spi_setupxfer(spi, NULL);
	if (ret < 0) {
		dev_err(&spi->dev, "setupxfer returned %d\n", ret);
		return ret;
	}

	dev_dbg(&spi->dev, "%s: mode %d, %u bpw, %d hz\n",
		__FUNCTION__, spi->mode, spi->bits_per_word,
		spi->max_speed_hz);

	return 0;
}

static void ls1b_spi_cleanup(struct spi_device *spi)
{
	kfree(spi->controller_state);
}

static inline unsigned int hw_txbyte(struct ls1b_spi *hw, int count)
{
	return hw->tx ? hw->tx[count] : 0x00;
}

static int ls1b_spi_txrx(struct spi_device *spi, struct spi_transfer *t)
{
	struct ls1b_spi *hw = to_hw(spi);

	dev_dbg(&spi->dev, "txrx: tx %p, rx %p, len %d\n",
		t->tx_buf, t->rx_buf, t->len);
		
	hw->tx = t->tx_buf;
	hw->rx = t->rx_buf;
	hw->len = t->len;
	hw->count = 0;

#ifdef USE_POLL
	for(hw->count=0;hw->count < hw->len;hw->count++)
	{
		writeb(hw_txbyte(hw, hw->count), hw->regs + REG_SPDR);
		while (readb(hw->regs + REG_SPSR)&0x01);
		if (hw->rx)
		{
			hw->rx[hw->count] = readb(hw->regs + REG_SPDR);
		}
		else
			readb(hw->regs + REG_SPDR);		//lxy
	}
#else
	init_completion(&hw->done);
	/* send the first byte */
	writeb(hw_txbyte(hw, 0), hw->regs + REG_SPDR);
	wait_for_completion(&hw->done);//在中断里等待处理
#endif

	return hw->count;
}

#ifndef USE_POLL
static irqreturn_t ls1b_spi_irq(int irq, void *dev)
{
	struct ls1b_spi *hw = dev;
	unsigned int spsta = readb(hw->regs + REG_SPSR);
	unsigned int count = hw->count;

	writeb(spsta,hw->regs + REG_SPSR);
	
	hw->count++;

	if (hw->rx){
		hw->rx[count] = readb(hw->regs + REG_SPDR);
	}
	else
		readb(hw->regs + REG_SPDR);		//lxy

	count++;

	if (count < hw->len){
		writeb(hw_txbyte(hw, count), hw->regs + REG_SPDR);
	}
	else
		complete(&hw->done);
		
	return IRQ_HANDLED;
}
#endif

static int ls1b_spi_probe(struct platform_device *pdev)
{
	struct ls1b_spi *hw;
	struct spi_master *master;
	struct spi_board_info *bi;
	struct resource *res;
	int err = 0;
	int i;
	unsigned char val;
	
	printk(KERN_EMERG "loongson 1B spi probe begin\n");
	
	//分配SPI主机控制器结构体 第二个参数 指预分配私有数据大小
	//分配spi_master结构体并做简单的初始化赋值(spi masr是作为一个SPI类初始化的)，同时分配(清零)自定义的sb2f_spi结构体
	master = spi_alloc_master(&pdev->dev, sizeof(struct ls1b_spi));
	if (master == NULL) {
		dev_err(&pdev->dev, "No memory for spi_master\n");
		err = -ENOMEM;
		goto err_nomem;
	}
	
	//填充struct spi_master结构
	//spi_master_get_devdata()函数用于取出struct ls1b_spi结构体指针 在spi_alloc_master()函数中通过spi_master_set_devdata()设置
	//return master->cdev->class_data;
	//master中设备类成员(cdev)中的私有数据class_data
	hw = spi_master_get_devdata(master);	//THF
	memset(hw, 0, sizeof(struct ls1b_spi));

	//spi_master_get()函数用于取出struct spi_master *master结构体指针
	//增加spi_master引用计数
	hw->master = spi_master_get(master);
	hw->pdata = pdev->dev.platform_data;
	hw->dev = &pdev->dev;

	if (hw->pdata == NULL) {
		dev_err(&pdev->dev, "No platform data supplied\n");
		err = -ENOENT;
		goto err_no_pdata;
	}

	platform_set_drvdata(pdev, hw);
	init_completion(&hw->done);
	
	/* the spi->mode bits understood by this driver: */
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH;

	master->num_chipselect = hw->pdata->num_cs;
	master->bus_num = hw->pdata->bus_num;
	
	/* setup the state for the bitbang driver */
	hw->bitbang.master         = hw->master;
	hw->bitbang.setup_transfer = ls1b_spi_setupxfer;
	hw->bitbang.chipselect     = ls1b_spi_chipsel;
	hw->bitbang.txrx_bufs      = ls1b_spi_txrx;
	hw->bitbang.master->bus_num	=	pdev->id;	//THF
	hw->master->setup  = ls1b_spi_setup;
	hw->master->cleanup = ls1b_spi_cleanup;

	dev_dbg(hw->dev, "bitbang at %p\n", &hw->bitbang);

	/* find and map our resources */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "Cannot get IORESOURCE_MEM\n");
		err = -ENOENT;
		goto err_no_iores;
	}

#if 1
	//调用request_mem_region()不是必须的，但是建议使用。
	//该函数的任务是检查申请的资源是否可用，如果可用则申请成功，并标志为已经使用，
	//其他驱动想再申请该资源时就会失败。
	hw->ioarea = request_mem_region(res->start, (res->end - res->start)+1,
					pdev->name);

	if (hw->ioarea == NULL) {
		dev_err(&pdev->dev, "Cannot reserve region\n");
		err = -ENXIO;
		goto err_no_iores;
	}
#endif

	hw->regs = ioremap(res->start, (res->end - res->start)+1);
	if (hw->regs == NULL) {
		dev_err(&pdev->dev, "Cannot map IO\n");
		err = -ENXIO;
		goto err_no_iomap;
	}

	hw->irq = platform_get_irq(pdev, 0);
	if (hw->irq < 0) {
		dev_err(&pdev->dev, "No IRQ specified\n");
		err = -ENOENT;
		goto err_no_irq;
	}

	//#define SPSR      0x1	//状态寄存器
	//val = 0xc0;//清零中断标记位 清零寄存器溢出标记 清空状态寄存器
	writeb(0xc0, hw->regs + REG_SPSR);	//THF
	val = readb(hw->regs + REG_PARAM);
	val &= 0xfe;	//memory_en spi flash 读使能，无效时 csn[0]可由软件控制。 
	writeb(val, hw->regs + REG_PARAM);
	
#ifndef USE_POLL
	printk(KERN_EMERG "SPI IRQ mode\n");
 	writeb(0xd0, hw->regs + REG_SPCR);	//THF
#else
 	writeb(0x50, hw->regs + REG_SPCR);	//THF
#endif
	//#define SPER      0x3	//外部寄存器
	//spre:01 [2]mode spi接口模式控制 1:采样与发送时机错开半周期  [1:0]spre 与Spr一起设定分频的比率
  	writeb(0x05, hw->regs + REG_SPER);	//THF

#ifndef USE_POLL
	err = request_irq(hw->irq, ls1b_spi_irq, 0, pdev->name, hw);
	if (err) {
		dev_err(&pdev->dev, "Cannot claim IRQ\n");
		goto err_no_irq;
	}
	printk(KERN_EMERG "SPI master IRQ num = %d\n", hw->irq);
#endif

	/* setup any gpio we can */

	/* register our spi controller */
	err = spi_bitbang_start(&hw->bitbang);
	if (err) {
		dev_err(&pdev->dev, "Failed to register SPI master\n");
		goto err_register;
	}

	/* register all the devices associated */
/*
#if 1
	if (spi_register_master(master)) {
		dev_err(&pdev->dev, "cannot register SPI master\n");
		err = -EBUSY;
		goto err_register;
	}
#else
	bi = &hw->pdata->board_info[0];
	for (i = 0; i < hw->pdata->board_size; i++, bi++) {
		dev_info(hw->dev, "registering %s\n", bi->modalias);

		bi->controller_data = hw;
		spi_new_device(master, bi);
	}
#endif
*/

	printk(KERN_EMERG "loongson 1B spi probe done\n");
	return 0;

 err_register:
	clk_disable(hw->clk);
	clk_put(hw->clk);

	free_irq(hw->irq, hw);

 err_no_irq:
	iounmap(hw->regs);

 err_no_iomap:
	release_resource(hw->ioarea);
	kfree(hw->ioarea);

 err_no_iores:
 err_no_pdata:
	spi_master_put(hw->master);;

 err_nomem:
	return err;
}

static int ls1b_spi_remove(struct platform_device *dev)
{
	struct ls1b_spi *hw = platform_get_drvdata(dev);

	platform_set_drvdata(dev, NULL);

	spi_unregister_master(hw->master);

	clk_disable(hw->clk);
	clk_put(hw->clk);

	free_irq(hw->irq, hw);
	iounmap(hw->regs);

	release_resource(hw->ioarea);
	kfree(hw->ioarea);

	spi_master_put(hw->master);
	return 0;
}


#ifdef CONFIG_PM
static int ls1b_spi_suspend(struct platform_device *pdev, pm_message_t msg)
{
	struct ls1b_spi *hw = platform_get_drvdata(pdev);

	clk_disable(hw->clk);
	return 0;
}

static int ls1b_spi_resume(struct platform_device *pdev)
{
	struct ls1b_spi *hw = platform_get_drvdata(pdev);

	clk_enable(hw->clk);
	return 0;
}
#else
#define ls1b_spi_suspend NULL
#define ls1b_spi_resume  NULL
#endif

static struct platform_driver ls1b_spidrv = {
	.probe		= ls1b_spi_probe,
	.remove	= ls1b_spi_remove,
	.suspend	= ls1b_spi_suspend,
	.resume	= ls1b_spi_resume,
	.driver	= {
		.name	= "ls1b-spi0",
		.owner	= THIS_MODULE,
	},
};

static int __init ls1b_spi_init(void)
{
	return platform_driver_register(&ls1b_spidrv);
}

static void __exit ls1b_spi_exit(void)
{
	platform_driver_unregister(&ls1b_spidrv);
}

module_init(ls1b_spi_init);
module_exit(ls1b_spi_exit);

MODULE_DESCRIPTION("loongson 1B SPI0 Driver");
MODULE_AUTHOR("tanghaifeng <tanghaifeng-gz@loongson.cn");
MODULE_LICENSE("GPL");
