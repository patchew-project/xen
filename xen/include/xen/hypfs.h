#ifndef __XEN_HYPFS_H__
#define __XEN_HYPFS_H__

#include <xen/list.h>
#include <xen/string.h>
#include <public/hypfs.h>

struct hypfs_entry_leaf;

struct hypfs_entry {
    unsigned short type;
    unsigned short encoding;
    unsigned int size;
    const char *name;
    struct list_head list;
    int (*read)(const struct hypfs_entry *entry,
                XEN_GUEST_HANDLE_PARAM(void) uaddr);
    int (*write)(struct hypfs_entry_leaf *leaf,
                 XEN_GUEST_HANDLE_PARAM(void) uaddr, unsigned long ulen);
};

struct hypfs_entry_leaf {
    struct hypfs_entry e;
    union {
        const void *content;
        void *write_ptr;
    };
};

struct hypfs_entry_dir {
    struct hypfs_entry e;
    struct list_head dirlist;
};

#define HYPFS_DIR_INIT(var, nam)                \
    struct hypfs_entry_dir var = {              \
        .e.type = XEN_HYPFS_TYPE_DIR,           \
        .e.encoding = XEN_HYPFS_ENC_PLAIN,      \
        .e.name = nam,                          \
        .e.size = 0,                            \
        .e.list = LIST_HEAD_INIT(var.e.list),   \
        .e.read = hypfs_read_dir,               \
        .dirlist = LIST_HEAD_INIT(var.dirlist), \
    }

/* Content and size need to be set via hypfs_string_set(). */
#define HYPFS_STRING_INIT(var, nam)             \
    struct hypfs_entry_leaf var = {             \
        .e.type = XEN_HYPFS_TYPE_STRING,        \
        .e.encoding = XEN_HYPFS_ENC_PLAIN,      \
        .e.name = nam,                          \
        .e.read = hypfs_read_leaf,              \
    }

static inline void hypfs_string_set(struct hypfs_entry_leaf *leaf,
                                    const char *str)
{
    leaf->content = str;
    leaf->e.size = strlen(str) + 1;
}

#define HYPFS_UINT_INIT(var, nam, uint)         \
    struct hypfs_entry_leaf var = {             \
        .e.type = XEN_HYPFS_TYPE_UINT,          \
        .e.encoding = XEN_HYPFS_ENC_PLAIN,      \
        .e.name = nam,                          \
        .e.size = sizeof(uint),                 \
        .e.read = hypfs_read_leaf,              \
        .content = &uint,                       \
    }


extern struct hypfs_entry_dir hypfs_root;

struct hypfs_entry *hypfs_get_entry(const char *path);
int hypfs_add_dir(struct hypfs_entry_dir *parent,
                  struct hypfs_entry_dir *dir, bool nofault);
int hypfs_add_leaf(struct hypfs_entry_dir *parent,
                   struct hypfs_entry_leaf *leaf, bool nofault);
int hypfs_read_dir(const struct hypfs_entry *entry,
                   XEN_GUEST_HANDLE_PARAM(void) uaddr);
int hypfs_read_leaf(const struct hypfs_entry *entry,
                    XEN_GUEST_HANDLE_PARAM(void) uaddr);
int hypfs_write_leaf(struct hypfs_entry_leaf *leaf,
                     XEN_GUEST_HANDLE_PARAM(void) uaddr, unsigned long ulen);
int hypfs_write_bool(struct hypfs_entry_leaf *leaf,
                     XEN_GUEST_HANDLE_PARAM(void) uaddr, unsigned long ulen);
int hypfs_write_custom(struct hypfs_entry_leaf *leaf,
                       XEN_GUEST_HANDLE_PARAM(void) uaddr, unsigned long ulen);
void hypfs_write_lock(void);
void hypfs_write_unlock(void);

#endif /* __XEN_HYPFS_H__ */
