/*
 * fixup-ls232-board.c
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
//#include <linux/config.h>
//#include <linux/autoconf.h>
#include <generated/autoconf.h>	//lxy
#include <linux/init.h>
#include <linux/pci.h>
#include <asm/mach-loongson/ls1x/ls1b_board.h>
#include <asm/mach-loongson/ls1x/irq.h>
#include <asm/gpio.h>


int __init pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	printk("pcibios_map_irq devfn : %d , slot : %d , pin : %d\n",PCI_SLOT(dev->devfn),slot,pin);
//	 dev->irq=(pin-1)+LS1A_BOARD_PCI_INTA_IRQ;
//	return dev->irq;
//	return ((pin-1)+LS1A_BOARD_PCI_INTA_IRQ);
//	return LS1A_BOARD_PCI_INTA_IRQ;
	//lxy: slot = IDSEL - 11;
	if (slot == 5) {
		gpio_request(LS1A_BOARD_PCI_INTA_IRQ-64, "pci inta irq");
		gpio_direction_input(LS1A_BOARD_PCI_INTA_IRQ-64);
		return LS1A_BOARD_PCI_INTA_IRQ;
	}
	else {
		gpio_request(LS1A_BOARD_PCI_INTB_IRQ-64, "pci intb irq");
		gpio_direction_input(LS1A_BOARD_PCI_INTB_IRQ-64);
		return LS1A_BOARD_PCI_INTB_IRQ;
	}
}

/* Do platform specific device initialization at pci_enable_device() time */
int pcibios_plat_dev_init(struct pci_dev *dev)
{
	return 0;
}

static void __init loongsonls1a_fixup_pcimap(struct pci_dev *pdev)
{
	static int first = 1;

	if (first)
		first = 0;
	else
		return;

	/* 1,00 0110 ,0001 01,00 0000 */
	LS1A_PCIMAP = 0x46140;
	LOONGSON_REG(LS1A_PCI_HEADER_CFG + 0x20) = 0x40000000;
	LOONGSON_REG(LS1A_PCI_HEADER_CFG + 0x24) = 0x0;
}

DECLARE_PCI_FIXUP_HEADER(PCI_ANY_ID, PCI_ANY_ID, loongsonls1a_fixup_pcimap);
