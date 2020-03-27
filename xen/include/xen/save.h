/*
 * save.h: support routines for save/restore
 *
 * Copyright Amazon.com Inc. or its affiliates.
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

#ifndef __XEN_SAVE_H__
#define __XEN_SAVE_H__

#include <xen/sched.h>
#include <xen/types.h>
#include <xen/init.h>

#include <public/xen.h>
#include <public/save.h>

struct domain_context;

int domain_save_entry(struct domain_context *c, unsigned int tc,
                      const char *name, const struct vcpu *v, void *src,
                      size_t src_len);

#define DOMAIN_SAVE_ENTRY(_x, _c, _v, _src, _len)                        \
        domain_save_entry((_c), DOMAIN_SAVE_CODE(_x), #_x, (_v), (_src), \
                          (_len));

int domain_load_entry(struct domain_context *c, unsigned int tc,
                      const char *name, const struct vcpu *v, void *dest,
                      size_t dest_len, bool exact);

#define DOMAIN_LOAD_ENTRY(_x, _c, _v, _src, _len, _exact)                \
        domain_load_entry((_c), DOMAIN_SAVE_CODE(_x), #_x, (_v), (_src), \
                          (_len), (_exact));

/*
 * The 'dry_run' flag indicates that the caller of domain_save() (see
 * below) is not trying to actually acquire the data, only the size
 * of the data. The save handler can therefore limit work to only that
 * which is necessary to call DOMAIN_SAVE_ENTRY() with an accurate value
 * for '_len'.
 */
typedef int (*domain_save_handler)(const struct vcpu *v,
                                   struct domain_context *h,
                                   bool dry_run);
typedef int (*domain_load_handler)(struct vcpu *v,
                                   struct domain_context *h);

void domain_register_save_type(unsigned int tc, const char *name,
                               bool per_vcpu,
                               domain_save_handler save,
                               domain_load_handler load);

#define DOMAIN_REGISTER_SAVE_RESTORE(_x, _per_vcpu, _save, _load) \
static int __init __domain_register_##_x##_save_restore(void)     \
{                                                                 \
    domain_register_save_type(                                    \
        DOMAIN_SAVE_CODE(_x),                                     \
        #_x,                                                      \
        (_per_vcpu),                                              \
        &(_save),                                                 \
        &(_load));                                                \
                                                                  \
    return 0;                                                     \
}                                                                 \
__initcall(__domain_register_##_x##_save_restore);

/* Copy callback function */
typedef int (*domain_copy_entry)(void *priv, void *data, size_t len);

/*
 * Entry points:
 *
 * int domain_save(struct domain *d, domain_copy_entry copy, void *priv,
 *                 unsigned long mask, bool dry_run);
 * int domain_load(struct domain *d, domain_copy_entry copy, void *priv,
 *                 unsigned long mask);
 *
 * copy:    This is a callback function provided by the caller that will be
 *          used to write to (in the save case) or read from (in the load
 *          case) the context buffer.
 * priv:    This is a pointer that will be passed to the copy function to
 *          allow it to identify the context buffer and the current state
 *          of the save or load operation.
 * mask:    This is a mask to determine which save record types should be
 *          copied to or from the buffer.
 *          If it is zero then all save record types will be copied.
 *          If it is non-zero then only record types with codes
 *          corresponding to set bits will be copied. I.e. to copy save
 *          record 'type', set the bit in position DOMAIN_SAVE_CODE(type).
 *          DOMAIN_SAVE_CODE(HEADER) and DOMAIN_SAVE_CODE(END) records must
 *          always be present and thus will be copied regardless of whether
 *          the bits in those positions are set or not.
 * dry_run: See the comment concerning (*domain_save) above.
 *
 * NOTE: A convenience macro, DOMAIN_SAVE_MASK(type), is defined to support
 *       setting bits in the mask field.
 */
int domain_save(struct domain *d, domain_copy_entry copy, void *priv,
                unsigned long mask, bool dry_run);
int domain_load(struct domain *d, domain_copy_entry copy, void *priv,
                unsigned long mask);

#endif /* __XEN_SAVE_H__ */
