#ifndef __XEN_PERCPU_H__
#define __XEN_PERCPU_H__

#include <asm/percpu.h>

/*
 * Separate out the type, so (int[3], foo) works.
 *
 * The _##name concatenation is being used here to prevent 'name' from getting
 * macro expanded, while still allowing a per-architecture symbol name prefix.
 */
#define DEFINE_PER_CPU(type, name) \
    __DEFINE_PER_CPU(__section(".bss.percpu"), type, _ ## name)

#define DEFINE_PER_CPU_PAGE_ALIGNED(type, name) \
    __DEFINE_PER_CPU(__section(".bss.percpu.page_aligned") \
                     __aligned(PAGE_SIZE), type, _ ## name)

#define DEFINE_PER_CPU_READ_MOSTLY(type, name) \
    __DEFINE_PER_CPU(__section(".bss.percpu.read_mostly"), type, _ ## name)

#define get_per_cpu_var(var)  (per_cpu__##var)

/* Linux compatibility. */
#define get_cpu_var(var) this_cpu(var)
#define put_cpu_var(var)

#endif /* __XEN_PERCPU_H__ */
