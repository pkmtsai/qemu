/*
 * Andes ATCSMU (System Management Unit)
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

#ifndef HW_ANDES_ATCSMU_H
#define HW_ANDES_ATCSMU_H

#define TYPE_ANDES_ATCSMU "riscv.andes.ae350.atcsmu"

#define ANDES_ATCSMU(obj) \
    OBJECT_CHECK(AndesATCSMUState, (obj), TYPE_ANDES_ATCSMU)

typedef struct AndesATCSMUState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion mmio;

    /* config */
    uint32_t smu_base_addr;
    uint32_t smu_base_size;
} AndesATCSMUState;

#define ATCSMU_SMUCR        0x14

/* SMUCR */
#define SMUCMD_RESET        0x3c
#define SMUCMD_POWEROFF     0x5a
#define SMUCMD_STANDBY      0x55

DeviceState *
andes_atcsmu_create(hwaddr addr, hwaddr size);

#endif
