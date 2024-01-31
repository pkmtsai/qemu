/*
 * Andes ACE helper header
 *
 * Copyright (c) 2023 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ANDES_ACE_HELPER
#define ANDES_ACE_HELPER 1
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
AcmStatus qemu_get_ACM(CPURISCVState *env, uint64_t addr,
                        uint32_t size, char *data);
AcmStatus qemu_set_ACM(CPURISCVState *env, uint64_t addr,
                        uint32_t size, char *data);

/* for set/get CPU MSTATUS ACES bits */
uint32_t qemu_get_ACES(CPURISCVState *env);
void qemu_set_ACES(CPURISCVState *env, uint32_t value);

/* PC */
uint64_t qemu_get_PC(CPURISCVState *env);

/* hart ID */
uint64_t qemu_get_hart_id(CPURISCVState *env);

/* CPU Priv Mode */
uint32_t qemu_get_cpu_priv(CPURISCVState *env);

int32_t qemu_ace_agent_load(const char *filename);
int32_t qemu_ace_agent_load_symbol(const char *symbol_name, void **func_ptr);
int32_t qemu_ace_agent_register(CPURISCVState *env, const char *extlibpath,
                                int32_t multi);
int32_t qemu_ace_agent_run_insn(CPURISCVState *env, uint32_t opcode);
#endif
