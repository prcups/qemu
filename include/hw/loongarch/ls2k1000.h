#ifndef HW_LS2K1000_H
#define HW_LS2K1000_H

#include "hw/boards.h"
#include "qemu/units.h"
#include "qemu/typedefs.h"
#include "target/loongarch/cpu.h"
#include "target/loongarch/cpu-qom.h"
#include "exec/target_page.h"

#define LS2K1000_CPUS 2
#define BIOS_ADDRESS 0x1c000000
#define UART_ADDRESS 0x1fe20000
#define LS2K_PCI_CFG_SIZE 0x2000000
#define LS2K_PCI_CFG_BASE 0x1a000000
#define LS2K_PCI_CFG_SIZE_HIGH 0x200000000
#define LS2K_PCI_CFG_BASE_HIGH 0xfe00000000
#define LS2K_PCI_MEM_SIZE 0x2000000
#define LS2K_PCI_MEM_BASE 0x10000000
#define LS2K_PCI_MEM_SIZE_HIGH 0x20000000
#define LS2K_PCI_MEM_BASE_HIGH 0x40000000
#define LS2K_PCI_IO_SIZE 0x10000
#define LS2K_PCI_IO_BASE 0x18000000

typedef struct LS2K1000MachineState {
    /*< private >*/
    MachineState parent_obj;

    MemoryRegion ram;
    MemoryRegion bios;

    DeviceState *ipi;
    DeviceState *liointc;

    PCIBus *pci_bus;

    LoongArchCPU* cpu[LS2K1000_CPUS];
} LS2K1000MachineState;

#define TYPE_LS2K1000_MACHINE MACHINE_TYPE_NAME("ls2k1000")
#define LS2K1000_MACHINE(obj) \
OBJECT_CHECK(LS2K1000MachineState, (obj), TYPE_LS2K1000_MACHINE)

#endif
