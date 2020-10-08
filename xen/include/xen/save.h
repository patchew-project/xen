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

#ifndef XEN_SAVE_H
#define XEN_SAVE_H

#include <xen/init.h>
#include <xen/sched.h>
#include <xen/types.h>

#include <public/save.h>

struct domain_ctxt_state;

int domain_save_ctxt_rec_begin(struct domain_ctxt_state *c, unsigned int type,
                               unsigned int instance);
int domain_save_ctxt_rec_data(struct domain_ctxt_state *c, const void *data,
                              size_t len);
int domain_save_ctxt_rec_end(struct domain_ctxt_state *c);

static inline int domain_save_ctxt_rec(struct domain_ctxt_state *c,
                                       unsigned int type, unsigned int instance,
                                       const void *src, size_t len)
{
    int rc;

    rc = domain_save_ctxt_rec_begin(c, type, instance);
    if ( rc )
        return rc;

    rc = domain_save_ctxt_rec_data(c, src, len);
    if ( rc )
        return rc;

    return domain_save_ctxt_rec_end(c);
}

int domain_load_ctxt_rec_begin(struct domain_ctxt_state *c, unsigned int type,
                               unsigned int *instance);
int domain_load_ctxt_rec_data(struct domain_ctxt_state *c, void *data,
                              size_t len);
int domain_load_ctxt_rec_end(struct domain_ctxt_state *c, bool ignore_data);

static inline int domain_load_ctxt_rec(struct domain_ctxt_state *c,
                                       unsigned int type,
                                       unsigned int *instance, void *dst,
                                       size_t len)
{
    int rc;

    rc = domain_load_ctxt_rec_begin(c, type, instance);
    if ( rc )
        return rc;

    rc = domain_load_ctxt_rec_data(c, dst, len);
    if ( rc )
        return rc;

    return domain_load_ctxt_rec_end(c, false);
}

/*
 * The 'dry_run' flag indicates that the caller of domain_save_ctxt() (see below)
 * is not trying to actually acquire the data, only the size of the data.
 * The save handler can therefore limit work to only that which is necessary
 * to call domain_save_ctxt_rec_data() the correct number of times with accurate
 * values for 'len'.
 *
 * NOTE: the domain pointer argument to domain_save_ctxt_type is not const as
 * some handlers may need to acquire locks.
 */
typedef int (*domain_save_ctxt_type)(struct domain *d,
                                     struct domain_ctxt_state *c,
                                     bool dry_run);
typedef int (*domain_load_ctxt_type)(struct domain *d,
                                     struct domain_ctxt_state *c);

void domain_register_ctxt_type(unsigned int type, const char *name,
                               domain_save_ctxt_type save,
                               domain_load_ctxt_type load);

/*
 * Register save and load handlers for a record type.
 *
 * Save handlers will be invoked in an order which copes with any inter-
 * entry dependencies. For now this means that HEADER will come first and
 * END will come last, all others being invoked in order of 'typecode'.
 *
 * Load handlers will be invoked in the order of entries present in the
 * buffer.
 */
#define DOMAIN_REGISTER_CTXT_TYPE(x, s, l)                    \
    static int __init __domain_register_##x##_ctxt_type(void) \
    {                                                         \
        domain_register_ctxt_type(                            \
            DOMAIN_CONTEXT_ ## x,                             \
            #x,                                               \
            &(s),                                             \
            &(l));                                            \
                                                              \
        return 0;                                             \
    }                                                         \
    __initcall(__domain_register_##x##_ctxt_type);

/* Callback functions */
struct domain_save_ctxt_ops {
    /*
     * Begin a new entry with the given record (only type and instance are
     * valid).
     */
    int (*begin)(void *priv, const struct domain_context_record *rec);
    /* Append data/padding to the buffer */
    int (*append)(void *priv, const void *data, size_t len);
    /*
     * Complete the entry by updating the record with the total length of the
     * appended data (not including padding).
     */
    int (*end)(void *priv, size_t len);
};

struct domain_load_ctxt_ops {
    /* Read data/padding from the buffer */
    int (*read)(void *priv, void *data, size_t len);
};

/*
 * Entry points:
 *
 * ops:     These are callback functions provided by the caller that will
 *          be used to write to (in the save case) or read from (in the
 *          load case) the context buffer. See above for more detail.
 * priv:    This is a pointer that will be passed to the copy function to
 *          allow it to identify the context buffer and the current state
 *          of the save or load operation.
 * dry_run: If this is set then the caller of domain_save() is only trying
 *          to acquire the total size of the data, not the data itself.
 *          In this case the caller may supply different ops to avoid doing
 *          unnecessary work.
 */
int domain_save_ctxt(struct domain *d, const struct domain_save_ctxt_ops *ops,
                     void *priv, bool dry_run);
int domain_load_ctxt(struct domain *d, const struct domain_load_ctxt_ops *ops,
                     void *priv);

#endif /* XEN_SAVE_H */
