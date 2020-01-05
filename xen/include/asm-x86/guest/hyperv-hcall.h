/******************************************************************************
 * asm-x86/guest/hyperv-hcall.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms and conditions of the GNU General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (c) 2019 Microsoft.
 */

#ifndef __X86_HYPERV_HCALL_H__
#define __X86_HYPERV_HCALL_H__

#include <xen/types.h>

#include <asm/asm_defns.h>
#include <asm/guest/hyperv-tlfs.h>
#include <asm/page.h>

static inline uint64_t hv_do_hypercall(uint64_t control, paddr_t input, paddr_t output)
{
    uint64_t status;

    asm volatile ("mov %[output], %%r8\n"
                  "call hv_hypercall_page"
                  : "=a" (status), "+c" (control),
                    "+d" (input) ASM_CALL_CONSTRAINT
                  : [output] "rm" (output)
                  : "cc", "memory", "r8", "r9", "r10", "r11");

    return status;
}

static inline uint64_t hv_do_fast_hypercall(uint16_t code,
                                            uint64_t input1, uint64_t input2)
{
    uint64_t status;
    uint64_t control = (uint64_t)code | HV_HYPERCALL_FAST_BIT;

    asm volatile ("mov %[input2], %%r8\n"
                  "call hv_hypercall_page"
                  : "=a" (status), "+c" (control),
                    "+d" (input1) ASM_CALL_CONSTRAINT
                  : [input2] "rm" (input2)
                  : "cc", "r8", "r9", "r10", "r11");

    return status;
}

static inline uint64_t hv_do_rep_hypercall(uint16_t code, uint16_t rep_count,
                                           uint16_t varhead_size,
                                           paddr_t input, paddr_t output)
{
    uint64_t control = code;
    uint64_t status;
    uint16_t rep_comp;

    control |= (uint64_t)varhead_size << HV_HYPERCALL_VARHEAD_OFFSET;
    control |= (uint64_t)rep_count << HV_HYPERCALL_REP_COMP_OFFSET;

    do {
        status = hv_do_hypercall(control, input, output);
        if ( (status & HV_HYPERCALL_RESULT_MASK) != HV_STATUS_SUCCESS )
            break;

        rep_comp = (status & HV_HYPERCALL_REP_COMP_MASK) >>
            HV_HYPERCALL_REP_COMP_OFFSET;

        control &= ~HV_HYPERCALL_REP_START_MASK;
        control |= (uint64_t)rep_comp << HV_HYPERCALL_REP_COMP_OFFSET;

    } while ( rep_comp < rep_count );

    return status;
}

#endif /* __X86_HYPERV_HCALL_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
