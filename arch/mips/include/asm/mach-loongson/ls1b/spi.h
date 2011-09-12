/* linux/include/asm-mips/sb2f-board/spi.h
*/

#ifndef __ASM_ARCH_SPI_H
#define __ASM_ARCH_SPI_H __FILE__
struct ls1b_spi_info;
struct spi_board_info;

struct ls1b_spi_info {
//	unsigned int		 pin_cs;	/* simple gpio cs */
	unsigned int		 num_cs;	/* total chipselects */
	int			 bus_num;       /* bus number to use. */
	unsigned int		 board_size;
	struct spi_board_info	*board_info;

//	void (*set_cs)(struct ls1b_spi_info *spi, int cs, int pol);
};

#endif /* __ASM_ARCH_SPI_H */
