/*
 * Andes PLMT (Platform Level Machine Timer)
 *
 * Copyright (c) 2021 Andes Tech. Corp.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/timer.h"
#include "qemu/module.h"
#include "hw/sysbus.h"
#include "target/riscv/cpu.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/timer/andes_plmt.h"

typedef struct andes_plmt_callback {
    AndesPLMTState *plmt;
    int hartid;
} andes_plmt_callback;

static uint64_t andes_cpu_riscv_read_rtc_raw(uint32_t timebase_freq)
{
    return muldiv64(qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL),
                    timebase_freq, NANOSECONDS_PER_SECOND);
}

static uint64_t andes_cpu_riscv_read_rtc(void *opaque)
{
    AndesPLMTState *plmt = opaque;
    return andes_cpu_riscv_read_rtc_raw(plmt->timebase_freq);
}

/*
 * Called when timecmp is written to update the QEMU timer or immediately
 * trigger timer interrupt if timecmp <= current timer value.
 */
static void
andes_plmt_write_timecmp(AndesPLMTState *plmt, RISCVCPU *cpu,
                         uint64_t value, uint64_t hartid)
{
    uint64_t next;
    uint64_t diff;

    uint64_t rtc_r = andes_cpu_riscv_read_rtc(plmt);

    plmt->timecmp[hartid] = (uint64_t)value;
    if (plmt->timecmp[hartid] <= rtc_r) {
        /*
         * if we're setting an timecmp value in the "past",
         * immediately raise the timer interrupt
         */
        qemu_irq_raise(plmt->timer_irqs[hartid]);
        return;
    }

    /* otherwise, set up the future timer interrupt */
    qemu_irq_lower(plmt->timer_irqs[hartid]);
    diff = plmt->timecmp[hartid] - rtc_r;

    /* back to ns (note args switched in muldiv64) */
    next = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
            muldiv64(diff, NANOSECONDS_PER_SECOND, plmt->timebase_freq);
    timer_mod(plmt->timers[hartid], next);
}

/*
 * Callback used when the timer set using timer_mod expires.
 * Should raise the timer interrupt line
 */
static void
andes_plmt_timer_cb(void *opaque)
{
    andes_plmt_callback *cb = opaque;
    qemu_irq_raise(cb->plmt->timer_irqs[cb->hartid]);
}

static uint64_t
andes_plmt_read(void *opaque, hwaddr addr, unsigned size)
{
    AndesPLMTState *plmt = opaque;
    uint64_t rz = 0;

    if ((addr >= (plmt->timecmp_base)) &&
        (addr < (plmt->timecmp_base + (plmt->num_harts << 3)))) {
        /* %8=0:timecmp_lo, %8=4:timecmp_hi */
        size_t hartid = plmt->hart_base + ((addr - plmt->timecmp_base) >> 3);
        CPUState *cpu = qemu_get_cpu(hartid);
        CPURISCVState *env = cpu_env(cpu);
        if (!env) {
            error_report("plmt: invalid timecmp hartid: %zu", hartid);
        } else if ((addr & 0x7) == 0) {
            rz = plmt->timecmp[hartid] & (unsigned long)0xFFFFFFFF;
        } else if ((addr & 0x7) == 4) {
            rz = (plmt->timecmp[hartid] >> 32) & (unsigned long)0xFFFFFFFF;
        } else {
            error_report("plmt: invalid read: %08x", (uint32_t)addr);
        }
    } else if (addr == (plmt->time_base)) {
        /* time_lo */
        rz = andes_cpu_riscv_read_rtc(plmt)
                & (unsigned long)0xFFFFFFFF;
    } else if (addr == (plmt->time_base + 4)) {
        /* time_hi */
        rz = ((andes_cpu_riscv_read_rtc((plmt)) >> 32)
                & (unsigned long)0xFFFFFFFF);
    } else {
        error_report("plmt: invalid read: %08x", (uint32_t)addr);
    }

    return rz;
}

static void
andes_plmt_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    AndesPLMTState *plmt = opaque;

    if ((addr >= (plmt->timecmp_base)) &&
        (addr < (plmt->timecmp_base + (plmt->num_harts << 3)))) {
        /* %8=0:timecmp_lo, %8=4:timecmp_hi */
        size_t hartid = plmt->hart_base + ((addr - plmt->timecmp_base) >> 3);
        CPUState *cpu = qemu_get_cpu(hartid);
        CPURISCVState *env = cpu_env(cpu);
        if (!env) {
            error_report("plmt: invalid timecmp hartid: %zu", hartid);
        } else if ((addr & 0x7) == 0) {
            uint64_t timecmp_hi = plmt->timecmp[hartid] >> 32;
            andes_plmt_write_timecmp(plmt, RISCV_CPU(cpu),
                                     (timecmp_hi << 32) |
                                     (value & (unsigned long)0xFFFFFFFF),
                                     hartid);
        } else if ((addr & 0x7) == 4) {
            uint64_t timecmp_lo = plmt->timecmp[hartid];
            andes_plmt_write_timecmp(plmt, RISCV_CPU(cpu),
                                     (value << 32) |
                                     (timecmp_lo & (unsigned long)0xFFFFFFFF),
                                     hartid);
        } else {
            error_report("plmt: invalid write: %08x", (uint32_t)addr);
        }
    } else if (addr == (plmt->time_base)) {
        /* time_lo */
        error_report("plmt: time_lo write not implemented");
    } else if (addr == (plmt->time_base + 4)) {
        /* time_hi */
        error_report("plmt: time_hi write not implemented");
    } else {
        error_report("plmt: invalid write: %08x", (uint32_t)addr);
    }
}

static const MemoryRegionOps andes_plmt_ops = {
    .read = andes_plmt_read,
    .write = andes_plmt_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 8
    }
};

static Property andes_plmt_properties[] = {
    DEFINE_PROP_UINT32("num-harts", AndesPLMTState, num_harts, 0),
    DEFINE_PROP_UINT32("time-base", AndesPLMTState, time_base, 0),
    DEFINE_PROP_UINT32("timecmp-base", AndesPLMTState, timecmp_base, 0),
    DEFINE_PROP_UINT32("aperture-size", AndesPLMTState, aperture_size, 0),
    DEFINE_PROP_UINT32("timebase-freq", AndesPLMTState, timebase_freq, 0),
    DEFINE_PROP_UINT32("hart-base", AndesPLMTState, hart_base, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void andes_plmt_realize(DeviceState *dev, Error **errp)
{
    AndesPLMTState *plmt = ANDES_PLMT(dev);
    memory_region_init_io(&plmt->mmio, OBJECT(dev), &andes_plmt_ops, plmt,
                          TYPE_ANDES_PLMT, plmt->aperture_size);

    plmt->timers = g_new0(QEMUTimer *, plmt->num_harts);
    plmt->timecmp = g_new0(uint64_t, plmt->num_harts);

    plmt->timer_irqs = g_new(qemu_irq, plmt->num_harts);
    qdev_init_gpio_out(dev, plmt->timer_irqs, plmt->num_harts);

    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &plmt->mmio);
}

static void andes_plmt_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = andes_plmt_realize;
    device_class_set_props(dc, andes_plmt_properties);
}

static const TypeInfo andes_plmt_info = {
    .name = TYPE_ANDES_PLMT,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AndesPLMTState),
    .class_init = andes_plmt_class_init,
};

static void andes_plmt_register_types(void)
{
    type_register_static(&andes_plmt_info);
}

type_init(andes_plmt_register_types)

/*
 * Create PLMT device.
 */
DeviceState*
andes_plmt_create(hwaddr addr, hwaddr size, uint32_t num_harts,
    uint32_t time_base, uint32_t timecmp_base, uint32_t timebase_freq,
    uint32_t hart_base)
{
    int i;
    DeviceState *dev = qdev_new(TYPE_ANDES_PLMT);
    AndesPLMTState *plmt = ANDES_PLMT(dev);

    qdev_prop_set_uint32(dev, "num-harts", num_harts);
    qdev_prop_set_uint32(dev, "time-base", time_base);
    qdev_prop_set_uint32(dev, "timecmp-base", timecmp_base);
    qdev_prop_set_uint32(dev, "aperture-size", size);
    qdev_prop_set_uint32(dev, "timebase-freq", timebase_freq);
    qdev_prop_set_uint32(dev, "hart-base", hart_base);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    for (i = 0; i < num_harts; i++) {
        CPUState *cpu = qemu_get_cpu(i);
        RISCVCPU *rvcpu = RISCV_CPU(cpu);
        CPURISCVState *env = cpu_env(cpu);
        if (!env) {
            continue;
        }
        andes_plmt_callback *cb = g_new0(andes_plmt_callback, 1);

        riscv_cpu_set_rdtime_fn(env, andes_cpu_riscv_read_rtc, dev);

        cb->plmt = plmt;
        cb->hartid = i;
        plmt->timers[i] = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                       &andes_plmt_timer_cb, cb);
        plmt->timecmp[i] = 0;

        qdev_connect_gpio_out(dev, i,
                              qdev_get_gpio_in(DEVICE(rvcpu), IRQ_M_TIMER));
    }

    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, addr);
    return dev;
}
