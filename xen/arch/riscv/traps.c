/*
 * RISC-V Trap handlers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <xen/domain_page.h>
#include <xen/errno.h>
#include <xen/hypercall.h>
#include <xen/init.h>
#include <xen/iocap.h>
#include <xen/irq.h>
#include <xen/lib.h>
#include <xen/livepatch.h>
#include <xen/mem_access.h>
#include <xen/mm.h>
#include <xen/perfc.h>
#include <xen/smp.h>
#include <xen/softirq.h>
#include <xen/string.h>
#include <xen/symbols.h>
#include <xen/version.h>
#include <xen/virtual_region.h>

#include <public/sched.h>
#include <public/xen.h>

void show_stack(const struct cpu_user_regs *regs)
{
    /* TODO */
}

void show_execution_state(const struct cpu_user_regs *regs)
{
    /* TODO */
}

void vcpu_show_execution_state(struct vcpu *v)
{
    /* TODO */
}

enum mc_disposition arch_do_multicall_call(struct mc_state *state)
{
    /* TODO */

    return mc_continue;
}
