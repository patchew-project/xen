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

#include <xen/save.h>

union domain_copy_entry {
    domain_write_entry write;
    domain_read_entry read;
};

struct domain_context {
    bool log;
    struct domain_save_descriptor desc;
    size_t data_len;
    union domain_copy_entry copy;
    void *priv;
};

static struct {
    const char *name;
    bool per_vcpu;
    domain_save_handler save;
    domain_load_handler load;
} handlers[DOMAIN_SAVE_CODE_MAX + 1];

void __init domain_register_save_type(unsigned int tc, const char *name,
                                      bool per_vcpu,
                                      domain_save_handler save,
                                      domain_load_handler load)
{
    BUG_ON(tc > ARRAY_SIZE(handlers));

    ASSERT(!handlers[tc].save);
    ASSERT(!handlers[tc].load);

    handlers[tc].name = name;
    handlers[tc].per_vcpu = per_vcpu;
    handlers[tc].save = save;
    handlers[tc].load = load;
}

int domain_save_begin(struct domain_context *c, unsigned int tc,
                      const char *name, const struct vcpu *v, size_t len)
{
    int rc;

    if ( c->log )
        gdprintk(XENLOG_INFO, "%pv save: %s (%lu)\n", v, name,
                 (unsigned long)len);

    BUG_ON(tc != c->desc.typecode);
    BUG_ON(v->vcpu_id != c->desc.vcpu_id);

    ASSERT(!c->data_len);
    c->data_len = c->desc.length = len;

    rc = c->copy.write(c->priv, &c->desc, sizeof(c->desc));
    if ( rc )
        return rc;

    c->desc.length = 0;

    return 0;
}

int domain_save_data(struct domain_context *c, const void *src, size_t len)
{
    if ( c->desc.length + len > c->data_len )
        return -ENOSPC;

    c->desc.length += len;

    return c->copy.write(c->priv, src, len);
}

int domain_save_end(struct domain_context *c)
{
    /*
     * If desc.length does not match the length specified in
     * domain_save_begin(), there should have been more data.
     */
    if ( c->desc.length != c->data_len )
        return -EIO;

    c->data_len = 0;

    return 0;
}

int domain_save(struct domain *d, domain_write_entry write, void *priv,
                bool dry_run)
{
    struct domain_context c = {
        .copy.write = write,
        .priv = priv,
        .log = !dry_run,
    };
    struct domain_save_header h = {
        .magic = DOMAIN_SAVE_MAGIC,
        .version = DOMAIN_SAVE_VERSION,
    };
    struct domain_save_header e;
    unsigned int i;
    int rc;

    ASSERT(d != current->domain);

    if ( d->is_dying )
        return -EINVAL;

    domain_pause(d);

    c.desc.typecode = DOMAIN_SAVE_CODE(HEADER);

    rc = DOMAIN_SAVE_ENTRY(HEADER, &c, d->vcpu[0], &h, sizeof(h));
    if ( rc )
        goto out;

    for ( i = 0; i < ARRAY_SIZE(handlers); i++ )
    {
        domain_save_handler save = handlers[i].save;

        if ( !save )
            continue;

        memset(&c.desc, 0, sizeof(c.desc));
        c.desc.typecode = i;

        if ( handlers[i].per_vcpu )
        {
            struct vcpu *v;

            for_each_vcpu ( d, v )
            {
                c.desc.vcpu_id = v->vcpu_id;

                rc = save(v, &c, dry_run);
                if ( rc )
                    goto out;
            }
        }
        else
        {
            rc = save(d->vcpu[0], &c, dry_run);
            if ( rc )
                goto out;
        }
    }

    memset(&c.desc, 0, sizeof(c.desc));
    c.desc.typecode = DOMAIN_SAVE_CODE(END);

    rc = DOMAIN_SAVE_ENTRY(END, &c, d->vcpu[0], &e, 0);

 out:
    domain_unpause(d);

    return rc;
}

int domain_load_begin(struct domain_context *c, unsigned int tc,
                      const char *name, const struct vcpu *v, size_t len,
                      bool exact)
{
    if ( c->log )
        gdprintk(XENLOG_INFO, "%pv load: %s (%lu)\n", v, name,
                 (unsigned long)len);

    BUG_ON(tc != c->desc.typecode);
    BUG_ON(v->vcpu_id != c->desc.vcpu_id);

    if ( (exact && (len != c->desc.length)) ||
         (len < c->desc.length) )
        return -EINVAL;

    ASSERT(!c->data_len);
    c->data_len = len;

    return 0;
}

int domain_load_data(struct domain_context *c, void *dst, size_t len)
{
    size_t copy_len = min_t(size_t, len, c->desc.length);
    int rc;

    if ( c->data_len < len )
        return -ENODATA;

    c->data_len -= len;
    c->desc.length -= copy_len;

    rc = c->copy.read(c->priv, dst, copy_len);
    if ( rc )
        return rc;

    /* Zero extend if the descriptor is exhausted */
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
    /* If data_len is non-zero there is unread data */
    if ( c->data_len )
        return -EIO;

    return 0;
}

int domain_load(struct domain *d, domain_read_entry read, void *priv)
{
    struct domain_context c = {
        .copy.read = read,
        .priv = priv,
        .log = true,
    };
    struct domain_save_header h;
    int rc;

    ASSERT(d != current->domain);

    if ( d->is_dying )
        return -EINVAL;

    rc = c.copy.read(c.priv, &c.desc, sizeof(c.desc));
    if ( rc )
        return rc;

    if ( c.desc.typecode != DOMAIN_SAVE_CODE(HEADER) || c.desc.vcpu_id ||
         c.desc.flags )
        return -EINVAL;

    rc = DOMAIN_LOAD_ENTRY(HEADER, &c, d->vcpu[0], &h, sizeof(h), true);
    if ( rc )
        return rc;

    if ( h.magic != DOMAIN_SAVE_MAGIC || h.version != DOMAIN_SAVE_VERSION )
        return -EINVAL;

    domain_pause(d);

    for (;;)
    {
        unsigned int i;
        unsigned int flags;
        domain_load_handler load;
        struct vcpu *v;

        rc = c.copy.read(c.priv, &c.desc, sizeof(c.desc));
        if ( rc )
            break;

        rc = -EINVAL;

        flags = c.desc.flags;
        if ( flags & ~DOMAIN_SAVE_FLAG_IGNORE )
            break;

        if ( c.desc.typecode == DOMAIN_SAVE_CODE(END) ) {
            if ( !(flags & DOMAIN_SAVE_FLAG_IGNORE) )
                rc = DOMAIN_LOAD_ENTRY(END, &c, d->vcpu[0], NULL, 0, true);

            break;
        }

        i = c.desc.typecode;
        if ( i >= ARRAY_SIZE(handlers) )
            break;

        if ( (!handlers[i].per_vcpu && c.desc.vcpu_id) ||
             (c.desc.vcpu_id >= d->max_vcpus) )
            break;

        v = d->vcpu[c.desc.vcpu_id];

        if ( flags & DOMAIN_SAVE_FLAG_IGNORE )
        {
            /* Sink the data */
            rc = domain_load_entry(&c, c.desc.typecode, "IGNORED",
                                   v, NULL, c.desc.length, true);
            if ( rc )
                break;

            continue;
        }

        load = handlers[i].load;

        rc = load ? load(v, &c) : -EOPNOTSUPP;
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
