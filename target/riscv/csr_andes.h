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

typedef void (*fp_spec_csr_init_fn)(AndesCsr *);

/* mmsc_cfg */
#define V5_MMSC_CFG_ECD                     3
#define V5_MMSC_CFG_LMSLVP                  14
#define V5_MMSC_CFG_CCTLCSR                 16
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

/* micm_cfg */
#define V5_MICM_CFG_ISZ                     6
#define V5_MICM_CFG_ILMB                    12
#define V5_MICM_CFG_ILMSZ                   15
#define MASK_MICM_CFG_ISET                  (0x07)
#define MASK_MICM_CFG_IWAY                  (0x07 << 3)
#define MASK_MICM_CFG_ISZ                   (0x07 << 6)
#define MASK_MICM_CFG_ILCK                  (0x01 << 9)
#define MASK_MICM_CFG_IC_ECC                (0x03 << 10)
#define MASK_MICM_CFG_ILMB                  (0x07 << 12)
#define MASK_MICM_CFG_ILMSZ                 (0x1F << 15)
#define MASK_MICM_CFG_ULM_2BANK             (0x01 << 20)
#define MASK_MICM_CFG_ILM_ECC               (0x03 << 21)
#define MASK_MICM_CFG_ILM_XONLY             (0x01 << 23)
#define MASK_MICM_CFG_SETH                  (0x01 << 24)
#define MASK_MICM_CFG_IC_REPL               (0x03 << 25)

/* mdcm_cfg */
#define V5_MDCM_CFG_DSZ                     6
#define V5_MDCM_CFG_DLMB                    12
#define V5_MDCM_CFG_DLMSZ                   15
#define MASK_MDCM_CFG_DSET                  (0x07)
#define MASK_MDCM_CFG_DWAY                  (0x07 << 3)
#define MASK_MDCM_CFG_DSZ                   (0x07 << 6)
#define MASK_MDCM_CFG_DLCK                  (0x01 << 9)
#define MASK_MDCM_CFG_DC_ECC                (0x03 << 10)
#define MASK_MDCM_CFG_DLMB                  (0x07 << 12)
#define MASK_MDCM_CFG_DLMSZ                 (0x1F << 15)
#define MASK_MDCM_CFG_ULM_2BANK             (0x01 << 20)
#define MASK_MDCM_CFG_DLM_ECC               (0x03 << 21)
#define MASK_MDCM_CFG_SETH                  (0x01 << 24)
#define MASK_MDCM_CFG_DC_REPL               (0x03 << 25)

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

/* mmsc_cfg2 */
#define MASK_MMSC_CFG2_BF16CVT              (0x01)
#define MASK_MMSC_CFG2_ZFH                  (0x01 << 1)
#define MASK_MMSC_CFG2_VL4                  (0x01 << 2)
#define MASK_MMSC_CFG2_CRASHSAVE            (0x01 << 3)
#define MASK_MMSC_CFG2_VECCFG               (0x01 << 4)
#define MASK_MMSC_CFG2_FINV                 (0x01 << 5)
#define MASK_MMSC_CFG2_PP16                 (0x01 << 6)
#define MASK_MMSC_CFG2_VSIH                 (0x01 << 8)
#define MASK_MMSC_CFG2_ECDV                 (0x03 << 9)
#define MASK_MMSC_CFG2_VDOT                 (0x01 << 11)
#define MASK_MMSC_CFG2_VPFH                 (0x01 << 12)
#define MASK_MMSC_CFG2_CCACHEMP_CFG         (0x01 << 13)
#define MASK_MMSC_CFG2_CCACHE               (0x01 << 14)
#define MASK_MMSC_CFG2_IO_COHP              (0x01 << 15)
#define MASK_MMSC_CFG2_CORE_PCLUS           (0x0F << 16)
#define MASK_MMSC_CFG2_RVARCH               (0x01 << 20)

/* mcause */
#define MASK_MCAUSE_EXCEPTION_CODE_32       (~MASK_MCAUSE_INTERRUPT_32)
#define MASK_MCAUSE_INTERRUPT_32            (1 << 31)
#define MASK_MCAUSE_EXCEPTION_CODE_64       (~MASK_MCAUSE_INTERRUPT_64)
#define MASK_MCAUSE_INTERRUPT_64            ((uint64_t)1 << 63)

/* mmisc_ctl */
#define MASK_MMISC_CTL_ACE                  (0x1)
#define MASK_MMISC_CTL_VEC_PLIC             (0x1 << 1)
#define MASK_MMISC_CTL_RVCOMPM              (0x1 << 2)
#define MASK_MMISC_CTL_BRPE                 (0x1 << 3)
#define MASK_MMISC_CTL_ACES                 (0x3 << 4)
#define MASK_MMISC_CTL_MSA_UNA              (0x1 << 6)
#define MASK_MMISC_CTL_NBLD_EN              (0x1 << 8)
#define MASK_MMISC_CTL_NEWNMI               (0x1 << 9)
#define MASK_MMISC_CTL_VCGL1_EN             (0x1 << 10)
#define MASK_MMISC_CTL_VCGL2_EN             (0x1 << 11)
#define MASK_MMISC_CTL_VCGL3_EN             (0x1 << 12)
#define MASK_MMISC_CTL_LDX0NXP              (0x1 << 13)

/* mrvarch_cfg */
#define MASK_MRVARCH_CFG_ZBA                (0x1)
#define MASK_MRVARCH_CFG_ZBB                (0x1 << 1)
#define MASK_MRVARCH_CFG_ZBC                (0x1 << 2)
#define MASK_MRVARCH_CFG_ZBS                (0x1 << 3)
#define MASK_MRVARCH_CFG_SMEPMP             (0x1 << 4)
#define MASK_MRVARCH_CFG_SVINVAL            (0x1 << 5)
#define MASK_MRVARCH_CFG_SMSTATEEN          (0x1 << 6)
#define MASK_MRVARCH_CFG_SSCOFPMF           (0x1 << 7)
#define MASK_MRVARCH_CFG_SSTC               (0x1 << 8)
#define MASK_MRVARCH_CFG_ZICBOM             (0x1 << 9)
#define MASK_MRVARCH_CFG_ZICBOP             (0x1 << 10)
#define MASK_MRVARCH_CFG_ZICBOZ             (0x1 << 11)
#define MASK_MRVARCH_CFG_ZBK                (0x1 << 12)
#define MASK_MRVARCH_CFG_ZKN                (0x1 << 13)
#define MASK_MRVARCH_CFG_ZKS                (0x1 << 14)
#define MASK_MRVARCH_CFG_ZKT                (0x1 << 15)
#define MASK_MRVARCH_CFG_ZKR                (0x1 << 16)
#define MASK_MRVARCH_CFG_SM_VERSION         (0x7 << 17)
#define MASK_MRVARCH_CFG_SS_VERSION         (0x7 << 20)
#define MASK_MRVARCH_CFG_SVPBMT             (0x1 << 23)
#define MASK_MRVARCH_CFG_SVNAPOT            (0x1 << 24)
#define MASK_MRVARCH_CFG_ZIHINTPAUSE        (0x1 << 25)

#define WRITE_MASK_CSR_MECC_CODE_32         0x7F
#define WRITE_MASK_CSR_MECC_CODE_64         0xFF
#define WRITE_MASK_CSR_UITB_32              0xFFFFFFFC
#define WRITE_MASK_CSR_UITB_64              0xFFFFFFFFFFFFFFFC
#define WRITE_MASK_CSR_MPFT_CTL             0x1F0
#define WRITE_MASK_CSR_MHSP_CTL             0x3F
#define WRITE_MASK_CSR_COUNTER              0x7D
#define WRITE_MASK_CSR_HPMEVENT             0x1FF
#define WRITE_MASK_CSR_SLIEP                0x10F0000
#define WRITE_MASK_CSR_MPPIB_32             0xFFFFFC3F
#define WRITE_MASK_CSR_MPPIB_64             0xFFFFFFFFFFFFFC3F
#define WRITE_MASK_CSR_MFIOB_32             0xFFFFFC3F
#define WRITE_MASK_CSR_MFIOB_64             0xFFFFFFFFFFFFFC3F
#define WRITE_MASK_CSR_UCODE                0x1
#define WRITE_MASK_CSR_MVEC_CFG             0x3FFFF
#define WRITE_MASK_CSR_MXSTATUS             0x3FF
#define WRITE_MASK_CSR_SMDCAUSE             0x7F
#define WRITE_MASK_CSR_MMISC_CTL            0x3F7F
#define WRITE_MASK_CSR_SMISC_CTL            0x30
#define WRITE_MASK_CSR_MCACHE_CTL           0x7FFFFF

void andes_csr_init(AndesCsr *);
void andes_vec_init(AndesVec *);
void andes_cpu_do_interrupt_post(CPUState *cpu);

void andes_spec_csr_init_nx45v(AndesCsr *);

#endif /* RISCV_CSR_ANDES_INC_H */