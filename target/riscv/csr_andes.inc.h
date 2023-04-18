#ifndef RISCV_CSR_ANDES_INC_H
#define RISCV_CSR_ANDES_INC_H

#include "andes_cpu_bits.h"

#define ANDES_CSR_TABLE_SIZE 0x1000

typedef struct andes_csr {
    target_long csrno[ANDES_CSR_TABLE_SIZE];
} AndesCsr;

typedef struct AndesVec {
    int vectored_irq_m;
    int vectored_irq_s;
} AndesVec;

/* mmsc_cfg */
#define V5_MMSC_CFG_ECD                     3
#define V5_MMSC_CFG_PPMA                    30

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


void andes_csr_init(AndesCsr *);
void andes_vec_init(AndesVec *);
void andes_cpu_do_interrupt_post(CPUState *cpu);

#endif /* RISCV_CSR_ANDES_INC_H */
