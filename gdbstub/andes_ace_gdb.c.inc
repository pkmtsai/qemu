/*
 * Andes ACE GDB stub
 *
 * Copyright (c) 2023 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _ANDES_ACE_GDB_
#define _ANDES_ACE_GDB_
#include <dlfcn.h>
#include <sys/utsname.h>
#include "gdbstub/andes_ace_gdb.h"
#include "qemu/qemu-print.h"

unsigned *global_acr_reg_count_v5;
unsigned *global_acr_type_count_v5;
unsigned *global_ace_lib_for_gdb_len_v5;
const char *global_ace_lib_for_gdb_v5;
ACR_INFO_T_V5 *global_acr_info_list_v5;
INSN_CODE_T_V5* (*gen_get_value_code) (char *name, unsigned index);
INSN_CODE_T_V5* (*gen_set_value_code) (char *name, unsigned index);

#define LE(msg) do {        \
    err_str = dlerror();    \
    if (err_str != NULL) {  \
        qemu_printf("%s\n", msg);  \
        return -1;          \
    }                       \
} while (0)

int32_t gdb_ace_load_lib(const char *so_name)
{
    char *err_str = NULL;
    void *handle;

    /*
     * Check whether already load ACE debugger library,
     * we don't support multi-ace and/or multi-ace-dbg
     */
    if (global_ace_lib_for_gdb_v5 != NULL) {
        return 0;
    }

    /* dlopen a shared object named 'so_name' */
    handle = dlopen(so_name, RTLD_NOW | RTLD_LOCAL);
    LE("unable to dlopen the acelibdbg library");

    assert(handle != NULL);

    global_ace_lib_for_gdb_v5 =
        (const char *) dlsym(handle, "ace_lib_for_gdb");
    LE("unable to load symbol ace_lib_for_gdb");

    global_ace_lib_for_gdb_len_v5 =
        (unsigned *) dlsym(handle, "ace_lib_for_gdb_len");
    LE("unable to load symbol ace_lib_for_gdb_len");

    /*
     * Below ACR relative symbols can be skip since some .ace file
     * has no any ACR
     */
    global_acr_reg_count_v5 = (unsigned *) dlsym(handle, "acr_reg_count");

    global_acr_type_count_v5 = (unsigned *) dlsym(handle, "acr_type_count");

    gen_get_value_code = dlsym(handle, "gen_get_value_code");

    gen_set_value_code = dlsym(handle, "gen_set_value_code");

    if (global_acr_type_count_v5 && *global_acr_type_count_v5 != 0) {
        global_acr_info_list_v5 = (ACR_INFO_T_V5 *) dlsym(handle, "acr_list");
    }

    return 0;
}

int32_t gdb_ace_get_file_name_for_gdb(unsigned char *platform, char *soname)
{
    int32_t ret = 0;

    if (global_ace_lib_for_gdb_v5) {
        struct utsname os;
        if (uname(&os) == 0) {
            const char *str;

            if (g_strcmp0((const char *)platform, os.sysname) == 0) {
                /* Return binary share library.  */
                str = global_ace_lib_for_gdb_v5;
            } else
                return -1;

            /* use size as filename to do fopen */
            sprintf(soname, "%u", *global_ace_lib_for_gdb_len_v5);

            FILE *fd = fopen(soname, "w");
            if (fd == NULL) {
                ret = -2;
            } else {
                if (fwrite(str, sizeof(char),
                    *global_ace_lib_for_gdb_len_v5, fd) !=
                    *global_ace_lib_for_gdb_len_v5) {
                    ret = -3;
                }
                fclose(fd);
            }
        }
    } else {
        return -4;
    }

    return ret;
}

#define NDS_QUERY_TARGET    "SID"
#define NDS_QUERY_ENDIAN    "LE"
#define NDS_QUERY_CPUID     "0"
#define NDS_QUERY_TARGET_CMD    "nds query target"
#define NDS_QUERY_ENDIAN_CMD    "nds query endian"
#define NDS_QUERY_CPUID_CMD     "nds query cpuid"
#define NDS_ACE_CMD             "nds ace "
#define NDS_OTHER_CMD           "nds "

int gdb_handle_query_rcmd_nds_query(GArray *params, void *user_ctx)
{
    char reply[16];
    /* nds query target only can be 'SID' or 'OCD' */
    if (!g_strcmp0((const char *)gdbserver_state.mem_buf->data,
        NDS_QUERY_TARGET_CMD)) {
        gdb_memtohex(gdbserver_state.str_buf,
                     (guint8 *)NDS_QUERY_TARGET, strlen(NDS_QUERY_TARGET));
        gdb_put_strbuf();
        return 0;
    }
    /* nds query endian only can be '[target]: [LE|BE]' */
    if (!g_strcmp0((const char *)gdbserver_state.mem_buf->data,
        NDS_QUERY_ENDIAN_CMD)) {
        snprintf(reply, sizeof(reply), "%s: %s",
                 NDS_QUERY_TARGET, NDS_QUERY_ENDIAN);
        gdb_memtohex(gdbserver_state.str_buf, (guint8 *)reply, strlen(reply));
        gdb_put_strbuf();
        return 0;
    }
    /* Should return 0 or hartid */
    if (!g_strcmp0((const char *)gdbserver_state.mem_buf->data,
        NDS_QUERY_CPUID_CMD)) {
        gdb_memtohex(gdbserver_state.str_buf,
                     (guint8 *)NDS_QUERY_CPUID, strlen(NDS_QUERY_CPUID));
        gdb_put_strbuf();
        return 0;
    }
    /* gdb client may send 'nds ace os.name', os.name may 'Linux' */
    if (!strncmp((const char *)gdbserver_state.mem_buf->data,
        NDS_ACE_CMD, strlen(NDS_ACE_CMD))) {
        /*
         * Should put libacetool.so file length as
         * filename since ICEman did it
         */
        char filename[64];
        if (gdb_ace_get_file_name_for_gdb(gdbserver_state.mem_buf->data +
            strlen(NDS_ACE_CMD), filename)) {
            gdb_put_packet("E04");
            return 0;
        }
        gdb_memtohex(gdbserver_state.str_buf,
                     (guint8 *)filename, strlen(filename));
        gdb_put_strbuf();
        return 0;
    }
    if (!strncmp((const char *)gdbserver_state.mem_buf->data,
        NDS_OTHER_CMD, strlen(NDS_OTHER_CMD))) {
         gdb_put_packet("OK");
        return 0;
    }
    return -1;
}
#endif
