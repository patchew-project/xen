/*
 * RISC-V Interrupt support
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

#include <xen/lib.h>
#include <xen/spinlock.h>
#include <xen/irq.h>
#include <xen/init.h>
#include <xen/errno.h>
#include <xen/sched.h>

const unsigned int nr_irqs = NR_IRQS;

static void ack_none(struct irq_desc *irq)
{
    printk("unexpected IRQ trap at irq %02x\n", irq->irq);
}

static void end_none(struct irq_desc *irq)
{
    /* TODO */
}

hw_irq_controller no_irq_type = {
    .typename = "none",
    .startup = irq_startup_none,
    .shutdown = irq_shutdown_none,
    .enable = irq_enable_none,
    .disable = irq_disable_none,
    .ack = ack_none,
    .end = end_none
};

static irq_desc_t irq_desc[NR_IRQS];
static DEFINE_PER_CPU(irq_desc_t[NR_LOCAL_IRQS], local_irq_desc);

int arch_init_one_irq_desc(struct irq_desc *desc)
{
    return 0;
}

struct pirq *alloc_pirq_struct(struct domain *d)
{
	/* TODO */

    return NULL;
}

irq_desc_t *__irq_to_desc(int irq)
{
    if ( irq < NR_LOCAL_IRQS )
        return &this_cpu(local_irq_desc)[irq];

    return &irq_desc[irq-NR_LOCAL_IRQS];
}

int pirq_guest_bind(struct vcpu *v, struct pirq *pirq, int will_share)
{
    BUG();
}

void pirq_guest_unbind(struct domain *d, struct pirq *pirq)
{
    BUG();
}

void pirq_set_affinity(struct domain *d, int pirq, const cpumask_t *mask)
{
    BUG();
}

void smp_send_state_dump(unsigned int cpu)
{
    /* TODO */
}

void arch_move_irqs(struct vcpu *v)
{
    /* TODO */
}

int setup_irq(unsigned int irq, unsigned int irqflags, struct irqaction *new)
{
    int rc = 0;
    unsigned long flags;
    struct irq_desc *desc;

    desc = irq_to_desc(irq);

    spin_lock_irqsave(&desc->lock, flags);

    /* TODO */

    spin_unlock_irqrestore(&desc->lock, flags);

    return rc;
}
