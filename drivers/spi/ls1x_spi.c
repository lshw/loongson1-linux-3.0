/*
 * Driver for Loongson1 SPI Controllers
 *
 * Copyright (C) 2013 Loongson Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>

#include <loongson1.h>
#include <irq.h>
#include <spi.h>

#define USE_POLL

#define REG_SPCR		0x00	//控制寄存器
#define REG_SPSR		0x01	//状态寄存器
#define REG_SPDR		0x02	//数据寄存器（TxFIFO）
#define REG_TXFIFO		0x02	//数据传输寄存器 输出
#define REG_RXFIFO		0x02	//数据传输寄存器 输入
#define REG_SPER		0x03	//外部寄存器
#define REG_PARAM		0x04	//SPI Flash参数控制寄存器
#define REG_SOFTCS		0x05	//SPI Flash片选控制寄存器
#define REG_PARAM2		0x06	//SPI Flash时序控制寄存器

struct ls1x_spi_devstate {
	unsigned int	hz;
	unsigned int	mode;
	u8		spcr;
	u8		sper;
};

struct ls1x_spi {
	/* bitbang has to be first */
	struct spi_bitbang	 bitbang;
	struct completion	 done;

	void __iomem		*regs;
	int irq;
	int	len;
	int	count;
	
	/* data buffers */
	const unsigned char	*tx;
	unsigned char		*rx;

	struct clk			*clk;
	struct resource		*ioarea;
	struct spi_master	*master;
	struct spi_device	*curdev;
	struct device		*dev;
	struct ls1x_spi_info *pdata;
};

static inline struct ls1x_spi *to_hw(struct spi_device *sdev)
{
	return spi_master_get_devdata(sdev->master);
}

static void ls1x_spi_chipsel(struct spi_device *spi, int value)
{
	struct ls1x_spi *hw = to_hw(spi);
	unsigned char cspol = spi->mode & SPI_CS_HIGH ? 1 : 0;
	unsigned char chip_select = (0x01 << (spi->chip_select + 4));

	switch (value) {
		case BITBANG_CS_INACTIVE:
			if (cspol) {
				writeb(0x00, hw->regs + REG_SOFTCS);
			} else {
				writeb(0xFF, hw->regs + REG_SOFTCS);
			}
		break;

		case BITBANG_CS_ACTIVE:
			if (cspol) {
				writeb(chip_select | (0x01 << spi->chip_select), hw->regs + REG_SOFTCS);
			} else {
				writeb(~(chip_select), hw->regs + REG_SOFTCS);
			}
		break;
	}
}

static int ls1x_spi_update_state(struct spi_device *spi,
				    struct spi_transfer *t)
{
	struct ls1x_spi *hw = to_hw(spi);
	struct ls1x_spi_devstate *cs = spi->controller_state;
	unsigned int bpw;
	unsigned int hz;
	unsigned int div, div_tmp;
	unsigned int bit;
	unsigned long clk;
	u8 spcr = readb(hw->regs + REG_SPCR);

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

	if (spi->mode != cs->mode) {
		spcr &= ~0x0c;

		if (spi->mode & SPI_CPHA)
			spcr |= 0x4;

		if (spi->mode & SPI_CPOL)
			spcr |= 0x8;

		cs->mode = spi->mode;
		cs->spcr = spcr;
	}

	if (cs->hz != hz) {
		clk = clk_get_rate(hw->clk);
		div = DIV_ROUND_UP(clk, hz);

		if (div < 2)
			div = 2;

		if (div > 4096)
			div = 4096;

		bit = fls(div) - 1;
		switch(1 << bit) {
			case 16: 
				div_tmp = 2;
				if (div > (1<<bit)) {
					div_tmp++;
				}
				break;
			case 32:
				div_tmp = 3;
				if (div > (1<<bit)) {
					div_tmp += 2;
				}
				break;
			case 8:
				div_tmp = 4;
				if (div > (1<<bit)) {
					div_tmp -= 2;
				}
				break;
			default:
				div_tmp = bit-1;
				if (div > (1<<bit)) {
					div_tmp++;
				}
				break;
		}
		dev_dbg(&spi->dev, "clk = %ld hz = %d div_tmp = %d bit = %d\n", 
		        clk, hz, div_tmp, bit);

		cs->hz = hz;
		spcr |= (div_tmp & 3);
		cs->spcr = spcr;
		cs->sper = (div_tmp >> 2) & 3;
	}

	return 0;
}

static int ls1x_spi_setupxfer(struct spi_device *spi,
				 struct spi_transfer *t)
{
	struct ls1x_spi_devstate *cs = spi->controller_state;
	struct ls1x_spi *hw = to_hw(spi);
	unsigned char val;
	int ret;

	ret = ls1x_spi_update_state(spi, t);
	if (!ret) {
		writeb(cs->spcr, hw->regs + REG_SPCR);
		val = readb(hw->regs + REG_SPER);
		writeb((val & ~3) | cs->sper, hw->regs + REG_SPER);
	}

	return ret;
}

static int ls1x_spi_setup(struct spi_device *spi)
{
	struct ls1x_spi_devstate *cs = spi->controller_state;
	struct ls1x_spi *hw = to_hw(spi);
	int ret;

	/* allocate settings on the first call */
	if (!cs) {
		cs = kzalloc(sizeof(struct ls1x_spi_devstate), GFP_KERNEL);
		if (!cs) {
			dev_err(&spi->dev, "no memory for controller state\n");
			return -ENOMEM;
		}

		cs->hz = -1;
		spi->controller_state = cs;
	}

	/* initialise the state from the device */
	ret = ls1x_spi_update_state(spi, NULL);
	if (ret)
		return ret;

	spin_lock(&hw->bitbang.lock);
	if (!hw->bitbang.busy) {
		hw->bitbang.chipselect(spi, BITBANG_CS_INACTIVE);
		/* need to ndelay for 0.5 clocktick ? */
	}
	spin_unlock(&hw->bitbang.lock);

	return 0;
}

static void ls1x_spi_cleanup(struct spi_device *spi)
{
	kfree(spi->controller_state);
}

static inline unsigned int hw_txbyte(struct ls1x_spi *hw, int count)
{
	return hw->tx ? hw->tx[count] : 0x00;
}

static int ls1x_spi_txrx(struct spi_device *spi, struct spi_transfer *t)
{
	struct ls1x_spi *hw = to_hw(spi);
		
	hw->tx = t->tx_buf;
	hw->rx = t->rx_buf;
	hw->len = t->len;
	hw->count = 0;

#ifdef USE_POLL
	for(hw->count=0; hw->count < hw->len; hw->count++) {
		writeb(hw_txbyte(hw, hw->count), hw->regs + REG_SPDR);
		while (readb(hw->regs + REG_SPSR) & 0x01) {
			cpu_relax();
		}
		if (hw->rx)
			hw->rx[hw->count] = readb(hw->regs + REG_SPDR);
		else
			readb(hw->regs + REG_SPDR);
	}
#else
//	init_completion(&hw->done);
	/* send the first byte */
	writeb(hw_txbyte(hw, 0), hw->regs + REG_SPDR);
	wait_for_completion(&hw->done);
#endif

	return hw->count;
}

#ifndef USE_POLL
static irqreturn_t ls1x_spi_irq(int irq, void *dev)
{
	struct ls1x_spi *hw = dev;
	unsigned int spsta = readb(hw->regs + REG_SPSR);
	unsigned int count = hw->count;

	writeb(spsta, hw->regs + REG_SPSR);
	
	hw->count++;

	if (hw->rx){
		hw->rx[count] = readb(hw->regs + REG_SPDR);
	} else {/* 由于发送和接收同时进行，即使SPI从设备没有发送有效数据也必须进行读出操作。*/
		readb(hw->regs + REG_SPDR);
	}

	count++;

	if (count < hw->len){
		writeb(hw_txbyte(hw, count), hw->regs + REG_SPDR);
	} else {
		complete(&hw->done);
	}
	
	return IRQ_HANDLED;
}
#endif

static int ls1x_spi_probe(struct platform_device *pdev)
{
	struct ls1x_spi_info *pdata;
	struct ls1x_spi *hw;
	struct spi_master *master;
	struct resource *res;
	int err = 0;
	unsigned char val;

	master = spi_alloc_master(&pdev->dev, sizeof(struct ls1x_spi));
	if (master == NULL) {
		dev_err(&pdev->dev, "No memory for spi_master\n");
		err = -ENOMEM;
		goto err_nomem;
	}

	hw = spi_master_get_devdata(master);
	memset(hw, 0, sizeof(struct ls1x_spi));

	hw->master = spi_master_get(master);
	hw->pdata = pdata = pdev->dev.platform_data;
	hw->dev = &pdev->dev;

	if (pdata == NULL) {
		dev_err(&pdev->dev, "No platform data supplied\n");
		err = -ENOENT;
		goto err_no_pdata;
	}

	platform_set_drvdata(pdev, hw);
	init_completion(&hw->done);
	
	/* setup the master state. */
	
	/* the spi->mode bits understood by this driver: */
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH;

	master->num_chipselect = hw->pdata->num_cs;
	master->bus_num = pdata->bus_num;
	
	/* setup the state for the bitbang driver */
	
	hw->bitbang.master         = hw->master;
	hw->bitbang.setup_transfer = ls1x_spi_setupxfer;
	hw->bitbang.chipselect     = ls1x_spi_chipsel;
	hw->bitbang.txrx_bufs      = ls1x_spi_txrx;
	
	hw->master->setup  = ls1x_spi_setup;
	hw->master->cleanup = ls1x_spi_cleanup;

	dev_dbg(hw->dev, "bitbang at %p\n", &hw->bitbang);

	/* find and map our resources */
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "Cannot get IORESOURCE_MEM\n");
		err = -ENOENT;
		goto err_no_iores;
	}

	hw->ioarea = request_mem_region(res->start, (res->end - res->start)+1,
					pdev->name);

	if (hw->ioarea == NULL) {
		dev_err(&pdev->dev, "Cannot reserve region\n");
		err = -ENXIO;
		goto err_no_iores;
	}

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

	/* program defaults into the registers */
	writeb(0xc0, hw->regs + REG_SPSR);
	val = readb(hw->regs + REG_PARAM);
	val &= 0xfe;
	writeb(val, hw->regs + REG_PARAM);

#ifndef USE_POLL
 	writeb(0xd0, hw->regs + REG_SPCR);
	err = request_irq(hw->irq, ls1x_spi_irq, 0, pdev->name, hw);
	if (err) {
		dev_err(&pdev->dev, "Cannot claim IRQ\n");
		goto err_no_irq;
	}
#else
 	writeb(0x50, hw->regs + REG_SPCR);
#endif
  	writeb(0x05, hw->regs + REG_SPER);
  	writeb(0x05, hw->regs + REG_PARAM2);


	hw->clk = clk_get(&pdev->dev, "apb");
	if (IS_ERR(hw->clk)) {
		dev_err(&pdev->dev, "No clock for device\n");
		err = PTR_ERR(hw->clk);
		goto err_no_clk;
	}

	/* setup any gpio we can */

	/* register our spi controller */
	err = spi_bitbang_start(&hw->bitbang);
	if (err) {
		dev_err(&pdev->dev, "Failed to register SPI master\n");
		goto err_register;
	}

	return 0;

err_register:

err_no_clk:
#ifndef USE_POLL
	free_irq(hw->irq, hw);
#endif

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

static int __exit ls1x_spi_remove(struct platform_device *dev)
{
	struct ls1x_spi *hw = platform_get_drvdata(dev);

	platform_set_drvdata(dev, NULL);

	spi_bitbang_stop(&hw->bitbang);
#ifndef USE_POLL
	free_irq(hw->irq, hw);
#endif
	iounmap(hw->regs);
	
	/* gpio free */
	
	release_resource(hw->ioarea);
	kfree(hw->ioarea);

	spi_master_put(hw->master);
	return 0;
}

#ifdef CONFIG_PM
static int ls1x_spi_suspend(struct device *dev)
{
//	struct ls1x_spi *hw = platform_get_drvdata(pdev);
	struct ls1x_spi *hw = platform_get_drvdata(to_platform_device(dev));

	return 0;
}

static int ls1x_spi_resume(struct device *dev)
{
	struct ls1x_spi *hw = platform_get_drvdata(to_platform_device(dev));
	unsigned char val;

/* program defaults into the registers */
	writeb(0xc0, hw->regs + REG_SPSR);
	val = readb(hw->regs + REG_PARAM);
	val &= 0xfe;
	writeb(val, hw->regs + REG_PARAM);
 	writeb(0xd0, hw->regs + REG_SPCR);
  	writeb(0x05, hw->regs + REG_SPER);

//	clk_enable(hw->clk);
	return 0;
}

static const struct dev_pm_ops ls1x_spi_pmops = {
	.suspend	= ls1x_spi_suspend,
	.resume		= ls1x_spi_resume,
};

#define LS1X_SPI_PMOPS &ls1x_spi_pmops
#else
#define LS1X_SPI_PMOPS NULL
#endif /* CONFIG_PM */

MODULE_ALIAS("platform:ls1x-spi");
static struct platform_driver ls1x_spi_driver = {
	.remove    = __exit_p(ls1x_spi_remove),
	.driver    = {
		.name  = "ls1x-spi",
		.owner = THIS_MODULE,
		.pm    = LS1X_SPI_PMOPS,
	},
};

static int __init ls1x_spi_init(void)
{
	return platform_driver_probe(&ls1x_spi_driver, ls1x_spi_probe);
}

static void __exit ls1x_spi_exit(void)
{
	platform_driver_unregister(&ls1x_spi_driver);
}

module_init(ls1x_spi_init);
module_exit(ls1x_spi_exit);

MODULE_DESCRIPTION("loongson1 SPI Driver");
MODULE_AUTHOR("tanghaifeng <tanghaifeng-gz@loongson.cn");
MODULE_LICENSE("GPL");
