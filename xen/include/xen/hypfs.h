#ifndef __XEN_HYPFS_H__
#define __XEN_HYPFS_H__

#ifdef CONFIG_HYPFS
#include <xen/lib.h>
#include <xen/list.h>
#include <xen/string.h>
#include <public/hypfs.h>

struct hypfs_entry_leaf;
struct hypfs_entry_dir;
struct hypfs_entry;

struct hypfs_funcs {
    int (*read)(const struct hypfs_entry *entry,
                XEN_GUEST_HANDLE_PARAM(void) uaddr);
    int (*write)(struct hypfs_entry_leaf *leaf,
                 XEN_GUEST_HANDLE_PARAM(void) uaddr, unsigned int ulen);
    unsigned int (*getsize)(const struct hypfs_entry *entry);
    struct hypfs_entry *(*findentry)(struct hypfs_entry_dir *dir,
                                     const char *name, unsigned int name_len);
};

extern struct hypfs_funcs hypfs_dir_funcs;
extern struct hypfs_funcs hypfs_leaf_ro_funcs;
extern struct hypfs_funcs hypfs_leaf_wr_funcs;
extern struct hypfs_funcs hypfs_bool_wr_funcs;
extern struct hypfs_funcs hypfs_custom_wr_funcs;

struct hypfs_entry {
    unsigned short type;
    unsigned short encoding;
    unsigned int size;
    unsigned int max_size;
    const char *name;
    struct list_head list;
    struct hypfs_funcs *funcs;
};

struct hypfs_entry_leaf {
    struct hypfs_entry e;
    union {
        const void *content;
        void *write_ptr;
    } u;
};

struct hypfs_entry_dir {
    struct hypfs_entry e;
    struct list_head dirlist;
};

struct hypfs_dyndir_id {
    struct hypfs_entry_dir dir;       /* Modified copy of template. */
    struct hypfs_funcs funcs;         /* Dynamic functions. */
    struct hypfs_entry_dir *template; /* Template used. */
    char name[12];                    /* Name of hypfs entry. */

    unsigned int id;                  /* Numerical id. */
};

#define HYPFS_DIRENTRY_NAME_OFF offsetof(struct xen_hypfs_dirlistentry, name)
#define HYPFS_DIRENTRY_SIZE(name_len) \
    (HYPFS_DIRENTRY_NAME_OFF +        \
     ROUNDUP((name_len) + 1, alignof(struct xen_hypfs_direntry)))

#define HYPFS_VARDIR_INIT(var, nam, fn)           \
    struct hypfs_entry_dir __read_mostly var = {  \
        .e.type = XEN_HYPFS_TYPE_DIR,             \
        .e.encoding = XEN_HYPFS_ENC_PLAIN,        \
        .e.name = (nam),                          \
        .e.size = 0,                              \
        .e.max_size = 0,                          \
        .e.list = LIST_HEAD_INIT(var.e.list),     \
        .e.funcs = (fn),                          \
        .dirlist = LIST_HEAD_INIT(var.dirlist),   \
    }

#define HYPFS_DIR_INIT(var, nam)                  \
    HYPFS_VARDIR_INIT(var, nam, &hypfs_dir_funcs)

#define HYPFS_VARSIZE_INIT(var, typ, nam, msz, fn) \
    struct hypfs_entry_leaf __read_mostly var = {  \
        .e.type = (typ),                           \
        .e.encoding = XEN_HYPFS_ENC_PLAIN,         \
        .e.name = (nam),                           \
        .e.max_size = (msz),                       \
        .e.funcs = (fn),                           \
    }

/* Content and size need to be set via hypfs_string_set_reference(). */
#define HYPFS_STRING_INIT(var, nam)               \
    HYPFS_VARSIZE_INIT(var, XEN_HYPFS_TYPE_STRING, nam, 0, &hypfs_leaf_ro_funcs)

/*
 * Set content and size of a XEN_HYPFS_TYPE_STRING node. The node will point
 * to str, so any later modification of *str should be followed by a call
 * to hypfs_string_set_reference() in order to update the size of the node
 * data.
 */
static inline void hypfs_string_set_reference(struct hypfs_entry_leaf *leaf,
                                              const char *str)
{
    leaf->u.content = str;
    leaf->e.size = strlen(str) + 1;
}

#define HYPFS_FIXEDSIZE_INIT(var, typ, nam, contvar, fn, wr) \
    struct hypfs_entry_leaf __read_mostly var = {            \
        .e.type = (typ),                                     \
        .e.encoding = XEN_HYPFS_ENC_PLAIN,                   \
        .e.name = (nam),                                     \
        .e.size = sizeof(contvar),                           \
        .e.max_size = (wr) ? sizeof(contvar) : 0,            \
        .e.funcs = (fn),                                     \
        .u.content = &(contvar),                             \
    }

#define HYPFS_UINT_INIT(var, nam, contvar)                       \
    HYPFS_FIXEDSIZE_INIT(var, XEN_HYPFS_TYPE_UINT, nam, contvar, \
                         &hypfs_leaf_ro_funcs, 0)
#define HYPFS_UINT_INIT_WRITABLE(var, nam, contvar)              \
    HYPFS_FIXEDSIZE_INIT(var, XEN_HYPFS_TYPE_UINT, nam, contvar, \
                         &hypfs_leaf_wr_funcs, 1)

#define HYPFS_INT_INIT(var, nam, contvar)                        \
    HYPFS_FIXEDSIZE_INIT(var, XEN_HYPFS_TYPE_INT, nam, contvar,  \
                         &hypfs_leaf_ro_funcs, 0)
#define HYPFS_INT_INIT_WRITABLE(var, nam, contvar)               \
    HYPFS_FIXEDSIZE_INIT(var, XEN_HYPFS_TYPE_INT, nam, contvar, \
                         &hypfs_leaf_wr_funcs, 1)

#define HYPFS_BOOL_INIT(var, nam, contvar)                       \
    HYPFS_FIXEDSIZE_INIT(var, XEN_HYPFS_TYPE_BOOL, nam, contvar, \
                         &hypfs_leaf_ro_funcs, 0)
#define HYPFS_BOOL_INIT_WRITABLE(var, nam, contvar)              \
    HYPFS_FIXEDSIZE_INIT(var, XEN_HYPFS_TYPE_BOOL, nam, contvar, \
                         &hypfs_bool_wr_funcs, 1)

extern struct hypfs_entry_dir hypfs_root;

int hypfs_add_dir(struct hypfs_entry_dir *parent,
                  struct hypfs_entry_dir *dir, bool nofault);
int hypfs_add_leaf(struct hypfs_entry_dir *parent,
                   struct hypfs_entry_leaf *leaf, bool nofault);
int hypfs_read_dir(const struct hypfs_entry *entry,
                   XEN_GUEST_HANDLE_PARAM(void) uaddr);
int hypfs_read_leaf(const struct hypfs_entry *entry,
                    XEN_GUEST_HANDLE_PARAM(void) uaddr);
int hypfs_write_leaf(struct hypfs_entry_leaf *leaf,
                     XEN_GUEST_HANDLE_PARAM(void) uaddr, unsigned int ulen);
int hypfs_write_bool(struct hypfs_entry_leaf *leaf,
                     XEN_GUEST_HANDLE_PARAM(void) uaddr, unsigned int ulen);
int hypfs_write_custom(struct hypfs_entry_leaf *leaf,
                       XEN_GUEST_HANDLE_PARAM(void) uaddr, unsigned int ulen);
unsigned int hypfs_getsize(const struct hypfs_entry *entry);
struct hypfs_entry *hypfs_dir_findentry(struct hypfs_entry_dir *dir,
                                        const char *name,
                                        unsigned int name_len);
void *hypfs_alloc_dyndata(unsigned long size, unsigned long align);
void *hypfs_get_dyndata(void);
int hypfs_read_dyndir_id_entry(struct hypfs_entry_dir *template,
                               unsigned int id, bool is_last,
                               XEN_GUEST_HANDLE_PARAM(void) *uaddr);
struct hypfs_entry *hypfs_gen_dyndir_entry_id(struct hypfs_entry_dir *template,
                                              unsigned int id);
#endif

#endif /* __XEN_HYPFS_H__ */
