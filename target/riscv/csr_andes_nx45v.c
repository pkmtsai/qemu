#include "qemu/osdep.h"
#include "cpu.h"
#include "csr_andes.h"

#define MASK_CSR_MECC_CODE_CODE  0xFF

static RISCVException write_mecc_code(CPURISCVState *env, int csrno,
                                      target_ulong val)
{
    // we only need to take care of CODE field, other fields are always zero.
    env->andes_csr.csrno[CSR_MECC_CODE] = val & MASK_CSR_MECC_CODE_CODE;
    return RISCV_EXCP_NONE;
}

void andes_spec_csr_init_nx45v(AndesCsr *andes_csr)
{
    andes_csr->csrno[CSR_MECC_CODE] = 0;

    andes_csr_ops[CSR_MECC_CODE].write = write_mecc_code;
}
