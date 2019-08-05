#include <xen/types.h>
#include <xen/lib.h>
#include <xen/percpu.h>
#include <xen/mm.h>

#include <asm/desc.h>

/*
 * Native and Compat GDTs used by Xen.
 *
 * The R1 and R3 descriptors are fixed in Xen's ABI for PV guests.  All other
 * descriptors are in principle variable, with the following restrictions.
 *
 * All R0 descriptors must line up in both GDTs to allow for correct
 * interrupt/exception handling.
 *
 * The SYSCALL/SYSRET GDT layout requires:
 *  - R0 long mode code followed by R0 data.
 *  - R3 compat code, followed by R3 data, followed by R3 long mode code.
 *
 * The SYSENTER GDT layout requirements are compatible with SYSCALL.  Xen does
 * not use the SYSEXIT instruction, and does not provide a compatible GDT.
 *
 * These tables are used directly by CPU0, and used as the template for the
 * GDTs of other CPUs.  Everything from the TSS onwards is unique per CPU.
 */
__section(".data.page_aligned") __aligned(PAGE_SIZE)
seg_desc_t boot_gdt[PAGE_SIZE / sizeof(seg_desc_t)] =
{
    [ 1] = { 0x00af9a000000ffff }, /* 0xe008 - Ring 0 code, 64bit mode      */
    [ 2] = { 0x00cf92000000ffff }, /* 0xe010 - Ring 0 data                  */
                                   /* 0xe018 - reserved                     */
    [ 4] = { 0x00cffa000000ffff }, /* 0xe023 - Ring 3 code, compatibility   */
    [ 5] = { 0x00cff2000000ffff }, /* 0xe02b - Ring 3 data                  */
    [ 6] = { 0x00affa000000ffff }, /* 0xe033 - Ring 3 code, 64-bit mode     */
    [ 7] = { 0x00cf9a000000ffff }, /* 0xe038 - Ring 0 code, compatibility   */
                                   /* 0xe040 - TSS                          */
                                   /* 0xe050 - LDT                          */
    [12] = { 0x0000910000000000 }, /* 0xe060 - per-CPU entry (limit == cpu) */
};

__section(".data.page_aligned") __aligned(PAGE_SIZE)
seg_desc_t boot_compat_gdt[PAGE_SIZE / sizeof(seg_desc_t)] =
{
    [ 1] = { 0x00af9a000000ffff }, /* 0xe008 - Ring 0 code, 64-bit mode     */
    [ 2] = { 0x00cf92000000ffff }, /* 0xe010 - Ring 0 data                  */
    [ 3] = { 0x00cfba000000ffff }, /* 0xe019 - Ring 1 code, compatibility   */
    [ 4] = { 0x00cfb2000000ffff }, /* 0xe021 - Ring 1 data                  */
    [ 5] = { 0x00cffa000000ffff }, /* 0xe02b - Ring 3 code, compatibility   */
    [ 6] = { 0x00cff2000000ffff }, /* 0xe033 - Ring 3 data                  */
    [ 7] = { 0x00cf9a000000ffff }, /* 0xe038 - Ring 0 code, compatibility   */
                                   /* 0xe040 - TSS                          */
                                   /* 0xe050 - LDT                          */
    [12] = { 0x0000910000000000 }, /* 0xe060 - per-CPU entry (limit == cpu) */
};

/*
 * Used by each CPU as it starts up, to enter C with a suitable %cs.
 * References boot_cpu_gdt_table for a short period, until the CPUs switch
 * onto their per-CPU GDTs.
 */
struct desc_ptr boot_gdtr = {
    .limit = LAST_RESERVED_GDT_BYTE,
    .base = (unsigned long)(boot_gdt - FIRST_RESERVED_GDT_ENTRY),
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
