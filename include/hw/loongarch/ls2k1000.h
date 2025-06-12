#ifndef HW_LS2K1000_H
#define HW_LS2K1000_H

#include "hw/boards.h"
#include "qemu/units.h"
#include "qemu/typedefs.h"
#include "target/loongarch/cpu.h"
#include "target/loongarch/cpu-qom.h"
#include "exec/target_page.h"

#define LS2K1000_CPUS 2

typedef struct LS2K1000MachineState {
    /*< private >*/
    MachineState parent_obj;

    MemoryRegion bios;
    MemoryRegion cpu_config;
    MemoryRegion ram1, ram2, ram3;
    MemoryRegion ddr_config;
    uint64_t reg_ddr;
    uint8_t ddr_level_reg[0x1000];

    DeviceState *ipi;
    DeviceState *liointc;
    DeviceState *spi;

    PCIBus *pci_bus;

    LoongArchCPU* cpu[LS2K1000_CPUS];
} LS2K1000MachineState;

#define TYPE_LS2K1000_MACHINE MACHINE_TYPE_NAME("ls2k1000")
#define LS2K1000_MACHINE(obj) \
OBJECT_CHECK(LS2K1000MachineState, (obj), TYPE_LS2K1000_MACHINE)

#endif
