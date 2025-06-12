/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * QEMU loongson 2k1000 develop board emulation
 *
 * Copyright (c) 2025 Loongson Technology Corporation Limited
 */

#include "qemu/osdep.h"
#include "qemu/cutils.h"
#include "qemu/datadir.h"
#include "qapi/error.h"
#include "hw/loongarch/ls2k1000.h"
#include "hw/intc/ls2k_ipi.h"
#include "hw/intc/ls2k_liointc.h"
#include "hw/char/serial-mm.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/ide/ahci-pci.h"
#include "hw/qdev-properties.h"
#include "hw/pci-host/gpex.h"
#include "system/system.h"
#include "exec/cpu-common.h"
#include "exec/cpu-defs.h"
#include "system/reset.h"
#include "elf.h"
#include "qemu/error-report.h"

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
            memory_region_init_ram(&ls2k1000ms->bios, NULL, "ls2k1000.bios", bios_size,
                                   &error_fatal);
            memory_region_set_readonly(&ls2k1000ms->bios, true);
            memory_region_add_subregion(get_system_memory(), BIOS_ADDRESS, &ls2k1000ms->bios);
            load_image_targphys(bios_path, BIOS_ADDRESS, bios_size);
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

static void ls2k1000_pci_devices_init(MachineState *ms)
{
    LS2K1000MachineState* ls2k1000ms = LS2K1000_MACHINE(ms);

    pci_create_simple_multifunction(ls2k1000ms->pci_bus,
                                        PCI_DEVFN(8, 0),
                                        TYPE_ICH9_AHCI);
}

static void ls2k1000_pci_init(MachineState *ms)
{
    LS2K1000MachineState* ls2k1000ms = LS2K1000_MACHINE(ms);
    DeviceState *dev;
    SysBusDevice *d;
    MemoryRegion *ecam_alias, *ecam_alias_high, *ecam_reg, *pio_alias, *pio_reg;
    MemoryRegion *mmio_alias, *mmio_alias_high, *mmio_reg;
    int i;

    dev = qdev_new(TYPE_GPEX_HOST);
    d = SYS_BUS_DEVICE(dev);
    sysbus_realize_and_unref(d, &error_fatal);
    ls2k1000ms->pci_bus = PCI_HOST_BRIDGE(dev)->bus;

    /* Map only part size_ecam bytes of ECAM space */
    ecam_alias = g_new0(MemoryRegion, 1);
    ecam_reg = sysbus_mmio_get_region(d, 0);
    memory_region_init_alias(ecam_alias, OBJECT(dev), "pcie-ecam",
                             ecam_reg, 0, LS2K_PCI_CFG_SIZE);
    memory_region_add_subregion(get_system_memory(), LS2K_PCI_CFG_BASE,
                                ecam_alias);

    ecam_alias_high = g_new0(MemoryRegion, 1);
    memory_region_init_alias(ecam_alias_high, OBJECT(dev), "pcie-ecam-high",
                             ecam_reg, 0, LS2K_PCI_CFG_SIZE_HIGH);
    memory_region_add_subregion(get_system_memory(), LS2K_PCI_CFG_BASE_HIGH,
                                ecam_alias_high);

    /* Map PCI mem space */
    mmio_alias = g_new0(MemoryRegion, 1);
    mmio_reg = sysbus_mmio_get_region(d, 1);
    memory_region_init_alias(mmio_alias, OBJECT(dev), "pcie-mmio",
                             mmio_reg, LS2K_PCI_MEM_BASE, LS2K_PCI_MEM_SIZE);
    memory_region_add_subregion(get_system_memory(), LS2K_PCI_MEM_BASE,
                                mmio_alias);

    mmio_alias_high = g_new0(MemoryRegion, 1);
    memory_region_init_alias(mmio_alias_high, OBJECT(dev), "pcie-mmio-high",
                             mmio_reg, LS2K_PCI_MEM_BASE_HIGH, LS2K_PCI_MEM_SIZE_HIGH);
    memory_region_add_subregion(get_system_memory(), LS2K_PCI_MEM_BASE_HIGH,
                                mmio_alias_high);

    /* Map PCI IO port space. */
    pio_alias = g_new0(MemoryRegion, 1);
    pio_reg = sysbus_mmio_get_region(d, 2);
    memory_region_init_alias(pio_alias, OBJECT(dev), "pcie-io", pio_reg,
                             0, LS2K_PCI_IO_SIZE);
    memory_region_add_subregion(get_system_memory(), LS2K_PCI_IO_BASE,
                                pio_alias);

    for (i = 0; i < PCI_NUM_PINS; i++) {
        sysbus_connect_irq(d, i,
                           qdev_get_gpio_in(ls2k1000ms->liointc, 32 + i));
        gpex_set_irq_num(GPEX_HOST(dev), i, 16 + i);
    }

    ls2k1000_pci_devices_init(ms);
}

static void ls2k1000_init(MachineState *ms)
{
    LS2K1000MachineState* ls2k1000ms = LS2K1000_MACHINE(ms);
    MemoryRegion *address_space_mem = get_system_memory();

    if (ms->smp.cpus != 2) {
        error_report("This machine can only be used with 2 Core");
        exit(EXIT_FAILURE);
    }


    ls2k1000ms->ipi = qdev_new(TYPE_LS2K_IPI);
    qdev_prop_set_uint32(ls2k1000ms->ipi, "num-cpu", ms->smp.cpus);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(ls2k1000ms->ipi), &error_fatal);


    ls2k1000ms->liointc = qdev_new(TYPE_LS2K_LIOINTC);
    qdev_prop_set_uint32(ls2k1000ms->liointc, "num-cpu", ms->smp.cpus);
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
            qdev_connect_gpio_out(ls2k1000ms->liointc, PARENT_COREx_IPy(i, j),
                                  qdev_get_gpio_in(DEVICE(cpuobj), 2 + j));
        }
    }

    memory_region_init_alias(&ls2k1000ms->ram, NULL, "mem", ms->ram, 0, ms->ram_size);
    memory_region_add_subregion(address_space_mem, 0, &ls2k1000ms->ram);

    ls2k1000_firmware_init(ms);

    serial_mm_init(get_system_memory(), UART_ADDRESS, 0,
                       qdev_get_gpio_in(ls2k1000ms->liointc, 0),
                       115200, serial_hd(0), DEVICE_LITTLE_ENDIAN);

    ls2k1000_pci_init(ms);
}

static void ls2k1000_class_init(ObjectClass *oc, const void* data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "ls2k1000 platform";
    mc->init = ls2k1000_init;
    mc->default_cpus = LS2K1000_CPUS;
    mc->max_cpus = LS2K1000_CPUS;
    mc->min_cpus = LS2K1000_CPUS;
    mc->block_default_type = IF_IDE;
    mc->default_cpu_type = LOONGARCH_CPU_TYPE_NAME("la464");
    mc->default_ram_id = "loongarch.ram";
    mc->default_ram_size = 1 * GiB;
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
