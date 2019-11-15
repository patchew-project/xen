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

#ifdef CONFIG_GUEST

struct hypervisor_ops {
    /* Name of the hypervisor */
    const char *name;
    /* Main setup routine */
    void (*setup)(void);
    /* AP setup */
    void (*ap_setup)(void);
    /* Resume from suspension */
    void (*resume)(void);
};

bool hypervisor_probe(void);
void hypervisor_setup(void);
void hypervisor_ap_setup(void);
void hypervisor_resume(void);
const char *hypervisor_name(void);

#else

#include <xen/lib.h>
#include <xen/types.h>

static inline bool hypervisor_probe(void) { return false; }
static inline void hypervisor_setup(void) {}
static inline void hypervisor_ap_setup(void) {}
static inline void hypervisor_resume(void) {}
static inline char *hypervisor_name(void) { ASSERT_UNREACHABLE(); return NULL; }

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
