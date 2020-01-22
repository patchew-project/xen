/**
 * Copyright (c) 2018 Anup Patel.
 * Copyright (c) 2019 Alistair Francis <alistair.francis@wdc.com>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _ASM_RISCV_ATOMIC_H
#define _ASM_RISCV_ATOMIC_H

#include <xen/atomic.h>
#include <asm/cmpxchg.h>
#include <asm/io.h>
#include <asm/system.h>

void __bad_atomic_size(void);

#define read_atomic(p) ({                                               \
    typeof(*p) __x;                                                     \
    switch ( sizeof(*p) ) {                                             \
    case 1: __x = (typeof(*p))readb((uint8_t *)p); break;               \
    case 2: __x = (typeof(*p))readw((uint16_t *)p); break;              \
    case 4: __x = (typeof(*p))readl((uint32_t *)p); break;              \
    case 8: __x = (typeof(*p))readq((uint64_t *)p); break;              \
    default: __x = 0; __bad_atomic_size(); break;                       \
    }                                                                   \
    __x;                                                                \
})

#define write_atomic(p, x) ({                                           \
    typeof(*p) __x = (x);                                               \
    switch ( sizeof(*p) ) {                                             \
    case 1: writeb((uint8_t)__x,  (uint8_t *)  p); break;              \
    case 2: writew((uint16_t)__x, (uint16_t *) p); break;              \
    case 4: writel((uint32_t)__x, (uint32_t *) p); break;              \
    case 8: writeq((uint64_t)__x, (uint64_t *) p); break;              \
    default: __bad_atomic_size(); break;                                \
    }                                                                   \
    __x;                                                                \
})

/* TODO: Fix this */
#define add_sized(p, x) ({                                              \
    typeof(*(p)) __x = (x);                                             \
    switch ( sizeof(*(p)) )                                             \
    {                                                                   \
    case 1: writeb(read_atomic(p) + __x, (uint8_t *)(p)); break;        \
    case 2: writew(read_atomic(p) + __x, (uint16_t *)(p)); break;       \
    case 4: writel(read_atomic(p) + __x, (uint32_t *)(p)); break;       \
    default: __bad_atomic_size(); break;                                \
    }                                                                   \
})

#if __riscv_xlen == 32
static inline void atomic_add(int i, atomic_t *v)
{
	__asm__ __volatile__ (
		"	amoadd.w zero, %1, %0"
		: "+A" (v->counter)
		: "r" (i)
		: "memory");
}

static inline int atomic_add_return(int i, atomic_t *v)
{
	int ret;

	__asm__ __volatile__ (
		"	amoadd.w.aqrl  %1, %2, %0"
		: "+A" (v->counter), "=r" (ret)
		: "r" (i)
		: "memory");

	return ret + i;
}

static inline void atomic_sub(int i, atomic_t *v)
{
	__asm__ __volatile__ (
		"	amoadd.w zero, %1, %0"
		: "+A" (v->counter)
		: "r" (-i)
		: "memory");
}

static inline int atomic_sub_return(int i, atomic_t *v)
{
	int ret;

	__asm__ __volatile__ (
		"	amoadd.w.aqrl  %1, %2, %0"
		: "+A" (v->counter), "=r" (ret)
		: "r" (-i)
		: "memory");

	return ret - i;
}
#else
static inline void atomic_add(int i, atomic_t *v)
{
	__asm__ __volatile__ (
		"	amoadd.d zero, %1, %0"
		: "+A" (v->counter)
		: "r" (i)
		: "memory");
}

static inline int atomic_add_return(int i, atomic_t *v)
{
	int ret;

	__asm__ __volatile__ (
		"	amoadd.d.aqrl  %1, %2, %0"
		: "+A" (v->counter), "=r" (ret)
		: "r" (i)
		: "memory");

	return ret + i;
}

static inline void atomic_sub(int i, atomic_t *v)
{
	__asm__ __volatile__ (
		"	amoadd.d zero, %1, %0"
		: "+A" (v->counter)
		: "r" (-i)
		: "memory");
}

static inline int atomic_sub_return(int i, atomic_t *v)
{
	int ret;

	__asm__ __volatile__ (
		"	amoadd.d.aqrl  %1, %2, %0"
		: "+A" (v->counter), "=r" (ret)
		: "r" (-i)
		: "memory");

	return ret - i;
}
#endif

static inline int atomic_read(const atomic_t *v)
{
    return *(volatile int *)&v->counter;
}

static inline int _atomic_read(atomic_t v)
{
    return v.counter;
}

static inline void atomic_set(atomic_t *v, int i)
{
    v->counter = i;
}

static inline void _atomic_set(atomic_t *v, int i)
{
    v->counter = i;
}

static inline int atomic_sub_and_test(int i, atomic_t *v)
{
    return atomic_sub_return(i, v) == 0;
}

static inline void atomic_inc(atomic_t *v)
{
    atomic_add(1, v);
}

static inline int atomic_inc_return(atomic_t *v)
{
    return atomic_add_return(1, v);
}

static inline int atomic_inc_and_test(atomic_t *v)
{
    return atomic_add_return(1, v) == 0;
}

static inline void atomic_dec(atomic_t *v)
{
    atomic_sub(1, v);
}

static inline int atomic_dec_return(atomic_t *v)
{
    return atomic_sub_return(1, v);
}

static inline int atomic_dec_and_test(atomic_t *v)
{
    return atomic_sub_return(1, v) == 0;
}

static inline int atomic_add_negative(int i, atomic_t *v)
{
    return atomic_add_return(i, v) < 0;
}

#define cmpxchg(ptr, o, n)						\
({									\
	__typeof__(*(ptr)) _o_ = (o);					\
	__typeof__(*(ptr)) _n_ = (n);					\
	(__typeof__(*(ptr))) __cmpxchg((ptr),				\
				       _o_, _n_, sizeof(*(ptr)));	\
})

static inline int atomic_cmpxchg(atomic_t *v, int old, int new)
{
	return cmpxchg(&v->counter, old, new);
}

static inline int atomic_add_unless(atomic_t *v, int a, int u)
{
       int prev, rc;

	__asm__ __volatile__ (
		"0:	lr.w     %[p],  %[c]\n"
		"	beq      %[p],  %[u], 1f\n"
		"	add      %[rc], %[p], %[a]\n"
		"	sc.w.rl  %[rc], %[rc], %[c]\n"
		"	bnez     %[rc], 0b\n"
		"	fence    rw, rw\n"
		"1:\n"
		: [p]"=&r" (prev), [rc]"=&r" (rc), [c]"+A" (v->counter)
		: [a]"r" (a), [u]"r" (u)
		: "memory");
	return prev;
}

#endif /* _ASM_RISCV_ATOMIC_H */
