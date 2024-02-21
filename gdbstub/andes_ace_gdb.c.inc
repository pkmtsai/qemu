/*
 * Andes ACE GDB stub
 *
 * Copyright (c) 2023 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _ANDES_ACE_GDB_
#define _ANDES_ACE_GDB_
#include <dlfcn.h>
#ifndef WIN32
#include <sys/utsname.h>
#endif
#include "gdbstub/andes_ace_gdb.h"
#include "qemu/qemu-print.h"

unsigned *ace_acr_reg_count;
unsigned *ace_acr_type_count;
unsigned *ace_lib_for_gdb_len;
const char *ace_lib_for_gdb;
AceAcrInfo *ace_acr_info_list;
AceInsnCode* (*ace_gen_get_value_code) (char *name, unsigned index);
AceInsnCode* (*ace_gen_set_value_code) (char *name, unsigned index);

int32_t gdb_ace_load_lib(const char *so_name)
{
    void *handle;
    /*
     * Check whether already load ACE debugger library,
     * we don't support multi-ace and/or multi-ace-dbg
     */
    if (ace_lib_for_gdb != NULL) {
        return 0;
    }

    /* dlopen a shared object named 'so_name' */
    handle = dlopen(so_name, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        return -1;
    }

    ace_lib_for_gdb =
        (const char *) dlsym(handle, "ace_lib_for_gdb");
    if (ace_lib_for_gdb == NULL) {
        qemu_printf("Unable to load symbol ace_lib_for_gdb\n");
        return -1;
    }

    ace_lib_for_gdb_len =
        (unsigned *) dlsym(handle, "ace_lib_for_gdb_len");
    if (ace_lib_for_gdb_len == NULL) {
        qemu_printf("Unable to load symbol ace_lib_for_gdb_len\n");
        return -1;
    }

    /*
     * Below ACR relative symbols can be skip since some .ace file
     * has no any ACR
     */
    ace_acr_reg_count = (unsigned *) dlsym(handle, "acr_reg_count");

    ace_acr_type_count = (unsigned *) dlsym(handle, "acr_type_count");

    ace_gen_get_value_code = dlsym(handle, "gen_get_value_code");

    ace_gen_set_value_code = dlsym(handle, "gen_set_value_code");

    if (ace_acr_type_count && *ace_acr_type_count != 0) {
        ace_acr_info_list = (AceAcrInfo *) dlsym(handle, "acr_list");
    }

    return 0;
}

int32_t gdb_ace_get_file_name_for_gdb(unsigned char *platform, char *soname)
{
    int32_t ret = 0;

    if (ace_lib_for_gdb) {
#ifndef WIN32
        struct utsname os;
        if (uname(&os) == 0) {
            const char *str;

            if (g_strcmp0((const char *)platform, os.sysname) == 0) {
                /* Return binary share library.  */
                str = ace_lib_for_gdb;
            } else
                return -1;
#else
	{
            const char *str;
            str = ace_lib_for_gdb;
#endif

            /* use size as filename to do fopen */
            sprintf(soname, "%u", *ace_lib_for_gdb_len);

            FILE *fd = fopen(soname, "w");
            if (fd == NULL) {
                ret = -2;
            } else {
                if (fwrite(str, sizeof(char),
                    *ace_lib_for_gdb_len, fd) !=
                    *ace_lib_for_gdb_len) {
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
#define NDS_QUERY_TARGET_CMD    "nds query target"
#define NDS_QUERY_ENDIAN_CMD    "nds query endian"
#define NDS_QUERY_CPUID_CMD     "nds query cpuid"
#define NDS_ACE_CMD             "nds ace "
#define NDS_VA_CMD              "nds va "
#define NDS_OTHER_CMD           "nds "

int gdb_handle_query_rcmd_andes_query(GArray *params, void *user_ctx)
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
    /* return hartid as string */
    if (!g_strcmp0((const char *)gdbserver_state.mem_buf->data,
        NDS_QUERY_CPUID_CMD)) {
        snprintf(reply, sizeof(reply), "%d", gdbserver_state.g_cpu->cpu_index);
        gdb_memtohex(gdbserver_state.str_buf, (guint8 *)reply, strlen(reply));
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
        NDS_VA_CMD, strlen(NDS_VA_CMD))) {
        /*
         * This command is implemented as a hmp command so we will let
         * qemu monitor handle the command.
         */
        return -1;
    }
    if (!strncmp((const char *)gdbserver_state.mem_buf->data,
        NDS_OTHER_CMD, strlen(NDS_OTHER_CMD))) {
        gdb_put_packet("OK");
        return 0;
    }
    /*
     * Returning -1 will let the caller(gdb_handle_query_rcmd) knows
     * the command was not handled here and will write this command
     * to the chardev of qemu monitor.
     */
    return -1;
}
#endif
