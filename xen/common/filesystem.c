/******************************************************************************
 *
 * filesystem.c
 *
 * Simple sysfs-like file system for the hypervisor.
 */

#include <xen/lib.h>
#include <xen/filesystem.h>
#include <xen/guest_access.h>
#include <xen/hypercall.h>
#include <public/filesystem.h>

static DEFINE_SPINLOCK(fs_lock);

struct fs_dir fs_root = {
    .list = LIST_HEAD_INIT(fs_root.list),
};

static struct fs_entry fs_root_entry = {
    .type = fs_type_dir,
    .name = "",
    .list = LIST_HEAD_INIT(fs_root_entry.list),
    .parent = &fs_root,
    .dir = &fs_root,
};

static int fs_add_entry(struct fs_dir *parent, struct fs_entry *new)
{
    int ret = -ENOENT;
    struct list_head *l;

    if ( !new->content )
        return -EINVAL;

    spin_lock(&fs_lock);

    list_for_each ( l, &parent->list )
    {
        struct fs_entry *e = list_entry(l, struct fs_entry, list);
        int cmp = strcmp(e->name, new->name);

        if ( cmp < 0 )
        {
            ret = 0;
            list_add_tail(&new->list, l);
            break;
        }
        if ( cmp == 0 )
        {
            ret = -EEXIST;
            break;
        }
    }

    if ( ret == -ENOENT )
    {
        ret = 0;
        list_add_tail(&new->list, &parent->list);
    }

    if ( !ret )
    {
        unsigned int sz = strlen(new->name) + 1;

        parent->content_size += sizeof(struct xen_fs_direntry) + ROUNDUP(sz, 4);
        new->parent = parent;
    }

    spin_unlock(&fs_lock);

    return ret;
}

int fs_new_entry_any(struct fs_dir *parent, const char *name,
                     enum fs_entry_type type, void *content)
{
    int ret = -ENOMEM;
    struct fs_entry *new = xzalloc(struct fs_entry);

    if ( !new )
        return ret;

    new->name = name;
    new->type = type;
    new->content = content;

    ret = fs_add_entry(parent, new);

    if ( ret )
        xfree(new);

    return ret;
}

int fs_new_entry(struct fs_dir *parent, const char *name, const char *val)
{
    return fs_new_entry_any(parent, name, fs_type_string, (void *)val);
}

int fs_new_dir(struct fs_dir *parent, const char *name, struct fs_dir *dir)
{
    if ( !dir )
        dir = xzalloc(struct fs_dir);

    return fs_new_entry_any(parent, name, fs_type_dir, dir);
}

static int fs_get_path_user(char *buf, XEN_GUEST_HANDLE_PARAM(void) uaddr,
                            unsigned long len)
{
    if ( len > XEN_FS_MAX_PATHLEN )
        return -EINVAL;

    if ( copy_from_guest(buf, uaddr, len) )
        return -EFAULT;

    buf[len - 1] = 0;

    return 0;
}

static struct fs_entry *fs_get_entry_rel(struct fs_entry *dir, char *path)
{
    char *slash;
    struct fs_entry *entry;
    struct list_head *l;
    unsigned int name_len;

    if ( *path == 0 )
        return dir;

    if ( dir->type != fs_type_dir )
        return NULL;

    slash = strchr(path, '/');
    if ( !slash )
        slash = strchr(path, '\0');
    name_len = slash - path;

    list_for_each ( l, &dir->dir->list )
    {
        int cmp;

        entry = list_entry(l, struct fs_entry, list);
        cmp = strncmp(path, entry->name, name_len);
        if ( cmp < 0 )
            return NULL;
        if ( cmp > 0 )
            continue;
        if ( strlen(entry->name) == name_len )
            return *slash ? fs_get_entry_rel(entry, slash + 1) : entry;
    }

    return NULL;
}

struct fs_entry *fs_get_entry(char *path)
{
    if ( path[0] != '/' )
        return NULL;

    return fs_get_entry_rel(&fs_root_entry, path + 1);
}

long do_filesystem_op(unsigned int cmd,
                      XEN_GUEST_HANDLE_PARAM(void) arg1, unsigned long arg2,
                      XEN_GUEST_HANDLE_PARAM(void) arg3, unsigned long arg4)
{
    int ret;
    struct fs_entry *entry;
    unsigned int len;
    static char path[XEN_FS_MAX_PATHLEN];

    if ( !is_control_domain(current->domain) &&
         !is_hardware_domain(current->domain) )
        return -EPERM;

    spin_lock(&fs_lock);

    ret = fs_get_path_user(path, arg1, arg2);
    if ( ret )
        goto out;

    entry = fs_get_entry(path);
    if ( !entry )
    {
        ret = -ENOENT;
        goto out;
    }

    switch ( cmd )
    {
    case XEN_FS_OP_read_contents:
        if ( entry->type == fs_type_dir )
        {
            ret = -EISDIR;
            break;
        }

        len = strlen(entry->val) + 1;
        if ( len > arg4 )
        {
            ret = len;
            break;
        }

        if ( copy_to_guest(arg3, entry->val, len) )
            ret = -EFAULT;

        break;

    case XEN_FS_OP_read_dir:
    {
        struct list_head *l;

        if ( entry->type != fs_type_dir )
        {
            ret = -ENOTDIR;
            break;
        }

        len = entry->dir->content_size;
        if ( len > arg4 )
        {
            ret = len;
            break;
        }

        list_for_each ( l, &entry->dir->list )
        {
            struct xen_fs_direntry direntry;
            struct fs_entry *e = list_entry(l, struct fs_entry, list);
            unsigned int e_len = strlen(e->name) + 1;

            e_len = sizeof(direntry) + ROUNDUP(e_len, 4);
            direntry.flags = (e->type == fs_type_dir) ? XEN_FS_ISDIR : 0;
            direntry.off_next = list_is_last(l, &entry->dir->list) ? 0 : e_len;
            direntry.content_len = (e->type == fs_type_dir)
                                       ? e->dir->content_size
                                       : strlen(e->val) + 1;
            if ( copy_to_guest(arg3, &direntry, 1) )
            {
                ret = -EFAULT;
                goto out;
            }

            if ( copy_to_guest_offset(arg3, sizeof(direntry), e->name,
                                      strlen(e->name) + 1) )
            {
                ret = -EFAULT;
                goto out;
            }

            guest_handle_add_offset(arg3, e_len);
        }

        break;
    }

    default:
        ret = -ENOSYS;
        break;
    }

 out:
    spin_unlock(&fs_lock);

    return ret;
}
