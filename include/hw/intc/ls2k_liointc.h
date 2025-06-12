/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2020 Huacai Chen <chenhc@lemote.com>
 * Copyright (c) 2020 Jiaxun Yang <jiaxun.yang@flygoat.com>
 * Copyright (c) 2025 SignKirigami <prcups@krgm.moe>
 *
 */

#ifndef LS2K_LIOINTC_H
#define LS2K_LIOINTC_H

#include "qemu/units.h"
#include "hw/sysbus.h"
#include "qom/object.h"

#define NUM_IRQS                64
#define NUM_IPS                 4
#define PARENT_COREx_IPy(x, y)  (NUM_IPS * x + y)

#define R_MAPPER_START          0x0
#define R_MAPPER_END            0x20
#define R_MAPPER_HIGH_START     0x40
#define R_MAPPER_HIGH_END       0x60
#define R_ISR                   R_MAPPER_END
#define R_IEN                   0x24
#define R_IEN_SET               0x28
#define R_IEN_CLR               0x2c
#define R_ISR_HIGH              R_MAPPER_HIGH_END
#define R_IEN_HIGH              0x64
#define R_IEN_SET_HIGH          0x68
#define R_IEN_CLR_HIGH          0x6c
#define R_ISR_SIZE              0x8
#define R_END                   0x80

#define TYPE_LS2K_LIOINTC "ls2k.liointc"
OBJECT_DECLARE_TYPE(LS2KLIOINTCState, LS2KLIOINTCClass, LS2K_LIOINTC)

struct LS2KLIOINTCState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    qemu_irq *parent_irq;

    uint32_t num_cpu;
    uint8_t mapper[NUM_IRQS]; /* 0:3 for core, 4:7 for IP */
    uint64_t isr;
    uint64_t ien;
    uint64_t pin_state;
    bool *parent_state;
};

struct LS2KLIOINTCClass {
    SysBusDeviceClass parent_class;

    DeviceRealize parent_realize;
    DeviceUnrealize parent_unrealize;
};

#endif /* LS2K_LIOINTC_H */
