/*
 * Andes ACE GDB stub
 *
 * Copyright (c) 2023 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _ANDES_ACE_GDB_H_
#define _ANDES_ACE_GDB_H_

/* ACR_info records essential information of ACR and SRAM-type ACM */
typedef struct ACR_info_v5 {
    char name[1024];
    unsigned width;
    unsigned num;
} ACR_INFO_T_V5;

/*
 * INSN_TYPE_V5 represents what kind of utility instruction
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
} INSN_TYPE_V5;

typedef struct Util_Insn {
  INSN_TYPE_V5 version;
  unsigned insn;
} UTIL_INSN_T_V5;

typedef struct Insn_Code {
  unsigned num;
  UTIL_INSN_T_V5 *code;
} INSN_CODE_T_V5;

extern unsigned *global_acr_reg_count_v5;
extern unsigned *global_acr_type_count_v5;
extern unsigned *global_ace_lib_for_gdb_len_v5;
extern const char *global_ace_lib_for_gdb_v5;
extern ACR_INFO_T_V5 *global_acr_info_list_v5;
extern INSN_CODE_T_V5* (*gen_get_value_code) (char *name, unsigned index);
extern INSN_CODE_T_V5* (*gen_set_value_code) (char *name, unsigned index);

int32_t gdb_ace_load_lib(const char *);
int32_t gdb_ace_get_file_name_for_gdb(unsigned char *, char *);
int gdb_handle_query_rcmd_nds_query(GArray *, void *);
#endif
