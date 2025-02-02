/*
 * RISC-V PMU file.
 *
 * Copyright (c) 2021 Western Digital Corporation or its affiliates.
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
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "cpu.h"
#include "pmu.h"
#include "sysemu/cpu-timers.h"
#include "sysemu/device_tree.h"
#include "qemu/andes-config.h"

static uint64_t riscv_timebase_freq = 1000000000; /* 1Ghz */

/*
 * To keep it simple, any event can be mapped to any programmable counters in
 * QEMU. The generic cycle & instruction count events can also be monitored
 * using programmable counters. In that case, mcycle & minstret must continue
 * to provide the correct value as well. Heterogeneous PMU per hart is not
 * supported yet. Thus, number of counters are same across all harts.
 */
void riscv_pmu_generate_fdt_node(void *fdt, uint32_t cmask, char *pmu_name)
{
    uint32_t fdt_event_ctr_map[15] = {};

   /*
    * The event encoding is specified in the SBI specification
    * Event idx is a 20bits wide number encoded as follows:
    * event_idx[19:16] = type
    * event_idx[15:0] = code
    * The code field in cache events are encoded as follows:
    * event_idx.code[15:3] = cache_id
    * event_idx.code[2:1] = op_id
    * event_idx.code[0:0] = result_id
    */

   /* SBI_PMU_HW_CPU_CYCLES: 0x01 : type(0x00) */
   fdt_event_ctr_map[0] = cpu_to_be32(0x00000001);
   fdt_event_ctr_map[1] = cpu_to_be32(0x00000001);
   fdt_event_ctr_map[2] = cpu_to_be32(cmask | 1 << 0);

   /* SBI_PMU_HW_INSTRUCTIONS: 0x02 : type(0x00) */
   fdt_event_ctr_map[3] = cpu_to_be32(0x00000002);
   fdt_event_ctr_map[4] = cpu_to_be32(0x00000002);
   fdt_event_ctr_map[5] = cpu_to_be32(cmask | 1 << 2);

   /* SBI_PMU_HW_CACHE_DTLB : 0x03 READ : 0x00 MISS : 0x00 type(0x01) */
   fdt_event_ctr_map[6] = cpu_to_be32(0x00010019);
   fdt_event_ctr_map[7] = cpu_to_be32(0x00010019);
   fdt_event_ctr_map[8] = cpu_to_be32(cmask);

   /* SBI_PMU_HW_CACHE_DTLB : 0x03 WRITE : 0x01 MISS : 0x00 type(0x01) */
   fdt_event_ctr_map[9] = cpu_to_be32(0x0001001B);
   fdt_event_ctr_map[10] = cpu_to_be32(0x0001001B);
   fdt_event_ctr_map[11] = cpu_to_be32(cmask);

   /* SBI_PMU_HW_CACHE_ITLB : 0x04 READ : 0x00 MISS : 0x00 type(0x01) */
   fdt_event_ctr_map[12] = cpu_to_be32(0x00010021);
   fdt_event_ctr_map[13] = cpu_to_be32(0x00010021);
   fdt_event_ctr_map[14] = cpu_to_be32(cmask);

   /* This a OpenSBI specific DT property documented in OpenSBI docs */
   qemu_fdt_setprop(fdt, pmu_name, "riscv,event-to-mhpmcounters",
                    fdt_event_ctr_map, sizeof(fdt_event_ctr_map));
}

static bool riscv_pmu_counter_valid(RISCVCPU *cpu, uint32_t ctr_idx)
{
    if (ctr_idx < 3 || ctr_idx >= RV_MAX_MHPMCOUNTERS ||
        !(cpu->pmu_avail_ctrs & BIT(ctr_idx))) {
        return false;
    } else {
        return true;
    }
}

static bool riscv_pmu_counter_enabled(RISCVCPU *cpu, uint32_t ctr_idx)
{
    CPURISCVState *env = &cpu->env;

    if (riscv_pmu_counter_valid(cpu, ctr_idx) &&
        !get_field(env->mcountinhibit, BIT(ctr_idx))) {
        return true;
    } else {
        return false;
    }
}

static bool riscv_pmu_has_andes_pmnds(RISCVCPU *cpu)
{
    if (cpu->cfg.ext_XAndesV5Ops == false) {
        return false;
    }

    target_ulong mmsc_cfg = cpu->env.andes_csr.csrno[CSR_MMSC_CFG];

    if (mmsc_cfg & MASK_MMSC_CFG_PMNDS) {
        return true;
    } else {
        return false;
    }
}

static void riscv_pmu_handle_andes_pmovi_interrupt(CPURISCVState *env,
                                                   uint32_t ctr_idx)
{
    /* HPM7~HPM31 are not implemented */
    if (ctr_idx > 6) {
        return;
    }

    uint32_t ctr_mask = 1 << ctr_idx;
    /* only need to set mcounterovf because scounterovf is an alias of it */
    env->andes_csr.csrno[CSR_MCOUNTEROVF] |= ctr_mask;

    if (riscv_has_ext(env, RVU) && riscv_has_ext(env, RVS)) {
        uint32_t mcountermask_m = env->andes_csr.csrno[CSR_MCOUNTERMASK_M];
        if (mcountermask_m & ctr_mask) {
            /* invoke s-mode interrupt if enabled*/
            uint32_t scounterinten = env->andes_csr.csrno[CSR_SCOUNTERINTEN];
            if (scounterinten & ctr_mask) {
                env->andes_csr.csrno[CSR_SLIP] |= MASK_LOCAL_IRQ_PMOVI;
                env->andes_csr.csrno[CSR_SDCAUSE] = 0;
                cpu_interrupt(env_cpu(env), CPU_INTERRUPT_HARD);
            }
            return;
        }
    }

    /* invoke m-mode interrupt if enabled */
    uint32_t mcounterinten = env->andes_csr.csrno[CSR_MCOUNTERINTEN];
    if (mcounterinten & ctr_mask) {
        env->andes_csr.csrno[CSR_MDCAUSE] = 0;
        riscv_cpu_update_mip(env, MIP_ANDES_PMOVI, BOOL_TO_MASK(1));
    }
}

static int riscv_pmu_incr_ctr_rv32(RISCVCPU *cpu, uint32_t ctr_idx)
{
    CPURISCVState *env = &cpu->env;
    target_ulong max_val = UINT32_MAX;
    PMUCTRState *counter = &env->pmu_ctrs[ctr_idx];
    bool virt_on = env->virt_enabled;

    if (!riscv_pmu_has_andes_pmnds(cpu)) {
        /* Privilege mode filtering */
        if ((env->priv == PRV_M &&
            (env->mhpmeventh_val[ctr_idx] & MHPMEVENTH_BIT_MINH)) ||
            (env->priv == PRV_S && virt_on &&
            (env->mhpmeventh_val[ctr_idx] & MHPMEVENTH_BIT_VSINH)) ||
            (env->priv == PRV_U && virt_on &&
            (env->mhpmeventh_val[ctr_idx] & MHPMEVENTH_BIT_VUINH)) ||
            (env->priv == PRV_S && !virt_on &&
            (env->mhpmeventh_val[ctr_idx] & MHPMEVENTH_BIT_SINH)) ||
            (env->priv == PRV_U && !virt_on &&
            (env->mhpmeventh_val[ctr_idx] & MHPMEVENTH_BIT_UINH))) {
            return 0;
        }
    }

    /* Handle the overflow scenario */
    if (counter->mhpmcounter_val == max_val) {
        if (counter->mhpmcounterh_val == max_val) {
            counter->mhpmcounter_val = 0;
            counter->mhpmcounterh_val = 0;

            if (riscv_pmu_has_andes_pmnds(cpu)) {
                riscv_pmu_handle_andes_pmovi_interrupt(env, ctr_idx);
            } else {
                /* Generate interrupt only if OF bit is clear */
                if (!(env->mhpmeventh_val[ctr_idx] & MHPMEVENTH_BIT_OF)) {
                    env->mhpmeventh_val[ctr_idx] |= MHPMEVENTH_BIT_OF;
                    riscv_cpu_update_mip(env, MIP_LCOFIP, BOOL_TO_MASK(1));
                }
            }
        } else {
            counter->mhpmcounterh_val++;
        }
    } else {
        counter->mhpmcounter_val++;
    }

    return 0;
}

static int riscv_pmu_incr_ctr_rv64(RISCVCPU *cpu, uint32_t ctr_idx)
{
    CPURISCVState *env = &cpu->env;
    PMUCTRState *counter = &env->pmu_ctrs[ctr_idx];
    uint64_t max_val = UINT64_MAX;
    bool virt_on = env->virt_enabled;

    if (!riscv_pmu_has_andes_pmnds(cpu)) {
        /* Privilege mode filtering */
        if ((env->priv == PRV_M &&
            (env->mhpmevent_val[ctr_idx] & MHPMEVENT_BIT_MINH)) ||
            (env->priv == PRV_S && virt_on &&
            (env->mhpmevent_val[ctr_idx] & MHPMEVENT_BIT_VSINH)) ||
            (env->priv == PRV_U && virt_on &&
            (env->mhpmevent_val[ctr_idx] & MHPMEVENT_BIT_VUINH)) ||
            (env->priv == PRV_S && !virt_on &&
            (env->mhpmevent_val[ctr_idx] & MHPMEVENT_BIT_SINH)) ||
            (env->priv == PRV_U && !virt_on &&
            (env->mhpmevent_val[ctr_idx] & MHPMEVENT_BIT_UINH))) {
            return 0;
        }
    }

    /* Handle the overflow scenario */
    if (counter->mhpmcounter_val == max_val) {
        counter->mhpmcounter_val = 0;

        if (riscv_pmu_has_andes_pmnds(cpu)) {
            riscv_pmu_handle_andes_pmovi_interrupt(env, ctr_idx);
        } else {
            /* Generate interrupt only if OF bit is clear */
            if (!(env->mhpmeventh_val[ctr_idx] & MHPMEVENTH_BIT_OF)) {
                env->mhpmeventh_val[ctr_idx] |= MHPMEVENTH_BIT_OF;
                riscv_cpu_update_mip(env, MIP_LCOFIP, BOOL_TO_MASK(1));
            }
        }
    } else {
        counter->mhpmcounter_val++;
    }
    return 0;
}

int riscv_pmu_incr_ctr(RISCVCPU *cpu, enum riscv_pmu_event_idx event_idx)
{
    uint32_t ctr_idx;
    int ret;
    CPURISCVState *env = &cpu->env;
    gpointer value;

    if (!cpu->cfg.pmu_mask) {
        return 0;
    }
    value = g_hash_table_lookup(cpu->pmu_event_ctr_map,
                                GUINT_TO_POINTER(event_idx));
    if (!value) {
        return -1;
    }

    ctr_idx = GPOINTER_TO_UINT(value);
    if (!riscv_pmu_counter_enabled(cpu, ctr_idx) ||
        get_field(env->mcountinhibit, BIT(ctr_idx))) {
        return -1;
    }

    if (riscv_cpu_mxl(env) == MXL_RV32) {
        ret = riscv_pmu_incr_ctr_rv32(cpu, ctr_idx);
    } else {
        ret = riscv_pmu_incr_ctr_rv64(cpu, ctr_idx);
    }

    return ret;
}

bool riscv_pmu_ctr_monitor_instructions(CPURISCVState *env,
                                        uint32_t target_ctr)
{
    RISCVCPU *cpu;
    uint32_t event_idx;
    uint32_t ctr_idx;

    /* Fixed instret counter */
    if (target_ctr == 2) {
        return true;
    }

    cpu = env_archcpu(env);
    if (!cpu->pmu_event_ctr_map) {
        return false;
    }

    if (riscv_pmu_has_andes_pmnds(cpu)) {
        event_idx = RISCV_PMU_EVENT_ANDES_INSTRUCTIONS;
    } else {
        event_idx = RISCV_PMU_EVENT_HW_INSTRUCTIONS;
    }
    ctr_idx = GPOINTER_TO_UINT(g_hash_table_lookup(cpu->pmu_event_ctr_map,
                               GUINT_TO_POINTER(event_idx)));
    if (!ctr_idx) {
        return false;
    }

    return target_ctr == ctr_idx ? true : false;
}

bool riscv_pmu_ctr_monitor_cycles(CPURISCVState *env, uint32_t target_ctr)
{
    RISCVCPU *cpu;
    uint32_t event_idx;
    uint32_t ctr_idx;

    /* Fixed mcycle counter */
    if (target_ctr == 0) {
        return true;
    }

    cpu = env_archcpu(env);
    if (!cpu->pmu_event_ctr_map) {
        return false;
    }

    if (riscv_pmu_has_andes_pmnds(cpu)) {
        event_idx = RISCV_PMU_EVENT_ANDES_CPU_CYCLES;
    } else {
        event_idx = RISCV_PMU_EVENT_HW_CPU_CYCLES;
    }
    ctr_idx = GPOINTER_TO_UINT(g_hash_table_lookup(cpu->pmu_event_ctr_map,
                               GUINT_TO_POINTER(event_idx)));

    /* Counter zero is not used for event_ctr_map */
    if (!ctr_idx) {
        return false;
    }

    return (target_ctr == ctr_idx) ? true : false;
}

static gboolean pmu_remove_event_map(gpointer key, gpointer value,
                                     gpointer udata)
{
    return (GPOINTER_TO_UINT(value) == GPOINTER_TO_UINT(udata)) ? true : false;
}

static int64_t pmu_icount_ticks_to_ns(int64_t value)
{
    int64_t ret = 0;

    if (icount_enabled()) {
        ret = icount_to_ns(value);
    } else {
        ret = (NANOSECONDS_PER_SECOND / riscv_timebase_freq) * value;
    }

    return ret;
}

int riscv_pmu_update_event_map(CPURISCVState *env, uint64_t value,
                               uint32_t ctr_idx)
{
    uint32_t event_idx;
    RISCVCPU *cpu = env_archcpu(env);

    if (!riscv_pmu_counter_valid(cpu, ctr_idx) || !cpu->pmu_event_ctr_map) {
        return -1;
    }

    /*
     * Expected mhpmevent value is zero for reset case. Remove the current
     * mapping.
     */
    if (!value) {
        g_hash_table_foreach_remove(cpu->pmu_event_ctr_map,
                                    pmu_remove_event_map,
                                    GUINT_TO_POINTER(ctr_idx));
        return 0;
    }

    event_idx = value & MHPMEVENT_IDX_MASK;
    if (g_hash_table_lookup(cpu->pmu_event_ctr_map,
                            GUINT_TO_POINTER(event_idx))) {
        return 0;
    }

    if (riscv_pmu_has_andes_pmnds(cpu)) {
        switch (event_idx) {
        case RISCV_PMU_EVENT_ANDES_CPU_CYCLES:
        case RISCV_PMU_EVENT_ANDES_INSTRUCTIONS:
        case RISCV_PMU_EVENT_ANDES_DTLB_MISS:
        case RISCV_PMU_EVENT_ANDES_ITLB_MISS:
            break;
        default:
            /* We don't support any raw events right now */
            return -1;
        }
    } else {
        switch (event_idx) {
        case RISCV_PMU_EVENT_HW_CPU_CYCLES:
        case RISCV_PMU_EVENT_HW_INSTRUCTIONS:
        case RISCV_PMU_EVENT_CACHE_DTLB_READ_MISS:
        case RISCV_PMU_EVENT_CACHE_DTLB_WRITE_MISS:
        case RISCV_PMU_EVENT_CACHE_ITLB_PREFETCH_MISS:
            break;
        default:
            /* We don't support any raw events right now */
            return -1;
        }
    }
    g_hash_table_insert(cpu->pmu_event_ctr_map, GUINT_TO_POINTER(event_idx),
                        GUINT_TO_POINTER(ctr_idx));

    return 0;
}

static void pmu_timer_trigger_irq(RISCVCPU *cpu,
                                  enum riscv_pmu_event_idx evt_idx)
{
    uint32_t ctr_idx;
    CPURISCVState *env = &cpu->env;
    PMUCTRState *counter;
    target_ulong *mhpmevent_val;
    uint64_t of_bit_mask;
    int64_t irq_trigger_at;

    if (riscv_pmu_has_andes_pmnds(cpu)) {
        if (evt_idx != RISCV_PMU_EVENT_ANDES_CPU_CYCLES &&
            evt_idx != RISCV_PMU_EVENT_ANDES_INSTRUCTIONS) {
            return;
        }
    } else {
        if (evt_idx != RISCV_PMU_EVENT_HW_CPU_CYCLES &&
            evt_idx != RISCV_PMU_EVENT_HW_INSTRUCTIONS) {
            return;
        }
    }

    ctr_idx = GPOINTER_TO_UINT(g_hash_table_lookup(cpu->pmu_event_ctr_map,
                               GUINT_TO_POINTER(evt_idx)));
    if (!riscv_pmu_counter_enabled(cpu, ctr_idx)) {
        return;
    }

    if (riscv_cpu_mxl(env) == MXL_RV32) {
        mhpmevent_val = &env->mhpmeventh_val[ctr_idx];
        of_bit_mask = MHPMEVENTH_BIT_OF;
     } else {
        mhpmevent_val = &env->mhpmevent_val[ctr_idx];
        of_bit_mask = MHPMEVENT_BIT_OF;
    }

    counter = &env->pmu_ctrs[ctr_idx];
    if (counter->irq_overflow_left > 0) {
        irq_trigger_at = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                        counter->irq_overflow_left;
        timer_mod_anticipate_ns(cpu->pmu_timer, irq_trigger_at);
        counter->irq_overflow_left = 0;
        return;
    }

    if (cpu->pmu_avail_ctrs & BIT(ctr_idx)) {
        if (riscv_pmu_has_andes_pmnds(cpu)) {
            riscv_pmu_handle_andes_pmovi_interrupt(env, ctr_idx);
        } else {
            /* Generate interrupt only if OF bit is clear */
            if (!(*mhpmevent_val & of_bit_mask)) {
                *mhpmevent_val |= of_bit_mask;
                riscv_cpu_update_mip(env, MIP_LCOFIP, BOOL_TO_MASK(1));
            }
        }
    }
}

/* Timer callback for instret and cycle counter overflow */
void riscv_pmu_timer_cb(void *priv)
{
    RISCVCPU *cpu = priv;

    /* Timer event was triggered only for these events */
    if (riscv_pmu_has_andes_pmnds(cpu)) {
        pmu_timer_trigger_irq(cpu, RISCV_PMU_EVENT_ANDES_CPU_CYCLES);
        pmu_timer_trigger_irq(cpu, RISCV_PMU_EVENT_ANDES_INSTRUCTIONS);
    } else {
        pmu_timer_trigger_irq(cpu, RISCV_PMU_EVENT_HW_CPU_CYCLES);
        pmu_timer_trigger_irq(cpu, RISCV_PMU_EVENT_HW_INSTRUCTIONS);
    }
}

int riscv_pmu_setup_timer(CPURISCVState *env, uint64_t value, uint32_t ctr_idx)
{
    uint64_t overflow_delta, overflow_at;
    int64_t overflow_ns, overflow_left = 0;
    RISCVCPU *cpu = env_archcpu(env);
    PMUCTRState *counter = &env->pmu_ctrs[ctr_idx];

    if (!riscv_pmu_counter_valid(cpu, ctr_idx) ||
            !(cpu->cfg.ext_sscofpmf || riscv_pmu_has_andes_pmnds(cpu))) {
        return -1;
    }

    if (value) {
        overflow_delta = UINT64_MAX - value + 1;
    } else {
        overflow_delta = UINT64_MAX;
    }

    /*
     * QEMU supports only int64_t timers while RISC-V counters are uint64_t.
     * Compute the leftover and save it so that it can be reprogrammed again
     * when timer expires.
     */
    if (overflow_delta > INT64_MAX) {
        overflow_left = overflow_delta - INT64_MAX;
    }

    if (riscv_pmu_ctr_monitor_cycles(env, ctr_idx) ||
        riscv_pmu_ctr_monitor_instructions(env, ctr_idx)) {
        overflow_ns = pmu_icount_ticks_to_ns((int64_t)overflow_delta);
        overflow_left = pmu_icount_ticks_to_ns(overflow_left) ;
    } else {
        return -1;
    }
    overflow_at = (uint64_t)qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                  overflow_ns;

    if (overflow_at > INT64_MAX) {
        overflow_left += overflow_at - INT64_MAX;
        counter->irq_overflow_left = overflow_left;
        overflow_at = INT64_MAX;
    }
    timer_mod_anticipate_ns(cpu->pmu_timer, overflow_at);

    return 0;
}


void riscv_pmu_init(RISCVCPU *cpu, Error **errp)
{
    if (cpu->cfg.pmu_mask & (COUNTEREN_CY | COUNTEREN_TM | COUNTEREN_IR)) {
        error_setg(errp, "\"pmu-mask\" contains invalid bits (0-2) set");
        return;
    }

    if (ctpop32(cpu->cfg.pmu_mask) > (RV_MAX_MHPMCOUNTERS - 3)) {
        error_setg(errp, "Number of counters exceeds maximum available");
        return;
    }

    cpu->pmu_event_ctr_map = g_hash_table_new(g_direct_hash, g_direct_equal);
    if (!cpu->pmu_event_ctr_map) {
        error_setg(errp, "Unable to allocate PMU event hash table");
        return;
    }

    cpu->pmu_avail_ctrs = cpu->cfg.pmu_mask;

    uint64_t val;
    if (andes_config_number(ANDES_CONFIG_ID_CPU, "freq", &val)) {
        if (riscv_timebase_freq) {
            riscv_timebase_freq = val;
        }
    }
}
