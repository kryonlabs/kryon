/* Plan 9 native shim: mkdir is provided by the plan9 OS helper
 * (src/platform/plan9/plan9_os.c); stat-style probing goes through Dir. */
#ifndef KRYON_PLAN9_SHIM_SYS_STAT_H
#define KRYON_PLAN9_SHIM_SYS_STAT_H

#include "kryon_plan9_libc.h"

#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_IFMT 0170000
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)

struct stat {
    ulong st_mode;
    vlong st_size;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
};

int kryon_plan9_mkdir(const char *path);

static int
kryon_plan9_stat(const char *path, struct stat *st)
{
    Dir *dir;

    if(path == nil || st == nil)
        return -1;
    dir = dirstat((char*)path);
    if(dir == nil)
        return -1;
    memset(st, 0, sizeof(*st));
    st->st_mode = dir->mode & 0777;
    if(dir->qid.type & QTDIR)
        st->st_mode |= S_IFDIR;
    else
        st->st_mode |= S_IFREG;
    st->st_size = dir->length;
    st->st_atime = dir->atime;
    st->st_mtime = dir->mtime;
    st->st_ctime = dir->mtime;
    free(dir);
    return 0;
}

#define mkdir(path, mode) kryon_plan9_mkdir(path)
#define stat(path, st) kryon_plan9_stat(path, st)

#endif
