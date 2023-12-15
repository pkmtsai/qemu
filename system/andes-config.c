/*
 * Andes config process file
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/andes-config.h"

QemuOptsList qemu_andes_config_opts = {
    .name = "andes-config",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_andes_config_opts.head),
    .desc = {
        {  }
    },
};

static int andes_config_query_func(void *opaque, QemuOpts *opts, Error **errp)
{
    QemuOpt *opt;
    AndesConfig *user_opts = (AndesConfig *)opaque;
    QTAILQ_FOREACH(opt, &opts->head, next) {
        if (((!opts->id && !user_opts->id) || (opts->id && user_opts->id &&
            !strcmp(user_opts->id, opts->id))) &&
            !strcmp(user_opts->name, opt->name)) {
            *user_opts->value = opt->str;
            return 1;
        }
    }
    return 0;

}

int andes_config_query(const char *id, const char *name, char **value)
{
    AndesConfig opts;
    opts.id = id;
    opts.name = name;
    opts.value = value;
    return qemu_opts_foreach(&qemu_andes_config_opts,
        andes_config_query_func, &opts, NULL);
}

bool andes_config_bool(const char *id, const char *name, uint64_t *val)
{
    char *value;
    bool bval;
    /* For CSR mask special config, name is NULL will always set bit mask */
    if (!name) {
        *val = true;
        return true;
    }
    if (andes_config_query(id, name, &value)) {
        if (qapi_bool_parse(name, value, &bval, NULL)) {
            *val = bval;
            return true;
        }
    }
    return false;
}

bool andes_config_number(const char *id, const char *name, uint64_t *val)
{
    char *value;
    int ret;

    /* For CSR mask special config, number type name cannot be set to NULL */
    if (!name) {
        return false;
    }
    if (andes_config_query(id, name, &value)) {
        ret = qemu_strtou64(value, NULL, 0, val);
        if (ret == -ERANGE) {
            error_setg(&error_warn,
                "Value '%s' is too large for parameter '%s'", value, name);
            return false;
        }
        if (ret) {
            error_setg(&error_warn,
                QERR_INVALID_PARAMETER_VALUE, name, "a number");
            return false;
        }
        return true;
    }
    return false;
}
