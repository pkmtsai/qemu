/*
 * Andes fp16 and bf16 mode switch
 *
 * Copyright (c) 2023 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
/* From fpu_helper.c */
#ifndef __ANDES_FPU_HELPER__
#define __ANDES_FPU_HELPER__
#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "fpu/softfloat.h"

int check_fp_mode(void);
float16 nds_check_nanbox_h(CPURISCVState *env, uint64_t f);

float16 nds_float16_muladd(float16, float16, float16, int,
    float_status *status);
float16 nds_float16_add(float16, float16, float_status *status);
float16 nds_float16_sub(float16, float16, float_status *status);
float16 nds_float16_mul(float16, float16, float_status *status);
float16 nds_float16_div(float16, float16, float_status *status);
float16 nds_float16_minnum(float16, float16, float_status *status);
float16 nds_float16_minimum_number(float16, float16, float_status *status);
float16 nds_float16_min(float16, float16, float_status *status);
float16 nds_float16_maxnum(float16, float16, float_status *status);
float16 nds_float16_maximum_number(float16, float16, float_status *status);
float16 nds_float16_max(float16, float16, float_status *status);
float16 nds_float16_sqrt(float16, float_status *status);
bool nds_float16_le(float16, float16, float_status *status);
bool nds_float16_le_quiet(float16, float16, float_status *status);
bool nds_float16_lt(float16, float16, float_status *status);
bool nds_float16_lt_quiet(float16, float16, float_status *status);
bool nds_float16_eq_quiet(float16, float16, float_status *status);
float16 nds_float16_round_to_int(float16, float_status *status);
int32_t nds_float16_to_int32(float16, float_status *status);
uint32_t nds_float16_to_uint32(float16, float_status *status);
int64_t nds_float16_to_int64(float16, float_status *status);
uint64_t nds_float16_to_uint64(float16, float_status *status);
float16 nds_int32_to_float16(int32_t, float_status *status);
float16 nds_uint32_to_float16(uint32_t, float_status *status);
float16 nds_int64_to_float16(int64_t, float_status *status);
float16 nds_uint64_to_float16(uint64_t, float_status *status);
float16 nds_float32_to_float16(float32, bool ieee, float_status *status);
float32 nds_float16_to_float32(float16, bool ieee, float_status *status);
float16 nds_float64_to_float16(float64, bool ieee, float_status *status);
float64 nds_float16_to_float64(float16, bool ieee, float_status *status);

FloatRelation nds_float16_compare_quiet(float16, float16, float_status *status);
FloatRelation nds_float16_compare(float16, float16, float_status *status);

/* int16 */
uint16_t nds_float16_to_uint16(float16, float_status *status);
int16_t nds_float16_to_int16(float16, float_status *status);
float16 nds_uint16_to_float16(uint16_t, float_status *status);
float16 nds_int16_to_float16(int16_t, float_status *status);
/* int8 */
float16 nds_int8_to_float16(int8_t, float_status *status);
float16 nds_uint8_to_float16(uint8_t, float_status *status);
uint8_t nds_float16_to_uint8(float16, float_status *status);
int8_t nds_float16_to_int8(float16, float_status *status);
/* int4 */
float16 nds_int64_to_float16_scalbn(int64_t a, int, float_status *status);
float16 nds_uint64_to_float16_scalbn(uint64_t a, int, float_status *status);
target_ulong fclass_h_bf16(uint64_t frs1);
#endif
