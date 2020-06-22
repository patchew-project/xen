/******************************************************************************
 * xc_vmtrace.c
 *
 * API for manipulating hardware tracing features
 *
 * Copyright (c) 2020, Michal Leszczynski
 *
 * Copyright 2020 CERT Polska. All rights reserved.
 * Use is subject to license terms.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; If not, see <http://www.gnu.org/licenses/>.
 */

#include "xc_private.h"
#include <xen/trace.h>

int xc_vmtrace_pt_enable(
        xc_interface *xch, uint32_t domid, uint32_t vcpu)
{
    DECLARE_HYPERCALL_BUFFER(xen_hvm_vmtrace_op_t, arg);
    int rc = -1;

    arg = xc_hypercall_buffer_alloc(xch, arg, sizeof(*arg));
    if ( arg == NULL )
        return -1;

    arg->cmd = HVMOP_vmtrace_pt_enable;
    arg->domain = domid;
    arg->vcpu = vcpu;

    rc = xencall2(xch->xcall, __HYPERVISOR_hvm_op, HVMOP_vmtrace,
                  HYPERCALL_BUFFER_AS_ARG(arg));

    xc_hypercall_buffer_free(xch, arg);
    return rc;
}

int xc_vmtrace_pt_get_offset(
        xc_interface *xch, uint32_t domid, uint32_t vcpu, uint64_t *offset)
{
    DECLARE_HYPERCALL_BUFFER(xen_hvm_vmtrace_op_t, arg);
    int rc = -1;

    arg = xc_hypercall_buffer_alloc(xch, arg, sizeof(*arg));
    if ( arg == NULL )
        return -1;

    arg->cmd = HVMOP_vmtrace_pt_get_offset;
    arg->domain = domid;
    arg->vcpu = vcpu;

    rc = xencall2(xch->xcall, __HYPERVISOR_hvm_op, HVMOP_vmtrace,
                  HYPERCALL_BUFFER_AS_ARG(arg));

    if ( rc == 0 )
    {
        *offset = arg->offset;
    }

    xc_hypercall_buffer_free(xch, arg);
    return rc;
}

int xc_vmtrace_pt_disable(xc_interface *xch, uint32_t domid, uint32_t vcpu)
{
    DECLARE_HYPERCALL_BUFFER(xen_hvm_vmtrace_op_t, arg);
    int rc = -1;

    arg = xc_hypercall_buffer_alloc(xch, arg, sizeof(*arg));
    if ( arg == NULL )
        return -1;

    arg->cmd = HVMOP_vmtrace_pt_disable;
    arg->domain = domid;
    arg->vcpu = vcpu;

    rc = xencall2(xch->xcall, __HYPERVISOR_hvm_op, HVMOP_vmtrace,
                  HYPERCALL_BUFFER_AS_ARG(arg));

    xc_hypercall_buffer_free(xch, arg);
    return rc;
}

