/******************************************************************************
 *
 * Copyright 2019 (C) Alistair Francis <alistair.francis@wdc.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _ASM_RISCV_PROCESSOR_H
#define _ASM_RISCV_PROCESSOR_H

#ifndef __ASSEMBLY__

/* On stack VCPU state */
struct cpu_user_regs
{
	unsigned long r0;
};

void show_execution_state(const struct cpu_user_regs *regs);
void show_registers(const struct cpu_user_regs *regs);

/* All a bit UP for the moment */
#define cpu_to_core(_cpu)   (0)
#define cpu_to_socket(_cpu) (0)

/* Based on Linux: arch/riscv/include/asm/processor.h */

static inline void cpu_relax(void)
{
	int dummy;
	/* In lieu of a halt instruction, induce a long-latency stall. */
	__asm__ __volatile__ ("div %0, %0, zero" : "=r" (dummy));
	barrier();
}

static inline void wait_for_interrupt(void)
{
	__asm__ __volatile__ ("wfi");
}

#endif /* __ASSEMBLY__ */

#endif /* _ASM_RISCV_PROCESSOR_H */
