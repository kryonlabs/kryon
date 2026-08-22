/*
 * k2g_lower.c - Kir -> Go backend. See k2g_lower.h for scope.
 */
#include "k2g_lower.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define K2G_TEXT_MAX 8192
#define K2G_NAME_MAX 256
#define K2G_RUNTIME_IMPORT "github.com/waozixyz/kryon/go/kryon"
#define K2G_RUNTIME_PKG "kryon"

/* ---------------------------------------------------------------- helpers */

static void
mkdir_parent(const char *path)
{
    char tmp[1024];
    size_t i;

    snprintf(tmp, sizeof(tmp), "%s", path);
    for(i = 1; i < strlen(tmp); i++) {
        if(tmp[i] == '/') {
            tmp[i] = '\0';
            mkdir(tmp, 0755);
            tmp[i] = '/';
        }
    }
}

/* stem of "path/app.kry" -> "app": Go output is flat (one package per
 * output directory), so nested source trees must not nest the output. */
static void
stem_from_source(const char *src, char *dst, size_t dst_size)
{
    const char *base = strrchr(src, '/');
    size_t n;

    base = base != NULL ? base + 1 : src;
    n = strlen(base);
    if(n > 4 && strcmp(base + n - 4, ".kry") == 0)
        n -= 4;
    if(n >= dst_size)
        n = dst_size - 1;
    memcpy(dst, base, n);
    dst[n] = '\0';
}

/* "some/path-part" -> "SomePathPart": CamelCase Go identifier */
static void
camel(const char *s, char *dst, size_t dst_size)
{
    size_t n = 0;
    int up = 1;

    for(const char *p = s; *p && n + 1 < dst_size; p++) {
        if(isalnum((unsigned char)*p)) {
            dst[n++] = up ? (char)toupper((unsigned char)*p) : *p;
            up = 0;
        } else {
            up = 1;
        }
    }
    if(n == 0 && dst_size > 1)
        dst[n++] = 'X';
    if(isdigit((unsigned char)dst[0]) && dst_size > 2) {
        memmove(dst + 1, dst, n + 1);
        dst[0] = 'M';
        n++;
    }
    dst[n] = '\0';
}

static int
is_ident_char(int c)
{
    return isalnum(c) || c == '_';
}

static int
is_runtime_go_type(const char *type)
{
    static const char *types[] = {
        "Vector2", "Rectangle", "Color", "Texture2D", "UIKey", "UISide",
        "ButtonStyle", "UISyntaxMode", "ThemeStyle", "ThemeSource",
        "ThemeMode", "PictureFit", "TextInputStyle", "ButtonProps",
        "IconButtonProps", "HrefProps", "TextFieldProps", "TextAreaProps",
        "ColumnProps", "RowProps", "UIFrame", "UIGrid", "ParagraphSpec",
        "PictureProps", "BottomNavItem", "BottomNavProps", "TopNavProps",
        "ToolbarProps", "RadioButtonProps", "ProgressBarProps",
        "SpinboxProps", "ComboboxProps", "LabelFrameProps", "ListBoxProps",
        "SourceViewProps", "UITableRow", "TableViewProps", "NotebookProps",
        "PanedViewProps", "CollapsibleProps", "MessageDialogProps",
        "ConfirmDialogProps", "PromptDialogProps", "UICanvas",
        "UICanvasResult", NULL
    };

    for(int i = 0; types[i] != NULL; i++)
        if(strcmp(type, types[i]) == 0)
            return 1;
    return 0;
}

static void
qualify_runtime_go_type(const char *type, char *dst, size_t dst_size)
{
    if(is_runtime_go_type(type))
        snprintf(dst, dst_size, "%s.%s", K2G_RUNTIME_PKG, type);
    else
        snprintf(dst, dst_size, "%s", type);
}

/* C-ish type -> Go type. Returns 0 when unknown (caller falls back). */
static int
go_type(const char *type, char *dst, size_t dst_size)
{
    struct {
        const char *c;
        const char *go;
    } map[] = {
        {"int", "int32"},   {"unsigned int", "uint32"},
        {"unsigned", "uint32"}, {"uint", "uint32"},
        {"long", "int64"},  {"unsigned long", "uint64"},
        {"long long", "int64"}, {"unsigned long long", "uint64"},
        {"short", "int16"}, {"unsigned short", "uint16"},
        {"size_t", "int64"}, {"ssize_t", "int64"},
        {"float", "float32"}, {"double", "float64"},
        {"bool", "bool"},   {"char*", "string"}, {"const char*", "string"},
        {"char**", "[]string"}, {"const char**", "[]string"},
        {"string", "string"},
        {"char", "byte"}, {"unsigned char", "byte"}, {"byte", "byte"},
        {"int8", "int8"}, {"int16", "int16"}, {"int32", "int32"},
        {"int64", "int64"}, {"float32", "float32"}, {"float64", "float64"},
        {"void", ""},
        {"Vector2", "Vector2"}, {"Rectangle", "Rectangle"},
        {"UIFrame", "UIFrame"}, {"UIGrid", "UIGrid"},
        {"UICanvas", "UICanvas"},
        {"UICanvasResult", "UICanvasResult"},
        {"UISide", "UISide"},
        {"Color", "Color"},     {"Texture2D", "Texture2D"},
        {NULL, NULL}
    };
    char t[K2G_NAME_MAX];
    size_t n;

    snprintf(t, sizeof(t), "%s", type);
    n = strlen(t);
    while(n > 0 && (t[n - 1] == ' ' || t[n - 1] == '\t'))
        t[--n] = '\0';
    {
        const char *p = t;

        while(*p == ' ' || *p == '\t')
            p++;
        if(p != t)
            memmove(t, p, strlen(p) + 1);
        n = strlen(t);
    }
    /* 'char *' / 'char  *' mean 'char*': drop spaces adjacent to '*' */
    for(size_t k = 0; t[k] != '\0'; k++) {
        if(t[k] == ' ' && t[k + 1] == '*') {
            memmove(t + k, t + k + 1, strlen(t + k));
            k = (size_t)-1;
        }
    }
    /* strip a leading 'const ' */
    if(strncmp(t, "const ", 6) == 0)
        memmove(t, t + 6, strlen(t + 6) + 1);
    if(t[0] == '[') {
        char *close = strchr(t, ']');
        const char *base;

        if(close != NULL) {
            base = close + 1;
            while(*base == ' ' || *base == '\t')
                base++;
            if(strcmp(base, "char") == 0) {
                *close = '\0';
                snprintf(dst, dst_size, "[%s]byte", t + 1);
                return 1;
            }
            if(strcmp(base, "char*") == 0 || strcmp(base, "string") == 0) {
                *close = '\0';
                snprintf(dst, dst_size, "[%s]string", t + 1);
                return 1;
            }
            {
                char gt[K2G_NAME_MAX];

                if(go_type(base, gt, sizeof(gt))) {
                    *close = '\0';
                    snprintf(dst, dst_size, "[%s]%s", t + 1, gt);
                    return 1;
                }
            }
        }
        return 0;
    }
    for(int i = 0; map[i].c != NULL; i++) {
        if(strcmp(t, map[i].c) == 0) {
            qualify_runtime_go_type(map[i].go, dst, dst_size);
            return 1;
        }
    }
    /* trailing '*': pointer to a mapped scalar or a module typedef */
    n = strlen(t);
    if(n > 1 && t[n - 1] == '*') {
        char base[K2G_NAME_MAX];
        char gt[K2G_NAME_MAX];

        snprintf(base, sizeof(base), "%.*s", (int)(n - 1), t);
        /* trailing spaces before the '*' */
        {
            size_t bn = strlen(base);

            while(bn > 0 && base[bn - 1] == ' ')
                base[--bn] = '\0';
        }
        if(go_type(base, gt, sizeof(gt)) && strcmp(gt, "string") != 0) {
            snprintf(dst, dst_size, "*%s", gt);
            return 1;
        }
        /* unknown base: keep the identifier (module typedef pointer) */
        {
            int identish = base[0] != '\0';

            for(char *c = base; *c != '\0'; c++)
                if(!is_ident_char((unsigned char)*c))
                    identish = 0;
            if(identish) {
                char qualified[K2G_NAME_MAX * 2];

                qualify_runtime_go_type(base, qualified, sizeof(qualified));
                snprintf(dst, dst_size, "*%s", qualified);
                return 1;
            }
        }
        return 0;
    }
    /* bare unknown identifier: a module typedef (struct/enum) */
    {
        int identish = t[0] != '\0';

        for(char *c = t; *c != '\0'; c++)
            if(!is_ident_char((unsigned char)*c))
                identish = 0;
        if(identish) {
            qualify_runtime_go_type(t, dst, dst_size);
            return 1;
        }
    }
    return 0;
}

static int
state_field_index(const KirModule *m, const char *name, size_t len)
{
    for(int i = 0; i < m->state_count; i++) {
        if(strlen(m->state_fields[i].name) == len &&
           strncmp(m->state_fields[i].name, name, len) == 0)
            return i;
    }
    return -1;
}

static int
module_fn_index(const KirModule *m, const char *name, size_t len)
{
    for(int i = 0; i < m->function_count; i++) {
        if(strlen(m->functions[i].name) == len &&
           strncmp(m->functions[i].name, name, len) == 0)
            return i;
    }
    return -1;
}

/* ------------------------------------------------ module lowering context */

static int split_top(const char *s, char parts[][K2G_TEXT_MAX], int max);

/* One '#extern' declaration bridged to a Go host method. */
typedef struct {
    char kry[K2G_NAME_MAX];        /* kry call name */
    char go[K2G_NAME_MAX];         /* Host interface method */
    char pnames[8][K2G_NAME_MAX];  /* parameter names */
    char ptypes[8][K2G_NAME_MAX];  /* parameter kry types */
    int pcount;
    char ret[K2G_NAME_MAX];
    char host_var[K2G_NAME_MAX + 8];   /* guard-prefixed host var */
} K2gExtern;

/* One enum member visible to expressions (bare kry name -> qualified Go const). */
typedef struct {
    char kry[K2G_NAME_MAX];
    char go[K2G_NAME_MAX * 2];
    char val[K2G_TEXT_MAX];        /* explicit value text, or "" */
} K2gEnumMember;

typedef struct {
    char go_type[K2G_NAME_MAX];    /* "" for anonymous '#enum' blocks */
    char prefix[K2G_NAME_MAX];     /* enum-name prefix for Go const names */
    K2gEnumMember members[64];
    int count;
} K2gEnum;

/* Lowering is single-threaded and per-module sequential: one cached context. */
static const KirModule *g_mod;
static char g_guard[K2G_NAME_MAX];
static K2gExtern g_externs[64];
static int g_extern_count;
static K2gEnum g_enums[32];
static int g_enum_count;
static K2gEnumMember *g_const_table[512];
static int g_const_count;

static int
k2g_extern_index(const char *name, size_t len)
{
    for(int i = 0; i < g_extern_count; i++) {
        if(strlen(g_externs[i].kry) == len &&
           strncmp(g_externs[i].kry, name, len) == 0)
            return i;
    }
    return -1;
}

static K2gEnumMember *
k2g_const_entry(const char *name, size_t len)
{
    for(int i = 0; i < g_const_count; i++) {
        if(strlen(g_const_table[i]->kry) == len &&
           strncmp(g_const_table[i]->kry, name, len) == 0)
            return g_const_table[i];
    }
    return NULL;
}

/* "a: int, b: char*" -> parameter names/types on ex. */
static void
split_params(const char *args, K2gExtern *ex)
{
    char parts[8][K2G_TEXT_MAX];
    int n = split_top(args, parts, 8);

    ex->pcount = 0;
    for(int i = 0; i < n; i++) {
        char *colon = strchr(parts[i], ':');
        size_t nl;

        if(colon == NULL)
            continue;
        nl = (size_t)(colon - parts[i]);
        while(nl > 0 && parts[i][nl - 1] == ' ')
            nl--;
        if(nl == 0 || ex->pcount >= 8)
            continue;
        snprintf(ex->pnames[ex->pcount], K2G_NAME_MAX, "%.*s", (int)nl,
                 parts[i]);
        {
            const char *pt = colon + 1;

            while(*pt == ' ' || *pt == '\t')
                pt++;
            snprintf(ex->ptypes[ex->pcount], K2G_NAME_MAX, "%s", pt);
        }
        ex->pcount++;
    }
}

/* Extract the Go method name from a '#extern "pkg.Fn"' target: the segment
 * after the last dot. */
static void
extern_go_name(const char *target, const char *kry, char *dst, size_t dst_size)
{
    const char *dot = strrchr(target, '.');
    const char *base = dot != NULL ? dot + 1 : target;

    if(base[0] != '\0') {
        snprintf(dst, dst_size, "%s", base);
        for(char *c = dst; *c != '\0'; c++)
            if(!is_ident_char((unsigned char)*c))
                *c = '_';
    } else {
        camel(kry, dst, dst_size);
    }
}

/* Register one extern (idempotent: first declaration wins). */
static void
add_extern(const char *kry, const char *args, const char *ret,
           const char *target)
{
    K2gExtern *ex;

    if(k2g_extern_index(kry, strlen(kry)) >= 0 || g_extern_count >= 64)
        return;
    ex = &g_externs[g_extern_count++];
    memset(ex, 0, sizeof(*ex));
    snprintf(ex->kry, sizeof(ex->kry), "%s", kry);
    snprintf(ex->ret, sizeof(ex->ret), "%s", ret);
    split_params(args, ex);
    extern_go_name(target, kry, ex->go, sizeof(ex->go));
    /* guard "KryApp" -> host var "kryAppHost" */
    snprintf(ex->host_var, sizeof(ex->host_var), "%c%sHost",
             (char)tolower((unsigned char)g_guard[0]), g_guard + 1);
}

/* Parse "name :: (args) -> ret #extern \"pkg.Fn\"" from a raw extern import
 * line (the KirImport.signature keeps the whole declaration). */
static void
parse_extern_import(const KirImport *imp)
{
    char args[K2G_TEXT_MAX];
    char ret[K2G_NAME_MAX];
    char target[K2G_NAME_MAX];
    const char *lp = strchr(imp->signature, '(');
    const char *rp = lp != NULL ? strchr(lp, ')') : NULL;
    const char *dir = strstr(imp->signature, "#extern");

    args[0] = '\0';
    snprintf(ret, sizeof(ret), "void");
    target[0] = '\0';
    if(lp != NULL && rp != NULL && rp > lp)
        snprintf(args, sizeof(args), "%.*s", (int)(rp - lp - 1), lp + 1);
    if(rp != NULL) {
        const char *arrow = strstr(rp, "->");

        if(arrow != NULL && (dir == NULL || arrow < dir)) {
            const char *r = arrow + 2;
            size_t n = 0;

            while(*r == ' ' || *r == '\t')
                r++;
            while(*r != '\0' && *r != '#' && n + 1 < sizeof(ret))
                ret[n++] = *r++;
            while(n > 0 && (ret[n - 1] == ' ' || ret[n - 1] == '\t'))
                n--;
            ret[n] = '\0';
        }
    }
    if(dir != NULL) {
        const char *q = strchr(dir + 7, '"');

        if(q != NULL) {
            size_t n = 0;
            const char *r = q + 1;

            while(*r != '\0' && *r != '"' && n + 1 < sizeof(target))
                target[n++] = *r++;
            target[n] = '\0';
        }
    }
    add_extern(imp->name, args, ret, target);
}

/* Parse enum body members. The parser may deliver them newline-separated or
 * comma-joined on one line ('A = 0, B, C'), so split on both. */
static void
parse_enum(const KirType *t)
{
    K2gEnum *e;
    const char *p = t->body;

    if(g_enum_count >= 32)
        return;
    if(strcmp(t->name, "#enum") == 0) {
        e = &g_enums[g_enum_count++];
        memset(e, 0, sizeof(*e));
        e->prefix[0] = '\0';
    } else {
        e = &g_enums[g_enum_count++];
        memset(e, 0, sizeof(*e));
        camel(t->name, e->go_type, sizeof(e->go_type));
        camel(t->name, e->prefix, sizeof(e->prefix));
    }
    while(*p != '\0') {
        char line[K2G_TEXT_MAX];
        char *name, *val;
        size_t n = 0;

        /* collect one member: up to ',' or newline */
        while(*p != '\0' && *p != ',' && *p != '\n' &&
              n + 1 < sizeof(line))
            line[n++] = *p++;
        if(*p == ',' || *p == '\n')
            p++;
        line[n] = '\0';
        /* trim */
        {
            char *s = line;

            while(*s == ' ' || *s == '\t' || *s == '\r')
                s++;
            memmove(line, s, strlen(s) + 1);
        }
        {
            size_t ln = strlen(line);

            while(ln > 0 && (line[ln - 1] == ' ' || line[ln - 1] == '\t' ||
                             line[ln - 1] == '\r'))
                line[--ln] = '\0';
        }
        if(line[0] == '\0')
            continue;
        name = line;
        val = strchr(line, '=');
        if(val != NULL) {
            *val = '\0';
            val++;
            while(*val == ' ' || *val == '\t')
                val++;
        }
        {
            size_t nn = strlen(name);

            while(nn > 0 && (name[nn - 1] == ' ' || name[nn - 1] == '\t'))
                name[--nn] = '\0';
        }
        if(name[0] == '\0' || e->count >= 64)
            continue;
        K2gEnumMember *m = &e->members[e->count++];
        memset(m, 0, sizeof(*m));
        snprintf(m->kry, sizeof(m->kry), "%s", name);
        if(e->prefix[0] != '\0')
            snprintf(m->go, sizeof(m->go), "%s%s", e->prefix, m->kry);
        else
            camel(m->kry, m->go, sizeof(m->go));
        if(val != NULL)
            snprintf(m->val, sizeof(m->val), "%s", val);
        if(g_const_count < 512)
            g_const_table[g_const_count++] = m;
    }
}

/* (Re)build the per-module context: extern bridge + enum constants. Must run
 * before any expression or statement is emitted for the module. */
static void
k2g_set_module(const KirModule *m, const char *guard)
{
    g_mod = m;
    snprintf(g_guard, sizeof(g_guard), "%s", guard);
    g_extern_count = 0;
    g_enum_count = 0;
    g_const_count = 0;
    for(int i = 0; i < m->import_count; i++) {
        if(m->imports[i].kind == KIR_IMPORT_EXTERN)
            parse_extern_import(&m->imports[i]);
    }
    for(int i = 0; i < m->function_count; i++) {
        const KirFunction *fn = &m->functions[i];

        if(!fn->is_extern)
            continue;
        if(fn->extern_target[0] != '\0')
            add_extern(fn->name, fn->args, fn->return_type,
                       fn->extern_target);
        else
            add_extern(fn->name, fn->args, fn->return_type, "");
    }
    for(int i = 0; i < m->type_count; i++) {
        if(m->types[i].is_enum)
            parse_enum(&m->types[i]);
    }
}

/* Wrap a translated argument in a Go conversion for its kry parameter type,
 * so int/long/float widening across the host bridge always compiles. */
static const char *
conv_arg(const char *kry_type, const char *expr)
{
    static char buf[K2G_TEXT_MAX];
    char t[K2G_NAME_MAX];

    snprintf(t, sizeof(t), "%s", kry_type);
    {
        size_t n = strlen(t);

        while(n > 0 && (t[n - 1] == ' ' || t[n - 1] == '\t'))
            t[--n] = '\0';
    }
    if(strcmp(t, "int") == 0 || strcmp(t, "int32") == 0)
        snprintf(buf, sizeof(buf), "int32(%s)", expr);
    else if(strcmp(t, "long") == 0 || strcmp(t, "long long") == 0 ||
            strcmp(t, "size_t") == 0 || strcmp(t, "ssize_t") == 0)
        snprintf(buf, sizeof(buf), "int64(%s)", expr);
    else if(strcmp(t, "unsigned int") == 0 || strcmp(t, "uint") == 0 ||
            strcmp(t, "unsigned") == 0)
        snprintf(buf, sizeof(buf), "uint32(%s)", expr);
    else if(strcmp(t, "unsigned long") == 0)
        snprintf(buf, sizeof(buf), "uint64(%s)", expr);
    else if(strcmp(t, "short") == 0)
        snprintf(buf, sizeof(buf), "int16(%s)", expr);
    else if(strcmp(t, "float") == 0 || strcmp(t, "float32") == 0)
        snprintf(buf, sizeof(buf), "float32(%s)", expr);
    else if(strcmp(t, "double") == 0 || strcmp(t, "float64") == 0)
        snprintf(buf, sizeof(buf), "float64(%s)", expr);
    else
        snprintf(buf, sizeof(buf), "%s", expr);
    return buf;
}

/* skip spaces from *pp */
static const char *
skip_ws(const char *p)
{
    while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    return p;
}

/* ------------------------------------------------------- expression pass */

/* Forward */
static void tx_expr(const KirModule *m, const char *src, char *dst,
                    size_t dst_size);

/* Translate the inside of a braced/paren group starting after the opener;
 * returns the position after the matching closer. */
static const char *
tx_group(const KirModule *m, const char *src, char *dst, size_t *dn,
         char open, char close)
{
    int depth = 1;
    char mid[K2G_TEXT_MAX];
    size_t mn = 0;
    const char *p = src;

    while(*p != '\0' && depth > 0 && mn + 1 < sizeof(mid)) {
        if(*p == open)
            depth++;
        else if(*p == close) {
            depth--;
            if(depth == 0)
                break;
        } else if(*p == '"' || *p == '\'') {
            char q = *p;
            mid[mn++] = *p++;
            while(*p != '\0' && *p != q && mn + 1 < sizeof(mid))
                mid[mn++] = *p++;
            if(*p == q)
                mid[mn++] = *p++;
            continue;
        }
        mid[mn++] = *p++;
    }
    mid[mn] = '\0';
    {
        char out[K2G_TEXT_MAX];

        tx_expr(m, mid, out, sizeof(out));
        if(*dn + strlen(out) + 1 < K2G_TEXT_MAX) {
            memcpy(dst + *dn, out, strlen(out));
            *dn += strlen(out);
        }
    }
    return *p == close && depth == 0 ? p + 1 : p;
}

/* Split top-level (depth-0) comma parts. */
static int
split_top(const char *s, char parts[][K2G_TEXT_MAX], int max)
{
    int depth = 0, n = 0;
    const char *start = s;

    for(const char *p = s; ; p++) {
        if(*p == '\0' || (*p == ',' && depth == 0)) {
            size_t len = (size_t)(p - start);

            if(n < max && len < K2G_TEXT_MAX) {
                memcpy(parts[n], start, len);
                parts[n][len] = '\0';
                n++;
            }
            if(*p == '\0')
                break;
            start = p + 1;
        } else if(*p == '(' || *p == '[' || *p == '{')
            depth++;
        else if(*p == ')' || *p == ']' || *p == '}')
            depth--;
    }
    return n;
}

/* "(Vector2){a,b}" style compound literal: p points after "(". */
/* C field order for the Props/Spec types .kry writes positionally, e.g.
 * Picture((PictureProps){"path", ...}). Designated initializers do not need
 * this table; positional parts index into it. Names are the Go field names. */
static const char *
props_field_at(const char *type, int index)
{
    static const struct {
        const char *type;
        const char *fields[20];
    } table[] = {
        {"ColumnProps", {"Bounds", "Gap", "Padding", "Key"}},
        {"PictureProps", {"AssetPath", "Bounds", "Source", "Origin",
                          "Rotation", "Tint", "Fit", "Style"}},
        {"IconButtonProps", {"Bounds", "Icon", "IconType", "IconSize",
                             "IconPadding", "FocusID", "Disabled",
                             "Background", "HoverBackground", "IconColor",
                             "Border", "Radius"}},
        {"HrefProps", {"Bounds", "Text", "Href", "Font", "FocusID",
                       "Disabled", "Color", "HoverColor"}},
        {"ParagraphSpec", {"Text", "IconType", "IconSize", "Width",
                             "Font", "LineGap", "Color"}},
        {"ButtonProps", {"Bounds", "Label", "Style", "Font", "ID",
                         "Disabled"}},
        {"TextFieldProps", {"Bounds", "Text", "TextSize", "CursorPosition",
                            "Focused", "MaxCodepoints", "Font", "FocusID",
                            "Style", "Filter", "FilterUserData",
                            "CommitPressed", "Secure", "ReadOnly"}},
        {"TextAreaProps", {"Bounds", "Text", "TextSize", "CursorPosition",
                           "Focused", "ScrollY", "MaxCodepoints", "Font",
                           "LineGap", "FocusID", "Placeholder", "Syntax",
                           "Style", "Filter", "FilterUserData",
                           "ContentVersion", "ReadOnly", "Wrap"}},
    };
    size_t i;

    for(i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if(strcmp(table[i].type, type) == 0) {
            size_t n = 0;

            while(table[i].fields[n] != NULL && n < sizeof(table[i].fields))
                n++;
            if(index >= 0 && (size_t)index < n)
                return table[i].fields[index];
            return NULL;
        }
    }
    return NULL;
}

/* .kry array variables ('name: [N] const char*' state or local): uses
 * lower to Go slices, so identifier references append '[:]' (valid Go in
 * argument, indexing, and len() positions alike). */
static char k2g_array_names[24][K2G_NAME_MAX];
static int k2g_array_count;

static void
k2g_register_arrays_module(const KirModule *m)
{
    int i;

    for(i = 0; i < m->state_count && k2g_array_count < 24; i++) {
        const KirStateField *sf = &m->state_fields[i];

        if(sf->type[0] == '[' && strstr(sf->type, "char") != NULL &&
           strchr(sf->type, '*') != NULL) {
            snprintf(k2g_array_names[k2g_array_count], KIR_NAME_MAX, "%s",
                     sf->name);
            k2g_array_count++;
        }
    }
}

static void
k2g_register_arrays_stmt(const char *text)
{
    const char *t = text;
    const char *colon;
    const char *eq;

    while(*t == ' ' || *t == '\t')
        t++;
    colon = strchr(t, ':');
    eq = colon != NULL ? strchr(colon, '=') : NULL;
    if(colon == NULL || eq == NULL || k2g_array_count >= 24)
        return;
    {
        char decl_type[KIR_NAME_MAX];
        size_t tl = (size_t)(eq - colon - 1);

        if(tl >= sizeof(decl_type))
            tl = sizeof(decl_type) - 1;
        memcpy(decl_type, colon + 1, tl);
        decl_type[tl] = '\0';
        if(decl_type[0] != '[' || strstr(decl_type, "char") == NULL ||
           strchr(decl_type, '*') == NULL)
            return;
    }
    {
        size_t nl = (size_t)(colon - t);

        if(nl > 0 && nl < KIR_NAME_MAX) {
            memcpy(k2g_array_names[k2g_array_count], t, nl);
            k2g_array_names[k2g_array_count][nl] = '\0';
            k2g_array_count++;
        }
    }
}

static int
k2g_is_array_name(const char *ident, size_t len)
{
    int i;

    for(i = 0; i < k2g_array_count; i++)
        if(strlen(k2g_array_names[i]) == len &&
           strncmp(k2g_array_names[i], ident, len) == 0)
            return 1;
    return 0;
}

/* Go Props fields that are bool while .kry writes C ints (0/1). */
static int
bool_prop_field(const char *field)
{
    static const char *names[] = {"Disabled", "DrawMenu", "Active",
                                  "Secure", "Closeable", "Italic",
                                  "FocusSelected", NULL};
    int i;

    for(i = 0; names[i] != NULL; i++)
        if(strcmp(names[i], field) == 0)
            return 1;
    return 0;
}

static const char *
tx_compound(const KirModule *m, const char *p, char *dst, size_t *dn)
{
    char type[K2G_NAME_MAX];
    size_t tn = 0;

    while(*p != '\0' && *p != ')' && tn + 1 < sizeof(type))
        type[tn++] = *p++;
    if(*p != ')')
        return p;
    p++;
    type[tn] = '\0';
    while(tn > 0 && (type[tn - 1] == ' ' || type[tn - 1] == '\t'))
        type[--tn] = '\0';

    if(*p == '{') {
        char qtype[K2G_NAME_MAX * 2];

        qualify_runtime_go_type(type, qtype, sizeof(qtype));
        p++;
        if(strcmp(type, "Vector2") == 0 || strcmp(type, "Rectangle") == 0) {
            char inner[K2G_TEXT_MAX], parts[4][K2G_TEXT_MAX], out[K2G_TEXT_MAX];
            int n, i;
            size_t on = 0;
            const char *tail;

            /* consume to the matching '}' */
            {
                char raw[K2G_TEXT_MAX];
                size_t rn = 0;
                int depth = 1;
                const char *q = p;

                while(*q != '\0' && depth > 0 && rn + 1 < sizeof(raw)) {
                    if(*q == '{')
                        depth++;
                    else if(*q == '}') {
                        depth--;
                        if(depth == 0)
                            break;
                    }
                    raw[rn++] = *q++;
                }
                raw[rn] = '\0';
                p = *q == '}' ? q + 1 : q;
                n = split_top(raw, parts, 4);
            }
            if(strcmp(type, "Vector2") == 0 && n == 2)
                snprintf(out + on, sizeof(out) - on, "%s.NewVector2",
                         K2G_RUNTIME_PKG);
            else if(strcmp(type, "Rectangle") == 0 && n == 4)
                snprintf(out + on, sizeof(out) - on, "%s.NewRectangle",
                         K2G_RUNTIME_PKG);
            else {
                /* odd arity: emit a TODO-safe zero value */
                snprintf(dst + *dn, K2G_TEXT_MAX - *dn, "%s",
                         K2G_RUNTIME_PKG ".NewVector2(0, 0)");
                *dn += strlen(dst + *dn);
                return p;
            }
            on = strlen(out);
            out[on++] = '(';
            for(i = 0; i < n; i++) {
                char arg[K2G_TEXT_MAX];

                tx_expr(m, skip_ws(parts[i]), arg, sizeof(arg));
                if(i > 0) {
                    out[on++] = ',';
                    out[on++] = ' ';
                }
                if(on + 8 < sizeof(out)) {
                    memcpy(out + on, "float32(", 8);
                    on += 8;
                }
                size_t al = strlen(arg);
                if(on + al + 1 < sizeof(out)) {
                    memcpy(out + on, arg, al);
                    on += al;
                }
                if(on + 1 < sizeof(out))
                    out[on++] = ')';
            }
            out[on++] = ')';
            out[on] = '\0';
            if(*dn + on + 1 < K2G_TEXT_MAX) {
                memcpy(dst + *dn, out, on);
                *dn += on;
            }
            (void)inner;
            (void)tail;
            return p;
        }
        if(strcmp(type, "Color") == 0) {
            char raw[K2G_TEXT_MAX], parts[4][K2G_TEXT_MAX];
            int n;

            {
                size_t rn = 0;
                int depth = 1;
                const char *q = p;

                while(*q != '\0' && depth > 0 && rn + 1 < sizeof(raw)) {
                    if(*q == '{')
                        depth++;
                    else if(*q == '}') {
                        depth--;
                        if(depth == 0)
                            break;
                    }
                    raw[rn++] = *q++;
                }
                raw[rn] = '\0';
                p = *q == '}' ? q + 1 : q;
                n = split_top(raw, parts, 4);
            }
            if(n == 4) {
                char args[4][K2G_TEXT_MAX];
                static const char *fields[4] = {"R", "G", "B", "A"};

                for(int i = 0; i < 4; i++)
                    tx_expr(m, skip_ws(parts[i]), args[i], sizeof(args[i]));
                if(*dn + 4096 < K2G_TEXT_MAX) {
                    *dn += (size_t)snprintf(dst + *dn, K2G_TEXT_MAX - *dn,
                        "%s.Color{%s: %s, %s: %s, %s: %s, %s: %s}",
                        K2G_RUNTIME_PKG,
                        fields[0], args[0], fields[1], args[1],
                        fields[2], args[2], fields[3], args[3]);
                }
            }
            return p;
        }
        /* Props/Spec use C designated initializers. Translate them to named
         * Go fields and give the untyped bounds literal its Rectangle type. */
        if(strstr(type, "Props") != NULL || strstr(type, "Spec") != NULL ||
           strcmp(type, "UICanvas") == 0) {
            char raw[K2G_TEXT_MAX], parts[32][K2G_TEXT_MAX];
            size_t rn = 0;
            int depth = 1;
            const char *q = p;
            int count;

            while(*q != '\0' && depth > 0 && rn + 1 < sizeof(raw)) {
                if(*q == '{')
                    depth++;
                else if(*q == '}') {
                    depth--;
                    if(depth == 0)
                        break;
                }
                raw[rn++] = *q++;
            }
            raw[rn] = '\0';
            p = *q == '}' ? q + 1 : q;
            count = split_top(raw, parts, 32);
            *dn += (size_t)snprintf(dst + *dn, K2G_TEXT_MAX - *dn,
                                    "%s{", qtype);
            for(int i = 0, emitted = 0, positional = 0; i < count; i++) {
                char *part = (char *)skip_ws(parts[i]);
                char *eq;
                char field[K2G_NAME_MAX];
                char value[K2G_TEXT_MAX];

                if(*part != '.') {
                    /* positional initializer: map by C field order */
                    const char *mapped = props_field_at(type, positional++);

                    if(mapped == NULL)
                        continue;
                    snprintf(field, sizeof(field), "%s", mapped);
                    if(*part == '\0')
                        continue;
                    tx_expr(m, part, value, sizeof(value));
                } else {
                    eq = strchr(part, '=');
                    if(eq == NULL)
                        continue;
                    *eq = '\0';
                    camel(part + 1, field, sizeof(field));
                    if(strcmp(field, "FocusId") == 0)
                        snprintf(field, sizeof(field), "FocusID");
                    if(strcmp(field, "Id") == 0)
                        snprintf(field, sizeof(field), "ID");
                    if(strcmp(field, "TextSize") == 0)
                        continue;
                    if(strcmp(field, "Bounds") == 0 && *skip_ws(eq + 1) == '{') {
                        char rect[K2G_TEXT_MAX];
                        snprintf(rect, sizeof(rect), "(Rectangle)%s", skip_ws(eq + 1));
                        tx_expr(m, rect, value, sizeof(value));
                    } else {
                        tx_expr(m, skip_ws(eq + 1), value, sizeof(value));
                    }
                }
                if((strcmp(type, "TextFieldProps") == 0 ||
                    strcmp(type, "TextAreaProps") == 0) &&
                   strcmp(field, "Text") == 0 && strncmp(value, "st.", 3) == 0)
                    strncat(value, "[:]", sizeof(value) - strlen(value) - 1);
                if(emitted++)
                    *dn += (size_t)snprintf(dst + *dn, K2G_TEXT_MAX - *dn, ", ");
                if(bool_prop_field(field) && strcmp(value, "true") != 0 &&
                   strcmp(value, "false") != 0)
                    *dn += (size_t)snprintf(dst + *dn, K2G_TEXT_MAX - *dn,
                                            "%s: (%s != 0)", field, value);
                else
                    *dn += (size_t)snprintf(dst + *dn, K2G_TEXT_MAX - *dn,
                                            "%s: %s", field, value);
            }
            if(*dn + 2 < K2G_TEXT_MAX)
                dst[(*dn)++] = '}';
            return p;
        }
        /* other struct literals: Type{...} — recurse and keep braces */
        {
            char ctor[K2G_NAME_MAX + 8];

            snprintf(ctor, sizeof(ctor), "%s{", qtype);
            if(*dn + strlen(ctor) + 1 < K2G_TEXT_MAX) {
                memcpy(dst + *dn, ctor, strlen(ctor));
                *dn += strlen(ctor);
            }
            p = tx_group(m, p, dst, dn, '{', '}');
            if(*dn + 2 < K2G_TEXT_MAX)
                dst[(*dn)++] = '}';
            return p;
        }
    }
    /* plain cast "(T)expr" */
    {
        char gt[K2G_NAME_MAX];

        if(strcmp(type, "char*") == 0 || strcmp(type, "const char*") == 0) {
            /* string cast: drop it, translate the operand below */
            return p;
        }
        if(go_type(type, gt, sizeof(gt)) && gt[0] != '\0') {
            if(*dn + strlen(gt) + 1 < K2G_TEXT_MAX) {
                memcpy(dst + *dn, gt, strlen(gt));
                *dn += strlen(gt);
            }
            return p; /* caller emits the operand as the cast argument;
                         Go cast syntax is T(operand), so open a paren */
        }
        return p; /* unknown cast: drop */
    }
}

static void
tx_expr(const KirModule *m, const char *src, char *dst, size_t dst_size)
{
    size_t dn = 0;
    const char *p = src;
    char out[K2G_TEXT_MAX];

    if(dst_size > K2G_TEXT_MAX)
        dst_size = K2G_TEXT_MAX;
    while(*p != '\0' && dn + 8 < dst_size) {
        if(*p == ';' || *p == '\n' || *p == '\r') {
            p++;
            continue;
        }
        if(*p == '"') { /* string literal, verbatim */
            dst[dn++] = *p++;
            while(*p != '\0' && *p != '"' && dn + 2 < dst_size)
                dst[dn++] = *p++;
            if(*p == '"')
                dst[dn++] = *p++;
            continue;
        }
        /* cast / compound literal: '(' ident ')' */
        if(*p == '(') {
            const char *q = p + 1;
            size_t tl = 0;

            if(!isalpha((unsigned char)*q) && *q != '_')
                q = p; /* numeric or expression: not a cast */
            else
                while(is_ident_char((unsigned char)*q) || *q == ' ' || *q == '*')
                    q++;
            tl = (size_t)(q - (p + 1));
            if(*q == ')' && tl > 0 && tl < sizeof(char) * K2G_NAME_MAX) {
                char maybe[K2G_NAME_MAX];
                int identish = 1;

                memcpy(maybe, p + 1, tl < K2G_NAME_MAX - 1 ? tl : K2G_NAME_MAX - 1);
                maybe[tl < K2G_NAME_MAX - 1 ? tl : K2G_NAME_MAX - 1] = '\0';
                for(char *c = maybe; *c != '\0'; c++)
                    if(!is_ident_char((unsigned char)*c) && *c != ' ' && *c != '*')
                        identish = 0;
                if(identish) {
                    p = tx_compound(m, p + 1, dst, &dn);
                    /* for scalar casts, wrap the next operand in parens:
                     * emit '(' now and rely on the trailing paren we add
                     * below when the expression ends — simplest correct
                     * form: emit operand inside parens manually */
                    if(*(p - 1) == ')' && strchr(maybe, '{') == NULL) {
                        /* scalar cast: consume one primary operand */
                        const char *op = skip_ws(p);
                        char primary[K2G_TEXT_MAX];
                        size_t pn = 0;

                        if(dn + 1 < dst_size)
                            dst[dn++] = '(';
                        if(*op == '(') {
                            const char *after;

                            dst[dn++] = '(';
                            after = tx_group(m, op + 1, dst, &dn, '(', ')');
                            if(dn + 1 < dst_size)
                                dst[dn++] = ')';
                            p = after;
                        } else if(is_ident_char((unsigned char)*op)) {
                            while(is_ident_char((unsigned char)*op) &&
                                  pn + 1 < sizeof(primary))
                                primary[pn++] = *op++;
                            primary[pn] = '\0';
                            p = op;
                            {
                                char po[K2G_TEXT_MAX];

                                tx_expr(m, primary, po, sizeof(po));
                                size_t pl = strlen(po);
                                if(dn + pl + 1 < dst_size) {
                                    memcpy(dst + dn, po, pl);
                                    dn += pl;
                                }
                            }
                        }
                        if(dn + 1 < dst_size)
                            dst[dn++] = ')';
                    }
                    continue;
                }
            }
            dst[dn++] = *p++;
            p = tx_group(m, p, dst, &dn, '(', ')');
            if(dn + 1 < dst_size)
                dst[dn++] = ')';
            continue;
        }
        if(*p == '{') {
            dst[dn++] = *p++;
            p = tx_group(m, p, dst, &dn, '{', '}');
            if(dn + 1 < dst_size)
                dst[dn++] = '}';
            continue;
        }
        /* NULL -> nil */
        if(strncmp(p, "NULL", 4) == 0 && !is_ident_char((unsigned char)p[4])) {
            const char *r = "nil";
            if(dn + 4 < dst_size) {
                memcpy(dst + dn, r, 3);
                dn += 3;
            }
            p += 4;
            continue;
        }
        /* 1.0f -> 1.0 */
        if(isdigit((unsigned char)*p)) {
            const char *q = p;
            const char *num_end;

            if(*q == '0' && (q[1] == 'x' || q[1] == 'X')) {
                q += 2;
                while(isxdigit((unsigned char)*q))
                    q++;
            } else {
                while(isdigit((unsigned char)*q) || *q == '.')
                    q++;
                if(*q == 'e' || *q == 'E') {  /* exponent */
                    q++;
                    if(*q == '+' || *q == '-')
                        q++;
                    while(isdigit((unsigned char)*q))
                        q++;
                }
            }
            num_end = q;
            if(*q == 'f' || *q == 'F')
                q++;   /* C float suffix: drop it */
            while(p < num_end && dn + 1 < dst_size)
                dst[dn++] = *p++;
            p = q;
            continue;
        }
        if(is_ident_char((unsigned char)*p) || *p == '&') {
            int addr = *p == '&';
            const char *q = addr ? p + 1 : p;
            char ident[K2G_NAME_MAX];
            size_t il = 0;
            int sfi, fni;

            while(is_ident_char((unsigned char)*q) && il + 1 < sizeof(ident))
                ident[il++] = *q++;
            ident[il] = '\0';
            if(il == 0) {
                dst[dn++] = *p++;
                continue;
            }
            sfi = state_field_index(m, ident, il);
            if(sfi >= 0) {
                char camel_name[K2G_NAME_MAX];
                size_t cl;

                camel(m->state_fields[sfi].name, camel_name, sizeof(camel_name));
                cl = strlen(camel_name);
                if(addr && dn + cl + 5 < dst_size) {
                    dst[dn++] = '&';
                    dst[dn++] = 's';
                    dst[dn++] = 't';
                    dst[dn++] = '.';
                    memcpy(dst + dn, camel_name, cl);
                    dn += cl;
                    p = q;
                    continue;
                }
                if(!addr && dn + cl + 4 < dst_size) {
                    dst[dn++] = 's';
                    dst[dn++] = 't';
                    dst[dn++] = '.';
                    memcpy(dst + dn, camel_name, cl);
                    dn += cl;
                    p = q;
                    continue;
                }
            }
            /* '#extern' host bridge: name(args) -> host.Method(args). The
             * check precedes the module-function path because extern
             * prototypes also sit in the function table (bodyless). */
            {
                int xi = k2g_extern_index(ident, il);

                if(xi >= 0 && *skip_ws(q) == '(') {
                    char raw[K2G_TEXT_MAX];
                    const char *ap = skip_ws(q) + 1;
                    const char *ae = ap;
                    int depth = 1;
                    size_t rn = 0;
                    int all_ws = 1;

                    while(*ae != '\0' && depth > 0 && rn + 1 < sizeof(raw)) {
                        if(*ae == '"') {
                            all_ws = 0;
                            raw[rn++] = *ae++;
                            while(*ae != '\0' && *ae != '"' &&
                                  rn + 1 < sizeof(raw))
                                raw[rn++] = *ae++;
                            if(*ae == '"')
                                raw[rn++] = *ae++;
                            continue;
                        }
                        if(*ae == '(' || *ae == '[' || *ae == '{')
                            depth++;
                        else if(*ae == ')' || *ae == ']' || *ae == '}') {
                            depth--;
                            if(depth == 0)
                                break;
                        } else if(*ae != ' ' && *ae != '\t')
                            all_ws = 0;
                        raw[rn++] = *ae++;
                    }
                    raw[rn] = '\0';
                    if(*ae == ')')
                        ae++;
                    p = ae;
                    dn += (size_t)snprintf(dst + dn, K2G_TEXT_MAX - dn,
                                           "%s.%s(", g_externs[xi].host_var,
                                           g_externs[xi].go);
                    if(!all_ws) {
                        char parts[8][K2G_TEXT_MAX];
                        int n = split_top(raw, parts, 8);

                        for(int i = 0; i < n; i++) {
                            char arg[K2G_TEXT_MAX];

                            tx_expr(m, skip_ws(parts[i]), arg, sizeof(arg));
                            if(i > 0)
                                dn += (size_t)snprintf(dst + dn,
                                                       K2G_TEXT_MAX - dn, ", ");
                            if(i < g_externs[xi].pcount) {
                                dn += (size_t)snprintf(
                                    dst + dn, K2G_TEXT_MAX - dn, "%s",
                                    conv_arg(g_externs[xi].ptypes[i], arg));
                            } else {
                                dn += (size_t)snprintf(dst + dn,
                                                       K2G_TEXT_MAX - dn, "%s",
                                                       arg);
                            }
                        }
                    }
                    if(dn + 1 < dst_size)
                        dst[dn++] = ')';
                    continue;
                }
            }
            fni = module_fn_index(m, ident, il);
            if(fni >= 0 && *skip_ws(q) == '(') {
                char fname[K2G_NAME_MAX * 2];
                size_t fl;

                camel(m->functions[fni].name, fname, sizeof(fname));
                fl = strlen(fname);
                if(dn + fl + 16 < dst_size) {
                    memcpy(dst + dn, fname, fl);
                    dn += fl;
                    dst[dn++] = '(';
                }
                p = skip_ws(q) + 1;
                /* empty arg list? */
                if(*skip_ws(p) == ')') {
                    if(m->state_count > 0 && dn + 3 < dst_size) {
                        memcpy(dst + dn, "st", 2);
                        dn += 2;
                    }
                } else if(m->state_count > 0 && dn + 5 < dst_size) {
                    memcpy(dst + dn, "st, ", 4);
                    dn += 4;
                }
                continue;
            }
            /* Public Kryon constants become package constants. */
            {
                struct { const char *c; const char *go; } constants[] = {
                    {"Text8", "Text8"},
                    {"Text12", "Text12"},
                    {"Text14", "Text14"},
                    {"Text16", "Text16"},
                    {"Text18", "Text18"},
                    {"Text20", "Text20"},
                    {"Text24", "Text24"},
                    {"Text32", "Text32"},
                    {"Text48", "Text48"},
                    {"ButtonStylePrimary", "ButtonStylePrimary"},
                    {"ButtonStyleSecondary", "ButtonStyleSecondary"},
                    {"ButtonStyleDanger", "ButtonStyleDanger"},
                    {"ButtonStyleTab", "ButtonStyleTab"},
                    {"ButtonStyleTabSelected", "ButtonStyleTabSelected"},
                    {"THEME_STYLE_SYSTEM", "THEME_STYLE_SYSTEM"},
                    {"THEME_STYLE_RETRO", "THEME_STYLE_RETRO"},
                    {"THEME_STYLE_MATERIAL", "THEME_STYLE_MATERIAL"},
                    {"PICTURE_FIT_STRETCH", "PICTURE_FIT_STRETCH"},
                    {"PICTURE_FIT_CONTAIN", "PICTURE_FIT_CONTAIN"},
                    {"PICTURE_FIT_COVER", "PICTURE_FIT_COVER"},
                    {"WHITE", "WHITE"},
                    {"BLACK", "BLACK"},
                    {"RAYWHITE", "RAYWHITE"},
                    {"BLANK", "BLANK"},
                    {"LIGHTGRAY", "LIGHTGRAY"},
                    {"GRAY", "GRAY"},
                    {"DARKGRAY", "DARKGRAY"},
                    {"YELLOW", "YELLOW"},
                    {"GOLD", "GOLD"},
                    {"ORANGE", "ORANGE"},
                    {"PINK", "PINK"},
                    {"RED", "RED"},
                    {"MAROON", "MAROON"},
                    {"GREEN", "GREEN"},
                    {"LIME", "LIME"},
                    {"DARKGREEN", "DARKGREEN"},
                    {"SKYBLUE", "SKYBLUE"},
                    {"BLUE", "BLUE"},
                    {"DARKBLUE", "DARKBLUE"},
                    {"PURPLE", "PURPLE"},
                    {"VIOLET", "VIOLET"},
                    {"DARKPURPLE", "DARKPURPLE"},
                    {"BEIGE", "BEIGE"},
                    {"BROWN", "BROWN"},
                    {"DARKBROWN", "DARKBROWN"},
                    {"MAGENTA", "MAGENTA"},
                    {NULL, NULL}
                };
                int matched = 0;

                for(int ci = 0; constants[ci].c != NULL; ci++) {
                    if(strlen(constants[ci].c) == il &&
                       strncmp(constants[ci].c, ident, il) == 0) {
                        size_t gl = strlen(constants[ci].go);
                        int written = snprintf(dst + dn, dst_size - dn,
                                               "%s.", K2G_RUNTIME_PKG);
                        if(written > 0)
                            dn += (size_t)written;
                        if(dn + gl + 1 < dst_size) {
                            memcpy(dst + dn, constants[ci].go, gl);
                            dn += gl;
                        }
                        matched = 1;
                        break;
                    }
                }
                if(matched) {
                    p = q;
                    continue;
                }
            }
            /* enum members: bare ALL_CAPS name -> qualified Go const */
            {
                K2gEnumMember *mem = k2g_const_entry(ident, il);

                if(mem != NULL) {
                    size_t gl = strlen(mem->go);

                    if(dn + gl + 1 < dst_size) {
                        memcpy(dst + dn, mem->go, gl);
                        dn += gl;
                    }
                    p = q;
                    continue;
                }
            }
            if(k2g_is_array_name(ident, il)) {
                if(dn + il + 4 < dst_size) {
                    memcpy(dst + dn, ident, il);
                    dn += il;
                    dst[dn++] = '[';
                    dst[dn++] = ':';
                    dst[dn++] = ']';
                }
                p = q;
                continue;
            }
            /* runtime call? Capitalized identifiers route to the package API. */
            if(isupper((unsigned char)ident[0]) && *skip_ws(q) == '(' &&
               sfi < 0) {
                {
                    int written = snprintf(dst + dn, dst_size - dn,
                                           "%s.", K2G_RUNTIME_PKG);

                    if(written > 0)
                        dn += (size_t)written;
                }
                if(dn + il + 1 < dst_size) {
                    memcpy(dst + dn, ident, il);
                    dn += il;
                }
                p = q;
                continue;
            }
            /* plain identifier: verbatim */
            if(dn + il + 1 < dst_size) {
                memcpy(dst + dn, ident, il);
                dn += il;
            }
            p = q;
            continue;
        }
        dst[dn++] = *p++;
    }
    dst[dn] = '\0';
    (void)out;
}

/* strip trailing '{' and whitespace from a control header */
static void
strip_block_brace(char *s)
{
    size_t n = strlen(s);

    while(n > 0 && isspace((unsigned char)s[n - 1]))
        n--;
    if(n > 0 && s[n - 1] == '{')
        n--;
    while(n > 0 && isspace((unsigned char)s[n - 1]))
        n--;
    s[n] = '\0';
}

/* -------------------------------------------------------- module lowering */

static void
emit_indent(FILE *f, int n)
{
    for(int i = 0; i < n; i++)
        fputc('\t', f);
}

/* Rewrite a C-style three-clause for header into Go:
 *   for int i = 0; i < n; i++   ->  for i := int32(0); i < n; i++
 * The header arrives raw (kry names, semicolons intact, no 'for', no '{');
 * each clause is translated separately so tx_expr never sees the ';'. */
static void
lower_for_header(const KirModule *m, char *head, size_t head_size)
{
    char initraw[K2G_TEXT_MAX], condraw[K2G_TEXT_MAX], stepraw[K2G_TEXT_MAX];
    char cond[K2G_TEXT_MAX], step[K2G_TEXT_MAX], out[K2G_TEXT_MAX];
    size_t seps[2];
    int nsep = 0;
    int depth = 0;

    for(size_t i = 0; head[i] != '\0' && nsep < 2; i++) {
        char ch = head[i];

        if(ch == '(' || ch == '[' || ch == '{')
            depth++;
        else if(ch == ')' || ch == ']' || ch == '}')
            depth--;
        else if(ch == ';' && depth == 0)
            seps[nsep++] = i;
    }
    if(nsep < 2) {
        /* Go-style header (cond-only or infinite): translate verbatim */
        if(head[0] != '\0') {
            char tmp[K2G_TEXT_MAX];

            tx_expr(m, head, tmp, sizeof(tmp));
            snprintf(head, head_size, "%s", tmp);
        }
        return;
    }
    {
        size_t len = strlen(head);

        snprintf(initraw, sizeof(initraw), "%.*s", (int)seps[0], head);
        snprintf(condraw, sizeof(condraw), "%.*s", (int)(seps[1] - seps[0] - 1),
                 head + seps[0] + 1);
        snprintf(stepraw, sizeof(stepraw), "%.*s",
                 (int)(len - seps[1] - 1), head + seps[1] + 1);
    }
    {
        char *c = condraw, *s2 = stepraw;

        while(*c == ' ' || *c == '\t')
            c++;
        snprintf(condraw, sizeof(condraw), "%s", c);
        while(*s2 == ' ' || *s2 == '\t')
            s2++;
        snprintf(stepraw, sizeof(stepraw), "%s", s2);
    }
    tx_expr(m, condraw, cond, sizeof(cond));
    tx_expr(m, stepraw, step, sizeof(step));
    /* init: 'T name = expr' -> 'name := GoT(expr)'; 'name = expr' stays. */
    {
        char trimmed[K2G_TEXT_MAX];
        char *src2 = trimmed;
        char *eq;

        snprintf(trimmed, sizeof(trimmed), "%s", initraw);
        while(*src2 == ' ' || *src2 == '\t')
            src2++;
        eq = strchr(src2, '=');
        if(eq != NULL && eq > src2 && eq[-1] != '=' && eq[-1] != '!' &&
           eq[-1] != '<' && eq[-1] != '>') {
            char *name_end = eq;
            char *name_start;

            while(name_end > src2 &&
                  (name_end[-1] == ' ' || name_end[-1] == '\t'))
                name_end--;
            name_start = name_end;
            while(name_start > src2 && is_ident_char((unsigned char)name_start[-1]))
                name_start--;
            if(name_start < name_end) {
                char before[K2G_NAME_MAX];
                size_t bl = (size_t)(name_start - src2);
                char name[K2G_NAME_MAX];
                size_t nl = (size_t)(name_end - name_start);
                char *valraw = eq + 1;
                char val[K2G_TEXT_MAX];

                while(*valraw == ' ' || *valraw == '\t')
                    valraw++;
                if(bl >= sizeof(before))
                    bl = sizeof(before) - 1;
                memcpy(before, src2, bl);
                before[bl] = '\0';
                if(nl >= sizeof(name))
                    nl = sizeof(name) - 1;
                memcpy(name, name_start, nl);
                name[nl] = '\0';
                tx_expr(m, valraw, val, sizeof(val));
                /* one C type identifier before the name? */
                {
                    char ctype[K2G_NAME_MAX];
                    size_t cn = strlen(before);
                    int wordstart = -1;
                    int words = 0;

                    while(cn > 0 && (before[cn - 1] == ' ' ||
                                     before[cn - 1] == '\t'))
                        cn--;
                    for(size_t k = 0; k < cn; k++) {
                        if(is_ident_char((unsigned char)before[k])) {
                            if(wordstart < 0)
                                wordstart = (int)k;
                        } else if(wordstart >= 0) {
                            words++;
                            wordstart = -1;
                        }
                    }
                    if(wordstart >= 0)
                        words++;
                    if(words == 1 && wordstart == 0) {
                        char gt[K2G_NAME_MAX];

                        snprintf(ctype, sizeof(ctype), "%.*s", (int)cn,
                                 before);
                        if(go_type(ctype, gt, sizeof(gt)) && gt[0] != '\0')
                            snprintf(out, sizeof(out), "%s := %s(%s)", name,
                                     gt, val);
                        else
                            snprintf(out, sizeof(out), "%s := %s", name, val);
                        goto init_done;
                    }
                }
                snprintf(out, sizeof(out), "%s = %s", name, val);
                goto init_done;
            }
        }
        snprintf(out, sizeof(out), "%s", initraw);
    }
init_done:
    snprintf(head, head_size, "%s; %s; %s", out, cond, step);
}

static void
lower_function(FILE *f, const KirModule *m, const KirFunction *fn,
               const char *guard)
{
    char fname[K2G_NAME_MAX * 2];
    char ret[K2G_NAME_MAX];
    int indent = 1;

    camel(fn->name, fname, sizeof(fname));
    /* signature: (st *State, <converted args>) */
    {
        char parts[8][K2G_TEXT_MAX];
        int n, i;
        int emitted = 0;

        fprintf(f, "func %s_%s(", guard, fname);
        if(m->state_count > 0) {
            fprintf(f, "st *%sState", guard);
            emitted = 1;
        }
        if(fn->args[0] != '\0') {
            n = split_top(fn->args, parts, 8);
            for(i = 0; i < n; i++) {
                char *colon = strchr(parts[i], ':');
                char aname[K2G_NAME_MAX], atype[K2G_NAME_MAX];
                char gt[K2G_NAME_MAX];
                size_t al;

                if(colon == NULL)
                    continue;
                al = (size_t)(colon - parts[i]);
                while(al > 0 && parts[i][al - 1] == ' ')
                    al--;
                memcpy(aname, parts[i], al);
                aname[al] = '\0';
                snprintf(atype, sizeof(atype), "%s", colon + 1);
                if(!go_type(atype, gt, sizeof(gt)))
                    snprintf(gt, sizeof(gt), "/* TODO %s */ any", atype);
                fprintf(f, "%s%s %s", emitted ? ", " : "", aname, gt);
                emitted = 1;
            }
        }
        fprintf(f, ")");
        if(go_type(fn->return_type, ret, sizeof(ret)) && ret[0] != '\0')
            fprintf(f, " %s", ret);
        fprintf(f, " {\n");
    }
    for(int j = 0; j < fn->stmt_count; j++) {
        const KirStmt *st = &fn->stmts[j];
        char raw[K2G_TEXT_MAX];
        char rw[K2G_TEXT_MAX];

        /* Strip the block-open brace BEFORE translating: tx_expr treats a
         * trailing '{' as a braced group and would invent a matching '}',
         * and it drops the ';' separators C-style for headers need. */
        snprintf(raw, sizeof(raw), "%s", st->text);
        if(st->kind == KIR_STMT_IF || st->kind == KIR_STMT_WHILE ||
           st->kind == KIR_STMT_FOR || st->kind == KIR_STMT_SWITCH)
            strip_block_brace(raw);
        tx_expr(m, raw, rw, sizeof(rw));
        switch(st->kind) {
        case KIR_STMT_BLOCK_OPEN:
            emit_indent(f, indent++);
            fprintf(f, "{\n");
            if(indent < 1)
                indent = 1;
            break;
        case KIR_STMT_BLOCK_CLOSE:
            /* '} else {' must share one line in Go: when the next statement
             * is an else/else-if line, let it emit the merged close. */
            if(j + 1 < fn->stmt_count &&
               fn->stmts[j + 1].kind == KIR_STMT_IF) {
                char peek[K2G_TEXT_MAX];

                snprintf(peek, sizeof(peek), "%s", fn->stmts[j + 1].text);
                strip_block_brace(peek);
                if(strncmp(peek, "else", 4) == 0 &&
                   (peek[4] == '\0' || peek[4] == ' '))
                    break;
            }
            if(--indent < 1)
                indent = 1;
            emit_indent(f, indent);
            fprintf(f, "}\n");
            break;
        case KIR_STMT_IF: {
            char cond[K2G_TEXT_MAX];
            int chained = 0;

            snprintf(cond, sizeof(cond), "%s", rw);
            strip_block_brace(cond);
            /* 'guard cond' lowers to a plain if: the body is the exit path
             * and must return by itself (k2c fires defers + returns; on the
             * Go path defer runs at function exit anyway). */
            if(strncmp(cond, "guard ", 6) == 0)
                memmove(cond, cond + 6, strlen(cond + 6) + 1);
            if(strncmp(cond, "else", 4) == 0 &&
               (cond[4] == '\0' || cond[4] == ' '))
                chained = 1;
            emit_indent(f, indent);
            if(strncmp(cond, "else if ", 8) == 0)
                fprintf(f, "} else if %s {\n", cond + 8);
            else if(chained)
                fprintf(f, "} else {\n");
            else if(strncmp(cond, "if ", 3) == 0)
                fprintf(f, "if %s {\n", cond + 3);
            else
                fprintf(f, "if %s {\n", cond);
            /* an else branch reuses the level its BLOCK_CLOSE skipped */
            if(!chained)
                indent++;
            break;
        }
        case KIR_STMT_WHILE: {
            char cond[K2G_TEXT_MAX];

            snprintf(cond, sizeof(cond), "%s", rw);
            strip_block_brace(cond);
            if(strncmp(cond, "while ", 6) == 0)
                memmove(cond, cond + 6, strlen(cond + 6) + 1);
            emit_indent(f, indent);
            fprintf(f, "for %s {\n", cond);
            indent++;
            break;
        }
        case KIR_STMT_FOR: {
            char head[K2G_TEXT_MAX];

            snprintf(head, sizeof(head), "%s", raw);
            if(strncmp(head, "for ", 4) == 0)
                memmove(head, head + 4, strlen(head + 4) + 1);
            lower_for_header(m, head, sizeof(head));
            emit_indent(f, indent);
            if(head[0] != '\0')
                fprintf(f, "for %s {\n", head);
            else
                fprintf(f, "for {\n");
            indent++;
            break;
        }
        case KIR_STMT_SWITCH: {
            char head[K2G_TEXT_MAX];

            snprintf(head, sizeof(head), "%s", rw);
            strip_block_brace(head);
            emit_indent(f, indent);
            if(strncmp(head, "switch", 6) == 0 &&
                (head[6] == '\0' || head[6] == ' '))
                fprintf(f, "%s {\n", head);
            else
                fprintf(f, "switch %s {\n", head);
            indent++;
            break;
        }
        case KIR_STMT_CASE: {
            char head[K2G_TEXT_MAX];
            int had_brace = 0;
            size_t hl;

            snprintf(head, sizeof(head), "%s", st->text);
            hl = strlen(head);
            while(hl > 0 && (head[hl - 1] == ' ' || head[hl - 1] == '\t' ||
                             head[hl - 1] == '\r'))
                head[--hl] = '\0';
            if(hl > 0 && head[hl - 1] == '{') {
                had_brace = 1;
                head[--hl] = '\0';
                while(hl > 0 && (head[hl - 1] == ' ' || head[hl - 1] == '\t'))
                    head[--hl] = '\0';
            }
            /* ensure the trailing ':' survived expression translation */
            if(hl > 0 && head[hl - 1] != ':') {
                /* a same-line body ('case 1: foo()') keeps the statement;
                 * split it so the label stands alone */
                char *colon;

                {
                    char tmp[K2G_TEXT_MAX];

                    tx_expr(m, head, tmp, sizeof(tmp));
                    snprintf(head, sizeof(head), "%s", tmp);
                }
                colon = strchr(head, ':');
                if(colon != NULL && colon[1] != '\0') {
                    char rest[K2G_TEXT_MAX];

                    snprintf(rest, sizeof(rest), "%s", colon + 1);
                    colon[1] = '\0';
                    emit_indent(f, indent > 1 ? indent - 1 : 1);
                    fprintf(f, "%s\n", head);
                    emit_indent(f, indent);
                    fprintf(f, "%s\n", rest);
                    if(had_brace) {
                        emit_indent(f, indent);
                        fprintf(f, "{\n");
                        indent++;
                    }
                    break;
                }
                snprintf(head + hl, sizeof(head) - hl, ":");
            } else {
                char tmp[K2G_TEXT_MAX];

                tx_expr(m, head, tmp, sizeof(tmp));
                snprintf(head, sizeof(head), "%s", tmp);
            }
            emit_indent(f, indent > 1 ? indent - 1 : 1);
            fprintf(f, "%s\n", head);
            if(had_brace) {
                emit_indent(f, indent);
                fprintf(f, "{\n");
                indent++;
            }
            break;
        }
        case KIR_STMT_DECL: {
            char *colon = strchr(rw, ':');
            char *assign;

            k2g_register_arrays_stmt(st->text);
            emit_indent(f, indent);
            if(colon != NULL && colon[1] != '=') {
                char aname[K2G_NAME_MAX], gt[K2G_NAME_MAX];
                char tbuf[K2G_TEXT_MAX];
                size_t al = (size_t)(colon - rw);

                while(al > 0 && rw[al - 1] == ' ')
                    al--;
                memcpy(aname, rw, al);
                aname[al] = '\0';
                assign = strstr(colon, "= ");
                /* the declared type ends where the initializer begins */
                snprintf(tbuf, sizeof(tbuf), "%s", colon + 1);
                if(assign != NULL)
                    tbuf[assign - (colon + 1)] = '\0';
                if(!go_type(tbuf, gt, sizeof(gt)))
                    snprintf(gt, sizeof(gt), "/* TODO %s */ any", tbuf);
                if(assign != NULL) {
                    const char *init = skip_ws(assign + 2);

                    /* Go composite literals carry their type: 'var x = T{...}' */
                    if(*init == '{' && strstr(gt, "TODO") == NULL)
                        fprintf(f, "var %s = %s%s\n", aname, gt, init);
                    else
                        fprintf(f, "var %s %s = %s\n", aname, gt, assign + 2);
                } else
                    fprintf(f, "var %s %s\n", aname, gt);
            } else if(colon != NULL) { /* ':=' */
                fprintf(f, "%s\n", rw);
            } else {
                fprintf(f, "// TODO k2g decl: %s\n", st->text);
            }
            break;
        }
        case KIR_STMT_WIDGET: {
            char wname[K2G_NAME_MAX];
            char wargs[K2G_TEXT_MAX];

            camel(st->widget, wname, sizeof(wname));
            tx_expr(m, st->args, wargs, sizeof(wargs));
            emit_indent(f, indent);
            fprintf(f, "%s.%s(%s)\n", K2G_RUNTIME_PKG, wname, wargs);
            break;
        }
        case KIR_STMT_RETURN:
            emit_indent(f, indent);
            if(rw[0] != '\0' && strcmp(rw, "return") != 0)
                fprintf(f, "return %s\n", rw);
            else
                fprintf(f, "return\n");
            break;
        case KIR_STMT_BREAK:
        case KIR_STMT_CONTINUE:
            emit_indent(f, indent);
            fprintf(f, "%s\n", KirStmtKindName(st->kind));
            break;
        case KIR_STMT_DEFER:
            emit_indent(f, indent);
            fprintf(f, "defer func() { %s }()\n", rw);
            break;
        case KIR_STMT_LABEL:
        case KIR_STMT_GOTO:
            /* 'name:' and 'goto name' are valid Go; forward jumps over
             * declarations are a Go compile error, not a silent miscompile */
            emit_indent(f, indent);
            fprintf(f, "%s\n", rw);
            break;
        case KIR_STMT_ASSIGN:
        case KIR_STMT_EXPR:
            if(rw[0] != '\0') {
                emit_indent(f, indent);
                fprintf(f, "%s\n", rw);
            }
            break;
        default:
            emit_indent(f, indent);
            fprintf(f, "// TODO k2g %s: %s\n", KirStmtKindName(st->kind), rw);
            break;
        }
    }
    fprintf(f, "}\n\n");
}

static int
k2g_validate_asserts(const KirModule *m)
{
    for(int i = 0; i < m->assert_count; i++) {
        const KirAssert *a = &m->asserts[i];

        if(a->guard[0] != '\0') {
            fprintf(stderr,
                    "k2g: %s:%d: guarded #assert is not supported by the Go backend: %s\n",
                    a->span.path, a->span.line, a->message);
            return 0;
        }
        if(!a->known) {
            fprintf(stderr,
                    "k2g: %s:%d: unresolved #assert is not supported by the Go backend: %s\n",
                    a->span.path, a->span.line, a->condition);
            return 0;
        }
        if(!a->value) {
            fprintf(stderr, "k2g: %s:%d: #assert failed: %s\n",
                    a->span.path, a->span.line, a->message);
            return 0;
        }
    }
    return 1;
}

int
k2g_lower(const KirProgram *const *progs, int prog_count,
          const char *root, const char *out_dir, const char *pkg,
          int no_main)
{
    char path[1024];
    char seen_stems[64][512];
    int seen_count = 0;

    (void)root;
    for(int pi = 0; pi < prog_count; pi++) {
        const KirProgram *prog = progs[pi];

        for(int mi = 0; mi < prog->module_count; mi++) {
            const KirModule *m = &prog->modules[mi];
            char stem[512], guard[K2G_NAME_MAX];
            FILE *f;

            stem_from_source(m->source_path, stem, sizeof(stem));
            /* flat output: two sources with the same basename would collide */
            for(int si = 0; si < seen_count; si++) {
                if(strcmp(seen_stems[si], stem) == 0) {
                    fprintf(stderr,
                            "k2g: duplicate source basename %s "
                            "(Go output is flat)\n", stem);
                    return 1;
                }
            }
            if(!k2g_validate_asserts(m))
                return 1;
            if(seen_count < 64)
                snprintf(seen_stems[seen_count++], sizeof(seen_stems[0]),
                         "%s", stem);
            camel(stem, guard, sizeof(guard));
            k2g_array_count = 0;
            k2g_register_arrays_module(m);
            snprintf(path, sizeof(path), "%s/%s.go", out_dir, stem);
            mkdir_parent(path);
            f = fopen(path, "wb");
            if(f == NULL) {
                fprintf(stderr, "k2g: cannot write %s\n", path);
                continue;
            }
            k2g_set_module(m, guard);
            fprintf(f, "// Code generated by k2g from %s. DO NOT EDIT.\n",
                    m->source_path);
            fprintf(f, "package %s\n\n", pkg);
            fprintf(f, "import %s \"%s\"\n\n", K2G_RUNTIME_PKG,
                    K2G_RUNTIME_IMPORT);

            for(int i = 0; i < m->import_count; i++) {
                const KirImport *imp = &m->imports[i];

                if(imp->kind == KIR_IMPORT_HEADER)
                    fprintf(f, "// #import %s\n", imp->target);
                /* KIR_IMPORT_EXTERN is bridged by the Host interface below. */
            }
            /* '#extern' host bridge: one interface, one package var, one
             * setter. Generated frames call hostVar.Method(...) directly. */
            if(g_extern_count > 0) {
                fprintf(f, "// %sHost bridges '#extern' declarations to the",
                        guard);
                fprintf(f, " embedding Go program.\ntype %sHost interface {\n",
                        guard);
                for(int i = 0; i < g_extern_count; i++) {
                    const K2gExtern *ex = &g_externs[i];
                    char gt[K2G_NAME_MAX];

                    fprintf(f, "\t%s(", ex->go);
                    for(int a = 0; a < ex->pcount; a++) {
                        char aname[K2G_NAME_MAX];

                        camel(ex->pnames[a], aname, sizeof(aname));
                        if(!go_type(ex->ptypes[a], gt, sizeof(gt)))
                            snprintf(gt, sizeof(gt), "any");
                        fprintf(f, "%s%s %s", a > 0 ? ", " : "", aname, gt);
                    }
                    fprintf(f, ")");
                    if(go_type(ex->ret, gt, sizeof(gt)) && gt[0] != '\0')
                        fprintf(f, " %s", gt);
                    fprintf(f, "\n");
                }
                fprintf(f, "}\n\n");
                fprintf(f, "var %s %sHost\n\n", g_externs[0].host_var, guard);
                fprintf(f, "// Set%sHost wires the '#extern' bridge before",
                        guard);
                fprintf(f, " the first frame runs.\nfunc Set%sHost(host %sHost)",
                        guard, guard);
                fprintf(f, " {\n\t%s = host\n}\n\n", g_externs[0].host_var);
            }
            /* types */
            int enum_idx = 0;

            for(int i = 0; i < m->type_count; i++) {
                const KirType *t = &m->types[i];

                if(t->is_enum) {
                    /* enums: typed constants with C counter semantics
                         * (g_enums was built in the same order as m->types) */
                        K2gEnum *e = enum_idx < g_enum_count
                                         ? &g_enums[enum_idx++] : NULL;

                        if(e == NULL)
                            continue;
                        if(e->go_type[0] != '\0')
                            fprintf(f, "type %s int32\n\n", e->go_type);
                        fprintf(f, "const (\n");
                        long counter = 0;   /* -1: unknown (non-literal value) */
                        char last_expr[K2G_TEXT_MAX];

                        last_expr[0] = '\0';
                        for(int mI = 0; mI < e->count; mI++) {
                            K2gEnumMember *mem = &e->members[mI];
                            char val[K2G_TEXT_MAX];

                            if(mem->val[0] != '\0') {
                                char *end;
                                long parsed;

                                tx_expr(m, mem->val, val, sizeof(val));
                                parsed = strtol(mem->val, &end, 0);
                                if(*end == '\0')
                                    counter = parsed + 1;
                                else {
                                    counter = -1;
                                    snprintf(last_expr, sizeof(last_expr),
                                             "%s", val);
                                }
                            } else if(counter >= 0) {
                                snprintf(val, sizeof(val), "%ld", counter);
                                counter++;
                            } else {
                                snprintf(val, sizeof(val), "%s + 1",
                                         last_expr);
                                snprintf(last_expr, sizeof(last_expr), "%s",
                                         val);
                            }
                            /* untyped: C enums convert implicitly; Go's
                             * typed consts would not mix with int fields */
                            fprintf(f, "\t%s = %s\n", mem->go, val);
                        }
                        fprintf(f, ")\n\n");
                    } else {
                        fprintf(f, "type %s struct {\n", t->name);
                    {
                        char line[K2G_TEXT_MAX];
                        const char *p = t->body;

                        while(*p != '\0') {
                            const char *e = strchr(p, '\n');
                            size_t len = e != NULL ? (size_t)(e - p)
                                                   : strlen(p);
                            char *colon;

                            if(len >= sizeof(line))
                                len = sizeof(line) - 1;
                            memcpy(line, p, len);
                            line[len] = '\0';
                            colon = strchr(line, ':');
                            if(colon != NULL) {
                                char fname[K2G_NAME_MAX], gt[K2G_NAME_MAX];
                                size_t fl = (size_t)(colon - line);

                                camel(line, fname, sizeof(fname));
                                if(!go_type(colon + 1, gt, sizeof(gt)))
                                    snprintf(gt, sizeof(gt),
                                             "/* TODO %s */ any", colon + 1);
                                fprintf(f, "\t%s %s\n", fname, gt);
                            }
                            p = e != NULL ? e + 1 : p + len;
                        }
                    }
                    fprintf(f, "}\n\n");
                }
            }
            /* defines -> consts */
            for(int i = 0; i < m->define_count; i++) {
                char cname[K2G_NAME_MAX];
                char cval[K2G_TEXT_MAX];

                camel(m->defines[i].name, cname, sizeof(cname));
                tx_expr(m, m->defines[i].value, cval, sizeof(cval));
                fprintf(f, "const %s = %s\n", cname, cval);
            }
            /* globals */
            for(int i = 0; i < m->global_count; i++) {
                const KirGlobal *g = &m->globals[i];
                char gname[K2G_NAME_MAX], gt[K2G_NAME_MAX], ginit[K2G_TEXT_MAX];

                camel(g->name, gname, sizeof(gname));
                if(!go_type(g->type, gt, sizeof(gt)))
                    snprintf(gt, sizeof(gt), "/* TODO %s */ any", g->type);
                tx_expr(m, g->init, ginit, sizeof(ginit));
                if(ginit[0] != '\0')
                    fprintf(f, "var %s %s = %s\n", gname, gt, ginit);
                else
                    fprintf(f, "var %s %s\n", gname, gt);
            }
            /* state struct + instance */
            if(m->state_count > 0) {
                fprintf(f, "type %sState struct {\n", guard);
                for(int i = 0; i < m->state_count; i++) {
                    const KirStateField *sf = &m->state_fields[i];
                    char fname[K2G_NAME_MAX], gt[K2G_NAME_MAX];

                    camel(sf->name, fname, sizeof(fname));
                    if(!go_type(sf->type, gt, sizeof(gt)))
                        snprintf(gt, sizeof(gt), "/* TODO %s */ any", sf->type);
                    fprintf(f, "\t%s %s\n", fname, gt);
                }
                fprintf(f, "}\n\n");
                fprintf(f, "var %sStateValue = &%sState{\n", guard, guard);
                for(int i = 0; i < m->state_count; i++) {
                    const KirStateField *sf = &m->state_fields[i];
                    char fname[K2G_NAME_MAX], finit[K2G_TEXT_MAX];
                    char gt[K2G_NAME_MAX];

                    camel(sf->name, fname, sizeof(fname));
                    if(!go_type(sf->type, gt, sizeof(gt)))
                        snprintf(gt, sizeof(gt), "any");
                    tx_expr(m, sf->init, finit, sizeof(finit));
                    if(finit[0] != '\0' &&
                       !(sf->type[0] == '[' && strstr(sf->type, "char") != NULL &&
                         strcmp(finit, "\"\"") == 0)) {
                        if(sf->type[0] == '[' && strstr(sf->type, "char") != NULL &&
                           finit[0] == '"')
                            fprintf(f,
                                    "\t%s: func() %s { var v %s; copy(v[:], %s); return v }(),\n",
                                    fname, gt, gt, finit);
                        else
                            fprintf(f, "\t%s: %s,\n", fname, finit);
                    }
                }
                fprintf(f, "}\n\n");
            }
            /* functions ('#extern' prototypes have no body: they lower to
             * Host interface methods, not Go functions) */
            for(int i = 0; i < m->function_count; i++) {
                if(m->functions[i].is_extern)
                    continue;
                lower_function(f, m, &m->functions[i], guard);
            }

            /* app -> main */
            if(m->app.has_app && !no_main) {
                char frame[K2G_NAME_MAX * 2];

                camel(m->app.frame, frame, sizeof(frame));
                fprintf(f, "func main() {\n");
                fprintf(f, "\t%s.Open(%s.AppConfig{\n", K2G_RUNTIME_PKG,
                        K2G_RUNTIME_PKG);
                fprintf(f, "\t\tTitle: \"%s\",\n", m->app.title);
                fprintf(f, "\t\tWidth: %d, Height: %d, FPS: %d,\n",
                        m->app.width, m->app.height, m->app.fps);
                fprintf(f, "\t})\n");
                fprintf(f, "\tdefer %s.Close()\n", K2G_RUNTIME_PKG);
                fprintf(f, "\tfor !%s.WindowShouldClose() {\n",
                        K2G_RUNTIME_PKG);
                if(m->state_count > 0)
                    fprintf(f, "\t\t%s_%s(%sStateValue)\n", guard, frame, guard);
                else
                    fprintf(f, "\t\t%s_%s()\n", guard, frame);
                fprintf(f, "\t}\n}\n");
            }
            fclose(f);
        }
    }
    return 0;
}
