#include "andes_ace_helper.h"
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

generic_fp cb_tables[] = { 
    (generic_fp)qemu_get_XRF, (generic_fp)qemu_set_XRF,
    (generic_fp)qemu_get_FRF, (generic_fp)qemu_set_FRF,
    (generic_fp)qemu_get_VRF, (generic_fp)qemu_set_VRF,
    (generic_fp)qemu_get_MEM, (generic_fp)qemu_set_MEM,
    (generic_fp)qemu_get_CSR, (generic_fp)qemu_set_CSR,
    (generic_fp)qemu_set_CSR_masked,
    (generic_fp)qemu_get_ACM, (generic_fp)qemu_set_ACM,
    (generic_fp)qemu_get_PC,
    (generic_fp)qemu_get_hart_id,
    (generic_fp)qemu_get_cpu_priv,
    (generic_fp)qemu_get_ACES, (generic_fp)qemu_set_ACES,
    };

/* Below variable may move to cpu environment for 
 * multi-core support different ACE 
 */
/* dlopen ace wrapper handle */
static void *qemu_ace_wrapper_handle = NULL;
/* register callback function */
ace_agent_register_t fp_ace_agent_register = NULL;
/* run_insn function */
ace_agent_run_insn_t fp_ace_agent_run_insn = NULL;

int32_t load_qemu_ace_wrapper(const char *filename)
{
    /* Check whether handler already loaded? */
    if (qemu_ace_wrapper_handle != NULL) {
        return 0;
    }
    qemu_ace_wrapper_handle = dlopen(filename , RTLD_NOW|RTLD_DEEPBIND);
    if (!qemu_ace_wrapper_handle) {
        fprintf(stderr, "%s\n", dlerror());
        return -1;
    }
    QAA_DEBUG("Loaded %s library\n", filename);
    return 0;
}

int32_t load_qemu_ace_wrapper_API(const char *symbol_name, void **func_ptr)
{
    if (qemu_ace_wrapper_handle == NULL) {
        if ( load_qemu_ace_wrapper("./libqemuace.so") != 0 )
            return -1;
    }
    dlerror();    /* Clear any existing error */
    *(void **)(func_ptr) = dlsym(qemu_ace_wrapper_handle, symbol_name);
    if (*func_ptr == NULL) {
        return -1;
    }
    QAA_DEBUG("Loaded %s symbol successful\n", symbol_name);
    return 0;
}

int32_t qemu_ace_agent_register(CPURISCVState *env)
{
    if (fp_ace_agent_register == NULL) {
        if (load_qemu_ace_wrapper_API("ace_agent_register", 
            (void **)&fp_ace_agent_register) != 0) {
            QAA_DEBUG("get ace_agent_register symbol failed\n");
            return -1;
        }
    }
    /* register with direct function tables and also pass number of 
     * total callback functions
     */
    return fp_ace_agent_register(env, cb_tables, CB_NAME_MAX);
}

int32_t qemu_ace_agent_run_insn(CPURISCVState *env, uint32_t opcode)
{
    if (fp_ace_agent_run_insn == NULL) {
        if (load_qemu_ace_wrapper_API("ace_agent_run_insn", 
            (void **)&fp_ace_agent_run_insn) != 0) {
            QAA_DEBUG("get ace_agent_run_insn symbol failed\n");
            return -1;
        }
    }
    QAA_DEBUG("Call ace_agent_run_insn %0x\n", opcode);
    return fp_ace_agent_run_insn(env, opcode);
}

uint64_t qemu_get_XRF(CPURISCVState *env, uint32_t index)
{
    QAA_DEBUG("get_XRF %d\n", index);
    if (index == 0) {
        return 0;
    }
    QAA_DEBUG("get_XRF value %ld %p\n", env->gpr[index], &env->gpr[index]);
    /* Not support RV128 */
    return env->gpr[index];
}
void qemu_set_XRF(CPURISCVState *env, uint32_t index, uint64_t value)
{
    QAA_DEBUG("set_XRF %d %ld\n", index, value);
    if (index == 0) {
        return;
    }
    QAA_DEBUG("set_XRF old value %ld\n", env->gpr[index]);
    env->gpr[index] = value;
    QAA_DEBUG("set_XRF new value %ld\n", env->gpr[index]);
}
uint64_t qemu_get_FRF(CPURISCVState *env, uint32_t index)
{
    /* Should we check FP enable? */
    QAA_DEBUG("get_FRF %d\n", index);
    return env->fpr[index];
}
void qemu_set_FRF(CPURISCVState *env, uint32_t index, uint64_t value)
{
    /* Should we check FP enable? */
    QAA_DEBUG("set_FRF %d %ld\n", index, value);
    /* only updates mstatus, doesn't update ctx->mstatus_fs */
#if !defined(CONFIG_USER_ONLY)
    /* mstatus.FS doesn't disable (init/clean/dirty) */
    if (env->mstatus & MSTATUS_FS) {
        env->mstatus |= MSTATUS_FS;
    }
#endif
    env->fpr[index] = value;
} 
unsigned char *qemu_get_VRF(CPURISCVState *env, uint32_t index)
{
    /* Should we check Vector enable? */
    QAA_DEBUG("get_VRF %d\n", index);
    /* div 64, check cpu.h */
    uint16_t vlenq = env_archcpu(env)->cfg.vlen >> 6;
    QAA_DEBUG("vlenq=%d\n", env_archcpu(env)->cfg.vlen >> 6);
    /* get v register index pointer */
    uint64_t *vreg = &env->vreg[index*vlenq];
    return (unsigned char *)vreg;
}
void qemu_set_VRF(CPURISCVState *env, uint32_t index, unsigned char *value)
{
    /* Should we check Vector enable? */
    QAA_DEBUG("set_VRF %d %s\n", index, value);
    /* div 64, check cpu.h */
    uint16_t vlenq = env_archcpu(env)->cfg.vlen >> 6;
    QAA_DEBUG("vlenq=%d\n", env_archcpu(env)->cfg.vlen >> 6);
    /* only update mstatus, doesn't update ctx->mstatus_vs */
#if !defined(CONFIG_USER_ONLY)
    /* mstatus.VS doesn't disable (init/clean/dirty) */
    if (env->mstatus & MSTATUS_VS) {
        env->mstatus |= MSTATUS_VS;
    }
#endif
    /* vlenq is div 64, memcpy use byte size 8 */
    memcpy(&env->vreg[index*vlenq], value, vlenq*8);
}
/* get_MEM/set_MEM don't support non 1,2,4,8 length,
 * and return/set data always small than or equal to 8
 */
uint64_t qemu_get_MEM(CPURISCVState *env, uint64_t vaddr, uint32_t size)
{
    QAA_DEBUG("get_MEM %x:%x\n", vaddr, size);
    uint64_t data = 0;
    switch (size) {
        case 1:
#ifndef CONFIG_USER_ONLY
            data = cpu_ldub_data_ra(env, vaddr, GETPC());
#else
            data = cpu_ldub_data(env, vaddr);
#endif
            break;
        case 2:
#ifndef CONFIG_USER_ONLY
            data = cpu_lduw_data_ra(env, vaddr, GETPC());
#else
            data = cpu_lduw_data(env, vaddr);
#endif
            break;
        case 4:
#ifndef CONFIG_USER_ONLY
            data = cpu_ldl_data_ra(env, vaddr, GETPC());
#else
            data = cpu_ldl_data(env, vaddr);
#endif
            break;
        case 8:
#ifndef CONFIG_USER_ONLY
            data = cpu_ldq_data_ra(env, vaddr, GETPC());
#else
            data = cpu_ldq_data(env, vaddr);
#endif
            break;
        default: /* Unknown right size? */
            break;
    }
    return data;
}
void qemu_set_MEM(CPURISCVState *env, uint64_t vaddr,
                  uint64_t value, uint32_t size)
{
    QAA_DEBUG("set_MEM %x:%x\n", vaddr, size);
    switch (size) {
        case 1:
#ifndef CONFIG_USER_ONLY
            cpu_stb_data_ra(env, vaddr, value, GETPC());
#else
            cpu_stb_data(env, vaddr, value);
#endif
            break;
        case 2:
#ifndef CONFIG_USER_ONLY
            cpu_stw_data_ra(env, vaddr, value, GETPC());
#else
            cpu_stw_data(env, vaddr, value);
#endif
            break;
        case 4:
#ifndef CONFIG_USER_ONLY
            cpu_stl_data_ra(env, vaddr, value, GETPC());
#else
            cpu_stl_data(env, vaddr, value);
#endif
            break;
        case 8:
#ifndef CONFIG_USER_ONLY
            cpu_stq_data_ra(env, vaddr, value, GETPC());
#else
            cpu_stq_data(env, vaddr, value);
#endif
            break;
        default: /* Unknown right size? */
            break;
    }
}
uint64_t qemu_get_CSR(CPURISCVState *env, uint32_t index, uint64_t mask)
{
    QAA_DEBUG("get_CSR %x:%x\n", index, mask);
    target_ulong value;
    riscv_csr_operations ops;
    riscv_get_csr_ops(index , &ops);
    if (ops.read != NULL) {
        ops.read(env, index, &value);
        return (value & mask);
    }
    return 0;
}
void qemu_set_CSR(CPURISCVState *env, uint32_t index,
                  uint64_t mask, uint64_t value)
{
    QAA_DEBUG("set_CSR %x:%x:%x\n", index, mask, value);
    riscv_csr_operations ops;
    riscv_get_csr_ops(index , &ops);
    if (ops.read != NULL && ops.write != NULL) {
        target_ulong old;
        ops.read(env, index, &old);
        target_ulong new = (value & mask) | (old & ~mask);
        ops.write(env, index, new);
    }
}
void qemu_set_CSR_masked(CPURISCVState *env, uint32_t index, uint64_t value)
{
    QAA_DEBUG("set_CSR_masked %x:%x\n", index, value);
    riscv_csr_operations ops;
    riscv_get_csr_ops(index , &ops);
    if (ops.write != NULL) {
        ops.write(env, index, value);
    }
}

/* get memory without via mmu */
ACM_Status qemu_get_ACM(CPURISCVState *env, uint64_t addr,
                        uint32_t size, char *data)
{
    QAA_DEBUG("get_ACM %x:%x\n", addr, size);
#ifndef CONFIG_USER_ONLY
    cpu_physical_memory_read(addr, data, size);
#endif
    return ACM_OK;
}
ACM_Status qemu_set_ACM(CPURISCVState *env, uint64_t addr,
                        uint32_t size, char *data)
{
    QAA_DEBUG("set_ACM %x:%x\n", addr, size);
#ifndef CONFIG_USER_ONLY
    cpu_physical_memory_write(addr, data, size);
#endif
    return ACM_OK;
}

/* information access */
bool is_big_endian(void)
{
    QAA_DEBUG("is_big_endian\n");
    return 0;
}
bool interruption_exist(void)
{
    QAA_DEBUG("interruption_exist\n");
    return 0;
}

#define V5_MMISC_CTL_ACES 0x30 /* bit [5:4] */
uint32_t qemu_get_ACES(CPURISCVState *env)
{
    QAA_DEBUG("get_ACES\n");
    return env->andes_csr.csrno[CSR_MMISC_CTL] & V5_MMISC_CTL_ACES;
}
void qemu_set_ACES(CPURISCVState *env, uint32_t value)
{
    QAA_DEBUG("set_ACES=%x\n", value);
    uint32_t old = env->andes_csr.csrno[CSR_MMISC_CTL] & V5_MMISC_CTL_ACES;
    uint32_t new = (value & V5_MMISC_CTL_ACES) | (old & ~V5_MMISC_CTL_ACES);
    env->andes_csr.csrno[CSR_MMISC_CTL] = new;
}

uint64_t qemu_get_PC(CPURISCVState *env)
{
    QAA_DEBUG("get_PC\n");
    return env->pc;
}

uint64_t qemu_get_hart_id(CPURISCVState *env)
{
    QAA_DEBUG("get_hart_id\n");
#ifndef CONFIG_USER_ONLY
    return env->mhartid;
#else
    return 0;
#endif
}

uint32_t qemu_get_cpu_priv(CPURISCVState *env)
{
    QAA_DEBUG("get_cpu_priv\n");
#ifndef CONFIG_USER_ONLY
    return env->priv;
#else
    return 0;
#endif
}

