/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * QEMU loongson 2k1000 develop board emulation
 *
 * Copyright (c) 2025 Loongson Technology Corporation Limited
 */

#include "qemu/error-report.h"
#include "qemu/cutils.h"
#include "qemu/datadir.h"
#include "qapi/error.h"
#include "qom/cpu.h"
#include "hw/loongarch/ls2k1000.h"
#include "hw/intc/ls2k_ipi.h"
#include "hw/intc/ls2k_liointc.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "exec/cpu-common.h"
#include "exec/cpu-defs.h"
#include "system/reset.h"
#include "elf.h"
#include <stdlib.h>
#include <stdint.h>

static void ls2k1000_firmware_init(MachineState *ms)
{
    LS2K1000MachineState* ls2k1000ms = LS2K1000_MACHINE(ms);
    int bios_size = -1;
    char *bios_path = NULL;
    if (ms->firmware)
    {
        bios_path = qemu_find_file(QEMU_FILE_TYPE_BIOS, ms->firmware);
        if (bios_path) {
            bios_size = get_image_size(bios_path);
            memory_region_init_ram(ls2k1000ms->bios, NULL, "ls2k1000.bios", BIOS_SIZE,
                                   &error_fatal);
            memory_region_set_readonly(ls2k1000ms->bios, true);
            memory_region_add_subregion(get_system_memory(), BIOS_ADDRESS, ls2k1000ms->bios);
            load_image_targphys(bios_path, BIOS_ADDRESS, BIOS_SIZE);
            g_free(bios_path);
        }
        else
        {
            error_report("BIOS Doesn't Exist");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        error_report("No BIOS Detected");
        exit(EXIT_FAILURE);
    }
}

static void ls2k1000_init(MachineState *ms)
{
    MachineClass *mc = MACHINE_GET_CLASS(ms);
    LS2K1000MachineState* ls2k1000ms = LS2K1000_MACHINE(ms);
    MemoryRegion *address_space_mem = get_system_memory();
    LoongsonIPICommonState *lics;

    if (ms->ram_size != 1 * GiB) {
        error_report("This machine can only be used with 1GiB of RAM");
        exit(EXIT_FAILURE);
    }

    if (ms->smp.cpus != 2) {
        error_report("This machine can only be used with 2 Core");
        exit(EXIT_FAILURE);
    }

    ls2k1000ms->ipi = qdev_new(TYPE_LS2K_IPI);
    qdev_prop_set_uint32(ls2k1000ms->ipi, "num-cpu", ms->smp.cpus);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(ls2k1000ms->ipi), &error_fatal);

    ls2k1000ms->liointc = qdev_new(TYPE_LOONGSON_LIOINTC);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(ls2k1000ms->liointc), &error_fatal);

    for (int i = 0; i < ms->smp.cpus; i++) {
        Object *cpuobj = NULL;
        cpuobj = object_new(LOONGARCH_CPU_TYPE_NAME("la464"));
        if (cpuobj == NULL) {
            error_report("Fail to create object with type %s ",
                         LOONGARCH_CPU_TYPE_NAME("la464"));
            exit(EXIT_FAILURE);
        }

        ls2k1000ms->cpu[i] = LOONGARCH_CPU(cpuobj);
        qdev_realize_and_unref(DEVICE(cpuobj), NULL, &error_fatal);
        qdev_connect_gpio_out(ls2k1000ms->ipi, i, qdev_get_gpio_in(DEVICE(cpuobj), IRQ_IPI));
        for (int j = 0; j < 4; j++) {
            qdev_connect_gpio_out(ls2k1000ms->liointc, i, qdev_get_gpio_in(DEVICE(cpuobj), IRQ_IPI));
        }
    }

    memory_region_init_alias(&ls2k1000ms->lowram, NULL, "lowmem", ms->ram, 0, 0x10000000);
    memory_region_add_subregion(address_space_mem, 0, &ls2k1000ms->lowram);
    memory_region_init_alias(&ls2k1000ms->highram, NULL, "highmem", ms->ram, 0, ms->ram_size);
    memory_region_add_subregion(address_space_mem, 0x100000000, &ls2k1000ms->highram);

    ls2k1000_firmware_init(ms);
}

static void ls2k1000_class_init(ObjectClass *oc)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "ls2k1000 platform";
    mc->init = ls2k1000_init;
    mc->default_cpus = LS2K1000_CPUS;
    mc->block_default_type = IF_SD;
    mc->default_cpu_type = LOONGARCH_CPU_TYPE_NAME("la464");
    mc->default_ram_id = "loongarch.ram";
    mc->default_ram_size = 1 * GiB;
    mc->auto_create_sdcard = true;
}

static const TypeInfo ls2k1000_machine_typeinfo = {
    .name       = MACHINE_TYPE_NAME("ls2k1000"),
    .parent     = TYPE_MACHINE,
    .class_init = ls2k1000_class_init,
    .instance_size = sizeof(LS2K1000MachineState),
};

static void ls2k1000_machine_init_register_types(void)
{
    type_register_static(&ls2k1000_machine_typeinfo);
}

type_init(ls2k1000_machine_init_register_types)
