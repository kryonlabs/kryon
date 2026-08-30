/*
 * kry_filesystem.h - Kry standard library: filesystem access.
 *
 * Directory iteration, file metadata, and text read/write, sized for an IDE
 * file tree and source editor. Windows currently has no implementation;
 * callers should treat a -1 / 0 return as "unavailable".
 */
#ifndef KRYON_KRY_FILESYSTEM_H
#define KRYON_KRY_FILESYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

/* A directory entry. `name` is the bare entry name (no path); the caller joins
 * it with the directory path. Capacity is KRY_FS_NAME_MAX. */
#define KRY_FS_NAME_MAX 256

typedef struct {
    char name[KRY_FS_NAME_MAX];
    int is_dir;   /* 1 for a directory, 0 for a file */
    long mtime;
    unsigned long long size;
    int readable;
    int hidden;
} KryDirEntry;

/* File metadata used for change detection and sorting. */
typedef struct {
    long mtime;   /* modification time in seconds, or -1 if unavailable */
    unsigned long long size;
    int is_dir;
    int exists;
    int readable;
} KryFileStat;

/* Iterate the direct children of `dir`. Fills `out` (up to `cap` entries) and
 * returns the count (0 if empty or unavailable). Hidden entries (leading .) ,
 * `.` and `..` are skipped. */
int kry_fs_list_dir(const char *dir, KryDirEntry *out, int cap);
int kry_fs_list_dir_ex(const char *dir, KryDirEntry *out, int cap,
                       int include_hidden);

/* Stat a path. Always fills the struct (exists=0 if missing). Returns 1 on
 * success (path stat-able, even if it doesnt exist), 0 on failure. */
int kry_fs_stat(const char *path, KryFileStat *out);

/* Convenience: just the modification time, or -1. */
long kry_fs_mtime(const char *path);

/* Recursive mkdir (like `mkdir -p`). Returns 0 on success or if the directory
 * already exists, -1 on failure. */
int kry_fs_mkdir_p(const char *path);

/* Resolve an absolute, canonicalized path (realpath). Writes up to cap-1 bytes
 * (NUL-terminated) into `out`. Returns 1 on success, 0 if the path cannot be
 * resolved. */
int kry_fs_realpath(const char *path, char *out, int cap);

/* Read a text file into `buf` (up to cap-1 bytes, NUL-terminated). Returns the
 * byte count, or -1 if the file cannot be opened or is too large. */
int kry_fs_read_file(const char *path, char *buf, int cap);

/* Write `len` bytes of text to `path` (truncating). Returns the byte count
 * written, or -1 on failure. */
int kry_fs_write_file(const char *path, const char *text, int len);

/* Test whether a path exists (file or directory). */
int kry_fs_exists(const char *path);

/* File-manager-grade local operations. These return 0 on success and -1 on
 * failure, matching mkdir-like C library conventions. */
int kry_fs_create_file(const char *path);
int kry_fs_create_dir(const char *path);
int kry_fs_copy_recursive(const char *src, const char *dst);
int kry_fs_move(const char *src, const char *dst);
int kry_fs_remove_recursive(const char *path);
int kry_fs_symlink(const char *target, const char *link_path);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_KRY_FILESYSTEM_H */
