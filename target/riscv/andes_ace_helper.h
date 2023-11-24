/*
 * Andes ACE helper header
 *
 * Copyright (c) 2023 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef __ANDES_ACE_HELPER__
#define __ANDES_ACE_HELPER__ 1
#include <stdint.h>
#include "qemu/qemu-print.h"
#include "ace-helper.h"

/* data access */
uint64_t qemu_get_XRF(CPURISCVState *env, uint32_t index);
void qemu_set_XRF(CPURISCVState *env, uint32_t index, uint64_t value);
uint64_t qemu_get_FRF(CPURISCVState *env, uint32_t index);
void qemu_set_FRF(CPURISCVState *env, uint32_t index, uint64_t value);
unsigned char *qemu_get_VRF(CPURISCVState *env, uint32_t index);
void qemu_set_VRF(CPURISCVState *env, uint32_t index, unsigned char *value);
uint64_t qemu_get_MEM(CPURISCVState *env, uint64_t vaddr, uint32_t size);
void qemu_set_MEM(CPURISCVState *env, uint64_t vaddr,
                  uint64_t value, uint32_t size);
uint64_t qemu_get_CSR(CPURISCVState *env, uint32_t index, uint64_t mask);
void qemu_set_CSR(CPURISCVState *env, uint32_t index,
                  uint64_t mask, uint64_t value);
void qemu_set_CSR_masked(CPURISCVState *env, uint32_t index, uint64_t value);

/* get memory without via mmu */
ACM_Status qemu_get_ACM(CPURISCVState *env, uint64_t addr,
                        uint32_t size, char *data);
ACM_Status qemu_set_ACM(CPURISCVState *env, uint64_t addr,
                        uint32_t size, char *data);

/* information access */
bool is_big_endian(void);
bool interruption_exist(void);

/* gdb target description */
enum Register_Type {
    NO_SPECIFY,
    DATA_POINTER,
    CODE_POINTER,
    FLOAT,
    DOUBLE,
    UINT8
};

/* for set/get CPU MSTATUS ACES bits */
uint32_t qemu_get_ACES(CPURISCVState *env);
void qemu_set_ACES(CPURISCVState *env, uint32_t value);

/*
 * for get the cause code of exception
 * the definition of cause code is in SPA spec 5.1.8
 */
uint64_t get_cause_code(void);

/* PC */
uint64_t qemu_get_PC(CPURISCVState *env);

/* hart ID */
uint64_t qemu_get_hart_id(CPURISCVState *env);

/* CPU Priv Mode */
uint32_t qemu_get_cpu_priv(CPURISCVState *env);

/* Execution ID */
uint64_t get_IS(void);

/* Exception is exist */
bool exception_exist(void);

int32_t qemu_ace_load_wrapper(const char *filename, target_ulong hartid);
int32_t qemu_ace_load_wrapper_symbol(const char *symbol_name,
                                     void **func_ptr, target_ulong hartid);
int32_t qemu_ace_agent_register(CPURISCVState *env,
                                const char *extlibpath, target_ulong hartid,
                                int32_t multi);
int32_t qemu_ace_agent_run_insn(CPURISCVState *env, uint32_t opcode,
                                target_ulong hartid);
typedef int32_t (*ace_agent_register_t)(void *, void *, uint32_t,
                                const char*, target_ulong, int32_t multi);
typedef int32_t (*ace_agent_run_insn_t)(void *, uint32_t insn_in,
                                        target_ulong);
typedef int32_t (*ace_agent_version_t)(void *);
typedef char* (*ace_agent_copilot_version_t)(void *, target_ulong);

#define ACE_HART_MAX    16

typedef struct ACEApis {
    /* dlopen ace wrapper handle */
    void *qemu_ace_wrapper_handle;
    /* register callback function */
    ace_agent_register_t fp_ace_agent_register;
    /* run_insn function */
    ace_agent_run_insn_t fp_ace_agent_run_insn;
    ace_agent_version_t fp_ace_agent_version;
    ace_agent_copilot_version_t fp_ace_agent_copilot_version;
} ACEApis;
#endif
