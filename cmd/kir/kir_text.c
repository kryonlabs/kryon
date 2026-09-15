#include "kir_text.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

int
kir_is_ident_char(int c)
{
    return isalnum((unsigned char)c) || c == '_';
}

const char *
kir_skip_ws(const char *s)
{
    while(s != NULL && isspace((unsigned char)*s))
        s++;
    return s;
}

const char *
kir_skip_inline_ws(const char *s)
{
    while(s != NULL && (*s == ' ' || *s == '\t'))
        s++;
    return s;
}

char *
kir_trim(char *s)
{
    char *e;

    if(s == NULL)
        return NULL;
    while(isspace((unsigned char)*s))
        s++;
    e = s + strlen(s);
    while(e > s && isspace((unsigned char)e[-1]))
        *--e = '\0';
    return s;
}

char *
kir_trim_in_place(char *s)
{
    char *p;

    if(s == NULL)
        return NULL;
    p = kir_trim(s);
    if(p != s)
        memmove(s, p, strlen(p) + 1);
    return s;
}

void
kir_strip_block_brace(char *s)
{
    size_t n;

    if(s == NULL)
        return;
    n = strlen(s);
    while(n > 0 && isspace((unsigned char)s[n - 1]))
        n--;
    if(n > 0 && s[n - 1] == '{')
        n--;
    while(n > 0 && isspace((unsigned char)s[n - 1]))
        n--;
    s[n] = '\0';
}

void
kir_camel_ident(const char *s, char *dst, size_t dst_size)
{
    size_t n = 0;
    int up = 1;

    if(dst_size == 0)
        return;
    for(const char *p = s; p != NULL && *p != '\0' && n + 1 < dst_size; p++) {
        if(isalnum((unsigned char)*p)) {
            dst[n++] = up ? (char)toupper((unsigned char)*p) : *p;
            up = 0;
        } else {
            up = 1;
        }
    }
    if(n == 0 && n + 1 < dst_size)
        dst[n++] = 'X';
    if(n > 0 && isdigit((unsigned char)dst[0]) && n + 1 < dst_size) {
        memmove(dst + 1, dst, n + 1);
        dst[0] = 'M';
        n++;
    }
    dst[n] = '\0';
}

void
kir_go_field_ident(const char *s, char *dst, size_t dst_size)
{
    static const struct {
        const char *camel;
        const char *go;
    } initialisms[] = {
        {"Id", "ID"},
        {"FocusId", "FocusID"},
        {"MenuId", "MenuID"},
        {"SelectedId", "SelectedID"},
        {"ActivatedId", "ActivatedID"},
        {"CanonicalUrl", "CanonicalURL"},
    };

    if(dst_size == 0)
        return;
    kir_camel_ident(s, dst, dst_size);
    for(size_t i = 0; i < sizeof(initialisms) / sizeof(initialisms[0]); i++) {
        if(strcmp(dst, initialisms[i].camel) == 0) {
            snprintf(dst, dst_size, "%s", initialisms[i].go);
            return;
        }
    }
}

/* Escape s for safe inclusion inside a C string literal in generated code:
 * quotes, backslashes, and non-printable bytes cannot terminate or alter the
 * surrounding literal. Returns the number of chars written; dst always holds
 * a NUL-terminated prefix of the escaped text when dst_size > 0. */
size_t
kir_escape_c_string(const char *s, char *dst, size_t dst_size)
{
    size_t n = 0;

    if(dst_size == 0)
        return 0;
    for(const char *p = s != NULL ? s : ""; *p != '\0'; p++) {
        char buf[8];
        const char *rep = NULL;

        switch(*p) {
        case '"': rep = "\\\""; break;
        case '\\': rep = "\\\\"; break;
        case '\n': rep = "\\n"; break;
        case '\r': rep = "\\r"; break;
        case '\t': rep = "\\t"; break;
        default:
            if((unsigned char)*p < 0x20 || (unsigned char)*p == 0x7f) {
                snprintf(buf, sizeof(buf), "\\x%02x", (unsigned char)*p);
                rep = buf;
            }
            break;
        }
        if(rep == NULL) {
            buf[0] = *p;
            buf[1] = '\0';
            rep = buf;
        }
        for(const char *q = rep; *q != '\0'; q++) {
            if(n + 1 < dst_size)
                dst[n++] = *q;
        }
    }
    dst[n] = '\0';
    return n;
}

int
kir_split_top(const char *s, char *parts, int max, size_t part_size)
{
    int depth = 0;
    int n = 0;
    const char *start = s;

    if(s == NULL || parts == NULL || max <= 0 || part_size == 0)
        return 0;
    for(const char *p = s; ; p++) {
        if(*p == '"' || *p == '\'') {
            char q = *p++;

            while(*p != '\0' && *p != q) {
                if(*p == '\\' && p[1] != '\0')
                    p++;
                p++;
            }
        } else if(*p == '\0' || (*p == ',' && depth == 0)) {
            size_t len = (size_t)(p - start);

            if(n < max && len < part_size) {
                char *part = parts + ((size_t)n * part_size);

                memcpy(part, start, len);
                part[len] = '\0';
                kir_trim_in_place(part);
                n++;
            }
            if(*p == '\0')
                break;
            start = p + 1;
        } else if(*p == '(' || *p == '[' || *p == '{') {
            depth++;
        } else if(*p == ')' || *p == ']' || *p == '}') {
            if(depth > 0)
                depth--;
        }
    }
    return n;
}
