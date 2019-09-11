#ifndef __XEN_FILESYSTEM_H__
#define __XEN_FILESYSTEM_H__

#include <xen/list.h>

struct fs_dir {
    unsigned int content_size;
    struct list_head list;
};

enum fs_entry_type {
    fs_type_dir,
    fs_type_string
};

struct fs_entry {
    enum fs_entry_type type;
    const char *name;
    struct list_head list;
    struct fs_dir *parent;
    union {
        void *content;
        struct fs_dir *dir;
        const char *val;
    };
};

extern struct fs_dir fs_root;

int fs_new_dir(struct fs_dir *parent, const char *name, struct fs_dir *dir);
int fs_new_entry(struct fs_dir *parent, const char *name, const char *val);
struct fs_entry *fs_get_entry(char *path);

#endif /* __XEN_FILESYSTEM_H__ */
