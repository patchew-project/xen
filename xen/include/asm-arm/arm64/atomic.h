
/*
 * Taken from Linux 5.10-rc2 (last commit 3cea11cd5)
 *
 * Summary of changes:
 * 		- Rename header include guard to reflect Xen directory structure
 * 		- Drop redundant includes and redirect others to Xen equivalents
 * 		- Rename declarations from arch_atomic_<op>() to atomic_<op>()
 * 		- Drop atomic64_t helper declarations
 *
 * Copyright (C) 1996 Russell King.
 * Copyright (C) 2002 Deep Blue Solutions Ltd.
 * Copyright (C) 2012 ARM Ltd.
 * SPDX-License-Identifier: GPL-2.0-only
 */
#ifndef __ASM_ARM_ARM64_ATOMIC_H
#define __ASM_ARM_ARM64_ATOMIC_H

#include <xen/compiler.h>
#include <xen/types.h>

#include "lse.h"
#include "cmpxchg.h"

#define ATOMIC_OP(op)							\
static inline void op(int i, atomic_t *v)			\
{									\
	__lse_ll_sc_body(op, i, v);					\
}

ATOMIC_OP(atomic_andnot)
ATOMIC_OP(atomic_or)
ATOMIC_OP(atomic_xor)
ATOMIC_OP(atomic_add)
ATOMIC_OP(atomic_and)
ATOMIC_OP(atomic_sub)

#undef ATOMIC_OP

#define ATOMIC_FETCH_OP(name, op)					\
static inline int op##name(int i, atomic_t *v)			\
{									\
	return __lse_ll_sc_body(op##name, i, v);			\
}

#define ATOMIC_FETCH_OPS(op)						\
	ATOMIC_FETCH_OP(_relaxed, op)					\
	ATOMIC_FETCH_OP(_acquire, op)					\
	ATOMIC_FETCH_OP(_release, op)					\
	ATOMIC_FETCH_OP(        , op)

ATOMIC_FETCH_OPS(atomic_fetch_andnot)
ATOMIC_FETCH_OPS(atomic_fetch_or)
ATOMIC_FETCH_OPS(atomic_fetch_xor)
ATOMIC_FETCH_OPS(atomic_fetch_add)
ATOMIC_FETCH_OPS(atomic_fetch_and)
ATOMIC_FETCH_OPS(atomic_fetch_sub)
ATOMIC_FETCH_OPS(atomic_add_return)
ATOMIC_FETCH_OPS(atomic_sub_return)

#undef ATOMIC_FETCH_OP
#undef ATOMIC_FETCH_OPS
#define atomic_read(v)			__READ_ONCE((v)->counter)
#define atomic_set(v, i)			__WRITE_ONCE(((v)->counter), (i))

#define atomic_add_return_relaxed		atomic_add_return_relaxed
#define atomic_add_return_acquire		atomic_add_return_acquire
#define atomic_add_return_release		atomic_add_return_release
#define atomic_add_return			atomic_add_return

#define atomic_sub_return_relaxed		atomic_sub_return_relaxed
#define atomic_sub_return_acquire		atomic_sub_return_acquire
#define atomic_sub_return_release		atomic_sub_return_release
#define atomic_sub_return			atomic_sub_return

#define atomic_fetch_add_relaxed		atomic_fetch_add_relaxed
#define atomic_fetch_add_acquire		atomic_fetch_add_acquire
#define atomic_fetch_add_release		atomic_fetch_add_release
#define atomic_fetch_add			atomic_fetch_add

#define atomic_fetch_sub_relaxed		atomic_fetch_sub_relaxed
#define atomic_fetch_sub_acquire		atomic_fetch_sub_acquire
#define atomic_fetch_sub_release		atomic_fetch_sub_release
#define atomic_fetch_sub			atomic_fetch_sub

#define atomic_fetch_and_relaxed		atomic_fetch_and_relaxed
#define atomic_fetch_and_acquire		atomic_fetch_and_acquire
#define atomic_fetch_and_release		atomic_fetch_and_release
#define atomic_fetch_and			atomic_fetch_and

#define atomic_fetch_andnot_relaxed	atomic_fetch_andnot_relaxed
#define atomic_fetch_andnot_acquire	atomic_fetch_andnot_acquire
#define atomic_fetch_andnot_release	atomic_fetch_andnot_release
#define atomic_fetch_andnot		atomic_fetch_andnot

#define atomic_fetch_or_relaxed		atomic_fetch_or_relaxed
#define atomic_fetch_or_acquire		atomic_fetch_or_acquire
#define atomic_fetch_or_release		atomic_fetch_or_release
#define atomic_fetch_or			atomic_fetch_or

#define atomic_fetch_xor_relaxed		atomic_fetch_xor_relaxed
#define atomic_fetch_xor_acquire		atomic_fetch_xor_acquire
#define atomic_fetch_xor_release		atomic_fetch_xor_release
#define atomic_fetch_xor			atomic_fetch_xor

#define atomic_xchg_relaxed(v, new) \
	xchg_relaxed(&((v)->counter), (new))
#define atomic_xchg_acquire(v, new) \
	xchg_acquire(&((v)->counter), (new))
#define atomic_xchg_release(v, new) \
	xchg_release(&((v)->counter), (new))
#define atomic_xchg(v, new) \
	xchg(&((v)->counter), (new))

#define atomic_cmpxchg_relaxed(v, old, new) \
	cmpxchg_relaxed(&((v)->counter), (old), (new))
#define atomic_cmpxchg_acquire(v, old, new) \
	cmpxchg_acquire(&((v)->counter), (old), (new))
#define atomic_cmpxchg_release(v, old, new) \
	cmpxchg_release(&((v)->counter), (old), (new))

#define atomic_andnot			atomic_andnot

#endif /* __ASM_ARM_ARM64_ATOMIC_H */