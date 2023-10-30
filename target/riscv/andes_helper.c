/*
 * RISC-V Andes Extension Helpers for QEMU
 *
 * Copyright (c) 2023 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "qemu/host-utils.h"
#include "exec/exec-all.h"
#include "exec/helper-proto.h"
#include "fpu/softfloat.h"
#include "internals.h"
typedef int (*test_function)(uint8_t a, uint8_t b);
target_ulong helper_andes_v5_bfo_x(target_ulong rd, target_ulong rs1,
                                   target_ulong insn)
{
    int msb, lsb, is_se;
    int lsbp1, msbm1, lsbm1, lenm1;
    uint64_t se;
    uint64_t nxrd = rd; /* for safety sake */

    msb = extract64(insn, 26, 6);
    lsb = extract64(insn, 20, 6);
    is_se = 0x3 == (0x7 & (insn >> 12)); /* BFOS */
    lsbp1 = lsb + 1;
    msbm1 = msb - 1;
    lsbm1 = lsb - 1;

    if (msb == 0) {
        nxrd = deposit64(nxrd, lsb, 1, 1 & rs1);
        if (lsb > 0) {
            nxrd = deposit64(nxrd, 0, lsbm1 + 1, 0);
        }
        if (lsb < 63) {
            se = (is_se && (1 & rs1)) ? -1LL : 0;
            nxrd = deposit64(nxrd, lsbp1, 64 - lsbp1, se);
        }
    } else if (msb < lsb) {
        lenm1 = lsb - msb;
        nxrd = deposit64(nxrd, msb, lenm1 + 1, rs1 >> 0);
        if (lsb < 63) {
            se = (is_se && (1 & (rs1 >> lenm1))) ? -1LL : 0;
            nxrd = deposit64(nxrd, lsbp1, 64 - lsbp1, se);
        }
        nxrd = deposit64(nxrd, 0, msbm1 + 1, 0);
    } else { /* msb >= lsb */
        lenm1 = msb - lsb;
        nxrd = deposit64(nxrd, 0, lenm1 + 1, rs1 >> lsb);
        se = (is_se && (1 & (rs1 >> msb))) ? -1LL : 0;
        nxrd = deposit64(nxrd, lenm1 + 1, 63 - lenm1, se);
    }

    return (target_long)nxrd;
}

static int andes_v5_fb_x_internal(uint8_t *bytes1, uint8_t *bytes2, int size,
                             int is_little_endian, test_function test)
{
    int i, found;

    found = 0;

    if (is_little_endian) {
        for (i = 0; i < size; ++i) {
            if (test(bytes1[i], bytes2[i])) {
                found = i - size;
                break;
            }
        }
    } else { /* is_big_endian */
        for (i = size - 1; i >= 0; --i) {
            if (test(bytes1[i], bytes2[i])) {
                found = i - size;
                break;
            }
        }
    }

    return found;
}

static int andes_v5_test_match(uint8_t a, uint8_t b)
{
    return (a == b);
}

static int andes_v5_test_mismatch(uint8_t a, uint8_t b)
{
    return (a != b);
}

static int andes_v5_test_zero_mismatch(uint8_t a, uint8_t b)
{
    return (a == 0) || (a != b);
}

target_ulong helper_andes_v5_fb_x(target_ulong rs1, target_ulong rs2,
                                  target_ulong op)
{
    target_ulong rd;
    uint8_t *pa, *pb;
    uint size;

    size = sizeof(target_ulong);
    pa = (uint8_t *)&rs1;
    pb = (uint8_t *)&rs2;
    rd = 0;

    switch (op) {
    case 0x10: /* FFB */
        /* Each byte in Rs1 is matched with the value in Rs2[7:0].  */
        memset(pb, *pb, size);
        rd = andes_v5_fb_x_internal(pa, pb, size, 1, andes_v5_test_match);
        break;
    case 0x11: /* FFZMISM */
        rd = andes_v5_fb_x_internal(pa, pb, size, 1,
                                    andes_v5_test_zero_mismatch);
        break;
    case 0x12: /* FFMISM */
        rd = andes_v5_fb_x_internal(pa, pb, size, 1, andes_v5_test_mismatch);
        break;
    case 0x13: /* FLMISM */
        /*
         * tricky!
         *   # reverse endian to find last
         *   # patch result
         */
        rd = andes_v5_fb_x_internal(pa, pb, size, 0, andes_v5_test_mismatch);
        break;
    default:
        /* helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST); */
        break;
    }

    return rd;
}

uint64_t helper_andes_nfcvt_bf16_s(CPURISCVState *env, uint64_t rs2)
{
    float32 frs = check_nanbox_s(env, rs2);
    return nanbox_h(env, float32_to_bfloat16(frs, &env->fp_status));
}

uint64_t helper_andes_nfcvt_s_bf16(CPURISCVState *env, uint64_t rs2)
{
    float16 frs = check_nanbox_h(env, rs2);
    return nanbox_s(env, bfloat16_to_float32(frs, &env->fp_status));
}

