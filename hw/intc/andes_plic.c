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
#include "hw/irq.h"

/* #define DEBUG_ANDES_PLIC */
#define LOGGE(x...) qemu_log_mask(LOG_GUEST_ERROR, x)
#define xLOG(x...)
#define yLOG(x...) qemu_log(x)
#ifdef DEBUG_ANDES_PLIC
  #define LOG(x...) yLOG(x)
#else
  #define LOG(x...) xLOG(x)
#endif
#define ANDES_PLIC_TRIGGER_TYPE_READONLY 0

static uint32_t atomic_set_masked(uint32_t *a, uint32_t mask, uint32_t value)
{
    uint32_t old, new, cmp = qatomic_read(a);

    do {
        old = cmp;
        new = (old & ~mask) | (value & mask);
        cmp = qatomic_cmpxchg(a, old, new);
    } while (old != cmp);

    return old;
}

static void andes_plic_set_pending(AndesPLICState *plic, int irq, bool level)
{
    RISCVPLICState *riscv_plic = RISCV_PLIC(plic);
    atomic_set_masked(&riscv_plic->pending[irq >> 5],
                      1 << (irq & 31), -!!level);
}

static void andes_plic_set_claimed(AndesPLICState *plic, int irq, bool level)
{
    RISCVPLICState *riscv_plic = RISCV_PLIC(plic);
    atomic_set_masked(&riscv_plic->claimed[irq >> 5],
                      1 << (irq & 31), -!!level);
}

static void andes_plic_set_gw_state(AndesPLICState *plic, int irq, bool level)
{
    atomic_set_masked(&plic->gw_state[irq >> 5], 1 << (irq & 31), -!!level);
}

void andes_plichw_update(void *plic)
{
    AndesPLICState *andes_plic = ANDES_PLIC(plic);
    RISCVPLICState *riscv_plic = RISCV_PLIC(andes_plic);
    int target_id;

    /* raise irq on harts where this irq is enabled */
    for (target_id = 0; target_id < riscv_plic->num_addrs; target_id++) {
        uint32_t hartid = riscv_plic->addr_config[target_id].hartid;
        PLICMode mode = riscv_plic->addr_config[target_id].mode;
        CPUState *cpu = qemu_get_cpu(hartid);
        CPURISCVState *env = cpu_env(cpu);
        if (!env) {
            continue;
        }
        int level = riscv_plic_irqs_pending(riscv_plic, target_id);

        AndesCsr *csr = &env->andes_csr;
        AndesVec *vec = &env->andes_vec;
        switch (mode) {
        case PlicMode_M:
            if (!vec->vectored_irq_m &&
                (csr->csrno[CSR_MMISC_CTL] & (1UL << V5_MMISC_CTL_VEC_PLIC)) &&
                (andes_plic->feature_enable & FER_VECTORED) && level) {
                    vec->vectored_irq_m =
                        riscv_plic->riscv_plic_claim(riscv_plic, target_id);
                    assert(vec->vectored_irq_m);
            }
            qemu_set_irq(riscv_plic->m_external_irqs[hartid -
                         riscv_plic->hartid_base], level);
            break;
        case PlicMode_S:
            if (!vec->vectored_irq_s &&
                (csr->csrno[CSR_MMISC_CTL] & (1UL << V5_MMISC_CTL_VEC_PLIC)) &&
                (andes_plic->feature_enable & FER_VECTORED) && level) {
                    vec->vectored_irq_s =
                        riscv_plic->riscv_plic_claim(riscv_plic, target_id);
                    assert(vec->vectored_irq_s);
            }
            qemu_set_irq(riscv_plic->s_external_irqs[hartid -
                         riscv_plic->hartid_base], level);
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
        uint32_t hartid = riscv_plic->addr_config[target_id].hartid;
        PLICMode mode = riscv_plic->addr_config[target_id].mode;
        CPUState *cpu = qemu_get_cpu(hartid);
        CPURISCVState *env = cpu_env(cpu);
        if (!env) {
            continue;
        }
        int level = riscv_plic_irqs_pending(riscv_plic, target_id);

        switch (mode) {
        case PlicMode_M:
            qemu_set_irq(riscv_plic->m_external_irqs[hartid -
                         riscv_plic->hartid_base], level);
            break;
        case PlicMode_S:
            qemu_set_irq(riscv_plic->s_external_irqs[hartid -
                         riscv_plic->hartid_base], level);
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
}

static bool andes_plic_check_enabled_by_addrid(AndesPLICState *plic,
                                              uint32_t addrid)
{
    RISCVPLICState *riscv_plic = RISCV_PLIC(plic);
    /*
     * If target M (via addrid) has one of source N enabled,
     * then we assume it is enabled
     */
    for (int i = 0; i < riscv_plic->bitfield_words; i++) {
        if (riscv_plic->enable[addrid * riscv_plic->bitfield_words + i]) {
            return true;
        }
    }
    return false;
}

static void andes_plic_write_complete(void *opaque,
        hwaddr addr, uint64_t value, unsigned size)
{
    AndesPLICState *andes_plic = ANDES_PLIC(opaque);
    RISCVPLICState *riscv_plic = RISCV_PLIC(andes_plic);
    uint32_t addrid =
        (addr - riscv_plic->context_base) / riscv_plic->context_stride;
    LOG("andes_plic: write complete: hart%d-%d irq=%x\n",
        riscv_plic->addr_config[addrid].hartid,
        riscv_plic->addr_config[addrid].mode, (uint32_t)value);
    if (value < riscv_plic->num_sources) {
        /*
         * Mark level triggered interrupts as pending if they are still raised
         */
        if ((!!(andes_plic->trigger_type[value >> 5] & (1 << (value & 31)))) ==
            ANDES_PLIC_TRIGGER_TYPE_LEVEL && andes_plic->level[value] &&
            andes_plic_check_enabled_by_addrid(andes_plic, addrid)) {
            andes_plic_set_pending(andes_plic, value, true);
        }
        andes_plic_set_claimed(andes_plic, value, false);
        /* Reset Interrupt Gateway State to non in-process */
        andes_plic_set_gw_state(andes_plic, value, false);
        riscv_plic->riscv_plic_update(riscv_plic);
    }
}

static uint64_t andes_plic_read_trigger_type(void *opaque,
        hwaddr addr, unsigned size)
{
    AndesPLICState *andes_plic = ANDES_PLIC(opaque);
    uint32_t word = (addr - REG_TRIGGER_TYPE_BASE) >> 2;
    LOG("andes_plic: read trigger_type: word=%d value=%d\n",
        word, andes_plic->trigger_type[word]);
    return andes_plic->trigger_type[word];
}

static void andes_plic_write_trigger_type(void *opaque,
        hwaddr addr, uint64_t value, unsigned size)
{
#if !ANDES_PLIC_TRIGGER_TYPE_READONLY
    AndesPLICState *andes_plic = ANDES_PLIC(opaque);
    uint32_t word = (addr - REG_TRIGGER_TYPE_BASE) >> 2;
    andes_plic->trigger_type[word] = value;
    LOG("andes_plic: write trigger_type: word=%d value=%d\n",
        word, andes_plic->trigger_type[word]);
#else
    qemu_log_mask(LOG_GUEST_ERROR,
        "%s: invalid trigger type write: 0x%" HWADDR_PRIx "",
        __func__, addr);
#endif

}

static uint64_t
andes_plic_read(void *opaque, hwaddr addr, unsigned size)
{
    AndesPLICState *andes_plic = ANDES_PLIC(opaque);
    RISCVPLICState *riscv_plic = RISCV_PLIC(andes_plic);
    uint64_t value;

    /* read must be 4 byte words */
    if ((addr & 0x3) != 0) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Invalid register read 0x%" HWADDR_PRIx "\n",
                      __func__, addr);
        return 0;
    }

    if (addr == REG_FEATURE_ENABLE) {
        value = andes_plic->feature_enable;
        return value;
    } else if (addr == REG_NUM_IRQ_TARGET) {
        return andes_plic->num_irq_target;
    } else if (addr_between(addr, REG_TRIGGER_TYPE_BASE,
                riscv_plic->num_sources >> 3)) { /* 1 bit per source */
        return andes_plic_read_trigger_type(andes_plic, addr, size);
    }

    memory_region_dispatch_read(&andes_plic->parent_mmio, addr, &value,
                size_memop(size) | MO_LE, MEMTXATTRS_UNSPECIFIED);

    return value;
}

static void
andes_plic_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    AndesPLICState *andes_plic = ANDES_PLIC(opaque);
    RISCVPLICState *riscv_plic = RISCV_PLIC(andes_plic);

    /* write must be 4 byte words */
    if ((addr & 0x3) != 0) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Invalid register write 0x%" HWADDR_PRIx "\n",
                      __func__, addr);
        return;
    }

    if (addr == REG_FEATURE_ENABLE) {
        andes_plic->feature_enable = value & (FER_PREEMPT | FER_VECTORED);
        return;
    } else if (addr == REG_NUM_IRQ_TARGET) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Invalid register write at 0x%" HWADDR_PRIx "\n",
                      __func__, addr);
        return;
    } else if (addr_between(addr, REG_TRIGGER_TYPE_BASE,
                riscv_plic->num_sources >> 3)) { /* 1 bit per source */
        andes_plic_write_trigger_type(andes_plic, addr, value, size);
        return;
    }

    memory_region_dispatch_write(&andes_plic->parent_mmio, addr, value,
            size_memop(size) | MO_LE, MEMTXATTRS_UNSPECIFIED);
}

static void andes_plic_irq_request(void *opaque, int irq, int level)
{
    AndesPLICState *andes_plic = ANDES_PLIC(opaque);
    RISCVPLICState *riscv_plic = RISCV_PLIC(andes_plic);

    uint32_t gw_state =
        qatomic_read(&andes_plic->gw_state[irq >> 5]) & 1 << (irq & 31);
    /*
     * Keep level data for level triggered to re-generate IRQ
     * while receives complete message
     * Regardless of whether gw_state is in processing, we should update
     * level value because we are receiving new request
     */
    andes_plic->level[irq] = level;

    /* Interrupt Gateway State in-processing */
    if (gw_state) {
        /*
         * Interrupt Gateway State in-processing,
         * If we receive upper level again, re-scan pending/claim
         * to avoid interrupt missing for handling
         */
        if (level) {
            riscv_plic->riscv_plic_update(riscv_plic);
        }
        return;
    }

    /* If level is not in low state, set gateway state to in-processing */
    if (level) {
        andes_plic_set_gw_state(andes_plic, irq, true);
    }

    andes_plic_set_pending(andes_plic, irq, level > 0);
    riscv_plic->riscv_plic_update(riscv_plic);
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
    /* Use uint32 to record level for each source irq */
    andes_plic->level = g_new0(uint32_t, riscv_plic->num_sources);
    /* Use uint32 to record gw_state for each source irq */
    andes_plic->gw_state = g_new0(uint32_t, riscv_plic->num_sources);

    /* Allocate trigger type register space */
    andes_plic->trigger_type = g_new0(uint32_t, riscv_plic->bitfield_words);

    if (strstr(andes_plic->plic_name , "SW") != NULL) {
        riscv_plic->riscv_plic_update = andes_plicsw_update;
    } else {
        riscv_plic->riscv_plic_update = andes_plichw_update;
    }
    /*
     * Let Andes PLIC and SWPLIC to disable IO re-entrant checking,
     * since both PLIC and SWPLIC are using riscv.plic type
     * see softmmu/memory.c:access_with_adjusted_size()
     */
    riscv_plic->mmio.disable_reentrancy_guard = true;
    riscv_plic->riscv_plic_write_pending = andes_plic_write_pending;
    riscv_plic->riscv_plic_write_complete = andes_plic_write_complete;
    /* register andes irq request function to process irq request */
    riscv_plic->riscv_plic_irq_request = andes_plic_irq_request;

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

static void andes_plic_reset(DeviceState *dev)
{
    AndesPLICState *andes_plic = ANDES_PLIC(dev);
    RISCVPLICState *riscv_plic = RISCV_PLIC(andes_plic);
    AndesPLICClass *andes_plic_class = ANDES_PLIC_GET_CLASS(andes_plic);

    andes_plic_class->parent_reset(dev);
    memset(andes_plic->level, 0, sizeof(uint32_t) *
           riscv_plic->bitfield_words);
    memset(andes_plic->gw_state, 0, sizeof(uint32_t) *
           riscv_plic->bitfield_words);
    /* No reset trigger type */
}

static void andes_plic_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    AndesPLICClass *apc = ANDES_PLIC_CLASS(klass);

    device_class_set_props(dc, andes_plic_properties);
    device_class_set_parent_realize(dc, andes_plic_realize,
                                    &apc->parent_realize);
    device_class_set_parent_reset(dc, andes_plic_reset,
                                    &apc->parent_reset);
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
    uint32_t num_harts, uint32_t hartid_base,
    uint32_t num_sources, uint32_t num_priorities,
    uint32_t priority_base, uint32_t pending_base,
    uint32_t enable_base, uint32_t enable_stride,
    uint32_t threshold_base, uint32_t threshold_stride,
    uint32_t aperture_size)
{
    DeviceState *dev = qdev_new(TYPE_ANDES_PLIC);
    uint32_t sw = 0;

    assert(enable_stride == (enable_stride & -enable_stride));
    assert(threshold_stride == (threshold_stride & -threshold_stride));
    qdev_prop_set_string(dev, "plic-name", plic_name);
    qdev_prop_set_uint32(dev, "hartid-base", hartid_base);
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

    if (strstr(plic_name, "SW") != NULL) {
        sw = 1;
    }

    AndesPLICState *andes_plic = ANDES_PLIC(dev);
    RISCVPLICState *riscv_plic = RISCV_PLIC(andes_plic);

    andes_plic->num_irq_target = (riscv_plic->num_addrs << 16) | num_sources;

    for (int i = 0; i < riscv_plic->num_addrs; i++) {
        int cpu_num = riscv_plic->addr_config[i].hartid;
        CPUState *cpu = qemu_get_cpu(cpu_num);

        if (riscv_plic->addr_config[i].mode == PLICMode_M) {
            qdev_connect_gpio_out(dev, cpu_num - hartid_base + num_harts,
                                  qdev_get_gpio_in(DEVICE(cpu),
                                  sw ? IRQ_M_SOFT : IRQ_M_EXT));
        }
        if (riscv_plic->addr_config[i].mode == PLICMode_S) {
            qdev_connect_gpio_out(dev, cpu_num - hartid_base,
                                  qdev_get_gpio_in(DEVICE(cpu),
                                  sw ? IRQ_S_SOFT : IRQ_S_EXT));
        }
    }

    return dev;
}
