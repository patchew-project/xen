/*
 * Copyright (C) 2009 Chen Liqin <liqin.chen@sunplusct.com>
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2017 SiFive
 * Copyright (C) 2017 XiaojingZhu <zhuxiaoj@ict.ac.cn>
 * Copyright (C) 2019 Bobby Eshleman <bobbyeshleman@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#ifndef _ASM_RISCV_PAGE_H
#define _ASM_RISCV_PAGE_H

#include <public/xen.h>
#include <xen/const.h>
#include <xen/config.h>
#include <asm/riscv_encoding.h>
#include <asm/asm.h>

/*
 * PAGE_OFFSET -- the first address of the first page of memory.
 * When not using MMU this corresponds to the first free page in
 * physical memory (aligned on a page boundary).
 */
#define PAGE_OFFSET		_AC(CONFIG_PAGE_OFFSET, UL)

#define KERN_VIRT_SIZE (-PAGE_OFFSET)

#define PAGE_ENTRIES           512
#define VPN_BITS               (9)
#define VPN_MASK               ((unsigned long)((1 << VPN_BITS) - 1))

#ifdef CONFIG_RISCV_64
/* L3 index Bit[47:39] */
#define THIRD_SHIFT            (39)
#define THIRD_MASK             (VPN_MASK << THIRD_SHIFT)
/* L2 index Bit[38:30] */
#define SECOND_SHIFT           (30)
#define SECOND_MASK            (VPN_MASK << SECOND_SHIFT)
/* L1 index Bit[29:21] */
#define FIRST_SHIFT            (21)
#define FIRST_MASK             (VPN_MASK << FIRST_SHIFT)
/* L0 index Bit[20:12] */
#define ZEROETH_SHIFT          (12)
#define ZEROETH_MASK           (VPN_MASK << ZEROETH_SHIFT)

#else // CONFIG_RISCV_32

/* L1 index Bit[31:22] */
#define FIRST_SHIFT            (22)
#define FIRST_MASK             (VPN_MASK << FIRST_SHIFT)

/* L0 index Bit[21:12] */
#define ZEROETH_SHIFT          (12)
#define ZEROETH_MASK           (VPN_MASK << ZEROETH_SHIFT)
#endif

#define THIRD_SIZE             (1 << THIRD_SHIFT)
#define THIRD_MAP_MASK         (~(THIRD_SIZE - 1))
#define SECOND_SIZE            (1 << SECOND_SHIFT)
#define SECOND_MAP_MASK        (~(SECOND_SIZE - 1))
#define FIRST_SIZE             (1 << FIRST_SHIFT)
#define FIRST_MAP_MASK         (~(FIRST_SIZE - 1))
#define ZEROETH_SIZE           (1 << ZEROETH_SHIFT)
#define ZEROETH_MAP_MASK       (~(ZEROETH_SIZE - 1))

#define PTE_ADDR_MASK          0x003FFFFFFFFFFC00ULL
#define PTE_SHIFT              10
#define PTE_RSW_MASK           0x0000000000000300ULL
#define PTE_RSW_SHIFT          8

#define PTE_USER_SHIFT         4
#define PTE_PERM_MASK                (PTE_EXECUTE_MASK | \
                                      PTE_WRITE_MASK | \
                                      PTE_READ_MASK)

#define PTE_VALID       BIT(0, UL)
#define PTE_READABLE    BIT(1, UL)
#define PTE_WRITABLE    BIT(2, UL)
#define PTE_EXECUTABLE  BIT(3, UL)
#define PTE_USER        BIT(4, UL)
#define PTE_GLOBAL      BIT(5, UL)
#define PTE_ACCESSED    BIT(6, UL)
#define PTE_DIRTY       BIT(7, UL)
#define PTE_RSW         (BIT(8, UL) | BIT(9, UL))

#define PTE_LEAF_DEFAULT (PTE_VALID | PTE_READABLE | PTE_WRITABLE | PTE_EXECUTABLE)
#define PTE_TABLE (PTE_VALID)

/* Calculate the offsets into the pagetables for a given VA */
#define zeroeth_linear_offset(va) ((va) >> ZEROETH_SHIFT)
#define first_linear_offset(va) ((va) >> FIRST_SHIFT)
#define second_linear_offset(va) ((va) >> SECOND_SHIFT)
#define third_linear_offset(va) ((va) >> THIRD_SHIFT)

#define pagetable_zeroeth_index(va) zeroeth_linear_offset((va) & ZEROETH_MASK)
#define pagetable_first_index(va) first_linear_offset((va) & FIRST_MASK)
#define pagetable_second_index(va) second_linear_offset((va) & SECOND_MASK)
#define pagetable_third_index(va) third_linear_offset((va) & THIRD_MASK)

#ifndef __ASSEMBLY__

#define PAGE_UP(addr)	(((addr)+((PAGE_SIZE)-1))&(~((PAGE_SIZE)-1)))
#define PAGE_DOWN(addr)	((addr)&(~((PAGE_SIZE)-1)))

/* align addr on a size boundary - adjust address up/down if needed */
#define _ALIGN_UP(addr, size)	(((addr)+((size)-1))&(~((size)-1)))
#define _ALIGN_DOWN(addr, size)	((addr)&(~((size)-1)))

/* align addr on a size boundary - adjust address up if needed */
#define _ALIGN(addr, size)	_ALIGN_UP(addr, size)

#define clear_page(pgaddr)			memset((pgaddr), 0, PAGE_SIZE)
#define copy_page(to, from)			memcpy((to), (from), PAGE_SIZE)

#define clear_user_page(pgaddr, vaddr, page)	memset((pgaddr), 0, PAGE_SIZE)
#define copy_user_page(vto, vfrom, vaddr, topg) \
			memcpy((vto), (vfrom), PAGE_SIZE)

/*
 * Attribute Indexes.
 *
 */
#define MT_NORMAL        0x0

#define _PAGE_XN_BIT    3
#define _PAGE_RO_BIT    4
#define _PAGE_XN    (1U << _PAGE_XN_BIT)
#define _PAGE_RO    (1U << _PAGE_RO_BIT)
#define PAGE_XN_MASK(x) (((x) >> _PAGE_XN_BIT) & 0x1U)
#define PAGE_RO_MASK(x) (((x) >> _PAGE_RO_BIT) & 0x1U)

/*
 * _PAGE_DEVICE and _PAGE_NORMAL are convenience defines. They are not
 * meant to be used outside of this header.
 */
#define _PAGE_DEVICE    _PAGE_XN
#define _PAGE_NORMAL    MT_NORMAL

#define PAGE_HYPERVISOR_RO      (_PAGE_NORMAL|_PAGE_RO|_PAGE_XN)
#define PAGE_HYPERVISOR_RX      (_PAGE_NORMAL|_PAGE_RO)
#define PAGE_HYPERVISOR_RW      (_PAGE_NORMAL|_PAGE_XN)

#define PAGE_HYPERVISOR         PAGE_HYPERVISOR_RW
#define PAGE_HYPERVISOR_NOCACHE (_PAGE_DEVICE)
#define PAGE_HYPERVISOR_WC      (_PAGE_DEVICE)

/* Invalidate all instruction caches in Inner Shareable domain to PoU */
static inline void invalidate_icache(void)
{
    asm volatile ("fence.i" ::: "memory");
}

static inline int invalidate_dcache_va_range(const void *p, unsigned long size)
{
	/* TODO */
	return 0;
}

static inline int clean_dcache_va_range(const void *p, unsigned long size)
{
    /* TODO */
    return 0;
}

static inline int clean_and_invalidate_dcache_va_range
    (const void *p, unsigned long size)
{
	/* TODO */
    return 0;
}

/*
 * Use struct definitions to apply C type checking
 */

/* Page Global Directory entry */
typedef struct {
	unsigned long pgd;
} pgd_t;

/* Page Table entry */
typedef struct {
    uint64_t pte;
} pte_t;

typedef struct {
	unsigned long pgprot;
} pgprot_t;

typedef struct page *pgtable_t;

#define pte_val(x)	((x).pte)
#define pgd_val(x)	((x).pgd)
#define pgprot_val(x)	((x).pgprot)

static inline bool pte_is_table(pte_t *p)
{
    return (((p->pte) & (PTE_VALID
                        | PTE_READABLE
                        | PTE_WRITABLE
                        | PTE_EXECUTABLE)) == PTE_VALID);
}

static inline bool pte_is_valid(pte_t *p)
{
    return p->pte & PTE_VALID;
}

static inline bool pte_is_leaf(pte_t *p)
{
    return (p->pte & (PTE_WRITABLE | PTE_READABLE | PTE_EXECUTABLE));
}

/* Shift the VPN[x] or PPN[x] fields of a virtual or physical address
 * to become the shifted PPN[x] fields of a page table entry */
#define addr_to_ppn(x) (((x) >> PAGE_SHIFT) << PTE_SHIFT)

static inline pte_t paddr_to_pte(unsigned long paddr)
{
    return (pte_t) { .pte = addr_to_ppn(paddr) };
}

static inline paddr_t pte_to_paddr(pte_t *p)
{
     return (paddr_t) ((p->pte >> PTE_SHIFT) << PAGE_SHIFT);
}

#define pte_get_mfn(pte_)      _mfn(((pte_).pte) >> PTE_SHIFT)

#define MEGAPAGE_ALIGN(x) ((x) & FIRST_MAP_MASK)
#define GIGAPAGE_ALIGN(x) ((x) & SECOND_MAP_MASK)

#define paddr_to_megapage_ppn(x) addr_to_ppn(MEGAPAGE_ALIGN(x))
#define paddr_to_gigapage_ppn(x) addr_to_ppn(GIGAPAGE_ALIGN(x))

#define __pte(x)	((pte_t) { (x) })
#define __pgd(x)	((pgd_t) { (x) })
#define __pgprot(x)	((pgprot_t) { (x) })

#ifdef CONFIG_64BIT
#define PTE_FMT "%016lx"
#else
#define PTE_FMT "%08lx"
#endif

extern unsigned long va_pa_offset;
extern unsigned long pfn_base;

extern unsigned long max_low_pfn;
extern unsigned long min_low_pfn;

#define __pa(x)		((unsigned long)(x) - va_pa_offset)
#define __va(x)		((void *)((unsigned long) (x) + va_pa_offset))

#define pfn_valid(pfn) \
	(((pfn) >= pfn_base) && (((pfn)-pfn_base) < max_mapnr))

#define ARCH_PFN_OFFSET		(pfn_base)

#endif /* __ASSEMBLY__ */

#define PAGE_ALIGN(x) (((x) + PAGE_SIZE - 1) & PAGE_MASK)

#define virt_addr_valid(vaddr)	(pfn_valid(virt_to_pfn(vaddr)))

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

/* Flush the dcache for an entire page. */
void flush_page_to_ram(unsigned long mfn, bool sync_icache);

static inline uint64_t va_to_par(vaddr_t va)
{
    register unsigned long __mepc asm ("a2") = va;
    register unsigned long __mstatus asm ("a3");
    register unsigned long __bsstatus asm ("a4");
    unsigned long val;
    unsigned long rvc_mask = 3, tmp;
    asm ("csrrs %[mstatus], "STR(CSR_MSTATUS)", %[mprv]\n"
        "csrrs %[bsstatus], "STR(CSR_BSSTATUS)", %[smxr]\n"
        "and %[tmp], %[addr], 2\n"
        "bnez %[tmp], 1f\n"
#if RISCV_64
        STR(LWU) " %[insn], (%[addr])\n"
#else
        STR(LW) " %[insn], (%[addr])\n"
#endif
        "and %[tmp], %[insn], %[rvc_mask]\n"
        "beq %[tmp], %[rvc_mask], 2f\n"
        "sll %[insn], %[insn], %[xlen_minus_16]\n"
        "srl %[insn], %[insn], %[xlen_minus_16]\n"
        "j 2f\n"
        "1:\n"
        "lhu %[insn], (%[addr])\n"
        "and %[tmp], %[insn], %[rvc_mask]\n"
        "bne %[tmp], %[rvc_mask], 2f\n"
        "lhu %[tmp], 2(%[addr])\n"
        "sll %[tmp], %[tmp], 16\n"
        "add %[insn], %[insn], %[tmp]\n"
        "2: csrw "STR(CSR_BSSTATUS)", %[bsstatus]\n"
        "csrw "STR(CSR_MSTATUS)", %[mstatus]"
    : [mstatus] "+&r" (__mstatus), [bsstatus] "+&r" (__bsstatus),
      [insn] "=&r" (val), [tmp] "=&r" (tmp)
    : [mprv] "r" (MSTATUS_MPRV | SSTATUS_MXR), [smxr] "r" (SSTATUS_MXR),
      [addr] "r" (__mepc), [rvc_mask] "r" (rvc_mask),
      [xlen_minus_16] "i" (__riscv_xlen - 16));

    return val;
}

/* Write a pagetable entry. */
static inline void write_pte(pte_t *p, pte_t pte)
{
    asm volatile ("sfence.vma");
    *p = pte;
    asm volatile ("sfence.vma");
}

#endif /* _ASM_RISCV_PAGE_H */
