/******************************************************************************
 *
 * Copyright 2019 (C) Alistair Francis <alistair.francis@wdc.com>
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
 */

#include <xen/bitops.h>
#include <xen/errno.h>
#include <xen/grant_table.h>
#include <xen/hypercall.h>
#include <xen/init.h>
#include <xen/lib.h>
#include <xen/livepatch.h>
#include <xen/sched.h>
#include <xen/softirq.h>
#include <xen/wait.h>

DEFINE_PER_CPU(struct vcpu *, curr_vcpu);

static void do_idle(void)
{
    unsigned int cpu = smp_processor_id();

    sched_tick_suspend();
    /* sched_tick_suspend() can raise TIMER_SOFTIRQ. Process it now. */
    process_pending_softirqs();

    local_irq_disable();
    if ( cpu_is_haltable(cpu) )
    {
        wait_for_interrupt();
    }
    local_irq_enable();

    sched_tick_resume();
}

void idle_loop(void)
{
    unsigned int cpu = smp_processor_id();

    for ( ; ; )
    {
        if ( cpu_is_offline(cpu) )
            stop_cpu();

        /* Are we here for running vcpu context tasklets, or for idling? */
        if ( unlikely(tasklet_work_to_do(cpu)) )
            do_tasklet();
        /*
         * Test softirqs twice --- first to see if should even try scrubbing
         * and then, after it is done, whether softirqs became pending
         * while we were scrubbing.
         */
        else if ( !softirq_pending(cpu) && !scrub_free_pages() &&
                  !softirq_pending(cpu) )
            do_idle();

        do_softirq();
        /*
         * We MUST be last (or before dsb, wfi). Otherwise after we get the
         * softirq we would execute dsb,wfi (and sleep) and not patch.
         */
        check_for_livepatch_work();
    }
}


void context_switch(struct vcpu *prev, struct vcpu *next)
{
    ASSERT(local_irq_is_enabled());
    ASSERT(prev != next);
    ASSERT(!vcpu_cpu_dirty(next));

    local_irq_disable();

    /* TODO */

    set_current(next);
}

void continue_running(struct vcpu *same)
{
    /* Nothing to do */
}

void sync_local_execstate(void)
{
    /* Nothing to do -- no lazy switching */
}

void sync_vcpu_execstate(struct vcpu *v)
{
    /* Nothing to do -- no lazy switching */
}

unsigned long hypercall_create_continuation(
    unsigned int op, const char *format, ...)
{
	/* TODO */

	return 0;
}

void startup_cpu_idle_loop(void)
{
    struct vcpu *v = current;

    ASSERT(is_idle_vcpu(v));

    reset_stack_and_jump(idle_loop);
}

struct domain *alloc_domain_struct(void)
{
    struct domain *d;
    BUILD_BUG_ON(sizeof(*d) > PAGE_SIZE);
    d = alloc_xenheap_pages(0, 0);
    if ( d == NULL )
        return NULL;

    clear_page(d);
    return d;
}

void free_domain_struct(struct domain *d)
{
    free_xenheap_page(d);
}

void dump_pageframe_info(struct domain *d)
{

}

int arch_sanitise_domain_config(struct xen_domctl_createdomain *config)
{
    /* TODO */

    return 0;
}


int arch_domain_create(struct domain *d,
                       struct xen_domctl_createdomain *config)
{
    /* TODO */

    return 0;
}

void arch_domain_destroy(struct domain *d)
{
}

void arch_domain_shutdown(struct domain *d)
{
}

void arch_domain_pause(struct domain *d)
{
}

void arch_domain_unpause(struct domain *d)
{
}

int arch_domain_soft_reset(struct domain *d)
{
    return -ENOSYS;
}

void arch_domain_creation_finished(struct domain *d)
{
    /* TODO */
}

int domain_relinquish_resources(struct domain *d)
{
    /* TODO */

    return 0;
}

void arch_dump_domain_info(struct domain *d)
{
    p2m_dump_info(d);
}

long arch_do_vcpu_op(int cmd, struct vcpu *v, XEN_GUEST_HANDLE_PARAM(void) arg)
{
    return -ENOSYS;
}

void arch_dump_vcpu_info(struct vcpu *v)
{
    /* TODO */
}

int arch_set_info_guest(
    struct vcpu *v, vcpu_guest_context_u c)
{
    /* TODO */

    return 0;
}

#define MAX_PAGES_PER_VCPU  2

struct vcpu *alloc_vcpu_struct(const struct domain *d)
{
    struct vcpu *v;

    BUILD_BUG_ON(sizeof(*v) > MAX_PAGES_PER_VCPU * PAGE_SIZE);
    v = alloc_xenheap_pages(get_order_from_bytes(sizeof(*v)), 0);
    if ( v != NULL )
    {
        unsigned int i;

        for ( i = 0; i < DIV_ROUND_UP(sizeof(*v), PAGE_SIZE); i++ )
            clear_page((void *)v + i * PAGE_SIZE);
    }

    return v;
}

void free_vcpu_struct(struct vcpu *v)
{
    free_xenheap_pages(v, get_order_from_bytes(sizeof(*v)));
}

int arch_initialise_vcpu(struct vcpu *v, XEN_GUEST_HANDLE_PARAM(void) arg)
{
    return default_initialise_vcpu(v, arg);
}

int arch_vcpu_reset(struct vcpu *v)
{
    /* TODO */
    return 0;
}

int arch_vcpu_create(struct vcpu *v)
{
    int rc = 0;

    /* TODO */

    return rc;
}

void arch_vcpu_destroy(struct vcpu *v)
{
    /* TODO */
}
