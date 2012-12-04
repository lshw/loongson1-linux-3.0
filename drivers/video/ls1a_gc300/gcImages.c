#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include "gcSdk.h"

void gcLoadImage(gcIMAGEDESCRIPTOR* ImageInfo)
{
#if gcENABLEVIRTUAL
	UINT32 Physical;

	// Allocate surface.
	ImageInfo->surface.address = gcMemAllocateVirtual(ImageInfo->size);

	// Get the physical address.
	Physical = gcGetPhysicalAddress(ImageInfo->surface.address);

	// Move the image to the aligned location.
	memcpy((void*) Physical, ImageInfo->bits, ImageInfo->size);
#else
	// Allocate surface.
	ImageInfo->surface.address = gcMemAllocate(ImageInfo->size);

#if 1  //zgj
	//zgj ImageInfo->surface.address += 640*240*4 ;
	//zgj-2010-3-22  ImageInfo->surface.address = GPU_IMG_LOAD_ADDR  ;
	if(!(ImageInfo->surface.address & 0x80000000))
		ImageInfo->surface.address |= 0xA0000000 ;
	printk("Image addr is : %x\n", ImageInfo->surface.address);
#endif
	// Move the image to the aligned location.
	memcpy((void*) ImageInfo->surface.address, ImageInfo->bits, ImageInfo->size);
#endif
}

UINT32 gcImageWidth(gcIMAGEDESCRIPTOR* Surface)
{
	return Surface->surface.rect.right - Surface->surface.rect.left;
}

UINT32 gcImageHeight(gcIMAGEDESCRIPTOR* Surface)
{
	return Surface->surface.rect.bottom - Surface->surface.rect.top;
}

