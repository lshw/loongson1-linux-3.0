#
# Makefile for LST ls232 board.
#

obj-y	 		+= clock.o setup.o prom.o reset.o irq.o mipsdha.o dbg_io.o printf.o platform.o gpio.o mem.o
obj-$(CONFIG_HAVE_PWM) += pwm.o
obj-$(CONFIG_PCI) += pci.o

EXTRA_AFLAGS := $(CFLAGS)
