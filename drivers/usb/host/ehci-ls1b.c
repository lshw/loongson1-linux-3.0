/*
 * (C) Copyright loongson 2009
 */

#if 0
#include <linux/kernel.h>		//lxy
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#endif


#include <linux/platform_device.h>
#include "ehci-ls1b.h"
#include <linux/fsl_devices.h>


/* FIXME: Power Managment is un-ported so temporarily disable it */
#undef CONFIG_PM

/* PCI-based HCs are common, but plenty of non-PCI HCs are used too */

/* configure so an HC device and id are always provided */
/* always called with process context; sleeping is OK */

/**
 * usb_hcd_ls1b_probe - initialize FSL-based HCDs
 * @drvier: Driver to be used for this HCD
 * @pdev: USB Host Controller being probed
 * Context: !in_interrupt()
 *
 * Allocates basic resources for this USB host controller.
 *
 */

#ifdef CONFIG_USB_OTG
static int ehci_start_port_reset(struct usb_hcd *hcd, unsigned port)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	u32 status;

	if (!port)
		return -EINVAL;

	port--;

	/* start port reset before HNP protocol time out */
	status = readl(&ehci->regs->port_status[port]);
	if (!(status & PORT_CONNECT))
		return -ENODEV;

	/* khubd will finish the reset later */
	if (ehci_is_TDI(ehci)) {
		writel(PORT_RESET |
		       (status & ~(PORT_CSC | PORT_PEC | PORT_OCC)),
		       &ehci->regs->port_status[port]);
	} else {
		writel(PORT_RESET, &ehci->regs->port_status[port]);
	}

	return 0;
}
#else
#define ehci_start_port_reset	NULL
#endif /* CONFIG_USB_OTG */


int usb_hcd_ls1b_probe(const struct hc_driver *driver,
		      struct platform_device *pdev)
{
	struct ls1b_usbh_data *pdata;
	struct usb_hcd *hcd;
	struct resource *res;
	int irq;
	int retval;

	pr_debug("initializing loongson southbridge ehci USB Controller\n");

	/* Need platform data for setup */
	pdata = (struct ls1b_usbh_data *)pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev,
			"No platform data for %s.\n", dev_name(&pdev->dev));
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"Found HC with no IRQ. Check %s setup!\n",
			dev_name(&pdev->dev));
		return -ENODEV;
	}
	irq = res->start;

	hcd = usb_create_hcd(driver, &pdev->dev, dev_name(&pdev->dev));
	if (!hcd) {
		retval = -ENOMEM;
		goto err1;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"Found HC with no register addr. Check %s setup!\n",
			dev_name(&pdev->dev));
		retval = -ENODEV;
		goto err2;
	}
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = res->end - res->start + 1;
	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len,
				driver->description)) {
		dev_dbg(&pdev->dev, "controller already in use\n");
		retval = -EBUSY;
		goto err2;
	}
	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);

	if (hcd->regs == NULL) {
		dev_dbg(&pdev->dev, "error mapping memory\n");
		retval = -EFAULT;
		goto err3;
	}

#if 0
	/* Enable USB controller */
	temp = in_be32(hcd->regs + 0x500);
	out_be32(hcd->regs + 0x500, temp | 0x4);

	/* Set to Host mode */
	temp = in_le32(hcd->regs + 0x1a8);
	out_le32(hcd->regs + 0x1a8, temp | 0x3);
#endif

	retval = usb_add_hcd(hcd, irq, IRQF_DISABLED | IRQF_SHARED);
	if (retval != 0)
		goto err4;
	return retval;

      err4:
	iounmap(hcd->regs);
      err3:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
      err2:
	usb_put_hcd(hcd);
      err1:
	dev_err(&pdev->dev, "init %s fail, %d\n", dev_name(&pdev->dev), retval);
	return retval;
}

/* may be called without controller electrically present */
/* may be called with controller, bus, and devices active */

/**
 * usb_hcd_ls1b_remove - shutdown processing for FSL-based HCDs
 * @dev: USB Host Controller being removed
 * Context: !in_interrupt()
 *
 * Reverses the effect of usb_hcd_ls1b_probe().
 *
 */
void usb_hcd_ls1b_remove(struct usb_hcd *hcd, struct platform_device *pdev)
{
	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
}

/* called after powerup, by probe or system-pm "wakeup" */
static int ehci_ls1b_reinit(struct ehci_hcd *ehci)
{
	ehci_port_power(ehci, 0);
	return 0;
}

/* called during probe() after chip reset completes */
static int ehci_ls1b_setup(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int retval;

#if 0
	struct fsl_usb2_platform_data *pdata;		//lxy

	pdata = hcd->self.controller->platform_data;
	ehci->big_endian_desc = pdata->big_endian_desc;
	ehci->big_endian_mmio = pdata->big_endian_mmio;
#endif

	/* EHCI registers start at offset 0*/
	ehci->caps = hcd->regs + 0x0;
	ehci->regs = hcd->regs + 0x0 +
	    HC_LENGTH(ehci, ehci_readl(ehci, &ehci->caps->hc_capbase));
	dbg_hcs_params(ehci, "reset");
	dbg_hcc_params(ehci, "reset");

	/* cache this readonly data; minimize chip reads */
	ehci->hcs_params = ehci_readl(ehci, &ehci->caps->hcs_params);

	retval = ehci_halt(ehci);
	if (retval)
		return retval;

	/* data structure init */
	retval = ehci_init(hcd);
	if (retval)
		return retval;

//	ehci->is_tdi_rh_tt = 1;
	hcd->has_tt = 1;


	ehci->sbrn = 0x20;

	ehci_reset(ehci);

	retval = ehci_ls1b_reinit(ehci);
	return retval;
}

static const struct hc_driver ehci_ls1b_hc_driver = {
	.description = hcd_name,
	.product_desc = "loongson2f southbridge EHCI Host Controller",
	.hcd_priv_size = sizeof(struct ehci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq = ehci_irq,
	.flags = HCD_USB2 | HCD_MEMORY,		//lxy

	/*
	 * basic lifecycle operations
	 */
	.reset = ehci_ls1b_setup,
	.start = ehci_run,
#ifdef	CONFIG_PM
	.suspend = ehci_bus_suspend,
	.resume = ehci_bus_resume,
#endif
	.stop = ehci_stop,
	.shutdown = ehci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue = ehci_urb_enqueue,
	.urb_dequeue = ehci_urb_dequeue,
	.endpoint_disable = ehci_endpoint_disable,
	.endpoint_reset = ehci_endpoint_reset,		//lxy

	/*
	 * scheduling support
	 */
	.get_frame_number = ehci_get_frame,

	/*
	 * root hub support
	 */

#if 0
	.hub_status_data = ehci_hub_status_data,
	.hub_control = ehci_hub_control,
	.bus_suspend = ehci_bus_suspend,
	.bus_resume = ehci_bus_resume,
#endif

	.hub_status_data = ehci_hub_status_data,		//lxy
	.hub_control = ehci_hub_control,
	.bus_suspend = ehci_bus_suspend,
	.bus_resume = ehci_bus_resume,
	.start_port_reset = ehci_start_port_reset,
	.relinquish_port = ehci_relinquish_port,
	.port_handed_over = ehci_port_handed_over,

	.clear_tt_buffer_complete = ehci_clear_tt_buffer_complete,
};

static int ehci_ls1b_drv_probe(struct platform_device *pdev)
{
	if (usb_disabled())
		return -ENODEV;

	return usb_hcd_ls1b_probe(&ehci_ls1b_hc_driver, pdev);
}

static int ehci_ls1b_drv_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	usb_hcd_ls1b_remove(hcd, pdev);

	return 0;
}

MODULE_ALIAS("ls1b-ehci");

static struct platform_driver ehci_ls1b_driver = {
	.probe = ehci_ls1b_drv_probe,
	.remove = ehci_ls1b_drv_remove,
	.shutdown = usb_hcd_platform_shutdown,
	.driver = {
		   .name = "ls1b-ehci",
		   },
};

