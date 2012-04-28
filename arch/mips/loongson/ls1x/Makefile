#
# Makefile for LST ls232 board.
#

obj-y	 		+= clock.o setup.o prom.o reset.o irq.o ls1b_board-irq.o mipsdha.o dbg_io.o printf.o platform.o gpio.o mem.o
obj-$(CONFIG_PCI) += pci.o

EXTRA_AFLAGS := $(CFLAGS)
