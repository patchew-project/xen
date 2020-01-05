#include <asm/asm_defns.h>
#include <asm/page.h>

        .section ".text.page_aligned", "ax", @progbits
        .p2align PAGE_SHIFT
GLOBAL(hv_hypercall_page)
        /* Return -1 for "not yet ready" state */
        mov -1, %rax
        ret
1:
        /* Fill the rest with `ret` */
        .fill PAGE_SIZE - (1b - hv_hypercall_page), 1, 0xc3
        .type hv_hypercall_page, STT_OBJECT
        .size hv_hypercall_page, PAGE_SIZE

/*
 * Local variables:
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 */
