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

int domain_save_begin(struct domain_context *c, unsigned int tc,
                      const char *name, const struct vcpu *v, size_t len);

#define DOMAIN_SAVE_BEGIN(_x, _c, _v, _len) \
        domain_save_begin((_c), DOMAIN_SAVE_CODE(_x), #_x, (_v), (_len))

int domain_save_data(struct domain_context *c, const void *data, size_t len);
int domain_save_end(struct domain_context *c);

static inline int domain_save_entry(struct domain_context *c,
                                    unsigned int tc, const char *name,
                                    const struct vcpu *v, const void *src,
                                    size_t len)
{
    int rc;

    rc = domain_save_begin(c, tc, name, v, len);
    if ( rc )
        return rc;

    rc = domain_save_data(c, src, len);
    if ( rc )
        return rc;

    return domain_save_end(c);
}

#define DOMAIN_SAVE_ENTRY(_x, _c, _v, _src, _len) \
    domain_save_entry((_c), DOMAIN_SAVE_CODE(_x), #_x, (_v), (_src), (_len))

int domain_load_begin(struct domain_context *c, unsigned int tc,
                      const char *name, const struct vcpu *v, size_t len,
                      bool exact);

#define DOMAIN_LOAD_BEGIN(_x, _c, _v, _len, _exact) \
        domain_load_begin((_c), DOMAIN_SAVE_CODE(_x), #_x, (_v), (_len), \
                          (_exact));

int domain_load_data(struct domain_context *c, void *data, size_t len);
int domain_load_end(struct domain_context *c);

static inline int domain_load_entry(struct domain_context *c,
                                    unsigned int tc, const char *name,
                                    const struct vcpu *v, void *dst,
                                    size_t len, bool exact)
{
    int rc;

    rc = domain_load_begin(c, tc, name, v, len, exact);
    if ( rc )
        return rc;

    rc = domain_load_data(c, dst, len);
    if ( rc )
        return rc;

    return domain_load_end(c);
}

#define DOMAIN_LOAD_ENTRY(_x, _c, _v, _dst, _len, _exact) \
    domain_load_entry((_c), DOMAIN_SAVE_CODE(_x), #_x, (_v), (_dst), (_len), \
                          (_exact))

/*
 * The 'dry_run' flag indicates that the caller of domain_save() (see
 * below) is not trying to actually acquire the data, only the size
 * of the data. The save handler can therefore limit work to only that
 * which is necessary to call DOMAIN_SAVE_BEGIN/ENTRY() with an accurate
 * value for '_len'.
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

/*
 * Register save and restore handlers. Save handlers will be invoked
 * in order of DOMAIN_SAVE_CODE().
 */
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

/* Copy callback functions */
typedef int (*domain_write_entry)(void *priv, const void *data, size_t len);
typedef int (*domain_read_entry)(void *priv, void *data, size_t len);

/*
 * Entry points:
 *
 * int domain_save(struct domain *d, domain_copy_entry copy, void *priv,
 *                 bool dry_run);
 * int domain_load(struct domain *d, domain_copy_entry copy, void *priv);
 *
 * write/read: This is a callback function provided by the caller that will
 *             be used to write to (in the save case) or read from (in the
 *             load case) the context buffer.
 * priv:       This is a pointer that will be passed to the copy function to
 *             allow it to identify the context buffer and the current state
 *             of the save or load operation.
 */
int domain_save(struct domain *d, domain_write_entry write, void *priv,
                bool dry_run);
int domain_load(struct domain *d, domain_read_entry read, void *priv);

#endif /* __XEN_SAVE_H__ */
