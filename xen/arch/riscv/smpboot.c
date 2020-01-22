/*
 * Dummy smpboot support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <xen/cpu.h>
#include <xen/cpumask.h>
#include <xen/delay.h>
#include <xen/domain_page.h>
#include <public/domctl.h>
#include <xen/errno.h>
#include <xen/init.h>
#include <xen/mm.h>
#include <xen/sched.h>
#include <xen/smp.h>
#include <xen/softirq.h>
#include <xen/timer.h>
#include <xen/warning.h>
#include <xen/irq.h>
#include <xen/console.h>

cpumask_t cpu_online_map;
cpumask_t cpu_present_map;
cpumask_t cpu_possible_map;

/* Fake one node for now. See also include/asm-arm/numa.h */
nodemask_t __read_mostly node_online_map = { { [0] = 1UL } };

/* Boot cpu data */
struct init_info init_data =
{
};

/* Shared state for coordinating CPU teardown */
static bool cpu_is_dead;

/* ID of the PCPU we're running on */
DEFINE_PER_CPU(unsigned int, cpu_id);
/* XXX these seem awfully x86ish... */
/* representing HT siblings of each logical CPU */
DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_sibling_mask);
/* representing HT and core siblings of each logical CPU */
DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_core_mask);

/*
 * By default non-boot CPUs not identical to the boot CPU will be
 * parked.
 */
static bool __read_mostly opt_hmp_unsafe = false;
boolean_param("hmp-unsafe", opt_hmp_unsafe);

int __cpu_up(unsigned int cpu)
{
    printk("Bringing up CPU%d\n", cpu);

    console_start_sync(); /* Secondary may use early_printk */

    /* TODO */

    return 0;
}

/* Shut down the current CPU */
void __cpu_disable(void)
{
    unsigned int cpu = get_processor_id();

    /* TODO */

    /* It's now safe to remove this processor from the online map */
    cpumask_clear_cpu(cpu, &cpu_online_map);

    smp_mb();

    /* Return to caller; eventually the IPI mechanism will unwind and the 
     * scheduler will drop to the idle loop, which will call stop_cpu(). */
}

void __cpu_die(unsigned int cpu)
{
    unsigned int i = 0;

    while ( !cpu_is_dead )
    {
        mdelay(100);
        cpu_relax();
        process_pending_softirqs();
        if ( (++i % 10) == 0 )
            printk(KERN_ERR "CPU %u still not dead...\n", cpu);
        smp_mb();
    }
    cpu_is_dead = false;
    smp_mb();
}

void stop_cpu(void)
{
    local_irq_disable();
    cpu_is_dead = true;

    /* TODO */

    while ( 1 )
        wait_for_interrupt();
}
