/******************************************************************************
 * arch/x86/guest/hyperv/hyperv.c
 *
 * Support for detecting and running under Hyper-V.
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
#include <xen/init.h>
#include <xen/domain_page.h>

#include <asm/guest.h>
#include <asm/guest/hyperv-tlfs.h>

struct ms_hyperv_info __read_mostly ms_hyperv;

extern char hv_hypercall_page[];
DEFINE_PER_CPU_READ_MOSTLY(void *, hv_pcpu_input_arg);
DEFINE_PER_CPU_READ_MOSTLY(unsigned int, hv_vp_index);

static const struct hypervisor_ops ops;
const struct hypervisor_ops *__init hyperv_probe(void)
{
    uint32_t eax, ebx, ecx, edx;
    uint64_t required_msrs = HV_X64_MSR_HYPERCALL_AVAILABLE |
        HV_X64_MSR_VP_INDEX_AVAILABLE;

    cpuid(0x40000000, &eax, &ebx, &ecx, &edx);
    if ( !((ebx == 0x7263694d) &&  /* "Micr" */
           (ecx == 0x666f736f) &&  /* "osof" */
           (edx == 0x76482074)) )  /* "t Hv" */
        return NULL;

    cpuid(0x40000001, &eax, &ebx, &ecx, &edx);
    if ( eax != 0x31237648 )    /* Hv#1 */
        return NULL;

    /* Extract more information from Hyper-V */
    cpuid(HYPERV_CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
    ms_hyperv.features = eax;
    ms_hyperv.misc_features = edx;

    ms_hyperv.hints = cpuid_eax(HYPERV_CPUID_ENLIGHTMENT_INFO);

    if ( ms_hyperv.hints & HV_X64_ENLIGHTENED_VMCS_RECOMMENDED )
        ms_hyperv.nested_features = cpuid_eax(HYPERV_CPUID_NESTED_FEATURES);

    cpuid(HYPERV_CPUID_IMPLEMENT_LIMITS, &eax, &ebx, &ecx, &edx);
    ms_hyperv.max_vp_index = eax;
    ms_hyperv.max_lp_index = ebx;

    if ( (ms_hyperv.features & required_msrs) != required_msrs )
    {
        /*
         * Oops, required MSRs are not available. Treat this as
         * "Hyper-V is not available".
         */
        memset(&ms_hyperv, 0, sizeof(ms_hyperv));
        return NULL;
    }

    return &ops;
}

static void __init setup_hypercall_page(void)
{
    union hv_x64_msr_hypercall_contents hypercall_msr;

    rdmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);
    hypercall_msr.enable = 1;
    hypercall_msr.guest_physical_address =
        __pa(hv_hypercall_page) >> HV_HYP_PAGE_SHIFT;
    wrmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);
}

static void setup_hypercall_pcpu_arg(void)
{
    void *mapping;
    uint64_t vp_index_msr;

    mapping = alloc_xenheap_page();
    if ( !mapping )
        panic("Failed to allocate hypercall input page for %u\n",
              smp_processor_id());

    this_cpu(hv_pcpu_input_arg) = mapping;

    rdmsrl(HV_X64_MSR_VP_INDEX, vp_index_msr);
    this_cpu(hv_vp_index) = vp_index_msr;
}

static void __init setup(void)
{
    setup_hypercall_page();
    setup_hypercall_pcpu_arg();
}

static void ap_setup(void)
{
    setup_hypercall_pcpu_arg();
}

static const struct hypervisor_ops ops = {
    .name = "Hyper-V",
    .setup = setup,
    .ap_setup = ap_setup,
};

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
