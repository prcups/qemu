/*
 * QEMU Loongson Local I/O interrupt controller.
 *
 * Copyright (c) 2020 Huacai Chen <chenhc@lemote.com>
 * Copyright (c) 2020 Jiaxun Yang <jiaxun.yang@flygoat.com>
 * Copyright (c) 2025 SignKirigami <prcups@krgm.moe>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/log.h"
#include "hw/irq.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "hw/intc/ls2k_liointc.h"

static void update_irq(LS2KLIOINTCState *p)
{
    int i, core, ip, j;
    bool *parent_new_state;

    /* level triggered interrupt */
    p->isr = p->pin_state;

    /* Clear disabled IRQs */
    p->isr &= p->ien;

    parent_new_state = g_new0(bool, p->num_cpu * NUM_IPS);

    /* Emit IRQ to parent! */
    for (i = 0; i < NUM_IRQS; ++i) {
        if (p->isr & (1 << i)) {
            core = -1;
            ip = -1;
            // TODO: consider intbounce and intauto
            for (j = 0; j < 4; ++j) {
                if (p->mapper[i] & (1 << j)) {
                    core = j;
                    break;
                }
            }
            for (j = 4; j < 8; ++j) {
                if (p->mapper[i] & (1 << j)) {
                    ip = j - 4;
                    break;
                }
            }
            if (core != -1 && ip != -1) {
                parent_new_state[PARENT_COREx_IPy(core, ip)] = 1;
            }
        }
    }
    for (i = 0; i < p->num_cpu * NUM_IPS; i++) {
        if (parent_new_state[i] != p->parent_state[i]) {
            p->parent_state[i] = parent_new_state[i];
            qemu_set_irq(p->parent_irq[i], p->parent_state[i]);
        }
    }
}

static uint64_t
liointc_read(void *opaque, hwaddr addr, unsigned int size)
{
    LS2KLIOINTCState *p = opaque;
    uint32_t r = 0;

    /* Mapper is 1 byte */
    if (size == 1 && addr < R_MAPPER_END) {
        r = p->mapper[addr];
        goto out;
    }

    if (size == 1 && addr > R_MAPPER_HIGH_START
        && addr < R_MAPPER_HIGH_END) {
        r = p->mapper[addr - R_MAPPER_HIGH_START + R_MAPPER_END];
        goto out;
    }

    /* Rest are 4 bytes */
    if (size != 4 || (addr % 4)) {
        goto out;
    }

    switch (addr) {
    case R_ISR:
        r = (uint32_t) p->isr;
        break;
    case R_IEN:
        r = (uint32_t) p->ien;
        break;
    case R_ISR_HIGH:
        r = (uint32_t) (p->isr >> 32);
        break;
    case R_IEN_HIGH:
        r = (uint32_t) (p->ien >> 32);
        break;
    default:
        break;
    }

out:
    qemu_log_mask(CPU_LOG_INT, "%s: size=%d, addr=%"HWADDR_PRIx", val=%x\n",
                  __func__, size, addr, r);
    return r;
}

static void
liointc_write(void *opaque, hwaddr addr,
          uint64_t val64, unsigned int size)
{
    LS2KLIOINTCState *p = opaque;
    uint32_t value = val64;

    /* Mapper is 1 byte */
    if (size == 1 && addr < R_MAPPER_END) {
        p->mapper[addr] = value;
        goto out;
    }

    if (size == 1 && addr > R_MAPPER_HIGH_START
        && addr < R_MAPPER_HIGH_END) {
        p->mapper[addr - R_MAPPER_HIGH_START + R_MAPPER_END] = value;
        goto out;
        }

    /* Rest are 4 bytes */
    if (size != 4 || (addr % 4)) {
        goto out;
    }

    switch (addr) {
    case R_IEN_SET:
        p->ien |= value;
        break;
    case R_IEN_CLR:
        p->ien &= ~value;
        break;
    case R_IEN_SET_HIGH:
        p->ien |= ((uint64_t) value << 32);
        break;
    case R_IEN_HIGH:
        p->ien &= ~((uint64_t) value << 32);
        break;
    default:
        break;
    }

out:
    qemu_log_mask(CPU_LOG_INT, "%s: size=%d, addr=%"HWADDR_PRIx", val=%x\n",
              __func__, size, addr, value);
    update_irq(p);
}

static const MemoryRegionOps pic_ops = {
    .read = liointc_read,
    .write = liointc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4
    }
};

static void irq_handler(void *opaque, int irq, int level)
{
    LS2KLIOINTCState *p = opaque;

    p->pin_state &= ~(1 << irq);
    p->pin_state |= level << irq;
    update_irq(p);
}

static const Property ls2k_liointc_properties[] = {
    DEFINE_PROP_UINT32("num-cpu", LS2KLIOINTCState, num_cpu, 1),
};

static void ls2k_liointc_realize(DeviceState *dev, Error **errp)
{
    LS2KLIOINTCState *s = LS2K_LIOINTC(dev);
    LS2KLIOINTCClass *k = LS2K_LIOINTC_GET_CLASS(dev);
    Error *local_err = NULL;

    k->parent_realize(dev, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    if (s->num_cpu == 0) {
        error_setg(errp, "num-cpu must be at least 1");
        return;
    }

    qdev_init_gpio_in(dev, irq_handler, 64);
    memory_region_init_io(&s->mmio, OBJECT(dev), &pic_ops, s,
                          TYPE_LS2K_LIOINTC, R_END);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);

    s->parent_irq = g_new0(qemu_irq, s->num_cpu * NUM_IPS);
    s->parent_state = g_new0(bool, s->num_cpu * NUM_IPS);

    qdev_init_gpio_out(dev, s->parent_irq, s->num_cpu * NUM_IPS);
}

static void ls2k_liointc_unrealize(DeviceState *dev)
{
    LS2KLIOINTCState *s = LS2K_LIOINTC(dev);
    LS2KLIOINTCClass *k = LS2K_LIOINTC_GET_CLASS(dev);

    g_free(s->parent_irq);
    g_free(s->parent_state);

    k->parent_unrealize(dev);
}

static void ls2k_liointc_class_init(ObjectClass* oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    LS2KLIOINTCClass *lc = LS2K_LIOINTC_CLASS(oc);
    device_class_set_parent_realize(dc, ls2k_liointc_realize,
                                    &lc->parent_realize);
    device_class_set_parent_unrealize(dc, ls2k_liointc_unrealize,
                                      &lc->parent_unrealize);
    device_class_set_props(dc, ls2k_liointc_properties);
}

static const TypeInfo ls2k_liointc_types[] = {
    {
        .name          = TYPE_LS2K_LIOINTC,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .class_size = sizeof(LS2KLIOINTCClass),
        .class_init = ls2k_liointc_class_init,
        .instance_size = sizeof(LS2KLIOINTCState),
    }
};

DEFINE_TYPES(ls2k_liointc_types)
