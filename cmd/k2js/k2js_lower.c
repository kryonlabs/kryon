/*
 * k2js_lower.c - Kir -> browser-loadable JavaScript backend.
 */
#include "k2js_lower.h"
#include "kir_text.h"
#include "kir_emit.h"
#include "kir_check.h"
#include "kir_expr.h"

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
    const KirModule *module;
    int used;
} K2jsGlobalFunction;

static K2jsGlobalFunction g_functions[512];
static int g_function_count;
static const KirModule *g_mod;
static char g_guard[K2JS_NAME_MAX];
static K2jsExtern g_externs[K2JS_EXTERN_MAX];
static int g_extern_count;
static const KirModule *g_enum_imports[128];
static int g_enum_import_count;

static int
enum_import_index(const KirModule *module)
{
    for(int i = 0; i < g_enum_import_count; i++) {
        if(g_enum_imports[i] == module)
            return i;
    }
    if(g_enum_import_count >= 128) {
        fprintf(stderr, "k2js: too many enum imports\n");
        exit(1);
    }
    g_enum_imports[g_enum_import_count] = module;
    return g_enum_import_count++;
}

static void
mkdir_parent(const char *path)
{
    char tmp[KIR_PATH_MAX * 2];
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

static void
js_ident(const char *s, char *dst, size_t dst_size)
{
    size_t n = 0;

    if(dst_size == 0)
        return;
    for(const char *p = s; *p != '\0' && n + 1 < dst_size; p++) {
        if((n == 0 && (isalpha((unsigned char)*p) || *p == '_')) ||
           (n > 0 && kir_is_ident_char((unsigned char)*p))) {
            dst[n++] = *p;
        } else if(n > 0) {
            dst[n++] = '_';
        }
    }
    if(n == 0)
        dst[n++] = '_';
    dst[n] = '\0';
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

static int
next_enum_member(const char **cursor, char *name, size_t name_size,
                 char *value, size_t value_size)
{
    const char *p = *cursor;
    size_t length = 0;

    while(isspace((unsigned char)*p) || *p == ',')
        p++;
    if(*p == '\0')
        return 0;
    while(kir_is_ident_char((unsigned char)*p)) {
        if(length + 1 < name_size)
            name[length++] = *p;
        p++;
    }
    name[length] = '\0';
    while(*p == ' ' || *p == '\t' || *p == '\r')
        p++;
    length = 0;
    if(*p == '=') {
        p++;
        while(*p != '\0' && *p != '\n' && *p != ',') {
            if(length + 1 < value_size)
                value[length++] = *p;
            p++;
        }
    }
    value[length] = '\0';
    kir_trim_in_place(value);
    while(*p != '\0' && *p != '\n' && *p != ',')
        p++;
    *cursor = p;
    return name[0] != '\0';
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
    char ident[KIR_NAME_MAX];
    const KirModule *owner = NULL;
    const KirFunction *function = NULL;
    if(len >= sizeof(ident))
        return -1;
    memcpy(ident, name, len);
    ident[len] = '\0';
    if(KirResolveFunction(g_mod, ident, &owner, &function) != 1)
        return -1;
    for(int i = 0; i < g_function_count; i++) {
        if(g_functions[i].module == owner && strcmp(g_functions[i].kry, ident) == 0)
            return i;
    }
    return -1;
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
        kir_camel_ident(fallback, dst, dst_size);
        return;
    }
    if(strncmp(target, "js.", 3) == 0)
        base = target + 3;
    else {
        const char *dot = strrchr(target, '.');

        base = dot != NULL ? dot + 1 : target;
    }
    kir_camel_ident(base[0] != '\0' ? base : fallback, dst, dst_size);
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
    g_enum_import_count = 0;
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
            kir_camel_ident(base, guard, sizeof(guard));
            for(int fi = 0; fi < m->function_count; fi++) {
                const KirFunction *fn = &m->functions[fi];
                char fname[K2JS_NAME_MAX];

                if(fn->is_extern || g_function_count >= 512)
                    continue;
                kir_camel_ident(fn->name, fname, sizeof(fname));
                snprintf(g_functions[g_function_count].kry,
                         sizeof(g_functions[0].kry), "%s", fn->name);
                snprintf(g_functions[g_function_count].guard,
                         sizeof(g_functions[0].guard), "%s", guard);
                snprintf(g_functions[g_function_count].js,
                         sizeof(g_functions[0].js), "%s_%s", guard, fname);
                g_functions[g_function_count].module = m;
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
    int n = kir_split_top(raw, parts[0], K2JS_PARAM_MAX, sizeof(parts[0]));
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
    if(*kir_skip_ws(src) == '(') {
        KirFunction parsed = {0};
        int root = KirParseExpr(&parsed, m, src, (KirSourceSpan){0});
        if(root >= 0 && parsed.exprs[root].kind == KIR_EXPR_CAST) {
            const KirExpr *cast = &parsed.exprs[root];
            const KirExpr *operand = &parsed.exprs[cast->right];
            if(*KirScalarType(cast->name) &&
               KirScalarLiteral(cast->name, operand->text, KIR_JS,
                                cast->span, dst, dst_size)) {
                free(parsed.exprs);
                return;
            }
            const KirModule *owner = NULL;
            const KirType *type = KirFindType(m, cast->name, &owner);
            if(type != NULL && type->is_enum) {
                char operand[K2JS_TEXT_MAX];
                tx_expr(m, parsed.exprs[cast->right].text, operand, sizeof(operand));
                snprintf(dst, dst_size, "Math.trunc(Number(%s))", operand);
                free(parsed.exprs);
                return;
            }
        }
        free(parsed.exprs);
    }
    if(contains_top_level_compound(src) || strstr(src, "sizeof") != NULL) {
        char q[K2JS_TEXT_MAX];

        js_string_buf(src, q, sizeof(q));
        snprintf(dst, dst_size, "kryon.expr(");
        strncat(dst, q, dst_size - strlen(dst) - 1);
        strncat(dst, ")", dst_size - strlen(dst) - 1);
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
        if(isdigit((unsigned char)*p) ||
           (*p == '.' && isdigit((unsigned char)p[1]))) {
            const char *q = p;

            while(isdigit((unsigned char)*q) || *q == '.')
                q++;
            if(*q == 'e' || *q == 'E') {
                q++;
                if(*q == '+' || *q == '-')
                    q++;
                while(isdigit((unsigned char)*q))
                    q++;
            }
            while(p < q && dn + 1 < dst_size)
                dst[dn++] = *p++;
            if((*q == 'f' || *q == 'F') &&
               !kir_is_ident_char((unsigned char)q[1]))
                p = q + 1;
            continue;
        }
        if(*p == '&') {
            const char *q = p + 1;
            char ident[K2JS_NAME_MAX];
            size_t il = 0;

            while(kir_is_ident_char((unsigned char)*q) && il + 1 < sizeof(ident))
                ident[il++] = *q++;
            ident[il] = '\0';
            if(il > 0 && state_field_index(m, ident, il) >= 0) {
                char qs[K2JS_TEXT_MAX];
                const char *index = kir_skip_ws(q);
                if(*index == '[') {
                    char raw_index[K2JS_TEXT_MAX];
                    char evaluated_index[K2JS_TEXT_MAX];
                    const char *end = consume_group(index + 1, raw_index, sizeof(raw_index));
                    tx_expr(m, raw_index, evaluated_index, sizeof(evaluated_index));
                    dn += (size_t)snprintf(dst + dn, dst_size - dn,
                                           "kryon.ref($state.%s, %s)", ident, evaluated_index);
                    p = end;
                    continue;
                }
                js_string_buf(ident, qs, sizeof(qs));
                dn += (size_t)snprintf(dst + dn, dst_size - dn,
                                       "kryon.ref($state, %s)", qs);
                p = q;
                continue;
            }
            if(il > 0) {
                dn += (size_t)snprintf(dst + dn, dst_size - dn, "%s", ident);
                p = q;
                continue;
            }
        }
        if(isalpha((unsigned char)*p) || *p == '_') {
            const char *q = p;
            char ident[K2JS_NAME_MAX];
            size_t il = 0;
            int sfi, fni, xi;

            while(kir_is_ident_char((unsigned char)*q) && il + 1 < sizeof(ident))
                ident[il++] = *q++;
            ident[il] = '\0';
            if((strcmp(ident, "NULL") == 0 || strcmp(ident, "nil") == 0) &&
               !kir_is_ident_char((unsigned char)*q)) {
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
                dn += (size_t)snprintf(dst + dn, dst_size - dn, "$state.%s",
                                       m->state_fields[sfi].name);
                p = q;
                continue;
            }
            xi = extern_index(ident, il);
            if(xi >= 0 && *kir_skip_ws(q) == '(') {
                char raw[K2JS_TEXT_MAX];
                char args[K2JS_TEXT_MAX];

                q = consume_group(kir_skip_ws(q) + 1, raw, sizeof(raw));
                tx_args(m, raw, args, sizeof(args));
                dn += (size_t)snprintf(dst + dn, dst_size - dn,
                                       "kryon.hostCall($host || moduleHost, \"%s\", [%s])",
                                       g_externs[xi].method, args);
                p = q;
                continue;
            }
            fni = module_fn_index(m, ident, il);
            if(fni >= 0 && *kir_skip_ws(q) == '(') {
                char raw[K2JS_TEXT_MAX];
                char args[K2JS_TEXT_MAX];
                char fname[K2JS_NAME_MAX];

                kir_camel_ident(m->functions[fni].name, fname, sizeof(fname));
                q = consume_group(kir_skip_ws(q) + 1, raw, sizeof(raw));
                tx_args(m, raw, args, sizeof(args));
                dn += (size_t)snprintf(dst + dn, dst_size - dn,
                                       "%s_%s($rt, $state, $host%s%s)",
                                       g_guard, fname, args[0] ? ", " : "",
                                       args);
                p = q;
                continue;
            }
            if(*kir_skip_ws(q) == '(') {
                int gfi = global_fn_index(ident, il);

                if(gfi >= 0 && strcmp(g_functions[gfi].guard, g_guard) != 0) {
                    char raw[K2JS_TEXT_MAX];
                    char args[K2JS_TEXT_MAX];

                    g_functions[gfi].used = 1;
                    q = consume_group(kir_skip_ws(q) + 1, raw, sizeof(raw));
                    tx_args(m, raw, args, sizeof(args));
                    dn += (size_t)snprintf(dst + dn, dst_size - dn,
                                           "%s($rt, undefined, $host%s%s)",
                                           g_functions[gfi].js,
                                           args[0] ? ", " : "", args);
                    p = q;
                    continue;
                }
                if(isupper((unsigned char)ident[0])) {
                    char raw[K2JS_TEXT_MAX];
                    char args[K2JS_TEXT_MAX];

                    q = consume_group(kir_skip_ws(q) + 1, raw, sizeof(raw));
                    tx_args(m, raw, args, sizeof(args));
                    dn += (size_t)snprintf(dst + dn, dst_size - dn,
                                           "kryon.%s(%s)", ident, args);
                    p = q;
                    continue;
                }
            }
            const KirModule *enum_owner = NULL;
            const KirType *enum_type = NULL;
            int enum_found = KirResolveEnumMember(m, ident, &enum_owner, &enum_type);
            if(enum_found == 1 && enum_owner != m)
                dn += (size_t)snprintf(dst + dn, dst_size - dn, "$enum%d.%s",
                                       enum_import_index(enum_owner), ident);
            else if(enum_found == 1)
                dn += (size_t)snprintf(dst + dn, dst_size - dn, "%s", ident);
            else if(isupper((unsigned char)ident[0]))
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
    fprintf(f, "kryon.statement($rt, ");
    js_string(f, raw);
    fprintf(f, ");\n");
}

static void emit_initializer_value(FILE *f, const KirModule *m, const char *source);

static int
is_positional_record(const char *type)
{
    return strcmp(type, "Vector2") == 0 || strcmp(type, "Vector3") == 0 ||
           strcmp(type, "Vector4") == 0 || strcmp(type, "Rectangle") == 0 ||
           strcmp(type, "Color") == 0;
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
        size_t ll = (size_t)(pos - raw);

        if(ll >= sizeof(lhs))
            ll = sizeof(lhs) - 1;
        memcpy(lhs, raw, ll);
        lhs[ll] = '\0';
        snprintf(rhs, sizeof(rhs), "%s", pos + strlen(op));
        kir_trim_in_place(lhs);
        kir_trim_in_place(rhs);
        tx_expr(m, lhs, out_lhs, sizeof(out_lhs));
        emit_indent(f, indent);
        fprintf(f, "%s %s ", out_lhs, op);
        if(strcmp(op, "=") == 0)
            fputs("kryon.copyValue(", f);
        emit_initializer_value(f, m, rhs);
        if(strcmp(op, "=") == 0)
            fputc(')', f);
        fputs(";\n", f);
    }
}

static int
split_direct_call(const char *src, char *name, size_t name_size,
                  char *args, size_t args_size)
{
    const char *p = kir_skip_ws(src);
    const char *q = p;
    size_t nl;
    char raw[K2JS_TEXT_MAX];

    if(!(isalpha((unsigned char)*q) || *q == '_'))
        return 0;
    while(kir_is_ident_char((unsigned char)*q))
        q++;
    nl = (size_t)(q - p);
    if(nl == 0 || nl >= name_size)
        return 0;
    q = kir_skip_ws(q);
    if(*q != '(')
        return 0;
    q = consume_group(q + 1, raw, sizeof(raw));
    if(*kir_skip_ws(q) != '\0')
        return 0;
    memcpy(name, p, nl);
    name[nl] = '\0';
    snprintf(args, args_size, "%s", raw);
    return 1;
}

static int
condition_is_widget_call(const char *cond, char *widget, size_t widget_size,
                         char *args, size_t args_size)
{
    static const char *const widgets[] = {
        "Button", "IconButton", "Checkbox", "Dropdown", "ListBox",
        "Radio", "Slider", "TableView", "TextField", "Toggle",
        "BeginTabBar", "BeginTabItem"
    };
    char name[K2JS_NAME_MAX];

    if(!split_direct_call(cond, name, sizeof(name), args, args_size))
        return 0;
    for(size_t i = 0; i < sizeof(widgets) / sizeof(widgets[0]); i++) {
        if(strcmp(name, widgets[i]) == 0) {
            snprintf(widget, widget_size, "%s", name);
            return 1;
        }
    }
    return 0;
}

/* Evaluate initializer leaves in lexical scope, rather than sending source
 * text to a runtime parser that cannot see widget parameters or local values. */
static void emit_widget_arguments(FILE *f, const KirModule *m,
                                  const char *widget, const char *args);
static void emit_zero_value(FILE *f, const KirModule *m, const char *type);

static void
emit_initializer_value(FILE *f, const KirModule *m, const char *source)
{
    const char *value = kir_skip_ws(source);
    char widget[K2JS_NAME_MAX];
    char arguments[K2JS_TEXT_MAX];
    const KirModule *owner = NULL;
    const KirFunction *declaration = NULL;
    if(condition_is_widget_call(value, widget, sizeof(widget),
                                arguments, sizeof(arguments)) &&
       KirResolveFunction(m, widget, &owner, &declaration) == 0) {
        /* A direct action call must execute the same runtime path whether
         * its result is tested, stored in a local, assigned, or returned. */
        fputs("kryon.widget($rt, ", f);
        js_string(f, widget);
        fputs(", ", f);
        emit_widget_arguments(f, m, widget, arguments);
        fputs(", $state)", f);
        return;
    }
    if(*value == '(') {
        const char *close = strchr(value, ')');
        if(close != NULL && *kir_skip_ws(close + 1) == '{') {
            char type[KIR_NAME_MAX];
            size_t length = (size_t)(close - value - 1);
            if(length < sizeof(type)) {
                memcpy(type, value + 1, length);
                type[length] = '\0';
                kir_trim_in_place(type);
                const KirType *record = KirFindType(m, type, NULL);
                if(record != NULL && !record->is_enum) {
                    KirFunction parsed = {0};
                    int root = KirParseExpr(&parsed, m, value, (KirSourceSpan){0});
                    if(root < 0 || parsed.exprs[root].kind != KIR_EXPR_COMPOUND) {
                        fprintf(stderr, "k2js: malformed record initializer: %s\n", type);
                        free(parsed.exprs);
                        exit(1);
                    }
                    fputs("(() => { const $record = ", f);
                    emit_zero_value(f, m, type);
                    fputs("; ", f);
                    int ordinal = 0;
                    for(int child = parsed.exprs[root].first_child; child >= 0;
                        child = parsed.exprs[child].next_sibling) {
                        const KirExpr *entry = &parsed.exprs[child];
                        KirTypeField field;
                        size_t offset = 0;
                        int position = 0;
                        int found = 0;
                        while(KirTypeNextField(record, &offset, &field) == 1) {
                            if(entry->name[0] ? !strcmp(entry->name, field.name) : position == ordinal) {
                                found = 1;
                                break;
                            }
                            position++;
                        }
                        if(!found) {
                            fprintf(stderr, "k2js: unknown record initializer field: %s\n", entry->name);
                            free(parsed.exprs);
                            exit(1);
                        }
                        fputs("$record[", f);
                        js_string(f, field.name);
                        fputs("] = kryon.copyValue(", f);
                        emit_initializer_value(f, m, parsed.exprs[entry->right].text);
                        fputs("); ", f);
                        ordinal++;
                    }
                    fputs("return $record; })()", f);
                    free(parsed.exprs);
                    return;
                }
                if(is_positional_record(type)) {
                    fputs("kryon.recordValue(", f);
                    js_string(f, type);
                    fputs(", ", f);
                    emit_initializer_value(f, m, close + 1);
                    fputc(')', f);
                    return;
                }
            }
            value = kir_skip_ws(close + 1);
        }
    }
    size_t length = strlen(value);
    if(*value != '{' || length < 2 || value[length - 1] != '}') {
        KirFunction parsed = {0};
        int root = KirParseExpr(&parsed, m, value, (KirSourceSpan){0});
        if(root >= 0 && parsed.exprs[root].kind == KIR_EXPR_CAST) {
            const KirExpr *cast = &parsed.exprs[root];
            const char *target = KirTargetType(cast->name, KIR_JS);
            if(target != NULL && (strcmp(cast->name, "float") == 0 ||
               strcmp(cast->name, "f32") == 0 || strcmp(cast->name, "double") == 0 ||
               strcmp(cast->name, "f64") == 0)) {
                int narrow = strcmp(cast->name, "float") == 0 || strcmp(cast->name, "f32") == 0;
                fputs(narrow ? "Math.fround(" : "Number(", f);
                emit_initializer_value(f, m, parsed.exprs[cast->right].text);
                fputc(')', f);
                free(parsed.exprs);
                return;
            }
        }
        free(parsed.exprs);
        char expression[K2JS_TEXT_MAX];
        tx_expr(m, value, expression, sizeof(expression));
        fputs(expression, f);
        return;
    }
    char body[K2JS_TEXT_MAX];
    kir_copy(body, sizeof(body), value + 1);
    body[strlen(body) - 1] = '\0';
    char (*parts)[K2JS_TEXT_MAX] = calloc(64, sizeof(*parts));
    if(parts == NULL) {
        fprintf(stderr, "k2js: cannot allocate initializer fields\n");
        exit(1);
    }
    int count = kir_split_top(body, parts[0], 64, sizeof(parts[0]));
    if(count >= 64) {
        fprintf(stderr, "k2js: initializer has too many fields\n");
        free(parts);
        exit(1);
    }
    int named = *kir_skip_ws(body) == '.' || *kir_skip_ws(body) == '\0';
    fputc(named ? '{' : '[', f);
    int emitted = 0;
    for(int i = 0; i < count; i++) {
        char *part = parts[i];
        kir_trim_in_place(part);
        if(*part == '\0')
            continue;
        if(emitted++)
            fputs(", ", f);
        if(named) {
            char *equals = strchr(part, '=');
            if(*part != '.' || equals == NULL) {
                fprintf(stderr, "k2js: expected a named initializer field\n");
                free(parts);
                exit(1);
            }
            *equals = '\0';
            kir_trim_in_place(part);
            js_string(f, part + 1);
            fputs(": ", f);
            emit_initializer_value(f, m, equals + 1);
        } else {
            emit_initializer_value(f, m, part);
        }
    }
    fputc(named ? '}' : ']', f);
    free(parts);
}

static void
emit_zero_value_nested(FILE *f, const KirModule *m, const char *type, int depth)
{
    if(depth >= 64) {
        fprintf(stderr, "k2js: recursive or excessively nested zero value: %s\n", type);
        exit(1);
    }
    type = kir_skip_ws(type);
    if(*type == '[') {
        char size_source[K2JS_TEXT_MAX];
        char size_expression[K2JS_TEXT_MAX];
        const char *element_type = consume_group(type + 1, size_source, sizeof(size_source));
        tx_expr(m, size_source, size_expression, sizeof(size_expression));
        fprintf(f, "Array.from({length: %s}, () => (", size_expression);
        emit_zero_value_nested(f, m, element_type, depth + 1);
        fputs("))", f);
        return;
    }
    if(KirEmitJsRecordValue(f, m, type, NULL))
        return;
    const KirType *record = KirFindType(m, type, NULL);
    if(record != NULL && !record->is_enum) {
        KirTypeField field;
        size_t offset = 0;
        int emitted = 0;
        fputc('{', f);
        while(KirTypeNextField(record, &offset, &field) == 1) {
            if(emitted++)
                fputs(", ", f);
            js_string(f, field.name);
            fputs(": ", f);
            emit_zero_value_nested(f, m, field.type, depth + 1);
        }
        fputc('}', f);
        return;
    }
    char literal[K2JS_TEXT_MAX];
    const char *value = (!strcmp(type, "string") || !strcmp(type, "const char*") ||
                         !strcmp(type, "char*")) ? "\"\"" :
                        !strcmp(KirScalarType(type), "bool") ? "false" : "0";
    if(KirScalarLiteral(type, value, KIR_JS, (KirSourceSpan){0}, literal, sizeof(literal)))
        fputs(literal, f);
    else
        fputs(value, f);
}

static void
emit_zero_value(FILE *f, const KirModule *m, const char *type)
{
    emit_zero_value_nested(f, m, type, 0);
}

static void
emit_widget_arguments(FILE *f, const KirModule *m, const char *widget, const char *args)
{
    if(strcmp(widget, "Text") == 0 || strcmp(widget, "Button") == 0 ||
       strcmp(widget, "MenuButton") == 0 || strcmp(widget, "SplitButton") == 0)
        emit_initializer_value(f, m, args);
    else
        js_string(f, args);
}

static void
emit_if(FILE *f, const KirModule *m, const char *raw, int indent, int *chained)
{
    char cond[K2JS_TEXT_MAX];
    char out[K2JS_TEXT_MAX];
    char widget[K2JS_NAME_MAX];
    char args[K2JS_TEXT_MAX];

    *chained = 0;
    snprintf(cond, sizeof(cond), "%s", raw);
    kir_strip_block_brace(cond);
    if(strncmp(cond, "guard ", 6) == 0)
        memmove(cond, cond + 6, strlen(cond + 6) + 1);
    if(strncmp(cond, "else if ", 8) == 0) {
        *chained = 1;
        memmove(cond, cond + 8, strlen(cond + 8) + 1);
        kir_trim_in_place(cond);
        emit_indent(f, indent);
        if(condition_is_widget_call(cond, widget, sizeof(widget), args,
                                    sizeof(args))) {
            fprintf(f, "} else if (kryon.widget($rt, ");
            js_string(f, widget);
            fprintf(f, ", ");
            emit_widget_arguments(f, m, widget, args);
            fprintf(f, ", $state)) {\n");
        } else {
            tx_expr(m, cond, out, sizeof(out));
            fprintf(f, "} else if (%s) {\n", out);
        }
        return;
    }
    if(strncmp(cond, "else", 4) == 0 &&
       (cond[4] == '\0' || isspace((unsigned char)cond[4]))) {
        *chained = 1;
        emit_indent(f, indent);
        fprintf(f, "} else {\n");
        return;
    }
    if(strncmp(cond, "if ", 3) == 0)
        memmove(cond, cond + 3, strlen(cond + 3) + 1);
    kir_trim_in_place(cond);
    emit_indent(f, indent);
    if(condition_is_widget_call(cond, widget, sizeof(widget), args,
                                sizeof(args))) {
        fprintf(f, "if (kryon.widget($rt, ");
        js_string(f, widget);
        fprintf(f, ", ");
        emit_widget_arguments(f, m, widget, args);
        fprintf(f, ", $state)) {\n");
    } else {
        tx_expr(m, cond, out, sizeof(out));
        fprintf(f, "if (%s) {\n", out);
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
        size_t nl = (size_t)(colon - raw);

        while(nl > 0 && isspace((unsigned char)raw[nl - 1]))
            nl--;
        if(nl >= sizeof(name))
            nl = sizeof(name) - 1;
        memcpy(name, raw, nl);
        name[nl] = '\0';
        kir_trim_in_place(name);
        js_ident(name, safe, sizeof(safe));
        if(eq == NULL) {
            char type[KIR_NAME_MAX];
            kir_copy(type, sizeof(type), colon + 1);
            kir_trim_in_place(type);
            if(KirFindType(m, type, NULL) != NULL) {
                emit_indent(f, indent);
                fprintf(f, "let %s = ", safe);
                emit_zero_value(f, m, type);
                fputs(";\n", f);
                return;
            }
        }
        if(eq != NULL) {
            snprintf(rhs, sizeof(rhs), "%s", eq + 1);
            kir_trim_in_place(rhs);
        }
        emit_indent(f, indent);
        fprintf(f, "let %s = ", safe);
        if(eq != NULL) {
            char type[KIR_NAME_MAX] = {0};
            size_t length = (size_t)(eq - colon - 1);
            if(length < sizeof(type)) {
                memcpy(type, colon + 1, length);
                type[length] = '\0';
                kir_trim_in_place(type);
            }
            int positional = is_positional_record(type) && *kir_skip_ws(rhs) == '{';
            fputs("kryon.copyValue(", f);
            if(positional) {
                fputs("kryon.recordValue(", f);
                js_string(f, type);
                fputs(", ", f);
            }
            emit_initializer_value(f, m, rhs);
            if(positional)
                fputc(')', f);
            fputc(')', f);
        } else
            fputs("null", f);
        fputs(";\n", f);
    }
}

static void
resolve_body_symbol(void *context, const char *text, char *out, size_t size)
{
    tx_expr(context,text,out,size);
}

static void
lower_function(FILE *f, const KirModule *m, const KirFunction *fn,
               const char *guard)
{
    char fname[K2JS_NAME_MAX];
    char parts[K2JS_PARAM_MAX][K2JS_TEXT_MAX];
    int n;
    int indent = 1;
    int block_stack[128];
    int block_top = 0;

    kir_camel_ident(fn->name, fname, sizeof(fname));
    fprintf(f, "export function %s_%s($rt, $state = moduleState, $host = moduleHost",
            guard, fname);
    n = kir_split_top(fn->args, parts[0], K2JS_PARAM_MAX, sizeof(parts[0]));
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
        kir_trim_in_place(name);
        js_ident(name, safe, sizeof(safe));
        fprintf(f, ", %s", safe);
    }
    fprintf(f, ") {\n");
    fprintf(f, "  $state = $state || moduleState;\n");
    if(KirEmitBody(f,m,fn,KIR_JS,resolve_body_symbol,(void *)m, "", NULL)) {
        fprintf(f,"}\n\n"); return;
    }
    for(int i = 0; i < n; i++) {
        char *colon = strchr(parts[i], ':');
        if(colon == NULL)
            continue;
        *colon++ = '\0';
        kir_trim_in_place(parts[i]);
        kir_trim_in_place(colon);
        if(KirFindType(m, colon, NULL) != NULL) {
            char safe[K2JS_NAME_MAX];
            js_ident(parts[i], safe, sizeof(safe));
            fprintf(f, "  %s = ", safe);
            if(!KirEmitJsRecordValue(f, m, colon, safe))
                fprintf(f, "kryon.copyValue(%s)", safe);
            fputs(";\n", f);
        }
    }
    fprintf(f, "  $rt = $rt || kryon.createRuntime();\n");
    for(int j = 0; j < fn->stmt_count; j++) {
        const KirStmt *st = &fn->stmts[j];
        char raw[K2JS_TEXT_MAX];

        snprintf(raw, sizeof(raw), "%s", st->text);
        kir_trim_in_place(raw);
        if(raw[0] == '\0')
            continue;
        switch(st->kind) {
        case KIR_STMT_BLOCK_OPEN:
            emit_indent(f, indent);
            fprintf(f, "{\n");
            if(block_top < (int)(sizeof(block_stack) / sizeof(block_stack[0])))
                block_stack[block_top++] = 1;
            indent++;
            break;
        case KIR_STMT_BLOCK_CLOSE: {
            int emitted = block_top > 0 ? block_stack[--block_top] : 1;

            if(j + 1 < fn->stmt_count &&
               fn->stmts[j + 1].kind == KIR_STMT_IF &&
               strncmp(fn->stmts[j + 1].text, "else", 4) == 0) {
                if(emitted && --indent < 1)
                    indent = 1;
                break;
            }
            if(!emitted)
                break;
            if(--indent < 1)
                indent = 1;
            emit_indent(f, indent);
            fprintf(f, "}\n");
            break;
        }
        case KIR_STMT_IF: {
            int chained = 0;

            emit_if(f, m, raw, indent, &chained);
            (void)chained;
            if(block_top < (int)(sizeof(block_stack) / sizeof(block_stack[0])))
                block_stack[block_top++] = 1;
            indent++;
            break;
        }
        case KIR_STMT_WIDGET:
            if(strcmp(st->widget, "End") == 0)
                break;
            emit_indent(f, indent);
            fprintf(f, "kryon.widget($rt, ");
            js_string(f, st->widget[0] ? st->widget : raw);
            fprintf(f, ", ");
            emit_widget_arguments(f, m, st->widget, st->args[0] ? st->args : raw);
            fprintf(f, ", $state);\n");
            break;
        case KIR_STMT_DECL:
            emit_decl(f, m, raw, indent);
            break;
        case KIR_STMT_ASSIGN:
            emit_assign(f, m, raw, indent);
            break;
        case KIR_STMT_RETURN: {
            char *expr = raw;

            if(strncmp(expr, "return", 6) == 0)
                expr += 6;
            kir_trim_in_place(expr);
            emit_indent(f, indent);
            if(expr[0] == '\0') {
                fprintf(f, "return kryon.snapshot($rt);\n");
            } else {
                fputs("return ", f);
                emit_initializer_value(f, m, expr);
                fputs(";\n", f);
            }
            break;
        }
        case KIR_STMT_EXPR: {
            char name[K2JS_NAME_MAX], args[K2JS_TEXT_MAX], out[K2JS_TEXT_MAX];
            if(split_direct_call(raw, name, sizeof(name), args, sizeof(args)) &&
               (strcmp(name, "BeginDisabled") == 0 || strcmp(name, "EndDisabled") == 0)) {
                emit_indent(f, indent);
                if(strcmp(name, "BeginDisabled") == 0) {
                    tx_expr(m, args, out, sizeof(out));
                    fprintf(f, "kryon.widget($rt, \"BeginDisabled\", (%s) ? 1 : 0, $state);\n", out);
                } else {
                    fprintf(f, "kryon.widget($rt, \"EndDisabled\", \"\", $state);\n");
                }
                break;
            }
            if(split_direct_call(raw, name, sizeof(name), args, sizeof(args)) &&
               (strcmp(name, "EndTabItem") == 0 || strcmp(name, "EndTabBar") == 0)) {
                emit_indent(f, indent);
                fprintf(f, "kryon.widget($rt, ");
                js_string(f, name);
                fprintf(f, ", \"\", $state);\n");
                break;
            }
            if(strncmp(raw, "BeginTree", 9) == 0 ||
               strncmp(raw, "EndTree", 7) == 0)
                break;
            int known_call = split_direct_call(raw, name, sizeof(name), args, sizeof(args)) &&
                (module_fn_index(m, name, strlen(name)) >= 0 || extern_index(name, strlen(name)) >= 0 ||
                 global_fn_index(name, strlen(name)) >= 0 ||
                 strcmp(name, "SetTheme") == 0 || strcmp(name, "SetThemeFamily") == 0 ||
                 strcmp(name, "SetThemeMode") == 0);
            int increment = st->expr_root >= 0 &&
                (fn->exprs[st->expr_root].kind == KIR_EXPR_POSTFIX ||
                 (fn->exprs[st->expr_root].kind == KIR_EXPR_UNARY &&
                  (!strcmp(fn->exprs[st->expr_root].op, "++") || !strcmp(fn->exprs[st->expr_root].op, "--"))));
            if(known_call || increment) {
                tx_expr(m, raw, out, sizeof(out));
                emit_indent(f, indent);
                fprintf(f, "%s;\n", out);
            } else {
                emit_statement_record(f, indent, raw);
            }
            break;
        }
        case KIR_STMT_WHILE:
        case KIR_STMT_SWITCH: {
            const char *keyword = st->kind == KIR_STMT_WHILE ? "while" : "switch";
            char out[K2JS_TEXT_MAX];
            kir_strip_block_brace(raw);
            tx_expr(m, kir_skip_ws(raw + strlen(keyword)), out, sizeof(out));
            emit_indent(f, indent);
            fprintf(f, "%s (%s) {\n", keyword, out);
            if(block_top < (int)(sizeof(block_stack) / sizeof(block_stack[0])))
                block_stack[block_top++] = 1;
            indent++;
            break;
        }
        case KIR_STMT_CASE: {
            char out[K2JS_TEXT_MAX];
            size_t length = strlen(raw);
            int opens_block = length > 0 && raw[length - 1] == '{';
            if(opens_block)
                kir_strip_block_brace(raw);
            kir_trim_in_place(raw);
            length = strlen(raw);
            if(length > 0 && raw[length - 1] == ':')
                raw[length - 1] = '\0';
            emit_indent(f, indent);
            if(strcmp(raw, "default") == 0) {
                fprintf(f, "default:%s\n", opens_block ? " {" : "");
            } else {
                tx_expr(m, kir_skip_ws(raw + 4), out, sizeof(out));
                fprintf(f, "case %s:%s\n", out, opens_block ? " {" : "");
            }
            if(opens_block) {
                if(block_top < (int)(sizeof(block_stack) / sizeof(block_stack[0])))
                    block_stack[block_top++] = 1;
                indent++;
            }
            break;
        }
        case KIR_STMT_BREAK:
        case KIR_STMT_CONTINUE:
            emit_indent(f, indent);
            fprintf(f, "%s;\n", KirStmtKindName(st->kind));
            break;
        case KIR_STMT_FOR: {
            char clause[K2JS_TEXT_MAX];
            char init[K2JS_TEXT_MAX];
            char condition[K2JS_TEXT_MAX];
            char step[K2JS_TEXT_MAX];
            char init_out[K2JS_TEXT_MAX];
            char condition_out[K2JS_TEXT_MAX];
            char step_out[K2JS_TEXT_MAX];
            char *first;
            char *second;
            const char *body = kir_skip_ws(raw + 3);

            snprintf(clause, sizeof(clause), "%s", body);
            kir_strip_block_brace(clause);
            first = strchr(clause, ';');
            second = first != NULL ? strchr(first + 1, ';') : NULL;
            if(first == NULL || second == NULL) {
                emit_statement_record(f, indent, raw);
                if(raw[strlen(raw) ? strlen(raw) - 1 : 0] == '{' &&
                   block_top < (int)(sizeof(block_stack) /
                                     sizeof(block_stack[0])))
                    block_stack[block_top++] = 0;
                break;
            }
            *first = '\0';
            *second = '\0';
            snprintf(init, sizeof(init), "%s", kir_skip_ws(clause));
            snprintf(condition, sizeof(condition), "%s", kir_skip_ws(first + 1));
            snprintf(step, sizeof(step), "%s", kir_skip_ws(second + 1));
            if(strncmp(init, "int ", 4) == 0)
                snprintf(init_out, sizeof(init_out), "let %s", init + 4);
            else
                tx_expr(m, init, init_out, sizeof(init_out));
            tx_expr(m, condition, condition_out, sizeof(condition_out));
            tx_expr(m, step, step_out, sizeof(step_out));
            emit_indent(f, indent);
            fprintf(f, "for (%s; %s; %s) {\n",
                    init_out, condition_out, step_out);
            if(block_top < (int)(sizeof(block_stack) / sizeof(block_stack[0])))
                block_stack[block_top++] = 1;
            indent++;
            break;
        }
        case KIR_STMT_UNUSED:
            break;
        case KIR_STMT_DEFER:
            fprintf(stderr, "internal error: cleanup was not lowered\n");
            exit(1);
        case KIR_STMT_LABEL:
        case KIR_STMT_GOTO:
        case KIR_STMT_RAW:
        default:
            emit_statement_record(f, indent, raw);
            if((st->kind == KIR_STMT_WHILE || st->kind == KIR_STMT_FOR ||
                st->kind == KIR_STMT_SWITCH) && raw[strlen(raw) ? strlen(raw) - 1 : 0] == '{' &&
               block_top < (int)(sizeof(block_stack) / sizeof(block_stack[0])))
                block_stack[block_top++] = 0;
            break;
        }
    }
    fprintf(f, "  return kryon.snapshot($rt);\n");
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

        const KirStateField *field=&m->state_fields[i];
        if(!field->init[0] && *kir_skip_ws(field->type) == '[') {
            fprintf(f, "    %s: ", field->name);
            emit_zero_value(f, m, field->type);
            fprintf(f, "%s\n", i + 1 < m->state_count ? "," : "");
            continue;
        }
        const char *value = field->init[0] ? field->init :
            !strcmp(field->type, "string") ? "\"\"" :
            !strcmp(KirScalarType(field->type), "bool") ? "false" : "0";
        if(!KirScalarLiteral(field->type,value,KIR_JS,field->span,init,sizeof(init)))
            tx_expr(m,value,init,sizeof(init));
        if(!strcmp(KirScalarType(field->type),"f32")) {
            char raw[K2JS_TEXT_MAX];snprintf(raw,sizeof(raw),"%s",init);
            snprintf(init,sizeof(init),"Math.fround(%.8000s)",raw);
        }
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
emit_enums(FILE *f, const KirModule *m)
{
    for(int i = 0; i < m->type_count; i++) {
        const KirType *type = &m->types[i];
        const char *cursor = type->body;
        char name[K2JS_NAME_MAX];
        char previous[K2JS_NAME_MAX] = "";
        char value[K2JS_TEXT_MAX];
        char expression[K2JS_TEXT_MAX];

        if(!type->is_enum && strcmp(type->name, "#enum") != 0)
            continue;
        while(next_enum_member(&cursor, name, sizeof(name), value, sizeof(value))) {
            if(value[0] != '\0')
                tx_expr(m, value, expression, sizeof(expression));
            else if(previous[0] != '\0')
                snprintf(expression, sizeof(expression), "%s + 1", previous);
            else
                snprintf(expression, sizeof(expression), "0");
            fprintf(f, "export const %s = %s;\n", name, expression);
            snprintf(previous, sizeof(previous), "%s", name);
        }
    }
}

/* Emit only referenced functions. Imports may appear after declarations in an
 * ES module, so lowering can discover dependencies without a second body pass.
 * Paths go through the output root, independent of source directory depth. */
static void
emit_function_imports(FILE *f, const char *consumer_stem)
{
    for(int i = 0; i < g_enum_import_count; i++) {
        char provider_stem[KIR_PATH_MAX];
        char path[KIR_PATH_MAX * 4 + 16] = "./";
        size_t length = 2;
        for(const char *p = consumer_stem; *p; p++) {
            if(*p == '/') {
                memcpy(path + length, "../", 3);
                length += 3;
            }
        }
        stem_from_source(g_enum_imports[i]->source_path, provider_stem, sizeof(provider_stem));
        snprintf(path + length, sizeof(path) - length, "%s.js", provider_stem);
        fprintf(f, "import * as $enum%d from ", i);
        js_string(f, path);
        fputs(";\n", f);
    }
    for(int i = 0; i < g_function_count; i++) {
        const K2jsGlobalFunction *fn = &g_functions[i];
        char provider_stem[KIR_PATH_MAX];
        char path[KIR_PATH_MAX * 4 + 16];
        size_t length = 2;

        if(!fn->used)
            continue;
        path[0] = '.';
        path[1] = '/';
        for(const char *p = consumer_stem; *p != '\0'; p++) {
            if(*p == '/') {
                memcpy(path + length, "../", 3);
                length += 3;
            }
        }
        stem_from_source(fn->module->source_path, provider_stem,
                         sizeof(provider_stem));
        snprintf(path + length, sizeof(path) - length, "%s.js", provider_stem);
        fprintf(f, "import { %s } from ", fn->js);
        js_string(f, path);
        fprintf(f, ";\n");
    }
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

/* Frames either have no arguments or consume the host viewport. A record
 * parameter on a reusable widget is not an implicit application entry. */
static int
frame_parameters(const KirModule *m, const KirFunction *fn)
{
    if(fn->is_extern)
        return -1;
    if(*kir_skip_ws(fn->args) == '\0')
        return 0;
    char parts[2][K2JS_TEXT_MAX];
    int count = kir_split_top(fn->args, parts[0], 2, sizeof(parts[0]));
    char *colon = count == 1 ? strchr(parts[0], ':') : NULL;
    if(colon == NULL)
        return -1;
    char *type = colon + 1;
    kir_trim_in_place(type);
    const KirType *record = KirFindType(m, type, NULL);
    if(strcmp(type, "Rectangle") == 0 &&
       (record == NULL || record == KirFindRuntimeType(type, NULL)))
        return 1;
    return -1;
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
        if(frame_parameters(m, &m->functions[i]) >= 0 &&
           strcmp(m->functions[i].name, "App") == 0)
            return &m->functions[i];
    for(int i = 0; i < m->function_count; i++)
        if(m->functions[i].is_ui && frame_parameters(m, &m->functions[i]) >= 0)
            return &m->functions[i];
    for(int i = 0; i < m->function_count; i++)
        if(!m->functions[i].is_extern && m->functions[i].args[0] == '\0')
            return &m->functions[i];
    return NULL;
}

int
k2js_lower(const KirProgram *const *progs, int prog_count,
           const char *root, const char *out_dir,
           const char *runtime_import, int no_main)
{
    char path[KIR_PATH_MAX * 2];

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
            kir_camel_ident(base, guard, sizeof(guard));
            if(!validate_asserts(m))
                return 1;
            set_module(m, guard);
            for(int i = 0; i < g_function_count; i++)
                g_functions[i].used = 0;
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
            KirEmitNumbers(f,m,KIR_JS);
            emit_enums(f, m);
            for(int i = 0; i < m->global_count; i++) {
                const KirGlobal *global = &m->globals[i];
                char value[K2JS_TEXT_MAX];
                fprintf(f, "let %s = ", global->name);
                if(KirScalarLiteral(global->type, global->init, KIR_JS,
                                    global->span, value, sizeof(value))) {
                    fputs(value, f);
                } else if(global->init[0]) {
                    emit_initializer_value(f, m, global->init);
                } else {
                    emit_zero_value(f, m, global->type);
                }
                fputs(";\n", f);
            }
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

                kir_camel_ident(frame_fn->name, fname, sizeof(fname));
                fprintf(f, "  kryon.beginFrame(rt);\n");
                fprintf(f, "  const result = %s_%s(rt, state, host%s);\n",
                        guard, fname, frame_parameters(m, frame_fn) == 1
                            ? ", kryon.viewport(rt, app)" : "");
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
            emit_function_imports(f, stem);
            fclose(f);
        }
    }
    return 0;
}
