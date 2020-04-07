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

#ifndef __XEN_PUBLIC_SAVE_H__
#define __XEN_PUBLIC_SAVE_H__

#include "xen.h"

/* Each entry is preceded by a descriptor */
struct domain_save_descriptor {
    uint16_t typecode;
    /*
     * Each entry will contain either to global or per-vcpu domain state.
     * Entries relating to global state should have zero in this field.
     */
    uint16_t vcpu_id;
    uint32_t flags;
    /*
     * When restoring state this flag can be set in a descriptor to cause
     * its content to be ignored.
     *
     * NOTE: It is invalid to set this flag for HEADER or END records (see
     *       below).
     */
#define _DOMAIN_SAVE_FLAG_IGNORE 0
#define DOMAIN_SAVE_FLAG_IGNORE (1u << _DOMAIN_SAVE_FLAG_IGNORE)

    /* Entry length not including this descriptor */
    uint64_t length;
};

/*
 * Each entry has a type associated with it. DECLARE_DOMAIN_SAVE_TYPE
 * binds these things together.
 */
#define DECLARE_DOMAIN_SAVE_TYPE(_x, _code, _type) \
    struct __DOMAIN_SAVE_TYPE_##_x { char c[_code]; _type t; };

#define DOMAIN_SAVE_CODE(_x) \
    (sizeof(((struct __DOMAIN_SAVE_TYPE_##_x *)(0))->c))
#define DOMAIN_SAVE_TYPE(_x) \
    typeof(((struct __DOMAIN_SAVE_TYPE_##_x *)(0))->t)

/* Terminating entry */
struct domain_save_end {};
DECLARE_DOMAIN_SAVE_TYPE(END, 0, struct domain_save_end);

#define DOMAIN_SAVE_MAGIC   0x53415645
#define DOMAIN_SAVE_VERSION 0x00000001

/* Initial entry */
struct domain_save_header {
    uint32_t magic;             /* Must be DOMAIN_SAVE_MAGIC */
    uint32_t version;           /* Save format version */
};
DECLARE_DOMAIN_SAVE_TYPE(HEADER, 1, struct domain_save_header);

#define DOMAIN_SAVE_CODE_MAX 1

#endif /* __XEN_PUBLIC_SAVE_H__ */
