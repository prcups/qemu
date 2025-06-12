/*
 * loongson 2k1000 on-chip PCI emulation.
 *
 * Copyright (c) 2025 SignKirigami <prcups@krgm.moe>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "hw/pci/pcie_host.h"
#include "hw/pci-host/ls2k_pci.h"
#include "hw/pci/pci_bridge.h"


static AddressSpace *pci_dma_context_fn(PCIBus *bus, void *opaque, int devfn)
{
    LS2KPCIEHostBridgeState *pcihost = opaque;
    return &pcihost->as_mem;
}

static void pci_ls2k_config_writel(void *opaque, hwaddr addr,
                                   uint64_t val, unsigned size)
{
    LS2KPCIEHostBridgeState *phb = opaque;

    addr &= 0xffffff;

    pci_data_write(phb->bus,  addr, val, size);
}

static uint64_t pci_ls2k_config_readl(void *opaque, hwaddr addr, unsigned size)
{
    LS2KPCIEHostBridgeState *phb = opaque;
    uint32_t val;

    addr &= 0xffffff;


    val = pci_data_read(phb->bus, addr, size);
    return val;
}


static const MemoryRegionOps pci_ls2k_config_ops = {
    .read = pci_ls2k_config_readl,
    .write = pci_ls2k_config_writel,
    .endianness = DEVICE_NATIVE_ENDIAN,
};


/*
 * two way to translate pci dma address:
 * pci_setup_iommu
 * memory_region_init_iommu
 * pci_setup_iommu will not change addr.
 * memory_region_init_iommu can translate region and addr.
 */

static void ls2k_pci_pcihost_initfn(DeviceState *dev, Error **errp)
{
    LS2KPCIEHostBridgeState *pcihost;
    PCIHostState *phb;
    SysBusDevice *sysbus;
    pcihost = LS2K_PCI_HOST_BRIDGE(dev);
    sysbus = SYS_BUS_DEVICE(pcihost);

    memory_region_init(&pcihost->iomem_mem, OBJECT(pcihost), "system",
                       UINT64_MAX);
    address_space_init(&pcihost->as_mem, &pcihost->iomem_mem,
                       "pcie memory");
    #ifdef DEBUG_PCIEDMA
    ls2k_pci_as = &pcihost->as_mem;
    #endif

    /* Host memory as seen from the PCI side, via the IOMMU.  */

    memory_region_init_alias(&pcihost->iomem_submem, NULL, "pcisubmem",
                             &pcihost->iomem_mem, 0x10000000, 0x2000000);
    memory_region_init_alias(&pcihost->iomem_subbigmem, NULL, "pcisubmem",
                             &pcihost->iomem_mem, 0x40000000, 0x20000000);

    memory_region_init(&pcihost->iomem_io, OBJECT(pcihost), "system",
                       0x10000);
    address_space_init(&pcihost->as_io, &pcihost->iomem_io, "pcie io");

    phb = PCI_HOST_BRIDGE(dev);
    pcihost->bus = phb->bus = pci_register_root_bus(DEVICE(dev), "pci",
                                                    pci_ls2k_set_irq, pcihost->pci_map_irq, pcihost->pic,
                                                    &pcihost->iomem_mem, &pcihost->iomem_io, PCI_DEVFN(0, 0), 64,
                                                    TYPE_PCIE_BUS);


    pci_setup_iommu(pcihost->bus, pci_dma_context_fn, pcihost);

    /* set the south bridge pci configure  mapping */
    memory_region_init_io(&pcihost->data_mem, NULL, &pci_ls2k_config_ops,
                          pcihost, "south-bridge-pci-config", 0x2000000);
    sysbus_init_mmio(sysbus, &pcihost->data_mem);

    memory_region_init_io(&pcihost->data_mem1, NULL, &pci_ls2k_config_ops,
                          pcihost, "south-bridge-pci-config", 0x200000000);
    sysbus_init_mmio(sysbus, &pcihost->data_mem1);
}

static const char *ls2k_host_root_bus_path(PCIHostState *host_bridge,
                                           PCIBus *rootbus)
{
    return "0000:00";
}

static void ls2k_pci_pcihost_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIHostBridgeClass *hc = PCI_HOST_BRIDGE_CLASS(klass);

    hc->root_bus_path = ls2k_host_root_bus_path;
    dc->realize = ls2k_pci_pcihost_initfn;
}

static const TypeInfo ls2k_pci_pcihost_info = {
    .name          = TYPE_LS2K_PCIE_HOST_BRIDGE,
    .parent        = TYPE_PCIE_HOST_BRIDGE,
    .instance_size = sizeof(LS2KPCIEHostBridgeState),
    .class_init    = ls2k_pci_pcihost_class_init,
};

static void ls2k_pci_pciconf_writel(void *opaque, hwaddr addr,
                                  uint64_t val, unsigned size)
{
    LS2KPCIBridgeState *s = opaque;
    PCIDevice *d = PCI_DEVICE(s);

    d->config_write(d, addr, val, 4);
}

static uint64_t ls2k_pci_pciconf_readl(void *opaque, hwaddr addr,
                                     unsigned size)
{

    LS2KPCIBridgeState *s = opaque;
    PCIDevice *d = PCI_DEVICE(s);

    return d->config_read(d, addr, 4);
}

static const MemoryRegionOps ls2k_pci_pciconf_ops = {
    .read = ls2k_pci_pciconf_readl,
    .write = ls2k_pci_pciconf_writel,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void ls2k_pci_bridge_initfn(PCIDevice *dev, Error **errp)
{
    LS2KPCIBridgeState *s = LS2K_PCI_HOST_BRIDGE(dev);
    SysBusDevice *sysbus = SYS_BUS_DEVICE(s->pcihost);

    pci_bridge_initfn(dev, TYPE_PCI_BUS);

    /* set the north bridge pci configure  mapping */
    memory_region_init_io(&s->conf_mem, NULL, &ls2k_pci_pciconf_ops, s,
                          "north-bridge-pci-config", 0x100);
    sysbus_init_mmio(sysbus, &s->conf_mem);


    pci_config_set_prog_interface(dev->config,
                                  PCI_CLASS_BRIDGE_PCI_INF_SUB);
    /* set the default value of north bridge pci config */

    pci_set_word(dev->config + PCI_COMMAND,
                 PCI_COMMAND_MEMORY | PCI_COMMAND_IO);
    pci_set_word(dev->config + PCI_STATUS,
                 PCI_STATUS_FAST_BACK | PCI_STATUS_66MHZ |
                 PCI_STATUS_DEVSEL_MEDIUM);
    pci_set_word(dev->config + PCI_SUBSYSTEM_VENDOR_ID, 0x0000);
    pci_set_word(dev->config + PCI_SUBSYSTEM_ID, 0x0000);

    pci_set_byte(dev->config + PCI_INTERRUPT_LINE, 0x00);
    pci_set_byte(dev->config + PCI_INTERRUPT_PIN, 0x01);
    pci_set_byte(dev->config + PCI_MIN_GNT, 0x3c);
    pci_set_byte(dev->config + PCI_MAX_LAT, 0x00);
    pci_set_word(dev->config + PCI_CLASS_DEVICE, 0x0604);

}

static void ls2k_pci_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = ls2k_pci_bridge_initfn;
    k->exit = pci_bridge_exitfn;
    k->vendor_id = 0xdf53;
    k->device_id = 0x00d5;
    k->revision = 0x01;
    k->class_id = PCI_CLASS_BRIDGE_HOST;
    k->config_write = pci_bridge_write_config;
    dc->desc = "LS2K PCI bridge";
}

static const TypeInfo ls2k_pci_info = {
    .name          = TYPE_LS2K_PCI_BRIDGE,
    .parent        = TYPE_PCI_BRIDGE,
    .instance_size = sizeof(LS2KPCIBridgeState),
    .class_init    = ls2k_pci_class_init,
    .interfaces = (InterfaceInfo[])
    {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void ls2k_pci_register_types(void)
{
    type_register_static(&ls2k_pci_pcihost_info);
    type_register_static(&ls2k_pci_info);
}

type_init(ls2k_pci_register_types)
