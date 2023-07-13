#ifndef __ANDES_ACE_HELPER__
#define __ANDES_ACE_HELPER__ 1
#include <stdint.h>
#include "qemu/qemu-print.h"
#include "ace-helper.h"

#define QEMU_ACE_AGENT_DEBUG 0 //0 or 1 to control debug message

#if 0
#define no_printf(fmt, ...)             \
({                          \
    if (0)                      \
        printf(fmt, ##__VA_ARGS__);     \
    0;                      \
})
#else
#define no_printf(fmt, ...) {}
#endif

#if QEMU_ACE_AGENT_DEBUG
    #define QAA_DEBUG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
    #define QAA_DEBUG(fmt, ...) no_printf(fmt, ##__VA_ARGS__)
#endif

#if 0
#ifndef __cplusplus
typedef enum { false, true } bool;
#endif
#endif
// data access
uint64_t qemu_get_XRF(CPURISCVState *env, uint32_t index);
void qemu_set_XRF(CPURISCVState *env, uint32_t index, uint64_t value);
uint64_t qemu_get_FRF(CPURISCVState *env, uint32_t index);
void qemu_set_FRF(CPURISCVState *env, uint32_t index, uint64_t value);
unsigned char *qemu_get_VRF(CPURISCVState *env, uint32_t index);
void qemu_set_VRF(CPURISCVState *env, uint32_t index, unsigned char *value);
uint64_t qemu_get_MEM(CPURISCVState *env, uint64_t vaddr, uint32_t size);
void qemu_set_MEM(CPURISCVState *env, uint64_t vaddr, uint64_t value, uint32_t size);
uint64_t qemu_get_CSR(CPURISCVState *env, uint32_t index, uint64_t mask);
void qemu_set_CSR(CPURISCVState *env, uint32_t index, uint64_t mask, uint64_t value);
void qemu_set_CSR_masked(CPURISCVState *env, uint32_t index, uint64_t value);

// get memory without via mmu
ACM_Status qemu_get_ACM(CPURISCVState *env, uint64_t addr, uint32_t size, char *data);
ACM_Status qemu_set_ACM(CPURISCVState *env, uint64_t addr, uint32_t size, char *data);

// information access
bool is_big_endian(void);
bool interruption_exist(void);

// gdb target description
enum Register_Type {
    NO_SPECIFY,
    DATA_POINTER,
    CODE_POINTER,
    FLOAT,
    DOUBLE,
    UINT8
};

// for set/get CPU MSTATUS ACES bits
uint32_t qemu_get_ACES(CPURISCVState *env);
void qemu_set_ACES(CPURISCVState *env, uint32_t value);

// for get the cause code of exception
// the definition of cause code is in SPA spec 5.1.8
uint64_t get_cause_code(void);

// PC
uint64_t qemu_get_PC(CPURISCVState *env);

// hart ID
uint64_t qemu_get_hart_id(CPURISCVState *env);

// CPU Priv Mode
uint32_t qemu_get_cpu_priv(CPURISCVState *env);

// Execution ID
uint64_t get_IS(void);

// Exception is exist
bool exception_exist(void);

int32_t load_qemu_ace_wrapper(const char *filename);
int32_t load_qemu_ace_wrapper_API(const char *symbol_name, void **func_ptr);
int32_t qemu_ace_agent_register(CPURISCVState *env);
int32_t qemu_ace_agent_run_insn(CPURISCVState *env, uint32_t opcode);
typedef int32_t (*ace_agent_register_t)(void *, void *, uint32_t);
typedef int32_t (*ace_agent_run_insn_t)(void *, uint32_t insn_in);


#endif
