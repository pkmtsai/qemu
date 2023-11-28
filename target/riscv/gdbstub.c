/*
 * RISC-V GDB Server Stub
 *
 * Copyright (c) 2016-2017 Sagar Karandikar, sagark@eecs.berkeley.edu
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/qemu-print.h"
#include "exec/gdbstub.h"
#include "gdbstub/helpers.h"
#include "cpu.h"
#include "gdbstub/andes_ace_gdb.h"
#include "andes_ace_helper.h"

struct TypeSize {
    const char *gdb_type;
    const char *id;
    int size;
    const char suffix;
};

static const struct TypeSize vec_lanes[] = {
    /* quads */
    { "uint128", "quads", 128, 'q' },
    /* 64 bit */
    { "uint64", "longs", 64, 'l' },
    /* 32 bit */
    { "uint32", "words", 32, 'w' },
    /* 16 bit */
    { "uint16", "shorts", 16, 's' },
    /*
     * TODO: currently there is no reliable way of telling
     * if the remote gdb actually understands ieee_half so
     * we don't expose it in the target description for now.
     * { "ieee_half", 16, 'h', 'f' },
     */
    /* bytes */
    { "uint8", "bytes", 8, 'b' },
};

int riscv_cpu_gdb_read_register(CPUState *cs, GByteArray *mem_buf, int n)
{
    RISCVCPU *cpu = RISCV_CPU(cs);
    CPURISCVState *env = &cpu->env;
    target_ulong tmp;

    if (n < 32) {
        tmp = env->gpr[n];
    } else if (n == 32) {
        tmp = env->pc;
    } else {
        return 0;
    }

    switch (env->misa_mxl_max) {
    case MXL_RV32:
        return gdb_get_reg32(mem_buf, tmp);
    case MXL_RV64:
    case MXL_RV128:
        return gdb_get_reg64(mem_buf, tmp);
    default:
        g_assert_not_reached();
    }
    return 0;
}

int riscv_cpu_gdb_write_register(CPUState *cs, uint8_t *mem_buf, int n)
{
    RISCVCPU *cpu = RISCV_CPU(cs);
    CPURISCVState *env = &cpu->env;
    int length = 0;
    target_ulong tmp;

    switch (env->misa_mxl_max) {
    case MXL_RV32:
        tmp = (int32_t)ldl_p(mem_buf);
        length = 4;
        break;
    case MXL_RV64:
    case MXL_RV128:
        if (env->xl < MXL_RV64) {
            tmp = (int32_t)ldq_p(mem_buf);
        } else {
            tmp = ldq_p(mem_buf);
        }
        length = 8;
        break;
    default:
        g_assert_not_reached();
    }
    if (n > 0 && n < 32) {
        env->gpr[n] = tmp;
    } else if (n == 32) {
        env->pc = tmp;
    }

    return length;
}

static int riscv_gdb_get_fpu(CPURISCVState *env, GByteArray *buf, int n)
{
    if (n < 32) {
        if (env->misa_ext & RVD) {
            return gdb_get_reg64(buf, env->fpr[n]);
        }
        if (env->misa_ext & RVF) {
            return gdb_get_reg32(buf, env->fpr[n]);
        }
    }
    return 0;
}

static int riscv_gdb_set_fpu(CPURISCVState *env, uint8_t *mem_buf, int n)
{
    if (n < 32) {
        env->fpr[n] = ldq_p(mem_buf); /* always 64-bit */
        return sizeof(uint64_t);
    }
    return 0;
}

static int riscv_gdb_get_vector(CPURISCVState *env, GByteArray *buf, int n)
{
    uint16_t vlenb = riscv_cpu_cfg(env)->vlen >> 3;
    if (n < 32) {
        int i;
        int cnt = 0;
        for (i = 0; i < vlenb; i += 8) {
            cnt += gdb_get_reg64(buf,
                                 env->vreg[(n * vlenb + i) / 8]);
        }
        return cnt;
    }

    return 0;
}

static int riscv_gdb_set_vector(CPURISCVState *env, uint8_t *mem_buf, int n)
{
    uint16_t vlenb = riscv_cpu_cfg(env)->vlen >> 3;
    if (n < 32) {
        int i;
        for (i = 0; i < vlenb; i += 8) {
            env->vreg[(n * vlenb + i) / 8] = ldq_p(mem_buf + i);
        }
        return vlenb;
    }

    return 0;
}

static int riscv_gdb_get_csr(CPURISCVState *env, GByteArray *buf, int n)
{
    if (n < CSR_TABLE_SIZE) {
        target_ulong val = 0;
        int result;

        result = riscv_csrrw_debug(env, n, &val, 0, 0);
        if (result == RISCV_EXCP_NONE) {
            return gdb_get_regl(buf, val);
        }
    }
    return 0;
}

static int riscv_gdb_set_csr(CPURISCVState *env, uint8_t *mem_buf, int n)
{
    if (n < CSR_TABLE_SIZE) {
        target_ulong val = ldtul_p(mem_buf);
        int result;

        result = riscv_csrrw_debug(env, n, NULL, val, -1);
        if (result == RISCV_EXCP_NONE) {
            return sizeof(target_ulong);
        }
    }
    return 0;
}

static int riscv_gdb_get_virtual(CPURISCVState *cs, GByteArray *buf, int n)
{
    if (n == 0) {
#ifdef CONFIG_USER_ONLY
        return gdb_get_regl(buf, 0);
#else
        return gdb_get_regl(buf, cs->priv);
#endif
    }
    return 0;
}

static int riscv_gdb_set_virtual(CPURISCVState *cs, uint8_t *mem_buf, int n)
{
    if (n == 0) {
#ifndef CONFIG_USER_ONLY
        cs->priv = ldtul_p(mem_buf) & 0x3;
        if (cs->priv == PRV_RESERVED) {
            cs->priv = PRV_S;
        }
#endif
        return sizeof(target_ulong);
    }
    return 0;
}

static int riscv_gen_dynamic_csr_xml(CPUState *cs, int base_reg)
{
    RISCVCPU *cpu = RISCV_CPU(cs);
    CPURISCVState *env = &cpu->env;
    GString *s = g_string_new(NULL);
    riscv_csr_predicate_fn predicate;
    int bitsize = 16 << env->misa_mxl_max;
    int i;

#if !defined(CONFIG_USER_ONLY)
    env->debugger = true;
#endif

    /* Until gdb knows about 128-bit registers */
    if (bitsize > 64) {
        bitsize = 64;
    }

    g_string_printf(s, "<?xml version=\"1.0\"?>");
    g_string_append_printf(s, "<!DOCTYPE feature SYSTEM \"gdb-target.dtd\">");
    g_string_append_printf(s, "<feature name=\"org.gnu.gdb.riscv.csr\">");

    for (i = 0; i < CSR_TABLE_SIZE; i++) {
        if (env->priv_ver < csr_ops[i].min_priv_ver) {
            continue;
        }
        predicate = csr_ops[i].predicate;
        if (predicate && (predicate(env, i) == RISCV_EXCP_NONE)) {
            if (csr_ops[i].name) {
                g_string_append_printf(s, "<reg name=\"%s\"", csr_ops[i].name);
            } else {
                g_string_append_printf(s, "<reg name=\"csr%03x\"", i);
            }
            g_string_append_printf(s, " bitsize=\"%d\"", bitsize);
            g_string_append_printf(s, " regnum=\"%d\"/>", base_reg + i);
        }
    }

    g_string_append_printf(s, "</feature>");

    cpu->dyn_csr_xml = g_string_free(s, false);

#if !defined(CONFIG_USER_ONLY)
    env->debugger = false;
#endif

    return CSR_TABLE_SIZE;
}

unsigned char *acr_value;

static int nds_ace_get_reg(CPURISCVState *env, int reg_n)
{
    bool is_rv64 = (MXL_RV32 == env->misa_mxl_max) ? false : true;
    uint32_t reg_bytes = is_rv64 ? 8ul : 4ul;
    unsigned int ByteSize = 0;
    unsigned int reg_idx = 0;
    char *type_name = NULL;
    uint32_t curr_reg_no = 0;
    ACR_INFO_T_V5 *acr_info_list_v5 = global_acr_info_list_v5;

    /* Check ACR get function whether exist */
    if (gen_get_value_code == NULL) {
        return -1;
    }
    /*
     * Backup temp register (x5, x6, x7)
     * x5: high part
     * x6: low part
     * x7: ACM's address
     */
    target_ulong s0, s1, s2;
    s0 = env->gpr[5];
    s1 = env->gpr[6];
    s2 = env->gpr[7];

    /* Allocate acr value array and get acr info by ACR register number */
    for (unsigned int i = 0; i < *global_acr_type_count_v5; i++) {
        if ((acr_info_list_v5->num + curr_reg_no) >= reg_n + 1) {
            reg_idx = reg_n - curr_reg_no;
            type_name = acr_info_list_v5->name;
            unsigned int acr_width = acr_info_list_v5->width;
            ByteSize =
                (acr_width % 8 == 0) ? acr_width / 8 : (acr_width / 8) + 1;
            if (acr_value != NULL) {
                free(acr_value);
            }
            acr_value = malloc(ByteSize);
            break;
        }
        curr_reg_no += acr_info_list_v5->num;
        acr_info_list_v5++;
    }
    if (acr_value == NULL) {
        qemu_printf("Cannot allocate acr value memory '%s'\n", type_name);
        return -1;
    }
    unsigned char *value = acr_value;

    /* Generate code to read value from ACR/ACM */
    INSN_CODE_T_V5 *insn_code = gen_get_value_code(type_name, reg_idx);

    bool init_acm_addr = false;
    /* Execute the code generated by gen_get_value_code() iteratively */
    for (unsigned i = 0; i < insn_code->num; i++) {

        /*
         * For ACM utility instruction,
         * write memory address to GDB_REGNO_XPR0 + 7
         */
        if (init_acm_addr == false &&
                ((insn_code->code + i)->version == acm_io1 ||
                 (insn_code->code + i)->version == acm_io2)) {
            env->gpr[7] = reg_idx;
            init_acm_addr = true;
        }

        unsigned insn = (insn_code->code + i)->insn;

        /*
         * determine the number of GPRs used to read/write data from/to ACR/ACM
         */
        bool isTwoGPR = false;
        if ((insn_code->code + i)->version == acr_io2 ||
                (insn_code->code + i)->version == acm_io2) {
            isTwoGPR = true;
        }

        /* execute the code */
#ifdef CONFIG_USER_ONLY
        int exec_out = qemu_ace_agent_run_insn(env, insn, 0);
#else
        int exec_out = qemu_ace_agent_run_insn(env, insn, env->mhartid);
#endif
        if (exec_out != 0) {
            qemu_printf("Unable to execute ACE utility program\n");

            /* Restore temp register */
            env->gpr[5] = s0;
            env->gpr[6] = s1;
            env->gpr[7] = s2;
            return -1;
        }

        /* read value from program buffer */
        if (isTwoGPR == false) {
            target_ulong reg_value;
            reg_value = env->gpr[5];
            memcpy(value, &reg_value, reg_bytes);
            value += reg_bytes;
        } else {
            target_ulong high = 0, low = 0;
            high = env->gpr[5];
            low = env->gpr[6];
            memcpy(value, &low, reg_bytes);
            value += reg_bytes;
            memcpy(value, &high, reg_bytes);
            value += reg_bytes;
        }
    }

    /* Restore temp register */
    env->gpr[5] = s0;
    env->gpr[6] = s1;
    env->gpr[7] = s2;

    return ByteSize;
}

static int nds_ace_set_reg(CPURISCVState *env, unsigned char *val, int reg_n)
{
    bool is_rv64 = (MXL_RV32 == env->misa_mxl_max) ? false : true;
    uint32_t reg_bytes = is_rv64 ? 8ul : 4ul;
    unsigned int ByteSize = 0;
    unsigned int reg_idx = 0;
    char *type_name = NULL;
    uint32_t curr_reg_no = 0;
    ACR_INFO_T_V5 *acr_info_list_v5 = global_acr_info_list_v5;

    /* Check ACR get function whether exist */
    if (gen_set_value_code == NULL) {
        return -1;
    }
    /*
     * Backup temp register (x5, x6, x7)
     * x5: high part
     * x6: low part
     * x7: ACM's address
     */
    target_ulong s0, s1, s2;
    s0 = env->gpr[5];
    s1 = env->gpr[6];
    s2 = env->gpr[7];

    /* Allocate acr value array and get acr info by ACR register number */
    for (unsigned int i = 0; i < *global_acr_type_count_v5; i++) {
        if ((acr_info_list_v5->num + curr_reg_no) >= reg_n + 1) {
            reg_idx = reg_n - curr_reg_no;
            type_name = acr_info_list_v5->name;
            unsigned int acr_width = acr_info_list_v5->width;
            ByteSize =
                (acr_width % 8 == 0) ? acr_width / 8 : (acr_width / 8) + 1;
            break;
        }
        curr_reg_no += acr_info_list_v5->num;
        acr_info_list_v5++;
    }
    if (type_name == NULL) {
        qemu_printf("Cannot find acr reg no %d\n", reg_n);
        return -1;
    }

    /* Allocate buffer which is the multiple of register size */
    unsigned int rounds = (ByteSize % reg_bytes) ?
                           ByteSize / reg_bytes + 1 : ByteSize / reg_bytes;
    char *buffer = (char *)alloca(rounds * reg_bytes);
    memset(buffer, 0, rounds * reg_bytes);
    memcpy(buffer, val, ByteSize);
    /* this pointer will be increased when extracting value partially */
    char *value = buffer;

    /* Generate code to write value to ACR/ACM */
    INSN_CODE_T_V5 *insn_code = gen_set_value_code(type_name, reg_idx);

    bool init_acm_addr = false;
    /* Execute the code generated by gen_set_value_code() iteratively */
    for (unsigned i = 0; i < insn_code->num; i++) {
        /*
         * determine the number of GPRs used to read/write data from/to ACR/ACM
         */
        bool isTwoGPR = false;
        if ((insn_code->code + i)->version == acr_io2 ||
                (insn_code->code + i)->version == acm_io2) {
            isTwoGPR = true;
        }

        /*
         * For ACM utility instruction,
         * write memory address to GDB_REGNO_XPR0 + 7
         */
        if (init_acm_addr == false &&
                ((insn_code->code + i)->version == acm_io1 ||
                 (insn_code->code + i)->version == acm_io2)) {
            env->gpr[7] = reg_idx;
            init_acm_addr = true;
        }

        /* write given value string to S0/1 */
        if (isTwoGPR == false) {
            /* Extract part of value from given value string */
            target_ulong reg_value = 0;
            memcpy(&reg_value, value, reg_bytes);
            value += reg_bytes;
            env->gpr[5] = reg_value;
        } else {
            /*
             * Extract part of value from given value string
             * [NOTE] the order of val is from low bit order
             *        e.g., for value of 0x111222333444555666777888999
             *        the traversing order is from 999 --> 888 --> ...
             */
            target_ulong high = 0, low = 0;
            memcpy(&low, value, reg_bytes);
            value += reg_bytes;
            memcpy(&high, value, reg_bytes);
            value += reg_bytes;
            env->gpr[5] = high;
            env->gpr[6] = low;
        }

        unsigned insn = (insn_code->code + i)->insn;

        /* execute the code */
#ifdef CONFIG_USER_ONLY
        int exec_out = qemu_ace_agent_run_insn(env, insn, 0);
#else
        int exec_out = qemu_ace_agent_run_insn(env, insn, env->mhartid);
#endif
        if (exec_out != 0) {
            qemu_printf("Unable to execute program\n");
            /* Restore temp register */
            env->gpr[5] = s0;
            env->gpr[6] = s1;
            env->gpr[7] = s2;
            return -1;
        }

    }

    /* Restore temp register */
    env->gpr[5] = s0;
    env->gpr[6] = s1;
    env->gpr[7] = s2;

    return ByteSize;
}

static int riscv_gdb_get_ace(CPURISCVState *env, GByteArray *buf, int n)
{
    unsigned total_bytes = nds_ace_get_reg(env, n);
    if (total_bytes != -1) {
        g_byte_array_append(buf, acr_value, total_bytes);
        return total_bytes;
    }
    return 0;
}

static int riscv_gdb_set_ace(CPURISCVState *env, uint8_t *mem_buf, int n)
{
    unsigned total_bytes = nds_ace_set_reg(env, mem_buf, n);
    if (total_bytes != -1) {
        return total_bytes;
    }
    return 0;
}

static int riscv_gen_dynamic_ace_xml(CPUState *cs, int base_reg)
{
    RISCVCPU *cpu = RISCV_CPU(cs);
#if !defined(CONFIG_USER_ONLY)
    CPURISCVState *env = &cpu->env;
#endif
    GString *s = g_string_new(NULL);
    int num_regs = 0;
    ACR_INFO_T_V5 *acr_info_list_v5 = global_acr_info_list_v5;

    if (global_acr_info_list_v5 == NULL) {
        return 0;
    }

#if !defined(CONFIG_USER_ONLY)
    env->debugger = true;
#endif

    g_string_printf(s, "<?xml version=\"1.0\"?>");
    g_string_append_printf(s, "<!DOCTYPE feature SYSTEM \"gdb-target.dtd\">");
    g_string_append_printf(s, "<feature name=\"org.gnu.gdb.riscv.ace\">");

    for (unsigned int i = 0; i < *global_acr_type_count_v5; i++) {
        unsigned int acr_number = acr_info_list_v5->num;
        unsigned int acr_width = acr_info_list_v5->width;
        char *acr_name = acr_info_list_v5->name;
        unsigned int ByteSize =
            (acr_width % 8 == 0) ? acr_width / 8 : (acr_width / 8) + 1;

        g_string_append_printf(s,
            "<vector id=\"acr_%d_t\" type=\"uint8\" count=\"%d\"/>",
            ByteSize * 8, ByteSize);


        for (unsigned int idx = 0; idx < acr_number; idx++) {
            char acr_reg_name[2048];
            snprintf(acr_reg_name, sizeof(acr_reg_name),
                     "%s_%u", acr_name, idx);
            g_string_append_printf(s, "<reg name=\"%s\"", acr_reg_name);
            g_string_append_printf(s, " bitsize=\"%d\"", acr_width);
            g_string_append_printf(s, " type=\"acr_%d_t\"", ByteSize * 8);
            g_string_append_printf(s, " regnum=\"%d\" group=\"ace\"/>",
                                   base_reg++);
            num_regs++;
        }
        acr_info_list_v5++;
    }

    g_string_append_printf(s, "</feature>");

    cpu->dyn_ace_xml = g_string_free(s, false);

#if !defined(CONFIG_USER_ONLY)
    env->debugger = false;
#endif

    return num_regs;
}

static int ricsv_gen_dynamic_vector_xml(CPUState *cs, int base_reg)
{
    RISCVCPU *cpu = RISCV_CPU(cs);
    GString *s = g_string_new(NULL);
    g_autoptr(GString) ts = g_string_new("");
    int reg_width = cpu->cfg.vlen;
    int num_regs = 0;
    int i;

    g_string_printf(s, "<?xml version=\"1.0\"?>");
    g_string_append_printf(s, "<!DOCTYPE target SYSTEM \"gdb-target.dtd\">");
    g_string_append_printf(s, "<feature name=\"org.gnu.gdb.riscv.vector\">");

    /* First define types and totals in a whole VL */
    for (i = 0; i < ARRAY_SIZE(vec_lanes); i++) {
        int count = reg_width / vec_lanes[i].size;
        g_string_printf(ts, "%s", vec_lanes[i].id);
        g_string_append_printf(s,
                               "<vector id=\"%s\" type=\"%s\" count=\"%d\"/>",
                               ts->str, vec_lanes[i].gdb_type, count);
    }

    /* Define unions */
    g_string_append_printf(s, "<union id=\"riscv_vector\">");
    for (i = 0; i < ARRAY_SIZE(vec_lanes); i++) {
        g_string_append_printf(s, "<field name=\"%c\" type=\"%s\"/>",
                               vec_lanes[i].suffix,
                               vec_lanes[i].id);
    }
    g_string_append(s, "</union>");

    /* Define vector registers */
    for (i = 0; i < 32; i++) {
        g_string_append_printf(s,
                               "<reg name=\"v%d\" bitsize=\"%d\""
                               " regnum=\"%d\" group=\"vector\""
                               " type=\"riscv_vector\"/>",
                               i, reg_width, base_reg++);
        num_regs++;
    }

    g_string_append_printf(s, "</feature>");

    cpu->dyn_vreg_xml = g_string_free(s, false);
    return num_regs;
}

void riscv_cpu_register_gdb_regs_for_features(CPUState *cs)
{
    RISCVCPU *cpu = RISCV_CPU(cs);
    CPURISCVState *env = &cpu->env;
    if (env->misa_ext & RVD) {
        gdb_register_coprocessor(cs, riscv_gdb_get_fpu, riscv_gdb_set_fpu,
                                 32, "riscv-64bit-fpu.xml", 0);
    } else if (env->misa_ext & RVF) {
        gdb_register_coprocessor(cs, riscv_gdb_get_fpu, riscv_gdb_set_fpu,
                                 32, "riscv-32bit-fpu.xml", 0);
    }
    if (env->misa_ext & RVV) {
        int base_reg = cs->gdb_num_regs;
        gdb_register_coprocessor(cs, riscv_gdb_get_vector,
                                 riscv_gdb_set_vector,
                                 ricsv_gen_dynamic_vector_xml(cs, base_reg),
                                 "riscv-vector.xml", 0);
    }
    switch (env->misa_mxl_max) {
    case MXL_RV32:
        gdb_register_coprocessor(cs, riscv_gdb_get_virtual,
                                 riscv_gdb_set_virtual,
                                 1, "riscv-32bit-virtual.xml", 0);
        break;
    case MXL_RV64:
    case MXL_RV128:
        gdb_register_coprocessor(cs, riscv_gdb_get_virtual,
                                 riscv_gdb_set_virtual,
                                 1, "riscv-64bit-virtual.xml", 0);
        break;
    default:
        g_assert_not_reached();
    }

    if (cpu->cfg.ext_zicsr) {
        int base_reg = cs->gdb_num_regs;
        gdb_register_coprocessor(cs, riscv_gdb_get_csr, riscv_gdb_set_csr,
                                 riscv_gen_dynamic_csr_xml(cs, base_reg),
                                 "riscv-csr.xml", 0);
    }
    int ace_regs = riscv_gen_dynamic_ace_xml(cs, cs->gdb_num_regs);
    if (ace_regs != 0) {
        gdb_register_coprocessor(cs, riscv_gdb_get_ace, riscv_gdb_set_ace,
            riscv_gen_dynamic_ace_xml(cs, cs->gdb_num_regs),
            "riscv-ace.xml", 0);
    }
}
