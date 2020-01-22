/*
 * Copyright (C) 2015 Regents of the University of California
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

#ifndef _ASM_RISCV_CSR_H
#define _ASM_RISCV_CSR_H

#include <asm/riscv_encoding.h>

/* Status register flags */
#define SR_SIE	0x00000002UL /* Supervisor Interrupt Enable */
#define SR_SPIE	0x00000020UL /* Previous Supervisor IE */
#define SR_SPP	0x00000100UL /* Previously Supervisor */
#define SR_SUM	0x00040000UL /* Supervisor may access User Memory */

#define SR_FS           0x00006000UL /* Floating-point Status */
#define SR_FS_OFF       0x00000000UL
#define SR_FS_INITIAL   0x00002000UL
#define SR_FS_CLEAN     0x00004000UL
#define SR_FS_DIRTY     0x00006000UL

#define SR_XS           0x00018000UL /* Extension Status */
#define SR_XS_OFF       0x00000000UL
#define SR_XS_INITIAL   0x00008000UL
#define SR_XS_CLEAN     0x00010000UL
#define SR_XS_DIRTY     0x00018000UL

#ifndef CONFIG_64BIT
#define SR_SD   0x80000000UL /* FS/XS dirty */
#else
#define SR_SD   0x8000000000000000UL /* FS/XS dirty */
#endif

#define EXC_INST_MISALIGNED     0
#define EXC_INST_ACCESS         1
#define EXC_BREAKPOINT          3
#define EXC_LOAD_ACCESS         5
#define EXC_STORE_ACCESS        7
#define EXC_SYSCALL             8
#define EXC_INST_PAGE_FAULT     12
#define EXC_LOAD_PAGE_FAULT     13
#define EXC_STORE_PAGE_FAULT    15

#ifndef __ASSEMBLY__

#define csr_swap(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrrw %0, " #csr ", %1"		\
			      : "=r" (__v) : "rK" (__v)		\
			      : "memory");			\
	__v;							\
})

#define csr_read(csr)						\
({								\
	register unsigned long __v;				\
	__asm__ __volatile__ ("csrr %0, " #csr			\
			      : "=r" (__v) :			\
			      : "memory");			\
	__v;							\
})

#define csr_write(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrw " #csr ", %0"		\
			      : : "rK" (__v)			\
			      : "memory");			\
})

#define csr_read_set(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrrs %0, " #csr ", %1"		\
			      : "=r" (__v) : "rK" (__v)		\
			      : "memory");			\
	__v;							\
})

#define csr_set(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrs " #csr ", %0"		\
			      : : "rK" (__v)			\
			      : "memory");			\
})

#define csr_read_clear(csr, val)				\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrrc %0, " #csr ", %1"		\
			      : "=r" (__v) : "rK" (__v)		\
			      : "memory");			\
	__v;							\
})

#define csr_clear(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrc " #csr ", %0"		\
			      : : "rK" (__v)			\
			      : "memory");			\
})

#endif /* __ASSEMBLY__ */

#endif /* _ASM_RISCV_CSR_H */
