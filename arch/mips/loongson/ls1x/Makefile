#
# Makefile for LST ls232 board.
#

obj-y	 		+= clock.o setup.o prom.o reset.o irq.o mipsdha.o platform.o gpio.o mem.o
obj-$(CONFIG_HAVE_PWM) += pwm.o
obj-$(CONFIG_LS1X_TIMER) += time.o
obj-$(CONFIG_LS1X_HPET) += hpet-time.o
obj-$(CONFIG_PCI) += pci.o
obj-$(CONFIG_SUSPEND)	+= pm.o

EXTRA_AFLAGS := $(CFLAGS)
