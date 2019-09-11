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
#include <xenfs.h>
#include <xencall.h>

#include <xentoolcore_internal.h>

struct xenfs_handle {
    xentoollog_logger *logger, *logger_tofree;
    unsigned int flags;
    xencall_handle *xcall;
};

xenfs_handle *xenfs_open(xentoollog_logger *logger,
                         unsigned open_flags)
{
    xenfs_handle *fshdl = calloc(1, sizeof(*fshdl));

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

int xenfs_close(xenfs_handle *fshdl)
{
    if (!fshdl)
        return 0;

    xencall_close(fshdl->xcall);
    xtl_logger_destroy(fshdl->logger_tofree);
    free(fshdl);
    return 0;
}

static void *xenfs_read_any(xenfs_handle *fshdl, const char *path,
                            unsigned int cmd)
{
    char *buf = NULL, *path_buf = NULL;
    int ret;
    int sz, path_sz;

    if (!fshdl) {
        errno = EBADF;
        goto out;
    }

    path_sz = strlen(path) + 1;
    if (path_sz > XEN_FS_MAX_PATHLEN)
    {
        errno = ENAMETOOLONG;
        goto out;
    }
    path_buf = xencall_alloc_buffer(fshdl->xcall, path_sz);
    if (!path_buf) {
        errno = ENOMEM;
        goto out;
    }
    strcpy(path_buf, path);

    for (sz = 4096; sz > 0; sz = ret) {
        if (buf)
            xencall_free_buffer(fshdl->xcall, buf);

        buf = xencall_alloc_buffer(fshdl->xcall, sz);
        if (!buf) {
            errno = ENOMEM;
            goto out;
        }

        ret = xencall5(fshdl->xcall, __HYPERVISOR_filesystem_op, cmd,
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

char *xenfs_read(xenfs_handle *fshdl, const char *path)
{
    char *buf, *ret_buf = NULL;
    int ret;

    buf = xenfs_read_any(fshdl, path, XEN_FS_OP_read_contents);
    if (buf)
        ret_buf = strdup(buf);

    ret = errno;
    xencall_free_buffer(fshdl->xcall, buf);
    errno = ret;

    return ret_buf;
}

struct xenfs_dirent *xenfs_readdir(xenfs_handle *fshdl, const char *path,
                                   unsigned int *num_entries)
{
    void *buf, *curr;
    int ret;
    char *names;
    struct xenfs_dirent *ret_buf = NULL;
    unsigned int n, name_sz = 0;
    struct xen_fs_direntry *entry;

    buf = xenfs_read_any(fshdl, path, XEN_FS_OP_read_dir);
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
        ret_buf[n].is_dir = entry->flags & XEN_FS_ISDIR;
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
