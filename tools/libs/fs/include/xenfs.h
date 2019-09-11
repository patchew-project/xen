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
#ifndef XENFS_H
#define XENFS_H

#include <stdbool.h>
#include <stdint.h>

#include <xen/xen.h>
#include <xen/filesystem.h>

/* Callers who don't care don't need to #include <xentoollog.h> */
struct xentoollog_logger;

typedef struct xenfs_handle xenfs_handle;

struct xenfs_dirent {
    char *name;
    bool is_dir;
};

xenfs_handle *xenfs_open(struct xentoollog_logger *logger,
                         unsigned int open_flags);
int xenfs_close(xenfs_handle *fshdl);

/* Returned buffer should be freed via free(). */
char *xenfs_read(xenfs_handle *fshdl, const char *path);

/* Returned buffer should be freed via free(). */
struct xenfs_dirent *xenfs_readdir(xenfs_handle *fshdl, const char *path,
                                   unsigned int *num_entries);

#endif /* XENFS_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
