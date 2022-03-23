/*
 * RISCV PLIC (Platform Level Interrupt Controller)
 *
 * Copyright (c) 2017 SiFive, Inc.
 *
 * This provides a parameterizable interrupt controller based on SiFive's PLIC.
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
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/error-report.h"
#include "hw/sysbus.h"
#include "hw/pci/msi.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/intc/riscv_plic.h"
#include "target/riscv/cpu.h"
#include "sysemu/sysemu.h"
#include "migration/vmstate.h"

#define RISCV_DEBUG_PLIC 0

static PLICMode char_to_mode(char c)
{
    switch (c) {
    case 'U': return PLICMode_U;
    case 'S': return PLICMode_S;
    case 'H': return PLICMode_H;
    case 'M': return PLICMode_M;
    default:
        error_report("plic: invalid mode '%c'", c);
        exit(1);
    }
}

static char mode_to_char(PLICMode m)
{
    switch (m) {
    case PLICMode_U: return 'U';
    case PLICMode_S: return 'S';
    case PLICMode_H: return 'H';
    case PLICMode_M: return 'M';
    default: return '?';
    }
}

static void riscv_plic_print_state(RISCVPLICState *plic)
{
    int i;
    int addrid;

    /* pending */
    qemu_log("pending       : ");
    for (i = plic->bitfield_words - 1; i >= 0; i--) {
        qemu_log("%08x", plic->pending[i]);
    }
    qemu_log("\n");

    /* pending */
    qemu_log("claimed       : ");
    for (i = plic->bitfield_words - 1; i >= 0; i--) {
        qemu_log("%08x", plic->claimed[i]);
    }
    qemu_log("\n");

    for (addrid = 0; addrid < plic->num_addrs; addrid++) {
        qemu_log("hart%d-%c enable: ",
            plic->addr_config[addrid].hartid,
            mode_to_char(plic->addr_config[addrid].mode));
        for (i = plic->bitfield_words - 1; i >= 0; i--) {
            qemu_log("%08x", plic->enable[addrid * plic->bitfield_words + i]);
        }
        qemu_log("\n");
    }
}

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

static void riscv_plic_set_pending(RISCVPLICState *plic, int irq, bool level)
{
    atomic_set_masked(&plic->pending[irq >> 5], 1 << (irq & 31), -!!level);
}

static void riscv_plic_set_claimed(RISCVPLICState *plic, int irq, bool level)
{
    atomic_set_masked(&plic->claimed[irq >> 5], 1 << (irq & 31), -!!level);
}

int riscv_plic_irqs_pending(RISCVPLICState *plic, uint32_t addrid)
{
    int i, j;
    for (i = 0; i < plic->bitfield_words; i++) {
        uint32_t pending_enabled_not_claimed =
            (plic->pending[i] & ~plic->claimed[i]) &
            plic->enable[addrid * plic->bitfield_words + i];
        if (!pending_enabled_not_claimed) {
            continue;
        }

        for (j = 0; j < 32; j++) {
            int irq = (i << 5) + j;
            uint32_t prio = plic->source_priority[irq];
            int enabled = pending_enabled_not_claimed & (1 << j);
            if (enabled && prio > plic->target_priority[addrid]) {
                return 1;
            }
        }
    }
    return 0;
}

static void riscv_plic_update(void *opaque)
{
    RISCVPLICState *plic = opaque;
    int addrid;

    /* raise irq on harts where this irq is enabled */
    for (addrid = 0; addrid < plic->num_addrs; addrid++) {
        uint32_t hartid = plic->addr_config[addrid].hartid;
        PLICMode mode = plic->addr_config[addrid].mode;
        CPUState *cpu = qemu_get_cpu(hartid);
        CPURISCVState *env = cpu ? cpu->env_ptr : NULL;
        if (!env) {
            continue;
        }
        int level = riscv_plic_irqs_pending(plic, addrid);
        switch (mode) {
        case PLICMode_M:
            riscv_cpu_update_mip(RISCV_CPU(cpu), MIP_MEIP, BOOL_TO_MASK(level));
            break;
        case PLICMode_S:
            riscv_cpu_update_mip(RISCV_CPU(cpu), MIP_SEIP, BOOL_TO_MASK(level));
            break;
        default:
            break;
        }
    }

    if (RISCV_DEBUG_PLIC) {
        riscv_plic_print_state(plic);
    }
}

static uint32_t riscv_plic_claim(RISCVPLICState *plic, uint32_t addrid)
{
    int i, j;
    uint32_t max_irq = 0;
    uint32_t max_prio = plic->target_priority[addrid];

    for (i = 0; i < plic->bitfield_words; i++) {
        uint32_t pending_enabled_not_claimed =
            (plic->pending[i] & ~plic->claimed[i]) &
            plic->enable[addrid * plic->bitfield_words + i];
        if (!pending_enabled_not_claimed) {
            continue;
        }
        for (j = 0; j < 32; j++) {
            int irq = (i << 5) + j;
            uint32_t prio = plic->source_priority[irq];
            int enabled = pending_enabled_not_claimed & (1 << j);
            if (enabled && prio > max_prio) {
                max_irq = irq;
                max_prio = prio;
            }
        }
    }

    if (max_irq) {
        riscv_plic_set_pending(plic, max_irq, false);
        riscv_plic_set_claimed(plic, max_irq, true);
    }
    return max_irq;
}

static uint64_t riscv_plic_read_priority(void *opaque,
        hwaddr addr, unsigned size)
{
    RISCVPLICState *plic = opaque;
    uint32_t irq = ((addr - plic->priority_base) >> 2) + 1;
    if (RISCV_DEBUG_PLIC) {
        qemu_log("plic: read priority: irq=%d priority=%d\n",
            irq, plic->source_priority[irq]);
    }
    return plic->source_priority[irq];
}

static uint64_t riscv_plic_read_pending(void *opaque,
        hwaddr addr, unsigned size)
{
    RISCVPLICState *plic = opaque;
    uint32_t word = (addr - plic->pending_base) >> 2;
    if (RISCV_DEBUG_PLIC) {
        qemu_log("plic: read pending: word=%d value=%d\n",
            word, plic->pending[word]);
    }
    return plic->pending[word];
}

static uint64_t riscv_plic_read_enable(void *opaque,
        hwaddr addr, unsigned size)
{
    RISCVPLICState *plic = opaque;
    uint32_t addrid = (addr - plic->enable_base) / plic->enable_stride;
    uint32_t wordid = (addr & (plic->enable_stride - 1)) >> 2;
    if (wordid < plic->bitfield_words) {
        if (RISCV_DEBUG_PLIC) {
            qemu_log("plic: read enable: hart%d-%c word=%d value=%x\n",
                plic->addr_config[addrid].hartid,
                mode_to_char(plic->addr_config[addrid].mode), wordid,
                plic->enable[addrid * plic->bitfield_words + wordid]);
        }
        return plic->enable[addrid * plic->bitfield_words + wordid];
    }

    return 0;
}

static uint64_t riscv_plic_read_threshold(void *opaque,
        hwaddr addr, unsigned size)
{
    RISCVPLICState *plic = opaque;
    uint32_t addrid = (addr - plic->context_base) / plic->context_stride;
    if (RISCV_DEBUG_PLIC) {
        qemu_log("plic: read priority: hart%d-%c priority=%x\n",
            plic->addr_config[addrid].hartid,
            mode_to_char(plic->addr_config[addrid].mode),
            plic->target_priority[addrid]);
    }
    return plic->target_priority[addrid];
}

static uint64_t riscv_plic_read_claim(void *opaque,
        hwaddr addr, unsigned size)
{
    RISCVPLICState *plic = opaque;
    uint32_t value = 0;
    uint32_t addrid = (addr - plic->context_base) / plic->context_stride;
    if (plic->riscv_plic_claim) {
        value = plic->riscv_plic_claim(plic, addrid);
    }
    if (RISCV_DEBUG_PLIC) {
        qemu_log("plic: read claim: hart%d-%c irq=%x\n",
            plic->addr_config[addrid].hartid,
            mode_to_char(plic->addr_config[addrid].mode),
            value);
    }
    plic->riscv_plic_update(plic);
    return value;
}

static void riscv_plic_write_priority(void *opaque,
        hwaddr addr, uint64_t value, unsigned size)
{
    RISCVPLICState *plic = opaque;
    uint32_t irq = ((addr - plic->priority_base) >> 2) + 1;
    plic->source_priority[irq] = value & 7;
    if (RISCV_DEBUG_PLIC) {
        qemu_log("plic: write priority: irq=%d priority=%d\n",
            irq, plic->source_priority[irq]);
    }
    plic->riscv_plic_update(plic);
    return;
}

static void riscv_plic_write_pending(void *opaque,
        hwaddr addr, uint64_t value, unsigned size)
{
    qemu_log_mask(LOG_GUEST_ERROR,
        "%s: invalid pending write: 0x%" HWADDR_PRIx "",
        __func__, addr);
    return;

}

static void riscv_plic_write_enable(void *opaque,
        hwaddr addr, uint64_t value, unsigned size)
{
    RISCVPLICState *plic = opaque;
    uint32_t addrid = (addr - plic->enable_base) / plic->enable_stride;
    uint32_t wordid = (addr & (plic->enable_stride - 1)) >> 2;
    if (wordid < plic->bitfield_words) {
        plic->enable[addrid * plic->bitfield_words + wordid] = value;
        if (RISCV_DEBUG_PLIC) {
            qemu_log("plic: write enable: hart%d-%c word=%d value=%x\n",
                plic->addr_config[addrid].hartid,
                mode_to_char(plic->addr_config[addrid].mode), wordid,
                plic->enable[addrid * plic->bitfield_words + wordid]);
        }
    }
    return;
}

static void riscv_plic_write_threshold(void *opaque,
        hwaddr addr, uint64_t value, unsigned size)
{
    RISCVPLICState *plic = opaque;
    uint32_t addrid = (addr - plic->context_base) / plic->context_stride;
    if (RISCV_DEBUG_PLIC) {
        qemu_log("plic: write priority: hart%d-%c priority=%x\n",
            plic->addr_config[addrid].hartid,
            mode_to_char(plic->addr_config[addrid].mode),
            plic->target_priority[addrid]);
    }
    if (value <= plic->num_priorities) {
        plic->target_priority[addrid] = value;
        plic->riscv_plic_update(plic);
    }

    return;
}

static void riscv_plic_write_complete(void *opaque,
        hwaddr addr, uint64_t value, unsigned size)
{
    RISCVPLICState *plic = opaque;
    uint32_t addrid = (addr - plic->context_base) / plic->context_stride;
    if (RISCV_DEBUG_PLIC) {
        qemu_log("plic: write complete: hart%d-%c irq=%x\n",
            plic->addr_config[addrid].hartid,
            mode_to_char(plic->addr_config[addrid].mode),
            (uint32_t)value);
    }
    if (value < plic->num_sources) {
        riscv_plic_set_claimed(plic, value, false);
        plic->riscv_plic_update(plic);
    }
    return;
}

static uint64_t riscv_plic_read(void *opaque, hwaddr addr, unsigned size)
{
    RISCVPLICState *plic = opaque;

    /* writes must be 4 byte words */
    if ((addr & 0x3) != 0) {
        goto err;
    }

    if (addr >= plic->priority_base && /* 4 bytes per source */
        addr < plic->priority_base + (plic->num_sources << 2))
    {
        if (plic->riscv_plic_read_priority) {
            return plic->riscv_plic_read_priority(plic, addr, size);
        }
    } else if (addr >= plic->pending_base && /* 1 bit per source */
               addr < plic->pending_base + (plic->num_sources >> 3))
    {
        if (plic->riscv_plic_read_pending) {
            return plic->riscv_plic_read_pending(plic, addr, size);
        }
    } else if (addr >= plic->enable_base && /* 1 bit per source */
             addr < plic->enable_base + plic->num_addrs * plic->enable_stride)
    {
        if (plic->riscv_plic_read_enable) {
            return plic->riscv_plic_read_enable(plic, addr, size);
        }
    } else if (addr >= plic->context_base && /* 1 bit per source */
             addr < plic->context_base + plic->num_addrs * plic->context_stride)
    {
        uint32_t contextid = (addr & (plic->context_stride - 1));
        if (contextid == 0) {
            if (plic->riscv_plic_read_threshold) {
                return plic->riscv_plic_read_threshold(plic, addr, size);
            }
        } else if (contextid == 4) {
            if (plic->riscv_plic_read_claim) {
                return plic->riscv_plic_read_claim(plic, addr, size);
            }
        }
    }

err:
    qemu_log_mask(LOG_GUEST_ERROR,
                  "%s: Invalid register read 0x%" HWADDR_PRIx "\n",
                  __func__, addr);
    return 0;
}

static void riscv_plic_write(void *opaque, hwaddr addr, uint64_t value,
        unsigned size)
{
    RISCVPLICState *plic = opaque;

    /* writes must be 4 byte words */
    if ((addr & 0x3) != 0) {
        goto err;
    }

    if (addr >= plic->priority_base && /* 4 bytes per source */
        addr < plic->priority_base + (plic->num_sources << 2))
    {
        if (plic->riscv_plic_write_priority) {
            return plic->riscv_plic_write_priority(plic, addr, value, size);
        }
    } else if (addr >= plic->pending_base && /* 1 bit per source */
               addr < plic->pending_base + (plic->num_sources >> 3))
    {
        if (plic->riscv_plic_write_pending) {
            return plic->riscv_plic_write_pending(plic, addr, value, size);
        }
    } else if (addr >= plic->enable_base && /* 1 bit per source */
        addr < plic->enable_base + plic->num_addrs * plic->enable_stride)
    {
        if (plic->riscv_plic_write_enable) {
            return plic->riscv_plic_write_enable(plic, addr, value, size);
        }
    } else if (addr >= plic->context_base && /* 4 bytes per reg */
        addr < plic->context_base + plic->num_addrs * plic->context_stride)
    {
        uint32_t contextid = (addr & (plic->context_stride - 1));
        if (contextid == 0) {
            if (plic->riscv_plic_write_threshold) {
                return plic->riscv_plic_write_threshold(plic, addr, value, size);
            }
        } else if (contextid == 4) {
            if (plic->riscv_plic_write_complete) {
                return plic->riscv_plic_write_complete(plic, addr, value, size);
            }
        }
    }

err:
    qemu_log_mask(LOG_GUEST_ERROR,
                  "%s: Invalid register write 0x%" HWADDR_PRIx "\n",
                  __func__, addr);
}

static const MemoryRegionOps riscv_plic_ops = {
    .read = riscv_plic_read,
    .write = riscv_plic_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 8
    }
};

static Property riscv_plic_properties[] = {
    DEFINE_PROP_STRING("hart-config", RISCVPLICState, hart_config),
    DEFINE_PROP_UINT32("hartid-base", RISCVPLICState, hartid_base, 0),
    DEFINE_PROP_UINT32("num-sources", RISCVPLICState, num_sources, 0),
    DEFINE_PROP_UINT32("num-priorities", RISCVPLICState, num_priorities, 0),
    DEFINE_PROP_UINT32("priority-base", RISCVPLICState, priority_base, 0),
    DEFINE_PROP_UINT32("pending-base", RISCVPLICState, pending_base, 0),
    DEFINE_PROP_UINT32("enable-base", RISCVPLICState, enable_base, 0),
    DEFINE_PROP_UINT32("enable-stride", RISCVPLICState, enable_stride, 0),
    DEFINE_PROP_UINT32("context-base", RISCVPLICState, context_base, 0),
    DEFINE_PROP_UINT32("context-stride", RISCVPLICState, context_stride, 0),
    DEFINE_PROP_UINT32("aperture-size", RISCVPLICState, aperture_size, 0),
    DEFINE_PROP_END_OF_LIST(),
};

/*
 * parse PLIC hart/mode address offset config
 *
 * "M"              1 hart with M mode
 * "MS,MS"          2 harts, 0-1 with M and S mode
 * "M,MS,MS,MS,MS"  5 harts, 0 with M mode, 1-5 with M and S mode
 */
static void parse_hart_config(RISCVPLICState *plic)
{
    int addrid, hartid, modes;
    const char *p;
    char c;

    /* count and validate hart/mode combinations */
    addrid = 0, hartid = 0, modes = 0;
    p = plic->hart_config;
    while ((c = *p++)) {
        if (c == ',') {
            addrid += ctpop8(modes);
            modes = 0;
            hartid++;
        } else {
            int m = 1 << char_to_mode(c);
            if (modes == (modes | m)) {
                error_report("plic: duplicate mode '%c' in config: %s",
                             c, plic->hart_config);
                exit(1);
            }
            modes |= m;
        }
    }
    if (modes) {
        addrid += ctpop8(modes);
    }
    hartid++;

    plic->num_addrs = addrid;
    plic->num_harts = hartid;

    /* store hart/mode combinations */
    plic->addr_config = g_new(PLICAddr, plic->num_addrs);
    addrid = 0, hartid = plic->hartid_base;
    p = plic->hart_config;
    while ((c = *p++)) {
        if (c == ',') {
            hartid++;
        } else {
            plic->addr_config[addrid].addrid = addrid;
            plic->addr_config[addrid].hartid = hartid;
            plic->addr_config[addrid].mode = char_to_mode(c);
            addrid++;
        }
    }
}

static void riscv_plic_irq_request(void *opaque, int irq, int level)
{
    RISCVPLICState *plic = opaque;

    riscv_plic_set_pending(plic, irq, level > 0);
    plic->riscv_plic_update(plic);
}

static void riscv_plic_realize(DeviceState *dev, Error **errp)
{
    RISCVPLICState *plic = RISCV_PLIC(dev);
    /* int i; */

    memory_region_init_io(&plic->mmio, OBJECT(dev), &riscv_plic_ops, plic,
                          TYPE_RISCV_PLIC, plic->aperture_size);
    parse_hart_config(plic);
    plic->bitfield_words = (plic->num_sources + 31) >> 5;
    plic->num_enables = plic->bitfield_words * plic->num_addrs;
    plic->source_priority = g_new0(uint32_t, plic->num_sources);
    plic->target_priority = g_new0(uint32_t, plic->num_addrs);
    plic->pending = g_new0(uint32_t, plic->bitfield_words);
    plic->claimed = g_new0(uint32_t, plic->bitfield_words);
    plic->enable = g_new0(uint32_t, plic->num_enables);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &plic->mmio);
    qdev_init_gpio_in(dev, riscv_plic_irq_request, plic->num_sources);

    /* set default read function */
    plic->riscv_plic_read_priority = riscv_plic_read_priority;
    plic->riscv_plic_read_pending = riscv_plic_read_pending;
    plic->riscv_plic_read_enable = riscv_plic_read_enable;
    plic->riscv_plic_read_threshold = riscv_plic_read_threshold;
    plic->riscv_plic_read_claim = riscv_plic_read_claim;
    plic->riscv_plic_claim = riscv_plic_claim;

    /* set default write function */
    plic->riscv_plic_write_priority = riscv_plic_write_priority;
    plic->riscv_plic_write_pending = riscv_plic_write_pending;
    plic->riscv_plic_write_enable = riscv_plic_write_enable;
    plic->riscv_plic_write_threshold = riscv_plic_write_threshold;
    plic->riscv_plic_write_complete = riscv_plic_write_complete;

    /* set default update function */
    plic->riscv_plic_update = riscv_plic_update;

    /*
     * We can't allow the supervisor to control SEIP as this would allow the
     * supervisor to clear a pending external interrupt which will result in
     * lost a interrupt in the case a PLIC is attached. The SEIP bit must be
     * hardware controlled when a PLIC is attached.
     *
     * for (i = 0; i < plic->num_harts; i++) {
     *   RISCVCPU *cpu = RISCV_CPU(qemu_get_cpu(plic->hartid_base + i));
     *   if (riscv_cpu_claim_interrupts(cpu, MIP_SEIP) < 0) {
     *       error_report("SEIP already claimed");
     *       exit(1);
     *   }
     * }
     */

    msi_nonbroken = true;
}

static const VMStateDescription vmstate_riscv_plic = {
    .name = "riscv_plic",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
            VMSTATE_VARRAY_UINT32(source_priority, RISCVPLICState,
                                  num_sources, 0,
                                  vmstate_info_uint32, uint32_t),
            VMSTATE_VARRAY_UINT32(target_priority, RISCVPLICState,
                                  num_addrs, 0,
                                  vmstate_info_uint32, uint32_t),
            VMSTATE_VARRAY_UINT32(pending, RISCVPLICState, bitfield_words, 0,
                                  vmstate_info_uint32, uint32_t),
            VMSTATE_VARRAY_UINT32(claimed, RISCVPLICState, bitfield_words, 0,
                                  vmstate_info_uint32, uint32_t),
            VMSTATE_VARRAY_UINT32(enable, RISCVPLICState, num_enables, 0,
                                  vmstate_info_uint32, uint32_t),
            VMSTATE_END_OF_LIST()
        }
};

static void riscv_plic_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, riscv_plic_properties);
    dc->realize = riscv_plic_realize;
    dc->vmsd = &vmstate_riscv_plic;
}

static const TypeInfo riscv_plic_info = {
    .name          = TYPE_RISCV_PLIC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(RISCVPLICState),
    .class_init    = riscv_plic_class_init,
};

static void riscv_plic_register_types(void)
{
    type_register_static(&riscv_plic_info);
}

type_init(riscv_plic_register_types)

/*
 * Create PLIC device.
 */
DeviceState *riscv_plic_create(hwaddr addr, char *hart_config,
    uint32_t hartid_base, uint32_t num_sources,
    uint32_t num_priorities, uint32_t priority_base,
    uint32_t pending_base, uint32_t enable_base,
    uint32_t enable_stride, uint32_t context_base,
    uint32_t context_stride, uint32_t aperture_size)
{
    DeviceState *dev = qdev_new(TYPE_RISCV_PLIC);
    assert(enable_stride == (enable_stride & -enable_stride));
    assert(context_stride == (context_stride & -context_stride));
    qdev_prop_set_string(dev, "hart-config", hart_config);
    qdev_prop_set_uint32(dev, "hartid-base", hartid_base);
    qdev_prop_set_uint32(dev, "num-sources", num_sources);
    qdev_prop_set_uint32(dev, "num-priorities", num_priorities);
    qdev_prop_set_uint32(dev, "priority-base", priority_base);
    qdev_prop_set_uint32(dev, "pending-base", pending_base);
    qdev_prop_set_uint32(dev, "enable-base", enable_base);
    qdev_prop_set_uint32(dev, "enable-stride", enable_stride);
    qdev_prop_set_uint32(dev, "context-base", context_base);
    qdev_prop_set_uint32(dev, "context-stride", context_stride);
    qdev_prop_set_uint32(dev, "aperture-size", aperture_size);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, addr);
    return dev;
}
