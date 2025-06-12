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
#include "hw/qdev-properties.h"
#include "hw/pci-host/gpex.h"
#include "system/system.h"
#include "exec/cpu-common.h"
#include "exec/cpu-defs.h"
#include "system/reset.h"
#include "system/block-backend.h"
#include "block/block_int.h"
#include "hw/ssi/ls1a_spi.h"
#include "elf.h"
#include "qemu/error-report.h"
#include "hw/ssi/ssi.h"

#define BIOS_ADDRESS 0x1c000000
#define PCI_CFG_SIZE 0x2000000
#define PCI_CFG_BASE 0x1a000000
#define PCI_CFG_SIZE_HIGH 0x200000000
#define PCI_CFG_BASE_HIGH 0xfe00000000
#define PCI_MEM_SIZE 0x2000000
#define PCI_MEM_BASE 0x10000000
#define PCI_MEM_SIZE_HIGH 0x20000000
#define PCI_MEM_BASE_HIGH 0x40000000
#define PCI_IO_MEM_SIZE 0x10000
#define PCI_IO_MEM_BASE 0x18000000

#define CFG_BASE 0x1fe00000
#define IPI_BASE 0x1fe01000
#define LIOINTC_BASE 0x1fe01400
#define UART_ADDRESS 0x1fe20000
#define SPI_BASE 0x1fff0220

#define LOWMEM_2G_BASE 0x80000000
#define LOWMEM_2G_SIZE 0x80000000
#define MEM_ALIAS_BASE 0x100000000

static void ls2k1000_firmware_init(MachineState *ms)
{
    LS2K1000MachineState* ls2k1000ms = LS2K1000_MACHINE(ms);
    DeviceState *flash_dev;
    BusState* spi_bus;
    DriveInfo* flash_dinfo;
    BlockBackend *bios_block;
    BlockDriverState* bs;
    qemu_irq flash_cs;

    if (ms->firmware)
    {
        error_report("BIOS not supported for this machine");
        exit(EXIT_FAILURE);
    }

    ls2k1000ms->spi = sysbus_create_simple(TYPE_LS1A_SPI, SPI_BASE,
                                           qdev_get_gpio_in(ls2k1000ms->liointc, 8));
    spi_bus = qdev_get_child_bus(ls2k1000ms->spi, "spi");
    flash_dev = qdev_new("gd25q64");
    flash_dinfo = drive_get(IF_PFLASH, 0, 0);
    if (!flash_dinfo)
    {
        error_report("No PFlash found.");
        exit(EXIT_FAILURE);
    }
    qdev_realize_and_unref(flash_dev, spi_bus, &error_fatal);

    bios_block = blk_by_legacy_dinfo(flash_dinfo);
    qdev_prop_set_drive_err(flash_dev, "drive", bios_block, &error_fatal);
    bs = blk_bs(bios_block);

    memory_region_init_ram(&ls2k1000ms->bios, NULL, "bios",
                                0x1000000, NULL);
    memory_region_add_subregion(get_system_memory(), BIOS_ADDRESS,
                                &ls2k1000ms->bios);
    load_image_mr(bs->filename, &ls2k1000ms->bios);

    flash_cs = qdev_get_gpio_in_named(flash_dev, SSI_GPIO_CS, 0);
    sysbus_connect_irq(SYS_BUS_DEVICE(ls2k1000ms->spi), 1, flash_cs);
}

static void ls2k1000_pci_devices_init(MachineState *ms)
{
    LS2K1000MachineState* ls2k1000ms = LS2K1000_MACHINE(ms);

    pci_create_simple_multifunction(ls2k1000ms->pci_bus,
                                    PCI_DEVFN(3, 0),
                                    "e1000");
    pci_create_simple_multifunction(ls2k1000ms->pci_bus,
                                    PCI_DEVFN(3, 1),
                                    "e1000");
    pci_create_simple_multifunction(ls2k1000ms->pci_bus,
                                    PCI_DEVFN(4, 1),
                                    "usb-ehci");
    pci_create_simple_multifunction(ls2k1000ms->pci_bus,
                                    PCI_DEVFN(4, 2),
                                    "pci-ohci");
    pci_create_simple_multifunction(ls2k1000ms->pci_bus,
                                    PCI_DEVFN(7, 0),
                                    "intel-hda");
    pci_create_simple_multifunction(ls2k1000ms->pci_bus,
                                    PCI_DEVFN(8, 0),
                                    "ich9-ahci");
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
                             ecam_reg, 0, PCI_CFG_SIZE);
    memory_region_add_subregion(get_system_memory(), PCI_CFG_BASE,
                                ecam_alias);

    ecam_alias_high = g_new0(MemoryRegion, 1);
    memory_region_init_alias(ecam_alias_high, OBJECT(dev), "pcie-ecam-high",
                             ecam_reg, 0, PCI_CFG_SIZE_HIGH);
    memory_region_add_subregion(get_system_memory(), PCI_CFG_BASE_HIGH,
                                ecam_alias_high);

    /* Map PCI mem space */
    mmio_alias = g_new0(MemoryRegion, 1);
    mmio_reg = sysbus_mmio_get_region(d, 1);
    memory_region_init_alias(mmio_alias, OBJECT(dev), "pcie-mmio",
                             mmio_reg, PCI_MEM_BASE, PCI_MEM_SIZE);
    memory_region_add_subregion(get_system_memory(), PCI_MEM_BASE,
                                mmio_alias);

    mmio_alias_high = g_new0(MemoryRegion, 1);
    memory_region_init_alias(mmio_alias_high, OBJECT(dev), "pcie-mmio-high",
                             mmio_reg, PCI_MEM_BASE_HIGH, PCI_MEM_SIZE_HIGH);
    memory_region_add_subregion(get_system_memory(), PCI_MEM_BASE_HIGH,
                                mmio_alias_high);

    /* Map PCI IO port space. */
    pio_alias = g_new0(MemoryRegion, 1);
    pio_reg = sysbus_mmio_get_region(d, 2);
    memory_region_init_alias(pio_alias, OBJECT(dev), "pcie-io", pio_reg,
                             0, PCI_IO_MEM_SIZE);
    memory_region_add_subregion(get_system_memory(), PCI_IO_MEM_BASE,
                                pio_alias);

    for (i = 0; i < PCI_NUM_PINS; i++) {
        sysbus_connect_irq(d, i,
                           qdev_get_gpio_in(ls2k1000ms->liointc, 32 + i));
        gpex_set_irq_num(GPEX_HOST(dev), i, 16 + i);
    }

    ls2k1000_pci_devices_init(ms);
}

static uint64_t ddr_config_readl(void *opaque, hwaddr addr, unsigned size) {
    LS2K1000MachineState* ls2k1000ms = LS2K1000_MACHINE(opaque);
    uint64_t r = 0;
    int i = 0;
    if (addr >= 0x160 && addr <= 0x167) {
        uint64_t data = 0x000000000f000101;
        r = 0;
        memcpy(&r, (char *)&data + (addr - 0x160), size);
    } else if (addr >= 0x180 && addr <= 0x187) {
        union {
            uint8_t b[8];
            uint64_t l;
        } d;
        memcpy(d.b, ls2k1000ms->ddr_level_reg + 0x180, 8);
        d.b[7] = 1;
        d.b[6] = 1;
        d.b[5] = 1;
        if (ls2k1000ms->ddr_level_reg[0x180] == 1) {
            d.b[7] = ls2k1000ms->ddr_level_reg[0x3a + 0 * 0x20] >= 1
            && ls2k1000ms->ddr_level_reg[0x3a + 0 * 0x20] < 20 ? 0x1 : 0;
            /*glvl*/
        } else if (ls2k1000ms->ddr_level_reg[0x180] == 2) {
            d.b[7] = ls2k1000ms->ddr_level_reg[0x38 + 0 * 0x20] >= 1
            && ls2k1000ms->ddr_level_reg[0x38 + 0 * 0x20] < 20 ? 0x3 : 0;
        }
        r = 0;
        memcpy(&r, d.b + addr - 0x180, size);
    } else if (addr >= 0x188 && addr <= 0x18f) {
        union {
            uint8_t b[8];
            uint64_t l;
        } d;
        memcpy(d.b, ls2k1000ms->ddr_level_reg + 0x188, 8);
        for (i = 0; i <= 7; i++) {
            if (ls2k1000ms->ddr_level_reg[0x180] == 1) {
                d.b[i] = ls2k1000ms->ddr_level_reg[0x3a + (i + 1) * 0x20] >= 1
                && ls2k1000ms->ddr_level_reg[0x3a + (i + 1) * 0x20] < 20 ? 0x1
                : 0;
                /*glvl */
            } else if (ls2k1000ms->ddr_level_reg[0x180] == 2) {
                d.b[i] = ls2k1000ms->ddr_level_reg[0x38 + (i + 1) * 0x20] >= 1
                && ls2k1000ms->ddr_level_reg[0x38 + (i + 1) * 0x20] < 20 ? 0x3
                : 0;
            }
        }
        r = 0;
        memcpy(&r, d.b + addr - 0x188, size);
    } else if (addr >= 0x168 && addr < 0x170) {
        r = 0;
        memcpy(&r, ls2k1000ms->ddr_level_reg + addr, size);
        ((char *)&r)[4] = 0;
    } else {
        r = 0;
        memcpy(&r, ls2k1000ms->ddr_level_reg + addr, size);
    }
    return r;
}

static void ddr_config_writel(void *opaque, hwaddr addr,
                              uint64_t val, unsigned size) {
    LS2K1000MachineState* ls2k1000ms = LS2K1000_MACHINE(opaque);
    memcpy(ls2k1000ms->ddr_level_reg + addr, &val, size);
}

static const MemoryRegionOps ddr_config_ops = {
    .read = ddr_config_readl,
    .write = ddr_config_writel,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .max_access_size = 8,
    },
};

static uint64_t cpu_config_readl(void *opaque, hwaddr addr, unsigned size)
{
    LS2K1000MachineState* ls2k1000ms = LS2K1000_MACHINE(opaque);
    uint64_t r = 0;

    switch (addr) {
        case 0x0424:
            r = ls2k1000ms->reg_ddr;
            break;
        case 0x0480:
            r = 0x50010c85;
            break;
        case 0x0484:
            r = 0x00000450;
            break;
        case 0x0488:
            r = 0x00000002;
            break;
        case 0x048c:
            r = 0;
            break;
        case 0x0490:
            r = 0x10010c87;
            break;
        case 0x0494:
            r = 0x00000440;
            break;
        case 0x0498:
            r = 0x01c00004;
            break;
        case 0x049c:
            r = 0x00064000;
            break;
        case 0x04a0 ... 0x04af: {
            uint64_t data[2] = { 0x45010010c87, 0x4000008 };
            memcpy(&r, (char *)data + (addr - 0x04a0), size);
            break;
        }
        case 0x04b0:
            r = 0x10000;
            break;
        case 0x04c0:
            r = 0x10000;
    }

    return r;
}

static void cpu_config_writel(void *opaque, hwaddr addr,
                             uint64_t val, unsigned size)
{
    LS2K1000MachineState* ls2k1000ms = LS2K1000_MACHINE(opaque);
    switch (addr) {
        case 0x0424:
            ls2k1000ms->reg_ddr = val;
            memory_region_transaction_begin();
            if (ls2k1000ms->ddr_config.container == get_system_memory())
                memory_region_del_subregion(get_system_memory(),
                                            &ls2k1000ms->ddr_config);

            if ((val & 0x100) == 0) {
                memory_region_add_subregion_overlap(get_system_memory(),
                                                    0x0ff00000, &ls2k1000ms->ddr_config, 1);
            }

            memory_region_transaction_commit();
            break;
    }
}


static const MemoryRegionOps cpu_config_ops = {
    .read = cpu_config_readl,
    .write = cpu_config_writel,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .max_access_size = 8,
    },
};

static void ls2k1000_init(MachineState *ms)
{
    LS2K1000MachineState* ls2k1000ms = LS2K1000_MACHINE(ms);
    int i;

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

    for (i = 0; i < ms->smp.cpus; i++) {
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

    memory_region_init_alias(&ls2k1000ms->ram1, NULL, "lowmem256M", ms->ram, 0, ms->ram_size);
    memory_region_add_subregion(get_system_memory(), 0, &ls2k1000ms->ram1);
    memory_region_init_alias(&ls2k1000ms->ram2, NULL, "lowmem2G", ms->ram, 0, LOWMEM_2G_SIZE);
    memory_region_add_subregion(get_system_memory(), LOWMEM_2G_BASE, &ls2k1000ms->ram2);
    memory_region_init_alias(&ls2k1000ms->ram3, NULL, "mem_alias", ms->ram, 0, ms->ram_size);
    memory_region_add_subregion(get_system_memory(), MEM_ALIAS_BASE, &ls2k1000ms->ram3);

    memory_region_init_io(&ls2k1000ms->cpu_config, OBJECT(ms), &cpu_config_ops,
                          ls2k1000ms, "cpu_config", 0x500);
    memory_region_add_subregion(get_system_memory(), CFG_BASE,
                                &ls2k1000ms->cpu_config);
    memory_region_add_subregion(get_system_memory(), LIOINTC_BASE,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(ls2k1000ms->liointc), 0));
    memory_region_add_subregion(get_system_memory(), IPI_BASE,
                                 sysbus_mmio_get_region(SYS_BUS_DEVICE(ls2k1000ms->ipi), 2));
    memory_region_add_subregion(get_system_memory(), IPI_BASE + 0x100,
                                 sysbus_mmio_get_region(SYS_BUS_DEVICE(ls2k1000ms->ipi), 3));
    memory_region_init_io(&ls2k1000ms->ddr_config, OBJECT(ms), &ddr_config_ops,
                          ls2k1000ms, "ddr", 0x1000);
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
