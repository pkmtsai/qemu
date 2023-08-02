#include "qemu/osdep.h"
#include "cpu.h"
#include "csr_andes.h"


void andes_spec_csr_init_nx45v(AndesCsr *andes_csr)
{
    andes_csr->csrno[CSR_MECC_CODE] = 0;
}