/*
 * Andes Watch Dog Timer, atcwdt200.
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
#include "hw/nmi.h"

#include "sysemu/watchdog.h"
#include "sysemu/runstate.h"
#include "qapi/qapi-commands-run-state.h"
#include "hw/watchdog/atcwdt200.h"
#include "hw/misc/andes_atcsmu.h"


#define xLOG(x...)
#define yLOG(x...) qemu_log(x)
#define zLOG(x...) printf(x)

#define LOG(x...) xLOG(x)
#define LOGGE(x...) qemu_log_mask(LOG_GUEST_ERROR, x)

#define DEVNAME "atcwdt200"

enum {
    ID_WDT = 0x03002,
    ID_WDT_SHIFT = 12,
};

enum {
    VER_MAJOR = 0x00,
    VER_MAJOR_SHIFT = 4,
    VER_MINOR = 0x00,
    VER_MINOR_SHIFT = 0,
};

enum {
    REG_IDREV = 0x00,
    REG_CTRL = 0x10,
    REG_RESTART = 0x14,
    REG_WREN = 0x18,
    REG_ST = 0x1c,
};

enum {
    CTRL_RSTTIME = 8,
    CTRL_RSTTIME_MASK = 0x7,
    CTRL_INTTIME = 4,
    CTRL_INTTIME_MASK = 0xF,
    CTRL_RSTEN = 3,
    CTRL_INTEN = 2,
    CTRL_CLK_SEL = 1,
    CTRL_EN = 0,
};

int RSTTIME_POW_2[] = {7, 8, 9, 10, 11, 12, 13, 14};
int INTTIME_POW_2[] = {6, 8, 10, 11, 12, 13, 14, 15,
                       17, 19, 21, 23, 25, 27, 29, 31};


static void
atcwdt200_reset(DeviceState *dev)
{
    Atcwdt200State *s = ATCWDT200(dev);
    s->ctrl = 0;
    s->st = 0;
    LOG("%s:\n", __func__);
}

static void
atcwdt200_realize(DeviceState *dev, Error **errp)
{
    LOG("%s:\n", __func__);
}

static void
write_atcwdt200_ctrl(Atcwdt200State *s, uint64_t value)
{
    if (!s->write_protection) {
        uint64_t clk;
        if (value >> CTRL_CLK_SEL & 0x1) {
            clk = s->pclk;
        } else {
            clk = s->extclk;
        }
        s->int_reload = 1 <<
            INTTIME_POW_2[((value >> CTRL_INTTIME) & CTRL_INTTIME_MASK)];
        s->rst_reload = 1 <<
            RSTTIME_POW_2[((value >> CTRL_RSTTIME) & CTRL_RSTTIME_MASK)];
        if (((value >> CTRL_INTEN) & 0x1) && ((value >> CTRL_EN) & 0x1)) {
            s->int_en = 1;
            timer_del(s->int_timer);
            timer_mod(s->int_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_REALTIME) +
                      s->int_reload * NANOSECONDS_PER_SECOND / clk);
        } else {
            s->int_en = 0;
            timer_del(s->int_timer);
        }

        if (((value >> CTRL_RSTEN) & 0x1) && ((value >> CTRL_EN) & 0x1)) {
            /* no interrupt stage */
            if (!s->int_en) {
                timer_del(s->rst_timer);
                timer_mod(s->rst_timer,
                          qemu_clock_get_ns(QEMU_CLOCK_REALTIME) +
                          s->rst_reload * NANOSECONDS_PER_SECOND / clk);
            }
        } else {
            timer_del(s->rst_timer);
        }
        s->ctrl = value;
    }
    s->write_protection = 1;
}

static void
write_atcwdt200_restart(Atcwdt200State *s, uint64_t value)
{
    if (!s->write_protection) {
        if (value == ATCWDT200_RESTART_NUM) {
            uint64_t clk;
            if (s->ctrl >> CTRL_CLK_SEL & 0x1) {
                clk = s->pclk;
            } else {
                clk = s->extclk;
            }
            if (((s->ctrl >> CTRL_INTEN) & 0x1) &&
                ((s->ctrl >> CTRL_EN) & 0x1)) {
                timer_del(s->int_timer);
                timer_mod(s->int_timer,
                          qemu_clock_get_ns(QEMU_CLOCK_REALTIME) +
                          s->int_reload * (NANOSECONDS_PER_SECOND / clk));
            }
        }
    }
    s->write_protection = 1;
}

static void
write_atcwdt200_wren(Atcwdt200State *s, uint64_t value)
{
    if (value == ATCWDT200_WP_NUM) {
        s->write_protection = 0;
    }
}

static uint64_t
atcwdt200_read(void *opaque, hwaddr addr, unsigned size)
{
    Atcwdt200State *s = ATCWDT200(opaque);
    uint64_t rz = 0;

    switch (addr) {
    case REG_IDREV ... REG_ST:
        switch (addr) {
        case REG_IDREV: /* RO */
            rz = ID_WDT << ID_WDT_SHIFT | VER_MAJOR << VER_MAJOR_SHIFT |
                 VER_MINOR << VER_MINOR_SHIFT;
            break;
        case REG_CTRL:
            rz = s->ctrl;
            break;
        case REG_RESTART: /* WP */
            break;
        case REG_WREN: /* WO */
            break;
        case REG_ST:
            rz = s->st;
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
atcwdt200_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    Atcwdt200State *s = ATCWDT200(opaque);

    switch (addr) {
    case REG_IDREV ... REG_ST:
        switch (addr) {
        case REG_IDREV: /* RO */
            break;
        case REG_CTRL:
            write_atcwdt200_ctrl(s, value);
            break;
        case REG_RESTART:
            write_atcwdt200_restart(s, value);
            break;
        case REG_WREN:
            write_atcwdt200_wren(s, value);
            break;
        case REG_ST:
            s->st &= ~(value & 0x1);
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
}

static void
atcwdt200_int_timer_cb(void *opaque)
{
    Atcwdt200State *s = ATCWDT200(opaque);
    s->st |= 0x1;
    if (((s->ctrl >> CTRL_RSTEN) & 0x1) && ((s->ctrl >> CTRL_EN) & 0x1)) {
        uint64_t clk;
        if (s->ctrl >> CTRL_CLK_SEL & 0x1) {
            clk = s->pclk;
        } else {
            clk = s->extclk;
        }
        timer_mod(s->rst_timer,
                  qemu_clock_get_ns(QEMU_CLOCK_REALTIME) +
                  s->rst_reload * (NANOSECONDS_PER_SECOND / clk));
    } else {
        timer_del(s->rst_timer);
    }
    qmp_watchdog_set_action(WATCHDOG_ACTION_INJECT_NMI, NULL);
    watchdog_perform_action();
}

static void
atcwdt200_reset_timer_cb(void *opaque)
{
    Object *obj;
    obj = container_get(qdev_get_machine(), "/soc/atcsmu");
    AndesATCSMUState *smu_state = ANDES_ATCSMU(obj);
    smu_state->wrsr |= 0x8;
    qmp_watchdog_set_action(WATCHDOG_ACTION_RESET, NULL);
    watchdog_perform_action();
}


static Property atcwdt200_properties[] = {
    DEFINE_PROP_UINT32("pclk", Atcwdt200State, pclk, 60 * 1000 * 1000),
    DEFINE_PROP_UINT32("extclk", Atcwdt200State, extclk, 32768),
    DEFINE_PROP_END_OF_LIST(),
};


static const MemoryRegionOps atcwdt200_ops = {
    .read = atcwdt200_read,
    .write = atcwdt200_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {.min_access_size = 4, .max_access_size = 4}
};


static void
atcwdt200_class_init(ObjectClass *klass, void *data)
{
    LOG("%s:\n", __func__);
    DeviceClass *k = DEVICE_CLASS(klass);

    set_bit(DEVICE_CATEGORY_WATCHDOG, k->categories);
    k->realize = atcwdt200_realize;
    k->reset = atcwdt200_reset;

    device_class_set_props(k, atcwdt200_properties);
}

static void
atcwdt200_init(Object *obj)
{
    LOG("%s:\n", __func__);
    Atcwdt200State *s = ATCWDT200(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    sysbus_init_irq(sbd, &s->period_irq);
    sysbus_init_irq(sbd, &s->alarm_irq);
    s->write_protection = 1;
    s->int_timer = timer_new_ns(QEMU_CLOCK_REALTIME,
        atcwdt200_int_timer_cb, obj);
    s->rst_timer = timer_new_ns(QEMU_CLOCK_REALTIME,
        atcwdt200_reset_timer_cb, obj);
    s->int_en = 0;
    memory_region_init_io(&s->mmio, obj, &atcwdt200_ops, s, TYPE_ATCWDT200,
                          0x100);
    sysbus_init_mmio(sbd, &s->mmio);
}

static const TypeInfo atcwdt200_info = {
    .name = TYPE_ATCWDT200,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Atcwdt200State),
    .instance_init = atcwdt200_init,
    .class_init = atcwdt200_class_init,
};

static void
atcwdt200_register_types(void)
{
    LOG("%s:\n", __func__);
    type_register_static(&atcwdt200_info);
}

type_init(atcwdt200_register_types)

DeviceState *
atcwdt200_create(hwaddr addr)
{
    LOG("%s:\n", __func__);
    DeviceState *dev;
    dev = sysbus_create_varargs(TYPE_ATCWDT200, addr, NULL);
    return dev;
}
