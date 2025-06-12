#ifndef LS2K_PCI_H_INCLUDED
#define LS2K_PCI_H_INCLUDED

#include "qemu/typedefs.h"

#define TYPE_LS2K_PCIE_HOST_BRIDGE "ls2k-pcie-host"
#define LS2K_PCIE_HOST_BRIDGE(obj) \
OBJECT_CHECK(LS2KPCIEHostState, (obj), TYPE_LS2K_PCIE_HOST_BRIDGE)

#define TYPE_LS2K_PCI_BRIDGE "ls2k-pci-bridge"
#define LS2K_PCI_BRIDGE(obj) \
OBJECT_CHECK(LS2KPCIBridgeState, (obj), TYPE_LS2K_PCI_BRIDGE)

struct LS2KPCIEHostBridgeState {
    PCIExpressHost parent_obj;
    PCIBus *bus;
    qemu_irq *pic;
    LS2KPCIBridgeState *pci_dev;
    MemoryRegion iomem_mem;
    MemoryRegion iomem_submem;
    MemoryRegion iomem_subbigmem;
    MemoryRegion iomem_io;
    AddressSpace as_mem;
    AddressSpace as_io;
    MemoryRegion data_mem;
    MemoryRegion data_mem1;
    int (*pci_map_irq)(PCIDevice *d, int irq_num);
} LS2KPCIEHostBridgeState;

typedef struct LS2KPCIBridgeState {
    /*< private >*/
    PCIBridge parent_obj;
    /*< public >*/
    LS2KPCIEHostBridgeState *pcihost;
    MemoryRegion iomem;
    MemoryRegion conf_mem;
    struct pcilocalreg {
        /*0*/
        unsigned int portctr0;
        unsigned int portctr1;
        unsigned int portstat0;
        unsigned int portstat1;
        /*0x10*/
        unsigned int usrmsgid;
        unsigned int nouse;
        unsigned int portintsts;
        unsigned int portintclr;
        /*0x20*/
        unsigned int portintmsk;
        unsigned int portcfg;
        unsigned int portctrsts;
        unsigned int physts;
        /*0x30*/
        unsigned int nouse1[2];
        unsigned int usrmsg0;
        unsigned int usrmsg1;
        /*0x40*/
        unsigned int usrmsgsend0;
        unsigned int usrmsgsend1;
        unsigned int noused2[5];
        /*0x5c*/
        unsigned int msi;
        unsigned int noused3[2];
        /*0x68*/
        unsigned int addrmsk;
        unsigned int addrmsk1;
        /*0x70*/
        unsigned int addrtrans;
        unsigned int addrtrans1;
        unsigned int dataload0;
        unsigned int dataload1;
    } mypcilocalreg;
} LS2KPCIBridgeState;

#endif // LS2K_PCI_H_INCLUDED
