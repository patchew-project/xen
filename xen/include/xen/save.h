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

struct domain_context;

int domain_save_begin(struct domain_context *c, unsigned int typecode,
                      unsigned int instance);

#define DOMAIN_SAVE_BEGIN(x, c, i) \
    domain_save_begin((c), DOMAIN_SAVE_CODE(x), (i))

int domain_save_data(struct domain_context *c, const void *data, size_t len);
int domain_save_end(struct domain_context *c);

static inline int domain_save_entry(struct domain_context *c,
                                    unsigned int typecode,
                                    unsigned int instance, const void *src,
                                    size_t len)
{
    int rc;

    rc = domain_save_begin(c, typecode, instance);
    if ( rc )
        return rc;

    rc = domain_save_data(c, src, len);
    if ( rc )
        return rc;

    return domain_save_end(c);
}

#define DOMAIN_SAVE_ENTRY(x, c, i, s, l) \
    domain_save_entry((c), DOMAIN_SAVE_CODE(x), (i), (s), (l))

int domain_load_begin(struct domain_context *c, unsigned int typecode,
                      unsigned int *instance);

#define DOMAIN_LOAD_BEGIN(x, c, i) \
    domain_load_begin((c), DOMAIN_SAVE_CODE(x), (i))

int domain_load_data(struct domain_context *c, void *data, size_t len);
int domain_load_end(struct domain_context *c);

static inline int domain_load_entry(struct domain_context *c,
                                    unsigned int typecode,
                                    unsigned int *instance, void *dst,
                                    size_t len)
{
    int rc;

    rc = domain_load_begin(c, typecode, instance);
    if ( rc )
        return rc;

    rc = domain_load_data(c, dst, len);
    if ( rc )
        return rc;

    return domain_load_end(c);
}

#define DOMAIN_LOAD_ENTRY(x, c, i, d, l) \
    domain_load_entry((c), DOMAIN_SAVE_CODE(x), (i), (d), (l))

/*
 * The 'dry_run' flag indicates that the caller of domain_save() (see below)
 * is not trying to actually acquire the data, only the size of the data.
 * The save handler can therefore limit work to only that which is necessary
 * to call domain_save_data() the correct number of times with accurate values
 * for 'len'.
 */
typedef int (*domain_save_handler)(const struct domain *d,
                                   struct domain_context *c,
                                   bool dry_run);
typedef int (*domain_load_handler)(struct domain *d,
                                   struct domain_context *c);

void domain_register_save_type(unsigned int typecode, const char *name,
                               domain_save_handler save,
                               domain_load_handler load);

/*
 * Register save and load handlers.
 *
 * Save handlers will be invoked in an order which copes with any inter-
 * entry dependencies. For now this means that HEADER will come first and
 * END will come last, all others being invoked in order of 'typecode'.
 *
 * Load handlers will be invoked in the order of entries present in the
 * buffer.
 */
#define DOMAIN_REGISTER_SAVE_LOAD(x, s, l)                    \
    static int __init __domain_register_##x##_save_load(void) \
    {                                                         \
        domain_register_save_type(                            \
            DOMAIN_SAVE_CODE(x),                              \
            #x,                                               \
            &(s),                                             \
            &(l));                                            \
                                                              \
        return 0;                                             \
    }                                                         \
    __initcall(__domain_register_##x##_save_load);

/* Callback functions */
struct domain_save_ops {
    /*
     * Begin a new entry with the given descriptor (only type and instance
     * are valid).
     */
    int (*begin)(void *priv, const struct domain_save_descriptor *desc);
    /* Append data/padding to the buffer */
    int (*append)(void *priv, const void *data, size_t len);
    /*
     * Complete the entry by updating the descriptor with the total
     * length of the appended data (not including padding).
     */
    int (*end)(void *priv, size_t len);
};

struct domain_load_ops {
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
int domain_save(struct domain *d, const struct domain_save_ops *ops,
                void *priv, bool dry_run);
int domain_load(struct domain *d, const struct domain_load_ops *ops,
                void *priv);

#endif /* XEN_SAVE_H */
