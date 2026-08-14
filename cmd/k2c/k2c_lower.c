/*
 * k2c_lower.c - Kir → C backend. Lowers a KirProgram to .c/.h source files.
 * This is the only .kry→C pipeline: the legacy kc line-matcher is gone.
 */
#include "k2c_lower.h"
#include "kir.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LOWER_NAME_MAX 128
#define LOWER_TEXT_MAX 1024

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

/* Strip a trailing '{' (and whitespace) from a control header's condition. */
static void
strip_block_brace(char *s)
{
    size_t n = strlen(s);

    while(n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                    s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = '\0';
    if(n > 0 && s[n - 1] == '{') {
        s[--n] = '\0';
        while(n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t'))
            s[--n] = '\0';
    }
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

/* Copy text replacing bare `nil` tokens with `NULL` (rewrite_kry_expr's
 * essential rule; alias/module rewriting lands with module support). */
static void
rewrite_nil(const char *src, char *dst, size_t dst_size)
{
    size_t n = 0;

    for(const char *p = src; *p != '\0' && n + 5 < dst_size; p++) {
        if(strncmp(p, "nil", 3) == 0 &&
           (p == src || !isalnum((unsigned char)p[-1])) &&
           !isalnum((unsigned char)p[3]) && p[3] != '_') {
            dst[n++] = 'N';
            dst[n++] = 'U';
            dst[n++] = 'L';
            dst[n++] = 'L';
            p += 2;
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

    if(m->name[0] != '\0' && strcmp(m->name, "main") != 0) {
        for(const char *p = m->name; *p && n + 1 < sizeof(mod); p++)
            mod[n++] = (*p == '.') ? '_' : *p;
        mod[n] = '\0';
        snprintf(dst, dst_size, "%s_%s_kry_draw", mod, fn->name);
    } else {
        snprintf(dst, dst_size, "%s_kry_draw", fn->name);
    }
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
                    if(!first && n + 2 < dst_size)
                        dst[n++] = ',';
                    if(!first && n + 1 < dst_size)
                        dst[n++] = ' ';
                    n += (size_t)snprintf(dst + n, dst_size - n,
                                          "%s %s", type, name);
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

typedef struct {
    char stmt[LOWER_TEXT_MAX];   /* deferred statement text (no 'defer ') */
    int scope;                   /* scope depth at registration */
} DeferEntry;

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

static void
emit_call_wrap(FILE *c, const KirModule *m, int line, const char *text)
{
    char rw[LOWER_TEXT_MAX];

    rewrite_nil(text, rw, sizeof(rw));
    fprintf(c, "    PushUIInspectSource(\"%s\", %d);\n", m->source_path, line);
    fprintf(c, "    %s;\n", rw);
    fprintf(c, "    PopUIInspectSource();\n");
}

/* Lower one function body. Defer entries are tracked by scope and spliced
 * LIFO at block close / return. */
static void
lower_body(FILE *c, const KirModule *m, const KirFunction *fn)
{
    DeferEntry defers[64];
    int defer_count = 0;
    int scope_stack[64];
    int scope_top = 0;
    int indent = 1;
    int j;

    for(j = 0; j < fn->stmt_count; j++) {
        const KirStmt *st = &fn->stmts[j];
        char rw[LOWER_TEXT_MAX];

        rewrite_nil(st->text, rw, sizeof(rw));
        switch(st->kind) {
        case KIR_STMT_BLOCK_CLOSE: {
            int target = scope_top > 0 ? scope_stack[--scope_top] : 0;
            int skip_close = 0;

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
            /* fire this scope's defers LIFO */
            while(defer_count > 0 && defers[defer_count - 1].scope > target) {
                defer_count--;
                emit_indent(c, indent);
                fprintf(c, "%s;\n", defers[defer_count].stmt);
            }
            break;
        }
        case KIR_STMT_IF: {
            char cond[LOWER_TEXT_MAX];

            snprintf(cond, sizeof(cond), "%s", rw);
            strip_block_brace(cond);
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
            scope_stack[scope_top++] = scope_top;
            indent++;
            break;
        }
        case KIR_STMT_WHILE: {
            char cond[LOWER_TEXT_MAX];

            snprintf(cond, sizeof(cond), "%s", rw);
            strip_block_brace(cond);
            if(strncmp(cond, "while ", 6) == 0)
                memmove(cond, cond + 6, strlen(cond + 6) + 1);
            emit_indent(c, indent);
            fprintf(c, "while(%s) {\n", cond);
            scope_stack[scope_top++] = scope_top;
            indent++;
            break;
        }
        case KIR_STMT_FOR:
        case KIR_STMT_SWITCH: {
            char head[LOWER_TEXT_MAX];
            const char *kw = (st->kind == KIR_STMT_FOR) ? "for" : "switch";

            snprintf(head, sizeof(head), "%s", rw);
            strip_block_brace(head);
            if(strncmp(head, kw, strlen(kw)) == 0 && head[strlen(kw)] == ' ')
                memmove(head, head + strlen(kw) + 1,
                        strlen(head + strlen(kw) + 1) + 1);
            emit_indent(c, indent);
            fprintf(c, "%s(%s) {\n", kw, head);
            scope_stack[scope_top++] = scope_top;
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
            /* fire all defers (non-spending) */
            for(int d = defer_count - 1; d >= 0; d--) {
                emit_indent(c, indent);
                fprintf(c, "%s;\n", defers[d].stmt);
            }
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
        case KIR_STMT_DEFER: {
            const char *body = rw;

            while(*body == ' ')
                body++;
            if(strncmp(body, "defer ", 6) == 0)
                body += 6;
            if(defer_count < 64) {
                snprintf(defers[defer_count].stmt,
                         sizeof(defers[0].stmt), "%s", body);
                defers[defer_count].scope = scope_top;
                defer_count++;
            }
            break;
        }
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

                        if(tl >= sizeof(type))
                            tl = sizeof(type) - 1;
                        memcpy(type, ty, tl);
                        type[tl] = '\0';
                        emit_indent(c, indent);
                        fprintf(c, "%s %s = %s;\n", type, name, init);
                    } else {
                        snprintf(type, sizeof(type), "%s", ty);
                        /* strip trailing brace if typed on block opener */
                        {
                            char *br = strchr(type, '{');
                            if(br != NULL)
                                *br = '\0';
                        }
                        emit_indent(c, indent);
                        fprintf(c, "%s %s = {0};\n", type, name);
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
                emit_call_wrap(c, m, st->span.line, rw);
            else {
                emit_indent(c, indent);
                fprintf(c, "%s;\n", rw);
            }
            break;
        default:
            if(rw[0] != '\0') {
                emit_indent(c, indent);
                fprintf(c, "%s\n", rw);
            }
            break;
        }
    }
    /* function-end defers */
    while(defer_count > 0) {
        defer_count--;
        emit_indent(c, 1);
        fprintf(c, "%s;\n", defers[defer_count].stmt);
    }
}

static void
lower_module(const KirModule *m, const char *out_dir)
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
    fprintf(h, "#ifndef %s\n#define %s\n\n", guard, guard);
    for(i = 0; i < m->import_count; i++) {
        const KirImport *imp = &m->imports[i];

        if(imp->kind == KIR_IMPORT_HEADER)
            fprintf(h, "#include \"%s\"\n", imp->target);
    }
    for(i = 0; i < m->function_count; i++) {
        const KirFunction *fn = &m->functions[i];
        char cname[LOWER_NAME_MAX];
        char cargs[LOWER_TEXT_MAX];
        char cret[LOWER_NAME_MAX];

        function_c_name(m, fn, cname, sizeof(cname));
        convert_args(m, fn->args, cargs, sizeof(cargs));
        strip_alias_type(m, fn->return_type, cret, sizeof(cret));
        fprintf(h, "%s %s(%s);\n",
                cret[0] ? cret : "void", cname, cargs);
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
    fprintf(c, "#include \"ui_inspect.h\"\n");
    fprintf(c, "\n#define KRYON_PRIVATE_UNUSED __attribute__((unused))\n");
    for(i = 0; i < m->global_count; i++) {
        const KirGlobal *g = &m->globals[i];
        char base[LOWER_TEXT_MAX];
        char suffix[LOWER_NAME_MAX];

        split_array_type(g->type, base, sizeof(base), suffix, sizeof(suffix));
        strip_alias_type(m, base, base, sizeof(base));
        fprintf(c, "static %s %s%s = %s;\n", base, g->name, suffix,
                g->init[0] ? g->init : "{0}");
    }
    for(i = 0; i < m->state_count; i++) {
        const KirStateField *f = &m->state_fields[i];
        char base[LOWER_NAME_MAX];
        char suffix[LOWER_NAME_MAX];

        split_array_type(f->type, base, sizeof(base), suffix, sizeof(suffix));
        fprintf(c, "static %s %s%s = %s;\n", base, f->name, suffix,
                f->init[0] ? f->init : "{0}");
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
            fprintf(c, "\n%s %s(%s);\n",
                    cret[0] ? cret : "void", cname, cargs);
            continue;
        }
        fprintf(c, "\n%s\n%s(%s)\n{\n", cret[0] ? cret : "void",
                cname, cargs);
        lower_body(c, m, fn);
        fprintf(c, "}\n");
    }
    fclose(c);
}

void
k2c_lower(const KirProgram *program, const char *root, const char *out_dir)
{
    int i;

    if(program == NULL)
        return;
    for(i = 0; i < program->module_count; i++)
        lower_module(&program->modules[i], out_dir);
}
