# File: Makefile
#
# Copyright (C) 2008-2011, Marvell International Ltd. 
#
# This software file (the "File") is distributed by Marvell International 
# Ltd. under the terms of the GNU General Public License Version 2, June 1991 
# (the "License").  You may use, redistribute and/or modify this File in 
# accordance with the terms and conditions of the License, a copy of which 
# is available by writing to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
# worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
#
# A copy of the GPL is available in file gpl-2.0.txt accompanying in this 
# deliverables.
#
# THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE 
# IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE 
# ARE EXPRESSLY DISCLAIMED.  The License provides additional details about 
# this warranty disclaimer.

#CROSS_COMPILE=	mipsel-linux-
#CC=		$(CROSS_COMPILE)gcc
#LD=		$(CROSS_COMPILE)ld
#BACKUP=		/root/backup
#YMD=		`date +%Y%m%d%H%M`

#############################################################################
# Configuration Options
#############################################################################

# Debug Option
# DEBUG LEVEL n/1/2:
# n: NO DEBUG
# 1: Only PRINTM(MMSG,...), PRINTM(MFATAL,...), ...
# 2: All PRINTM()
CONFIG_DEBUG=1

# Proc debug file
CONFIG_PROC_DEBUG=y

# Enable STA mode support
CONFIG_STA_SUPPORT=y

# Enable uAP mode support
CONFIG_UAP_SUPPORT=y

# Enable WFD support
CONFIG_WFD_SUPPORT=y

# Re-association in driver
CONFIG_REASSOCIATION=y

# Manufacturing firmware support
CONFIG_MFG_CMD_SUPPORT=y

# Big-endian platform
CONFIG_BIG_ENDIAN=n







#############################################################################
# Select Platform Tools
#############################################################################

#MODEXT = ko
#EXTRA_CFLAGS += -I$(M)/mlan
#EXTRA_CFLAGS += -DLINUX

#LD += -S
#BINDIR = ../bin_usb8782

#ifeq ($(CROSS_COMPILE),)
#KERNELVERSION_X86 := 	$(shell uname -r)
#KERNELDIR=/lib/modules/$(KERNELVERSION_X86)/build
#INSTALLDIR=/lib/modules/$(KERNELVERSION_X86)/kernel/drivers/usb
#else
#KERNELDIR=/home/22/1b-linux-3.0
#INSTALLDIR=$(BINDIR)
#endif

#############################################################################
# Compiler Flags
#############################################################################

#	EXTRA_CFLAGS += -I$(KERNELDIR)/include
#ifeq ($(CROSS_COMPILE),)
#	EXTRA_CFLAGS += -DFPNUM='"62"'
#endif

obj-$(CONFIG_WM8782_LS1X):=mlan.o usb8xxx.o
ifeq ($(CONFIG_DEBUG),1)
	EXTRA_CFLAGS += -DDEBUG_LEVEL1
endif

ifeq ($(CONFIG_DEBUG),2)
	EXTRA_CFLAGS += -DDEBUG_LEVEL1
	EXTRA_CFLAGS += -DDEBUG_LEVEL2
	DBG=	-dbg
endif

ifeq ($(CONFIG_PROC_DEBUG),y)
	EXTRA_CFLAGS += -DPROC_DEBUG
	export CONFIG_PROC_DEBUG
endif

ifeq ($(CONFIG_STA_SUPPORT),y)
	EXTRA_CFLAGS += -DSTA_SUPPORT
ifeq ($(CONFIG_REASSOCIATION),y)
	EXTRA_CFLAGS += -DREASSOCIATION
endif
endif

ifeq ($(CONFIG_UAP_SUPPORT),y)
	EXTRA_CFLAGS += -DUAP_SUPPORT
endif

ifeq ($(CONFIG_WFD_SUPPORT),y)
	EXTRA_CFLAGS += -DWFD_SUPPORT
endif

ifeq ($(CONFIG_MFG_CMD_SUPPORT),y)
	EXTRA_CFLAGS += -DMFG_CMD_SUPPORT
endif

ifeq ($(CONFIG_BIG_ENDIAN),y)
	EXTRA_CFLAGS += -DBIG_ENDIAN_SUPPORT
endif





#############################################################################
# Make Targets
#############################################################################

#ifneq ($(KERNELRELEASE),)

MOALOBJS =	mlinux/moal_main.o \
		mlinux/moal_ioctl.o \
		mlinux/moal_shim.o

MLANOBJS =	mlan/mlan_shim.o mlan/mlan_init.o \
		mlan/mlan_txrx.o \
		mlan/mlan_cmdevt.o mlan/mlan_misc.o \
		mlan/mlan_cfp.o \
		mlan/mlan_module.o

MLANOBJS += mlan/mlan_wmm.o
MLANOBJS += mlan/mlan_usb.o
MLANOBJS += mlan/mlan_11n_aggr.o
MLANOBJS += mlan/mlan_11n_rxreorder.o
MLANOBJS += mlan/mlan_11n.o
MLANOBJS += mlan/mlan_11d.o
MLANOBJS += mlan/mlan_11h.o
ifeq ($(CONFIG_STA_SUPPORT),y)
MLANOBJS += mlan/mlan_meas.o
MLANOBJS += mlan/mlan_scan.o \
			mlan/mlan_sta_ioctl.o \
			mlan/mlan_sta_rx.o \
			mlan/mlan_sta_tx.o \
			mlan/mlan_sta_event.o \
			mlan/mlan_sta_cmd.o \
			mlan/mlan_sta_cmdresp.o \
			mlan/mlan_join.o
MOALOBJS += mlinux/moal_priv.o \
            mlinux/moal_wext.o
endif
ifeq ($(CONFIG_UAP_SUPPORT),y)
MLANOBJS += mlan/mlan_uap_ioctl.o
MLANOBJS += mlan/mlan_uap_cmdevent.o
MLANOBJS += mlan/mlan_uap_txrx.o
MOALOBJS += mlinux/moal_uap.o
MOALOBJS += mlinux/moal_uap_priv.o
MOALOBJS += mlinux/moal_uap_wext.o
endif

ifdef CONFIG_PROC_FS
MOALOBJS += mlinux/moal_proc.o
ifeq ($(CONFIG_PROC_DEBUG),y)
MOALOBJS += mlinux/moal_debug.o
endif
endif

#obj-m := mlan.o
mlan-objs := $(MLANOBJS)
MOALOBJS += mlinux/moal_usb.o
#obj-m += usb8xxx.o
usb8xxx-objs := $(MOALOBJS)

# Otherwise we were called directly from the command line; invoke the kernel build system.
#else

#default:
#	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

#endif

###############################################################

#export		CC LD EXTRA_CFLAGS KERNELDIR

#ifeq ($(CONFIG_STA_SUPPORT),y)
#ifeq ($(CONFIG_UAP_SUPPORT),y)
#.PHONY: mapp/mlanconfig mapp/mlan2040coex mapp/mlanevent mapp/uaputl clean distclean
#else
#.PHONY: mapp/mlanconfig mapp/mlanevent mapp/mlan2040coex clean distclean
#endif
#else
#ifeq ($(CONFIG_UAP_SUPPORT),y)
#.PHONY: mapp/mlanevent mapp/uaputl clean distclean
#endif
#endif
#	@echo "Finished Making Marvell Wlan Linux Driver"

#ifeq ($(CONFIG_STA_SUPPORT),y)
#mapp/mlanconfig:
#	$(MAKE) -C $@
#mapp/mlan2040coex:
#	$(MAKE) -C $@
#endif
#ifeq ($(CONFIG_UAP_SUPPORT),y)
#mapp/uaputl:
#	$(MAKE) -C $@
#endif
#ifeq ($(CONFIG_WFD_SUPPORT),y)
#mapp/wifidirectutl:
#	$(MAKE) -C $@
#endif
#mapp/mlanevent:
#	$(MAKE) -C $@
#
#echo:
#
#build:		echo default
#
#	@if [ ! -d $(BINDIR) ]; then \
#		mkdir $(BINDIR); \
#	fi 
#	cp -f mlan.$(MODEXT) $(BINDIR)/mlan$(DBG).$(MODEXT)
#	mv usb8xxx.$(MODEXT) $(BINDIR)/usb8782$(DBG).$(MODEXT)
#
#ifeq ($(CONFIG_STA_SUPPORT),y)
#	cp -f README $(BINDIR)
#	$(MAKE) -C mapp/mlanconfig $@ INSTALLDIR=$(BINDIR)
#	$(MAKE) -C mapp/mlan2040coex $@ INSTALLDIR=$(BINDIR)
#endif
#ifeq ($(CONFIG_UAP_SUPPORT),y)
#	cp -f README_UAP $(BINDIR)
#	$(MAKE) -C mapp/uaputl $@ INSTALLDIR=$(BINDIR)
#endif
#ifeq ($(CONFIG_WFD_SUPPORT),y)
#	cp -f README_WIFIDIRECT $(BINDIR)
#	cp -rpf script/wifidirect $(BINDIR)
#	$(MAKE) -C mapp/wifidirectutl $@ INSTALLDIR=$(BINDIR)
#endif
#	$(MAKE) -C mapp/mlanevent $@ INSTALLDIR=$(BINDIR)
#
#
#install: default
#
#	cp -f mlan.$(MODEXT) $(INSTALLDIR)/mlan$(DBG).$(MODEXT)
#	cp -f usb8xxx.$(MODEXT) $(INSTALLDIR)/usb8782$(DBG).$(MODEXT)
#	echo $(INSTALLDIR)
#	echo "usb8782 Driver Installed"


# End of file
