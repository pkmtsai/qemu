/*
 * Andes ACE GDB stub
 *
 * Copyright (c) 2023 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _ANDES_ACE_GDB_H_
#define _ANDES_ACE_GDB_H_

/* AceAcrInfo records essential information of ACR and SRAM-type ACM */
typedef struct {
    char name[1024];
    unsigned width;
    unsigned num;
} AceAcrInfo;

/*
 * AceInsnType represents what kind of utility instruction
 * acr_io1 : ACR utility instruction and exists one GPR for din/dout
 * acr_io2 : ACR utility instruction and exists two GPRs for din/dout
 *           (din_high, din_low/dout_high, din_low)
 * acm_io1 : ACM utility instruction and exists one GPR for din/dout
 * acm_io2 : ACM utility instruction and exists two GPRs for din/dout
 *           (din_high, din_low/dout_high, din_low)
 */
typedef enum {
  acr_io1 = 0, acr_io2 = 1,
  acm_io1 = 2, acm_io2 = 3
} AceInsnType;

typedef struct {
  AceInsnType version;
  unsigned insn;
} AceUtilInsn;

typedef struct {
  unsigned num;
  AceUtilInsn *code;
} AceInsnCode;

extern unsigned *ace_acr_reg_count;
extern unsigned *ace_acr_type_count;
extern unsigned *ace_lib_for_gdb_len;
extern const char *ace_lib_for_gdb;
extern AceAcrInfo *ace_acr_info_list;
extern AceInsnCode *(*ace_gen_get_value_code) (char *name, unsigned index);
extern AceInsnCode *(*ace_gen_set_value_code) (char *name, unsigned index);

int32_t gdb_ace_load_lib(const char *);
int32_t gdb_ace_get_file_name_for_gdb(unsigned char *, char *);
int gdb_handle_query_rcmd_andes_query(GArray *, void *);
#endif
