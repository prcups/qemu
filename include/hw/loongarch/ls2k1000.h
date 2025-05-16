#ifndef HW_LS2K1000_H
#define HW_LS2K1000_H

#include "qemu/units.h"
#include "qemu/typedefs.h"
#include "target/loongarch/cpu.h"
#include "target/loongarch/cpu-qom.h"
#include "exec/target_page.h"

#define LS2K1000_CPUS 2
#define BIOS_SIZE (16 * MiB)
#define BIOS_ADDRESS 0x1c000000
#define APB_BASE_ADDRESS 0x1fe20000
#define CFG_BASE_ADDRESS 0x1fe00000

typedef struct LS2K1000MachineState {
    /*< private >*/
    MachineState parent_obj;

    MemoryRegion lowram;
    MemoryRegion highram;
    MemoryRegion bios;
    MemoryRegion iocsr;

    DeviceState *ipi;
    DeviceState *liointc;

    LoongArchCPU* cpu[LS2K1000_CPUS];
} LS2K1000MachineState;

#define TYPE_LS2K1000_MACHINE MACHINE_TYPE_NAME("ls2k1000")
#define LS2K1000_MACHINE(obj) \
OBJECT_CHECK(LS2K1000MachineState, (obj), TYPE_LS2K1000_MACHINE)

#endif
