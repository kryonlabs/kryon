/*
 * k2ir - emit a debuggable KIR text artifact from .kry source.
 *
 * This is the first standalone KIR tool. It intentionally captures the stable
 * top-level shape before the full shared frontend migration lands.
 */
#include "kir.h"

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
    if(strchr(s, '(') != NULL)
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
                      size_t args_size, const char *line)
{
    const char *p;
    const char *q;
    size_t n = 0;

    name[0] = '\0';
    args[0] = '\0';
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
    }
}

static void
emit_kir_file(const char *root, const char *out_dir, const char *path)
{
    FILE *in;
    FILE *out;
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
    enum { TOP, STATE, FUNCTION } mode = TOP;
    int depth = 0;

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
        char *t;

        line_no++;
        snprintf(raw, sizeof(raw), "%s", line);
        t = trim(raw);
        if(t[0] == '\0' || strncmp(t, "//", 2) == 0)
            continue;
        if(mode == TOP && strncmp(t, "#module", 7) == 0) {
            if(parse_quoted(t, module_name, sizeof(module_name)))
                snprintf(module->name, sizeof(module->name), "%s", module_name);
        } else if(mode == TOP && strncmp(t, "#import", 7) == 0) {
            char target[K2IR_PATH_MAX];
            char name[KIR_NAME_MAX];
            KirImportKind kind = KIR_IMPORT_HEADER;

            target[0] = '\0';
            name[0] = '\0';
            if(parse_quoted(t, target, sizeof(target)) ||
               parse_angled(t, target, sizeof(target))) {
                snprintf(name, sizeof(name), "%s", target);
                kind = strstr(t, "::") != NULL ? KIR_IMPORT_MODULE : KIR_IMPORT_HEADER;
            }
            KirModuleAddImport(module, kind, name, target, "", 1,
                               KirSpan(rel, line_no, 1));
        } else if(mode == TOP && starts_word(t, "state") && strchr(t, '{') != NULL) {
            mode = STATE;
        } else if(mode == STATE) {
            if(t[0] == '}') {
                mode = TOP;
            } else {
                parse_state_field(module, rel, line_no, t);
            }
        } else if(mode == TOP &&
                  (strstr(t, "::") != NULL || starts_word(t, "screen") ||
                   starts_word(t, "preview") || starts_word(t, "page") ||
                   starts_word(t, "scene") || starts_word(t, "frame") ||
                   starts_word(t, "fn"))) {
            char name[KIR_NAME_MAX];
            char args[KIR_TEXT_MAX];

            parse_function_header(name, sizeof(name), args, sizeof(args), t);
            if(name[0] != '\0') {
                fn = KirModuleAddFunction(module, name, args, "void", 0,
                                          KirSpan(rel, line_no, 1));
                mode = FUNCTION;
                depth = strchr(t, '{') != NULL ? 1 : 0;
            }
        } else if(mode == FUNCTION) {
            if(t[0] == '}') {
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

                KirFunctionAddStmt(fn, kind, t, widget,
                                   KirSpan(rel, line_no, 1));
                if(strchr(t, '{') != NULL)
                    depth++;
            }
        }
    }
    fclose(in);

    strip_source_ext(stem, sizeof(stem), rel);
    snprintf(out_rel, sizeof(out_rel), "%s.kir", stem);
    path_join(out_path, sizeof(out_path), out_dir, out_rel);
    mkdir_parent(out_path);
    out = fopen(out_path, "wb");
    if(out == NULL)
        die("%s: open failed: %s", out_path, strerror(errno));
    KirProgramDump(program, out);
    fclose(out);
    KirProgramFree(program);
}

static void
usage(void)
{
    fprintf(stderr, "usage: k2ir --root DIR -o DIR file.kry ...\n");
}

int
main(int argc, char **argv)
{
    const char *root = NULL;
    const char *out_dir = NULL;
    int i;

    for(i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if(strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_dir = argv[++i];
        } else if(argv[i][0] == '-') {
            usage();
            return 1;
        } else {
            break;
        }
    }
    if(root == NULL || out_dir == NULL || i >= argc) {
        usage();
        return 1;
    }
    for(; i < argc; i++)
        emit_kir_file(root, out_dir, argv[i]);
    return 0;
}
