/*
 * SiFive PLIC (Platform Level Interrupt Controller) interface
 *
 * Copyright (c) 2017 SiFive, Inc.
 *
 * This provides a RISC-V PLIC device
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

#ifndef HW_RISCV_PLIC_H
#define HW_RISCV_PLIC_H

#include "hw/sysbus.h"
#include "qom/object.h"

typedef struct RISCVPLICClass {
    /*< private >*/
    SysBusDeviceClass parent_class;
    /*< public >*/
} RISCVPLICClass;

#define TYPE_RISCV_PLIC "riscv.plic"

typedef struct RISCVPLICState RISCVPLICState;
DECLARE_INSTANCE_CHECKER(RISCVPLICState, RISCV_PLIC,
                         TYPE_RISCV_PLIC)

typedef enum PLICMode {
    PLICMode_U,
    PLICMode_S,
    PLICMode_H,
    PLICMode_M
} PLICMode;

typedef struct PLICAddr {
    uint32_t addrid;
    uint32_t hartid;
    PLICMode mode;
} PLICAddr;

struct RISCVPLICState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion mmio;
    uint32_t num_addrs;
    uint32_t num_harts;
    uint32_t bitfield_words;
    uint32_t num_enables;
    PLICAddr *addr_config;
    uint32_t *source_priority;
    uint32_t *target_priority;
    uint32_t *pending;
    uint32_t *claimed;
    uint32_t *enable;

    /* config */
    char *hart_config;
    uint32_t hartid_base;
    uint32_t num_sources;
    uint32_t num_priorities;
    uint32_t priority_base;
    uint32_t pending_base;
    uint32_t enable_base;
    uint32_t enable_stride;
    uint32_t context_base;
    uint32_t context_stride;
    uint32_t aperture_size;

    qemu_irq *m_external_irqs;
    qemu_irq *s_external_irqs;

    /* interface */
    uint64_t (*riscv_plic_read_priority)(void *opaque,
        hwaddr addr, unsigned size);
    uint64_t (*riscv_plic_read_pending)(void *opaque,
        hwaddr addr, unsigned size);
    uint64_t (*riscv_plic_read_enable)(void *opaque,
        hwaddr addr, unsigned size);
    uint64_t (*riscv_plic_read_threshold)(void *opaque,
        hwaddr addr, unsigned size);
    uint64_t (*riscv_plic_read_claim)(void *opaque,
        hwaddr addr, unsigned size);

    void (*riscv_plic_write_priority)(void *opaque,
        hwaddr addr, uint64_t value, unsigned size);
    void (*riscv_plic_write_pending)(void *opaque,
        hwaddr addr, uint64_t value, unsigned size);
    void (*riscv_plic_write_enable)(void *opaque,
        hwaddr addr, uint64_t value, unsigned size);
    void (*riscv_plic_write_threshold)(void *opaque,
        hwaddr addr, uint64_t value, unsigned size);
    void (*riscv_plic_write_complete)(void *opaque,
        hwaddr addr, uint64_t value, unsigned size);

    void (*riscv_plic_update)(void *opaque);
    uint32_t (*riscv_plic_claim)(RISCVPLICState *plic,
        uint32_t addrid);
};

DeviceState *riscv_plic_create(hwaddr addr, char *hart_config,
    uint32_t hartid_base, uint32_t num_sources,
    uint32_t num_priorities, uint32_t priority_base,
    uint32_t pending_base, uint32_t enable_base,
    uint32_t enable_stride, uint32_t context_base,
    uint32_t context_stride, uint32_t aperture_size);

int riscv_plic_irqs_pending(RISCVPLICState *plic, uint32_t addrid);

#endif
