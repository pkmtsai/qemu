/*
 * Andes config process header file
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef QEMU_ANDES_CONFIG_H
#define QEMU_ANDES_CONFIG_H

#include "qemu/osdep.h"
#include "qemu/option.h"
#include "qemu/config-file.h"
#include "qemu/error-report.h"
#include "chardev/char.h"
#include "qemu/option_int.h"
#include "qapi/qmp/qerror.h"
#include "qapi/error.h"
#include "qemu/cutils.h"

typedef struct AndesConfig {
    const char *id;
    const char *name;
    char **value;
} AndesConfig;

int andes_config_query(const char *id, const char *name, char **value);
int qemu_andes_config_options(const char *optarg);
bool andes_config_bool(const char *id, const char *name, uint64_t *val);
bool andes_config_number(const char *id, const char *name, uint64_t *val);

typedef enum AndesConfigType {
    CONFIG_BOOL = 0,
    CONFIG_NUMBER
} AndesConfigType;

typedef bool andes_config_func(const char *, const char *, uint64_t *);

#define ANDES_CONFIG_ID_CPU "cpu"
#endif
