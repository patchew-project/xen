/*
 * save.h
 *
 * Structure definitions for common PV/HVM domain state that is held by Xen.
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

/*
 * C structures for the Domain Context v1 format.
 * See docs/specs/domain-context.md
 */

struct domain_context_record {
    uint32_t type;
    uint32_t instance;
    uint64_t length;
    uint8_t body[XEN_FLEX_ARRAY_DIM];
};

#define _DOMAIN_CONTEXT_RECORD_ALIGN 3
#define DOMAIN_CONTEXT_RECORD_ALIGN (1U << _DOMAIN_CONTEXT_RECORD_ALIGN)

enum {
    DOMAIN_CONTEXT_END,
    DOMAIN_CONTEXT_START,
    /* New types go here */
    DOMAIN_CONTEXT_NR_TYPES
};

/* Initial entry */
struct domain_context_start {
    uint32_t xen_major, xen_minor;
};

/* Terminating entry */
struct domain_context_end {};

#endif /* defined(__XEN__) || defined(__XEN_TOOLS__) */

#endif /* XEN_PUBLIC_SAVE_H */
