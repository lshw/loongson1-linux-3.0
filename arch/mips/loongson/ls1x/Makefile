#
# Makefile for LST ls232 board.
#

obj-y	 		+= clock.o setup.o prom.o reset.o irq.o mipsdha.o gpio.o mem.o
obj-$(CONFIG_HAVE_PWM) += pwm.o
obj-$(CONFIG_LS1X_TIMER) += time.o
obj-$(CONFIG_LS1X_HPET) += hpet-time.o
obj-$(CONFIG_PCI) += pci.o

ifdef CONFIG_LS1A_MACH
obj-$(CONFIG_SUSPEND)	+= pm.o
endif

# 1A 板型定义，如：云终端、方案核心板
obj-$(CONFIG_LS1A_CORE_BOARD) += ls1a-core/
obj-$(CONFIG_LS1A_CLOUD_TERMIAL) += ls1a-cloud/
# 1B 板型定义，如：开发板、核心板
obj-$(CONFIG_LS1B_BOARD) += ls1b-board/
obj-$(CONFIG_LS1B_CORE_BOARD) += ls1b-core/

obj-$(CONFIG_LS1C_MACH) += ls1c/

EXTRA_AFLAGS := $(CFLAGS)
