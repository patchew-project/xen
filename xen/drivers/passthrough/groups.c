/*
 * Copyright (c) 2019 Citrix Systems Inc.
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
#include <xen/radix-tree.h>

struct iommu_group {
    unsigned int id;
};

static struct radix_tree_root iommu_groups;

void __init iommu_groups_init(void)
{
    radix_tree_init(&iommu_groups);
}

static struct iommu_group *alloc_iommu_group(unsigned int id)
{
    struct iommu_group *grp = xzalloc(struct iommu_group);

    if ( !grp )
        return NULL;

    grp->id = id;

    if ( radix_tree_insert(&iommu_groups, id, grp) )
    {
        xfree(grp);
        grp = NULL;
    }

    return grp;
}

static struct iommu_group *get_iommu_group(unsigned int id)
{
    struct iommu_group *grp = radix_tree_lookup(&iommu_groups, id);

    if ( !grp )
        grp = alloc_iommu_group(id);

    return grp;
}

int iommu_group_assign(struct pci_dev *pdev, void *arg)
{
    const struct iommu_ops *ops = iommu_get_ops();
    unsigned int id;
    struct iommu_group *grp;

    if ( !ops->get_device_group_id )
        return 0;

    id = ops->get_device_group_id(pdev->seg, pdev->bus, pdev->devfn);
    if ( id < 0 )
        return -ENODATA;

    grp = get_iommu_group(id);
    if ( !grp )
        return -ENOMEM;

    if ( iommu_verbose )
        printk(XENLOG_INFO "Assign %04x:%02x:%02x.%u -> IOMMU group %x\n",
               pdev->seg, pdev->bus, PCI_SLOT(pdev->devfn),
               PCI_FUNC(pdev->devfn), grp->id);

    pdev->grp = grp;

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
