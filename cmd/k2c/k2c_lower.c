/*
 * k2c_lower.c - Kir → C backend. Lowers a KirProgram to .c/.h source files.
 * This is the only .kry→C pipeline: the legacy kc line-matcher is gone.
 */
#include "k2c_lower.h"
#include "k2c_plan9.h"
#include "kir.h"
#include "kir_text.h"
#include "kir_emit.h"
#include "kir_check.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LOWER_NAME_MAX 128
#define LOWER_TEXT_MAX 4096

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
guard_from_stem(const char *stem, char *dst, size_t dst_size)
{
    size_t n = 0;

    if(dst_size > 2) {
        dst[n++] = 'K';
        dst[n++] = '_';
    }
    for(const char *p = stem; *p && n + 3 < dst_size; p++) {
        char ch = *p;

        if(isalnum((unsigned char)ch))
            dst[n++] = isalpha((unsigned char)ch) ? (char)toupper(ch) : ch;
        else
            dst[n++] = '_';
    }
    dst[n++] = '_';
    dst[n++] = 'H';
    dst[n] = '\0';
}

/* Convert a .kry type like "[64] char" or "[2][3] int" to C declarator
 * pieces: base "char" + array suffix "[64]" (placed after the name). */
static void
split_array_type(const char *type, char *base, size_t base_size,
                 char *suffix, size_t suffix_size)
{
    const char *p = type;
    size_t sn = 0;

    suffix[0] = '\0';
    while(*p == '[') {
        const char *close = strchr(p, ']');

        if(close == NULL)
            break;
        {
            size_t len = (size_t)(close - p + 1);

            if(sn + len + 1 < suffix_size) {
                memcpy(suffix + sn, p, len);
                sn += len;
                suffix[sn] = '\0';
            }
        }
        p = close + 1;
        while(*p == ' ' || *p == '\t')
            p++;
    }
    snprintf(base, base_size, "%s", p);
}

static int is_module_alias(const KirModule *m, const char *alias,
                           size_t alias_len);
static void function_c_name(const KirModule *m, const KirFunction *fn,
                            char *dst, size_t dst_size);

static int
function_name_needs_global_prefix(const char *name)
{
    return strcmp(name, "App") == 0 ||
           strcmp(name, "AppHost") == 0 ||
           strcmp(name, "AppRouteInfo") == 0;
}

/* If ident (len chars, followed by '(') names a function in this module,
 * write its full C name into dst and return its length; else return 0. */
static size_t
resolve_module_fn(const KirModule *m, const char *ident, size_t len,
                  char *dst, size_t dst_size)
{
    int i;

    for(i = 0; i < m->function_count; i++) {
        const KirFunction *fn = &m->functions[i];

        if(strlen(fn->name) == len && strncmp(fn->name, ident, len) == 0) {
            function_c_name(m, fn, dst, dst_size);
            return strlen(dst);
        }
    }
    return 0;
}

/* Resolve alias.fn( via the cross-module symbol table: find the import
 * named alias, get its target module, look up fn there. */
static size_t
resolve_aliased_fn(const KirModule *m, const K2cModuleSyms *restab,
                   int restab_count, const char *alias, size_t alen,
                   const char *fn, size_t flen, char *dst, size_t dst_size)
{
    int i, j;

    if(restab == NULL)
        return 0;
    for(i = 0; i < m->import_count; i++) {
        const KirImport *imp = &m->imports[i];

        if(imp->kind != KIR_IMPORT_MODULE)
            continue;
        if(strlen(imp->name) != alen || strncmp(imp->name, alias, alen) != 0)
            continue;
        for(j = 0; j < restab_count; j++) {
            if(strcmp(restab[j].module_slash, imp->target) != 0 &&
               strcmp(restab[j].module_stem, imp->target) != 0)
                continue;
            for(int k = 0; k < restab[j].fn_count; k++) {
                if(strlen(restab[j].fns[k].kry) == flen &&
                   strncmp(restab[j].fns[k].kry, fn, flen) == 0) {
                    snprintf(dst, dst_size, "%s", restab[j].fns[k].c);
                    return strlen(dst);
                }
            }
        }
    }
    return 0;
}

/* Rewrite a statement body: `nil` -> `NULL`, `alias.X` -> `X` for module
 * aliases (enum/type members), bare calls to module functions -> C names,
 * and alias.fn( cross-module calls -> the target's C name. */

/* Is ident (len chars) in the space-separated shadow list? Parameters and
 * locals shadow module functions of the same name. */
static int
name_is_shadowed(const char *shadow, const char *ident, size_t len)
{
    const char *p;

    if(shadow == NULL || shadow[0] == '\0')
        return 0;
    p = shadow;
    while(*p != '\0') {
        const char *e = p;
        size_t l;

        while(*e != '\0' && *e != ' ')
            e++;
        l = (size_t)(e - p);
        if(l == len && strncmp(p, ident, len) == 0)
            return 1;
        p = e;
        while(*p == ' ')
            p++;
    }
    return 0;
}

static void
rewrite_body2(const KirModule *m, const K2cModuleSyms *restab,
              int restab_count, const char *src, char *dst, size_t dst_size,
              const char *shadow)
{
    size_t n = 0;

    for(const char *p = src; *p != '\0' && n + 6 < dst_size; p++) {
        if(strncmp(p, "nil", 3) == 0 &&
           (p == src || !isalnum((unsigned char)p[-1])) &&
           !isalnum((unsigned char)p[3]) && p[3] != '_') {
            dst[n++] = 'N';
            dst[n++] = 'U';
            dst[n++] = 'L';
            dst[n++] = 'L';
            p += 2;
        } else if(isalpha((unsigned char)*p) || *p == '_') {
            const char *e = p;

            while(isalnum((unsigned char)*e) || *e == '_')
                e++;
            if(*e == '.' && e[1] != '\0' &&
               is_module_alias(m, p, (size_t)(e - p))) {
                /* alias.member — cross-module call or enum/type member */
                const char *m0 = e + 1;
                const char *me = m0;

                while(isalnum((unsigned char)*me) || *me == '_')
                    me++;
                if(*me == '(' && restab != NULL) {
                    char cname[LOWER_NAME_MAX * 3];
                    size_t clen = resolve_aliased_fn(m, restab, restab_count,
                                                     p, (size_t)(e - p),
                                                     m0, (size_t)(me - m0),
                                                     cname, sizeof(cname));

                    if(clen > 0) {
                        if(n + clen < dst_size) {
                            memcpy(dst + n, cname, clen);
                            n += clen;
                        }
                        p = me - 1;   /* loop's p++ lands on '(' */
                        continue;
                    }
                }
                p = e;   /* strip the alias; loop's p++ skips the '.' */
                continue;
            }
            if(!(p > src && p[-1] == '.') &&
               !(p > src + 1 && p[-1] == '>' && p[-2] == '-') &&
               *e == '(' && e[-1] != ' ') {
                /* a call (not a member access 'x.fn' / 'p->fn'): resolve
                 * module-local functions to C names */
                char cname[LOWER_NAME_MAX * 2];
                size_t clen = resolve_module_fn(m, p, (size_t)(e - p),
                                                cname, sizeof(cname));

                if(clen > 0) {
                    if(n + clen < dst_size) {
                        memcpy(dst + n, cname, clen);
                        n += clen;
                    }
                    p = e - 1;   /* loop's p++ lands past the ident */
                    continue;
                }
            }
            /* bare function reference in assignment-RHS position
             * ('= name' / '= name;'): resolve, but never inside call
             * parens where a local of the same name may shadow it. */
            if(!(p > src && p[-1] == '.') &&
               !(p > src + 1 && p[-1] == '>' && p[-2] == '-') &&
               ((*e == '-' && e[1] == '>') || *e == '.')) {
                /* 'name->' / 'name.' — a variable access, not a function
                 * reference ('habits->count' must never resolve against a
                 * screen named 'habits'). Skip resolution; fall through. */
            } else if(!name_is_shadowed(shadow, p, (size_t)(e - p)) &&
               !(p > src && p[-1] == '.') &&
               !(p > src + 1 && p[-1] == '>' && p[-2] == '-') &&
               *e != '(' && n >= 2 && dst[n - 1] == ' ' && dst[n - 2] == '=' &&
               (n < 3 || (dst[n - 3] != '=' && dst[n - 3] != '!'))) {
                char cname[LOWER_NAME_MAX * 2];
                size_t clen = resolve_module_fn(m, p, (size_t)(e - p),
                                                cname, sizeof(cname));

                if(clen > 0) {
                    if(n + clen < dst_size) {
                        memcpy(dst + n, cname, clen);
                        n += clen;
                    }
                    p = e - 1;
                    continue;
                }
            }
            /* standalone call argument ('set_cb(name)' / 'f(a, name)'):
             * a bare identifier passing a function by reference. */
            if(!name_is_shadowed(shadow, p, (size_t)(e - p)) &&
               !(p > src && p[-1] == '.') &&
               !(p > src + 1 && p[-1] == '>' && p[-2] == '-') &&
               *e != '(' && (*e == ')' || *e == ',') &&
               n >= 1 && (dst[n - 1] == '(' ||
                          (n >= 2 && dst[n - 1] == ' ' &&
                           dst[n - 2] == ','))) {
                char cname[LOWER_NAME_MAX * 2];
                size_t clen = resolve_module_fn(m, p, (size_t)(e - p),
                                                cname, sizeof(cname));

                if(clen > 0) {
                    if(n + clen < dst_size) {
                        memcpy(dst + n, cname, clen);
                        n += clen;
                    }
                    p = e - 1;
                    continue;
                }
            }
            while(p < e && n + 1 < dst_size)
                dst[n++] = *p++;
            p--;   /* compensate for the loop's p++ */
        } else {
            dst[n++] = *p;
        }
    }
    dst[n] = '\0';
}

/* C function name: <module>_<name> (module dots -> underscores), with a
 * _kry_draw suffix for screen/body functions. */
static void
function_c_name(const KirModule *m, const KirFunction *fn,
                char *dst, size_t dst_size)
{
    char mod[LOWER_NAME_MAX];
    size_t n = 0;
    const char *suffix = fn->is_colon ? "" : "_kry_draw";

    /* '#export' keeps the plain Kry name: the symbol is project-global,
     * not module-prefixed (legacy global_name rule). */
    if(fn->exported) {
        snprintf(dst, dst_size, "%s%s", fn->name, suffix);
        return;
    }
    if(m->name[0] != '\0' && strcmp(m->name, "main") != 0) {
        for(const char *p = m->name; *p && n + 1 < sizeof(mod); p++)
            mod[n++] = (*p == '.') ? '_' : *p;
        mod[n] = '\0';
        snprintf(dst, dst_size, "%s_%s%s", mod, fn->name, suffix);
    } else if(function_name_needs_global_prefix(fn->name)) {
        snprintf(dst, dst_size, "kry_%s%s", fn->name, suffix);
    } else {
        snprintf(dst, dst_size, "%s%s", fn->name, suffix);
    }
}

void
k2c_function_c_name(const KirModule *m, const KirFunction *fn,
                    char *dst, size_t dst_size)
{
    function_c_name(m, fn, dst, dst_size);
}

/* Is `alias` a module-import alias in this module (alias :: #import "path")?
 * If so, `alias.Type` qualifiers strip to the bare type. */
static int
is_module_alias(const KirModule *m, const char *alias, size_t alias_len)
{
    int i;

    for(i = 0; i < m->import_count; i++) {
        if(m->imports[i].kind == KIR_IMPORT_MODULE &&
           strlen(m->imports[i].name) == alias_len &&
           strncmp(m->imports[i].name, alias, alias_len) == 0)
            return 1;
    }
    return 0;
}

/* Strip leading `alias.` module qualifiers from a type when the alias is an
 * import in this module (legacy strip_module_alias behavior). */
static void
strip_alias_type(const KirModule *m, const char *type,
                 char *dst, size_t dst_size)
{
    const char *dot = strchr(type, '.');
    const char *scalar = KirScalarType(type);
    if(*scalar && !strcmp(scalar,type) && KirTargetType(type,KIR_C)) {
        snprintf(dst,dst_size,"%s",KirTargetType(type,KIR_C)); return;
    }

    if(dot != NULL) {
        size_t alen = (size_t)(dot - type);

        if(is_module_alias(m, type, alen)) {
            snprintf(dst, dst_size, "%s", dot + 1);
            return;
        }
    }
    snprintf(dst, dst_size, "%s", type);
}

/* Convert .kry args "viewport: Rectangle, st: state.IdeState*" to C
 * "Rectangle viewport, IdeState* st" (alias-qualified types stripped). */
static void
convert_args(const KirModule *m, const char *args, char *dst, size_t dst_size)
{
    size_t n = 0;

    dst[0] = '\0';
    if(args == NULL || args[0] == '\0') {
        snprintf(dst, dst_size, "void");
        return;
    }
    /* Split on top-level commas; each part is "name: Type". */
    {
        const char *p = args;
        int depth = 0;
        const char *start = p;
        int first = 1;

        while(1) {
            if(*p == '(' || *p == '[' || *p == '{')
                depth++;
            else if(*p == ')' || *p == ']' || *p == '}')
                depth--;
            if((*p == ',' && depth == 0) || *p == '\0') {
                char part[LOWER_TEXT_MAX];
                size_t len = (size_t)(p - start);
                const char *colon;

                if(len >= sizeof(part))
                    len = sizeof(part) - 1;
                memcpy(part, start, len);
                part[len] = '\0';
                /* trim */
                {
                    char *e = part + strlen(part);
                    while(e > part && (e[-1] == ' ' || e[-1] == '\t'))
                        *--e = '\0';
                }
                colon = strchr(part, ':');
                if(colon != NULL) {
                    char name[LOWER_NAME_MAX];
                    char type[LOWER_NAME_MAX];
                    size_t nl = (size_t)(colon - part);
                    const char *ty = colon + 1;

                    while(*ty == ' ' || *ty == '\t')
                        ty++;
                    if(nl >= sizeof(name))
                        nl = sizeof(name) - 1;
                    memcpy(name, part, nl);
                    name[nl] = '\0';
                    strip_alias_type(m, ty, type, sizeof(type));
                    {
                        /* 'name: [N] Type' parameters must emit C array
                         * syntax 'Type name[N]', not '[N] Type name'. */
                        char pbase[LOWER_NAME_MAX];
                        char psuffix[LOWER_NAME_MAX];

                        split_array_type(type, pbase, sizeof(pbase),
                                         psuffix, sizeof(psuffix));
                        if(!first && n + 2 < dst_size)
                            dst[n++] = ',';
                        if(!first && n + 1 < dst_size)
                            dst[n++] = ' ';
                        n += (size_t)snprintf(dst + n, dst_size - n,
                                              "%s %s%s", pbase, name,
                                              psuffix);
                    }
                    first = 0;
                } else if(part[0] != '\0') {
                    /* C-style parameter text (no 'name: Type' colon):
                     * emit as-is ('InbeApp *app'). */
                    if(!first && n + 2 < dst_size)
                        dst[n++] = ',';
                    if(!first && n + 1 < dst_size)
                        dst[n++] = ' ';
                    n += (size_t)snprintf(dst + n, dst_size - n, "%s", part);
                    first = 0;
                }
                if(*p == '\0')
                    break;
                start = p + 1;
            }
            p++;
        }
    }
    if(n == 0)
        snprintf(dst, dst_size, "void");
}

/* ---- statement lowering ---- */

static void
emit_indent(FILE *c, int indent)
{
    for(int i = 0; i < indent; i++)
        fputs("    ", c);
}

/* Split "a, b := e1, e2" / "a, b = e1, e2" into names + exprs. Returns the
 * count of names, or 0 if not a multi-assign. */
static int
split_multi(const char *t, char names[][LOWER_NAME_MAX], int name_cap,
            char exprs[][LOWER_TEXT_MAX], int expr_cap)
{
    const char *colon = strstr(t, ":=");
    const char *eq = NULL;
    const char *lhs_end;
    const char *rhs;
    int nn = 0;
    int ne = 0;

    if(colon != NULL) {
        lhs_end = colon;
        rhs = colon + 2;
    } else {
        eq = strstr(t, " = ");
        if(eq == NULL)
            return 0;
        lhs_end = eq;
        rhs = eq + 3;
    }
    /* split lhs on commas */
    {
        const char *start = t;
        const char *p = t;
        int depth = 0;

        while(p <= lhs_end) {
            if(*p == '(' || *p == '[')
                depth++;
            else if(*p == ')' || *p == ']')
                depth--;
            if((*p == ',' && depth == 0) || p == lhs_end) {
                size_t len = (size_t)(p - start);

                if(nn < name_cap && len > 0 && len < LOWER_NAME_MAX) {
                    memcpy(names[nn], start, len);
                    names[nn][len] = '\0';
                    nn++;
                }
                start = p + 1;
            }
            p++;
        }
    }
    if(nn < 2)
        return 0;
    /* split rhs on top-level commas */
    {
        const char *start = rhs;
        const char *p = rhs;
        int depth = 0;

        while(1) {
            if(*p == '(' || *p == '[' || *p == '{')
                depth++;
            else if(*p == ')' || *p == ']' || *p == '}')
                depth--;
            if((*p == ',' && depth == 0) || *p == '\0') {
                size_t len = (size_t)(p - start);

                if(ne < expr_cap && len < LOWER_TEXT_MAX) {
                    memcpy(exprs[ne], start, len);
                    exprs[ne][len] = '\0';
                    ne++;
                }
                if(*p == '\0')
                    break;
                start = p + 1;
            }
            p++;
        }
    }
    if(ne == 0)
        return 0;
    return nn;
}

/* >0 while a multi-line array initializer is open: item statements join
 * with ',' and the brace closes with '};' */
static int k2c_in_array_init;

static void
emit_call_wrap(FILE *c, const KirModule *m, const K2cModuleSyms *restab,
               int restab_count, int line, const char *text,
               const char *shadow)
{
    char rw[LOWER_TEXT_MAX];

    rewrite_body2(m, restab, restab_count, text, rw, sizeof(rw), shadow);
    if(k2c_in_array_init) {
        /* initializer items: bare expressions, no inspect wrapper */
        fprintf(c, "    %s\n", rw);
        return;
    }
    fprintf(c, "    PushUIInspectSource(\"%s\", %d);\n", m->source_path, line);
    fprintf(c, "    %s;\n", rw);
    fprintf(c, "    PopUIInspectSource();\n");
}

typedef struct BodySymbols {
    const KirModule *module;
    const K2cModuleSyms *symbols;
    int count;
} BodySymbols;

static void
resolve_body_symbol(void *context, const char *text, char *out, size_t size)
{
    BodySymbols *symbols = context;
    rewrite_body2(symbols->module, symbols->symbols, symbols->count, text, out, size, "");
}

/* Cleanup is expanded by the shared KIR pass before target emission. */
static void
lower_body(FILE *c, const KirModule *m, const K2cModuleSyms *restab, int restab_count, const KirFunction *fn)
{
    BodySymbols symbols = {m, restab, restab_count};
    if(KirEmitBody(c, m, fn, KIR_C, resolve_body_symbol, &symbols)) return;
    int indent = 1;
    int j;
    char shadow[LOWER_TEXT_MAX];   /* param + local names shadow functions */

    shadow[0] = '\0';
    {
        /* parameter names from 'a: T, b: T' (colon-less C-style parts
         * end at the last space before ',' / end). */
        const char *p = fn->args;

        while(p != NULL && *p != '\0') {
            const char *e = p;

            while(*e != '\0' && *e != ',')
                e++;
            {
                const char *colon = NULL;
                const char *q;

                for(q = p; q < e; q++)
                    if(*q == ':')
                        colon = q;
                if(colon == NULL) {
                    /* C-style 'InbeApp *app': name = trailing ident */
                    q = e;
                    while(q > p && *(q - 1) == ' ')
                        q--;
                    while(q > p && (isalnum((unsigned char)*(q - 1)) ||
                                    *(q - 1) == '_'))
                        q--;
                    colon = q;
                }
                if(colon > p) {
                    size_t nl = (size_t)(colon - p);

                    if(strlen(shadow) + nl + 2 < sizeof(shadow)) {
                        if(shadow[0] != '\0')
                            strcat(shadow, " ");
                        strncat(shadow, p, nl);
                    }
                }
            }
            p = *e == ',' ? e + 1 : NULL;
        }
    }
    for(j = 0; j < fn->stmt_count; j++) {
        const KirStmt *st = &fn->stmts[j];
        char rw[LOWER_TEXT_MAX];

        rewrite_body2(m, restab, restab_count, st->text, rw, sizeof(rw),
                      shadow);
        switch(st->kind) {
        case KIR_STMT_BLOCK_CLOSE: {
            int skip_close = 0;

            if(k2c_in_array_init) {
                /* close the initializer, not a block */
                emit_indent(c, indent);
                fprintf(c, "};\n");
                k2c_in_array_init = 0;
                break;
            }

            /* An else / else-if statement emits its own leading '}', so the
             * block close right before it is suppressed. */
            if(j + 1 < fn->stmt_count &&
               fn->stmts[j + 1].kind == KIR_STMT_IF &&
               strncmp(fn->stmts[j + 1].text, "else", 4) == 0)
                skip_close = 1;
            indent--;
            if(indent < 1)
                indent = 1;
            if(!skip_close) {
                emit_indent(c, indent);
                fprintf(c, "}\n");
            }

            break;
        }
        case KIR_STMT_IF: {
            char cond[LOWER_TEXT_MAX];

            snprintf(cond, sizeof(cond), "%s", rw);
            kir_strip_block_brace(cond);
            if(strncmp(cond, "guard ", 6) == 0) {
                /* 'guard cond' exits when the condition is true. */
                memmove(cond, cond + 6, strlen(cond + 6) + 1);
                emit_indent(c, indent);
                fprintf(c, "if(%s) {\n", cond);
                emit_indent(c, indent + 1);
                fprintf(c, "return;\n");
                emit_indent(c, indent);
                fprintf(c, "}\n");
                break;
            }
            if(strncmp(cond, "else if ", 8) == 0) {
                memmove(cond, cond + 8, strlen(cond + 8) + 1);
                emit_indent(c, indent);
                fprintf(c, "} else if(%s) {\n", cond);
            } else if(strncmp(cond, "else", 4) == 0 && cond[4] == '\0') {
                emit_indent(c, indent);
                fprintf(c, "} else {\n");
            } else if(strncmp(cond, "if ", 3) == 0) {
                memmove(cond, cond + 3, strlen(cond + 3) + 1);
                emit_indent(c, indent);
                fprintf(c, "if(%s) {\n", cond);
            } else {
                emit_indent(c, indent);
                fprintf(c, "if(%s) {\n", cond);
            }
            indent++;
            break;
        }
        case KIR_STMT_WHILE: {
            char cond[LOWER_TEXT_MAX];

            snprintf(cond, sizeof(cond), "%s", rw);
            kir_strip_block_brace(cond);
            if(strncmp(cond, "while ", 6) == 0)
                memmove(cond, cond + 6, strlen(cond + 6) + 1);
            emit_indent(c, indent);
            fprintf(c, "while(%s) {\n", cond);
            indent++;
            break;
        }
        case KIR_STMT_FOR:
        case KIR_STMT_SWITCH: {
            char head[LOWER_TEXT_MAX];
            const char *kw = (st->kind == KIR_STMT_FOR) ? "for" : "switch";

            snprintf(head, sizeof(head), "%s", rw);
            kir_strip_block_brace(head);
            if(strncmp(head, kw, strlen(kw)) == 0 && head[strlen(kw)] == ' ')
                memmove(head, head + strlen(kw) + 1,
                        strlen(head + strlen(kw) + 1) + 1);
            emit_indent(c, indent);
            fprintf(c, "%s(%s) {\n", kw, head);
            indent++;
            break;
        }
        case KIR_STMT_CASE:
            emit_indent(c, indent - 1 > 0 ? indent - 1 : 1);
            fprintf(c, "%s\n", rw);
            break;
        case KIR_STMT_RETURN:
            emit_indent(c, indent);
            fprintf(c, "%s;\n", rw);
            break;
        case KIR_STMT_BREAK:
        case KIR_STMT_CONTINUE:
            emit_indent(c, indent);
            fprintf(c, "%s;\n", rw);
            break;
        case KIR_STMT_GOTO:
            emit_indent(c, indent);
            fprintf(c, "%s;\n", rw);
            break;
        case KIR_STMT_LABEL:
            fprintf(c, "%s\n", rw);   /* no indent for labels */
            break;
        case KIR_STMT_DEFER:
            fprintf(stderr, "internal error: cleanup was not lowered\n");
            exit(1);
        case KIR_STMT_UNUSED: {
            const char *u = rw;

            while(*u == ' ')
                u++;
            if(strncmp(u, "unused ", 7) == 0)
                u += 7;
            emit_indent(c, indent);
            fprintf(c, "(void)%s;\n", u);
            break;
        }
        case KIR_STMT_DECL: {
            /* x := e  |  x, y := e1, e2  |  x: T = e  |  x: T */
            char names[8][LOWER_NAME_MAX];
            char exprs[8][LOWER_TEXT_MAX];
            int n = split_multi(rw, names, 8, exprs, 8);

            /* declared names shadow functions from here on */
            {
                int k;

                for(k = 0; k < (n >= 2 ? n : 1); k++) {
                    const char *nm = n >= 2 ? names[k] : rw;

                    if(n < 2) {
                        /* single decl: name is rw up to ':=' / ':' */
                        const char *c2 = strstr(rw, ":=");

                        if(c2 == NULL)
                            c2 = strchr(rw, ':');
                        if(c2 != NULL) {
                            size_t nl = (size_t)(c2 - rw);

                            if(nl > 0 &&
                               strlen(shadow) + nl + 2 < sizeof(shadow)) {
                                if(shadow[0] != '\0')
                                    strcat(shadow, " ");
                                strncat(shadow, rw, nl);
                            }
                        }
                    } else if(nm[0] != '\0' &&
                              strlen(shadow) + strlen(nm) + 2 <
                              sizeof(shadow)) {
                        if(shadow[0] != '\0')
                            strcat(shadow, " ");
                        strcat(shadow, nm);
                    }
                }
            }

            if(n >= 2) {
                /* multi: temps first, then assignments */
                for(int k = 0; k < n; k++) {
                    emit_indent(c, indent);
                    fprintf(c, "__auto_type __kryon_assign_%d_%d = %s;\n",
                            st->span.line, k, exprs[k % (n > 0 ? n : 1)]);
                }
                for(int k = 0; k < n; k++) {
                    emit_indent(c, indent);
                    fprintf(c, "%s = __kryon_assign_%d_%d;\n",
                            names[k], st->span.line, k);
                }
            } else {
                const char *colon2 = strstr(rw, ":=");
                const char *typed = strchr(rw, ':');

                if(colon2 != NULL) {
                    char name[LOWER_NAME_MAX];
                    size_t nl = (size_t)(colon2 - rw);
                    const char *expr = colon2 + 2;

                    while(*expr == ' ')
                        expr++;
                    while(nl > 0 && (rw[nl - 1] == ' ' || rw[nl - 1] == '\t'))
                        nl--;
                    if(nl >= sizeof(name))
                        nl = sizeof(name) - 1;
                    memcpy(name, rw, nl);
                    name[nl] = '\0';
                    emit_indent(c, indent);
                    fprintf(c, "__auto_type %s = %s;\n", name, expr);
                } else if(typed != NULL) {
                    char name[LOWER_NAME_MAX];
                    char type[LOWER_NAME_MAX];
                    size_t nl = (size_t)(typed - rw);
                    const char *ty = typed + 1;
                    const char *eq2 = strstr(typed, " = ");

                    while(*ty == ' ' || *ty == '\t')
                        ty++;
                    while(nl > 0 && (rw[nl - 1] == ' ' || rw[nl - 1] == '\t'))
                        nl--;
                    if(nl >= sizeof(name))
                        nl = sizeof(name) - 1;
                    memcpy(name, rw, nl);
                    name[nl] = '\0';
                    if(eq2 != NULL) {
                        size_t tl = (size_t)(eq2 - ty);
                        const char *init = eq2 + 3;
                        char base[LOWER_NAME_MAX];
                        char suffix[LOWER_NAME_MAX];

                        if(tl >= sizeof(type))
                            tl = sizeof(type) - 1;
                        memcpy(type, ty, tl);
                        type[tl] = '\0';
                        split_array_type(type, base, sizeof(base),
                                         suffix, sizeof(suffix));
                        {
                            char tmpb[LOWER_NAME_MAX];

                            strip_alias_type(m, base, tmpb, sizeof(tmpb));
                            snprintf(base, sizeof(base), "%s", tmpb);
                        }
                        emit_indent(c, indent);
                        if(init[0] == '{' && init[1] == '\0') {
                            fprintf(c, "%s %s%s = {\n", base, name, suffix);
                            k2c_in_array_init = 1;
                        } else {
                            fprintf(c, "%s %s%s = %s;\n", base, name, suffix,
                                    init);
                        }
                    } else {
                        char base[LOWER_NAME_MAX];
                        char suffix[LOWER_NAME_MAX];

                        snprintf(type, sizeof(type), "%s", ty);
                        /* strip trailing brace if typed on block opener */
                        {
                            char *br = strchr(type, '{');
                            if(br != NULL)
                                *br = '\0';
                        }
                        split_array_type(type, base, sizeof(base),
                                         suffix, sizeof(suffix));
                        {
                            char tmpb[LOWER_NAME_MAX];

                            strip_alias_type(m, base, tmpb, sizeof(tmpb));
                            snprintf(base, sizeof(base), "%s", tmpb);
                        }
                        emit_indent(c, indent);
                        fprintf(c, "%s %s%s = {0};\n", base, name, suffix);
                    }
                } else {
                    emit_indent(c, indent);
                    fprintf(c, "%s;\n", rw);
                }
            }
            break;
        }
        case KIR_STMT_ASSIGN: {
            char names[8][LOWER_NAME_MAX];
            char exprs[8][LOWER_TEXT_MAX];
            int n = split_multi(rw, names, 8, exprs, 8);

            if(n >= 2) {
                for(int k = 0; k < n; k++) {
                    emit_indent(c, indent);
                    fprintf(c, "__auto_type __kryon_assign_%d_%d = %s;\n",
                            st->span.line, k, exprs[k]);
                }
                for(int k = 0; k < n; k++) {
                    emit_indent(c, indent);
                    fprintf(c, "%s = __kryon_assign_%d_%d;\n",
                            names[k], st->span.line, k);
                }
            } else {
                emit_indent(c, indent);
                fprintf(c, "%s;\n", rw);
            }
            break;
        }
        case KIR_STMT_EXPR:
            if(strchr(rw, '(') != NULL)
                emit_call_wrap(c, m, restab, restab_count, st->span.line,
                               st->text, shadow);
            else {
                emit_indent(c, indent);
                fprintf(c, "%s;\n", rw);
            }
            break;
        case KIR_STMT_WIDGET:
            emit_call_wrap(c, m, restab, restab_count, st->span.line,
                           st->text, shadow);
            break;
        default:
            if(rw[0] != '\0') {
                emit_indent(c, indent);
                fprintf(c, "%s\n", rw);
            }
            break;
        }
    }
}

/* '#if' regions stamp their captures with the expanded C preprocessor
 * condition; emit each guarded item wrapped in '#if cond / #endif'. */
static void
emit_guard_open(FILE *out, const char *guard)
{
    if(guard[0] != '\0')
        fprintf(out, "#if %s\n", guard);
}

static void
emit_guard_close(FILE *out, const char *guard)
{
    if(guard[0] != '\0')
        fprintf(out, "#endif\n");
}

static void
emit_compile_assert(FILE *out, const KirAssert *assertion)
{
    emit_guard_open(out, assertion->guard);
    fprintf(out, "#if !(%s)\n", assertion->condition);
    fprintf(out, "#error %s\n", assertion->message);
    fprintf(out, "#endif\n");
    emit_guard_close(out, assertion->guard);
}

/* '#intrinsic "web"' wrappers: static EM_ASM shims, built only when
 * PLATFORM_WEB is defined (matching the legacy compiler). */
static const char *const web_download_body[] = {
    "    return EM_ASM_INT({",
    "        try {",
    "            const path = UTF8ToString($0);",
    "            const filename = UTF8ToString($1);",
    "            const mime = UTF8ToString($2);",
    "            const bytes = FS.readFile(path);",
    "            const blob = new Blob([bytes], "
    "{type: mime || \"application/octet-stream\"});",
    "            const url = URL.createObjectURL(blob);",
    "            const a = document.createElement(\"a\");",
    "            a.href = url;",
    "            a.download = filename || \"download\";",
    "            a.style.display = \"none\";",
    "            document.body.appendChild(a);",
    "            a.click();",
    "            a.remove();",
    "            setTimeout(() => URL.revokeObjectURL(url), 1000);",
    "            return 1;",
    "        } catch(e) {",
    "            console.error(\"Kry web download failed:\", e);",
    "            return 0;",
    "        }",
    "    }, path, filename, mime);",
    NULL
};

static const char *const web_context_click_body[] = {
    "    return EM_ASM_INT({",
    "        const click = Module.__kryonContextClick;",
    "        if(!click)",
    "            return 0;",
    "        if(Date.now() - click.time > 750) {",
    "            Module.__kryonContextClick = null;",
    "            return 0;",
    "        }",
    "        if(click.x >= $0 && click.x <= $2 && "
    "click.y >= $1 && click.y <= $3) {",
    "            Module.__kryonContextClick = null;",
    "            return 1;",
    "        }",
    "        return 0;",
    "    }, x0, y0, x1, y1);",
    NULL
};

static void
emit_web_intrinsic_wrapper(FILE *c, const KirModule *m, const KirImport *imp)
{
    const char *sig = imp->signature;
    const char *op = strchr(sig, '(');
    const char *cl = op != NULL ? strrchr(sig, ')') : NULL;
    const char *const *body = NULL;
    char guard[LOWER_TEXT_MAX];
    char cargs[LOWER_TEXT_MAX];
    char conv[LOWER_TEXT_MAX];

    if(op != NULL && cl != NULL && cl > op)
        snprintf(cargs, sizeof(cargs), "%.*s", (int)(cl - op - 1), op + 1);
    else
        snprintf(cargs, sizeof(cargs), "void");
    convert_args(m, cargs, conv, sizeof(conv));
    if(strcmp(imp->name, "web_download_file") == 0)
        body = web_download_body;
    else if(strcmp(imp->name, "web_context_click_in_bounds") == 0)
        body = web_context_click_body;
    else
        body = NULL;
    if(imp->guard[0] != '\0') {
        snprintf(guard, sizeof(guard), "(");
        strncat(guard, imp->guard, sizeof(guard) - strlen(guard) - 1);
        strncat(guard, ") && (defined(PLATFORM_WEB))",
                sizeof(guard) - strlen(guard) - 1);
    } else {
        snprintf(guard, sizeof(guard), "defined(PLATFORM_WEB)");
    }
    fprintf(c, "\n#if %s\n", guard);
    fprintf(c, "static int\n%s(%s)\n{\n", imp->name,
            conv[0] ? conv : "void");
    if(body != NULL) {
        int i;

        for(i = 0; body[i] != NULL; i++)
            fprintf(c, "%s\n", body[i]);
    } else {
        fprintf(c, "    return 0;\n");
    }
    fprintf(c, "}\n#endif\n");
}

static int
c_extern_symbol(const KirImport *imp, char *dst, size_t dst_size)
{
    if(imp == NULL || strncmp(imp->target, "c.", 2) != 0 ||
       imp->target[2] == '\0')
        return 0;
    snprintf(dst, dst_size, "%s", imp->target + 2);
    return 1;
}

static void
extract_extern_signature(const KirImport *imp, char *ret, size_t ret_size,
                         char *args, size_t args_size)
{
    const char *sig = imp->signature;
    const char *op = strchr(sig, '(');
    const char *cl = op != NULL ? strchr(op, ')') : NULL;
    const char *arrow = cl != NULL ? strstr(cl, "->") : NULL;

    snprintf(ret, ret_size, "void");
    if(arrow != NULL) {
        const char *r = arrow + 2;
        size_t rn = 0;

        while(*r == ' ' || *r == '\t')
            r++;
        while(*r != '\0' && *r != '#' && rn + 1 < ret_size)
            ret[rn++] = *r++;
        while(rn > 0 && (ret[rn - 1] == ' ' || ret[rn - 1] == '\t'))
            rn--;
        ret[rn] = '\0';
    }
    if(op != NULL && cl != NULL && cl > op)
        snprintf(args, args_size, "%.*s", (int)(cl - op - 1), op + 1);
    else
        snprintf(args, args_size, "void");
}

static void
extern_call_args(const char *args, char *dst, size_t dst_size)
{
    const char *p = args;
    const char *start = args;
    int depth = 0;
    int first = 1;
    size_t n = 0;

    dst[0] = '\0';
    if(args == NULL || args[0] == '\0' || strcmp(args, "void") == 0)
        return;
    while(1) {
        if(*p == '(' || *p == '[' || *p == '{')
            depth++;
        else if(*p == ')' || *p == ']' || *p == '}')
            depth--;
        if((*p == ',' && depth == 0) || *p == '\0') {
            const char *colon = start;
            const char *name_start = start;
            const char *name_end;

            while(*name_start == ' ' || *name_start == '\t')
                name_start++;
            while(colon < p && *colon != ':')
                colon++;
            name_end = colon;
            while(name_end > name_start &&
                  (name_end[-1] == ' ' || name_end[-1] == '\t'))
                name_end--;
            if(colon < p && name_end > name_start) {
                n += (size_t)snprintf(dst + n, dst_size > n ? dst_size - n : 0,
                                      "%s%.*s", first ? "" : ", ",
                                      (int)(name_end - name_start), name_start);
                first = 0;
            }
            if(*p == '\0')
                break;
            start = p + 1;
        }
        p++;
    }
}

static void
emit_extern_prototype(FILE *c, const KirModule *m, const KirImport *imp)
{
    char ret[LOWER_NAME_MAX];
    char cargs[LOWER_TEXT_MAX];
    char conv[LOWER_TEXT_MAX];
    char symbol[LOWER_NAME_MAX];
    const char *cname = imp->name;

    extract_extern_signature(imp, ret, sizeof(ret), cargs, sizeof(cargs));
    convert_args(m, cargs, conv, sizeof(conv));
    if(c_extern_symbol(imp, symbol, sizeof(symbol))) {
        cname = symbol;
        fprintf(c, "%s %s(%s);\n", ret[0] ? ret : "void", cname, conv);
        if(strcmp(symbol, imp->name) != 0) {
            char call[LOWER_TEXT_MAX];

            extern_call_args(cargs, call, sizeof(call));
            fprintf(c, "static %s\n%s(%s)\n{\n",
                    ret[0] ? ret : "void", imp->name, conv);
            if(ret[0] != '\0' && strcmp(ret, "void") != 0)
                fprintf(c, "    return %s(%s);\n", symbol, call);
            else
                fprintf(c, "    %s(%s);\n", symbol, call);
            fprintf(c, "}\n");
        }
    } else {
        fprintf(c, "%s %s(%s);\n", ret[0] ? ret : "void", cname, conv);
    }
}

static void
lower_module(const KirModule *m, const K2cModuleSyms *restab, int restab_count, const char *out_dir)
{
    char stem[512];
    char guard[600];
    char hpath[1024];
    char cpath[1024];
    FILE *h;
    FILE *c;
    int i;

    stem_from_source(m->source_path, stem, sizeof(stem));
    guard_from_stem(stem, guard, sizeof(guard));
    snprintf(hpath, sizeof(hpath), "%s/%s.h", out_dir, stem);
    snprintf(cpath, sizeof(cpath), "%s/%s.c", out_dir, stem);
    mkdir_parent(hpath);

    /* --- header --- */
    h = fopen(hpath, "wb");
    if(h == NULL)
        return;
    fprintf(h, "/* Generated by k2c from %s. */\n", m->source_path);
    fprintf(h, "#ifndef %s\n#define %s\n\n#include <stdint.h>\n#include <stdbool.h>\n", guard, guard);
    for(i = 0; i < m->import_count; i++) {
        const KirImport *imp = &m->imports[i];

        if(!imp->required)
            continue;   /* '#private' imports go to the .c only */
        if(imp->kind == KIR_IMPORT_HEADER) {
            const char *dot = strrchr(imp->target, '.');
            const char *slash = strrchr(imp->target, '/');
            int has_ext = dot != NULL && (slash == NULL || dot > slash);

            /* Angled includes stay angled; extension-less targets get
             * the .h of their generated header. */
            emit_guard_open(h, imp->guard);
            if(strchr(imp->signature, '<') != NULL)
                fprintf(h, "#include <%s>\n", imp->target);
            else if(has_ext)
                fprintf(h, "#include \"%s\"\n", imp->target);
            else
                fprintf(h, "#include \"%s.h\"\n", imp->target);
            emit_guard_close(h, imp->guard);
        } else if(imp->kind == KIR_IMPORT_MODULE)
            fprintf(h, "#include \"%s.h\"\n", imp->target);
    }
    /* 'Name :: #define value' constants first: headers reference them in
     * array bounds and extern declarations, and other modules use them
     * through the generated header -- a .c-only emission starves those. */
    for(i = 0; i < m->define_count; i++) {
        const KirDefine *d = &m->defines[i];

        emit_guard_open(h, d->guard);
        fprintf(h, "#define %s %s\n", d->name, d->value);
        emit_guard_close(h, d->guard);
    }
    /* Typedefs and enums first (structs + globals reference them). */
    for(i = 0; i < m->type_count; i++) {
        const KirType *ty = &m->types[i];

        if(strcmp(ty->name, "#typedef") == 0) {
            emit_guard_open(h, ty->guard);
            fprintf(h, "\ntypedef %s;\n", ty->body);
            emit_guard_close(h, ty->guard);
        }
    }
    for(i = 0; i < m->type_count; i++) {
        const KirType *ty = &m->types[i];

        if(strcmp(ty->name, "#enum") != 0)
            continue;
        /* #enum { A, B } — newline-separated members need commas in C. */
        emit_guard_open(h, ty->guard);
        fprintf(h, "\nenum {\n");
        {
            const char *line = ty->body;

            while(line != NULL && *line != '\0') {
                const char *nl = strchr(line, '\n');
                size_t len = nl ? (size_t)(nl - line) : strlen(line);

                if(len > 0) {
                    char raw[LOWER_TEXT_MAX];

                    if(len >= sizeof(raw))
                        len = sizeof(raw) - 1;
                    memcpy(raw, line, len);
                    raw[len] = '\0';
                    /* strip trailing comma if present, then add one */
                    while(len > 0 && (raw[len - 1] == ' ' || raw[len - 1] == ','))
                        raw[--len] = '\0';
                    if(raw[0] != '\0')
                        fprintf(h, "    %s,\n", raw);
                }
                line = nl ? nl + 1 : NULL;
            }
        }
        fprintf(h, "};\n");
        emit_guard_close(h, ty->guard);
    }
    for(i = 0; i < m->type_count; i++) {
        const KirType *ty = &m->types[i];

        if(strcmp(ty->name, "#enum") == 0 ||
           strcmp(ty->name, "#typedef") == 0)
            continue;
        emit_guard_open(h, ty->guard);
        if(ty->is_enum) {
            /* 'Name :: enum { A, B }' — emit a named C enum. */
            fprintf(h, "\ntypedef enum {\n");
            {
                const char *line = ty->body;

                while(line != NULL && *line != '\0') {
                    const char *nl = strchr(line, '\n');
                    size_t len = nl ? (size_t)(nl - line) : strlen(line);

                    if(len > 0) {
                        char raw[LOWER_TEXT_MAX];

                        if(len >= sizeof(raw))
                            len = sizeof(raw) - 1;
                        memcpy(raw, line, len);
                        raw[len] = '\0';
                        while(len > 0 && (raw[len - 1] == ' ' ||
                                          raw[len - 1] == ','))
                            raw[--len] = '\0';
                        if(raw[0] != '\0')
                            fprintf(h, "    %s,\n", raw);
                    }
                    line = nl ? nl + 1 : NULL;
                }
            }
            fprintf(h, "} %s;\n", ty->name);
            continue;
        }
        fprintf(h, "\ntypedef struct {\n");
        /* Each body line is a field decl: 'name: [N] Type' / 'name: Type'. */
        {
            const char *line = ty->body;

            while(line != NULL && *line != '\0') {
                const char *nl = strchr(line, '\n');
                size_t len = nl ? (size_t)(nl - line) : strlen(line);
                char raw[LOWER_TEXT_MAX];
                char name[LOWER_NAME_MAX];
                char type[LOWER_TEXT_MAX];
                char base[LOWER_TEXT_MAX];
                char suffix[LOWER_NAME_MAX];
                const char *colon;

                if(len >= sizeof(raw))
                    len = sizeof(raw) - 1;
                memcpy(raw, line, len);
                raw[len] = '\0';
                colon = strchr(raw, ':');
                if(colon != NULL) {
                    const char *ty2 = colon + 1;
                    size_t nl2 = (size_t)(colon - raw);

                    while(*ty2 == ' ' || *ty2 == '\t')
                        ty2++;
                    if(nl2 >= sizeof(name))
                        nl2 = sizeof(name) - 1;
                    memcpy(name, raw, nl2);
                    name[nl2] = '\0';
                    snprintf(type, sizeof(type), "%s", ty2);
                    split_array_type(type, base, sizeof(base),
                                     suffix, sizeof(suffix));
                    {
                        char tmpb[LOWER_TEXT_MAX];

                        strip_alias_type(m, base, tmpb, sizeof(tmpb));
                        snprintf(base, sizeof(base), "%s", tmpb);
                    }
                    fprintf(h, "    %s %s%s;\n", base, name, suffix);
                }
                line = nl ? nl + 1 : NULL;
            }
        }
        fprintf(h, "} %s;\n", ty->name);
        emit_guard_close(h, ty->guard);
    }
    /* #global variables have external linkage: declare extern in the header,
     * after every named type they reference. 'static'/#private globals stay
     * in the .c. */
    for(i = 0; i < m->global_count; i++) {
        const KirGlobal *g = &m->globals[i];
        char base[LOWER_TEXT_MAX];
        char suffix[LOWER_NAME_MAX];

        if(g->is_static)
            continue;
        split_array_type(g->type, base, sizeof(base), suffix, sizeof(suffix));
        {
            char tmpb[LOWER_TEXT_MAX];

            strip_alias_type(m, base, tmpb, sizeof(tmpb));
            snprintf(base, sizeof(base), "%s", tmpb);
            if(suffix[0] != '\0') {
                char tmps[LOWER_NAME_MAX];

                rewrite_body2(m, NULL, 0, suffix, tmps, sizeof(tmps), NULL);
                snprintf(suffix, sizeof(suffix), "%s", tmps);
            }
        }
        emit_guard_open(h, g->guard);
        fprintf(h, "extern %s %s%s;\n", base, g->name, suffix);
        emit_guard_close(h, g->guard);
    }
    for(i = 0; i < m->function_count; i++) {
        const KirFunction *fn = &m->functions[i];
        char cname[LOWER_NAME_MAX];
        char cargs[LOWER_TEXT_MAX];
        char cret[LOWER_NAME_MAX];

        if(!fn->is_public)
            continue;   /* private functions are file-static */
        function_c_name(m, fn, cname, sizeof(cname));
        convert_args(m, fn->args, cargs, sizeof(cargs));
        strip_alias_type(m, fn->return_type, cret, sizeof(cret));
        emit_guard_open(h, fn->guard);
        fprintf(h, "%s %s(%s);\n",
                cret[0] ? cret : "void", cname, cargs);
        emit_guard_close(h, fn->guard);
    }
    fprintf(h, "\n#endif /* %s */\n", guard);
    fclose(h);

    /* --- source --- */
    c = fopen(cpath, "wb");
    if(c == NULL)
        return;
    fprintf(c, "/* Generated by k2c from %s. */\n", m->source_path);
    fprintf(c, "#include \"%s.h\"\n", stem);
    fprintf(c, "#include <stdio.h>\n");
    int needs_inspection = 0;
    for(int fi = 0; fi < m->function_count; fi++)
        if(!m->functions[fi].is_extern && !KirCanEmitBody(&m->functions[fi])) needs_inspection = 1;
    if(needs_inspection)
        fprintf(c, "#include \"ui_inspect.h\"\n");
    KirEmitNumbers(c, m, KIR_C);
    /* '#private' imports include here (implementation-only). */
    for(i = 0; i < m->import_count; i++) {
        const KirImport *imp = &m->imports[i];

        if(imp->required || imp->kind != KIR_IMPORT_HEADER)
            continue;
        emit_guard_open(c, imp->guard);
        if(strchr(imp->signature, '<') != NULL)
            fprintf(c, "#include <%s>\n", imp->target);
        else
            fprintf(c, "#include \"%s\"\n", imp->target);
        emit_guard_close(c, imp->guard);
    }
    fprintf(c, "\n#define KRYON_PRIVATE_UNUSED __attribute__((unused))\n");
    /* 'Name :: #define value' module constants. */
    for(i = 0; i < m->define_count; i++) {
        const KirDefine *d = &m->defines[i];

        emit_guard_open(c, d->guard);
        fprintf(c, "#define %s %s\n", d->name, d->value);
        emit_guard_close(c, d->guard);
    }
    for(i = 0; i < m->assert_count; i++)
        emit_compile_assert(c, &m->asserts[i]);
    /* '#intrinsic "web"' wrappers: static EM_ASM shims for web builds. */
    for(i = 0; i < m->import_count; i++) {
        const KirImport *imp = &m->imports[i];

        if(imp->kind != KIR_IMPORT_INTRINSIC)
            continue;
        emit_web_intrinsic_wrapper(c, m, imp);
    }
    /* #extern imports: emit C prototypes parsed from the raw signature
     * ('name :: (args) -> Ret #extern'). */
    for(i = 0; i < m->import_count; i++) {
        const KirImport *imp = &m->imports[i];

        if(imp->kind != KIR_IMPORT_EXTERN || imp->signature[0] == '\0')
            continue;
        emit_extern_prototype(c, m, imp);
    }
    /* Forward prototypes for private functions: initializers and earlier
     * definitions may reference them before their definition. */
    for(i = 0; i < m->function_count; i++) {
        const KirFunction *fn = &m->functions[i];
        char cname[LOWER_NAME_MAX];
        char cargs[LOWER_TEXT_MAX];
        char cret[LOWER_NAME_MAX];

        if(fn->is_public || fn->is_extern)
            continue;
        function_c_name(m, fn, cname, sizeof(cname));
        convert_args(m, fn->args, cargs, sizeof(cargs));
        strip_alias_type(m, fn->return_type, cret, sizeof(cret));
        emit_guard_open(c, fn->guard);
        fprintf(c, "static KRYON_PRIVATE_UNUSED %s %s(%s);\n",
                cret[0] ? cret : "void", cname, cargs);
        emit_guard_close(c, fn->guard);
    }
    for(i = 0; i < m->global_count; i++) {
        const KirGlobal *g = &m->globals[i];
        char base[LOWER_TEXT_MAX];
        char suffix[LOWER_NAME_MAX];

        split_array_type(g->type, base, sizeof(base), suffix, sizeof(suffix));
        {
            char tmpb[LOWER_TEXT_MAX];

            strip_alias_type(m, base, tmpb, sizeof(tmpb));
            snprintf(base, sizeof(base), "%s", tmpb);
            if(suffix[0] != '\0') {
                char tmps[LOWER_NAME_MAX];

                /* the alias sits inside brackets ('[state.MAX]'), so use the
                 * body rewriter (strips alias.member anywhere), not the
                 * leading-alias-only type strip */
                rewrite_body2(m, NULL, 0, suffix, tmps, sizeof(tmps), NULL);
                snprintf(suffix, sizeof(suffix), "%s", tmps);
            }
        }
        {
            char initw[LOWER_TEXT_MAX];

            /* initializers carry 'nil' and module-local function refs */
            rewrite_body2(m, NULL, 0, g->init, initw, sizeof(initw), NULL);
            emit_guard_open(c, g->guard);
            fprintf(c, "%s%s %s%s = %s;\n", g->is_static ? "static " : "",
                    base, g->name, suffix,
                    initw[0] ? initw : "{0}");
            emit_guard_close(c, g->guard);
        }
    }
    for(i = 0; i < m->state_count; i++) {
        const KirStateField *f = &m->state_fields[i];
        char base[LOWER_NAME_MAX];
        char suffix[LOWER_NAME_MAX];

        split_array_type(f->type, base, sizeof(base), suffix, sizeof(suffix));
        char mapped[LOWER_NAME_MAX], init[LOWER_TEXT_MAX];
        strip_alias_type(m,base,mapped,sizeof(mapped));
        if(!KirScalarLiteral(f->type,f->init[0]?f->init:"0",KIR_C,f->span,init,sizeof(init)))
            snprintf(init,sizeof(init),"%s",f->init[0]?f->init:"{0}");
        emit_guard_open(c, f->guard);
        fprintf(c, "static %s %s%s = %s;\n", mapped, f->name, suffix, init);
        emit_guard_close(c, f->guard);
    }
    for(i = 0; i < m->function_count; i++) {
        const KirFunction *fn = &m->functions[i];
        char cname[LOWER_NAME_MAX];
        char cargs[LOWER_TEXT_MAX];
        char cret[LOWER_NAME_MAX];

        function_c_name(m, fn, cname, sizeof(cname));
        convert_args(m, fn->args, cargs, sizeof(cargs));
        strip_alias_type(m, fn->return_type, cret, sizeof(cret));
        if(fn->is_extern) {
            /* extern: prototype only, no body */
            fprintf(c, "\n");
            emit_guard_open(c, fn->guard);
            fprintf(c, "%s %s(%s);\n",
                    cret[0] ? cret : "void", cname, cargs);
            emit_guard_close(c, fn->guard);
            continue;
        }
        fprintf(c, "\n");
        emit_guard_open(c, fn->guard);
        if(fn->is_public)
            fprintf(c, "%s\n%s(%s)\n{\n", cret[0] ? cret : "void",
                    cname, cargs);
        else
            fprintf(c, "static KRYON_PRIVATE_UNUSED %s\n%s(%s)\n{\n",
                    cret[0] ? cret : "void", cname, cargs);
        lower_body(c, m, restab, restab_count, fn);
        fprintf(c, "}\n");
        emit_guard_close(c, fn->guard);
    }
    fclose(c);
    k2c_plan9_rewrite_file(cpath, 0);
}

void
k2c_lower(const KirProgram *program, const char *root, const char *out_dir, const K2cModuleSyms *restab, int restab_count)
{
    int i;

    (void)root;
    if(program == NULL)
        return;
    for(i = 0; i < program->module_count; i++)
        lower_module(&program->modules[i], restab, restab_count, out_dir);
}

void
k2c_build_syms(const KirProgram *program, K2cModuleSyms *out)
{
    int i, j;

    memset(out, 0, sizeof(*out));
    if(program == NULL || program->module_count == 0)
        return;
    /* module slash path: the module name with dots -> slashes ("ide.state"
     * -> "ide/state"), matching how #import targets name modules. */
    {
        size_t n = 0;
        const char *p = program->modules[0].name;

        for(; *p != '\0' && n + 1 < sizeof(out->module_slash); p++)
            out->module_slash[n++] = (*p == '.') ? '/' : *p;
        out->module_slash[n] = '\0';
    }
    /* module stem: the source path minus '.kry' — imports may name the
     * file path ('src/screens/settings/settings_theme') instead of the
     * dotted module name. */
    {
        const char *sp = program->modules[0].source_path;
        size_t n = strlen(sp);

        if(n > 4 && strcmp(sp + n - 4, ".kry") == 0)
            n -= 4;
        if(n >= sizeof(out->module_stem))
            n = sizeof(out->module_stem) - 1;
        memcpy(out->module_stem, sp, n);
        out->module_stem[n] = '\0';
    }
    for(i = 0; i < program->module_count && out->fn_count < 256; i++) {
        const KirModule *m = &program->modules[i];

        for(j = 0; j < m->function_count && out->fn_count < 256; j++) {
            const KirFunction *fn = &m->functions[j];

            snprintf(out->fns[out->fn_count].kry,
                     sizeof(out->fns[0].kry), "%s", fn->name);
            function_c_name(m, fn, out->fns[out->fn_count].c,
                            sizeof(out->fns[0].c));
            out->fn_count++;
        }
    }
}
