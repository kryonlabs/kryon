/* Plan 9 native shim: small POSIX dirent surface over dirreadall(). */
#ifndef KRYON_PLAN9_SHIM_DIRENT_H
#define KRYON_PLAN9_SHIM_DIRENT_H

#include "kryon_plan9_libc.h"

#define MAXNAMLEN 255

struct dirent {
    char d_name[MAXNAMLEN + 1];
};

typedef struct DIR DIR;
struct DIR {
    int fd;
    Dir *entries;
    long count;
    long index;
    struct dirent current;
};

static DIR*
opendir(const char *path)
{
    DIR *dir;
    long count;
    int fd;

    if(path == nil)
        return nil;
    fd = open((char*)path, OREAD);
    if(fd < 0)
        return nil;
    dir = malloc(sizeof(*dir));
    if(dir == nil){
        close(fd);
        return nil;
    }
    memset(dir, 0, sizeof(*dir));
    count = dirreadall(fd, &dir->entries);
    if(count < 0){
        close(fd);
        free(dir);
        return nil;
    }
    dir->fd = fd;
    dir->count = count;
    return dir;
}

static struct dirent*
readdir(DIR *dir)
{
    if(dir == nil || dir->index >= dir->count)
        return nil;
    snprint(dir->current.d_name, sizeof(dir->current.d_name), "%s", dir->entries[dir->index].name);
    dir->index++;
    return &dir->current;
}

static int
closedir(DIR *dir)
{
    int result;

    if(dir == nil)
        return -1;
    result = close(dir->fd);
    free(dir->entries);
    free(dir);
    return result;
}

#endif
