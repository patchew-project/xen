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

#include <public/version.h>

#include <asm/guest/hypercall.h>
#include <asm/guest/xen.h>

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
