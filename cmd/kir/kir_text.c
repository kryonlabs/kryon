#include "kir_text.h"

#include <ctype.h>
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
