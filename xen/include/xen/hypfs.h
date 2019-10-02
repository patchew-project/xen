#ifndef __XEN_HYPFS_H__
#define __XEN_HYPFS_H__

#include <xen/list.h>

struct hypfs_dir {
    unsigned int content_size;
    struct list_head list;
};

enum hypfs_entry_type {
    hypfs_type_dir,
    hypfs_type_string,
    hypfs_type_uint
};

struct hypfs_entry {
    enum hypfs_entry_type type;
    const char *name;
    struct list_head list;
    struct hypfs_dir *parent;
    union {
        void *content;
        struct hypfs_dir *dir;
        char *str_val;
        unsigned int *uint_val;
    };
};

extern struct hypfs_dir hypfs_root;

int hypfs_new_dir(struct hypfs_dir *parent, const char *name,
                  struct hypfs_dir *dir);
int hypfs_new_entry_string(struct hypfs_dir *parent, const char *name,
                           char *val);
int hypfs_new_entry_uint(struct hypfs_dir *parent, const char *name,
                         unsigned int *val);
struct hypfs_entry *hypfs_get_entry(char *path);

#endif /* __XEN_HYPFS_H__ */
