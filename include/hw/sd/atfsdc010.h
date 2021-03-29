/*
 * Andes Secure Digital (SD) host controller, ATFSDC010.
 *
 * Copyright (c) 2018 Andes Tech. Corp.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef HW_ATFSDC010_H
#define HW_ATFSDC010_H

#include "hw/sysbus.h"
#include "hw/sd/sd.h"

#define TYPE_ATFSDC010 "atfsdc010"
#define ATFSDC010(obj) \
        OBJECT_CHECK(ATFSDC010State, (obj), TYPE_ATFSDC010)

#define TYPE_ATFSDC010_BUS "atfsdc010-bus"

#define ATFSDC_REVISION     0x00030107
#define ATFSDC010_FIFO_LEN  16

typedef struct {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    SDBus sdbus;

    uint32_t cmd;
    uint32_t cmdarg;
    uint32_t response[4];
    uint32_t respcmd;
    uint32_t datactrl;
    uint32_t datatimer;
    uint32_t datalength;
    uint32_t status;
          /* clear register */
    uint32_t mask;
    uint32_t power;
    uint32_t clock;
    uint32_t buswidth;
          /* data window */
    uint32_t mmc_irq_resptime;
    uint32_t gpo;
    uint32_t sdio_ctrl1;
    uint32_t sdio_ctrl2;
    uint32_t sdio_status;
    uint32_t feature;
    uint32_t revision;

    uint32_t base;
    uint32_t datacnt;
    int fifo_pos;
    int fifo_len;
    uint32_t fifo[ATFSDC010_FIFO_LEN];
    qemu_irq irq;
    /* GPIO outputs for 'card is readonly' and 'card inserted' */
    qemu_irq card_readonly;
    qemu_irq card_inserted;
} ATFSDC010State;

DeviceState *atfsdc010_create(hwaddr addr, qemu_irq irq);

#endif
