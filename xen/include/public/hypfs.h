/******************************************************************************
 * Xen Hypervisor Filesystem
 *
 * Copyright (c) 2019, SUSE Software Solutions Germany GmbH
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
 *
 */

#ifndef __XEN_PUBLIC_HYPFS_H__
#define __XEN_PUBLIC_HYPFS_H__

#include "xen.h"

/*
 * Definitions for the __HYPERVISOR_hypfs_op hypercall.
 */

/* Maximum length of a path in the filesystem. */
#define XEN_HYPFS_MAX_PATHLEN 1024

struct xen_hypfs_direntry {
    uint16_t flags;
#define XEN_HYPFS_ISDIR      0x0001
#define XEN_HYPFS_WRITEABLE  0x0002
    /* Offset in bytes to next entry (0 == this is the last entry). */
    uint16_t off_next;
    uint32_t content_len;
    char name[XEN_FLEX_ARRAY_DIM];
};

/*
 * Hypercall operations.
 */

/*
 * XEN_HYPFS_OP_read_contents
 *
 * Read contents of a filesystem entry.
 *
 * Returns the contents of an entry in the buffer supplied by the caller.
 * Only text data with a trailing zero byte is returned.
 *
 * arg1: XEN_GUEST_HANDLE(path name)
 * arg2: length of path name (including trailing zero byte)
 * arg3: XEN_GUEST_HANDLE(content buffer)
 * arg4: content buffer size
 *
 * Possible return values:
 * 0: success
 * -EPERM:   operation not permitted
 * -ENOENT:  entry not found
 * -EACCESS: access to entry not permitted
 * -EISDIR:  entry is a directory
 * -EINVAL:  invalid parameter
 * positive value: content buffer was too small, returned value is needed size
 */
#define XEN_HYPFS_OP_read_contents     1

/*
 * XEN_HYPFS_OP_read_dir
 *
 * Read directory entries of a directory.
 *
 * Returns a struct xen_fs_direntry for each entry in a directory.
 *
 * arg1: XEN_GUEST_HANDLE(path name)
 * arg2: length of path name (including trailing zero byte)
 * arg3: XEN_GUEST_HANDLE(content buffer)
 * arg4: content buffer size
 *
 * Possible return values:
 * 0: success
 * -EPERM:   operation not permitted
 * -ENOENT:  entry not found
 * -EACCESS: access to entry not permitted
 * -ENOTDIR: entry is not a directory
 * -EINVAL:  invalid parameter
 * positive value: content buffer was too small, returned value is needed size
 */
#define XEN_HYPFS_OP_read_dir          2

/*
 * XEN_HYPFS_OP_read_contents
 *
 * Write contents of a filesystem entry.
 *
 * Writes an entry with the contents of a buffer supplied by the caller.
 * Only text data with a trailing zero byte can be written.
 *
 * arg1: XEN_GUEST_HANDLE(path name)
 * arg2: length of path name (including trailing zero byte)
 * arg3: XEN_GUEST_HANDLE(content buffer)
 * arg4: content buffer size
 *
 * Possible return values:
 * 0: success
 * -EPERM:   operation not permitted
 * -ENOENT:  entry not found
 * -EACCESS: access to entry not permitted
 * -EISDIR:  entry is a directory
 * -EINVAL:  invalid parameter
 * -ENOMEM:  memory shortage in the hypervisor
 */
#define XEN_HYPFS_OP_write_contents    3

#endif /* __XEN_PUBLIC_HYPFS_H__ */
