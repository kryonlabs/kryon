/*
 * kry_json.c - JSON parser and emitter for the Kry standard library.
 *
 * Recursive descent over a NUL-terminated buffer. Object member lists are
 * flat (parallel keys/items arrays) so lookups and iteration stay simple;
 * nesting depth is capped to keep hostile input off the C stack.
 */
#include "kry_json.h"
#include "../core/kry_alloc.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define KRY_JSON_DEPTH_MAX 64

typedef struct {
    const char *p;
    int depth;
} Parser;

static void
skip_ws(const char **pp)
{
    while(**pp == ' ' || **pp == '\t' || **pp == '\n' || **pp == '\r')
        (*pp)++;
}

static KryJson *
value_new(KryJsonType type)
{
    KryJson *v = calloc(1, sizeof(*v));

    if(v != NULL)
        v->type = type;
    return v;
}

void
kry_json_free(KryJson *v)
{
    int i;

    if(v == NULL)
        return;
    for(i = 0; i < v->count; i++) {
        kry_json_free(v->items[i]);
        if(v->keys != NULL)
            free(v->keys[i]);
    }
    free(v->items);
    free(v->keys);
    free(v->string);
    free(v);
}

static int
hex_digit(char c)
{
    if(c >= '0' && c <= '9')
        return c - '0';
    if(c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* Append one UTF-8 code point (from a \u escape surrogate pair or a raw
 * byte above 0x7F kept as-is by the caller). */
static void
append_utf8(char **dst, unsigned long cp)
{
    char *d = *dst;

    if(cp < 0x80) {
        *d++ = (char)cp;
    } else if(cp < 0x800) {
        *d++ = (char)(0xC0 | (cp >> 6));
        *d++ = (char)(0x80 | (cp & 0x3F));
    } else if(cp < 0x10000) {
        *d++ = (char)(0xE0 | (cp >> 12));
        *d++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *d++ = (char)(0x80 | (cp & 0x3F));
    } else {
        *d++ = (char)(0xF0 | (cp >> 18));
        *d++ = (char)(0x80 | ((cp >> 12) & 0x3F));
        *d++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *d++ = (char)(0x80 | (cp & 0x3F));
    }
    *dst = d;
}

/* Parse a JSON string literal at pp (which must point at "). Returns a
 * decoded mallocd string or NULL on error; advances pp past the closing
 * quote. */
static char *
parse_string_lit(const char **pp)
{
    const char *p = *pp + 1;
    /* A decoded string is never longer than the raw literal (escapes only
     * shrink it; \uXXXX yields at most 4 bytes from 6). */
    size_t out_cap;
    char *out;
    char *d;

    if(!kry_size_add(strlen(p), 1, &out_cap))
        return NULL;
    out = malloc(out_cap);
    if(out == NULL)
        return NULL;
    d = out;
    while(*p != '"') {
        if(*p == '\0' || (unsigned char)*p < 0x20)
            goto fail;
        if(*p == '\\') {
            p++;
            switch(*p) {
            case '"': *d++ = '"'; break;
            case '\\': *d++ = '\\'; break;
            case '/': *d++ = '/'; break;
            case 'b': *d++ = '\b'; break;
            case 'f': *d++ = '\f'; break;
            case 'n': *d++ = '\n'; break;
            case 'r': *d++ = '\r'; break;
            case 't': *d++ = '\t'; break;
            case 'u': {
                int h[4];
                unsigned long cp;
                int i;

                for(i = 0; i < 4; i++) {
                    h[i] = hex_digit(p[1 + i]);
                    if(h[i] < 0)
                        goto fail;
                }
                p += 4;
                cp = (unsigned long)h[0] << 12 | (unsigned long)h[1] << 8
                   | (unsigned long)h[2] << 4 | (unsigned long)h[3];
                /* surrogate pair: D800-DBFF followed by \uDC00-DFFF */
                if(cp >= 0xD800 && cp <= 0xDBFF && p[1] == '\\' && p[2] == 'u') {
                    int h2[4];
                    unsigned long lo;

                    for(i = 0; i < 4; i++) {
                        h2[i] = hex_digit(p[3 + i]);
                        if(h2[i] < 0)
                            goto fail;
                    }
                    lo = (unsigned long)h2[0] << 12 | (unsigned long)h2[1] << 8
                       | (unsigned long)h2[2] << 4 | (unsigned long)h2[3];
                    if(lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        p += 6;
                    }
                }
                append_utf8(&d, cp);
                break;
            }
            default:
                goto fail;
            }
            p++;
        } else {
            *d++ = *p++;
        }
    }
    *d = '\0';
    *pp = p + 1;
    return out;
fail:
    free(out);
    return NULL;
}

static int
list_push(KryJson *v, char *key, KryJson *item)
{
    size_t next_count;
    KryJson **items;
    char **keys = v->keys;

    if(v->count < 0 ||
       !kry_size_add((size_t)v->count, 1, &next_count) ||
       next_count > (size_t)INT_MAX)
        return 0;
    items = kry_realloc_array(v->items, next_count, sizeof(*items));
    if(items == NULL)
        return 0;
    v->items = items;
    if(v->type == KRY_JSON_OBJECT) {
        keys = kry_realloc_array(v->keys, next_count, sizeof(*keys));
        if(keys == NULL)
            return 0;
        v->keys = keys;
        v->keys[v->count] = key;
    }
    v->items[v->count] = item;
    v->count++;
    return 1;
}

static KryJson *
parse_value(Parser *ps);

static KryJson *
parse_object(Parser *ps)
{
    KryJson *v = value_new(KRY_JSON_OBJECT);

    if(v == NULL)
        return NULL;
    ps->p++;   /* { */
    skip_ws(&ps->p);
    if(*ps->p == '}') {
        ps->p++;
        return v;
    }
    for(;;) {
        char *key;
        KryJson *item;

        skip_ws(&ps->p);
        if(*ps->p != '"')
            goto fail;
        key = parse_string_lit(&ps->p);
        if(key == NULL)
            goto fail;
        skip_ws(&ps->p);
        if(*ps->p != ':') {
            free(key);
            goto fail;
        }
        ps->p++;
        item = parse_value(ps);
        if(item == NULL || !list_push(v, key, item)) {
            free(key);
            kry_json_free(item);
            goto fail;
        }
        skip_ws(&ps->p);
        if(*ps->p == ',') {
            ps->p++;
            continue;
        }
        if(*ps->p == '}') {
            ps->p++;
            return v;
        }
        goto fail;
    }
fail:
    kry_json_free(v);
    return NULL;
}

static KryJson *
parse_array(Parser *ps)
{
    KryJson *v = value_new(KRY_JSON_ARRAY);

    if(v == NULL)
        return NULL;
    ps->p++;   /* [ */
    skip_ws(&ps->p);
    if(*ps->p == ']') {
        ps->p++;
        return v;
    }
    for(;;) {
        KryJson *item = parse_value(ps);

        if(item == NULL || !list_push(v, NULL, item)) {
            kry_json_free(item);
            goto fail;
        }
        skip_ws(&ps->p);
        if(*ps->p == ',') {
            ps->p++;
            continue;
        }
        if(*ps->p == ']') {
            ps->p++;
            return v;
        }
        goto fail;
    }
fail:
    kry_json_free(v);
    return NULL;
}

static KryJson *
parse_value(Parser *ps)
{
    KryJson *v;

    skip_ws(&ps->p);
    if(ps->depth >= KRY_JSON_DEPTH_MAX)
        return NULL;
    switch(*ps->p) {
    case '{':
        ps->depth++;
        v = parse_object(ps);
        ps->depth--;
        return v;
    case '[':
        ps->depth++;
        v = parse_array(ps);
        ps->depth--;
        return v;
    case '"':
        v = value_new(KRY_JSON_STRING);
        if(v != NULL) {
            v->string = parse_string_lit(&ps->p);
            if(v->string == NULL) {
                kry_json_free(v);
                return NULL;
            }
        }
        return v;
    case 't':
        if(strncmp(ps->p, "true", 4) != 0)
            return NULL;
        ps->p += 4;
        v = value_new(KRY_JSON_BOOL);
        if(v != NULL)
            v->boolean = 1;
        return v;
    case 'f':
        if(strncmp(ps->p, "false", 5) != 0)
            return NULL;
        ps->p += 5;
        return value_new(KRY_JSON_BOOL);
    case 'n':
        if(strncmp(ps->p, "null", 4) != 0)
            return NULL;
        ps->p += 4;
        return value_new(KRY_JSON_NULL);
    default: {
        char *end;
        double d = strtod(ps->p, &end);

        if(end == ps->p)
            return NULL;
        ps->p = end;
        v = value_new(KRY_JSON_NUMBER);
        if(v != NULL)
            v->number = d;
        return v;
    }
    }
}

KryJson *
kry_json_parse(const char *text)
{
    Parser ps;
    KryJson *v;

    if(text == NULL)
        return NULL;
    ps.p = text;
    ps.depth = 0;
    v = parse_value(&ps);
    if(v == NULL)
        return NULL;
    skip_ws(&ps.p);
    if(*ps.p != '\0') {
        kry_json_free(v);
        return NULL;
    }
    return v;
}

KryJson *
kry_json_get(const KryJson *v, const char *key)
{
    int i;

    if(v == NULL || v->type != KRY_JSON_OBJECT)
        return NULL;
    for(i = 0; i < v->count; i++)
        if(strcmp(v->keys[i], key) == 0)
            return v->items[i];
    return NULL;
}

KryJson *
kry_json_at(const KryJson *v, int index)
{
    if(v == NULL || index < 0 || index >= v->count)
        return NULL;
    return v->items[index];
}

const char *
kry_json_key(const KryJson *v, int index)
{
    if(v == NULL || v->type != KRY_JSON_OBJECT || index < 0 ||
       index >= v->count)
        return NULL;
    return v->keys[index];
}

KryJsonType
kry_json_type(const KryJson *v)
{
    return v != NULL ? v->type : KRY_JSON_NULL;
}

const char *
kry_json_string(const KryJson *v)
{
    return v != NULL && v->type == KRY_JSON_STRING ? v->string : NULL;
}

double
kry_json_number(const KryJson *v)
{
    return v != NULL && v->type == KRY_JSON_NUMBER ? v->number : 0.0;
}

int
kry_json_bool(const KryJson *v)
{
    return v != NULL && v->type == KRY_JSON_BOOL ? v->boolean : 0;
}

int
kry_json_count(const KryJson *v)
{
    return v != NULL ? v->count : 0;
}

/* --- emitter ------------------------------------------------------------ */

static int
buf_reserve(KryJsonBuf *b, unsigned long need)
{
    size_t want;
    size_t cap;
    void *buf;

    if(!kry_size_add((size_t)b->len, (size_t)need, &want) ||
       !kry_size_add(want, 1, &want))
        return 0;
    cap = (size_t)b->cap;
    buf = b->buf;
    if(!kry_reserve_bytes_max(&buf, &cap, want, 128, (size_t)ULONG_MAX))
        return 0;
    b->buf = (char *)buf;
    b->cap = (unsigned long)cap;
    return 1;
}

void
kry_json_buf_raw(KryJsonBuf *b, const char *text)
{
    unsigned long n = strlen(text);

    if(!buf_reserve(b, n))
        return;
    memcpy(b->buf + b->len, text, n + 1);
    b->len += n;
}

void
kry_json_buf_str(KryJsonBuf *b, const char *text)
{
    const char *p;
    size_t len;
    size_t escaped_cap;

    len = strlen(text);
    if(!kry_size_mul(len, 6, &escaped_cap) ||
       !kry_size_add(escaped_cap, 2, &escaped_cap) ||
       escaped_cap > ULONG_MAX ||
       !buf_reserve(b, (unsigned long)escaped_cap))
        return;
    b->buf[b->len++] = '"';
    for(p = text; *p != '\0'; p++) {
        unsigned char c = (unsigned char)*p;

        switch(c) {
        case '"': kry_json_buf_raw(b, "\\\""); continue;
        case '\\': kry_json_buf_raw(b, "\\\\"); continue;
        case '\b': kry_json_buf_raw(b, "\\b"); continue;
        case '\f': kry_json_buf_raw(b, "\\f"); continue;
        case '\n': kry_json_buf_raw(b, "\\n"); continue;
        case '\r': kry_json_buf_raw(b, "\\r"); continue;
        case '\t': kry_json_buf_raw(b, "\\t"); continue;
        }
        if(c < 0x20) {
            char esc[8];

            snprintf(esc, sizeof(esc), "\\u%04x", c);
            kry_json_buf_raw(b, esc);
        } else {
            if(!buf_reserve(b, 1))
                return;
            b->buf[b->len++] = (char)c;
            b->buf[b->len] = '\0';
        }
    }
    if(!buf_reserve(b, 1))
        return;
    b->buf[b->len++] = '"';
    b->buf[b->len] = '\0';
}

void
kry_json_buf_num(KryJsonBuf *b, double value)
{
    char tmp[32];
    int n = snprintf(tmp, sizeof(tmp), "%g", value);

    if(n > 0)
        kry_json_buf_raw(b, tmp);
}

const char *
kry_json_buf_finish(KryJsonBuf *b)
{
    if(b->buf == NULL) {
        if(!buf_reserve(b, 0))
            return "";
        b->buf[0] = '\0';
    }
    return b->buf;
}

void
kry_json_buf_free(KryJsonBuf *b)
{
    free(b->buf);
    b->buf = NULL;
    b->len = 0;
    b->cap = 0;
}
