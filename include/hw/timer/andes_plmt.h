/*
 * Andes PLMT (Platform Level Machine Timer) interface
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

#ifndef HW_ANDES_PLMT_H
#define HW_ANDES_PLMT_H

#include "hw/intc/riscv_aclint.h"

#define TYPE_ANDES_PLMT "riscv.andes.plmt"

#define AndesPLMTState RISCVAclintMTimerState
#define ANDES_PLMT(obj) \
    OBJECT_CHECK(AndesPLMTState, (obj), TYPE_ANDES_PLMT)

DeviceState *
andes_plmt_create(hwaddr addr, hwaddr size, uint32_t num_harts,
    uint32_t time_base, uint32_t timecmp_base, uint32_t timebase_freq);

enum {
    ANDES_PLMT_TIME_BASE = 0,
    ANDES_PLMT_TIMECMP_BASE = 8,
    ANDES_PLMT_MMIO_SIZE = 0x100000,
    ANDES_PLMT_TIMEBASE_FREQ = 0x3938700, /* 60MHZ */
};

#endif
