/*
 * kry_filesystem.c - Kry standard library: filesystem access.
 *
 * Filesystem helpers for Kry apps: opendir/readdir/stat, text read/write,
 * recursive mkdir, and realpath wrapped in a small C surface.
 */
/* Request POSIX 2008 + default (BSD) extensions so realpath(), lstat(), and
 * dt_type are declared regardless of how the including TU sets feature macros. */
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "kry_filesystem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(KRYON_PLATFORM_PLAN9)
/* Native Plan 9: directories through Dir/dirreadall,
 * metadata through dirstat, path normalization through cleanname. */
#include <errno.h>
#include <sys/stat.h>

static int
kry_ascii_lower(int c)
{
    if(c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

static int
kry_name_cmp(const char *a, const char *b)
{
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;

    while(*pa != '\0' && *pb != '\0') {
        int ca = kry_ascii_lower(*pa);
        int cb = kry_ascii_lower(*pb);

        if(ca != cb)
            return ca - cb;
        pa++;
        pb++;
    }
    if(*pa != *pb)
        return (int)*pa - (int)*pb;
    return strcmp(a, b);
}

static int
kry_dir_entry_cmp(const void *a, const void *b)
{
    const KryDirEntry *ea = (const KryDirEntry *)a;
    const KryDirEntry *eb = (const KryDirEntry *)b;

    if(ea->is_dir != eb->is_dir)
        return eb->is_dir - ea->is_dir;
    return kry_name_cmp(ea->name, eb->name);
}

int
kry_fs_list_dir(const char *dir, KryDirEntry *out, int cap)
{
    int fd;
    Dir *db;
    long n;
    int count = 0;

    if(dir == NULL || out == NULL || cap <= 0)
        return 0;
    fd = open(dir, OREAD);
    if(fd < 0)
        return 0;
    n = dirreadall(fd, &db);
    close(fd);
    if(n < 0)
        return 0;
    for(long i = 0; i < n && count < cap; i++) {
        size_t len;

        if(db[i].name[0] == '.')
            continue;   /* skip dot files, including . and .. */
        len = strlen(db[i].name);
        if(len >= KRY_FS_NAME_MAX)
            continue;
        memcpy(out[count].name, db[i].name, len + 1);
        out[count].is_dir = (db[i].mode & DMDIR) != 0;
        count++;
    }
    free(db);
    qsort(out, (size_t)count, sizeof(out[0]), kry_dir_entry_cmp);
    return count;
}

int
kry_fs_stat(const char *path, KryFileStat *out)
{
    Dir *d;

    if(path == NULL || out == NULL)
        return 0;
    out->exists = 0;
    out->is_dir = 0;
    out->mtime = -1;
    d = dirstat(path);
    if(d == NULL)
        return 1;   /* probe succeeded; path just doesnt exist */
    out->exists = 1;
    out->is_dir = (d->mode & DMDIR) != 0;
    out->mtime = (long)d->mtime;
    free(d);
    return 1;
}

long
kry_fs_mtime(const char *path)
{
    KryFileStat st;

    if(!kry_fs_stat(path, &st))
        return -1;
    return st.mtime;
}

int
kry_fs_mkdir_p(const char *path)
{
    char tmp[4096];
    size_t len;
    Dir *d;

    if(path == NULL || path[0] == '\0')
        return -1;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if(len == 0)
        return -1;
    /* Strip trailing separators. */
    while(len > 0 && tmp[len - 1] == '/')
        tmp[--len] = '\0';
    for(size_t i = 1; i < len; i++) {
        if(tmp[i] == '/') {
            tmp[i] = '\0';
            if(mkdir(tmp, 0755) != 0) {
                d = dirstat(tmp);
                if(d == NULL) {
                    free(d);
                    return -1;
                }
                if((d->mode & DMDIR) == 0) {
                    free(d);
                    return -1;
                }
                free(d);
            }
            tmp[i] = '/';
        }
    }
    if(mkdir(tmp, 0755) != 0) {
        d = dirstat(tmp);
        if(d == NULL) {
            free(d);
            return -1;
        }
        if((d->mode & DMDIR) == 0) {
            free(d);
            return -1;
        }
        free(d);
    }
    return 0;
}

int
kry_fs_realpath(const char *path, char *out, int cap)
{
    if(path == NULL || out == NULL || cap <= 0)
        return 0;
    snprintf(out, (size_t)cap, "%s", cleanname((char *)path));
    return 1;
}

int
kry_fs_read_file(const char *path, char *buf, int cap)
{
    FILE *f;
    size_t got;

    if(path == NULL || buf == NULL || cap <= 0)
        return -1;
    f = fopen(path, "rb");
    if(f == NULL)
        return -1;
    got = fread(buf, 1, (size_t)cap - 1, f);
    fclose(f);
    buf[got] = '\0';
    return (int)got;
}

int
kry_fs_write_file(const char *path, const char *text, int len)
{
    FILE *f;
    size_t wrote;

    if(path == NULL || text == NULL || len < 0)
        return -1;
    f = fopen(path, "wb");
    if(f == NULL)
        return -1;
    wrote = fwrite(text, 1, (size_t)len, f);
    fclose(f);
    return (int)wrote == len ? (int)wrote : -1;
}

int
kry_fs_exists(const char *path)
{
    Dir *d;
    int exists;

    if(path == NULL)
        return 0;
    d = dirstat(path);
    if(d == NULL)
        return 0;
    exists = 1;
    free(d);
    return exists;
}

#elif !defined(_WIN32)
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

static int
kry_ascii_lower(int c)
{
    if(c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

static int
kry_name_cmp(const char *a, const char *b)
{
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;

    while(*pa != '\0' && *pb != '\0') {
        int ca = kry_ascii_lower(*pa);
        int cb = kry_ascii_lower(*pb);

        if(ca != cb)
            return ca - cb;
        pa++;
        pb++;
    }
    if(*pa != *pb)
        return (int)*pa - (int)*pb;
    return strcmp(a, b);
}

static int
kry_dir_entry_cmp(const void *a, const void *b)
{
    const KryDirEntry *ea = (const KryDirEntry *)a;
    const KryDirEntry *eb = (const KryDirEntry *)b;

    if(ea->is_dir != eb->is_dir)
        return eb->is_dir - ea->is_dir;
    return kry_name_cmp(ea->name, eb->name);
}

int
kry_fs_list_dir(const char *dir, KryDirEntry *out, int cap)
{
    DIR *d;
    struct dirent *e;
    int count = 0;

    if(dir == NULL || out == NULL || cap <= 0)
        return 0;
    d = opendir(dir);
    if(d == NULL)
        return 0;
    while((e = readdir(d)) != NULL) {
        const char *name = e->d_name;
        size_t len;

        if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;
        if(name[0] == '.')
            continue;   /* skip hidden entries */
        if(count >= cap)
            break;
        len = strlen(name);
        if(len >= KRY_FS_NAME_MAX)
            continue;
        memcpy(out[count].name, name, len + 1);
        /* Use d_type when available; fall back to stat. */
#ifdef DTTOIF
        if(e->d_type != DT_UNKNOWN)
            out[count].is_dir = (e->d_type == DT_DIR);
        else
#endif
        {
            char full[4096];
            struct stat st;

            snprintf(full, sizeof(full), "%s/%s", dir, name);
            out[count].is_dir = (stat(full, &st) == 0 && S_ISDIR(st.st_mode));
        }
        count++;
    }
    closedir(d);
    qsort(out, (size_t)count, sizeof(out[0]), kry_dir_entry_cmp);
    return count;
}

int
kry_fs_stat(const char *path, KryFileStat *out)
{
    struct stat st;

    if(path == NULL || out == NULL)
        return 0;
    out->exists = 0;
    out->is_dir = 0;
    out->mtime = -1;
    if(stat(path, &st) != 0)
        return 1;   /* stat-able call; path just doesnt exist */
    out->exists = 1;
    out->is_dir = S_ISDIR(st.st_mode);
    out->mtime = (long)st.st_mtime;
    return 1;
}

long
kry_fs_mtime(const char *path)
{
    KryFileStat st;

    if(!kry_fs_stat(path, &st))
        return -1;
    return st.mtime;
}

int
kry_fs_mkdir_p(const char *path)
{
    char tmp[4096];
    size_t len;

    if(path == NULL || path[0] == '\0')
        return -1;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if(len == 0)
        return -1;
    /* Strip trailing separators. */
    while(len > 0 && tmp[len - 1] == '/')
        tmp[--len] = '\0';
    for(size_t i = 1; i < len; i++) {
        if(tmp[i] == '/') {
            tmp[i] = '\0';
            if(mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            tmp[i] = '/';
        }
    }
    if(mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

int
kry_fs_realpath(const char *path, char *out, int cap)
{
    char *resolved;

    if(path == NULL || out == NULL || cap <= 0)
        return 0;
    resolved = realpath(path, NULL);
    if(resolved == NULL)
        return 0;
    snprintf(out, (size_t)cap, "%s", resolved);
    free(resolved);
    return 1;
}

int
kry_fs_read_file(const char *path, char *buf, int cap)
{
    FILE *f;
    size_t got;

    if(path == NULL || buf == NULL || cap <= 0)
        return -1;
    f = fopen(path, "rb");
    if(f == NULL)
        return -1;
    got = fread(buf, 1, (size_t)cap - 1, f);
    fclose(f);
    buf[got] = '\0';
    return (int)got;
}

int
kry_fs_write_file(const char *path, const char *text, int len)
{
    FILE *f;
    size_t wrote;

    if(path == NULL || text == NULL || len < 0)
        return -1;
    f = fopen(path, "wb");
    if(f == NULL)
        return -1;
    wrote = fwrite(text, 1, (size_t)len, f);
    fclose(f);
    return (int)wrote == len ? (int)wrote : -1;
}

int
kry_fs_exists(const char *path)
{
    struct stat st;

    if(path == NULL)
        return 0;
    return stat(path, &st) == 0;
}

#else  /* _WIN32 */

int kry_fs_list_dir(const char *dir, KryDirEntry *out, int cap)
{
    (void)dir; (void)out; (void)cap; return 0;
}
int kry_fs_stat(const char *path, KryFileStat *out)
{
    (void)path; (void)out; return 0;
}
long kry_fs_mtime(const char *path) { (void)path; return -1; }
int kry_fs_mkdir_p(const char *path) { (void)path; return -1; }
int kry_fs_realpath(const char *path, char *out, int cap)
{
    (void)path; (void)out; (void)cap; return 0;
}
int kry_fs_read_file(const char *path, char *buf, int cap)
{
    (void)path; (void)buf; (void)cap; return -1;
}
int kry_fs_write_file(const char *path, const char *text, int len)
{
    (void)path; (void)text; (void)len; return -1;
}
int kry_fs_exists(const char *path) { (void)path; return 0; }

#endif /* _WIN32 */
