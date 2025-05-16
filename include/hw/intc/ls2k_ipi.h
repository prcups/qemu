/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Loongson ipi interrupt header files
 *
 * Copyright (C) 2021 Loongson Technology Corporation Limited
 */

#ifndef HW_LS2K_IPI_H
#define HW_LS2K_IPI_H

#include "qom/object.h"
#include "hw/intc/loongson_ipi_common.h"
#include "hw/sysbus.h"

#define TYPE_LS2K_IPI "ls2k_ipi"
OBJECT_DECLARE_TYPE(LS2KIPIState, LS2KIPIClass, LS2K_IPI)

struct LS2KIPIClass {
    LoongsonIPICommonClass parent_class;

    DeviceRealize parent_realize;
    DeviceUnrealize parent_unrealize;
};

struct LS2KIPIState {
    LoongsonIPICommonState parent_obj;

    MemoryRegion *ipi_mmio_mem;
};

#endif
