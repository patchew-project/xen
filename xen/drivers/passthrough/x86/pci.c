/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; If not, see <http://www.gnu.org/licenses/>.
 */

#include <xen/sched.h>
#include <xen/pci.h>

static int pci_clean_dpci_irq(struct domain *d,
                              struct hvm_pirq_dpci *pirq_dpci, void *arg)
{
    struct dev_intx_gsi_link *digl, *tmp;

    pirq_guest_unbind(d, dpci_pirq(pirq_dpci));

    if ( pt_irq_need_timer(pirq_dpci->flags) )
        kill_timer(&pirq_dpci->timer);

    list_for_each_entry_safe ( digl, tmp, &pirq_dpci->digl_list, list )
    {
        list_del(&digl->list);
        xfree(digl);
    }

    radix_tree_delete(&d->pirq_tree, dpci_pirq(pirq_dpci)->pirq);

    if ( !pt_pirq_softirq_active(pirq_dpci) )
        return 0;

    domain_get_irq_dpci(d)->pending_pirq_dpci = pirq_dpci;

    return -ERESTART;
}

int arch_pci_clean_pirqs(struct domain *d)
{
    struct hvm_irq_dpci *hvm_irq_dpci = NULL;

    if ( !is_iommu_enabled(d) )
        return 0;

    if ( !is_hvm_domain(d) )
        return 0;

    spin_lock(&d->event_lock);
    hvm_irq_dpci = domain_get_irq_dpci(d);
    if ( hvm_irq_dpci != NULL )
    {
        int ret = 0;

        if ( hvm_irq_dpci->pending_pirq_dpci )
        {
            if ( pt_pirq_softirq_active(hvm_irq_dpci->pending_pirq_dpci) )
                 ret = -ERESTART;
            else
                 hvm_irq_dpci->pending_pirq_dpci = NULL;
        }

        if ( !ret )
            ret = pt_pirq_iterate(d, pci_clean_dpci_irq, NULL);
        if ( ret )
        {
            spin_unlock(&d->event_lock);
            return ret;
        }

        hvm_domain_irq(d)->dpci = NULL;
        free_hvm_irq_dpci(hvm_irq_dpci);
    }
    spin_unlock(&d->event_lock);

    return 0;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
