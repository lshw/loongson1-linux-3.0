#
# Makefile for DWC_otg Highspeed USB controller driver
#

# Use the BUS_INTERFACE variable to compile the software for either 
# PCI(PCI_INTERFACE) or LM(LM_INTERFACE) bus.
BUS_INTERFACE	= -DLM_INTERFACE

PORTLIB		:= drivers/usb/gadget/dwc/dwc_common_port
EXTRA_CFLAGS	+= -DDEBUG  -DVERBOSE

# Use one of the following flags to compile the software in host-only or
# device-only mode.
#EXTRA_CFLAGS        += -DDWC_HOST_ONLY
#EXTRA_CFLAGS        += -DDWC_DEVICE_ONLY

EXREA_CFLAGS	+= -Dlinux -DDWC_HS_ELECT_TST
#EXTRA_CFLAGS	+= -DDWC_EN_ISOC
EXTRA_CFLAGS   	+= -I$(PORTLIB)
EXTRA_CFLAGS   	+= -DDWC_LINUX
EXTRA_CFLAGS   	+= $(CFI)
EXTRA_CFLAGS	+= $(BUS_INTERFACE)
EXTRA_CFLAGS	+= -DDWC_DEV_SRPCAP

obj-y	:= dwc_otg.o lm.o

dwc_otg-objs	:= dwc_otg_driver.o dwc_otg_attr.o 
dwc_otg-objs	+= dwc_otg_cil.o dwc_otg_cil_intr.o 
dwc_otg-objs	+= dwc_otg_pcd_linux.o dwc_otg_pcd.o dwc_otg_pcd_intr.o 
dwc_otg-objs	+= dwc_otg_hcd.o dwc_otg_hcd_linux.o dwc_otg_hcd_intr.o dwc_otg_hcd_queue.o dwc_otg_hcd_ddma.o
dwc_otg-objs	+= dwc_otg_adp.o
ifneq ($(CFI),)
dwc_otg-objs	+= dwc_otg_cfi.o
endif
