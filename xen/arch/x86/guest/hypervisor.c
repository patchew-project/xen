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
 * Copyright (c) 2019 Microsoft.
 */

#include <xen/types.h>

#include <asm/cache.h>
#include <asm/guest.h>

static struct hypervisor_ops *hops __read_mostly;

bool hypervisor_probe(void)
{
    if ( hops )
        return true;

    /* Too early to use cpu_has_hypervisor */
    if ( !(cpuid_ecx(1) & cpufeat_mask(X86_FEATURE_HYPERVISOR)) )
        return false;

#ifdef CONFIG_XEN_GUEST
    if ( xen_probe() )
    {
        hops = &xen_ops;
        return true;
    }
#endif

    return false;
}

const char *hypervisor_name(void)
{
    ASSERT(hops);
    return hops->name;
}

void hypervisor_setup(void)
{
    if ( hops && hops->setup )
        hops->setup();
}

void hypervisor_ap_setup(void)
{
    if ( hops && hops->ap_setup )
        hops->ap_setup();
}

void hypervisor_resume(void)
{
    if ( hops && hops->resume )
        hops->resume();
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
