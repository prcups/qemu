/*
 * QEMU loongson 1a spi emulation
 *
 * Copyright (c) 2013 qiaochong@loongson.cn
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"
#include "hw/pci/pci.h"
#include "hw/ssi/ls1a_spi.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"

#define next_ptr(p) ((p + 1) & 7)

#define         W_FULL          (1 << 3)
#define         W_EMPTY         (1 << 2)
#define         R_FULL          (1 << 1)
#define         R_EMPTY         (1 << 0)

static void ls1a_spi_update(LS1ASPIState *s)
{
        s->spsr &= ~W_FULL;
        s->spsr |= W_EMPTY;
        if (s->rx_fifo_wptr != s->rx_fifo_rptr) {
                s->spsr &= ~R_EMPTY;
        } else {
                s->spsr |= R_EMPTY;
        }

        if (next_ptr(s->rx_fifo_wptr) == s->rx_fifo_rptr) {
                s->spsr |= R_FULL;
        } else {
                s->spsr &= ~R_FULL;
        }

        if (s->spsr & R_FULL) {
                qemu_set_irq(s->irq, 1);
        } else {
                qemu_set_irq(s->irq, 0);
        }
}



static uint64_t ls1a_spi_read(void *opaque, hwaddr offset, unsigned size)
{
        LS1ASPIState *s = (LS1ASPIState *)opaque;
        int val;

        switch (offset) {
        case 0x00: /* spcr */
                return s->spcr;
        case 0x01: /* spsr */
                return s->spsr;
        case 0x02: /* spdr */
                if (!(s->spsr & R_EMPTY)) {
                        val = s->rx_fifo[s->rx_fifo_rptr];
                        s->rx_fifo_rptr = next_ptr(s->rx_fifo_rptr);
                } else {
                        val = 0;
                }
                ls1a_spi_update(s);
                return val;
        case 0x03: /* sper */
                return s->sper;
        case 0x04:
                if (s->io_only) {
                        return s->cs;
                } else {
                        return s->param;
                }
        case 0x05: /*setcs*/
                if (!s->io_only) {
                        return s->cs;
                } else {
                        return 0;
                }
                break;
        case 0x06:
                if (!s->io_only) {
                        return s->timing;
                } else {
                        return 0;
                }
                break;
        default:
                return 0;
        }
}

static void ls1a_spi_write(void *opaque, hwaddr offset, uint64_t value,
                           unsigned size)
{
        LS1ASPIState *s = (LS1ASPIState *)opaque;
        uint32_t val;

        switch (offset) {
        case 0x00: /* spcr */
                s->spcr = value;
                break;
        case 0x01: /* spsr */
                s->spsr &= ~value;
                break;
        case 0x02: /* DR */
                val = ssi_transfer(s->ssi, value);
                s->spsr &= ~0x80;
                if (!(s->spsr & R_FULL)) {
                        s->rx_fifo[s->rx_fifo_wptr] = val;
                        s->rx_fifo_wptr = next_ptr(s->rx_fifo_wptr);
                        s->spsr |= 0x80;
                }
                ls1a_spi_update(s);
                break;
        case 0x3: /* sper */
                s->sper = value;
                break;
        case 0x4:
                if (s->io_only) {
                                if ((value & 3) == 1) {
                                        qemu_irq_raise(s->cs_line[0]);
                                } else if ((value & 3) == 0) {
                                        qemu_irq_lower(s->cs_line[0]);
                                }
                                s->cs = value;
                } else {
                        s->param = value;
                }
                break;
        case 0x5: /*softcs*/
                if (!s->io_only) {
                        int i;
                        for (i = 0; i < 4; i++) {
                                if ((value & (0x11 << i)) == (0x11 << i)) {
                                        qemu_irq_raise(s->cs_line[i]);
                                } else if ((value & (0x11 << i)) ==
                                           (0x01 << i)) {
                                        qemu_irq_lower(s->cs_line[i]);
                                }
                        }
                        s->cs = value;
                }
                break;
        case 0x6:
                if (!s->io_only) {
                        s->timing = value;
                }
                break;
        default:
                ;
        }
}

static void ls1a_spi_reset(LS1ASPIState *s)
{
        s->rx_fifo_len = 0;
        s->tx_fifo_len = 0;
        s->spcr = 0;
        s->spsr = 5;
        s->sper = 0;
}

static const MemoryRegionOps ls1a_spi_ops = {
        .read = ls1a_spi_read,
        .write = ls1a_spi_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
};

static void ls1a_spi_init(Object *obj)
{
    SysBusDevice *dev = SYS_BUS_DEVICE(obj);
    LS1ASPIState *d = LS1A_SPI(dev);
    int i;

    memory_region_init_io(&d->iomem, NULL, &ls1a_spi_ops, (void *)d,
                          "ls1a spi", 0x8);

    sysbus_init_irq(dev, &d->irq);
    for (i = 0; i < 4; i++) {
        sysbus_init_irq(dev, &d->cs_line[i]);
    }
    sysbus_init_mmio(dev, &d->iomem);
    d->ssi = ssi_create_bus(DEVICE(dev), "spi");
    ls1a_spi_reset(d);
}

static Property spi_sysbus_properties[] = {
        DEFINE_PROP_BOOL("io-only", LS1ASPIState, io_only, false),
};

static void ls1a_spi_class_init(ObjectClass *klass, const void *data)
{
        DeviceClass *dc = DEVICE_CLASS(klass);

        dc->desc = "ls1a i2c";
        device_class_set_props(dc, spi_sysbus_properties);
}

static const TypeInfo ls1a_spi_info = {
        .name          = TYPE_LS1A_SPI,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(LS1ASPIState),
        .instance_init = ls1a_spi_init,
        .class_init    = ls1a_spi_class_init,
};

static void ls1a_spi_register_types(void)
{
        type_register_static(&ls1a_spi_info);
}

type_init(ls1a_spi_register_types)
