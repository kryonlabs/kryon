/* Pure leaf helpers shared across the kc transpiler: small string scanners,
 * path munging, and brace/indent analysis. None of these touch KryFile parse
 * state except validate_output_name/mkdir_parent, which report through die(). */
#include "kc_internal.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void
die(const char *fmt, ...);

char *
trim(char *s)
{
    char *end;

    while(*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        s++;
    end = s + strlen(s);
    while(end > s &&
          (end[-1] == ' ' || end[-1] == '\t' ||
           end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';
    return s;
}

int
starts_word(const char *s, const char *word)
{
    size_t n = strlen(word);

    return strncmp(s, word, n) == 0 &&
           (s[n] == '\0' || isspace((unsigned char)s[n]));
}

int
starts_statement_word(const char *s, const char *word)
{
    size_t n = strlen(word);
    const char *p;

    if(!starts_word(s, word))
        return 0;
    p = s + n;
    while(*p == ' ' || *p == '\t')
        p++;
    return *p != '=' && *p != ':' && *p != ',' && *p != '[';
}

int
starts_header_directive(const char *s, const char *word)
{
    size_t n = strlen(word);
    const char *p;

    if(!starts_word(s, word))
        return 0;
    p = s + n;
    while(*p == ' ' || *p == '\t')
        p++;
    return isalpha((unsigned char)*p) || *p == '_' || *p == '*';
}

int
parse_ident(char **sp, char *dst, size_t dst_size)
{
    char *s = *sp;
    size_t n = 0;

    while(*s == ' ' || *s == '\t')
        s++;
    if(!((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || *s == '_'))
        return 0;
    while((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') ||
          (*s >= '0' && *s <= '9') || *s == '_') {
        if(n + 1 < dst_size)
            dst[n++] = *s;
        s++;
    }
    dst[n] = '\0';
    *sp = s;
    return 1;
}

int
is_ident_text(const char *text)
{
    char tmp[KC_NAME_MAX];
    char *p;

    if(text == NULL || text[0] == '\0')
        return 0;
    snprintf(tmp, sizeof(tmp), "%s", text);
    p = tmp;
    if(!parse_ident(&p, tmp, sizeof(tmp)))
        return 0;
    p = trim(p);
    return p[0] == '\0';
}

int
line_is_goto_label(const char *line, char *label, size_t label_size)
{
    char tmp[KC_BODY_LINE_MAX];
    char name[KC_NAME_MAX];
    char *p;

    if(line == NULL || line[0] == '\0')
        return 0;
    snprintf(tmp, sizeof(tmp), "%s", line);
    p = tmp;
    if(!parse_ident(&p, name, sizeof(name)))
        return 0;
    p = trim(p);
    if(p[0] != ':' || p[1] != '\0')
        return 0;
    if(label != NULL && label_size > 0)
        snprintf(label, label_size, "%s", name);
    return 1;
}

int
parse_quoted(char **sp, char *dst, size_t dst_size)
{
    char *s = *sp;
    size_t n = 0;

    while(*s == ' ' || *s == '\t')
        s++;
    if(*s != '"')
        return 0;
    s++;
    while(*s != '\0' && *s != '"') {
        if(*s == '\\' && s[1] != '\0') {
            if(n + 1 < dst_size)
                dst[n++] = *s;
            s++;
        }
        if(n + 1 < dst_size)
            dst[n++] = *s;
        s++;
    }
    if(*s != '"')
        return 0;
    dst[n] = '\0';
    *sp = s + 1;
    return 1;
}

int
parse_c_header_token(char **sp, char *dst, size_t dst_size)
{
    char *s = *sp;
    char end;
    size_t n = 0;

    while(*s == ' ' || *s == '\t')
        s++;
    if(*s == '"') {
        if(!parse_quoted(&s, dst, dst_size))
            return 0;
        char quoted[KC_PATH_MAX];

        snprintf(quoted, sizeof(quoted), "\"%s\"", dst);
        snprintf(dst, dst_size, "%s", quoted);
        *sp = s;
        return 1;
    }
    if(*s != '<')
        return 0;
    end = '>';
    if(n + 1 < dst_size)
        dst[n++] = *s;
    s++;
    while(*s != '\0' && *s != end) {
        if(n + 1 < dst_size)
            dst[n++] = *s;
        s++;
    }
    if(*s != end)
        return 0;
    if(n + 1 < dst_size)
        dst[n++] = *s;
    dst[n] = '\0';
    *sp = s + 1;
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

void
module_symbol(char *dst, size_t dst_size, const char *module)
{
    size_t n = 0;

    if(dst_size == 0)
        return;
    for(const char *p = module != NULL ? module : ""; *p != '\0' && n + 1 < dst_size; p++) {
        if(*p == '.' || *p == '/' || *p == '\\')
            dst[n++] = '_';
        else
            dst[n++] = *p;
    }
    dst[n] = '\0';
}

void
module_header(char *dst, size_t dst_size, const char *module)
{
    char base[KC_PATH_MAX];
    size_t n = 0;

    strip_kry_ext(base, sizeof(base), module);
    if(dst_size == 0)
        return;
    for(const char *p = base; *p != '\0' && n + 1 < dst_size; p++) {
        if(*p == '.')
            dst[n++] = '/';
        else
            dst[n++] = *p;
    }
    if(n + 3 < dst_size) {
        dst[n++] = '.';
        dst[n++] = 'h';
    }
    dst[n] = '\0';
}

void
validate_output_name(KryFile *file, int line_no, const char *name)
{
    if(!is_ident_text(name))
        die("%s:%d: output name must be a plain identifier", file->path,
            line_no);
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

const char *
relative_path(const char *root, const char *path)
{
    static char rel[KC_PATH_MAX];
    char root_abs[KC_PATH_MAX];
    char path_abs[KC_PATH_MAX];
    size_t n;

    if(root == NULL || root[0] == '\0')
        return path;
    normalize_source_path(root_abs, sizeof(root_abs), root);
    normalize_source_path(path_abs, sizeof(path_abs), path);
    n = strlen(root_abs);
    if(strncmp(path_abs, root_abs, n) == 0 && path_abs[n] == '/') {
        snprintf(rel, sizeof(rel), "%s", path_abs + n + 1);
        return rel;
    }
    n = strlen(root);
    if(strncmp(path, root, n) == 0 && path[n] == '/')
        return path + n + 1;
    return path;
}

void
normalize_source_path(char *dst, size_t dst_size, const char *path)
{
    char absolute[KC_PATH_MAX];
    char normalized[KC_PATH_MAX];
    char *parts[KC_PATH_MAX / 2];
    int count = 0;
    char *save = NULL;
    char *part;
    size_t used = 0;

    if(dst_size == 0)
        return;
    dst[0] = '\0';
    if(path == NULL || path[0] == '\0')
        return;

    if(path[0] == '/') {
        snprintf(absolute, sizeof(absolute), "%s", path);
    } else {
        char cwd[KC_PATH_MAX];

        if(getcwd(cwd, sizeof(cwd)) == NULL)
            snprintf(cwd, sizeof(cwd), ".");
        snprintf(absolute, sizeof(absolute), "%s/%s", cwd, path);
    }

    for(part = strtok_r(absolute, "/", &save); part != NULL;
        part = strtok_r(NULL, "/", &save)) {
        if(strcmp(part, ".") == 0 || part[0] == '\0')
            continue;
        if(strcmp(part, "..") == 0) {
            if(count > 0)
                count--;
            continue;
        }
        if(count < (int)(sizeof(parts) / sizeof(parts[0])))
            parts[count++] = part;
    }

    normalized[used++] = '/';
    normalized[used] = '\0';
    for(int i = 0; i < count; i++) {
        size_t len = strlen(parts[i]);

        if(used > 1 && used + 1 < sizeof(normalized))
            normalized[used++] = '/';
        if(used + len >= sizeof(normalized))
            len = sizeof(normalized) - used - 1;
        memcpy(normalized + used, parts[i], len);
        used += len;
        normalized[used] = '\0';
        if(len < strlen(parts[i]))
            break;
    }
    snprintf(dst, dst_size, "%s", normalized);
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
mkdir_parent(const char *path)
{
    char tmp[KC_PATH_MAX];

    snprintf(tmp, sizeof(tmp), "%s", path);
    for(char *p = tmp + 1; *p != '\0'; p++) {
        if(*p != '/')
            continue;
        *p = '\0';
        if(mkdir(tmp, 0755) != 0 && errno != EEXIST)
            die("%s: mkdir failed: %s", tmp, strerror(errno));
        *p = '/';
    }
}

void
header_guard(char *dst, size_t dst_size, const char *rel)
{
    size_t n = 0;

    if(dst_size > 2) {
        dst[n++] = 'K';
        dst[n++] = '_';
    }
    for(const char *p = rel; *p != '\0' && n + 1 < dst_size; p++) {
        unsigned char ch = (unsigned char)*p;

        if((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))
            dst[n++] = (char)ch;
        else if(ch >= 'a' && ch <= 'z')
            dst[n++] = (char)(ch - 'a' + 'A');
        else
            dst[n++] = '_';
    }
    if(n + 7 < dst_size) {
        memcpy(dst + n, "_H", 3);
        n += 2;
    }
    dst[n] = '\0';
}

const char *
skip_indent(const char *line)
{
    while(*line == ' ' || *line == '\t')
        line++;
    return line;
}

int
brace_delta(const char *line)
{
    int delta = 0;

    if(line[0] == '#')
        return 0;
    for(const char *p = line; *p != '\0'; p++) {
        if(*p == '{')
            delta++;
        else if(*p == '}')
            delta--;
    }
    return delta;
}
