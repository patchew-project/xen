/*
 * save.c: save and load state common to all domain types.
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

struct domain_ctxt_state {
    struct domain *d;
    struct domain_context_record rec;
    size_t len; /* for internal accounting */
    union {
        const struct domain_save_ctxt_ops *save;
        const struct domain_load_ctxt_ops *load;
    } ops;
    void *priv;
    bool dry_run;
};

static struct {
    const char *name;
    domain_save_ctxt_type save;
    domain_load_ctxt_type load;
} fns[DOMAIN_CONTEXT_NR_TYPES];

void __init domain_register_ctxt_type(unsigned int type, const char *name,
                                      domain_save_ctxt_type save,
                                      domain_load_ctxt_type load)
{
    BUG_ON(type >= ARRAY_SIZE(fns));

    ASSERT(!fns[type].save);
    ASSERT(!fns[type].load);

    fns[type].name = name;
    fns[type].save = save;
    fns[type].load = load;
}

int domain_save_ctxt_rec_begin(struct domain_ctxt_state *c,
                               unsigned int type, unsigned int instance)
{
    struct domain_context_record rec = { .type = type, .instance = instance };
    int rc;

    c->rec = rec;
    c->len = 0;

    rc = c->ops.save->begin(c->priv, &c->rec);
    if ( rc )
        return rc;

    return 0;
}

int domain_save_ctxt_rec_data(struct domain_ctxt_state *c, const void *src,
                              size_t len)
{
    int rc = c->ops.save->append(c->priv, src, len);

    if ( !rc )
        c->len += len;

    return rc;
}

int domain_save_ctxt_rec_end(struct domain_ctxt_state *c)
{
    size_t len = c->len;
    size_t pad = ROUNDUP(len, DOMAIN_CONTEXT_RECORD_ALIGN) - len;
    int rc;

    if ( pad )
    {
        static const uint8_t zeroes[DOMAIN_CONTEXT_RECORD_ALIGN] = {};

        rc = c->ops.save->append(c->priv, zeroes, pad);
        if ( rc )
            return rc;
    }

    if ( !c->dry_run )
        gdprintk(XENLOG_DEBUG, "%pd save: %s[%u] +%zu (+%zu)\n", c->d,
                 fns[c->rec.type].name, c->rec.instance,
                 len, pad);

    rc = c->ops.save->end(c->priv, c->len);

    return rc;
}

int domain_save_ctxt(struct domain *d, const struct domain_save_ctxt_ops *ops,
                     void *priv, bool dry_run)
{
    struct domain_ctxt_state c = {
        .d = d,
        .ops.save = ops,
        .priv = priv,
        .dry_run = dry_run,
    };
    domain_save_ctxt_type save;
    unsigned int type;
    int rc;

    ASSERT(d != current->domain);

    save = fns[DOMAIN_CONTEXT_START].save;
    BUG_ON(!save);

    rc = save(d, &c, dry_run);
    if ( rc )
        return rc;

    domain_pause(d);

    for ( type = DOMAIN_CONTEXT_START + 1; type < ARRAY_SIZE(fns); type++ )
    {
        save = fns[type].save;
        if ( !save )
            continue;

        rc = save(d, &c, dry_run);
        if ( rc )
            break;
    }

    domain_unpause(d);

    if ( rc )
        return rc;

    save = fns[DOMAIN_CONTEXT_END].save;
    BUG_ON(!save);

    return save(d, &c, dry_run);
}

int domain_load_ctxt_rec_begin(struct domain_ctxt_state *c,
                               unsigned int type, unsigned int *instance)
{
    if ( type != c->rec.type )
    {
        ASSERT_UNREACHABLE();
        return -EINVAL;
    }

    *instance = c->rec.instance;
    c->len = 0;

    return 0;
}

int domain_load_ctxt_rec_data(struct domain_ctxt_state *c, void *dst,
                              size_t len)
{
    int rc = 0;

    c->len += len;
    if (c->len > c->rec.length)
        return -ENODATA;

    if ( dst )
        rc = c->ops.load->read(c->priv, dst, len);
    else /* sink data */
    {
        uint8_t ignore;

        while ( !rc && len-- )
            rc = c->ops.load->read(c->priv, &ignore, sizeof(ignore));
    }

    return rc;
}

int domain_load_ctxt_rec_end(struct domain_ctxt_state *c, bool ignore_data)
{
    size_t len = c->len;
    size_t pad = ROUNDUP(len, DOMAIN_CONTEXT_RECORD_ALIGN) - len;

    gdprintk(XENLOG_DEBUG, "%pd load: %s[%u] +%zu (+%zu)\n", c->d,
             fns[c->rec.type].name, c->rec.instance,
             len, pad);

    while ( pad-- )
    {
        uint8_t zero;
        int rc = c->ops.load->read(c->priv, &zero, sizeof(zero));

        if ( rc )
            return rc;

        if ( zero )
            return -EINVAL;
    }

    return 0;
}

int domain_load_ctxt(struct domain *d, const struct domain_load_ctxt_ops *ops,
                     void *priv)
{
    struct domain_ctxt_state c = { .d = d, .ops.load = ops, .priv = priv, };
    domain_load_ctxt_type load;
    int rc;

    ASSERT(d != current->domain);

    rc = c.ops.load->read(c.priv, &c.rec, sizeof(c.rec));
    if ( rc )
        return rc;

    load = fns[DOMAIN_CONTEXT_START].load;
    BUG_ON(!load);

    rc = load(d, &c);
    if ( rc )
        return rc;

    domain_pause(d);

    for (;;)
    {
        unsigned int type;

        rc = c.ops.load->read(c.priv, &c.rec, sizeof(c.rec));
        if ( rc )
            break;

        type = c.rec.type;
        if ( type == DOMAIN_CONTEXT_END )
            break;

        rc = -EINVAL;
        if ( type >= ARRAY_SIZE(fns) )
            break;

        load = fns[type].load;
        if ( !load )
            break;

        rc = load(d, &c);
        if ( rc )
            break;
    }

    domain_unpause(d);

    if ( rc )
        return rc;

    load = fns[DOMAIN_CONTEXT_END].load;
    BUG_ON(!load);

    return load(d, &c);
}

static int save_start(struct domain *d, struct domain_ctxt_state *c,
                      bool dry_run)
{
    static const struct domain_context_start s = {
        .xen_major = XEN_VERSION,
        .xen_minor = XEN_SUBVERSION,
    };

    return domain_save_ctxt_rec(c, DOMAIN_CONTEXT_START, 0, &s, sizeof(s));
}

static int load_start(struct domain *d, struct domain_ctxt_state *c)
{
    static struct domain_context_start s;
    unsigned int i;
    int rc = domain_load_ctxt_rec(c, DOMAIN_CONTEXT_START, &i, &s, sizeof(s));

    if ( rc )
        return rc;

    if ( i )
        return -EINVAL;

    /*
     * Make sure we are not attempting to load an image generated by a newer
     * version of Xen.
     */
    if ( s.xen_major > XEN_VERSION && s.xen_minor > XEN_SUBVERSION )
        return -EOPNOTSUPP;

    return 0;
}

DOMAIN_REGISTER_CTXT_TYPE(START, save_start, load_start);

static int save_end(struct domain *d, struct domain_ctxt_state *c,
                    bool dry_run)
{
    static const struct domain_context_end e = {};

    return domain_save_ctxt_rec(c, DOMAIN_CONTEXT_END, 0, &e, sizeof(e));
}

static int load_end(struct domain *d, struct domain_ctxt_state *c)
{
    unsigned int i;
    int rc = domain_load_ctxt_rec(c, DOMAIN_CONTEXT_END, &i, NULL,
                                  sizeof(struct domain_context_end));

    if ( rc )
        return rc;

    if ( i )
        return -EINVAL;

    return 0;
}

DOMAIN_REGISTER_CTXT_TYPE(END, save_end, load_end);

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
