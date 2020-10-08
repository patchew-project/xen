/*
 * xen-domctx.c
 *
 * Print out domain save records in a human-readable way.
 *
 * Copyright Amazon.com Inc. or its affiliates.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include <xenctrl.h>
#include <xen-tools/libs.h>
#include <xen/xen.h>
#include <xen/domctl.h>
#include <xen/save.h>

static void *buf = NULL;
static size_t len, off;

#define GET_PTR(_x)                                                        \
    do {                                                                   \
        if ( len - off < sizeof(*(_x)) )                                   \
            errx(1, "error: need another %lu bytes, only %lu available",   \
                    sizeof(*(_x)), len - off);                             \
        (_x) = buf + off;                                                  \
    } while (false);

static void dump_start(void)
{
    struct domain_context_start *s;

    GET_PTR(s);

    printf("    START: Xen %u.%u\n", s->xen_major, s->xen_minor);
}

static void print_binary(const char *prefix, const void *val, size_t size,
                         const char *suffix)
{
    printf("%s", prefix);

    while ( size-- )
    {
        uint8_t octet = *(const uint8_t *)val++;
        unsigned int i;

        for ( i = 0; i < 8; i++ )
        {
            printf("%u", octet & 1);
            octet >>= 1;
        }
    }

    printf("%s", suffix);
}

static void dump_shared_info(void)
{
    struct domain_context_shared_info *s;
    bool has_32bit_shinfo;
    shared_info_any_t *info;
    unsigned int i, n;

    GET_PTR(s);
    has_32bit_shinfo = s->flags & DOMAIN_CONTEXT_32BIT_SHARED_INFO;

    printf("    SHARED_INFO: has_32bit_shinfo: %s\n",
           has_32bit_shinfo ? "true" : "false");

    info = (shared_info_any_t *)s->buffer;

#define GET_FIELD_PTR(_f)            \
    (has_32bit_shinfo ?              \
     (const void *)&(info->x32._f) : \
     (const void *)&(info->x64._f))
#define GET_FIELD_SIZE(_f) \
    (has_32bit_shinfo ? sizeof(info->x32._f) : sizeof(info->x64._f))
#define GET_FIELD(_f) \
    (has_32bit_shinfo ? info->x32._f : info->x64._f)

    n = has_32bit_shinfo ?
        ARRAY_SIZE(info->x32.evtchn_pending) :
        ARRAY_SIZE(info->x64.evtchn_pending);

    for ( i = 0; i < n; i++ )
    {
        const char *prefix = !i ?
            "                 evtchn_pending: " :
            "                                 ";

        print_binary(prefix, GET_FIELD_PTR(evtchn_pending[0]),
                 GET_FIELD_SIZE(evtchn_pending[0]), "\n");
    }

    for ( i = 0; i < n; i++ )
    {
        const char *prefix = !i ?
            "                    evtchn_mask: " :
            "                                 ";

        print_binary(prefix, GET_FIELD_PTR(evtchn_mask[0]),
                 GET_FIELD_SIZE(evtchn_mask[0]), "\n");
    }

    printf("                 wc: version: %u sec: %u nsec: %u",
           GET_FIELD(wc_version), GET_FIELD(wc_sec), GET_FIELD(wc_nsec));
    if ( !has_32bit_shinfo )
        printf(" sec_hi: %u", info->x64.xen_wc_sec_hi);
    printf("\n");

#undef GET_FIELD
#undef GET_FIELD_SIZE
#undef GET_FIELD_PTR
}

static void dump_tsc_info(void)
{
    struct domain_context_tsc_info *t;

    GET_PTR(t);

    printf("    TSC_INFO: mode: %u incarnation: %u\n"
           "              khz %u nsec: %"PRIu64"\n",
           t->mode, t->incarnation, t->khz, t->nsec);
}

static void dump_end(void)
{
    struct domain_context_end *e;

    GET_PTR(e);

    printf("    END\n");
}

static void usage(void)
{
    errx(1, "usage: <domid> [ <type> [ <instance> ]]");
}

int main(int argc, char **argv)
{
    char *s, *e;
    long domid;
    long type = -1;
    long instance = -1;
    unsigned int entry;
    xc_interface *xch;
    int rc;

    if ( argc < 2 || argc > 4 )
        usage();

    s = e = argv[1];
    domid = strtol(s, &e, 0);

    if ( *s == '\0' || *e != '\0' ||
         domid < 0 || domid >= DOMID_FIRST_RESERVED )
        errx(1, "invalid domid '%s'", s);

    if ( argc >= 3 )
    {
        s = e = argv[2];
        type = strtol(s, &e, 0);

        if ( *s == '\0' || *e != '\0' )
            errx(1, "invalid type '%s'", s);
    }

    if ( argc == 4 )
    {
        s = e = argv[3];
        instance = strtol(s, &e, 0);

        if ( *s == '\0' || *e != '\0' )
            errx(1, "invalid instance '%s'", s);
    }

    xch = xc_interface_open(0, 0, 0);
    if ( !xch )
        err(1, "can't open libxc handle");

    rc = xc_domain_get_context(xch, domid, NULL, &len);
    if ( rc < 0 )
        err(1, "can't get context length for dom %lu", domid);

    buf = malloc(len);
    if ( !buf )
        err(1, "can't allocate %lu bytes", len);

    rc = xc_domain_get_context(xch, domid, buf, &len);
    if ( rc < 0 )
        err(1, "can't get context for dom %lu", domid);

    off = 0;

    entry = 0;
    for ( ;; )
    {
        struct domain_context_record *rec;

        GET_PTR(rec);

        off += sizeof(*rec);

        if ( (type < 0 || type == rec->type) &&
             (instance < 0 || instance == rec->instance) )
        {
            printf("[%u] type: %u instance: %u length: %"PRIu64"\n", entry++,
                   rec->type, rec->instance, rec->length);

            switch (rec->type)
            {
            case DOMAIN_CONTEXT_START: dump_start(); break;
            case DOMAIN_CONTEXT_SHARED_INFO: dump_shared_info(); break;
            case DOMAIN_CONTEXT_TSC_INFO: dump_tsc_info(); break;
            case DOMAIN_CONTEXT_END: dump_end(); break;
            default:
                printf("Unknown type %u: skipping\n", rec->type);
                break;
            }
        }

        if ( rec->type == DOMAIN_CONTEXT_END )
            break;

        off += ROUNDUP(rec->length, _DOMAIN_CONTEXT_RECORD_ALIGN);
    }

    return 0;
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
