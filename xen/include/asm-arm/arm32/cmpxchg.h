/*
 * Taken from Linux 5.10-rc2 (last commit 3cea11cd5)
 *
 * SPDX-License-Identifier: GPL-2.0
 */
#ifndef __ASM_ARM_ARM32_CMPXCHG_H
#define __ASM_ARM_ARM32_CMPXCHG_H

#include <xen/prefetch.h>
#include <xen/types.h>

extern void __bad_cmpxchg(volatile void *ptr, int size);

static inline unsigned long __xchg(unsigned long x, volatile void *ptr, int size)
{
	unsigned long ret;
	unsigned int tmp;

	prefetchw((const void *)ptr);

	switch (size) {
	case 1:
		asm volatile("@	__xchg1\n"
		"1:	ldrexb	%0, [%3]\n"
		"	strexb	%1, %2, [%3]\n"
		"	teq	%1, #0\n"
		"	bne	1b"
			: "=&r" (ret), "=&r" (tmp)
			: "r" (x), "r" (ptr)
			: "memory", "cc");
		break;
	case 2:
		asm volatile("@	__xchg2\n"
		"1:	ldrexh	%0, [%3]\n"
		"	strexh	%1, %2, [%3]\n"
		"	teq	%1, #0\n"
		"	bne	1b"
			: "=&r" (ret), "=&r" (tmp)
			: "r" (x), "r" (ptr)
			: "memory", "cc");
		break;
	case 4:
		asm volatile("@	__xchg4\n"
		"1:	ldrex	%0, [%3]\n"
		"	strex	%1, %2, [%3]\n"
		"	teq	%1, #0\n"
		"	bne	1b"
			: "=&r" (ret), "=&r" (tmp)
			: "r" (x), "r" (ptr)
			: "memory", "cc");
		break;

	default:
		/* Cause a link-time error, the size is not supported */
		__bad_cmpxchg(ptr, size), ret = 0;
		break;
	}

	return ret;
}

#define xchg_relaxed(ptr, x) ({						\
	(__typeof__(*(ptr)))__xchg((unsigned long)(x), (ptr),		\
				   sizeof(*(ptr)));			\
})

static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old,
				      unsigned long new, int size)
{
	unsigned long oldval, res;

	prefetchw((const void *)ptr);

	switch (size) {
	case 1:
		do {
			asm volatile("@ __cmpxchg1\n"
			"	ldrexb	%1, [%2]\n"
			"	mov	%0, #0\n"
			"	teq	%1, %3\n"
			"	strexbeq %0, %4, [%2]\n"
				: "=&r" (res), "=&r" (oldval)
				: "r" (ptr), "Ir" (old), "r" (new)
				: "memory", "cc");
		} while (res);
		break;
	case 2:
		do {
			asm volatile("@ __cmpxchg1\n"
			"	ldrexh	%1, [%2]\n"
			"	mov	%0, #0\n"
			"	teq	%1, %3\n"
			"	strexheq %0, %4, [%2]\n"
				: "=&r" (res), "=&r" (oldval)
				: "r" (ptr), "Ir" (old), "r" (new)
				: "memory", "cc");
		} while (res);
		break;
	case 4:
		do {
			asm volatile("@ __cmpxchg4\n"
			"	ldrex	%1, [%2]\n"
			"	mov	%0, #0\n"
			"	teq	%1, %3\n"
			"	strexeq %0, %4, [%2]\n"
				: "=&r" (res), "=&r" (oldval)
				: "r" (ptr), "Ir" (old), "r" (new)
				: "memory", "cc");
		} while (res);
		break;

	default:
		__bad_cmpxchg(ptr, size);
		oldval = 0;
	}

	return oldval;
}

#define cmpxchg_relaxed(ptr,o,n) ({					\
	(__typeof__(*(ptr)))__cmpxchg((ptr),				\
				      (unsigned long)(o),		\
				      (unsigned long)(n),		\
				      sizeof(*(ptr)));			\
})

static inline unsigned long long __cmpxchg64(volatile unsigned long long *ptr,
					     unsigned long long old,
					     unsigned long long new)
{
	unsigned long long oldval;
	unsigned long res;

	prefetchw((const void *)ptr);

	__asm__ __volatile__(
"1:	ldrexd		%1, %H1, [%3]\n"
"	teq		%1, %4\n"
"	teqeq		%H1, %H4\n"
"	bne		2f\n"
"	strexd		%0, %5, %H5, [%3]\n"
"	teq		%0, #0\n"
"	bne		1b\n"
"2:"
	: "=&r" (res), "=&r" (oldval), "+Qo" (*ptr)
	: "r" (ptr), "r" (old), "r" (new)
	: "cc");

	return oldval;
}

#define cmpxchg64_relaxed(ptr, o, n) ({					\
	(__typeof__(*(ptr)))__cmpxchg64((ptr),				\
					(unsigned long long)(o),	\
					(unsigned long long)(n));	\
})


/*
 * Linux doesn't provide strict versions of xchg(), cmpxchg(), and cmpxchg64(),
 * so manually define these for Xen as smp_mb() wrappers around the relaxed
 * variants.
 */

#define xchg(ptr, x) ({ \
	long ret; \
	smp_mb(); \
	ret = xchg_relaxed(ptr, x); \
	smp_mb(); \
	ret; \
})

#define cmpxchg(ptr, o, n) ({ \
	long ret; \
	smp_mb(); \
	ret = cmpxchg_relaxed(ptr, o, n); \
	smp_mb(); \
	ret; \
})

#define cmpxchg64(ptr, o, n) ({ \
	long long ret; \
	smp_mb(); \
	ret = cmpxchg64_relaxed(ptr, o, n); \
	smp_mb(); \
	ret; \
})

/*
 * This code is from the original Xen arm32 cmpxchg.h, from before the
 * Linux 5.10-rc2 atomics helpers were ported over. The only changes
 * here are renaming the macros and functions to explicitly use
 * "timeout" in their names so that they don't clash with the above.
 *
 * We need this here for guest atomics (the only user of the timeout
 * variants).
 */

#define __CMPXCHG_TIMEOUT_CASE(sz, name)                                        \
static inline bool __cmpxchg_timeout_case_##name(volatile void *ptr,            \
                                         unsigned long *old,            \
                                         unsigned long new,             \
                                         bool timeout,                  \
                                         unsigned int max_try)          \
{                                                                       \
        unsigned long oldval;                                           \
        unsigned long res;                                              \
                                                                        \
        do {                                                            \
                asm volatile("@ __cmpxchg_timeout_case_" #name "\n"             \
                "       ldrex" #sz "    %1, [%2]\n"                     \
                "       mov     %0, #0\n"                               \
                "       teq     %1, %3\n"                               \
                "       strex" #sz "eq %0, %4, [%2]\n"                  \
                : "=&r" (res), "=&r" (oldval)                           \
                : "r" (ptr), "Ir" (*old), "r" (new)                     \
                : "memory", "cc");                                      \
                                                                        \
                if (!res)                                               \
                        break;                                          \
        } while (!timeout || ((--max_try) > 0));                        \
                                                                        \
        *old = oldval;                                                  \
                                                                        \
        return !res;                                                    \
}

__CMPXCHG_TIMEOUT_CASE(b, 1)
__CMPXCHG_TIMEOUT_CASE(h, 2)
__CMPXCHG_TIMEOUT_CASE( , 4)

static inline bool __cmpxchg_timeout_case_8(volatile uint64_t *ptr,
                                    uint64_t *old,
                                    uint64_t new,
                                    bool timeout,
                                    unsigned int max_try)
{
        uint64_t oldval;
        uint64_t res;

        do {
                asm volatile(
                "       ldrexd          %1, %H1, [%3]\n"
                "       teq             %1, %4\n"
                "       teqeq           %H1, %H4\n"
                "       movne           %0, #0\n"
                "       movne           %H0, #0\n"
                "       bne             2f\n"
                "       strexd          %0, %5, %H5, [%3]\n"
                "2:"
                : "=&r" (res), "=&r" (oldval), "+Qo" (*ptr)
                : "r" (ptr), "r" (*old), "r" (new)
                : "memory", "cc");
                if (!res)
                        break;
        } while (!timeout || ((--max_try) > 0));

        *old = oldval;

        return !res;
}

static always_inline bool __int_cmpxchg(volatile void *ptr, unsigned long *old,
                                        unsigned long new, int size,
                                        bool timeout, unsigned int max_try)
{
        prefetchw((const void *)ptr);

        switch (size) {
        case 1:
                return __cmpxchg_timeout_case_1(ptr, old, new, timeout, max_try);
        case 2:
                return __cmpxchg_timeout_case_2(ptr, old, new, timeout, max_try);
        case 4:
                return __cmpxchg_timeout_case_4(ptr, old, new, timeout, max_try);
        default:
                __bad_cmpxchg(ptr, size);
				return false;
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
static always_inline bool __cmpxchg64_timeout(volatile uint64_t *ptr,
                                              uint64_t *old,
                                              uint64_t new,
                                              unsigned int max_try)
{
        bool ret;

        smp_mb();
        ret = __cmpxchg_timeout_case_8(ptr, old, new, true, max_try);
        smp_mb();

        return ret;
}

#endif /* __ASM_ARM_ARM32_CMPXCHG_H */
