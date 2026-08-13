/* Emit a krb cartridge from reconstructed AST widget calls. */
#include "kc_ast.h"
#include "kc_internal.h"
#include "krb.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KRB_BUILD_NODE_MAX 256
#define KRB_BUILD_STR_MAX 8192
#define KRB_BUILD_IMPORT_MAX 32
#define KRB_BUILD_HANDLER_LINES 16
#define KRB_OUT_MAX 65536

typedef struct KrbBuildNode {
    char name[KC_NAME_MAX];
    int type;
    int parent;
    unsigned flags;
    int bind_slot;
    int x;
    int y;
    int w;
    int h;
    unsigned color;
    char text[KC_BODY_LINE_MAX];
    int font_size;
    int style;
} KrbBuildNode;

typedef struct KrbHandler {
    char name[KC_NAME_MAX];
    char body[KRB_BUILD_HANDLER_LINES][KC_BODY_LINE_MAX];
    int body_count;
} KrbHandler;

typedef struct KrbStateField {
    char name[KC_NAME_MAX];
    char decl[KC_BODY_LINE_MAX];
    unsigned kind;
    unsigned size;
} KrbStateField;

typedef struct KrbBuild {
    KrbBuildNode nodes[KRB_BUILD_NODE_MAX];
    int node_count;
    char strings[KRB_BUILD_STR_MAX];
    int string_used;
    char imports[KRB_BUILD_IMPORT_MAX][KC_NAME_MAX];
    int import_count;
    KrbHandler handlers[KRB_BUILD_IMPORT_MAX];
    int handler_count;
    KrbStateField fields[32];
    int field_count;
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

static unsigned
parse_color(const char *expr)
{
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
split_args(const char *src, char parts[][KC_BODY_LINE_MAX], int max)
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
            if(n >= KC_BODY_LINE_MAX)
                n = KC_BODY_LINE_MAX - 1;
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
    const char *eq = strstr(text, " = ");

    if(eq != NULL)
        return skip_ws(eq + 3);
    return text;
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
    char parts[8][KC_BODY_LINE_MAX];
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
        char ident[KC_NAME_MAX];
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
        char ident[KC_NAME_MAX];

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

static int
parse_rect(KrbBuild *b, const char *call)
{
    const char *args = strchr(call, '(');
    char parts[8][KC_BODY_LINE_MAX];
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
    char slug[KC_NAME_MAX];
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
        char parts[4][KC_BODY_LINE_MAX];
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
    snprintf(h->body[h->body_count], sizeof(h->body[0]), "%s", t);
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

static void
collect_widgets(KrbBuild *b, const AstFunction *af)
{
    int j;
    int last_button = -1;

    if(af == NULL)
        return;
    for(j = 0; j < af->stmt_count; j++) {
        const char *raw = af->stmts[j].text;
        int before = b->node_count;

        if(try_widget(b, raw)) {
            if(b->node_count > before &&
               b->nodes[b->node_count - 1].type == 4)
                last_button = b->node_count - 1;
            continue;
        }
        if(last_button >= 0 && af->stmts[j].kind == AST_STMT_IF) {
            KrbHandler *h = handler_for(b, b->nodes[last_button].name);
            int depth = af->stmts[j].depth;
            int k;

            if(h != NULL) {
                for(k = j + 1; k < af->stmt_count; k++) {
                    if(af->stmts[k].kind == AST_STMT_BLOCK_CLOSE &&
                       af->stmts[k].depth <= depth)
                        break;
                    add_handler_line(h, af->stmts[k].text);
                }
            }
            last_button = -1;
        }
    }
}

static void
collect_state(KrbBuild *b, const KryFile *file)
{
    int i;

    for(i = 0; i < file->state_count; i++) {
        const char *s = skip_ws(file->state[i]);
        KrbStateField *f;
        KrbBuildNode *n;
        char name[KC_NAME_MAX];
        const char *p;
        unsigned kind = 1; /* KRB_I32 */
        unsigned size = 4;

        if(strncmp(s, "static ", 7) == 0)
            s = skip_ws(s + 7);
        if(strncmp(s, "char ", 5) == 0) {
            const char *br;

            s = skip_ws(s + 5);
            kind = 5; /* KRB_CSTR */
            p = s;
            while((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                  (*p >= '0' && *p <= '9') || *p == '_')
                p++;
            snprintf(name, sizeof(name), "%.*s", (int)(p - s), s);
            br = strchr(s, '[');
            size = br != NULL ? (unsigned)atoi(br + 1) : 64;
        } else {
            /* skip type words */
            p = s;
            while(*p != '\0' && *p != '=' && *p != ';')
                p++;
            while(p > s && (p[-1] == ' ' || p[-1] == '\t' || p[-1] == '*'))
                p--;
            {
                const char *start = p;
                while(start > s && ((start[-1] >= 'a' && start[-1] <= 'z') ||
                                    (start[-1] >= 'A' && start[-1] <= 'Z') ||
                                    (start[-1] >= '0' && start[-1] <= '9') ||
                                    start[-1] == '_'))
                    start--;
                snprintf(name, sizeof(name), "%.*s", (int)(p - start), start);
            }
            kind = 1;
            size = 4;
        }
        if(name[0] == '\0' || b->field_count >= 32)
            continue;
        f = &b->fields[b->field_count++];
        snprintf(f->name, sizeof(f->name), "%s", name);
        snprintf(f->decl, sizeof(f->decl), "%s", file->state[i]);
        f->kind = kind;
        f->size = size;
        n = add_node(b, 5, name);
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
    char body[KC_BODY_LINE_MAX];
    char parts[4][KC_BODY_LINE_MAX];
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
    char parts[8][KC_BODY_LINE_MAX];
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
    char parts[8][KC_BODY_LINE_MAX];
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
    char parts[8][KC_BODY_LINE_MAX];
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
    char body[KC_BODY_LINE_MAX];
    char parts[8][KC_BODY_LINE_MAX];
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
    char parts[8][KC_BODY_LINE_MAX];
    char path[KC_NAME_MAX];
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
    char parts[8][KC_BODY_LINE_MAX];
    char path[KC_NAME_MAX];
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
    return 0;
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

    need = 32 + b->node_count * 28 + b->string_used + 1 + b->import_count * 4;
    if(need > cap)
        return -1;
    wr_u32(&hp, KRB_MAGIC);
    wr_u16(&hp, 1);
    wr_u16(&hp, 0);
    wr_u32(&hp, (unsigned)b->node_count);
    wr_u32(&hp, (unsigned)b->string_used);
    wr_u32(&hp, 1);
    wr_u32(&hp, (unsigned)b->import_count);
    wr_u32(&hp, 0);
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
    *out++ = 0x01;
    for(i = 0; i < b->import_count; i++) {
        unsigned char slot[4];
        unsigned char *sp = slot;

        wr_u32(&sp, (unsigned)import_off[i]);
        memcpy(out, slot, 4);
        out += 4;
    }
    return (int)(out - dst);
}

static void
c_ident(char *dst, size_t dst_size, const char *src)
{
    slug_name(dst, dst_size, src);
    if(dst[0] >= '0' && dst[0] <= '9') {
        char tmp[KC_NAME_MAX];

        snprintf(tmp, sizeof(tmp), "h_%s", dst);
        snprintf(dst, dst_size, "%s", tmp);
    }
}

static void
write_krb_host(const KryFile *file, const char *gen_rel, const char *out_dir,
               const KrbBuild *b, const unsigned char *bytes, int len)
{
    char hrel[KC_PATH_MAX];
    char crel[KC_PATH_MAX];
    char hpath[KC_PATH_MAX];
    char cpath[KC_PATH_MAX];
    char screen[KC_NAME_MAX];
    char guard[KC_NAME_MAX * 2];
    FILE *out;
    int i;
    int j;

    snprintf(screen, sizeof(screen), "App");
    for(i = 0; i < file->function_count; i++) {
        if(file->functions[i].screen[0] != '\0') {
            snprintf(screen, sizeof(screen), "%s", file->functions[i].screen);
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
    fprintf(out, "/* Generated by kc --emit-krb from %s. */\n",
            relative_path(file->root, file->path));
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
    fprintf(out, "/* Generated by kc --emit-krb from %s. */\n",
            relative_path(file->root, file->path));
    fprintf(out, "#include \"%s\"\n", hrel);
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <string.h>\n");
    fprintf(out, "#include \"krb.h\"\n");
    fprintf(out, "#if !defined(KRYON_KRB_NO_MAIN)\n");
    fprintf(out, "#include \"kryon.h\"\n");
    if(file->app_font_examples)
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
        char fn[KC_NAME_MAX + 8];

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
        char fn[KC_NAME_MAX + 8];

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
    if(!file->no_main && file->app_title[0] != '\0') {
        int width = file->app_width > 0 ? file->app_width : 800;
        int height = file->app_height > 0 ? file->app_height : 600;
        int fps = file->app_fps > 0 ? file->app_fps : 60;
        char title[512];

        c_string_literal(title, sizeof(title), file->app_title);
        fprintf(out, "\n#if !defined(KRYON_KRB_NO_MAIN)\n");
        fprintf(out, "int\nmain(void)\n{\n");
        fprintf(out, "    InitWindow(%d, %d, %s);\n", width, height, title);
        fprintf(out, "    SetTargetFPS(%d);\n", fps);
        if(file->app_font_examples)
            fprintf(out, "    LoadExampleUIFont();\n");
        fprintf(out, "    InitUI(%d, %d, GetUIScale());\n", width, height);
        if(file->app_theme[0] != '\0')
            fprintf(out, "    SetCurrentTheme(%s, %d);\n",
                    file->app_theme, file->app_dark_mode);
        fprintf(out, "    while(!WindowShouldClose()) {\n");
        fprintf(out, "        BeginDrawing();\n");
        fprintf(out, "        BeginUIFrame(%d, %d, GetUIScale());\n",
                width, height);
        fprintf(out, "        %s_krb_draw(0, 0, GetScreenWidth(), GetScreenHeight());\n",
                screen);
        fprintf(out, "        EndUIFocus();\n");
        fprintf(out, "        EndDrawing();\n");
        fprintf(out, "    }\n");
        if(file->app_font_examples)
            fprintf(out, "    UnloadExampleUIFont();\n");
        fprintf(out, "    CloseWindow();\n");
        fprintf(out, "    return 0;\n");
        fprintf(out, "}\n#endif\n");
    }
    fclose(out);
}

void
write_krb(const KryFile *file, const char *root, const char *out_dir)
{
    const char *rel = relative_path(root, file->path);
    char gen_rel[KC_PATH_MAX];
    char krel[KC_PATH_MAX];
    char kpath[KC_PATH_MAX];
    unsigned char bytes[KRB_OUT_MAX];
    KrbBuild build;
    FILE *out;
    int i;
    int len;

    strip_kry_ext(gen_rel, sizeof(gen_rel), rel);
    if(file->module_file[0] != '\0') {
        char module_rel[KC_PATH_MAX];

        replace_path_basename(module_rel, sizeof(module_rel), gen_rel,
                              file->module_file);
        snprintf(gen_rel, sizeof(gen_rel), "%s", module_rel);
    }
    snprintf(krel, sizeof(krel), "%s.krb", gen_rel);
    path_join(kpath, sizeof(kpath), out_dir, krel);
    mkdir_parent(kpath);

    memset(&build, 0, sizeof(build));
    build.strings[0] = '\0';
    build.string_used = 1;
    collect_state(&build, file);
    for(i = 0; i < file->function_count; i++) {
        AstFunction *af = ast_function_from_body(&file->functions[i]);

        collect_widgets(&build, af);
        ast_function_free(af);
    }

    len = emit_krb_mem(bytes, KRB_OUT_MAX, &build);
    if(len < 0)
        die("%s: cartridge is too large", file->path);
    out = fopen(kpath, "wb");
    if(out == NULL)
        die("%s: open failed: %s", kpath, strerror(errno));
    if(fwrite(bytes, 1, (size_t)len, out) != (size_t)len)
        die("%s: write failed", kpath);
    fclose(out);
    write_krb_host(file, gen_rel, out_dir, &build, bytes, len);
}
