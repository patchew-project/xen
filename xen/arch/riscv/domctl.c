/******************************************************************************
 * Arch-specific domctl.c
 *
 * Copyright (c) 2012, Citrix Systems
 */

#include <xen/errno.h>
#include <xen/guest_access.h>
#include <xen/hypercall.h>
#include <xen/iocap.h>
#include <xen/lib.h>
#include <xen/mm.h>
#include <xen/sched.h>
#include <xen/types.h>
#include <xsm/xsm.h>
#include <public/domctl.h>

void arch_get_domain_info(const struct domain *d,
                          struct xen_domctl_getdomaininfo *info)
{
    info->flags |= XEN_DOMINF_hap;
}

long arch_do_domctl(struct xen_domctl *domctl, struct domain *d,
                    XEN_GUEST_HANDLE_PARAM(xen_domctl_t) u_domctl)
{
    /* TODO */

    switch ( domctl->cmd ) {
    case XEN_DOMCTL_cacheflush:
    case XEN_DOMCTL_bind_pt_irq:
    case XEN_DOMCTL_unbind_pt_irq:
    case XEN_DOMCTL_disable_migrate:
    case XEN_DOMCTL_vuart_op:
    default:
        return 0;
    }
}

void arch_get_info_guest(struct vcpu *v, vcpu_guest_context_u c)
{
    /* TODO */
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
