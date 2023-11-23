/*
 * Andes fp16 and bf16 mode switch
 *
 * Copyright (c) 2023 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
/* From fpu_helper.c */
#include "qemu/osdep.h"
#include "cpu.h"
#include "qemu/host-utils.h"
#include "exec/exec-all.h"
#include "exec/helper-proto.h"
#include "fpu/softfloat.h"
#include "internals.h"
#include "andes_fpu_helper.h"

bool check_fp_mode(CPURISCVState *env)
{
    target_ulong umisc_ctl_val;
    RISCVException ret;
    ret = riscv_csrrw(env, CSR_UMISC_CTL, &umisc_ctl_val, 0, 0);
    if (ret != RISCV_EXCP_NONE) {
        return false;
    }
    return (umisc_ctl_val & MASK_UMISC_CTL_FP_MODE);
}

float16 nds_check_nanbox_h(CPURISCVState *env, uint64_t f)
{
    if (check_fp_mode(env)) {
        return check_nanbox_h_bf16(env, f);
    }
    return check_nanbox_h(env, f);
}

#define GEN_NDS_FP_MODE_TYPE1(FP16, BF16, ITYPE, OTYPE) \
OTYPE nds_##FP16(ITYPE a, float_status *status)         \
{                                                       \
    CPURISCVState *env = container_of(status, CPURISCVState, fp_status); \
    if (check_fp_mode(env)) {                              \
        return BF16(a, status);                         \
    }                                                   \
    return FP16(a, status);                             \
}

#define GEN_NDS_FP_MODE_TYPE2(FP16, BF16, ITYPE, ITYPE2, OTYPE) \
OTYPE nds_##FP16(ITYPE a, ITYPE2 b, float_status *status)       \
{                                                               \
    CPURISCVState *env = container_of(status, CPURISCVState, fp_status); \
    if (check_fp_mode(env)) {                                      \
        return BF16(a, b, status);                              \
    }                                                           \
    return FP16(a, b, status);                                  \
}

float16 nds_float16_muladd(float16 a, float16 b, float16 c,
    int z, float_status *status)
{
    CPURISCVState *env = container_of(status, CPURISCVState, fp_status);
    if (check_fp_mode(env)) {
        return bfloat16_muladd(a, b, c, z, status);
    }
    return float16_muladd(a, b, c, z, status);
}

GEN_NDS_FP_MODE_TYPE2(float16_add, bfloat16_add, float16, float16, float16);
GEN_NDS_FP_MODE_TYPE2(float16_sub, bfloat16_sub, float16, float16, float16);
GEN_NDS_FP_MODE_TYPE2(float16_mul, bfloat16_mul, float16, float16, float16);
GEN_NDS_FP_MODE_TYPE2(float16_div, bfloat16_div, float16, float16, float16);
GEN_NDS_FP_MODE_TYPE2(float16_minnum, bfloat16_minnum, float16, float16,
    float16);
GEN_NDS_FP_MODE_TYPE2(float16_minimum_number, bfloat16_minimum_number,
    float16, float16, float16);
GEN_NDS_FP_MODE_TYPE2(float16_min, bfloat16_min, float16, float16, float16);
GEN_NDS_FP_MODE_TYPE2(float16_maxnum, bfloat16_maxnum, float16, float16,
    float16);
GEN_NDS_FP_MODE_TYPE2(float16_maximum_number, bfloat16_maximum_number,
    float16, float16, float16);
GEN_NDS_FP_MODE_TYPE2(float16_max, bfloat16_max, float16, float16, float16);
GEN_NDS_FP_MODE_TYPE1(float16_sqrt, bfloat16_sqrt, float16, float16);
GEN_NDS_FP_MODE_TYPE2(float16_le, bfloat16_le, float16,
    float16, bool);
GEN_NDS_FP_MODE_TYPE2(float16_le_quiet, bfloat16_le_quiet, float16,
    float16, bool);
GEN_NDS_FP_MODE_TYPE2(float16_lt, bfloat16_lt, float16,
    float16, bool);
GEN_NDS_FP_MODE_TYPE2(float16_lt_quiet, bfloat16_lt_quiet, float16,
    float16, bool);
GEN_NDS_FP_MODE_TYPE2(float16_eq_quiet, bfloat16_eq_quiet, float16,
    float16, bool);
/* assume float16 and bfloat type are the same */
GEN_NDS_FP_MODE_TYPE1(float16_round_to_int, bfloat16_round_to_int,
    float16, float16);

GEN_NDS_FP_MODE_TYPE1(float16_to_int32, bfloat16_to_int32, float16, int32_t);
GEN_NDS_FP_MODE_TYPE1(float16_to_uint32, bfloat16_to_uint32, float16, uint32_t);
GEN_NDS_FP_MODE_TYPE1(float16_to_int64, bfloat16_to_int64, float16, int64_t);
GEN_NDS_FP_MODE_TYPE1(float16_to_uint64, bfloat16_to_uint64, float16, uint64_t);
GEN_NDS_FP_MODE_TYPE1(int32_to_float16, int32_to_bfloat16, int32_t, float16);
GEN_NDS_FP_MODE_TYPE1(uint32_to_float16, uint32_to_bfloat16, uint32_t, float16);
GEN_NDS_FP_MODE_TYPE1(int64_to_float16, int64_to_bfloat16, int64_t, float16);
GEN_NDS_FP_MODE_TYPE1(uint64_to_float16, uint64_to_bfloat16, uint64_t, float16);

/* Since fp16 and bf16 below four functions are differnt parameters */
float16 nds_float32_to_float16(float32 a, bool b, float_status *status)
{
    CPURISCVState *env = container_of(status, CPURISCVState, fp_status);
    if (check_fp_mode(env)) {
        /* bf16 function doesn't have ieee option */
        return float32_to_bfloat16(a, status);
    }
    return float32_to_float16(a, b, status);
}
float32 nds_float16_to_float32(float16 a, bool b, float_status *status)
{
    CPURISCVState *env = container_of(status, CPURISCVState, fp_status);
    if (check_fp_mode(env)) {
        /* bf16 function doesn't have ieee option */
        return bfloat16_to_float32(a, status);
    }
    return float16_to_float32(a, b, status);
}
float16 nds_float64_to_float16(float64 a, bool b, float_status *status)
{
    CPURISCVState *env = container_of(status, CPURISCVState, fp_status);
    if (check_fp_mode(env)) {
        /* bf16 function doesn't have ieee option */
        return float64_to_bfloat16(a, status);
    }
    return float64_to_float16(a, b, status);
}
float64 nds_float16_to_float64(float16 a, bool b, float_status *status)
{
    CPURISCVState *env = container_of(status, CPURISCVState, fp_status);
    if (check_fp_mode(env)) {
        /* bf16 function doesn't have ieee option */
        return bfloat16_to_float64(a, status);
    }
    return float16_to_float64(a, b, status);
}

/* From vector_helper.c */
GEN_NDS_FP_MODE_TYPE2(float16_compare_quiet, bfloat16_compare_quiet,
    float16, float16, FloatRelation);
GEN_NDS_FP_MODE_TYPE2(float16_compare, bfloat16_compare,
    float16, float16, FloatRelation);

/* int16 */
GEN_NDS_FP_MODE_TYPE1(float16_to_uint16, bfloat16_to_uint16, float16, uint16_t);
GEN_NDS_FP_MODE_TYPE1(float16_to_int16, bfloat16_to_int16, float16, int16_t);
GEN_NDS_FP_MODE_TYPE1(uint16_to_float16, uint16_to_bfloat16, uint16_t, float16);
GEN_NDS_FP_MODE_TYPE1(int16_to_float16, int16_to_bfloat16, int16_t, float16);
/* int8 */
GEN_NDS_FP_MODE_TYPE1(float16_to_uint8, bfloat16_to_uint8, float16, uint8_t);
GEN_NDS_FP_MODE_TYPE1(float16_to_int8, bfloat16_to_int8, float16, int8_t);
GEN_NDS_FP_MODE_TYPE1(uint8_to_float16, uint8_to_bfloat16, uint8_t, float16);
GEN_NDS_FP_MODE_TYPE1(int8_to_float16, int8_to_bfloat16, int8_t, float16);
/* int4 */
float16 nds_int64_to_float16_scalbn(int64_t a, int b, float_status *status)
{
    CPURISCVState *env = container_of(status, CPURISCVState, fp_status);
    if (check_fp_mode(env)) {
        return int64_to_bfloat16_scalbn(a, b, status);
    }
    return int64_to_float16_scalbn(a, b, status);
}
float16 nds_uint64_to_float16_scalbn(uint64_t a, int b, float_status *status)
{
    CPURISCVState *env = container_of(status, CPURISCVState, fp_status);
    if (check_fp_mode(env)) {
        return uint64_to_bfloat16_scalbn(a, b, status);
    }
    return uint64_to_float16_scalbn(a, b, status);
}
