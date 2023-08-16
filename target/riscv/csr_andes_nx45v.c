#include "qemu/osdep.h"
#include "cpu.h"
#include "csr_andes.h"


#define MASK_MMSC_CFG_L2CMP_CFG    ((uint64_t)0x1 << 45)
#define MASK_MMSC_CFG_L2C          ((uint64_t)0x1 << 46)
#define MASK_MMSC_CFG_IOCP         ((uint64_t)0x1 << 47)

static RISCVException read_mmsc_cfg_nx45v(CPURISCVState *env, int csrno,
                                          target_ulong *val)
{
    static uint64_t mask = MASK_MMSC_CFG_ECC     | MASK_MMSC_CFG_ECD
                         | MASK_MMSC_CFG_PFT     | MASK_MMSC_CFG_HSP
                         | MASK_MMSC_CFG_ACE     | MASK_MMSC_CFG_VPLIC
                         | MASK_MMSC_CFG_EV5PE   | MASK_MMSC_CFG_LMSLVP
                         | MASK_MMSC_CFG_PMNDS   | MASK_MMSC_CFG_CCTLCSR
                         | MASK_MMSC_CFG_VCCTL   | MASK_MMSC_CFG_NOPMC
                         | MASK_MMSC_CFG_EDSP    | MASK_MMSC_CFG_PPMA
                         | MASK_MMSC_CFG_BF16CVT | MASK_MMSC_CFG_ZFH
                         | MASK_MMSC_CFG_VL4     | MASK_MMSC_CFG_VECCFG
                         | MASK_MMSC_CFG_VSIH    | MASK_MMSC_CFG_VDOT
                         | MASK_MMSC_CFG_VPFH    | MASK_MMSC_CFG_L2CMP_CFG
                         | MASK_MMSC_CFG_L2C     | MASK_MMSC_CFG_IOCP
                         | MASK_MMSC_CFG_CORE_PCLUS
                         | MASK_MMSC_CFG_RVARCH;

    *val = env->andes_csr.csrno[CSR_MMSC_CFG] & (target_ulong)mask;
    return RISCV_EXCP_NONE;
}

void andes_spec_csr_init_nx45v(AndesCsr *andes_csr)
{
    uint64_t mmsc_init_val = MASK_MMSC_CFG_ECD   | MASK_MMSC_CFG_PFT
                           | MASK_MMSC_CFG_HSP   | MASK_MMSC_CFG_ACE
                           | MASK_MMSC_CFG_VPLIC | MASK_MMSC_CFG_EV5PE
                           | MASK_MMSC_CFG_PMNDS
                           | MASK_MMSC_CFG_CCTLCSR
                           | MASK_MMSC_CFG_PPMA  | MASK_MMSC_CFG_ZFH
                           | MASK_MMSC_CFG_VL4
                           | MASK_MMSC_CFG_VECCFG;
    andes_csr->csrno[CSR_MMSC_CFG] = (target_ulong)mmsc_init_val;

    andes_csr_ops[CSR_MMSC_CFG].read = read_mmsc_cfg_nx45v;
}