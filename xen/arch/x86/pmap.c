#include <xen/init.h>
#include <xen/mm.h>
#include <xen/spinlock.h>

#include <asm/bitops.h>
#include <asm/fixmap.h>
#include <asm/flushtlb.h>

/*
 * Simple mapping infrastructure to map / unmap pages in fixed map.
 * This is used to set up the page table for mapcache, which is used
 * by map domain page infrastructure.
 *
 * This structure is not protected by any locks, so it must not be used after
 * smp bring-up.
 */

/* Bitmap to track which slot is used */
static unsigned long __initdata inuse;

void *__init pmap_map(mfn_t mfn)
{
    unsigned long flags;
    unsigned int idx;
    void *linear = NULL;
    enum fixed_addresses slot;
    l1_pgentry_t *pl1e;

    BUILD_BUG_ON(sizeof(inuse) * BITS_PER_LONG < NUM_FIX_PMAP);

    ASSERT(system_state < SYS_STATE_smp_boot);

    local_irq_save(flags);

    idx = find_first_zero_bit(&inuse, NUM_FIX_PMAP);
    if ( idx == NUM_FIX_PMAP )
        panic("Out of PMAP slots\n");

    __set_bit(idx, &inuse);

    slot = idx + FIX_PMAP_BEGIN;
    ASSERT(slot >= FIX_PMAP_BEGIN && slot <= FIX_PMAP_END);

    linear = fix_to_virt(slot);
    /*
     * We cannot use set_fixmap() here. We use PMAP when there is no direct map,
     * so map_pages_to_xen() called by set_fixmap() needs to map pages on
     * demand, which then calls pmap() again, resulting in a loop. Modify the
     * PTEs directly instead. The same is true for pmap_unmap().
     */
    pl1e = &l1_fixmap[l1_table_offset((unsigned long)linear)];
    l1e_write_atomic(pl1e, l1e_from_mfn(mfn, PAGE_HYPERVISOR));

    local_irq_restore(flags);

    return linear;
}

void __init pmap_unmap(void *p)
{
    unsigned long flags;
    unsigned int idx;
    l1_pgentry_t *pl1e;
    enum fixed_addresses slot = __virt_to_fix((unsigned long)p);

    ASSERT(system_state < SYS_STATE_smp_boot);
    ASSERT(slot >= FIX_PMAP_BEGIN && slot <= FIX_PMAP_END);

    idx = slot - FIX_PMAP_BEGIN;
    local_irq_save(flags);

    __clear_bit(idx, &inuse);
    pl1e = &l1_fixmap[l1_table_offset((unsigned long)p)];
    l1e_write_atomic(pl1e, l1e_empty());
    flush_tlb_one_local(p);

    local_irq_restore(flags);
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
