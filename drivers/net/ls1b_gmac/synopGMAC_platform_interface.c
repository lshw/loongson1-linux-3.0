#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include "synopGMAC_Host.h"
#include "synopGMAC_network_interface.h"

static int gmac_probe(struct platform_device *pdev)
{
	synopGMACPciNetworkAdapter *synopGMACadapter;
	u8 *synopGMACMappedAddr;
	u32 synopGMACMappedAddrSize;
	u32 irq;
	struct resource *res;

	irq = platform_get_irq(pdev, 0);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(irq < 0 || res == NULL){
		printk("get resource from platform error.\n");
		return -EBUSY;
	}

	//synopGMACMappedAddr=ioremap_cachable(res->start,synopGMACMappedAddrSize);
	synopGMACMappedAddrSize=res->end-res->start+1;
	synopGMACMappedAddr=ioremap_nocache(res->start,synopGMACMappedAddrSize);
	if(!synopGMACMappedAddr){
		printk("%s: ioremap fault.\n", __FUNCTION__);
		return -ENOMEM;
	}

	printk("<0>gmac%d is init,Map address is %p\n",pdev->id,synopGMACMappedAddr);

	if((synopGMACadapter=synopGMAC_init_network_interface(&pdev->dev,synopGMACMappedAddr,synopGMACMappedAddrSize,irq)) < 0){
		printk("init network interface fault.\n");
		return -EFAULT;
	}
	
	platform_set_drvdata(pdev,synopGMACadapter);

	return 0;
}

static int gmac_remove(struct platform_device *pdev)
{
	synopGMACPciNetworkAdapter *adapter=platform_get_drvdata(pdev);
	struct net_device *netdev=adapter->synopGMACnetdev;

    	/* Do the reverse of what probe does */ 
    	if (adapter->synopGMACMappedAddr)
    	{
      		TR0 ("Releaseing synopGMACMappedAddr 0x%p whose size is %d\n",  adapter->synopGMACMappedAddr, adapter->synopGMACMappedAddrSize);
      
      		/*release the memory region which we locked using request_mem_region */ 
      		release_mem_region ((resource_size_t) adapter->synopGMACMappedAddr, adapter->synopGMACMappedAddrSize);
    	}
  	TR0 ("Unmapping synopGMACMappedAddr =0x%p\n",  adapter->synopGMACMappedAddr);
  	iounmap (adapter->synopGMACMappedAddr);
	if(netdev) {
		unregister_netdev(netdev);
		free_netdev(netdev);
		platform_set_drvdata(pdev, NULL);
	}
	
	return 0;
}

static struct platform_driver ls1b_gmac_driver = {
	.probe			= gmac_probe,
	.remove			= gmac_remove,
	.driver		= {
		.name	= "ls1b-gmac",
		.owner	= THIS_MODULE,
	},
};


static int __init ls1b_gmac_init(void)
{
	return	platform_driver_register(&ls1b_gmac_driver);
}


static void __exit ls1b_gmac_exit(void)
{
	platform_driver_unregister(&ls1b_gmac_driver);
}

module_exit(ls1b_gmac_exit);
module_init(ls1b_gmac_init);
