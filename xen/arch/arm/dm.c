/*
 * Copyright (c) 2019 Arm ltd.
 *
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

#include <xen/dm.h>
#include <xen/hypercall.h>

#include <asm/vgic.h>

int arch_dm_op(struct xen_dm_op *op, struct domain *d,
               const struct dmop_args *op_args, bool *const_op)
{
    int rc;

    switch ( op->op )
    {
    case XEN_DMOP_set_irq_level:
    {
        const struct xen_dm_op_set_irq_level *data =
            &op->u.set_irq_level;
        unsigned int i;

        /* Only SPIs are supported */
        if ( (data->irq < NR_LOCAL_IRQS) || (data->irq >= vgic_num_irqs(d)) )
        {
            rc = -EINVAL;
            break;
        }

        if ( data->level != 0 && data->level != 1 )
        {
            rc = -EINVAL;
            break;
        }

        /* Check that padding is always 0 */
        for ( i = 0; i < sizeof(data->pad); i++ )
        {
            if ( data->pad[i] )
            {
                rc = -EINVAL;
                break;
            }
        }

        /*
         * Allow to set the logical level of a line for non-allocated
         * interrupts only.
         */
        if ( test_bit(data->irq, d->arch.vgic.allocated_irqs) )
        {
            rc = -EINVAL;
            break;
        }

        vgic_inject_irq(d, NULL, data->irq, data->level);
        rc = 0;
        break;
    }

    default:
        rc = -EOPNOTSUPP;
        break;
    }

    return rc;
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
