/******************************************************************************
 *
 * hypfs.c
 *
 * Simple sysfs-like file system for the hypervisor.
 */

#include <xen/lib.h>
#include <xen/hypfs.h>
#include <xen/guest_access.h>
#include <xen/hypercall.h>
#include <public/hypfs.h>

static DEFINE_SPINLOCK(hypfs_lock);

struct hypfs_dir hypfs_root = {
    .list = LIST_HEAD_INIT(hypfs_root.list),
};

static struct hypfs_entry hypfs_root_entry = {
    .type = hypfs_type_dir,
    .name = "",
    .list = LIST_HEAD_INIT(hypfs_root_entry.list),
    .parent = &hypfs_root,
    .dir = &hypfs_root,
};

static int hypfs_add_entry(struct hypfs_dir *parent, struct hypfs_entry *new)
{
    int ret = -ENOENT;
    struct list_head *l;

    if ( !new->content )
        return -EINVAL;

    spin_lock(&hypfs_lock);

    list_for_each ( l, &parent->list )
    {
        struct hypfs_entry *e = list_entry(l, struct hypfs_entry, list);
        int cmp = strcmp(e->name, new->name);

        if ( cmp > 0 )
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

        parent->content_size += sizeof(struct xen_hypfs_direntry) +
                                ROUNDUP(sz, 4);
        new->parent = parent;
    }

    spin_unlock(&hypfs_lock);

    return ret;
}

int hypfs_new_entry_any(struct hypfs_dir *parent, const char *name,
                        enum hypfs_entry_type type, void *content)
{
    int ret;
    struct hypfs_entry *new;

    if ( strchr(name, '/') || !strcmp(name, ".") || !strcmp(name, "..") )
        return -EINVAL;

    new = xzalloc(struct hypfs_entry);
    if ( !new )
        return -ENOMEM;

    new->name = name;
    new->type = type;
    new->content = content;

    ret = hypfs_add_entry(parent, new);

    if ( ret )
        xfree(new);

    return ret;
}

int hypfs_new_entry_string(struct hypfs_dir *parent, const char *name,
                           char *val)
{
    return hypfs_new_entry_any(parent, name, hypfs_type_string, val);
}

int hypfs_new_entry_uint(struct hypfs_dir *parent, const char *name,
                         unsigned int *val)
{
    return hypfs_new_entry_any(parent, name, hypfs_type_uint, val);
}

int hypfs_new_dir(struct hypfs_dir *parent, const char *name,
                  struct hypfs_dir *dir)
{
    if ( !dir )
        dir = xzalloc(struct hypfs_dir);

    return hypfs_new_entry_any(parent, name, hypfs_type_dir, dir);
}

static int hypfs_get_path_user(char *buf, XEN_GUEST_HANDLE_PARAM(void) uaddr,
                               unsigned long len)
{
    if ( len > XEN_HYPFS_MAX_PATHLEN )
        return -EINVAL;

    if ( copy_from_guest(buf, uaddr, len) )
        return -EFAULT;

    buf[len - 1] = 0;

    return 0;
}

static struct hypfs_entry *hypfs_get_entry_rel(struct hypfs_entry *dir,
                                               char *path)
{
    char *slash;
    struct hypfs_entry *entry;
    struct list_head *l;
    unsigned int name_len;

    if ( *path == 0 )
        return dir;

    if ( dir->type != hypfs_type_dir )
        return NULL;

    slash = strchr(path, '/');
    if ( !slash )
        slash = strchr(path, '\0');
    name_len = slash - path;

    list_for_each ( l, &dir->dir->list )
    {
        int cmp;

        entry = list_entry(l, struct hypfs_entry, list);
        cmp = strncmp(path, entry->name, name_len);
        if ( cmp < 0 )
            return NULL;
        if ( cmp > 0 )
            continue;
        if ( strlen(entry->name) == name_len )
            return *slash ? hypfs_get_entry_rel(entry, slash + 1) : entry;
    }

    return NULL;
}

struct hypfs_entry *hypfs_get_entry(char *path)
{
    if ( path[0] != '/' )
        return NULL;

    return hypfs_get_entry_rel(&hypfs_root_entry, path + 1);
}

static unsigned int hypfs_get_entry_len(struct hypfs_entry *entry)
{
    unsigned int len = 0;

    switch ( entry->type )
    {
    case hypfs_type_dir:
        len = entry->dir->content_size;
        break;
    case hypfs_type_string:
        len = strlen(entry->str_val) + 1;
        break;
    case hypfs_type_uint:
        len = 11;      /* longest possible printed value + 1 */
        break;
    }

    return len;
}

long do_hypfs_op(unsigned int cmd,
                 XEN_GUEST_HANDLE_PARAM(void) arg1, unsigned long arg2,
                 XEN_GUEST_HANDLE_PARAM(void) arg3, unsigned long arg4)
{
    int ret;
    struct hypfs_entry *entry;
    unsigned int len;
    static char path[XEN_HYPFS_MAX_PATHLEN];

    if ( !is_control_domain(current->domain) &&
         !is_hardware_domain(current->domain) )
        return -EPERM;

    spin_lock(&hypfs_lock);

    ret = hypfs_get_path_user(path, arg1, arg2);
    if ( ret )
        goto out;

    entry = hypfs_get_entry(path);
    if ( !entry )
    {
        ret = -ENOENT;
        goto out;
    }

    switch ( cmd )
    {
    case XEN_HYPFS_OP_read_contents:
    {
        char buf[12];
        char *val = buf;

        len = hypfs_get_entry_len(entry);
        if ( len > arg4 )
        {
            ret = len;
            break;
        }

        switch ( entry->type )
        {
        case hypfs_type_dir:
            ret = -EISDIR;
            break;
        case hypfs_type_string:
            val = entry->str_val;
            break;
        case hypfs_type_uint:
            len = snprintf(buf, sizeof(buf), "%u", *entry->uint_val) + 1;
            break;
        }

        if ( !ret && copy_to_guest(arg3, val, len) )
            ret = -EFAULT;

        break;
    }

    case XEN_HYPFS_OP_read_dir:
    {
        struct list_head *l;

        if ( entry->type != hypfs_type_dir )
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
            struct xen_hypfs_direntry direntry;
            struct hypfs_entry *e = list_entry(l, struct hypfs_entry, list);
            unsigned int e_len = strlen(e->name) + 1;

            e_len = sizeof(direntry) + ROUNDUP(e_len, 4);
            direntry.flags = (e->type == hypfs_type_dir) ? XEN_HYPFS_ISDIR : 0;
            direntry.off_next = list_is_last(l, &entry->dir->list) ? 0 : e_len;
            direntry.content_len = hypfs_get_entry_len(e);
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

    case XEN_HYPFS_OP_write_contents:
        ret = -EACCES;
        break;

    default:
        ret = -ENOSYS;
        break;
    }

 out:
    spin_unlock(&hypfs_lock);

    return ret;
}
