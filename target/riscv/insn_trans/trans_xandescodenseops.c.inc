/*
 * RISC-V translation routines for Andes CoDense
 *
 * Copyright (c) 2023 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "andes_cpu_bits.h"

#define ANDES_V5_GET_JAL_UIMM(inst) ((extract32(inst, 21, 10) << 1) \
                           | (extract32(inst, 20, 1) << 11) \
                           | (extract32(inst, 12, 8) << 12) \
                           | (extract32(inst, 31, 1) << 20))

/* If require CSR field(s) is not zero, return false */
#define REQUIRE_CFG_0(CSRNO, CSRFIELD) do {\
    RISCVException ret; \
    target_ulong value; \
    riscv_csr_operations ops; \
    riscv_get_csr_ops(CSRNO, &ops); \
    if (ops.read == NULL) { \
        return false; \
    } \
    ret = ops.read(cpu_env(ctx->cs), CSRNO, &value); \
    if (ret != RISCV_EXCP_NONE) \
        return false; \
    if (value & CSRFIELD) \
        return false; \
} while (0);


/* ECD = 1, ECDV = 0, PP16 = 0 */
#define REQUIRE_EXECIT do { \
    REQUIRE_CFG(CSR_MMSC_CFG, MASK_MMSC_CFG_ECD) \
    if (cpu_env(ctx->cs)->misa_mxl == MXL_RV32) { \
        REQUIRE_CFG_0(CSR_MMSC_CFG2, MASK_MMSC_CFG2_ECDV) \
        REQUIRE_CFG_0(CSR_MMSC_CFG2, MASK_MMSC_CFG2_PP16) \
    } else { \
        REQUIRE_CFG_0(CSR_MMSC_CFG, MASK_MMSC_CFG_ECDV) \
        REQUIRE_CFG_0(CSR_MMSC_CFG, MASK_MMSC_CFG_PP16) \
    } \
} while (0)

/*
 * This is assumed that CSR mmsc_cfg.ECDV are set
 * to non-zero(1,2,3) treat as NEXEC.IT, it may be different
 * with SPA spec, but should work now
 * ECD = 1, ECDV = 1, PP16 = 0
 */
#define REQUIRE_NEXECIT do { \
    REQUIRE_CFG(CSR_MMSC_CFG, MASK_MMSC_CFG_ECD) \
    if (cpu_env(ctx->cs)->misa_mxl == MXL_RV32) { \
        REQUIRE_CFG(CSR_MMSC_CFG2, MASK_MMSC_CFG2_ECDV) \
        REQUIRE_CFG_0(CSR_MMSC_CFG2, MASK_MMSC_CFG2_PP16) \
    } else { \
        REQUIRE_CFG(CSR_MMSC_CFG, MASK_MMSC_CFG_ECDV) \
        REQUIRE_CFG_0(CSR_MMSC_CFG, MASK_MMSC_CFG_PP16) \
    } \
} while (0)

static void decode_opc_andes(CPURISCVState *env,
                             DisasContext *ctx,
                             uint32_t opcode)
{
    /*
     * A table with predicate (i.e., guard) functions and decoder functions
     * that are tested in-order until a decoder matches onto the opcode.
     */
    static const struct {
        bool (*guard_func)(const RISCVCPUConfig *);
        bool (*decode_func)(DisasContext *, uint32_t);
    } decoders[] = {
        { always_true_p,  decode_insn32 },
        { has_xthead_p, decode_xthead },
        { has_XVentanaCondOps_p,  decode_XVentanaCodeOps },
        { has_XAndesV5Ops_p,  decode_XAndesV5Ops },
        { has_XAndesAce_p,  decode_XAndesAce },
    };

    static const struct {
        bool (*guard_func)(const RISCVCPUConfig *);
        bool (*decode_func)(DisasContext *, uint16_t);
    } decoders_16[] = {
        { always_true_p,  decode_insn16 },
        { has_XAndesCodenseOps_p, decode_XAndesCodenseOps },
    };

    /* Check for compressed insn */
    uint16_t opcode16 = opcode;
    if (extract16(opcode16, 0, 2) != 3) {
        if (!has_ext(ctx, RVC)) {
            gen_exception_illegal(ctx);
        } else {
            ctx->opcode = opcode16;

            for (size_t i = 0; i < ARRAY_SIZE(decoders_16); ++i) {
                if (decoders_16[i].guard_func(ctx->cfg_ptr) &&
                    decoders_16[i].decode_func(ctx, opcode16)) {
                    return;
                }
            }
        }
    } else {
        ctx->opcode = opcode;
        for (size_t i = 0; i < ARRAY_SIZE(decoders); ++i) {
            if (decoders[i].guard_func(ctx->cfg_ptr) &&
                decoders[i].decode_func(ctx, opcode)) {
                return;
            }
        }
    }

    gen_exception_illegal(ctx);
}

static bool andes_v5_gen_codense_exec_it(DisasContext *ctx,
                                        arg_execit *a)
{
    uint32_t insn;
    uint32_t imm_ex10 = a->imm_codense;
    CPURISCVState *env = cpu_env(ctx->cs);
    target_ulong uitb_val = 0;
    riscv_csrrw(env, CSR_UITB, &uitb_val, 0, 0);

    if (extract32(uitb_val, 0, 1)) { /* UTIB.HW == 1 */
        qemu_log_mask(LOG_GUEST_ERROR,
            "exec.it: UITB.HW == 1 is not supported by now!\n");
        gen_exception_illegal(ctx);
        uint32_t instruction_table[0];
        insn = instruction_table[imm_ex10 >> 2];
        return false;
    } else { /* UTIB.HW == 0 */
        target_ulong vaddr = (uitb_val & ~0x3) + (imm_ex10 << 2);
        insn = cpu_ldl_code(env, vaddr);
    }

    /*
     * Execute(insn)
     * do as the replaced instruction, even exceptions,
     * except ctx->pc_succ_insn value (2).
     */
    uint32_t op = MASK_OP_MAJOR(insn);
    if (op == OPC_RISC_JAL) {
        /* implement this by hack imm */
        /* Direct calculate rd and imm instead of decode_insn32_extract_j */
        int rd = GET_RD(insn);
        target_long imm = ANDES_V5_GET_JAL_UIMM(insn);
        target_ulong next_pc = (ctx->base.pc_next >> 21 << 21) | imm;
        imm = next_pc - ctx->base.pc_next;
        gen_jal(ctx, rd, imm);
    } else {
        /* JARL done as SPEC already */
        /* presume ctx->pc_succ_insn not changed in any ISA extension */
        decode_opc_andes(env, ctx, insn);
    }
    return true;
}

static bool trans_execit(DisasContext *ctx, arg_execit *a)
{
    REQUIRE_EXECIT;
    return andes_v5_gen_codense_exec_it(ctx, a);
}

static bool trans_nexecit(DisasContext *ctx, arg_nexecit *a)
{
    REQUIRE_NEXECIT;
    /* type casting arg_nexecit to arg_execit since they will be the same */
    return andes_v5_gen_codense_exec_it(ctx, (arg_execit *)a);
}
