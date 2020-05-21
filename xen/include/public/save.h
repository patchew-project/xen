/*
 * save.h
 *
 * Structure definitions for common PV/HVM domain state that is held by
 * Xen and must be saved along with the domain's memory.
 *
 * Copyright Amazon.com Inc. or its affiliates.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef XEN_PUBLIC_SAVE_H
#define XEN_PUBLIC_SAVE_H

#if defined(__XEN__) || defined(__XEN_TOOLS__)

#include "xen.h"

/* Entry data is preceded by a descriptor */
struct domain_save_descriptor {
    uint16_t typecode;

    /*
     * Instance number of the entry (since there may be multiple of some
     * types of entries).
     */
    uint16_t instance;

    /* Entry length not including this descriptor */
    uint32_t length;
};

/*
 * Each entry has a type associated with it. DECLARE_DOMAIN_SAVE_TYPE
 * binds these things together, although it is not intended that the
 * resulting type is ever instantiated.
 */
#define DECLARE_DOMAIN_SAVE_TYPE(_x, _code, _type) \
    struct DOMAIN_SAVE_TYPE_##_x { char c[_code]; _type t; };

#define DOMAIN_SAVE_CODE(_x) \
    (sizeof(((struct DOMAIN_SAVE_TYPE_##_x *)0)->c))
#define DOMAIN_SAVE_TYPE(_x) \
    typeof(((struct DOMAIN_SAVE_TYPE_##_x *)0)->t)

/*
 * All entries will be zero-padded to the next 64-bit boundary when saved,
 * so there is no need to include trailing pad fields in structure
 * definitions.
 * When loading, entries will be zero-extended if the load handler reads
 * beyond the length specified in the descriptor.
 */

/* Terminating entry */
struct domain_save_end {};
DECLARE_DOMAIN_SAVE_TYPE(END, 0, struct domain_save_end);

#define DOMAIN_SAVE_MAGIC   0x53415645
#define DOMAIN_SAVE_VERSION 0x00000001

/* Initial entry */
struct domain_save_header {
    uint32_t magic;                /* Must be DOMAIN_SAVE_MAGIC */
    uint16_t xen_major, xen_minor; /* Xen version */
    uint32_t version;              /* Save format version */
};
DECLARE_DOMAIN_SAVE_TYPE(HEADER, 1, struct domain_save_header);

struct domain_shared_info_context {
    uint32_t flags;

#define DOMAIN_SAVE_32BIT_SHINFO 0x00000001

    uint32_t buffer_size;
    uint8_t buffer[XEN_FLEX_ARRAY_DIM]; /* Implementation specific size */
};

DECLARE_DOMAIN_SAVE_TYPE(SHARED_INFO, 2, struct domain_shared_info_context);

#define DOMAIN_SAVE_CODE_MAX 2

#endif /* defined(__XEN__) || defined(__XEN_TOOLS__) */

#endif /* XEN_PUBLIC_SAVE_H */
