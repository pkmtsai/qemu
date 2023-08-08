#ifndef RISCV_CSR_ANDES_INC_H
#define RISCV_CSR_ANDES_INC_H

#include "andes_cpu_bits.h"

#define ANDES_CSR_TABLE_SIZE 0x1000

typedef struct andes_csr {
    target_ulong csrno[ANDES_CSR_TABLE_SIZE];
} AndesCsr;

typedef struct AndesVec {
    int vectored_irq_m;
    int vectored_irq_s;
} AndesVec;

/* mmsc_cfg */
#define V5_MMSC_CFG_ECD                     3
#define V5_MMSC_CFG_PPMA                    30
#define V5_MMSC_CFG_L2C                     46
#define V5_MMSC_CFG2_L2C                    14

/* mmisc_ctl */
#define V5_MMISC_CTL_VEC_PLIC               1
#define V5_MMISC_CTL_RVCOMPM                2
#define V5_MMISC_CTL_BRPE                   3
#define V5_MMISC_CTL_MSA_OR_UNA             6
#define V5_MMISC_CTL_NON_BLOCKING           8

/* mcache_ctl */
#define V5_MCACHE_CTL_IC_FIRST_WORD         11
#define V5_MCACHE_CTL_DC_FIRST_WORD         12
#define V5_MCACHE_CTL_DC_COHEN              19
#define V5_MCACHE_CTL_DC_COHSTA             20

/* mmsc_cfg */
#define MASK_MMSC_CFG_ECC                   (0x01)
#define MASK_MMSC_CFG_TLB_ECC               (0x03 << 1)
#define MASK_MMSC_CFG_ECD                   (0x01 << 3)
#define MASK_MMSC_CFG_PFT                   (0x01 << 4)
#define MASK_MMSC_CFG_HSP                   (0x01 << 5)
#define MASK_MMSC_CFG_ACE                   (0x01 << 6)
#define MASK_MMSC_CFG_ADDPMC                (0x1F << 7)
#define MASK_MMSC_CFG_VPLIC                 (0x01 << 12)
#define MASK_MMSC_CFG_EV5PE                 (0x01 << 13)
#define MASK_MMSC_CFG_LMSLVP                (0x01 << 14)
#define MASK_MMSC_CFG_PMNDS                 (0x01 << 15)
#define MASK_MMSC_CFG_CCTLCSR               (0x01 << 16)
#define MASK_MMSC_CFG_EFHW                  (0x01 << 17)
#define MASK_MMSC_CFG_VCCTL                 (0x03 << 18)
#define MASK_MMSC_CFG_EXCSLVL               (0x03 << 20)
#define MASK_MMSC_CFG_NOPMC                 (0x01 << 22)
#define MASK_MMSC_CFG_SPE_AFT               (0x01 << 23)
#define MASK_MMSC_CFG_ESLEEP                (0x01 << 24)
#define MASK_MMSC_CFG_PPI                   (0x01 << 25)
#define MASK_MMSC_CFG_FIO                   (0x01 << 26)
#define MASK_MMSC_CFG_CLIC                  (0x01 << 27)
#define MASK_MMSC_CFG_ECLIC                 (0x01 << 28)
#define MASK_MMSC_CFG_EDSP                  (0x01 << 29)
#define MASK_MMSC_CFG_PPMA                  (0x01 << 30)
#define MASK_MMSC_CFG_MSC_EXT               (0x01 << 31)
/* for 64-bit mmsc_cfg */
#define MASK_MMSC_CFG_BF16CVT               ((uint64_t)0x01 << 32)
#define MASK_MMSC_CFG_ZFH                   ((uint64_t)0x01 << 33)
#define MASK_MMSC_CFG_VL4                   ((uint64_t)0x01 << 34)
#define MASK_MMSC_CFG_CRASHSAVE             ((uint64_t)0x01 << 35)
#define MASK_MMSC_CFG_VECCFG                ((uint64_t)0x01 << 36)
#define MASK_MMSC_CFG_FINV                  ((uint64_t)0x01 << 37)
#define MASK_MMSC_CFG_PP16                  ((uint64_t)0x01 << 38)
#define MASK_MMSC_CFG_VSIH                  ((uint64_t)0x01 << 40)
#define MASK_MMSC_CFG_ECDV                  ((uint64_t)0x03 << 41)
#define MASK_MMSC_CFG_VDOT                  ((uint64_t)0x01 << 43)
#define MASK_MMSC_CFG_VPFH                  ((uint64_t)0x01 << 44)
#define MASK_MMSC_CFG_CCACHEMP_CFG          ((uint64_t)0x01 << 45)
#define MASK_MMSC_CFG_CCACHE                ((uint64_t)0x01 << 46)
#define MASK_MMSC_CFG_IO_COHP               ((uint64_t)0x01 << 47)
#define MASK_MMSC_CFG_CORE_PCLUS            ((uint64_t)0x0F << 48)
#define MASK_MMSC_CFG_RVARCH                ((uint64_t)0x01 << 52)

/* mmsc_cfg_2 */
#define MASK_MMSC_CFG_2_BF16CVT             (0x01)
#define MASK_MMSC_CFG_2_ZFH                 (0x01 << 1)
#define MASK_MMSC_CFG_2_VL4                 (0x01 << 2)
#define MASK_MMSC_CFG_2_CRASHSAVE           (0x01 << 3)
#define MASK_MMSC_CFG_2_VECCFG              (0x01 << 4)
#define MASK_MMSC_CFG_2_FINV                (0x01 << 5)
#define MASK_MMSC_CFG_2_PP16                (0x01 << 6)
#define MASK_MMSC_CFG_2_VSIH                (0x01 << 8)
#define MASK_MMSC_CFG_2_ECDV                (0x03 << 9)
#define MASK_MMSC_CFG_2_VDOT                (0x01 << 11)
#define MASK_MMSC_CFG_2_VPFH                (0x01 << 12)
#define MASK_MMSC_CFG_2_CCACHEMP_CFG        (0x01 << 13)
#define MASK_MMSC_CFG_2_CCACHE              (0x01 << 14)
#define MASK_MMSC_CFG_2_IO_COHP             (0x01 << 15)
#define MASK_MMSC_CFG_2_CORE_PCLUS          (0x0F << 16)
#define MASK_MMSC_CFG_2_RVARCH              (0x01 << 20)

#define WRITE_MASK_CSR_MECC_CODE            0xFF
#define WRITE_MASK_CSR_UITB_32              0xFFFC
#define WRITE_MASK_CSR_UITB_64              0xFFFFFFFC
#define WRITE_MASK_CSR_MPFT_CTL             0x1F0
#define WRITE_MASK_CSR_MHSP_CTL             0x3F
#define WRITE_MASK_CSR_COUNTER_COMMON       0xFFFD

void andes_csr_init(AndesCsr *);
void andes_vec_init(AndesVec *);
void andes_cpu_do_interrupt_post(CPUState *cpu);

void andes_spec_csr_init_nx45v_meta(AndesCsr *);

#endif /* RISCV_CSR_ANDES_INC_H */
