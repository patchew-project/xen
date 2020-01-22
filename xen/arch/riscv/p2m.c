#include <xen/cpu.h>
#include <xen/domain_page.h>
#include <xen/iocap.h>
#include <xen/lib.h>
#include <xen/sched.h>
#include <xen/softirq.h>
#include <asm/event.h>
#include <asm/flushtlb.h>
#include <asm/page.h>


#define INVALID_VMID 0 /* VMID 0 is reserved */

/* Unlock the flush and do a P2M TLB flush if necessary */
void p2m_write_unlock(struct p2m_domain *p2m)
{
    /* TODO */

    write_unlock(&p2m->lock);
}

void p2m_dump_info(struct domain *d)
{
    struct p2m_domain *p2m = p2m_get_hostp2m(d);

    p2m_read_lock(p2m);
    printk("p2m mappings for domain %d (vmid %d):\n",
           d->domain_id, p2m->vmid);
    BUG_ON(p2m->stats.mappings[0] || p2m->stats.shattered[0]);
    printk("  1G mappings: %ld (shattered %ld)\n",
           p2m->stats.mappings[1], p2m->stats.shattered[1]);
    printk("  2M mappings: %ld (shattered %ld)\n",
           p2m->stats.mappings[2], p2m->stats.shattered[2]);
    printk("  4K mappings: %ld\n", p2m->stats.mappings[3]);
    p2m_read_unlock(p2m);
}

void memory_type_changed(struct domain *d)
{
}

void dump_p2m_lookup(struct domain *d, paddr_t addr)
{
    struct p2m_domain *p2m = p2m_get_hostp2m(d);

    printk("dom%d IPA 0x%"PRIpaddr"\n", d->domain_id, addr);

    printk("P2M @ %p mfn:%#"PRI_mfn"\n",
           p2m->root, mfn_x(page_to_mfn(p2m->root)));
}

/*
 * p2m_save_state and p2m_restore_state work in pair to workaround
 * ARM64_WORKAROUND_AT_SPECULATE. p2m_save_state will set-up VTTBR to
 * point to the empty page-tables to stop allocating TLB entries.
 */
void p2m_save_state(struct vcpu *p)
{
    /* TODO */
}

void p2m_restore_state(struct vcpu *n)
{
    /* TODO */
}

mfn_t gfn_to_mfn(struct domain *d, gfn_t gfn)
{
    return p2m_lookup(d, gfn, NULL);
}

/*
 * Force a synchronous P2M TLB flush.
 *
 * Must be called with the p2m lock held.
 */
static void p2m_force_tlb_flush_sync(struct p2m_domain *p2m)
{
    /* TODO */
}

int p2m_set_entry(struct p2m_domain *p2m,
                  gfn_t sgfn,
                  unsigned long nr,
                  mfn_t smfn,
                  p2m_type_t t,
                  p2m_access_t a)
{
    int rc = 0;

    /* TODO */

    return rc;
}

mfn_t p2m_lookup(struct domain *d, gfn_t gfn, p2m_type_t *t)
{
    mfn_t mfn;
    struct p2m_domain *p2m = p2m_get_hostp2m(d);

    p2m_read_lock(p2m);
    mfn = p2m_get_entry(p2m, gfn, t, NULL, NULL, NULL);
    p2m_read_unlock(p2m);

    return mfn;
}

mfn_t p2m_get_entry(struct p2m_domain *p2m, gfn_t gfn,
                    p2m_type_t *t, p2m_access_t *a,
                    unsigned int *page_order,
                    bool *valid)
{
    /* TODO */

    mfn_t mfn;

    p2m_read_lock(p2m);
    mfn = p2m_get_entry(p2m, gfn, t, NULL, NULL, NULL);
    p2m_read_unlock(p2m);

    return mfn;
}

static inline int p2m_insert_mapping(struct domain *d,
                                     gfn_t start_gfn,
                                     unsigned long nr,
                                     mfn_t mfn,
                                     p2m_type_t t)
{
    struct p2m_domain *p2m = p2m_get_hostp2m(d);
    int rc;

    p2m_write_lock(p2m);
    rc = p2m_set_entry(p2m, start_gfn, nr, mfn, t, p2m->default_access);
    p2m_write_unlock(p2m);

    return rc;
}

static inline int p2m_remove_mapping(struct domain *d,
                                     gfn_t start_gfn,
                                     unsigned long nr,
                                     mfn_t mfn)
{
    struct p2m_domain *p2m = p2m_get_hostp2m(d);
    int rc;

    p2m_write_lock(p2m);
    rc = p2m_set_entry(p2m, start_gfn, nr, INVALID_MFN,
                       p2m_invalid, p2m_access_rwx);
    p2m_write_unlock(p2m);

    return rc;
}

void p2m_tlb_flush_sync(struct p2m_domain *p2m)
{
    if ( p2m->need_flush )
        p2m_force_tlb_flush_sync(p2m);
}

int map_regions_p2mt(struct domain *d,
                     gfn_t gfn,
                     unsigned long nr,
                     mfn_t mfn,
                     p2m_type_t p2mt)
{
    return p2m_insert_mapping(d, gfn, nr, mfn, p2mt);
}

int unmap_regions_p2mt(struct domain *d,
                       gfn_t gfn,
                       unsigned long nr,
                       mfn_t mfn)
{
    return p2m_remove_mapping(d, gfn, nr, mfn);
}

int map_mmio_regions(struct domain *d,
                     gfn_t start_gfn,
                     unsigned long nr,
                     mfn_t mfn)
{
    return p2m_insert_mapping(d, start_gfn, nr, mfn, p2m_mmio_direct_dev);
}

int unmap_mmio_regions(struct domain *d,
                       gfn_t start_gfn,
                       unsigned long nr,
                       mfn_t mfn)
{
    return p2m_remove_mapping(d, start_gfn, nr, mfn);
}

int map_dev_mmio_region(struct domain *d,
                        gfn_t gfn,
                        unsigned long nr,
                        mfn_t mfn)
{
    /* TODO */

    return 0;
}

int guest_physmap_add_entry(struct domain *d,
                            gfn_t gfn,
                            mfn_t mfn,
                            unsigned long page_order,
                            p2m_type_t t)
{
    return p2m_insert_mapping(d, gfn, (1 << page_order), mfn, t);
}

int guest_physmap_remove_page(struct domain *d, gfn_t gfn, mfn_t mfn,
                              unsigned int page_order)
{
    return p2m_remove_mapping(d, gfn, (1 << page_order), mfn);
}

struct page_info *p2m_get_page_from_gfn(struct domain *d, gfn_t gfn,
                                        p2m_type_t *t)
{
    struct page_info *page;
    p2m_type_t p2mt;
    mfn_t mfn = p2m_lookup(d, gfn, &p2mt);

    if ( t )
        *t = p2mt;

    if ( !p2m_is_any_ram(p2mt) )
        return NULL;

    if ( !mfn_valid(mfn) )
        return NULL;

    page = mfn_to_page(mfn);

    /*
     * get_page won't work on foreign mapping because the page doesn't
     * belong to the current domain.
     */
    if ( p2m_is_foreign(p2mt) )
    {
        struct domain *fdom = page_get_owner_and_reference(page);
        ASSERT(fdom != NULL);
        ASSERT(fdom != d);
        return page;
    }

    return get_page(page, d) ? page : NULL;
}

void vcpu_mark_events_pending(struct vcpu *v)
{
    /* TODO */
}

void vcpu_update_evtchn_irq(struct vcpu *v)
{
    /* TODO */
}
