/* Emit a krb cartridge from Kir widget calls. Lowers a KirModule (shared
 * kir_parse.c frontend) to a .krb binary + C host. */
#include "kir.h"
#include "krb.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* k2b_util.c */
void die(const char *fmt, ...);
void path_join(char *dst, size_t dst_size, const char *a, const char *b);
void mkdir_parent(const char *path);
void header_guard(char *dst, size_t dst_size, const char *rel);
void strip_kry_ext(char *dst, size_t dst_size, const char *path);
void replace_path_basename(char *dst, size_t dst_size, const char *path,
                           const char *base);
const char *relative_path(const char *root, const char *path);
int is_ident_text(const char *text);
void c_string_literal(char *dst, size_t dst_size, const char *src);
void write_krb(const KirModule *m, const char *root, const char *out_dir,
               int no_main);

#define KRB_BUILD_NODE_MAX 256
#define KRB_BUILD_STR_MAX 8192
#define KRB_BUILD_IMPORT_MAX 32
#define KRB_BUILD_CTRL_MAX 128
#define KRB_BUILD_HANDLER_LINES 16
#define KRB_OUT_MAX 65536

typedef struct KrbUpdate {
    char path[KIR_NAME_MAX];
    int kind; /* 0 = set, 1 = add, 2 = sub */
    int val;
} KrbUpdate;

typedef struct KrbGuard {
    int start; /* node index range [start, end) the guard covers */
    int end;
    char path[KIR_NAME_MAX];
    int op; /* KRB_OP_EQ..GE */
    int val;
} KrbGuard;

typedef struct KrbBuildNode {
    char name[KIR_NAME_MAX];
    int type;
    int parent;
    unsigned flags;
    int bind_slot;
    int x;
    int y;
    int w;
    int h;
    unsigned color;
    char text[KIR_TEXT_MAX];
    int font_size;
    int style;
} KrbBuildNode;

typedef struct KrbHandler {
    char name[KIR_NAME_MAX];
    char body[KRB_BUILD_HANDLER_LINES][KIR_TEXT_MAX];
    int body_count;
} KrbHandler;

typedef struct KrbStateField {
    char name[KIR_NAME_MAX];
    char decl[KIR_TEXT_MAX];
    unsigned kind;
    unsigned size;
} KrbStateField;

typedef struct KrbBuildControl {
    unsigned char kind;
    unsigned short id;
    int min;
    int max;
    int step;
    char value[KIR_NAME_MAX];
    char label[KIR_TEXT_MAX];
} KrbBuildControl;

typedef struct KrbBuild {
    KrbBuildNode nodes[KRB_BUILD_NODE_MAX];
    int node_count;
    char strings[KRB_BUILD_STR_MAX];
    int string_used;
    char imports[KRB_BUILD_IMPORT_MAX][KIR_NAME_MAX];
    int import_count;
    KrbHandler handlers[KRB_BUILD_IMPORT_MAX];
    int handler_count;
    KrbStateField fields[32];
    int field_count;
    KrbBuildControl controls[KRB_BUILD_CTRL_MAX];
    int control_count;
    /* what the widget vocabulary could not express; reported per file so
     * a thin cartridge is explained instead of silently surprising */
    char dropped[16][64];
    int dropped_count[16];
    int dropped_kinds;
    /* v2 logic: simple state updates (path op= literal) collected from the
     * frame body, and if-guards over widget node ranges. */
    KrbUpdate updates[32];
    int update_count;
    KrbGuard guards[16];
    int guard_count;
} KrbBuild;

static void
wr_u16(unsigned char **p, unsigned v)
{
    (*p)[0] = (unsigned char)(v & 0xff);
    (*p)[1] = (unsigned char)((v >> 8) & 0xff);
    *p += 2;
}

static void
wr_i16(unsigned char **p, int v)
{
    wr_u16(p, (unsigned)(uint16_t)(int16_t)v);
}

static void
wr_u32(unsigned char **p, unsigned v)
{
    (*p)[0] = (unsigned char)(v & 0xff);
    (*p)[1] = (unsigned char)((v >> 8) & 0xff);
    (*p)[2] = (unsigned char)((v >> 16) & 0xff);
    (*p)[3] = (unsigned char)((v >> 24) & 0xff);
    *p += 4;
}

static const char *
skip_ws(const char *s)
{
    while(s != NULL && (*s == ' ' || *s == '\t'))
        s++;
    return s;
}

static int
starts_ident(const char *s, const char *word)
{
    size_t n = strlen(word);

    return strncmp(s, word, n) == 0 &&
           !((s[n] >= 'a' && s[n] <= 'z') || (s[n] >= 'A' && s[n] <= 'Z') ||
             (s[n] >= '0' && s[n] <= '9') || s[n] == '_');
}

static int
add_import(KrbBuild *b, const char *name)
{
    int i;

    if(name == NULL || name[0] == '\0' || b->import_count >= KRB_BUILD_IMPORT_MAX)
        return -1;
    for(i = 0; i < b->import_count; i++) {
        if(strcmp(b->imports[i], name) == 0)
            return i;
    }
    snprintf(b->imports[b->import_count], sizeof(b->imports[0]), "%s", name);
    return b->import_count++;
}

static void
slug_name(char *dst, size_t dst_size, const char *src)
{
    size_t n = 0;
    int last_us = 1;

    if(dst_size == 0)
        return;
    for(; src != NULL && *src != '\0' && n + 1 < dst_size; src++) {
        char c = *src;

        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9')) {
            if(c >= 'A' && c <= 'Z')
                c = (char)(c - 'A' + 'a');
            dst[n++] = c;
            last_us = 0;
        } else if(!last_us) {
            dst[n++] = '_';
            last_us = 1;
        }
    }
    while(n > 0 && dst[n - 1] == '_')
        n--;
    if(n == 0) {
        snprintf(dst, dst_size, "bind");
        return;
    }
    dst[n] = '\0';
}

static int
khex(int ch)
{
    if(ch >= '0' && ch <= '9')
        return ch - '0';
    if(ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if(ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

static int parse_color_ctor(const char *expr, unsigned *out);

static unsigned
parse_color(const char *expr)
{
    unsigned ctor;

    if(parse_color_ctor(expr, &ctor))
        return ctor;
    static const struct {
        const char *name;
        unsigned value;
    } named[] = {
        { "WHITE", 0xffffffffu }, { "RAYWHITE", 0xf5f5f5ffu },
        { "BLACK", 0x000000ffu }, { "BLANK", 0x00000000u },
        { "RED", 0xff0000ffu }, { "MAROON", 0x800000ffu },
        { "GREEN", 0x00ff00ffu }, { "LIME", 0x32cd32ffu },
        { "BLUE", 0x0000ffffu }, { "SKYBLUE", 0x87ceebffu },
        { "YELLOW", 0xffff00ffu }, { "GOLD", 0xffd700ffu },
        { "ORANGE", 0xff8c00ffu }, { "PINK", 0xffc0cbffu },
        { "MAGENTA", 0xff00ffffu }, { "PURPLE", 0x7f00ffffu },
        { "VIOLET", 0xee82eeffu }, { "BEIGE", 0xddbb99ffu },
        { "BROWN", 0x8b4513ffu }, { "LIGHTGRAY", 0xc8c8c8ffu },
        { "DARKGRAY", 0x505050ffu }, { "GRAY", 0x828282ffu },
        { "GREY", 0x828282ffu },
    };
    const char *p;
    size_t i;
    size_t nlen;

    expr = skip_ws(expr);
    /* Theme getters (matched as substrings, so they also resolve inside
     * DarkenUIColor/LightenUIColor wrappers). */
    if(strstr(expr, "GetThemeBackground") != NULL)
        return 0x80000000u;
    if(strstr(expr, "GetThemeText") != NULL)
        return 0x80000001u;
    if(strstr(expr, "GetThemeIcon") != NULL)
        return 0x80000002u;
    if(strstr(expr, "GetThemeSurface") != NULL)
        return 0x80000003u;
    if(strstr(expr, "GetThemeButton") != NULL)
        return 0x80000004u;
    /* Hex literal 0xRRGGBBAA or 0xRRGGBB (6 digits -> opaque). */
    p = strstr(expr, "0x");
    if(p == NULL)
        p = strstr(expr, "0X");
    if(p != NULL) {
        unsigned v = 0;
        int n = 0;

        p += 2;
        while(n < 8 && khex((unsigned char)p[n]) >= 0)
            n++;
        if(n == 8 || n == 6) {
            for(i = 0; i < (size_t)n; i++)
                v = (v << 4) | (unsigned)khex((unsigned char)p[i]);
            if(n == 6)
                v = (v << 8) | 0xffu;
            return v;
        }
    }
    /* Named raylib sentinels (whole-token match). */
    for(i = 0; i < sizeof(named) / sizeof(named[0]); i++) {
        nlen = strlen(named[i].name);
        if(strncmp(expr, named[i].name, nlen) == 0) {
            char c = expr[nlen];
            if(c == '\0' || c == ')' || c == ',' || isspace((unsigned char)c))
                return named[i].value;
        }
    }
    return 0x000000ffu;
}

static int
parse_coord(const char *expr, int *value, int *scaled)
{
    const char *p = skip_ws(expr);

    *scaled = 0;
    *value = 0;
    if(strncmp(p, "ScaleUIPx(", 10) == 0) {
        *scaled = 1;
        p += 10;
    }
    if(*p == '-' || (*p >= '0' && *p <= '9')) {
        *value = atoi(p);
        return 1;
    }
    return 0;
}

static int
font_size_of(const char *expr)
{
    const char *p = skip_ws(expr);

    if(strstr(p, "UI_TEXT_24") != NULL)
        return 24;
    if(strstr(p, "UI_TEXT_16") != NULL)
        return 16;
    if(strstr(p, "UI_TEXT_12") != NULL)
        return 12;
    if(strstr(p, "UI_TEXT_8") != NULL)
        return 8;
    if(*p >= '0' && *p <= '9')
        return atoi(p);
    return 16;
}

static int
button_style_of(const char *expr)
{
    if(strstr(expr, "UI_BUTTON_STYLE_DANGER") != NULL)
        return 2;
    if(strstr(expr, "UI_BUTTON_STYLE_SECONDARY") != NULL)
        return 1;
    return 0;
}

static int
split_args(const char *src, char parts[][KIR_TEXT_MAX], int max)
{
    int count = 0;
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    const char *start = src;

    for(const char *p = src; p != NULL; p++) {
        int at_end = *p == '\0';

        if(!at_end && in_string) {
            if(escaped)
                escaped = 0;
            else if(*p == '\\')
                escaped = 1;
            else if(*p == '"')
                in_string = 0;
            continue;
        }
        if(!at_end) {
            if(*p == '"') {
                in_string = 1;
                continue;
            }
            if(*p == '(' || *p == '{' || *p == '[')
                depth++;
            else if(*p == ')' || *p == '}' || *p == ']')
                depth--;
        }
        if((at_end || (*p == ',' && depth == 0)) && count < max) {
            size_t n = (size_t)(p - start);

            while(n > 0 && isspace((unsigned char)start[n - 1]))
                n--;
            if(n >= KIR_TEXT_MAX)
                n = KIR_TEXT_MAX - 1;
            memcpy(parts[count], start, n);
            parts[count][n] = '\0';
            count++;
            if(at_end)
                break;
            start = p + 1;
            while(*start == ' ' || *start == '\t')
                start++;
        }
        if(at_end)
            break;
    }
    return count;
}

static int
extract_string(const char *expr, char *dst, size_t dst_size)
{
    const char *p = strchr(expr, '"');
    size_t n = 0;

    if(p == NULL || dst_size == 0)
        return 0;
    p++;
    while(*p != '\0' && *p != '"' && n + 1 < dst_size) {
        if(*p == '\\' && p[1] != '\0') {
            p++;
            dst[n++] = *p++;
            continue;
        }
        dst[n++] = *p++;
    }
    dst[n] = '\0';
    return 1;
}

static KrbBuildNode *
add_node(KrbBuild *b, int type, const char *name)
{
    KrbBuildNode *n;

    if(b->node_count >= KRB_BUILD_NODE_MAX)
        return NULL;
    n = &b->nodes[b->node_count++];
    memset(n, 0, sizeof(*n));
    n->type = type;
    n->parent = -1;
    n->bind_slot = 0xffff;
    n->font_size = 16;
    snprintf(n->name, sizeof(n->name), "%s", name);
    return n;
}

static const char *
call_after_eq(const char *text)
{
    const char *p = skip_ws(text);

    /* Strip leading control keywords so 'if Button(...)' / 'else if ...' reach
     * the call, and 'x := Widget(...)' / 'x = Widget(...)' skip past the lhs by
     * locating the first identifier immediately followed by '('. */
    for(;;) {
        if(strncmp(p, "if", 2) == 0 && (p[2] == ' ' || p[2] == '\t')) {
            p = skip_ws(p + 2);
            continue;
        }
        if(strncmp(p, "else", 4) == 0 && (p[4] == ' ' || p[4] == '\t')) {
            p = skip_ws(p + 4);
            continue;
        }
        break;
    }
    for(; *p != '\0'; p++) {
        if(isalpha((unsigned char)*p) || *p == '_') {
            const char *e = p;

            while(isalnum((unsigned char)*e) || *e == '_')
                e++;
            if(*e == '(')
                return p;
            p = e - 1;
        }
    }
    return p;
}

static int
parse_background(KrbBuild *b, const char *call)
{
    const char *args = strchr(call, '(');
    KrbBuildNode *n;
    char name[32];

    if(args == NULL)
        return 0;
    snprintf(name, sizeof(name), "bg%d", b->node_count);
    n = add_node(b, 1, name);
    if(n == NULL)
        return 0;
    n->color = parse_color(args + 1);
    return 1;
}

static int
parse_text(KrbBuild *b, const char *call)
{
    const char *args = strchr(call, '(');
    char parts[8][KIR_TEXT_MAX];
    int count;
    KrbBuildNode *n;
    char name[32];
    int scaled;

    if(args == NULL)
        return 0;
    count = split_args(args + 1, parts, 8);
    if(count < 1)
        return 0;
    snprintf(name, sizeof(name), "text%d", b->node_count);
    if(strstr(parts[0], "TextFormat") != NULL) {
        const char *comma = strrchr(parts[0], ',');
        char ident[KIR_NAME_MAX];
        const char *q;
        size_t nident = 0;

        extract_string(parts[0], name, sizeof(name));
        snprintf(ident, sizeof(ident), "%s", name);
        if(comma != NULL) {
            q = skip_ws(comma + 1);
            while((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') ||
                  (*q >= '0' && *q <= '9') || *q == '_') {
                if(nident + 1 < sizeof(ident))
                    ident[nident++] = *q;
                q++;
            }
            ident[nident] = '\0';
            if(ident[0] != '\0')
                snprintf(name, sizeof(name), "%s", ident);
        }
        n = add_node(b, 2, name);
        if(n == NULL)
            return 0;
        extract_string(parts[0], n->text, sizeof(n->text));
    } else if(parts[0][0] != '"' && is_ident_text(skip_ws(parts[0]))) {
        char ident[KIR_NAME_MAX];

        snprintf(ident, sizeof(ident), "%s", skip_ws(parts[0]));
        {
            char *cut = ident;
            while((*cut >= 'a' && *cut <= 'z') || (*cut >= 'A' && *cut <= 'Z') ||
                  (*cut >= '0' && *cut <= '9') || *cut == '_')
                cut++;
            *cut = '\0';
        }
        n = add_node(b, 2, ident[0] != '\0' ? ident : name);
        if(n == NULL)
            return 0;
        snprintf(n->text, sizeof(n->text), "%%s");
    } else {
        n = add_node(b, 2, name);
        if(n == NULL)
            return 0;
        extract_string(parts[0], n->text, sizeof(n->text));
    }
    if(count > 1 && parse_coord(parts[1], &n->x, &scaled) && scaled)
        n->flags |= 1 << 2;
    if(count > 2 && parse_coord(parts[2], &n->y, &scaled) && scaled)
        n->flags |= 1 << 3;
    if(count > 3)
        n->font_size = font_size_of(parts[3]);
    if(count > 4)
        n->color = parse_color(parts[4]);
    else
        n->color = 0x80000001u;
    return 1;
}

/* "(Color){r, g, b, a}" literal -> packed RGBA; 0 if not that form. */
static int
parse_color_ctor(const char *expr, unsigned *out)
{
    const char *p = skip_ws(expr);
    unsigned comps[4];
    int i;

    if(strncmp(p, "(Color){", 8) != 0)
        return 0;
    p += 8;
    for(i = 0; i < 4; i++) {
        comps[i] = (unsigned)strtol(skip_ws(p), NULL, 0);
        p = strchr(p, ',');
        if(p != NULL)
            p++;
        else if(i != 3)
            return 0;
    }
    if(out != NULL)
        *out = (comps[0] << 24) | (comps[1] << 16) | (comps[2] << 8) |
               (comps[3] & 0xff);
    return 1;
}

/* DrawCircleV((Vector2){X, Y}, R, COLOR) -> CIRCLE node (center + radius). */
static int
parse_circle(KrbBuild *b, const char *call)
{
    char parts[10][KIR_TEXT_MAX];
    const char *args = strchr(call, '(');
    const char *inner;
    char *comma;
    char center[KIR_TEXT_MAX];
    size_t ilen;
    int scaled;
    KrbBuildNode *n;
    char name[32];
    unsigned color;

    if(args == NULL)
        return 0;
    if(split_args(args + 1, parts, 10) < 3)
        return 0;
    inner = skip_ws(parts[0]);
    if(strncmp(inner, "(Vector2){", 10) != 0)
        return 0;
    inner += 10;
    ilen = strlen(inner);
    while(ilen > 0 && (inner[ilen - 1] == '}' || inner[ilen - 1] == ' '))
        ilen--;
    if(ilen >= sizeof(center))
        ilen = sizeof(center) - 1;
    memcpy(center, inner, ilen);
    center[ilen] = '\0';
    comma = strchr(center, ',');
    if(comma == NULL)
        return 0;
    *comma = '\0';
    snprintf(name, sizeof(name), "circle%d", b->node_count);
    n = add_node(b, KRB_NODE_CIRCLE, name);
    if(n == NULL)
        return 0;
    if(parse_coord(center, &n->x, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_X;
    if(parse_coord(comma + 1, &n->y, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_Y;
    if(parse_coord(parts[1], &n->w, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_W;
    if(parse_color_ctor(parts[2], &color))
        n->color = color;
    else
        n->color = parse_color(parts[2]);
    return 1;
}

/* DrawRing((Vector2){X, Y}, INNER, OUTER, start, end, seg, COLOR) -> RING
 * node: w = outer radius, h = inner. Angles/segments are ignored; the
 * cartridge ring is a full annulus. */
static int
parse_ring(KrbBuild *b, const char *call)
{
    char parts[10][KIR_TEXT_MAX];
    const char *args = strchr(call, '(');
    KrbBuildNode *n;
    int scaled;
    unsigned color;
    char name[32];

    if(args == NULL)
        return 0;
    if(split_args(args + 1, parts, 10) < 7)
        return 0;
    snprintf(name, sizeof(name), "ring%d", b->node_count);
    n = add_node(b, KRB_NODE_RING, name);
    if(n == NULL)
        return 0;
    {
        const char *inner = skip_ws(parts[0]);
        char center[KIR_TEXT_MAX];
        char *comma;
        size_t ilen;

        if(strncmp(inner, "(Vector2){", 10) != 0)
            return 0;
        inner += 10;
        ilen = strlen(inner);
        while(ilen > 0 && (inner[ilen - 1] == '}' || inner[ilen - 1] == ' '))
            ilen--;
        if(ilen >= sizeof(center))
            ilen = sizeof(center) - 1;
        memcpy(center, inner, ilen);
        center[ilen] = '\0';
        comma = strchr(center, ',');
        if(comma == NULL)
            return 0;
        *comma = '\0';
        if(parse_coord(center, &n->x, &scaled) && scaled)
            n->flags |= KRB_FLAG_SCALE_X;
        if(parse_coord(comma + 1, &n->y, &scaled) && scaled)
            n->flags |= KRB_FLAG_SCALE_Y;
    }
    if(parse_coord(parts[1], &n->h, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_H; /* inner */
    if(parse_coord(parts[2], &n->w, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_W; /* outer */
    if(parse_color_ctor(parts[6], &color))
        n->color = color;
    else
        n->color = parse_color(parts[6]);
    return 1;
}

static int
parse_rect(KrbBuild *b, const char *call)
{
    const char *args = strchr(call, '(');
    char parts[8][KIR_TEXT_MAX];
    int count;
    KrbBuildNode *n;
    char name[32];
    int scaled;

    if(args == NULL)
        return 0;
    count = split_args(args + 1, parts, 8);
    snprintf(name, sizeof(name), "rect%d", b->node_count);
    n = add_node(b, 3, name);
    if(n == NULL)
        return 0;
    if(count > 0 && parse_coord(parts[0], &n->x, &scaled) && scaled)
        n->flags |= 1 << 2;
    if(count > 1 && parse_coord(parts[1], &n->y, &scaled) && scaled)
        n->flags |= 1 << 3;
    if(count > 2 && parse_coord(parts[2], &n->w, &scaled) && scaled)
        n->flags |= 1 << 4;
    if(count > 3 && parse_coord(parts[3], &n->h, &scaled) && scaled)
        n->flags |= 1 << 5;
    if(count > 4)
        n->color = parse_color(parts[4]);
    return 1;
}

static int
parse_button(KrbBuild *b, const char *call)
{
    const char *props = strstr(call, "ButtonProps");
    KrbBuildNode *n;
    char slug[KIR_NAME_MAX];
    const char *p;
    int scaled;

    if(props == NULL)
        props = call;
    n = add_node(b, 4, "button");
    if(n == NULL)
        return 0;
    p = strstr(props, ".label");
    if(p != NULL)
        extract_string(p, n->text, sizeof(n->text));
    p = strstr(props, ".style");
    if(p != NULL)
        n->style = button_style_of(p);
    p = strstr(props, ".bounds");
    if(p != NULL) {
        const char *brace = strchr(p, '{');
        char parts[4][KIR_TEXT_MAX];
        int count;

        if(brace != NULL) {
            count = split_args(brace + 1, parts, 4);
            if(count > 0 && parse_coord(parts[0], &n->x, &scaled) && scaled)
                n->flags |= 1 << 2;
            if(count > 1 && parse_coord(parts[1], &n->y, &scaled) && scaled)
                n->flags |= 1 << 3;
            if(count > 2 && parse_coord(parts[2], &n->w, &scaled) && scaled)
                n->flags |= 1 << 4;
            if(count > 3 && parse_coord(parts[3], &n->h, &scaled) && scaled)
                n->flags |= 1 << 5;
        }
    }
    slug_name(slug, sizeof(slug), n->text[0] != '\0' ? n->text : "button");
    snprintf(n->name, sizeof(n->name), "%s", slug);
    n->bind_slot = add_import(b, slug);
    n->color = 0x80000004u;
    n->font_size = 16;
    return 1;
}

static int try_widget(KrbBuild *b, const char *raw);

static void
add_handler_line(KrbHandler *h, const char *raw)
{
    const char *t = skip_ws(raw);

    if(t[0] == '\0' || t[0] == '{' || t[0] == '}')
        return;
    if(strncmp(t, "PushUIInspectSource(", 20) == 0 ||
       strncmp(t, "PopUIInspectSource();", 21) == 0)
        return;
    if(strncmp(t, "if(", 3) == 0 || strncmp(t, "if (", 4) == 0)
        return;
    if(h->body_count >= KRB_BUILD_HANDLER_LINES)
        return;
    {
        /* Raw .kry expression statements have no trailing ';' but the generated
         * host is C, so terminate them. Control/header lines are left alone. */
        size_t n = strlen(t);
        const char *semi = "";

        if(n > 0 && t[n - 1] != ';' && t[n - 1] != '{' &&
           t[n - 1] != '}' && t[n - 1] != ':')
            semi = ";";
        snprintf(h->body[h->body_count], sizeof(h->body[0]), "%s%s", t, semi);
    }
    h->body_count++;
}

static KrbHandler *
handler_for(KrbBuild *b, const char *name)
{
    int i;

    for(i = 0; i < b->handler_count; i++) {
        if(strcmp(b->handlers[i].name, name) == 0)
            return &b->handlers[i];
    }
    if(b->handler_count >= KRB_BUILD_IMPORT_MAX)
        return NULL;
    snprintf(b->handlers[b->handler_count].name,
             sizeof(b->handlers[0].name), "%s", name);
    b->handlers[b->handler_count].body_count = 0;
    return &b->handlers[b->handler_count++];
}

/* "<path> = N;" / "+=" / "-=" / "++" / "--" -> update record */
static int
parse_update(const char *text, char *path, int *kind, int *val)
{
    const char *p = skip_ws(text);
    const char *q;
    size_t plen;

    q = p;
    while(*q == '_' || (*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') ||
          (*q >= '0' && *q <= '9') || *q == '.')
        q++;
    plen = (size_t)(q - p);
    if(plen == 0 || plen >= KIR_NAME_MAX)
        return 0;
    memcpy(path, p, plen);
    path[plen] = '\0';
    q = skip_ws(q);
    if(strncmp(q, "++;", 3) == 0 ||
       (strncmp(q, "++", 2) == 0 && q[2] == '\0')) {
        *kind = 1;
        *val = 1;
        return 1;
    }
    if(strncmp(q, "--;", 3) == 0) {
        *kind = 2;
        *val = 1;
        return 1;
    }
    if(strncmp(q, "+=", 2) == 0 || strncmp(q, "-=", 2) == 0) {
        *kind = q[0] == '+' ? 1 : 2;
        *val = atoi(skip_ws(q + 2));
        return 1;
    }
    if(*q == '=' && q[1] != '=') {
        *kind = 0;
        *val = atoi(skip_ws(q + 1));
        return 1;
    }
    return 0;
}

/* "if (<path> <cmp> <int>)" -> guard fields. Only simple state-vs-literal
 * conditions compile to cartridge logic; anything else is ignored here. */
static int
parse_cond(const char *text, char *path, int *op, int *val)
{
    const char *p = strstr(text, "if");
    const char *q;
    const char *cmp;
    size_t plen;
    int oplen = 0;

    if(p == NULL || p != skip_ws(text))
        return 0;
    p = skip_ws(p + 2);
    if(*p != '(')
        return 0;
    p = skip_ws(p + 1);
    q = p;
    while(*q == '_' || (*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') ||
          (*q >= '0' && *q <= '9') || *q == '.')
        q++;
    plen = (size_t)(q - p);
    if(plen == 0 || plen >= KIR_NAME_MAX)
        return 0;
    cmp = skip_ws(q);
    if(strncmp(cmp, "==", 2) == 0) oplen = 2;
    else if(strncmp(cmp, "!=", 2) == 0) oplen = 2;
    else if(strncmp(cmp, "<=", 2) == 0) oplen = 2;
    else if(strncmp(cmp, ">=", 2) == 0) oplen = 2;
    else if(*cmp == '<') oplen = 1;
    else if(*cmp == '>') oplen = 1;
    else return 0;
    *op = cmp[0] == '=' ? KRB_OP_EQ : cmp[0] == '!' ? KRB_OP_NE :
          cmp[0] == '<' ? (oplen == 2 ? KRB_OP_LE : KRB_OP_LT) :
                          (oplen == 2 ? KRB_OP_GE : KRB_OP_GT);
    *val = atoi(skip_ws(cmp + oplen));
    memcpy(path, p, plen);
    path[plen] = '\0';
    return 1;
}

static void
collect_widgets(KrbBuild *b, const KirFunction *fn)
{
    int j;
    int depth = 0;
    int open_guard = -1;
    int guard_depth = 0;

    if(fn == NULL)
        return;
    for(j = 0; j < fn->stmt_count; j++) {
        const KirStmt *st = &fn->stmts[j];
        int before = b->node_count;
        int opens = st->kind == KIR_STMT_IF || st->kind == KIR_STMT_WHILE ||
                    st->kind == KIR_STMT_FOR || st->kind == KIR_STMT_SWITCH;

        if(st->kind == KIR_STMT_BLOCK_CLOSE) {
            depth--;
            if(open_guard >= 0 && depth < guard_depth) {
                b->guards[open_guard].end = b->node_count;
                open_guard = -1;
            }
        }
        if(opens && st->kind == KIR_STMT_IF && open_guard < 0 &&
           b->guard_count < 16) {
            KrbGuard *g = &b->guards[b->guard_count];

            if(parse_cond(st->text, g->path, &g->op, &g->val)) {
                g->start = b->node_count;
                g->end = b->node_count;
                /* the loop's own depth++ for this 'if' lands after this
                 * statement; the close brace must bring depth below it */
                guard_depth = depth + 1;
                open_guard = b->guard_count++;
            }
        }
        if(st->kind != KIR_STMT_IF && st->kind != KIR_STMT_WHILE &&
           st->kind != KIR_STMT_FOR && st->kind != KIR_STMT_BLOCK_CLOSE &&
           b->update_count < 32) {
            char upath[KIR_NAME_MAX];
            int ukind;
            int uval;

            if(parse_update(st->text, upath, &ukind, &uval)) {
                KrbUpdate *u = &b->updates[b->update_count++];

                snprintf(u->path, sizeof(u->path), "%s", upath);
                u->kind = ukind;
                u->val = uval;
                continue;
            }
        }
        if(try_widget(b, st->text)) {
            /* In raw .kry the button call and its 'if' are one statement
             * ('if Button(...) {'); capture the handler body that follows. */
            if(b->node_count > before &&
               b->nodes[b->node_count - 1].type == KRB_NODE_BUTTON &&
               st->kind == KIR_STMT_IF) {
                KrbHandler *h;
                int capdepth = depth;
                int d = capdepth;
                int k;

                h = handler_for(b, b->nodes[b->node_count - 1].name);
                if(h != NULL) {
                    for(k = j + 1; k < fn->stmt_count; k++) {
                        if(fn->stmts[k].kind == KIR_STMT_BLOCK_CLOSE) {
                            if(d <= capdepth)
                                break;
                            d--;
                        } else if(fn->stmts[k].kind == KIR_STMT_IF ||
                                  fn->stmts[k].kind == KIR_STMT_WHILE ||
                                  fn->stmts[k].kind == KIR_STMT_FOR ||
                                  fn->stmts[k].kind == KIR_STMT_SWITCH) {
                            d++;
                        }
                        add_handler_line(h, fn->stmts[k].text);
                    }
                }
            }
            if(opens)
                depth++;
            continue;
        }
        if(opens)
            depth++;
    }
}

static void
collect_state(KrbBuild *b, const KirModule *m)
{
    int i;

    for(i = 0; i < m->state_count; i++) {
        const KirStateField *sf = &m->state_fields[i];
        KrbStateField *f;
        KrbBuildNode *n;
        unsigned kind = 1; /* KRB_I32 */
        unsigned size = 4;

        if(strstr(sf->type, "char") != NULL) {
            const char *br = strchr(sf->type, '[');

            kind = 5; /* KRB_CSTR */
            size = br != NULL ? (unsigned)atoi(br + 1) : 64;
        }
        if(sf->name[0] == '\0' || b->field_count >= 32)
            continue;
        f = &b->fields[b->field_count++];
        snprintf(f->name, sizeof(f->name), "%s", sf->name);
        /* Convert .kry type order ('[N] char') to C ('char name[N]'). */
        {
            const char *t = sf->type;
            char base[KIR_NAME_MAX];
            char suffix[KIR_NAME_MAX];
            size_t sn = 0;
            const char *bp = t;

            suffix[0] = '\0';
            while(*bp == '[') {
                const char *cl = strchr(bp, ']');

                if(cl == NULL)
                    break;
                {
                    size_t l = (size_t)(cl - bp + 1);

                    if(sn + l + 1 < sizeof(suffix)) {
                        memcpy(suffix + sn, bp, l);
                        sn += l;
                        suffix[sn] = '\0';
                    }
                }
                bp = cl + 1;
                while(*bp == ' ' || *bp == '\t')
                    bp++;
            }
            snprintf(base, sizeof(base), "%s", bp);
            snprintf(f->decl, sizeof(f->decl), "static %s %s%s = %s;",
                     base, sf->name, suffix, sf->init[0] ? sf->init : "{0}");
        }
        f->kind = kind;
        f->size = size;
        n = add_node(b, 5, sf->name);
        if(n != NULL) {
            n->style = (int)kind;
            n->w = (int)size;
        }
    }
}

/* Fill n->x/y/w/h and the SCALE_* flag bits from the first {a,b,c,d}
 * Rectangle compound literal found in expr. */
static int
node_rect(KrbBuildNode *n, const char *expr)
{
    char body[KIR_TEXT_MAX];
    char parts[4][KIR_TEXT_MAX];
    const char *open = strchr(expr, '{');
    const char *close;
    int scaled;
    size_t len;

    if(open == NULL)
        return 0;
    close = strchr(open, '}');
    if(close == NULL || close <= open)
        return 0;
    len = (size_t)(close - open - 1);
    if(len >= sizeof(body))
        len = sizeof(body) - 1;
    memcpy(body, open + 1, len);
    body[len] = '\0';
    if(split_args(body, parts, 4) < 4)
        return 0;
    n->flags = 0;
    if(parse_coord(parts[0], &n->x, &scaled) && scaled) n->flags |= KRB_FLAG_SCALE_X;
    if(parse_coord(parts[1], &n->y, &scaled) && scaled) n->flags |= KRB_FLAG_SCALE_Y;
    if(parse_coord(parts[2], &n->w, &scaled) && scaled) n->flags |= KRB_FLAG_SCALE_W;
    if(parse_coord(parts[3], &n->h, &scaled) && scaled) n->flags |= KRB_FLAG_SCALE_H;
    return 1;
}

/* Separator((Rectangle){x,y,w,h}, vertical) -> a thin themed RECT rail. */
static int
parse_separator(KrbBuild *b, const char *call)
{
    KrbBuildNode *n;
    char name[32];

    snprintf(name, sizeof(name), "sep%d", b->node_count);
    n = add_node(b, KRB_NODE_RECT, name);
    if(n == NULL || !node_rect(n, call))
        return 0;
    n->color = KRB_COLOR_THEME | KRY_THEME_ICON;
    return 1;
}

/* Line(x1,y1,x2,y2,color) -> the axis-aligned bounding RECT (thickness 1). */
static int
parse_line(KrbBuild *b, const char *call)
{
    const char *args = strchr(call, '(');
    char parts[8][KIR_TEXT_MAX];
    char name[32];
    KrbBuildNode *n;
    int count, x1, y1, x2, y2, junk;

    if(args == NULL)
        return 0;
    count = split_args(args + 1, parts, 8);
    if(count < 5)
        return 0;
    snprintf(name, sizeof(name), "line%d", b->node_count);
    n = add_node(b, KRB_NODE_RECT, name);
    if(n == NULL)
        return 0;
    parse_coord(parts[0], &x1, &junk);
    parse_coord(parts[1], &y1, &junk);
    parse_coord(parts[2], &x2, &junk);
    parse_coord(parts[3], &y2, &junk);
    n->x = x1 < x2 ? x1 : x2;
    n->y = y1 < y2 ? y1 : y2;
    n->w = (x1 == x2) ? 1 : (x1 < x2 ? x2 - x1 : x1 - x2);
    n->h = (y1 == y2) ? 1 : (y1 < y2 ? y2 - y1 : y1 - y2);
    if(n->w == 0) n->w = 1;
    if(n->h == 0) n->h = 1;
    n->color = parse_color(parts[4]);
    return 1;
}

/* Bevel(x,y,w,h,light,dark) -> a RECT filled with the light face. */
static int
parse_bevel(KrbBuild *b, const char *call)
{
    const char *args = strchr(call, '(');
    char parts[8][KIR_TEXT_MAX];
    char name[32];
    KrbBuildNode *n;
    int count, scaled;

    if(args == NULL)
        return 0;
    count = split_args(args + 1, parts, 8);
    if(count < 5)
        return 0;
    snprintf(name, sizeof(name), "bevel%d", b->node_count);
    n = add_node(b, KRB_NODE_RECT, name);
    if(n == NULL)
        return 0;
    if(parse_coord(parts[0], &n->x, &scaled) && scaled) n->flags |= KRB_FLAG_SCALE_X;
    if(parse_coord(parts[1], &n->y, &scaled) && scaled) n->flags |= KRB_FLAG_SCALE_Y;
    if(parse_coord(parts[2], &n->w, &scaled) && scaled) n->flags |= KRB_FLAG_SCALE_W;
    if(parse_coord(parts[3], &n->h, &scaled) && scaled) n->flags |= KRB_FLAG_SCALE_H;
    n->color = parse_color(parts[4]);
    return 1;
}

/* TextInRect(text, Rectangle, font, color) -> a TEXT node at the rect origin. */
static int
parse_textinrect(KrbBuild *b, const char *call)
{
    const char *args = strchr(call, '(');
    char parts[8][KIR_TEXT_MAX];
    char name[32];
    KrbBuildNode *n;
    int count;

    if(args == NULL)
        return 0;
    count = split_args(args + 1, parts, 8);
    if(count < 2)
        return 0;
    snprintf(name, sizeof(name), "tir%d", b->node_count);
    n = add_node(b, KRB_NODE_TEXT, name);
    if(n == NULL)
        return 0;
    extract_string(parts[0], n->text, sizeof(n->text));
    node_rect(n, parts[1]);
    if(count > 2)
        n->font_size = font_size_of(parts[2]);
    if(count > 3)
        n->color = parse_color(parts[3]);
    else
        n->color = KRB_COLOR_THEME | KRY_THEME_TEXT;
    return 1;
}

static int
fit_of(const char *expr)
{
    if(strstr(expr, "CONTAIN") != NULL)
        return 1;
    if(strstr(expr, "COVER") != NULL)
        return 2;
    return 0;
}

/* Given a pointer to '{', return the matching '}' (depth-aware). Compound
 * literals nest braces (PictureProps holds Rectangle/Vector2), so strchr would
 * stop at the first inner '}'. */
static const char *
find_match_brace(const char *open)
{
    int depth = 0;

    for(; open != NULL && *open != '\0'; open++) {
        if(*open == '{')
            depth++;
        else if(*open == '}') {
            depth--;
            if(depth == 0)
                return open;
        }
    }
    return NULL;
}

/* Picture((PictureProps){asset_path, bounds, source, origin, rot, tint, fit})
 * -> a PICTURE node; text holds the asset path, style holds the UIPictureFit. */
static int
parse_picture(KrbBuild *b, const char *call)
{
    const char *p = strstr(call, "PictureProps");
    const char *open;
    const char *close;
    char body[KIR_TEXT_MAX];
    char parts[8][KIR_TEXT_MAX];
    char name[32];
    KrbBuildNode *n;
    size_t len;
    int count;

    if(p == NULL)
        return 0;
    open = strchr(p, '{');
    if(open == NULL)
        return 0;
    close = find_match_brace(open);
    if(close == NULL || close <= open)
        return 0;
    len = (size_t)(close - open - 1);
    if(len >= sizeof(body))
        len = sizeof(body) - 1;
    memcpy(body, open + 1, len);
    body[len] = '\0';
    count = split_args(body, parts, 8);
    if(count < 2)
        return 0;
    snprintf(name, sizeof(name), "pic%d", b->node_count);
    n = add_node(b, KRB_NODE_PICTURE, name);
    if(n == NULL)
        return 0;
    extract_string(parts[0], n->text, sizeof(n->text));   /* asset_path */
    node_rect(n, parts[1]);                                /* bounds */
    n->color = count > 5 ? parse_color(parts[5]) : 0xffffffffu;
    n->style = count > 6 ? fit_of(parts[6]) : 0;
    return 1;
}

/* "&flag" (or "& flag", with trailing junk) -> "flag". Empty if not an lvalue
 * reference, so callers can skip binding. */
static void
strip_amp(const char *expr, char *dst, size_t dst_size)
{
    size_t n = 0;

    if(dst_size == 0)
        return;
    expr = skip_ws(expr);
    if(*expr == '&')
        expr++;
    expr = skip_ws(expr);
    while(n + 1 < dst_size && ((*expr >= 'a' && *expr <= 'z') ||
           (*expr >= 'A' && *expr <= 'Z') || (*expr >= '0' && *expr <= '9') ||
           *expr == '_'))
        dst[n++] = *expr++;
    dst[n] = '\0';
}

/* Checkbox(id,x,y,label,&val) -> a CHECKBOX node bound to the state field in
 * &val (name = path), label in text, id in bind_slot. */
static int
parse_checkbox(KrbBuild *b, const char *call)
{
    const char *args = strchr(call, '(');
    char parts[8][KIR_TEXT_MAX];
    char path[KIR_NAME_MAX];
    KrbBuildNode *n;
    int count, scaled;

    if(args == NULL)
        return 0;
    count = split_args(args + 1, parts, 8);
    if(count < 5)
        return 0;
    strip_amp(parts[4], path, sizeof(path));
    if(path[0] == '\0')
        return 0;
    n = add_node(b, KRB_NODE_CHECKBOX, path);
    if(n == NULL)
        return 0;
    if(parse_coord(parts[1], &n->x, &scaled) && scaled) n->flags |= KRB_FLAG_SCALE_X;
    if(parse_coord(parts[2], &n->y, &scaled) && scaled) n->flags |= KRB_FLAG_SCALE_Y;
    n->w = 16;
    n->h = 16;
    n->flags |= KRB_FLAG_SCALE_W | KRB_FLAG_SCALE_H;
    extract_string(parts[3], n->text, sizeof(n->text));
    n->bind_slot = atoi(skip_ws(parts[0]));
    n->color = KRB_COLOR_THEME | KRY_THEME_TEXT;
    return 1;
}

/* Toggle(id,x,y,w,h,&val,off,on) -> a TOGGLE node bound to &val. */
static int
parse_toggle(KrbBuild *b, const char *call)
{
    const char *args = strchr(call, '(');
    char parts[8][KIR_TEXT_MAX];
    char path[KIR_NAME_MAX];
    KrbBuildNode *n;
    int count, scaled;

    if(args == NULL)
        return 0;
    count = split_args(args + 1, parts, 8);
    if(count < 6)
        return 0;
    strip_amp(parts[5], path, sizeof(path));
    if(path[0] == '\0')
        return 0;
    n = add_node(b, KRB_NODE_TOGGLE, path);
    if(n == NULL)
        return 0;
    if(parse_coord(parts[1], &n->x, &scaled) && scaled) n->flags |= KRB_FLAG_SCALE_X;
    if(parse_coord(parts[2], &n->y, &scaled) && scaled) n->flags |= KRB_FLAG_SCALE_Y;
    if(parse_coord(parts[3], &n->w, &scaled) && scaled) n->flags |= KRB_FLAG_SCALE_W;
    if(parse_coord(parts[4], &n->h, &scaled) && scaled) n->flags |= KRB_FLAG_SCALE_H;
    if(count > 6)
        extract_string(parts[6], n->text, sizeof(n->text));
    n->bind_slot = atoi(skip_ws(parts[0]));
    n->color = KRB_COLOR_THEME | KRY_THEME_SURFACE;
    return 1;
}

static int
coord_flag(const char *expr, int *val, unsigned *flags, unsigned flagbit)
{
    int scaled;

    if(parse_coord(expr, val, &scaled)) {
        if(scaled)
            *flags |= flagbit;
        return 1;
    }
    return 0;
}

/* Parse a (Rectangle){x,y,w,h} compound into out coords + SCALE flag bits. */
static int
parse_rect_fields(const char *expr, int *x, int *y, int *w, int *h,
                  unsigned *flags)
{
    char body[KIR_TEXT_MAX];
    char parts[4][KIR_TEXT_MAX];
    const char *open = strchr(expr, '{');
    const char *close;
    int scaled;
    size_t len;

    if(open == NULL)
        return 0;
    close = strchr(open, '}');
    if(close == NULL || close <= open)
        return 0;
    len = (size_t)(close - open - 1);
    if(len >= sizeof(body))
        len = sizeof(body) - 1;
    memcpy(body, open + 1, len);
    body[len] = '\0';
    if(split_args(body, parts, 4) < 4)
        return 0;
    *flags = 0;
    if(parse_coord(parts[0], x, &scaled) && scaled) *flags |= KRB_FLAG_SCALE_X;
    if(parse_coord(parts[1], y, &scaled) && scaled) *flags |= KRB_FLAG_SCALE_Y;
    if(parse_coord(parts[2], w, &scaled) && scaled) *flags |= KRB_FLAG_SCALE_W;
    if(parse_coord(parts[3], h, &scaled) && scaled) *flags |= KRB_FLAG_SCALE_H;
    return 1;
}

static KrbBuildControl *
add_control(KrbBuild *b, int kind)
{
    KrbBuildControl *c;

    if(b->control_count >= KRB_BUILD_CTRL_MAX)
        return NULL;
    c = &b->controls[b->control_count++];
    memset(c, 0, sizeof(*c));
    c->kind = (unsigned char)kind;
    return c;
}

/* Emit a CONTROL node bound to a freshly-added control record. The node's
 * bind_slot is the control index; draw_node reads the record for kind/args. */
static int
add_control_node(KrbBuild *b, int kind, const char *path, int x, int y,
                 int w, int h, unsigned flags, int id, int min, int max,
                 int step, const char *label)
{
    KrbBuildControl *c;
    KrbBuildNode *n;
    char name[32];

    c = add_control(b, kind);
    if(c == NULL)
        return 0;
    snprintf(c->value, sizeof(c->value), "%s", path);
    snprintf(c->label, sizeof(c->label), "%s", label == NULL ? "" : label);
    c->id = (unsigned short)id;
    c->min = min;
    c->max = max;
    c->step = step;
    snprintf(name, sizeof(name), "ctl%d", b->node_count);
    n = add_node(b, KRB_NODE_CONTROL, path);
    if(n == NULL)
        return 0;
    n->x = x;
    n->y = y;
    n->w = w;
    n->h = h;
    n->flags = flags;
    n->bind_slot = b->control_count - 1;
    return 1;
}

/* Slider(id,x,y,w,label,min,max,&val,...) -> horizontal range control. */
static int
parse_slider(KrbBuild *b, const char *call)
{
    char parts[12][KIR_TEXT_MAX];
    char path[KIR_NAME_MAX];
    char label[KIR_TEXT_MAX];
    const char *args = strchr(call, '(');
    int count, x = 0, y = 0, w = 0;
    unsigned flags = 0;

    if(args == NULL)
        return 0;
    count = split_args(args + 1, parts, 12);
    if(count < 8)
        return 0;
    strip_amp(parts[7], path, sizeof(path));
    if(path[0] == '\0')
        return 0;
    extract_string(parts[4], label, sizeof(label));
    coord_flag(parts[1], &x, &flags, KRB_FLAG_SCALE_X);
    coord_flag(parts[2], &y, &flags, KRB_FLAG_SCALE_Y);
    coord_flag(parts[3], &w, &flags, KRB_FLAG_SCALE_W);
    return add_control_node(b, KRB_CTRL_SLIDER, path, x, y, w, 16, flags,
                            atoi(skip_ws(parts[0])), atoi(skip_ws(parts[5])),
                            atoi(skip_ws(parts[6])), 1, label);
}

/* VerticalSlider(id,x,y,h,min,max,&val) -> vertical range control. */
static int
parse_vslider(KrbBuild *b, const char *call)
{
    char parts[12][KIR_TEXT_MAX];
    char path[KIR_NAME_MAX];
    const char *args = strchr(call, '(');
    int count, x = 0, y = 0, h = 0;
    unsigned flags = 0;

    if(args == NULL)
        return 0;
    count = split_args(args + 1, parts, 12);
    if(count < 7)
        return 0;
    strip_amp(parts[6], path, sizeof(path));
    if(path[0] == '\0')
        return 0;
    coord_flag(parts[1], &x, &flags, KRB_FLAG_SCALE_X);
    coord_flag(parts[2], &y, &flags, KRB_FLAG_SCALE_Y);
    coord_flag(parts[3], &h, &flags, KRB_FLAG_SCALE_H);
    return add_control_node(b, KRB_CTRL_VSLIDER, path, x, y, 16, h, flags,
                            atoi(skip_ws(parts[0])), atoi(skip_ws(parts[4])),
                            atoi(skip_ws(parts[5])), 1, "");
}

/* Spinbox((SpinboxProps){bounds,id,min,max,step,&val,disabled}) -> step control. */
static int
parse_spinbox(KrbBuild *b, const char *call)
{
    const char *p = strstr(call, "SpinboxProps");
    const char *open;
    const char *close;
    char body[KIR_TEXT_MAX];
    char parts[8][KIR_TEXT_MAX];
    char path[KIR_NAME_MAX];
    int x = 0, y = 0, w = 0, h = 0, count;
    unsigned flags = 0;
    size_t len;

    if(p == NULL)
        return 0;
    open = strchr(p, '{');
    if(open == NULL)
        return 0;
    close = find_match_brace(open);
    if(close == NULL || close <= open)
        return 0;
    len = (size_t)(close - open - 1);
    if(len >= sizeof(body))
        len = sizeof(body) - 1;
    memcpy(body, open + 1, len);
    body[len] = '\0';
    count = split_args(body, parts, 8);
    if(count < 6)
        return 0;
    strip_amp(parts[5], path, sizeof(path));
    if(path[0] == '\0')
        return 0;
    parse_rect_fields(parts[0], &x, &y, &w, &h, &flags);
    return add_control_node(b, KRB_CTRL_SPINBOX, path, x, y, w, h, flags,
                            atoi(skip_ws(parts[1])), atoi(skip_ws(parts[2])),
                            atoi(skip_ws(parts[3])), atoi(skip_ws(parts[4])), "");
}

static int
try_widget(KrbBuild *b, const char *raw)
{
    const char *text = skip_ws(raw);
    const char *call;

    if(strncmp(text, "PushUIInspectSource(", 20) == 0 ||
       strncmp(text, "PopUIInspectSource();", 21) == 0)
        return 0;
    call = call_after_eq(text);
    call = skip_ws(call);
    if(starts_ident(call, "Background"))
        return parse_background(b, call);
    if(starts_ident(call, "Text") && !starts_ident(call, "TextFormat") &&
       !starts_ident(call, "TextInRect") && !starts_ident(call, "TextLines") &&
       !starts_ident(call, "TextField") && !starts_ident(call, "TextInputControl") &&
       !starts_ident(call, "TextButton") && !starts_ident(call, "TextArea"))
        return parse_text(b, call);
    if(starts_ident(call, "Rect"))
        return parse_rect(b, call);
    if(starts_ident(call, "DrawCircleV"))
        return parse_circle(b, call);
    if(starts_ident(call, "DrawRing"))
        return parse_ring(b, call);
    if(strstr(call, "Button((") != NULL || strstr(call, "ButtonProps") != NULL)
        return parse_button(b, call);
    if(starts_ident(call, "Separator"))
        return parse_separator(b, call);
    if(starts_ident(call, "Line"))
        return parse_line(b, call);
    if(starts_ident(call, "Bevel"))
        return parse_bevel(b, call);
    if(starts_ident(call, "TextInRect"))
        return parse_textinrect(b, call);
    if(starts_ident(call, "Picture"))
        return parse_picture(b, call);
    if(starts_ident(call, "Checkbox"))
        return parse_checkbox(b, call);
    if(starts_ident(call, "Toggle"))
        return parse_toggle(b, call);
    if(starts_ident(call, "VerticalSlider"))
        return parse_vslider(b, call);
    if(starts_ident(call, "Slider"))
        return parse_slider(b, call);
    if(starts_ident(call, "Spinbox"))
        return parse_spinbox(b, call);
    {
        char name[64];
        const char *p = call;
        int i = 0;

        while(p[i] != '\0' && p[i] != '(' && i < (int)sizeof(name) - 1) {
            name[i] = p[i];
            i++;
        }
        name[i] = '\0';
        for(i = 0; i < b->dropped_kinds; i++)
            if(strcmp(b->dropped[i], name) == 0) {
                b->dropped_count[i]++;
                return 0;
            }
        if(b->dropped_kinds < 16 && name[0] != '\0') {
            snprintf(b->dropped[b->dropped_kinds], 64, "%s", name);
            b->dropped_count[b->dropped_kinds] = 1;
            b->dropped_kinds++;
        }
    }
    return 0;
}


/* Build the program section. Render-only cartridges stay one OP_DRAW_TREE;
 * updates/guards emit a v2 VM program: state updates first, then one
 * OP_DRAW_NODE per drawable node, with guarded ranges wrapped in
 * compare + JZ so the condition hides them. */
static int
emit_prog(KrbBuild *b, const unsigned char *dst, int cap,
          const int *upath_off, const int *gpath_off)
{
    unsigned char *q = (unsigned char *)dst;
    int i;
    int open_guard = -1;
    int jz_patch = -1;

#define EMIT1(v) do { if(q - dst >= cap) return -1; *q++ = (unsigned char)(v); } while(0)
#define EMIT2(v) do { if(q - dst + 2 > cap) return -1; wr_u16(&q, (unsigned)v); } while(0)
#define EMIT4(v) do { if(q - dst + 4 > cap) return -1; wr_u32(&q, (unsigned long)(v)); } while(0)

    if(b->update_count == 0 && b->guard_count == 0) {
        EMIT1(KRB_OP_DRAW_TREE);
        return (int)(q - dst);
    }
    for(i = 0; i < b->update_count; i++) {
        const KrbUpdate *u = &b->updates[i];

        if(u->kind == 0) {
            EMIT1(KRB_OP_PUSH_CONST);
            EMIT4(u->val);
        } else {
            EMIT1(KRB_OP_PUSH_PATH);
            EMIT2(upath_off[i]);
            EMIT1(KRB_OP_PUSH_CONST);
            EMIT4(u->val);
            EMIT1(u->kind == 1 ? KRB_OP_ADD : KRB_OP_SUB);
        }
        EMIT1(KRB_OP_POP_STORE);
        EMIT2(upath_off[i]);
    }
    for(i = 0; i < b->node_count; i++) {
        int g = -1;
        int k;

        if(b->nodes[i].type == KRB_NODE_DATA)
            continue;
        for(k = 0; k < b->guard_count; k++) {
            if(i >= b->guards[k].start && i < b->guards[k].end) {
                g = k;
                break;
            }
        }
        if(g != open_guard) {
            if(open_guard >= 0 && jz_patch >= 0) {
                unsigned char *pp = dst + jz_patch;

                wr_u32(&pp, (unsigned long)(q - dst));
                jz_patch = -1;
            }
            open_guard = g;
            if(g >= 0) {
                EMIT1(KRB_OP_PUSH_PATH);
                EMIT2(gpath_off[g]);
                EMIT1(KRB_OP_PUSH_CONST);
                EMIT4(b->guards[g].val);
                EMIT1(b->guards[g].op);
                EMIT1(KRB_OP_JZ);
                jz_patch = (int)(q - dst);
                EMIT4(0);
            }
        }
        EMIT1(KRB_OP_DRAW_NODE);
        EMIT2(i);
    }
    if(jz_patch >= 0) {
        unsigned char *pp = dst + jz_patch;

        wr_u32(&pp, (unsigned long)(q - dst));
    }
#undef EMIT1
#undef EMIT2
#undef EMIT4
    return (int)(q - dst);
}


static int
intern_string(KrbBuild *b, const char *s)
{
    int i;
    size_t n;

    if(s == NULL || s[0] == '\0')
        return 0;
    for(i = 1; i < b->string_used;) {
        if(strcmp(b->strings + i, s) == 0)
            return i;
        i += (int)strlen(b->strings + i) + 1;
    }
    n = strlen(s) + 1;
    if(b->string_used + (int)n > KRB_BUILD_STR_MAX)
        return 0;
    i = b->string_used;
    memcpy(b->strings + i, s, n);
    b->string_used += (int)n;
    return i;
}

static int
emit_krb_mem(unsigned char *dst, int cap, KrbBuild *b)
{
    unsigned char header[32];
    unsigned char *hp = header;
    unsigned char *out = dst;
    int i;
    int name_off[KRB_BUILD_NODE_MAX];
    int text_off[KRB_BUILD_NODE_MAX];
    int import_off[KRB_BUILD_IMPORT_MAX];
    int cvalue_off[KRB_BUILD_CTRL_MAX];
    int clabel_off[KRB_BUILD_CTRL_MAX];
    unsigned char prog_buf[4096];
    int prog_bytes = 1;
    int need;

    memset(header, 0, sizeof(header));
    b->strings[0] = '\0';
    b->string_used = 1;
    for(i = 0; i < b->node_count; i++) {
        name_off[i] = intern_string(b, b->nodes[i].name);
        text_off[i] = intern_string(b, b->nodes[i].text);
    }
    for(i = 0; i < b->import_count; i++)
        import_off[i] = intern_string(b, b->imports[i]);
    for(i = 0; i < b->control_count; i++) {
        cvalue_off[i] = intern_string(b, b->controls[i].value);
        clabel_off[i] = intern_string(b, b->controls[i].label);
    }
    {
        static int upath_off[32];
        static int gpath_off[16];

        for(i = 0; i < b->update_count; i++)
            upath_off[i] = intern_string(b, b->updates[i].path);
        for(i = 0; i < b->guard_count; i++)
            gpath_off[i] = intern_string(b, b->guards[i].path);
        prog_bytes = emit_prog(b, prog_buf, sizeof(prog_buf), upath_off,
                               gpath_off);
        if(prog_bytes < 0)
            return -1;
    }

    need = 32 + b->node_count * 28 + b->string_used + prog_bytes +
           b->import_count * 4 + b->control_count * KRB_CONTROL_SIZE;
    if(need > cap)
        return -1;
    wr_u32(&hp, KRB_MAGIC);
    wr_u16(&hp, 2);
    wr_u16(&hp, 0);
    wr_u32(&hp, (unsigned)b->node_count);
    wr_u32(&hp, (unsigned)b->string_used);
    wr_u32(&hp, (unsigned)prog_bytes);
    wr_u32(&hp, (unsigned)b->import_count);
    wr_u32(&hp, (unsigned)b->control_count);
    memcpy(out, header, 32);
    out += 32;
    for(i = 0; i < b->node_count; i++) {
        unsigned char node[28];
        unsigned char *np = node;
        const KrbBuildNode *n = &b->nodes[i];

        wr_u16(&np, (unsigned)i);
        wr_i16(&np, n->parent);
        wr_u16(&np, (unsigned)name_off[i]);
        *np++ = (unsigned char)n->type;
        *np++ = (unsigned char)n->flags;
        wr_u16(&np, n->bind_slot >= 0 ? (unsigned)n->bind_slot : 0xffff);
        wr_i16(&np, n->x);
        wr_i16(&np, n->y);
        wr_i16(&np, n->w);
        wr_i16(&np, n->h);
        wr_u32(&np, n->color);
        wr_u16(&np, (unsigned)text_off[i]);
        wr_u16(&np, (unsigned)n->font_size);
        *np++ = (unsigned char)n->style;
        *np++ = 0;
        memcpy(out, node, 28);
        out += 28;
    }
    memcpy(out, b->strings, (size_t)b->string_used);
    out += b->string_used;
    memcpy(out, prog_buf, (size_t)prog_bytes);
    out += prog_bytes;
    for(i = 0; i < b->import_count; i++) {
        unsigned char slot[4];
        unsigned char *sp = slot;

        wr_u32(&sp, (unsigned)import_off[i]);
        memcpy(out, slot, 4);
        out += 4;
    }
    for(i = 0; i < b->control_count; i++) {
        unsigned char c[KRB_CONTROL_SIZE];
        unsigned char *cp = c;
        const KrbBuildControl *ctl = &b->controls[i];

        *cp++ = (unsigned char)ctl->kind;
        *cp++ = 0;                         /* option_count (range widgets: 0) */
        wr_u16(&cp, ctl->id);
        wr_u32(&cp, (unsigned)ctl->min);
        wr_u32(&cp, (unsigned)ctl->max);
        wr_u32(&cp, (unsigned)ctl->step);
        wr_u16(&cp, (unsigned)cvalue_off[i]);
        wr_u16(&cp, (unsigned)clabel_off[i]);
        wr_u16(&cp, 0);                    /* options_off */
        wr_u16(&cp, 0);                    /* reserved */
        memcpy(out, c, KRB_CONTROL_SIZE);
        out += KRB_CONTROL_SIZE;
    }
    return (int)(out - dst);
}

static void
c_ident(char *dst, size_t dst_size, const char *src)
{
    slug_name(dst, dst_size, src);
    if(dst[0] >= '0' && dst[0] <= '9') {
        char tmp[KIR_NAME_MAX];

        snprintf(tmp, sizeof(tmp), "h_%s", dst);
        snprintf(dst, dst_size, "%s", tmp);
    }
}

static void
write_krb_host(const KirModule *m, const char *root, const char *gen_rel,
               const char *out_dir, const KrbBuild *b,
               const unsigned char *bytes, int len, int no_main)
{
    char hrel[KIR_PATH_MAX];
    char crel[KIR_PATH_MAX];
    char hpath[KIR_PATH_MAX];
    char cpath[KIR_PATH_MAX];
    char screen[KIR_NAME_MAX];
    char guard[KIR_NAME_MAX * 2];
    FILE *out;
    int i;
    int j;

    snprintf(screen, sizeof(screen), "App");
    for(i = 0; i < m->function_count; i++) {
        if(m->functions[i].name[0] != '\0') {
            snprintf(screen, sizeof(screen), "%s", m->functions[i].name);
            break;
        }
    }
    snprintf(hrel, sizeof(hrel), "%s.krb.h", gen_rel);
    snprintf(crel, sizeof(crel), "%s.krb.c", gen_rel);
    path_join(hpath, sizeof(hpath), out_dir, hrel);
    path_join(cpath, sizeof(cpath), out_dir, crel);
    mkdir_parent(hpath);
    header_guard(guard, sizeof(guard), hrel);

    out = fopen(hpath, "wb");
    if(out == NULL)
        die("%s: open failed: %s", hpath, strerror(errno));
    fprintf(out, "/* Generated by k2b from %s. */\n",
            relative_path(root, m->source_path));
    fprintf(out, "#ifndef %s\n#define %s\n\n", guard, guard);
    fprintf(out, "void %s_krb_draw(int x, int y, int w, int h);\n", screen);
    fprintf(out, "int %s_krb_press(const char *name);\n", screen);
    fprintf(out, "int %s_krb_read_i32(const char *path, int *out);\n", screen);
    fprintf(out, "int %s_krb_read_cstr(const char *path, char *out, unsigned n);\n",
            screen);
    fprintf(out, "\n#endif\n");
    fclose(out);

    out = fopen(cpath, "wb");
    if(out == NULL)
        die("%s: open failed: %s", cpath, strerror(errno));
    fprintf(out, "/* Generated by k2b from %s. */\n",
            relative_path(root, m->source_path));
    fprintf(out, "#include \"%s\"\n", hrel);
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <string.h>\n");
    fprintf(out, "#include \"krb.h\"\n");
    fprintf(out, "#if !defined(KRYON_KRB_NO_MAIN)\n");
    fprintf(out, "#include \"kryon.h\"\n");
    if(m->app.font_examples)
        fprintf(out, "#include \"example_ui_font.h\"\n");
    fprintf(out, "#endif\n\n");
    for(i = 0; i < b->field_count; i++)
        fprintf(out, "%s\n", b->fields[i].decl);
    if(b->field_count > 0)
        fputc('\n', out);
    fprintf(out, "static const unsigned char krb_bytes[%d] = {", len);
    for(i = 0; i < len; i++) {
        if(i % 12 == 0)
            fprintf(out, "\n    ");
        fprintf(out, "0x%02x%s", bytes[i], i + 1 < len ? "," : "");
    }
    fprintf(out, "\n};\n\n");
    fprintf(out, "static KrbImage krb_img;\n");
    fprintf(out, "static int krb_ready;\n\n");
    for(i = 0; i < b->handler_count; i++) {
        char fn[KIR_NAME_MAX + 8];

        c_ident(fn, sizeof(fn), b->handlers[i].name);
        fprintf(out, "static int\non_%s(void *ud)\n{\n", fn);
        fprintf(out, "    (void)ud;\n");
        for(j = 0; j < b->handlers[i].body_count; j++)
            fprintf(out, "    %s\n", b->handlers[i].body[j]);
        fprintf(out, "    return 1;\n}\n\n");
    }
    fprintf(out, "static void\nkrb_ensure(void)\n{\n");
    fprintf(out, "    if(krb_ready)\n        return;\n");
    fprintf(out, "    memset(&krb_img, 0, sizeof(krb_img));\n");
    fprintf(out, "    if(KrbLoad(&krb_img, krb_bytes, sizeof(krb_bytes)) != 0)\n");
    fprintf(out, "        return;\n");
    for(i = 0; i < b->field_count; i++) {
        const KrbStateField *f = &b->fields[i];

        fprintf(out, "    KrbBindMem(&krb_img, \"%s\", %s%s, %u, %u);\n",
                f->name, f->kind == 5 ? "" : "&", f->name, f->kind, f->size);
    }
    for(i = 0; i < b->handler_count; i++) {
        char fn[KIR_NAME_MAX + 8];

        c_ident(fn, sizeof(fn), b->handlers[i].name);
        fprintf(out, "    KrbBind(&krb_img, \"%s\", on_%s, NULL);\n",
                b->handlers[i].name, fn);
    }
    fprintf(out, "    krb_ready = 1;\n}\n\n");
    fprintf(out, "void\n%s_krb_draw(int x, int y, int w, int h)\n{\n", screen);
    fprintf(out, "    krb_ensure();\n");
    fprintf(out, "    if(krb_ready)\n");
    fprintf(out, "        KrbDraw(&krb_img, x, y, w, h);\n");
    fprintf(out, "}\n\n");
    fprintf(out, "int\n%s_krb_press(const char *name)\n{\n", screen);
    fprintf(out, "    unsigned i;\n\n");
    fprintf(out, "    krb_ensure();\n");
    fprintf(out, "    if(!krb_ready || name == NULL)\n        return -1;\n");
    fprintf(out, "    for(i = 0; i < KrbImportCount(&krb_img); i++) {\n");
    fprintf(out, "        if(strcmp(KrbImportName(&krb_img, i), name) == 0) {\n");
    fprintf(out, "            if(krb_img.binds[i] != NULL)\n");
    fprintf(out, "                return krb_img.binds[i](krb_img.bind_ud[i]);\n");
    fprintf(out, "            return 0;\n");
    fprintf(out, "        }\n");
    fprintf(out, "    }\n");
    fprintf(out, "    return -1;\n");
    fprintf(out, "}\n\n");
    fprintf(out, "int\n%s_krb_read_i32(const char *path, int *out)\n{\n",
            screen);
    fprintf(out, "    krb_ensure();\n");
    fprintf(out, "    return krb_ready ? KrbReadI32(&krb_img, path, out) : -1;\n");
    fprintf(out, "}\n\n");
    fprintf(out, "int\n%s_krb_read_cstr(const char *path, char *out, unsigned n)\n{\n",
            screen);
    fprintf(out, "    krb_ensure();\n");
    fprintf(out, "    return krb_ready ? KrbReadCStr(&krb_img, path, out, n) : -1;\n");
    fprintf(out, "}\n");
    if(!no_main && m->app.title[0] != '\0') {
        int width = m->app.width > 0 ? m->app.width : 800;
        int height = m->app.height > 0 ? m->app.height : 600;
        int fps = m->app.fps > 0 ? m->app.fps : 60;
        char title[512];

        c_string_literal(title, sizeof(title), m->app.title);
        fprintf(out, "\n#if !defined(KRYON_KRB_NO_MAIN)\n");
        fprintf(out, "int\nmain(void)\n{\n");
        fprintf(out, "    InitWindow(%d, %d, %s);\n", width, height, title);
        fprintf(out, "    SetTargetFPS(%d);\n", fps);
        if(m->app.font_examples)
            fprintf(out, "    LoadExampleUIFont();\n");
        fprintf(out, "    InitUI(%d, %d, GetUIScale());\n", width, height);
        if(m->app.theme[0] != '\0')
            fprintf(out, "    SetCurrentTheme(%s, %d);\n",
                    m->app.theme, m->app.dark_mode);
        fprintf(out, "    while(!WindowShouldClose()) {\n");
        fprintf(out, "        BeginDrawing();\n");
        fprintf(out, "        BeginUIFrame(%d, %d, GetUIScale());\n",
                width, height);
        fprintf(out, "        %s_krb_draw(0, 0, GetScreenWidth(), GetScreenHeight());\n",
                screen);
        fprintf(out, "        EndUIFocus();\n");
        fprintf(out, "        EndDrawing();\n");
        fprintf(out, "    }\n");
        if(m->app.font_examples)
            fprintf(out, "    UnloadExampleUIFont();\n");
        fprintf(out, "    CloseWindow();\n");
        fprintf(out, "    return 0;\n");
        fprintf(out, "}\n#endif\n");
    }
    fclose(out);
}

void
write_krb(const KirModule *m, const char *root, const char *out_dir,
          int no_main)
{
    const char *rel = relative_path(root, m->source_path);
    char gen_rel[KIR_PATH_MAX];
    char krel[KIR_PATH_MAX];
    char kpath[KIR_PATH_MAX];
    unsigned char bytes[KRB_OUT_MAX];
    KrbBuild build;
    FILE *out;
    int i;
    int len;

    strip_kry_ext(gen_rel, sizeof(gen_rel), rel);
    snprintf(krel, sizeof(krel), "%s.krb", gen_rel);
    path_join(kpath, sizeof(kpath), out_dir, krel);
    mkdir_parent(kpath);

    memset(&build, 0, sizeof(build));
    build.strings[0] = '\0';
    build.string_used = 1;
    collect_state(&build, m);
    for(i = 0; i < m->function_count; i++)
        collect_widgets(&build, &m->functions[i]);

    len = emit_krb_mem(bytes, KRB_OUT_MAX, &build);
    if(len < 0)
        die("%s: cartridge is too large", m->source_path);
    out = fopen(kpath, "wb");
    if(out == NULL)
        die("%s: open failed: %s", kpath, strerror(errno));
    if(fwrite(bytes, 1, (size_t)len, out) != (size_t)len)
        die("%s: write failed", kpath);
    fclose(out);
    if(build.dropped_kinds > 0) {
        /* explain thin cartridges instead of surprising with them */
        fprintf(stderr, "k2b: %s: %d call kinds not in KRB:", rel,
                build.dropped_kinds);
        for(int d = 0; d < build.dropped_kinds; d++)
            fprintf(stderr, " %s x%d", build.dropped[d],
                    build.dropped_count[d]);
        fprintf(stderr, "\n");
    }
    write_krb_host(m, root, gen_rel, out_dir, &build, bytes, len, no_main);
}
