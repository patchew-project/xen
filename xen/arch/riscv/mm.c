/*
 * xen/arch/riscv/mm.c
 *
 * MMU code for a RISC-V RV32/64 with hypervisor extensions.
 *
 * Copyright (c) 2019 Bobby Eshleman <bobbyeshleman@gmail.com>
 *
 * Based on code that is Copyright (c) 2018 Anup Patel.
 * Based on code that is Copyright (c) 2011 Tim Deegan <tim@xen.org>
 * Based on code that is Copyright (c) 2011 Citrix Systems.
 *
 * Parts of this code are based on:
 *     ARM/Xen: xen/arch/arm/mm.c.
 *     Xvisor: arch/riscv/cpu/generic/cpu_mmu_initial_pgtbl.c
 *         (https://github.com/xvisor/xvisor/tree/v0.2.11)
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

#include <xen/compile.h>
#include <xen/types.h>
#include <xen/init.h>
#include <xen/mm.h>
#include <asm/p2m.h>
#include <public/domctl.h>
#include <asm/page.h>
#include <xen/preempt.h>
#include <xen/errno.h>
#include <xen/grant_table.h>
#include <xen/softirq.h>
#include <xen/event.h>
#include <xen/guest_access.h>
#include <xen/domain_page.h>
#include <xen/err.h>
#include <asm/page.h>
#include <asm/current.h>
#include <asm/flushtlb.h>
#include <public/memory.h>
#include <xen/sched.h>
#include <xen/vmap.h>
#include <xsm/xsm.h>
#include <xen/pfn.h>
#include <xen/sizes.h>
#include <asm/setup.h>

#ifdef NDEBUG
static inline void __attribute__((__format__(__printf__, 1, 2)))
mm_printk(const char *fmt, ...)
{
}
#else
#define mm_printk(fmt, args...)                                                \
    do                                                                         \
    {                                                                          \
        dprintk(XENLOG_ERR, fmt, ##args);                                      \
        WARN();                                                                \
    } while ( 0 );
#endif

#define XEN_TABLE_MAP_FAILED 0
#define XEN_TABLE_SUPER_PAGE 1
#define XEN_TABLE_NORMAL_PAGE 2

/* Override macros from asm/page.h to make them work with mfn_t */
#undef virt_to_mfn
#define virt_to_mfn(va) _mfn(__virt_to_mfn(va))
#undef mfn_to_virt
#define mfn_to_virt(mfn) __mfn_to_virt(mfn_x(mfn))

/* Limits of the Xen heap */
mfn_t xenheap_mfn_start __read_mostly = INVALID_MFN_INITIALIZER;
mfn_t xenheap_mfn_end __read_mostly;
vaddr_t xenheap_virt_end __read_mostly;
vaddr_t xenheap_virt_start __read_mostly;
unsigned long xenheap_base_pdx __read_mostly;

/* Limits of frametable */
unsigned long frametable_virt_end __read_mostly;
unsigned long frametable_base_pdx;

/*
 * xen_second_pagetable is indexed with the VPN[2] page table entry field
 * xen_first_pagetable is accessed from the VPN[1] page table entry field
 * xen_zeroeth_pagetable is accessed from the VPN[0] page table entry field
 */
pte_t xen_second_pagetable[PAGE_ENTRIES] __attribute__((__aligned__(4096)));
static pte_t xen_first_pagetable[PAGE_ENTRIES]
    __attribute__((__aligned__(4096)));
static pte_t xen_zeroeth_pagetable[PAGE_ENTRIES]
    __attribute__((__aligned__(4096)));
static pte_t xen_heap_megapages[PAGE_ENTRIES]
    __attribute__((__aligned__(4096)));

/*
 * The second level slot which points to xen_heap_megapages.
 * This slot indexes into the PTE that points to the first level table
 * of megapages that we used to map in and then initialize our first
 * set of boot pages.  Once it has been used to map/init boot page,
 * those pages can be used to alloc the rest of the page tables with
 * the alloc_boot_pages().
 */
static __initdata int xen_second_heap_slot = -1;

#define THIS_CPU_PGTABLE xen_second_pagetable

/* Used by _setup_initial_pagetables() and initialized by head.S */
extern unsigned long _text_start;
extern unsigned long _text_end;
extern unsigned long _cpuinit_start;
extern unsigned long _cpuinit_end;
extern unsigned long _spinlock_start;
extern unsigned long _spinlock_end;
extern unsigned long _init_start;
extern unsigned long _init_end;
extern unsigned long _rodata_start;
extern unsigned long _rodata_end;

paddr_t phys_offset;
unsigned long max_page;

static inline pte_t mfn_to_pte(mfn_t mfn)
{
    unsigned long pte = mfn_x(mfn) << PTE_SHIFT;
    return (pte_t){ .pte = pte };
}

void *__init arch_vmap_virt_end(void)
{
    return (void *)VMAP_VIRT_END;
}

static inline pte_t mfn_to_xen_entry(mfn_t mfn)
{
    return mfn_to_pte(mfn);
}

/* Map a 4k page in a fixmap entry */
void set_fixmap(unsigned map, mfn_t mfn, unsigned int flags)
{
    /* TODO */
}

/* Remove a mapping from a fixmap entry */
void clear_fixmap(unsigned map)
{
    /* TODO */
}

#ifdef CONFIG_DOMAIN_PAGE
void *map_domain_page_global(mfn_t mfn)
{
    return vmap(&mfn, 1);
}

void unmap_domain_page_global(const void *va)
{
    vunmap(va);
}
#endif

void flush_page_to_ram(unsigned long mfn, bool sync_icache)
{
    void *va = map_domain_page(_mfn(mfn));
    unmap_domain_page(va);

    /* TODO */

    if ( sync_icache )
        invalidate_icache();
}

enum xenmap_operation { INSERT, REMOVE, MODIFY, RESERVE };

static int alloc_xen_table(pte_t *entry)
{
    void *p;
    pte_t pte;

    p = alloc_xenheap_page();
    if ( p == NULL )
        return -ENOMEM;

    clear_page(p);
    pte = mfn_to_xen_entry(maddr_to_mfn(virt_to_maddr(p)));
    pte.pte |= PTE_TABLE;

    write_pte(entry, pte);
    return 0;
}

static int xen_pt_update(unsigned long va, mfn_t mfn, unsigned int flags)
{
    pte_t *entry;
    pte_t *first;
    pte_t *zeroeth;

    pte_t pte;
    int rc;

    if ( mfn_eq(mfn, INVALID_MFN) )
    {
        return -EINVAL;
    }

    /* TODO: Support pagetable root for different CPUs (SMP) */
    entry = &xen_second_pagetable[pagetable_second_index(va)];
    if ( !pte_is_valid(entry) )
    {
        rc = alloc_xen_table(entry);
        if ( rc )
        {
            return rc;
        }
    }
    else if ( pte_is_leaf(entry) )
    {
        /* Breaking up gigapages is not supported */
        return -EOPNOTSUPP;
    }

    first = (pte_t *)maddr_to_virt(pte_to_paddr(entry));

    entry = &first[pagetable_first_index(va)];
    if ( !pte_is_valid(entry) )
    {
        rc = alloc_xen_table(entry);
        if ( rc )
        {
            return rc;
        }
    }
    else if ( pte_is_leaf(entry) )
    {
        /* Breaking up megapages is not supported */
        return -EOPNOTSUPP;
    }

    zeroeth = (pte_t *)maddr_to_virt(pte_to_paddr(entry));

    entry = &zeroeth[pagetable_zeroeth_index(va)];
    pte = mfn_to_xen_entry(mfn);
    pte.pte |= PTE_LEAF_DEFAULT;
    write_pte(entry, pte);

    return 0;
}

static DEFINE_SPINLOCK(xen_pt_lock);

int map_pages_to_xen(unsigned long virt, mfn_t mfn, unsigned long nr_mfns,
                     unsigned int flags)
{
    int rc = 0;
    unsigned long addr = virt, addr_end = addr + nr_mfns * PAGE_SIZE;

    rc = 1;

    if ( !IS_ALIGNED(virt, PAGE_SIZE) )
    {
        mm_printk("The virtual address is not aligned to the page-size.\n");
        return -EINVAL;
    }

    spin_lock(&xen_pt_lock);
    while ( addr < addr_end )
    {
        rc = xen_pt_update(addr, mfn, flags);
        if ( rc == XEN_TABLE_MAP_FAILED )
            break;

        mfn = mfn_add(mfn, 1);
        addr += PAGE_SIZE;
    }

    /*
     * Flush the TLBs even in case of failure because we may have
     * partially modified the PT. This will prevent any unexpected
     * behavior afterwards.
     */
    asm volatile("sfence.vma");
    spin_unlock(&xen_pt_lock);

    return 0;
}

int populate_pt_range(unsigned long virt, unsigned long nr_mfns)
{
    (void) virt;
    (void) nr_mfns;

    /* TODO */

    return  0;
}

int destroy_xen_mappings(unsigned long v, unsigned long e)
{
    (void) v;
    (void) e;

    /* TODO */

    return 0;
}

int modify_xen_mappings(unsigned long s, unsigned long e, unsigned int flags)
{
    (void) s;
    (void) e;
    (void) flags;

    /* TODO */

    return 0;
}

void arch_dump_shared_mem_info(void)
{
    /* TODO */
}

int donate_page(struct domain *d, struct page_info *page, unsigned int memflags)
{
    ASSERT_UNREACHABLE();
    return -ENOSYS;
}

int steal_page(struct domain *d, struct page_info *page, unsigned int memflags)
{
    return -EOPNOTSUPP;
}

int page_is_ram_type(unsigned long mfn, unsigned long mem_type)
{
    ASSERT_UNREACHABLE();
    return 0;
}

unsigned long domain_get_maximum_gpfn(struct domain *d)
{
    return gfn_x(d->arch.p2m.max_mapped_gfn);
}

void share_xen_page_with_guest(struct page_info *page, struct domain *d,
                               enum XENSHARE_flags flags)
{
    if ( page_get_owner(page) == d )
        return;

    spin_lock(&d->page_alloc_lock);

    /* TODO */

    spin_unlock(&d->page_alloc_lock);
}

int xenmem_add_to_physmap_one(struct domain *d, unsigned int space,
                              union xen_add_to_physmap_batch_extra extra,
                              unsigned long idx, gfn_t gfn)
{
    /* TODO */

    return 0;
}

long arch_memory_op(int op, XEN_GUEST_HANDLE_PARAM(void) arg)
{
    /* TODO */
    return 0;
}

struct domain *page_get_owner_and_reference(struct page_info *page)
{
    unsigned long x, y = page->count_info;
    struct domain *owner;

    do
    {
        x = y;
        /*
         * Count ==  0: Page is not allocated, so we cannot take a reference.
         * Count == -1: Reference count would wrap, which is invalid.
         */
        if ( unlikely(((x + 1) & PGC_count_mask) <= 1) )
            return NULL;
    } while ( (y = cmpxchg(&page->count_info, x, x + 1)) != x );

    owner = page_get_owner(page);
    ASSERT(owner);

    return owner;
}

void put_page(struct page_info *page)
{
    unsigned long nx, x, y = page->count_info;

    do
    {
        ASSERT((y & PGC_count_mask) != 0);
        x = y;
        nx = x - 1;
    } while ( unlikely((y = cmpxchg(&page->count_info, x, nx)) != x) );

    if ( unlikely((nx & PGC_count_mask) == 0) )
    {
        free_domheap_page(page);
    }
}

int get_page(struct page_info *page, struct domain *domain)
{
    struct domain *owner = page_get_owner_and_reference(page);

    if ( likely(owner == domain) )
        return 1;

    if ( owner != NULL )
        put_page(page);

    return 0;
}

/* Common code requires get_page_type and put_page_type.
 * We don't care about typecounts so we just do the minimum to make it
 * happy. */
int get_page_type(struct page_info *page, unsigned long type)
{
    return 1;
}

void put_page_type(struct page_info *page)
{
    return;
}

/*
 * This function should only be used to remap device address ranges
 * TODO: add a check to verify this assumption
 */
void *ioremap_attr(paddr_t pa, size_t len, unsigned int attributes)
{
    mfn_t mfn = _mfn(PFN_DOWN(pa));
    unsigned int offs = pa & (PAGE_SIZE - 1);
    unsigned int nr = PFN_UP(offs + len);

    void *ptr = __vmap(&mfn, nr, 1, 1, attributes, VMAP_DEFAULT);

    if ( ptr == NULL )
        return NULL;

    return ptr + offs;
}

void *ioremap(paddr_t pa, size_t len)
{
    return ioremap_attr(pa, len, PAGE_HYPERVISOR_NOCACHE);
}

void gnttab_clear_flags(struct domain *d, unsigned long nr, uint16_t *addr)
{
    /*
     * Note that this cannot be clear_bit(), as the access must be
     * confined to the specified 2 bytes.
     */
    uint16_t mask = ~(1 << nr), old;

    do
    {
        old = *addr;
    } while ( cmpxchg(addr, old, old & mask) != old );
}

void gnttab_mark_dirty(struct domain *d, mfn_t mfn)
{
    /* XXX: mark dirty */
    static int warning;
    if ( !warning )
    {
        gdprintk(XENLOG_WARNING, "gnttab_mark_dirty not implemented yet\n");
        warning = 1;
    }
}

int create_grant_host_mapping(unsigned long addr, mfn_t frame,
                              unsigned int flags, unsigned int cache_flags)
{
    int rc;
    p2m_type_t t = p2m_grant_map_rw;

    if ( cache_flags || (flags & ~GNTMAP_readonly) != GNTMAP_host_map )
        return GNTST_general_error;

    if ( flags & GNTMAP_readonly )
        t = p2m_grant_map_ro;

    rc = guest_physmap_add_entry(current->domain, gaddr_to_gfn(addr), frame, 0,
                                 t);

    if ( rc )
        return GNTST_general_error;
    else
        return GNTST_okay;
}

int replace_grant_host_mapping(unsigned long addr, mfn_t mfn,
                               unsigned long new_addr, unsigned int flags)
{
    gfn_t gfn = gaddr_to_gfn(addr);
    struct domain *d = current->domain;
    int rc;

    if ( new_addr != 0 || (flags & GNTMAP_contains_pte) )
        return GNTST_general_error;

    rc = guest_physmap_remove_page(d, gfn, mfn, 0);

    return rc ? GNTST_general_error : GNTST_okay;
}

bool is_iomem_page(mfn_t mfn)
{
    return !mfn_valid(mfn);
}

unsigned long get_upper_mfn_bound(void)
{
    /* No memory hotplug yet, so current memory limit is the final one. */
    return max_page - 1;
}

static void setup_second_level_mappings(pte_t *first_pagetable,
                                        unsigned long vaddr)
{
    unsigned long paddr;
    unsigned long index;
    pte_t *p;

    index = pagetable_second_index(vaddr);
    p = &xen_second_pagetable[index];

    if ( !pte_is_valid(p) )
    {
        paddr = phys_offset + ((unsigned long)first_pagetable);
        p->pte = addr_to_ppn(paddr);
        p->pte |= PTE_TABLE;
    }
}

void setup_megapages(pte_t *first_pagetable, unsigned long virtual_start,
                     unsigned long physical_start, unsigned long page_cnt)
{
    unsigned long frame_addr = physical_start;
    unsigned long end = physical_start + (page_cnt << PAGE_SHIFT);
    unsigned long vaddr = virtual_start;
    unsigned long index;
    pte_t *p;

    BUG_ON(!IS_ALIGNED(physical_start, FIRST_SIZE));

    while ( frame_addr < end )
    {
        setup_second_level_mappings(first_pagetable, vaddr);

        index = pagetable_first_index(vaddr);
        p = &first_pagetable[index];
        p->pte = paddr_to_megapage_ppn(frame_addr);
        p->pte |= PTE_LEAF_DEFAULT;

        frame_addr += FIRST_SIZE;
        vaddr += FIRST_SIZE;
    }

    asm volatile("sfence.vma");
}

/*
 * Convert a virtual address to a PTE with the correct PPN.
 *
 * WARNING: Only use this function while the physical addresses
 * of Xen are still mapped in as virtual addresses OR before
 * the MMU is enabled (i.e., phys_offset must still be valid).
 */
static inline pte_t pte_of_xenaddr(vaddr_t va)
{
    paddr_t ma = va + phys_offset;
    return mfn_to_xen_entry(maddr_to_mfn(ma));
}

/* Creates megapages of 2MB size based on sv39 spec */
void __init setup_xenheap_mappings(unsigned long base_mfn,
                                   unsigned long nr_mfns)
{
    unsigned long mfn, end_mfn;
    vaddr_t vaddr;
    pte_t *first, pte;

    /* Align to previous 2MB boundary */
    mfn = base_mfn & ~((FIRST_SIZE >> PAGE_SHIFT) - 1);

    /* First call sets the xenheap physical and virtual offset. */
    if ( mfn_eq(xenheap_mfn_start, INVALID_MFN) )
    {
        xenheap_mfn_start = _mfn(base_mfn);
        xenheap_base_pdx = mfn_to_pdx(_mfn(base_mfn));
        xenheap_virt_start =
            DIRECTMAP_VIRT_START + (base_mfn - mfn) * PAGE_SIZE;
    }

    if ( base_mfn < mfn_x(xenheap_mfn_start) )
        panic("cannot add xenheap mapping at %lx below heap start %lx\n",
              base_mfn, mfn_x(xenheap_mfn_start));

    end_mfn = base_mfn + nr_mfns;

    /*
     * Virtual address aligned to previous 2MB to match physical
     * address alignment done above.
     */
    vaddr = (vaddr_t)__mfn_to_virt(base_mfn) & (SECOND_MASK | FIRST_MASK);

    while ( mfn < end_mfn )
    {
        unsigned long slot = pagetable_second_index(vaddr);
        pte_t *p = &xen_second_pagetable[slot];

        if ( pte_is_valid(p) )
        {
            /* mfn_to_virt is not valid on the xen_heap_megapages mfn, since it
             * is not within the xenheap. */
            first = (slot == xen_second_heap_slot)
                        ? xen_heap_megapages
                        : mfn_to_virt(pte_get_mfn(*p));
        }
        else if ( xen_second_heap_slot == -1 )
        {
            /* Use xen_heap_megapages to bootstrap the mappings */
            first = xen_heap_megapages;
            pte = pte_of_xenaddr((vaddr_t)xen_heap_megapages);
            pte.pte |= PTE_TABLE;
            write_pte(p, pte);
            xen_second_heap_slot = slot;
        }
        else
        {
            mfn_t first_mfn = alloc_boot_pages(1, 1);
            clear_page(mfn_to_virt(first_mfn));
            pte = mfn_to_xen_entry(first_mfn);
            pte.pte |= PTE_TABLE;
            write_pte(p, pte);
            first = mfn_to_virt(first_mfn);
        }

        pte = mfn_to_xen_entry(_mfn(mfn));
        pte.pte |= PTE_LEAF_DEFAULT;
        write_pte(&first[pagetable_first_index(vaddr)], pte);

        /*
         * We are mapping pages at the 2MB first-level granularity, so increment
         * by FIRST_SIZE.
         */
        mfn += FIRST_SIZE >> PAGE_SHIFT;
        vaddr += FIRST_SIZE;
    }

    asm volatile("sfence.vma");
}

void __init clear_pagetables(unsigned long load_addr, unsigned long linker_addr)
{
    unsigned long *p;
    unsigned long page;
    unsigned long i;

    page = (unsigned long)&xen_second_pagetable[0];
    p = (unsigned long *)(page + load_addr - linker_addr);
    for ( i = 0; i < ARRAY_SIZE(xen_second_pagetable); i++ )
    {
        p[i] = 0ULL;
    }

    page = (unsigned long)&xen_first_pagetable[0];
    p = (unsigned long *)(page + load_addr - linker_addr);
    for ( i = 0; i < ARRAY_SIZE(xen_first_pagetable); i++ )
    {
        p[i] = 0ULL;
    }

    page = (unsigned long)&xen_zeroeth_pagetable[0];
    p = (unsigned long *)(page + load_addr - linker_addr);
    for ( i = 0; i < ARRAY_SIZE(xen_zeroeth_pagetable); i++ )
    {
        p[i] = 0ULL;
    }
}

void __attribute__((section(".entry")))
setup_initial_pagetables(pte_t *second, pte_t *first, pte_t *zeroeth,
                         unsigned long map_start, unsigned long map_end,
                         unsigned long pa_start)
{
    unsigned long page_addr;
    unsigned long index2;
    unsigned long index1;
    unsigned long index0;

    /* align start addresses */
    map_start &= ZEROETH_MAP_MASK;
    pa_start &= ZEROETH_MAP_MASK;

    page_addr = map_start;
    while ( page_addr < map_end )
    {
        index2 = pagetable_second_index(page_addr);
        index1 = pagetable_first_index(page_addr);
        index0 = pagetable_zeroeth_index(page_addr);

        /* Setup level2 table */
        second[index2] = paddr_to_pte((unsigned long)first);
        second[index2].pte |= PTE_TABLE;

        /* Setup level1 table */
        first[index1] = paddr_to_pte((unsigned long)zeroeth);
        first[index1].pte |= PTE_TABLE;

        /* Setup level0 table */
        if ( !pte_is_valid(&zeroeth[index0]) )
        {
            /* Update level0 table */
            zeroeth[index0] = paddr_to_pte((page_addr - map_start) + pa_start);
            zeroeth[index0].pte |= PTE_LEAF_DEFAULT;
        }

        /* Point to next page */
        page_addr += ZEROETH_SIZE;
    }
}

/*
 * WARNING: load_addr() and linker_addr() are to be called only when the MMU is
 * disabled and only when executed by the primary CPU.  They cannot refer to
 * any global variable or functions.
 */

/*
 * Convert an addressed layed out at link time to the address where it was loaded
 * by the bootloader.
 */
#define load_addr(linker_address)                                              \
    ({                                                                         \
        unsigned long __linker_address = (unsigned long)(linker_address);      \
        if ( linker_addr_start <= __linker_address &&                           \
            __linker_address < linker_addr_end )                                \
        {                                                                      \
            __linker_address =                                                 \
                __linker_address - linker_addr_start + load_addr_start;        \
        }                                                                      \
        __linker_address;                                                      \
    })

/* Convert boot-time Xen address from where it was loaded by the boot loader to the address it was layed out
 * at link-time.
 */
#define linker_addr(load_address)                                              \
    ({                                                                         \
        unsigned long __load_address = (unsigned long)(load_address);          \
        if ( load_addr_start <= __load_address &&                               \
            __load_address < load_addr_end )                                    \
        {                                                                      \
            __load_address =                                                   \
                __load_address - load_addr_start + linker_addr_start;          \
        }                                                                      \
        __load_address;                                                        \
    })

/*
 * _setup_initial_pagetables:
 *
 * 1) Build the page tables for Xen that map the following:
 *   1.1)  The physical location of Xen (where the bootloader loaded it)
 *   1.2)  The link-time location of Xen (where the linker expected Xen's
 *         addresses to be)
 * 2) Load the page table into the SATP and enable the MMU
 */
void __attribute__((section(".entry")))
_setup_initial_pagetables(unsigned long load_addr_start,
                          unsigned long load_addr_end,
                          unsigned long linker_addr_start,
                          unsigned long linker_addr_end)
{
    pte_t *second;
    pte_t *first;
    pte_t *zeroeth;

    clear_pagetables(load_addr_start, linker_addr_start);

    /* Get the addresses where the page tables were loaded */
    second = (pte_t *)load_addr(&xen_second_pagetable);
    first = (pte_t *)load_addr(&xen_first_pagetable);
    zeroeth = (pte_t *)load_addr(&xen_zeroeth_pagetable);

    /*
     * Create a mapping of the load time address range to... the load time address range.
     * This mapping is used at boot time only.
     */
    setup_initial_pagetables(second, first, zeroeth, load_addr_start,
                             load_addr_end, load_addr_start);

    /*
     * Create a mapping from Xen's link-time addresses to where they were actually loaded.
     *
     * TODO: Protect regions accordingly (e.g., protect text and rodata from writes).
     */
    setup_initial_pagetables(second, first, zeroeth, linker_addr(&_text_start),
                             linker_addr(&_text_end), load_addr(&_text_start));
    setup_initial_pagetables(second, first, zeroeth, linker_addr(&_init_start),
                             linker_addr(&_init_end), load_addr(&_init_start));
    setup_initial_pagetables(second, first, zeroeth,
                             linker_addr(&_cpuinit_start),
                             linker_addr(&_cpuinit_end),
                             load_addr(&_cpuinit_start));
    setup_initial_pagetables(second, first, zeroeth,
                             linker_addr(&_spinlock_start),
                             linker_addr(&_spinlock_end),
                             load_addr(&_spinlock_start));
    setup_initial_pagetables(second, first, zeroeth,
                             linker_addr(&_rodata_start),
                             linker_addr(&_rodata_end),
                             load_addr(&_rodata_start));
    setup_initial_pagetables(second, first, zeroeth, linker_addr_start,
                             linker_addr_end, load_addr_start);

    /* Ensure page table writes precede loading the SATP */
    asm volatile("sfence.vma");

    /* Enable the MMU and load the new pagetable for Xen */
    csr_write(satp, 
              (load_addr(xen_second_pagetable) >> PAGE_SHIFT) | SATP_MODE);

    phys_offset = load_addr_start > linker_addr_start ?
                      load_addr_start - linker_addr_start :
                      linker_addr_start - load_addr_start;
}

/* Map a frame table to cover physical addresses ps through pe */
void __init setup_frametable_mappings(paddr_t ps, paddr_t pe)
{
    unsigned long nr_pdxs = mfn_to_pdx(mfn_add(maddr_to_mfn(pe), -1)) -
                            mfn_to_pdx(maddr_to_mfn(ps)) + 1;
    unsigned long frametable_size = nr_pdxs * sizeof(struct page_info);
    unsigned long virt_end;
    pte_t *first_table;
    mfn_t mfn, base, first;
    pte_t pte;
    unsigned long i, first_entries_remaining;

    frametable_base_pdx = mfn_to_pdx(maddr_to_mfn(ps));

    /* Allocate enough pages to hold the whole address space */
    base = alloc_boot_pages(frametable_size >> PAGE_SHIFT, MB(2) >> PAGE_SHIFT);
    virt_end = FRAMETABLE_VIRT_START + frametable_size;

    first_entries_remaining = 0;
    mfn = base;

    /* Map the frametable virtual address speace to thse pages */
    for ( i = ROUNDUP(FRAMETABLE_VIRT_START, MB(2)); i < virt_end; i += MB(2) )
    {
        /* If this frame has filled up all entries, then allocate a new table */
        if ( first_entries_remaining <= 0 )
        {
            /* Allocate page for a first-level table */
            first = alloc_boot_pages(1, 1);

            /* Reset counter */
            first_entries_remaining = 512;
        }

        /* Convert the first-level table from it's machine frame number to a virtual_address */
        first_table = (pte_t *)mfn_to_virt(first);

        pte = mfn_to_xen_entry(mfn);
        pte.pte |= PTE_LEAF_DEFAULT;

        /* Point the first-level table to the machine frame */
        write_pte(&first_table[pagetable_first_index(i)], pte);

        /* Convert the first-level table address into a PTE */
        pte = mfn_to_xen_entry(maddr_to_mfn(virt_to_maddr(&first_table[0])));
        pte.pte |= PTE_TABLE;

        /* Point the second-level table to the first-level table */
        write_pte(&xen_second_pagetable[pagetable_second_index(i)], pte);

        /* First-level tables are at a 2MB granularity so go to the next 2MB page */
        mfn = mfn_add(mfn, MB(2) >> PAGE_SHIFT);

        /* We've used an entry, so decrement the counter */
        first_entries_remaining--;
    }

    memset(&frame_table[0], 0, nr_pdxs * sizeof(struct page_info));
    memset(&frame_table[nr_pdxs], -1,
           frametable_size - (nr_pdxs * sizeof(struct page_info)));

    frametable_virt_end =
        FRAMETABLE_VIRT_START + (nr_pdxs * sizeof(struct page_info));
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
