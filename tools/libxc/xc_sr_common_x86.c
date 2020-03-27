#include "xc_sr_common_x86.h"

int write_x86_tsc_info(struct xc_sr_context *ctx)
{
    xc_interface *xch = ctx->xch;
    struct xc_sr_rec_x86_tsc_info tsc = {};
    struct xc_sr_record rec = {
        .type = REC_TYPE_X86_TSC_INFO,
        .length = sizeof(tsc),
        .data = &tsc,
    };

    if ( xc_domain_get_tsc_info(xch, ctx->domid, &tsc.mode,
                                &tsc.nsec, &tsc.khz, &tsc.incarnation) < 0 )
    {
        PERROR("Unable to obtain TSC information");
        return -1;
    }

    return write_record(ctx, &rec);
}

int handle_x86_tsc_info(struct xc_sr_context *ctx, struct xc_sr_record *rec)
{
    xc_interface *xch = ctx->xch;
    struct xc_sr_rec_x86_tsc_info *tsc = rec->data;

    if ( rec->length != sizeof(*tsc) )
    {
        ERROR("X86_TSC_INFO record wrong size: length %u, expected %zu",
              rec->length, sizeof(*tsc));
        return -1;
    }

    if ( xc_domain_set_tsc_info(xch, ctx->domid, tsc->mode,
                                tsc->nsec, tsc->khz, tsc->incarnation) )
    {
        PERROR("Unable to set TSC information");
        return -1;
    }

    return 0;
}

int x86_get_context(struct xc_sr_context *ctx, uint32_t mask)
{
    xc_interface *xch = ctx->xch;
    int rc;

    if ( ctx->x86.domain_context.buffer )
    {
        ERROR("Domain context already present");
        return -1;
    }

    rc = xc_domain_getcontext(xch, ctx->domid, mask, NULL, 0);
    if ( rc < 0 )
    {
        PERROR("Unable to get size of domain context");
        return -1;
    }

    ctx->x86.domain_context.buffer = malloc(rc);
    if ( ctx->x86.domain_context.buffer == NULL )
    {
        PERROR("Unable to allocate memory for domain context");
        return -1;
    }

    rc = xc_domain_getcontext(xch, ctx->domid, mask,
                              ctx->x86.domain_context.buffer, rc);
    if ( rc < 0 )
    {
        PERROR("Unable to get domain context");
        return -1;
    }

    ctx->x86.domain_context.len = rc;

    return 0;
}

int x86_set_context(struct xc_sr_context *ctx, uint32_t mask)
{
    xc_interface *xch = ctx->xch;

    if ( !ctx->x86.domain_context.buffer )
    {
        ERROR("Domain context not present");
        return -1;
    }

    return xc_domain_setcontext(xch, ctx->domid, mask,
                                ctx->x86.domain_context.buffer,
                                ctx->x86.domain_context.len);
}

void x86_cleanup(struct xc_sr_context *ctx)
{
    free(ctx->x86.domain_context.buffer);
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
