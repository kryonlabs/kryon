/*
 * k2js_lower.c - Kir -> browser-loadable JavaScript backend.
 */
#include "k2js_lower.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define K2JS_TEXT_MAX 8192
#define K2JS_NAME_MAX 256
#define K2JS_PARAM_MAX 32
#define K2JS_EXTERN_MAX 128

typedef struct {
    char kry[K2JS_NAME_MAX];
    char js[K2JS_NAME_MAX];
    char method[K2JS_NAME_MAX];
} K2jsExtern;

typedef struct {
    char kry[K2JS_NAME_MAX];
    char js[K2JS_NAME_MAX * 2];
    char guard[K2JS_NAME_MAX];
    int state_count;
} K2jsGlobalFunction;

static K2jsGlobalFunction g_functions[512];
static int g_function_count;
static const KirModule *g_mod;
static char g_guard[K2JS_NAME_MAX];
static K2jsExtern g_externs[K2JS_EXTERN_MAX];
static int g_extern_count;

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

static void
base_from_source(const char *src, char *dst, size_t dst_size)
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

static int
is_ident_char(int c)
{
    return isalnum(c) || c == '_';
}

static void
camel(const char *s, char *dst, size_t dst_size)
{
    size_t n = 0;
    int up = 1;

    for(const char *p = s; *p != '\0' && n + 1 < dst_size; p++) {
        if(isalnum((unsigned char)*p)) {
            dst[n++] = up ? (char)toupper((unsigned char)*p) : *p;
            up = 0;
        } else {
            up = 1;
        }
    }
    if(n == 0 && dst_size > 1)
        dst[n++] = 'X';
    if(isdigit((unsigned char)dst[0]) && n + 1 < dst_size) {
        memmove(dst + 1, dst, n + 1);
        dst[0] = 'M';
        n++;
    }
    dst[n] = '\0';
}

static void
js_ident(const char *s, char *dst, size_t dst_size)
{
    size_t n = 0;

    if(dst_size == 0)
        return;
    for(const char *p = s; *p != '\0' && n + 1 < dst_size; p++) {
        if((n == 0 && (isalpha((unsigned char)*p) || *p == '_')) ||
           (n > 0 && is_ident_char((unsigned char)*p))) {
            dst[n++] = *p;
        } else if(n > 0) {
            dst[n++] = '_';
        }
    }
    if(n == 0)
        dst[n++] = '_';
    dst[n] = '\0';
}

static const char *
skip_ws(const char *p)
{
    while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    return p;
}

static void
trim(char *s)
{
    size_t n;
    char *p = s;

    while(*p == ' ' || *p == '\t')
        p++;
    if(p != s)
        memmove(s, p, strlen(p) + 1);
    n = strlen(s);
    while(n > 0 && isspace((unsigned char)s[n - 1]))
        s[--n] = '\0';
}

static void
js_string(FILE *f, const char *s)
{
    fputc('"', f);
    for(const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
        if(*p == '\\' || *p == '"')
            fprintf(f, "\\%c", *p);
        else if(*p == '\n')
            fprintf(f, "\\n");
        else if(*p == '\r')
            fprintf(f, "\\r");
        else if(*p == '\t')
            fprintf(f, "\\t");
        else if(*p < 0x20)
            fprintf(f, "\\u%04x", *p);
        else
            fputc(*p, f);
    }
    fputc('"', f);
}

static void
js_string_buf(const char *s, char *dst, size_t dst_size)
{
    size_t n = 0;

    if(dst_size == 0)
        return;
    dst[n++] = '"';
    for(const unsigned char *p = (const unsigned char *)s;
        *p != '\0' && n + 8 < dst_size; p++) {
        if(*p == '\\' || *p == '"') {
            dst[n++] = '\\';
            dst[n++] = (char)*p;
        } else if(*p == '\n') {
            dst[n++] = '\\';
            dst[n++] = 'n';
        } else if(*p == '\r') {
            dst[n++] = '\\';
            dst[n++] = 'r';
        } else if(*p == '\t') {
            dst[n++] = '\\';
            dst[n++] = 't';
        } else {
            dst[n++] = (char)*p;
        }
    }
    if(n + 1 < dst_size)
        dst[n++] = '"';
    dst[n] = '\0';
}

static int
split_top(const char *s, char parts[][K2JS_TEXT_MAX], int max)
{
    int depth = 0, n = 0;
    const char *start = s;

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

            if(n < max && len < K2JS_TEXT_MAX) {
                memcpy(parts[n], start, len);
                parts[n][len] = '\0';
                trim(parts[n]);
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

static int
global_fn_index(const char *name, size_t len)
{
    int match = -1;

    for(int i = 0; i < g_function_count; i++) {
        if(strlen(g_functions[i].kry) != len ||
           strncmp(g_functions[i].kry, name, len) != 0)
            continue;
        if(match >= 0)
            return -2;
        match = i;
    }
    return match;
}

static int
extern_index(const char *name, size_t len)
{
    for(int i = 0; i < g_extern_count; i++) {
        if(strlen(g_externs[i].kry) == len &&
           strncmp(g_externs[i].kry, name, len) == 0)
            return i;
    }
    return -1;
}

static void
method_name_from_target(const char *target, const char *fallback,
                        char *dst, size_t dst_size)
{
    const char *base;

    if(target == NULL || target[0] == '\0') {
        camel(fallback, dst, dst_size);
        return;
    }
    if(strncmp(target, "js.", 3) == 0)
        base = target + 3;
    else {
        const char *dot = strrchr(target, '.');

        base = dot != NULL ? dot + 1 : target;
    }
    camel(base[0] != '\0' ? base : fallback, dst, dst_size);
}

static void
add_extern(const char *kry, const char *target)
{
    K2jsExtern *ex;

    if(extern_index(kry, strlen(kry)) >= 0 || g_extern_count >= K2JS_EXTERN_MAX)
        return;
    ex = &g_externs[g_extern_count++];
    memset(ex, 0, sizeof(*ex));
    snprintf(ex->kry, sizeof(ex->kry), "%s", kry);
    js_ident(kry, ex->js, sizeof(ex->js));
    method_name_from_target(target, kry, ex->method, sizeof(ex->method));
}

static void
set_module(const KirModule *m, const char *guard)
{
    g_mod = m;
    snprintf(g_guard, sizeof(g_guard), "%s", guard);
    g_extern_count = 0;
    for(int i = 0; i < m->import_count; i++) {
        if(m->imports[i].kind == KIR_IMPORT_EXTERN)
            add_extern(m->imports[i].name, m->imports[i].target);
    }
    for(int i = 0; i < m->function_count; i++) {
        const KirFunction *fn = &m->functions[i];

        if(fn->is_extern)
            add_extern(fn->name, fn->extern_target);
    }
}

static void
build_global_functions(const KirProgram *const *progs, int prog_count)
{
    g_function_count = 0;
    for(int pi = 0; pi < prog_count; pi++) {
        const KirProgram *prog = progs[pi];

        for(int mi = 0; mi < prog->module_count; mi++) {
            const KirModule *m = &prog->modules[mi];
            char base[KIR_PATH_MAX];
            char guard[K2JS_NAME_MAX];

            base_from_source(m->source_path, base, sizeof(base));
            camel(base, guard, sizeof(guard));
            for(int fi = 0; fi < m->function_count; fi++) {
                const KirFunction *fn = &m->functions[fi];
                char fname[K2JS_NAME_MAX];

                if(fn->is_extern || g_function_count >= 512)
                    continue;
                camel(fn->name, fname, sizeof(fname));
                snprintf(g_functions[g_function_count].kry,
                         sizeof(g_functions[0].kry), "%s", fn->name);
                snprintf(g_functions[g_function_count].guard,
                         sizeof(g_functions[0].guard), "%s", guard);
                snprintf(g_functions[g_function_count].js,
                         sizeof(g_functions[0].js), "%s_%s", guard, fname);
                g_functions[g_function_count].state_count = m->state_count;
                g_function_count++;
            }
        }
    }
}

static int
contains_top_level_compound(const char *s)
{
    int in_string = 0;
    char quote = '\0';

    for(const char *p = s; *p != '\0'; p++) {
        if(in_string) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == quote)
                in_string = 0;
            continue;
        }
        if(*p == '"' || *p == '\'') {
            in_string = 1;
            quote = *p;
        } else if(*p == '{' || *p == '}') {
            return 1;
        }
    }
    return 0;
}

static void tx_expr(const KirModule *m, const char *src,
                    char *dst, size_t dst_size);

static void
tx_args(const KirModule *m, const char *raw, char *dst, size_t dst_size)
{
    char parts[K2JS_PARAM_MAX][K2JS_TEXT_MAX];
    int n = split_top(raw, parts, K2JS_PARAM_MAX);
    size_t dn = 0;

    dst[0] = '\0';
    for(int i = 0; i < n; i++) {
        char arg[K2JS_TEXT_MAX];

        tx_expr(m, parts[i], arg, sizeof(arg));
        dn += (size_t)snprintf(dst + dn, dst_size - dn, "%s%s",
                               i > 0 ? ", " : "", arg);
        if(dn >= dst_size)
            break;
    }
}

static const char *
consume_group(const char *p, char *raw, size_t raw_size)
{
    int depth = 1;
    size_t rn = 0;

    while(*p != '\0' && depth > 0 && rn + 1 < raw_size) {
        if(*p == '"' || *p == '\'') {
            char q = *p;

            raw[rn++] = *p++;
            while(*p != '\0' && *p != q && rn + 2 < raw_size) {
                if(*p == '\\') {
                    raw[rn++] = *p++;
                    if(*p != '\0')
                        raw[rn++] = *p++;
                } else {
                    raw[rn++] = *p++;
                }
            }
            if(*p == q)
                raw[rn++] = *p++;
            continue;
        }
        if(*p == '(' || *p == '[' || *p == '{')
            depth++;
        else if(*p == ')' || *p == ']' || *p == '}') {
            depth--;
            if(depth == 0)
                break;
        }
        raw[rn++] = *p++;
    }
    raw[rn] = '\0';
    return depth == 0 && *p != '\0' ? p + 1 : p;
}

static void
tx_expr(const KirModule *m, const char *src, char *dst, size_t dst_size)
{
    size_t dn = 0;
    const char *p = src;

    dst[0] = '\0';
    if(contains_top_level_compound(src) || strstr(src, "sizeof") != NULL) {
        char q[K2JS_TEXT_MAX];

        js_string_buf(src, q, sizeof(q));
        snprintf(dst, dst_size, "kryon.expr(%s)", q);
        return;
    }
    while(*p != '\0' && dn + 16 < dst_size) {
        if(*p == ';' || *p == '\r' || *p == '\n') {
            p++;
            continue;
        }
        if(*p == '"' || *p == '\'') {
            char q = *p;

            dst[dn++] = *p++;
            while(*p != '\0' && *p != q && dn + 2 < dst_size) {
                if(*p == '\\')
                    dst[dn++] = *p++;
                dst[dn++] = *p++;
            }
            if(*p == q)
                dst[dn++] = *p++;
            continue;
        }
        if(*p == '&') {
            const char *q = p + 1;
            char ident[K2JS_NAME_MAX];
            size_t il = 0;

            while(is_ident_char((unsigned char)*q) && il + 1 < sizeof(ident))
                ident[il++] = *q++;
            ident[il] = '\0';
            if(il > 0 && state_field_index(m, ident, il) >= 0) {
                char qs[K2JS_TEXT_MAX];

                js_string_buf(ident, qs, sizeof(qs));
                dn += (size_t)snprintf(dst + dn, dst_size - dn,
                                       "kryon.ref(state, %s)", qs);
                p = q;
                continue;
            }
        }
        if(isalpha((unsigned char)*p) || *p == '_') {
            const char *q = p;
            char ident[K2JS_NAME_MAX];
            size_t il = 0;
            int sfi, fni, xi;

            while(is_ident_char((unsigned char)*q) && il + 1 < sizeof(ident))
                ident[il++] = *q++;
            ident[il] = '\0';
            if((strcmp(ident, "NULL") == 0 || strcmp(ident, "nil") == 0) &&
               !is_ident_char((unsigned char)*q)) {
                dn += (size_t)snprintf(dst + dn, dst_size - dn, "null");
                p = q;
                continue;
            }
            if(strcmp(ident, "true") == 0 || strcmp(ident, "false") == 0) {
                dn += (size_t)snprintf(dst + dn, dst_size - dn, "%s", ident);
                p = q;
                continue;
            }
            sfi = state_field_index(m, ident, il);
            if(sfi >= 0) {
                dn += (size_t)snprintf(dst + dn, dst_size - dn, "state.%s",
                                       m->state_fields[sfi].name);
                p = q;
                continue;
            }
            xi = extern_index(ident, il);
            if(xi >= 0 && *skip_ws(q) == '(') {
                char raw[K2JS_TEXT_MAX];
                char args[K2JS_TEXT_MAX];

                q = consume_group(skip_ws(q) + 1, raw, sizeof(raw));
                tx_args(m, raw, args, sizeof(args));
                dn += (size_t)snprintf(dst + dn, dst_size - dn,
                                       "kryon.hostCall(host || moduleHost, \"%s\", [%s])",
                                       g_externs[xi].method, args);
                p = q;
                continue;
            }
            fni = module_fn_index(m, ident, il);
            if(fni >= 0 && *skip_ws(q) == '(') {
                char raw[K2JS_TEXT_MAX];
                char args[K2JS_TEXT_MAX];
                char fname[K2JS_NAME_MAX];

                camel(m->functions[fni].name, fname, sizeof(fname));
                q = consume_group(skip_ws(q) + 1, raw, sizeof(raw));
                tx_args(m, raw, args, sizeof(args));
                dn += (size_t)snprintf(dst + dn, dst_size - dn,
                                       "%s_%s(rt, state, host%s%s)",
                                       g_guard, fname, args[0] ? ", " : "",
                                       args);
                p = q;
                continue;
            }
            if(*skip_ws(q) == '(') {
                int gfi = global_fn_index(ident, il);

                if(gfi >= 0 && strcmp(g_functions[gfi].guard, g_guard) != 0) {
                    char raw[K2JS_TEXT_MAX];
                    char args[K2JS_TEXT_MAX];

                    q = consume_group(skip_ws(q) + 1, raw, sizeof(raw));
                    tx_args(m, raw, args, sizeof(args));
                    dn += (size_t)snprintf(dst + dn, dst_size - dn,
                                           "%s(rt, %s, host%s%s)",
                                           g_functions[gfi].js,
                                           g_functions[gfi].state_count
                                               ? "kryon.stateForModule(\"cross\")"
                                               : "state",
                                           args[0] ? ", " : "", args);
                    p = q;
                    continue;
                }
                if(isupper((unsigned char)ident[0])) {
                    dn += (size_t)snprintf(dst + dn, dst_size - dn,
                                           "kryon.%s", ident);
                    p = q;
                    continue;
                }
            }
            if(isupper((unsigned char)ident[0]))
                dn += (size_t)snprintf(dst + dn, dst_size - dn, "kryon.%s",
                                       ident);
            else
                dn += (size_t)snprintf(dst + dn, dst_size - dn, "%s", ident);
            p = q;
            continue;
        }
        dst[dn++] = *p++;
    }
    dst[dn] = '\0';
}

static int
validate_asserts(const KirModule *m)
{
    for(int i = 0; i < m->assert_count; i++) {
        const KirAssert *a = &m->asserts[i];

        if(a->guard[0] != '\0') {
            fprintf(stderr,
                    "k2js: %s:%d: guarded #assert is not supported by the JS backend: %s\n",
                    a->span.path, a->span.line, a->message);
            return 0;
        }
        if(!a->known) {
            fprintf(stderr,
                    "k2js: %s:%d: unresolved #assert is not supported by the JS backend: %s\n",
                    a->span.path, a->span.line, a->condition);
            return 0;
        }
        if(!a->value) {
            fprintf(stderr, "k2js: %s:%d: #assert failed: %s\n",
                    a->span.path, a->span.line, a->message);
            return 0;
        }
    }
    return 1;
}

static void
emit_indent(FILE *f, int n)
{
    for(int i = 0; i < n; i++)
        fputc(' ', f), fputc(' ', f);
}

static void
emit_statement_record(FILE *f, int indent, const char *raw)
{
    emit_indent(f, indent);
    fprintf(f, "kryon.statement(rt, ");
    js_string(f, raw);
    fprintf(f, ");\n");
}

static void
emit_assign(FILE *f, const KirModule *m, const char *raw, int indent)
{
    static const char *ops[] = {"+=", "-=", "*=", "/=", "%=", "="};
    const char *op = NULL;
    const char *pos = NULL;

    for(size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        pos = strstr(raw, ops[i]);
        if(pos != NULL) {
            op = ops[i];
            break;
        }
    }
    if(op == NULL) {
        emit_statement_record(f, indent, raw);
        return;
    }
    {
        char lhs[K2JS_TEXT_MAX];
        char rhs[K2JS_TEXT_MAX];
        char out_lhs[K2JS_TEXT_MAX];
        char out_rhs[K2JS_TEXT_MAX];
        size_t ll = (size_t)(pos - raw);

        if(ll >= sizeof(lhs))
            ll = sizeof(lhs) - 1;
        memcpy(lhs, raw, ll);
        lhs[ll] = '\0';
        snprintf(rhs, sizeof(rhs), "%s", pos + strlen(op));
        trim(lhs);
        trim(rhs);
        tx_expr(m, lhs, out_lhs, sizeof(out_lhs));
        tx_expr(m, rhs, out_rhs, sizeof(out_rhs));
        emit_indent(f, indent);
        fprintf(f, "%s %s %s;\n", out_lhs, op, out_rhs);
    }
}

static void
emit_decl(FILE *f, const KirModule *m, const char *raw, int indent)
{
    const char *colon = strchr(raw, ':');
    const char *eq = colon != NULL ? strchr(colon, '=') : NULL;

    if(colon == NULL) {
        emit_statement_record(f, indent, raw);
        return;
    }
    {
        char name[K2JS_NAME_MAX];
        char safe[K2JS_NAME_MAX];
        char rhs[K2JS_TEXT_MAX];
        char val[K2JS_TEXT_MAX];
        size_t nl = (size_t)(colon - raw);

        while(nl > 0 && isspace((unsigned char)raw[nl - 1]))
            nl--;
        if(nl >= sizeof(name))
            nl = sizeof(name) - 1;
        memcpy(name, raw, nl);
        name[nl] = '\0';
        trim(name);
        js_ident(name, safe, sizeof(safe));
        if(eq != NULL) {
            snprintf(rhs, sizeof(rhs), "%s", eq + 1);
            trim(rhs);
            tx_expr(m, rhs, val, sizeof(val));
        } else {
            snprintf(val, sizeof(val), "null");
        }
        emit_indent(f, indent);
        fprintf(f, "let %s = %s;\n", safe, val);
    }
}

static void
lower_function(FILE *f, const KirModule *m, const KirFunction *fn,
               const char *guard)
{
    char fname[K2JS_NAME_MAX];
    char parts[K2JS_PARAM_MAX][K2JS_TEXT_MAX];
    int n;

    camel(fn->name, fname, sizeof(fname));
    fprintf(f, "export function %s_%s(rt, state = moduleState, host = moduleHost",
            guard, fname);
    n = split_top(fn->args, parts, K2JS_PARAM_MAX);
    for(int i = 0; i < n; i++) {
        char *colon = strchr(parts[i], ':');
        char name[K2JS_NAME_MAX], safe[K2JS_NAME_MAX];
        size_t nl;

        if(colon == NULL)
            continue;
        nl = (size_t)(colon - parts[i]);
        while(nl > 0 && isspace((unsigned char)parts[i][nl - 1]))
            nl--;
        if(nl >= sizeof(name))
            nl = sizeof(name) - 1;
        memcpy(name, parts[i], nl);
        name[nl] = '\0';
        trim(name);
        js_ident(name, safe, sizeof(safe));
        fprintf(f, ", %s", safe);
    }
    fprintf(f, ") {\n");
    fprintf(f, "  rt = rt || kryon.createRuntime();\n");
    fprintf(f, "  state = state || moduleState;\n");
    for(int j = 0; j < fn->stmt_count; j++) {
        const KirStmt *st = &fn->stmts[j];
        char raw[K2JS_TEXT_MAX];

        snprintf(raw, sizeof(raw), "%s", st->text);
        trim(raw);
        if(raw[0] == '\0')
            continue;
        switch(st->kind) {
        case KIR_STMT_WIDGET:
            if(strcmp(st->widget, "End") == 0)
                break;
            emit_indent(f, 1);
            fprintf(f, "kryon.widget(rt, ");
            js_string(f, st->widget[0] ? st->widget : raw);
            fprintf(f, ", ");
            js_string(f, st->args[0] ? st->args : raw);
            fprintf(f, ");\n");
            break;
        case KIR_STMT_DECL:
            emit_decl(f, m, raw, 1);
            break;
        case KIR_STMT_ASSIGN:
            emit_assign(f, m, raw, 1);
            break;
        case KIR_STMT_RETURN: {
            char *expr = raw;
            char out[K2JS_TEXT_MAX];

            if(strncmp(expr, "return", 6) == 0)
                expr += 6;
            trim(expr);
            emit_indent(f, 1);
            if(expr[0] == '\0') {
                fprintf(f, "return kryon.snapshot(rt);\n");
            } else {
                tx_expr(m, expr, out, sizeof(out));
                fprintf(f, "return %s;\n", out);
            }
            break;
        }
        case KIR_STMT_EXPR:
            if(strncmp(raw, "BeginUI", 7) == 0 ||
               strncmp(raw, "EndUI", 5) == 0)
                break;
            emit_statement_record(f, 1, raw);
            break;
        case KIR_STMT_BLOCK_OPEN:
        case KIR_STMT_BLOCK_CLOSE:
        case KIR_STMT_IF:
        case KIR_STMT_WHILE:
        case KIR_STMT_FOR:
        case KIR_STMT_SWITCH:
        case KIR_STMT_CASE:
        case KIR_STMT_LABEL:
        case KIR_STMT_GOTO:
        case KIR_STMT_BREAK:
        case KIR_STMT_CONTINUE:
        case KIR_STMT_DEFER:
        case KIR_STMT_UNUSED:
        case KIR_STMT_RAW:
        default:
            emit_statement_record(f, 1, raw);
            break;
        }
    }
    fprintf(f, "  return kryon.snapshot(rt);\n");
    fprintf(f, "}\n\n");
}

static void
runtime_import_for_stem(const char *stem, const char *override,
                        char *dst, size_t dst_size)
{
    int depth = 0;

    if(override != NULL && override[0] != '\0') {
        snprintf(dst, dst_size, "%s", override);
        return;
    }
    for(const char *p = stem; *p != '\0'; p++)
        if(*p == '/')
            depth++;
    dst[0] = '\0';
    for(int i = 0; i < depth; i++)
        strncat(dst, "../", dst_size - strlen(dst) - 1);
    strncat(dst, "kryon-runtime.js", dst_size - strlen(dst) - 1);
}

static void
emit_state(FILE *f, const KirModule *m)
{
    fprintf(f, "export function createState() {\n");
    fprintf(f, "  return {\n");
    for(int i = 0; i < m->state_count; i++) {
        char init[K2JS_TEXT_MAX];

        tx_expr(m, m->state_fields[i].init[0] ? m->state_fields[i].init : "0",
                init, sizeof(init));
        fprintf(f, "    %s: %s%s\n", m->state_fields[i].name, init,
                i + 1 < m->state_count ? "," : "");
    }
    fprintf(f, "  };\n");
    fprintf(f, "}\n\n");
    fprintf(f, "export const moduleState = createState();\n");
    fprintf(f, "let moduleHost = null;\n");
    fprintf(f, "export function setHost(host) { moduleHost = host; }\n\n");
}

static void
emit_app(FILE *f, const KirModule *m)
{
    const KirAppMeta *a = &m->app;

    fprintf(f, "export const app = {\n");
    fprintf(f, "  title: ");
    js_string(f, a->has_app ? a->title : m->name);
    fprintf(f, ",\n");
    fprintf(f, "  width: %d,\n", a->width > 0 ? a->width : 800);
    fprintf(f, "  height: %d,\n", a->height > 0 ? a->height : 600);
    fprintf(f, "  fps: %d,\n", a->fps > 0 ? a->fps : 60);
    fprintf(f, "  frame: ");
    js_string(f, a->frame[0] ? a->frame : "");
    fprintf(f, "\n};\n\n");
}

static const KirFunction *
pick_frame_function(const KirModule *m)
{
    if(m->app.frame[0] != '\0') {
        for(int i = 0; i < m->function_count; i++)
            if(strcmp(m->functions[i].name, m->app.frame) == 0)
                return &m->functions[i];
    }
    for(int i = 0; i < m->function_count; i++)
        if(!m->functions[i].is_extern && strcmp(m->functions[i].name, "App") == 0)
            return &m->functions[i];
    for(int i = 0; i < m->function_count; i++)
        if(!m->functions[i].is_extern && m->functions[i].is_ui)
            return &m->functions[i];
    for(int i = 0; i < m->function_count; i++)
        if(!m->functions[i].is_extern)
            return &m->functions[i];
    return NULL;
}

int
k2js_lower(const KirProgram *const *progs, int prog_count,
           const char *root, const char *out_dir,
           const char *runtime_import, int no_main)
{
    char path[1024];

    (void)root;
    build_global_functions(progs, prog_count);
    for(int pi = 0; pi < prog_count; pi++) {
        const KirProgram *prog = progs[pi];

        for(int mi = 0; mi < prog->module_count; mi++) {
            const KirModule *m = &prog->modules[mi];
            char stem[KIR_PATH_MAX];
            char base[KIR_PATH_MAX];
            char guard[K2JS_NAME_MAX];
            char runtime_path[KIR_PATH_MAX];
            const KirFunction *frame_fn;
            FILE *f;

            stem_from_source(m->source_path, stem, sizeof(stem));
            base_from_source(m->source_path, base, sizeof(base));
            camel(base, guard, sizeof(guard));
            if(!validate_asserts(m))
                return 1;
            set_module(m, guard);
            runtime_import_for_stem(stem, runtime_import, runtime_path,
                                    sizeof(runtime_path));
            snprintf(path, sizeof(path), "%s/%s.js", out_dir, stem);
            mkdir_parent(path);
            f = fopen(path, "wb");
            if(f == NULL) {
                fprintf(stderr, "k2js: cannot write %s\n", path);
                return 1;
            }
            fprintf(f, "// Code generated by k2js from %s. DO NOT EDIT.\n",
                    m->source_path);
            fprintf(f, "import * as kryon from ");
            js_string(f, runtime_path);
            fprintf(f, ";\n\n");
            emit_app(f, m);
            emit_state(f, m);
            for(int i = 0; i < m->import_count; i++) {
                const KirImport *imp = &m->imports[i];

                if(imp->kind == KIR_IMPORT_HEADER)
                    fprintf(f, "// #import %s\n", imp->target);
            }
            if(m->import_count > 0)
                fprintf(f, "\n");
            for(int i = 0; i < g_extern_count; i++) {
                fprintf(f, "export function %s(...args) {\n",
                        g_externs[i].js);
                fprintf(f, "  return kryon.hostCall(moduleHost, \"%s\", args);\n",
                        g_externs[i].method);
                fprintf(f, "}\n\n");
            }
            for(int i = 0; i < m->function_count; i++) {
                if(!m->functions[i].is_extern)
                    lower_function(f, m, &m->functions[i], guard);
            }
            frame_fn = pick_frame_function(m);
            fprintf(f, "export function frame(rt = kryon.createRuntime(), state = moduleState, host = moduleHost) {\n");
            if(frame_fn != NULL) {
                char fname[K2JS_NAME_MAX];

                camel(frame_fn->name, fname, sizeof(fname));
                fprintf(f, "  kryon.beginFrame(rt);\n");
                fprintf(f, "  const result = %s_%s(rt, state, host);\n",
                        guard, fname);
                fprintf(f, "  kryon.endFrame(rt);\n");
                fprintf(f, "  return result || kryon.snapshot(rt);\n");
            } else {
                fprintf(f, "  return kryon.snapshot(rt);\n");
            }
            fprintf(f, "}\n\n");
            if(!no_main) {
                fprintf(f, "export function main(target, host = moduleHost) {\n");
                fprintf(f, "  if (host) setHost(host);\n");
                fprintf(f, "  const rt = kryon.createRuntime({ target, app });\n");
                fprintf(f, "  frame(rt, moduleState, host || moduleHost);\n");
                fprintf(f, "  kryon.mount(rt, target);\n");
                fprintf(f, "  return rt;\n");
                fprintf(f, "}\n\n");
            }
            fprintf(f, "export default { app, createState, moduleState, setHost, frame%s };\n",
                    no_main ? "" : ", main");
            fclose(f);
        }
    }
    return 0;
}
