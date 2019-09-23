/******************************************************************************
 * arch/x86/guest/hypervisor.c
 *
 * Support for detecting and running under a hypervisor.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (c) 2017 Citrix Systems Ltd.
 */

#include <xen/init.h>
#include <xen/mm.h>
#include <xen/rangeset.h>

#include <asm/guest.h>
#include <asm/processor.h>

static struct rangeset *mem;

void __init probe_hypervisor(void)
{
    /* Too early to use cpu_has_hypervisor */
    if ( !(cpuid_ecx(1) & cpufeat_mask(X86_FEATURE_HYPERVISOR)) )
        return;

    if ( probe_xen() )
        return;

    if ( probe_hyperv() )
        return;
}

static void __init init_memmap(void)
{
    unsigned int i;

    mem = rangeset_new(NULL, "host memory map", 0);
    if ( !mem )
        panic("failed to allocate PFN usage rangeset\n");

    /*
     * Mark up to the last memory page (or 4GiB) as RAM. This is done because
     * Xen doesn't know the position of possible MMIO holes, so at least try to
     * avoid the know MMIO hole below 4GiB. Note that this is subject to future
     * discussion and improvements.
     */
    if ( rangeset_add_range(mem, 0, max_t(unsigned long, max_page - 1,
                                          PFN_DOWN(GB(4) - 1))) )
        panic("unable to add RAM to in-use PFN rangeset\n");

    for ( i = 0; i < e820.nr_map; i++ )
    {
        struct e820entry *e = &e820.map[i];

        if ( rangeset_add_range(mem, PFN_DOWN(e->addr),
                                PFN_UP(e->addr + e->size - 1)) )
            panic("unable to add range [%#lx, %#lx] to in-use PFN rangeset\n",
                  PFN_DOWN(e->addr), PFN_UP(e->addr + e->size - 1));
    }
}

void __init hypervisor_setup(void)
{
    init_memmap();

    xen_setup();
}

void hypervisor_ap_setup(void)
{
    xen_ap_setup();
}

int hypervisor_alloc_unused_page(mfn_t *mfn)
{
    unsigned long m;
    int rc;

    rc = rangeset_claim_range(mem, 1, &m);
    if ( !rc )
        *mfn = _mfn(m);

    return rc;
}

int hypervisor_free_unused_page(mfn_t mfn)
{
    return rangeset_remove_range(mem, mfn_x(mfn), mfn_x(mfn));
}

void hypervisor_resume(void)
{
    xen_resume();
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */

