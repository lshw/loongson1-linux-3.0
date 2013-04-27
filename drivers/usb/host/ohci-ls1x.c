/*
 * OHCI HCD (Host Controller Driver) for USB.
 *
 *  Copyright (C) 2009 loongson.
 *
 * This file is licenced under the GPL.
 */
#include <linux/platform_device.h>

extern int usb_disabled(void);

static void ls1x_start_ohc(void)
{
#if defined(CONFIG_LS1A_MACH)
	*(volatile int *)0xbfd00420 &= ~0x200000;/* enable USB */
	*(volatile int *)0xbff10204 = 0;
	mdelay(105);
	*(volatile int *)0xbff10204 |= 0x40000000;/* ls1a usb reset stop */
#elif defined(CONFIG_LS1B_MACH)
	*(volatile int *)0xbfd00424 &= ~0x800;/* enable USB */
	*(volatile int *)0xbfd00424 &= ~0x80000000;/* ls1g usb reset */
	mdelay(105);
	*(volatile int *)0xbfd00424 |= 0x80000000;/* ls1g usb reset stop */
#endif	//CONFIG_LS1A_MACH
}

static void ls1x_stop_ohc(void)
{
#if 0
#ifndef CONFIG_USB_EHCI_HCD_LS1B
#ifdef CONFIG_LS1A_MACH
	*(volatile int *)0xbfd00420 |= 0x200000;/* disable USB */
	*(volatile int *)0xbff10204 &= ~0x40000000;/* ls1a usb reset */
#elif CONFIG_LS1B_MACH
	*(volatile int *)0xbfd00424 |= 0x800;/* disable USB */
	*(volatile int *)0xbfd00424 &= ~0x80000000;/* ls1g usb reset */
#endif
#endif
#endif
}

static int __devinit ohci_ls1x_start(struct usb_hcd *hcd)
{
	struct ohci_hcd	*ohci = hcd_to_ohci(hcd);
	int ret;

	ohci_dbg(ohci, "ohci_ls1x_start, ohci:%p", ohci);

	if ((ret = ohci_init(ohci)) < 0)
		return ret;

	if ((ret = ohci_run(ohci)) < 0) {
		err ("can't start %s", hcd->self.bus_name);
		ohci_stop(hcd);
		return ret;
	}

	return 0;
}

static const struct hc_driver ohci_ls1x_hc_driver = {
	.description =		hcd_name,
	.product_desc =		"loongson1 OHCI",
	.hcd_priv_size =	sizeof(struct ohci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq =			ohci_irq,
	.flags =		HCD_USB11 | HCD_MEMORY,

	/*
	 * basic lifecycle operations
	 */
	.start =		ohci_ls1x_start,
	.stop =			ohci_stop,
	.shutdown =		ohci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		ohci_urb_enqueue,
	.urb_dequeue =		ohci_urb_dequeue,
	.endpoint_disable =	ohci_endpoint_disable,

	/*
	 * scheduling support
	 */
	.get_frame_number =	ohci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data =	ohci_hub_status_data,
	.hub_control =		ohci_hub_control,
#ifdef	CONFIG_PM
	.bus_suspend =		ohci_bus_suspend,
	.bus_resume =		ohci_bus_resume,
#endif
	.start_port_reset =	ohci_start_port_reset,
};

static int ohci_hcd_ls1x_drv_probe(struct platform_device *pdev)
{
	int ret;
	struct usb_hcd *hcd;

	if (usb_disabled())
		return -ENODEV;

	if (pdev->resource[1].flags != IORESOURCE_IRQ) {
		pr_debug("resource[1] is not IORESOURCE_IRQ\n");
		return -ENOMEM;
	}

	hcd = usb_create_hcd(&ohci_ls1x_hc_driver, &pdev->dev, "ls1x");
	if (!hcd)
		return -ENOMEM;

	hcd->rsrc_start = pdev->resource[0].start;
	hcd->rsrc_len = pdev->resource[0].end - pdev->resource[0].start + 1;

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len, hcd_name)) {
		pr_debug("request_mem_region failed\n");
		ret = -EBUSY;
		goto err1;
	}

	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs) {
		pr_debug("ioremap failed\n");
		ret = -ENOMEM;
		goto err2;
	}

//	ls1x_start_ohc();
	ohci_hcd_init(hcd_to_ohci(hcd));

	ret = usb_add_hcd(hcd, pdev->resource[1].start,
			  IRQF_DISABLED | IRQF_SHARED);
	if (ret == 0) {
		platform_set_drvdata(pdev, hcd);
		return ret;
	}

	ls1x_stop_ohc();
	iounmap(hcd->regs);
err2:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
err1:
	usb_put_hcd(hcd);
	return ret;
}

static int ohci_hcd_ls1x_drv_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	usb_remove_hcd(hcd);
	ls1x_stop_ohc();
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int ohci_hcd_ls1x_drv_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct ohci_hcd	*ohci = hcd_to_ohci(hcd);
	unsigned long flags;
	int rc;

	rc = 0;

	/* Root hub was already suspended. Disable irq emission and
	 * mark HW unaccessible, bail out if RH has been resumed. Use
	 * the spinlock to properly synchronize with possible pending
	 * RH suspend or resume activity.
	 *
	 * This is still racy as hcd->state is manipulated outside of
	 * any locks =P But that will be a different fix.
	 */
	spin_lock_irqsave(&ohci->lock, flags);
	if (hcd->state != HC_STATE_SUSPENDED) {
		rc = -EINVAL;
		goto bail;
	}
	ohci_writel(ohci, OHCI_INTR_MIE, &ohci->regs->intrdisable);
	(void)ohci_readl(ohci, &ohci->regs->intrdisable);

	clear_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);

	ls1x_stop_ohc();
bail:
	spin_unlock_irqrestore(&ohci->lock, flags);

	return rc;
}

static int ohci_hcd_ls1x_drv_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);

	ls1x_start_ohc();

	set_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);
	ohci_finish_controller_resume(hcd);

	return 0;
}

static const struct dev_pm_ops ls1x_ohci_pmops = {
	.suspend	= ohci_hcd_ls1x_drv_suspend,
	.resume		= ohci_hcd_ls1x_drv_resume,
};

#define LS1X_OHCI_PMOPS &ls1x_ohci_pmops

#else
#define LS1X_OHCI_PMOPS NULL
#endif

static struct platform_driver ohci_hcd_ls1x_driver = {
	.probe		= ohci_hcd_ls1x_drv_probe,
	.remove		= ohci_hcd_ls1x_drv_remove,
	.shutdown	= usb_hcd_platform_shutdown,
	.driver		= {
		.name	= "ls1x-ohci",
		.owner	= THIS_MODULE,
		.pm	= LS1X_OHCI_PMOPS,
	},
};

MODULE_ALIAS("platform:ls1x-ohci");
