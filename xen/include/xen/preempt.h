/******************************************************************************
 * preempt.h
 * 
 * Track atomic regions in the hypervisor which disallow sleeping.
 * 
 * Copyright (c) 2010, Keir Fraser <keir@xen.org>
 */

#ifndef __XEN_PREEMPT_H__
#define __XEN_PREEMPT_H__

#include <xen/types.h>
#include <xen/percpu.h>

#ifndef NDEBUG

DECLARE_PER_CPU(unsigned int, __preempt_count);

#define preempt_count() (this_cpu(__preempt_count))

#define preempt_disable() do {                  \
    preempt_count()++;                          \
    barrier();                                  \
} while (0)

#define preempt_enable() do {                   \
    barrier();                                  \
    preempt_count()--;                          \
} while (0)

void ASSERT_NOT_IN_ATOMIC(void);

#else
#define preempt_disable()    barrier();
#define preempt_enable()     barrier();
#define ASSERT_NOT_IN_ATOMIC() ((void)0)
#endif

#endif /* __XEN_PREEMPT_H__ */
