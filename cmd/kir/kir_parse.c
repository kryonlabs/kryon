/*
 * kir_parse.c - shared Kir frontend: parse .kry source into a KirProgram.
 * Linked by k2ir (dump tool), k2c (C backend), and k2b (krb backend).
 */
#include "kir.h"
#include "kir_parse.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum {
    K2IR_PATH_MAX = 1024,
    K2IR_LINE_MAX = 1024
};

static void
die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "k2ir: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

static char *
trim(char *s)
{
    char *e;

    while(*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        s++;
    e = s + strlen(s);
    while(e > s && (e[-1] == ' ' || e[-1] == '\t' ||
                    e[-1] == '\r' || e[-1] == '\n'))
        *--e = '\0';
    return s;
}

static int
starts_word(const char *s, const char *word)
{
    size_t n = strlen(word);

    return strncmp(s, word, n) == 0 &&
           (s[n] == '\0' || s[n] == ' ' || s[n] == '\t' ||
            s[n] == '(' || s[n] == '"' || s[n] == '{');
}

static int
parse_symbol_before_colons(const char *s, char *out, size_t out_size)
{
    const char *p;
    const char *q;
    size_t n = 0;

    out[0] = '\0';
    p = strstr(s, "::");
    if(p == NULL)
        return 0;
    q = s;
    while(q < p && (*q == ' ' || *q == '\t'))
        q++;
    while(q < p && (isalnum((unsigned char)*q) || *q == '_') &&
          n + 1 < out_size)
        out[n++] = *q++;
    out[n] = '\0';
    while(q < p && (*q == ' ' || *q == '\t'))
        q++;
    return out[0] != '\0' && q == p;
}

static void
path_join(char *dst, size_t dst_size, const char *a, const char *b)
{
    if(a == NULL || a[0] == '\0')
        snprintf(dst, dst_size, "%s", b);
    else if(a[strlen(a) - 1] == '/')
        snprintf(dst, dst_size, "%s%s", a, b);
    else
        snprintf(dst, dst_size, "%s/%s", a, b);
}

static void
mkdir_parent(const char *path)
{
    char tmp[K2IR_PATH_MAX];
    size_t i;

    snprintf(tmp, sizeof(tmp), "%s", path);
    for(i = 1; i < strlen(tmp); i++) {
        if(tmp[i] != '/')
            continue;
        tmp[i] = '\0';
        if(mkdir(tmp, 0755) != 0 && errno != EEXIST)
            die("%s: mkdir failed: %s", tmp, strerror(errno));
        tmp[i] = '/';
    }
}

static const char *
relative_path(const char *root, const char *path)
{
    size_t n;

    if(root == NULL || root[0] == '\0')
        return path;
    n = strlen(root);
    if(strncmp(path, root, n) == 0 && (path[n] == '/' || path[n] == '\0')) {
        if(path[n] == '/')
            return path + n + 1;
        return path + n;
    }
    return path;
}

static void
strip_source_ext(char *dst, size_t dst_size, const char *path)
{
    size_t len;

    len = strlen(path);
    if(len > 4 && strcmp(path + len - 4, ".kry") == 0)
        len -= 4;
    if(len >= dst_size)
        len = dst_size - 1;
    memcpy(dst, path, len);
    dst[len] = '\0';
}

static int
parse_quoted(const char *s, char *out, size_t out_size)
{
    const char *q;
    size_t n = 0;

    q = strchr(s, '"');
    if(q == NULL)
        return 0;
    q++;
    while(*q != '\0' && *q != '"' && n + 1 < out_size)
        out[n++] = *q++;
    out[n] = '\0';
    return *q == '"';
}

static int
parse_angled(const char *s, char *out, size_t out_size)
{
    const char *q;
    size_t n = 0;

    q = strchr(s, '<');
    if(q == NULL)
        return 0;
    q++;
    while(*q != '\0' && *q != '>' && n + 1 < out_size)
        out[n++] = *q++;
    out[n] = '\0';
    return *q == '>';
}

static KirStmtKind
classify_stmt(const char *s)
{
    if(s[0] == '}')
        return KIR_STMT_BLOCK_CLOSE;
    if(strcmp(s, "{") == 0)
        return KIR_STMT_BLOCK_OPEN;
    if(starts_word(s, "if") || starts_word(s, "else"))
        return KIR_STMT_IF;
    if(starts_word(s, "while"))
        return KIR_STMT_WHILE;
    if(starts_word(s, "for"))
        return KIR_STMT_FOR;
    if(starts_word(s, "switch"))
        return KIR_STMT_SWITCH;
    if(starts_word(s, "case") || starts_word(s, "default"))
        return KIR_STMT_CASE;
    if(starts_word(s, "return"))
        return KIR_STMT_RETURN;
    if(strcmp(s, "break") == 0 || strcmp(s, "break;") == 0)
        return KIR_STMT_BREAK;
    if(strcmp(s, "continue") == 0 || strcmp(s, "continue;") == 0)
        return KIR_STMT_CONTINUE;
    if(starts_word(s, "goto"))
        return KIR_STMT_GOTO;
    if(starts_word(s, "defer"))
        return KIR_STMT_DEFER;
    if(starts_word(s, "unused"))
        return KIR_STMT_UNUSED;
    if(starts_word(s, "c"))
        return KIR_STMT_RAW;
    if(strstr(s, ":=") != NULL)
        return KIR_STMT_DECL;
    if(strstr(s, "=") != NULL)
        return KIR_STMT_ASSIGN;
    if(strchr(s, '(') != NULL || strchr(s, '+') != NULL ||
       strchr(s, '-') != NULL)
        return KIR_STMT_EXPR;
        return KIR_STMT_EXPR;
    return KIR_STMT_UNKNOWN;
}

static void
parse_state_field(KirModule *module, const char *path, int line_no, char *line)
{
    char *colon;
    char *eq;
    char *name;
    char *type;
    char *init;

    colon = strchr(line, ':');
    if(colon == NULL)
        return;
    *colon = '\0';
    name = trim(line);
    type = trim(colon + 1);
    init = "";
    eq = strchr(type, '=');
    if(eq != NULL) {
        *eq = '\0';
        init = trim(eq + 1);
    }
    KirModuleAddStateField(module, name, trim(type), init,
                           KirSpan(path, line_no, 1));
}

static void
parse_function_header(char *name, size_t name_size, char *args,
                      size_t args_size, char *ret, size_t ret_size,
                      const char *line)
{
    const char *p;
    const char *q;
    size_t n = 0;

    name[0] = '\0';
    args[0] = '\0';
    snprintf(ret, ret_size, "void");
    p = strstr(line, "::");
    if(p != NULL) {
        q = line;
        while(q < p && (*q == ' ' || *q == '\t'))
            q++;
        while(q < p && (isalnum((unsigned char)*q) || *q == '_') &&
              n + 1 < name_size)
            name[n++] = *q++;
        name[n] = '\0';
    } else if(starts_word(line, "screen") || starts_word(line, "preview") ||
              starts_word(line, "page") || starts_word(line, "scene") ||
              starts_word(line, "frame") || starts_word(line, "fn")) {
        p = strchr(line, ' ');
        if(p != NULL) {
            p = trim((char *)(void *)p);
            while((isalnum((unsigned char)*p) || *p == '_') &&
                  n + 1 < name_size)
                name[n++] = *p++;
            name[n] = '\0';
        }
    }
    p = strchr(line, '(');
    q = p == NULL ? NULL : strchr(p, ')');
    if(p != NULL && q != NULL && q > p) {
        n = (size_t)(q - p - 1);
        if(n >= args_size)
            n = args_size - 1;
        memcpy(args, p + 1, n);
        args[n] = '\0';
        /* Return type: after the closing ')', an optional '-> T' before any
         * trailing directive (#extern / #global / ...). */
        q++;
        while(*q == ' ' || *q == '\t')
            q++;
        if(q[0] == '-' && q[1] == '>') {
            q += 2;
            while(*q == ' ' || *q == '\t')
                q++;
            n = 0;
            while(*q != '\0' && *q != '#' && *q != '{' && n + 1 < ret_size)
                ret[n++] = *q++;
            while(n > 0 && (ret[n - 1] == ' ' || ret[n - 1] == '\t'))
                n--;
            ret[n] = '\0';
        }
    }
}

static int
parse_import_line(KirModule *module, const char *path, int line_no,
                  const char *line)
{
    const char *directive;
    char target[K2IR_PATH_MAX];
    char name[KIR_NAME_MAX];
    KirImportKind kind;

    directive = strstr(line, "#import");
    if(directive == NULL)
        return 0;
    target[0] = '\0';
    name[0] = '\0';
    if(!parse_quoted(directive, target, sizeof(target)) &&
       !parse_angled(directive, target, sizeof(target)))
        return 0;
    if(parse_symbol_before_colons(line, name, sizeof(name)))
        kind = KIR_IMPORT_MODULE;
    else {
        snprintf(name, sizeof(name), "%s", target);
        kind = KIR_IMPORT_HEADER;
    }
    KirModuleAddImport(module, kind, name, target, "", 1,
                       KirSpan(path, line_no, 1));
    return 1;
}

static int
parse_extern_line(KirModule *module, const char *path, int line_no,
                  const char *line)
{
    char name[KIR_NAME_MAX];

    if(strstr(line, "#extern") == NULL)
        return 0;
    if(!parse_symbol_before_colons(line, name, sizeof(name)))
        return 0;
    KirModuleAddImport(module, KIR_IMPORT_EXTERN, name, name, line, 1,
                       KirSpan(path, line_no, 1));
    return 1;
}

static int
looks_like_function_header(const char *line)
{
    char tmp[K2IR_LINE_MAX];
    char *p;
    char *body;

    if(starts_word(line, "screen") || starts_word(line, "preview") ||
       starts_word(line, "page") || starts_word(line, "scene") ||
       starts_word(line, "frame") || starts_word(line, "fn"))
        return 1;
    p = strstr(line, "::");
    if(p == NULL)
        return 0;
    snprintf(tmp, sizeof(tmp), "%s", p + 2);
    body = trim(tmp);
    if(starts_word(body, "#import") || starts_word(body, "#defined") ||
       starts_word(body, "#define") || starts_word(body, "struct") ||
       starts_word(body, "enum"))
        return 0;
    if(strstr(body, "#type") != NULL)
        return 0;
    return strchr(body, '(') != NULL;
}

KirProgram *
kir_parse_file(const char *path, const char *root)
{
    FILE *in;
    KirProgram *program;
    KirModule *module;
    KirFunction *fn = NULL;
    char line[K2IR_LINE_MAX];
    char module_name[KIR_NAME_MAX] = "main";
    char rel[K2IR_PATH_MAX];
    char stem[K2IR_PATH_MAX];
    char out_rel[K2IR_PATH_MAX];
    char out_path[K2IR_PATH_MAX];
    int line_no = 0;
    enum { TOP, APP, STATE, TYPE, ENUM, FUNCTION } mode = TOP;
    int depth = 0;
    char pending[K2IR_LINE_MAX * 4];
    pending[0] = '\0';
    int pending_len = 0;
    int paren_depth = 0;
    int bracket_depth = 0;
    int in_string = 0;
    int expr_brace = 0;

    in = fopen(path, "rb");
    if(in == NULL)
        die("%s: open failed: %s", path, strerror(errno));
    snprintf(rel, sizeof(rel), "%s", relative_path(root, path));
    program = KirProgramNew();
    if(program == NULL)
        die("out of memory");

    module = KirProgramAddModule(program, module_name, rel, KirSpan(rel, 1, 1));
    if(module == NULL)
        die("out of memory");

    while(fgets(line, sizeof(line), in) != NULL) {
        char raw[K2IR_LINE_MAX];
        const char *t;

        line_no++;
        snprintf(raw, sizeof(raw), "%s", line);
        {
            char *trimmed = trim(raw);

            if(trimmed[0] == '\0' || strncmp(trimmed, "//", 2) == 0) {
                if(pending_len == 0)
                    continue;
                continue;
            }
            if(pending_len > 0 && pending_len + 2 < (int)sizeof(pending)) {
                pending[pending_len++] = ' ';
                pending[pending_len] = '\0';
            }
            strncat(pending, trimmed, sizeof(pending) - pending_len - 1);
            pending_len = (int)strlen(pending);
            /* Decide whether braces at paren-depth 0 on this logical line are
             * block braces (control/headers open scopes) or expression braces
             * (compound literals / initializers continue the statement). */
            {
                int header_line = 0;
                char w0[16];
                size_t wl = 0;

                for(const char *w = pending;
                    *w != '\0' && (isalnum((unsigned char)*w) || *w == '_') &&
                    wl + 1 < sizeof(w0); w++)
                    w0[wl++] = *w;
                w0[wl] = '\0';
                header_line =
                    strcmp(w0, "if") == 0 || strcmp(w0, "else") == 0 ||
                    strcmp(w0, "while") == 0 || strcmp(w0, "for") == 0 ||
                    strcmp(w0, "switch") == 0 || strcmp(w0, "do") == 0 ||
                    strcmp(w0, "screen") == 0 || strcmp(w0, "preview") == 0 ||
                    strcmp(w0, "page") == 0 || strcmp(w0, "scene") == 0 ||
                    strcmp(w0, "frame") == 0 || strcmp(w0, "fn") == 0 ||
                    strcmp(w0, "struct") == 0 || strcmp(w0, "enum") == 0 ||
                    strcmp(w0, "state") == 0 || strcmp(w0, "app") == 0 ||
                    strstr(pending, " :: ") != NULL;
                for(const char *p = trimmed; *p != '\0'; p++) {
                    if(in_string) {
                        if(*p == '\\' && p[1] != '\0')
                            p++;
                        else if(*p == '"')
                            in_string = 0;
                    } else if(*p == '"') {
                        in_string = 1;
                    } else if(*p == '(') {
                        paren_depth++;
                    } else if(*p == ')') {
                        paren_depth--;
                    } else if(*p == '[') {
                        bracket_depth++;
                    } else if(*p == ']') {
                        bracket_depth--;
                    } else if(paren_depth == 0 && bracket_depth == 0) {
                        if(*p == '{') {
                            if(!header_line)
                                expr_brace++;
                        } else if(*p == '}') {
                            if(!header_line && expr_brace > 0)
                                expr_brace--;
                        }
                    }
                }
            }
            if(paren_depth > 0 || bracket_depth > 0 || in_string ||
               expr_brace > 0)
                continue;
        }
        t = pending;
        {
            static char logical[K2IR_LINE_MAX * 4];

            snprintf(logical, sizeof(logical), "%s", pending);
            t = logical;
        }
        pending[0] = '\0';
        pending_len = 0;
        if(mode == TOP && strncmp(t, "#module", 7) == 0) {
            if(parse_quoted(t, module_name, sizeof(module_name)))
                snprintf(module->name, sizeof(module->name), "%s", module_name);
        } else if(mode == TOP &&
                  (parse_import_line(module, rel, line_no, t) ||
                   parse_extern_line(module, rel, line_no, t))) {
            continue;
        } else if(mode == TOP && starts_word(t, "state") && strchr(t, '{') != NULL) {
            mode = STATE;
        } else if(mode == STATE) {
            if(t[0] == '}') {
                mode = TOP;
            } else {
                parse_state_field(module, rel, line_no, t);
            }
        } else if(mode == TOP && starts_word(t, "app") &&
                  strchr(t, '{') != NULL) {
            parse_quoted(t, module->app.title, sizeof(module->app.title));
            module->app.has_app = 1;
            module->app.width = 800;
            module->app.height = 600;
            module->app.fps = 60;
            mode = APP;
        } else if(mode == APP) {
            if(t[0] == '}') {
                mode = TOP;
            } else if(starts_word(t, "size")) {
                sscanf(t, "size %d %d",
                       &module->app.width, &module->app.height);
            } else if(starts_word(t, "fps")) {
                module->app.fps = atoi(t + 3);
            } else if(starts_word(t, "theme")) {
                char m2[32] = "";

                sscanf(t, "theme %127s %31s", module->app.theme, m2);
                module->app.dark_mode = strcmp(m2, "dark") == 0;
            } else if(starts_word(t, "font") && strstr(t, "examples")) {
                module->app.font_examples = 1;
            } else if(starts_word(t, "frame")) {
                sscanf(t, "frame %127s", module->app.frame);
            } else if(starts_word(t, "init")) {
                sscanf(t, "init %127s", module->app.init);
            } else if(starts_word(t, "scene")) {
                sscanf(t, "scene %127s", module->app.scene);
            } else if(starts_word(t, "shutdown")) {
                sscanf(t, "shutdown %127s", module->app.shutdown);
            }
        } else if(mode == TOP && looks_like_function_header(t)) {
            char name[KIR_NAME_MAX];
            char args[KIR_TEXT_MAX];
            char ret[KIR_NAME_MAX];
            int is_extern = strstr(t, "#extern") != NULL;
            int has_body = strchr(t, '{') != NULL;

            parse_function_header(name, sizeof(name), args, sizeof(args),
                                  ret, sizeof(ret), t);
            if(name[0] != '\0') {
                fn = KirModuleAddFunction(module, name, args, ret, 0,
                                          KirSpan(rel, line_no, 1));
                fn->is_extern = is_extern;
                if(has_body && !is_extern) {
                    mode = FUNCTION;
                    depth = 1;
                } else {
                    /* extern / body-less prototype: no body follows */
                    fn = NULL;
                }
            }
        } else if(mode == TOP && strstr(t, "::") != NULL &&
                  strstr(t, "#global") != NULL) {
            /* name :: Type #global — a module-level global variable. */
            char gname[KIR_NAME_MAX];
            char gtype[KIR_TEXT_MAX];
            const char *colon = strstr(t, "::");
            const char *ty = colon + 2;
            const char *hash = strstr(t, "#global");
            const char *eq = strstr(t, " = ");
            size_t nn = 0;

            while(t < colon && (isalnum((unsigned char)*t) || *t == '_') &&
                  nn + 1 < sizeof(gname))
                gname[nn++] = *t++;
            gname[nn] = '\0';
            while(*ty == ' ' || *ty == '\t')
                ty++;
            nn = 0;
            while(ty < hash && nn + 1 < sizeof(gtype))
                gtype[nn++] = *ty++;
            while(nn > 0 && (gtype[nn - 1] == ' ' || gtype[nn - 1] == '\t'))
                nn--;
            gtype[nn] = '\0';
            if(eq != NULL && eq < hash)
                KirModuleAddGlobal(module, gname, gtype, eq + 3,
                                   KirSpan(rel, line_no, 1));
            else
                KirModuleAddGlobal(module, gname, gtype, "",
                                   KirSpan(rel, line_no, 1));
        } else if(mode == TOP && strstr(t, "::") != NULL) {
            /* Name :: struct { ... } — capture the type body verbatim. */
            const char *colons = strstr(t, "::");
            const char *after = colons + 2;
            char tname[KIR_NAME_MAX];
            size_t tn = 0;

            while(after < after + strlen(after) &&
                  (*after == ' ' || *after == '\t'))
                after++;
            if(strncmp(after, "struct", 6) == 0 &&
               (after[6] == '\0' || after[6] == ' ' || after[6] == '{')) {
                const char *q = t;
                KirType *ty;

                while(q < colons && (isalnum((unsigned char)*q) || *q == '_') &&
                      tn + 1 < sizeof(tname))
                    tname[tn++] = *q++;
                tname[tn] = '\0';
                ty = KirModuleAddType(module, tname,
                                      KirSpan(rel, line_no, 1));
                if(ty != NULL)
                    mode = TYPE;
                fn = NULL;
            }
        } else if(mode == TOP && strncmp(t, "#enum", 5) == 0) {
            /* #enum { ... } — capture the constants as a type body. */
            KirType *ety = KirModuleAddType(module, "#enum",
                                            KirSpan(rel, line_no, 1));

            if(ety != NULL && strchr(t, '}') == NULL)
                mode = ENUM;
        } else if(mode == ENUM) {
            if(t[0] == '}') {
                mode = TOP;
            } else {
                KirType *ety = &module->types[module->type_count - 1];
                size_t used = strlen(ety->body);

                snprintf(ety->body + used, sizeof(ety->body) - used,
                         "%s\n", t);
            }
        } else if(mode == TYPE) {
            if(t[0] == '}') {
                mode = TOP;
            } else if(t[0] == '#') {
                /* comment inside a struct body — skip */
            } else {
                KirType *ty = &module->types[module->type_count - 1];
                size_t used = strlen(ty->body);

                snprintf(ty->body + used, sizeof(ty->body) - used, "%s\n", t);
            }
        } else if(mode == FUNCTION) {
            if(t[0] == '#') {
                /* comment inside a body — skip (directives are top-level) */
            } else if(t[0] == '}') {
                if(depth > 0)
                    depth--;
                if(depth == 0) {
                    mode = TOP;
                    fn = NULL;
                } else {
                    KirFunctionAddStmt(fn, KIR_STMT_BLOCK_CLOSE, t, "",
                                       KirSpan(rel, line_no, 1));
                }
            } else {
                KirStmtKind kind = classify_stmt(t);
                const char *widget = kind == KIR_STMT_EXPR ? t : "";
                int brace_delta = 0;

                /* Count NET block braces: only '{'/'}' at paren/bracket depth 0
                 * open/close blocks. Braces inside parens (compound literals
                 * like (Props){...}) are expression braces, not blocks. */
                {
                    int pd = 0;
                    int in_s = 0;

                    for(const char *p = t; *p != '\0'; p++) {
                        if(in_s) {
                            if(*p == '\\' && p[1] != '\0')
                                p++;
                            else if(*p == '"')
                                in_s = 0;
                        } else if(*p == '"') {
                            in_s = 1;
                        } else if(*p == '(' || *p == '[') {
                            pd++;
                        } else if(*p == ')' || *p == ']') {
                            if(pd > 0)
                                pd--;
                        } else if(pd == 0) {
                            if(*p == '{')
                                brace_delta++;
                            else if(*p == '}')
                                brace_delta--;
                        }
                    }
                }
                KirFunctionAddStmt(fn, kind, t, widget,
                                   KirSpan(rel, line_no, 1));
                depth += brace_delta;
                if(depth < 0)
                    depth = 0;
            }
        }
    }
    fclose(in);
    return program;
}
