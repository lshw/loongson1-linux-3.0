/*
 * ops-godson1.c
 *
 * Copyright (C) 2004 ICT CAS
 * Author: Li xiaoyu, ICT CAS
 *   lixy@ict.ac.cn
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>

#include <asm/mach-loongson/ls1x/ls1b_board.h>

#define PCI_ACCESS_READ  0
#define PCI_ACCESS_WRITE 1


static inline void
bflush (void)
{
 	/* flush Bonito register writes */
  	(void) LS1A_PCIMAP_CFG;
}
static int ls1a_pci_config_access(unsigned char access_type,
        struct pci_bus *bus, unsigned int devfn, int where, u32 * data)
{

    unsigned char busnum = bus->number;
	
	u_int32_t addr, type;
	void *addrp;
	int device = devfn >> 3;
	int function = devfn & 0x7;
	int reg = where & ~3;

	if (busnum == 0) {
	  /* Type 0 configuration on onboard PCI bus */
		if (device > 20 || function > 7) {
	 			*data = -1;	/* device out of range */
				return PCIBIOS_DEVICE_NOT_FOUND;
		}
		addr = (1 << (device+11)) | (function << 8) | reg;
		type = 0;
	} else {
	   /* Type 1 configuration on offboard PCI bus */
	    if (busnum > 255 || device > 31 || function > 7) {
				*data = -1;	/* device out of range */
		        return PCIBIOS_DEVICE_NOT_FOUND;
		}
		addr = (busnum << 16) | (device << 11) | (function << 8) | reg;
		type = 0x10000;
	}

	/* clear aborts */
//	LS1A_PCICMD |= LS1A_PCICMD_MABORT | LS1A_PCICMD_MTABORT;

	LS1A_PCIMAP_CFG = (addr >> 16) | type;
	bflush ();

	addrp = (void *)CKSEG1ADDR(LS1A_PCICFG_BASE | (addr & 0xffff));
	if (access_type == PCI_ACCESS_WRITE){
  		*(volatile unsigned int *)addrp = cpu_to_le32(*data);
	}else {
  		*data = le32_to_cpu(*(volatile unsigned int *)addrp);
	}

#if 0
	if (LS1A_PCICMD & (LS1A_PCICMD_MABORT | LS1A_PCICMD_MTABORT)) {
  	    LS1A_PCICMD |= LS1A_PCICMD_MABORT | LS1A_PCICMD_MTABORT;
	    *data = -1;
	    return PCIBIOS_DEVICE_NOT_FOUND;
	}
#endif

	return PCIBIOS_SUCCESSFUL;

}


static int ls1a_pci_pcibios_read(struct pci_bus *bus, unsigned int devfn,
                                int where, int size, u32 * val)
{
        u32 data = 0;

        if (ls1a_pci_config_access(PCI_ACCESS_READ, bus, devfn, where,&data))
                return PCIBIOS_DEVICE_NOT_FOUND;

        if (size == 1)
                *val = (data >> ((where & 3) << 3)) & 0xff;
        else if (size == 2)
                *val = (data >> ((where & 3) << 3)) & 0xffff;
        else
                *val = data;

        return PCIBIOS_SUCCESSFUL;
}


static int ls1a_pci_pcibios_write(struct pci_bus *bus, unsigned int devfn,
                              int where, int size, u32 val)
{
        u32 data = 0;

        if (size == 4)
                data = val;
        else {
                if (ls1a_pci_config_access(PCI_ACCESS_READ, bus, devfn,where, &data))
                        return PCIBIOS_DEVICE_NOT_FOUND;

                if (size == 1)
                        data = (data & ~(0xff << ((where & 3) << 3))) |
                                (val << ((where & 3) << 3));
                else if (size == 2)
                        data = (data & ~(0xffff << ((where & 3) << 3))) |
                                (val << ((where & 3) << 3));
        }

        if (ls1a_pci_config_access(PCI_ACCESS_WRITE, bus, devfn, where,&data))
                return PCIBIOS_DEVICE_NOT_FOUND;

        return PCIBIOS_SUCCESSFUL;
}

struct pci_ops ls1a_pci_pci_ops = {
        .read = ls1a_pci_pcibios_read,
        .write = ls1a_pci_pcibios_write
};
