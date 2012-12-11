#include "gcSdk.h"

void gcLoadImage(gcIMAGEDESCRIPTOR* ImageInfo)
{
#if 0
	gcSURFACEINFO *img_surface = &ImageInfo->surface;
	int xcount = img_surface->rect.right * img_surface->stride;
	int ycount = img_surface->rect.bottom;
	int yaddr = 0, xaddr = 0;	//THF
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
	//zgj ImageInfo->surface.address += 640*240*4;
	//zgj-2010-3-22  ImageInfo->surface.address = GPU_IMG_LOAD_ADDR;
	if(!(ImageInfo->surface.address & 0x80000000))
		ImageInfo->surface.address |= 0xA0000000;
	printf("Image addr is : %x\n", ImageInfo->surface.address);
#endif
	// Move the image to the aligned location.
//	memcpy((void*) ImageInfo->surface.address, ImageInfo->bits, ImageInfo->size);
	/* 根据图像(24bit bmp)的大小正确填充到fb缓冲区 */
	while (ycount--) {	//THF
		memcpy((void *) (ImageInfo->surface.address+yaddr), ImageInfo->bits+xaddr, xcount);
		xaddr += xcount;
		yaddr += gcDisplaySurface.stride;
	}
#endif

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

