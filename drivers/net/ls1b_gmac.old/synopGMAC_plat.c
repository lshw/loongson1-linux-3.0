/**\file
 *  This file defines the wrapper for the platform/OS related functions
 *  The function definitions needs to be modified according to the platform 
 *  and the Operating system used.
 *  This file should be handled with greatest care while porting the driver
 *  to a different platform running different operating system other than
 *  Linux 2.6.xx.
 * \internal
 * ----------------------------REVISION HISTORY-----------------------------
 * Synopsys			01/Aug/2007			Created
 */
 
#include "synopGMAC_plat.h"

/**
  * This is a wrapper function for Memory allocation routine. In linux Kernel 
  * it it kmalloc function
  * @param[in] bytes in bytes to allocate
  */

void *plat_alloc_memory(u32 bytes) 
{
	return kmalloc((size_t)bytes, GFP_KERNEL);
}

/**
  * This is a wrapper function for consistent dma-able Memory allocation routine. 
  * In linux Kernel, it depends on pci dev structure
  * @param[in] bytes in bytes to allocate
  */

void *plat_alloc_consistent_dmaable_memory(struct device *dev, u32 size, dma_addr_t *addr) 
{
#ifdef CONFIG_GMAC_COHERENT
 return (dma_alloc_noncoherent(dev,size,addr,GFP_ATOMIC));
#else
 return (dma_alloc_coherent(dev,size,addr,GFP_ATOMIC));
#endif
}

/**
  * This is a wrapper function for freeing consistent dma-able Memory.
  * In linux Kernel, it depends on pci dev structure
  * @param[in] bytes in bytes to allocate
  */

void plat_free_consistent_dmaable_memory(struct device *dev, u32 size, void * addr,u32 dma_addr) 
{
#ifdef CONFIG_GMAC_COHERENT
 dma_free_noncoherent (dev,size,addr,dma_addr);
#else
 dma_free_coherent (dev,size,addr,dma_addr);
#endif
 return;
}



/**
  * This is a wrapper function for Memory free routine. In linux Kernel 
  * it it kfree function
  * @param[in] buffer pointer to be freed
  */
void plat_free_memory(void *buffer) 
{
	kfree(buffer);
	return ;
}


/**
  * This is a wrapper function for platform dependent delay 
  * Take care while passing the argument to this function 
  * @param[in] buffer pointer to be freed
  */
void plat_delay(u32 delay)
{
	while (delay--);
	return;
}



