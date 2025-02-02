/*
 * QEMU RISC-V Native Debug Support
 *
 * Copyright (c) 2022 Wind River Systems, Inc.
 *
 * Author:
 *   Bin Meng <bin.meng@windriver.com>
 *
 * This provides the native debug support via the Trigger Module, as defined
 * in the RISC-V Debug Specification:
 * https://github.com/riscv/riscv-debug-spec/raw/master/riscv-debug-stable.pdf
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
#include "qemu/log.h"
#include "qapi/error.h"
#include "cpu.h"
#include "trace.h"
#include "exec/exec-all.h"
#include "exec/helper-proto.h"
#include "sysemu/cpu-timers.h"

/*
 * The following M-mode trigger CSRs are implemented:
 *
 * - tselect
 * - tdata1
 * - tdata2
 * - tdata3
 * - tinfo
 *
 * The following triggers are initialized by default:
 *
 * Index | Type |          tdata mapping | Description
 * ------+------+------------------------+------------
 *     0 |    2 |         tdata1, tdata2 | Address / Data Match
 *     1 |    2 |         tdata1, tdata2 | Address / Data Match
 */

/* tdata availability of a trigger */
typedef bool tdata_avail[TDATA_NUM];

static tdata_avail tdata_mapping[TRIGGER_TYPE_NUM] = {
    [TRIGGER_TYPE_NO_EXIST] = { false, false, false },
    [TRIGGER_TYPE_AD_MATCH] = { true, true, true },
    [TRIGGER_TYPE_INST_CNT] = { true, false, true },
    [TRIGGER_TYPE_INT] = { true, true, true },
    [TRIGGER_TYPE_EXCP] = { true, true, true },
    [TRIGGER_TYPE_AD_MATCH6] = { true, true, true },
    [TRIGGER_TYPE_EXT_SRC] = { true, false, false },
    [TRIGGER_TYPE_DISABLED] = { true, true, true }
};

/* only breakpoint size 1/2/4/8 supported */
static int access_size[SIZE_NUM] = {
    [SIZE_ANY] = 0,
    [SIZE_1B]  = 1,
    [SIZE_2B]  = 2,
    [SIZE_4B]  = 4,
    [SIZE_6B]  = -1,
    [SIZE_8B]  = 8,
    [6 ... 15] = -1,
};

static inline target_ulong extract_trigger_type(CPURISCVState *env,
                                                target_ulong tdata1)
{
    switch (riscv_cpu_mxl(env)) {
    case MXL_RV32:
        return extract32(tdata1, 28, 4);
    case MXL_RV64:
    case MXL_RV128:
        return extract64(tdata1, 60, 4);
    default:
        g_assert_not_reached();
    }
}

static inline bool extract_tdata1_dmode(CPURISCVState *env, target_ulong tdata1)
{
    switch (riscv_cpu_mxl(env)) {
    case MXL_RV32:
        return extract32(tdata1, 27, 1);
    case MXL_RV64:
    case MXL_RV128:
        return extract64(tdata1, 59, 1);
    default:
        g_assert_not_reached();
    }
}

static inline target_ulong get_trigger_type(CPURISCVState *env,
                                            target_ulong trigger_index)
{
    return extract_trigger_type(env, env->tdata1[trigger_index]);
}

static trigger_action_t get_trigger_action(CPURISCVState *env,
                                           target_ulong trigger_index)
{
    target_ulong tdata1 = env->tdata1[trigger_index];
    int trigger_type = get_trigger_type(env, trigger_index);
    trigger_action_t action = DBG_ACTION_NONE;

    switch (trigger_type) {
    case TRIGGER_TYPE_AD_MATCH:
        action = (tdata1 & TYPE2_ACTION) >> 12;
        break;
    case TRIGGER_TYPE_AD_MATCH6:
        action = (tdata1 & TYPE6_ACTION) >> 12;
        break;
    case TRIGGER_TYPE_INST_CNT:
        action = (tdata1 & ITRIGGER_ACTION);
        break;
    case TRIGGER_TYPE_INT:
    case TRIGGER_TYPE_EXCP:
    case TRIGGER_TYPE_EXT_SRC:
        qemu_log_mask(LOG_UNIMP, "trigger type: %d is not supported\n",
                      trigger_type);
        break;
    case TRIGGER_TYPE_NO_EXIST:
        qemu_log_mask(LOG_GUEST_ERROR, "trigger type: %d does not exist\n",
                      trigger_type);
        break;
    case TRIGGER_TYPE_DISABLED:
        break;
    default:
        g_assert_not_reached();
    }

    return action;
}

static inline target_ulong build_tdata1(CPURISCVState *env,
                                        trigger_type_t type,
                                        bool dmode, target_ulong data)
{
    target_ulong tdata1;

    switch (riscv_cpu_mxl(env)) {
    case MXL_RV32:
        tdata1 = RV32_TYPE(type) |
                 (dmode ? RV32_DMODE : 0) |
                 (data & RV32_DATA_MASK);
        break;
    case MXL_RV64:
    case MXL_RV128:
        tdata1 = RV64_TYPE(type) |
                 (dmode ? RV64_DMODE : 0) |
                 (data & RV64_DATA_MASK);
        break;
    default:
        g_assert_not_reached();
    }

    return tdata1;
}

bool tdata_available(CPURISCVState *env, int tdata_index)
{
    int trigger_type = get_trigger_type(env, env->trigger_cur);

    if (unlikely(tdata_index >= TDATA_NUM)) {
        return false;
    }

    return tdata_mapping[trigger_type][tdata_index];
}

target_ulong tselect_csr_read(CPURISCVState *env)
{
    return env->trigger_cur;
}

void tselect_csr_write(CPURISCVState *env, target_ulong val)
{
    if (val < RV_MAX_TRIGGERS) {
        env->trigger_cur = val;
    }
}

static target_ulong tdata1_validate(CPURISCVState *env, target_ulong val,
                                    trigger_type_t t)
{
    uint32_t type, dmode;
    target_ulong tdata1;

    switch (riscv_cpu_mxl(env)) {
    case MXL_RV32:
        type = extract32(val, 28, 4);
        dmode = extract32(val, 27, 1);
        tdata1 = RV32_TYPE(t);
        break;
    case MXL_RV64:
    case MXL_RV128:
        type = extract64(val, 60, 4);
        dmode = extract64(val, 59, 1);
        tdata1 = RV64_TYPE(t);
        break;
    default:
        g_assert_not_reached();
    }

    if (type != t) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "ignoring type write to tdata1 register\n");
    }

    if (dmode != 0) {
        qemu_log_mask(LOG_UNIMP, "debug mode is not supported\n");
    }

    return tdata1;
}

static inline void warn_always_zero_bit(target_ulong val, target_ulong mask,
                                        const char *msg)
{
    if (val & mask) {
        qemu_log_mask(LOG_UNIMP, "%s bit is always zero\n", msg);
    }
}

static target_ulong textra_validate(CPURISCVState *env, target_ulong tdata3)
{
    target_ulong mhvalue, mhselect, sbytemask, svalue, sselect;
    target_ulong mhvalue_new, mhselect_new;
    target_ulong sbytemask_new, svalue_new, sselect_new;
    target_ulong textra = 0;

    const uint32_t mhselect_if_h[8] = { 0, 1, 2, 0, 4, 5, 6, 4 };
    const uint32_t mhselect_no_h[8] = { 0, 0, 0, 0, 4, 4, 4, 4 };
    const uint32_t sselect_if_s[4]  = { 0, 1, 2, 0 };

    if (riscv_cpu_mxl(env) == MXL_RV32) {
        mhvalue_new   = mhvalue   = extract32(tdata3, 26,  6);
        mhselect_new  = mhselect  = extract32(tdata3, 23,  3);
        sbytemask_new = sbytemask = extract32(tdata3, 18,  2);
        svalue_new    = svalue    = extract32(tdata3,  2, 16);
        sselect_new   = sselect   = extract32(tdata3,  0,  2);
    } else {
        mhvalue_new   = mhvalue   = extract64(tdata3, 51, 13);
        mhselect_new  = mhselect  = extract64(tdata3, 48,  3);
        sbytemask_new = sbytemask = extract64(tdata3, 36,  5);
        svalue_new    = svalue    = extract64(tdata3,  2, 34);
        sselect_new   = sselect   = extract64(tdata3,  0,  2);
    }

    /* Validate mhvalue and mhselect. */
    if (riscv_has_ext(env, RVH)) {
        mhselect_new = mhselect_if_h[mhselect];
    } else {
        mhselect_new = mhselect_no_h[mhselect];
    }

    if ((mhselect_new == 1 || mhselect_new == 4 || mhselect_new == 5) &&
        !riscv_cpu_cfg(env)->ext_sdtrig_mcontext) {
        /* 1, 4, 5 are only illegal when CSR mcontext is implemented. */
        mhselect_new = 0;
    }

    if (mhselect_new == 0) {
        /* Hardwire mhvalue to 0 since mhselect is 0. */
        mhvalue_new = 0;
    }

    /* Only set sselect when S-mode is enabled.  */
    if (riscv_has_ext(env, RVS)) {
        sselect_new = sselect_if_s[sselect];
    } else {
        sselect_new = 0;
    }

    if ((sselect_new == 1) && !riscv_cpu_cfg(env)->ext_sdtrig_scontext) {
        /* 1 is only illegal when CSR scontext is implemented. */
        sselect_new = 0;
    }

    if (sselect_new == 0) {
        /* Hardwire svalue to 0 since sselect is 0 */
        svalue_new = 0;
    }

    /* Write legal values into textra */
    if (riscv_cpu_mxl(env) == MXL_RV32) {
        textra = set_field(textra, TEXTRA32_MHVALUE, mhvalue_new);
        textra = set_field(textra, TEXTRA32_MHSELECT, mhselect_new);
        textra = set_field(textra, TEXTRA32_SBYTEMASK, sbytemask_new);
        textra = set_field(textra, TEXTRA32_SVALUE, svalue_new);
        textra = set_field(textra, TEXTRA32_SSELECT, sselect_new);
    } else {
        textra = set_field(textra, TEXTRA64_MHVALUE, mhvalue_new);
        textra = set_field(textra, TEXTRA64_MHSELECT, mhselect_new);
        textra = set_field(textra, TEXTRA64_SBYTEMASK, sbytemask_new);
        textra = set_field(textra, TEXTRA64_SVALUE, svalue_new);
        textra = set_field(textra, TEXTRA64_SSELECT, sselect_new);
    }

    if (textra != tdata3) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "different value 0x" TARGET_FMT_lx " write to tdata3\n",
                      textra);
    }

    return textra;
}

static void do_trigger_action(CPURISCVState *env, target_ulong trigger_index)
{
    trigger_action_t action = get_trigger_action(env, trigger_index);

    switch (action) {
    case DBG_ACTION_NONE:
        break;
    case DBG_ACTION_BP:
        riscv_raise_exception(env, RISCV_EXCP_BREAKPOINT, 0);
        break;
    case DBG_ACTION_DBG_MODE:
    case DBG_ACTION_TRACE0:
    case DBG_ACTION_TRACE1:
    case DBG_ACTION_TRACE2:
    case DBG_ACTION_TRACE3:
    case DBG_ACTION_EXT_DBG0:
    case DBG_ACTION_EXT_DBG1:
        qemu_log_mask(LOG_UNIMP, "action: %d is not supported\n", action);
        break;
    default:
        g_assert_not_reached();
    }
}

static bool trigger_textra_match(CPURISCVState *env, trigger_type_t type,
                                 int trigger_index)
{
    target_ulong textra;
    target_ulong mhvalue, mhselect;
    target_ulong svalue, sbytemask, sselect;
    target_ulong asid, asidmax, vmid, vmidmax;
    uint64_t bytemask;
    int sbytemask_len;

    if (type < TRIGGER_TYPE_AD_MATCH || type > TRIGGER_TYPE_AD_MATCH6) {
        /* textra check only accesible when type is 2, 3, 4, 5, or 6 */
        return true;
    }

    textra = env->tdata3[trigger_index];

    bool rv32 = riscv_cpu_mxl(env) == MXL_RV32 ? true : false;
    if (rv32) {
        mhvalue   = extract32(textra, 26,  6);
        mhselect  = extract32(textra, 23,  3);
        sbytemask = extract32(textra, 18,  2);
        svalue    = extract32(textra,  2, 16);
        sselect   = extract32(textra,  0,  2);
    } else {
        mhvalue   = extract64(textra, 51, 13);
        mhselect  = extract64(textra, 48,  3);
        sbytemask = extract64(textra, 36,  5);
        svalue    = extract64(textra,  2, 34);
        sselect   = extract64(textra,  0,  2);
    }

    switch (sselect) {
    case 1:
        /* Generate bytemask */
        if (rv32) {
            bytemask = ((uint32_t)1 << TEXTRA32_SVALUE_LENGTH) - 1;
            sbytemask_len = TEXTRA32_SBYTEMASK_LENGTH;
        } else {
            bytemask = ((uint64_t)1 << TEXTRA64_SVALUE_LENGTH) - 1;
            sbytemask_len = TEXTRA64_SBYTEMASK_LENGTH;
        }
        for (int i = 0; i < sbytemask_len; i++) {
            /* Ignore bits[8*(i+1)-1:8*i] when sbytemask[i] is 1. */
            if (sbytemask & (1 << i)) {
                bytemask &= ~((uint64_t)0xFF << (8 * i));
            }
        }

        /* Match if the bytemasked low bits of scontext equal svalue. */
        if ((env->scontext & bytemask) != (svalue & bytemask)) {
            return false;
        }
        break;
    case 2:
        if (rv32) {
            if (env->virt_enabled) {
                asid = extract32(env->vsatp, 22, 9);
            } else {
                asid = extract32(env->satp, 22, 9);
            }
            asidmax = RV32_ASIDMAX;
        } else {
            if (env->virt_enabled) {
                asid = extract64(env->vsatp, 44, 16);
            } else {
                asid = extract64(env->satp, 44, 16);
            }
            asidmax = RV64_ASIDMAX;
        }

        /* Match if ASID in vsatp/satp equals lower ASIDMAX bits of svalue. */
        if (asid != (svalue & ((1 << asidmax) - 1))) {
            return false;
        }
        break;
    default:
        break;
    }

    switch (mhselect) {
    case 1:
    case 5:
        /* 1, 5 should shift mhvalue as {mhvalue, mhselect[2]} first. */
        mhvalue = ((mhvalue << 1) | (mhselect >> 2));
        /* fall through */
    case 4:
        /* Match if the low bits of mcontext/hcontext equal mhvalue. */
        if (riscv_has_ext(env, RVH)) {
            /* Compare mhvalue with hcontext(whole bits of mcontext) */
            if (mhvalue != env->mcontext) {
                return false;
            }
        } else {
            /* Compare mhvalue with parts of bits of mcontext */
            if (rv32 && (mhvalue != (env->mcontext & MCONTEXT32))) {
                return false;
            } else if (mhvalue != (env->mcontext & MCONTEXT64)) {
                return false;
            }
        }
        break;
    case 2:
    case 6:
        if (rv32) {
            vmid = extract32(env->hgatp, 22, 7);
            vmidmax = RV32_VMIDMAX;
        } else {
            vmid = extract64(env->hgatp, 44, 14);
            vmidmax = RV64_VMIDMAX;
        }

        /*
         * Match if VMID in hgatp equals the lower VMIDMAX bits of
         * {mhvalue, mhselect[2]}.
         */
        mhvalue = ((mhvalue << 1) | (mhselect >> 2));
        if (vmid != (mhvalue & ((1 << vmidmax) - 1))) {
            return false;
        }
        break;
    default:
        break;
    }

    return true;
}

static bool tcontrol_match(CPURISCVState *env)
{
    /*
     * tcontrol CSR controls whether the triggers can match/fire while the hart
     * is in M-mode.
     */
    if (riscv_cpu_cfg(env)->ext_sdtrig_tcontrol && (env->priv == PRV_M)) {
        if ((env->tcontrol & TCONTROL_MTE) == 0) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "trigger in M-mode but tcontrol.MTE is not set\n");
            return false;
        }
    }

    return true;
}

static bool trigger_mode_match(CPURISCVState *env, trigger_type_t type,
                               int trigger_index)
{
    target_ulong ctrl = env->tdata1[trigger_index];

    switch (type) {
    case TRIGGER_TYPE_AD_MATCH:
        /* type 2 trigger cannot be fired in VU/VS mode */
        if (env->virt_enabled) {
            return false;
        }
        /* check U/S/M bit against current privilege level */
        if ((ctrl >> TYPE2_U_SHIFT) & BIT(env->priv)) {
            return true;
        }
        break;
    case TRIGGER_TYPE_AD_MATCH6:
        if (env->virt_enabled) {
            /* check VU/VS bit against current privilege level */
            if ((ctrl >> TYPE6_VU_SHIFT) & BIT(env->priv)) {
                return true;
            }
        } else {
            /* check U/S/M bit against current privilege level */
            if ((ctrl >> TYPE6_U_SHIFT) & BIT(env->priv)) {
                return true;
            }
        }
        break;
    case TRIGGER_TYPE_INST_CNT:
        if (env->virt_enabled) {
            /* check VU/VS bit against current privilege level */
            if ((ctrl >> ITRIGGER_VU_SHIFT) & BIT(env->priv)) {
                return true;
            }
        } else {
            /* check U/S/M bit against current privilege level */
            if ((ctrl >> ITRIGGER_U_SHIFT) & BIT(env->priv)) {
                return true;
            }
        }
        break;
    case TRIGGER_TYPE_INT:
    case TRIGGER_TYPE_EXCP:
    case TRIGGER_TYPE_EXT_SRC:
        qemu_log_mask(LOG_UNIMP, "trigger type: %d is not supported\n", type);
        break;
    case TRIGGER_TYPE_NO_EXIST:
        qemu_log_mask(LOG_GUEST_ERROR, "trigger type: %d does not exist\n",
                      type);
        break;
    case TRIGGER_TYPE_DISABLED:
        break;
    default:
        g_assert_not_reached();
    }

    return false;
}

static bool trigger_common_match(CPURISCVState *env, trigger_type_t type,
                                 int trigger_index)
{
    return trigger_mode_match(env, type, trigger_index) &&
           trigger_textra_match(env, type, trigger_index) &&
           tcontrol_match(env);
}

/* type 2 trigger */

static uint32_t type2_breakpoint_size(CPURISCVState *env, target_ulong ctrl)
{
    uint32_t sizelo, sizehi = 0;

    if (riscv_cpu_mxl(env) == MXL_RV64) {
        sizehi = extract32(ctrl, 21, 2);
    }
    sizelo = extract32(ctrl, 16, 2);
    return (sizehi << 2) | sizelo;
}

static inline bool type2_breakpoint_enabled(target_ulong ctrl)
{
    bool mode = !!(ctrl & (TYPE2_U | TYPE2_S | TYPE2_M));
    bool rwx = !!(ctrl & (TYPE2_LOAD | TYPE2_STORE | TYPE2_EXEC));

    return mode && rwx;
}

static target_ulong type2_mcontrol_validate(CPURISCVState *env,
                                            target_ulong ctrl)
{
    target_ulong val;
    uint32_t size;

    /* validate the generic part first */
    val = tdata1_validate(env, ctrl, TRIGGER_TYPE_AD_MATCH);

    /* validate unimplemented (always zero) bits */
    warn_always_zero_bit(ctrl, TYPE2_MATCH, "match");
    warn_always_zero_bit(ctrl, TYPE2_CHAIN, "chain");
    warn_always_zero_bit(ctrl, TYPE2_ACTION, "action");
    warn_always_zero_bit(ctrl, TYPE2_TIMING, "timing");
    warn_always_zero_bit(ctrl, TYPE2_SELECT, "select");
    warn_always_zero_bit(ctrl, TYPE2_HIT, "hit");

    /* validate size encoding */
    size = type2_breakpoint_size(env, ctrl);
    if (access_size[size] == -1) {
        qemu_log_mask(LOG_UNIMP, "access size %d is not supported, using "
                                 "SIZE_ANY\n", size);
    } else {
        val |= (ctrl & TYPE2_SIZELO);
        if (riscv_cpu_mxl(env) == MXL_RV64) {
            val |= (ctrl & TYPE2_SIZEHI);
        }
    }

    /* keep the mode and attribute bits */
    val |= (ctrl & (TYPE2_U | TYPE2_S | TYPE2_M |
                    TYPE2_LOAD | TYPE2_STORE | TYPE2_EXEC));

    return val;
}

static void type2_breakpoint_insert(CPURISCVState *env, target_ulong index)
{
    target_ulong ctrl = env->tdata1[index];
    target_ulong addr = env->tdata2[index];
    bool enabled = type2_breakpoint_enabled(ctrl);
    CPUState *cs = env_cpu(env);
    int flags = BP_CPU | BP_STOP_BEFORE_ACCESS;
    uint32_t size;

    if (!enabled) {
        return;
    }

    if (ctrl & TYPE2_EXEC) {
        cpu_breakpoint_insert(cs, addr, flags, &env->cpu_breakpoint[index]);
    }

    if (ctrl & TYPE2_LOAD) {
        flags |= BP_MEM_READ;
    }
    if (ctrl & TYPE2_STORE) {
        flags |= BP_MEM_WRITE;
    }

    if (flags & BP_MEM_ACCESS) {
        size = type2_breakpoint_size(env, ctrl);
        if (size != 0) {
            cpu_watchpoint_insert(cs, addr, size, flags,
                                  &env->cpu_watchpoint[index]);
        } else {
            cpu_watchpoint_insert(cs, addr, 8, flags,
                                  &env->cpu_watchpoint[index]);
        }
    }
}

static void type2_breakpoint_remove(CPURISCVState *env, target_ulong index)
{
    CPUState *cs = env_cpu(env);

    if (env->cpu_breakpoint[index]) {
        cpu_breakpoint_remove_by_ref(cs, env->cpu_breakpoint[index]);
        env->cpu_breakpoint[index] = NULL;
    }

    if (env->cpu_watchpoint[index]) {
        cpu_watchpoint_remove_by_ref(cs, env->cpu_watchpoint[index]);
        env->cpu_watchpoint[index] = NULL;
    }
}

static void type2_reg_write(CPURISCVState *env, target_ulong index,
                            int tdata_index, target_ulong val)
{
    target_ulong new_val;

    switch (tdata_index) {
    case TDATA1:
        new_val = type2_mcontrol_validate(env, val);
        if (new_val != env->tdata1[index]) {
            env->tdata1[index] = new_val;
            type2_breakpoint_remove(env, index);
            type2_breakpoint_insert(env, index);
        }
        break;
    case TDATA2:
        if (val != env->tdata2[index]) {
            env->tdata2[index] = val;
            type2_breakpoint_remove(env, index);
            type2_breakpoint_insert(env, index);
        }
        break;
    case TDATA3:
        new_val = textra_validate(env, val);
        if (new_val != env->tdata3[index]) {
            env->tdata3[index] = new_val;
        }
        break;
    default:
        g_assert_not_reached();
    }

    return;
}

/* type 6 trigger */

static inline bool type6_breakpoint_enabled(target_ulong ctrl)
{
    bool mode = !!(ctrl & (TYPE6_VU | TYPE6_VS | TYPE6_U | TYPE6_S | TYPE6_M));
    bool rwx = !!(ctrl & (TYPE6_LOAD | TYPE6_STORE | TYPE6_EXEC));

    return mode && rwx;
}

static target_ulong type6_mcontrol6_validate(CPURISCVState *env,
                                             target_ulong ctrl)
{
    target_ulong val;
    uint32_t size;

    /* validate the generic part first */
    val = tdata1_validate(env, ctrl, TRIGGER_TYPE_AD_MATCH6);

    /* validate unimplemented (always zero) bits */
    warn_always_zero_bit(ctrl, TYPE6_MATCH, "match");
    warn_always_zero_bit(ctrl, TYPE6_CHAIN, "chain");
    warn_always_zero_bit(ctrl, TYPE6_ACTION, "action");
    warn_always_zero_bit(ctrl, TYPE6_TIMING, "timing");
    warn_always_zero_bit(ctrl, TYPE6_SELECT, "select");
    warn_always_zero_bit(ctrl, TYPE6_HIT, "hit");

    /* validate size encoding */
    size = extract32(ctrl, 16, 4);
    if (access_size[size] == -1) {
        qemu_log_mask(LOG_UNIMP, "access size %d is not supported, using "
                                 "SIZE_ANY\n", size);
    } else {
        val |= (ctrl & TYPE6_SIZE);
    }

    /* keep the mode and attribute bits */
    val |= (ctrl & (TYPE6_VU | TYPE6_VS | TYPE6_U | TYPE6_S | TYPE6_M |
                    TYPE6_LOAD | TYPE6_STORE | TYPE6_EXEC));

    return val;
}

static void type6_breakpoint_insert(CPURISCVState *env, target_ulong index)
{
    target_ulong ctrl = env->tdata1[index];
    target_ulong addr = env->tdata2[index];
    bool enabled = type6_breakpoint_enabled(ctrl);
    CPUState *cs = env_cpu(env);
    int flags = BP_CPU | BP_STOP_BEFORE_ACCESS;
    uint32_t size;

    if (!enabled) {
        return;
    }

    if (ctrl & TYPE6_EXEC) {
        cpu_breakpoint_insert(cs, addr, flags, &env->cpu_breakpoint[index]);
    }

    if (ctrl & TYPE6_LOAD) {
        flags |= BP_MEM_READ;
    }

    if (ctrl & TYPE6_STORE) {
        flags |= BP_MEM_WRITE;
    }

    if (flags & BP_MEM_ACCESS) {
        size = extract32(ctrl, 16, 4);
        if (size != 0) {
            cpu_watchpoint_insert(cs, addr, size, flags,
                                  &env->cpu_watchpoint[index]);
        } else {
            cpu_watchpoint_insert(cs, addr, 8, flags,
                                  &env->cpu_watchpoint[index]);
        }
    }
}

static void type6_breakpoint_remove(CPURISCVState *env, target_ulong index)
{
    type2_breakpoint_remove(env, index);
}

static void type6_reg_write(CPURISCVState *env, target_ulong index,
                            int tdata_index, target_ulong val)
{
    target_ulong new_val;

    switch (tdata_index) {
    case TDATA1:
        new_val = type6_mcontrol6_validate(env, val);
        if (new_val != env->tdata1[index]) {
            env->tdata1[index] = new_val;
            type6_breakpoint_remove(env, index);
            type6_breakpoint_insert(env, index);
        }
        break;
    case TDATA2:
        if (val != env->tdata2[index]) {
            env->tdata2[index] = val;
            type6_breakpoint_remove(env, index);
            type6_breakpoint_insert(env, index);
        }
        break;
    case TDATA3:
        new_val = textra_validate(env, val);
        if (new_val != env->tdata3[index]) {
            env->tdata3[index] = new_val;
        }
        break;
    default:
        g_assert_not_reached();
    }

    return;
}

/* icount trigger type */
static inline int
itrigger_get_count(CPURISCVState *env, int index)
{
    return get_field(env->tdata1[index], ITRIGGER_COUNT);
}

static inline void
itrigger_set_count(CPURISCVState *env, int index, int value)
{
    env->tdata1[index] = set_field(env->tdata1[index],
                                   ITRIGGER_COUNT, value);
}

static inline void itrigger_set_pending(CPURISCVState *env, int index,
                                        bool pending)
{
    env->tdata1[index] = set_field(env->tdata1[index],
                                   ITRIGGER_PENDING, pending);
}

static inline bool itrigger_get_pending(CPURISCVState *env, int index)
{
    return get_field(env->tdata1[index], ITRIGGER_PENDING);
}

static inline void itrigger_set_hit(CPURISCVState *env, int index, bool hit)
{
    env->tdata1[index] = set_field(env->tdata1[index],
                                   ITRIGGER_HIT, hit);
}

void helper_itrigger_fire(CPURISCVState *env)
{
    bool hit = false;
    int hit_index = -1;

    for (int i = 0; i < RV_MAX_TRIGGERS; i++) {
        if (get_trigger_type(env, i) != TRIGGER_TYPE_INST_CNT) {
            continue;
        }
        if (!trigger_common_match(env, TRIGGER_TYPE_INST_CNT, i)) {
            continue;
        }
        if (itrigger_get_pending(env, i)) {
            /* This icount trigger can fire */
            itrigger_set_pending(env, i, false);    /* clear icount.PENDING */
            itrigger_set_hit(env, i, true);         /* set icount.HIT       */
            hit = true;
            hit_index = i;
            break;
        }
    }

    /* Re-determine if any icount trigger is pending */
    env->itrigger_pending = false;
    for (int i = 0; i < RV_MAX_TRIGGERS; i++) {
        if (get_trigger_type(env, i) != TRIGGER_TYPE_INST_CNT) {
            continue;
        }
        if (itrigger_get_pending(env, i)) {
            env->itrigger_pending = true;
            break;
        }
    }

    if (hit) {
        do_trigger_action(env, hit_index);
    }
}

bool riscv_itrigger_enabled(CPURISCVState *env)
{
    int count;
    for (int i = 0; i < RV_MAX_TRIGGERS; i++) {
        if (get_trigger_type(env, i) != TRIGGER_TYPE_INST_CNT) {
            continue;
        }
        if ((env->tdata1[i] & ITRIGGER_VS) == 0 &&
            (env->tdata1[i] & ITRIGGER_VU) == 0 &&
            (env->tdata1[i] & ITRIGGER_U)  == 0 &&
            (env->tdata1[i] & ITRIGGER_S)  == 0 &&
            (env->tdata1[i] & ITRIGGER_M)  == 0) {
            continue;
        }
        count = itrigger_get_count(env, i);
        if (!count) {
            continue;
        }
        return true;
    }

    return false;
}

void helper_itrigger_match(CPURISCVState *env)
{
    int count;
    for (int i = 0; i < RV_MAX_TRIGGERS; i++) {
        if (get_trigger_type(env, i) != TRIGGER_TYPE_INST_CNT) {
            continue;
        }
        if (!trigger_common_match(env, TRIGGER_TYPE_INST_CNT, i)) {
            continue;
        }
        count = itrigger_get_count(env, i);
        if (!count) {
            continue;
        }
        if (count == 1) {
            /* When count is 1 and the trigger matches, set pending. */
            itrigger_set_pending(env, i, true);
            env->itrigger_pending = true;
        }
        itrigger_set_count(env, i, --count);
        if (!count) {
            env->itrigger_enabled = riscv_itrigger_enabled(env);
        }
    }
}

static void riscv_itrigger_update_count(CPURISCVState *env)
{
    int count, executed;
    /*
     * Record last icount, so that we can evaluate the executed instructions
     * since last privilege mode change or timer expire.
     */
    int64_t last_icount = env->last_icount, current_icount;
    current_icount = env->last_icount = icount_get_raw();

    for (int i = 0; i < RV_MAX_TRIGGERS; i++) {
        if (get_trigger_type(env, i) != TRIGGER_TYPE_INST_CNT) {
            continue;
        }
        count = itrigger_get_count(env, i);
        if (!count) {
            continue;
        }
        /*
         * Only when privilege is changed or itrigger timer expires,
         * the count field in itrigger tdata1 register is updated.
         * And the count field in itrigger only contains remaining value.
         */
        if (trigger_common_match(env, TRIGGER_TYPE_INST_CNT, i)) {
            /*
             * If itrigger enabled in this privilege mode, the number of
             * executed instructions since last privilege change
             * should be reduced from current itrigger count.
             */
            executed = current_icount - last_icount;
            itrigger_set_count(env, i, count - executed);
            if (count == executed) {
                do_trigger_action(env, i);
            }
        } else {
            /*
             * If itrigger is not enabled in this privilege mode,
             * the number of executed instructions will be discard and
             * the count field in itrigger will not change.
             */
            timer_mod(env->itrigger_timer[i],
                      current_icount + count);
        }
    }
}

static void riscv_itrigger_timer_cb(void *opaque)
{
    g_assert(icount_enabled());
    riscv_itrigger_update_count((CPURISCVState *)opaque);
}

void riscv_itrigger_update_priv(CPURISCVState *env)
{
    g_assert(icount_enabled());
    riscv_itrigger_update_count(env);
}

static target_ulong itrigger_validate(CPURISCVState *env,
                                      target_ulong ctrl)
{
    target_ulong val;

    /* validate the generic part first */
    val = tdata1_validate(env, ctrl, TRIGGER_TYPE_INST_CNT);

    /* validate unimplemented (always zero) bits */
    warn_always_zero_bit(ctrl, ITRIGGER_ACTION, "action");
    warn_always_zero_bit(ctrl, ITRIGGER_HIT, "hit");
    warn_always_zero_bit(ctrl, ITRIGGER_PENDING, "pending");

    /* keep the mode and attribute bits */
    val |= ctrl & (ITRIGGER_VU | ITRIGGER_VS | ITRIGGER_U | ITRIGGER_S |
                   ITRIGGER_M | ITRIGGER_COUNT);

    return val;
}

static void itrigger_reg_write(CPURISCVState *env, target_ulong index,
                               int tdata_index, target_ulong val)
{
    target_ulong new_val;

    switch (tdata_index) {
    case TDATA1:
        /* set timer for icount */
        new_val = itrigger_validate(env, val);
        if (new_val != env->tdata1[index]) {
            env->tdata1[index] = new_val;
            if (icount_enabled()) {
                env->last_icount = icount_get_raw();
                /* set the count to timer */
                timer_mod(env->itrigger_timer[index],
                          env->last_icount + itrigger_get_count(env, index));
            } else {
                env->itrigger_enabled = riscv_itrigger_enabled(env);
            }
        }
        break;
    case TDATA2:
        qemu_log_mask(LOG_UNIMP,
                      "tdata2 is not supported for icount trigger\n");
        break;
    case TDATA3:
        new_val = textra_validate(env, val);
        if (new_val != env->tdata3[index]) {
            env->tdata3[index] = new_val;
        }
        break;
    default:
        g_assert_not_reached();
    }

    return;
}

static int itrigger_get_adjust_count(CPURISCVState *env)
{
    g_assert(icount_enabled());

    int count = itrigger_get_count(env, env->trigger_cur), executed;
    if ((count != 0) &&
        trigger_common_match(env, TRIGGER_TYPE_INST_CNT, env->trigger_cur)) {
        executed = icount_get_raw() - env->last_icount;
        count += executed;
    }
    return count;
}

static void riscv_disable_trigger(CPURISCVState *env,
                                  target_ulong trigger_index)
{
    int trigger_type;

    trigger_type = extract_trigger_type(env, env->tdata1[trigger_index]);
    switch (trigger_type) {
    case TRIGGER_TYPE_AD_MATCH:
        type2_breakpoint_remove(env, trigger_index);
        break;
    case TRIGGER_TYPE_AD_MATCH6:
        type6_breakpoint_remove(env, trigger_index);
        break;
    }
}

static void type15_reg_write(CPURISCVState *env, target_ulong index,
                             int tdata_index, target_ulong val)
{
    switch (tdata_index) {
    case TDATA1:
        /* Disable trigger. */
        riscv_disable_trigger(env, index);
        /* Bits are ignored except for dmode field. */
        env->tdata1[index] = build_tdata1(env, TRIGGER_TYPE_DISABLED,
                                          extract_tdata1_dmode(env, val), 0);
        break;
    case TDATA2:
        if (val != env->tdata2[index]) {
            env->tdata2[index] = val;
        }
        break;
    case TDATA3:
        val = textra_validate(env, val);
        if (val != env->tdata3[index]) {
            env->tdata3[index] = val;
        }
        break;
    default:
        g_assert_not_reached();
    }
}

target_ulong tdata_csr_read(CPURISCVState *env, int tdata_index)
{
    int trigger_type;
    switch (tdata_index) {
    case TDATA1:
        trigger_type = extract_trigger_type(env,
                                            env->tdata1[env->trigger_cur]);
        if ((trigger_type == TRIGGER_TYPE_INST_CNT) && icount_enabled()) {
            return deposit64(env->tdata1[env->trigger_cur], 10, 14,
                             itrigger_get_adjust_count(env));
        }
        return env->tdata1[env->trigger_cur];
    case TDATA2:
        return env->tdata2[env->trigger_cur];
    case TDATA3:
        return env->tdata3[env->trigger_cur];
    default:
        g_assert_not_reached();
    }
}

void tdata_csr_write(CPURISCVState *env, int tdata_index, target_ulong val)
{
    int trigger_type;

    if (tdata_index == TDATA1 && val == 0) {
        /* Write 0 to tdata1 disables the trigger. */
        riscv_disable_trigger(env, env->trigger_cur);
        env->tdata1[env->trigger_cur] = build_tdata1(env, TRIGGER_TYPE_DISABLED,
                                                     0, 0);
        return;
    }

    if (tdata_index == TDATA1) {
        trigger_type = extract_trigger_type(env, val);
    } else {
        trigger_type = get_trigger_type(env, env->trigger_cur);
    }

    switch (trigger_type) {
    case TRIGGER_TYPE_AD_MATCH:
        type2_reg_write(env, env->trigger_cur, tdata_index, val);
        break;
    case TRIGGER_TYPE_AD_MATCH6:
        type6_reg_write(env, env->trigger_cur, tdata_index, val);
        break;
    case TRIGGER_TYPE_INST_CNT:
        itrigger_reg_write(env, env->trigger_cur, tdata_index, val);
        break;
    case TRIGGER_TYPE_INT:
    case TRIGGER_TYPE_EXCP:
    case TRIGGER_TYPE_EXT_SRC:
        qemu_log_mask(LOG_UNIMP, "trigger type: %d is not supported\n",
                      trigger_type);
        break;
    case TRIGGER_TYPE_NO_EXIST:
        qemu_log_mask(LOG_GUEST_ERROR, "trigger type: %d does not exist\n",
                      trigger_type);
        break;
    case TRIGGER_TYPE_DISABLED:
        type15_reg_write(env, env->trigger_cur, tdata_index, val);
        break;
    default:
        g_assert_not_reached();
    }
}

target_ulong tinfo_csr_read(CPURISCVState *env)
{
    /* assume all triggers support the same types of triggers */
    target_ulong tinfo;

    tinfo = BIT(TRIGGER_TYPE_AD_MATCH) |
            BIT(TRIGGER_TYPE_INST_CNT) |
            BIT(TRIGGER_TYPE_AD_MATCH6);
    tinfo = set_field(tinfo, TINFO_VERSION, RV_SDTRIG_VERSION);

    return tinfo;
}

static int get_hit_breakpoint_idx(CPUState *cs, vaddr pc, int mask)
{
    RISCVCPU *cpu = RISCV_CPU(cs);
    CPURISCVState *env = &cpu->env;
    CPUBreakpoint *bp;
    int i;

    for (i = 0; i < ARRAY_SIZE(env->cpu_breakpoint); i++) {
        bp = env->cpu_breakpoint[i];
        if (bp && bp->pc == pc && (bp->flags & mask)) {
            return i;
        }
    }

    return -1;
}

static int get_hit_watchpoint_idx(CPUState *cs)
{
    RISCVCPU *cpu = RISCV_CPU(cs);
    CPURISCVState *env = &cpu->env;
    CPUWatchpoint *wp = cs->watchpoint_hit;
    int i;

    for (i = 0; i < ARRAY_SIZE(env->cpu_watchpoint); i++) {
        if (wp == env->cpu_watchpoint[i]) {
            return i;
        }
    }

    return -1;
}

void riscv_cpu_debug_excp_handler(CPUState *cs)
{
    RISCVCPU *cpu = RISCV_CPU(cs);
    CPURISCVState *env = &cpu->env;

    if (cs->watchpoint_hit) {
        if (cs->watchpoint_hit->flags & BP_CPU) {
            do_trigger_action(env, get_hit_watchpoint_idx(cs));
        }
    } else {
        if (cpu_breakpoint_test(cs, env->pc, BP_CPU)) {
            do_trigger_action(env, get_hit_breakpoint_idx(cs, env->pc, BP_CPU));
        }
    }
}

bool riscv_cpu_debug_check_breakpoint(CPUState *cs)
{
    RISCVCPU *cpu = RISCV_CPU(cs);
    CPURISCVState *env = &cpu->env;
    CPUBreakpoint *bp;
    target_ulong ctrl;
    target_ulong pc;
    int trigger_type;
    int i;

    QTAILQ_FOREACH(bp, &cs->breakpoints, entry) {
        for (i = 0; i < RV_MAX_TRIGGERS; i++) {
            trigger_type = get_trigger_type(env, i);

            if (!trigger_common_match(env, trigger_type, i)) {
                continue;
            }

            switch (trigger_type) {
            case TRIGGER_TYPE_AD_MATCH:
                ctrl = env->tdata1[i];
                pc = env->tdata2[i];

                if ((ctrl & TYPE2_EXEC) && (bp->pc == pc)) {
                    return true;
                }
                break;
            case TRIGGER_TYPE_AD_MATCH6:
                ctrl = env->tdata1[i];
                pc = env->tdata2[i];

                if ((ctrl & TYPE6_EXEC) && (bp->pc == pc)) {
                    return true;
                }
                break;
            default:
                /* other trigger types are not supported or irrelevant */
                break;
            }
        }
    }

    return false;
}

bool riscv_cpu_debug_check_watchpoint(CPUState *cs, CPUWatchpoint *wp)
{
    RISCVCPU *cpu = RISCV_CPU(cs);
    CPURISCVState *env = &cpu->env;
    target_ulong ctrl;
    target_ulong addr;
    int trigger_type;
    int flags;
    int i;

    for (i = 0; i < RV_MAX_TRIGGERS; i++) {
        trigger_type = get_trigger_type(env, i);

        if (!trigger_common_match(env, trigger_type, i)) {
            continue;
        }

        switch (trigger_type) {
        case TRIGGER_TYPE_AD_MATCH:
            ctrl = env->tdata1[i];
            addr = env->tdata2[i];
            flags = 0;

            if (ctrl & TYPE2_LOAD) {
                flags |= BP_MEM_READ;
            }
            if (ctrl & TYPE2_STORE) {
                flags |= BP_MEM_WRITE;
            }

            if ((wp->flags & flags) && (wp->vaddr == addr)) {
                return true;
            }
            break;
        case TRIGGER_TYPE_AD_MATCH6:
            ctrl = env->tdata1[i];
            addr = env->tdata2[i];
            flags = 0;

            if (ctrl & TYPE6_LOAD) {
                flags |= BP_MEM_READ;
            }
            if (ctrl & TYPE6_STORE) {
                flags |= BP_MEM_WRITE;
            }

            if ((wp->flags & flags) && (wp->vaddr == addr)) {
                return true;
            }
            break;
        default:
            /* other trigger types are not supported */
            break;
        }
    }

    return false;
}

void riscv_trigger_realize(CPURISCVState *env)
{
    if (icount_enabled()) {
        int i;

        for (i = 0; i < RV_MAX_TRIGGERS; i++) {
            env->itrigger_timer[i] = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                                  riscv_itrigger_timer_cb, env);
        }
    }
}

void riscv_trigger_reset_hold(CPURISCVState *env)
{
    target_ulong tdata1 = build_tdata1(env, TRIGGER_TYPE_AD_MATCH, 0, 0);
    int i;

    /* init to type 2 triggers */
    for (i = 0; i < RV_MAX_TRIGGERS; i++) {
        /*
         * type = TRIGGER_TYPE_AD_MATCH
         * dmode = 0 (both debug and M-mode can write tdata)
         * maskmax = 0 (unimplemented, always 0)
         * sizehi = 0 (match against any size, RV64 only)
         * hit = 0 (unimplemented, always 0)
         * select = 0 (always 0, perform match on address)
         * timing = 0 (always 0, trigger before instruction)
         * sizelo = 0 (match against any size)
         * action = 0 (always 0, raise a breakpoint exception)
         * chain = 0 (unimplemented, always 0)
         * match = 0 (always 0, when any compare value equals tdata2)
         */
        env->tdata1[i] = tdata1;
        env->tdata2[i] = 0;
        env->tdata3[i] = 0;
        env->cpu_breakpoint[i] = NULL;
        env->cpu_watchpoint[i] = NULL;
        if (icount_enabled()) {
            timer_del(env->itrigger_timer[i]);
        }
    }

    env->tcontrol = 0;
    env->scontext = 0;
    env->mcontext = 0;
}
