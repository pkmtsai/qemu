/*** ACE instructions ***/
#include "andes_ace_helper.c.inc"
static bool trans_andes_ace(DisasContext *ctx, arg_andes_ace *a)
{
    CPUState *cpu = ctx->cs;
    CPURISCVState *env = cpu->env_ptr;
    if (!env_archcpu(env)->cfg.ext_XAndesAce) {
        qemu_printf("XAndesAce not set\n");
        riscv_raise_exception(env, RISCV_EXCP_ILLEGAL_INST, GETPC());
    }
    /* We should check ACE whether is enabled by checking 
     * mmisc_ctl.ACES or mstatus.XS 
     */
    TCGv opc = tcg_constant_tl(ctx->opcode);
    gen_helper_andes_v5_ace(opc, cpu_env, opc);
    gen_update_pc(ctx, ctx->cur_insn_len);
    lookup_and_goto_ptr(ctx);

    return true;
}
