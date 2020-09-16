/******************************************************************************
 * sched_arinc653.c
 *
 * An ARINC653-compatible scheduling algorithm for use in Xen.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Copyright (c) 2010, DornerWorks, Ltd. <DornerWorks.com>
 */

#include <xen/lib.h>
#include <xen/sched.h>
#include <xen/timer.h>
#include <xen/softirq.h>
#include <xen/time.h>
#include <xen/errno.h>
#include <xen/list.h>
#include <xen/guest_access.h>
#include <public/sysctl.h>

#include "private.h"

/*
 * Known design quirks:
 * - If a pCPU is added to the cpupool, inactive UNITs (those that are not
 *   assigned to a pCPU in the minor frame) will not be immediately activated
 *   or migrated. Workaround is to use pinning to force a migration.
 * - When pinning, it is recommended to specify a single pCPU. When using
 *   multiple pCPUs IDs or "all", the scheduler will only look at pCPUs that are
 *   not assigned to a UNIT during the minor frame. It will not try to find a
 *   "best fit" and shuffle active UNITs around.
 * - If a domain has more UNITs than available pCPUs, the excess UNITs will be
 *   put into an inactive state and will not be scheduled. This is important to
 *   note for domain 0, as Linux may hang if a vCPU is not scheduled.
 */

/*
 * Locking:
 * - Scheduler lock:
 *  + is per-pCPU
 *  + serialize accesses to UNIT schedule of the pCPU
 *  + serializes assignment and deassignment of UNITs to a pCPU
 * - Private lock (a.k.a. private scheduler lock):
 *  + is scheduler-wide
 *  + serializes accesses to the private scheduler, domain, and UNIT structures
 *
 * Ordering is: Scheduler lock, private lock. Or, OTOH, private lock nests
 * inside the scheduler lock. More specifically:
 *  + if we need both scheduler and private locks, we must acquire the scheduler
 *    lock first
 *  + if we already own the private lock, we must never acquire the scheduler
 *    lock
 */

/*
 * A handle with all zeros represents domain 0 if present, otherwise idle UNIT
 */
#define DOM0_HANDLE ((const xen_domain_handle_t){0})

/*
 * Default timeslice for domain 0
 */
#define DEFAULT_TIMESLICE MILLISECS(10)

/*
 * Return a pointer to the ARINC 653-specific scheduler data information
 * associated with the given pCPU
 */
#define APCPU(cpu) ((struct a653sched_pcpu *)get_sched_res(cpu)->sched_priv)

/*
 * Return a pointer to the ARINC 653-specific scheduler data information
 * associated with the given UNIT
 */
#define AUNIT(unit) ((struct a653sched_unit *)(unit)->priv)

/*
 * Return the global scheduler private data given the scheduler ops pointer
 */
#define SCHED_PRIV(s) ((struct a653sched_private *)((s)->sched_data))

/*
 * UNIT frame entry for a pCPU
 */
struct unit_sched_entry
{
    struct sched_unit *unit;            /* UNIT to run */
    s_time_t runtime;                   /* Duration of the frame */
};

/*
 * Physical pCPU
 */
struct a653sched_pcpu
{
    unsigned int cpu;                   /* pCPU id */
    struct list_head pcpu_list_elem;    /* On the scheduler private data */

    spinlock_t lock;                    /* Scheduler lock */

    /* Schedule of UNITs to run on this pCPU */
    struct unit_sched_entry sched[ARINC653_MAX_DOMAINS_PER_SCHEDULE];

    unsigned int sched_len;             /* Active entries in sched */
    unsigned int sched_index;           /* Current frame */

    s_time_t epoch;                     /* Sync to this point in time */
    s_time_t major_frame;               /* Duration of a major frame */
    s_time_t next_switch_time;          /* When to switch to the next frame */
};

/*
 * Schedule unit
 */
struct a653sched_unit
{
    struct sched_unit *unit;            /* Up-pointer to UNIT */
    struct a653sched_dom *adom;         /* Up-pointer to domain */
    struct list_head unit_list_elem;    /* On the domain */
    bool active;                        /* Is this UNIT active on a pCPU? */
};

/*
 * Domain
 */
struct a653sched_dom
{
    struct domain *dom;                 /* Up-pointer to domain */
    struct list_head dom_list_elem;     /* On the scheduler private data */
    struct list_head unit_list;         /* UNITs belonging to this domain */
    cpumask_t active_pcpus;             /* Active pCPUs for this domain */
};

/*
 * Domain frame entry in the ARINC 653 schedule
 */
struct sched_entry
{
    xen_domain_handle_t dom_handle;     /* UUID of the domain */
    s_time_t runtime;                   /* Duration of the frame */
};

/*
 * Scheduler private data
 */
struct a653sched_private
{
    spinlock_t lock;                    /* Private lock */

    /* Schedule of domains to run */
    struct sched_entry sched[ARINC653_MAX_DOMAINS_PER_SCHEDULE];

    unsigned int sched_len;             /* Active entries in sched */

    s_time_t epoch;                     /* Sync to this point in time */
    s_time_t major_frame;               /* Duration of a major frame */

    cpumask_t pcpus;                    /* pCPUs in this cpupool */

    struct list_head pcpu_list;         /* pCPUs belonging to this scheduler */
    struct list_head dom_list;          /* Doms belonging to this scheduler */
};

/* This function compares two domain handles */
static inline bool dom_handle_cmp(const xen_domain_handle_t h1,
                                  const xen_domain_handle_t h2)
{
    return memcmp(h1, h2, sizeof(xen_domain_handle_t)) == 0;
}

static struct a653sched_dom *find_dom(const struct a653sched_private *prv,
                                      const xen_domain_handle_t handle)
{
    struct a653sched_dom *adom;

    list_for_each_entry ( adom, &prv->dom_list, dom_list_elem )
        if ( dom_handle_cmp(adom->dom->handle, handle) )
            return adom;

    return NULL;
}

static struct a653sched_unit *find_unit(const struct a653sched_dom *adom,
                                        unsigned int cpu)
{
    struct a653sched_unit *aunit;

    list_for_each_entry ( aunit, &adom->unit_list, unit_list_elem )
        if ( aunit->active && sched_unit_master(aunit->unit) == cpu )
            return aunit;

    return NULL;
}

static struct a653sched_unit *find_inactive_unit(const struct a653sched_dom *adom,
                                                 unsigned int cpu)
{
    struct a653sched_unit *aunit;

    list_for_each_entry ( aunit, &adom->unit_list, unit_list_elem )
        if ( !aunit->active &&
             cpumask_test_cpu(cpu, cpupool_domain_master_cpumask(adom->dom)) &&
             cpumask_test_cpu(cpu, aunit->unit->cpu_hard_affinity) )
            return aunit;

    return NULL;
}

static void sync_to_epoch(struct a653sched_pcpu *apc, s_time_t now)
{
    s_time_t next;
    unsigned int index;

    ASSERT(spin_is_locked(&apc->lock));

    /* Determine the start of the current major frame */
    next = now - ((now - apc->epoch) % apc->major_frame);

    /* Determine which minor frame should be running */
    for ( index = 0; index < apc->sched_len; index++ )
    {
        next += apc->sched[index].runtime;

        if ( now < next )
            break;
    }

    ASSERT(index < apc->sched_len);

    apc->sched_index = index;
    apc->next_switch_time = next;
}

static void build_pcpu_sched(const struct a653sched_private *prv,
                             struct a653sched_pcpu *apc, s_time_t now)
{
    struct a653sched_dom *adom;
    struct a653sched_unit *aunit;
    unsigned int index;

    ASSERT(spin_is_locked(&apc->lock));

    for ( index = 0; index < prv->sched_len; index++ )
    {
        aunit = NULL;

        adom = find_dom(prv, prv->sched[index].dom_handle);
        if ( adom )
        {
            aunit = find_unit(adom, apc->cpu);
        }

        if ( aunit )
            apc->sched[index].unit = aunit->unit;
        else
            apc->sched[index].unit = sched_idle_unit(apc->cpu);

        apc->sched[index].runtime = prv->sched[index].runtime;
    }

    apc->sched_len = prv->sched_len;
    apc->epoch = prv->epoch;
    apc->major_frame = prv->major_frame;

    sync_to_epoch(apc, now);
}

static int a653sched_init(struct scheduler *ops)
{
    struct a653sched_private *prv;

    prv = xzalloc(struct a653sched_private);
    if ( prv == NULL )
        return -ENOMEM;

    spin_lock_init(&prv->lock);
    INIT_LIST_HEAD(&prv->pcpu_list);
    INIT_LIST_HEAD(&prv->dom_list);

    prv->epoch = NOW();

    /* Initialize the schedule to run dom0 if present, otherwise idle UNIT */
    prv->sched_len = 1;
    memcpy(prv->sched[0].dom_handle, DOM0_HANDLE,
           sizeof(prv->sched[0].dom_handle));
    prv->sched[0].runtime = DEFAULT_TIMESLICE;
    prv->major_frame = DEFAULT_TIMESLICE;

    ops->sched_data = prv;

    return 0;
}

static void a653sched_deinit(struct scheduler *ops)
{
    struct a653sched_private *prv = SCHED_PRIV(ops);

    ASSERT(prv);
    ASSERT(list_empty(&prv->pcpu_list));
    ASSERT(list_empty(&prv->dom_list));

    xfree(prv);
    ops->sched_data = NULL;
}

static void *a653sched_alloc_pdata(const struct scheduler *ops, int cpu)
{
    struct a653sched_pcpu *apc;

    apc = xzalloc(struct a653sched_pcpu);
    if ( apc == NULL )
        return ERR_PTR(-ENOMEM);

    spin_lock_init(&apc->lock);
    INIT_LIST_HEAD(&apc->pcpu_list_elem);
    apc->cpu = cpu;

    return apc;
}

static void init_pdata(struct a653sched_private *prv,
                       struct a653sched_pcpu *apc)
{
    ASSERT(!cpumask_test_cpu(apc->cpu, &prv->pcpus));

    cpumask_set_cpu(apc->cpu, &prv->pcpus);
    list_add(&apc->pcpu_list_elem, &prv->pcpu_list);

    build_pcpu_sched(prv, apc, NOW());
}

static spinlock_t *a653sched_switch_sched(struct scheduler *new_ops,
                                          unsigned int cpu, void *pdata,
                                          void *vdata)
{
    struct a653sched_private *prv = SCHED_PRIV(new_ops);
    struct a653sched_pcpu *apc = pdata;
    struct a653sched_unit *aunit = vdata;
    unsigned long flags;

    ASSERT(apc && aunit && is_idle_unit(aunit->unit));

    sched_idle_unit(cpu)->priv = vdata;

    spin_lock_irqsave(&apc->lock, flags);
    spin_lock(&prv->lock);
    init_pdata(prv, apc);
    spin_unlock(&prv->lock);
    spin_unlock_irqrestore(&apc->lock, flags);

    /* Return the scheduler lock */
    return &apc->lock;
}

static void a653sched_deinit_pdata(const struct scheduler *ops, void *pcpu,
                                   int cpu)
{
    struct a653sched_private *prv = SCHED_PRIV(ops);
    struct a653sched_pcpu *apc = pcpu;
    unsigned long flags;

    ASSERT(apc);
    ASSERT(cpumask_test_cpu(cpu, &prv->pcpus));

    spin_lock_irqsave(&prv->lock, flags);
    cpumask_clear_cpu(cpu, &prv->pcpus);
    list_del(&apc->pcpu_list_elem);
    spin_unlock_irqrestore(&prv->lock, flags);
}

static void a653sched_free_pdata(const struct scheduler *ops, void *pcpu,
                                 int cpu)
{
    struct a653sched_pcpu *apc = pcpu;

    xfree(apc);
}

static void *a653sched_alloc_domdata(const struct scheduler *ops,
                                     struct domain *dom)
{
    struct a653sched_private *prv = SCHED_PRIV(ops);
    struct a653sched_dom *adom;
    unsigned long flags;

    adom = xzalloc(struct a653sched_dom);
    if ( adom == NULL )
        return ERR_PTR(-ENOMEM);

    INIT_LIST_HEAD(&adom->dom_list_elem);
    INIT_LIST_HEAD(&adom->unit_list);
    adom->dom = dom;

    spin_lock_irqsave(&prv->lock, flags);
    list_add(&adom->dom_list_elem, &prv->dom_list);
    spin_unlock_irqrestore(&prv->lock, flags);

    return adom;
}

static void a653sched_free_domdata(const struct scheduler *ops, void *data)
{
    struct a653sched_private *prv = SCHED_PRIV(ops);
    struct a653sched_dom *adom = data;
    unsigned long flags;

    ASSERT(adom);
    ASSERT(list_empty(&adom->unit_list));

    spin_lock_irqsave(&prv->lock, flags);
    list_del(&adom->dom_list_elem);
    spin_unlock_irqrestore(&prv->lock, flags);

    xfree(adom);
}

static struct sched_resource *pick_res(struct a653sched_unit *aunit)
{
    struct a653sched_dom *adom = aunit->adom;
    unsigned int cpu = sched_unit_master(aunit->unit);
    cpumask_t *cpus = cpumask_scratch_cpu(cpu);
    unsigned int valid;
    unsigned int available;

    cpumask_and(cpus, cpupool_domain_master_cpumask(adom->dom),
                aunit->unit->cpu_hard_affinity);

    /* Stick with the current pCPU if it is still valid */
    if ( cpumask_test_cpu(cpu, cpus) )
        return get_sched_res(cpu);

    valid = cpumask_first(cpus);

    /* Find an available pCPU */
    cpumask_andnot(cpus, cpus, &adom->active_pcpus);
    available = cpumask_first(cpus);
    if ( available < nr_cpu_ids )
        return get_sched_res(available);

    /* All pCPUs are in use - return the first valid ID */
    return get_sched_res(valid);
}

static void unit_assign(struct a653sched_private *prv,
                        struct a653sched_unit *aunit,
                        unsigned int cpu)
{
    struct a653sched_dom *adom = aunit->adom;

    ASSERT(!aunit->active);
    ASSERT(!cpumask_test_cpu(cpu, &adom->active_pcpus));

    cpumask_set_cpu(cpu, &adom->active_pcpus);
    aunit->active = true;

    sched_set_res(aunit->unit, get_sched_res(cpu));
}

static void unit_deassign(struct a653sched_private *prv,
                          struct a653sched_unit *aunit)
{
    unsigned int cpu = sched_unit_master(aunit->unit);
    struct a653sched_dom *adom = aunit->adom;
    struct a653sched_unit *next_aunit;

    ASSERT(aunit->active);
    ASSERT(cpumask_test_cpu(cpu, &adom->active_pcpus));

    cpumask_clear_cpu(cpu, &adom->active_pcpus);
    aunit->active = false;

    /* Find an inactive UNIT to run next */
    next_aunit = find_inactive_unit(adom, cpu);
    if ( next_aunit )
        unit_assign(prv, next_aunit, cpu);
}

static void *a653sched_alloc_udata(const struct scheduler *ops,
                                   struct sched_unit *unit,
                                   void *dd)
{
    struct a653sched_unit *aunit;

    aunit = xzalloc(struct a653sched_unit);
    if ( aunit == NULL )
        return NULL;

    INIT_LIST_HEAD(&aunit->unit_list_elem);
    aunit->unit = unit;
    aunit->adom = dd;
    aunit->active = false;

    return aunit;
}

static void a653sched_insert_unit(const struct scheduler *ops,
                                  struct sched_unit *unit)
{
    struct a653sched_private *prv = SCHED_PRIV(ops);
    struct a653sched_unit *aunit = AUNIT(unit);
    struct a653sched_dom *adom = aunit->adom;
    struct a653sched_pcpu *apc;
    spinlock_t *lock;
    unsigned int cpu;
    unsigned long flags;
    bool assigned = false;

    /* Add the UNIT to the ARINC653 cpupool */
    spin_lock_irqsave(&prv->lock, flags);

    list_add(&aunit->unit_list_elem, &adom->unit_list);

    sched_set_res(unit, pick_res(aunit));
    cpu = sched_unit_master(unit);
    apc = APCPU(cpu);

    if ( cpumask_test_cpu(cpu, cpupool_domain_master_cpumask(adom->dom)) &&
         cpumask_test_cpu(cpu, unit->cpu_hard_affinity) &&
         !cpumask_test_cpu(cpu, &adom->active_pcpus) )
    {
        unit_assign(prv, aunit, cpu);
        assigned = true;
    }

    spin_unlock_irqrestore(&prv->lock, flags);

    /* Rebuild the UNIT schedule for the pCPU, if needed */
    if ( assigned )
    {
        lock = pcpu_schedule_lock_irqsave(cpu, &flags);
        spin_lock(&prv->lock);
        build_pcpu_sched(prv, apc, NOW());
        cpu_raise_softirq(cpu, SCHEDULE_SOFTIRQ);
        spin_unlock(&prv->lock);
        pcpu_schedule_unlock_irqrestore(lock, flags, cpu);
    }
}

static void a653sched_remove_unit(const struct scheduler *ops,
                                  struct sched_unit *unit)
{
    struct a653sched_private *prv = SCHED_PRIV(ops);
    struct a653sched_unit *aunit = AUNIT(unit);
    spinlock_t *lock;
    unsigned int cpu = sched_unit_master(unit);
    unsigned long flags;
    bool removed = false;

    ASSERT(!is_idle_unit(unit));

    /* Remove the UNIT from the ARINC653 cpupool */
    spin_lock_irqsave(&prv->lock, flags);

    list_del(&aunit->unit_list_elem);

    if ( aunit->active )
    {
        unit_deassign(prv, aunit);
        removed = true;
    }

    spin_unlock_irqrestore(&prv->lock, flags);

    /* Rebuild the UNIT schedule for the pCPU, if needed */
    if ( removed )
    {
        lock = pcpu_schedule_lock_irqsave(cpu, &flags);
        spin_lock(&prv->lock);
        build_pcpu_sched(prv, APCPU(cpu), NOW());
        cpu_raise_softirq(cpu, SCHEDULE_SOFTIRQ);
        spin_unlock(&prv->lock);
        pcpu_schedule_unlock_irqrestore(lock, flags, cpu);
    }
}

static void a653sched_free_udata(const struct scheduler *ops, void *priv)
{
    struct a653sched_unit *aunit = priv;

    xfree(aunit);
}

static void a653sched_unit_sleep(const struct scheduler *ops,
                                 struct sched_unit *unit)
{
    const unsigned int cpu = sched_unit_master(unit);

    ASSERT(!is_idle_unit(unit));

    /*
     * If the UNIT being put to sleep is the same one that is currently
     * running, raise a softirq to invoke the scheduler to switch domains.
     */
    if ( curr_on_cpu(cpu) == unit )
        cpu_raise_softirq(cpu, SCHEDULE_SOFTIRQ);
}

static void a653sched_unit_wake(const struct scheduler *ops,
                                struct sched_unit *unit)
{
    const unsigned int cpu = sched_unit_master(unit);

    ASSERT(!is_idle_unit(unit));

    cpu_raise_softirq(cpu, SCHEDULE_SOFTIRQ);
}

static struct sched_resource *a653sched_pick_resource(const struct scheduler *ops,
                                                      const struct sched_unit *unit)
{
    struct a653sched_private *prv = SCHED_PRIV(ops);
    struct a653sched_unit *aunit = AUNIT(unit);
    struct sched_resource *sr;
    unsigned long flags;

    ASSERT(!is_idle_unit(unit));

    spin_lock_irqsave(&prv->lock, flags);
    sr = pick_res(aunit);
    spin_unlock_irqrestore(&prv->lock, flags);

    return sr;
}

static void a653sched_migrate(const struct scheduler *ops,
                              struct sched_unit *unit, unsigned int new_cpu)
{
    const unsigned int old_cpu = sched_unit_master(unit);
    struct a653sched_private *prv = SCHED_PRIV(ops);
    struct a653sched_unit *aunit = AUNIT(unit);
    struct a653sched_dom *adom = aunit->adom;
    unsigned long flags;

    ASSERT(!is_idle_unit(unit));

    spin_lock_irqsave(&prv->lock, flags);

    /* Migrate off the old pCPU */
    if ( aunit->active && old_cpu != new_cpu )
    {
        /*
         * If the UNIT is currently running, we need to mark it as migrating
         * and wait for the scheduler to switch it out.
         */
        if ( curr_on_cpu(old_cpu) == unit )
        {
            sched_set_pause_flags(unit, _VPF_migrating);
            cpu_raise_softirq(old_cpu, SCHEDULE_SOFTIRQ);
        }

        unit_deassign(prv, aunit);
        build_pcpu_sched(prv, APCPU(old_cpu), NOW());
        cpu_raise_softirq(old_cpu, SCHEDULE_SOFTIRQ);
    }

    /* Migrate on to the new pCPU */
    if ( !aunit->active && !cpumask_test_cpu(new_cpu, &adom->active_pcpus) )
    {
        unit_assign(prv, aunit, new_cpu);
        build_pcpu_sched(prv, APCPU(new_cpu), NOW());
    }
    else
    {
        sched_set_res(unit, get_sched_res(new_cpu));
    }

    spin_unlock_irqrestore(&prv->lock, flags);
}

static void a653sched_do_schedule(const struct scheduler *ops,
                                  struct sched_unit *prev, s_time_t now,
                                  bool tasklet_work_scheduled)
{
    const unsigned int cpu = sched_get_resource_cpu(smp_processor_id());
    struct a653sched_pcpu *apc = APCPU(cpu);
    struct sched_unit *next_task;

    ASSERT(spin_is_locked(&apc->lock));

    /* Advance to the next frame if the current one has expired */
    if ( now >= apc->next_switch_time )
    {
        apc->sched_index++;
        if ( apc->sched_index >= apc->sched_len )
            apc->sched_index = 0;

        apc->next_switch_time += apc->sched[apc->sched_index].runtime;
    }

    /* Frames were somehow missed - resynchronize to epoch */
    if ( unlikely(now >= apc->next_switch_time) )
        sync_to_epoch(apc, now);

    next_task = apc->sched[apc->sched_index].unit;

    ASSERT(next_task);

    /* Check if the new task is runnable */
    if ( !unit_runnable_state(next_task) )
        next_task = sched_idle_unit(cpu);

    /* Tasklet work (which runs in idle UNIT context) overrides all else. */
    if ( tasklet_work_scheduled )
        next_task = sched_idle_unit(cpu);

    prev->next_time = apc->next_switch_time - now;
    prev->next_task = next_task;
    next_task->migrated = false;

    ASSERT(prev->next_time > 0);
}

static int a653sched_set(struct a653sched_private *prv,
                         struct xen_sysctl_arinc653_schedule *schedule)
{
    struct a653sched_pcpu *apc;
    s_time_t total_runtime = 0;
    s_time_t now;
    spinlock_t *lock;
    unsigned int i;
    unsigned long flags;

    now = NOW();

    /* Check for valid major frame and number of schedule entries */
    if ( (schedule->major_frame <= 0)
         || (schedule->num_sched_entries < 1)
         || (schedule->num_sched_entries > ARINC653_MAX_DOMAINS_PER_SCHEDULE) )
        return -EINVAL;

    for ( i = 0; i < schedule->num_sched_entries; i++ )
    {
        /* Check for a valid run time. */
        if ( schedule->sched_entries[i].runtime <= 0 )
            return -EINVAL;

        /* Add this entry's run time to total run time. */
        total_runtime += schedule->sched_entries[i].runtime;
    }

    /*
     * Error if the major frame is not large enough to run all entries as
     * indicated by comparing the total run time to the major frame length
     */
    if ( total_runtime > schedule->major_frame )
        return -EINVAL;

    spin_lock_irqsave(&prv->lock, flags);

    /* Copy the new schedule into place. */
    prv->sched_len = schedule->num_sched_entries;
    prv->major_frame = schedule->major_frame;
    prv->epoch = now;
    for ( i = 0; i < schedule->num_sched_entries; i++ )
    {
        memcpy(prv->sched[i].dom_handle,
               schedule->sched_entries[i].dom_handle,
               sizeof(prv->sched[i].dom_handle));
        prv->sched[i].runtime =
            schedule->sched_entries[i].runtime;
    }

    spin_unlock_irqrestore(&prv->lock, flags);

    /*
     * The newly-installed schedule takes effect immediately - update the UNIT
     * schedule for each pCPU
     */
    list_for_each_entry ( apc, &prv->pcpu_list, pcpu_list_elem )
    {
        lock = pcpu_schedule_lock_irqsave(apc->cpu, &flags);
        spin_lock(&prv->lock);
        build_pcpu_sched(prv, apc, now);
        spin_unlock(&prv->lock);
        pcpu_schedule_unlock_irqrestore(lock, flags, apc->cpu);
    }

    /* Trigger scheduler on all pCPUs */
    cpumask_raise_softirq(&prv->pcpus, SCHEDULE_SOFTIRQ);

    return 0;
}

static int a653sched_get(struct a653sched_private *prv,
                         struct xen_sysctl_arinc653_schedule *schedule)
{
    unsigned int i;
    unsigned long flags;

    spin_lock_irqsave(&prv->lock, flags);

    schedule->num_sched_entries = prv->sched_len;
    schedule->major_frame = prv->major_frame;
    for ( i = 0; i < prv->sched_len; i++ )
    {
        memcpy(schedule->sched_entries[i].dom_handle,
               prv->sched[i].dom_handle,
               sizeof(schedule->sched_entries[i].dom_handle));
        schedule->sched_entries[i].vcpu_id = 0;
        schedule->sched_entries[i].runtime = prv->sched[i].runtime;
    }

    spin_unlock_irqrestore(&prv->lock, flags);

    return 0;
}

static int a653sched_adjust_global(const struct scheduler *ops,
                                   struct xen_sysctl_scheduler_op *sc)
{
    struct a653sched_private *prv = SCHED_PRIV(ops);
    struct xen_sysctl_arinc653_schedule *sched = NULL;
    int rc = -EINVAL;

    switch ( sc->cmd )
    {
    case XEN_SYSCTL_SCHEDOP_putinfo:
        sched = xzalloc(struct xen_sysctl_arinc653_schedule);
        if ( sched == NULL )
        {
            rc = -ENOMEM;
            break;
        }

        if ( copy_from_guest(sched, sc->u.sched_arinc653.schedule, 1) )
        {
            rc = -EFAULT;
            break;
        }

        rc = a653sched_set(prv, sched);
        break;
    case XEN_SYSCTL_SCHEDOP_getinfo:
        sched = xzalloc(struct xen_sysctl_arinc653_schedule);
        if ( sched == NULL )
        {
            rc = -ENOMEM;
            break;
        }

        rc = a653sched_get(prv, sched);
        if ( rc )
            break;

        if ( copy_to_guest(sc->u.sched_arinc653.schedule, sched, 1) )
            rc = -EFAULT;
        break;
    }

    xfree(sched);

    return rc;
}

static const struct scheduler sched_arinc653_def = {
    .name           = "ARINC 653 Scheduler",
    .opt_name       = "arinc653",
    .sched_id       = XEN_SCHEDULER_ARINC653,
    .sched_data     = NULL,

    .global_init    = NULL,
    .init           = a653sched_init,
    .deinit         = a653sched_deinit,

    .alloc_pdata    = a653sched_alloc_pdata,
    .switch_sched   = a653sched_switch_sched,
    .deinit_pdata   = a653sched_deinit_pdata,
    .free_pdata     = a653sched_free_pdata,

    .alloc_domdata  = a653sched_alloc_domdata,
    .free_domdata   = a653sched_free_domdata,

    .alloc_udata    = a653sched_alloc_udata,
    .insert_unit    = a653sched_insert_unit,
    .remove_unit    = a653sched_remove_unit,
    .free_udata     = a653sched_free_udata,

    .sleep          = a653sched_unit_sleep,
    .wake           = a653sched_unit_wake,
    .yield          = NULL,
    .context_saved  = NULL,

    .pick_resource  = a653sched_pick_resource,
    .migrate        = a653sched_migrate,

    .do_schedule    = a653sched_do_schedule,

    .adjust         = NULL,
    .adjust_affinity= NULL,
    .adjust_global  = a653sched_adjust_global,

    .dump_settings  = NULL,
    .dump_cpu_state = NULL,
};

REGISTER_SCHEDULER(sched_arinc653_def);

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
