#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xenfs.h>

static struct xenfs_handle *hdl;

static int xenfs_cat(char *path)
{
    int ret = 0;
    char *result;

    result = xenfs_read(hdl, path);
    if (!result) {
        perror("could not read");
        ret = 3;
    } else {
        printf("%s\n", result);
        free(result);
    }
    return ret;
}

static int xenfs_ls(char *path)
{
    struct xenfs_dirent *ent;
    unsigned int n, i;
    int ret = 0;

    ent = xenfs_readdir(hdl, path, &n);
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

static int xenfs_tree_sub(char *path, unsigned int depth)
{
    struct xenfs_dirent *ent;
    unsigned int n, i;
    int ret = 0;
    char *p;

    ent = xenfs_readdir(hdl, path, &n);
    if (!ent)
        return 1;

    for (i = 0; i < n; i++) {
        printf("%*s%s%s\n", depth * 2, "", ent[i].name,
               ent[i].is_dir ? "/" : "");
        if (ent[i].is_dir) {
            asprintf(&p, "%s%s%s", path, (depth == 1) ? "" : "/", ent[i].name);
            if (xenfs_tree_sub(p, depth + 1))
                ret = 1;
        }
    }

    free(ent);

    return ret;
}

static int xenfs_tree(void)
{
    printf("/\n");

    return xenfs_tree_sub("/", 1);
}

int main(int argc, char *argv[])
{
    int ret;

    hdl = xenfs_open(NULL, 0);

    if (!hdl) {
        fprintf(stderr, "Could not open libxenfs\n");
        ret = 2;
    } else if (argc == 3 && !strcmp(argv[1], "--cat"))
        ret = xenfs_cat(argv[2]);
    else if (argc == 3 && !strcmp(argv[1], "--ls"))
        ret = xenfs_ls(argv[2]);
    else if (argc == 2 && !strcmp(argv[1], "--tree"))
        ret = xenfs_tree();
    else {
        fprintf(stderr, "usage: xenfs --ls <path>\n");
        fprintf(stderr, "       xenfs --cat <path>\n");
        fprintf(stderr, "       xenfs --tree\n");
        ret = 1;
    }

    xenfs_close(hdl);

    return ret;
}
