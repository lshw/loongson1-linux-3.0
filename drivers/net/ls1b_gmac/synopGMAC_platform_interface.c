#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include "synopGMAC_Host.h"
#include "synopGMAC_network_interface.h"

static int init_one_platform(struct platform_device *pdev)
{
  synopGMACPciNetworkAdapter *synopGMACadapter;
  u8 *synopGMACMappedAddr;
  u32 synopGMACMappedAddrSize;
  u32 irq;
  struct resource *res;
  

	irq = platform_get_irq(pdev, 0);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    synopGMACMappedAddrSize=res->end-res->start+1;
    //synopGMACMappedAddr=ioremap_cachable(res->start,synopGMACMappedAddrSize);
    synopGMACMappedAddr=ioremap_nocache(res->start,synopGMACMappedAddrSize);
printk("<0>init_one_platform %p\n",synopGMACMappedAddr);

	if((synopGMACadapter=synopGMAC_init_network_interface(&pdev->dev,synopGMACMappedAddr,synopGMACMappedAddrSize,irq)))
	platform_set_drvdata(pdev,synopGMACadapter);
	return 0;
}

static int remove_one_platform(struct platform_device *pdev)
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

static struct platform_driver ls1f_gmac_driver = {
	.probe			= init_one_platform,
	.remove			= remove_one_platform,
	.driver		= {
		.name	= "ls1b-gmac",
		.owner	= THIS_MODULE,
	},
};

static int __init ls1f_gmac_init(void)
{
	platform_driver_register(&ls1f_gmac_driver);
	return 0;
}


module_init(ls1f_gmac_init);

static void __exit ls1f_gmac_exit(void)
{
	platform_driver_unregister(&ls1f_gmac_driver);
}

module_exit(ls1f_gmac_exit);
