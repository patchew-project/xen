/*
 * Taken from Linux 5.10-rc2 (last commit 3cea11cd5)
 *
 * Copyright (C) 2012 ARM Ltd.
 * SPDX-License-Identifier: GPL-2.0-only
 */
#ifndef __ASM_ARM_ARM64_CMPXCHG_H
#define __ASM_ARM_ARM64_CMPXCHG_H

#include <asm/bug.h>
#include "lse.h"

extern unsigned long __bad_cmpxchg(volatile void *ptr, int size);

/*
 * We need separate acquire parameters for ll/sc and lse, since the full
 * barrier case is generated as release+dmb for the former and
 * acquire+release for the latter.
 */
#define __XCHG_CASE(w, sfx, name, sz, mb, nop_lse, acq, acq_lse, rel, cl)	\
static inline u##sz __xchg_case_##name##sz(u##sz x, volatile void *ptr)		\
{										\
	u##sz ret;								\
	unsigned long tmp;							\
										\
	asm volatile(ARM64_LSE_ATOMIC_INSN(					\
	/* LL/SC */								\
	"	prfm	pstl1strm, %2\n"					\
	"1:	ld" #acq "xr" #sfx "\t%" #w "0, %2\n"				\
	"	st" #rel "xr" #sfx "\t%w1, %" #w "3, %2\n"			\
	"	cbnz	%w1, 1b\n"						\
	"	" #mb,								\
	/* LSE atomics */							\
	"	swp" #acq_lse #rel #sfx "\t%" #w "3, %" #w "0, %2\n"		\
		"nop\n"							\
		"nop\n"							\
		"nop\n"							\
	"	" #nop_lse)							\
	: "=&r" (ret), "=&r" (tmp), "+Q" (*(u##sz *)ptr)			\
	: "r" (x)								\
	: cl);									\
										\
	return ret;								\
}

__XCHG_CASE(w, b,     ,  8,        ,    ,  ,  ,  ,         )
__XCHG_CASE(w, h,     , 16,        ,    ,  ,  ,  ,         )
__XCHG_CASE(w,  ,     , 32,        ,    ,  ,  ,  ,         )
__XCHG_CASE( ,  ,     , 64,        ,    ,  ,  ,  ,         )
__XCHG_CASE(w, b, acq_,  8,        ,    , a, a,  , "memory")
__XCHG_CASE(w, h, acq_, 16,        ,    , a, a,  , "memory")
__XCHG_CASE(w,  , acq_, 32,        ,    , a, a,  , "memory")
__XCHG_CASE( ,  , acq_, 64,        ,    , a, a,  , "memory")
__XCHG_CASE(w, b, rel_,  8,        ,    ,  ,  , l, "memory")
__XCHG_CASE(w, h, rel_, 16,        ,    ,  ,  , l, "memory")
__XCHG_CASE(w,  , rel_, 32,        ,    ,  ,  , l, "memory")
__XCHG_CASE( ,  , rel_, 64,        ,    ,  ,  , l, "memory")
__XCHG_CASE(w, b,  mb_,  8, dmb ish, nop,  , a, l, "memory")
__XCHG_CASE(w, h,  mb_, 16, dmb ish, nop,  , a, l, "memory")
__XCHG_CASE(w,  ,  mb_, 32, dmb ish, nop,  , a, l, "memory")
__XCHG_CASE( ,  ,  mb_, 64, dmb ish, nop,  , a, l, "memory")

#undef __XCHG_CASE

#define __XCHG_GEN(sfx)							\
static always_inline  unsigned long __xchg##sfx(unsigned long x,	\
					volatile void *ptr,		\
					int size)			\
{									\
	switch (size) {							\
	case 1:								\
		return __xchg_case##sfx##_8(x, ptr);			\
	case 2:								\
		return __xchg_case##sfx##_16(x, ptr);			\
	case 4:								\
		return __xchg_case##sfx##_32(x, ptr);			\
	case 8:								\
		return __xchg_case##sfx##_64(x, ptr);			\
	default:							\
		return __bad_cmpxchg(ptr, size);						\
	}								\
									\
	unreachable();							\
}

__XCHG_GEN()
__XCHG_GEN(_acq)
__XCHG_GEN(_rel)
__XCHG_GEN(_mb)

#undef __XCHG_GEN

#define __xchg_wrapper(sfx, ptr, x)					\
({									\
	__typeof__(*(ptr)) __ret;					\
	__ret = (__typeof__(*(ptr)))					\
		__xchg##sfx((unsigned long)(x), (ptr), sizeof(*(ptr))); \
	__ret;								\
})

/* xchg */
#define xchg_relaxed(...)	__xchg_wrapper(    , __VA_ARGS__)
#define xchg_acquire(...)	__xchg_wrapper(_acq, __VA_ARGS__)
#define xchg_release(...)	__xchg_wrapper(_rel, __VA_ARGS__)
#define xchg(...)		__xchg_wrapper( _mb, __VA_ARGS__)

#define __CMPXCHG_CASE(name, sz)			\
static inline u##sz __cmpxchg_case_##name##sz(volatile void *ptr,	\
					      u##sz old,		\
					      u##sz new)		\
{									\
	return __lse_ll_sc_body(_cmpxchg_case_##name##sz,		\
				ptr, old, new);				\
}

__CMPXCHG_CASE(    ,  8)
__CMPXCHG_CASE(    , 16)
__CMPXCHG_CASE(    , 32)
__CMPXCHG_CASE(    , 64)
__CMPXCHG_CASE(acq_,  8)
__CMPXCHG_CASE(acq_, 16)
__CMPXCHG_CASE(acq_, 32)
__CMPXCHG_CASE(acq_, 64)
__CMPXCHG_CASE(rel_,  8)
__CMPXCHG_CASE(rel_, 16)
__CMPXCHG_CASE(rel_, 32)
__CMPXCHG_CASE(rel_, 64)
__CMPXCHG_CASE(mb_,  8)
__CMPXCHG_CASE(mb_, 16)
__CMPXCHG_CASE(mb_, 32)
__CMPXCHG_CASE(mb_, 64)

#undef __CMPXCHG_CASE

#define __CMPXCHG_DBL(name)						\
static inline long __cmpxchg_double##name(unsigned long old1,		\
					 unsigned long old2,		\
					 unsigned long new1,		\
					 unsigned long new2,		\
					 volatile void *ptr)		\
{									\
	return __lse_ll_sc_body(_cmpxchg_double##name, 			\
				old1, old2, new1, new2, ptr);		\
}

__CMPXCHG_DBL(   )
__CMPXCHG_DBL(_mb)

#undef __CMPXCHG_DBL

#define __CMPXCHG_GEN(sfx)						\
static always_inline unsigned long __cmpxchg##sfx(volatile void *ptr,	\
					   unsigned long old,		\
					   unsigned long new,		\
					   int size)			\
{									\
	switch (size) {							\
	case 1:								\
		return __cmpxchg_case##sfx##_8(ptr, old, new);		\
	case 2:								\
		return __cmpxchg_case##sfx##_16(ptr, old, new);		\
	case 4:								\
		return __cmpxchg_case##sfx##_32(ptr, old, new);		\
	case 8:								\
		return __cmpxchg_case##sfx##_64(ptr, old, new);		\
	default:							\
		return __bad_cmpxchg(ptr, size);						\
	}								\
									\
	unreachable();							\
}

__CMPXCHG_GEN()
__CMPXCHG_GEN(_acq)
__CMPXCHG_GEN(_rel)
__CMPXCHG_GEN(_mb)

#undef __CMPXCHG_GEN

#define __cmpxchg_wrapper(sfx, ptr, o, n)				\
({									\
	__typeof__(*(ptr)) __ret;					\
	__ret = (__typeof__(*(ptr)))					\
		__cmpxchg##sfx((ptr), (unsigned long)(o),		\
				(unsigned long)(n), sizeof(*(ptr)));	\
	__ret;								\
})

/* cmpxchg */
#define cmpxchg_relaxed(...)	__cmpxchg_wrapper(    , __VA_ARGS__)
#define cmpxchg_acquire(...)	__cmpxchg_wrapper(_acq, __VA_ARGS__)
#define cmpxchg_release(...)	__cmpxchg_wrapper(_rel, __VA_ARGS__)
#define cmpxchg(...)		__cmpxchg_wrapper( _mb, __VA_ARGS__)
#define cmpxchg_local		cmpxchg_relaxed

/* cmpxchg64 */
#define cmpxchg64_relaxed		cmpxchg_relaxed
#define cmpxchg64_acquire		cmpxchg_acquire
#define cmpxchg64_release		cmpxchg_release
#define cmpxchg64			cmpxchg
#define cmpxchg64_local		cmpxchg_local

/* cmpxchg_double */
#define system_has_cmpxchg_double()     1

#define __cmpxchg_double_check(ptr1, ptr2)					\
({										\
	if (sizeof(*(ptr1)) != 8)						\
		return __bad_cmpxchg(ptr, size);							\
	BUG_ON((unsigned long *)(ptr2) - (unsigned long *)(ptr1) != 1);	\
})

#define cmpxchg_double(ptr1, ptr2, o1, o2, n1, n2)				\
({										\
	int __ret;								\
	__cmpxchg_double_check(ptr1, ptr2);					\
	__ret = !__cmpxchg_double_mb((unsigned long)(o1), (unsigned long)(o2),	\
				     (unsigned long)(n1), (unsigned long)(n2),	\
				     ptr1);					\
	__ret;									\
})

#define cmpxchg_double_local(ptr1, ptr2, o1, o2, n1, n2)			\
({										\
	int __ret;								\
	__cmpxchg_double_check(ptr1, ptr2);					\
	__ret = !__cmpxchg_double((unsigned long)(o1), (unsigned long)(o2),	\
				  (unsigned long)(n1), (unsigned long)(n2),	\
				  ptr1);					\
	__ret;									\
})

#define __CMPWAIT_CASE(w, sfx, sz)					\
static inline void __cmpwait_case_##sz(volatile void *ptr,		\
				       unsigned long val)		\
{									\
	unsigned long tmp;						\
									\
	asm volatile(							\
	"	sevl\n"							\
	"	wfe\n"							\
	"	ldxr" #sfx "\t%" #w "[tmp], %[v]\n"			\
	"	eor	%" #w "[tmp], %" #w "[tmp], %" #w "[val]\n"	\
	"	cbnz	%" #w "[tmp], 1f\n"				\
	"	wfe\n"							\
	"1:"								\
	: [tmp] "=&r" (tmp), [v] "+Q" (*(unsigned long *)ptr)		\
	: [val] "r" (val));						\
}

__CMPWAIT_CASE(w, b, 8);
__CMPWAIT_CASE(w, h, 16);
__CMPWAIT_CASE(w,  , 32);
__CMPWAIT_CASE( ,  , 64);

#undef __CMPWAIT_CASE

#define __CMPWAIT_GEN(sfx)						\
static always_inline void __cmpwait##sfx(volatile void *ptr,		\
				  unsigned long val,			\
				  int size)				\
{									\
	switch (size) {							\
	case 1:								\
		return __cmpwait_case##sfx##_8(ptr, (u8)val);		\
	case 2:								\
		return __cmpwait_case##sfx##_16(ptr, (u16)val);		\
	case 4:								\
		return __cmpwait_case##sfx##_32(ptr, val);		\
	case 8:								\
		return __cmpwait_case##sfx##_64(ptr, val);		\
	default:							\
		__bad_cmpxchg(ptr, size);						\
	}								\
									\
	unreachable();							\
}

__CMPWAIT_GEN()

#undef __CMPWAIT_GEN

#define __cmpwait_relaxed(ptr, val) \
	__cmpwait((ptr), (unsigned long)(val), sizeof(*(ptr)))

/*
 * This code is from the original Xen arm64 cmpxchg.h, from before the
 * Linux 5.10-rc2 atomics helpers were ported over. The only changes
 * here are renaming the macros and functions to explicitly use
 * "timeout" in their names so that they don't clash with the above.
 *
 * We need this here for guest atomics (the only user of the timeout
 * variants).
 */

#define __CMPXCHG_TIMEOUT_CASE(w, sz, name)                             \
static inline bool __cmpxchg_timeout_case_##name(volatile void *ptr,    \
                                         unsigned long *old,            \
                                         unsigned long new,             \
                                         bool timeout,                  \
                                         unsigned int max_try)          \
{                                                                       \
        unsigned long oldval;                                           \
        unsigned long res;                                              \
                                                                        \
        do {                                                            \
                asm volatile("// __cmpxchg_timeout_case_" #name "\n"    \
                "       ldxr" #sz "     %" #w "1, %2\n"                 \
                "       mov     %w0, #0\n"                              \
                "       cmp     %" #w "1, %" #w "3\n"                   \
                "       b.ne    1f\n"                                   \
                "       stxr" #sz "     %w0, %" #w "4, %2\n"            \
                "1:\n"                                                  \
                : "=&r" (res), "=&r" (oldval),                          \
                  "+Q" (*(unsigned long *)ptr)                          \
                : "Ir" (*old), "r" (new)                                \
                : "cc");                                                \
                                                                        \
                if (!res)                                               \
                        break;                                          \
        } while (!timeout || ((--max_try) > 0));                        \
                                                                        \
        *old = oldval;                                                  \
                                                                        \
        return !res;                                                    \
}

__CMPXCHG_TIMEOUT_CASE(w, b, 1)
__CMPXCHG_TIMEOUT_CASE(w, h, 2)
__CMPXCHG_TIMEOUT_CASE(w,  , 4)
__CMPXCHG_TIMEOUT_CASE( ,  , 8)

static always_inline bool __int_cmpxchg(volatile void *ptr, unsigned long *old,
                                        unsigned long new, int size,
                                        bool timeout, unsigned int max_try)
{
        switch (size) {
        case 1:
                return __cmpxchg_timeout_case_1(ptr, old, new, timeout, max_try);
        case 2:
                return __cmpxchg_timeout_case_2(ptr, old, new, timeout, max_try);
        case 4:
                return __cmpxchg_timeout_case_4(ptr, old, new, timeout, max_try);
        case 8:
                return __cmpxchg_timeout_case_8(ptr, old, new, timeout, max_try);
        default:
                return __bad_cmpxchg(ptr, size);
        }

        ASSERT_UNREACHABLE();
}

/*
 * The helper may fail to update the memory if the action takes too long.
 *
 * @old: On call the value pointed contains the expected old value. It will be
 * updated to the actual old value.
 * @max_try: Maximum number of iterations
 *
 * The helper will return true when the update has succeeded (i.e no
 * timeout) and false if the update has failed.
 */
static always_inline bool __cmpxchg_timeout(volatile void *ptr,
                                            unsigned long *old,
                                            unsigned long new,
                                            int size,
                                            unsigned int max_try)
{
        bool ret;

        smp_mb();
        ret = __int_cmpxchg(ptr, old, new, size, true, max_try);
        smp_mb();

        return ret;
}

#define __cmpxchg64_timeout(ptr, old, new, max_try)     \
        __cmpxchg_timeout(ptr, old, new, 8, max_try)


#endif	/* __ASM_ARM_ARM64_CMPXCHG_H */
