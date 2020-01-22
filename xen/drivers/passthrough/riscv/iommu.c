/*
 * IOMMU framework for RISC-V
 *
 * Based off passthrough/arm/iommu.c
 *
 * Bobby Eshleman <bobbyeshleman@gmail>
 * Copyright (c) 2019
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

#include <xen/iommu.h>

static const struct iommu_ops *iommu_ops;

const struct iommu_ops *iommu_get_ops(void)
{
    return iommu_ops;
}

void __init iommu_set_ops(const struct iommu_ops *ops)
{
    BUG_ON(ops == NULL);

    if ( iommu_ops && iommu_ops != ops )
    {
        printk("WARNING: Cannot set IOMMU ops, already set to a different value\n");
        return;
    }

    iommu_ops = ops;
}

int __init iommu_hardware_setup(void)
{
    /* TODO */
    return 0;
}

void __hwdom_init arch_iommu_check_autotranslated_hwdom(struct domain *d)
{
    /* TODO */
    return;
}

int arch_iommu_domain_init(struct domain *d)
{
    /* TODO */
    return 0;
}

void arch_iommu_domain_destroy(struct domain *d)
{
    /* TODO */
}

int arch_iommu_populate_page_table(struct domain *d)
{
    /* TODO */
    return -ENOSYS;
}

void __hwdom_init arch_iommu_hwdom_init(struct domain *d)
{
    /* TODO */
}
