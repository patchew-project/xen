#include <asm/asm.h>
#include <asm/asm-offsets.h>
#include <asm/init.h>
#include <asm/csr.h>
#include <asm/config.h>

.global _start

ENTRY(start)
	.section .entry, "ax", %progbits
_start:
	/* Mask all interrupts */
	csrw CSR_SIE, zero

	/*
	 * Disable FPU to detect illegal usage of
	 * floating point in kernel space
	 */
	li t0, SSTATUS_FS
	csrc CSR_SSTATUS, t0

	/* Save HART ID and DTB base */
	la	a6, _bootcpu_reg0
	REG_S	a0, (a6)
	la	a6, _bootcpu_reg1
	REG_S	a1, (a6)

	/* Save load addresses
	 * a2 -> load start
	 * a3 -> load end
	 * a4 -> execution start
	 * a5 -> execution end
	 */
	la	a2, _start
	la	a6, __exec_start
	REG_L	a4, (a6)
	la	a6, __exec_end
	REG_L	a5, (a6)
	sub	a6, a5, a4
	add	a3, a2, a6
	la	a6, _load_start
	REG_S	a2, (a6)
	la	a6, _load_end
	REG_S	a3, (a6)
	la	a6, _end
	REG_S	a3, (a6)

	/* Zero-out bss section */
	la	a6, __bss_start
	REG_L	a0, (a6)
	sub	a0, a0, a4
	add	a0, a0, a2
	la	a6, __bss_end
	REG_L	a1, (a6)
	sub	a1, a1, a4
	add	a1, a1, a2
_bss_zero:
	REG_S	zero, (a0)
	add	a0, a0, __SIZEOF_POINTER__
	blt	a0, a1, _bss_zero

	/* Setup temporary stack */
	la	a6, __hvc_stack_end
	REG_L	a0, (a6)
	sub	a0, a0, a4
	add	sp, a0, a2

        /* Setup hang for IRQ vector w/ virtual address */
	la	a6, __debug_irq_vector
        REG_L   a4, (a6)
	csrw	CSR_STVEC, a4

	/* Setup initial page table */
	la	a6, _load_start
	REG_L	a0, (a6)
	la	a6, _load_end
	REG_L	a1, (a6)
	la	a6, __exec_start
	REG_L	a2, (a6)
	la	a6, __exec_end
	REG_L	a3, (a6)
	call	_setup_initial_pagetables

	j	_start_secondary_nopen

	.align	3
_start_lottery:
	RISCV_PTR	0
	.align	3
__start_secondary_pen_release:
	RISCV_PTR	start_secondary_pen_release

	/*
	 * Note: From this point primary CPU startup is same as secondary CPU
	 */
_start_secondary_nopen:
	/* Set trap vector to spin forever to help debug */
	//la	a6, _start_hang
	//csrw	CSR_STVEC, a6

	/* Jump to final execution address */
	la	a6, __cpu_init
	REG_L	a0, (a6)
	jalr	a0

_start_hang:
	wfi
	j	_start_hang

	.align 3
__phys_offset:
	RISCV_PTR phys_offset
__pgtbl_root:
	RISCV_PTR xen_second_pagetable
__exec_start:
	RISCV_PTR _code_start
__exec_end:
	RISCV_PTR _code_end
__bss_start:
	RISCV_PTR _bss_start
__bss_end:
	RISCV_PTR _bss_end
__debug_irq_vector:
	RISCV_PTR _start_hang
__cpu_init:
	RISCV_PTR _cpu_init

	/*
	 * Boot register 0 passed by bootloader
	 */
	.globl _bootcpu_reg0
_bootcpu_reg0:
	RISCV_PTR 0x0

	/*
	 * Boot register 1 passed by bootloader
	 */
	.globl _boot_reg1
_bootcpu_reg1:
	RISCV_PTR 0x0

	/*
	 * Load start address storage
	 */
	.globl _load_start
_load_start:
	RISCV_PTR 0x0

	/*
	 * Load end address storage
	 */
	.globl _load_end
_load_end:
	RISCV_PTR 0x0

        .globl _end
_end:
	RISCV_PTR 0x0

	/*
	 * Exception stacks.
	 */
__hvc_stack_end:
	RISCV_PTR _hvc_stack_end

	.align 3
	.globl _cpu_init
_cpu_init:
        /* Set stack pointer to virtual address at end of the Xen executable */
	la	a6, __exec_end
        REG_L   sp, (a6)


	/* Jump to C code */
	call	start_xen

	/* Hang !!! */
	j	_start_hang

	ret
