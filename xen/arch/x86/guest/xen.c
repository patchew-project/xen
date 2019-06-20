/******************************************************************************
 * arch/x86/guest/xen.c
 *
 * Support for detecting and running under Xen.
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
#include <xen/event.h>
#include <xen/init.h>
#include <xen/mm.h>
#include <xen/pfn.h>
#include <xen/types.h>

#include <asm/apic.h>
#include <asm/e820.h>
#include <asm/guest.h>
#include <asm/msr.h>
#include <asm/processor.h>

#include <public/arch-x86/cpuid.h>
#include <public/hvm/params.h>

/* xen_detected: Xen running on Xen detected */
bool __read_mostly xen_detected;

static __read_mostly uint32_t xen_cpuid_base;

static void __init find_xen_leaves(void)
{
    uint32_t eax, ebx, ecx, edx, base;

    for ( base = XEN_CPUID_FIRST_LEAF;
          base < XEN_CPUID_FIRST_LEAF + 0x10000; base += 0x100 )
    {
        cpuid(base, &eax, &ebx, &ecx, &edx);

        if ( (ebx == XEN_CPUID_SIGNATURE_EBX) &&
             (ecx == XEN_CPUID_SIGNATURE_ECX) &&
             (edx == XEN_CPUID_SIGNATURE_EDX) &&
             ((eax - base) >= 2) )
        {
            xen_cpuid_base = base;
            break;
        }
    }
}

void __init probe_hypervisor(void)
{
    if ( xen_detected )
        return;

    /* Too early to use cpu_has_hypervisor */
    if ( !(cpuid_ecx(1) & cpufeat_mask(X86_FEATURE_HYPERVISOR)) )
        return;

    find_xen_leaves();

    if ( !xen_cpuid_base )
        return;

    xen_detected = true;

    xen_guest_enable();
}

void __init hypervisor_print_info(void)
{
    uint32_t eax, ebx, ecx, edx;
    unsigned int major, minor;

    cpuid(xen_cpuid_base + 1, &eax, &ebx, &ecx, &edx);

    major = eax >> 16;
    minor = eax & 0xffff;
    printk("Nested Xen version %u.%u.\n", major, minor);
}

uint32_t hypervisor_cpuid_base(void)
{
    return xen_cpuid_base;
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
