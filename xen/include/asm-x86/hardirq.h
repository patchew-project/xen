#ifndef __ASM_HARDIRQ_H
#define __ASM_HARDIRQ_H

#include <xen/cache.h>
#include <xen/lib.h>
#include <xen/smp.h>
#include <xen/types.h>

typedef struct {
	unsigned int __softirq_pending;
	unsigned int __local_irq_count;
	bool in_nmi;
	unsigned int mc_count;
	bool_t __mwait_wakeup;
} __cacheline_aligned irq_cpustat_t;

#include <xen/irq_cpustat.h>	/* Standard mappings for irq_cpustat_t above */

#define in_irq() (local_irq_count(smp_processor_id()) != 0)

#define irq_enter()	(local_irq_count(smp_processor_id())++)
#define irq_exit()	(local_irq_count(smp_processor_id())--)

#define in_mc() 	(mc_count(smp_processor_id()) != 0)
#define mc_enter()	(mc_count(smp_processor_id())++)
#define mc_exit()	(mc_count(smp_processor_id())--)

#define in_nmi()	__IRQ_STAT(smp_processor_id(), in_nmi)

static inline void nmi_enter(void)
{
    ASSERT(!in_nmi());
    in_nmi() = true;
}

static inline void nmi_exit(void)
{
    ASSERT(in_nmi());
    in_nmi() = false;
}

void ack_bad_irq(unsigned int irq);

extern void apic_intr_init(void);
extern void smp_intr_init(void);

#endif /* __ASM_HARDIRQ_H */
