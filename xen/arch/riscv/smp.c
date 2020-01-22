#include <xen/mm.h>
#include <xen/smp.h>
#include <asm/system.h>
#include <asm/smp.h>
#include <asm/page.h>
#include <asm/flushtlb.h>

volatile unsigned long start_secondary_pen_release = HARTID_INVALID;

void flush_tlb_mask(const cpumask_t *mask)
{
    flush_all_guests_tlb();
}

void smp_send_event_check_mask(const cpumask_t *mask)
{
    /* TODO */
}

void smp_send_call_function_mask(const cpumask_t *mask)
{
    cpumask_t target_mask;

    cpumask_andnot(&target_mask, mask, cpumask_of(smp_processor_id()));

    if ( cpumask_test_cpu(smp_processor_id(), mask) )
    {
        local_irq_disable();
        smp_call_function_interrupt();
        local_irq_enable();
    }
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
