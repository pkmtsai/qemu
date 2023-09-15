#define GP 3

enum andes_v5_inst_id {
    /* Code Dense Extension */
    EXEC_IT = (0x8000),

    /* V5 Performance Extension */
    /* custom 0 */
    ADDIGP = 0x01,
    LBGP = 0x00,
    LBUGP = 0x02,
    SBGP = 0x03,

    /* custom 1 */
    LHGP = 0x01,
    LHUGP = 0x05,
    LWGP = 0x02,
    LWUGP = 0x06,
    LDGP = 0x03,
    SHGP = 0x00,
    SWGP = 0x04,
    SDGP = 0x07,

    /* custom 2 */
    BBX = 0x07,
    BBX_BBC = 0x00,
    BBX_BBS = 0x01,
    BEQC = 0x05,
    BNEC = 0x06,
    BFOS = 0x03,
    BFOZ = 0x02,
    LEAF = 0x00,
    LEA_H = 0x05,
    LEA_W = 0x06,
    LEA_D = 0x07,
    LEA_B_ZE = 0x08,
    LEA_H_ZE = 0x09,
    LEA_W_ZE = 0x0a,
    LEA_D_ZE = 0x0b,
    F_FFB = 0x10,
    F_FFZMISM = 0x11,
    F_FFMISM = 0x12,
    F_FLMISM = 0x13,

    /* custom 3 */
    /* empty by now */
};

/*** Custom-2 Instructions ***/
static void andes_v5_gen_branch_tcgv(DisasContext *ctx, uint32_t opc,
                            TCGv source1, TCGv source2, target_long imm)
{
    TCGLabel *label = gen_new_label();

    switch (opc) {
    case OPC_RISC_BEQ:
        tcg_gen_brcond_tl(TCG_COND_EQ, source1, source2, label);
        break;
    case OPC_RISC_BNE:
        tcg_gen_brcond_tl(TCG_COND_NE, source1, source2, label);
        break;
    case OPC_RISC_BLT:
        tcg_gen_brcond_tl(TCG_COND_LT, source1, source2, label);
        break;
    case OPC_RISC_BGE:
        tcg_gen_brcond_tl(TCG_COND_GE, source1, source2, label);
        break;
    case OPC_RISC_BLTU:
        tcg_gen_brcond_tl(TCG_COND_LTU, source1, source2, label);
        break;
    case OPC_RISC_BGEU:
        tcg_gen_brcond_tl(TCG_COND_GEU, source1, source2, label);
        break;
    default:
        gen_exception_illegal(ctx);
        return;
    }

    gen_goto_tb(ctx, 1, ctx->cur_insn_len);
    gen_set_label(label); /* branch taken */
    if (!has_ext(ctx, RVC) && ((ctx->base.pc_next + imm) & 0x3)) {
        /* misaligned */
        TCGv target_pc = tcg_temp_new();
        gen_pc_plus_diff(target_pc, ctx, imm);
        gen_exception_inst_addr_mis(ctx, target_pc);
    } else {
        gen_goto_tb(ctx, 0, ctx->base.pc_next + imm);
    }
    ctx->base.is_jmp = DISAS_NORETURN;
}

static bool trans_bbx_bbc(DisasContext *ctx, arg_bbx_bbc *a)
{
    TCGv v0, v1, src;

    /* XXX: make it use tcg_constant_* instead */
    v0 = tcg_constant_tl(0);
    v1 = tcg_temp_new();
    src = get_gpr(ctx, a->rs1, EXT_NONE);
    tcg_gen_andi_tl(v1, src, (target_ulong)1u << a->cimms);
    andes_v5_gen_branch_tcgv(ctx, OPC_RISC_BEQ, v0, v1, a->imm);
    return true;
}

static bool trans_bbx_bbs(DisasContext *ctx, arg_bbx_bbs *a)
{
    TCGv v0, v1, src;
    v0 = tcg_constant_tl(0);
    v1 = tcg_temp_new();
    src = get_gpr(ctx, a->rs1, EXT_NONE);
    tcg_gen_andi_tl(v1, src, (target_ulong)1u << a->cimms);
    andes_v5_gen_branch_tcgv(ctx, OPC_RISC_BNE, v0, v1, a->imm);
    return true;
}

static bool trans_beqc(DisasContext *ctx, arg_beqc *a)
{
    TCGv v0, src;
    v0 = tcg_constant_tl(a->cimml);
    src = get_gpr(ctx, a->rs1, EXT_NONE);
    andes_v5_gen_branch_tcgv(ctx, OPC_RISC_BEQ, v0, src, a->imm);
    return true;
}

static bool trans_bnec(DisasContext *ctx, arg_bnec *a)
{
    TCGv v0, src;
    v0 = tcg_constant_tl(a->cimml);
    src = get_gpr(ctx, a->rs1, EXT_NONE);
    andes_v5_gen_branch_tcgv(ctx, OPC_RISC_BNE, v0, src, a->imm);
    return true;
}

static bool trans_bfos(DisasContext *ctx, arg_bfos *a)
{
    TCGv src, v1, opc;
    opc = tcg_constant_tl(ctx->opcode);
    src = get_gpr(ctx, a->rs1, EXT_NONE);
    v1 = get_gpr(ctx, a->rd, EXT_NONE);
    gen_helper_andes_v5_bfo_x(v1, v1, src, opc);
    return true;
}

static bool trans_bfoz(DisasContext *ctx, arg_bfoz *a)
{
    TCGv src, v1, opc;
    opc = tcg_constant_tl(ctx->opcode);
    src = get_gpr(ctx, a->rs1, EXT_NONE);
    v1 = get_gpr(ctx, a->rd,  EXT_NONE);
    gen_helper_andes_v5_bfo_x(v1, v1, src, opc);
    return true;
}

static bool translate_andes_lea(DisasContext *ctx, int rd,
                                int rs1, int rs2, int func)
{
    TCGv src1, src2, temp;

    temp = tcg_temp_new();
    src1 = get_gpr(ctx, rs1, EXT_NONE);
    src2 = get_gpr(ctx, rs2, EXT_NONE);

    switch (func) {
    case LEA_H: /* lea.h rd, rs1, rs2 */
    case LEA_W: /* lea.w rd, rs1, rs2 */
    case LEA_D: /* lea.d rd, rs1, rs2 */
        tcg_gen_shli_tl(temp, src2, func - LEA_H + 1);
        tcg_gen_add_tl(temp, src1, temp);
        break;
#if defined(TARGET_RISCV64)
    case LEA_B_ZE: /* lea.b.ze rd, rs1, rs2 */
    case LEA_H_ZE: /* lea.h.ze rd, rs1, rs2 */
    case LEA_W_ZE: /* lea.w.ze rd, rs1, rs2 */
    case LEA_D_ZE: /* lea.d.ze rd, rs1, rs2 */
        tcg_gen_ext32u_tl(temp, src2);
        tcg_gen_shli_tl(temp, temp, func - LEA_B_ZE);
        tcg_gen_add_tl(temp, src1, temp);
        break;
#endif
    default:
        /*
         * Seems to be an opcode collision,
         * fall through for official decoder
         */
        return false;
    }
    gen_set_gpr(ctx, rd, temp);

    return true;
}
static bool trans_lea_h(DisasContext *ctx, arg_lea_h *a)
{
    return translate_andes_lea(ctx, a->rd, a->rs1, a->rs2, LEA_H);
}
static bool trans_lea_w(DisasContext *ctx, arg_lea_w *a)
{
    return translate_andes_lea(ctx, a->rd, a->rs1, a->rs2, LEA_W);
}
static bool trans_lea_d(DisasContext *ctx, arg_lea_d *a)
{
    return translate_andes_lea(ctx, a->rd, a->rs1, a->rs2, LEA_D);
}
static bool trans_lea_b_ze(DisasContext *ctx, arg_lea_b_ze *a)
{
    return translate_andes_lea(ctx, a->rd, a->rs1, a->rs2, LEA_B_ZE);
}
static bool trans_lea_h_ze(DisasContext *ctx, arg_lea_h_ze *a)
{
    return translate_andes_lea(ctx, a->rd, a->rs1, a->rs2, LEA_H_ZE);
}
static bool trans_lea_w_ze(DisasContext *ctx, arg_lea_w_ze *a)
{
    return translate_andes_lea(ctx, a->rd, a->rs1, a->rs2, LEA_W_ZE);
}
static bool trans_lea_d_ze(DisasContext *ctx, arg_lea_d_ze *a)
{
    return translate_andes_lea(ctx, a->rd, a->rs1, a->rs2, LEA_D_ZE);
}

static bool trans_find_first_x(DisasContext *ctx, int rd,
                               int rs1, int rs2, int func)
{
    TCGv src1, src2, dest, tmp;
    src1 = get_gpr(ctx, rs1, EXT_NONE);
    src2 = get_gpr(ctx, rs2, EXT_NONE);
    dest = get_gpr(ctx, rd,  EXT_NONE);
    tmp = tcg_constant_tl(func);
    gen_helper_andes_v5_fb_x(dest, src1, src2, tmp);
    return true;
}
static bool trans_f_ffb(DisasContext *ctx, arg_f_ffb *a)
{
    return trans_find_first_x(ctx, a->rd, a->rs1, a->rs2, F_FFB);
}
static bool trans_f_ffzmism(DisasContext *ctx, arg_f_ffzmism *a)
{
    return trans_find_first_x(ctx, a->rd, a->rs1, a->rs2, F_FFZMISM);
}
static bool trans_f_ffmism(DisasContext *ctx, arg_f_ffmism *a)
{
    return trans_find_first_x(ctx, a->rd, a->rs1, a->rs2, F_FFMISM);
}
static bool trans_f_flmism(DisasContext *ctx, arg_f_flmism *a)
{
    return trans_find_first_x(ctx, a->rd, a->rs1, a->rs2, F_FLMISM);
}

/*** Custom-0 instructions ***/
static void gen_store_legacy(DisasContext *ctx, uint32_t opc, int rs1,
                             int rs2, target_long imm)
{
    TCGv t0, dat, base;
    int memop;
    const int tcg_memop_lookup_local[8] = {
        [0 ... 7] = -1,
        [0] = MO_SB,
        [1] = MO_TESW,
        [2] = MO_TESL,
        [4] = MO_UB,
        [5] = MO_TEUW,
    #ifdef TARGET_RISCV64
        [3] = MO_TESQ,
        [6] = MO_TEUL,
    #endif
    };

    memop = tcg_memop_lookup_local[(opc >> 12) & 0x7];

    if (memop < 0) {
        gen_exception_illegal(ctx);
        return;
    }

    t0 = tcg_temp_new();
    base = get_gpr(ctx, rs1, EXT_NONE);
    dat = get_gpr(ctx, rs2, EXT_NONE);
    tcg_gen_addi_tl(t0, base, imm);
    tcg_gen_qemu_st_tl(dat, t0, ctx->mem_idx, memop);
}

static void gen_load_legacy(DisasContext *ctx, uint32_t opc, int rd,
                            int rs1, target_long imm)
{
    TCGv t0, base, dat;
    int memop;
    const int tcg_memop_lookup_local[8] = {
        [0 ... 7] = -1,
        [0] = MO_SB,
        [1] = MO_TESW,
        [2] = MO_TESL,
        [4] = MO_UB,
        [5] = MO_TEUW,
    #ifdef TARGET_RISCV64
        [3] = MO_TEUQ,
        [6] = MO_TEUL,
    #endif
    };

    memop = tcg_memop_lookup_local[(opc >> 12) & 0x7];

    if (memop < 0) {
        gen_exception_illegal(ctx);
        return;
    }

    t0 = tcg_temp_new();
    base = get_gpr(ctx, rs1, EXT_NONE);
    dat = get_gpr(ctx, rd, EXT_NONE);
    tcg_gen_addi_tl(t0, base, imm);
    tcg_gen_qemu_ld_tl(dat, t0, ctx->mem_idx, memop);
}

static bool trans_addigp(DisasContext *ctx, arg_addigp *a)
{
    TCGv gp, v0;
    gp = get_gpr(ctx, GP, EXT_NONE);
    v0 = tcg_temp_new();
    tcg_gen_addi_tl(v0, gp, a->imm);
    gen_set_gpr(ctx, a->rd, v0);
    return true;
}

static bool trans_lbgp(DisasContext *ctx, arg_lbgp *a)
{
    gen_load_legacy(ctx, OPC_RISC_LB, a->rd, GP, a->imm);
    return true;
}
static bool trans_lbugp(DisasContext *ctx, arg_lbugp *a)
{
    gen_load_legacy(ctx, OPC_RISC_LBU, a->rd, GP, a->imm);
    return true;
}
static bool trans_sbgp(DisasContext *ctx, arg_sbgp *a)
{
    gen_store_legacy(ctx, OPC_RISC_SB, GP, a->rs2, a->imm);
    return true;
}

/*** Custom-1 instructions ***/
static bool trans_lhgp(DisasContext *ctx, arg_lhgp *a)
{
    gen_load_legacy(ctx, OPC_RISC_LH, a->rd, GP, a->imm_c1);
    return true;
}
static bool trans_lhugp(DisasContext *ctx, arg_lhugp *a)
{
    gen_load_legacy(ctx, OPC_RISC_LHU, a->rd, GP, a->imm_c1);
    return true;
}
static bool trans_lwgp(DisasContext *ctx, arg_lwgp *a)
{
    gen_load_legacy(ctx, OPC_RISC_LW, a->rd, GP, a->imm_c1);
    return true;
}
static bool trans_lwugp(DisasContext *ctx, arg_lwugp *a)
{
    gen_load_legacy(ctx, OPC_RISC_LWU, a->rd, GP, a->imm_c1);
    return true;
}
static bool trans_ldgp(DisasContext *ctx, arg_ldgp *a)
{
    gen_load_legacy(ctx, OPC_RISC_LD, a->rd, GP, a->imm_c1);
    return true;
}
static bool trans_shgp(DisasContext *ctx, arg_shgp *a)
{
    gen_store_legacy(ctx, OPC_RISC_SH, GP, a->rs2, a->imm_c1);
    return true;
}
static bool trans_swgp(DisasContext *ctx, arg_swgp *a)
{
    gen_store_legacy(ctx, OPC_RISC_SW, GP, a->rs2, a->imm_c1);
    return true;
}
static bool trans_sdgp(DisasContext *ctx, arg_sdgp *a)
{
    gen_store_legacy(ctx, OPC_RISC_SD, GP, a->rs2, a->imm_c1);
    return true;
}

