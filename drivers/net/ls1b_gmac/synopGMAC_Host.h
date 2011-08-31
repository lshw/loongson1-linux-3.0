#ifndef SYNOP_GMAC_HOST_H
#define SYNOP_GMAC_HOST_H


#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/mii.h>

#include "synopGMAC_plat.h"
#include "synopGMAC_pci_bus_interface.h"
#include "synopGMAC_Dev.h"

typedef struct synopGMACAdapterStruct{

/*Device Dependent Data structur*/
synopGMACdevice * synopGMACdev;
/*Os/Platform Dependent Data Structures*/
struct device * dev;
struct net_device *synopGMACnetdev;
struct net_device_stats synopGMACNetStats;
struct mii_if_info mii;
u32 synopGMACPciState[16];
u8* synopGMACMappedAddr;
u32 synopGMACMappedAddrSize;
int irq;

} synopGMACPciNetworkAdapter;


#endif
