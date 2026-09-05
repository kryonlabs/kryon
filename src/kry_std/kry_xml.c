/*
 * kry_xml.c - XML parser and emitter for the Kry standard library.
 *
 * Single-pass DOM builder over a NUL-terminated buffer. Nodes carry flat
 * parallel attribute arrays like kry_json; text is accumulated across
 * entity references and CDATA sections. Nesting depth is capped to keep
 * hostile input off the C stack.
 */
#include "kry_xml.h"

#include <stdlib.h>
#include <string.h>

#define KRY_XML_DEPTH_MAX 64

typedef struct {
    const char *p;
    int depth;
    int failed;
} Parser;

/* --- node helpers ------------------------------------------------------- */

static KryXml *
node_new(const char *name, size_t name_len)
{
    KryXml *n = calloc(1, sizeof(*n));

    if(n == NULL)
        return NULL;
    n->name = malloc(name_len + 1);
    if(n->name == NULL) {
        free(n);
        return NULL;
    }
    memcpy(n->name, name, name_len);
    n->name[name_len] = '\0';
    n->text = NULL;
    return n;
}

void
kry_xml_free(KryXml *node)
{
    int i;

    if(node == NULL)
        return;
    for(i = 0; i < node->count; i++)
        kry_xml_free(node->children[i]);
    for(i = 0; i < node->attr_count; i++) {
        free(node->attrs[i]);
        free(node->attr_values[i]);
    }
    free(node->children);
    free(node->attrs);
    free(node->attr_values);
    free(node->text);
    free(node->name);
    free(node);
}

const char *
kry_xml_name(const KryXml *node)
{
    return node != NULL ? node->name : NULL;
}

const char *
kry_xml_name_local(const KryXml *node)
{
    const char *name = kry_xml_name(node);
    const char *colon;

    if(name == NULL)
        return NULL;
    colon = strchr(name, ':');
    return colon != NULL ? colon + 1 : name;
}

const char *
kry_xml_attr(const KryXml *node, const char *name)
{
    int i;

    if(node == NULL || name == NULL)
        return NULL;
    for(i = 0; i < node->attr_count; i++)
        if(strcmp(node->attrs[i], name) == 0)
            return node->attr_values[i];
    return NULL;
}

long
kry_xml_attr_long(const KryXml *node, const char *name, long fallback)
{
    const char *v = kry_xml_attr(node, name);
    char *end;
    long parsed;

    if(v == NULL || *v == '\0')
        return fallback;
    parsed = strtol(v, &end, 10);
    if(end == v)
        return fallback;
    return parsed;
}

const char *
kry_xml_text(const KryXml *node)
{
    return node != NULL && node->text != NULL ? node->text : "";
}

KryXml *
kry_xml_child(const KryXml *node, int index)
{
    if(node == NULL || index < 0 || index >= node->count)
        return NULL;
    return node->children[index];
}

KryXml *
kry_xml_find(const KryXml *node, const char *name)
{
    int i;

    if(node == NULL || name == NULL)
        return NULL;
    for(i = 0; i < node->count; i++)
        if(strcmp(node->children[i]->name, name) == 0)
            return node->children[i];
    return NULL;
}

static const char *
local_part(const char *name)
{
    const char *colon = strchr(name, ':');

    return colon != NULL ? colon + 1 : name;
}

KryXml *
kry_xml_find_local(const KryXml *node, const char *local_name)
{
    int i;

    if(node == NULL || local_name == NULL)
        return NULL;
    for(i = 0; i < node->count; i++)
        if(strcmp(local_part(node->children[i]->name), local_name) == 0)
            return node->children[i];
    return NULL;
}

KryXml *
kry_xml_find_deep(const KryXml *node, const char *local_name)
{
    KryXml *hit;
    int i;

    if(node == NULL || local_name == NULL)
        return NULL;
    for(i = 0; i < node->count; i++) {
        if(strcmp(local_part(node->children[i]->name), local_name) == 0)
            return node->children[i];
        hit = kry_xml_find_deep(node->children[i], local_name);
        if(hit != NULL)
            return hit;
    }
    return NULL;
}

/* --- parsing ------------------------------------------------------------ */

static void
skip_ws(const char **pp)
{
    while(**pp == ' ' || **pp == '\t' || **pp == '\n' || **pp == '\r')
        (*pp)++;
}

static int
is_name_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' ||
           c == ':' || (unsigned char)c >= 0x80;
}

/* Growable text accumulator shared by character data and attribute values. */
typedef struct {
    char *buf;
    unsigned long len;
    unsigned long cap;
} TextAcc;

static int
acc_push(TextAcc *a, char c)
{
    if(a->len + 1 >= a->cap) {
        unsigned long ncap = a->cap != 0 ? a->cap * 2 : 32;
        char *nbuf = realloc(a->buf, ncap);

        if(nbuf == NULL)
            return 0;
        a->buf = nbuf;
        a->cap = ncap;
    }
    a->buf[a->len++] = c;
    a->buf[a->len] = '\0';
    return 1;
}

static int
acc_utf8(TextAcc *a, unsigned long cp)
{
    if(cp < 0x80)
        return acc_push(a, (char)cp);
    if(cp < 0x800) {
        return acc_push(a, (char)(0xC0 | (cp >> 6))) &&
               acc_push(a, (char)(0x80 | (cp & 0x3F)));
    }
    if(cp < 0x10000) {
        return acc_push(a, (char)(0xE0 | (cp >> 12))) &&
               acc_push(a, (char)(0x80 | ((cp >> 6) & 0x3F))) &&
               acc_push(a, (char)(0x80 | (cp & 0x3F)));
    }
    return acc_push(a, (char)(0xF0 | (cp >> 18))) &&
           acc_push(a, (char)(0x80 | ((cp >> 12) & 0x3F))) &&
           acc_push(a, (char)(0x80 | ((cp >> 6) & 0x3F))) &&
           acc_push(a, (char)(0x80 | (cp & 0x3F)));
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

/* Decode one entity at p (which points at '&'). Advances *pp past the ';' on
 * success and appends the decoded value to the accumulator. */
static int
decode_entity(TextAcc *a, const char **pp)
{
    const char *p = *pp + 1;
    const char *semi = strchr(p, ';');
    unsigned long cp;
    int h;
    char c;

    if(semi == NULL || semi - p > 10)
        return 0;
    if(semi - p == 2 && p[0] == 'l' && p[1] == 't')
        c = '<';
    else if(semi - p == 2 && p[0] == 'g' && p[1] == 't')
        c = '>';
    else if(semi - p == 3 && p[0] == 'a' && p[1] == 'm' && p[2] == 'p')
        c = '&';
    else if(semi - p == 4 && p[0] == 'q' && p[1] == 'u' && p[2] == 'o' &&
            p[3] == 't')
        c = '"';
    else if(semi - p == 4 && p[0] == 'a' && p[1] == 'p' && p[2] == 'o' &&
            p[3] == 's')
        c = '\'';
    else if(p[0] == '#') {
        const char *d = p + 1;
        int radix = 10;

        if(*d == 'x' || *d == 'X') {
            radix = 16;
            d++;
        }
        if(d == semi)
            return 0;
        cp = 0;
        while(d < semi) {
            h = radix == 16 ? hex_digit(*d) : (*d >= '0' && *d <= '9')
                                                 ? *d - '0'
                                                 : -1;
            if(h < 0)
                return 0;
            cp = cp * (unsigned long)radix + (unsigned long)h;
            d++;
        }
        if(!acc_utf8(a, cp))
            return 0;
        *pp = semi + 1;
        return 1;
    } else
        return 0;
    if(!acc_push(a, c))
        return 0;
    *pp = semi + 1;
    return 1;
}

/* Read character data up to the next '<', decoding entities. */
static int
read_text(Parser *ps, TextAcc *a)
{
    const char *p = ps->p;

    while(*p != '\0' && *p != '<') {
        if(*p == '&') {
            if(!decode_entity(a, &p)) {
                /* tolerate a bare ampersand */
                if(!acc_push(a, *p++))
                    return 0;
                continue;
            }
            continue;
        }
        if(!acc_push(a, *p++))
            return 0;
    }
    ps->p = p;
    return *p == '<';
}

/* Skip a comment, CDATA section, processing instruction or DOCTYPE at '<'.
 * Returns 1 and advances past it; 0 on malformed input. For CDATA the raw
 * text is appended to the accumulator instead of skipping. */
static int
skip_markup(Parser *ps, TextAcc *a)
{
    const char *p = ps->p;

    /* p points at '<' */
    if(strncmp(p, "<!--", 4) == 0) {
        const char *end = strstr(p + 4, "-->");

        if(end == NULL)
            return 0;
        ps->p = end + 3;
        return 1;
    }
    if(strncmp(p, "<![CDATA[", 9) == 0) {
        const char *end = strstr(p + 9, "]]>");

        if(end == NULL)
            return 0;
        if(a != NULL) {
            const char *d = p + 9;

            while(d < end && acc_push(a, *d++))
                ;
            if(d != end)
                return 0;
        }
        ps->p = end + 3;
        return 1;
    }
    if(p[1] == '?') {
        const char *end = strstr(p + 2, "?>");

        if(end == NULL)
            return 0;
        ps->p = end + 2;
        return 1;
    }
    if(strncmp(p, "<!", 2) == 0) {
        const char *end = strstr(p + 2, ">");

        if(end == NULL)
            return 0;
        ps->p = end + 1;
        return 1;
    }
    return 0;
}

static int
add_child(KryXml *parent, KryXml *child)
{
    KryXml **grown = realloc(parent->children,
                             sizeof(*grown) *
                                 (unsigned long)(parent->count + 1));

    if(grown == NULL)
        return 0;
    parent->children = grown;
    parent->children[parent->count++] = child;
    return 1;
}

static int
add_attr(KryXml *node, char *name, char *value)
{
    char **ng = realloc(node->attrs,
                        sizeof(*ng) * (unsigned long)(node->attr_count + 1));
    char **vg = realloc(node->attr_values,
                        sizeof(*vg) *
                            (unsigned long)(node->attr_count + 1));

    if(ng == NULL || vg == NULL) {
        free(ng);
        free(vg);
        return 0;
    }
    node->attrs = ng;
    node->attr_values = vg;
    node->attrs[node->attr_count] = name;
    node->attr_values[node->attr_count] = value;
    node->attr_count++;
    return 1;
}

/* Parse one attribute into freshly allocated name and decoded value. */
static int
parse_attribute(Parser *ps, char **name_out, char **value_out)
{
    const char *p = ps->p;
    const char *start;
    TextAcc val;
    char quote;
    char *name;
    char *value;

    skip_ws(&p);
    if(!is_name_char(*p))
        return 0;
    start = p;
    while(is_name_char(*p))
        p++;
    name = malloc((size_t)(p - start) + 1);
    if(name == NULL)
        return 0;
    memcpy(name, start, (size_t)(p - start));
    name[p - start] = '\0';
    skip_ws(&p);
    if(*p != '=') {
        free(name);
        return 0;
    }
    p++;
    skip_ws(&p);
    quote = *p;
    if(quote != '"' && quote != '\'') {
        free(name);
        return 0;
    }
    p++;
    val.buf = NULL;
    val.len = 0;
    val.cap = 0;
    while(*p != '\0' && *p != quote) {
        if(*p == '&') {
            if(!decode_entity(&val, &p)) {
                if(!acc_push(&val, *p++))
                    break;
                continue;
            }
            continue;
        }
        if(*p == '<') {
            acc_push(&val, *p++); /* tolerate; producers escape it */
            continue;
        }
        if(!acc_push(&val, *p++))
            break;
    }
    if(*p != quote) {
        free(name);
        free(val.buf);
        return 0;
    }
    p++;
    value = val.buf != NULL ? val.buf : calloc(1, 1);
    if(value == NULL) {
        free(name);
        free(val.buf);
        return 0;
    }
    *name_out = name;
    *value_out = value;
    ps->p = p;
    return 1;
}

/* Parse an element after its '<'. Returns the filled node (caller attaches)
 * or NULL on malformed input / stack overflow. */
static KryXml *
parse_element(Parser *ps, KryXml *parent)
{
    const char *p;
    const char *start;
    KryXml *node;
    KryXml *child;
    TextAcc text;

    text.buf = NULL;
    text.len = 0;
    text.cap = 0;
    if(ps->depth >= KRY_XML_DEPTH_MAX)
        return NULL;
    ps->depth++;
    p = ps->p + 1; /* past '<' */
    if(!is_name_char(*p)) {
        ps->depth--;
        return NULL;
    }
    start = p;
    while(is_name_char(*p))
        p++;
    node = node_new(start, (size_t)(p - start));
    if(node == NULL) {
        ps->depth--;
        return NULL;
    }

    /* attributes (and xmlns noise) until '>' or '/>' */
    for(;;) {
        char *aname = NULL;
        char *avalue = NULL;

        skip_ws(&p);
        if(*p == '\0')
            goto fail;
        if(*p == '>') {
            p++;
            break;
        }
        if(*p == '/' && p[1] == '>') {
            p += 2;
            ps->p = p;
            ps->depth--;
            return node;
        }
        ps->p = p;
        if(!parse_attribute(ps, &aname, &avalue))
            goto fail;
        p = ps->p;
        if(!add_attr(node, aname, avalue)) {
            free(aname);
            free(avalue);
            goto fail;
        }
    }

    /* content */
    ps->p = p;
    for(;;) {
        if(*ps->p == '\0')
            goto fail;
        if(strncmp(ps->p, "</", 2) == 0)
            break;
        if(ps->p[0] == '<') {
            if(ps->p[1] == '!' || ps->p[1] == '?') {
                if(!skip_markup(ps, &text))
                    goto fail;
                continue;
            }
            if(ps->p[1] == '/') {
                break;
            }
            child = parse_element(ps, node);
            if(child == NULL)
                goto fail;
            if(!add_child(node, child))
                goto fail;
            continue;
        }
        if(!read_text(ps, &text))
            goto fail;
    }

    /* closing tag must match the opening name */
    p = ps->p + 2;
    {
        size_t nlen = strlen(node->name);

        if(strncmp(p, node->name, nlen) != 0 ||
           !(p[nlen] == '>' || p[nlen] == ' ' || p[nlen] == '\t' ||
             p[nlen] == '\n' || p[nlen] == '\r'))
            goto fail;
        p += nlen;
        skip_ws(&p);
        if(*p != '>')
            goto fail;
        p++;
    }

    node->text = text.buf != NULL ? text.buf : calloc(1, 1);
    if(node->text == NULL) {
        free(text.buf);
        goto fail;
    }
    ps->p = p;
    ps->depth--;
    if(parent == NULL && *ps->p != '\0') {
        /* trailing junk after the root element */
        const char *q = ps->p;

        while(*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')
            q++;
        if(*q != '\0')
            goto fail_after;
    }
    return node;

fail:
    free(text.buf);
fail_after:
    kry_xml_free(node);
    ps->depth--;
    ps->failed = 1;
    return NULL;
}

KryXml *
kry_xml_parse(const char *text)
{
    Parser ps;
    KryXml *root;

    if(text == NULL)
        return NULL;
    ps.p = text;
    ps.depth = 0;
    ps.failed = 0;

    skip_ws(&ps.p);
    while(*ps.p == '<' && (ps.p[1] == '?' || ps.p[1] == '!')) {
        if(!skip_markup(&ps, NULL))
            return NULL;
        skip_ws(&ps.p);
    }
    if(*ps.p != '<' || ps.p[1] == '/')
        return NULL;

    root = parse_element(&ps, NULL);
    if(root == NULL)
        return NULL;
    skip_ws(&ps.p);
    if(*ps.p != '\0') {
        kry_xml_free(root);
        return NULL;
    }
    return root;
}

/* --- emitter ------------------------------------------------------------ */

static void
buf_reserve(KryXmlBuf *b, unsigned long extra)
{
    if(b->len + extra + 1 <= b->cap)
        return;
    while(b->len + extra + 1 > b->cap)
        b->cap = b->cap != 0 ? b->cap * 2 : 256;
    b->buf = realloc(b->buf, b->cap);
}

void
kry_xml_buf_raw(KryXmlBuf *b, const char *text)
{
    unsigned long len;

    if(b == NULL || text == NULL)
        return;
    len = (unsigned long)strlen(text);
    buf_reserve(b, len);
    if(b->buf == NULL)
        return;
    memcpy(b->buf + b->len, text, len);
    b->len += len;
    b->buf[b->len] = '\0';
}

void
kry_xml_buf_esc(KryXmlBuf *b, const char *text)
{
    if(b == NULL || text == NULL)
        return;
    for(; *text != '\0'; text++) {
        const char *rep = NULL;

        switch(*text) {
        case '&':
            rep = "&amp;";
            break;
        case '<':
            rep = "&lt;";
            break;
        case '>':
            rep = "&gt;";
            break;
        case '"':
            rep = "&quot;";
            break;
        default:
            buf_reserve(b, 1);
            if(b->buf == NULL)
                return;
            b->buf[b->len++] = *text;
            b->buf[b->len] = '\0';
            continue;
        }
        kry_xml_buf_raw(b, rep);
    }
}

const char *
kry_xml_buf_finish(KryXmlBuf *b)
{
    if(b == NULL || b->buf == NULL)
        return "";
    b->buf[b->len] = '\0';
    return b->buf;
}

void
kry_xml_buf_free(KryXmlBuf *b)
{
    if(b == NULL)
        return;
    free(b->buf);
    b->buf = NULL;
    b->len = 0;
    b->cap = 0;
}
