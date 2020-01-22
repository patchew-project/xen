/*
 * Copyright (C) 2012 Regents of the University of California
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

#ifndef _ASM_RISCV_BUG_H
#define _ASM_RISCV_BUG_H

#define BUGFRAME_NR     3

struct bug_frame {
    signed int loc_disp;    /* Relative address to the bug address */
    signed int file_disp;   /* Relative address to the filename */
    signed int msg_disp;    /* Relative address to the predicate (for ASSERT) */
    uint16_t line;          /* Line number */
    uint32_t pad0:16;       /* Padding for 8-bytes align */
};

#ifndef __ASSEMBLY__

#define BUG()							\
do {								\
	__asm__ __volatile__ ("ebreak\n");			\
	unreachable();						\
} while (0)

#define WARN()             \
do {                \
  __asm__ __volatile__ ("ebreak\n");      \
} while (0)

#endif /* !__ASSEMBLY__ */

#ifndef __ASSEMBLY__

struct pt_regs;
struct task_struct;

#endif /* !__ASSEMBLY__ */

#define assert_failed(msg) do {                                \
    BUG();                                                     \
    unreachable();                                             \
} while (0)

extern const struct bug_frame __start_bug_frames[],
                              __stop_bug_frames_0[],
                              __stop_bug_frames_1[],
                              __stop_bug_frames_2[];

#endif /* _ASM_RISCV_BUG_H */
