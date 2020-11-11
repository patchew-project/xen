/*
 * Taken from Linux 5.10-rc2 (last commit 3cea11cd5)
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef __ASM_GENERIC_RWONCE_H
#define __ASM_GENERIC_RWONCE_H

#ifndef __ASSEMBLY__

#define __READ_ONCE(x)	(*(const volatile typeof(x) *)&(x))

#define __WRITE_ONCE(x, val)						\
do {									\
	*(volatile typeof(x) *)&(x) = (val);				\
} while (0)


#endif /* __ASSEMBLY__ */
#endif	/* __ASM_GENERIC_RWONCE_H */
