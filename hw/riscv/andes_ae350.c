/*
 * Andes RISC-V AE350 Board
 *
 * Copyright (c) 2021 Andes Tech. Corp.
 *
 * Andes AE350 Board supports ns16550a UART and VirtIO MMIO.
 * The interrupt controllers are andes PLIC and andes PLICSW.
 * Timer is Andes PLMT.
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
#include "qemu/units.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/nmi.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/char/serial.h"
#include "hw/misc/unimp.h"
#include "target/riscv/cpu.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/boot.h"
#include "hw/riscv/numa.h"
#include "chardev/char.h"
#include "sysemu/arch_init.h"
#include "sysemu/device_tree.h"
#include "sysemu/sysemu.h"
#include "hw/pci/pci.h"
#include "hw/pci-host/gpex.h"
#include "elf.h"

#include "hw/intc/andes_plic.h"
#include "hw/timer/andes_plmt.h"
#include "hw/timer/atcpit100.h"
#include "hw/riscv/andes_ae350.h"
#include "hw/misc/andes_atcsmu.h"
#include "hw/sd/atfsdc010.h"
#include "hw/rtc/atcrtc100.h"
#include "hw/watchdog/atcwdt200.h"

#define BIOS_FILENAME ""

static const struct MemmapEntry {
    hwaddr base;
    hwaddr size;
} andes_ae350_memmap[] = {
    [ANDES_AE350_DRAM]      = { 0x00000000, 0x80000000 },
    [ANDES_AE350_MROM]      = { 0x80000000,  0x8000000 },
    [ANDES_AE350_SLAVEPORT0_ILM] = { 0xa0000000,   0x200000 },
    [ANDES_AE350_SLAVEPORT0_DLM] = { 0xa0200000,   0x200000 },
    [ANDES_AE350_SLAVEPORT1_ILM] = { 0xa0400000,   0x200000 },
    [ANDES_AE350_SLAVEPORT1_DLM] = { 0xa0600000,   0x200000 },
    [ANDES_AE350_SLAVEPORT2_ILM] = { 0xa0800000,   0x200000 },
    [ANDES_AE350_SLAVEPORT2_DLM] = { 0xa0a00000,   0x200000 },
    [ANDES_AE350_SLAVEPORT3_ILM] = { 0xa0c00000,   0x200000 },
    [ANDES_AE350_SLAVEPORT3_DLM] = { 0xa0e00000,   0x200000 },
    [ANDES_AE350_NOR]       = { 0x88000000,  0x4000000 },
    [ANDES_AE350_MAC]       = { 0xe0100000,   0x100000 },
    [ANDES_AE350_LCD]       = { 0xe0200000,   0x100000 },
    [ANDES_AE350_SMC]       = { 0xe0400000,   0x100000 },
    [ANDES_AE350_L2C]       = { 0xe0500000,   0x100000 },
    [ANDES_AE350_PLIC]      = { 0xe4000000,   0x400000 },
    [ANDES_AE350_PLMT]      = { 0xe6000000,   0x100000 },
    [ANDES_AE350_PLICSW]    = { 0xe6400000,   0x400000 },
    [ANDES_AE350_SMU]       = { 0xf0100000,   0x100000 },
    [ANDES_AE350_UART1]     = { 0xf0200000,   0x100000 },
    [ANDES_AE350_UART2]     = { 0xf0300000,   0x100000 },
    [ANDES_AE350_PIT]       = { 0xf0400000,   0x100000 },
    [ANDES_AE350_WDT]       = { 0xf0500000,   0x100000 },
    [ANDES_AE350_RTC]       = { 0xf0600000,   0x100000 },
    [ANDES_AE350_GPIO]      = { 0xf0700000,   0x100000 },
    [ANDES_AE350_I2C]       = { 0xf0a00000,   0x100000 },
    [ANDES_AE350_SPI]       = { 0xf0b00000,   0x100000 },
    [ANDES_AE350_DMAC]      = { 0xf0c00000,   0x100000 },
    [ANDES_AE350_SND]       = { 0xf0d00000,   0x100000 },
    [ANDES_AE350_SDC]       = { 0xf0e00000,   0x100000 },
    [ANDES_AE350_SPI2]      = { 0xf0f00000,   0x100000 },
    [ANDES_AE350_VIRTIO]    = { 0xfe000000,     0x1000 },
};

static void
create_fdt(AndesAe350BoardState *bs, const struct MemmapEntry *memmap,
    uint64_t mem_size)
{
    AndesAe350SocState *s = &bs->soc;
    MachineState *ms = MACHINE(bs);
    void *fdt;
    int cpu, i;
    uint64_t mem_addr;
    uint32_t *plic_irq_ext, *plicsw_irq_ext, *plmt_irq_ext;
    unsigned long plic_addr, plicsw_addr, plmt_addr;
    char *plic_name, *plicsw_name, *plmt_name;
    uint32_t intc_phandle = 0, plic_phandle = 0;
    uint32_t phandle = 1;
    char *isa_name, *mem_name, *cpu_name, *intc_name, *uart_name, *virtio_name;

    fdt = ms->fdt = create_device_tree(&bs->fdt_size);
    if (!fdt) {
        error_report("create_device_tree() failed");
        exit(1);
    }

    qemu_fdt_setprop_string(fdt, "/", "model", "Andes AE350 Board");
    qemu_fdt_setprop_string(fdt, "/", "compatible", "andestech,ae350");
    qemu_fdt_setprop_cell(fdt, "/", "#size-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/", "#address-cells", 0x2);

    qemu_fdt_add_subnode(fdt, "/soc");
    qemu_fdt_setprop(fdt, "/soc", "ranges", NULL, 0);
    qemu_fdt_setprop_string(fdt, "/soc", "compatible", "simple-bus");
    qemu_fdt_setprop_cell(fdt, "/soc", "#size-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/soc", "#address-cells", 0x2);

    qemu_fdt_add_subnode(fdt, "/cpus");
    qemu_fdt_setprop_cell(fdt, "/cpus", "timebase-frequency",
                          ANDES_PLMT_TIMEBASE_FREQ);
    qemu_fdt_setprop_cell(fdt, "/cpus", "#size-cells", 0x0);
    qemu_fdt_setprop_cell(fdt, "/cpus", "#address-cells", 0x1);
    qemu_fdt_add_subnode(fdt, "/cpus/cpu-map");

    plic_irq_ext = g_new0(uint32_t, s->cpus.num_harts * 4);
    plicsw_irq_ext = g_new0(uint32_t, s->cpus.num_harts * 2);
    plmt_irq_ext = g_new0(uint32_t, s->cpus.num_harts * 2);

    for (cpu = 0; cpu < s->cpus.num_harts; cpu++) {
        intc_phandle = phandle++;

        cpu_name = g_strdup_printf("/cpus/cpu@%d",
            s->cpus.hartid_base + cpu);
        qemu_fdt_add_subnode(fdt, cpu_name);
#if defined(TARGET_RISCV32)
        qemu_fdt_setprop_string(fdt, cpu_name, "mmu-type", "riscv,sv32");
#else
        qemu_fdt_setprop_string(fdt, cpu_name, "mmu-type", "riscv,sv39");
#endif
        isa_name = riscv_isa_string(&s->cpus.harts[cpu]);
        qemu_fdt_setprop_string(fdt, cpu_name, "riscv,isa", isa_name);
        g_free(isa_name);
        qemu_fdt_setprop_string(fdt, cpu_name, "compatible", "riscv");
        qemu_fdt_setprop_string(fdt, cpu_name, "status", "okay");
        qemu_fdt_setprop_cell(fdt, cpu_name, "reg",
            s->cpus.hartid_base + cpu);
        qemu_fdt_setprop_string(fdt, cpu_name, "device_type", "cpu");

        intc_name = g_strdup_printf("%s/interrupt-controller", cpu_name);
        qemu_fdt_add_subnode(fdt, intc_name);
        qemu_fdt_setprop_cell(fdt, intc_name, "phandle", intc_phandle);
        qemu_fdt_setprop_string(fdt, intc_name, "compatible",
            "riscv,cpu-intc");
        qemu_fdt_setprop(fdt, intc_name, "interrupt-controller", NULL, 0);
        qemu_fdt_setprop_cell(fdt, intc_name, "#interrupt-cells", 1);

        plic_irq_ext[cpu * 4 + 0] = cpu_to_be32(intc_phandle);
        plic_irq_ext[cpu * 4 + 1] = cpu_to_be32(IRQ_M_EXT);
        plic_irq_ext[cpu * 4 + 2] = cpu_to_be32(intc_phandle);
        plic_irq_ext[cpu * 4 + 3] = cpu_to_be32(IRQ_S_EXT);

        plicsw_irq_ext[cpu * 2 + 0] = cpu_to_be32(intc_phandle);
        plicsw_irq_ext[cpu * 2 + 1] = cpu_to_be32(IRQ_M_SOFT);

        plmt_irq_ext[cpu * 2 + 0] = cpu_to_be32(intc_phandle);
        plmt_irq_ext[cpu * 2 + 1] = cpu_to_be32(IRQ_M_TIMER);

        g_free(intc_name);
    }

    mem_addr = memmap[ANDES_AE350_DRAM].base;
    mem_name = g_strdup_printf("/memory@%lx", (long)mem_addr);
    qemu_fdt_add_subnode(fdt, mem_name);
    qemu_fdt_setprop_cells(fdt, mem_name, "reg",
        mem_addr >> 32, mem_addr, mem_size >> 32, mem_size);
    qemu_fdt_setprop_string(fdt, mem_name, "device_type", "memory");
    g_free(mem_name);

    /* create plic */
    plic_phandle = phandle++;
    plic_addr = memmap[ANDES_AE350_PLIC].base;
    plic_name = g_strdup_printf("/soc/interrupt-controller@%lx", plic_addr);
    qemu_fdt_add_subnode(fdt, plic_name);
    qemu_fdt_setprop_cell(fdt, plic_name,
        "#address-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, plic_name,
        "#interrupt-cells", 0x2);
    qemu_fdt_setprop_string(fdt, plic_name, "compatible", "riscv,plic0");
    qemu_fdt_setprop(fdt, plic_name, "interrupt-controller", NULL, 0);
    qemu_fdt_setprop(fdt, plic_name, "interrupts-extended",
        plic_irq_ext, s->cpus.num_harts * sizeof(uint32_t) * 4);
    qemu_fdt_setprop_cells(fdt, plic_name, "reg",
        0x0, plic_addr, 0x0, memmap[ANDES_AE350_PLIC].size);
    qemu_fdt_setprop_cell(fdt, plic_name, "riscv,ndev", 0x47);
    qemu_fdt_setprop_cell(fdt, plic_name, "phandle", plic_phandle);
    g_free(plic_name);
    g_free(plic_irq_ext);

    /* create plicsw */
    plicsw_addr = memmap[ANDES_AE350_PLICSW].base;
    plicsw_name = g_strdup_printf("/soc/interrupt-controller@%lx", plicsw_addr);
    qemu_fdt_add_subnode(fdt, plicsw_name);
    qemu_fdt_setprop_cell(fdt, plicsw_name,
        "#address-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, plicsw_name,
        "#interrupt-cells", 0x2);
    qemu_fdt_setprop_string(fdt, plicsw_name, "compatible", "riscv,plic1");
    qemu_fdt_setprop(fdt, plicsw_name, "interrupt-controller", NULL, 0);
    qemu_fdt_setprop(fdt, plicsw_name, "interrupts-extended",
        plicsw_irq_ext, s->cpus.num_harts * sizeof(uint32_t) * 2);
    qemu_fdt_setprop_cells(fdt, plicsw_name, "reg",
        0x0, plicsw_addr, 0x0, memmap[ANDES_AE350_PLICSW].size);
    qemu_fdt_setprop_cell(fdt, plicsw_name, "riscv,ndev", 0x1);
    g_free(plicsw_name);
    g_free(plicsw_irq_ext);

    /* create plmt */
    plmt_addr = memmap[ANDES_AE350_PLMT].base;
    plmt_name = g_strdup_printf("/soc/plmt0@%lx", plmt_addr);
    qemu_fdt_add_subnode(fdt, plmt_name);
    qemu_fdt_setprop_string(fdt, plmt_name, "compatible", "riscv,plmt0");
    qemu_fdt_setprop(fdt, plmt_name, "interrupts-extended",
        plmt_irq_ext, s->cpus.num_harts * sizeof(uint32_t) * 2);
    qemu_fdt_setprop_cells(fdt, plmt_name, "reg",
        0x0, plmt_addr, 0x0, memmap[ANDES_AE350_PLMT].size);
    g_free(plmt_name);
    g_free(plmt_irq_ext);

    uart_name = g_strdup_printf("/serial@%lx", memmap[ANDES_AE350_UART1].base);
    qemu_fdt_add_subnode(fdt, uart_name);
    qemu_fdt_setprop_string(fdt, uart_name, "compatible", "ns16550a");
    qemu_fdt_setprop_cells(fdt, uart_name, "reg",
        0x0, memmap[ANDES_AE350_UART1].base,
        0x0, memmap[ANDES_AE350_UART1].size);
    qemu_fdt_setprop_cell(fdt, uart_name, "clock-frequency", 3686400);
    qemu_fdt_setprop_cell(fdt, uart_name, "reg-shift", ANDES_UART_REG_SHIFT);
    qemu_fdt_setprop_cell(fdt, uart_name, "reg-offset", ANDES_UART_REG_OFFSET);
    qemu_fdt_setprop_cell(fdt, uart_name, "interrupt-parent", plic_phandle);
    qemu_fdt_setprop_cells(fdt, uart_name, "interrupts",
                            ANDES_AE350_UART1_IRQ, 0x4);

    uart_name = g_strdup_printf("/serial@%lx", memmap[ANDES_AE350_UART2].base);
    qemu_fdt_add_subnode(fdt, uart_name);
    qemu_fdt_setprop_string(fdt, uart_name, "compatible", "ns16550a");
    qemu_fdt_setprop_cells(fdt, uart_name, "reg",
        0x0, memmap[ANDES_AE350_UART2].base,
        0x0, memmap[ANDES_AE350_UART2].size);
    qemu_fdt_setprop_cell(fdt, uart_name, "reg-shift", ANDES_UART_REG_SHIFT);
    qemu_fdt_setprop_cell(fdt, uart_name, "reg-offset", ANDES_UART_REG_OFFSET);
    qemu_fdt_setprop_cell(fdt, uart_name, "clock-frequency", 3686400);
    qemu_fdt_setprop_cell(fdt, uart_name, "interrupt-parent", plic_phandle);
    qemu_fdt_setprop_cells(fdt, uart_name, "interrupts",
                            ANDES_AE350_UART2_IRQ, 0x4);

    qemu_fdt_add_subnode(fdt, "/chosen");
    qemu_fdt_setprop_string(fdt, "/chosen", "bootargs",
            "console=ttyS0,38400n8 earlycon=sbi debug loglevel=7");
    qemu_fdt_setprop_string(fdt, "/chosen", "stdout-path", uart_name);
    g_free(uart_name);

    for (i = 0; i < ANDES_AE350_VIRTIO_COUNT; i++) {
        virtio_name = g_strdup_printf("/virtio_mmio@%lx",
            (memmap[ANDES_AE350_VIRTIO].base +
                (i * memmap[ANDES_AE350_VIRTIO].size)));
        qemu_fdt_add_subnode(fdt, virtio_name);
        qemu_fdt_setprop_string(fdt, virtio_name, "compatible", "virtio,mmio");
        qemu_fdt_setprop_cells(fdt, virtio_name, "reg",
            0x0,
            memmap[ANDES_AE350_VIRTIO].base +
                (i * memmap[ANDES_AE350_VIRTIO].size),
            0x0,
            memmap[ANDES_AE350_VIRTIO].size);
        qemu_fdt_setprop_cell(fdt, virtio_name, "interrupt-parent",
                                plic_phandle);
        qemu_fdt_setprop_cells(fdt, virtio_name, "interrupts",
                                ANDES_AE350_VIRTIO_IRQ + i, 0x4);
        g_free(virtio_name);
    }
}

static char *init_hart_config(const char *hart_config, int num_harts)
{
    int length = 0, i = 0;
    char *result;

    length = (strlen(hart_config) + 1) * num_harts;
    result = g_malloc0(length);
    for (i = 0; i < num_harts; i++) {
        if (i != 0) {
            strncat(result, ",", length);
        }
        strncat(result, hart_config, length);
        length -= (strlen(hart_config) + 1);
    }

    return result;
}

static void andes_ae350_soc_realize(DeviceState *dev_soc, Error **errp)
{
    const struct MemmapEntry *memmap = andes_ae350_memmap;
    MachineState *machine = MACHINE(qdev_get_machine());
    MemoryRegion *system_memory = get_system_memory();
    AndesAe350SocState *s = ANDES_AE350_SOC(dev_soc);
    char *plic_hart_config, *plicsw_hart_config;
    NICInfo *nd = &nd_table[0];

    if (s->ilm_size < 0x1000 || s->ilm_size > 0x1000000) {
        error_report("Cannot set instruction local memory size beyond the range"
                     "of 0x1000 to 0x1000000");
        exit(1);
    }
    if (s->dlm_size < 0x1000 || s->dlm_size > 0x1000000) {
        error_report("Cannot set data local memory size beyond the range of"
                     "0x1000 to 0x1000000");
        exit(1);
    }

    /* round down local memory size to valid value */
    s->ilm_size = 1 << (31 - __builtin_clz(s->ilm_size));
    s->dlm_size = 1 << (31 - __builtin_clz(s->dlm_size));
    /* align local memory base to local memory size */
    s->ilm_base &= ~(s->ilm_size - 1);
    s->dlm_base &= ~(s->dlm_size - 1);

    /* Set riscv_harts properties for Local Memory */
    qdev_prop_set_bit(DEVICE(&s->cpus), "ilm_default_enable",
                        s->ilm_default_enable);
    qdev_prop_set_bit(DEVICE(&s->cpus), "dlm_default_enable",
                        s->dlm_default_enable);
    qdev_prop_set_uint64(DEVICE(&s->cpus), "ilm_base",
                        s->ilm_base);
    qdev_prop_set_uint64(DEVICE(&s->cpus), "dlm_base",
                        s->dlm_base);
    qdev_prop_set_uint64(DEVICE(&s->cpus), "ilm_size",
                        s->ilm_size);
    qdev_prop_set_uint64(DEVICE(&s->cpus), "dlm_size",
                        s->dlm_size);

    sysbus_realize(SYS_BUS_DEVICE(&s->cpus), &error_abort);

    plicsw_hart_config =
        init_hart_config(ANDES_PLICSW_HART_CONFIG, machine->smp.cpus);

    /* Per-socket SW-PLIC */
    s->plic_sw = andes_plic_create(
        memmap[ANDES_AE350_PLICSW].base,
        ANDES_PLICSW_NAME,
        plicsw_hart_config, machine->smp.cpus,
        ANDES_PLICSW_NUM_SOURCES,
        ANDES_PLICSW_NUM_PRIORITIES,
        ANDES_PLICSW_PRIORITY_BASE,
        ANDES_PLICSW_PENDING_BASE,
        ANDES_PLICSW_ENABLE_BASE,
        ANDES_PLICSW_ENABLE_STRIDE,
        ANDES_PLICSW_THRESHOLD_BASE,
        ANDES_PLICSW_THRESHOLD_STRIDE,
        memmap[ANDES_AE350_PLICSW].size);

    g_free(plicsw_hart_config);

    andes_plmt_create(memmap[ANDES_AE350_PLMT].base,
                      memmap[ANDES_AE350_PLMT].size,
                      machine->smp.cpus,
                      ANDES_PLMT_TIME_BASE,
                      ANDES_PLMT_TIMECMP_BASE,
                      ANDES_PLMT_TIMEBASE_FREQ);

    plic_hart_config =
        init_hart_config(ANDES_PLIC_HART_CONFIG, machine->smp.cpus);

    /* Per-socket PLIC */
    s->plic = andes_plic_create(
        memmap[ANDES_AE350_PLIC].base,
        ANDES_PLIC_NAME,
        plic_hart_config, machine->smp.cpus,
        ANDES_PLIC_NUM_SOURCES,
        ANDES_PLIC_NUM_PRIORITIES,
        ANDES_PLIC_PRIORITY_BASE,
        ANDES_PLIC_PENDING_BASE,
        ANDES_PLIC_ENABLE_BASE,
        ANDES_PLIC_ENABLE_STRIDE,
        ANDES_PLIC_THRESHOLD_BASE,
        ANDES_PLIC_THRESHOLD_STRIDE,
        memmap[ANDES_AE350_PLIC].size);

    g_free(plic_hart_config);

    /* VIRTIO */
    for (int i = 0; i < ANDES_AE350_VIRTIO_COUNT; i++) {
        sysbus_create_simple("virtio-mmio",
            (memmap[ANDES_AE350_VIRTIO].base +
                (i * memmap[ANDES_AE350_VIRTIO].size)),
            qdev_get_gpio_in(DEVICE(s->plic), (ANDES_AE350_VIRTIO_IRQ + i)));
    }

    /* SMU */
    andes_atcsmu_create(&s->atcsmu, memmap[ANDES_AE350_SMU].base,
                        memmap[ANDES_AE350_SMU].size);

    /* SMC */
    create_unimplemented_device("riscv.andes.ae350.smc",
        memmap[ANDES_AE350_SMC].base, memmap[ANDES_AE350_SMC].size);

    /* SPI */
    create_unimplemented_device("riscv.andes.ae350.spi",
        memmap[ANDES_AE350_SPI].base, memmap[ANDES_AE350_SPI].size);

    /* RTC */
    atcrtc100_create(memmap[ANDES_AE350_RTC].base,
                     qdev_get_gpio_in(DEVICE(s->plic),
                     ANDES_AE350_RTC_PERIOD_IRQ),
                     qdev_get_gpio_in(DEVICE(s->plic),
                     ANDES_AE350_RTC_ALARM_IRQ));

    /* GPIO */
    create_unimplemented_device("riscv.andes.ae350.gpio",
        memmap[ANDES_AE350_GPIO].base, memmap[ANDES_AE350_GPIO].size);

    /* I2C */
    create_unimplemented_device("riscv.andes.ae350.i2c",
        memmap[ANDES_AE350_I2C].base, memmap[ANDES_AE350_I2C].size);

    /* LCD */
    create_unimplemented_device("riscv.andes.ae350.lcd",
        memmap[ANDES_AE350_LCD].base, memmap[ANDES_AE350_LCD].size);

    /* SND */
    create_unimplemented_device("riscv.andes.ae350.snd",
        memmap[ANDES_AE350_SND].base, memmap[ANDES_AE350_SND].size);

    /* DMAC */
    atcdmac300_create(&s->dma, "atcdmac300",
                memmap[ANDES_AE350_DMAC].base,
                memmap[ANDES_AE350_DMAC].size,
                qdev_get_gpio_in(DEVICE(s->plic), ANDES_AE350_DMAC_IRQ));

    /* NIC */
    atfmac100_create(&s->atfmac100, "atfmac100",
                 nd, memmap[ANDES_AE350_MAC].base,
                 qdev_get_gpio_in(DEVICE(s->plic), ANDES_AE350_MAC_IRQ));

    /* PIT */
    atcpit100_create(memmap[ANDES_AE350_PIT].base,
                qdev_get_gpio_in(DEVICE(s->plic), ANDES_AE350_PIT_IRQ));

    /* SDC */
    atfsdc010_create(memmap[ANDES_AE350_SDC].base,
                qdev_get_gpio_in(DEVICE(s->plic), ANDES_AE350_SDC_IRQ));

    /* SPI2 */
    create_unimplemented_device("riscv.andes.ae350.spi2",
        memmap[ANDES_AE350_SPI2].base, memmap[ANDES_AE350_SPI2].size);

    /* WDT */
    atcwdt200_create(memmap[ANDES_AE350_WDT].base);

    /* UART */
    serial_mm_init(system_memory,
        memmap[ANDES_AE350_UART1].base + ANDES_UART_REG_OFFSET,
        ANDES_UART_REG_SHIFT,
        qdev_get_gpio_in(DEVICE(s->plic), ANDES_AE350_UART1_IRQ),
        38400, serial_hd(1), DEVICE_LITTLE_ENDIAN);

    serial_mm_init(system_memory,
        memmap[ANDES_AE350_UART2].base + ANDES_UART_REG_OFFSET,
        ANDES_UART_REG_SHIFT,
        qdev_get_gpio_in(DEVICE(s->plic), ANDES_AE350_UART2_IRQ),
        38400, serial_hd(0), DEVICE_LITTLE_ENDIAN);
}

static void andes_ae350_soc_instance_init(Object *obj)
{
    const struct MemmapEntry *memmap = andes_ae350_memmap;
    MachineState *machine = MACHINE(qdev_get_machine());
    AndesAe350SocState *s = ANDES_AE350_SOC(obj);

    object_initialize_child(obj, "atcdmac300", &s->dma,
                                TYPE_ATCDMAC300);

    object_initialize_child(obj, "atfmac100", &s->atfmac100,
                                TYPE_ATFMAC100);

    object_initialize_child(obj, "atcsmu", &s->atcsmu,
                            TYPE_ANDES_ATCSMU);

    object_initialize_child(obj, "cpus", &s->cpus, TYPE_RISCV_HART_ARRAY);
    object_property_set_str(OBJECT(&s->cpus), "cpu-type",
                            machine->cpu_type, &error_abort);
    object_property_set_int(OBJECT(&s->cpus), "num-harts",
                            machine->smp.cpus, &error_abort);
    qdev_prop_set_uint64(DEVICE(&s->cpus), "resetvec",
                            memmap[ANDES_AE350_MROM].base);
}

static int andes_load_elf(MachineState *machine,
                          const char *default_machine_firmware)
{
    char *firmware_filename = NULL;
    bool elf_is64;
    union {
        Elf32_Ehdr h32;
        Elf64_Ehdr h64;
    } elf_header;
    Error *err = NULL;

    firmware_filename = riscv_find_firmware(machine->firmware,
                                            default_machine_firmware);

    /* If not "none" load the firmware */
    if (firmware_filename) {
        load_elf_hdr(firmware_filename, &elf_header, &elf_is64, &err);

        if (err) {
            error_free(err);
            exit(1);
        }

        if (elf_is64) {
            return elf_header.h64.e_entry;
        } else {
            return elf_header.h32.e_entry;
        }
    }

    return 0;
}

typedef struct Slaveport_status {
    int hart_id;
    bool dlm;
} Slaveport_status;
static uint64_t slaveport_read(void *opaque, hwaddr addr, unsigned size)
{
    uint64_t ret = 0;
    Slaveport_status *s = (Slaveport_status *)opaque;
    CPUState *cs = qemu_get_cpu(s->hart_id);
    if (!cs) {
        return ret;
    }
    CPURISCVState *env = &RISCV_CPU(cs)->env;
    if (s->dlm) {
        memory_region_dispatch_read(env->mask_dlm, addr, &ret,
            size_memop(size) | MO_LE, (MemTxAttrs) { .memory = 1 });
    } else {
        memory_region_dispatch_read(env->mask_ilm, addr, &ret,
            size_memop(size) | MO_LE, (MemTxAttrs) { .memory = 1 });
    }
    return ret;
}

static void slaveport_write(void *opaque, hwaddr addr, uint64_t value,
                            unsigned size)
{
    Slaveport_status *s = (Slaveport_status *)opaque;
    CPUState *cs = qemu_get_cpu(s->hart_id);
    if (!cs) {
        return;
    }
    CPURISCVState *env = &RISCV_CPU(cs)->env;
    if (s->dlm) {
        memory_region_dispatch_write(env->mask_dlm, addr, value,
            size_memop(size) | MO_LE, (MemTxAttrs) { .memory = 1 });
    } else {
        memory_region_dispatch_write(env->mask_ilm, addr, value,
            size_memop(size) | MO_LE, (MemTxAttrs) { .memory = 1 });
    }
}

static const MemoryRegionOps slaveport_ops = {
    .read = slaveport_read,
    .write = slaveport_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
        .unaligned = true,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
        .unaligned = true,
    },
};

static void slaveport_create(int hart_id, bool dlm, hwaddr base, hwaddr size)
{
    Slaveport_status *s = g_new(Slaveport_status, 1);
    s->hart_id = hart_id;
    s->dlm = dlm;
    MemoryRegion *slaveport_mr = g_new(MemoryRegion, 1);
    MemoryRegion *system_memory = get_system_memory();
    char *name;
    if (dlm) {
        name = g_strdup_printf("%s%d_%s", "riscv.andes.ae350.slaveport",
                               hart_id, "dlm");
    } else {
        name = g_strdup_printf("%s%d_%s", "riscv.andes.ae350.slaveport",
                               hart_id, "ilm");
    }
    memory_region_init_io(slaveport_mr, NULL, &slaveport_ops, s,
                          name, size);
    memory_region_add_subregion(system_memory, base, slaveport_mr);
}

static void andes_ae350_machine_init(MachineState *machine)
{
    const struct MemmapEntry *memmap = andes_ae350_memmap;

    AndesAe350BoardState *bs = ANDES_AE350_MACHINE(machine);
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *main_mem = g_new(MemoryRegion, 1);
    MemoryRegion *mask_rom = g_new(MemoryRegion, 1);
    MemoryRegion *mask_nor = g_new(MemoryRegion, 1);
    MemoryRegion *mask_l2c = g_new(MemoryRegion, 1);
    target_ulong start_addr = memmap[ANDES_AE350_DRAM].base;
    target_ulong firmware_end_addr, kernel_start_addr;
    uint32_t fdt_load_addr;
    uint64_t kernel_entry;

    /* Initialize SoC */
    object_initialize_child(OBJECT(machine), "soc",
                    &bs->soc, TYPE_ANDES_AE350_SOC);
    qdev_realize(DEVICE(&bs->soc), NULL, &error_abort);

    /* Check ram size is validate */
    if (machine->ram_size > memmap[ANDES_AE350_DRAM].size) {
        error_report("Cannot model more than %ldGB RAM",
            memmap[ANDES_AE350_DRAM].size / (1024 * 1024 * 1024));
        exit(1);
    }

    /* register system main memory (actual RAM) */
    memory_region_init_ram(main_mem, NULL, "riscv.andes.ae350.ram",
                           machine->ram_size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[ANDES_AE350_DRAM].base,
        main_mem);

    /* NOR FLASH */
    memory_region_init_rom(mask_nor, NULL, "riscv.andes.ae350.nor",
                           memmap[ANDES_AE350_NOR].size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[ANDES_AE350_NOR].base,
                                mask_nor);

    /* L2C */
    memory_region_init_ram(mask_l2c, NULL, "riscv.andes.ae350.l2c",
                           memmap[ANDES_AE350_L2C].size, &error_fatal);
    memory_region_set_readonly(mask_l2c, false);
    memory_region_add_subregion(system_memory, memmap[ANDES_AE350_L2C].base,
                                mask_l2c);

    for (int i = 0 ; i < ANDES_LM_SLAVEPORTS_MAX; i++) {
        struct MemmapEntry silm_map =
            memmap[ANDES_AE350_SLAVEPORT0_ILM + i * 2];
        struct MemmapEntry sdlm_map =
             memmap[ANDES_AE350_SLAVEPORT0_ILM + i * 2 + 1];
        slaveport_create(i, 0, silm_map.base, silm_map.size);
        slaveport_create(i, 1, sdlm_map.base, sdlm_map.size);
    }

    /* load/create device tree */
    if (machine->dtb) {
        machine->fdt = load_device_tree(machine->dtb, &bs->fdt_size);
        if (!machine->fdt) {
            error_report("load_device_tree() failed");
            exit(1);
        }
    } else {
        create_fdt(bs, memmap, machine->ram_size);
    }

    if (machine->kernel_cmdline && *machine->kernel_cmdline) {
        qemu_fdt_setprop_string(machine->fdt, "/chosen", "bootargs",
                                machine->kernel_cmdline);
    }

    /* boot rom */
    memory_region_init_rom(mask_rom, NULL, "riscv.andes.ae350.mrom",
                           memmap[ANDES_AE350_MROM].size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[ANDES_AE350_MROM].base,
                                mask_rom);

    start_addr = andes_load_elf(machine, BIOS_FILENAME);
    firmware_end_addr = riscv_find_and_load_firmware(machine, BIOS_FILENAME,
                                                     start_addr, NULL);
    if (machine->kernel_filename) {
        kernel_start_addr = riscv_calc_kernel_start_addr(&bs->soc.cpus,
                                                         firmware_end_addr);

        kernel_entry = riscv_load_kernel(machine, &bs->soc.cpus,
                                         kernel_start_addr, true,
                                         NULL);
    } else {
       /*
        * If dynamic firmware is used, it doesn't know where is the next mode
        * if kernel argument is not set.
        */
        kernel_entry = 0;
    }

    /* Compute the fdt load address in dram */
    fdt_load_addr = riscv_compute_fdt_addr(memmap[ANDES_AE350_DRAM].base,
                                           memmap[ANDES_AE350_DRAM].size,
                                           machine);
    riscv_load_fdt(fdt_load_addr, machine->fdt);

    /* load the reset vector */
    riscv_setup_rom_reset_vec(machine, &bs->soc.cpus, start_addr,
                andes_ae350_memmap[ANDES_AE350_MROM].base,
                andes_ae350_memmap[ANDES_AE350_MROM].size,
                kernel_entry, fdt_load_addr);
}

static void ae350_do_nmi_on_cpu(CPUState *cs, run_on_cpu_data arg)
{
    RISCVCPU *cpu = RISCV_CPU(cs);
    CPURISCVState *env = &cpu->env;
    env->mcause = 0x1;
    env->mepc = env->pc;
    env->pc = env->resetvec;
}

static void ae350_nmi(NMIState *n, int cpu_index, Error **errp)
{
    CPUState *cs = qemu_get_cpu(cpu_index);
    async_run_on_cpu(cs, ae350_do_nmi_on_cpu, RUN_ON_CPU_NULL);
}

static void andes_ae350_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    NMIClass *nc = NMI_CLASS(oc);
    mc->desc = "RISC-V Board compatible with Andes AE350";
    mc->init = andes_ae350_machine_init;
    mc->max_cpus = ANDES_CPUS_MAX;
    mc->default_cpu_type = VIRT_CPU;
    nc->nmi_monitor_handler = ae350_nmi;
}

static void andes_ae350_machine_instance_init(Object *obj)
{

}

static const TypeInfo andes_ae350_machine_typeinfo = {
    .name       = MACHINE_TYPE_NAME("andes_ae350"),
    .parent     = TYPE_MACHINE,
    .class_init = andes_ae350_machine_class_init,
    .instance_init = andes_ae350_machine_instance_init,
    .instance_size = sizeof(AndesAe350BoardState),
    .interfaces = (InterfaceInfo[]) {
         { TYPE_NMI },
         { }
    },
};

static void andes_ae350_machine_init_register_types(void)
{
    type_register_static(&andes_ae350_machine_typeinfo);
}

type_init(andes_ae350_machine_init_register_types)


static Property andes_ae350_soc_property[] = {
    /* Defaults for standard extensions */
    DEFINE_PROP_UINT64("ilm_base", AndesAe350SocState, ilm_base, 0),
    DEFINE_PROP_UINT64("dlm_base", AndesAe350SocState, dlm_base, 0x200000),
    DEFINE_PROP_UINT32("ilm_size", AndesAe350SocState, ilm_size, 0x200000),
    DEFINE_PROP_UINT32("dlm_size", AndesAe350SocState, dlm_size, 0x200000),
    DEFINE_PROP_BOOL("ilm_default_enable", AndesAe350SocState,
                     ilm_default_enable, false),
    DEFINE_PROP_BOOL("dlm_default_enable", AndesAe350SocState,
                     dlm_default_enable, false),
    DEFINE_PROP_END_OF_LIST(),
};

static void andes_ae350_soc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    device_class_set_props(dc, andes_ae350_soc_property);
    dc->realize = andes_ae350_soc_realize;
    dc->user_creatable = false;
}

static const TypeInfo andes_ae350_soc_type_info = {
    .name       = TYPE_ANDES_AE350_SOC,
    .parent     = TYPE_DEVICE,
    .instance_init = andes_ae350_soc_instance_init,
    .instance_size = sizeof(AndesAe350SocState),
    .class_init = andes_ae350_soc_class_init,
};

static void andes_ae350_soc_init_register_types(void)
{
    type_register_static(&andes_ae350_soc_type_info);
}

type_init(andes_ae350_soc_init_register_types)
