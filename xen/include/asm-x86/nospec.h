/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved. */

#ifndef _ASM_X86_NOSPEC_H
#define _ASM_X86_NOSPEC_H

#include <asm/alternative.h>

/**
 * array_index_mask_nospec() - generate a mask that is ~0UL when the
 *      bounds check succeeds and 0 otherwise
 * @index: array element index
 * @size: number of elements in array
 *
 * Returns:
 *     0 - (index < size)
 */
#define array_index_mask_nospec array_index_mask_nospec
static inline unsigned long array_index_mask_nospec(unsigned long index,
                                                    unsigned long size)
{
    unsigned long mask;

    asm volatile ( "cmp %[size], %[index]; sbb %[mask], %[mask];"
                   : [mask] "=r" (mask)
                   : [size] "g" (size), [index] "r" (index) );

    return mask;
}

/* Allow to insert a read memory barrier into conditionals */
static always_inline bool barrier_nospec_true(void)
{
#ifdef CONFIG_SPECULATIVE_HARDEN_BRANCH
    alternative("", "lfence", X86_FEATURE_SC_BRANCH_HARDEN);
#endif
    return true;
}

/* Allow to protect evaluation of conditionals with respect to speculation */
static always_inline bool evaluate_nospec(bool condition)
{
    return condition ? barrier_nospec_true() : !barrier_nospec_true();
}

/* Allow to block speculative execution in generic code */
static always_inline void block_speculation(void)
{
    barrier_nospec_true();
}

#endif /* _ASM_X86_NOSPEC_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
