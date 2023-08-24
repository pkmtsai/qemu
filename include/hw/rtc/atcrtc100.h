#ifndef HW_ATCRTC100_H
#define HW_ATCRTC100_H

#include "hw/sysbus.h"
#include "qemu/typedefs.h"
#define TYPE_ATCRTC100 "atcrtc100"
#define ATCRTC100(obj) OBJECT_CHECK(Atcrtc100State, (obj), TYPE_ATCRTC100)

typedef struct Atcrtc100State {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    qemu_irq period_irq;
    qemu_irq alarm_irq;
    int64_t ref_rtc_start_ns;
    int64_t pause_ns;
    struct tm period_tm;
    QEMUTimer *rtc_timer;

    uint32_t id_rev;
    uint32_t cntr;
    uint32_t alarm;
    uint32_t ctrl;
    uint32_t st;
    uint32_t trim;
} Atcrtc100State;

DeviceState *atcrtc100_create(hwaddr addr, qemu_irq period_irq,
                              qemu_irq alarm_irq);
#endif /* HW_ATCRTC100_H */
