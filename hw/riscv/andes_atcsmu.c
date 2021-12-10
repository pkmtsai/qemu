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

static uint64_t
andes_atcsmu_read(void *opaque, hwaddr addr, unsigned size)
{
    return 0;
}

static void
andes_atcsmu_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    switch (addr) {
    case ATCSMU_SMUCR:
        switch (value) {
        case SMUCMD_RESET:
            qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
            break;
        case SMUCMD_POWEROFF:
            qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
            break;
        }
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
