#ifndef HW_ATCWDT200_H
#define HW_ATCWDT200_H

#include "hw/sysbus.h"
#include "qemu/typedefs.h"
#define TYPE_ATCWDT200 "atcwdt200"
#define ATCWDT200(obj) OBJECT_CHECK(Atcwdt200State, (obj), TYPE_ATCWDT200)
#define ATCWDT200_WP_NUM 0x5aa5
#define ATCWDT200_RESTART_NUM 0xcafe

typedef struct Atcwdt200State {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    qemu_irq period_irq;
    qemu_irq alarm_irq;

    QEMUTimer *int_timer;
    QEMUTimer *rst_timer;
    uint64_t int_reload;
    uint64_t rst_reload;

    uint32_t ctrl;
    uint32_t st;

    uint32_t pclk;
    uint32_t extclk;

    uint32_t write_protection;
    uint32_t int_en;
} Atcwdt200State;

DeviceState *atcwdt200_create(hwaddr addr);
#endif /* HW_ATCWDT200_H */
