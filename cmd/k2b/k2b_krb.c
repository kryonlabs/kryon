/* Emit a krb cartridge from Kir widget calls. Lowers a KirModule (shared
 * kir_parse.c frontend) to a .krb binary + C host. */
#include "kir.h"
#include "kir_text.h"
#include "krb.h"
#include "k2b_stb.h"
#include "k2b_atlas.h"

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
               int no_main, int allow_unsupported);

#define KRB_BUILD_NODE_MAX 256
#define KRB_BUILD_STR_MAX 8192
#define KRB_BUILD_IMPORT_MAX 32
#define KRB_BUILD_CTRL_MAX 128
#define KRB_BUILD_HANDLER_LINES 16
#define KRB_OUT_MAX 16777216

typedef struct KrbAsset {
    char path[256];  /* cartridge path (also the PICTURE text) */
    char file[1024]; /* host file read at emit time */
    unsigned char *mem; /* decoded RGBA8 pixels (kind 0), malloc'd */
    unsigned mem_len;
    unsigned kind;   /* 0 raw RGBA (.kraw), 2 opaque file blob */
    unsigned w;
    unsigned h;
} KrbAsset;

typedef struct KrbTerm {
    int is_path; /* 0 literal, 1 path, 2 TIME */
    char path[KIR_NAME_MAX];
    int val;
} KrbTerm;

typedef struct KrbUpdate {
    char path[KIR_NAME_MAX];
    int kind; /* 0 = set, 1 = add, 2 = sub */
    int val;
    /* v2: expression rhs: path = rhs0 [op rhs1] */
    KrbTerm rhs[2];
    int rhs_n;
    int rhs_op; /* arith opcode when rhs_n == 2 */
} KrbUpdate;

/* ordered program items: updates, node draws, and guard brackets in
 * source order so conditional assignments compile correctly */
typedef struct KrbItem {
    int kind; /* 0 update, 1 node draw, 2 guard open, 3 else, 4 guard close */
    int idx;  /* update index, node index, or guard index */
} KrbItem;

typedef struct KrbAnim {
    int node; /* node index, resolved at emit */
    char node_name[KIR_NAME_MAX];
    int field; /* 0=x 1=y 2=w 3=h */
    char path[KIR_NAME_MAX];
} KrbAnim;

typedef struct KrbGuard {
    int start; /* item index range [start, end) the guard covers */
    int end;
    KrbTerm l[2]; /* left: term, optionally term op term */
    int ln; /* 1 or 2 terms */
    int lop; /* KRB_OP_ADD/SUB/MUL/DIV when ln == 2 */
    int op; /* comparison KRB_OP_EQ..GE */
    KrbTerm r[2];
    int rn;
    int rop;
    int else_start; /* [else_start, end) draws when the guard is false */
    int has_else;
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
    char options[512]; /* semicolon-separated; empty = none */
    int option_count;
} KrbBuildControl;

typedef struct KrbScrollAreaDef {
    char name[KIR_NAME_MAX];
    int x;
    int y;
    int w;
    int h;
    int content_h;
    unsigned flags;
    char path[KIR_NAME_MAX];
} KrbScrollAreaDef;

typedef struct KrbScrollViewDef {
    char name[KIR_NAME_MAX];
    int content_x;
    int content_y;
    int flags;
} KrbScrollViewDef;

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
    KrbAsset assets[24];
    int asset_count;
    char asset_root[1024];
    int scroll_open; /* open SCROLL node index, -1 none */
    KrbAnim anims[32];
    int anim_count;
    KrbItem items[512];
    int item_count;
    /* .kry string-array variables ('name: [N] const char* = {...}') usable
     * as control option lists when lowering Dropdown/Combobox calls */
    char array_name[8][KIR_NAME_MAX];
    char array_joined[8][KIR_TEXT_MAX];
    int array_count;
    KrbScrollAreaDef scroll_areas[16];
    int scroll_area_count;
    KrbScrollViewDef scroll_views[16];
    int scroll_view_count;
} KrbBuild;

static void register_string_array(KrbBuild *b, const char *name,
                                  const char *init);
static int parse_scroll_area_decl(KrbBuild *b, const char *text);
static KrbBuild *g_coord_build;


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

    expr = kir_skip_inline_ws(expr);
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
    const char *p = kir_skip_inline_ws(expr);

    *scaled = 0;
    *value = 0;
    if(g_coord_build != NULL) {
        int i;

        for(i = 0; i < g_coord_build->scroll_view_count; i++) {
            KrbScrollViewDef *view = &g_coord_build->scroll_views[i];
            size_t len = strlen(view->name);

            if(strncmp(p, view->name, len) == 0 && p[len] == '.') {
                const char *field = p + len + 1;
                int base = 0;
                int base_scaled = 0;

                if(strncmp(field, "content_x", 9) == 0) {
                    base = view->content_x;
                    base_scaled = (view->flags & KRB_FLAG_SCALE_X) != 0;
                    p = field + 9;
                } else if(strncmp(field, "content_y", 9) == 0) {
                    base = view->content_y;
                    base_scaled = (view->flags & KRB_FLAG_SCALE_Y) != 0;
                    p = field + 9;
                } else {
                    continue;
                }
                p = kir_skip_inline_ws(p);
                if(*p == '+' || *p == '-') {
                    int sign = *p == '-' ? -1 : 1;
                    int add = 0;
                    int add_scaled = 0;

                    if(parse_coord(p + 1, &add, &add_scaled)) {
                        *value = base + sign * add;
                        *scaled = base_scaled || add_scaled;
                        return 1;
                    }
                } else {
                    *value = base;
                    *scaled = base_scaled;
                    return 1;
                }
            }
        }
    }
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
    const char *p = kir_skip_inline_ws(expr);

    if(strstr(p, "Text24") != NULL)
        return 24;
    if(strstr(p, "Text16") != NULL)
        return 16;
    if(strstr(p, "Text12") != NULL)
        return 12;
    if(strstr(p, "Text8") != NULL)
        return 8;
    if(*p >= '0' && *p <= '9')
        return atoi(p);
    return 16;
}

static int
button_style_of(const char *expr)
{
    if(strstr(expr, "ButtonStyleDanger") != NULL)
        return 2;
    if(strstr(expr, "ButtonStyleSecondary") != NULL)
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
    const char *p = kir_skip_inline_ws(text);

    /* Strip leading control keywords so 'if Button(...)' / 'else if ...' reach
     * the call, and 'x := Widget(...)' / 'x = Widget(...)' skip past the lhs by
     * locating the first identifier immediately followed by '('. */
    for(;;) {
        if(strncmp(p, "if", 2) == 0 && (p[2] == ' ' || p[2] == '\t')) {
            p = kir_skip_inline_ws(p + 2);
            continue;
        }
        if(strncmp(p, "else", 4) == 0 && (p[4] == ' ' || p[4] == '\t')) {
            p = kir_skip_inline_ws(p + 4);
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
    char name[KIR_NAME_MAX];

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
    char name[KIR_NAME_MAX];
    int scaled;

    if(args == NULL)
        return 0;
    count = split_args(args + 1, parts, 8);
    if(count < 1)
        return 0;
    snprintf(name, sizeof(name), "text%d", b->node_count);
    if(strstr(parts[0], "TextFormat") != NULL) {
        /* TextFormat("fmt", value, x, y, font, color): split_args puts the
         * format string in parts[0] and the bound value in parts[1] —
         * everything shifts one slot relative to plain Text. */
        char ident[KIR_NAME_MAX];
        const char *q = kir_skip_inline_ws(parts[1]);
        size_t nident = 0;

        while((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') ||
              (*q >= '0' && *q <= '9') || *q == '_') {
            if(nident + 1 < sizeof(ident))
                ident[nident++] = *q;
            q++;
        }
        ident[nident] = '\0';
        if(ident[0] != '\0')
            snprintf(name, sizeof(name), "%s", ident);
        else
            snprintf(name, sizeof(name), "text%d", b->node_count);
        n = add_node(b, 2, name);
        if(n == NULL)
            return 0;
        extract_string(parts[0], n->text, sizeof(n->text));
    } else if(parts[0][0] != '"' && is_ident_text(kir_skip_inline_ws(parts[0]))) {
        char ident[KIR_NAME_MAX];

        snprintf(ident, sizeof(ident), "%s", kir_skip_inline_ws(parts[0]));
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
    {
        int is_tf = strstr(call, "TextFormat") != NULL;
        int xi = is_tf ? 2 : 1;
        int yi = is_tf ? 3 : 2;
        int fi = is_tf ? 4 : 3;
        int ci = is_tf ? 5 : 4;

        if(count > xi && parse_coord(parts[xi], &n->x, &scaled) && scaled)
            n->flags |= 1 << 2;
        if(count > yi && parse_coord(parts[yi], &n->y, &scaled) && scaled)
            n->flags |= 1 << 3;
        if(count > fi)
            n->font_size = font_size_of(parts[fi]);
        if(count > ci)
            n->color = parse_color(parts[ci]);
        else
            n->color = 0x80000001u;
    }
    return 1;
}

/* "(Color){r, g, b, a}" literal -> packed RGBA; 0 if not that form. */
static int
parse_color_ctor(const char *expr, unsigned *out)
{
    const char *p = kir_skip_inline_ws(expr);
    unsigned comps[4];
    int i;

    if(strncmp(p, "(Color){", 8) != 0)
        return 0;
    p += 8;
    for(i = 0; i < 4; i++) {
        comps[i] = (unsigned)strtol(kir_skip_inline_ws(p), NULL, 0);
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
    inner = kir_skip_inline_ws(parts[0]);
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
        const char *inner = kir_skip_inline_ws(parts[0]);
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
    if(p == NULL && strstr(props, "ButtonProps") != NULL)
        extract_string(strstr(props, "{") + 1, n->text, sizeof(n->text));
    if(p != NULL)
        extract_string(p, n->text, sizeof(n->text));
    p = strstr(props, ".style");
    if(p != NULL)
        n->style = button_style_of(p);
    p = strstr(props, ".bounds");
    if(p == NULL)
        p = strstr(props, "Rectangle){"); /* positional form */
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
    const char *t = kir_skip_inline_ws(raw);

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
static void
embed_asset(KrbBuild *b, const char *path)
{
    FILE *f;
    char file[1200];
    unsigned char hdr[8];
    int i;

    if(path == NULL || path[0] == '\0')
        return;
    for(i = 0; i < b->asset_count; i++)
        if(strcmp(b->assets[i].path, path) == 0)
            return;
    if(b->asset_count >= 24)
        return;
    snprintf(file, sizeof(file), "%s/%s", b->asset_root, path);
    f = fopen(file, "rb");
    if(f == NULL)
        return; /* not packable; host loads it at runtime as before */
    fclose(f);
    {
        KrbAsset *a = &b->assets[b->asset_count];
        unsigned char *rgba = NULL;
        int iw = 0;
        int ih = 0;

        snprintf(a->path, sizeof(a->path), "%s", path);
        snprintf(a->file, sizeof(a->file), "%s", file);
        /* .kraw: KRAW magic + u16 w,h + RGBA8 */
        {
            FILE *g = fopen(file, "rb");

            if(g != NULL && fread(hdr, 1, 8, g) == 8 && hdr[0] == 'K' &&
               hdr[1] == 'R' && hdr[2] == 'A' && hdr[3] == 'W') {
                fclose(g);
                a->kind = 0;
                a->w = hdr[4] | (hdr[5] << 8);
                a->h = hdr[6] | (hdr[7] << 8);
                b->asset_count++;
                return;
            }
            if(g != NULL)
                fclose(g);
        }
        /* png/jpg/bmp/webp: decode to RGBA8 and embed as pixels */
        if(k2b_decode_image(file, &rgba, &iw, &ih) == 0 && rgba != NULL &&
           iw > 0 && ih > 0) {
            a->kind = 0;
            a->mem = rgba;
            a->mem_len = (unsigned)(iw * ih * 4);
            a->w = (unsigned)iw;
            a->h = (unsigned)ih;
            b->asset_count++;
            return;
        }
        free(rgba);
        /* audio and anything else: opaque blob for host-side decode */
        a->kind = 2;
        a->w = 0;
        a->h = 0;
        b->asset_count++;
    }
}

static int parse_term(const char *p, KrbTerm *t);
static int parse_side(const char *p, KrbTerm t[2], int *n, int *aop);
static int intern_string(KrbBuild *b, const char *s);

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
int emit_prog(KrbBuild *b, const unsigned char *dst, int cap,
              const int *upath_off, const int *gpath_off,
              const int grpath_off[16][4]);

static int
parse_update(const char *text, char *path, int *kind, int *val)
{
    const char *p = kir_skip_inline_ws(text);
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
    q = kir_skip_inline_ws(q);
    if(strncmp(q, "++;", 3) == 0) {
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
        *val = atoi(kir_skip_inline_ws(q + 2));
        return 1;
    }
    if(*q == '=' && q[1] != '=') {
        const char *r = kir_skip_inline_ws(q + 1);
        KrbUpdate dummy;
        int used;

        *kind = 0;
        *val = atoi(r);
        /* rhs: term or term arith term; parse_term knows TIME */
        dummy.rhs_n = 1;
        used = parse_side(r, dummy.rhs, &dummy.rhs_n, &dummy.rhs_op);
        if(used > 0)
            return 3; /* caller copies expression */
        return 1;
    }
    return 0;
}

/* Parse one term: identifier path or integer. Returns consumed length. */
static int
parse_term(const char *p, KrbTerm *t)
{
    const char *q = p;
    size_t len;

    while(*q == '_' || (*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') ||
          (*q >= '0' && *q <= '9') || *q == '.')
        q++;
    len = (size_t)(q - p);
    if(len == 0)
        return 0;
    if((p[0] >= '0' && p[0] <= '9') || (p[0] == '-' && p[1] >= '0' &&
                                         p[1] <= '9')) {
        t->is_path = 0;
        t->val = atoi(p);
        t->path[0] = '\0';
        return (int)len;
    }
    if(len >= KIR_NAME_MAX)
        return 0;
    if(len == 4 && memcmp(p, "TIME", 4) == 0) {
        t->is_path = 2;
        t->path[0] = '\0';
        t->val = 0;
        return (int)len;
    }
    t->is_path = 1;
    memcpy(t->path, p, len);
    t->path[len] = '\0';
    t->val = 0;
    return (int)len;
}

/* Parse one side: term, or term arith term. */
static int
parse_side(const char *p, KrbTerm t[2], int *n, int *aop)
{
    const char *q;
    int used = parse_term(p, &t[0]);

    if(used == 0)
        return 0;
    q = kir_skip_inline_ws(p + used);
    *n = 1;
    *aop = 0;
    if(*q == '+' || *q == '-' || *q == '*' || *q == '/' || *q == '%') {
        int u2;
        int kind = *q;

        q = kir_skip_inline_ws(q + 1);
        u2 = parse_term(q, &t[1]);
        if(u2 == 0)
            return 0;
        *n = 2;
        *aop = kind == '+' ? KRB_OP_ADD : kind == '-' ? KRB_OP_SUB :
               kind == '*' ? KRB_OP_MUL : kind == '%' ? KRB_OP_MOD :
               KRB_OP_DIV;
        q = kir_skip_inline_ws(q + u2);
    }
    return (int)(q - p);
}

/* "if (<expr> <cmp> <expr>)" where each expr is term or term arith term. */
static int
parse_cond(const char *text, KrbGuard *g)
{
    const char *p = strstr(text, "if");
    const char *cmp;
    int used;
    int oplen = 0;

    if(p == NULL || p != kir_skip_inline_ws(text))
        return 0;
    p = kir_skip_inline_ws(p + 2);
    if(*p != '(')
        return 0;
    p = kir_skip_inline_ws(p + 1);
    used = parse_side(p, g->l, &g->ln, &g->lop);
    if(used == 0)
        return 0;
    cmp = kir_skip_inline_ws(p + used);
    if(strncmp(cmp, "==", 2) == 0) oplen = 2;
    else if(strncmp(cmp, "!=", 2) == 0) oplen = 2;
    else if(strncmp(cmp, "<=", 2) == 0) oplen = 2;
    else if(strncmp(cmp, ">=", 2) == 0) oplen = 2;
    else if(*cmp == '<') oplen = 1;
    else if(*cmp == '>') oplen = 1;
    else return 0;
    g->op = cmp[0] == '=' ? KRB_OP_EQ : cmp[0] == '!' ? KRB_OP_NE :
            cmp[0] == '<' ? (oplen == 2 ? KRB_OP_LE : KRB_OP_LT) :
                            (oplen == 2 ? KRB_OP_GE : KRB_OP_GT);
    p = kir_skip_inline_ws(cmp + oplen);
    used = parse_side(p, g->r, &g->rn, &g->rop);
    if(used == 0)
        return 0;
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
    g_coord_build = b;
    for(j = 0; j < fn->stmt_count; j++) {
        const KirStmt *st = &fn->stmts[j];
        int before = b->node_count;
        int opens = st->kind == KIR_STMT_IF || st->kind == KIR_STMT_WHILE ||
                    st->kind == KIR_STMT_FOR || st->kind == KIR_STMT_SWITCH;

        /* string-array locals ('opts: [3] const char* = {...}') become
         * option-list sources for Dropdown/Combobox lowering */
        {
            const char *t = kir_skip_inline_ws(st->text);
            const char *colon = t != NULL ? strchr(t, ':') : NULL;
            const char *eq = colon != NULL ? strchr(colon, '=') : NULL;

            if(colon != NULL && eq != NULL) {
                char decl_type[KIR_NAME_MAX];
                size_t tl = (size_t)(eq - colon - 1);

                if(tl >= sizeof(decl_type))
                    tl = sizeof(decl_type) - 1;
                memcpy(decl_type, colon + 1, tl);
                decl_type[tl] = '\0';
                if(strstr(decl_type, "char") != NULL &&
                   strchr(decl_type, '*') != NULL) {
                    char aname[KIR_NAME_MAX];
                    size_t nl = (size_t)(colon - t);

                    if(nl >= sizeof(aname))
                        nl = sizeof(aname) - 1;
                    memcpy(aname, t, nl);
                    aname[nl] = '\0';
                    register_string_array(b, aname, eq + 1);
                }
                if(strstr(decl_type, "UIScrollArea") != NULL)
                    parse_scroll_area_decl(b, st->text);
            }
        }
        if(st->kind == KIR_STMT_BLOCK_CLOSE) {
            depth--;
            if(open_guard >= 0 && depth < guard_depth) {
                KrbGuard *g = &b->guards[open_guard];

                if(!g->has_else && j + 1 < fn->stmt_count &&
                   fn->stmts[j + 1].kind == KIR_STMT_IF &&
                   strncmp(kir_skip_inline_ws(fn->stmts[j + 1].text), "else", 4) == 0) {
                    /* '} else {': marker item; guard stays open */
                    g->else_start = b->item_count;
                    g->has_else = 1;
                    if(b->item_count < 512)
                        b->items[b->item_count++] = (KrbItem){3, open_guard};
                    guard_depth = depth + 1; /* the else's own depth++ */
                } else {
                    g->end = b->item_count;
                    if(b->item_count < 512)
                        b->items[b->item_count++] = (KrbItem){4, open_guard};
                    open_guard = -1;
                }
            }
        }
        if(opens && st->kind == KIR_STMT_IF && open_guard < 0 &&
           b->guard_count < 16) {
            KrbGuard *g = &b->guards[b->guard_count];

            if(parse_cond(st->text, g)) {
                g->start = b->item_count;
                g->end = b->item_count;
                if(b->item_count < 512)
                    b->items[b->item_count++] = (KrbItem){2, b->guard_count};
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

            {
                char upath2[KIR_NAME_MAX];
                int rc2 = parse_update(st->text, upath2, &ukind, &uval);

                if(rc2 == 2 || rc2 == 3) {
                    KrbUpdate *u = &b->updates[b->update_count++];

                    snprintf(u->path, sizeof(u->path), "%s", upath2);
                    u->kind = 0;
                    u->val = 0;
                    u->rhs_n = 0;
                    u->rhs_op = 0;
                    if(rc2 == 2) {
                        u->rhs[0].is_path = 2; /* TIME marker */
                        u->rhs_n = 1;
                    } else {
                        int un;
                        int uop;

                        parse_side(kir_skip_inline_ws(strchr(st->text, '=') + 1),
                                   u->rhs, &un, &uop);
                        u->rhs_n = un;
                        u->rhs_op = uop;
                    }
                    if(b->item_count < 512)
                        b->items[b->item_count++] = (KrbItem){0,
                                                    b->update_count - 1};
                    continue;
                }
                if(rc2 == 1) {
                    KrbUpdate *u = &b->updates[b->update_count++];

                    snprintf(u->path, sizeof(u->path), "%s", upath2);
                    u->kind = ukind;
                    u->val = uval;
                    u->rhs_n = 0;
                    u->rhs_op = 0;
                    if(b->item_count < 512)
                        b->items[b->item_count++] = (KrbItem){0,
                                                    b->update_count - 1};
                    continue;
                }
            }
            if(parse_update(st->text, upath, &ukind, &uval)) {
                KrbUpdate *u = &b->updates[b->update_count++];

                snprintf(u->path, sizeof(u->path), "%s", upath);
                u->kind = ukind;
                u->val = uval;
                if(b->item_count < 512)
                    b->items[b->item_count++] = (KrbItem){0,
                                                b->update_count - 1};
                continue;
            }
        }
        if(try_widget(b, st->text)) {
            if(b->node_count > before &&
               b->nodes[b->node_count - 1].type == KRB_NODE_SCROLL) {
                b->scroll_open = b->node_count - 1;
            } else if(b->node_count > before && b->scroll_open >= 0) {
                int k;

                for(k = before; k < b->node_count; k++)
                    b->nodes[k].parent = b->scroll_open;
            }
            {
                int k2;

                for(k2 = before; k2 < b->node_count; k2++)
                    if(b->item_count < 512)
                        b->items[b->item_count++] = (KrbItem){1, k2};
            }
            if(b->node_count > before &&
               b->nodes[b->node_count - 1].type == KRB_NODE_PICTURE)
                embed_asset(b, b->nodes[b->node_count - 1].text);
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
            /* carry the initializer so hosts without generated binds can
             * auto-mount: ints in x, CSTRs via the text field */
            if(kind == 1)
                n->x = atoi(sf->init);
            else if(kind == 5)
                extract_string(sf->init, n->text, sizeof(n->text));
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

/* Picture((PictureProps){asset_path, bounds, source, origin, rot, tint, fit, style})
 * -> a PICTURE node; text holds the asset path, style holds the PictureFit. */
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
    expr = kir_skip_inline_ws(expr);
    if(*expr == '&')
        expr++;
    expr = kir_skip_inline_ws(expr);
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
    n->bind_slot = atoi(kir_skip_inline_ws(parts[0]));
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
    n->bind_slot = atoi(kir_skip_inline_ws(parts[0]));
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

static int
parse_scroll_area_decl(KrbBuild *b, const char *text)
{
    const char *t = kir_skip_inline_ws(text);
    const char *colon = t != NULL ? strchr(t, ':') : NULL;
    const char *eq = colon != NULL ? strchr(colon, '=') : NULL;
    const char *open;
    const char *close;
    char body[KIR_TEXT_MAX];
    char parts[8][KIR_TEXT_MAX];
    KrbScrollAreaDef *area;
    size_t len;
    size_t name_len;
    int count;
    int scaled;

    if(t == NULL || colon == NULL || eq == NULL ||
       strstr(colon, "UIScrollArea") == NULL ||
       b->scroll_area_count >= 16)
        return 0;
    open = strchr(eq, '{');
    if(open == NULL || open[1] != '{')
        return 0;
    close = strrchr(open, '}');
    if(close == NULL || close <= open)
        return 0;
    len = (size_t)(close - open - 1);
    if(len >= sizeof(body))
        len = sizeof(body) - 1;
    memcpy(body, open + 1, len);
    body[len] = '\0';
    count = split_args(body, parts, 8);
    if(count < 5)
        return 0;
    area = &b->scroll_areas[b->scroll_area_count];
    memset(area, 0, sizeof(*area));
    name_len = (size_t)(colon - t);
    while(name_len > 0 && isspace((unsigned char)t[name_len - 1]))
        name_len--;
    if(name_len >= sizeof(area->name))
        name_len = sizeof(area->name) - 1;
    memcpy(area->name, t, name_len);
    area->name[name_len] = '\0';
    if(!parse_rect_fields(parts[0], &area->x, &area->y, &area->w, &area->h,
                          &area->flags))
        return 0;
    if(parse_coord(parts[1], &area->content_h, &scaled) && scaled)
        area->flags |= KRB_FLAG_SCALE_H;
    strip_amp(parts[4], area->path, sizeof(area->path));
    if(area->path[0] == '\0')
        return 0;
    b->scroll_area_count++;
    return 1;
}

static KrbScrollAreaDef *
find_scroll_area(KrbBuild *b, const char *name)
{
    int i;

    for(i = 0; i < b->scroll_area_count; i++) {
        if(strcmp(b->scroll_areas[i].name, name) == 0)
            return &b->scroll_areas[i];
    }
    return NULL;
}

static void
remember_scroll_view(KrbBuild *b, const char *raw, const KrbScrollAreaDef *area)
{
    const char *t = kir_skip_inline_ws(raw);
    const char *colon = t != NULL ? strchr(t, ':') : NULL;
    const char *eq = t != NULL ? strchr(t, '=') : NULL;
    KrbScrollViewDef *view;
    size_t len;

    if(t == NULL || colon == NULL || eq == NULL || colon > eq ||
       strstr(colon, "UIScrollView") == NULL ||
       b->scroll_view_count >= 16)
        return;
    view = &b->scroll_views[b->scroll_view_count];
    memset(view, 0, sizeof(*view));
    len = (size_t)(colon - t);
    while(len > 0 && isspace((unsigned char)t[len - 1]))
        len--;
    if(len >= sizeof(view->name))
        len = sizeof(view->name) - 1;
    memcpy(view->name, t, len);
    view->name[len] = '\0';
    view->content_x = area->x;
    view->content_y = area->y;
    view->flags = area->flags;
    b->scroll_view_count++;
}

static int
parse_begin_scroll_container(KrbBuild *b, const char *raw, const char *call)
{
    const char *args = strchr(call, '(');
    char area_name[KIR_NAME_MAX];
    char name[32];
    const char *p;
    size_t len = 0;
    KrbScrollAreaDef *area;
    KrbBuildNode *n;

    if(args == NULL)
        return 0;
    p = kir_skip_inline_ws(args + 1);
    while((isalnum((unsigned char)p[len]) || p[len] == '_') &&
          len + 1 < sizeof(area_name))
        len++;
    if(len == 0)
        return 0;
    memcpy(area_name, p, len);
    area_name[len] = '\0';
    area = find_scroll_area(b, area_name);
    if(area == NULL)
        return 0;
    snprintf(name, sizeof(name), "scroll%d", b->node_count);
    n = add_node(b, KRB_NODE_SCROLL, name);
    if(n == NULL)
        return 0;
    n->x = area->x;
    n->y = area->y;
    n->w = area->w;
    n->h = area->h;
    n->flags = (unsigned)area->flags;
    n->font_size = area->content_h;
    snprintf(n->name, sizeof(n->name), "%s", area->path);
    remember_scroll_view(b, raw, area);
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
                            atoi(kir_skip_inline_ws(parts[0])), atoi(kir_skip_inline_ws(parts[5])),
                            atoi(kir_skip_inline_ws(parts[6])), 1, label);
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
                            atoi(kir_skip_inline_ws(parts[1])), atoi(kir_skip_inline_ws(parts[2])),
                            atoi(kir_skip_inline_ws(parts[3])), atoi(kir_skip_inline_ws(parts[4])), "");
}

static int
copy_text(char *dst, size_t dst_size, const char *src)
{
    size_t n;

    if(dst_size == 0)
        return 0;
    if(src == NULL)
        src = "";
    n = strlen(src);
    if(n >= dst_size)
        n = dst_size - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
    return 1;
}

static int
path_expr(const char *expr, char *dst, size_t dst_size)
{
    const char *p = kir_skip_inline_ws(expr);
    size_t n = 0;

    if(dst_size == 0)
        return 0;
    while(n + 1 < dst_size &&
          (isalnum((unsigned char)p[n]) || p[n] == '_' || p[n] == '.'))
        n++;
    if(n == 0 || isdigit((unsigned char)p[0])) {
        dst[0] = '\0';
        return 0;
    }
    if(*kir_skip_inline_ws(p + n) != '\0') {
        dst[0] = '\0';
        return 0;
    }
    memcpy(dst, p, n);
    dst[n] = '\0';
    return 1;
}

static void
trim_copy(char *dst, size_t dst_size, const char *start, size_t len)
{
    if(dst_size == 0)
        return;
    while(len > 0 && isspace((unsigned char)*start)) {
        start++;
        len--;
    }
    while(len > 0 && isspace((unsigned char)start[len - 1]))
        len--;
    if(len >= dst_size)
        len = dst_size - 1;
    memcpy(dst, start, len);
    dst[len] = '\0';
}

static int
path_from_eq_value(const char *expr, int value, char *dst, size_t dst_size)
{
    const char *eq = strstr(expr, "==");
    char left[KIR_TEXT_MAX];
    char right[KIR_TEXT_MAX];
    char candidate[KIR_NAME_MAX];

    if(eq == NULL)
        return 0;
    trim_copy(left, sizeof(left), expr, (size_t)(eq - expr));
    trim_copy(right, sizeof(right), eq + 2, strlen(eq + 2));
    if(path_expr(left, candidate, sizeof(candidate)) &&
       atoi(kir_skip_inline_ws(right)) == value) {
        copy_text(dst, dst_size, candidate);
        return 1;
    }
    if(path_expr(right, candidate, sizeof(candidate)) &&
       atoi(kir_skip_inline_ws(left)) == value) {
        copy_text(dst, dst_size, candidate);
        return 1;
    }
    return 0;
}

/* Radio((RadioButtonProps){bounds,label,id,selected == id,disabled}) ->
 * read/write CONTROL node. On click the runtime writes id into the selected
 * state path recovered from the checked expression. */
static int
parse_radio(KrbBuild *b, const char *call)
{
    const char *p = strstr(call, "RadioButtonProps");
    const char *open;
    const char *close;
    char body[KIR_TEXT_MAX];
    char parts[8][KIR_TEXT_MAX];
    char f_bounds[KIR_TEXT_MAX] = "";
    char f_label[KIR_TEXT_MAX] = "";
    char f_id[KIR_TEXT_MAX] = "";
    char f_checked[KIR_TEXT_MAX] = "";
    char path[KIR_NAME_MAX];
    char label[KIR_TEXT_MAX];
    int x = 0, y = 0, w = 0, h = 0, count;
    int id;
    unsigned flags = 0;
    size_t len;
    int i;

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
    if(count < 4)
        return 0;
    for(i = 0; i < count && i < 5; i++) {
        const char *part = kir_skip_inline_ws(parts[i]);

        if(*part == '.') {
            const char *eq = strchr(part, '=');

            if(eq == NULL)
                continue;
            if(strncmp(part, ".bounds", 7) == 0)
                copy_text(f_bounds, sizeof(f_bounds), kir_skip_inline_ws(eq + 1));
            else if(strncmp(part, ".label", 6) == 0)
                copy_text(f_label, sizeof(f_label), kir_skip_inline_ws(eq + 1));
            else if(strncmp(part, ".id", 3) == 0)
                copy_text(f_id, sizeof(f_id), kir_skip_inline_ws(eq + 1));
            else if(strncmp(part, ".checked", 8) == 0)
                copy_text(f_checked, sizeof(f_checked), kir_skip_inline_ws(eq + 1));
        } else {
            switch(i) {
            case 0: copy_text(f_bounds, sizeof(f_bounds), part); break;
            case 1: copy_text(f_label, sizeof(f_label), part); break;
            case 2: copy_text(f_id, sizeof(f_id), part); break;
            case 3: copy_text(f_checked, sizeof(f_checked), part); break;
            default: break;
            }
        }
    }
    if(f_id[0] == '\0')
        return 0;
    id = atoi(kir_skip_inline_ws(f_id));
    if(!path_from_eq_value(f_checked, id, path, sizeof(path)))
        return 0;
    label[0] = '\0';
    extract_string(f_label, label, sizeof(label));
    if(!parse_rect_fields(f_bounds, &x, &y, &w, &h, &flags))
        return 0;
    return add_control_node(b, KRB_CTRL_RADIO, path, x, y, w, h, flags, id,
                            0, 0, 1, label);
}

/* Progress((ProgressBarProps){bounds,min,max,value,label}) -> read-only
 * CONTROL node. The value field is a mounted state path, matching the KRB
 * control table used by sliders and spinboxes. */
static int
parse_progress(KrbBuild *b, const char *call)
{
    const char *p = strstr(call, "ProgressBarProps");
    const char *open;
    const char *close;
    char body[KIR_TEXT_MAX];
    char parts[8][KIR_TEXT_MAX];
    char f_bounds[KIR_TEXT_MAX] = "";
    char f_min[KIR_TEXT_MAX] = "";
    char f_max[KIR_TEXT_MAX] = "";
    char f_value[KIR_TEXT_MAX] = "";
    char f_label[KIR_TEXT_MAX] = "";
    char path[KIR_NAME_MAX];
    char label[KIR_TEXT_MAX];
    int x = 0, y = 0, w = 0, h = 0, count;
    unsigned flags = 0;
    size_t len;
    int i;

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
    if(count < 4)
        return 0;
    for(i = 0; i < count && i < 5; i++) {
        const char *part = kir_skip_inline_ws(parts[i]);

        if(*part == '.') {
            const char *eq = strchr(part, '=');

            if(eq == NULL)
                continue;
            if(strncmp(part, ".bounds", 7) == 0)
                copy_text(f_bounds, sizeof(f_bounds), kir_skip_inline_ws(eq + 1));
            else if(strncmp(part, ".min", 4) == 0)
                copy_text(f_min, sizeof(f_min), kir_skip_inline_ws(eq + 1));
            else if(strncmp(part, ".max", 4) == 0)
                copy_text(f_max, sizeof(f_max), kir_skip_inline_ws(eq + 1));
            else if(strncmp(part, ".value", 6) == 0)
                copy_text(f_value, sizeof(f_value), kir_skip_inline_ws(eq + 1));
            else if(strncmp(part, ".label", 6) == 0)
                copy_text(f_label, sizeof(f_label), kir_skip_inline_ws(eq + 1));
        } else {
            switch(i) {
            case 0: copy_text(f_bounds, sizeof(f_bounds), part); break;
            case 1: copy_text(f_min, sizeof(f_min), part); break;
            case 2: copy_text(f_max, sizeof(f_max), part); break;
            case 3: copy_text(f_value, sizeof(f_value), part); break;
            case 4: copy_text(f_label, sizeof(f_label), part); break;
            default: break;
            }
        }
    }
    if(!path_expr(f_value, path, sizeof(path)))
        return 0;
    label[0] = '\0';
    if(f_label[0] != '\0')
        extract_string(f_label, label, sizeof(label));
    if(!parse_rect_fields(f_bounds, &x, &y, &w, &h, &flags))
        return 0;
    return add_control_node(b, KRB_CTRL_PROGRESS, path, x, y, w, h, flags,
                            (int)b->control_count, atoi(kir_skip_inline_ws(f_min)),
                            atoi(kir_skip_inline_ws(f_max)), 1, label);
}

static int
add_rect_node(KrbBuild *b, const char *prefix, int x, int y, int w, int h,
              unsigned flags, unsigned color)
{
    KrbBuildNode *n;
    char name[32];

    snprintf(name, sizeof(name), "%s%d", prefix, b->node_count);
    n = add_node(b, KRB_NODE_RECT, name);
    if(n == NULL)
        return 0;
    n->x = x;
    n->y = y;
    n->w = w;
    n->h = h;
    n->flags = flags;
    n->color = color;
    return 1;
}

static int
parse_labelframe(KrbBuild *b, const char *call)
{
    const char *p = strstr(call, "LabelFrameProps");
    const char *open;
    const char *close;
    char body[KIR_TEXT_MAX];
    char parts[4][KIR_TEXT_MAX];
    char f_bounds[KIR_TEXT_MAX] = "";
    char f_title[KIR_TEXT_MAX] = "";
    char title[KIR_TEXT_MAX];
    size_t len;
    int count;
    int i;
    int x = 0, y = 0, w = 0, h = 0;
    unsigned flags = 0;
    unsigned xflag;
    unsigned yflag;
    unsigned wflag;
    unsigned hflag;
    unsigned border = KRB_COLOR_THEME | KRY_THEME_BUTTON;

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
    count = split_args(body, parts, 4);
    if(count < 1)
        return 0;
    for(i = 0; i < count && i < 2; i++) {
        const char *part = kir_skip_inline_ws(parts[i]);

        if(*part == '.') {
            const char *eq = strchr(part, '=');

            if(eq == NULL)
                continue;
            if(strncmp(part, ".bounds", 7) == 0)
                copy_text(f_bounds, sizeof(f_bounds), kir_skip_inline_ws(eq + 1));
            else if(strncmp(part, ".title", 6) == 0)
                copy_text(f_title, sizeof(f_title), kir_skip_inline_ws(eq + 1));
        } else {
            if(i == 0)
                copy_text(f_bounds, sizeof(f_bounds), part);
            else if(i == 1)
                copy_text(f_title, sizeof(f_title), part);
        }
    }
    if(!parse_rect_fields(f_bounds, &x, &y, &w, &h, &flags))
        return 0;
    xflag = flags & KRB_FLAG_SCALE_X;
    yflag = flags & KRB_FLAG_SCALE_Y;
    wflag = flags & KRB_FLAG_SCALE_W;
    hflag = flags & KRB_FLAG_SCALE_H;
    if(!add_rect_node(b, "lf_top", x, y, w, 1, xflag | yflag | wflag,
                      border) ||
       !add_rect_node(b, "lf_bottom", x, y + h - 1, w, 1,
                      xflag | yflag | hflag | wflag, border) ||
       !add_rect_node(b, "lf_left", x, y, 1, h, xflag | yflag | hflag,
                      border) ||
       !add_rect_node(b, "lf_right", x + w - 1, y, 1, h,
                      xflag | wflag | yflag | hflag, border))
        return 0;
    title[0] = '\0';
    if(f_title[0] != '\0')
        extract_string(f_title, title, sizeof(title));
    if(title[0] != '\0') {
        KrbBuildNode *n;
        char name[32];
        int bg_w = (int)strlen(title) * 7 + 16;

        if(!add_rect_node(b, "lf_title_bg", x + 8, y - 8, bg_w, 18,
                          xflag | yflag | KRB_FLAG_SCALE_W |
                          KRB_FLAG_SCALE_H,
                          KRB_COLOR_THEME | KRY_THEME_BACKGROUND))
            return 0;
        snprintf(name, sizeof(name), "lf_title%d", b->node_count);
        n = add_node(b, KRB_NODE_TEXT, name);
        if(n == NULL)
            return 0;
        n->x = x + 16;
        n->y = y - 9;
        n->flags = xflag | yflag;
        n->font_size = 14;
        n->color = KRB_COLOR_THEME | KRY_THEME_TEXT;
        copy_text(n->text, sizeof(n->text), title);
    }
    return 1;
}

/* Scroll(x, y, w, h, contentH, &offset) -> SCROLL node opening a child
 * range; EndScroll() closes it. Widgets between get parent = the scroll. */
/* TextField(x, y, w, h, &state.field) -> TEXTINPUT node. */
/* Register a .kry string-array initializer as a joined "a;b;c" option list. */
static void
register_string_array(KrbBuild *b, const char *name, const char *init)
{
    const char *p = kir_skip_inline_ws(init);
    size_t n = 0;

    if(p == NULL || *p != '{' || name == NULL || name[0] == '\0')
        return;
    if(b->array_count >= 8)
        return;
    snprintf(b->array_name[b->array_count], KIR_NAME_MAX, "%s", name);
    p++;
    while(*p != '\0' && *p != '}') {
        if(*p == '"') {
            const char *q = p + 1;

            while(*q != '\0' && *q != '"') {
                if(*q == '\\' && q[1] != '\0')
                    q++;
                q++;
            }
            if(q > p + 1 &&
               n + (size_t)(q - p - 1) < KIR_TEXT_MAX - 2) {
                memcpy(b->array_joined[b->array_count] + n, p + 1,
                       (size_t)(q - p - 1));
                n += (size_t)(q - p - 1);
            }
            b->array_joined[b->array_count][n++] = ';';
            p = q;
        }
        p++;
    }
    if(n > 0 && b->array_joined[b->array_count][n - 1] == ';')
        n--;
    b->array_joined[b->array_count][n] = '\0';
    if(n > 0)
        b->array_count++;
}

static void
collect_string_arrays(KrbBuild *b, const KirModule *m)
{
    int i;

    for(i = 0; i < m->state_count; i++) {
        const KirStateField *sf = &m->state_fields[i];

        if(strstr(sf->type, "char") != NULL && strchr(sf->type, '*') != NULL)
            register_string_array(b, sf->name, sf->init);
    }
}

/* Options expression: a ';'-joined string literal or a registered array
 * variable name ('choices' etc.). Returns 1 when a non-empty list was found. */
static int
resolve_options(KrbBuild *b, const char *expr, char *dst, size_t dst_size)
{
    const char *p = kir_skip_inline_ws(expr);
    int i;

    if(p == NULL)
        return 0;
    if(*p == '"') {
        extract_string(expr, dst, dst_size);
        return dst[0] != '\0';
    }
    for(i = 0; i < b->array_count; i++) {
        if(strcmp(b->array_name[i], p) == 0) {
            snprintf(dst, dst_size, "%s", b->array_joined[i]);
            return dst[0] != '\0';
        }
    }
    return 0;
}

/* Combobox((ComboboxProps){...}) -> COMBOBOX control. The C widget forwards
 * to the dropdown renderer, so the cartridge runtime does the same; options
 * come from a joined literal or a string-array variable. */
static int
parse_combobox(KrbBuild *b, const char *call)
{
    char parts[8][KIR_TEXT_MAX];
    char coords[4][KIR_TEXT_MAX];
    const char *props = strstr(call, "ComboboxProps");
    const char *brace;
    char inner[KIR_TEXT_MAX];
    char f_bounds[KIR_TEXT_MAX] = "";
    char f_opts[KIR_TEXT_MAX] = "";
    char f_count[KIR_TEXT_MAX] = "";
    char f_sel[KIR_TEXT_MAX] = "";
    char opts[KIR_TEXT_MAX];
    char path[KIR_NAME_MAX];
    KrbBuildNode *n;
    char name[32];
    int scaled;
    int count;
    int option_count;
    int ncoords;
    int i;

    if(props == NULL)
        return 0;
    brace = strchr(props, '{');
    if(brace == NULL)
        return 0;
    {
        const char *q = brace + 1;
        int depth = 1;
        int in_string = 0;
        size_t k = 0;

        while(*q != '\0' && depth > 0 && k + 1 < sizeof(inner)) {
            if(!in_string && *q == '{')
                depth++;
            else if(!in_string && *q == '}') {
                depth--;
                if(depth == 0)
                    break;
            } else if(*q == '"')
                in_string = !in_string;
            inner[k++] = *q++;
        }
        inner[k] = '\0';
        if(depth != 0)
            return 0;
    }
    count = split_args(inner, parts, 8);
    if(count < 5)
        return 0;
    /* positional: bounds, id, options, option_count, &selected, disabled */
    for(i = 0; i < count && i < 6; i++) {
        const char *part = kir_skip_inline_ws(parts[i]);

        if(*part == '.') {
            const char *eq = strchr(part, '=');

            if(eq == NULL)
                continue;
            if(strncmp(part, ".bounds", 7) == 0)
                snprintf(f_bounds, sizeof(f_bounds), "%s", kir_skip_inline_ws(eq + 1));
            else if(strncmp(part, ".options", 8) == 0)
                snprintf(f_opts, sizeof(f_opts), "%s", kir_skip_inline_ws(eq + 1));
            else if(strncmp(part, ".option_count", 13) == 0)
                snprintf(f_count, sizeof(f_count), "%s", kir_skip_inline_ws(eq + 1));
            else if(strncmp(part, ".selected_index", 16) == 0)
                snprintf(f_sel, sizeof(f_sel), "%s", kir_skip_inline_ws(eq + 1));
        } else {
            switch(i) {
            case 0: snprintf(f_bounds, sizeof(f_bounds), "%s", part); break;
            case 2: snprintf(f_opts, sizeof(f_opts), "%s", part); break;
            case 3: snprintf(f_count, sizeof(f_count), "%s", part); break;
            case 4: snprintf(f_sel, sizeof(f_sel), "%s", part); break;
            default: break;
            }
        }
    }
    if(f_bounds[0] != '{')
        return 0;
    strip_amp(f_sel, path, sizeof(path));
    if(path[0] == '\0')
        return 0;
    if(!resolve_options(b, f_opts, opts, sizeof(opts)))
        return 0;
    option_count = atoi(f_count);
    if(option_count <= 0) {
        const char *p2 = opts;

        option_count = 1;
        while((p2 = strchr(p2, ';')) != NULL) {
            option_count++;
            p2++;
        }
    }
    {
        const char *bp = f_bounds;
        char bounds_inner[KIR_TEXT_MAX];
        size_t bn = 0;
        int bd = 0;

        while(*bp != '\0') {
            if(*bp == '{')
                bd++;
            else if(*bp == '}') {
                bd--;
                if(bd == 0)
                    break;
            } else if(bd > 0 && bn + 1 < sizeof(bounds_inner))
                bounds_inner[bn++] = *bp;
            bp++;
        }
        bounds_inner[bn] = '\0';
        ncoords = split_args(bounds_inner, coords, 4);
        if(ncoords < 4)
            return 0;
    }
    snprintf(name, sizeof(name), "cb%d", b->node_count);
    n = add_node(b, KRB_NODE_CONTROL, name);
    if(n == NULL)
        return 0;
    n->bind_slot = b->control_count;
    if(parse_coord(coords[0], &n->x, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_X;
    if(parse_coord(coords[1], &n->y, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_Y;
    if(parse_coord(coords[2], &n->w, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_W;
    if(parse_coord(coords[3], &n->h, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_H;
    if(n->h <= 0)
        n->h = 24;
    n->font_size = 16;
    {
        KrbBuildControl *c = &b->controls[b->control_count];

        if(b->control_count >= KRB_BUILD_CTRL_MAX)
            return 1;
        memset(c, 0, sizeof(*c));
        c->kind = KRB_CTRL_COMBOBOX;
        c->id = (unsigned short)b->control_count;
        snprintf(c->value, sizeof(c->value), "%s", path);
        snprintf(c->options, sizeof(c->options), "%s", opts);
        c->option_count = option_count;
        b->control_count++;
    }
    return 1;
}

/* Dropdown(id, x, y, w, "opt;opt;opt", &val) -> DROPDOWN control. */
static int
parse_dropdown(KrbBuild *b, const char *call)
{
    char parts[8][KIR_TEXT_MAX];
    const char *args = strchr(call, '(');
    char path[KIR_NAME_MAX];
    char opts[KIR_TEXT_MAX];
    KrbBuildNode *n;
    char name[32];
    int scaled;
    int count;
    int dd_h = 0;

    if(args == NULL)
        return 0;
    count = split_args(args + 1, parts, 8);
    if(count < 6)
        return 0;
    if(count >= 7) {
        /* Dropdown(id, x, y, w, h, "opts", &val) */
        strip_amp(parts[6], path, sizeof(path));
        if(path[0] == '\0')
            return 0;
        if(!resolve_options(b, parts[5], opts, sizeof(opts)))
            return 0;
        {
            KrbBuildNode dummy;
            int sc2;

            (void)parse_coord(parts[4], &dummy.x, &sc2);
            dd_h = dummy.x;
        }
    } else {
        strip_amp(parts[5], path, sizeof(path));
        if(path[0] == '\0')
            return 0;
        if(!resolve_options(b, parts[4], opts, sizeof(opts)))
            return 0;
    }
    if(opts[0] == '\0')
        return 0;
    snprintf(name, sizeof(name), "dd%d", b->node_count);
    n = add_node(b, KRB_NODE_CONTROL, name);
    if(n == NULL)
        return 0;
    n->bind_slot = b->control_count;
    if(parse_coord(parts[1], &n->x, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_X;
    if(parse_coord(parts[2], &n->y, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_Y;
    if(parse_coord(parts[3], &n->w, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_W;
    n->h = dd_h > 0 ? dd_h : 24;
    n->font_size = 16;
    {
        KrbBuildControl *c = &b->controls[b->control_count];

        if(b->control_count >= KRB_BUILD_CTRL_MAX)
            return 1;
        memset(c, 0, sizeof(*c));
        c->kind = KRB_CTRL_DROPDOWN;
        c->id = (unsigned short)b->control_count;
        c->min = 0;
        c->max = 0;
        c->step = 1;
        snprintf(c->value, sizeof(c->value), "%s", path);
        snprintf(c->options, sizeof(c->options), "%s", opts);
        {
            const char *p = c->options;

            c->option_count = 1;
            while((p = strchr(p, ';')) != NULL) {
                c->option_count++;
                p++;
            }
        }
        b->control_count++;
    }
    return 1;
}

/* NavButton(label, x, y, w, h, &state.screen, id) -> BUTTON with the
 * NAV flag: on press the runtime writes id to the screen state path. */
/* AnimNode("nodename", "w", &state.path) -> NODE_SET emission: the
 * node's geometry field tracks the state value every frame. */
static int
parse_animnode(KrbBuild *b, const char *call)
{
    char parts[6][KIR_TEXT_MAX];
    const char *args = strchr(call, '(');
    char path[KIR_NAME_MAX];
    char field_name[8];
    KrbAnim *a;

    if(args == NULL)
        return 0;
    if(split_args(args + 1, parts, 6) < 3)
        return 0;
    strip_amp(parts[2], path, sizeof(path));
    if(path[0] == '\0')
        return 0;
    extract_string(parts[1], field_name, sizeof(field_name));
    if(b->anim_count >= 32)
        return 1;
    a = &b->anims[b->anim_count++];
    extract_string(parts[0], a->node_name, sizeof(a->node_name));
    a->field = field_name[0] == 'x' ? 0 : field_name[0] == 'y' ? 1 :
               field_name[0] == 'w' ? 2 : field_name[0] == 'h' ? 3 : 2;
    snprintf(a->path, sizeof(a->path), "%s", path);
    a->node = -1;
    return 1;
}

static int
parse_navbutton(KrbBuild *b, const char *call)
{
    char parts[8][KIR_TEXT_MAX];
    const char *args = strchr(call, '(');
    char path[KIR_NAME_MAX];
    char name[32];
    KrbBuildNode *n;
    int scaled;
    int count;

    if(args == NULL)
        return 0;
    count = split_args(args + 1, parts, 8);
    if(count < 7)
        return 0;
    strip_amp(parts[5], path, sizeof(path));
    if(path[0] == '\0')
        return 0;
    snprintf(name, sizeof(name), "nav%d", b->node_count);
    n = add_node(b, KRB_NODE_BUTTON, name);
    if(n == NULL)
        return 0;
    extract_string(parts[0], n->text, sizeof(n->text));
    snprintf(n->name, sizeof(n->name), "%s", path);
    n->flags |= KRB_FLAG_NAV;
    if(parse_coord(parts[1], &n->x, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_X;
    if(parse_coord(parts[2], &n->y, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_Y;
    if(parse_coord(parts[3], &n->w, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_W;
    if(parse_coord(parts[4], &n->h, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_H;
    n->font_size = (unsigned short)atoi(kir_skip_inline_ws(parts[6]));
    n->bind_slot = -1;
    n->color = 0x80000004u;
    return 1;
}

static int
parse_textfield(KrbBuild *b, const char *call)
{
    char parts[8][KIR_TEXT_MAX];
    const char *args = strchr(call, '(');
    char path[KIR_NAME_MAX];
    char name[32];
    KrbBuildNode *n;
    int scaled;
    int count;
    int aggregate = 0;
    int ax = 0, ay = 0, aw = 0, ah = 0;
    unsigned aggregate_flags = 0;

    if(args == NULL)
        return 0;
    count = split_args(args + 1, parts, 8);
    if(count < 5) {
        const char *bounds = strstr(args, ".bounds");
        const char *text = strstr(args, ".text =");
        const char *p;

        if(bounds == NULL || text == NULL)
            return 0;
        {
            KrbBuildNode tmp;

            memset(&tmp, 0, sizeof(tmp));
            if(!node_rect(&tmp, bounds))
                return 0;
            ax = tmp.x;
            ay = tmp.y;
            aw = tmp.w;
            ah = tmp.h;
            aggregate_flags = tmp.flags;
        }
        if(aw <= 0 || ah <= 0)
            return 0;
        p = text + strlen(".text =");
        p = kir_skip_inline_ws(p);
        {
            size_t len = 0;
            while((isalnum((unsigned char)p[len]) || p[len] == '_' ||
                   p[len] == '.') && len + 1 < sizeof(path))
                len++;
            memcpy(path, p, len);
            path[len] = '\0';
        }
        aggregate = 1;
    } else {
        strip_amp(parts[4], path, sizeof(path));
    }
    if(path[0] == '\0')
        return 0;
    snprintf(name, sizeof(name), "field%d", b->node_count);
    n = add_node(b, KRB_NODE_TEXTINPUT, name);
    if(n == NULL)
        return 0;
    snprintf(n->name, sizeof(n->name), "%s", path);
    if(aggregate) {
        n->x = ax;
        n->y = ay;
        n->w = aw;
        n->h = ah;
        n->flags = aggregate_flags;
        n->font_size = 16;
        return 1;
    }
    if(parse_coord(parts[0], &n->x, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_X;
    if(parse_coord(parts[1], &n->y, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_Y;
    if(parse_coord(parts[2], &n->w, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_W;
    if(parse_coord(parts[3], &n->h, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_H;
    n->font_size = 16;
    return 1;
}

static int
parse_scroll(KrbBuild *b, const char *call)
{
    char parts[8][KIR_TEXT_MAX];
    const char *args = strchr(call, '(');
    char name[32];
    char path[KIR_NAME_MAX];
    KrbBuildNode *n;
    int scaled;
    int count;

    if(args == NULL)
        return 0;
    count = split_args(args + 1, parts, 8);
    if(count < 6)
        return 0;
    strip_amp(parts[5], path, sizeof(path));
    snprintf(name, sizeof(name), "scroll%d", b->node_count);
    n = add_node(b, KRB_NODE_SCROLL, name);
    if(n == NULL)
        return 0;
    snprintf(n->text, sizeof(n->text), "%s", path); /* name_off comes from node name interning; we need the mount path as name_off instead */
    if(parse_coord(parts[0], &n->x, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_X;
    if(parse_coord(parts[1], &n->y, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_Y;
    if(parse_coord(parts[2], &n->w, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_W;
    if(parse_coord(parts[3], &n->h, &scaled) && scaled)
        n->flags |= KRB_FLAG_SCALE_H;
    {
        int sc3;

        if(parse_coord(parts[4], &n->font_size, &sc3) && sc3)
            n->flags |= KRB_FLAG_SCALE_H; /* approximate: scale with UI */
    }
    snprintf(n->name, sizeof(n->name), "%s", path); /* mount path */
    return 1;
}

typedef int (*KrbWidgetParser)(KrbBuild *b, const char *call);

typedef struct KrbWidgetParserEntry {
    const char *name;
    KrbWidgetParser parse;
} KrbWidgetParserEntry;

static int
is_krb_layout_call(const char *call)
{
    static const char *const names[] = {
        "BeginTree", "Column", "Row", "Stack", "EndTree", "End", NULL
    };

    for(int i = 0; names[i] != NULL; i++)
        if(starts_ident(call, names[i]))
            return 1;
    return 0;
}

static int
is_kry_widget_call(const char *name)
{
    static const char *const names[] = {
        "Background", "Text", "TextInRect", "Paragraph", "TextLines",
        "Rect", "Line", "Bevel", "Icon", "Picture", "Button",
        "IconButton", "Href", "TextField", "TextArea", "Dropdown",
        "Slider", "Toggle", "Checkbox", "Radio", "Progress", "Spinbox",
        "Combobox", "Screen", "Column", "Row", "Stack", "End", "Scroll",
        "Canvas", "Modal", "ActionModal", "MessageDialog",
        "ConfirmDialog", "PromptDialog", "TitleBar", "TabBar", "BottomNav",
        "TopNav", "Toolbar", "ShowToast", "ShowToastFor", "LabelFrame",
        "Notebook", "PanedView", "Collapsible", "ListBox", "SourceView",
        "TableView", "CanvasGrid", "SelectableText", "InfoButton", NULL
    };

    for(int i = 0; names[i] != NULL; i++)
        if(strcmp(name, names[i]) == 0)
            return 1;
    return 0;
}

static int
parse_widget_from_table(KrbBuild *b, const char *call)
{
    static const KrbWidgetParserEntry entries[] = {
        { "Background", parse_background },
        { "Rect", parse_rect },
        { "Scroll", parse_scroll },
        { "TextField", parse_textfield },
        { "Dropdown", parse_dropdown },
        { "Combobox", parse_combobox },
        { "NavButton", parse_navbutton },
        { "TextFormat", parse_text },
        { "AnimNode", parse_animnode },
        { "DrawCircleV", parse_circle },
        { "DrawRing", parse_ring },
        { "Separator", parse_separator },
        { "Line", parse_line },
        { "Bevel", parse_bevel },
        { "TextInRect", parse_textinrect },
        { "Picture", parse_picture },
        { "Checkbox", parse_checkbox },
        { "Radio", parse_radio },
        { "Toggle", parse_toggle },
        { "Slider", parse_slider },
        { "Spinbox", parse_spinbox },
        { "Progress", parse_progress },
        { "LabelFrame", parse_labelframe },
        { NULL, NULL }
    };

    for(int i = 0; entries[i].name != NULL; i++)
        if(starts_ident(call, entries[i].name))
            return entries[i].parse(b, call);
    return -1;
}

static int
try_widget(KrbBuild *b, const char *raw)
{
    const char *text = kir_skip_inline_ws(raw);
    const char *call;
    int parsed;

    if(strncmp(text, "PushUIInspectSource(", 20) == 0 ||
       strncmp(text, "PopUIInspectSource();", 21) == 0)
        return 0;
    call = call_after_eq(text);
    call = kir_skip_inline_ws(call);
    /* Retained declaration scopes are represented by the cartridge node
     * table itself; they are semantic no-ops for the KRB renderer. */
    if(starts_ident(call, "BeginUIScrollContainer"))
        return parse_begin_scroll_container(b, raw, call);
    if(starts_ident(call, "EndUIScrollContainer")) {
        b->scroll_open = -1;
        return 1;
    }
    if(starts_ident(call, "EndScroll")) {
        b->scroll_open = -1;
        return 1;
    }
    if(is_krb_layout_call(call))
        return 1;
    if(starts_ident(call, "Text") && !starts_ident(call, "TextFormat") &&
       !starts_ident(call, "TextInRect") && !starts_ident(call, "TextLines") &&
       !starts_ident(call, "TextField") &&
       !starts_ident(call, "TextArea"))
        return parse_text(b, call);
    if(strstr(call, "Button((") != NULL || strstr(call, "ButtonProps") != NULL)
        return parse_button(b, call);
    parsed = parse_widget_from_table(b, call);
    if(parsed >= 0)
        return parsed;
    {
        char name[64];
        const char *p = call;
        int i = 0;

        while(p[i] != '\0' && p[i] != '(' && i < (int)sizeof(name) - 1) {
            name[i] = p[i];
            i++;
        }
        name[i] = '\0';
        if(!is_kry_widget_call(name))
            return 0;
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


int
emit_prog(KrbBuild *b, const unsigned char *dst, int cap,
          const int *upath_off, const int *gpath_off,
          const int grpath_off[16][4])
{
    unsigned char *q = (unsigned char *)dst;
    int i;
    int jz_patch = -1;
    int jmp_patch = -1;

#define EMIT1(v) do { if(q - dst >= cap) return -1; *q++ = (unsigned char)(v); } while(0)
#define EMIT2(v) do { if(q - dst + 2 > cap) return -1; wr_u16(&q, (unsigned)v); } while(0)
#define EMIT4(v) do { if(q - dst + 4 > cap) return -1; wr_u32(&q, (unsigned long)(v)); } while(0)
#define PATCH(off) do { unsigned char *pp = dst + (off); wr_u32(&pp, \
                          (unsigned long)(q - dst)); } while(0)

    (void)gpath_off;
    if(b->update_count == 0 && b->guard_count == 0 && b->anim_count == 0) {
        EMIT1(KRB_OP_DRAW_TREE);
        return (int)(q - dst);
    }
    for(i = 0; i < b->anim_count; i++) {
        int k;

        for(k = 0; k < b->node_count; k++) {
            if(strcmp(b->nodes[k].name, b->anims[i].node_name) == 0)
                break;
        }
        if(k >= b->node_count)
            continue;
        EMIT1(KRB_OP_PUSH_PATH);
        EMIT2(intern_string(b, b->anims[i].path));
        EMIT1(KRB_OP_NODE_SET);
        EMIT2((unsigned)k);
        EMIT1(b->anims[i].field);
    }
    for(i = 0; i < b->item_count; i++) {
        const KrbItem *it = &b->items[i];

        if(it->kind == 2) {
            /* guard open: compare + JZ over the true branch */
            const KrbGuard *gd = &b->guards[it->idx];
            int side;
            int ti;

            for(side = 0; side < 2; side++) {
                const KrbTerm *ts = side == 0 ? gd->l : gd->r;
                int tn = side == 0 ? gd->ln : gd->rn;

                for(ti = 0; ti < tn; ti++) {
                    if(ts[ti].is_path == 2)
                        EMIT1(KRB_OP_TIME);
                    else if(ts[ti].is_path == 1) {
                        EMIT1(KRB_OP_PUSH_PATH);
                        EMIT2(grpath_off[it->idx][side * 2 + ti]);
                    } else {
                        EMIT1(KRB_OP_PUSH_CONST);
                        EMIT4(ts[ti].val);
                    }
                }
                if(tn == 2)
                    EMIT1(side == 0 ? gd->lop : gd->rop);
            }
            EMIT1(gd->op);
            EMIT1(KRB_OP_JZ);
            jz_patch = (int)(q - dst);
            EMIT4(0);
        } else if(it->kind == 3) {
            /* else: end of true branch; jump over the false branch */
            EMIT1(KRB_OP_JMP);
            jmp_patch = (int)(q - dst);
            EMIT4(0);
            if(jz_patch >= 0) {
                PATCH(jz_patch);
                jz_patch = -1;
            }
        } else if(it->kind == 4) {
            /* guard close: land here after either branch */
            if(jz_patch >= 0) {
                PATCH(jz_patch);
                jz_patch = -1;
            }
            if(jmp_patch >= 0) {
                PATCH(jmp_patch);
                jmp_patch = -1;
            }
        } else if(it->kind == 0) {
            const KrbUpdate *u = &b->updates[it->idx];

            if(u->rhs_n > 0) {
                int t;

                for(t = 0; t < u->rhs_n; t++) {
                    if(u->rhs[t].is_path == 2)
                        EMIT1(KRB_OP_TIME);
                    else if(u->rhs[t].is_path == 1) {
                        EMIT1(KRB_OP_PUSH_PATH);
                        EMIT2(intern_string(b, u->rhs[t].path));
                    } else {
                        EMIT1(KRB_OP_PUSH_CONST);
                        EMIT4(u->rhs[t].val);
                    }
                }
                if(u->rhs_n == 2)
                    EMIT1(u->rhs_op);
            } else if(u->kind == 0) {
                EMIT1(KRB_OP_PUSH_CONST);
                EMIT4(u->val);
            } else {
                EMIT1(KRB_OP_PUSH_PATH);
                EMIT2(upath_off[it->idx]);
                EMIT1(KRB_OP_PUSH_CONST);
                EMIT4(u->val);
                EMIT1(u->kind == 1 ? KRB_OP_ADD : KRB_OP_SUB);
            }
            EMIT1(KRB_OP_POP_STORE);
            EMIT2(upath_off[it->idx]);
        } else if(it->kind == 1) {
            if(b->nodes[it->idx].type == KRB_NODE_DATA)
                continue;
            EMIT1(KRB_OP_DRAW_NODE);
            EMIT2((unsigned)it->idx);
        }
    }
    if(jz_patch >= 0)
        PATCH(jz_patch);
    if(jmp_patch >= 0)
        PATCH(jmp_patch);
#undef EMIT1
#undef EMIT2
#undef EMIT4
#undef PATCH
    return (int)(q - dst);
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
    int copt_off[KRB_BUILD_CTRL_MAX];
    unsigned char prog_buf[4096];
    int prog_bytes = 1;
    static int apath_off[24];
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
        copt_off[i] = 0;
        if(b->controls[i].option_count > 0) {
            char opt[KIR_TEXT_MAX];
            const char *p = b->controls[i].options;
            int k;

            copt_off[i] = intern_string(b, ""); /* reset marker */
            for(k = 0; k < b->controls[i].option_count; k++) {
                const char *semi = strchr(p, ';');
                size_t len = semi != NULL ? (size_t)(semi - p) : strlen(p);

                if(len >= sizeof(opt))
                    len = sizeof(opt) - 1;
                memcpy(opt, p, len);
                opt[len] = '\0';
                if(k == 0)
                    copt_off[i] = intern_string(b, opt);
                else
                    intern_string(b, opt); /* must land contiguously */
                p = semi != NULL ? semi + 1 : p + len;
            }
        }
    }
    for(i = 0; i < b->asset_count; i++)
        apath_off[i] = intern_string(b, b->assets[i].path);
    {
        static int upath_off[32];
        static int gpath_off[16];
        static int grpath_off[16][4];

        for(i = 0; i < b->update_count; i++)
            upath_off[i] = intern_string(b, b->updates[i].path);
        for(i = 0; i < b->guard_count; i++) {
            const KrbGuard *gd = &b->guards[i];
            const KrbTerm *ts[2] = { gd->l, gd->r };
            int tn[2] = { gd->ln, gd->rn };
            int side;
            int ti;

            gpath_off[i] = 0;
            for(side = 0; side < 2; side++) {
                for(ti = 0; ti < tn[side]; ti++) {
                    int off = 0;

                    if(ts[side][ti].is_path)
                        off = intern_string(b, ts[side][ti].path);
                    grpath_off[i][side * 2 + ti] = off;
                    if(side == 0 && ti == 0)
                        gpath_off[i] = off;
                }
            }
        }
        prog_bytes = emit_prog(b, prog_buf, sizeof(prog_buf), upath_off,
                               gpath_off, grpath_off);
        if(prog_bytes < 0)
            return -1;
    }

    need = 32 + b->node_count * 28 + b->string_used + prog_bytes +
           b->import_count * 4 + b->control_count * KRB_CONTROL_SIZE;
    if(b->asset_count > 0)
        need += 4 + b->asset_count * 20; /* blobs checked again at copy */
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
        *cp++ = (unsigned char)(ctl->option_count > 0 ? ctl->option_count
                                                      : 0);
        wr_u16(&cp, ctl->id);
        wr_u32(&cp, (unsigned)ctl->min);
        wr_u32(&cp, (unsigned)ctl->max);
        wr_u32(&cp, (unsigned)ctl->step);
        wr_u16(&cp, (unsigned)cvalue_off[i]);
        wr_u16(&cp, (unsigned)clabel_off[i]);
        wr_u16(&cp, (unsigned)copt_off[i]); /* options_off */
        wr_u16(&cp, 0);                    /* reserved */
        memcpy(out, c, KRB_CONTROL_SIZE);
        out += KRB_CONTROL_SIZE;
    }
    /* v2 asset section: u32 count, 20-byte entries, then blobs */
    if(b->asset_count > 0) {
        unsigned long blob_base;
        unsigned long need_assets_mark = (unsigned long)(out - dst);
        FILE *af[24];
        unsigned long asize[24];
        int k;
        unsigned long total;

        total = 0;
        for(k = 0; k < b->asset_count; k++) {
            af[k] = NULL;
            asize[k] = 0;
            if(b->assets[k].mem != NULL) {
                asize[k] = b->assets[k].mem_len;
            } else if((af[k] = fopen(b->assets[k].file, "rb")) != NULL) {
                fseek(af[k], 0, SEEK_END);
                asize[k] = (unsigned long)ftell(af[k]);
                fseek(af[k], 0, SEEK_SET);
                if(b->assets[k].kind == 0 && asize[k] >= 8)
                    asize[k] -= 8; /* strip the KRAW header */
            }
            total += asize[k];
        }
        blob_base = (unsigned long)(out - dst) + 4 +
                    (unsigned long)b->asset_count * 20;
        wr_u32(&out, (unsigned)b->asset_count);
        for(k = 0; k < b->asset_count; k++) {
            wr_u32(&out, (unsigned)apath_off[k]);
            wr_u32(&out, (unsigned)(blob_base));
            blob_base += asize[k];
            wr_u32(&out, (unsigned)asize[k]);
            wr_u16(&out, (unsigned)b->assets[k].kind);
            wr_u16(&out, b->assets[k].w);
            wr_u16(&out, b->assets[k].h);
            wr_u16(&out, 0);
        }
        for(k = 0; k < b->asset_count; k++) {
            unsigned long left = asize[k];
            const unsigned char *src = b->assets[k].mem;

            if(src != NULL) {
                if(out - dst + (long)left > cap)
                    return -1;
                memcpy(out, src, (size_t)left);
                out += left;
                continue;
            }
            if(af[k] == NULL)
                continue;
            if(b->assets[k].kind == 0)
                fseek(af[k], 8, SEEK_SET); /* skip KRAW header */
            while(left > 0) {
                unsigned char chunk[4096];
                size_t got = fread(chunk, 1, left < sizeof(chunk) ? left
                                                    : sizeof(chunk), af[k]);

                if(got == 0)
                    break;
                if(out - dst + (long)got > cap) {
                    fclose(af[k]);
                    return -1;
                }
                memcpy(out, chunk, got);
                out += got;
                left -= got;
            }
            fclose(af[k]);
        }
        {
            unsigned long asset_bytes = (unsigned long)(out - dst) -
                (unsigned long)need_assets_mark;

            dst[28] = (unsigned char)asset_bytes;
            dst[29] = (unsigned char)(asset_bytes >> 8);
            dst[30] = (unsigned char)(asset_bytes >> 16);
            dst[31] = (unsigned char)(asset_bytes >> 24);
        }
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
        fprintf(out, "        EndUIFrame();\n");
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
          int no_main, int allow_unsupported)
{
    const char *rel = relative_path(root, m->source_path);
    char gen_rel[KIR_PATH_MAX];
    char krel[KIR_PATH_MAX];
    char kpath[KIR_PATH_MAX];
    static unsigned char bytes[KRB_OUT_MAX];
    KrbBuild build;
    FILE *out;
    int i;
    int len;

    strip_kry_ext(gen_rel, sizeof(gen_rel), rel);
    snprintf(krel, sizeof(krel), "%s.krb", gen_rel);
    path_join(kpath, sizeof(kpath), out_dir, krel);
    mkdir_parent(kpath);

    memset(&build, 0, sizeof(build));
    build.scroll_open = -1;
    build.strings[0] = '\0';
    build.string_used = 1;
    snprintf(build.asset_root, sizeof(build.asset_root), "%s",
             root != NULL ? root : ".");
    collect_state(&build, m);
    collect_string_arrays(&build, m);
    for(i = 0; i < m->function_count; i++)
        collect_widgets(&build, &m->functions[i]);

    if(build.dropped_kinds > 0 && !allow_unsupported) {
        fprintf(stderr, "k2b: %s: unsupported calls:", rel);
        for(i = 0; i < build.dropped_kinds; i++)
            fprintf(stderr, " %s x%d", build.dropped[i],
                    build.dropped_count[i]);
        fprintf(stderr, " (pass --allow-unsupported to omit them)\n");
        exit(1);
    }

    /* bake the glyph atlas from the UI font over the cartridge charset */
    if(build.asset_count < 24) {
        unsigned cps[512];
        int cp_count = 0;
        int sizes[4];
        int size_count = 0;
        int i;
        int j;
        int k;

        for(i = 32; i <= 126 && cp_count < 512; i++)
            cps[cp_count++] = (unsigned)i;
        for(i = 0; i < build.node_count && cp_count < 512; i++) {
            const char *t = build.nodes[i].text;

            if(t == NULL)
                continue;
            for(j = 0; t[j] != '\0' && cp_count < 512;) {
                const char *p = t + j;
                unsigned cp = 0;
                unsigned char c0 = (unsigned char)p[0];

                if(c0 < 0x80) {
                    cp = c0;
                    j++;
                } else if((c0 & 0xe0) == 0xc0) {
                    cp = ((c0 & 0x1f) << 6) | (p[1] & 0x3f);
                    j += 2;
                } else if((c0 & 0xf0) == 0xe0) {
                    cp = ((c0 & 0x0f) << 12) | ((p[1] & 0x3f) << 6) |
                         (p[2] & 0x3f);
                    j += 3;
                } else {
                    cp = '?';
                    j++;
                }
                for(k = 0; k < cp_count; k++)
                    if(cps[k] == cp)
                        break;
                if(k == cp_count)
                    cps[cp_count++] = cp;
            }
            if(build.nodes[i].font_size > 0 && size_count < 4 &&
               (build.nodes[i].type == KRB_NODE_TEXT ||
                build.nodes[i].type == KRB_NODE_TEXTINPUT ||
                build.nodes[i].type == KRB_NODE_CONTROL ||
                build.nodes[i].type == KRB_NODE_BUTTON)) {
                int fs = build.nodes[i].font_size;

                for(k = 0; k < size_count; k++)
                    if(sizes[k] == fs)
                        break;
                if(k == size_count)
                    sizes[size_count++] = fs;
            }
        }
        if(size_count == 0)
            sizes[size_count++] = 16;
        {
            /* physical tiers: the engine renders font_size through
             * scale_px, so bake at the physical pixel sizes */
            const char *us = getenv("K2B_UI_SCALE");
            float uis = us != NULL ? (float)atof(us) : 1.0f;
            int fi2;

            if(uis <= 0.0f || uis > 8.0f)
                uis = 1.0f;
            for(fi2 = 0; fi2 < size_count; fi2++)
                sizes[fi2] = (int)((float)sizes[fi2] * uis + 0.5f);
        }
        {
            const char *font = getenv("K2B_FONT");

            if(font == NULL || font[0] == '\0')
                font = "fonts/noto/NotoSans-Regular.ttf";
            {
                unsigned alen = 0;
                unsigned char *atlas = k2b_bake_atlas(font, cps, cp_count,
                                                      sizes, size_count,
                                                      &alen);

                if(atlas != NULL && build.asset_count < 24) {
                    KrbAsset *a = &build.assets[build.asset_count++];

                    snprintf(a->path, sizeof(a->path), "@atlas");
                    a->file[0] = '\0';
                    a->mem = atlas;
                    a->mem_len = alen;
                    a->kind = 1;
                    a->w = 0;
                    a->h = 0;
                } else {
                    free(atlas);
                }
            }
        }
    }
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
