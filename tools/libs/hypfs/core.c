/*
 * Copyright (c) 2019 SUSE Software Solutions Germany GmbH
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; If not, see <http://www.gnu.org/licenses/>.
 */

#define __XEN_TOOLS__ 1

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <xentoollog.h>
#include <xenhypfs.h>
#include <xencall.h>

#include <xentoolcore_internal.h>

struct xenhypfs_handle {
    xentoollog_logger *logger, *logger_tofree;
    unsigned int flags;
    xencall_handle *xcall;
};

xenhypfs_handle *xenhypfs_open(xentoollog_logger *logger,
                               unsigned open_flags)
{
    xenhypfs_handle *fshdl = calloc(1, sizeof(*fshdl));

    if (!fshdl)
        return NULL;

    fshdl->flags = open_flags;
    fshdl->logger = logger;
    fshdl->logger_tofree = NULL;

    if (!fshdl->logger) {
        fshdl->logger = fshdl->logger_tofree =
            (xentoollog_logger*)
            xtl_createlogger_stdiostream(stderr, XTL_PROGRESS, 0);
        if (!fshdl->logger)
            goto err;
    }

    fshdl->xcall = xencall_open(fshdl->logger, 0);
    if (!fshdl->xcall)
        goto err;


    return fshdl;

err:
    xtl_logger_destroy(fshdl->logger_tofree);
    xencall_close(fshdl->xcall);
    free(fshdl);
    return NULL;
}

int xenhypfs_close(xenhypfs_handle *fshdl)
{
    if (!fshdl)
        return 0;

    xencall_close(fshdl->xcall);
    xtl_logger_destroy(fshdl->logger_tofree);
    free(fshdl);
    return 0;
}

static int xenhypfs_get_pathbuf(xenhypfs_handle *fshdl, const char *path,
                                char **path_buf)
{
    int ret = -1;
    int path_sz;

    if (!fshdl) {
        errno = EBADF;
        goto out;
    }

    path_sz = strlen(path) + 1;
    if (path_sz > XEN_HYPFS_MAX_PATHLEN)
    {
        errno = ENAMETOOLONG;
        goto out;
    }

    *path_buf = xencall_alloc_buffer(fshdl->xcall, path_sz);
    if (!*path_buf) {
        errno = ENOMEM;
        goto out;
    }
    strcpy(*path_buf, path);

    ret = path_sz;

 out:
    return ret;
}

static void *xenhypfs_read_any(xenhypfs_handle *fshdl, const char *path,
                               unsigned int cmd)
{
    char *buf = NULL, *path_buf = NULL;
    int ret;
    int sz, path_sz;

    ret = xenhypfs_get_pathbuf(fshdl, path, &path_buf);
    if (ret < 0)
        goto out;

    path_sz = ret;

    for (sz = 4096; sz > 0; sz = ret) {
        if (buf)
            xencall_free_buffer(fshdl->xcall, buf);

        buf = xencall_alloc_buffer(fshdl->xcall, sz);
        if (!buf) {
            errno = ENOMEM;
            goto out;
        }

        ret = xencall5(fshdl->xcall, __HYPERVISOR_hypfs_op, cmd,
                       (unsigned long)path_buf, path_sz,
                       (unsigned long)buf, sz);
    }

    if (ret < 0) {
        errno = -ret;
        xencall_free_buffer(fshdl->xcall, buf);
        buf = NULL;
        goto out;
    }

 out:
    ret = errno;
    xencall_free_buffer(fshdl->xcall, path_buf);
    errno = ret;

    return buf;
}

char *xenhypfs_read(xenhypfs_handle *fshdl, const char *path)
{
    char *buf, *ret_buf = NULL;
    int ret;

    buf = xenhypfs_read_any(fshdl, path, XEN_HYPFS_OP_read_contents);
    if (buf)
        ret_buf = strdup(buf);

    ret = errno;
    xencall_free_buffer(fshdl->xcall, buf);
    errno = ret;

    return ret_buf;
}

struct xenhypfs_dirent *xenhypfs_readdir(xenhypfs_handle *fshdl,
                                         const char *path,
                                         unsigned int *num_entries)
{
    void *buf, *curr;
    int ret;
    char *names;
    struct xenhypfs_dirent *ret_buf = NULL;
    unsigned int n, name_sz = 0;
    struct xen_hypfs_direntry *entry;

    buf = xenhypfs_read_any(fshdl, path, XEN_HYPFS_OP_read_dir);
    if (!buf)
        goto out;

    curr = buf;
    for (n = 1;; n++) {
        entry = curr;
        name_sz += strlen(entry->name) + 1;
        if (!entry->off_next)
            break;

        curr += entry->off_next;
    }

    ret_buf = malloc(n * sizeof(*ret_buf) + name_sz);
    if (!ret_buf)
        goto out;

    *num_entries = n;
    names = (char *)(ret_buf  + n);
    curr = buf;
    for (n = 0; n < *num_entries; n++) {
        entry = curr;
        ret_buf[n].name = names;
        ret_buf[n].is_dir = entry->flags & XEN_HYPFS_ISDIR;
        strcpy(names, entry->name);
        names += strlen(entry->name) + 1;
        curr += entry->off_next;
    }

 out:
    ret = errno;
    xencall_free_buffer(fshdl->xcall, buf);
    errno = ret;

    return ret_buf;
}

int xenhypfs_write(xenhypfs_handle *fshdl, const char *path, const char *val)
{
    char *buf = NULL, *path_buf = NULL;
    int ret, saved_errno;
    int sz, path_sz;

    ret = xenhypfs_get_pathbuf(fshdl, path, &path_buf);
    if (ret < 0)
        goto out;

    path_sz = ret;

    sz = strlen(val) + 1;
    buf = xencall_alloc_buffer(fshdl->xcall, sz);
    if (!buf) {
        ret = -1;
        errno = ENOMEM;
        goto out;
    }
    strcpy(buf, val);

    ret = xencall5(fshdl->xcall, __HYPERVISOR_hypfs_op,
                   XEN_HYPFS_OP_write_contents,
                   (unsigned long)path_buf, path_sz,
                   (unsigned long)buf, sz);

 out:
    saved_errno = errno;
    xencall_free_buffer(fshdl->xcall, path_buf);
    xencall_free_buffer(fshdl->xcall, buf);
    errno = saved_errno;
    return ret;
}
