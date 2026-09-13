#include "kir_style_imports.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
dir_from_source(const char *src, char *dst, size_t dst_size)
{
    const char *slash;
    size_t n;

    if(dst_size == 0)
        return;
    slash = strrchr(src, '/');
    if(slash == NULL) {
        dst[0] = '\0';
        return;
    }
    n = (size_t)(slash - src);
    if(n >= dst_size)
        n = dst_size - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

int
KirStyleImportIsBuiltIn(const char *target)
{
    return strcmp(target, "material") == 0 ||
           strcmp(target, "tk") == 0 ||
           strcmp(target, "vanilla") == 0 ||
           strcmp(target, "glow") == 0 ||
           strcmp(target, "lightfield") == 0;
}

static int
style_package_path(const char *target, int dotted_dirs,
                   char *dst, size_t dst_size)
{
    char name[KIR_PATH_MAX];
    size_t n = 0;
    int last_sep = 1;

    if(target[0] == '\0' || target[0] == '/' || target[0] == '\\')
        return 0;
    for(const char *p = target; *p != '\0'; p++) {
        unsigned char ch = (unsigned char)*p;

        if(isalnum(ch) || ch == '_' || ch == '-') {
            if(n + 1 >= sizeof(name))
                return 0;
            name[n++] = (char)ch;
            last_sep = 0;
        } else if(ch == '.' || ch == '/') {
            if(last_sep || n + 1 >= sizeof(name))
                return 0;
            name[n++] = dotted_dirs || ch == '/' ? '/' : '.';
            last_sep = 1;
        } else {
            return 0;
        }
    }
    if(last_sep)
        return 0;
    name[n] = '\0';
    snprintf(dst, dst_size, "styles/%s.kss", name);
    return 1;
}

static char *
read_text_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    long size;
    char *text;

    if(f == NULL)
        return NULL;
    if(fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    size = ftell(f);
    if(size < 0) {
        fclose(f);
        return NULL;
    }
    if(fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    text = malloc((size_t)size + 1);
    if(text == NULL) {
        fclose(f);
        return NULL;
    }
    if(fread(text, 1, (size_t)size, f) != (size_t)size) {
        free(text);
        fclose(f);
        return NULL;
    }
    text[size] = '\0';
    fclose(f);
    return text;
}

static char *
trim_ascii_ws(char *s)
{
    char *end;

    while(*s != '\0' && isspace((unsigned char)*s))
        s++;
    end = s + strlen(s);
    while(end > s && isspace((unsigned char)end[-1]))
        *--end = '\0';
    return s;
}

static int
path_has_parent_segment(const char *path)
{
    const char *p = path;

    while(*p != '\0') {
        const char *start = p;
        size_t n;

        while(*p != '\0' && *p != '/')
            p++;
        n = (size_t)(p - start);
        if(n == 2 && start[0] == '.' && start[1] == '.')
            return 1;
        if(*p == '/')
            p++;
    }
    return 0;
}

static int
style_registry_path_is_safe(const char *path)
{
    return path[0] != '\0' && path[0] != '/' && path[0] != '\\' &&
           strchr(path, '\\') == NULL && !path_has_parent_segment(path);
}

static int
style_registry_package_path(const char *root, const char *target,
                            char *dst, size_t dst_size)
{
    char path[KIR_PATH_MAX * 4];
    char *text;
    char *line;

    snprintf(path, sizeof(path), "%s/styles/packages.kssmap", root);
    text = read_text_file(path);
    if(text == NULL)
        return 0;
    for(line = text; line != NULL && *line != '\0';) {
        char *next = strchr(line, '\n');
        char *comment;
        char *sep;
        char *key;
        char *value;

        if(next != NULL)
            *next++ = '\0';
        comment = strchr(line, '#');
        if(comment != NULL)
            *comment = '\0';
        key = trim_ascii_ws(line);
        if(*key == '\0') {
            line = next;
            continue;
        }
        sep = strchr(key, '=');
        if(sep == NULL) {
            for(sep = key; *sep != '\0' && !isspace((unsigned char)*sep);
                sep++) {}
        }
        if(*sep == '\0') {
            line = next;
            continue;
        }
        *sep++ = '\0';
        value = trim_ascii_ws(sep);
        key = trim_ascii_ws(key);
        if(strcmp(key, target) == 0 && style_registry_path_is_safe(value)) {
            snprintf(dst, dst_size, "%s", value);
            free(text);
            return 1;
        }
        line = next;
    }
    free(text);
    return 0;
}

char *
KirReadStyleImportSource(const KirModule *module, const char *root,
                         const KirStyleImport *style)
{
    char relative[KIR_PATH_MAX * 2];
    char dotted_relative[KIR_PATH_MAX * 2];
    char path[KIR_PATH_MAX * 4];
    char module_dir[KIR_PATH_MAX];
    char *text;

    if(style->kind == KIR_STYLE_IMPORT_BUILTIN) {
        if(KirStyleImportIsBuiltIn(style->target))
            return NULL;
        if(style_package_path(style->target, 0, relative, sizeof(relative))) {
            snprintf(path, sizeof(path), "%s/%s", root, relative);
            text = read_text_file(path);
            if(text != NULL)
                return text;
        }
        if(style_package_path(style->target, 1,
                              dotted_relative, sizeof(dotted_relative)) &&
           strcmp(relative, dotted_relative) != 0) {
            snprintf(path, sizeof(path), "%s/%s", root, dotted_relative);
            text = read_text_file(path);
            if(text != NULL)
                return text;
        }
        if(style_registry_package_path(root, style->target,
                                       relative, sizeof(relative))) {
            snprintf(path, sizeof(path), "%s/%s", root, relative);
            text = read_text_file(path);
            if(text != NULL)
                return text;
        }
        return NULL;
    }
    if(style->target[0] == '/')
        return read_text_file(style->target);
    dir_from_source(module->source_path, module_dir, sizeof(module_dir));
    if(module_dir[0] != '\0')
        snprintf(path, sizeof(path), "%s/%s/%s", root, module_dir, style->target);
    else
        snprintf(path, sizeof(path), "%s/%s", root, style->target);
    text = read_text_file(path);
    if(text != NULL)
        return text;
    snprintf(path, sizeof(path), "%s/%s", root, style->target);
    return read_text_file(path);
}
