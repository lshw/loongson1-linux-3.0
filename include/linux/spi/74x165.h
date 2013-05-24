#ifndef LINUX_SPI_74X165_H
#define LINUX_SPI_74X165_H

struct nxp_74x165_chip_platform_data {
	/* number assigned to the first GPIO */
	unsigned	base;
	/* GPIO used for latching chip(s) */
	unsigned 	latch;
	/* number of chips daisy chained */
	unsigned	daisy_chained;
};

#endif