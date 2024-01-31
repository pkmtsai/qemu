/*
 * Andes ACE common header
 *
 * Copyright (c) 2023 Andes Technology Corp.
 */
#ifndef __ACE_HELPER_H__
#define __ACE_HELPER_H__

#ifdef __cplusplus
#include <cstdint>
#define EXPORT_C extern "C"
#else
#include <stdint.h>
#define EXPORT_C
#endif

/* define ACE agent version, just use int value */
#define ACE_AGENT_VERSION   100

enum ACE_CB_NAME {
    ACE_GET_XRF = 0,
    ACE_SET_XRF,
    ACE_GET_FRF,
    ACE_SET_FRF,
    ACE_GET_VRF,
    ACE_SET_VRF,
    ACE_GET_MEM,
    ACE_SET_MEM,
    ACE_GET_CSR,
    ACE_SET_CSR,
    ACE_SET_CSR_MASKED,
    ACE_GET_ACM,
    ACE_SET_ACM,
    ACE_GET_PC,
    ACE_GET_HARD_ID,
    ACE_GET_CPU_PRIV,
    ACE_GET_ACES,
    ACE_SET_ACES,
    ACE_CB_NAME_MAX
};

typedef int (*AceAgentFuncPtr)(void);

typedef enum {
    ACM_OK,
    ACM_ERROR,
} AcmStatus;

/* For Get PC/HART_ID/PRIV */
typedef uint64_t (*func0)(void *);
#define FUNC_0(f, p0) ((func0)(f))(p0)

/* For Get/Set XRF/FRF */
typedef uint64_t (*func1)(void *, uint32_t x);
typedef void (*func2)(void *, uint32_t x, uint64_t y);
#define FUNC_1(f, p0, p1) ((func1)(f))(p0, p1)
#define FUNC_2(f, p0, p1, p2) ((func2)(f))(p0, p1, p2)

/* For Get/Set CRS */
typedef uint64_t (*func2c)(void *, uint32_t x, uint64_t y);
typedef void (*func2cm)(void *, uint32_t x, uint64_t y);
typedef void (*func3c)(void *, uint32_t x, uint64_t y, uint64_t z);
#define FUNC_2C(f, p0, p1, p2) ((func2c)(f))(p0, p1, p2)
#define FUNC_2CM(f, p0, p1, p2) ((func2cm)(f))(p0, p1, p2)
#define FUNC_3C(f, p0, p1, p2, p3) ((func3c)(f))(p0, p1, p2, p3)

/* For Get/Set VRF */
typedef unsigned char* (*func1v)(void *, uint32_t x);
typedef void (*func2v)(void *, uint32_t x, unsigned char *y);
#define FUNC_1V(f, p0, p1) ((func1v)(f))(p0, p1)
#define FUNC_2V(f, p0, p1, p2) ((func2v)(f))(p0, p1, p2)

/* For Get/Set Mem */
typedef uint64_t (*func2m)(void *, uint64_t x, uint32_t y);
typedef void (*func3m)(void *, uint64_t x, uint64_t y, uint32_t z);
#define FUNC_2M(f, p0, p1, p2) ((func2m)(f))(p0, p1, p2)
#define FUNC_3M(f, p0, p1, p2, p3) ((func3m)(f))(p0, p1, p2, p3)

/* For Get/Set ACM */
typedef AcmStatus (*func3a)(void *, uint64_t x, uint32_t y, char *z);
#define FUNC_3A(f, p0, p1, p2, p3) ((func3a)(f))(p0, p1, p2, p3)

typedef int32_t (*AceAgentReg)(void *, void *, uint32_t,
                               const char*, uint64_t, int32_t);
typedef int32_t (*AceAgentRunInsn)(void *, uint32_t, uint64_t);
typedef int32_t (*AceAgentVersion)(void *);
typedef char* (*AceAgentCopilotVersion)(void *, uint64_t);
EXPORT_C int32_t ace_agent_register(void *, AceAgentFuncPtr *,
                                    uint32_t, const char *, uint64_t, int32_t);
EXPORT_C int32_t ace_agent_run_insn(void *, uint32_t, uint64_t);
EXPORT_C int32_t ace_agent_version(void *);
EXPORT_C const char *ace_agent_copilot_version(void *, uint64_t);
#endif
