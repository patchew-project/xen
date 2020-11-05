
#include <asm/cpufeature.h>
#include <xen/init.h>

static int __init update_lse_caps(void)
{
    if ( cpu_has_lse_atomics )
        cpus_set_cap(ARM64_HAS_LSE_ATOMICS);

    return 0;
}

__initcall(update_lse_caps);
