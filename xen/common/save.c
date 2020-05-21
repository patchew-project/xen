/*
 * save.c: Save and restore PV guest state common to all domain types.
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

#include <xen/compile.h>
#include <xen/save.h>

struct domain_context {
    struct domain *domain;
    const char *name; /* for logging purposes */
    struct domain_save_descriptor desc;
    size_t len; /* for internal accounting */
    union {
        const struct domain_save_ops *save;
        const struct domain_load_ops *load;
    } ops;
    void *priv;
};

static struct {
    const char *name;
    domain_save_handler save;
    domain_load_handler load;
} handlers[DOMAIN_SAVE_CODE_MAX + 1];

void __init domain_register_save_type(unsigned int typecode,
                                      const char *name,
                                      domain_save_handler save,
                                      domain_load_handler load)
{
    BUG_ON(typecode >= ARRAY_SIZE(handlers));

    ASSERT(!handlers[typecode].save);
    ASSERT(!handlers[typecode].load);

    handlers[typecode].name = name;
    handlers[typecode].save = save;
    handlers[typecode].load = load;
}

int domain_save_begin(struct domain_context *c, unsigned int typecode,
                      unsigned int instance)
{
    int rc;

    if ( typecode != c->desc.typecode )
    {
        ASSERT_UNREACHABLE();
        return -EINVAL;
    }
    ASSERT(!c->desc.length); /* Should always be zero during domain_save() */
    ASSERT(!c->len); /* Verify domain_save_end() was called */

    rc = c->ops.save->begin(c->priv, &c->desc);
    if ( rc )
        return rc;

    return 0;
}

int domain_save_data(struct domain_context *c, const void *src, size_t len)
{
    int rc = c->ops.save->append(c->priv, src, len);

    if ( !rc )
        c->len += len;

    return rc;
}

#define DOMAIN_SAVE_ALIGN 8

int domain_save_end(struct domain_context *c)
{
    struct domain *d = c->domain;
    size_t len = ROUNDUP(c->len, DOMAIN_SAVE_ALIGN) - c->len; /* padding */
    int rc;

    if ( len )
    {
        static const uint8_t pad[DOMAIN_SAVE_ALIGN] = {};

        rc = domain_save_data(c, pad, len);

        if ( rc )
            return rc;
    }
    ASSERT(IS_ALIGNED(c->len, DOMAIN_SAVE_ALIGN));

    if ( c->name )
        gdprintk(XENLOG_INFO, "%pd save: %s[%u] +%zu (-%zu)\n", d, c->name,
                 c->desc.instance, c->len, len);

    rc = c->ops.save->end(c->priv, c->len);
    c->len = 0;

    return rc;
}

int domain_save(struct domain *d, const struct domain_save_ops *ops,
                void *priv, bool dry_run)
{
    struct domain_context c = {
        .domain = d,
        .ops.save = ops,
        .priv = priv,
    };
    static const struct domain_save_header h = {
        .magic = DOMAIN_SAVE_MAGIC,
        .xen_major = XEN_VERSION,
        .xen_minor = XEN_SUBVERSION,
        .version = DOMAIN_SAVE_VERSION,
    };
    const struct domain_save_end e = {};
    unsigned int i;
    int rc;

    ASSERT(d != current->domain);
    domain_pause(d);

    c.name = !dry_run ? "HEADER" : NULL;
    c.desc.typecode = DOMAIN_SAVE_CODE(HEADER);

    rc = DOMAIN_SAVE_ENTRY(HEADER, &c, 0, &h, sizeof(h));
    if ( rc )
        goto out;

    for ( i = 0; i < ARRAY_SIZE(handlers); i++ )
    {
        domain_save_handler save = handlers[i].save;

        if ( !save )
            continue;

        c.name = !dry_run ? handlers[i].name : NULL;
        memset(&c.desc, 0, sizeof(c.desc));
        c.desc.typecode = i;

        rc = save(d, &c, dry_run);
        if ( rc )
            goto out;
    }

    c.name = !dry_run ? "END" : NULL;
    memset(&c.desc, 0, sizeof(c.desc));
    c.desc.typecode = DOMAIN_SAVE_CODE(END);

    rc = DOMAIN_SAVE_ENTRY(END, &c, 0, &e, sizeof(e));

 out:
    domain_unpause(d);

    return rc;
}

int domain_load_begin(struct domain_context *c, unsigned int typecode,
                      unsigned int *instance)
{
    if ( typecode != c->desc.typecode )
    {
        ASSERT_UNREACHABLE();
        return -EINVAL;
    }

    ASSERT(!c->len); /* Verify domain_load_end() was called */

    *instance = c->desc.instance;

    return 0;
}

int domain_load_data(struct domain_context *c, void *dst, size_t len)
{
    size_t copy_len = min_t(size_t, len, c->desc.length - c->len);
    int rc;

    c->len += copy_len;
    ASSERT(c->len <= c->desc.length);

    rc = copy_len ? c->ops.load->read(c->priv, dst, copy_len) : 0;
    if ( rc )
        return rc;

    /* Zero extend if the entry is exhausted */
    len -= copy_len;
    if ( len )
    {
        dst += copy_len;
        memset(dst, 0, len);
    }

    return 0;
}

int domain_load_end(struct domain_context *c)
{
    struct domain *d = c->domain;
    size_t len = c->desc.length - c->len;

    while ( c->len != c->desc.length ) /* unconsumed data or pad */
    {
        uint8_t pad;
        int rc = domain_load_data(c, &pad, sizeof(pad));

        if ( rc )
            return rc;

        if ( pad )
            return -EINVAL;
    }

    gdprintk(XENLOG_INFO, "%pd load: %s[%u] +%zu (-%zu)\n", d, c->name,
             c->desc.instance, c->len, len);

    c->len = 0;

    return 0;
}

int domain_load(struct domain *d, const struct domain_load_ops *ops,
                void *priv)
{
    struct domain_context c = {
        .domain = d,
        .ops.load = ops,
        .priv = priv,
    };
    unsigned int instance;
    struct domain_save_header h;
    int rc;

    ASSERT(d != current->domain);

    rc = c.ops.load->read(c.priv, &c.desc, sizeof(c.desc));
    if ( rc )
        return rc;

    c.name = "HEADER";

    rc = DOMAIN_LOAD_ENTRY(HEADER, &c, &instance, &h, sizeof(h));
    if ( rc )
        return rc;

    if ( instance || h.magic != DOMAIN_SAVE_MAGIC ||
         h.version != DOMAIN_SAVE_VERSION )
        return -EINVAL;

    domain_pause(d);

    for (;;)
    {
        unsigned int i;
        domain_load_handler load;

        rc = c.ops.load->read(c.priv, &c.desc, sizeof(c.desc));
        if ( rc )
            return rc;

        rc = -EINVAL;

        if ( c.desc.typecode == DOMAIN_SAVE_CODE(END) )
        {
            struct domain_save_end e;

            c.name = "END";

            rc = DOMAIN_LOAD_ENTRY(END, &c, &instance, &e, sizeof(e));

            if ( instance )
                return -EINVAL;

            break;
        }

        i = c.desc.typecode;
        if ( i >= ARRAY_SIZE(handlers) )
            break;

        c.name = handlers[i].name;
        load = handlers[i].load;

        rc = load ? load(d, &c) : -EOPNOTSUPP;
        if ( rc )
            break;
    }

    domain_unpause(d);

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
