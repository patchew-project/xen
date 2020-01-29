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
#include <xen/version.h>

#include <asm/fixmap.h>
#include <asm/guest.h>
#include <asm/guest/hyperv-hcall.h>
#include <asm/guest/hyperv-tlfs.h>
#include <asm/processor.h>

#include "private.h"

struct ms_hyperv_info __read_mostly ms_hyperv;
DEFINE_PER_CPU_READ_MOSTLY(void *, hv_pcpu_input_page);
DEFINE_PER_CPU_READ_MOSTLY(void *, hv_vp_assist);
DEFINE_PER_CPU_READ_MOSTLY(unsigned int, hv_vp_index);

static uint64_t generate_guest_id(void)
{
    uint64_t id;

    id = (uint64_t)HV_XEN_VENDOR_ID << 48;
    id |= (xen_major_version() << 16) | xen_minor_version();

    return id;
}

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
    union hv_guest_os_id guest_id;
    unsigned long mfn;

    rdmsrl(HV_X64_MSR_GUEST_OS_ID, guest_id.raw);
    if ( !guest_id.raw )
    {
        guest_id.raw = generate_guest_id();
        wrmsrl(HV_X64_MSR_GUEST_OS_ID, guest_id.raw);
    }

    rdmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);
    if ( !hypercall_msr.enable )
    {
        mfn = HV_HCALL_MFN;
        hypercall_msr.enable = 1;
        hypercall_msr.guest_physical_address = mfn;
        wrmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);
    } else {
        mfn = hypercall_msr.guest_physical_address;
    }

    rdmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);
    BUG_ON(!hypercall_msr.enable);

    set_fixmap_x(FIX_X_HYPERV_HCALL, mfn << PAGE_SHIFT);

    /* XXX Wei: Issue an hypercall here to make sure things are set up
     * correctly.  When there is actual use of the hypercall facility,
     * this can be removed.
     */
    {
        uint16_t r = hv_do_hypercall(0xffff, 0, 0);
        BUG_ON(r != HV_STATUS_INVALID_HYPERCALL_CODE);
        r = hv_do_fast_hypercall(0xffff, 0, 0);
        BUG_ON(r != HV_STATUS_INVALID_HYPERCALL_CODE);

        printk("Successfully issued Hyper-V hypercalls\n");
    }
}

static int setup_hypercall_pcpu_arg(void)
{
    void *mapping;
    uint64_t vp_index_msr;

    if ( this_cpu(hv_pcpu_input_page) )
        return 0;

    mapping = alloc_xenheap_page();
    if ( !mapping )
    {
        printk("Failed to allocate hypercall input page for CPU%u\n",
               smp_processor_id());
        return -ENOMEM;
    }

    this_cpu(hv_pcpu_input_page) = mapping;

    rdmsrl(HV_X64_MSR_VP_INDEX, vp_index_msr);
    this_cpu(hv_vp_index) = vp_index_msr;

    return 0;
}

static int setup_vp_assist(void)
{
    void *mapping;
    uint64_t val;

    mapping = this_cpu(hv_vp_assist);

    if ( !mapping )
    {
        mapping = alloc_xenheap_page();
        if ( !mapping )
        {
            printk("Failed to allocate vp_assist page for CPU%u\n",
                   smp_processor_id());
            return -ENOMEM;
        }

        clear_page(mapping);
        this_cpu(hv_vp_assist) = mapping;
    }

    val = (virt_to_mfn(mapping) << HV_HYP_PAGE_SHIFT)
        | HV_X64_MSR_VP_ASSIST_PAGE_ENABLE;
    wrmsrl(HV_X64_MSR_VP_ASSIST_PAGE, val);

    return 0;
}

static void __init setup(void)
{
    setup_hypercall_page();

    if ( setup_hypercall_pcpu_arg() )
        panic("Hypercall percpu arg setup failed\n");

    if ( setup_vp_assist() )
        panic("VP assist page setup failed\n");
}

static int ap_setup(void)
{
    int rc;

    rc = setup_hypercall_pcpu_arg();
    if ( rc )
        goto out;

    rc = setup_vp_assist();

 out:
    return rc;
}

static const struct hypervisor_ops ops = {
    .name = "Hyper-V",
    .setup = setup,
    .ap_setup = ap_setup,
};

static void __maybe_unused build_assertions(void)
{
    /* We use 1 in linker script */
    BUILD_BUG_ON(FIX_X_HYPERV_HCALL != 1);
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
