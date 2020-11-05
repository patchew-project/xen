/*
 * Taken from Linux 5.10-rc2 (last commit 3cea11cd5)
 *
 * Summary of changes:
 * 		- Rename header include guard to reflect Xen directory structure
 * 		- Drop redundant includes and redirect others to Xen equivalents
 * 		- Modify hwcap check to use cpus_have_cap()
 *
 * SPDX-License-Identifier: GPL-2.0
 */
#ifndef __ASM_ARM_ARM64_LSE_H
#define __ASM_ARM_ARM64_LSE_H

#include "atomic_ll_sc.h"

#ifdef CONFIG_ARM64_LSE_ATOMICS

#define __LSE_PREAMBLE	".arch_extension lse\n"

#include <xen/compiler.h>
#include <xen/stringify.h>
#include <xen/types.h>

#include <asm/alternative.h>

#include "atomic_lse.h"

static inline bool system_uses_lse_atomics(void)
{
	return cpus_have_cap(ARM64_HAS_LSE_ATOMICS);
}

#define __lse_ll_sc_body(op, ...)					\
({									\
	system_uses_lse_atomics() ?					\
		__lse_##op(__VA_ARGS__) :				\
		__ll_sc_##op(__VA_ARGS__);				\
})

/* In-line patching at runtime */
#define ARM64_LSE_ATOMIC_INSN(llsc, lse)				\
	ALTERNATIVE(llsc, __LSE_PREAMBLE lse, ARM64_HAS_LSE_ATOMICS)

#else	/* CONFIG_ARM64_LSE_ATOMICS */

static inline bool system_uses_lse_atomics(void) { return false; }

#define __lse_ll_sc_body(op, ...)		__ll_sc_##op(__VA_ARGS__)

#define ARM64_LSE_ATOMIC_INSN(llsc, lse)	llsc

#endif	/* CONFIG_ARM64_LSE_ATOMICS */
#endif	/* __ASM_ARM_ARM64_LSE_H */