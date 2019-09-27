#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xenhypfs.h>

static struct xenhypfs_handle *hdl;

static int xenhypfs_cat(char *path)
{
    int ret = 0;
    char *result;

    result = xenhypfs_read(hdl, path);
    if (!result) {
        perror("could not read");
        ret = 3;
    } else {
        printf("%s\n", result);
        free(result);
    }

    return ret;
}

static int xenhypfs_wr(char *path, char *val)
{
    int ret;

    ret = xenhypfs_write(hdl, path, val);
    if (ret) {
        perror("could not write");
        ret = 3;
    }

    return ret;
}

static int xenhypfs_ls(char *path)
{
    struct xenhypfs_dirent *ent;
    unsigned int n, i;
    int ret = 0;

    ent = xenhypfs_readdir(hdl, path, &n);
    if (!ent) {
        perror("could not read dir");
        ret = 3;
    } else {
        for (i = 0; i < n; i++)
            printf("%c %s\n", ent[i].is_dir ? 'd' : '-', ent[i].name);

        free(ent);
    }

    return ret;
}

static int xenhypfs_tree_sub(char *path, unsigned int depth)
{
    struct xenhypfs_dirent *ent;
    unsigned int n, i;
    int ret = 0;
    char *p;

    ent = xenhypfs_readdir(hdl, path, &n);
    if (!ent)
        return 1;

    for (i = 0; i < n; i++) {
        printf("%*s%s%s\n", depth * 2, "", ent[i].name,
               ent[i].is_dir ? "/" : "");
        if (ent[i].is_dir) {
            asprintf(&p, "%s%s%s", path, (depth == 1) ? "" : "/", ent[i].name);
            if (xenhypfs_tree_sub(p, depth + 1))
                ret = 1;
        }
    }

    free(ent);

    return ret;
}

static int xenhypfs_tree(void)
{
    printf("/\n");

    return xenhypfs_tree_sub("/", 1);
}

int main(int argc, char *argv[])
{
    int ret;

    hdl = xenhypfs_open(NULL, 0);

    if (!hdl) {
        fprintf(stderr, "Could not open libxenhypfs\n");
        ret = 2;
    } else if (argc == 3 && !strcmp(argv[1], "cat"))
        ret = xenhypfs_cat(argv[2]);
    else if (argc == 3 && !strcmp(argv[1], "ls"))
        ret = xenhypfs_ls(argv[2]);
    else if (argc == 4 && !strcmp(argv[1], "write"))
        ret = xenhypfs_wr(argv[2], argv[3]);
    else if (argc == 2 && !strcmp(argv[1], "tree"))
        ret = xenhypfs_tree();
    else {
        fprintf(stderr, "usage: xenhypfs ls <path>\n");
        fprintf(stderr, "       xenhypfs cat <path>\n");
        fprintf(stderr, "       xenhypfs write <path> <val>\n");
        fprintf(stderr, "       xenhypfs tree\n");
        ret = 1;
    }

    xenhypfs_close(hdl);

    return ret;
}
