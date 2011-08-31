/**\file
 *  The top most file which makes use of synopsys GMAC driver code.
 *
 *  This file can be treated as the example code for writing a application driver
 *  for synopsys GMAC device using the driver provided by Synopsys.
 *  This exmple is for Linux 2.6.xx kernel 
 *  - Uses 32 bit 33MHz PCI Interface as the host bus interface
 *  - Uses Linux network driver and the TCP/IP stack framework
 *  - Uses the Device Specific Synopsys GMAC Kernel APIs
 *  \internal
 * ---------------------------REVISION HISTORY--------------------------------
 * Synopsys 			01/Aug/2007			Created
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/device.h>

#include <linux/pci.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include "synopGMAC_Host.h"
#include "synopGMAC_banner.h"
#include "synopGMAC_plat.h"
#include "synopGMAC_pci_bus_interface.h"
#include "synopGMAC_network_interface.h"
#include "synopGMAC_Dev.h"

/****************************************************/


/* Global declarations: these are required to handle 
   Os and Platform dependent functionalities        */

/*GMAC IP Base address and Size   */

/*global adapter gmacdev pcidev and netdev pointers */

/***************************************************/

int __init SynopGMAC_Host_Interface_init(void)
{

        int retval;

	TR0("**********************************************************\n");
	TR0("* Driver    :%s\n",synopGMAC_driver_string);
	TR0("* Version   :%s\n",synopGMAC_driver_version);
	TR0("* Copyright :%s\n",synopGMAC_copyright);
	TR0("**********************************************************\n");

        TR0("Initializing synopsys GMAC interfaces ..\n") ;
        /* Initialize the bus interface for the hostcontroller E.g PCI in our case */
        if ((retval = synopGMAC_init_pci_bus_interface())) {
	        TR0("Could not initiliase the bus interface. Is PCI device connected ?\n");
                return retval;
        }
	
	
      return 0 ;
}

void __exit SynopGMAC_Host_Interface_exit(void)
{

        TR0("Exiting synopsys GMAC interfaces ..\n") ;
}

module_init(SynopGMAC_Host_Interface_init);
module_exit(SynopGMAC_Host_Interface_exit);

MODULE_AUTHOR("Synopsys India");
MODULE_LICENSE("GPL/BSD");
MODULE_DESCRIPTION("SYNOPSYS GMAC NETWORK DRIVER WITH PCI INTERFACE");

