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

/* stem of "examples/foo.kry" relative to root -> "foo"-ish path */
static void
stem_from_source(const char *src, char *dst, size_t dst_size)
{
    size_t n = strlen(src);

    if(n > 4 && strcmp(src + n - 4, ".kry") == 0)
        n -= 4;
    if(n >= dst_size)
        n = dst_size - 1;
    memcpy(dst, src, n);
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

/* C-ish type -> Go type. Returns 0 when unknown (caller falls back). */
static int
go_type(const char *type, char *dst, size_t dst_size)
{
    struct {
        const char *c;
        const char *go;
    } map[] = {
        {"int", "int32"},   {"unsigned int", "uint32"},
        {"long", "int64"},  {"unsigned long", "uint64"},
        {"float", "float32"}, {"double", "float64"},
        {"bool", "bool"},   {"char*", "string"}, {"const char*", "string"},
        {"void", ""},
        {"Vector2", "kryruntime.Vector2"}, {"Rectangle", "kryruntime.Rectangle"},
        {"Color", "kryruntime.Color"},     {"Texture2D", "kryruntime.Texture2D"},
        {NULL, NULL}
    };
    char t[K2G_NAME_MAX];
    size_t n = strlen(type);

    snprintf(t, sizeof(t), "%s", type);
    while(n > 0 && (t[n - 1] == ' ' || t[n - 1] == '\t'))
        t[--n] = '\0';
    for(int i = 0; map[i].c != NULL; i++) {
        if(strcmp(t, map[i].c) == 0) {
            snprintf(dst, dst_size, "%s", map[i].go);
            return 1;
        }
    }
    return 0;
}

static int
is_ident_char(int c)
{
    return isalnum(c) || c == '_';
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
                snprintf(out + on, sizeof(out) - on, "kryruntime.NewVector2");
            else if(strcmp(type, "Rectangle") == 0 && n == 4)
                snprintf(out + on, sizeof(out) - on, "kryruntime.NewRectangle");
            else {
                /* odd arity: emit a TODO-safe zero value */
                snprintf(dst + *dn, K2G_TEXT_MAX - *dn, "%s", "kryruntime.NewVector2(0, 0)");
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
                        "kryruntime.Color{%s: %s, %s: %s, %s: %s, %s: %s}",
                        fields[0], args[0], fields[1], args[1],
                        fields[2], args[2], fields[3], args[3]);
                }
            }
            return p;
        }
        /* other struct literals: Type{...} — recurse and keep braces */
        {
            char ctor[K2G_NAME_MAX + 8];

            snprintf(ctor, sizeof(ctor), "kryruntime.%s{", type);
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
                    if(dn + 8 < dst_size) {
                        memcpy(dst + dn, "rt, st", 6);
                        dn += 6;
                    }
                } else if(dn + 8 < dst_size) {
                    memcpy(dst + dn, "rt, st, ", 8);
                    dn += 8;
                }
                continue;
            }
            /* Public Kryon constants become package constants. */
            {
                struct { const char *c; const char *go; } constants[] = {
                    {"UI_TEXT_8", "kryruntime.UIText8"},
                    {"UI_TEXT_12", "kryruntime.UIText12"},
                    {"UI_TEXT_16", "kryruntime.UIText16"},
                    {"UI_TEXT_24", "kryruntime.UIText24"},
                    {"UI_BUTTON_STYLE_PRIMARY", "kryruntime.UIButtonStylePrimary"},
                    {"UI_BUTTON_STYLE_SECONDARY", "kryruntime.UIButtonStyleSecondary"},
                    {"UI_BUTTON_STYLE_DANGER", "kryruntime.UIButtonStyleDanger"},
                    {NULL, NULL}
                };
                int matched = 0;

                for(int ci = 0; constants[ci].c != NULL; ci++) {
                    if(strlen(constants[ci].c) == il &&
                       strncmp(constants[ci].c, ident, il) == 0) {
                        size_t gl = strlen(constants[ci].go);
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
            /* runtime call? Capitalized identifiers route to rt. */
            if(isupper((unsigned char)ident[0]) && *skip_ws(q) == '(' &&
               sfi < 0) {
                if(dn + il + 5 < dst_size) {
                    dst[dn++] = 'r';
                    dst[dn++] = 't';
                    dst[dn++] = '.';
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

static void
lower_function(FILE *f, const KirModule *m, const KirFunction *fn,
               const char *guard)
{
    char fname[K2G_NAME_MAX * 2];
    char ret[K2G_NAME_MAX];
    int indent = 1;

    camel(fn->name, fname, sizeof(fname));
    /* signature: (rt Runtime, st *State, <converted args>) */
    {
        char parts[8][K2G_TEXT_MAX];
        int n, i, first = 1;

        fprintf(f, "func %s_%s(rt kryruntime.Runtime", guard, fname);
        if(m->state_count > 0)
            fprintf(f, ", st *%sState", guard);
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
                fprintf(f, ", %s %s", aname, gt);
                (void)first;
            }
        }
        fprintf(f, ")");
        if(go_type(fn->return_type, ret, sizeof(ret)) && ret[0] != '\0')
            fprintf(f, " %s", ret);
        fprintf(f, " {\n");
    }
    for(int j = 0; j < fn->stmt_count; j++) {
        const KirStmt *st = &fn->stmts[j];
        char rw[K2G_TEXT_MAX];

        tx_expr(m, st->text, rw, sizeof(rw));
        switch(st->kind) {
        case KIR_STMT_BLOCK_OPEN:
            emit_indent(f, indent++);
            fprintf(f, "{\n");
            if(indent < 1)
                indent = 1;
            break;
        case KIR_STMT_BLOCK_CLOSE:
            if(--indent < 1)
                indent = 1;
            emit_indent(f, indent);
            fprintf(f, "}\n");
            break;
        case KIR_STMT_IF: {
            char cond[K2G_TEXT_MAX];

            snprintf(cond, sizeof(cond), "%s", rw);
            strip_block_brace(cond);
            emit_indent(f, indent);
            if(strncmp(cond, "else if ", 8) == 0)
                fprintf(f, "} else if %s {\n", cond + 8);
            else if(strncmp(cond, "else", 4) == 0 && cond[4] == '\0')
                fprintf(f, "} else {\n");
            else if(strncmp(cond, "if ", 3) == 0)
                fprintf(f, "if %s {\n", cond + 3);
            else
                fprintf(f, "if %s {\n", cond);
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
        case KIR_STMT_FOR:
        case KIR_STMT_SWITCH: {
            char head[K2G_TEXT_MAX];
            const char *kw = st->kind == KIR_STMT_FOR ? "for" : "switch";

            snprintf(head, sizeof(head), "%s", rw);
            strip_block_brace(head);
            emit_indent(f, indent);
            if(strncmp(head, kw, strlen(kw)) == 0 && head[strlen(kw)] == ' ')
                fprintf(f, "%s %s {\n", kw, head + strlen(kw) + 1);
            else
                fprintf(f, "%s %s {\n", kw, head);
            indent++;
            break;
        }
        case KIR_STMT_DECL: {
            char *colon = strchr(rw, ':');
            char *assign;

            emit_indent(f, indent);
            if(colon != NULL && colon[1] != '=') {
                char aname[K2G_NAME_MAX], gt[K2G_NAME_MAX];
                size_t al = (size_t)(colon - rw);

                while(al > 0 && rw[al - 1] == ' ')
                    al--;
                memcpy(aname, rw, al);
                aname[al] = '\0';
                if(!go_type(colon + 1, gt, sizeof(gt)))
                    snprintf(gt, sizeof(gt), "/* TODO %s */ any", colon + 1);
                assign = strstr(colon, "= ");
                if(assign != NULL)
                    fprintf(f, "var %s %s = %s\n", aname, gt, assign + 2);
                else
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
            fprintf(f, "rt.%s(%s)\n", wname, wargs);
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
            emit_indent(f, indent);
            fprintf(f, "// TODO k2g %s: %s\n",
                    st->kind == KIR_STMT_LABEL ? "label" : "goto", rw);
            break;
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

void
k2g_lower(const KirProgram *const *progs, int prog_count,
          const char *root, const char *out_dir, const char *pkg,
          const char *runtime_import, int no_main)
{
    char path[1024];

    (void)root;
    for(int pi = 0; pi < prog_count; pi++) {
        const KirProgram *prog = progs[pi];

        for(int mi = 0; mi < prog->module_count; mi++) {
            const KirModule *m = &prog->modules[mi];
            char stem[512], guard[K2G_NAME_MAX];
            FILE *f;

            stem_from_source(m->source_path, stem, sizeof(stem));
            camel(stem, guard, sizeof(guard));
            snprintf(path, sizeof(path), "%s/%s.go", out_dir, stem);
            mkdir_parent(path);
            f = fopen(path, "wb");
            if(f == NULL) {
                fprintf(stderr, "k2g: cannot write %s\n", path);
                continue;
            }
            fprintf(f, "// Code generated by k2g from %s. DO NOT EDIT.\n",
                    m->source_path);
            fprintf(f, "package %s\n\n", pkg);
            fprintf(f, "import kryruntime \"%s\"\n\n", runtime_import);

            for(int i = 0; i < m->import_count; i++) {
                const KirImport *imp = &m->imports[i];

                if(imp->kind == KIR_IMPORT_HEADER)
                    fprintf(f, "// #import %s\n", imp->target);
                else if(imp->kind == KIR_IMPORT_EXTERN)
                    fprintf(f, "// TODO k2g extern %s :: %s\n", imp->name,
                            imp->signature);
            }
            /* types */
            for(int i = 0; i < m->type_count; i++) {
                const KirType *t = &m->types[i];

                if(t->is_enum) {
                    fprintf(f, "// TODO k2g enum %s\n\ntype %s int32\n\n",
                            t->name, t->name);
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

                    camel(sf->name, fname, sizeof(fname));
                    tx_expr(m, sf->init, finit, sizeof(finit));
                    if(finit[0] != '\0')
                        fprintf(f, "\t%s: %s,\n", fname, finit);
                }
                fprintf(f, "}\n\n");
            }
            /* functions */
            for(int i = 0; i < m->function_count; i++)
                lower_function(f, m, &m->functions[i], guard);

            /* app -> main */
            if(m->app.has_app && !no_main) {
                char frame[K2G_NAME_MAX * 2];

                camel(m->app.frame, frame, sizeof(frame));
                fprintf(f, "func main() {\n");
                fprintf(f, "\trt := kryruntime.New(kryruntime.AppConfig{\n");
                fprintf(f, "\t\tTitle: \"%s\",\n", m->app.title);
                fprintf(f, "\t\tWidth: %d, Height: %d, FPS: %d,\n",
                        m->app.width, m->app.height, m->app.fps);
                fprintf(f, "\t})\n");
                fprintf(f, "\tdefer rt.Close()\n");
                fprintf(f, "\tfor !rt.WindowShouldClose() {\n");
                if(m->state_count > 0)
                    fprintf(f, "\t\t%s_%s(rt, %sStateValue)\n", guard, frame, guard);
                else
                    fprintf(f, "\t\t%s_%s(rt)\n", guard, frame);
                fprintf(f, "\t}\n}\n");
            }
            fclose(f);
        }
    }
}
