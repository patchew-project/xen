/*
 * Portions are:
 *  Copyright (c) 2002 Pavel Machek <pavel@suse.cz>
 *  Copyright (c) 2001 Patrick Mochel <mochel@osdl.org>
 */

#include <asm/system.h>
#include <asm/xstate.h>

void restore_rest_processor_state(void)
{
    load_system_tables();

    /* Restore full CR4 (inc MCE) now that the IDT is in place. */
    write_cr4(mmu_cr4_features);

    percpu_traps_init();

    if ( cpu_has_xsave && !set_xcr0(get_xcr0()) )
        BUG();

    wrmsrl(MSR_IA32_CR_PAT, XEN_MSR_PAT);

    mtrr_bp_restore();
}
