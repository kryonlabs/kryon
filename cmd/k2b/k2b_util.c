/*
 * k2b util — small path/string helpers used by the codegen.
 */
#include "krb.h"
#include "kir.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

void
die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "k2b: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

void
path_join(char *dst, size_t dst_size, const char *a, const char *b)
{
    if(a == NULL || a[0] == '\0')
        snprintf(dst, dst_size, "%s", b);
    else if(a[strlen(a) - 1] == '/')
        snprintf(dst, dst_size, "%s%s", a, b);
    else
        snprintf(dst, dst_size, "%s/%s", a, b);
}

void
mkdir_parent(const char *path)
{
    char tmp[KIR_PATH_MAX];
    size_t i;

    snprintf(tmp, sizeof(tmp), "%s", path);
    for(i = 1; i < strlen(tmp); i++) {
        if(tmp[i] != '/')
            continue;
        tmp[i] = '\0';
        if(mkdir(tmp, 0755) != 0 && errno != EEXIST)
            die("%s: mkdir failed: %s", tmp, strerror(errno));
        tmp[i] = '/';
    }
}

void
header_guard(char *dst, size_t dst_size, const char *rel)
{
    size_t n = 0;

    if(dst_size > 4) {
        dst[n++] = 'K';
        dst[n++] = '2';
        dst[n++] = 'B';
        dst[n++] = '_';
    }
    for(const char *p = rel; *p != '\0' && n + 1 < dst_size; p++) {
        char c = *p;
        if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9'))
            dst[n++] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
        else
            dst[n++] = '_';
    }
    dst[n] = '\0';
}

void
strip_kry_ext(char *dst, size_t dst_size, const char *path)
{
    size_t len;

    if(path == NULL) {
        if(dst_size > 0)
            dst[0] = '\0';
        return;
    }
    len = strlen(path);
    if(len > 4 && strcmp(path + len - 4, ".kry") == 0)
        len -= 4;
    if(len >= dst_size)
        len = dst_size - 1;
    memcpy(dst, path, len);
    dst[len] = '\0';
}

void
replace_path_basename(char *dst, size_t dst_size, const char *path,
                      const char *base)
{
    const char *slash = NULL;
    size_t dir_len;

    if(dst_size == 0)
        return;
    for(const char *p = path; p != NULL && *p != '\0'; p++) {
        if(*p == '/' || *p == '\\')
            slash = p;
    }
    if(slash == NULL) {
        snprintf(dst, dst_size, "%s", base);
        return;
    }
    dir_len = (size_t)(slash - path + 1);
    if(dir_len >= dst_size)
        dir_len = dst_size - 1;
    memcpy(dst, path, dir_len);
    dst[dir_len] = '\0';
    snprintf(dst + dir_len, dst_size - dir_len, "%s", base);
}

/* Path relative to root. Handles the common cases (path under root, or path
 * already relative); does not fully normalize, which is enough for emitting
 * cartridge output paths next to their source. */
const char *
relative_path(const char *root, const char *path)
{
    static char rel[KIR_PATH_MAX];
    size_t n;

    if(root == NULL || root[0] == '\0')
        return path;
    n = strlen(root);
    if(strncmp(path, root, n) == 0 && path[n] == '/')
        snprintf(rel, sizeof(rel), "%s", path + n + 1);
    else
        snprintf(rel, sizeof(rel), "%s", path);
    return rel;
}

int
is_ident_text(const char *text)
{
    if(text == NULL || text[0] == '\0')
        return 0;
    if(!(isalpha((unsigned char)text[0]) || text[0] == '_'))
        return 0;
    for(const char *p = text + 1; *p != '\0'; p++) {
        if(!(isalnum((unsigned char)*p) || *p == '_'))
            return 0;
    }
    return 1;
}

void
c_string_literal(char *dst, size_t dst_size, const char *src)
{
    size_t n = 0;

    if(dst_size == 0)
        return;
    dst[n++] = '"';
    for(const char *p = src != NULL ? src : ""; *p != '\0' && n + 2 < dst_size; p++) {
        if(*p == '"' || *p == '\\') {
            if(n + 2 >= dst_size)
                break;
            dst[n++] = '\\';
            dst[n++] = *p;
        } else if(*p == '\n') {
            if(n + 2 >= dst_size)
                break;
            dst[n++] = '\\';
            dst[n++] = 'n';
        } else {
            dst[n++] = *p;
        }
    }
    if(n + 1 < dst_size)
        dst[n++] = '"';
    dst[n] = '\0';
}
