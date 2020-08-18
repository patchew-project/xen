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
        {                                                                  \
            fprintf(stderr,                                                \
                    "error: need another %lu bytes, only %lu available\n", \
                    sizeof(*(_x)), len - off);                             \
            exit(1);                                                       \
        }                                                                  \
        (_x) = buf + off;                                                  \
    } while (false);

static void dump_header(void)
{
    DOMAIN_SAVE_TYPE(HEADER) *h;

    GET_PTR(h);

    printf("    HEADER: magic %#x, version %u\n",
           h->magic, h->version);

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
    DOMAIN_SAVE_TYPE(SHARED_INFO) *s;
    bool has_32bit_shinfo;
    shared_info_any_t *info;
    unsigned int i, n;

    GET_PTR(s);
    has_32bit_shinfo = s->flags & DOMAIN_SAVE_32BIT_SHINFO;

    printf("    SHARED_INFO: has_32bit_shinfo: %s buffer_size: %u\n",
           has_32bit_shinfo ? "true" : "false", s->buffer_size);

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

    printf("                 wc: version: %u sec: %u nsec: %u\n",
           GET_FIELD(wc_version), GET_FIELD(wc_sec), GET_FIELD(wc_nsec));

#undef GET_FIELD
#undef GET_FIELD_SIZE
#undef GET_FIELD_PTR
}

static void dump_end(void)
{
    DOMAIN_SAVE_TYPE(END) *e;

    GET_PTR(e);

    printf("    END\n");
}

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s <domid> [ <typecode> [ <instance> ]]\n",
            prog);
    exit(1);
}

int main(int argc, char **argv)
{
    char *s, *e;
    long domid;
    long typecode = -1;
    long instance = -1;
    unsigned int entry;
    xc_interface *xch;
    int rc;

    if ( argc < 2 || argc > 4 )
        usage(argv[0]);

    s = e = argv[1];
    domid = strtol(s, &e, 0);

    if ( *s == '\0' || *e != '\0' ||
         domid < 0 || domid >= DOMID_FIRST_RESERVED )
    {
        fprintf(stderr, "invalid domid '%s'\n", s);
        exit(1);
    }

    if ( argc >= 3 )
    {
        s = e = argv[2];
        typecode = strtol(s, &e, 0);

        if ( *s == '\0' || *e != '\0' )
        {
            fprintf(stderr, "invalid typecode '%s'\n", s);
            exit(1);
        }
    }

    if ( argc == 4 )
    {
        s = e = argv[3];
        instance = strtol(s, &e, 0);

        if ( *s == '\0' || *e != '\0' )
        {
            fprintf(stderr, "invalid instance '%s'\n", s);
            exit(1);
        }
    }

    xch = xc_interface_open(0, 0, 0);
    if ( !xch )
    {
        fprintf(stderr, "error: can't open libxc handle\n");
        exit(1);
    }

    rc = xc_domain_getcontext(xch, domid, NULL, &len);
    if ( rc < 0 )
    {
        fprintf(stderr, "error: can't get record length for dom %lu: %s\n",
                domid, strerror(errno));
        exit(1);
    }

    buf = malloc(len);
    if ( !buf )
    {
        fprintf(stderr, "error: can't allocate %lu bytes\n", len);
        exit(1);
    }

    rc = xc_domain_getcontext(xch, domid, buf, &len);
    if ( rc < 0 )
    {
        fprintf(stderr, "error: can't get domain record for dom %lu: %s\n",
                domid, strerror(errno));
        exit(1);
    }
    off = 0;

    entry = 0;
    for ( ; ; )
    {
        struct domain_save_descriptor *desc;

        GET_PTR(desc);

        off += sizeof(*desc);

        if ( (typecode < 0 || typecode == desc->typecode) &&
             (instance < 0 || instance == desc->instance) )
        {
            printf("[%u] type: %u instance: %u length: %u\n", entry++,
                   desc->typecode, desc->instance, desc->length);

            switch (desc->typecode)
            {
            case DOMAIN_SAVE_CODE(HEADER): dump_header(); break;
            case DOMAIN_SAVE_CODE(SHARED_INFO): dump_shared_info(); break;
            case DOMAIN_SAVE_CODE(END): dump_end(); break;
            default:
                printf("Unknown type %u: skipping\n", desc->typecode);
                break;
            }
        }

        if ( desc->typecode == DOMAIN_SAVE_CODE(END) )
            break;

        off += desc->length;
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
