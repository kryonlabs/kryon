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

static void
copy_text(char *out, int cap, const char *text)
{
    int i;

    if(out == NULL || cap <= 0)
        return;
    if(text == NULL)
        text = "";
    for(i = 0; i < cap - 1 && text[i] != '\0'; i++)
        out[i] = text[i];
    out[i] = '\0';
}

int
kry_fs_join_path(char *out, int cap, const char *base, const char *name)
{
    int base_len;

    if(out == NULL || cap <= 0 || name == NULL)
        return 0;
    if(base == NULL || base[0] == '\0' || strcmp(base, "/") == 0) {
        snprintf(out, (size_t)cap, "/%.*s", cap - 2, name);
        return 1;
    }
    base_len = (int)strlen(base);
    while(base_len > 1 && base[base_len - 1] == '/')
        base_len--;
    if(base_len > cap - 2)
        base_len = cap - 2;
    snprintf(out, (size_t)cap, "%.*s/%.*s", base_len, base,
             cap - base_len - 2, name);
    return 1;
}

const char *
kry_fs_base_name(const char *path)
{
    const char *slash;

    if(path == NULL || path[0] == '\0')
        return "";
    slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

int
kry_fs_parent_path(char *out, int cap, const char *path)
{
    char tmp[KRY_FS_PATH_MAX];
    char *slash;

    if(out == NULL || cap <= 0 || path == NULL || path[0] == '\0')
        return 0;
    copy_text(tmp, sizeof(tmp), path);
    slash = strrchr(tmp, '/');
    if(slash == NULL || slash == tmp) {
        copy_text(out, cap, "/");
        return 1;
    }
    *slash = '\0';
    copy_text(out, cap, tmp);
    return 1;
}

int
kry_fs_home_dir(char *out, int cap)
{
    const char *home;

    if(out == NULL || cap <= 0)
        return 0;
#if defined(KRYON_PLATFORM_PLAN9)
    home = getenv("home");
    if(home != NULL && home[0] != '\0') {
        copy_text(out, cap, home);
        return 1;
    }
#endif
    home = getenv("HOME");
    if(home != NULL && home[0] != '\0') {
        copy_text(out, cap, home);
        return 1;
    }
    copy_text(out, cap, "/");
    return 1;
}

int
kry_fs_data_home_dir(char *out, int cap)
{
    const char *data_home;
    char home[KRY_FS_PATH_MAX];

    if(out == NULL || cap <= 0)
        return 0;
    data_home = getenv("XDG_DATA_HOME");
    if(data_home != NULL && data_home[0] != '\0') {
        copy_text(out, cap, data_home);
        return 1;
    }
    if(!kry_fs_home_dir(home, sizeof(home)))
        return 0;
    snprintf(out, (size_t)cap, "%s/.local/share", home);
    return 1;
}

static const char *
user_dir_name(KryUserDir dir)
{
    switch(dir) {
    case KRY_USER_DIR_DESKTOP: return "Desktop";
    case KRY_USER_DIR_DOCUMENTS: return "Documents";
    case KRY_USER_DIR_DOWNLOAD: return "Downloads";
    case KRY_USER_DIR_MUSIC: return "Music";
    case KRY_USER_DIR_PICTURES: return "Pictures";
    case KRY_USER_DIR_VIDEOS: return "Videos";
    case KRY_USER_DIR_TEMPLATES: return "Templates";
    case KRY_USER_DIR_PUBLIC_SHARE: return "Public";
    case KRY_USER_DIR_HOME:
    default: return "";
    }
}

static const char *
user_dir_key(KryUserDir dir)
{
    switch(dir) {
    case KRY_USER_DIR_DESKTOP: return "XDG_DESKTOP_DIR";
    case KRY_USER_DIR_DOCUMENTS: return "XDG_DOCUMENTS_DIR";
    case KRY_USER_DIR_DOWNLOAD: return "XDG_DOWNLOAD_DIR";
    case KRY_USER_DIR_MUSIC: return "XDG_MUSIC_DIR";
    case KRY_USER_DIR_PICTURES: return "XDG_PICTURES_DIR";
    case KRY_USER_DIR_VIDEOS: return "XDG_VIDEOS_DIR";
    case KRY_USER_DIR_TEMPLATES: return "XDG_TEMPLATES_DIR";
    case KRY_USER_DIR_PUBLIC_SHARE: return "XDG_PUBLICSHARE_DIR";
    case KRY_USER_DIR_HOME:
    default: return "";
    }
}

static int
config_home(char *out, int cap)
{
    const char *config_home = getenv("XDG_CONFIG_HOME");
    char home[KRY_FS_PATH_MAX];

    if(config_home != NULL && config_home[0] != '\0') {
        copy_text(out, cap, config_home);
        return 1;
    }
    if(!kry_fs_home_dir(home, sizeof(home)))
        return 0;
    snprintf(out, (size_t)cap, "%s/.config", home);
    return 1;
}

static int
expand_xdg_path(char *out, int cap, const char *value)
{
    char home[KRY_FS_PATH_MAX];
    char tmp[KRY_FS_PATH_MAX];
    int used = 0;
    int i;

    if(out == NULL || cap <= 0 || value == NULL)
        return 0;
    for(i = 0; value[i] != '\0' && used < (int)sizeof(tmp) - 1; i++) {
        if(value[i] == '\\' && value[i + 1] != '\0') {
            tmp[used++] = value[++i];
        } else {
            tmp[used++] = value[i];
        }
    }
    tmp[used] = '\0';
    if(strncmp(tmp, "$HOME", 5) == 0) {
        if(!kry_fs_home_dir(home, sizeof(home)))
            return 0;
        snprintf(out, (size_t)cap, "%s%s", home, tmp + 5);
    } else if(tmp[0] == '~' && (tmp[1] == '/' || tmp[1] == '\0')) {
        if(!kry_fs_home_dir(home, sizeof(home)))
            return 0;
        snprintf(out, (size_t)cap, "%s%s", home, tmp + 1);
    } else {
        copy_text(out, cap, tmp);
    }
    return out[0] != '\0';
}

static int
read_xdg_user_dir(KryUserDir dir, char *out, int cap)
{
    char config[KRY_FS_PATH_MAX];
    char path[KRY_FS_PATH_MAX];
    char text[8192];
    const char *key = user_dir_key(dir);
    char *line;

    if(key[0] == '\0' || !config_home(config, sizeof(config)))
        return 0;
    snprintf(path, sizeof(path), "%s/user-dirs.dirs", config);
    if(kry_fs_read_file(path, text, sizeof(text)) < 0)
        return 0;
    line = strstr(text, key);
    while(line != NULL) {
        char *eq;
        char *start;
        char *end;

        if(line == text || line[-1] == '\n') {
            eq = strchr(line, '=');
            if(eq != NULL) {
                start = strchr(eq, '"');
                if(start != NULL) {
                    start++;
                    end = strchr(start, '"');
                    if(end != NULL) {
                        *end = '\0';
                        return expand_xdg_path(out, cap, start);
                    }
                }
            }
        }
        line = strstr(line + 1, key);
    }
    return 0;
}

int
kry_fs_user_dir(KryUserDir dir, char *out, int cap)
{
    char home[KRY_FS_PATH_MAX];
    const char *name;

    if(out == NULL || cap <= 0)
        return 0;
    if(dir == KRY_USER_DIR_HOME)
        return kry_fs_home_dir(out, cap);
#if !defined(KRYON_PLATFORM_PLAN9) && !defined(_WIN32)
    if(read_xdg_user_dir(dir, out, cap))
        return 1;
#endif
    if(!kry_fs_home_dir(home, sizeof(home)))
        return 0;
    name = user_dir_name(dir);
    if(name[0] == '\0')
        return 0;
    snprintf(out, (size_t)cap, "%s/%s", home, name);
    return 1;
}

int
kry_fs_trash_dir(KryTrashDir dir, char *out, int cap)
{
    char root[KRY_FS_PATH_MAX];
    char data_home[KRY_FS_PATH_MAX];

    if(out == NULL || cap <= 0)
        return 0;
    if(!kry_fs_data_home_dir(data_home, sizeof(data_home)))
        return 0;
    snprintf(root, sizeof(root), "%s/Trash", data_home);
    if(dir == KRY_TRASH_DIR_ROOT) {
        copy_text(out, cap, root);
        return 1;
    }
    return kry_fs_join_path(out, cap, root,
                            dir == KRY_TRASH_DIR_INFO ? "info" : "files");
}

static int
read_line_value(char *out, int cap, const char *path, const char *key)
{
    char text[16384];
    char *line;
    size_t key_len;

    if(out == NULL || cap <= 0 || path == NULL || key == NULL)
        return 0;
    if(kry_fs_read_file(path, text, sizeof(text)) < 0)
        return 0;
    key_len = strlen(key);
    line = text;
    while(line != NULL && *line != '\0') {
        char *next = strchr(line, '\n');
        char *value;

        if(next != NULL)
            *next = '\0';
        if(strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            value = line + key_len + 1;
            while(*value == ' ' || *value == '\t')
                value++;
            copy_text(out, cap, value);
            return out[0] != '\0';
        }
        line = next != NULL ? next + 1 : NULL;
    }
    return 0;
}

static int
read_xfce_icon_theme(char *out, int cap)
{
    char config[KRY_FS_PATH_MAX];
    char path[KRY_FS_PATH_MAX];
    char text[32768];
    char *p;
    char *value;
    char *end;

    if(!config_home(config, sizeof(config)))
        return 0;
    snprintf(path, sizeof(path),
             "%s/xfce4/xfconf/xfce-perchannel-xml/xsettings.xml", config);
    if(kry_fs_read_file(path, text, sizeof(text)) < 0)
        return 0;
    p = strstr(text, "IconThemeName");
    if(p == NULL)
        return 0;
    value = strstr(p, "value=\"");
    if(value == NULL)
        return 0;
    value += 7;
    end = strchr(value, '"');
    if(end == NULL)
        return 0;
    *end = '\0';
    copy_text(out, cap, value);
    return out[0] != '\0';
}

int
kry_fs_icon_theme(char *out, int cap)
{
    const char *env_theme;
    char config[KRY_FS_PATH_MAX];
    char path[KRY_FS_PATH_MAX];

    if(out == NULL || cap <= 0)
        return 0;
    env_theme = getenv("GTK_ICON_THEME");
    if(env_theme != NULL && env_theme[0] != '\0') {
        copy_text(out, cap, env_theme);
        return 1;
    }
    if(config_home(config, sizeof(config))) {
        snprintf(path, sizeof(path), "%s/gtk-3.0/settings.ini", config);
        if(read_line_value(out, cap, path, "gtk-icon-theme-name"))
            return 1;
        snprintf(path, sizeof(path), "%s/gtk-4.0/settings.ini", config);
        if(read_line_value(out, cap, path, "gtk-icon-theme-name"))
            return 1;
    }
    if(read_xfce_icon_theme(out, cap))
        return 1;
    copy_text(out, cap, "Adwaita");
    return 1;
}

static int
icon_candidate(char *out, int cap, const char *root,
                      const char *theme, const char *middle,
                      const char *context, const char *name,
                      const char *ext)
{
    if(middle == NULL || middle[0] == '\0')
        snprintf(out, (size_t)cap, "%s/%s/%s/%s.%s", root, theme, context,
                 name, ext);
    else
        snprintf(out, (size_t)cap, "%s/%s/%s/%s/%s.%s", root, theme, middle,
                 context, name, ext);
    return kry_fs_exists(out);
}

static int
find_icon_in_theme(const char *theme, const char *name, int size,
                          char *out, int cap)
{
    const char *home_roots[] = {
        "%s/.local/share/icons",
        "%s/.icons",
        NULL
    };
    const char *fixed_roots[] = {
        "/usr/local/share/icons",
        "/usr/share/icons",
        NULL
    };
    const char *contexts[] = {
        "Places", "places", "Mimetypes", "mimetypes", "Devices", "devices",
        "Actions", "actions", "Apps", "apps", "Status", "status", NULL
    };
    const char *exts[] = {"svg", "png", "xpm", NULL};
    char sizes[8][32];
    char root[KRY_FS_PATH_MAX];
    char home[KRY_FS_PATH_MAX];
    int size_count = 0;
    int i;
    int r;

    if(theme == NULL || theme[0] == '\0' || name == NULL || name[0] == '\0')
        return 0;
    if(size > 0)
        snprintf(sizes[size_count++], sizeof(sizes[0]), "%d", size);
    if(size > 0)
        snprintf(sizes[size_count++], sizeof(sizes[0]), "%dx%d", size, size);
    copy_text(sizes[size_count++], sizeof(sizes[0]), "48");
    copy_text(sizes[size_count++], sizeof(sizes[0]), "48x48");
    copy_text(sizes[size_count++], sizeof(sizes[0]), "64");
    copy_text(sizes[size_count++], sizeof(sizes[0]), "64x64");
    copy_text(sizes[size_count++], sizeof(sizes[0]), "scalable");

    for(r = 0; fixed_roots[r] != NULL; r++) {
        int s;

        for(s = 0; s < size_count; s++) {
            int c;

            for(c = 0; contexts[c] != NULL; c++) {
                int e;

                for(e = 0; exts[e] != NULL; e++) {
                    if(icon_candidate(out, cap, fixed_roots[r], theme,
                                             sizes[s], contexts[c], name,
                                             exts[e]))
                        return 1;
                    if(icon_candidate(out, cap, fixed_roots[r], theme,
                                             contexts[c], sizes[s], name,
                                             exts[e]))
                        return 1;
                }
            }
        }
    }
    if(!kry_fs_home_dir(home, sizeof(home)))
        return 0;
    for(r = 0; home_roots[r] != NULL; r++) {
        int s;

        snprintf(root, sizeof(root), home_roots[r], home);
        for(s = 0; s < size_count; s++) {
            int c;

            for(c = 0; contexts[c] != NULL; c++) {
                int e;

                for(e = 0; exts[e] != NULL; e++) {
                    if(icon_candidate(out, cap, root, theme, sizes[s],
                                             contexts[c], name, exts[e]))
                        return 1;
                    if(icon_candidate(out, cap, root, theme, contexts[c],
                                             sizes[s], name, exts[e]))
                        return 1;
                }
            }
        }
    }
    for(i = 0; i < cap; i++)
        out[i] = '\0';
    return 0;
}

int
kry_fs_find_icon(const char *name, int size, char *out, int cap)
{
    char theme[128];
    const char *fallbacks[] = {
        "Papirus-Dark", "Papirus", "Tela", "Adwaita", "hicolor", NULL
    };
    int i;

    if(out == NULL || cap <= 0)
        return 0;
    if(kry_fs_icon_theme(theme, sizeof(theme)) &&
       find_icon_in_theme(theme, name, size, out, cap))
        return 1;
    for(i = 0; fallbacks[i] != NULL; i++) {
        if(strcmp(fallbacks[i], theme) == 0)
            continue;
        if(find_icon_in_theme(fallbacks[i], name, size, out, cap))
            return 1;
    }
    out[0] = '\0';
    return 0;
}

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
    return kry_fs_list_dir_ex(dir, out, cap, 0);
}

int
kry_fs_list_dir_ex(const char *dir, KryDirEntry *out, int cap,
                   int include_hidden)
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

        if(!include_hidden && db[i].name[0] == '.')
            continue;   /* skip dot files, including . and .. */
        if(strcmp(db[i].name, ".") == 0 || strcmp(db[i].name, "..") == 0)
            continue;
        len = strlen(db[i].name);
        if(len >= KRY_FS_NAME_MAX)
            continue;
        memcpy(out[count].name, db[i].name, len + 1);
        out[count].is_dir = (db[i].mode & DMDIR) != 0;
        out[count].mtime = (long)db[i].mtime;
        out[count].size = (unsigned long long)db[i].length;
        out[count].readable = 1;
        out[count].hidden = db[i].name[0] == '.';
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
    out->size = 0;
    out->readable = 0;
    d = dirstat(path);
    if(d == NULL)
        return 1;   /* probe succeeded; path just doesnt exist */
    out->exists = 1;
    out->is_dir = (d->mode & DMDIR) != 0;
    out->mtime = (long)d->mtime;
    out->size = (unsigned long long)d->length;
    out->readable = 1;
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

int
kry_fs_create_file(const char *path)
{
    int fd;

    if(path == NULL)
        return -1;
    fd = create((char *)path, OWRITE, 0666);
    if(fd < 0)
        return -1;
    close(fd);
    return 0;
}

int
kry_fs_create_dir(const char *path)
{
    int fd;

    if(path == NULL)
        return -1;
    fd = create((char *)path, OREAD, DMDIR | 0777);
    if(fd < 0)
        return -1;
    close(fd);
    return 0;
}

int kry_fs_copy_recursive(const char *src, const char *dst)
{
    (void)src; (void)dst; return -1;
}

int kry_fs_move(const char *src, const char *dst)
{
    if(src == NULL || dst == NULL)
        return -1;
    return rename(src, dst);
}

int kry_fs_remove_recursive(const char *path)
{
    return path != NULL && remove(path) == 0 ? 0 : -1;
}

int kry_fs_symlink(const char *target, const char *link_path)
{
    (void)target; (void)link_path; return -1;
}

#elif !defined(_WIN32)
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
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
    return kry_fs_list_dir_ex(dir, out, cap, 0);
}

int
kry_fs_list_dir_ex(const char *dir, KryDirEntry *out, int cap,
                   int include_hidden)
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
        if(!include_hidden && name[0] == '.')
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
        {
            char full[4096];
            struct stat st;

            snprintf(full, sizeof(full), "%s/%s", dir, name);
            if(stat(full, &st) == 0) {
                out[count].mtime = (long)st.st_mtime;
                out[count].size = (unsigned long long)st.st_size;
                out[count].readable = access(full, R_OK) == 0;
            }
        }
        out[count].hidden = name[0] == '.';
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
    out->size = 0;
    out->readable = 0;
    if(stat(path, &st) != 0)
        return 1;   /* stat-able call; path just doesnt exist */
    out->exists = 1;
    out->is_dir = S_ISDIR(st.st_mode);
    out->mtime = (long)st.st_mtime;
    out->size = (unsigned long long)st.st_size;
    out->readable = access(path, R_OK) == 0;
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

int
kry_fs_create_file(const char *path)
{
    int fd;

    if(path == NULL)
        return -1;
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if(fd < 0)
        return -1;
    return close(fd) == 0 ? 0 : -1;
}

int
kry_fs_create_dir(const char *path)
{
    if(path == NULL)
        return -1;
    return mkdir(path, 0755) == 0 ? 0 : -1;
}

static int
kry_fs_copy_file_bytes(const char *src, const char *dst, mode_t mode)
{
    int in_fd;
    int out_fd;
    char buf[65536];
    ssize_t n;
    int ok = 1;

    in_fd = open(src, O_RDONLY);
    if(in_fd < 0)
        return -1;
    out_fd = open(dst, O_WRONLY | O_CREAT | O_EXCL, mode & 0777);
    if(out_fd < 0) {
        close(in_fd);
        return -1;
    }
    while((n = read(in_fd, buf, sizeof(buf))) > 0) {
        char *p = buf;

        while(n > 0) {
            ssize_t written = write(out_fd, p, (size_t)n);

            if(written < 0) {
                ok = 0;
                break;
            }
            p += written;
            n -= written;
        }
        if(!ok)
            break;
    }
    if(n < 0)
        ok = 0;
    if(close(out_fd) != 0)
        ok = 0;
    close(in_fd);
    if(!ok) {
        unlink(dst);
        return -1;
    }
    return 0;
}

int
kry_fs_copy_recursive(const char *src, const char *dst)
{
    struct stat st;

    if(src == NULL || dst == NULL || lstat(src, &st) != 0)
        return -1;
    if(S_ISDIR(st.st_mode)) {
        DIR *dir;
        struct dirent *de;

        if(mkdir(dst, st.st_mode & 0777) != 0)
            return -1;
        dir = opendir(src);
        if(dir == NULL)
            return -1;
        while((de = readdir(dir)) != NULL) {
            char child_src[4096];
            char child_dst[4096];

            if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;
            snprintf(child_src, sizeof(child_src), "%s/%s", src, de->d_name);
            snprintf(child_dst, sizeof(child_dst), "%s/%s", dst, de->d_name);
            if(kry_fs_copy_recursive(child_src, child_dst) != 0) {
                closedir(dir);
                return -1;
            }
        }
        closedir(dir);
        return 0;
    }
    if(S_ISLNK(st.st_mode)) {
        char target[4096];
        ssize_t len = readlink(src, target, sizeof(target) - 1);

        if(len < 0)
            return -1;
        target[len] = '\0';
        return symlink(target, dst) == 0 ? 0 : -1;
    }
    if(S_ISREG(st.st_mode))
        return kry_fs_copy_file_bytes(src, dst, st.st_mode);
    errno = ENOTSUP;
    return -1;
}

int
kry_fs_remove_recursive(const char *path)
{
    struct stat st;

    if(path == NULL || lstat(path, &st) != 0)
        return -1;
    if(S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        struct dirent *de;

        if(dir == NULL)
            return -1;
        while((de = readdir(dir)) != NULL) {
            char child[4096];

            if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;
            snprintf(child, sizeof(child), "%s/%s", path, de->d_name);
            if(kry_fs_remove_recursive(child) != 0) {
                closedir(dir);
                return -1;
            }
        }
        closedir(dir);
        return rmdir(path) == 0 ? 0 : -1;
    }
    return unlink(path) == 0 ? 0 : -1;
}

int
kry_fs_move(const char *src, const char *dst)
{
    if(src == NULL || dst == NULL)
        return -1;
    if(rename(src, dst) == 0)
        return 0;
    if(errno != EXDEV)
        return -1;
    if(kry_fs_copy_recursive(src, dst) != 0)
        return -1;
    return kry_fs_remove_recursive(src);
}

int
kry_fs_symlink(const char *target, const char *link_path)
{
    if(target == NULL || link_path == NULL)
        return -1;
    return symlink(target, link_path) == 0 ? 0 : -1;
}

#else  /* _WIN32 */

int kry_fs_list_dir(const char *dir, KryDirEntry *out, int cap)
{
    (void)dir; (void)out; (void)cap; return 0;
}
int kry_fs_list_dir_ex(const char *dir, KryDirEntry *out, int cap,
                       int include_hidden)
{
    (void)dir; (void)out; (void)cap; (void)include_hidden; return 0;
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
int kry_fs_create_file(const char *path) { (void)path; return -1; }
int kry_fs_create_dir(const char *path) { (void)path; return -1; }
int kry_fs_copy_recursive(const char *src, const char *dst)
{
    (void)src; (void)dst; return -1;
}
int kry_fs_move(const char *src, const char *dst)
{
    (void)src; (void)dst; return -1;
}
int kry_fs_remove_recursive(const char *path) { (void)path; return -1; }
int kry_fs_symlink(const char *target, const char *link_path)
{
    (void)target; (void)link_path; return -1;
}

#endif /* _WIN32 */
