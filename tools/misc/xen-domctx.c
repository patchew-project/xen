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
#include <xen/xen.h>
#include <xen/domctl.h>
#include <xen/save.h>

static void *buf = NULL;
static size_t len, off;

#define READ(_x) do {                                                   \
    if ( len - off < sizeof (_x) )                                      \
    {                                                                   \
        fprintf(stderr,                                                 \
                "Error: need another %lu bytes, only %lu available\n",  \
                sizeof(_x), len - off);                                 \
        exit(1);                                                        \
    }                                                                   \
    memcpy(&(_x), buf + off, sizeof (_x));                              \
} while (0)

static void dump_header(struct domain_save_descriptor *desc)
{
    DOMAIN_SAVE_TYPE(HEADER) h;
    READ(h);
    printf("    HEADER: magic %#x, version %u\n",
           h.magic, h.version);

    off += desc->length;
}

static void dump_shared_info(struct domain_save_descriptor *desc)
{
    DOMAIN_SAVE_TYPE(SHARED_INFO) s;
    READ(s);
    printf("    SHARED_INFO: field_width %u buffer size: %lu\n",
           s.field_width, desc->length - sizeof(s));

    off += desc->length;
}

static void dump_end(struct domain_save_descriptor *desc)
{
    DOMAIN_SAVE_TYPE(END) e;
    READ(e);
    printf("    END\n");
}

int main(int argc, char **argv)
{
    uint32_t domid;
    unsigned int entry;
    xc_interface *xch;
    int rc;

    if ( argc != 2 || !argv[1] || (rc = atoi(argv[1])) < 0 )
    {
        fprintf(stderr, "usage: %s <domid>\n", argv[0]);
        exit(1);
    }
    domid = rc;

    xch = xc_interface_open(0,0,0);
    if ( !xch )
    {
        fprintf(stderr, "Error: can't open libxc handle\n");
        exit(1);
    }

    rc = xc_domain_getcontext(xch, domid, NULL, &len);
    if ( rc < 0 )
    {
        fprintf(stderr, "Error: can't get record length for dom %u: %s\n",
                domid, strerror(errno));
        exit(1);
    }

    buf = malloc(len);
    if ( !buf )
    {
        fprintf(stderr, "Error: can't allocate %lu bytes\n", len);
        exit(1);
    }

    rc = xc_domain_getcontext(xch, domid, buf, &len);
    if ( rc < 0 )
    {
        fprintf(stderr, "Error: can't get domain record for dom %u: %s\n",
                domid, strerror(errno));
        exit(1);
    }
    off = 0;

    printf("Domain save records for d%u\n", domid);

    entry = 0;
    for (;;) {
        struct domain_save_descriptor desc;

        READ(desc);
        printf("[%u] type %u v%u, length %lu\n", entry++,
               desc.typecode, desc.vcpu_id, (unsigned long)desc.length);
        off += sizeof(desc);

        switch (desc.typecode)
        {
        case DOMAIN_SAVE_CODE(HEADER): dump_header(&desc); break;
        case DOMAIN_SAVE_CODE(SHARED_INFO): dump_shared_info(&desc); break;
        case DOMAIN_SAVE_CODE(END): dump_end(&desc); return 0;
        default:
            printf("Unknown type %u: skipping\n", desc.typecode);
            off += desc.length;
            break;
        }
    }
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
