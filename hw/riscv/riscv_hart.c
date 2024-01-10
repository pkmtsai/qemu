/*
 * QEMU RISCV Hart Array
 *
 * Copyright (c) 2017 SiFive, Inc.
 *
 * Holds the state of a homogeneous array of RISC-V harts
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
#include "qapi/error.h"
#include "qemu/module.h"
#include "sysemu/reset.h"
#include "hw/sysbus.h"
#include "target/riscv/cpu.h"
#include "hw/qdev-properties.h"
#include "hw/riscv/riscv_hart.h"
#include "exec/address-spaces.h"
#include "exec/ramblock.h"

static Property riscv_harts_props[] = {
    DEFINE_PROP_UINT32("num-harts", RISCVHartArrayState, num_harts, 1),
    DEFINE_PROP_UINT32("hartid-base", RISCVHartArrayState, hartid_base, 0),
    DEFINE_PROP_STRING("cpu-type", RISCVHartArrayState, cpu_type),
    DEFINE_PROP_UINT64("resetvec", RISCVHartArrayState, resetvec,
                       DEFAULT_RSTVEC),
    DEFINE_PROP_END_OF_LIST(),
};

static Property andes_riscv_harts_props[] = {
    DEFINE_PROP_UINT64("ilm_base", RISCVHartArrayState, ilm_base, 0),
    DEFINE_PROP_UINT64("dlm_base", RISCVHartArrayState, dlm_base, 0x200000),
    DEFINE_PROP_UINT32("ilm_size", RISCVHartArrayState, ilm_size, 0x200000),
    DEFINE_PROP_UINT32("dlm_size", RISCVHartArrayState, dlm_size, 0x200000),
    DEFINE_PROP_BOOL("ilm_default_enable", RISCVHartArrayState,
                     ilm_default_enable, false),
    DEFINE_PROP_BOOL("dlm_default_enable", RISCVHartArrayState,
                     dlm_default_enable, false),
    DEFINE_PROP_END_OF_LIST(),
};

static void riscv_harts_cpu_reset(void *opaque)
{
    RISCVCPU *cpu = opaque;
    cpu_reset(CPU(cpu));
}

static void andes_hart_prop_set(RISCVHartArrayState *s, int idx)
{
    s->harts[idx].env.ilm_base = s->ilm_base;
    s->harts[idx].env.dlm_base = s->dlm_base;
    s->harts[idx].env.ilm_size = s->ilm_size;
    s->harts[idx].env.dlm_size = s->dlm_size;
    s->harts[idx].env.ilm_default_enable = s->ilm_default_enable;
    s->harts[idx].env.dlm_default_enable = s->dlm_default_enable;
}


static bool riscv_hart_realize(RISCVHartArrayState *s, int idx,
                               char *cpu_type, Error **errp)
{
    object_initialize_child(OBJECT(s), "harts[*]", &s->harts[idx], cpu_type);
    qdev_prop_set_uint64(DEVICE(&s->harts[idx]), "resetvec", s->resetvec);
    if (strncmp(cpu_type, "andes", 5) == 0) {
        andes_hart_prop_set(s, idx);
    }
    s->harts[idx].env.mhartid = s->hartid_base + idx;
    qemu_register_reset(riscv_harts_cpu_reset, &s->harts[idx]);
    return qdev_realize(DEVICE(&s->harts[idx]), NULL, errp);
}

static void riscv_harts_realize(DeviceState *dev, Error **errp)
{
    RISCVHartArrayState *s = RISCV_HART_ARRAY(dev);
    int n;

    s->harts = g_new0(RISCVCPU, s->num_harts);

    for (n = 0; n < s->num_harts; n++) {
        if (!riscv_hart_realize(s, n, s->cpu_type, errp)) {
            return;
        }
    }
}

static void riscv_harts_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, riscv_harts_props);
    dc->realize = riscv_harts_realize;
}

static void register_andes_harts_props(DeviceState *dev)
{
    Property *prop;
    for (prop = andes_riscv_harts_props; prop && prop->name; prop++) {
        qdev_property_add_static(dev, prop);
    }
}

static void riscv_harts_init(Object *obj)
{
    DeviceState *dev = DEVICE(obj);
    register_andes_harts_props(dev);
}

static const TypeInfo riscv_harts_info = {
    .name          = TYPE_RISCV_HART_ARRAY,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(RISCVHartArrayState),
    .instance_init = riscv_harts_init,
    .class_init    = riscv_harts_class_init,
};

static void riscv_harts_register_types(void)
{
    type_register_static(&riscv_harts_info);
}

type_init(riscv_harts_register_types)
