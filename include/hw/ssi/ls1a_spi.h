#ifndef LS1A_SPI_H_INCLUDED
#define LS1A_SPI_H_INCLUDED

#include "hw/sysbus.h"

#define TYPE_LS1A_SPI "ls1a.spi"
#define LS1A_SPI(obj) OBJECT_CHECK(LS1ASPIState, (obj), TYPE_LS1A_SPI)

typedef struct LS1ASPIState{
    SysBusDevice busdev;
    MemoryRegion iomem;
    uint8_t spcr;
    uint8_t spsr;
    uint8_t spdr;
    uint8_t sper;
    uint8_t param;
    uint8_t cs;
    uint8_t timing;
    bool io_only;


    /* The FIFO head points to the next empty entry.  */
    uint32_t tx_fifo_wptr;
    uint32_t tx_fifo_rptr;
    uint32_t rx_fifo_wptr;
    uint32_t rx_fifo_rptr;
    int tx_fifo_len;
    int rx_fifo_len;
    uint16_t tx_fifo[8];
    uint16_t rx_fifo[8];
    qemu_irq irq;
    qemu_irq cs_line[4];
    SSIBus *ssi;
} LS1ASPIState;

#endif // LS1A_SPI_H_INCLUDED
