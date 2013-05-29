/*
 * ATT7022/58/68/78 Polyphase Multifunction Energy Metering IC Driver
 *
 * Copyright 2010 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/list.h>

#include "../iio.h"
#include "../sysfs.h"
#include "meter.h"
#include "att7022.h"

static ssize_t att7022_read_24bit(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;
	u32 val;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct att7022_state *st = iio_dev_get_devdata(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);

	ret = st->read_reg_24(dev, this_attr->address, &val);
	if (ret)
		return ret;

	return sprintf(buf, "%u\n", val);
}

static ssize_t att7022_write_24bit(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t len)
{
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct att7022_state *st = iio_dev_get_devdata(indio_dev);

	int ret;
	long val;

	ret = strict_strtol(buf, 10, &val);
	if (ret)
		goto error_ret;
	ret = st->write_reg_24(dev, this_attr->address, val);

error_ret:
	return ret ? ret : len;
}

static int att7022_reset(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct att7022_state *st = iio_dev_get_devdata(indio_dev);
	u32 val = 0x00000000;

	return st->write_reg_24(dev, ATT7022_RESET, val);
}


static ssize_t att7022_write_reset(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t len)
{
	if (len < 1)
		return -1;
	switch (buf[0]) {
	case '1':
	case 'y':
	case 'Y':
		return att7022_reset(dev);
	}
	return -1;
}

static IIO_DEVICE_ATTR(dev_id, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_DEV_ID);

static IIO_DEVICE_ATTR(pa, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_PA);
static IIO_DEVICE_ATTR(pb, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_PB);
static IIO_DEVICE_ATTR(pc, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_PC);
static IIO_DEVICE_ATTR(pt, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_PT);

static IIO_DEVICE_ATTR(qa, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_QA);
static IIO_DEVICE_ATTR(qb, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_QB);
static IIO_DEVICE_ATTR(qc, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_QC);
static IIO_DEVICE_ATTR(qt, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_QT);

static IIO_DEVICE_ATTR(sa, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_SA);
static IIO_DEVICE_ATTR(sb, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_SB);
static IIO_DEVICE_ATTR(sc, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_SC);
static IIO_DEVICE_ATTR(st, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_ST);

static IIO_DEVICE_ATTR(uarms, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_UARMS);
static IIO_DEVICE_ATTR(ubrms, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_UBRMS);
static IIO_DEVICE_ATTR(ucrms, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_UCRMS);

static IIO_DEVICE_ATTR(iarms, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_IARMS);
static IIO_DEVICE_ATTR(ibrms, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_IBRMS);
static IIO_DEVICE_ATTR(icrms, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_ICRMS);
static IIO_DEVICE_ATTR(itrms, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_ITRMS);

static IIO_DEVICE_ATTR(pfa, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_PFA);
static IIO_DEVICE_ATTR(pfb, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_PFB);
static IIO_DEVICE_ATTR(pfc, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_PFC);
static IIO_DEVICE_ATTR(pft, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_PFT);

static IIO_DEVICE_ATTR(pga, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_PGA);
static IIO_DEVICE_ATTR(pgb, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_PGB);
static IIO_DEVICE_ATTR(pgc, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_PGC);

static IIO_DEVICE_ATTR(intflag, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_INTFLAG);
static IIO_DEVICE_ATTR(freq, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_FREQ);
static IIO_DEVICE_ATTR(eflag, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_EFLAG);

static IIO_DEVICE_ATTR(epa, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_EPA);
static IIO_DEVICE_ATTR(epb, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_EPB);
static IIO_DEVICE_ATTR(epc, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_EPC);
static IIO_DEVICE_ATTR(ept, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_EPT);

static IIO_DEVICE_ATTR(eqa, S_IWUSR | S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_EQA);
static IIO_DEVICE_ATTR(eqb, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_EQB);
static IIO_DEVICE_ATTR(eqc, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_EQC);
static IIO_DEVICE_ATTR(eqt, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_EQT);

static IIO_DEVICE_ATTR(yuaub, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_YUAUB);
static IIO_DEVICE_ATTR(yuauc, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_YUAUC);
static IIO_DEVICE_ATTR(yubuc, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_YUBUC);

static IIO_DEVICE_ATTR(i0rms, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_I0RMS);
static IIO_DEVICE_ATTR(tpsd0, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_TPSD0);
static IIO_DEVICE_ATTR(utrms, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_UTRMS);
static IIO_DEVICE_ATTR(sflag, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_SFLAG);
static IIO_DEVICE_ATTR(bckreg, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_BCKREG);
static IIO_DEVICE_ATTR(cmdchksum, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_CMDCHKSUM);

static IIO_DEVICE_ATTR(sample_ia, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_SAMPLE_IA);
static IIO_DEVICE_ATTR(sample_ib, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_SAMPLE_IB);
static IIO_DEVICE_ATTR(sample_ic, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_SAMPLE_IC);
static IIO_DEVICE_ATTR(sample_ua, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_SAMPLE_UA);
static IIO_DEVICE_ATTR(sample_ub, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_SAMPLE_UB);
static IIO_DEVICE_ATTR(sample_uc, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_SAMPLE_UC);

static IIO_DEVICE_ATTR(esa, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_ESA);
static IIO_DEVICE_ATTR(esb, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_ESB);
static IIO_DEVICE_ATTR(esc, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_ESC);
static IIO_DEVICE_ATTR(est, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_EST);

static IIO_DEVICE_ATTR(fstcnta, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_FSTCNTA);
static IIO_DEVICE_ATTR(fstcntb, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_FSTCNTB);
static IIO_DEVICE_ATTR(fstcntc, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_FSTCNTC);
static IIO_DEVICE_ATTR(fstcntt, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_FSTCNTT);

static IIO_DEVICE_ATTR(pflag, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_PFLAG);
static IIO_DEVICE_ATTR(chksum, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_CHKSUM);
static IIO_DEVICE_ATTR(sample_io, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_SAMPLE_I0);

static IIO_DEVICE_ATTR(linepa, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_LINEPA);
static IIO_DEVICE_ATTR(linepb, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_LINEPB);
static IIO_DEVICE_ATTR(linepc, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_LINEPC);
static IIO_DEVICE_ATTR(linept, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_LINEPT);
static IIO_DEVICE_ATTR(lineepa, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_LINEEPA);
static IIO_DEVICE_ATTR(lineepb, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_LINEEPB);
static IIO_DEVICE_ATTR(lineepc, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_LINEEPC);
static IIO_DEVICE_ATTR(lineept, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_LINEEPT);

static IIO_DEVICE_ATTR(lineuarrms, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_LINEUARRMS);
static IIO_DEVICE_ATTR(lineubrrms, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_LINEUBRRMS);
static IIO_DEVICE_ATTR(lineucrrms, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_LINEUCRRMS);
static IIO_DEVICE_ATTR(lineiarrms, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_LINEIARRMS);
static IIO_DEVICE_ATTR(lineibrrms, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_LINEIBRRMS);
static IIO_DEVICE_ATTR(lineicrrms, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_LINEICRRMS);

static IIO_DEVICE_ATTR(leflag, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_LEFLAG);
static IIO_DEVICE_ATTR(ptrwavebuff, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_PTRWAVEBUFF);
static IIO_DEVICE_ATTR(wavebuff, S_IRUGO, att7022_read_24bit,
		NULL, ATT7022_WAVEBUFF);

static int att7022_set_irq(struct device *dev, bool enable)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct att7022_state *st = iio_dev_get_devdata(indio_dev);

	int ret;
	u32 irqen;
#if 0
	ret = st->read_reg_32(dev, ATT7022_MASK0, &irqen);
	if (ret)
		goto error_ret;

	if (enable)
		irqen |= 1 << 17; /* 1: interrupt enabled when all periodical
				     (at 8 kHz rate) DSP computations finish. */
	else
		irqen &= ~(1 << 17);

	ret = st->write_reg_32(dev, ATT7022_MASK0, irqen);
	if (ret)
		goto error_ret;
#endif
error_ret:
	return ret;
}

static int att7022_initial_setup(struct att7022_state *st)
{
	int ret = 0;
	struct device *dev = &st->indio_dev->dev;
	u32 val;

	/* Disable IRQ */
//	ret = att7022_set_irq(dev, false);
//	if (ret) {
//		dev_err(dev, "disable irq failed");
//		goto err_ret;
//	}

	att7022_reset(dev);
	msleep(ATT7022_STARTUP_DELAY);

	st->write_reg_24(dev, 0xc6, 0x00005A);
	st->read_reg_24(dev, 0x00, &val);

	st->write_reg_24(dev, 0xc9, 0x000000);
	st->write_reg_24(dev, 0xc6, 0x00005A);
	st->read_reg_24(dev, 0x00, &val);

	st->write_reg_24(dev, 0xc6, 0x000000);

err_ret:
	return ret;
}

static IIO_DEV_ATTR_RESET(att7022_write_reset);

static IIO_CONST_ATTR_SAMP_FREQ_AVAIL("8000");

static IIO_CONST_ATTR(name, "att7022");

static struct attribute *att7022_attributes[] = {
	&iio_dev_attr_dev_id.dev_attr.attr,
	&iio_dev_attr_pa.dev_attr.attr,
	&iio_dev_attr_pb.dev_attr.attr,
	&iio_dev_attr_pc.dev_attr.attr,
	&iio_dev_attr_pt.dev_attr.attr,
	&iio_dev_attr_qa.dev_attr.attr,
	&iio_dev_attr_qb.dev_attr.attr,
	&iio_dev_attr_qc.dev_attr.attr,
	&iio_dev_attr_qt.dev_attr.attr,
	&iio_dev_attr_sa.dev_attr.attr,
	&iio_dev_attr_sb.dev_attr.attr,
	&iio_dev_attr_sc.dev_attr.attr,
	&iio_dev_attr_st.dev_attr.attr,

	&iio_dev_attr_uarms.dev_attr.attr,
	&iio_dev_attr_ubrms.dev_attr.attr,
	&iio_dev_attr_ucrms.dev_attr.attr,
	&iio_dev_attr_iarms.dev_attr.attr,
	&iio_dev_attr_ibrms.dev_attr.attr,
	&iio_dev_attr_icrms.dev_attr.attr,
	&iio_dev_attr_itrms.dev_attr.attr,
	
	&iio_dev_attr_pfa.dev_attr.attr,
	&iio_dev_attr_pfb.dev_attr.attr,
	&iio_dev_attr_pfc.dev_attr.attr,
	&iio_dev_attr_pft.dev_attr.attr,
	&iio_dev_attr_pga.dev_attr.attr,
	&iio_dev_attr_pgb.dev_attr.attr,
	&iio_dev_attr_pgc.dev_attr.attr,

	&iio_dev_attr_intflag.dev_attr.attr,
	&iio_dev_attr_freq.dev_attr.attr,
	&iio_dev_attr_eflag.dev_attr.attr,

	&iio_dev_attr_epa.dev_attr.attr,
	&iio_dev_attr_epb.dev_attr.attr,
	&iio_dev_attr_epc.dev_attr.attr,
	&iio_dev_attr_ept.dev_attr.attr,
	&iio_dev_attr_eqa.dev_attr.attr,
	&iio_dev_attr_eqb.dev_attr.attr,
	&iio_dev_attr_eqc.dev_attr.attr,
	&iio_dev_attr_eqt.dev_attr.attr,

	&iio_dev_attr_yuaub.dev_attr.attr,
	&iio_dev_attr_yuauc.dev_attr.attr,
	&iio_dev_attr_yubuc.dev_attr.attr,

	&iio_dev_attr_i0rms.dev_attr.attr,
	&iio_dev_attr_tpsd0.dev_attr.attr,
	&iio_dev_attr_utrms.dev_attr.attr,
	&iio_dev_attr_sflag.dev_attr.attr,
	&iio_dev_attr_bckreg.dev_attr.attr,
	&iio_dev_attr_cmdchksum.dev_attr.attr,

	&iio_dev_attr_sample_ia.dev_attr.attr,
	&iio_dev_attr_sample_ib.dev_attr.attr,
	&iio_dev_attr_sample_ic.dev_attr.attr,
	&iio_dev_attr_sample_ua.dev_attr.attr,
	&iio_dev_attr_sample_ub.dev_attr.attr,
	&iio_dev_attr_sample_uc.dev_attr.attr,

	&iio_dev_attr_esa.dev_attr.attr,
	&iio_dev_attr_esb.dev_attr.attr,
	&iio_dev_attr_esc.dev_attr.attr,
	&iio_dev_attr_est.dev_attr.attr,
	&iio_dev_attr_fstcnta.dev_attr.attr,
	&iio_dev_attr_fstcntb.dev_attr.attr,
	&iio_dev_attr_fstcntc.dev_attr.attr,
	&iio_dev_attr_fstcntt.dev_attr.attr,

	&iio_dev_attr_pflag.dev_attr.attr,
	&iio_dev_attr_chksum.dev_attr.attr,
	&iio_dev_attr_sample_io.dev_attr.attr,

	&iio_dev_attr_linepa.dev_attr.attr,
	&iio_dev_attr_linepb.dev_attr.attr,
	&iio_dev_attr_linepc.dev_attr.attr,
	&iio_dev_attr_linept.dev_attr.attr,
	&iio_dev_attr_lineepa.dev_attr.attr,
	&iio_dev_attr_lineepb.dev_attr.attr,
	&iio_dev_attr_lineepc.dev_attr.attr,
	&iio_dev_attr_lineept.dev_attr.attr,

	&iio_dev_attr_lineuarrms.dev_attr.attr,
	&iio_dev_attr_lineubrrms.dev_attr.attr,
	&iio_dev_attr_lineucrrms.dev_attr.attr,
	&iio_dev_attr_lineiarrms.dev_attr.attr,
	&iio_dev_attr_lineibrrms.dev_attr.attr,
	&iio_dev_attr_lineicrrms.dev_attr.attr,

	&iio_dev_attr_leflag.dev_attr.attr,
	&iio_dev_attr_ptrwavebuff.dev_attr.attr,
	&iio_dev_attr_wavebuff.dev_attr.attr,
	&iio_dev_attr_reset.dev_attr.attr,
	&iio_const_attr_name.dev_attr.attr,
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	NULL,
};

static const struct attribute_group att7022_attribute_group = {
	.attrs = att7022_attributes,
};

static const struct iio_info att7022_info = {
	.attrs = &att7022_attribute_group,
	.driver_module = THIS_MODULE,
};

int att7022_probe(struct att7022_state *st, struct device *dev)
{
	int ret;

	/* Allocate the comms buffers */
	st->rx = kzalloc(sizeof(*st->rx)*ATT7022_MAX_RX, GFP_KERNEL);
	if (st->rx == NULL) {
		ret = -ENOMEM;
		goto error_free_st;
	}
	st->tx = kzalloc(sizeof(*st->tx)*ATT7022_MAX_TX, GFP_KERNEL);
	if (st->tx == NULL) {
		ret = -ENOMEM;
		goto error_free_rx;
	}
	mutex_init(&st->buf_lock);
	/* setup the industrialio driver allocated elements */
	st->indio_dev = iio_allocate_device(0);
	if (st->indio_dev == NULL) {
		ret = -ENOMEM;
		goto error_free_tx;
	}

	st->indio_dev->dev.parent = dev;
	st->indio_dev->info = &att7022_info;
	st->indio_dev->dev_data = (void *)(st);
	st->indio_dev->modes = INDIO_DIRECT_MODE;

	ret = iio_device_register(st->indio_dev);
	if (ret)
		goto error_free_dev;

	/* Get the device into a sane initial state */
	ret = att7022_initial_setup(st);
	if (ret)
		goto error_unreg_dev;

	return 0;

error_unreg_dev:
	iio_device_unregister(st->indio_dev);
error_free_dev:
	iio_free_device(st->indio_dev);
error_free_tx:
	kfree(st->tx);
error_free_rx:
	kfree(st->rx);
error_free_st:
	kfree(st);

	return ret;
}
EXPORT_SYMBOL(att7022_probe);

int att7022_remove(struct att7022_state *st)
{
	struct iio_dev *indio_dev = st->indio_dev;

	iio_device_unregister(indio_dev);
	kfree(st->tx);
	kfree(st->rx);
	kfree(st);

	return 0;
}
EXPORT_SYMBOL(att7022_remove);

MODULE_AUTHOR("Barry Song <21cnbao@gmail.com>");
MODULE_DESCRIPTION("Analog Devices ATT7022/58/68/78 Polyphase Energy Meter");
MODULE_LICENSE("GPL v2");
