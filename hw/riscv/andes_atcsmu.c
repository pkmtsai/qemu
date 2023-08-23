/*
 * Andes ATCSMU (System Management Unit)
 *
 * Copyright (c) 2021 Andes Tech. Corp.
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
#include "qemu/log.h"
#include "hw/sysbus.h"
#include "sysemu/runstate.h"
#include "hw/qdev-properties.h"
#include "hw/riscv/andes_atcsmu.h"
#include "target/riscv/cpu.h"

/* #define DEBUG_ATCSMU */
/* #define MORE_HOOK */

#define xLOG(x...)
#define yLOG(x...) qemu_log(x)
#define zLOG(x...) printf(x)

#define LOG(x...) xLOG(x)
#define LOGGE(x...) qemu_log_mask(LOG_GUEST_ERROR, x)

#ifdef DEBUG_ATCPIT100
#undef LOG
#define LOG(x...) yLOG(x)
#endif

static uint32_t
reset_vector_read(int hartid, bool data_hi)
{
    CPUState *cpu = qemu_get_cpu(hartid);
    if (cpu) {
        CPURISCVState *env = cpu->env_ptr;
        if (data_hi) {
            return env->resetvec = (env->resetvec >> 32) ;
        } else {
            return env->resetvec & (UINT32_MAX);
        }
    }
    return 0;
}


static void
reset_vector_write(int hartid, uint64_t value, bool data_hi)
{
    CPUState *cpu = qemu_get_cpu(hartid);
    if (cpu) {
        CPURISCVState *env = cpu->env_ptr;
        if (data_hi) {
            env->resetvec = (env->resetvec << 32 >> 32) |
                            ((value & UINT32_MAX) << 32);
        } else {
            env->resetvec = (env->resetvec >> 32 << 32) | (value & UINT32_MAX);
        }
    }
}


static uint64_t
andes_atcsmu_read(void *opaque, hwaddr addr, unsigned size)
{
    AndesATCSMUState *smu = ANDES_ATCSMU(opaque);
    switch (addr) {
    case ATCSMU_WRSR:
        return smu->wrsr;
        break;
    case ATCSMU_SCRATCH:
        return smu->scratch;
        break;
    case ATCSMU_HART0_RESET_VECTOR_LO:
        return reset_vector_read(0, 0);
        break;
    case ATCSMU_HART1_RESET_VECTOR_LO:
        return reset_vector_read(1, 0);
        break;
    case ATCSMU_HART2_RESET_VECTOR_LO:
        return reset_vector_read(2, 0);
        break;
    case ATCSMU_HART3_RESET_VECTOR_LO:
        return reset_vector_read(3, 0);
        break;
    case ATCSMU_HART4_RESET_VECTOR_LO:
        return reset_vector_read(4, 0);
        break;
    case ATCSMU_HART5_RESET_VECTOR_LO:
        return reset_vector_read(5, 0);
        break;
    case ATCSMU_HART6_RESET_VECTOR_LO:
        return reset_vector_read(6, 0);
        break;
    case ATCSMU_HART7_RESET_VECTOR_LO:
        return reset_vector_read(7, 0);
        break;
    case ATCSMU_HART0_RESET_VECTOR_HI:
        return reset_vector_read(0, 1);
        break;
    case ATCSMU_HART1_RESET_VECTOR_HI:
        return reset_vector_read(1, 1);
        break;
    case ATCSMU_HART2_RESET_VECTOR_HI:
        return reset_vector_read(2, 1);
        break;
    case ATCSMU_HART3_RESET_VECTOR_HI:
        return reset_vector_read(3, 1);
        break;
    case ATCSMU_HART4_RESET_VECTOR_HI:
        return reset_vector_read(4, 1);
        break;
    case ATCSMU_HART5_RESET_VECTOR_HI:
        return reset_vector_read(5, 1);
        break;
    case ATCSMU_HART6_RESET_VECTOR_HI:
        return reset_vector_read(6, 1);
        break;
    case ATCSMU_HART7_RESET_VECTOR_HI:
        return reset_vector_read(7, 1);
        break;
    case ATCSMU_PCS3_SCRATCH:
        return smu->pcs_regs[0].pcs_scratch;
        break;
    case ATCSMU_PCS4_SCRATCH:
        return smu->pcs_regs[1].pcs_scratch;
        break;
    case ATCSMU_PCS5_SCRATCH:
        return smu->pcs_regs[2].pcs_scratch;
        break;
    case ATCSMU_PCS6_SCRATCH:
        return smu->pcs_regs[3].pcs_scratch;
        break;
    case ATCSMU_PCS7_SCRATCH:
        return smu->pcs_regs[4].pcs_scratch;
        break;
    case ATCSMU_PCS8_SCRATCH:
        return smu->pcs_regs[5].pcs_scratch;
        break;
    case ATCSMU_PCS9_SCRATCH:
        return smu->pcs_regs[6].pcs_scratch;
        break;
    case ATCSMU_PCS10_SCRATCH:
        return smu->pcs_regs[7].pcs_scratch;
        break;
    default:
        LOGGE("%s: Bad addr %x\n", __func__, (int)addr);
    }
    return 0;
}

static void
andes_atcsmu_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    AndesATCSMUState *smu = ANDES_ATCSMU(opaque);
    switch (addr) {
    case ATCSMU_WRSR:
        smu->wrsr &= ~value;
        break;
    case ATCSMU_SCRATCH:
        smu->scratch = value;
        break;
    case ATCSMU_HART0_RESET_VECTOR_LO:
        reset_vector_write(0, value, 0);
        break;
    case ATCSMU_HART1_RESET_VECTOR_LO:
        reset_vector_write(1, value, 0);
        break;
    case ATCSMU_HART2_RESET_VECTOR_LO:
        reset_vector_write(2, value, 0);
        break;
    case ATCSMU_HART3_RESET_VECTOR_LO:
        reset_vector_write(3, value, 0);
        break;
    case ATCSMU_HART4_RESET_VECTOR_LO:
        reset_vector_write(4, value, 0);
        break;
    case ATCSMU_HART5_RESET_VECTOR_LO:
        reset_vector_write(5, value, 0);
        break;
    case ATCSMU_HART6_RESET_VECTOR_LO:
        reset_vector_write(6, value, 0);
        break;
    case ATCSMU_HART7_RESET_VECTOR_LO:
        reset_vector_write(7, value, 0);
        break;
    case ATCSMU_HART0_RESET_VECTOR_HI:
        reset_vector_write(0, value, 1);
        break;
    case ATCSMU_HART1_RESET_VECTOR_HI:
        reset_vector_write(1, value, 1);
        break;
    case ATCSMU_HART2_RESET_VECTOR_HI:
        reset_vector_write(2, value, 1);
        break;
    case ATCSMU_HART3_RESET_VECTOR_HI:
        reset_vector_write(3, value, 1);
        break;
    case ATCSMU_HART4_RESET_VECTOR_HI:
        reset_vector_write(4, value, 1);
        break;
    case ATCSMU_HART5_RESET_VECTOR_HI:
        reset_vector_write(5, value, 1);
        break;
    case ATCSMU_HART6_RESET_VECTOR_HI:
        reset_vector_write(6, value, 1);
        break;
    case ATCSMU_HART7_RESET_VECTOR_HI:
        reset_vector_write(7, value, 1);
        break;
    case ATCSMU_SMUCR:
        switch (value) {
        case SMUCMD_RESET:
            smu->wrsr |= 0x10;
            qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
            break;
        case SMUCMD_POWEROFF:
            qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
            break;
        }
        break;
    case ATCSMU_PCS3_SCRATCH:
        smu->pcs_regs[0].pcs_scratch = value;
        break;
    case ATCSMU_PCS4_SCRATCH:
        smu->pcs_regs[1].pcs_scratch = value;
        break;
    case ATCSMU_PCS5_SCRATCH:
        smu->pcs_regs[2].pcs_scratch = value;
        break;
    case ATCSMU_PCS6_SCRATCH:
        smu->pcs_regs[3].pcs_scratch = value;
        break;
    case ATCSMU_PCS7_SCRATCH:
        smu->pcs_regs[4].pcs_scratch = value;
        break;
    case ATCSMU_PCS8_SCRATCH:
        smu->pcs_regs[5].pcs_scratch = value;
        break;
    case ATCSMU_PCS9_SCRATCH:
        smu->pcs_regs[6].pcs_scratch = value;
        break;
    case ATCSMU_PCS10_SCRATCH:
        smu->pcs_regs[7].pcs_scratch = value;
        break;
    default:
        LOGGE("%s: Bad addr %x (value %x)\n", __func__, (int)addr, (int)value);
    }
}

static const MemoryRegionOps andes_atcsmu_ops = {
    .read = andes_atcsmu_read,
    .write = andes_atcsmu_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4
    }
};

static Property andes_atcsmu_properties[] = {
    DEFINE_PROP_UINT32("smu-base-addr", AndesATCSMUState, smu_base_addr, 0),
    DEFINE_PROP_UINT32("smu-base-size", AndesATCSMUState, smu_base_size, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void andes_atcsmu_realize(DeviceState *dev, Error **errp)
{
    AndesATCSMUState *smu = ANDES_ATCSMU(dev);
    memory_region_init_io(&smu->mmio, OBJECT(dev), &andes_atcsmu_ops, smu,
        TYPE_ANDES_ATCSMU, smu->smu_base_size);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &smu->mmio);
}

static void andes_atcsmu_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = andes_atcsmu_realize;
    device_class_set_props(dc, andes_atcsmu_properties);
}

static const TypeInfo andes_atcsmu_info = {
    .name = TYPE_ANDES_ATCSMU,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AndesATCSMUState),
    .class_init = andes_atcsmu_class_init,
};

static void andes_atcsmu_register_types(void)
{
    type_register_static(&andes_atcsmu_info);
}

type_init(andes_atcsmu_register_types)

/*
 * Create ATCSMU device.
 */
DeviceState*
andes_atcsmu_create(hwaddr addr, hwaddr size)
{
    DeviceState *dev = qdev_new(TYPE_ANDES_ATCSMU);
    qdev_prop_set_uint32(dev, "smu-base-addr", addr);
    qdev_prop_set_uint32(dev, "smu-base-size", size);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, addr);
    return dev;
}
