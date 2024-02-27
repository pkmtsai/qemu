/*
 * Raspberry Pi emulation (c) 2012 Gregory Estrade
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/dma/atcdmac300.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "qemu/module.h"

/* #define DEBUG_ANDES_ATCDMAC300 */
#define LOGGE(x...) qemu_log_mask(LOG_GUEST_ERROR, x)
#define xLOG(x...)
#define yLOG(x...) qemu_log(x)
#ifdef DEBUG_ANDES_ATCDMAC300
  #define LOG(x...) yLOG(x)
#else
  #define LOG(x...) xLOG(x)
#endif

static void atcdmac300_dma_int_stat_update(ATCDMAC300State *s, int status,
                                           int ch)
{
    s->IntStatus |= (1 << (status + ch));
}

static void atcdmac300_dma_reset_chan(ATCDMAC300State *s, int ch)
{
    if (s) {
        s->chan[ch].ChnCtrl &= ~(1 << CHAN_CTL_ENABLE);
        s->ChEN &= ~(1 << ch);
    }
}

static void atcdmac300_dma_reset(ATCDMAC300State *s)
{
    int ch;
    for (ch = 0; ch < ATCDMAC300_MAX_CHAN; ch++) {
        atcdmac300_dma_reset_chan(s, ch);
    }
}

static uint64_t atcdmac300_read(void *opaque, hwaddr offset, unsigned size)
{
    ATCDMAC300State *s = opaque;
    int ch = 0;
    uint64_t result = 0;

    if (offset >= 0x40) {
        ch = ATCDMAC300_GET_CHAN(offset);
        offset = ATCDMAC300_GET_OFF(offset, ch);
    }

    switch (offset) {
    case ATCDMAC300_DMA_CFG:
        result = s->DMACfg;
        break;
    case ATCDMAC300_DMAC_CTRL:
        break;
    case ATCDMAC300_INT_STATUS:
        result = s->IntStatus;
        break;
    case ATCDMAC300_CHAN_ENABLE:
        result = s->ChEN;
        break;
    case ATCDMAC300_CHAN_CTL:
        result = s->chan[ch].ChnCtrl;
        break;
    default:
        LOGGE("%s: Bad offset 0x%" HWADDR_PRIX "\n",
                      __func__, offset);
        break;
    }

    LOG("### atcdmac300_read()=0x%lx, val=0x%lx\n", offset, result);
    return result;
}

static void atcdmac300_write(void *opaque, hwaddr offset, uint64_t value,
                               unsigned size)
{
    ATCDMAC300State *s = opaque;
    int ch = 0;
    uint64_t buf[ATCDMAC300_MAX_BURST_SIZE];
    int64_t remain_size, burst_remain_size, copy_size, copy_size_dst;
    uint64_t src_addr, dst_addr, src_width, dst_width, burst_size,
             src_addr_ctl, dst_addr_ctl, int_tc_mask, int_err_mask;

    LOG("@@@ atcdmac300_write()=0x%lx, value=0x%lx\n", offset, value);

    if (offset >= 0x40) {
        ch = ATCDMAC300_GET_CHAN(offset);
        offset = ATCDMAC300_GET_OFF(offset, ch);
    }

    switch (offset) {
    case ATCDMAC300_INT_STATUS:
        /* Write 1 to clear */
        s->IntStatus &= ~value;
        if (!s->IntStatus) {
            qemu_irq_lower(s->irq);
        }
        break;
    case ATCDMAC300_DMAC_CTRL:
        atcdmac300_dma_reset(s);
        break;
    case ATCDMAC300_CHAN_CTL:
        s->chan[ch].ChnCtrl = value;

        if (((s->chan[ch].ChnCtrl >> CHAN_CTL_ENABLE) & 0x1) == 0x1) {
            LOG("### Start transmit(ch=%d) ###\n", ch);

            src_width = (s->chan[ch].ChnCtrl >> CHAN_CTL_SRC_WIDTH)
                            & CHAN_CTL_SRC_WIDTH_MASK;
            dst_width = (s->chan[ch].ChnCtrl >> CHAN_CTL_DST_WIDTH)
                            & CHAN_CTL_DST_WIDTH_MASK;
            burst_size = (s->chan[ch].ChnCtrl >> CHAN_CTL_SRC_BURST_SZ)
                            & CHAN_CTL_SRC_BURST_SZ_MASK;
            src_addr = (s->chan[ch].ChnSrcAddrH << 32) | s->chan[ch].ChnSrcAddr;
            dst_addr = (s->chan[ch].ChnDstAddrH << 32) | s->chan[ch].ChnDstAddr;
            src_addr_ctl = (s->chan[ch].ChnCtrl >> CHAN_CTL_SRC_ADDR_CTL)
                            & CHAN_CTL_SRC_ADDR_CTL_MASK;
            dst_addr_ctl = (s->chan[ch].ChnCtrl >> CHAN_CTL_DST_ADDR_CTL)
                            & CHAN_CTL_DST_ADDR_CTL_MASK;

            src_width = (1 << src_width);
            dst_width = (1 << dst_width);
            burst_size = (1 << burst_size);
            remain_size = (s->chan[ch].ChnTranSize * src_width);
            int_tc_mask = (s->chan[ch].ChnCtrl >> CHAN_CTL_INT_TC_MASK_POS)
                          & 0x1;
            int_err_mask = (s->chan[ch].ChnCtrl >> CHAN_CTL_INT_ERR_MASK_POS)
                           & 0x1;
            if (remain_size && burst_size < (1 << 11) &&
                src_width < (1 << 6) && dst_width < (1 << 6) &&
                (src_addr & (src_width - 1)) == 0 &&
                (dst_addr & (dst_width - 1)) == 0 &&
                (remain_size & (dst_width - 1)) == 0 &&
                (burst_size * src_width & (dst_width - 1)) == 0) {

                while (remain_size > 0) {
                    int i;
                    for (i = 0; i < burst_size; i++) {
                        if (remain_size <= 0) {
                            break;
                        }

                        memset(buf, 0, sizeof(buf));

                        copy_size = MIN(remain_size, src_width);
                        cpu_physical_memory_read(src_addr, buf, copy_size);
                        burst_remain_size = copy_size;
                        uint64_t *curr_ptr = buf;
                        while (burst_remain_size > 0) {
                            copy_size_dst = MIN(burst_remain_size, dst_width);
                            cpu_physical_memory_write(dst_addr, curr_ptr,
                                                        copy_size_dst);
                            curr_ptr += copy_size_dst;
                            burst_remain_size -= copy_size_dst;
                        }

                        LOG("ATCDMAC300: ch[%d]: 0x%lx->0x%lx buf=0x%lx,"
                                " size=%ld\n", ch, src_addr, dst_addr, buf[0]
                                , copy_size);

                        remain_size -= copy_size;

                        if (src_addr_ctl == 0) {
                            src_addr += copy_size;
                        }
                        if (src_addr_ctl == 1) {
                            src_addr -= copy_size;
                        }
                        if (dst_addr_ctl == 0) {
                            dst_addr += copy_size;
                        }
                        if (dst_addr_ctl == 1) {
                            dst_addr -= copy_size;
                        }
                    }
                }
                atcdmac300_dma_reset_chan(s, ch);
                atcdmac300_dma_int_stat_update(s, INT_STATUS_TC, ch);
                if (!int_tc_mask) {
                    qemu_irq_raise(s->irq);
                }
            } else {
                atcdmac300_dma_reset_chan(s, ch);
                atcdmac300_dma_int_stat_update(s, INT_STATUS_ERR, ch);
                if (!int_err_mask) {
                    qemu_irq_raise(s->irq);
                }
            }

        } else {
            atcdmac300_dma_reset_chan(s, ch);
            qemu_irq_lower(s->irq);
        }
        break;
    case ATCDMAC300_CHAN_TRAN_SZ:
        s->chan[ch].ChnTranSize = value;
        break;
    case ATCDMAC300_CHAN_SRC_ADDR:
        s->chan[ch].ChnSrcAddr = value;
        break;
    case ATCDMAC300_CHAN_SRC_ADDR_H:
        s->chan[ch].ChnSrcAddrH = value;
        break;
    case ATCDMAC300_CHAN_DST_ADDR:
        s->chan[ch].ChnDstAddr = value;
        break;
    case ATCDMAC300_CHAN_DST_ADDR_H:
        s->chan[ch].ChnDstAddrH = value;
        break;
    case ATCDMAC300_CHAN_LL_POINTER:
        s->chan[ch].ChnLLPointer = value;
        break;
    case ATCDMAC300_CHAN_LL_POINTER_H:
        s->chan[ch].ChnLLPointerH = value;
        break;
    default:
        LOGGE("%s: Bad offset 0x%" HWADDR_PRIX "\n",
                      __func__, offset);
        break;
    }
}

static const MemoryRegionOps atcdmac300_ops = {
    .read = atcdmac300_read,
    .write = atcdmac300_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 8
    }
};

static void atcdmac300_init(Object *obj)
{
    ATCDMAC300State *s = ATCDMAC300(obj);
    SysBusDevice *sbus = SYS_BUS_DEVICE(obj);

    sysbus_init_irq(sbus, &s->irq);
    memory_region_init_io(&s->mmio, obj, &atcdmac300_ops,
                            s, TYPE_ATCDMAC300, s->mmio_size);
    sysbus_init_mmio(sbus, &s->mmio);
}

static Property atcdmac300_properties[] = {
    DEFINE_PROP_UINT32("mmio-size", ATCDMAC300State, mmio_size, 0x100000),
    DEFINE_PROP_UINT32("id-and-revision", ATCDMAC300State, IdRev,
                    (ATCDMAC300_PRODUCT_ID  << 8) |
                    ((ATCDMAC300_PRODUCT_ID & 0x7) << 4) |
                    ((ATCDMAC300_PRODUCT_ID & 0x7))),
    DEFINE_PROP_UINT32("inturrupt-status", ATCDMAC300State, IntStatus, 0),
    DEFINE_PROP_UINT32("dmac-configuration", ATCDMAC300State,
                            DMACfg, 0xc3404108),
    DEFINE_PROP_END_OF_LIST(),
};

static void atcdmac300_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *k = DEVICE_CLASS(klass);
    device_class_set_props(k, atcdmac300_properties);
}

static const TypeInfo atcdmac300_info = {
    .name          = TYPE_ATCDMAC300,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ATCDMAC300State),
    .class_init    = atcdmac300_class_init,
    .instance_init = atcdmac300_init,
};

void
atcdmac300_create(ATCDMAC300State *atcdmac, const char *name,
                 hwaddr addr, hwaddr mmio_size, qemu_irq irq)
{
    qdev_prop_set_uint32(DEVICE(atcdmac), "mmio-size", mmio_size);

    sysbus_realize(SYS_BUS_DEVICE(atcdmac), NULL);
    sysbus_mmio_map(SYS_BUS_DEVICE(atcdmac), 0, addr);
    sysbus_connect_irq(SYS_BUS_DEVICE(atcdmac), 0, irq);
    return;
}

static void atcdmac300_register_types(void)
{
    type_register_static(&atcdmac300_info);
}

type_init(atcdmac300_register_types)
