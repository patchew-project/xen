/******************************************************************************
 * asm-x86/guest/hypervisor.h
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

#ifndef __X86_HYPERVISOR_H__
#define __X86_HYPERVISOR_H__

struct hypervisor_ops {
    /* Name of the hypervisor */
    const char *name;
    /* Main setup routine */
    void (*setup)(void);
    /* AP setup */
    int (*ap_setup)(void);
    /* Resume from suspension */
    void (*resume)(void);
    /* How many top pages to be reserved in machine address space? */
    unsigned int (*reserve_top_pages)(void);
};

#ifdef CONFIG_GUEST

const char *hypervisor_probe(void);
void hypervisor_setup(void);
int hypervisor_ap_setup(void);
void hypervisor_resume(void);
unsigned int hypervisor_reserve_top_pages(void);

#else

#include <xen/lib.h>
#include <xen/types.h>

static inline const char *hypervisor_probe(void) { return NULL; }
static inline void hypervisor_setup(void) { ASSERT_UNREACHABLE(); }
static inline int hypervisor_ap_setup(void) { ASSERT_UNREACHABLE(); return 0; }
static inline void hypervisor_resume(void) { ASSERT_UNREACHABLE(); }
static inline unsigned int hypervisor_reserve_top_pages(void) { return 0; }

#endif  /* CONFIG_GUEST */

#endif /* __X86_HYPERVISOR_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
