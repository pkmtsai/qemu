/*
 * Andes custom CSR table and handling functions
 *
 * Copyright (c) 2021 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/main-loop.h"
#include "exec/exec-all.h"
#include "andes_cpu_bits.h"
#include "csr_andes.h"

static RISCVException any(CPURISCVState *env,
                          int csrno)
{
    return RISCV_EXCP_NONE;
}

static RISCVException write_mcache_ctl(CPURISCVState *env,
                                       int csrno,
                                       target_ulong val)
{
    /* Change DC_COHSTA to 1 if DC_COHEN is set. Vice versa */
    if (val & (1UL << V5_MCACHE_CTL_DC_COHEN)) {
        val |= (1UL << V5_MCACHE_CTL_DC_COHSTA);
    } else {
        val &= ~(1UL << V5_MCACHE_CTL_DC_COHSTA);
    }
    env->andes_csr.csrno[csrno] = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_csr(CPURISCVState *env,
                               int csrno,
                               target_ulong *val)
{
    *val = env->andes_csr.csrno[csrno];
    return RISCV_EXCP_NONE;
}

static RISCVException write_csr(CPURISCVState *env,
                                int csrno,
                                target_ulong val)
{
    env->andes_csr.csrno[csrno] = val;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mecc_code(CPURISCVState *env, int csrno,
                                      target_ulong val)
{
    // we only need to take care of CODE field, other fields are always zero.
    env->andes_csr.csrno[CSR_MECC_CODE] = val & MASK_CSR_MECC_CODE_CODE;
    return RISCV_EXCP_NONE;
}

static RISCVException write_uitb(CPURISCVState *env, int csrno,
                                 target_ulong val)
{
    // we only need to take care of ADDR field
    env->andes_csr.csrno[CSR_UITB] = val & MASK_CSR_UITB_ADDR;
    return RISCV_EXCP_NONE;
}

void andes_csr_init(AndesCsr *andes_csr)
{
    int i;

    /* Register CSR read/write method */
    for (i = 0; i < CSR_TABLE_SIZE; i++) {
        if (andes_csr_ops[i].name != NULL) {
            riscv_set_csr_ops(i, &andes_csr_ops[i]);
        }
    }

    /* Initilaize Andes CSRs default value */
    andes_csr->csrno[CSR_UITB] = 0;
    andes_csr->csrno[CSR_MMSC_CFG] =    (1UL << V5_MMSC_CFG_ECD) |
                                        (1UL << V5_MMSC_CFG_PPMA);
    andes_csr->csrno[CSR_MMISC_CTL] =   (1UL << V5_MMISC_CTL_BRPE) |
                                        (1UL << V5_MMISC_CTL_MSA_OR_UNA);
    andes_csr->csrno[CSR_MCACHE_CTL] =  (1UL << V5_MCACHE_CTL_IC_FIRST_WORD) |
                                        (1UL << V5_MCACHE_CTL_DC_FIRST_WORD);
}

void andes_vec_init(AndesVec *andes_vec)
{
    andes_vec->vectored_irq_m = 0;
    andes_vec->vectored_irq_s = 0;
}

void andes_cpu_do_interrupt_post(CPUState *cs)
{
#if !defined(CONFIG_USER_ONLY)
    RISCVCPU *cpu = RISCV_CPU(cs);
    CPURISCVState *env = &cpu->env;
    AndesCsr *csr = &env->andes_csr;
    AndesVec *vec = &env->andes_vec;

    if (csr->csrno[CSR_MMISC_CTL] & (1UL << V5_MMISC_CTL_VEC_PLIC)) {
        if (env->priv == PRV_M) {
            int irq_id = vec->vectored_irq_m;
            vec->vectored_irq_m = 0;
            target_ulong base = env->mtvec;
            env->pc = ((uint64_t)env->pc >> 32) << 32;
            env->pc |= cpu_ldl_data(env, base + (irq_id << 2));
            return;
        }

        if (env->priv == PRV_S) {
            int irq_id = vec->vectored_irq_s;
            vec->vectored_irq_s = 0;
            target_ulong base = env->stvec;
            env->pc = ((uint64_t)env->pc >> 32) << 32;
            env->pc |= cpu_ldl_data(env, base + (irq_id << 2));
            return;
        }
    }
#endif
}

riscv_csr_operations andes_csr_ops[CSR_TABLE_SIZE] = {
    /* ================== AndeStar V5 machine mode CSRs ================== */
    /* Configuration Registers */
    [CSR_MICM_CFG]  = { "micm_cfg",          any, read_csr },
    [CSR_MDCM_CFG]  = { "mdcm_cfg",          any, read_csr },
    [CSR_MMSC_CFG]  = { "mmsc_cfg",          any, read_csr },
    [CSR_MMSC_CFG2] = { "mmsc_cfg2",         any, read_csr },
    [CSR_MVEC_CFG]  = { "mvec_cfg",          any, read_csr },

    /* Crash Debug CSRs */
    [CSR_MCRASH_STATESAVE]  = { "mcrash_statesave",  any, read_csr },
    [CSR_MSTATUS_CRASHSAVE] = { "mstatus_crashsave", any, read_csr },

    /* Memory CSRs */
    [CSR_MILMB]          = { "milmb",             any, read_csr, write_csr},
    [CSR_MDLMB]          = { "mdlmb",             any, read_csr, write_csr},
    [CSR_MECC_CODE]      = { "mecc_code",         any, read_csr,
                                                       write_mecc_code    },
    [CSR_MNVEC]          = { "mnvec",             any, read_csr, write_csr},
    [CSR_MCACHE_CTL]     = { "mcache_ctl",        any, read_csr,
                                                  write_mcache_ctl        },
    [CSR_MCCTLBEGINADDR] = { "mcctlbeginaddr",    any, read_csr, write_csr},
    [CSR_MCCTLCOMMAND]   = { "mcctlcommand",      any, read_csr, write_csr},
    [CSR_MCCTLDATA]      = { "mcctldata",         any, read_csr, write_csr},
    [CSR_MPPIB]          = { "mppib",             any, read_csr, write_csr},
    [CSR_MFIOB]          = { "mfiob",             any, read_csr, write_csr},

    /* Hardware Stack Protection & Recording */
    [CSR_MHSP_CTL]     = { "mhsp_ctl",            any, read_csr, write_csr},
    [CSR_MSP_BOUND]    = { "msp_bound",           any, read_csr, write_csr},
    [CSR_MSP_BASE]     = { "msp_base",            any, read_csr, write_csr},
    [CSR_MXSTATUS]     = { "mxstatus",            any, read_csr, write_csr},
    [CSR_MDCAUSE]      = { "mdcause",             any, read_csr, write_csr},
    [CSR_MSLIDELEG]    = { "mslideleg",           any, read_csr, write_csr},
    [CSR_MSAVESTATUS]  = { "msavestatus",         any, read_csr, write_csr},
    [CSR_MSAVEEPC1]    = { "msaveepc1",           any, read_csr, write_csr},
    [CSR_MSAVECAUSE1]  = { "msavecause1",         any, read_csr, write_csr},
    [CSR_MSAVEEPC2]    = { "msaveepc2",           any, read_csr, write_csr},
    [CSR_MSAVECAUSE2]  = { "msavecause2",         any, read_csr, write_csr},
    [CSR_MSAVEDCAUSE1] = { "msavedcause1",        any, read_csr, write_csr},
    [CSR_MSAVEDCAUSE2] = { "msavedcause2",        any, read_csr, write_csr},

    /* Control CSRs */
    [CSR_MPFT_CTL]  = { "mpft_ctl",               any, read_csr, write_csr},
    [CSR_MMISC_CTL] = { "mmisc_ctl",              any, read_csr, write_csr},
    [CSR_MCLK_CTL]  = { "mclk_ctl",               any, read_csr, write_csr},

    /* Counter related CSRs */
    [CSR_MCOUNTERWEN]    = { "mcounterwen",       any, read_csr, write_csr},
    [CSR_MCOUNTERINTEN]  = { "mcounterinten",     any, read_csr, write_csr},
    [CSR_MCOUNTERMASK_M] = { "mcountermask_m",    any, read_csr, write_csr},
    [CSR_MCOUNTERMASK_S] = { "mcountermask_s",    any, read_csr, write_csr},
    [CSR_MCOUNTERMASK_U] = { "mcountermask_u",    any, read_csr, write_csr},
    [CSR_MCOUNTEROVF]    = { "mcounterovf",       any, read_csr, write_csr},

    /* Enhanced CLIC CSRs */
    [CSR_MIRQ_ENTRY]   = { "mirq_entry",          any, read_csr, write_csr},
    [CSR_MINTSEL_JAL]  = { "mintsel_jal",         any, read_csr, write_csr},
    [CSR_PUSHMCAUSE]   = { "pushmcause",          any, read_csr, write_csr},
    [CSR_PUSHMEPC]     = { "pushmepc",            any, read_csr, write_csr},
    [CSR_PUSHMXSTATUS] = { "pushmxstatus",        any, read_csr, write_csr},

    /* Andes Physical Memory Attribute(PMA) CSRs */
    [CSR_PMACFG0]   = { "pmacfg0",                any, read_csr, write_csr},
    [CSR_PMACFG1]   = { "pmacfg1",                any, read_csr, write_csr},
    [CSR_PMACFG2]   = { "pmacfg2",                any, read_csr, write_csr},
    [CSR_PMACFG3]   = { "pmacfg3",                any, read_csr, write_csr},
    [CSR_PMAADDR0]  = { "pmaaddr0",               any, read_csr, write_csr},
    [CSR_PMAADDR1]  = { "pmaaddr1",               any, read_csr, write_csr},
    [CSR_PMAADDR2]  = { "pmaaddr2",               any, read_csr, write_csr},
    [CSR_PMAADDR3]  = { "pmaaddr3",               any, read_csr, write_csr},
    [CSR_PMAADDR4]  = { "pmaaddr4",               any, read_csr, write_csr},
    [CSR_PMAADDR5]  = { "pmaaddr5",               any, read_csr, write_csr},
    [CSR_PMAADDR6]  = { "pmaaddr6",               any, read_csr, write_csr},
    [CSR_PMAADDR7]  = { "pmaaddr7",               any, read_csr, write_csr},
    [CSR_PMAADDR8]  = { "pmaaddr8",               any, read_csr, write_csr},
    [CSR_PMAADDR9]  = { "pmaaddr9",               any, read_csr, write_csr},
    [CSR_PMAADDR10] = { "pmaaddr10",              any, read_csr, write_csr},
    [CSR_PMAADDR11] = { "pmaaddr11",              any, read_csr, write_csr},
    [CSR_PMAADDR12] = { "pmaaddr12",              any, read_csr, write_csr},
    [CSR_PMAADDR13] = { "pmaaddr13",              any, read_csr, write_csr},
    [CSR_PMAADDR14] = { "pmaaddr14",              any, read_csr, write_csr},
    [CSR_PMAADDR15] = { "pmaaddr15",              any, read_csr, write_csr},

    /* Debug/Trace Registers (shared with Debug Mode) */
    [CSR_TSELECT] = { "tselect",                  any, read_csr, write_csr},
    [CSR_TDATA1]  = { "tdata1",                   any, read_csr, write_csr},
    [CSR_TDATA2]  = { "tdata2",                   any, read_csr, write_csr},
    [CSR_TDATA3]  = { "tdata3",                   any, read_csr, write_csr},
    [CSR_TINFO]   = { "tinfo",                    any, read_csr, write_csr},

    /* ================ AndeStar V5 supervisor mode CSRs ================ */
    /* Supervisor trap registers */
    [CSR_SLIE]    = { "slie",                     any, read_csr, write_csr},
    [CSR_SLIP]    = { "slip",                     any, read_csr, write_csr},
    [CSR_SDCAUSE] = { "sdcause",                  any, read_csr, write_csr},

    /* Supervisor counter registers */
    [CSR_SCOUNTERINTEN]  = { "scounterinten",     any, read_csr, write_csr},
    [CSR_SCOUNTERMASK_M] = { "scountermask_m",    any, read_csr, write_csr},
    [CSR_SCOUNTERMASK_S] = { "scountermask_s",    any, read_csr, write_csr},
    [CSR_SCOUNTERMASK_U] = { "scountermask_u",    any, read_csr, write_csr},
    [CSR_SCOUNTEROVF]    = { "scounterovf",       any, read_csr, write_csr},
    [CSR_SCOUNTINHIBIT]  = { "scountinhibit",     any, read_csr, write_csr},
    [CSR_SHPMEVENT3]     = { "shpmevent3",        any, read_csr, write_csr},
    [CSR_SHPMEVENT4]     = { "shpmevent4",        any, read_csr, write_csr},
    [CSR_SHPMEVENT5]     = { "shpmevent5",        any, read_csr, write_csr},
    [CSR_SHPMEVENT6]     = { "shpmevent6",        any, read_csr, write_csr},

    /* Supervisor control registers */
    [CSR_SCCTLDATA] = { "scctldata",              any, read_csr, write_csr},
    [CSR_SMISC_CTL] = { "smisc_ctl",              any, read_csr, write_csr},

    /* =================== AndeStar V5 user mode CSRs =================== */
    /* User mode control registers */
    [CSR_UITB]           = { "uitb",              any, read_csr, write_uitb},
    [CSR_UCODE]          = { "ucode",             any, read_csr, write_csr},
    [CSR_UDCAUSE]        = { "udcause",           any, read_csr, write_csr},
    [CSR_UCCTLBEGINADDR] = { "ucctlbeginaddr",    any, read_csr, write_csr},
    [CSR_UCCTLCOMMAND]   = { "ucctlcommand",      any, read_csr, write_csr},
    [CSR_WFE]            = { "wfe",               any, read_csr, write_csr},
    [CSR_SLEEPVALUE]     = { "sleepvalue",        any, read_csr, write_csr},
    [CSR_TXEVT]          = { "csr_txevt",         any, read_csr, write_csr},
};
