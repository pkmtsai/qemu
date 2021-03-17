/*
 * Andes ATFMAC100 10/100 Ethernet
 *
 * Copyright (c) 2018 Andes Tech. Corp.
 *
 * This code is licensed under the GPL version 2 or later. See the
 * COPYING file in the top-level directory.
 */

#ifndef HW_ATFMAC100_H
#define HW_ATFMAC100_H

#include "hw/sysbus.h"
#include "net/net.h"

#define ATFMAC100_FRAME_SIZE_MAX (1518)
#define ATFMAC100_FRAME_BUFFER_SIZE (1 << 11)

#define TYPE_ATFMAC100 "atfmac100"
#define ATFMAC100(obj) OBJECT_CHECK(ATFMAC100State, (obj), TYPE_ATFMAC100)

typedef struct ATFMAC100State {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    NICState *nic;
    NICConf conf;
    qemu_irq irq;
    MemoryRegion mmio;

    /* Static properties */
    uint32_t revision;

    /* registers */
    uint32_t isr;
    uint32_t ier; /* interrupt enable register */
    uint32_t mac_madr;
    uint32_t mac_ladr;
    uint32_t maht[2];
    uint32_t txpd;
    uint32_t rxpd;
    uint32_t txr_badr;
    uint32_t rxr_badr;
    uint32_t itc;
    uint32_t aptc;
    uint32_t dblac;
    uint32_t revr; /* revision register */
    uint32_t fear; /* feature register */
    uint32_t maccr;
    uint32_t macsr;
    uint32_t phycr;
    uint32_t phywdata;
    uint32_t fcr;
    uint32_t bpr;
    uint32_t wolcr;
    uint32_t wolsr;
    uint32_t wfcrc;
    uint32_t wfbm1;
    uint32_t wfbm2;
    uint32_t wfbm3;
    uint32_t wfbm4;
    uint32_t ts;
    uint32_t dmafifos; /* DMA/FIFO state register */
    uint32_t tm;
    uint32_t txmscol;
    uint32_t rpfaepcnt;
    uint32_t xmpgcnt;
    uint32_t runttlccnt; /* RUNT_CNT and TLCC counter register */
    uint32_t crftlcnt;
    uint32_t rlcrccnt; /* RLC and RCC counter register */
    uint32_t brpcnt;
    uint32_t mulcacnt;
    uint32_t rpcnt;
    uint32_t xpcnt;

    /* PHY stuff */
    uint32_t phy_status;
    uint32_t phy_control;
    uint32_t phy_advertise;
    uint32_t phy_int;
    uint32_t phy_int_mask;

    /* internals */
    uint32_t rx_desc_addr;
    uint32_t tx_desc_addr;
    uint8_t frame[ATFMAC100_FRAME_BUFFER_SIZE];

} ATFMAC100State;

void
atfmac100_create(ATFMAC100State *atfmac, const char *name,
                 NICInfo *nd, hwaddr addr, qemu_irq irq);

#endif /* HW_ATFMAC100_H */
