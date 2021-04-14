/*
 * Andes Programmable Interval Timer, ATCPIT100, interface.
 *
 * Copyright (c) 2018 Andes Tech. Corp.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef HW_ATCPIT100_H
#define HW_ATCPIT100_H

#include "hw/ptimer.h"
#include "hw/sysbus.h"

#define ATCPIT100_CHANNEL_NUM (4)
#define ATCPIT100_CHANNEL_TIMER_NUM (4)

#define TYPE_ATCPIT100 "atcpit100"
#define ATCPIT100(obj) OBJECT_CHECK(Atcpit100State, (obj), TYPE_ATCPIT100)

typedef struct Atcpit100Timer {
    ptimer_state *ptimer;
    void *channel;
    int id;
} Atcpit100Timer;

typedef struct Atcpit100Channel {
    Atcpit100Timer timers[ATCPIT100_CHANNEL_TIMER_NUM];
    void *state;
    int id;

    uint32_t control;
    uint32_t reload;
    uint32_t counter;
} Atcpit100Channel;

typedef struct Atcpit100State {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    Atcpit100Channel channels[ATCPIT100_CHANNEL_NUM];
    qemu_irq irq;
    uint32_t pclk;
    uint32_t extclk;

    uint32_t id_rev;
    uint32_t cfg;
    uint32_t int_en;
    uint32_t int_st;
    uint32_t ch_en;
} Atcpit100State;

DeviceState *atcpit100_create(hwaddr addr, qemu_irq irq);

#endif /* HW_ATCPIT100_H */
