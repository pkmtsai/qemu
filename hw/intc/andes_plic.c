/*
 * Andes PLIC (Platform Level Interrupt Controller)
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
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "target/riscv/cpu.h"
#include "hw/sysbus.h"
#include "hw/intc/andes_plic.h"

/* #define DEBUG_ANDES_PLIC */
#define LOGGE(x...) qemu_log_mask(LOG_GUEST_ERROR, x)
#define xLOG(x...)
#define yLOG(x...) qemu_log(x)
#ifdef DEBUG_ANDES_PLIC
  #define LOG(x...) yLOG(x)
#else
  #define LOG(x...) xLOG(x)
#endif

enum register_names {
    REG_FEATURE_ENABLE = 0x0000,
    REG_TRIGGER_TYPE_BASE = 0x1080,
    REG_NUM_IRQ_TARGET = 0x1100,
    REG_VER_MAX_PRIORITY = 0x1104,
};

enum feature_enable_register {
    FER_PREEMPT = (1u << 0),
    FER_VECTORED = (1u << 1),
};

void andes_plichw_update(void *plic)
{
    AndesPLICState *andes_plic = ANDES_PLIC(plic);
    RISCVPLICState *riscv_plic = RISCV_PLIC(andes_plic);
    int target_id;

    /* raise irq on harts where this irq is enabled */
    for (target_id = 0; target_id < riscv_plic->num_addrs; target_id++) {
        uint32_t hart_id = riscv_plic->addr_config[target_id].hartid;
        PLICMode mode = riscv_plic->addr_config[target_id].mode;
        CPUState *cpu = qemu_get_cpu(hart_id);
        CPURISCVState *env = cpu ? cpu->env_ptr : NULL;
        if (!env) {
            continue;
        }
        int level = riscv_plic_irqs_pending(riscv_plic, target_id);

        switch (mode) {
        case PlicMode_M:
            riscv_cpu_update_mip(RISCV_CPU(cpu), MIP_MEIP, BOOL_TO_MASK(level));
            break;
        case PlicMode_S:
            riscv_cpu_update_mip(RISCV_CPU(cpu), MIP_SEIP, BOOL_TO_MASK(level));
            break;
        default:
            break;
        }
    }
}

void andes_plicsw_update(void *plic)
{
    AndesPLICState *andes_plic = ANDES_PLIC(plic);
    RISCVPLICState *riscv_plic = RISCV_PLIC(andes_plic);
    int target_id;

    /* raise irq on harts where this irq is enabled */
    for (target_id = 0; target_id < riscv_plic->num_addrs; target_id++) {
        uint32_t hart_id = riscv_plic->addr_config[target_id].hartid;
        PLICMode mode = riscv_plic->addr_config[target_id].mode;
        CPUState *cpu = qemu_get_cpu(hart_id);
        CPURISCVState *env = cpu ? cpu->env_ptr : NULL;
        if (!env) {
            continue;
        }
        int level = riscv_plic_irqs_pending(riscv_plic, target_id);

        switch (mode) {
        case PlicMode_M:
            riscv_cpu_update_mip(RISCV_CPU(cpu), MIP_MSIP, BOOL_TO_MASK(level));
            break;
        case PlicMode_S:
            riscv_cpu_update_mip(RISCV_CPU(cpu), MIP_SSIP, BOOL_TO_MASK(level));
            break;
        default:
            break;
        }
    }
}

static void andes_plic_write_pending(void *plic,
    hwaddr addr, uint64_t value, unsigned size)
{
    AndesPLICState *andes_plic = ANDES_PLIC(plic);
    RISCVPLICState *riscv_plic = RISCV_PLIC(andes_plic);

    uint32_t word = (addr - riscv_plic->pending_base) >> 2;
    uint32_t xchg = riscv_plic->pending[word] ^ (uint32_t)value;
    if (xchg) {
        riscv_plic->pending[word] |= value;
        riscv_plic->riscv_plic_update(riscv_plic);
    }

    return;
}

static uint64_t
andes_plic_read(void *opaque, hwaddr addr, unsigned size)
{
    AndesPLICState *andes_plic = ANDES_PLIC(opaque);
    uint64_t value;

    memory_region_dispatch_read(&andes_plic->parent_mmio, addr, &value,
                size_memop(size) | MO_LE, MEMTXATTRS_UNSPECIFIED);

    return value;
}

static void
andes_plic_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    AndesPLICState *andes_plic = ANDES_PLIC(opaque);

    memory_region_dispatch_write(&andes_plic->parent_mmio, addr, value,
            size_memop(size) | MO_LE, MEMTXATTRS_UNSPECIFIED);

    return;
}

static const MemoryRegionOps andes_plic_ops = {
    .read = andes_plic_read,
    .write = andes_plic_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 8
    }
};

static void
andes_plic_realize(DeviceState *dev, Error **errp)
{
    LOG("%s:\n", __func__);
    AndesPLICState *andes_plic = ANDES_PLIC(dev);
    RISCVPLICState *riscv_plic = RISCV_PLIC(dev);
    AndesPLICClass *andes_plic_class = ANDES_PLIC_GET_CLASS(andes_plic);
    Error *local_err = NULL;

    andes_plic_class->parent_realize(dev, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    if (strstr(andes_plic->plic_name , "SW") != NULL) {
        riscv_plic->riscv_plic_update = andes_plicsw_update;
    } else {
        riscv_plic->riscv_plic_update = andes_plichw_update;
    }
    riscv_plic->riscv_plic_write_pending = andes_plic_write_pending;

    andes_plic->parent_mmio = riscv_plic->mmio;
    memory_region_init_io(&riscv_plic->mmio, OBJECT(dev),
                        &andes_plic_ops, andes_plic,
                        TYPE_ANDES_PLIC, riscv_plic->aperture_size);

    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &riscv_plic->mmio);
}

static Property andes_plic_properties[] = {
    DEFINE_PROP_STRING("plic-name", AndesPLICState, plic_name),
    DEFINE_PROP_END_OF_LIST(),
};

static void andes_plic_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    AndesPLICClass *apc = ANDES_PLIC_CLASS(klass);

    device_class_set_props(dc, andes_plic_properties);
    device_class_set_parent_realize(dc, andes_plic_realize,
                                    &apc->parent_realize);
}

static const TypeInfo andes_plic_info = {
    .name          = TYPE_ANDES_PLIC,
    .parent        = TYPE_RISCV_PLIC,
    .instance_size = sizeof(AndesPLICState),
    .class_init    = andes_plic_class_init,
};

static void andes_plic_register_types(void)
{
    LOG("%s:\n", __func__);
    type_register_static(&andes_plic_info);
}

type_init(andes_plic_register_types)

/*
 * Create PLIC device.
 */
DeviceState *andes_plic_create(hwaddr plic_base,
    const char *plic_name, char *hart_config,
    uint32_t num_sources, uint32_t num_priorities,
    uint32_t priority_base, uint32_t pending_base,
    uint32_t enable_base, uint32_t enable_stride,
    uint32_t threshold_base, uint32_t threshold_stride,
    uint32_t aperture_size)
{
    DeviceState *dev = qdev_new(TYPE_ANDES_PLIC);

    assert(enable_stride == (enable_stride & -enable_stride));
    assert(threshold_stride == (threshold_stride & -threshold_stride));
    qdev_prop_set_string(dev, "plic-name", plic_name);
    qdev_prop_set_uint32(dev, "hartid-base", 0);
    qdev_prop_set_string(dev, "hart-config", hart_config);
    qdev_prop_set_uint32(dev, "num-sources", num_sources);
    qdev_prop_set_uint32(dev, "num-priorities", num_priorities);
    qdev_prop_set_uint32(dev, "priority-base", priority_base);
    qdev_prop_set_uint32(dev, "pending-base", pending_base);
    qdev_prop_set_uint32(dev, "enable-base", enable_base);
    qdev_prop_set_uint32(dev, "enable-stride", enable_stride);
    qdev_prop_set_uint32(dev, "context-base", threshold_base);
    qdev_prop_set_uint32(dev, "context-stride", threshold_stride);
    qdev_prop_set_uint32(dev, "aperture-size", aperture_size);

    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, plic_base);
    return dev;
}
