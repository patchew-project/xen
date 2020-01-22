/*
 * xen/arch/riscv/setup.c
 *
 *
 * Early bringup code for a RISC-V RV32/64 with hypervisor
 * extensions (code H).
 *
 * Based off the ARM setup code with copyright Tim Deegan <tim@xen.org>
 *
 * Copyright (c) 2019 Bobby Eshleman <bobbyeshleman@gmail.com>
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
#include <xen/domain_page.h>
#include <xen/grant_table.h>
#include <xen/types.h>
#include <xen/string.h>
#include <xen/serial.h>
#include <xen/sched.h>
#include <xen/console.h>
#include <xen/err.h>
#include <xen/init.h>
#include <xen/irq.h>
#include <xen/mm.h>
#include <xen/softirq.h>
#include <xen/keyhandler.h>
#include <xen/cpu.h>
#include <xen/pfn.h>
#include <xen/virtual_region.h>
#include <xen/vmap.h>
#include <xen/trace.h>
#include <asm/page.h>
#include <asm/current.h>
#include <asm/setup.h>
#include <asm/setup.h>
#include <xsm/xsm.h>

/* The lucky hart to first increment this variable will boot the other cores */
atomic_t hart_lottery;
unsigned long boot_cpu_hartid;
unsigned long total_pages;

void arch_get_xen_caps(xen_capabilities_info_t *info)
{
    /* Interface name is always xen-3.0-* for Xen-3.x. */
    int major = 3, minor = 0;
    char s[32];

    (*info)[0] = '\0';

    snprintf(s, sizeof(s), "xen-%d.%d-riscv ", major, minor);
    safe_strcat(*info, s);
}

/*
 * TODO: Do not hardcode this.  There has been discussion on how OpenSBI will
 * communicate it's protected space to its payload.  Xen will need to conform
 * to that approach.
 *
 * 0x80000000 - 0x80200000 is PMP protected by OpenSBI so exclude it from the
 * ram range (any attempt at using it will trigger a PMP fault).
 */
#define OPENSBI_OFFSET 0x0200000

static void __init setup_mm(void)
{
    paddr_t ram_start, ram_end, ram_size;

    /* TODO: Use FDT instead of hardcoding these values */
    ram_start = 0x80000000 + OPENSBI_OFFSET;
    ram_size  = 0x08000000 - OPENSBI_OFFSET;
    ram_end   = ram_start + ram_size;
    total_pages = ram_size >> PAGE_SHIFT;
    pfn_pdx_hole_setup(0);
    setup_xenheap_mappings(ram_start>>PAGE_SHIFT, total_pages);
    xenheap_virt_end = XENHEAP_VIRT_START + ram_size;
    xenheap_mfn_end = maddr_to_mfn(ram_end);
    init_boot_pages(mfn_to_maddr(xenheap_mfn_start),
                    mfn_to_maddr(xenheap_mfn_end));
    max_page = PFN_DOWN(ram_end);
    setup_frametable_mappings(0, ram_end);
}

void __init start_xen(void)
{
    struct ns16550_defaults ns16550 = {
        .data_bits = 8,
        .parity    = 'n',
        .stop_bits = 1
    };

    setup_virtual_regions(NULL, NULL);
    setup_mm();
    end_boot_allocator();
    vm_init();

    ns16550.io_base = 0x10000000;
    ns16550.irq     = 10;
    ns16550.baud    = 115200;
    ns16550_init(0, &ns16550);
    console_init_preirq();

    printk("RISC-V Xen Boot!\n");
}
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
