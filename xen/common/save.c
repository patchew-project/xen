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

struct domain_context {
    bool log;
    struct domain_save_descriptor desc;
    domain_copy_entry copy;
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

int domain_save_entry(struct domain_context *c, unsigned int tc,
                      const char *name, const struct vcpu *v, void *src,
                      size_t src_len)
{
    int rc;

    if ( c->log && tc != DOMAIN_SAVE_CODE(HEADER) &&
         tc != DOMAIN_SAVE_CODE(END) )
        gdprintk(XENLOG_INFO, "%pv save: %s (%lu)\n", v, name, src_len);

    if ( !IS_ALIGNED(src_len, 8) )
        return -EINVAL;

    BUG_ON(tc != c->desc.typecode);
    BUG_ON(v->vcpu_id != c->desc.instance);
    c->desc.length = src_len;

    rc = c->copy(c->priv, &c->desc, sizeof(c->desc));
    if ( rc )
        return rc;

    return c->copy(c->priv, src, src_len);
}

int domain_save(struct domain *d, domain_copy_entry copy, void *priv,
                unsigned long mask, bool dry_run)
{
    struct domain_context c = {
        .copy = copy,
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

        if ( (mask && !test_bit(i, &mask)) || !save )
            continue;

        memset(&c.desc, 0, sizeof(c.desc));
        c.desc.typecode = i;

        if ( handlers[i].per_vcpu )
        {
            struct vcpu *v;

            for_each_vcpu ( d, v )
            {
                c.desc.instance = v->vcpu_id;

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

int domain_load_entry(struct domain_context *c, unsigned int tc,
                      const char *name, const struct vcpu *v, void *dst,
                      size_t dst_len, bool exact)
{
    int rc;

    if ( c->log && tc != DOMAIN_SAVE_CODE(HEADER) &&
         tc != DOMAIN_SAVE_CODE(END) )
        gdprintk(XENLOG_INFO, "%pv load: %s (%lu)\n", v, name, dst_len);

    BUG_ON(tc != c->desc.typecode);
    BUG_ON(v->vcpu_id != c->desc.instance);

    if ( (exact ?
          (dst_len != c->desc.length) : (dst_len < c->desc.length)) ||
         !IS_ALIGNED(c->desc.length, 8) )
        return -EINVAL;

    rc = c->copy(c->priv, dst, c->desc.length);
    if ( rc )
        return rc;

    if ( !exact )
    {
        dst += c->desc.length;
        memset(dst, 0, dst_len - c->desc.length);
    }

    return 0;
}

int domain_load(struct domain *d,  domain_copy_entry copy, void *priv,
                unsigned long mask)
{
    struct domain_context c = {
        .copy = copy,
        .priv = priv,
        .log = true,
    };
    struct domain_save_header h, e;
    int rc;

    ASSERT(d != current->domain);

    if ( d->is_dying )
        return -EINVAL;

    rc = c.copy(c.priv, &c.desc, sizeof(c.desc));
    if ( rc )
        return rc;

    if ( c.desc.typecode != DOMAIN_SAVE_CODE(HEADER) ||
         c.desc.instance != 0 )
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
        domain_load_handler load;
        struct vcpu *v;

        rc = c.copy(c.priv, &c.desc, sizeof(c.desc));
        if ( rc )
            break;

        if ( c.desc.typecode == DOMAIN_SAVE_CODE(END) ) {
            rc = DOMAIN_LOAD_ENTRY(END, &c, d->vcpu[0], &e, 0, true);
            break;
        }

        rc = -EINVAL;
        if ( c.desc.typecode >= ARRAY_SIZE(handlers) ||
             c.desc.instance >= d->max_vcpus )
            break;

        i = c.desc.typecode;
        load = handlers[i].load;
        v = d->vcpu[c.desc.instance];

        if ( mask && !test_bit(i, &mask) )
        {
            /* Sink the data */
            rc = c.copy(c.priv, NULL, c.desc.length);
            if ( rc )
                break;

            continue;
        }

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
