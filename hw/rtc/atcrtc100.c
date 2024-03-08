/*
 * Andes Real-time Clcok, ATCRTC100.
 *
 * Copyright (c) 2023 Andes Tech. Corp.
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
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/irq.h"
#include "hw/ptimer.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "hw/rtc/atcrtc100.h"

#define xLOG(x...)
#define yLOG(x...) qemu_log(x)
#define zLOG(x...) printf(x)

#define LOG(x...) xLOG(x)
#define LOGGE(x...) qemu_log_mask(LOG_GUEST_ERROR, x)

#define DEVNAME "ATCRTC100"

enum {
    ID_RTC = 0x030110,
    ID_RTC_SHIFT = 8,
};

enum {
    VER_MAJOR = 0x00,
    VER_MAJOR_SHIFT = 4,
    VER_MINOR = 0x00,
    VER_MINOR_SHIFT = 0,
};

enum {
    REG_IDREV = 0x00,
    REG_CNTR = 0x10,
    REG_ALARM = 0x14,
    REG_CTRL = 0x18,
    REG_ST = 0x1c,
    REG_TRIM = 0x20,
};

enum {
    CNTR_DAY = 17,
    CNTR_HOUR = 12,
    CNTR_MIN = 6,
    CNTR_SEC = 0,
};

/* ATCRTC100_DAY_BITS 5 */
#define CNTR_DAY_MASK  0x1F
#define CNTR_HOUR_MASK 0x1F
#define CNTR_MIN_MASK  0x3F
#define CNTR_SEC_MASK  0x3F

enum {
    CTRL_FREQ_TEST_EN = 8,
    CTRL_HSEC = 7,
    CTRL_SEC = 6,
    CTRL_MIN = 5,
    CTRL_HOUR = 4,
    CTRL_DAY = 3,
    CTRL_ALARM_INT = 2,
    CTRL_ALARM_WAKEUP = 1,
    CTRL_RTC_EN = 0,
};

/* CTRL_INT: CTRL_ALARM_INT ~ CTRL_HSEC */
#define CTRL_INT      CTRL_ALARM_INT
#define CTRL_INT_MASK 0x3F

enum {
    ST_WRITEDONE = 16,
    ST_HSEC = 7,
    ST_SEC = 6,
    ST_MIN = 5,
    ST_HOUR = 4,
    ST_DAY = 3,
    ST_ALARM_INT = 2,
};

/* ST_PERIOD: ST_DAY ~ ST_HSEC*/
#define ST_PERIOD          ST_DAY
#define ST_PERIOD_MASK     0x1F
#define ST_ALARM_INT_MASK  0x1

#define NANOSECONDS_PER_HALFSECOND (NANOSECONDS_PER_SECOND / 2)

static inline uint64_t
tm2ns(struct tm tm)
{
    return (((((((uint64_t)tm.tm_mday * 24)
              + tm.tm_hour) * 60)
              + tm.tm_min) * 60)
              + tm.tm_sec) * NANOSECONDS_PER_SECOND;
}

static inline struct tm
ns2tm(uint64_t ns)
{
    struct tm tm;
    tm.tm_sec = ns / NANOSECONDS_PER_SECOND;
    tm.tm_min = tm.tm_sec / 60;
    tm.tm_sec %= 60;
    tm.tm_hour = tm.tm_min / 60;
    tm.tm_min %= 60;
    tm.tm_mday = tm.tm_hour / 24;
    return tm;
}

static inline struct tm
cntr2tm(uint32_t cntr)
{
    struct tm tm;
    tm.tm_mday = (cntr >> CNTR_DAY) & CNTR_DAY_MASK;
    tm.tm_hour = (cntr >> CNTR_HOUR) & CNTR_HOUR_MASK;
    tm.tm_min = (cntr >> CNTR_MIN) & CNTR_MIN_MASK;
    tm.tm_sec = (cntr >> CNTR_SEC) & CNTR_SEC_MASK;
    return tm;
}

static inline uint32_t
tm2cntr(struct tm tm)
{
    return tm.tm_mday << CNTR_DAY | tm.tm_hour << CNTR_HOUR |
           tm.tm_min << CNTR_MIN | tm.tm_sec;
}

static inline uint32_t
ns2cntr(uint64_t ns)
{
    return tm2cntr(ns2tm(ns));
}

static void
atcrtc100_reset(DeviceState *dev)
{
    Atcrtc100State *s = ATCRTC100(dev);
    s->cntr = 0;
    s->alarm = 0;
    s->ctrl = 0;
    s->st = (1 << ST_WRITEDONE);
    s->trim = 0;
    s->ref_rtc_start_ns = 0;
    s->pause_ns = 0;
    s->period_tm = cntr2tm(0);
    LOG("%s:\n", __func__);
}

static void
atcrtc100_realize(DeviceState *dev, Error **errp)
{
    LOG("%s:\n", __func__);
}

static uint64_t
read_atcrtc100_cntr(Atcrtc100State *s)
{
    if (s->ctrl & (1 << CTRL_RTC_EN)) {
        return ns2cntr(
                qemu_clock_get_ns(QEMU_CLOCK_REALTIME)
                - s->ref_rtc_start_ns);
    } else {
        return ns2cntr(s->pause_ns);
    }
}

static void
write_atcrtc100_cntr(Atcrtc100State *s, uint64_t value)
{
    struct tm value_tm = cntr2tm(value);
    s->period_tm = value_tm;
    s->ref_rtc_start_ns = qemu_clock_get_ns(QEMU_CLOCK_REALTIME)
                          - tm2ns(value_tm);
    s->pause_ns = 0;
}

static inline uint64_t
next_half_second_ns(uint64_t ns)
{
    return ((ns + (NANOSECONDS_PER_HALFSECOND)) / NANOSECONDS_PER_HALFSECOND)
           * NANOSECONDS_PER_HALFSECOND;
}

static void
write_atcrtc100_ctrl(Atcrtc100State *s, uint64_t value)
{
    /* RTC time = qemu_clock_get_ns - ref_rtc_start_ns */
    if (value & (1 << CTRL_RTC_EN) && !(s->ctrl & (1 << CTRL_RTC_EN))) {
        /*
         * Resume, modify ref_rtc_start_ns to make resumed RTC time start
         * from pause_ns
         */
        s->ref_rtc_start_ns = qemu_clock_get_ns(QEMU_CLOCK_REALTIME)
                              - s->pause_ns;
    } else if ((!(value & (1 << CTRL_RTC_EN))) &&
               (s->ctrl & (1 << CTRL_RTC_EN))) {
        /* Pause, record current RTC time to pause_ns */
        s->pause_ns = qemu_clock_get_ns(QEMU_CLOCK_REALTIME)
                      - s->ref_rtc_start_ns;
    }
    if (value & (1 << CTRL_RTC_EN) && value & (CTRL_INT_MASK << CTRL_INT)) {
        uint64_t ns = qemu_clock_get_ns(QEMU_CLOCK_REALTIME)
                      - s->ref_rtc_start_ns;
        timer_mod(s->rtc_timer, next_half_second_ns(ns) + s->ref_rtc_start_ns);
    }
    s->ctrl = value;
}

static uint64_t
atcrtc100_read(void *opaque, hwaddr addr, unsigned size)
{
    Atcrtc100State *s = ATCRTC100(opaque);
    uint64_t rz = 0;

    switch (addr) {
    case REG_IDREV ... REG_TRIM:
        switch (addr) {
        case REG_IDREV: /* RO */
            rz = ID_RTC << ID_RTC_SHIFT | VER_MAJOR << VER_MAJOR_SHIFT |
                 VER_MINOR << VER_MINOR_SHIFT;
            break;
        case REG_CNTR:
            rz = read_atcrtc100_cntr(s);
            break;
        case REG_ALARM:
            rz = s->alarm;
            break;
        case REG_CTRL:
            rz = s->ctrl;
            break;
        case REG_ST:
            rz = s->st;
            break;
        case REG_TRIM:
            rz = s->trim;
            break;

        default:
            LOGGE("%s: Bad addr %x\n", __func__, (int)addr);
            break;
        }
        LOG("\e[95m%s: addr %08x, value %08x\e[0m\n", __func__, (int)addr,
            (int)rz);
        break;
    default:
        LOGGE("%s: Bad addr %x\n", __func__, (int)addr);
    }

    return rz;
}


static void
atcrtc100_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    Atcrtc100State *s = ATCRTC100(opaque);

    s->st &= ~(1 << ST_WRITEDONE);
    switch (addr) {
    case REG_IDREV ... REG_TRIM:
        switch (addr) {
        case REG_IDREV: /* RO */
            break;
        case REG_CNTR:
            write_atcrtc100_cntr(s, value);
            break;
        case REG_ALARM:
            s->alarm = value;
            break;
        case REG_CTRL:
            write_atcrtc100_ctrl(s, value);
            break;
        case REG_ST:
            s->st &= ~(value & 0xFC);
            if (!(s->st & (ST_PERIOD_MASK << ST_PERIOD))) {
                qemu_irq_lower(s->period_irq);
            }
            if (!(s->st & (ST_ALARM_INT_MASK << ST_ALARM_INT))) {
                qemu_irq_lower(s->alarm_irq);
            }
            break;
        case REG_TRIM:
            s->trim = value;
            break;

        default:
            LOGGE("%s: Bad addr %x\n", __func__, (int)addr);
            break;
        }
        LOG("\e[95m%s: addr %08x, value %08x\e[0m\n", __func__, (int)addr,
            (int)rz);
        break;
    default:
        LOGGE("%s: Bad addr %x\n", __func__, (int)addr);
    }
    s->st |= (1 << ST_WRITEDONE);
}

static void
atcrtc100_timer_cb(void *opaque)
{
    Atcrtc100State *s = ATCRTC100(opaque);
    uint64_t curr_ns = qemu_clock_get_ns(QEMU_CLOCK_REALTIME)
                       - s->ref_rtc_start_ns;
    struct tm curr_tm = ns2tm(curr_ns);
    timer_mod(s->rtc_timer, next_half_second_ns(curr_ns) + s->ref_rtc_start_ns);
    if (s->ctrl & (1 << CTRL_HSEC)) {
        s->st |= (1 << ST_HSEC);
    }
    if (s->ctrl & (1 << CTRL_SEC)) {
        if (curr_tm.tm_sec != s->period_tm.tm_sec) {
            s->st |= (1 << ST_SEC);
        }
    }
    if (s->ctrl & (1 << CTRL_MIN)) {
        if (curr_tm.tm_min != s->period_tm.tm_min) {
            s->st |= (1 << ST_MIN);
        }
    }
    if (s->ctrl & (1 << CTRL_HOUR)) {
        if (curr_tm.tm_hour != s->period_tm.tm_hour) {
            s->st |= (1 << ST_HOUR);
        }
    }
    if (s->ctrl & (1 << CTRL_DAY)) {
        if (curr_tm.tm_mday != s->period_tm.tm_mday) {
            s->st |= (1 << ST_DAY);
        }
    }
    s->period_tm = curr_tm;

    if (s->ctrl & (1 << CTRL_ALARM_INT)) {
        /* Only trigger alarm in first half second */
        if (tm2cntr(curr_tm) == s->alarm && (curr_ns % NANOSECONDS_PER_SECOND)
            < NANOSECONDS_PER_SECOND / 2) {
            s->st |= (1 << ST_ALARM_INT);
        }
    }
    if (s->st & (ST_PERIOD_MASK << ST_PERIOD)) {
        qemu_irq_raise(s->period_irq);
    }
    if (s->st & (ST_ALARM_INT_MASK << ST_ALARM_INT)) {
        qemu_irq_raise(s->alarm_irq);
    }
}

static const MemoryRegionOps atcrtc100_ops = {
    .read = atcrtc100_read,
    .write = atcrtc100_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {.min_access_size = 4, .max_access_size = 4}
};


static void
atcrtc100_class_init(ObjectClass *klass, void *data)
{
    LOG("%s:\n", __func__);
    DeviceClass *k = DEVICE_CLASS(klass);
    k->realize = atcrtc100_realize;
    k->reset = atcrtc100_reset;
}

static void
atcrtc100_init(Object *obj)
{
    LOG("%s:\n", __func__);
    Atcrtc100State *s = ATCRTC100(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    sysbus_init_irq(sbd, &s->period_irq);
    sysbus_init_irq(sbd, &s->alarm_irq);
    s->st = (1 << ST_WRITEDONE);
    s->rtc_timer = timer_new_ns(QEMU_CLOCK_REALTIME, atcrtc100_timer_cb, obj);
    memory_region_init_io(&s->mmio, obj, &atcrtc100_ops, s, TYPE_ATCRTC100,
                          0x100);
    sysbus_init_mmio(sbd, &s->mmio);
}

static const TypeInfo atcrtc100_info = {
    .name = TYPE_ATCRTC100,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Atcrtc100State),
    .instance_init = atcrtc100_init,
    .class_init = atcrtc100_class_init,
};

static void
atcrtc100_register_types(void)
{
    LOG("%s:\n", __func__);
    type_register_static(&atcrtc100_info);
}

type_init(atcrtc100_register_types)

DeviceState *
atcrtc100_create(hwaddr addr, qemu_irq period_irq, qemu_irq alarm_irq)
{
    LOG("%s:\n", __func__);
    DeviceState *dev;
    dev = sysbus_create_varargs(TYPE_ATCRTC100, addr, period_irq, alarm_irq,
                                NULL);
    return dev;
}
