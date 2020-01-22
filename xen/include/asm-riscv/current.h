#ifndef __ASM_CURRENT_H
#define __ASM_CURRENT_H

#include <xen/percpu.h>

#include <asm/processor.h>

#ifndef __ASSEMBLY__

struct vcpu;

/* Which VCPU is "current" on this PCPU. */
DECLARE_PER_CPU(struct vcpu *, curr_vcpu);

#define current            (this_cpu(curr_vcpu))
#define set_current(vcpu)  do { current = (vcpu); } while (0)

/* Per-VCPU state that lives at the top of the stack */
struct cpu_info {
    struct cpu_user_regs guest_cpu_user_regs;
    unsigned long elr;
    uint32_t flags;
};

static inline struct cpu_info *get_cpu_info(void)
{
    register unsigned long sp asm ("sp");
    return (struct cpu_info *)((sp & ~(STACK_SIZE - 1)) + STACK_SIZE - sizeof(struct cpu_info));
}

#define guest_cpu_user_regs() (&get_cpu_info()->guest_cpu_user_regs)

/* TODO */
#define switch_stack_and_jump(stack, fn)                                \
	wait_for_interrupt();
    //asm volatile ("mov sp,%0; b " STR(fn) : : "r" (stack) : "memory" )

#define reset_stack_and_jump(fn) switch_stack_and_jump(get_cpu_info(), fn)

DECLARE_PER_CPU(unsigned int, cpu_id);

#define get_processor_id()    (this_cpu(cpu_id))
#define set_processor_id(id)  do {                      \
    csr_write(sscratch, __per_cpu_offset[id]);      \
    this_cpu(cpu_id) = (id);                            \
} while(0)

#endif /* __ASSEMBLY__ */

#endif /* __ASM_CURRENT_H */
