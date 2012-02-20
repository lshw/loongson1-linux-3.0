/** \file
 * This is the network dependent layer to handle network related functionality.
 * This file is tightly coupled to neworking frame work of linux 2.6.xx kernel.
 * The functionality carried out in this file should be treated as an example only
 * if the underlying operating system is not Linux. 
 * 
 * \note Many of the functions other than the device specific functions
 *  changes for operating system other than Linux 2.6.xx
 * \internal 
 *-----------------------------REVISION HISTORY-----------------------------------
 * Synopsys			01/Aug/2007				Created
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>


#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/ethtool.h>
#include <linux/string.h>

#include "synopGMAC_Host.h"
#include "synopGMAC_plat.h"
#include "synopGMAC_network_interface.h"
#include "synopGMAC_Dev.h"

///#define USE_CHAIN_MODE


#define BUS_SIZE_ALIGN(x) ((x+15)&~15)

#define ETHER_MAX_LEN	1518

#define IOCTL_READ_REGISTER  SIOCDEVPRIVATE+1
#define IOCTL_WRITE_REGISTER SIOCDEVPRIVATE+2
#define IOCTL_READ_IPSTRUCT  SIOCDEVPRIVATE+3
#define IOCTL_READ_RXDESC    SIOCDEVPRIVATE+4
#define IOCTL_READ_TXDESC    SIOCDEVPRIVATE+5
#define IOCTL_POWER_DOWN     SIOCDEVPRIVATE+6

//static struct timer_list synopGMAC_cable_unplug_timer;
static u32 GMAC_Power_down; // This global variable is used to indicate the ISR whether the interrupts occured in the process of powering down the mac or not


/*These are the global pointers for their respecive structures*/


/*these are the global data for base address and its size*/
u32 synop_pci_using_dac;

/*Sample Wake-up frame filter configurations*/

u32 synopGMAC_wakeup_filter_config0[] = {
					0x00000000,	// For Filter0 CRC is not computed may be it is 0x0000
					0x00000000,	// For Filter1 CRC is not computed may be it is 0x0000
					0x00000000,	// For Filter2 CRC is not computed may be it is 0x0000
					0x5F5F5F5F,     // For Filter3 CRC is based on 0,1,2,3,4,6,8,9,10,11,12,14,16,17,18,19,20,22,24,25,26,27,28,30 bytes from offset
					0x09000000,     // Filter 0,1,2 are disabled, Filter3 is enabled and filtering applies to only multicast packets
					0x1C000000,     // Filter 0,1,2 (no significance), filter 3 offset is 28 bytes from start of Destination MAC address 
					0x00000000,     // No significance of CRC for Filter0 and Filter1
					0xBDCC0000      // No significance of CRC for Filter2, Filter3 CRC is 0xBDCC
					};
u32 synopGMAC_wakeup_filter_config1[] = {
					0x00000000,	// For Filter0 CRC is not computed may be it is 0x0000
					0x00000000,	// For Filter1 CRC is not computed may be it is 0x0000
					0x7A7A7A7A,	// For Filter2 CRC is based on 1,3,4,5,6,9,11,12,13,14,17,19,20,21,25,27,28,29,30 bytes from offset
					0x00000000,     // For Filter3 CRC is not computed may be it is 0x0000
					0x00010000,     // Filter 0,1,3 are disabled, Filter2 is enabled and filtering applies to only unicast packets
					0x00100000,     // Filter 0,1,3 (no significance), filter 2 offset is 16 bytes from start of Destination MAC address 
					0x00000000,     // No significance of CRC for Filter0 and Filter1
					0x0000A0FE      // No significance of CRC for Filter3, Filter2 CRC is 0xA0FE
					};
u32 synopGMAC_wakeup_filter_config2[] = {
					0x00000000,	// For Filter0 CRC is not computed may be it is 0x0000
					0x000000FF,	// For Filter1 CRC is computed on 0,1,2,3,4,5,6,7 bytes from offset
					0x00000000,	// For Filter2 CRC is not computed may be it is 0x0000
					0x00000000,     // For Filter3 CRC is not computed may be it is 0x0000
					0x00000100,     // Filter 0,2,3 are disabled, Filter 1 is enabled and filtering applies to only unicast packets
					0x0000DF00,     // Filter 0,2,3 (no significance), filter 1 offset is 223 bytes from start of Destination MAC address 
					0xDB9E0000,     // No significance of CRC for Filter0, Filter1 CRC is 0xDB9E
					0x00000000      // No significance of CRC for Filter2 and Filter3 
					};

/*
The synopGMAC_wakeup_filter_config3[] is a sample configuration for wake up filter. 
Filter1 is used here
Filter1 offset is programmed to 50 (0x32)
Filter1 mask is set to 0x000000FF, indicating First 8 bytes are used by the filter
Filter1 CRC= 0x7EED this is the CRC computed on data 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55

Refer accompanied software DWC_gmac_crc_example.c for CRC16 generation and how to use the same.
*/

u32 synopGMAC_wakeup_filter_config3[] = {
					0x00000000,	// For Filter0 CRC is not computed may be it is 0x0000
					0x000000FF,	// For Filter1 CRC is computed on 0,1,2,3,4,5,6,7 bytes from offset
					0x00000000,	// For Filter2 CRC is not computed may be it is 0x0000
					0x00000000,     // For Filter3 CRC is not computed may be it is 0x0000
					0x00000100,     // Filter 0,2,3 are disabled, Filter 1 is enabled and filtering applies to only unicast packets
					0x00003200,     // Filter 0,2,3 (no significance), filter 1 offset is 50 bytes from start of Destination MAC address 
					0x7eED0000,     // No significance of CRC for Filter0, Filter1 CRC is 0x7EED, 
					0x00000000      // No significance of CRC for Filter2 and Filter3 
					};
/**
 * Function used to detect the cable plugging and unplugging.
 * This function gets scheduled once in every second and polls
 * the PHY register for network cable plug/unplug. Once the 
 * connection is back the GMAC device is configured as per
 * new Duplex mode and Speed of the connection.
 * @param[in] u32 type but is not used currently. 
 * \return returns void.
 * \note This function is tightly coupled with Linux 2.6.xx.
 * \callgraph
 */

static u32 gmac_get_link(struct net_device *dev);
static void synopGMAC_linux_cable_unplug_function(synopGMACPciNetworkAdapter *adapter)
{
	s32 status;
	u16 data,data1;
	synopGMACdevice *gmacdev = adapter->synopGMACdev;
	struct ethtool_cmd cmd;

	init_timer(&adapter->synopGMAC_cable_unplug_timer); //lv
	adapter->synopGMAC_cable_unplug_timer.expires = CHECK_TIME + jiffies; //lv

	if(!gmac_get_link(adapter->synopGMACnetdev)){
		if(gmacdev->LinkState)
		TR0("No Link: %08x %08x\n",data,data1);
		gmacdev->DuplexMode = 0;
		gmacdev->Speed = 0;
		gmacdev->LoopBackMode = 0; 
		gmacdev->LinkState = 0; 
	}else{
		data = synopGMAC_check_phy_init(adapter);

		if(gmacdev->LinkState != data){
			gmacdev->LinkState = data;
			synopGMAC_mac_init(gmacdev);
			TR("Link UP data=%08x\n",data);
			TR("Link is up in %s mode\n",(gmacdev->DuplexMode == FULLDUPLEX) ? "FULL DUPLEX": "HALF DUPLEX");
		if(gmacdev->Speed == SPEED1000)	
			TR0("Link is with 1000M Speed \n");
		if(gmacdev->Speed == SPEED100)	
			TR0("Link is with 100M Speed \n");
		if(gmacdev->Speed == SPEED10)	
			TR0("Link is with 10M Speed \n");
		}
//	synopGMAC_intr_handler(0, adapter->synopGMACnetdev);
	}

	add_timer(&adapter->synopGMAC_cable_unplug_timer); //lv
	
}

s32 synopGMAC_check_phy_init (synopGMACPciNetworkAdapter *adapter) 
{	
	struct ethtool_cmd cmd;
	synopGMACdevice *gmacdev = adapter->synopGMACdev;

	if(!gmac_get_link(adapter->synopGMACnetdev)){

		gmacdev->DuplexMode = FULLDUPLEX;
		gmacdev->Speed      =   SPEED100;

		return 0;
	}else{
		gmac_get_settings(adapter->synopGMACnetdev,&cmd);

		gmacdev->DuplexMode = (cmd.duplex == DUPLEX_FULL)  ? FULLDUPLEX: HALFDUPLEX ;
		if(cmd.speed == SPEED_1000)
	        	gmacdev->Speed = SPEED1000;
		else if(cmd.speed == SPEED_100)
			gmacdev->Speed = SPEED100;
		else
			gmacdev->Speed = SPEED10;

	}

	return gmacdev->Speed | (gmacdev->DuplexMode << 4);
}

static void synopGMAC_linux_powerdown_mac(synopGMACdevice *gmacdev)
{
	TR0("Put the GMAC to power down mode..\n");
	// Disable the Dma engines in tx path
	GMAC_Power_down = 1;	// Let ISR know that Mac is going to be in the power down mode
	synopGMAC_disable_dma_tx(gmacdev);
	plat_delay(10000);		//allow any pending transmission to complete
	// Disable the Mac for both tx and rx
	synopGMAC_tx_disable(gmacdev);
	synopGMAC_rx_disable(gmacdev);
        plat_delay(10000); 		//Allow any pending buffer to be read by host
	//Disable the Dma in rx path
        synopGMAC_disable_dma_rx(gmacdev);

	//enable the power down mode
	//synopGMAC_pmt_unicast_enable(gmacdev);
	
	//prepare the gmac for magic packet reception and wake up frame reception
	synopGMAC_magic_packet_enable(gmacdev);
	synopGMAC_write_wakeup_frame_register(gmacdev, synopGMAC_wakeup_filter_config3);

	synopGMAC_wakeup_frame_enable(gmacdev);

	//gate the application and transmit clock inputs to the code. This is not done in this driver :).

	//enable the Mac for reception
	synopGMAC_rx_enable(gmacdev);

	//Enable the assertion of PMT interrupt
	synopGMAC_pmt_int_enable(gmacdev);
	//enter the power down mode
	synopGMAC_power_down_enable(gmacdev);
	return;
}

static void synopGMAC_linux_powerup_mac(synopGMACdevice *gmacdev)
{
	GMAC_Power_down = 0;	// Let ISR know that MAC is out of power down now
	if( synopGMAC_is_magic_packet_received(gmacdev))
		TR("GMAC wokeup due to Magic Pkt Received\n");
	if(synopGMAC_is_wakeup_frame_received(gmacdev))
		TR("GMAC wokeup due to Wakeup Frame Received\n");
	//Disable the assertion of PMT interrupt
	synopGMAC_pmt_int_disable(gmacdev);
	//Enable the mac and Dma rx and tx paths
	synopGMAC_rx_enable(gmacdev);
       	synopGMAC_enable_dma_rx(gmacdev);

	synopGMAC_tx_enable(gmacdev);
	synopGMAC_enable_dma_tx(gmacdev);
	return;
}
/**
  * This sets up the transmit Descriptor queue in ring or chain mode.
  * This function is tightly coupled to the platform and operating system
  * Device is interested only after the descriptors are setup. Therefore this function
  * is not included in the device driver API. This function should be treated as an
  * example code to design the descriptor structures for ring mode or chain mode.
  * This function depends on the dev structure for allocation consistent dma-able memory in case of linux.
  * This limitation is due to the fact that linux uses pci structure to allocate a dmable memory
  *	- Allocates the memory for the descriptors.
  *	- Initialize the Busy and Next descriptors indices to 0(Indicating first descriptor).
  *	- Initialize the Busy and Next descriptors to first descriptor address.
  * 	- Initialize the last descriptor with the endof ring in case of ring mode.
  *	- Initialize the descriptors in chain mode.
  * @param[in] pointer to synopGMACdevice.
  * @param[in] pointer to pci_device structure.
  * @param[in] number of descriptor expected in tx descriptor queue.
  * @param[in] whether descriptors to be created in RING mode or CHAIN mode.
  * \return 0 upon success. Error code upon failure.
  * \note This function fails if allocation fails for required number of descriptors in Ring mode, but in chain mode
  *  function returns -ESYNOPGMACNOMEM in the process of descriptor chain creation. once returned from this function
  *  user should for gmacdev->TxDescCount to see how many descriptors are there in the chain. Should continue further
  *  only if the number of descriptors in the chain meets the requirements  
  */

s32 synopGMAC_setup_tx_desc_queue(synopGMACdevice * gmacdev,struct device * dev,u32 no_of_desc, u32 desc_mode)
{
s32 i;

DmaDesc *first_desc = NULL;
DmaDesc *second_desc = NULL;
dma_addr_t dma_addr;
gmacdev->TxDescCount = 0;

if(desc_mode == RINGMODE){
	TR("Total size of memory required for Tx Descriptors in Ring Mode = 0x%08lx\n",((sizeof(DmaDesc) * no_of_desc)));
	first_desc = plat_alloc_consistent_dmaable_memory (dev, sizeof(DmaDesc) * no_of_desc,&dma_addr);
	if(first_desc == NULL){
		TR("Error in Tx Descriptors memory allocation\n");
		return -ESYNOPGMACNOMEM;
	}
	gmacdev->TxDescCount = no_of_desc;
	gmacdev->TxDesc      = first_desc;
	gmacdev->TxDescDma   = dma_addr;
	
	for(i =0; i < gmacdev -> TxDescCount; i++){
		synopGMAC_tx_desc_init_ring(gmacdev->TxDesc + i, i == gmacdev->TxDescCount-1);
//		TR("%02d %p \n",i, (gmacdev->TxDesc + i) );
	}

}
else{
//Allocate the head descriptor
	first_desc = plat_alloc_consistent_dmaable_memory (dev, sizeof(DmaDesc),&dma_addr);
	if(first_desc == NULL){
		TR("Error in Tx Descriptor Memory allocation in Ring mode\n");
		return -ESYNOPGMACNOMEM;
	}
	gmacdev->TxDesc       = first_desc;
	gmacdev->TxDescDma    = dma_addr;

	TR("Tx===================================================================Tx\n");
	first_desc->buffer2   = gmacdev->TxDescDma;
 	first_desc->data2     = (u64)gmacdev->TxDesc;

	gmacdev->TxDescCount = 1;
	
	for(i =0; i <(no_of_desc-1); i++){
		second_desc = plat_alloc_consistent_dmaable_memory(dev, sizeof(DmaDesc),&dma_addr);
		if(second_desc == NULL){	
			TR("Error in Tx Descriptor Memory allocation in Chain mode\n");
			return -ESYNOPGMACNOMEM;
		}
		first_desc->buffer2  = dma_addr;
		first_desc->data2    = (u64)second_desc;
		
		second_desc->buffer2 = gmacdev->TxDescDma;
		second_desc->data2   = (u64)gmacdev->TxDesc;

	        synopGMAC_tx_desc_init_chain(first_desc);
//		TR("%02d %p %08x %08x %08x %08x %lx %lx \n",gmacdev->TxDescCount, first_desc, first_desc->status, first_desc->length, first_desc->buffer1,first_desc->buffer2, first_desc->data1, first_desc->data2);
		gmacdev->TxDescCount += 1;
		first_desc = second_desc;
	}
		
		synopGMAC_tx_desc_init_chain(first_desc);
//		TR("%02d %p %08x %08x %08x %08x %lx %lx \n",gmacdev->TxDescCount, first_desc, first_desc->status, first_desc->length, first_desc->buffer1,first_desc->buffer2, first_desc->data1, first_desc->data2);
	TR("Tx===================================================================Tx\n");
}

	gmacdev->TxNext = 0;
	gmacdev->TxBusy = 0;
	gmacdev->TxNextDesc = gmacdev->TxDesc;
	gmacdev->TxBusyDesc = gmacdev->TxDesc;
	gmacdev->BusyTxDesc  = 0; 

	return -ESYNOPGMACNOERR;
}


/**
  * This sets up the receive Descriptor queue in ring or chain mode.
  * This function is tightly coupled to the platform and operating system
  * Device is interested only after the descriptors are setup. Therefore this function
  * is not included in the device driver API. This function should be treated as an
  * example code to design the descriptor structures in ring mode or chain mode.
  * This function depends on the dev structure for allocation of consistent dma-able memory in case of linux.
  * This limitation is due to the fact that linux uses pci structure to allocate a dmable memory
  *	- Allocates the memory for the descriptors.
  *	- Initialize the Busy and Next descriptors indices to 0(Indicating first descriptor).
  *	- Initialize the Busy and Next descriptors to first descriptor address.
  * 	- Initialize the last descriptor with the endof ring in case of ring mode.
  *	- Initialize the descriptors in chain mode.
  * @param[in] pointer to synopGMACdevice.
  * @param[in] pointer to pci_device structure.
  * @param[in] number of descriptor expected in rx descriptor queue.
  * @param[in] whether descriptors to be created in RING mode or CHAIN mode.
  * \return 0 upon success. Error code upon failure.
  * \note This function fails if allocation fails for required number of descriptors in Ring mode, but in chain mode
  *  function returns -ESYNOPGMACNOMEM in the process of descriptor chain creation. once returned from this function
  *  user should for gmacdev->RxDescCount to see how many descriptors are there in the chain. Should continue further
  *  only if the number of descriptors in the chain meets the requirements  
  */
s32 synopGMAC_setup_rx_desc_queue(synopGMACdevice * gmacdev,struct device * dev,u32 no_of_desc, u32 desc_mode)
{
s32 i;

DmaDesc *first_desc = NULL;
DmaDesc *second_desc = NULL;
dma_addr_t dma_addr;
gmacdev->RxDescCount = 0;

if(desc_mode == RINGMODE){
	TR("total size of memory required for Rx Descriptors in Ring Mode = 0x%08lx\n",((sizeof(DmaDesc) * no_of_desc)));
	first_desc = plat_alloc_consistent_dmaable_memory (dev, sizeof(DmaDesc) * no_of_desc, &dma_addr);
	if(first_desc == NULL){
		TR("Error in Rx Descriptor Memory allocation in Ring mode\n");
		return -ESYNOPGMACNOMEM;
	}
	gmacdev->RxDescCount = no_of_desc;
	gmacdev->RxDesc      = first_desc;
	gmacdev->RxDescDma   = dma_addr;
	
	for(i =0; i < gmacdev -> RxDescCount; i++){
		synopGMAC_rx_desc_init_ring(gmacdev->RxDesc + i, i == gmacdev->RxDescCount-1);
//		TR("%02d %p \n",i, (gmacdev->RxDesc + i));

	}

}
else{
//Allocate the head descriptor
	first_desc = plat_alloc_consistent_dmaable_memory (dev, sizeof(DmaDesc),&dma_addr);
	if(first_desc == NULL){
		TR("Error in Rx Descriptor Memory allocation in Ring mode\n");
		return -ESYNOPGMACNOMEM;
	}
	gmacdev->RxDesc       = first_desc;
	gmacdev->RxDescDma    = dma_addr;

	TR("Rx===================================================================Rx\n");
	first_desc->buffer2   = gmacdev->RxDescDma;
	first_desc->data2     = (u64) gmacdev->RxDesc;

	gmacdev->RxDescCount = 1;
	
	for(i =0; i < (no_of_desc-1); i++){
		second_desc = plat_alloc_consistent_dmaable_memory(dev, sizeof(DmaDesc),&dma_addr);
		if(second_desc == NULL){	
			TR("Error in Rx Descriptor Memory allocation in Chain mode\n");
			return -ESYNOPGMACNOMEM;
		}
		first_desc->buffer2  = dma_addr;
		first_desc->data2    = (u64)second_desc;
		
		second_desc->buffer2 = gmacdev->RxDescDma;
		second_desc->data2   = (u64)gmacdev->RxDesc;

		synopGMAC_rx_desc_init_chain(first_desc);
//		TR("%02d  %p %08x %08x %08x %08x %lx %lx \n",gmacdev->RxDescCount, first_desc, first_desc->status, first_desc->length, first_desc->buffer1,first_desc->buffer2, first_desc->data1, first_desc->data2);
		gmacdev->RxDescCount += 1;
		first_desc = second_desc;
	}
                synopGMAC_rx_desc_init_chain(first_desc);
//		TR("%02d  %p %08x %08x %08x %08x %lx %lx \n",gmacdev->RxDescCount, first_desc, first_desc->status, first_desc->length, first_desc->buffer1,first_desc->buffer2, first_desc->data1, first_desc->data2);
	TR("Rx===================================================================Rx\n");

}

	gmacdev->RxNext = 0;
	gmacdev->RxBusy = 0;
	gmacdev->RxNextDesc = gmacdev->RxDesc;
	gmacdev->RxBusyDesc = gmacdev->RxDesc;

	gmacdev->BusyRxDesc   = 0; 

	return -ESYNOPGMACNOERR;
}

/**
  * This gives up the receive Descriptor queue in ring or chain mode.
  * This function is tightly coupled to the platform and operating system
  * Once device's Dma is stopped the memory descriptor memory and the buffer memory deallocation,
  * is completely handled by the operating system, this call is kept outside the device driver Api.
  * This function should be treated as an example code to de-allocate the descriptor structures in ring mode or chain mode
  * and network buffer deallocation.
  * This function depends on the dev structure for dma-able memory deallocation for both descriptor memory and the
  * network buffer memory under linux.
  * The responsibility of this function is to 
  *     - Free the network buffer memory if any.
  *	- Fee the memory allocated for the descriptors.
  * @param[in] pointer to synopGMACdevice.
  * @param[in] pointer to pci_device structure.
  * @param[in] number of descriptor expected in rx descriptor queue.
  * @param[in] whether descriptors to be created in RING mode or CHAIN mode.
  * \return 0 upon success. Error code upon failure.
  * \note No referece should be made to descriptors once this function is called. This function is invoked when the device is closed.
  */
void synopGMAC_giveup_rx_desc_queue(synopGMACdevice * gmacdev, struct device *dev, u32 desc_mode)
{
s32 i;

DmaDesc *first_desc = NULL;
dma_addr_t first_desc_dma_addr;
u32 status;
dma_addr_t dma_addr1;
dma_addr_t dma_addr2;
u32 length1;
u32 length2;
u64 data1;
u64 data2;

if(desc_mode == RINGMODE){
	for(i =0; i < gmacdev -> RxDescCount; i++){
		synopGMAC_get_desc_data(gmacdev->RxDesc + i, &status, &dma_addr1, &length1, &data1, &dma_addr2, &length2, &data2);
		if((length1 != 0) && (data1 != 0)){
			dma_unmap_single(dev,dma_addr1,ETHER_MAX_LEN,PCI_DMA_FROMDEVICE);
			dev_kfree_skb((struct sk_buff *) data1);	// free buffer1
			TR("(Ring mode) rx buffer1 %lx of size %d from %d rx descriptor is given back\n",data1, length1, i);
		}
		if((length2 != 0) && (data2 != 0)){
			dma_unmap_single(dev,dma_addr2,ETHER_MAX_LEN,PCI_DMA_FROMDEVICE);
			dev_kfree_skb((struct sk_buff *) data2);	//free buffer2
			TR("(Ring mode) rx buffer2 %lx of size %d from %d rx descriptor is given back\n",data2, length2, i);
		}
	}
	plat_free_consistent_dmaable_memory(dev,(sizeof(DmaDesc) * gmacdev->RxDescCount),gmacdev->RxDesc,gmacdev->RxDescDma); //free descriptors memory
	TR("Memory allocated %p  for Rx Desriptors (ring) is given back\n",gmacdev->RxDesc);
}
else{
	TR("rx-------------------------------------------------------------------rx\n");
	first_desc          = gmacdev->RxDesc;
	first_desc_dma_addr = gmacdev->RxDescDma;
	for(i =0; i < gmacdev -> RxDescCount; i++){
		synopGMAC_get_desc_data(first_desc, &status, &dma_addr1, &length1, &data1, &dma_addr2, &length2, &data2);
//		TR("%02d %p %08x %08x %08x %08x %lx %lx\n",i,first_desc,first_desc->status,first_desc->length,first_desc->buffer1,first_desc->buffer2,first_desc->data1,first_desc->data2);
		if((length1 != 0) && (data1 != 0)){
			dma_unmap_single(dev,dma_addr1,ETHER_MAX_LEN,PCI_DMA_FROMDEVICE);
			dev_kfree_skb((struct sk_buff *) data1);	// free buffer1
			TR("(Chain mode) rx buffer1 %lx of size %d from %d rx descriptor is given back\n",data1, length1, i);
		}
		plat_free_consistent_dmaable_memory(dev,(sizeof(DmaDesc)),first_desc,first_desc_dma_addr); //free descriptors
		TR("Memory allocated %lx for Rx Descriptor (chain) at  %d is given back\n",data2,i);

		first_desc = (DmaDesc *)data2;
		first_desc_dma_addr = dma_addr2;
	}

	TR("rx-------------------------------------------------------------------rx\n");
}
gmacdev->RxDesc    = NULL;
gmacdev->RxDescDma = 0;
return;
}

/**
  * This gives up the transmit Descriptor queue in ring or chain mode.
  * This function is tightly coupled to the platform and operating system
  * Once device's Dma is stopped the memory descriptor memory and the buffer memory deallocation,
  * is completely handled by the operating system, this call is kept outside the device driver Api.
  * This function should be treated as an example code to de-allocate the descriptor structures in ring mode or chain mode
  * and network buffer deallocation.
  * This function depends on the dev structure for dma-able memory deallocation for both descriptor memory and the
  * network buffer memory under linux.
  * The responsibility of this function is to 
  *     - Free the network buffer memory if any.
  *	- Fee the memory allocated for the descriptors.
  * @param[in] pointer to synopGMACdevice.
  * @param[in] pointer to pci_device structure.
  * @param[in] number of descriptor expected in tx descriptor queue.
  * @param[in] whether descriptors to be created in RING mode or CHAIN mode.
  * \return 0 upon success. Error code upon failure.
  * \note No reference should be made to descriptors once this function is called. This function is invoked when the device is closed.
  */
void synopGMAC_giveup_tx_desc_queue(synopGMACdevice * gmacdev,struct device * dev, u32 desc_mode)
{
s32 i;

DmaDesc *first_desc = NULL;
dma_addr_t first_desc_dma_addr;
u32 status;
dma_addr_t dma_addr1;
dma_addr_t dma_addr2;
u32 length1;
u32 length2;
u64 data1;
u64 data2;

if(desc_mode == RINGMODE){
	for(i =0; i < gmacdev -> TxDescCount; i++){
		synopGMAC_get_desc_data(gmacdev->TxDesc + i,&status, &dma_addr1, &length1, &data1, &dma_addr2, &length2, &data2);
		if((length1 != 0) && (data1 != 0)){
			dma_unmap_single(dev,dma_addr1,ETHER_MAX_LEN,PCI_DMA_TODEVICE);
			dev_kfree_skb((struct sk_buff *) data1);	// free buffer1
			TR("(Ring mode) tx buffer1 %lx of size %d from %d rx descriptor is given back\n",data1, length1, i);
		}
		if((length2 != 0) && (data2 != 0)){
			dma_unmap_single(dev,dma_addr2,ETHER_MAX_LEN,PCI_DMA_TODEVICE);
			dev_kfree_skb((struct sk_buff *) data2);	//free buffer2
			TR("(Ring mode) tx buffer2 %lx of size %d from %d rx descriptor is given back\n",data2, length2, i);
		}
	}
	plat_free_consistent_dmaable_memory(dev,(sizeof(DmaDesc) * gmacdev->TxDescCount),gmacdev->TxDesc,gmacdev->TxDescDma); //free descriptors
	TR("Memory allocated %p for Tx Desriptors (ring) is given back\n",gmacdev->TxDesc);
}
else{
	TR("tx-------------------------------------------------------------------tx\n");
	first_desc          = gmacdev->TxDesc;
	first_desc_dma_addr = gmacdev->TxDescDma;
	for(i =0; i < gmacdev -> TxDescCount; i++){
		synopGMAC_get_desc_data(first_desc, &status, &dma_addr1, &length1, &data1, &dma_addr2, &length2, &data2);
//		TR("%02d %p %08x %08x %08x %08x %lx %lx\n",i,first_desc,first_desc->status,first_desc->length,first_desc->buffer1,first_desc->buffer2,first_desc->data1,first_desc->data2);
		if((length1 != 0) && (data1 != 0)){
			dma_unmap_single(dev,dma_addr1,ETHER_MAX_LEN,PCI_DMA_TODEVICE);
			dev_kfree_skb((struct sk_buff *) data2);	// free buffer1
			TR("(Chain mode) tx buffer1 %lx of size %d from %d rx descriptor is given back\n",data1, length1, i);
		}
		plat_free_consistent_dmaable_memory(dev,(sizeof(DmaDesc)),first_desc,first_desc_dma_addr); //free descriptors
		TR("Memory allocated %lx for Tx Descriptor (chain) at  %d is given back\n",data2,i);

		first_desc = (DmaDesc *)data2;
		first_desc_dma_addr = dma_addr2;
	}
	TR("tx-------------------------------------------------------------------tx\n");

}
gmacdev->TxDesc    = NULL;
gmacdev->TxDescDma = 0;
return;
}


/**
 * Function to handle housekeeping after a packet is transmitted over the wire.
 * After the transmission of a packet DMA generates corresponding interrupt 
 * (if it is enabled). It takes care of returning the sk_buff to the linux
 * kernel, updating the networking statistics and tracking the descriptors.
 * @param[in] pointer to net_device structure. 
 * \return void.
 * \note This function runs in interrupt context
 */
void synop_handle_transmit_over(struct net_device *netdev)
{
	synopGMACPciNetworkAdapter *adapter;
	synopGMACdevice * gmacdev;
	struct device *dev;
	s32 desc_index;
	u64 data1, data2;
	u32 status;
	u32 length1, length2;
	u32 dma_addr1, dma_addr2;
#ifdef ENH_DESC_8W
	u32 ext_status;
	u16 time_stamp_higher;
	u32 time_stamp_high;
	u32 time_stamp_low;
#endif
	adapter = netdev_priv(netdev);
	if(adapter == NULL){
		TR("Unknown Device\n");
		return;
	}
	
	gmacdev = adapter->synopGMACdev;
	if(gmacdev == NULL){
		TR("GMAC device structure is missing\n");
		return;
	}

 	dev  = adapter->dev;	
	/*Handle the transmit Descriptors*/
	do {
#ifdef ENH_DESC_8W
	desc_index = synopGMAC_get_tx_qptr(gmacdev, &status, &dma_addr1, &length1, &data1, &dma_addr2, &length2, &data2,&ext_status,&time_stamp_high,&time_stamp_low);
        synopGMAC_TS_read_timestamp_higher_val(gmacdev, &time_stamp_higher);
#else
	desc_index = synopGMAC_get_tx_qptr(gmacdev, &status, &dma_addr1, &length1, &data1, &dma_addr2, &length2, &data2);
#endif
	//desc_index = synopGMAC_get_tx_qptr(gmacdev, &status, &dma_addr, &length, &data1);
		if(desc_index >= 0 && data1 != 0){
			TR("Finished Transmit at Tx Descriptor %d for skb 0x%lx and buffer = %08x whose status is %08x \n", desc_index,data1,dma_addr1,status);
			#ifdef	IPC_OFFLOAD
			if(synopGMAC_is_tx_ipv4header_checksum_error(gmacdev, status)){
			TR("Harware Failed to Insert IPV4 Header Checksum\n");
			}
			if(synopGMAC_is_tx_payload_checksum_error(gmacdev, status)){
			TR("Harware Failed to Insert Payload Checksum\n");
			}
			#endif
		
			dma_unmap_single(dev,dma_addr1,length1,PCI_DMA_TODEVICE);
			dev_kfree_skb_irq((struct sk_buff *)data1);

			if(synopGMAC_is_desc_valid(status)){
				adapter->synopGMACNetStats.tx_bytes += length1;
				adapter->synopGMACNetStats.tx_packets++;
			}
			else {	
				TR("Error in Status %08x\n",status);
				adapter->synopGMACNetStats.tx_errors++;
				adapter->synopGMACNetStats.tx_aborted_errors += synopGMAC_is_tx_aborted(status);
				adapter->synopGMACNetStats.tx_carrier_errors += synopGMAC_is_tx_carrier_error(status);
			}
		}	adapter->synopGMACNetStats.collisions += synopGMAC_get_tx_collision_count(status);
	} while(desc_index >= 0);
	netif_wake_queue(netdev);
}




/**
 * Function to Receive a packet from the interface.
 * After Receiving a packet, DMA transfers the received packet to the system memory
 * and generates corresponding interrupt (if it is enabled). This function prepares
 * the sk_buff for received packet after removing the ethernet CRC, and hands it over
 * to linux networking stack.
 * 	- Updataes the networking interface statistics
 *	- Keeps track of the rx descriptors
 * @param[in] pointer to net_device structure. 
 * \return void.
 * \note This function runs in interrupt context.
 */

void synop_handle_received_data(struct net_device *netdev)
{
	synopGMACPciNetworkAdapter *adapter;
	synopGMACdevice * gmacdev;
	struct device *dev;
	s32 desc_index;
	
	u64 data1;
	u64 data2;
	u32 len;
	u32 status;
	u32 dma_addr1;
	u32 dma_addr2;
#ifdef ENH_DESC_8W
	u32 ext_status;
	u16 time_stamp_higher;
	u32 time_stamp_high;
	u32 time_stamp_low;
#endif	
	struct sk_buff *skb; //This is the pointer to hold the received data
	
	TR("%s is called.\n",__FUNCTION__);	
	
	if((adapter = netdev_priv(netdev)) == NULL){
		TR("Unknown Device\n");
		return;
	}
	
	if((gmacdev = adapter->synopGMACdev) == NULL){
		TR("GMAC device structure is missing\n");
		return;
	}

 	dev  = adapter->dev;	

	/*Handle the Receive Descriptors*/
	do{
#ifdef ENH_DESC_8W
	desc_index = synopGMAC_get_rx_qptr(gmacdev, &status,&dma_addr1,NULL, &data1,&dma_addr2,NULL,&data2,&ext_status,&time_stamp_high,&time_stamp_low);
	if(desc_index >0){ 
	        synopGMAC_TS_read_timestamp_higher_val(gmacdev, &time_stamp_higher);
//		TR("S:%08x ES:%08x DA1:%08x d1:%08x DA2:%08x d2:%08x TSH:%08x TSL:%08x TSHW:%08x \n",status,ext_status,dma_addr1, data1,dma_addr2,data2, time_stamp_high,time_stamp_low,time_stamp_higher);
	}
#else
	desc_index = synopGMAC_get_rx_qptr(gmacdev, &status,&dma_addr1,NULL, &data1,&dma_addr2,NULL,&data2);
#endif
	//desc_index = synopGMAC_get_rx_qptr(gmacdev, &status,&dma_addr,NULL, &data1);
	TR("<0>desc_index=%d\n",desc_index);

	if(desc_index >= 0 && data1 != 0){
		TR("<0>Received Data at Rx Descriptor %d for skb 0x%lx whose status is %08x\n",desc_index,data1,status);
		
		/*At first step unmapped the dma address*/
		dma_unmap_single(dev,dma_addr1,ETHER_MAX_LEN,PCI_DMA_FROMDEVICE);

		skb = (struct sk_buff *)data1;
		if(synopGMAC_is_rx_desc_valid(status)){
			len =  synopGMAC_get_rx_desc_frame_length(status) - 4; //Not interested in Ethernet CRC bytes

			skb_put(skb,len);
		#ifdef IPC_OFFLOAD
			
			// Now lets check for the IPC offloading
			/*  Since we have enabled the checksum offloading in hardware, lets inform the kernel
			    not to perform the checksum computation on the incoming packet. Note that ip header 
  			    checksum will be computed by the kernel immaterial of what we inform. Similary TCP/UDP/ICMP
			    pseudo header checksum will be computed by the stack. What we can inform is not to perform
			    payload checksum. 		
   			    When CHECKSUM_UNNECESSARY is set kernel bypasses the checksum computation.		    
			*/
	
			TR("Checksum Offloading will be done now\n");
			skb->ip_summed = CHECKSUM_UNNECESSARY;
				
		  	#ifdef ENH_DESC_8W
			if(synopGMAC_is_ext_status(gmacdev, status)){ // extended status present indicates that the RDES4 need to be probed
				TR("Extended Status present\n");
				if(synopGMAC_ES_is_IP_header_error(gmacdev,ext_status)){       // IP header (IPV4) checksum error
					//Linux Kernel doesnot care for ipv4 header checksum. So we will simply proceed by printing a warning ....
					TR("(EXTSTS)Error in IP header error\n");
					skb->ip_summed = CHECKSUM_NONE;     //Let Kernel compute the checkssum
				}	
				if(synopGMAC_ES_is_rx_checksum_bypassed(gmacdev,ext_status)){   // Hardware engine bypassed the checksum computation/checking
					TR("(EXTSTS)Hardware bypassed checksum computation\n");	
					skb->ip_summed = CHECKSUM_NONE;             // Let Kernel compute the checksum
				}
				if(synopGMAC_ES_is_IP_payload_error(gmacdev,ext_status)){       // IP payload checksum is in error (UDP/TCP/ICMP checksum error)
					TR("(EXTSTS) Error in EP payload\n");	
					skb->ip_summed = CHECKSUM_NONE;             // Let Kernel compute the checksum
				}				
			}else{ // No extended status. So relevant information is available in the status itself
				if(synopGMAC_is_rx_checksum_error(gmacdev, status) == RxNoChkError ){
					TR("Ip header and TCP/UDP payload checksum Bypassed <Chk Status = 4>  \n");
					skb->ip_summed = CHECKSUM_UNNECESSARY;	//Let Kernel bypass computing the Checksum
				}
				if(synopGMAC_is_rx_checksum_error(gmacdev, status) == RxIpHdrChkError ){
					//Linux Kernel doesnot care for ipv4 header checksum. So we will simply proceed by printing a warning ....
					TR(" Error in 16bit IPV4 Header Checksum <Chk Status = 6>  \n");
					skb->ip_summed = CHECKSUM_UNNECESSARY;	//Let Kernel bypass the TCP/UDP checksum computation
				}				
				if(synopGMAC_is_rx_checksum_error(gmacdev, status) == RxLenLT600 ){
					TR("IEEE 802.3 type frame with Length field Lesss than 0x0600 <Chk Status = 0> \n");
					skb->ip_summed = CHECKSUM_NONE;	//Let Kernel compute the Checksum
				}
				if(synopGMAC_is_rx_checksum_error(gmacdev, status) == RxIpHdrPayLoadChkBypass ){
					TR("Ip header and TCP/UDP payload checksum Bypassed <Chk Status = 1>\n");
					skb->ip_summed = CHECKSUM_NONE;	//Let Kernel compute the Checksum
				}
				if(synopGMAC_is_rx_checksum_error(gmacdev, status) == RxChkBypass ){
					TR("Ip header and TCP/UDP payload checksum Bypassed <Chk Status = 3>  \n");
					skb->ip_summed = CHECKSUM_NONE;	//Let Kernel compute the Checksum
				}
				if(synopGMAC_is_rx_checksum_error(gmacdev, status) == RxPayLoadChkError ){
					TR(" TCP/UDP payload checksum Error <Chk Status = 5>  \n");
					skb->ip_summed = CHECKSUM_NONE;	//Let Kernel compute the Checksum
				}
				if(synopGMAC_is_rx_checksum_error(gmacdev, status) == RxIpHdrChkError ){
					//Linux Kernel doesnot care for ipv4 header checksum. So we will simply proceed by printing a warning ....
					TR(" Both IP header and Payload Checksum Error <Chk Status = 7>  \n");
					skb->ip_summed = CHECKSUM_NONE;	        //Let Kernel compute the Checksum
				}
				}
			#else	
				if(synopGMAC_is_rx_checksum_error(gmacdev, status) == RxNoChkError ){
				TR("Ip header and TCP/UDP payload checksum Bypassed <Chk Status = 4>  \n");
				skb->ip_summed = CHECKSUM_UNNECESSARY;	//Let Kernel bypass computing the Checksum
				}
				if(synopGMAC_is_rx_checksum_error(gmacdev, status) == RxIpHdrChkError ){
				//Linux Kernel doesnot care for ipv4 header checksum. So we will simply proceed by printing a warning ....
				TR(" Error in 16bit IPV4 Header Checksum <Chk Status = 6>  \n");
				skb->ip_summed = CHECKSUM_UNNECESSARY;	//Let Kernel bypass the TCP/UDP checksum computation
				}				
				if(synopGMAC_is_rx_checksum_error(gmacdev, status) == RxLenLT600 ){
				TR("IEEE 802.3 type frame with Length field Lesss than 0x0600 <Chk Status = 0> \n");
				skb->ip_summed = CHECKSUM_NONE;	//Let Kernel compute the Checksum
				}
				if(synopGMAC_is_rx_checksum_error(gmacdev, status) == RxIpHdrPayLoadChkBypass ){
				TR("Ip header and TCP/UDP payload checksum Bypassed <Chk Status = 1>\n");
				skb->ip_summed = CHECKSUM_NONE;	//Let Kernel compute the Checksum
				}
				if(synopGMAC_is_rx_checksum_error(gmacdev, status) == RxChkBypass ){
				TR("Ip header and TCP/UDP payload checksum Bypassed <Chk Status = 3>  \n");
				skb->ip_summed = CHECKSUM_NONE;	//Let Kernel compute the Checksum
				}
				if(synopGMAC_is_rx_checksum_error(gmacdev, status) == RxPayLoadChkError ){
				TR(" TCP/UDP payload checksum Error <Chk Status = 5>  \n");
				skb->ip_summed = CHECKSUM_NONE;	//Let Kernel compute the Checksum
				}
				if(synopGMAC_is_rx_checksum_error(gmacdev, status) == RxIpHdrChkError ){
				//Linux Kernel doesnot care for ipv4 header checksum. So we will simply proceed by printing a warning ....
				TR(" Both IP header and Payload Checksum Error <Chk Status = 7>  \n");
				skb->ip_summed = CHECKSUM_NONE;	        //Let Kernel compute the Checksum
				}
				
			#endif
		#endif //IPC_OFFLOAD	
		#if 0
		printk("---------received data----------.\n");
                int counter;
                printk(KERN_CRIT"skb->ip_summed = CHECKSUM_HW\n");                                                                                     
//              printk(KERN_CRIT"skb->h.th=%08x skb->h.th->check=%08x\n",(u32)(skb->h.th),(u32)(skb->h.th->check));                                    
//              printk(KERN_CRIT"skb->h.uh=%08x skb->h.uh->check=%08x\n",(u32)(skb->h.uh),(u32)(skb->h.uh->check));
                printk(KERN_CRIT"\n skb->len = %d skb->mac_len = %d skb->data = %08x skb->csum = %08x skb->h.raw = %08x\n",skb->len,skb->mac_len,(u32)(skb->data),skb->csum,(u32)(skb->h.raw));
                printk(KERN_CRIT"DST MAC addr:%02x %02x %02x %02x %02x %02x\n",*(skb->data+0),*(skb->data+1),*(skb->data+2),*(skb->data+3),*(skb->data+4),*(skb->data+5));
                printk(KERN_CRIT"SRC MAC addr:%02x %02x %02x %02x %02x %02x\n",*(skb->data+6),*(skb->data+7),*(skb->data+8),*(skb->data+9),*(skb->data+10),*(skb->data+11));
                printk(KERN_CRIT"Len/type    :%02x %02x\n",*(skb->data+12),*(skb->data+13));                                                           
                if(((*(skb->data+14)) & 0xF0) == 0x40){
                        printk(KERN_CRIT"IPV4 Header:\n");
                        printk(KERN_CRIT"%02x %02x %02x %02x\n",*(skb->data+14),*(skb->data+15),*(skb->data+16),*(skb->data+17));                      
                        printk(KERN_CRIT"%02x %02x %02x %02x\n",*(skb->data+18),*(skb->data+19),*(skb->data+20),*(skb->data+21));
                        printk(KERN_CRIT"%02x %02x %02x %02x\n",*(skb->data+22),*(skb->data+23),*(skb->data+24),*(skb->data+25));                      
                        printk(KERN_CRIT"%02x %02x %02x %02x\n",*(skb->data+26),*(skb->data+27),*(skb->data+28),*(skb->data+29));
                        printk(KERN_CRIT"%02x %02x %02x %02x\n\n",*(skb->data+30),*(skb->data+31),*(skb->data+32),*(skb->data+33));                    
                        for(counter = 34; counter < skb->len; counter++)
                                printk("%02X ",*(skb->data + counter));                                                                                
                }
                else{
                        printk(KERN_CRIT"IPV6 FRAME:\n");
                        for(counter = 14; counter < skb->len; counter++)                                                                               
                                printk("%02X ",*(skb->data + counter));
                }             
                #endif

				skb->dev = netdev;
				skb->protocol = eth_type_trans(skb, netdev);
				netif_rx(skb);

				netdev->last_rx = jiffies;
				adapter->synopGMACNetStats.rx_packets++;
				adapter->synopGMACNetStats.rx_bytes += len;
			}
			else{
				/*Now the present skb should be set free*/
				dev_kfree_skb_irq(skb);
				printk(KERN_CRIT "s: %08x\n",status);
				adapter->synopGMACNetStats.rx_errors++;
				adapter->synopGMACNetStats.collisions       += synopGMAC_is_rx_frame_collision(status);
				adapter->synopGMACNetStats.rx_crc_errors    += synopGMAC_is_rx_crc(status);
				adapter->synopGMACNetStats.rx_frame_errors  += synopGMAC_is_frame_dribbling_errors(status);
				adapter->synopGMACNetStats.rx_length_errors += synopGMAC_is_rx_frame_length_errors(status);
			}
			
			//Now lets allocate the skb for the emptied descriptor
			skb = alloc_skb(BUS_SIZE_ALIGN(netdev->mtu + ETHERNET_PACKET_EXTRA)+2,GFP_ATOMIC);
			if(skb == NULL){
				TR("SKB memory allocation failed \n");
				adapter->synopGMACNetStats.rx_dropped++;
			}
			skb_reserve(skb,2);
						
			dma_addr1 = dma_map_single(dev,skb->data,BUS_SIZE_ALIGN(netdev->mtu + ETHERNET_PACKET_EXTRA),PCI_DMA_FROMDEVICE);
			desc_index = synopGMAC_set_rx_qptr(gmacdev,dma_addr1, BUS_SIZE_ALIGN(netdev->mtu + ETHERNET_PACKET_EXTRA), (u64)skb,0,0,0);
			
			
			if(desc_index < 0){
				TR("Cannot set Rx Descriptor for skb %p\n",skb);
				dev_kfree_skb_irq(skb);
			}
					
		}
	}while(desc_index >= 0);
}


/**
 * Interrupt service routing.
 * This is the function registered as ISR for device interrupts.
 * @param[in] interrupt number. 
 * @param[in] void pointer to device unique structure (Required for shared interrupts in Linux).
 * @param[in] pointer to pt_regs (not used).
 * \return Returns IRQ_NONE if not device interrupts IRQ_HANDLED for device interrupts.
 * \note This function runs in interrupt context
 *
 */
irqreturn_t synopGMAC_intr_handler(s32 intr_num, void * dev_id)
{       
	/*Kernels passes the netdev structure in the dev_id. So grab it*/
        struct net_device *netdev;
        synopGMACPciNetworkAdapter *adapter;
        synopGMACdevice * gmacdev;
	struct device *dev;
        u32 interrupt,dma_status_reg;
	s32 status;
	u32 dma_addr;

        netdev  = (struct net_device *) dev_id;
        if(netdev == NULL){
                TR("Unknown Device\n");
                return -1;
        }

        adapter  = netdev_priv(netdev);
        if(adapter == NULL){
                TR("Adapter Structure Missing\n");
                return -1;
        }

        gmacdev = adapter->synopGMACdev;
        if(gmacdev == NULL){
                TR("GMAC device structure Missing\n");
                return -1;
        }
	dev  = adapter->dev;	

	/*Read the Dma interrupt status to know whether the interrupt got generated by our device or not*/
	dma_status_reg = synopGMACReadReg((u32 *)gmacdev->DmaBase, DmaStatus);
	TR("<0>%s :dma_status_reg=%x\n",__FUNCTION__, dma_status_reg);
	if(dma_status_reg == 0)
		return IRQ_HANDLED;

        synopGMAC_disable_interrupt_all(gmacdev);

	synopGMACReadReg((u32 *)gmacdev->MacBase,GmacStatus);
     	TR("%s:Dma Status Reg: 0x%08x\n",__FUNCTION__,dma_status_reg);
	
	if(dma_status_reg & GmacPmtIntr){
		TR("%s:: Interrupt due to PMT module\n",__FUNCTION__);
		synopGMAC_linux_powerup_mac(gmacdev);
	}
	
	if(dma_status_reg & GmacMmcIntr){
		TR("%s:: Interrupt due to MMC module\n",__FUNCTION__);
		TR("%s:: synopGMAC_rx_int_status = %08x\n",__FUNCTION__,synopGMAC_read_mmc_rx_int_status(gmacdev));
		TR("%s:: synopGMAC_tx_int_status = %08x\n",__FUNCTION__,synopGMAC_read_mmc_tx_int_status(gmacdev));
	}

	if(dma_status_reg & GmacLineIntfIntr){
		TR("%s:: Interrupt due to GMAC LINE module\n",__FUNCTION__);
		//synopGMAC_linux_cable_unplug_function(adapter);
	}

	/*Now lets handle the DMA interrupts*/  
        interrupt = synopGMAC_get_interrupt_type(gmacdev);
       	TR("%s:Interrupts to be handled: 0x%08x\n",__FUNCTION__,interrupt);


        if(interrupt & synopGMACDmaError){
		u8 mac_addr0[6];//after soft reset, configure the MAC address to default value
		TR("%s::Fatal Bus Error Inetrrupt Seen\n",__FUNCTION__);

		memcpy(mac_addr0,netdev->dev_addr,6);
		synopGMAC_disable_dma_tx(gmacdev);
                synopGMAC_disable_dma_rx(gmacdev);
                
		synopGMAC_take_desc_ownership_tx(gmacdev);
		synopGMAC_take_desc_ownership_rx(gmacdev);
		
		synopGMAC_init_tx_rx_desc_queue(gmacdev);
		
		synopGMAC_reset(gmacdev);//reset the DMA engine and the GMAC ip
		
		synopGMAC_set_mac_addr(gmacdev,GmacAddr0High,GmacAddr0Low, mac_addr0); 
		synopGMAC_dma_bus_mode_init(gmacdev,DmaFixedBurstEnable| DmaBurstLength8 | DmaDescriptorSkip2 );
	 	synopGMAC_dma_control_init(gmacdev,DmaStoreAndForward);	
		synopGMAC_init_rx_desc_base(gmacdev);
		synopGMAC_init_tx_desc_base(gmacdev);
		synopGMAC_mac_init(gmacdev);
		synopGMAC_enable_dma_rx(gmacdev);
		synopGMAC_enable_dma_tx(gmacdev);

        }


	if(interrupt & synopGMACDmaRxNormal){
		TR("%s:: Rx Normal \n", __FUNCTION__);
                synop_handle_received_data(netdev);
	}

        if(interrupt & synopGMACDmaRxAbnormal){
	        TR("%s::Abnormal Rx Interrupt Seen\n",__FUNCTION__);
		#if 1
	
	       if(GMAC_Power_down == 0){	// If Mac is not in powerdown
                adapter->synopGMACNetStats.rx_over_errors++;
		/*Now Descriptors have been created in synop_handle_received_data(). Just issue a poll demand to resume DMA operation*/
//		synopGMACWriteReg(gmacdev->DmaBase, DmaStatus ,0x80); 	//sw: clear the rxb ua bit
		synopGMAC_resume_dma_rx(gmacdev);//To handle GBPS with 12 descriptors
		}
		#endif
	}



        if(interrupt & synopGMACDmaRxStopped){
        	TR("%s::Receiver stopped seeing Rx interrupts\n",__FUNCTION__); //Receiver gone in to stopped state
		#if 1
	        if(GMAC_Power_down == 0){	// If Mac is not in powerdown
		adapter->synopGMACNetStats.rx_over_errors++;
		do{
			struct sk_buff *skb = alloc_skb(BUS_SIZE_ALIGN(netdev->mtu + ETHERNET_PACKET_EXTRA)+2, GFP_ATOMIC);
			if(skb == NULL){
				TR("%s::ERROR in skb buffer allocation Better Luck Next time\n",__FUNCTION__);
				break;
				//			return -ESYNOPGMACNOMEM;
			}
			skb_reserve(skb,2);
			
			dma_addr = dma_map_single(dev,skb->data,BUS_SIZE_ALIGN(netdev->mtu + ETHERNET_PACKET_EXTRA),PCI_DMA_FROMDEVICE);
			status = synopGMAC_set_rx_qptr(gmacdev,dma_addr, BUS_SIZE_ALIGN(netdev->mtu + ETHERNET_PACKET_EXTRA), (u64)skb,0,0,0);
//			TR("%s::Set Rx Descriptor no %08x for skb %p \n",__FUNCTION__,status,skb);
			if(status < 0)
				dev_kfree_skb_irq(skb);//changed from dev_free_skb. If problem check this again--manju
		
		}while(status >= 0);
		
		synopGMAC_enable_dma_rx(gmacdev);
		}
		#endif
	}

	if(interrupt & synopGMACDmaTxNormal){
		//xmit function has done its job
		TR("%s::Finished Normal Transmission \n",__FUNCTION__);
		synop_handle_transmit_over(netdev);//Do whatever you want after the transmission is over

		
	}

        if(interrupt & synopGMACDmaTxAbnormal){
		TR("%s::Abnormal Tx Interrupt Seen\n",__FUNCTION__);
		#if 1
	       if(GMAC_Power_down == 0){	// If Mac is not in powerdown
                synop_handle_transmit_over(netdev);
		}
		#endif
	}



	if(interrupt & synopGMACDmaTxStopped){
		TR("%s::Transmitter stopped sending the packets\n",__FUNCTION__);
		#if 1
	       if(GMAC_Power_down == 0){	// If Mac is not in powerdown
		synopGMAC_disable_dma_tx(gmacdev);
                synopGMAC_take_desc_ownership_tx(gmacdev);
		
		synopGMAC_enable_dma_tx(gmacdev);
		netif_wake_queue(netdev);
		TR("%s::Transmission Resumed\n",__FUNCTION__);
		}
		#endif
	}

        /* Enable the interrrupt before returning from ISR*/
        synopGMAC_enable_interrupt(gmacdev,DmaIntEnable);
        return IRQ_HANDLED;
}


/**
 * Function used when the interface is opened for use.
 * We register synopGMAC_linux_open function to linux open(). Basically this 
 * function prepares the the device for operation . This function is called whenever ifconfig (in Linux)
 * activates the device (for example "ifconfig eth0 up"). This function registers
 * system resources needed 
 * 	- Attaches device to device specific structure
 * 	- Programs the MDC clock for PHY configuration
 * 	- Check and initialize the PHY interface 
 *	- ISR registration
 * 	- Setup and initialize Tx and Rx descriptors
 *	- Initialize MAC and DMA
 *	- Allocate Memory for RX descriptors (The should be DMAable)
 * 	- Initialize one second timer to detect cable plug/unplug
 *	- Configure and Enable Interrupts
 *	- Enable Tx and Rx
 *	- start the Linux network queue interface
 * @param[in] pointer to net_device structure. 
 * \return Returns 0 on success and error status upon failure.
 * \callgraph
 */

s32 synopGMAC_linux_open(struct net_device *netdev)
{
	s32 status = 0;
	s32 retval = 0;
	u32 dma_addr;
	struct sk_buff *skb;
        synopGMACPciNetworkAdapter *adapter;
        synopGMACdevice * gmacdev;
	struct device *dev;
	adapter = (synopGMACPciNetworkAdapter *) netdev_priv(netdev);
	gmacdev = (synopGMACdevice *)adapter->synopGMACdev;
 	dev  = adapter->dev;	
	u32 cnt;
	
	TR0("%s called \n",__FUNCTION__);

	/*Now platform dependent initialization.*/

	/*Lets reset the IP*/
	TR("adapter= %p gmacdev = %p netdev = %p dev= %p\n",adapter,gmacdev,netdev,dev);
	synopGMAC_reset(gmacdev);
	
	/*Attach the device to MAC struct This will configure all the required base addresses
	  such as Mac base, configuration base, phy base address(out of 32 possible phys )*/
	synopGMAC_attach(adapter->synopGMACdev, adapter->synopGMACMappedAddr + MACBASE, adapter->synopGMACMappedAddr + DMABASE, DEFAULT_PHY_BASE,netdev->dev_addr);

	/*Lets read the version of ip in to device structure*/	
	synopGMAC_read_version(gmacdev);
	
	synopGMAC_get_mac_addr(adapter->synopGMACdev,GmacAddr0High,GmacAddr0Low, netdev->dev_addr); 
	/*Now set the broadcast address*/	
	for(cnt = 0; cnt < 6; cnt++){
		netdev->broadcast[cnt] = 0xff;
	}

	#ifdef DEBUG
	for(cnt = 0; cnt < 6; cnt++){
		TR("netdev->dev_addr[%d] = %02x and netdev->broadcast[%d] = %02x\n",cnt,netdev->dev_addr[cnt],cnt,netdev->broadcast[cnt]);
	}
	#endif

	/*Check for Phy initialization*/
	synopGMAC_set_mdc_clk_div(gmacdev,GmiiCsrClk3); 
	gmacdev->ClockDivMdc = synopGMAC_get_mdc_clk_div(gmacdev);

	{
		unsigned short data;
		synopGMAC_read_phy_reg(gmacdev->MacBase,gmacdev->PhyBase,2,&data);
       		/*set 88e1111 clock phase delay*/
       		if(data == 0x141)
			synopGMAC_phy88e1111_phase_init(gmacdev);
	}

	status = synopGMAC_check_phy_init(adapter);
	 /*must reinit mac after check phy state status change*/
	synopGMAC_mac_init(gmacdev);
	
	/*Request for an shared interrupt. Instead of using netdev->irq lets use adapter->irq*/
	if(request_irq (adapter->irq, synopGMAC_intr_handler, IRQF_SHARED, netdev->name,netdev)){
 		TR0("Error in request_irq %d\n",adapter->irq);
		goto error_in_irq;	
	}
 	TR("%s owns a shared interrupt on line %d\n",netdev->name, adapter->irq);

	/*Set up the tx and rx descriptor queue/ring*/

#ifndef USE_CHAIN_MODE
	synopGMAC_setup_tx_desc_queue(gmacdev,dev,TRANSMIT_DESC_SIZE, RINGMODE);
#else
	synopGMAC_setup_tx_desc_queue(gmacdev,dev,TRANSMIT_DESC_SIZE, CHAINMODE);
#endif
	synopGMAC_init_tx_desc_base(gmacdev);	//Program the transmit descriptor base address in to DmaTxBase addr


#ifndef USE_CHAIN_MODE
	synopGMAC_setup_rx_desc_queue(gmacdev,dev,RECEIVE_DESC_SIZE, RINGMODE);
#else
	synopGMAC_setup_rx_desc_queue(gmacdev,dev,RECEIVE_DESC_SIZE, CHAINMODE);
#endif


	synopGMAC_init_rx_desc_base(gmacdev);	//Program the transmit descriptor base address in to DmaTxBase addr

//	synopGMAC_dma_bus_mode_init(gmacdev,DmaFixedBurstEnable| DmaBurstLength8 | DmaDescriptorSkip2 );
//	synopGMAC_dma_bus_mode_init(gmacdev, DmaBurstLength32 | DmaDescriptorSkip2 );
//	synopGMAC_dma_control_init(gmacdev,DmaStoreAndForward |DmaTxSecondFrame);	
	
//	synopGMAC_dma_bus_mode_init(gmacdev,DmaFixedBurstEnable | DmaBurstLength8 | DmaDescriptorSkip2 ); //pbl8 incrx
//	synopGMAC_dma_control_init(gmacdev,DmaStoreAndForward |DmaTxSecondFrame|DmaRxThreshCtrl64);	

//	synopGMAC_dma_bus_mode_init(gmacdev, DmaBurstLength8 | DmaDescriptorSkip2 );                      //pbl8 incr
//	synopGMAC_dma_control_init(gmacdev,DmaStoreAndForward |DmaTxSecondFrame|DmaRxThreshCtrl64);	

//	synopGMAC_dma_bus_mode_init(gmacdev,DmaFixedBurstEnable | DmaBurstLength16 | DmaDescriptorSkip2 ); //pbl16 incrx
//	synopGMAC_dma_control_init(gmacdev,DmaStoreAndForward |DmaTxSecondFrame|DmaRxThreshCtrl64);	

//	synopGMAC_dma_bus_mode_init(gmacdev, DmaBurstLength16 | DmaDescriptorSkip2 );                      //pbl16 incr
//	synopGMAC_dma_control_init(gmacdev,DmaStoreAndForward |DmaTxSecondFrame|DmaRxThreshCtrl64);	

#ifdef ENH_DESC_8W
	synopGMAC_dma_bus_mode_init(gmacdev, DmaBurstLength32 | DmaDescriptorSkip2 | DmaDescriptor8Words ); //pbl32 incr with rxthreshold 128 and Desc is 8 Words
#else
	synopGMAC_dma_bus_mode_init(gmacdev, DmaBurstLength4 | DmaDescriptorSkip1);                      //pbl32 incr with rxthreshold 128
#endif
	synopGMAC_dma_control_init(gmacdev,DmaStoreAndForward |DmaTxSecondFrame|DmaRxThreshCtrl128);

//	synopGMAC_dma_bus_mode_init(gmacdev, DmaBurstLength64 | DmaDescriptorSkip2 );                      //pbl64 incr with rxthreshold 128
//	synopGMAC_dma_control_init(gmacdev,DmaStoreAndForward |DmaTxSecondFrame|DmaRxThreshCtrl128);	

//	synopGMAC_dma_bus_mode_init(gmacdev, DmaBurstLength64 | DmaDescriptorSkip2 );                      //pbl64 incr with rxthreshold 128
//	synopGMAC_dma_control_init(gmacdev,DmaStoreAndForward |DmaRxThreshCtrl128);	

	/*Initialize the mac interface*/

	synopGMAC_pause_control(gmacdev); // This enables the pause control in Full duplex mode of operation
	#ifdef IPC_OFFLOAD
	/*IPC Checksum offloading is enabled for this driver. Should only be used if Full Ip checksumm offload engine is configured in the hardware*/
	synopGMAC_enable_rx_chksum_offload(gmacdev);  	//Enable the offload engine in the receive path
	synopGMAC_rx_tcpip_chksum_drop_enable(gmacdev); // This is default configuration, DMA drops the packets if error in encapsulated ethernet payload
							// The FEF bit in DMA control register is configured to 0 indicating DMA to drop the errored frames.
	/*Inform the Linux Networking stack about the hardware capability of checksum offloading*/
	netdev->features = NETIF_F_HW_CSUM;
	#endif

     do{
		skb = alloc_skb(BUS_SIZE_ALIGN(netdev->mtu + ETHERNET_PACKET_EXTRA)+2, GFP_ATOMIC);
		if(skb == NULL){
			TR0("ERROR in skb buffer allocation\n");
			break;
//			return -ESYNOPGMACNOMEM;
		}
		skb_reserve(skb,2);
//		skb_reserve(skb,reserve_len);
//		TR("skb = %08x skb->tail = %08x skb_tailroom(skb)=%08x skb->data = %08x\n",(u32)skb,(u32)skb->tail,(skb_tailroom(skb)),(u32)skb->data);
//		skb->dev = netdev;
		//TR("skb_tailroom(skb)=%x,len=%x\n",skb_tailroom(skb),netdev->mtu + ETHERNET_PACKET_EXTRA);
		dma_addr = dma_map_single(dev,skb->data,BUS_SIZE_ALIGN(netdev->mtu + ETHERNET_PACKET_EXTRA),PCI_DMA_FROMDEVICE);
		status = synopGMAC_set_rx_qptr(gmacdev,dma_addr, BUS_SIZE_ALIGN(netdev->mtu + ETHERNET_PACKET_EXTRA), (u64)skb,0,0,0);
		if(status < 0)
			dev_kfree_skb(skb);
			
	}while(status >= 0);
	

	TR("Setting up the cable unplug timer\n");
	init_timer(&adapter->synopGMAC_cable_unplug_timer);  //lv
	adapter->synopGMAC_cable_unplug_timer.function = (void *)synopGMAC_linux_cable_unplug_function; //lv
	adapter->synopGMAC_cable_unplug_timer.data = (ulong) adapter;  //lv
	adapter->synopGMAC_cable_unplug_timer.expires = CHECK_TIME + jiffies; //lv
	add_timer(&adapter->synopGMAC_cable_unplug_timer); //lv

	synopGMAC_clear_interrupt(gmacdev);
	/*
	Disable the interrupts generated by MMC and IPC counters.
	If these are not disabled ISR should be modified accordingly to handle these interrupts.
	*/	
	synopGMAC_disable_mmc_tx_interrupt(gmacdev, 0xFFFFFFFF);
	synopGMAC_disable_mmc_rx_interrupt(gmacdev, 0xFFFFFFFF);
	synopGMAC_disable_mmc_ipc_rx_interrupt(gmacdev, 0xFFFFFFFF);

	synopGMAC_enable_interrupt(gmacdev,DmaIntEnable);
	synopGMAC_enable_dma_rx(gmacdev);
	synopGMAC_enable_dma_tx(gmacdev);
	
	netif_start_queue(netdev);

	return retval;

error_in_irq:
	/*Lets free the allocated memory*/
	plat_free_memory(gmacdev);
	return -ESYNOPGMACBUSY;
}

/**
 * Function used when the interface is closed.
 *
 * This function is registered to linux stop() function. This function is 
 * called whenever ifconfig (in Linux) closes the device (for example "ifconfig eth0 down").
 * This releases all the system resources allocated during open call.
 * system resources int needs 
 * 	- Disable the device interrupts
 * 	- Stop the receiver and get back all the rx descriptors from the DMA
 * 	- Stop the transmitter and get back all the tx descriptors from the DMA 
 * 	- Stop the Linux network queue interface
 *	- Free the irq (ISR registered is removed from the kernel)
 * 	- Release the TX and RX descripor memory
 *	- De-initialize one second timer rgistered for cable plug/unplug tracking
 * @param[in] pointer to net_device structure. 
 * \return Returns 0 on success and error status upon failure.
 * \callgraph
 */

s32 synopGMAC_linux_close(struct net_device *netdev)
{
	
	synopGMACPciNetworkAdapter *adapter;
        synopGMACdevice * gmacdev;
	struct device *dev;
	
	TR("%s\n",__FUNCTION__);
#if 0
#if CONFIG_LS1B_GMAC0_OPEN && CONFIG_LS1B_GMAC1_OPEN//close gmac0 and gmac1  
  printk("close gmac0 and gmac1.\n");
  (*(volatile unsigned int *)0xbfd00420) &= ~(1 << 4 | 1 << 3);
  (*(volatile unsigned int *)0xbfd00424) &= ~(0xf);

#elif (CONFIG_LS1B_GMAC0_OPEN) && (~CONFIG_LS1B_GMAC1_OPEN)//close gmac0,open gmac1
  printk("close gmac0 open gmac1.\n");
  (*(volatile unsigned int *)0xbfd00424) &= ~(1 << 0 | 1 << 2); //close gmac0
  (*(volatile unsigned int *)0xbfd00424) |= (1 << 1 | 1 << 3); //open gmac1
  (*(volatile unsigned int *)0xbfd00420) |= (1 << 3 | 1 << 4);  //close uart0/1

#elif (~CONFIG_LS1BGMAC0_OPEN) && (CONFIG_LS1B_GMAC1_OPEN) //open gmac0,close gmac 1
  printk("open gmac0 close gmac1.\n");
  (*(volatile unsigned int *)0xbfd00424) |= (1 << 0 | 1 << 2); //open gmac0
  (*(volatile unsigned int *)0xbfd00424) &= ~(1 << 1 | 1 << 3); //close gmac1
  (*(volatile unsigned int *)0xbfd00420) &= ~(1 << 3 | 1 <<4); //open uart0/1
#endif
#endif
	adapter = (synopGMACPciNetworkAdapter *) netdev_priv(netdev);
	if(adapter == NULL){
		TR0("OOPS adapter is null\n");
		return -1;
	}

	gmacdev = (synopGMACdevice *) adapter->synopGMACdev;
	if(gmacdev == NULL){
		TR0("OOPS gmacdev is null\n");
		return -1;
	}

	dev = adapter->dev;
	if(dev == NULL){
		TR("OOPS dev is null\n");
		return -1;
	}

	/*Disable all the interrupts*/
	synopGMAC_disable_interrupt_all(gmacdev);
	TR("the synopGMAC interrupt has been disabled\n");

	/*Disable the reception*/	
	synopGMAC_disable_dma_rx(gmacdev);
        synopGMAC_take_desc_ownership_rx(gmacdev);
	TR("the synopGMAC Reception has been disabled\n");

	/*Disable the transmission*/
	synopGMAC_disable_dma_tx(gmacdev);
        synopGMAC_take_desc_ownership_tx(gmacdev);

	TR("the synopGMAC Transmission has been disabled\n");
	netif_stop_queue(netdev);
	/*Now free the irq: This will detach the interrupt handler registered*/
	free_irq(adapter->irq, netdev);
	TR("the synopGMAC interrupt handler has been removed\n");
	
	/*Free the Rx Descriptor contents*/
	TR("Now calling synopGMAC_giveup_rx_desc_queue \n");
#ifndef USE_CHAIN_MODE
	synopGMAC_giveup_rx_desc_queue(gmacdev, dev, RINGMODE);
#else
	synopGMAC_giveup_rx_desc_queue(gmacdev, dev, CHAINMODE);
#endif
	TR("Now calling synopGMAC_giveup_tx_desc_queue \n");
#ifndef USE_CHAIN_MODE
	synopGMAC_giveup_tx_desc_queue(gmacdev, dev, RINGMODE);
#else
	synopGMAC_giveup_tx_desc_queue(gmacdev, dev, CHAINMODE);
#endif
	
	TR("Freeing the cable unplug timer\n");	
	del_timer(&adapter->synopGMAC_cable_unplug_timer);

	return -ESYNOPGMACNOERR;

}

/**
 * Function to transmit a given packet on the wire.
 * Whenever Linux Kernel has a packet ready to be transmitted, this function is called.
 * The function prepares a packet and prepares the descriptor and 
 * enables/resumes the transmission.
 * @param[in] pointer to sk_buff structure. 
 * @param[in] pointer to net_device structure.
 * \return Returns 0 on success and Error code on failure. 
 * \note structure sk_buff is used to hold packet in Linux networking stacks.
 */
s32 synopGMAC_linux_xmit_frames(struct sk_buff *skb, struct net_device *netdev)
{
	s32 status = 0;
	u32 offload_needed = 0;
	u32 dma_addr;
	synopGMACPciNetworkAdapter *adapter;
	synopGMACdevice * gmacdev;
	struct device * dev;

	TR("%s called \n",__FUNCTION__);

	if(skb == NULL){
		TR0("skb is NULL What happened to Linux Kernel? \n ");
		return -1;
	}
	
	adapter = (synopGMACPciNetworkAdapter *) netdev_priv(netdev);
	if(adapter == NULL)
		return -1;

	gmacdev = (synopGMACdevice *) adapter->synopGMACdev;
	if(gmacdev == NULL)
		return -1;
//printk("%s: gmacdev is %p.\n",__FUNCTION__,gmacdev);
 	dev  = adapter->dev;	
	/*Stop the network queue*/	
	netif_stop_queue(netdev); 

		
	if(skb->ip_summed == CHECKSUM_PARTIAL){
		TR0("ip_summed=%x\n",skb->ip_summed);
		/*	
		   In Linux networking, if kernel indicates skb->ip_summed = CHECKSUM_HW, then only checksum offloading should be performed
		   Make sure that the OS on which this code runs have proper support to enable offloading.
		*/
		offload_needed = 0x00000001;
		#if 0
		printk(KERN_CRIT"skb->ip_summed = CHECKSUM_HW\n");
		printk(KERN_CRIT"skb->h.th=%08x skb->h.th->check=%08x\n",(u32)(skb->h.th),(u32)(skb->h.th->check));
		printk(KERN_CRIT"skb->h.uh=%08x skb->h.uh->check=%08x\n",(u32)(skb->h.uh),(u32)(skb->h.uh->check));
		printk(KERN_CRIT"\n skb->len = %d skb->mac_len = %d skb->data = %08x skb->csum = %08x skb->h.raw = %08x\n",skb->len,skb->mac_len,(u32)(skb->data),skb->csum,(u32)(skb->h.raw));
		printk(KERN_CRIT"DST MAC addr:%02x %02x %02x %02x %02x %02x\n",*(skb->data+0),*(skb->data+1),*(skb->data+2),*(skb->data+3),*(skb->data+4),*(skb->data+5));
		printk(KERN_CRIT"SRC MAC addr:%02x %02x %02x %02x %02x %02x\n",*(skb->data+6),*(skb->data+7),*(skb->data+8),*(skb->data+9),*(skb->data+10),*(skb->data+11));
		printk(KERN_CRIT"Len/type    :%02x %02x\n",*(skb->data+12),*(skb->data+13));
		if(((*(skb->data+14)) & 0xF0) == 0x40){
			printk(KERN_CRIT"IPV4 Header:\n");
			printk(KERN_CRIT"%02x %02x %02x %02x\n",*(skb->data+14),*(skb->data+15),*(skb->data+16),*(skb->data+17));
			printk(KERN_CRIT"%02x %02x %02x %02x\n",*(skb->data+18),*(skb->data+19),*(skb->data+20),*(skb->data+21));
			printk(KERN_CRIT"%02x %02x %02x %02x\n",*(skb->data+22),*(skb->data+23),*(skb->data+24),*(skb->data+25));
			printk(KERN_CRIT"%02x %02x %02x %02x\n",*(skb->data+26),*(skb->data+27),*(skb->data+28),*(skb->data+29));
			printk(KERN_CRIT"%02x %02x %02x %02x\n\n",*(skb->data+30),*(skb->data+31),*(skb->data+32),*(skb->data+33));
			for(counter = 34; counter < skb->len; counter++)
				printk("%02X ",*(skb->data + counter));
		}
		else{
			printk(KERN_CRIT"IPV6 FRAME:\n");
			for(counter = 14; counter < skb->len; counter++)
				printk("%02X ",*(skb->data + counter));
		}
		#endif
	}

#ifdef CONFIG_FIX_COHERENT_UNALIGNED
	if(((long)skb->data)&0x10)
	{
		struct sk_buff *oldskb;
		oldskb=skb;
		skb = alloc_skb(oldskb->len,GFP_ATOMIC);

			skb_copy_from_linear_data (oldskb, skb->data, oldskb->len);
			skb_put (skb, oldskb->len);
		
			dev_kfree_skb(oldskb);
	}
#endif

	#if 0
	printk("---------sending data----------.\n");
        int counter;
        printk(KERN_CRIT"skb->ip_summed = CHECKSUM_HW\n");                                                                                     
//      printk(KERN_CRIT"skb->h.th=%08x skb->h.th->check=%08x\n",(u32)(skb->h.th),(u32)(skb->h.th->check));                                    
//      printk(KERN_CRIT"skb->h.uh=%08x skb->h.uh->check=%08x\n",(u32)(skb->h.uh),(u32)(skb->h.uh->check));
        printk(KERN_CRIT"\n skb->len = %d skb->mac_len = %d skb->data = %08x skb->csum = %08x skb->h.raw = %08x\n",skb->len,skb->mac_len,(u32)(skb->data),skb->csum,(u32)(skb->h.raw));
        printk(KERN_CRIT"DST MAC addr:%02x %02x %02x %02x %02x %02x\n",*(skb->data+0),*(skb->data+1),*(skb->data+2),*(skb->data+3),*(skb->data+4),*(skb->data+5));
        printk(KERN_CRIT"SRC MAC addr:%02x %02x %02x %02x %02x %02x\n",*(skb->data+6),*(skb->data+7),*(skb->data+8),*(skb->data+9),*(skb->data+10),*(skb->data+11));
        printk(KERN_CRIT"Len/type    :%02x %02x\n",*(skb->data+12),*(skb->data+13));                                                           
        if(((*(skb->data+14)) & 0xF0) == 0x40){
		printk(KERN_CRIT"IPV4 Header:\n");
		printk(KERN_CRIT"%02x %02x %02x %02x\n",*(skb->data+14),*(skb->data+15),*(skb->data+16),*(skb->data+17));                      
		printk(KERN_CRIT"%02x %02x %02x %02x\n",*(skb->data+18),*(skb->data+19),*(skb->data+20),*(skb->data+21));
		printk(KERN_CRIT"%02x %02x %02x %02x\n",*(skb->data+22),*(skb->data+23),*(skb->data+24),*(skb->data+25));                      
		printk(KERN_CRIT"%02x %02x %02x %02x\n",*(skb->data+26),*(skb->data+27),*(skb->data+28),*(skb->data+29));
		printk(KERN_CRIT"%02x %02x %02x %02x\n\n",*(skb->data+30),*(skb->data+31),*(skb->data+32),*(skb->data+33));                    
		for(counter = 34; counter < skb->len; counter++)
                                printk("%02X ",*(skb->data + counter));                                                                                
        }else{
		printk(KERN_CRIT"IPV6 FRAME:\n");
               	for(counter = 14; counter < skb->len; counter++)                                                                               
			printk("%02X ",*(skb->data + counter));
        }             
        #endif

	
	/*Now we have skb ready and OS invoked this function. Lets make our DMA know about this*/
	dma_addr = dma_map_single(dev,skb->data,skb->len,PCI_DMA_TODEVICE);
	status = synopGMAC_set_tx_qptr(gmacdev, dma_addr, skb->len, (u64)skb,0,0,0,offload_needed);
	if(status < 0){
		TR0("%s No More Free Tx Descriptors\n",__FUNCTION__);
		dev_kfree_skb (skb); //with this, system used to freeze.. ??
		return -EBUSY;
	}
	
	/*Now force the DMA to start transmission*/	
	synopGMAC_resume_dma_tx(gmacdev);
	netdev->trans_start = jiffies;
	
	/*Now start the netdev queue*/
	netif_wake_queue(netdev);
	
	return -ESYNOPGMACNOERR;
}

/**
 * Function provides the network interface statistics.
 * Function is registered to linux get_stats() function. This function is 
 * called whenever ifconfig (in Linux) asks for networkig statistics
 * (for example "ifconfig eth0").
 * @param[in] pointer to net_device structure. 
 * \return Returns pointer to net_device_stats structure.
 * \callgraph
 */
struct net_device_stats *  synopGMAC_linux_get_stats(struct net_device *netdev)
{
TR("%s called \n",__FUNCTION__);
return( &(((synopGMACPciNetworkAdapter *)(netdev_priv(netdev)))->synopGMACNetStats) );
}

/**
 * Function to set multicast and promiscous mode.
 * @param[in] pointer to net_device structure. 
 * \return returns void.
 */
void synopGMAC_linux_set_multicast_list(struct net_device *netdev)
{
synopGMACPciNetworkAdapter *adapter;
synopGMACdevice * gmacdev;
adapter = (synopGMACPciNetworkAdapter *) netdev_priv(netdev);
gmacdev = adapter->synopGMACdev;

if(netdev->flags & IFF_PROMISC)
{
synopGMAC_promisc_enable(gmacdev);
}
else
{
synopGMAC_promisc_disable(gmacdev);
}
TR("%s called \n",__FUNCTION__);
//todo Function not yet implemented.
return;
}

/**
 * Function to set ethernet address of the NIC.
 * @param[in] pointer to net_device structure. 
 * @param[in] pointer to an address structure. 
 * \return Returns 0 on success Errorcode on failure.
 */
s32 synopGMAC_linux_set_mac_address(struct net_device *netdev, void * macaddr)
{

synopGMACPciNetworkAdapter *adapter = NULL;
synopGMACdevice * gmacdev = NULL;
struct sockaddr *addr = macaddr;

adapter = (synopGMACPciNetworkAdapter *) netdev_priv(netdev);
if(adapter == NULL)
	return -1;

gmacdev = adapter->synopGMACdev;
if(gmacdev == NULL)
	return -1;

if(!is_valid_ether_addr(addr->sa_data))
	return -EADDRNOTAVAIL;

synopGMAC_set_mac_addr(gmacdev,GmacAddr0High,GmacAddr0Low, addr->sa_data); 
synopGMAC_get_mac_addr(gmacdev,GmacAddr0High,GmacAddr0Low, netdev->dev_addr); 

TR("%s called \n",__FUNCTION__);
return 0;
}

/**
 * Function to change the Maximum Transfer Unit.
 * @param[in] pointer to net_device structure. 
 * @param[in] New value for maximum frame size.
 * \return Returns 0 on success Errorcode on failure.
 */
s32 synopGMAC_linux_change_mtu(struct net_device *netdev, s32 newmtu)
{
TR("%s called \n",__FUNCTION__);
//todo Function not yet implemented.
return 0;

}

/**
 * IOCTL interface.
 * This function is mainly for debugging purpose.
 * This provides hooks for Register read write, Retrieve descriptor status
 * and Retreiving Device structure information.
 * @param[in] pointer to net_device structure. 
 * @param[in] pointer to ifreq structure.
 * @param[in] ioctl command. 
 * \return Returns 0 on success Error code on failure.
 */
s32 synopGMAC_linux_do_ioctl(struct net_device *netdev, struct ifreq *ifr, s32 cmd)
{
s32 retval = 0;
u16 temp_data = 0;
synopGMACPciNetworkAdapter *adapter = NULL;
synopGMACdevice * gmacdev = NULL;
struct ifr_data_struct
{
	u32 unit;
	u32 addr;
	u32 data;
} *req;


if(netdev == NULL)
	return -1;
if(ifr == NULL)
	return -1;

req = (struct ifr_data_struct *)ifr->ifr_data;

adapter = (synopGMACPciNetworkAdapter *) netdev_priv(netdev);
if(adapter == NULL)
	return -1;

gmacdev = adapter->synopGMACdev;
if(gmacdev == NULL)
	return -1;
//TR("%s :: on device %s req->unit = %08x req->addr = %08x req->data = %08x cmd = %08x \n",__FUNCTION__,netdev->name,req->unit,req->addr,req->data,cmd);

switch(cmd)
{
	case IOCTL_READ_REGISTER:		//IOCTL for reading IP registers : Read Registers
		if      (req->unit == 0)	// Read Mac Register
			req->data = synopGMACReadReg((u32 *)gmacdev->MacBase,req->addr);
		else if (req->unit == 1)	// Read DMA Register
			req->data = synopGMACReadReg((u32 *)gmacdev->DmaBase,req->addr);
		else if (req->unit == 2){	// Read Phy Register
			retval = synopGMAC_read_phy_reg((u32 *)gmacdev->MacBase,gmacdev->PhyBase,req->addr,&temp_data);
			req->data = (u32)temp_data;
			if(retval != -ESYNOPGMACNOERR)
				TR("ERROR in Phy read\n");	
		}
		break;

	case IOCTL_WRITE_REGISTER:		//IOCTL for reading IP registers : Read Registers
		if      (req->unit == 0)	// Write Mac Register
			synopGMACWriteReg((u32 *)gmacdev->MacBase,req->addr,req->data);
		else if (req->unit == 1)	// Write DMA Register
			synopGMACWriteReg((u32 *)gmacdev->DmaBase,req->addr,req->data);
		else if (req->unit == 2){	// Write Phy Register
			retval = synopGMAC_write_phy_reg((u32 *)gmacdev->MacBase,gmacdev->PhyBase,req->addr,req->data);
			if(retval != -ESYNOPGMACNOERR)
				TR("ERROR in Phy read\n");	
		}
		break;

	case IOCTL_READ_IPSTRUCT:		//IOCTL for reading GMAC DEVICE IP private structure
	        memcpy(ifr->ifr_data, gmacdev, sizeof(synopGMACdevice));
		break;

	case IOCTL_READ_RXDESC:			//IOCTL for Reading Rx DMA DESCRIPTOR
		memcpy(ifr->ifr_data, gmacdev->RxDesc + ((DmaDesc *) (ifr->ifr_data))->data1, sizeof(DmaDesc) );
		break;

	case IOCTL_READ_TXDESC:			//IOCTL for Reading Tx DMA DESCRIPTOR
		memcpy(ifr->ifr_data, gmacdev->TxDesc + ((DmaDesc *) (ifr->ifr_data))->data1, sizeof(DmaDesc) );
		break;
	case IOCTL_POWER_DOWN:
		if	(req->unit == 1){	//power down the mac
			TR("============I will Power down the MAC now =============\n");
			// If it is already in power down don't power down again
			retval = 0;
			if(((synopGMACReadReg((u32 *)gmacdev->MacBase,GmacPmtCtrlStatus)) & GmacPmtPowerDown) != GmacPmtPowerDown){
			synopGMAC_linux_powerdown_mac(gmacdev);			
			retval = 0;
			}
		}
		if	(req->unit == 2){	//Disable the power down  and wake up the Mac locally
			TR("============I will Power up the MAC now =============\n");
			//If already powered down then only try to wake up
			retval = -1;
			if(((synopGMACReadReg((u32 *)gmacdev->MacBase,GmacPmtCtrlStatus)) & GmacPmtPowerDown) == GmacPmtPowerDown){
			synopGMAC_power_down_disable(gmacdev);
			synopGMAC_linux_powerup_mac(gmacdev);
			retval = 0;
			}
		}
		break;
	default:
		retval = -1;

}


return retval;
}

/**
 * Function to handle a Tx Hang.
 * This is a software hook (Linux) to handle transmitter hang if any.
 * We get transmitter hang in the device interrupt status, and is handled
 * in ISR. This function is here as a place holder.
 * @param[in] pointer to net_device structure 
 * \return void.
 */
void synopGMAC_linux_tx_timeout(struct net_device *netdev)
{
	TR0("%s called \n",__FUNCTION__);

	void __iomem *mac_addr;
	synopGMACdevice * gmacdev;

	//todo Function not yet implemented
	synopGMACPciNetworkAdapter *adapter = NULL;

	if((adapter = (synopGMACPciNetworkAdapter *) netdev_priv(netdev)) == NULL){
		TR0("%s : get adapter struct failed.\n", __FUNCTION__);
		return;
	}

	gmacdev = (synopGMACdevice *)adapter->synopGMACdev;

	synopGMAC_get_mac_addr(adapter->synopGMACdev,GmacAddr0High,GmacAddr0Low, netdev->dev_addr); 
        mac_addr = netdev->dev_addr;

	synopGMAC_disable_interrupt_all(gmacdev);

	netif_stop_queue(netdev);
	
	synopGMAC_reset(gmacdev);

        synopGMAC_mac_init(gmacdev);

	synopGMAC_set_mac_addr(gmacdev,GmacAddr0High,GmacAddr0Low, mac_addr);

	netif_wake_queue(netdev);

        synopGMAC_enable_interrupt(gmacdev,DmaIntEnable);

	return;
}

#define GMAC_NET_STATS_LEN	21
#define GMAC_STATS_LEN	ARRAY_SIZE(gmac_gstrings_stats)
static const char gmac_gstrings_stats[][ETH_GSTRING_LEN] = {
	"rx_packets", "tx_packets", "rx_bytes", "tx_bytes", "rx_errors",
	"tx_errors", "rx_dropped", "tx_dropped", "multicast", "collisions",
	"rx_length_errors", "rx_over_errors", "rx_crc_errors",
	"rx_frame_errors", "rx_fifo_errors", "rx_missed_errors",
	"tx_aborted_errors", "tx_carrier_errors", "tx_fifo_errors",
	"tx_heartbeat_errors", "tx_window_errors",
	/* device-specific stats */
	"tx_deferred", "tx_single_collisions", "tx_multi_collisions",
	"tx_flow_control_pause", "rx_flow_control_pause",
	"rx_flow_control_unsupported", "tx_tco_packets", "rx_tco_packets",
};

static int mdio_read(struct net_device *netdev, int addr, int reg)
{
	synopGMACPciNetworkAdapter *adapter;
	synopGMACdevice * gmacdev;
	u16 data;
	adapter = netdev_priv(netdev);
	gmacdev = adapter->synopGMACdev;
	
	synopGMAC_read_phy_reg((u32 *)gmacdev->MacBase,addr,reg, &data);
	return data;
}

static void mdio_write(struct net_device *netdev, int addr, int reg, int data)
{
	synopGMACPciNetworkAdapter *adapter;
	synopGMACdevice * gmacdev;
	adapter = netdev_priv(netdev);
	gmacdev = adapter->synopGMACdev;
	synopGMAC_write_phy_reg((u32 *)gmacdev->MacBase,addr,reg,data);
}

static void gmac_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
#define DRV_NAME "gmac"
#define DRV_VERSION "0.1"
	strcpy(info->driver, DRV_NAME);
	strcpy(info->version, DRV_VERSION);
	strcpy(info->bus_info, "platform");
	info->regdump_len = 256;
}

static int gmac_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	synopGMACPciNetworkAdapter *adapter = (synopGMACPciNetworkAdapter *)netdev_priv(dev);
	mii_ethtool_gset(&adapter->mii, cmd);
	return 0;
}

static int gmac_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	synopGMACPciNetworkAdapter *adapter = (synopGMACPciNetworkAdapter *)netdev_priv(dev);
	synopGMACdevice            *gmacdev = adapter->synopGMACdev;
	int rc;
	mdio_write(dev, adapter->mii.phy_id, MII_BMCR, BMCR_RESET);
	rc = mii_ethtool_sset(&adapter->mii, cmd);
	return rc;
}

static int gmac_get_regs_len(struct net_device *dev)
{
	return DMABASE - MACBASE + 0x58;
}

static void gmac_get_regs(struct net_device *dev, struct ethtool_regs *regs, void *regbuf)
{
	synopGMACPciNetworkAdapter *adapter = (synopGMACPciNetworkAdapter *)netdev_priv(dev);
	synopGMACdevice            *gmacdev = adapter->synopGMACdev;
	int i;
	u32 data;
	for(i = 0;i < regs->len; i += 4)
	{
	data=readl(gmacdev->MacBase + i);
	memcpy(regbuf + i,&data,4);
	}
}



static int gmac_nway_reset(struct net_device *dev)
{
	synopGMACPciNetworkAdapter *adapter =(synopGMACPciNetworkAdapter *)netdev_priv(dev);
	return mii_nway_restart(&adapter->mii);
}

static u32 gmac_get_link(struct net_device *dev)
{
	synopGMACPciNetworkAdapter *adapter =(synopGMACPciNetworkAdapter *)netdev_priv(dev);
	return mii_link_ok(&adapter->mii);
}

static int gmac_get_sset_count(struct net_device *dev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return GMAC_STATS_LEN;
	default:
		return -EOPNOTSUPP;
	}
}

static void gmac_get_ethtool_stats(struct net_device *dev, struct ethtool_stats *stats, u64 *data)
{
//	int i;

//	for(i = 0; i < GMAC_NET_STATS_LEN; i++)
//		data[i] = ((unsigned long *)&dev->stats)[i];
}


static void gmac_get_strings(struct net_device *netdev, u32 stringset, u8 *data)
{
	switch(stringset) {
	case ETH_SS_STATS:
		memcpy(data, *gmac_gstrings_stats, sizeof(gmac_gstrings_stats));
		break;
	}
}


static const struct ethtool_ops gmac_ethtool_ops = {
	.get_drvinfo		= gmac_get_drvinfo,
	.get_settings		= gmac_get_settings,
	.set_settings		= gmac_set_settings,
	.get_regs_len		= gmac_get_regs_len,
	.get_regs		= gmac_get_regs,
	.nway_reset		= gmac_nway_reset,
	.get_link		= gmac_get_link,
	.get_strings		= gmac_get_strings,
	//.get_sset_count		= gmac_get_sset_count,
	.get_ethtool_stats	= gmac_get_ethtool_stats,
};

/*
 * process the kernel start param
 */

static int ether_set=0;
static char hwaddr[ETH_ALEN]=DEFAULT_MAC_ADDRESS;
static int __init setether(char *str)
{
	int i;
	for(i=0;i<6;i++,str+=3)
		hwaddr[i]=simple_strtoul(str,0,16);
	
	ether_set=1;
	
	return 1;
}

__setup("etheraddr=", setether);


static const struct net_device_ops gmac_netdev_ops = {		//lxy
	.ndo_open		= synopGMAC_linux_open,
	.ndo_stop		= synopGMAC_linux_close,
	.ndo_start_xmit		= synopGMAC_linux_xmit_frames,
	.ndo_tx_timeout		= synopGMAC_linux_tx_timeout,
	.ndo_set_multicast_list	= synopGMAC_linux_set_multicast_list,
	.ndo_do_ioctl		= synopGMAC_linux_do_ioctl,
	.ndo_change_mtu		= synopGMAC_linux_change_mtu,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address	= synopGMAC_linux_set_mac_address,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= poll_gmac,
#endif
};



/**
 * Function to initialize the Linux network interface.
 * 
 * Linux dependent Network interface is setup here. This provides 
 * an example to handle the network dependent functionality.
 *
 * \return Returns 0 on success and Error code on failure.
 */
void * __init synopGMAC_init_network_interface(struct device *dev,u8* synopGMACMappedAddr,u32 synopGMACMappedAddrSize,int irq)
{
	s32 err;
	struct net_device *netdev;
	synopGMACPciNetworkAdapter *synopGMACadapter;

	TR("Now Going to Call register_netdev to register the network interface for GMAC core\n");
	/*
	Lets allocate and set up an ethernet device, it takes the sizeof the private structure. This is mandatory as a 32 byte 
	allignment is required for the private data structure.
	*/
	netdev = alloc_etherdev(sizeof(synopGMACPciNetworkAdapter));
	if(!netdev){
	err = -ESYNOPGMACNOMEM;
	goto err_alloc_etherdev;
	}
	
	synopGMACadapter = (synopGMACPciNetworkAdapter *)netdev_priv(netdev);
	synopGMACadapter->synopGMACnetdev = netdev;
	synopGMACadapter->dev = dev;
	synopGMACadapter->synopGMACdev    = NULL;
    	synopGMACadapter->synopGMACMappedAddr=synopGMACMappedAddr;
	synopGMACadapter->synopGMACMappedAddrSize=synopGMACMappedAddrSize;
    	synopGMACadapter->irq=irq;

	
	/*Allocate Memory for the the GMACip structure*/
	synopGMACadapter->synopGMACdev = (synopGMACdevice *) plat_alloc_memory(sizeof (synopGMACdevice));
	if(!synopGMACadapter->synopGMACdev){
		TR0("Error in Memory Allocataion \n");
		return -ENOMEM;
	}

	if(1){
		static int index=0;
		if(!ether_set)
		{
			get_random_bytes(&hwaddr[3], 3);
			ether_set = 1;
		}	
		hwaddr[5] += index;
		memcpy(netdev->dev_addr,hwaddr,6);
		index++;
	}

	/*Attach the device to MAC struct This will configure all the required base addresses
	  such as Mac base, configuration base, phy base address(out of 32 possible phys )*/
	synopGMAC_attach(synopGMACadapter->synopGMACdev, synopGMACMappedAddr + MACBASE, synopGMACMappedAddr + DMABASE, DEFAULT_PHY_BASE,netdev->dev_addr);

	synopGMAC_reset(synopGMACadapter->synopGMACdev);

	if(synop_pci_using_dac){
		TR("netdev->features = %08lx\n",netdev->features);
		TR("synop_pci_using dac is %08x\n",synop_pci_using_dac);
		netdev->features |= NETIF_F_HIGHDMA;
		TR("netdev->features = %08lx\n",netdev->features);
	}
	
	netdev->netdev_ops	= &gmac_netdev_ops;
	netdev->watchdog_timeo = 5 * HZ;
	SET_ETHTOOL_OPS(netdev, &gmac_ethtool_ops);

	/* MII setup */
	synopGMACadapter->mii.phy_id_mask = 0x1F;
	synopGMACadapter->mii.reg_num_mask = 0x1F;
	synopGMACadapter->mii.dev = netdev;
	synopGMACadapter->mii.mdio_read = mdio_read;
	synopGMACadapter->mii.mdio_write = mdio_write;
	synopGMACadapter->mii.phy_id = synopGMACadapter->synopGMACdev->PhyBase;
	synopGMACadapter->mii.supports_gmii = mii_check_gmii_support(&synopGMACadapter->mii);

			


	/*Now start the network interface*/
	TR("Now Registering the netdevice\n");
	if((err = register_netdev(netdev)) != 0) {
		TR0("Error in Registering netdevice\n");
		return -EFAULT;
	}
  
 	return synopGMACadapter;

err_alloc_etherdev:
	TR0("Problem in alloc_etherdev()..Take Necessary action\n");
	return -EFAULT;
}



/*
module_init(synopGMAC_init_network_interface);
module_exit(synopGMAC_exit_network_interface);

MODULE_AUTHOR("Synopsys India");
MODULE_LICENSE("GPL/BSD");
MODULE_DESCRIPTION("SYNOPSYS GMAC DRIVER Network INTERFACE");

EXPORT_SYMBOL(synopGMAC_init_pci_bus_interface);
*/
