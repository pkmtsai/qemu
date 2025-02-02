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

typedef struct pcs_registers {
    uint32_t pcs_scratch;
} pcs_registers;

typedef struct AndesATCSMUState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion mmio;
    uint32_t wrsr;
    /* config */
    uint32_t smu_base_addr;
    uint32_t smu_base_size;
    uint32_t systemver;
    uint32_t boardver;
    uint32_t systemcfg;
    uint32_t smuver;
    uint32_t scratch;
    pcs_registers pcs_regs[8];
} AndesATCSMUState;

#define ATCSMU_SYSTEMVER    0x00
#define ATCSMU_BOARDVER     0x04
#define ATCSMU_SYSTEMCFG    0x08
#define ATCSMU_SMUVER       0x0C
#define ATCSMU_WRSR         0x10
#define ATCSMU_SMUCR        0x14
#define ATCSMU_SCRATCH      0x40
#define ATCSMU_HART0_RESET_VECTOR_LO 0x50
#define ATCSMU_HART1_RESET_VECTOR_LO 0x54
#define ATCSMU_HART2_RESET_VECTOR_LO 0x58
#define ATCSMU_HART3_RESET_VECTOR_LO 0x5C
#define ATCSMU_HART0_RESET_VECTOR_HI 0x60
#define ATCSMU_HART1_RESET_VECTOR_HI 0x64
#define ATCSMU_HART2_RESET_VECTOR_HI 0x68
#define ATCSMU_HART3_RESET_VECTOR_HI 0x6C

#define ATCSMU_PCS3_SCRATCH          0x84
#define ATCSMU_PCS4_SCRATCH          0xA4
#define ATCSMU_PCS5_SCRATCH          0xC4
#define ATCSMU_PCS6_SCRATCH          0xE4
#define ATCSMU_PCS7_SCRATCH          0x104
#define ATCSMU_PCS8_SCRATCH          0x124
#define ATCSMU_PCS9_SCRATCH          0x144
#define ATCSMU_PCS10_SCRATCH         0x164

#define ATCSMU_HART4_RESET_VECTOR_LO 0x200
#define ATCSMU_HART5_RESET_VECTOR_LO 0x204
#define ATCSMU_HART6_RESET_VECTOR_LO 0x208
#define ATCSMU_HART7_RESET_VECTOR_LO 0x20C
#define ATCSMU_HART4_RESET_VECTOR_HI 0x210
#define ATCSMU_HART5_RESET_VECTOR_HI 0x214
#define ATCSMU_HART6_RESET_VECTOR_HI 0x218
#define ATCSMU_HART7_RESET_VECTOR_HI 0x21C

/* SYSTEMVER */
#define SYSTEMVER_MINOR     0x0
#define SYSTEMVER_MAJOR     0x0
#define SYSTEMVER_ID        0x414535

/* BOARDVER */
#define BOARDVER_MINOR      0x0
#define BOARDVER_MAJOR      0x1
#define BOARDVER_ID         0x0174b0

/* SMUVER */
#define SMUVER_LEGACY       0x0000
#define SMUVER_SAMPLE       0x0100

/* SMUCR */
#define SMUCMD_RESET        0x3c
#define SMUCMD_POWEROFF     0x5a
#define SMUCMD_STANDBY      0x55

void
andes_atcsmu_create(AndesATCSMUState *dev, hwaddr addr, hwaddr size,
                    int num_harts);

#endif
