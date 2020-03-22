/******************************************************************************
 * include/xen/mm_types.h
 *
 * Definitions for memory pages, frame numbers, addresses, allocations, etc.
 *
 * Copyright (c) 2002-2006, K A Fraser <keir@xensource.com>
 *
 *                         +---------------------+
 *                          Xen Memory Management
 *                         +---------------------+
 *
 * Xen has to handle many different address spaces.  It is important not to
 * get these spaces mixed up.  The following is a consistent terminology which
 * should be adhered to.
 *
 * mfn: Machine Frame Number
 *   The values Xen puts into its own pagetables.  This is the host physical
 *   memory address space with RAM, MMIO etc.
 *
 * gfn: Guest Frame Number
 *   The values a guest puts in its own pagetables.  For an auto-translated
 *   guest (hardware assisted with 2nd stage translation, or shadowed), gfn !=
 *   mfn.  For a non-translated guest which is aware of Xen, gfn == mfn.
 *
 * pfn: Pseudophysical Frame Number
 *   A linear idea of a guest physical address space. For an auto-translated
 *   guest, pfn == gfn while for a non-translated guest, pfn != gfn.
 *
 * dfn: Device DMA Frame Number (definitions in include/xen/iommu.h)
 *   The linear frame numbers of device DMA address space. All initiators for
 *   (i.e. all devices assigned to) a guest share a single DMA address space
 *   and, by default, Xen will ensure dfn == pfn.
 *
 * WARNING: Some of these terms have changed over time while others have been
 * used inconsistently, meaning that a lot of existing code does not match the
 * definitions above.  New code should use these terms as described here, and
 * over time older code should be corrected to be consistent.
 *
 * An incomplete list of larger work area:
 * - Phase out the use of 'pfn' from the x86 pagetable code.  Callers should
 *   know explicitly whether they are talking about mfns or gfns.
 * - Phase out the use of 'pfn' from the ARM mm code.  A cursory glance
 *   suggests that 'mfn' and 'pfn' are currently used interchangeably, where
 *   'mfn' is the appropriate term to use.
 * - Phase out the use of gpfn/gmfn where pfn/mfn are meant.  This excludes
 *   the x86 shadow code, which uses gmfn/smfn pairs with different,
 *   documented, meanings.
 */

#ifndef __XEN_MM_TYPES_H__
#define __XEN_MM_TYPES_H__

#include <xen/typesafe.h>
#include <xen/kernel.h>

TYPE_SAFE(unsigned long, mfn);
#define PRI_mfn          "05lx"
#define INVALID_MFN      _mfn(~0UL)
/*
 * To be used for global variable initialization. This workaround a bug
 * in GCC < 5.0.
 */
#define INVALID_MFN_INITIALIZER { ~0UL }

#ifndef mfn_t
#define mfn_t /* Grep fodder: mfn_t, _mfn() and mfn_x() are defined above */
#define _mfn
#define mfn_x
#undef mfn_t
#undef _mfn
#undef mfn_x
#endif

static inline mfn_t mfn_add(mfn_t mfn, unsigned long i)
{
    return _mfn(mfn_x(mfn) + i);
}

static inline mfn_t mfn_max(mfn_t x, mfn_t y)
{
    return _mfn(max(mfn_x(x), mfn_x(y)));
}

static inline mfn_t mfn_min(mfn_t x, mfn_t y)
{
    return _mfn(min(mfn_x(x), mfn_x(y)));
}

static inline bool_t mfn_eq(mfn_t x, mfn_t y)
{
    return mfn_x(x) == mfn_x(y);
}

TYPE_SAFE(unsigned long, gfn);
#define PRI_gfn          "05lx"
#define INVALID_GFN      _gfn(~0UL)
/*
 * To be used for global variable initialization. This workaround a bug
 * in GCC < 5.0 https://gcc.gnu.org/bugzilla/show_bug.cgi?id=64856
 */
#define INVALID_GFN_INITIALIZER { ~0UL }

#ifndef gfn_t
#define gfn_t /* Grep fodder: gfn_t, _gfn() and gfn_x() are defined above */
#define _gfn
#define gfn_x
#undef gfn_t
#undef _gfn
#undef gfn_x
#endif

static inline gfn_t gfn_add(gfn_t gfn, unsigned long i)
{
    return _gfn(gfn_x(gfn) + i);
}

static inline gfn_t gfn_max(gfn_t x, gfn_t y)
{
    return _gfn(max(gfn_x(x), gfn_x(y)));
}

static inline gfn_t gfn_min(gfn_t x, gfn_t y)
{
    return _gfn(min(gfn_x(x), gfn_x(y)));
}

static inline bool_t gfn_eq(gfn_t x, gfn_t y)
{
    return gfn_x(x) == gfn_x(y);
}

TYPE_SAFE(unsigned long, pfn);
#define PRI_pfn          "05lx"
#define INVALID_PFN      (~0UL)

#ifndef pfn_t
#define pfn_t /* Grep fodder: pfn_t, _pfn() and pfn_x() are defined above */
#define _pfn
#define pfn_x
#undef pfn_t
#undef _pfn
#undef pfn_x
#endif

#endif /* __XEN_MM_TYPES_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
