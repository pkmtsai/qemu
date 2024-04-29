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
#include "exec/cpu_ldst.h"
#include "andes_cpu_bits.h"
#include "csr_andes.h"
#include "exec/address-spaces.h"
#include "qemu/andes-config.h"

static RISCVException any(CPURISCVState *env,
                          int csrno)
{
    return RISCV_EXCP_NONE;
}

static RISCVException any32(CPURISCVState *env, int csrno)
{
    if (riscv_cpu_mxl(env) != MXL_RV32) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return any(env, csrno);
}

static RISCVException smode(CPURISCVState *env, int csrno)
{
    if (riscv_has_ext(env, RVS)) {
        return RISCV_EXCP_NONE;
    }

    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException mcfg2(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if ((csr->csrno[CSR_MMSC_CFG] & MASK_MMSC_CFG_MSC_EXT) > 0) {
        return any32(env, csrno);
    } else {
        return RISCV_EXCP_ILLEGAL_INST;
    }
}

static RISCVException mcfg3(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if (riscv_cpu_mxl(env) == MXL_RV32) {
        if ((csr->csrno[CSR_MMSC_CFG2] & MASK_MMSC_CFG2_MSC_EXT3) > 0) {
            return RISCV_EXCP_NONE;
        }
    } else {
        if ((csr->csrno[CSR_MMSC_CFG] & MASK_MMSC_CFG_MSC_EXT3) > 0) {
            return RISCV_EXCP_NONE;
        }
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException ecc(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if (get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_ECC) == 0) {
        return RISCV_EXCP_ILLEGAL_INST;
    } else {
        return RISCV_EXCP_NONE;
    }
}

static RISCVException ecd(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if (get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_ECD) == 0) {
        return RISCV_EXCP_ILLEGAL_INST;
    } else {
        return RISCV_EXCP_NONE;
    }
}

static RISCVException edsp(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if (get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_EDSP) == 0) {
        return RISCV_EXCP_ILLEGAL_INST;
    } else {
        return RISCV_EXCP_NONE;
    }
}

static RISCVException pft(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if (get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_PFT) == 0) {
        return RISCV_EXCP_ILLEGAL_INST;
    } else {
        return RISCV_EXCP_NONE;
    }
}

static RISCVException hsp(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if (get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_HSP) == 0) {
        return RISCV_EXCP_ILLEGAL_INST;
    } else {
        return RISCV_EXCP_NONE;
    }
}

static RISCVException ppi(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if (get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_PPI) == 0) {
        return RISCV_EXCP_ILLEGAL_INST;
    } else {
        return RISCV_EXCP_NONE;
    }
}

static RISCVException ppma(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if (get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_PPMA) == 0) {
        return RISCV_EXCP_ILLEGAL_INST;
    } else {
        if (csrno == CSR_PMACFG1 || csrno == CSR_PMACFG3) {
            if (riscv_cpu_mxl(env) != MXL_RV32) {
                return RISCV_EXCP_ILLEGAL_INST;
            }
        }
        return RISCV_EXCP_NONE;
    }
}

static RISCVException fio(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if (get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_FIO) == 0) {
        return RISCV_EXCP_ILLEGAL_INST;
    } else {
        return RISCV_EXCP_NONE;
    }
}

static RISCVException ilmb(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if (get_field(csr->csrno[CSR_MICM_CFG], MASK_MICM_CFG_ILMB) != 0) {
        return RISCV_EXCP_NONE;
    } else {
        return RISCV_EXCP_ILLEGAL_INST;
    }
}

static RISCVException dlmb(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if (get_field(csr->csrno[CSR_MDCM_CFG], MASK_MDCM_CFG_DLMB) != 0) {
        return RISCV_EXCP_NONE;
    } else {
        return RISCV_EXCP_ILLEGAL_INST;
    }
}

static RISCVException isz_dsz(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if (get_field(csr->csrno[CSR_MICM_CFG], MASK_MICM_CFG_ISZ) != 0) {
        return RISCV_EXCP_NONE;
    }
    if (get_field(csr->csrno[CSR_MDCM_CFG], MASK_MDCM_CFG_DSZ) != 0) {
        return RISCV_EXCP_NONE;
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException tlb_btb_ram_cmd(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    bool tlbbit = false;
    bool btbbit = false;

    if (riscv_cpu_mxl(env) == MXL_RV32) {
        if (mcfg2(env, csrno) == RISCV_EXCP_NONE) {
            if (get_field(csr->csrno[CSR_MMSC_CFG2],
                          MASK_MMSC_CFG2_TLB_RAM_CMD) == 1) {
                tlbbit = true;
            }
        }
    } else {
        if (get_field(csr->csrno[CSR_MMSC_CFG],
                      MASK_MMSC_CFG_TLB_RAM_CMD) == 1) {
            tlbbit = true;
        }
    }
    if (mcfg3(env, csrno) == RISCV_EXCP_NONE) {
        if (get_field(csr->csrno[CSR_MMSC_CFG3],
                      MASK_MMSC_CFG3_BTB_RAM_CMD) == 1) {
            btbbit = true;
        }
    }
    if (tlbbit || btbbit) {
        return RISCV_EXCP_NONE;
    } else {
        return RISCV_EXCP_ILLEGAL_INST;
    }
}

static RISCVException cctlcsr(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if (get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_CCTLCSR) == 1) {
        return RISCV_EXCP_NONE;
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException mcctl(CPURISCVState *env, int csrno)
{
    if (isz_dsz(env, csrno) == RISCV_EXCP_NONE
          || tlb_btb_ram_cmd(env, csrno) == RISCV_EXCP_NONE) {
        return cctlcsr(env, csrno);
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException ucctl(CPURISCVState *env, int csrno)
{
    if (riscv_has_ext(env, RVU)) {
        if (isz_dsz(env, csrno) == RISCV_EXCP_NONE
              || tlb_btb_ram_cmd(env, csrno) == RISCV_EXCP_NONE) {
            return cctlcsr(env, csrno);
        }
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException scctl(CPURISCVState *env, int csrno)
{
    if (riscv_has_ext(env, RVS)) {
        if (isz_dsz(env, csrno) == RISCV_EXCP_NONE
              || tlb_btb_ram_cmd(env, csrno) == RISCV_EXCP_NONE) {
            return cctlcsr(env, csrno);
        }
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException rvarch(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    bool rvarchbit;

    if (riscv_cpu_mxl(env) == MXL_RV32) {
        rvarchbit = get_field(csr->csrno[CSR_MMSC_CFG2], MASK_MMSC_CFG2_RVARCH);
    } else {
        rvarchbit = get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_RVARCH);
    }

    if (rvarchbit) {
        return RISCV_EXCP_NONE;
    } else {
        return RISCV_EXCP_ILLEGAL_INST;
    }
}

static RISCVException rvarch2(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    bool rvarch2bit;

    if (riscv_cpu_mxl(env) == MXL_RV32) {
        rvarch2bit = get_field(csr->csrno[CSR_MMSC_CFG2],
                               MASK_MMSC_CFG2_RVARCH2);
    } else {
        rvarch2bit = false;
    }

    if (rvarch2bit) {
        return RISCV_EXCP_NONE;
    } else {
        return RISCV_EXCP_ILLEGAL_INST;
    }
}

static RISCVException ccache(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    bool ccachebit;

    if (riscv_cpu_mxl(env) == MXL_RV32) {
        ccachebit = get_field(csr->csrno[CSR_MMSC_CFG2], MASK_MMSC_CFG2_CCACHE);
    } else {
        ccachebit = get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_CCACHE);
    }

    if (ccachebit) {
        return RISCV_EXCP_NONE;
    } else {
        return RISCV_EXCP_ILLEGAL_INST;
    }
}

static RISCVException veccfg(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    bool veccfgbit;

    if (riscv_has_ext(env, RVV)) {
        if (riscv_cpu_mxl(env) == MXL_RV32) {
            veccfgbit = get_field(csr->csrno[CSR_MMSC_CFG2],
                                  MASK_MMSC_CFG2_VECCFG);
        } else {
            veccfgbit = get_field(csr->csrno[CSR_MMSC_CFG],
                                  MASK_MMSC_CFG_VECCFG);
        }
        if (veccfgbit) {
            return RISCV_EXCP_NONE;
        }
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException bf16cvt(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    bool bf16bit;

    if (riscv_has_ext(env, RVV)) {
            return RISCV_EXCP_NONE;
    }
    if (riscv_cpu_mxl(env) == MXL_RV32) {
        bf16bit = get_field(csr->csrno[CSR_MMSC_CFG2],
                            MASK_MMSC_CFG2_BF16CVT);
    } else {
        bf16bit = get_field(csr->csrno[CSR_MMSC_CFG],
                                MASK_MMSC_CFG_BF16CVT);
    }
    if (bf16bit) {
        return RISCV_EXCP_NONE;
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException mmisc_ctl(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if (get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_ACE) == 1
          || get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_VPLIC) == 1
          || get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_ECD) == 1
          || get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_EV5PE) == 1) {
        return RISCV_EXCP_NONE;
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException smisc_ctl(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if (riscv_has_ext(env, RVS)) {
        if (get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_ACE) == 1) {
            return RISCV_EXCP_NONE;
        }
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException pmnds(CPURISCVState *env, int csrno)
{
    AndesCsr *csr = &env->andes_csr;
    if (get_field(csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_PMNDS) == 1) {
        return RISCV_EXCP_NONE;
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException pmnds_u(CPURISCVState *env, int csrno)
{
    if (riscv_has_ext(env, RVU)) {
        return pmnds(env, csrno);
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException pmnds_s(CPURISCVState *env, int csrno)
{
    if (riscv_has_ext(env, RVS)) {
        return pmnds(env, csrno);
    }
    return RISCV_EXCP_ILLEGAL_INST;
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
    env->andes_csr.csrno[csrno] = val & WRITE_MASK_CSR_MCACHE_CTL;
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
    switch (riscv_cpu_mxl(env)) {
    case MXL_RV32:
        env->andes_csr.csrno[CSR_MECC_CODE] = val & WRITE_MASK_CSR_MECC_CODE_32;
        return RISCV_EXCP_NONE;
        break;
    case MXL_RV64:
        env->andes_csr.csrno[CSR_MECC_CODE] = val & WRITE_MASK_CSR_MECC_CODE_64;
        return RISCV_EXCP_NONE;
        break;
    default:
        return RISCV_EXCP_ILLEGAL_INST;
    }
}

static RISCVException write_ucode(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    env->andes_csr.csrno[CSR_UCODE] = val & WRITE_MASK_CSR_UCODE;
    return RISCV_EXCP_NONE;
}

static RISCVException write_uitb(CPURISCVState *env, int csrno,
                                 target_ulong val)
{
    switch (riscv_cpu_mxl(env)) {
    case MXL_RV32:
        env->andes_csr.csrno[CSR_UITB] = val & WRITE_MASK_CSR_UITB_32;
        return RISCV_EXCP_NONE;
        break;
    case MXL_RV64:
        env->andes_csr.csrno[CSR_UITB] = val & WRITE_MASK_CSR_UITB_64;
        return RISCV_EXCP_NONE;
        break;
    default:
        return RISCV_EXCP_ILLEGAL_INST;
    }
}

static RISCVException write_mppib(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    switch (riscv_cpu_mxl(env)) {
    case MXL_RV32:
        env->andes_csr.csrno[CSR_MPPIB] = val & WRITE_MASK_CSR_MPPIB_32;
        return RISCV_EXCP_NONE;
        break;
    case MXL_RV64:
        env->andes_csr.csrno[CSR_MPPIB] = val & WRITE_MASK_CSR_MPPIB_64;
        return RISCV_EXCP_NONE;
        break;
    default:
        return RISCV_EXCP_ILLEGAL_INST;
    }
}

static RISCVException write_mfiob(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    switch (riscv_cpu_mxl(env)) {
    case MXL_RV32:
        env->andes_csr.csrno[CSR_MFIOB] = val & WRITE_MASK_CSR_MFIOB_32;
        return RISCV_EXCP_NONE;
        break;
    case MXL_RV64:
        env->andes_csr.csrno[CSR_MFIOB] = val & WRITE_MASK_CSR_MFIOB_64;
        return RISCV_EXCP_NONE;
        break;
    default:
        return RISCV_EXCP_ILLEGAL_INST;
    }
}

static RISCVException write_mpft_ctl(CPURISCVState *env, int csrno,
                                     target_ulong val)
{
    env->andes_csr.csrno[CSR_MPFT_CTL] = val & WRITE_MASK_CSR_MPFT_CTL;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mhsp_ctl(CPURISCVState *env, int csrno,
                                     target_ulong val)
{
    target_ulong wmask = MASK_MHSP_CTL_OVF_EN | MASK_MHSP_CTL_UDF_EN |
                         MASK_MHSP_CTL_SCHM;
    if (riscv_has_ext(env, RVU)) {
        wmask |= MASK_MHSP_CTL_U;
    }
    if (riscv_has_ext(env, RVS)) {
        wmask |= MASK_MHSP_CTL_S;
    }
    if (riscv_has_ext(env, RVM)) {
        wmask |= MASK_MHSP_CTL_M;
    }
    env->andes_csr.csrno[CSR_MHSP_CTL] = val & wmask;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mcounter(CPURISCVState *env, int csrno,
                                     target_ulong val)
{
    env->andes_csr.csrno[csrno] = val & WRITE_MASK_CSR_COUNTER;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mcounterovf(CPURISCVState *env, int csrno,
                                        target_ulong val)
{
    if (env_archcpu(env)->cfg.marchid == ANDES_CPUID_N25) {
        /* write 1 to clear a bit */
        env->andes_csr.csrno[csrno] &= ~val & WRITE_MASK_CSR_COUNTER;
    } else {
        /* write 0 to clear a bit */
        env->andes_csr.csrno[csrno] &= val & WRITE_MASK_CSR_COUNTER;
    }
    return RISCV_EXCP_NONE;
}

static RISCVException write_mxstatus(CPURISCVState *env, int csrno,
                                     target_ulong val)
{
    env->andes_csr.csrno[csrno] = val & WRITE_MASK_CSR_MXSTATUS;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mmisc_ctl(CPURISCVState *env, int csrno,
                                      target_ulong val)
{
    env->andes_csr.csrno[csrno] = val & WRITE_MASK_CSR_MMISC_CTL;
    return RISCV_EXCP_NONE;
}

static RISCVException write_smisc_ctl(CPURISCVState *env, int csrno,
                                      target_ulong val)
{
    env->andes_csr.csrno[csrno] = val & WRITE_MASK_CSR_SMISC_CTL;
    return RISCV_EXCP_NONE;
}

static RISCVException write_smdcause(CPURISCVState *env, int csrno,
                                     target_ulong val)
{
    bool interrupt = false;
    target_ulong mcause;
    csr_ops[CSR_MCAUSE].read(env, CSR_MCAUSE, &mcause);

    if (riscv_cpu_mxl(env) == MXL_RV32) {
        interrupt = get_field(mcause, MASK_MCAUSE_INTERRUPT_32);
    } else {
        interrupt = get_field(mcause, MASK_MCAUSE_INTERRUPT_64);
    }

    if (interrupt) {
        env->andes_csr.csrno[csrno] = val & WRITE_MASK_CSR_SMDCAUSE;
    } else {
        env->andes_csr.csrno[csrno] = val;
    }
    return RISCV_EXCP_NONE;
}

static RISCVException read_scounter(CPURISCVState *env, int csrno,
                                    target_ulong *val)
{
    int real_csrno;

    switch (csrno) {
    case CSR_SCOUNTERINTEN:
        real_csrno = CSR_MCOUNTERINTEN;
        break;
    case CSR_SCOUNTERMASK_M:
        real_csrno = CSR_MCOUNTERMASK_M;
        break;
    case CSR_SCOUNTERMASK_S:
        real_csrno = CSR_MCOUNTERMASK_S;
        break;
    case CSR_SCOUNTERMASK_U:
        real_csrno = CSR_MCOUNTERMASK_U;
        break;
    case CSR_SCOUNTEROVF:
        real_csrno = CSR_MCOUNTEROVF;
        break;
    default:
        return RISCV_EXCP_ILLEGAL_INST;
    }

    *val = env->andes_csr.csrno[real_csrno];
    return RISCV_EXCP_NONE;
}

static RISCVException write_scounter(CPURISCVState *env, int csrno,
                                     target_ulong val)
{
    int real_csrno;

    switch (csrno) {
    case CSR_SCOUNTERINTEN:
        real_csrno = CSR_MCOUNTERINTEN;
        break;
    case CSR_SCOUNTERMASK_M:
        real_csrno = CSR_MCOUNTERMASK_M;
        break;
    case CSR_SCOUNTERMASK_S:
        real_csrno = CSR_MCOUNTERMASK_S;
        break;
    case CSR_SCOUNTERMASK_U:
        real_csrno = CSR_MCOUNTERMASK_U;
        break;
    case CSR_SCOUNTEROVF:
        real_csrno = CSR_MCOUNTEROVF;
        break;
    default:
        return RISCV_EXCP_ILLEGAL_INST;
    }

    target_ulong wen = env->andes_csr.csrno[CSR_MCOUNTERWEN];
    if (wen == 0) {
        env->andes_csr.csrno[real_csrno] = val & WRITE_MASK_CSR_COUNTER;
    } else {
        for (int i = 0; i < RV_MAX_MHPMCOUNTERS; i++) {
            target_ulong mask = 1 << i;
            if (get_field(wen, mask) == 1) {
                target_long mval = get_field(val, mask);
                env->andes_csr.csrno[real_csrno] =
                    set_field(env->andes_csr.csrno[real_csrno], mask, mval);
            }
        }
    }
    return RISCV_EXCP_NONE;
}

static RISCVException read_scountinhibit(CPURISCVState *env, int csrno,
                                         target_ulong *val)
{
    return csr_ops[CSR_MCOUNTINHIBIT].read(env, csrno, val);
}

static RISCVException write_scountinhibit(CPURISCVState *env, int csrno,
                                          target_ulong val)
{
    target_ulong wen = env->andes_csr.csrno[CSR_MCOUNTERWEN];
    target_ulong new_val = val & WRITE_MASK_CSR_COUNTER;
    target_ulong read_val;

    if (wen == 0) {
            csr_ops[CSR_MCOUNTINHIBIT].write(env, CSR_MCOUNTINHIBIT, new_val);
    } else {
        csr_ops[CSR_MCOUNTINHIBIT].read(env, CSR_MCOUNTINHIBIT, &read_val);
        for (int i = 0; i < RV_MAX_MHPMCOUNTERS; i++) {
            target_ulong mask = 1 << i;
            if (get_field(wen, mask) == 1) {
                target_long mval = get_field(val, mask);
                read_val = set_field(read_val, mask, mval);
            }
        }
        csr_ops[CSR_MCOUNTINHIBIT].write(env, CSR_MCOUNTINHIBIT, read_val);
    }
    return RISCV_EXCP_NONE;
}

static RISCVException write_shpmevent(CPURISCVState *env, int csrno,
                                      target_ulong val)
{
    env->andes_csr.csrno[csrno] = val & WRITE_MASK_CSR_HPMEVENT;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mslideleg(CPURISCVState *env, int csrno,
                                      target_ulong val)
{
    env->andes_csr.csrno[csrno] = val & WRITE_MASK_CSR_SLIE;
    return RISCV_EXCP_NONE;
}

static RISCVException read_slix(CPURISCVState *env, int csrno,
                                target_ulong *val)
{
#ifndef CONFIG_USER_ONLY
    target_ulong deleg = env->andes_csr.csrno[CSR_MSLIDELEG];
    target_ulong slix = env->andes_csr.csrno[csrno];

    if (env->priv == PRV_M) {
        *val = slix;
    } else {
        *val = deleg & slix;
    }
#else
    *val = env->andes_csr.csrno[csrno];
#endif
    return RISCV_EXCP_NONE;
}

static RISCVException write_slix(CPURISCVState *env, int csrno,
                                 target_ulong val)
{
    target_ulong mask;

    if (csrno == CSR_SLIP) {
        mask = WRITE_MASK_CSR_SLIP;
    } else {
        mask = WRITE_MASK_CSR_SLIE;
    }

#ifndef CONFIG_USER_ONLY
    target_ulong deleg = env->andes_csr.csrno[CSR_MSLIDELEG];
    target_ulong slix = env->andes_csr.csrno[csrno];

    if (env->priv == PRV_M) {
        env->andes_csr.csrno[csrno] = val & mask;
    } else {
        deleg &= mask;
        slix = (slix & ~deleg) | (val & deleg);
        env->andes_csr.csrno[csrno] = slix;
    }
#else
    env->andes_csr.csrno[csrno] = val & mask;
#endif
    return RISCV_EXCP_NONE;
}

static RISCVException write_pmnds(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    if (csr_ops[CSR_MCOUNTERWEN].predicate(env, csrno) != RISCV_EXCP_NONE) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    target_ulong wen = env->andes_csr.csrno[CSR_MCOUNTERWEN];
    if (wen == 0) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    switch (csrno) {
    case CSR_CYCLE:
    case CSR_CYCLEH:
        if (wen & MASK_COUNTER_CY) {
            csr_ops[csrno].write(env, csrno, val);
            return RISCV_EXCP_NONE;
        }
        break;
    case CSR_INSTRET:
    case CSR_INSTRETH:
        if (wen & MASK_COUNTER_IR) {
            csr_ops[csrno].write(env, csrno, val);
            return RISCV_EXCP_NONE;
        }
        break;
    case CSR_HPMCOUNTER3:
    case CSR_HPMCOUNTER3H:
        if (wen & MASK_COUNTER_HPM3) {
            csr_ops[csrno].write(env, csrno, val);
            return RISCV_EXCP_NONE;
        }
        break;
    case CSR_HPMCOUNTER4:
    case CSR_HPMCOUNTER4H:
        if (wen & MASK_COUNTER_HPM4) {
            csr_ops[csrno].write(env, csrno, val);
            return RISCV_EXCP_NONE;
        }
        break;
    case CSR_HPMCOUNTER5:
    case CSR_HPMCOUNTER5H:
        if (wen & MASK_COUNTER_HPM5) {
            csr_ops[csrno].write(env, csrno, val);
            return RISCV_EXCP_NONE;
        }
        break;
    case CSR_HPMCOUNTER6:
    case CSR_HPMCOUNTER6H:
        if (wen & MASK_COUNTER_HPM6) {
            csr_ops[csrno].write(env, csrno, val);
            return RISCV_EXCP_NONE;
        }
        break;
    default:
        break;
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException write_all_ignore(CPURISCVState *env, int csrno,
                                       target_ulong val)
{
    return RISCV_EXCP_NONE;
}

#ifndef CONFIG_USER_ONLY
static RISCVException write_lmb(CPURISCVState *env,
                                int csrno,
                                target_ulong val)
{
    uint64_t enable = val & 0x1;
    bool ilm_mapped, dlm_mapped;
    bool locked = false;
    if (!qemu_mutex_iothread_locked()) {
        locked = true;
        qemu_mutex_lock_iothread();
    }
    if (csrno == CSR_MILMB) {
        ilm_mapped = memory_region_is_mapped(env->mask_ilm);
        if (enable && !ilm_mapped) {
            memory_region_add_subregion_overlap(env->cpu_as_root,
                                env->ilm_base, env->mask_ilm, 1);
        } else if (!enable && ilm_mapped) {
            memory_region_del_subregion(env->cpu_as_root, env->mask_ilm);
        }
        env->andes_csr.csrno[csrno] = env->ilm_base | (val & 0xf);
    }
    if (csrno == CSR_MDLMB) {
        dlm_mapped = memory_region_is_mapped(env->mask_dlm);
        if (enable && !dlm_mapped) {
            memory_region_add_subregion_overlap(env->cpu_as_root,
                                env->dlm_base, env->mask_dlm, 1);
        } else if (!enable && dlm_mapped) {
            memory_region_del_subregion(env->cpu_as_root, env->mask_dlm);
        }
        env->andes_csr.csrno[csrno] = env->dlm_base | (val & 0xf);
    }
    tlb_flush(env_cpu(env));
    if (locked) {
        qemu_mutex_unlock_iothread();
    }
    return RISCV_EXCP_NONE;
}
#endif

#ifndef CONFIG_USER_ONLY
typedef struct AndesCsrConfigInfo {
    AndesConfigType type;
    target_ulong mask;
    const char *key;
} AndesCsrConfigInfo;

AndesCsrConfigInfo csr_mmsc_cfg_map[] = {
    {CONFIG_BOOL,   MASK_MMSC_CFG_ECC, "ecc"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_ECD, "isa-codense"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_PFT, "powerbrake"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_HSP, "stack-protection"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_ACE, "isa-ace"}, /* spike uses aces? */
    {CONFIG_BOOL,   MASK_MMSC_CFG_VPLIC, "vectored-plic"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_EV5PE, NULL}, /* always set */
    {CONFIG_BOOL,   MASK_MMSC_CFG_LMSLVP, "slave-port"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_PMNDS, "pfm-nds"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_CCTLCSR, "cctl"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_EFHW, "isa-efhw"},
    {CONFIG_NUMBER, MASK_MMSC_CFG_VCCTL, "cctl-version"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_PPI, "ppi"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_EDSP, "isa-dsp"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_PPMA, "ppma"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_MSC_EXT, "msc-ext"},
#ifdef TARGET_RISCV64
    /* bits:[32] */
    {CONFIG_BOOL,   MASK_MMSC_CFG_BF16CVT, "isa-bf16cvt"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_ZFH, "isa-zfh"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_VL4, "isa-vl4"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_CRASHSAVE, "crash-save"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_VECCFG, "veccfg"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_PP16, "isa-pp16"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_VSIH, "isa-vsih"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_CCACHEMP_CFG, "mmsc-cfg-ccachemp-cfg"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_CCACHE, "mmsc-cfg-ccache"},
    {CONFIG_NUMBER, MASK_MMSC_CFG_ECDV, "codense-ver"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_VDOT, "isa-vdot"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_VPFH, "isa-vpfh"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_HSPO, "stack-protection-only"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_RVARCH, "mmsc-cfg-rvarch"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_ALT_FP_FMT, "alt-fp-fmt"},
    {CONFIG_BOOL,   MASK_MMSC_CFG_MSC_EXT3, "msc-ext3"},
#endif
};
AndesCsrConfigInfo csr_mmsc_cfg2_map[]  = {
    {CONFIG_BOOL,   MASK_MMSC_CFG2_BF16CVT, "isa-bf16cvt"},
    {CONFIG_BOOL,   MASK_MMSC_CFG2_ZFH, "isa-zfh"},
    {CONFIG_BOOL,   MASK_MMSC_CFG2_VL4, "isa-vl4"},
    {CONFIG_BOOL,   MASK_MMSC_CFG2_CRASHSAVE, "crash-save"},
    {CONFIG_BOOL,   MASK_MMSC_CFG2_VECCFG, "veccfg"},
    {CONFIG_BOOL,   MASK_MMSC_CFG2_PP16, "isa-pp16"},
    {CONFIG_BOOL,   MASK_MMSC_CFG2_VSIH, "isa-vsih"},
    {CONFIG_BOOL,   MASK_MMSC_CFG2_CCACHEMP_CFG, "mmsc-cfg-ccachemp-cfg"},
    {CONFIG_BOOL,   MASK_MMSC_CFG2_CCACHE, "mmsc-cfg-ccache"},
    {CONFIG_NUMBER, MASK_MMSC_CFG2_ECDV, "codense-ver"},
    {CONFIG_BOOL,   MASK_MMSC_CFG2_VDOT, "isa-vdot"},
    {CONFIG_BOOL,   MASK_MMSC_CFG2_VPFH, "isa-vpfh"},
    {CONFIG_BOOL,   MASK_MMSC_CFG2_HSPO, "stack-protection-only"},
    {CONFIG_BOOL,   MASK_MMSC_CFG2_RVARCH, "mmsc-cfg-rvarch"},
    {CONFIG_BOOL,   MASK_MMSC_CFG2_ALT_FP_FMT, "alt-fp-fmt"},
    {CONFIG_BOOL,   MASK_MMSC_CFG2_MSC_EXT3, "msc-ext3"},
};

AndesCsrConfigInfo csr_mvec_cfg_map[]  = {
    {CONFIG_NUMBER, MASK_MVEC_CFG_MISEW, "mvec-cfg-misew"},
    {CONFIG_NUMBER, MASK_MVEC_CFG_MFSEW, "mvec-cfg-mfsew"},
};

static andes_config_func *const andes_config_fns[2] = {
    andes_config_bool,
    andes_config_number
};


static void andes_csr_from_config(AndesCsrConfigInfo *map, uint32_t size,
    target_ulong *csr_val)
{
    target_ulong init_val = *csr_val;
    int i;
    for (i = 0; i < size; i++) {
        uint64_t val;
        if (andes_config_fns[map[i].type] != NULL
            && andes_config_fns[map[i].type](ANDES_CONFIG_ID_CPU,
            map[i].key, &val)) {
            init_val = set_field(init_val, map[i].mask, val);
        }
    }
    *csr_val = init_val;
}

void andes_csr_configs(CPURISCVState *env)
{
    /* Update mmsc_cfg */
    target_ulong mmsc_init_val = env->andes_csr.csrno[CSR_MMSC_CFG];
    andes_csr_from_config(csr_mmsc_cfg_map,
        sizeof(csr_mmsc_cfg_map) / sizeof(AndesCsrConfigInfo), &mmsc_init_val);
    env->andes_csr.csrno[CSR_MMSC_CFG] = mmsc_init_val;
    if (env_archcpu(env)->cfg.ext_XAndesAce) {
        env->andes_csr.csrno[CSR_MMSC_CFG] = set_field(
            env->andes_csr.csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_ACE, true);
    }
    /* Update mmsc_cfg2 for RV32 */
    if (riscv_cpu_mxl(env) == MXL_RV32) {
        target_ulong mmsc2_init_val = env->andes_csr.csrno[CSR_MMSC_CFG2];
        andes_csr_from_config(csr_mmsc_cfg2_map,
            sizeof(csr_mmsc_cfg2_map) / sizeof(AndesCsrConfigInfo),
            &mmsc2_init_val);
        env->andes_csr.csrno[CSR_MMSC_CFG2] = mmsc2_init_val;
    }
    /* Update mvev_cfg */
    target_ulong mvec_init_val = env->andes_csr.csrno[CSR_MVEC_CFG];
    andes_csr_from_config(csr_mvec_cfg_map,
        sizeof(csr_mvec_cfg_map) / sizeof(AndesCsrConfigInfo), &mvec_init_val);
    env->andes_csr.csrno[CSR_MVEC_CFG] = mvec_init_val;
}
#endif

void andes_csr_init(AndesCsr *andes_csr)
{
    int i;

    /* Register CSR read/write method */
    for (i = 0; i < CSR_TABLE_SIZE; i++) {
        if (andes_csr_ops[i].name != NULL) {
            riscv_set_csr_ops(i, &andes_csr_ops[i]);
        }
    }

    /* PMNDS write enable */
    if (get_field(andes_csr->csrno[CSR_MMSC_CFG], MASK_MMSC_CFG_PMNDS) == 1) {
        csr_ops[CSR_CYCLE].write = write_pmnds;
        csr_ops[CSR_CYCLEH].write = write_pmnds;
        csr_ops[CSR_INSTRET].write = write_pmnds;
        csr_ops[CSR_INSTRETH].write = write_pmnds;
        csr_ops[CSR_HPMCOUNTER3].write = write_pmnds;
        csr_ops[CSR_HPMCOUNTER3H].write = write_pmnds;
        csr_ops[CSR_HPMCOUNTER4].write = write_pmnds;
        csr_ops[CSR_HPMCOUNTER4H].write = write_pmnds;
        csr_ops[CSR_HPMCOUNTER5].write = write_pmnds;
        csr_ops[CSR_HPMCOUNTER5H].write = write_pmnds;
        csr_ops[CSR_HPMCOUNTER6].write = write_pmnds;
        csr_ops[CSR_HPMCOUNTER6H].write = write_pmnds;
    }
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

    /*
     * ToDo: Should check CSR mtvec spec for VEC_PLIC enabled for
     *       different mode configurations
     *       Also need to check csr mmisc_ctl for mtvec and stvec for
     *       interrupt pending conditions
     */
    if (csr->csrno[CSR_MMISC_CTL] & (1UL << V5_MMISC_CTL_VEC_PLIC)) {
        /*
         * Only in interrupt need to process pc with mtvec since
         * this case should get handler PC by mtvec with source ID
         */
        if (env->priv == PRV_M) {
            /* check the MSB of mcause */
            if (env->mcause >> ((sizeof(env->mcause) << 3) - 1)) {
                int irq_id = vec->vectored_irq_m;
                vec->vectored_irq_m = 0;
                target_ulong base = env->mtvec;
                env->pc = ((uint64_t)env->pc >> 32) << 32;
                env->pc |= cpu_ldl_data(env, base + (irq_id << 2));
                target_ulong cause = env->mcause & (((target_ulong)-1) >> 1);
                /*
                 * If in PLIC vector mode and is an external interrupt,
                 * set mcause to source ID
                 */
                if (cause == IRQ_M_EXT) {
                    env->mcause = irq_id;
                    riscv_cpu_update_mip(env, MIP_MEIP, 0);
                }
                return;
            }
        } else if (env->priv == PRV_M) {
            /* check the MSB of scause */
            if (env->scause >> ((sizeof(env->scause) << 3) - 1)) {
                int irq_id = vec->vectored_irq_s;
                vec->vectored_irq_s = 0;
                target_ulong base = env->stvec;
                env->pc = ((uint64_t)env->pc >> 32) << 32;
                env->pc |= cpu_ldl_data(env, base + (irq_id << 2));
                target_ulong cause = env->scause & (((target_ulong)-1) >> 1);
                /*
                 * If in PLIC vector mode and is an external interrupt,
                 * set scause to source ID
                 */
                if (cause == IRQ_S_EXT) {
                    env->scause = irq_id;
                    riscv_cpu_update_mip(env, MIP_SEIP, 0);
                }
                return;
            }
        }
    }
#endif
}

riscv_csr_operations andes_csr_ops[CSR_TABLE_SIZE] = {
    /* ================== AndeStar V5 machine mode CSRs ================== */
    /* Configuration Registers */
    [CSR_MICM_CFG]          = { "micm_cfg",          any,     read_csr },
    [CSR_MDCM_CFG]          = { "mdcm_cfg",          any,     read_csr },
    [CSR_MMSC_CFG]          = { "mmsc_cfg",          any,     read_csr },
    [CSR_MMSC_CFG2]         = { "mmsc_cfg2",         mcfg2,   read_csr },
    [CSR_MMSC_CFG3]         = { "mmsc_cfg3",         mcfg3,   read_csr },
    [CSR_MVEC_CFG]          = { "mvec_cfg",          veccfg,  read_csr },
    [CSR_MRVARCH_CFG]       = { "mrvarch_cfg",       rvarch,  read_csr },
    [CSR_MRVARCH_CFG2]      = { "mrvarch_cfg2",      rvarch2, read_csr },
    [CSR_MCCACHE_CTL_BASE]  = { "mccache_ctl_base",  ccache,  read_csr },

    /* Crash Debug CSRs */
    [CSR_MCRASH_STATESAVE]  = { "mcrash_statesave",  any, read_csr },
    [CSR_MSTATUS_CRASHSAVE] = { "mstatus_crashsave", any, read_csr },

    /* Memory CSRs */
#ifndef CONFIG_USER_ONLY
    [CSR_MILMB]          = { "milmb",             ilmb, read_csr, write_lmb },
    [CSR_MDLMB]          = { "mdlmb",             dlmb, read_csr, write_lmb },
#else
    [CSR_MILMB]          = { "milmb",             ilmb, read_csr, write_csr },
    [CSR_MDLMB]          = { "mdlmb",             dlmb, read_csr, write_csr },
#endif
    [CSR_MECC_CODE]      = { "mecc_code",         ecc, read_csr,
                                                       write_mecc_code       },
    [CSR_MNVEC]          = { "mnvec",             any, read_csr,
                                                       write_all_ignore      },
    [CSR_MCACHE_CTL]     = { "mcache_ctl",        isz_dsz, read_csr,
                                                           write_mcache_ctl  },
    [CSR_MCCTLBEGINADDR] = { "mcctlbeginaddr",    mcctl, read_csr, write_csr },
    [CSR_MCCTLCOMMAND]   = { "mcctlcommand",      mcctl, read_csr, write_csr },
    [CSR_MCCTLDATA]      = { "mcctldata",         mcctl, read_csr, write_csr },
    [CSR_MPPIB]          = { "mppib",             ppi, read_csr, write_mppib },
    [CSR_MFIOB]          = { "mfiob",             fio, read_csr, write_mfiob },

    /* Hardware Stack Protection & Recording */
    [CSR_MHSP_CTL]     = { "mhsp_ctl",            hsp, read_csr,
                                                       write_mhsp_ctl         },
    [CSR_MSP_BOUND]    = { "msp_bound",           hsp, read_csr, write_csr    },
    [CSR_MSP_BASE]     = { "msp_base",            hsp, read_csr, write_csr    },
    [CSR_MXSTATUS]     = { "mxstatus",            any, read_csr,
                                                       write_mxstatus         },
    [CSR_MDCAUSE]      = { "mdcause",             any, read_csr,
                                                       write_smdcause         },
    [CSR_MSLIDELEG]    = { "mslideleg",           any, read_csr,
                                                       write_mslideleg        },
    [CSR_MSAVESTATUS]  = { "msavestatus",         any, read_csr, write_csr    },
    [CSR_MSAVEEPC1]    = { "msaveepc1",           any, read_csr, write_csr    },
    [CSR_MSAVECAUSE1]  = { "msavecause1",         any, read_csr, write_csr    },
    [CSR_MSAVEEPC2]    = { "msaveepc2",           any, read_csr, write_csr    },
    [CSR_MSAVECAUSE2]  = { "msavecause2",         any, read_csr, write_csr    },
    [CSR_MSAVEDCAUSE1] = { "msavedcause1",        any, read_csr, write_csr    },
    [CSR_MSAVEDCAUSE2] = { "msavedcause2",        any, read_csr, write_csr    },

    /* Control CSRs */
    [CSR_MPFT_CTL]  = { "mpft_ctl",               pft, read_csr,
                                                       write_mpft_ctl         },
    [CSR_MMISC_CTL] = { "mmisc_ctl",              mmisc_ctl, read_csr,
                                                             write_mmisc_ctl  },
    [CSR_MCLK_CTL]  = { "mclk_ctl",               bf16cvt, read_csr, write_csr},

    /* Counter related CSRs */
    [CSR_MCOUNTERWEN]    = { "mcounterwen",       pmnds_u, read_csr,
                                                         write_mcounter   },
    [CSR_MCOUNTERINTEN]  = { "mcounterinten",     pmnds, read_csr,
                                                         write_mcounter   },
    [CSR_MCOUNTERMASK_M] = { "mcountermask_m",    pmnds_u, read_csr,
                                                         write_mcounter   },
    [CSR_MCOUNTERMASK_S] = { "mcountermask_s",    pmnds_s, read_csr,
                                                         write_mcounter   },
    [CSR_MCOUNTERMASK_U] = { "mcountermask_u",    pmnds_u, read_csr,
                                                         write_mcounter   },
    [CSR_MCOUNTEROVF]    = { "mcounterovf",       pmnds, read_csr,
                                                         write_mcounterovf},

    /* Enhanced CLIC CSRs */
    [CSR_MIRQ_ENTRY]   = { "mirq_entry",          any, read_csr, write_csr},
    [CSR_MINTSEL_JAL]  = { "mintsel_jal",         any, read_csr, write_csr},
    [CSR_PUSHMCAUSE]   = { "pushmcause",          any, read_csr, write_csr},
    [CSR_PUSHMEPC]     = { "pushmepc",            any, read_csr, write_csr},
    [CSR_PUSHMXSTATUS] = { "pushmxstatus",        any, read_csr, write_csr},

    /* Andes Physical Memory Attribute(PMA) CSRs */
    [CSR_PMACFG0]   = { "pmacfg0",                ppma, read_csr, write_csr},
    [CSR_PMACFG1]   = { "pmacfg1",                ppma, read_csr, write_csr},
    [CSR_PMACFG2]   = { "pmacfg2",                ppma, read_csr, write_csr},
    [CSR_PMACFG3]   = { "pmacfg3",                ppma, read_csr, write_csr},
    [CSR_PMAADDR0]  = { "pmaaddr0",               ppma, read_csr, write_csr},
    [CSR_PMAADDR1]  = { "pmaaddr1",               ppma, read_csr, write_csr},
    [CSR_PMAADDR2]  = { "pmaaddr2",               ppma, read_csr, write_csr},
    [CSR_PMAADDR3]  = { "pmaaddr3",               ppma, read_csr, write_csr},
    [CSR_PMAADDR4]  = { "pmaaddr4",               ppma, read_csr, write_csr},
    [CSR_PMAADDR5]  = { "pmaaddr5",               ppma, read_csr, write_csr},
    [CSR_PMAADDR6]  = { "pmaaddr6",               ppma, read_csr, write_csr},
    [CSR_PMAADDR7]  = { "pmaaddr7",               ppma, read_csr, write_csr},
    [CSR_PMAADDR8]  = { "pmaaddr8",               ppma, read_csr, write_csr},
    [CSR_PMAADDR9]  = { "pmaaddr9",               ppma, read_csr, write_csr},
    [CSR_PMAADDR10] = { "pmaaddr10",              ppma, read_csr, write_csr},
    [CSR_PMAADDR11] = { "pmaaddr11",              ppma, read_csr, write_csr},
    [CSR_PMAADDR12] = { "pmaaddr12",              ppma, read_csr, write_csr},
    [CSR_PMAADDR13] = { "pmaaddr13",              ppma, read_csr, write_csr},
    [CSR_PMAADDR14] = { "pmaaddr14",              ppma, read_csr, write_csr},
    [CSR_PMAADDR15] = { "pmaaddr15",              ppma, read_csr, write_csr},

    /* ================ AndeStar V5 supervisor mode CSRs ================ */
    /* Supervisor trap registers */
    [CSR_SLIE]    = { "slie",                     smode, read_slix, write_slix},
    [CSR_SLIP]    = { "slip",                     smode, read_slix, write_slix},
    [CSR_SDCAUSE] = { "sdcause",                  smode, read_csr,
                                                         write_smdcause       },

    /* Supervisor counter registers */
    [CSR_SCOUNTERINTEN]  = { "scounterinten",     pmnds_s, read_scounter,
                                                           write_scounter     },
    [CSR_SCOUNTERMASK_M] = { "scountermask_m",    pmnds_s, read_scounter,
                                                           write_scounter     },
    [CSR_SCOUNTERMASK_S] = { "scountermask_s",    pmnds_s, read_scounter,
                                                           write_scounter     },
    [CSR_SCOUNTERMASK_U] = { "scountermask_u",    pmnds_s, read_scounter,
                                                           write_scounter     },
    [CSR_SCOUNTEROVF]    = { "scounterovf",       pmnds_s, read_scounter,
                                                           write_scounter     },
    [CSR_SCOUNTINHIBIT]  = { "scountinhibit",     pmnds_s, read_scountinhibit,
                                                           write_scountinhibit},
    [CSR_SHPMEVENT3]     = { "shpmevent3",        pmnds_s, read_csr,
                                                           write_shpmevent    },
    [CSR_SHPMEVENT4]     = { "shpmevent4",        pmnds_s, read_csr,
                                                           write_shpmevent    },
    [CSR_SHPMEVENT5]     = { "shpmevent5",        pmnds_s, read_csr,
                                                           write_shpmevent    },
    [CSR_SHPMEVENT6]     = { "shpmevent6",        pmnds_s, read_csr,
                                                           write_shpmevent    },

    /* Supervisor control registers */
    [CSR_SCCTLDATA] = { "scctldata",              scctl,     read_csr,
                                                             write_csr        },
    [CSR_SMISC_CTL] = { "smisc_ctl",              smisc_ctl, read_csr,
                                                             write_smisc_ctl  },

    /* =================== AndeStar V5 user mode CSRs =================== */
    /* User mode control registers */
    [CSR_UITB]           = { "uitb",              ecd,   read_csr, write_uitb },
    [CSR_UCODE]          = { "ucode",             edsp,  read_csr,
                                                         write_ucode          },
    [CSR_UDCAUSE]        = { "udcause",           any,   read_csr, write_csr  },
    [CSR_UCCTLBEGINADDR] = { "ucctlbeginaddr",    ucctl, read_csr, write_csr  },
    [CSR_UCCTLCOMMAND]   = { "ucctlcommand",      ucctl, read_csr, write_csr  },
    [CSR_WFE]            = { "wfe",               any,   read_csr, write_csr  },
    [CSR_SLEEPVALUE]     = { "sleepvalue",        any,   read_csr, write_csr  },
    [CSR_TXEVT]          = { "csr_txevt",         any,   read_csr, write_csr  },
    [CSR_UMISC_CTL]      = { "umisc_ctl",         any,   read_csr, write_csr  },
};
