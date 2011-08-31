/*
 * OHCI HCD (Host Controller Driver) for USB.
 *
 *  Copyright (C) 2009 loongson.
 *
 * This file is licenced under the GPL.
 */
#include <linux/platform_device.h>


static void sb2f_start_hc(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct ohci_regs __iomem *regs = hcd->regs;

	dev_dbg(&pdev->dev, "start\n");

	/*
	 * The USB host controller must remain in reset.
	 */
	writel(0, &regs->control);
}

static void sb2f_stop_hc(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct ohci_regs __iomem *regs = hcd->regs;

	dev_dbg(&pdev->dev, "stop\n");

	/*
	 * Put the USB host controller into reset.
	 */
	writel(0, &regs->control);

}


/*-------------------------------------------------------------------------*/

static int usb_hcd_sb2f_remove (struct usb_hcd *, struct platform_device *);

/* configure so an HC device and id are always provided */
/* always called with process context; sleeping is OK */


/**
 * usb_hcd_sb2f_probe - initialize AT91-based HCDs
 * Context: !in_interrupt()
 *
 * Allocates basic resources for this USB host controller, and
 * then invokes the start() method for the HCD associated with it
 * through the hotplug entry's driver_data.
 */
static int usb_hcd_sb2f_probe(const struct hc_driver *driver,
			struct platform_device *pdev)
{
	int retval;
	struct usb_hcd *hcd = NULL;

	if (pdev->num_resources != 2) {
		pr_debug("hcd probe: invalid num_resources");
		return -ENODEV;
	}

	if ((pdev->resource[0].flags != IORESOURCE_MEM)
			|| (pdev->resource[1].flags != IORESOURCE_IRQ)) {
		pr_debug("hcd probe: invalid resource type\n");
		return -ENODEV;
	}

	hcd = usb_create_hcd(driver, &pdev->dev, "sb2f");
	if (!hcd)
		return -ENOMEM;
	hcd->rsrc_start = pdev->resource[0].start;
	hcd->rsrc_len = pdev->resource[0].end - pdev->resource[0].start + 1;

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len, hcd_name)) {
		pr_debug("request_mem_region failed\n");
		retval = -EBUSY;
		goto err1;
	}

	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs) {
		pr_debug("ioremap failed\n");
		retval = -EIO;
		goto err2;
	}

	printk ("lxy: -----1----->\n");
	sb2f_start_hc(pdev);
	printk ("lxy: -----2----->\n");
	ohci_hcd_init(hcd_to_ohci(hcd));

	retval = usb_add_hcd(hcd, pdev->resource[1].start, IRQF_DISABLED);
	printk ("lxy: -----3----->\n");
	if (retval == 0)
		return retval;

	/* Error handling */
	sb2f_stop_hc(pdev);

	printk ("lxy: -----4----->\n");
	iounmap(hcd->regs);

 err2:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);

 err1:
	usb_put_hcd(hcd);
	return retval;
}


/* may be called with controller, bus, and devices active */

/**
 * usb_hcd_sb2f_remove - shutdown processing for AT91-based HCDs
 * @dev: USB Host Controller being removed
 * Context: !in_interrupt()
 *
 * Reverses the effect of usb_hcd_sb2f_probe(), first invoking
 * the HCD's stop() method.  It is always called from a thread
 * context, "rmmod" or something similar.
 *
 */
static int usb_hcd_sb2f_remove(struct usb_hcd *hcd,
				struct platform_device *pdev)
{
	usb_remove_hcd(hcd);
	sb2f_stop_hc(pdev);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);

	dev_set_drvdata(&pdev->dev, NULL);
	return 0;
}

/*-------------------------------------------------------------------------*/
struct sb2f_usbh_data {
    u8      ports;      /* number of ports on root hub */
    u8      vbus_pin[]; /* port power-control pin */
}; 

static int __devinit
ohci_sb2f_start (struct usb_hcd *hcd)
{
	struct sb2f_usbh_data	*board = hcd->self.controller->platform_data;
	struct ohci_hcd		*ohci = hcd_to_ohci (hcd);
	int			ret;

	if ((ret = ohci_init(ohci)) < 0)
		return ret;

	ohci->num_ports = board->ports;

	if ((ret = ohci_run(ohci)) < 0) {
		err("can't start %s", hcd->self.bus_name);
		ohci_stop(hcd);
		return ret;
	}
	return 0;
}

/*-------------------------------------------------------------------------*/

static const struct hc_driver ohci_sb2f_hc_driver = {
	.description =		hcd_name,
	.product_desc =		"loongson southbridge OHCI",
	.hcd_priv_size =	sizeof(struct ohci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq =			ohci_irq,
	.flags =		HCD_USB11 | HCD_MEMORY,

	/*
	 * basic lifecycle operations
	 */
	.start =		ohci_sb2f_start,
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
//	.hub_irq_enable =	ohci_rhsc_enable,
#ifdef CONFIG_PM
	.bus_suspend =		ohci_bus_suspend,
	.bus_resume =		ohci_bus_resume,
#endif
	.start_port_reset =	ohci_start_port_reset,
};

/*-------------------------------------------------------------------------*/

static int ohci_hcd_sb2f_drv_probe(struct platform_device *pdev)
{
	device_init_wakeup(&pdev->dev, 1);
	return usb_hcd_sb2f_probe(&ohci_sb2f_hc_driver, pdev);
}

static int ohci_hcd_sb2f_drv_remove(struct platform_device *pdev)
{
	device_init_wakeup(&pdev->dev, 0);
	return usb_hcd_sb2f_remove(platform_get_drvdata(pdev), pdev);
}

#ifdef CONFIG_PM

static int
ohci_hcd_sb2f_drv_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	struct usb_hcd	*hcd = platform_get_drvdata(pdev);
	struct ohci_hcd	*ohci = hcd_to_ohci(hcd);

	if (device_may_wakeup(&pdev->dev))
		enable_irq_wake(hcd->irq);

	return 0;
}

static int ohci_hcd_sb2f_drv_resume(struct platform_device *pdev)
{
	struct usb_hcd	*hcd = platform_get_drvdata(pdev);

	if (device_may_wakeup(&pdev->dev))
		disable_irq_wake(hcd->irq);

	return 0;
}
#else
#define ohci_hcd_sb2f_drv_suspend NULL
#define ohci_hcd_sb2f_drv_resume  NULL
#endif

MODULE_ALIAS("sb2f_ohci");

static struct platform_driver ohci_hcd_sb2f_driver = {
	.probe		= ohci_hcd_sb2f_drv_probe,
	.remove		= ohci_hcd_sb2f_drv_remove,
	.shutdown	= usb_hcd_platform_shutdown,
	.suspend	= ohci_hcd_sb2f_drv_suspend,
	.resume		= ohci_hcd_sb2f_drv_resume,
	.driver		= {
		.name	= "sb2f-ohci",
		.owner	= THIS_MODULE,
	},
};

