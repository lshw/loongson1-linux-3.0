/*
 * pci.c
 *
 * Copyright (C) 2004 ICT CAS
 * Author: Lin Wei, ICT CAS
 *   wlin@ict.ac.cn
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

#include <generated/autoconf.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <loongson1.h>

extern struct pci_ops ls1a_pci_pci_ops;

static struct resource ls1a_pci_mem_resource = {
	.name   = "pci memory space",
	.start  = 0x14000000UL,
	.end    = 0x17ffffffUL,
	.flags  = IORESOURCE_MEM,
};

static struct resource ls1a_pci_io_resource = {
	.name   = "pci io space",
	.start  = 0x00004000UL,
	.end    = 0x000fffffUL,
	.flags  = IORESOURCE_IO,
};


static struct pci_controller  ls1a_pci_controller = {
	.pci_ops        = &ls1a_pci_pci_ops,
	.io_resource    = &ls1a_pci_io_resource,
	.mem_resource   = &ls1a_pci_mem_resource,
	.mem_offset     = 0x00000000UL,
	.io_offset      = 0x00000000UL,
};

#ifdef CONFIG_LOAD_PCICFG
#include "pciload.c"
#endif

#ifdef CONFIG_DISABLE_PCI
static int disablepci=1;
#else
static int disablepci=0;
#endif

static int __init pcibios_init(void)
{
    extern int pci_probe_only;

	/* init base address of io space */
	set_io_port_base((unsigned long)
		ioremap(LS1A_PCIIO_BASE, LS1A_PCIIO_SIZE));
	
	ioport_resource.start = 0;
	ioport_resource.end = 0xffffffffUL;
	iomem_resource.start = 0;
	iomem_resource.end = 0xffffffffUL;

#ifdef CONFIG_PCI_AUTO
	pci_probe_only = 0; //not auto assign the resource 
#else
	pci_probe_only = 1; //not auto assign the resource 
#endif

#ifdef CONFIG_TRACE_BOOT
#endif
	pr_info("arch_initcall:pcibios_init\n");
	pr_info("register_pci_controller : %p\n",&ls1a_pci_controller);

	if (!disablepci)
		register_pci_controller(&ls1a_pci_controller);

#ifdef CONFIG_LOAD_PCICFG
	pciload(&ls1a_pci_pci_ops);
#endif
	return 0;
}
arch_initcall(pcibios_init);

static int __init disablepci_setup(char *options)
{
    if (!options || !*options)
        return 0;
    if (options[0] == '0')
    	disablepci = 0;
    else
    	disablepci = simple_strtoul(options, 0, 0);
    return 1;
}
__setup("disablepci=", disablepci_setup);

