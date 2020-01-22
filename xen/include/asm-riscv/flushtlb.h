#ifndef __ASM_RISCV_FLUSHTLB_H__
#define __ASM_RISCV_FLUSHTLB_H__

#include <xen/cpumask.h>
#include <public/domctl.h>

/*
 * Filter the given set of CPUs, removing those that definitely flushed their
 * TLB since @page_timestamp.
 */
/* XXX lazy implementation just doesn't clear anything.... */
static inline void tlbflush_filter(cpumask_t *mask, uint32_t page_timestamp) {}

#define tlbflush_current_time()                 (0)

static inline void page_set_tlbflush_timestamp(struct page_info *page)
{
    page->tlbflush_timestamp = tlbflush_current_time();
}

/* Flush specified CPUs' TLBs */
void flush_tlb_mask(const cpumask_t *mask);

/* Flush local TLBs, current VMID only */
static inline void flush_guest_tlb_local(void)
{
    /* TODO */
}

/* Flush innershareable TLBs, current VMID only */
static inline void flush_guest_tlb(void)
{
    /* TODO */
}

/* Flush local TLBs, all VMIDs, non-hypervisor mode */
static inline void flush_all_guests_tlb_local(void)
{
    /* TODO */
}

/* Flush innershareable TLBs, all VMIDs, non-hypervisor mode */
static inline void flush_all_guests_tlb(void)
{
	/* TODO */
}

#endif /* __ASM_RISCV_FLUSHTLB_H__ */
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
