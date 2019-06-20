/*
 * arch/x86/guest/xen-nested.c
 *
 * Hypercall implementations for nested PV drivers interface.
 *
 * Copyright (c) 2019 Star Lab Corp
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <xen/config.h>
#include <xen/errno.h>
#include <xen/guest_access.h>
#include <xen/hypercall.h>
#include <xen/lib.h>
#include <xen/sched.h>

#include <public/hvm/hvm_op.h>
#include <public/memory.h>
#include <public/version.h>
#include <public/xen.h>

#include <asm/guest/hypercall.h>
#include <asm/guest/xen.h>

#ifdef CONFIG_COMPAT
#include <compat/memory.h>
#endif

extern char hypercall_page[];

/* xen_nested: support for nested PV interface enabled */
static bool __read_mostly xen_nested;

void xen_nested_enable(void)
{
    /* Fill the hypercall page. */
    wrmsrl(cpuid_ebx(hypervisor_cpuid_base() + 2), __pa(hypercall_page));

    xen_nested = true;
}

long do_nested_xen_version(int cmd, XEN_GUEST_HANDLE_PARAM(void) arg)
{
    long ret;

    if ( !xen_nested )
        return -ENOSYS;

    ret = xsm_nested_xen_version(XSM_PRIV, current->domain, cmd);
    if ( ret )
        return ret;

    gprintk(XENLOG_DEBUG, "Nested xen_version: %d.\n", cmd);

    switch ( cmd )
    {
    case XENVER_version:
        return xen_hypercall_xen_version(XENVER_version, 0);

    case XENVER_get_features:
    {
        xen_feature_info_t fi;

        if ( copy_from_guest(&fi, arg, 1) )
            return -EFAULT;

        ret = xen_hypercall_xen_version(XENVER_get_features, &fi);
        if ( ret )
            return ret;

        if ( __copy_to_guest(arg, &fi, 1) )
            return -EFAULT;

        return 0;
    }

    default:
        gprintk(XENLOG_ERR, "Nested xen_version op %d not implemented.\n", cmd);
        return -EOPNOTSUPP;
    }
}

static long nested_add_to_physmap(struct xen_add_to_physmap xatp)
{
    struct domain *d;
    long ret;

    if ( !xen_nested )
        return -ENOSYS;

    if ( (xatp.space != XENMAPSPACE_shared_info) &&
         (xatp.space != XENMAPSPACE_grant_table) )
    {
        gprintk(XENLOG_ERR, "Nested memory op: unknown xatp.space: %u\n",
                xatp.space);
        return -EINVAL;
    }

    if ( xatp.domid != DOMID_SELF )
        return -EPERM;

    ret = xsm_nested_add_to_physmap(XSM_PRIV, current->domain);
    if ( ret )
        return ret;

    gprintk(XENLOG_DEBUG, "Nested XENMEM_add_to_physmap: %d\n", xatp.space);

    d = rcu_lock_current_domain();

    ret = xen_hypercall_memory_op(XENMEM_add_to_physmap, &xatp);

    rcu_unlock_domain(d);

    if ( ret )
        gprintk(XENLOG_ERR, "Nested memory op failed add_to_physmap"
                            " for %d with %ld\n", xatp.space, ret);
    return ret;
}

long do_nested_memory_op(int cmd, XEN_GUEST_HANDLE_PARAM(void) arg)
{
    struct xen_add_to_physmap xatp;

    if ( cmd != XENMEM_add_to_physmap )
    {
        gprintk(XENLOG_ERR, "Nested memory op %u not implemented.\n", cmd);
        return -EOPNOTSUPP;
    }

    if ( copy_from_guest(&xatp, arg, 1) )
        return -EFAULT;

    return nested_add_to_physmap(xatp);
}

#ifdef CONFIG_COMPAT
int compat_nested_memory_op(int cmd, XEN_GUEST_HANDLE_PARAM(void) arg)
{
    struct compat_add_to_physmap cmp;
    struct xen_add_to_physmap *nat = COMPAT_ARG_XLAT_VIRT_BASE;

    if ( cmd != XENMEM_add_to_physmap )
    {
        gprintk(XENLOG_ERR, "Nested memory op %u not implemented.\n", cmd);
        return -EOPNOTSUPP;
    }

    if ( copy_from_guest(&cmp, arg, 1) )
        return -EFAULT;

    XLAT_add_to_physmap(nat, &cmp);

    return nested_add_to_physmap(*nat);
}
#endif

long do_nested_hvm_op(int cmd, XEN_GUEST_HANDLE_PARAM(void) arg)
{
    struct xen_hvm_param a;
    long ret;

    if ( !xen_nested )
        return -ENOSYS;

    ret = xsm_nested_hvm_op(XSM_PRIV, current->domain, cmd);
    if ( ret )
        return ret;

    switch ( cmd )
    {
    case HVMOP_set_param:
    {
        if ( copy_from_guest(&a, arg, 1) )
            return -EFAULT;

        return xen_hypercall_hvm_op(cmd, &a);
    }

    case HVMOP_get_param:
    {
        if ( copy_from_guest(&a, arg, 1) )
            return -EFAULT;

        ret = xen_hypercall_hvm_op(cmd, &a);

        if ( !ret && __copy_to_guest(arg, &a, 1) )
            return -EFAULT;

        return ret;
    }

    default:
        gprintk(XENLOG_ERR, "Nested hvm op %d not implemented.\n", cmd);
        return -EOPNOTSUPP;
    }
}
