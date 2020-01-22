/******************************************************************************
 * config.h
 *
 * A Linux-style configuration list.
 */

#ifndef __RISCV_CONFIG_H__
#define __RISCV_CONFIG_H__

#include <xen/const.h>

/*
 * RISC-V Layout:
 *   0  -   2M   Unmapped
 *   2M -   4M   Xen text, data, bss
 *   4M -   6M   Fixmap: special-purpose 4K mapping slots
 *   6M -  10M   Early boot mapping of FDT
 *   10M - 12M   Early relocation address (used when relocating Xen)
 *               and later for livepatch vmap (if compiled in)
 *
 *   All of the above is mapped in L2 slot[0] (except for Unmapped)
 *
 *   1G - 2G   VMAP: ioremap and early_ioremap (L2 slot 2)
 *
 *   2G - 5G: Unused
 *
 *   5G - 8G
 *   0x140000000 - 0x200000000
 *   Frametable: 24 bytes per page for 371GB of RAM, GB-aligned (slightly over 2GB, L2 slots [6..7])
 *
 *   8G - 12G : Unused
 *
 *   0x300000000  - 0x5fffffffff : 371GB, L2 Slots [12...384)
 *   1:1 mapping of RAM
 *
 *   0x6000000000 - 0x7fffffffff : 127GB, L2 slots [384..512)
 *   Unused
 */


#if defined(CONFIG_RISCV_64)
# define LONG_BYTEORDER 3
# define ELFSIZE 64
#else
# define LONG_BYTEORDER 2
# define ELFSIZE 32
#endif

#define BYTES_PER_LONG (1 << LONG_BYTEORDER)
#define BITS_PER_LONG (BYTES_PER_LONG << 3)
#define POINTER_ALIGN BYTES_PER_LONG

#define BITS_PER_LLONG 64

#ifdef CONFIG_RISCV_64
#define PADDR_BITS              39
#else
#define PADDR_BITS              32
#endif
#define PADDR_MASK              ((1ULL << PADDR_BITS)-1)

#define VADDR_BITS              32
#define VADDR_MASK              (~0UL)

#define PAGE_SHIFT	(12)
#define PAGE_SIZE	(_AC(1, UL) << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE - 1))

#ifdef CONFIG_RISCV_64

/* Bit counts for virtual address fields (sv39) */
#define VPN2_BITS   (9)
#define VPN1_BITS   (9)
#define VPN0_BITS   (9)
#define OFFSET_BITS (12)

/* SLOT2_ENTRY_BITS == 30 */
#define SLOT2_ENTRY_BITS  (VPN1_BITS + VPN2_BITS + OFFSET_BITS) 
#define SLOT2(slot) (_AT(vaddr_t,slot) << SLOT2_ENTRY_BITS)
#define SLOT2_ENTRY_SIZE  SLOT2(1)

#define DIRECTMAP_VIRT_START   SLOT2(12)

/* See above "RISC-V Layout" for description of layout (and
 * where these magic numbers come from */
#define DIRECTMAP_SIZE         (SLOT2_ENTRY_SIZE * (384-12))
#define DIRECTMAP_VIRT_END     (DIRECTMAP_VIRT_START + DIRECTMAP_SIZE - 1)
#define XENHEAP_VIRT_START     xenheap_virt_start
#define HYPERVISOR_VIRT_END    DIRECTMAP_VIRT_END

#else /* RISCV_32 */
#error "RISC-V 32-bit is not supported yet"
#define XENHEAP_VIRT_START     _AT(vaddr_t,0x40000000)
#define XENHEAP_VIRT_END       _AT(vaddr_t,0x7fffffff)
#define DOMHEAP_VIRT_START     _AT(vaddr_t,0x80000000)
#define DOMHEAP_VIRT_END       _AT(vaddr_t,0xffffffff)
#endif

/* xen_ulong_t is always 64 bits */
#define BITS_PER_XEN_ULONG 64

#define CONFIG_PAGING_LEVELS 3

#define CONFIG_RISCV 1

#define CONFIG_RISCV_L1_CACHE_SHIFT 7 /* XXX */

#define CONFIG_SMP 1

#define CONFIG_IRQ_HAS_MULTIPLE_ACTION 1

#define CONFIG_PAGEALLOC_MAX_ORDER 18
#define CONFIG_DOMU_MAX_ORDER      9
#define CONFIG_HWDOM_MAX_ORDER     10

#define OPT_CONSOLE_STR "dtuart"

#ifdef CONFIG_RISCV_64
#define MAX_VIRT_CPUS 128u
#else
#define MAX_VIRT_CPUS 8u
#endif

#define XEN_VIRT_START         _AT(vaddr_t,0x00200000)

#define HYPERVISOR_VIRT_START  XEN_VIRT_START

#define INVALID_VCPU_ID MAX_VIRT_CPUS

#define STACK_ORDER 3
#define STACK_SIZE  (PAGE_SIZE << STACK_ORDER)

#define VMAP_VIRT_START  GB(1)
#define VMAP_VIRT_END    (VMAP_VIRT_START + GB(1))

#define FRAMETABLE_VIRT_START  GB(5)
#define FRAMETABLE_SIZE        GB(1)
#define FRAMETABLE_NR          (FRAMETABLE_SIZE / sizeof(*frame_table))
#define FRAMETABLE_VIRT_END    (FRAMETABLE_VIRT_START + FRAMETABLE_SIZE - 1)

#ifndef ASM_NL
#define ASM_NL		 ;
#endif

#ifndef __ALIGN
#define __ALIGN		.align 4,0x90
#define __ALIGN_STR	".align 4,0x90"
#endif

#ifdef __ASSEMBLY__

#define ALIGN __ALIGN
#define ALIGN_STR __ALIGN_STR

#ifndef GLOBAL
#define GLOBAL(name) \
	.globl name ASM_NL \
	name:
#endif

#ifndef ENTRY
#define ENTRY(name) \
	.globl name ASM_NL \
	ALIGN ASM_NL \
	name:
#endif

#ifndef WEAK
#define WEAK(name)	   \
	.weak name ASM_NL   \
	ALIGN ASM_NL \
	name:
#endif

#ifndef END
#define END(name) \
	.size name, .-name
#endif

/* If symbol 'name' is treated as a subroutine (gets called, and returns)
 * then please use ENDPROC to mark 'name' as STT_FUNC for the benefit of
 * static analysis tools such as stack depth analyzer.
 */
#ifndef ENDPROC
#define ENDPROC(name) \
	.type name, @function ASM_NL \
	END(name)
#endif

#define __PAGE_ALIGNED_DATA	.section ".data..page_aligned", "aw"
#define __PAGE_ALIGNED_BSS	.section ".bss..page_aligned", "aw"

#endif

#endif /* __RISCV_CONFIG_H__ */
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
