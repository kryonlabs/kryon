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
            s[n] == '(' || s[n] == '"' || s[n] == '{' ||
            s[n] == ':');   /* 'default:' — label-style case keyword */
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

/* Net block braces: only '{'/'}' at paren/bracket depth 0 open/close
 * blocks. Braces inside parens (compound literals like (Props){...}) are
 * expression braces, not blocks. */
static int
net_block_braces(const char *s)
{
    int pd = 0;
    int in_s = 0;
    int delta = 0;

    for(const char *p = s; *p != '\0'; p++) {
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
                delta++;
            else if(*p == '}')
                delta--;
        }
    }
    return delta;
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
    if(starts_word(s, "guard"))
        return KIR_STMT_IF;   /* 'guard cond' lowers to if(cond) return */
    if(starts_word(s, "while"))
        return KIR_STMT_WHILE;
    if(starts_word(s, "for"))
        return KIR_STMT_FOR;
    if(starts_word(s, "switch"))
        return KIR_STMT_SWITCH;
    if(starts_word(s, "case") || starts_word(s, "default"))
        return KIR_STMT_CASE;
    if(strcmp(s, "default:") == 0)
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
    if(strstr(s, ":=") != NULL)
        return KIR_STMT_DECL;   /* ':=' wins over the raw 'c' prefix (a
                                   variable may be named 'c') */
    if(starts_word(s, "c") && s[1] != ':')
        return KIR_STMT_RAW;   /* 'c:' is a typed decl of a variable named c */
    if(strstr(s, "::") != NULL)
        return KIR_STMT_RAW;   /* nested '::' definitions stay raw */
    if(strstr(s, ": ") != NULL || strstr(s, ": [") != NULL) {
        /* typed decl only when an identifier precedes the colon */
        const char *c2 = strstr(s, ": ");

        if(c2 == NULL)
            c2 = strstr(s, ": [");
        if(c2 != NULL && c2 > s &&
           (isalpha((unsigned char)s[0]) || s[0] == '_')) {
            int ident_only = 1;

            for(const char *q = s; q < c2; q++)
                if(!(isalnum((unsigned char)*q) || *q == '_'))
                    ident_only = 0;
            if(ident_only)
                return KIR_STMT_DECL;  /* 'x: T' / 'x: [N] T' */
        }
    }
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
    int quoted;

    directive = strstr(line, "#import");
    if(directive == NULL)
        return 0;
    target[0] = '\0';
    name[0] = '\0';
    quoted = parse_quoted(directive, target, sizeof(target));
    if(!quoted && !parse_angled(directive, target, sizeof(target)))
        return 0;
    if(parse_symbol_before_colons(line, name, sizeof(name)))
        kind = KIR_IMPORT_MODULE;
    else {
        snprintf(name, sizeof(name), "%s", target);
        kind = KIR_IMPORT_HEADER;
    }
    /* Signature records the bracket style so backends can keep angled
     * includes angled ("<") instead of quoted. required=0 marks '#private'
     * (include in the .c only, not the header). */
    KirModuleAddImport(module, kind, name, target, quoted ? "" : "<",
                       strstr(line, "#private") == NULL,
                       KirSpan(path, line_no, 1));
    return 1;
}

static int
parse_extern_line(KirModule *module, const char *path, int line_no,
                  const char *line)
{
    char name[KIR_NAME_MAX];
    const char *intrinsic = strstr(line, "#intrinsic");

    if(intrinsic == NULL && strstr(line, "#extern") == NULL)
        return 0;
    if(!parse_symbol_before_colons(line, name, sizeof(name)))
        return 0;
    if(intrinsic != NULL) {
        /* 'name :: (args) -> int #intrinsic "web"' — lowered by k2c to a
         * static EM_ASM wrapper on web builds. Only the two known web
         * intrinsics exist, mirroring the legacy compiler. */
        const char *b = intrinsic + strlen("#intrinsic");
        char backend[KIR_NAME_MAX];
        char ret[KIR_NAME_MAX] = "";
        const char *arrow = strstr(line, "->");

        while(*b == ' ' || *b == '\t')
            b++;
        if(*b == '"') {
            size_t n = 0;

            b++;
            while(*b != '\0' && *b != '"' && n + 1 < sizeof(backend))
                backend[n++] = *b++;
            backend[n] = '\0';
        } else {
            snprintf(backend, sizeof(backend), "%s", b);
        }
        if(strcmp(backend, "web") != 0)
            die("%s:%d: unknown intrinsic backend '%s'", path, line_no,
                backend);
        if(arrow != NULL && arrow < intrinsic) {
            size_t n = 0;
            const char *r = arrow + 2;

            while(r < intrinsic && n + 1 < sizeof(ret)) {
                if(*r != ' ' && *r != '\t')
                    ret[n++] = *r;
                r++;
            }
            ret[n] = '\0';
        }
        if(strcmp(ret, "int") != 0)
            die("%s:%d: web intrinsic '%s' must return int", path, line_no,
                name);
        if(strcmp(name, "web_download_file") != 0 &&
           strcmp(name, "web_context_click_in_bounds") != 0)
            die("%s:%d: unknown web intrinsic '%s'", path, line_no, name);
    }
    KirModuleAddImport(module,
                       intrinsic != NULL ? KIR_IMPORT_INTRINSIC
                                         : KIR_IMPORT_EXTERN,
                       name, name, line, 1, KirSpan(path, line_no, 1));
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


/* ---- compile-time conditionals ------------------------------------------
 * '#if COND { ... } #else { ... }' regions keep the legacy model: top-level
 * captures inside a region are stamped with the expanded C preprocessor
 * condition and the emitter wraps each item in '#if cond / #endif'; the
 * condition's 'Name' constants ('WEB :: #defined(PLATFORM_WEB)') expand to
 * their expressions. Body-level regions lower to raw #if/#elif/#else/#endif
 * statements whose braces are consumed here. */

typedef struct {
    char names[16][KIR_NAME_MAX];
    char exprs[16][KIR_TEXT_MAX];
    int count;
} KirConsts;

typedef struct {
    char cond[KIR_TEXT_MAX];      /* active branch condition (C form) */
    char excluded[KIR_TEXT_MAX];  /* conditions handled by earlier branches */
    int braces;                   /* net '{' until the region's closing '}' */
} KirCondFrame;

static int
line_is_hash_else(const char *line)
{
    return strcmp(line, "} #else {") == 0 || strcmp(line, "#else {") == 0;
}

/* '#if COND {' / '#else_if COND {' (with optional leading '}'): strips the
 * trailing '{' — region braces are consumed, never emitted. Returns 1 for
 * '#if', 2 for '#else_if', 0 otherwise; *condition points into line. */
static int
parse_cond_start(char *line, char **condition)
{
    char *q = NULL;
    int kind = 0;
    size_t n;

    if(strncmp(line, "#if", 3) == 0 &&
       (line[3] == '\0' || isspace((unsigned char)line[3]))) {
        q = line + 3;
        kind = 1;
    } else if(strncmp(line, "#else_if", 8) == 0 &&
              (line[8] == '\0' || isspace((unsigned char)line[8]))) {
        q = line + 8;
        kind = 2;
    } else if(strncmp(line, "} #else_if", 10) == 0 &&
              (line[10] == '\0' || isspace((unsigned char)line[10]))) {
        q = line + 10;
        kind = 2;
    } else {
        return 0;
    }
    q = trim(q);
    n = strlen(q);
    if(n == 0 || q[n - 1] != '{')
        return 0;
    q[n - 1] = '\0';
    q = trim(q);
    if(q[0] == '\0')
        return 0;
    *condition = q;
    return kind;
}

static void
expand_compile_expr_depth(char *dst, size_t dst_size, const KirConsts *consts,
                          const char *src, int depth)
{
    size_t n = 0;
    int in_string = 0;
    int escaped = 0;
    int i;

    if(dst_size == 0)
        return;
    if(depth > 16) {
        dst[0] = '\0';
        return;
    }
    for(const char *p = src; p != NULL && *p != '\0' && n + 1 < dst_size;) {
        if(in_string) {
            dst[n++] = *p;
            if(escaped)
                escaped = 0;
            else if(*p == '\\')
                escaped = 1;
            else if(*p == '"')
                in_string = 0;
            p++;
            continue;
        }
        if(*p == '"') {
            in_string = 1;
            dst[n++] = *p++;
            continue;
        }
        if(*p == '#' && strncmp(p, "#defined", 8) == 0) {
            const char *word_end = p + 8;

            if(*word_end == '\0' || *word_end == '(' ||
               isspace((unsigned char)*word_end)) {
                if(n + 7 >= dst_size)
                    break;
                memcpy(dst + n, "defined", 7);
                n += 7;
                p += 8;
                continue;
            }
        }
        if(isalpha((unsigned char)*p) || *p == '_') {
            char ident[KIR_NAME_MAX];
            size_t il = 0;
            int found = 0;

            while(isalnum((unsigned char)*p) || *p == '_') {
                if(il + 1 < sizeof(ident))
                    ident[il++] = *p;
                p++;
            }
            ident[il] = '\0';
            for(i = 0; i < consts->count; i++) {
                if(strcmp(consts->names[i], ident) == 0) {
                    char expanded[KIR_TEXT_MAX];
                    int written;

                    expand_compile_expr_depth(expanded, sizeof(expanded),
                                              consts, consts->exprs[i],
                                              depth + 1);
                    written = snprintf(dst + n, dst_size - n, "(%s)", expanded);
                    if(written < 0)
                        written = 0;
                    if((size_t)written >= dst_size - n)
                        n = dst_size - 1;
                    else
                        n += (size_t)written;
                    found = 1;
                    break;
                }
            }
            if(!found) {
                if(n + il >= dst_size)
                    break;
                memcpy(dst + n, ident, il);
                n += il;
            }
            continue;
        }
        dst[n++] = *p++;
    }
    dst[n] = '\0';
}

static void
expand_compile_expr(char *dst, size_t dst_size, const KirConsts *consts,
                    const char *src)
{
    expand_compile_expr_depth(dst, dst_size, consts, src, 0);
}

static void
combine_active_guard(char *dst, size_t dst_size, const KirCondFrame *frames,
                     int count)
{
    int i;

    dst[0] = '\0';
    for(i = 0; i < count; i++) {
        if(dst[0] == '\0')
            snprintf(dst, dst_size, "%s", frames[i].cond);
        else
            snprintf(dst + strlen(dst), dst_size - strlen(dst), " && %s",
                     frames[i].cond);
    }
}

/* Returns 1 when the line is consumed by top-level conditional handling
 * ('#if'/'#else'/'#else_if' open or retarget a frame; the matching '}'
 * pops one). A plain line inside a region only settles the frame's brace
 * count and returns 0, so normal captures proceed — stamped with the
 * active guard by the caller. */
static int
cond_top_step(char *line, KirCondFrame *frames, int *count, char *guard,
              size_t guard_size, const KirConsts *consts, const char *path,
              int line_no)
{
    char *cnd = NULL;
    int ck = parse_cond_start(line, &cnd);
    KirCondFrame *fr;

    if(ck == 1) {
        char expanded[KIR_TEXT_MAX];

        if(*count >= 8)
            die("%s:%d: too many nested #if blocks", path, line_no);
        expand_compile_expr(expanded, sizeof(expanded), consts, cnd);
        fr = &frames[(*count)++];
        snprintf(fr->cond, sizeof(fr->cond), "%s", expanded);
        snprintf(fr->excluded, sizeof(fr->excluded), "%s", expanded);
        fr->braces = 1;
        combine_active_guard(guard, guard_size, frames, *count);
        return 1;
    }
    if(*count <= 0)
        return 0;
    fr = &frames[*count - 1];
    if(ck == 2) {
        char expanded[KIR_TEXT_MAX];
        char next[KIR_TEXT_MAX * 2];

        expand_compile_expr(expanded, sizeof(expanded), consts, cnd);
        snprintf(fr->cond, sizeof(fr->cond), "!(%s) && (%s)",
                 fr->excluded, expanded);
        snprintf(next, sizeof(next), "(%s) || (%s)", fr->excluded, expanded);
        snprintf(fr->excluded, sizeof(fr->excluded), "%s", next);
        fr->braces = 1;
        combine_active_guard(guard, guard_size, frames, *count);
        return 1;
    }
    if(line_is_hash_else(line)) {
        snprintf(fr->cond, sizeof(fr->cond), "!(%s)", fr->excluded);
        fr->braces = 1;
        combine_active_guard(guard, guard_size, frames, *count);
        return 1;
    }
    fr->braces += net_block_braces(line);
    if(fr->braces <= 0) {
        (*count)--;
        combine_active_guard(guard, guard_size, frames, *count);
        return 1;
    }
    return 0;
}

/* A sub-mode (state/app/type/enum/function) consumed exactly one net '{'
 * from the enclosing region — settle the frame count. */
static void
cond_frame_settle(KirCondFrame *frames, int count)
{
    if(count > 0)
        frames[count - 1].braces--;
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
    int enum_return = TOP;
    int depth = 0;
    char pending[K2IR_LINE_MAX * 4];
    pending[0] = '\0';
    char lookahead[K2IR_LINE_MAX];
    int have_look = 0;
    int pending_len = 0;
    int paren_depth = 0;
    int bracket_depth = 0;
    int in_string = 0;
    int expr_brace = 0;
    KirCondFrame tframes[8];
    int tframe_count = 0;
    KirConsts consts;
    char cur_guard[KIR_TEXT_MAX];
    int body_mdepth[8];
    int body_mcount = 0;

    memset(&consts, 0, sizeof(consts));
    cur_guard[0] = '\0';
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

    while(have_look || fgets(line, sizeof(line), in) != NULL) {
        char raw[K2IR_LINE_MAX];
        char *t;

        if(have_look) {
            snprintf(line, sizeof(line), "%s", lookahead);
            have_look = 0;
        }

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
                /* Keyword headers must be followed by ' ', '(' or '{':
                 * 'app->x = ...' / 'state.x' are member statements, not
                 * block headers (their compound-literal braces are
                 * expression braces). */
                {
                    char nc = pending[wl];

                    header_line =
                        pending[0] == '#' ||
                        strcmp(pending, "{") == 0 ||   /* bare scope-open */
                        /* 'name :: Type = {' carries an initializer, not a
                         * body: its braces are expression braces so the
                         * logical line continues until they balance. Header
                         * forms ('name :: struct {', 'f :: (args) {', typedefs,
                         * externs) never contain ' = '. */
                        (strstr(pending, " :: ") != NULL &&
                         strstr(pending, " = ") == NULL) ||
                        (nc != '\0' && nc != '-' && nc != '.' &&
                         (strchr(" ({", nc) != NULL || nc == ':') &&
                         (strcmp(w0, "if") == 0 ||
                          strcmp(w0, "else") == 0 ||
                          strcmp(w0, "while") == 0 ||
                          strcmp(w0, "for") == 0 ||
                          strcmp(w0, "switch") == 0 ||
                          strcmp(w0, "do") == 0 ||
                          strcmp(w0, "case") == 0 ||
                          strcmp(w0, "default") == 0 ||
                          strcmp(w0, "screen") == 0 ||
                          strcmp(w0, "preview") == 0 ||
                          strcmp(w0, "page") == 0 ||
                          strcmp(w0, "scene") == 0 ||
                          strcmp(w0, "frame") == 0 ||
                          strcmp(w0, "fn") == 0 ||
                          strcmp(w0, "struct") == 0 ||
                          strcmp(w0, "enum") == 0 ||
                          strcmp(w0, "state") == 0 ||
                          strcmp(w0, "app") == 0));
                }
                /* K&R "} else {" / "} else if (...) {": the leading '}' closes
                 * the if-body and the trailing '{' re-opens the else-body, so
                 * both braces are block braces even though the leading word
                 * extraction above saw only '}'. The same holds for chained
                 * regions: "} #else_if COND {" / "} #else {" / "} #if COND {"
                 * (parse_cond_start accepts the optional leading '}'), whose
                 * braces belong to the region, not the statement. */
                if(!header_line && pending[0] == '}') {
                    const char *eq = pending + 1;

                    while(*eq == ' ' || *eq == '\t')
                        eq++;
                    if(starts_word(eq, "else") ||
                       strncmp(eq, "#else", 5) == 0 ||
                       starts_word(eq, "#if ") ||
                       starts_word(eq, "#elif "))
                        header_line = 1;
                }
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
            /* Continuation: a line ending in a binary operator or comma
             * continues onto the next (legacy line_needs_continuation).
             * Exclude ++/-- (they end statements). */
            {
                size_t pl = (size_t)pending_len;
                char last;
                char prev;

                while(pl > 0 && (pending[pl - 1] == ' ' ||
                                 pending[pl - 1] == '\t'))
                    pl--;
                last = pl > 0 ? pending[pl - 1] : '\0';
                prev = pl > 1 ? pending[pl - 2] : '\0';
                /* Continuation operators: ',','=','%','/' always; '+','-','*',
                 * '<','>' only in binary position (prev is space — excludes
                 * 'char*','x++' handled below,'<stdlib.h>'); '&','|' when
                 * doubled ('&&','||') or space-preceded; ':' only with an
                 * open ternary ('?' pending) — 'case 1:' and goto labels
                 * ('fail:') end their statement. */
                if(last == ',' || last == '=' || last == '%' ||
                   last == '?' ||
                   (last == ':' && prev != ':' &&
                    strchr(pending, '?') != NULL) ||
                   (last == '/' && prev != '>'))
                    continue;
                if((last == '+' || last == '-' || last == '*' ||
                    last == '<' || last == '>') &&
                   (prev == ' ' || prev == '\t') &&
                   !(prev == last))
                    continue;
                if((last == '&' || last == '|') &&
                   (prev == last || prev == ' ' || prev == '\t'))
                    continue;
                /* Look ahead: a next line starting with a continuation
                 * token ('?' / ':' ternary branches, '.', ',', leading
                 * binary operators) continues this statement (legacy
                 * line_starts_continuation). */
                {
                    char la[K2IR_LINE_MAX];
                    int pend_str;

                    /* C adjacent-literal concatenation: a statement whose
                     * last token closes a string ("...") continues when the
                     * next line opens a new literal ("...") — otherwise each
                     * fragment becomes its own orphan expression statement. */
                    {
                        int pl2 = (int)strlen(pending);

                        while(pl2 > 0 && (pending[pl2 - 1] == ' ' ||
                                          pending[pl2 - 1] == '\t'))
                            pl2--;
                        pend_str = pl2 > 0 && pending[pl2 - 1] == '"';
                    }

                    /* Keep consuming lookahead lines while they continue this
                     * statement; the first non-continuation line is stashed
                     * for the next iteration (appending it blindly here is
                     * how block-closing '}'s used to get swallowed). */
                    while(fgets(la, sizeof(la), in) != NULL) {
                        const char *lt;
                        int cont;

                        /* trim in place: the lookahead is appended verbatim,
                         * and a raw fgets line would carry its '\n' into the
                         * joined statement text. */
                        lt = trim(la);
                        if(lt[0] == '\0') {
                            line_no++;   /* blank lookaheads still count */
                            continue;
                        }
                        cont =
                            *lt == '?' || (*lt == ':' && lt[1] != ':') ||
                            *lt == '.' || *lt == ',' || *lt == '+' ||
                            *lt == '/' || *lt == '%' ||
                            (*lt == '-' && lt[1] != '>') ||
                            ((*lt == '&' && lt[1] == '&') ||
                             (*lt == '|' && lt[1] == '|') ||
                             (*lt == '=' && lt[1] == '=') ||
                             (*lt == '!' && lt[1] == '=') ||
                             (*lt == '<' && lt[1] == '=') ||
                             (*lt == '>' && lt[1] == '=')) ||
                            (*lt == '"' && pend_str);
                        if(!cont) {
                            snprintf(lookahead, sizeof(lookahead), "%s", la);
                            have_look = 1;
                            break;
                        }
                        if(pending_len > 0 &&
                           pending_len + 2 < (int)sizeof(pending)) {
                            pending[pending_len++] = ' ';
                            pending[pending_len] = '\0';
                        }
                        strncat(pending, lt,
                                sizeof(pending) - pending_len - 1);
                        pending_len = (int)strlen(pending);
                        line_no++;
                        /* the joined statement now ends with whatever this
                         * fragment ended with */
                        {
                            int pl2 = pending_len;

                            while(pl2 > 0 && (pending[pl2 - 1] == ' ' ||
                                              pending[pl2 - 1] == '\t'))
                                pl2--;
                            pend_str = pl2 > 0 && pending[pl2 - 1] == '"';
                        }
                    }
                }
            }
        }
        t = pending;
        {
            static char logical[K2IR_LINE_MAX * 4];

            snprintf(logical, sizeof(logical), "%s", pending);
            t = logical;
        }
        pending[0] = '\0';
        pending_len = 0;
        if(mode == TOP &&
           cond_top_step(t, tframes, &tframe_count, cur_guard,
                         sizeof(cur_guard), &consts, rel, line_no)) {
            continue;
        } else if(mode == TOP && strncmp(t, "#module", 7) == 0) {
            if(parse_quoted(t, module_name, sizeof(module_name)))
                snprintf(module->name, sizeof(module->name), "%s", module_name);
        } else if(mode == TOP &&
                  (parse_import_line(module, rel, line_no, t) ||
                   parse_extern_line(module, rel, line_no, t))) {
            if(module->import_count > 0)
                snprintf(module->imports[module->import_count - 1].guard,
                         sizeof(module->imports[0].guard), "%s", cur_guard);
            continue;
        } else if(mode == TOP && starts_word(t, "state") && strchr(t, '{') != NULL) {
            mode = STATE;
        } else if(mode == STATE) {
            if(t[0] == '}') {
                mode = TOP;
                cond_frame_settle(tframes, tframe_count);
            } else {
                parse_state_field(module, rel, line_no, t);
                if(module->state_count > 0)
                    snprintf(module->state_fields[module->state_count - 1].guard,
                             sizeof(module->state_fields[0].guard), "%s",
                             cur_guard);
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
                cond_frame_settle(tframes, tframe_count);
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
                snprintf(fn->guard, sizeof(fn->guard), "%s", cur_guard);
                fn->is_extern = is_extern;
                fn->is_colon = strstr(t, "::") != NULL;
                /* '#export' on a colon function keeps the plain Kry name as
                 * the C symbol (legacy global_name) so handwritten C and
                 * JNI entry points can call it directly. */
                fn->exported = fn->is_colon && strstr(t, "#export") != NULL;
                /* Legacy rule: screen-keyword and colon functions are public
                 * (project routes); '#private' opts out. */
                fn->is_public = !is_extern &&
                                strstr(t, "#private") == NULL &&
                                (fn->is_colon ||
                                 starts_word(t, "screen") ||
                                 starts_word(t, "preview") ||
                                 starts_word(t, "page") ||
                                 starts_word(t, "frame") ||
                                 starts_word(t, "scene") ||
                                 starts_word(t, "fn"));
                if(has_body && !is_extern) {
                    mode = FUNCTION;
                    depth = 1;
                } else {
                    /* extern / body-less prototype: no body follows */
                    fn = NULL;
                }
            }
        } else if(mode == TOP && strncmp(t, "static ", 7) == 0 &&
                  strchr(t, ':') != NULL) {
            /* 'static name: T = init' — an internal-linkage global
             * (multi-line initializers arrive joined). */
            const char *rest = t + 7;
            const char *colon = strchr(rest, ':');
            const char *eq = strstr(rest, " = ");
            char gname[KIR_NAME_MAX];
            char gtype[KIR_TEXT_MAX];
            size_t nn = 0;

            while(rest < colon && (isalnum((unsigned char)*rest) ||
                   *rest == '_') && nn + 1 < sizeof(gname))
                gname[nn++] = *rest++;
            gname[nn] = '\0';
            nn = 0;
            {
                const char *ty = colon + 1;
                const char *end = eq != NULL ? eq : ty + strlen(ty);

                while(*ty == ' ' || *ty == '\t')
                    ty++;
                while(end > ty && (end[-1] == ' ' || end[-1] == '\t'))
                    end--;
                if((size_t)(end - ty) >= sizeof(gtype))
                    end = ty + sizeof(gtype) - 1;
                memcpy(gtype, ty, (size_t)(end - ty));
                gtype[end - ty] = '\0';
            }
            if(eq != NULL)
                KirModuleAddStatic(module, gname, gtype, eq + 3,
                                   KirSpan(rel, line_no, 1));
            else
                KirModuleAddStatic(module, gname, gtype, "",
                                   KirSpan(rel, line_no, 1));
            if(module->global_count > 0)
                snprintf(module->globals[module->global_count - 1].guard,
                         sizeof(module->globals[0].guard), "%s", cur_guard);
        } else if(mode == TOP && strstr(t, "::") != NULL &&
                  strstr(t, "#global") != NULL) {
            /* name :: Type #global — a module-level global variable.
             * 'name :: Type = init #global' carries the initializer between
             * ' = ' and the trailing directives; both type and init end
             * there, not at '#global'. */
            char gname[KIR_NAME_MAX];
            char gtype[KIR_TEXT_MAX];
            char ginit[KIR_TEXT_MAX];
            const char *colon = strstr(t, "::");
            const char *ty = colon + 2;
            const char *hash = strstr(t, "#global");
            const char *eq = strstr(t, " = ");
            const char *tyend = (eq != NULL && eq < hash) ? eq : hash;
            size_t nn = 0;

            while(t < colon && (isalnum((unsigned char)*t) || *t == '_') &&
                   nn + 1 < sizeof(gname))
                gname[nn++] = *t++;
            gname[nn] = '\0';
            while(*ty == ' ' || *ty == '\t')
                ty++;
            nn = 0;
            while(ty < tyend && nn + 1 < sizeof(gtype))
                gtype[nn++] = *ty++;
            while(nn > 0 && (gtype[nn - 1] == ' ' || gtype[nn - 1] == '\t'))
                nn--;
            gtype[nn] = '\0';
            nn = 0;
            if(eq != NULL && eq < hash) {
                const char *ib = eq + 3;
                const char *ie = hash;

                while(ib < ie && nn + 1 < sizeof(ginit))
                    ginit[nn++] = *ib++;
                while(nn > 0 && (ginit[nn - 1] == ' ' ||
                                 ginit[nn - 1] == '\t'))
                    nn--;
            }
            ginit[nn] = '\0';
            if(strstr(t, "#private") != NULL)
                KirModuleAddStatic(module, gname, gtype, ginit,
                                   KirSpan(rel, line_no, 1));
            else
                KirModuleAddGlobal(module, gname, gtype, ginit,
                                   KirSpan(rel, line_no, 1));
            if(module->global_count > 0)
                snprintf(module->globals[module->global_count - 1].guard,
                         sizeof(module->globals[0].guard), "%s", cur_guard);
        } else if(mode == TOP && strstr(t, "::") != NULL &&
                  strstr(t, "#type") != NULL) {
            /* 'Name :: C-type #type' — a typedef. Build the C declarator:
             * function-pointer types insert the name after '(*'; others
             * append ' NAME'. Must precede the struct catch-all below. */
            KirType *tty;
            const char *colons0 = strstr(t, "::");
            const char *tybegin = colons0 + 2;
            const char *hash = strstr(t, "#type");
            char tname[KIR_NAME_MAX];
            size_t tn = 0;
            const char *q = t;

            while(q < colons0 && (isalnum((unsigned char)*q) || *q == '_') &&
                  tn + 1 < sizeof(tname))
                tname[tn++] = *q++;
            tname[tn] = '\0';
            tty = KirModuleAddType(module, "#typedef",
                                   KirSpan(rel, line_no, 1));
            if(tty != NULL)
                snprintf(tty->guard, sizeof(tty->guard), "%s", cur_guard);
            if(tty != NULL && tname[0] != '\0') {
                char tytext[KIR_TEXT_MAX];
                size_t tl;
                const char *lp;

                while(tybegin < hash && (*tybegin == ' ' || *tybegin == '\t'))
                    tybegin++;
                tl = (size_t)(hash - tybegin);
                while(tl > 0 && (tybegin[tl - 1] == ' ' || tybegin[tl - 1] == '\t'))
                    tl--;
                if(tl >= sizeof(tytext))
                    tl = sizeof(tytext) - 1;
                memcpy(tytext, tybegin, tl);
                tytext[tl] = '\0';
                lp = strstr(tytext, "(*");
                if(lp != NULL) {
                    size_t off = (size_t)(lp - tytext) + 2;

                    snprintf(tty->body, sizeof(tty->body), "%.*s%s%s",
                             (int)off, tytext, tname, tytext + off);
                } else {
                    snprintf(tty->body, sizeof(tty->body), "%s %s",
                             tytext, tname);
                }
            }
        } else if(mode == TOP && strstr(t, "::") != NULL &&
                  strchr(t, '{') == NULL &&
                  !looks_like_function_header(t)) {
            /* 'Name :: expr' — a compile-time constant ('WEB :: #defined(X)'),
             * expanded inside '#if' conditions; never emitted as C. The
             * '#define' form ('TAG :: #define "X"') becomes a real C
             * #define instead. */
            const char *colons = strstr(t, "::");
            const char *expr = colons + 2;
            char cname[KIR_NAME_MAX];
            size_t cn = 0;
            const char *q = t;

            while(q < colons && (isalnum((unsigned char)*q) || *q == '_') &&
                  cn + 1 < sizeof(cname))
                cname[cn++] = *q++;
            cname[cn] = '\0';
            while(*expr == ' ' || *expr == '\t')
                expr++;
            if(cname[0] != '\0' && *expr != '\0') {
                if(starts_word(expr, "#define")) {
                    char value[KIR_TEXT_MAX];
                    KirDefine *def;

                    snprintf(value, sizeof(value), "%s", expr + 7);
                    def = KirModuleAddDefine(module, cname, trim(value),
                                             KirSpan(rel, line_no, 1));
                    if(def != NULL)
                        snprintf(def->guard, sizeof(def->guard), "%s",
                                 cur_guard);
                } else {
                    if(consts.count >= 16)
                        die("%s:%d: too many compile-time constants",
                            rel, line_no);
                    snprintf(consts.names[consts.count],
                             sizeof(consts.names[0]), "%s", cname);
                    snprintf(consts.exprs[consts.count],
                             sizeof(consts.exprs[0]), "%s", expr);
                    consts.count++;
                }
            }
        } else if(mode == TOP && strstr(t, "::") != NULL) {
            /* Name :: struct { ... } | Name :: enum { ... } — capture the
             * type body verbatim (enums emit as typedef enum). */
            const char *colons = strstr(t, "::");
            const char *after = colons + 2;
            char tname[KIR_NAME_MAX];
            size_t tn = 0;

            while(after < after + strlen(after) &&
                  (*after == ' ' || *after == '\t'))
                after++;
            if((strncmp(after, "struct", 6) == 0 &&
                (after[6] == '\0' || after[6] == ' ' || after[6] == '{')) ||
               (strncmp(after, "enum", 4) == 0 &&
                (after[4] == '\0' || after[4] == ' ' || after[4] == '{'))) {
                const char *q = t;
                KirType *ty;

                while(q < colons && (isalnum((unsigned char)*q) || *q == '_') &&
                      tn + 1 < sizeof(tname))
                    tname[tn++] = *q++;
                tname[tn] = '\0';
                ty = KirModuleAddType(module, tname,
                                      KirSpan(rel, line_no, 1));
                if(ty != NULL) {
                    ty->is_enum = strncmp(after, "enum", 4) == 0;
                    snprintf(ty->guard, sizeof(ty->guard), "%s", cur_guard);
                    mode = TYPE;
                }
                fn = NULL;
            }
        } else if((mode == TOP || mode == TYPE) &&
                  strncmp(t, "#enum", 5) == 0) {
            /* #enum { ... } — capture the constants as a type body. */
            KirType *ety = KirModuleAddType(module, "#enum",
                                            KirSpan(rel, line_no, 1));

            if(ety != NULL)
                snprintf(ety->guard, sizeof(ety->guard), "%s", cur_guard);
            (void)ety;
            enum_return = mode;
            if(strchr(t, '}') == NULL)
                mode = ENUM;
        } else if(mode == ENUM) {
            size_t tl = strlen(t);

            if(t[0] == '}') {
                mode = enum_return;
                cond_frame_settle(tframes, tframe_count);
            } else if(tl > 0 && t[tl - 1] == '}') {
                /* joined constants + closing brace on one line */
                KirType *ety = &module->types[module->type_count - 1];
                size_t used = strlen(ety->body);

                snprintf(ety->body + used, sizeof(ety->body) - used,
                         "%.*s\n", (int)(tl - 1), t);
                mode = enum_return;
                cond_frame_settle(tframes, tframe_count);
            } else {
                KirType *ety = &module->types[module->type_count - 1];
                size_t used = strlen(ety->body);

                snprintf(ety->body + used, sizeof(ety->body) - used,
                         "%s\n", t);
            }
        } else if(mode == TYPE) {
            if(t[0] == '}') {
                mode = TOP;
                cond_frame_settle(tframes, tframe_count);
            } else if(t[0] == '#') {
                /* comment inside a struct body — skip */
            } else {
                KirType *ty = &module->types[module->type_count - 1];
                size_t used = strlen(ty->body);

                snprintf(ty->body + used, sizeof(ty->body) - used, "%s\n", t);
            }
        } else if(mode == FUNCTION) {
            char *bcnd = NULL;
            int bck = parse_cond_start(t, &bcnd);

            if(strncmp(t, "args ", 5) == 0 && depth <= 1 &&
               fn != NULL && !fn->is_colon) {
                /* 'args <decl>' header directive: append parameters to the
                 * screen's signature ('args InbeApp *app'). */
                const char *extra = t + 5;

                if(fn->args[0] != '\0') {
                    size_t used = strlen(fn->args);

                    snprintf(fn->args + used, sizeof(fn->args) - used,
                             ", %s", extra);
                } else {
                    snprintf(fn->args, sizeof(fn->args), "%s", extra);
                }
            } else if(bck != 0 || line_is_hash_else(t)) {
                /* body-level '#if COND {' — the braces are consumed here;
                 * the region lowers to raw #if/#elif/#else/#endif lines.
                 * (bck was computed once above: parse_cond_start strips the
                 * trailing '{' in place, so re-parsing would misfire.) */
                char raw[KIR_TEXT_MAX];
                char expanded[KIR_TEXT_MAX];

                if(line_is_hash_else(t)) {
                    snprintf(raw, sizeof(raw), "#else");
                } else if(bck == 1) {
                    if(body_mcount >= 8)
                        die("%s:%d: too many nested #if blocks", rel, line_no);
                    body_mdepth[body_mcount++] = depth;
                    expand_compile_expr(expanded, sizeof(expanded), &consts,
                                        bcnd);
                    snprintf(raw, sizeof(raw), "#if %s", expanded);
                } else {
                    expand_compile_expr(expanded, sizeof(expanded), &consts,
                                        bcnd);
                    snprintf(raw, sizeof(raw), "#elif %s", expanded);
                }
                KirFunctionAddStmt(fn, KIR_STMT_RAW, raw, "",
                                   KirSpan(rel, line_no, 1));
            } else if(t[0] == '}' && body_mcount > 0 &&
                      depth == body_mdepth[body_mcount - 1]) {
                /* this '}' closes a body-level '#if' region, not a block */
                body_mcount--;
                KirFunctionAddStmt(fn, KIR_STMT_RAW, "#endif", "",
                                   KirSpan(rel, line_no, 1));
            } else if(t[0] == '#') {
                /* comment inside a body — skip (directives are top-level) */
            } else if(t[0] == '}') {
                /* K&R "} else {" / "} else if (...) {": the brace closes the
                 * if-body and the else re-opens a new one, so depth is net
                 * unchanged. Recorded as BLOCK_CLOSE plus an IF whose text
                 * starts with "else" — the k2c lowering emits the "} else"
                 * itself and suppresses the duplicate close. */
                const char *eq = t + 1;

                while(*eq == ' ' || *eq == '\t')
                    eq++;
                if(depth > 1 && starts_word(eq, "else")) {
                    KirFunctionAddStmt(fn, KIR_STMT_BLOCK_CLOSE, "}", "",
                                       KirSpan(rel, line_no, 1));
                    KirFunctionAddStmt(fn, KIR_STMT_IF, eq, "",
                                       KirSpan(rel, line_no, 1));
                } else {
                    if(depth > 0)
                        depth--;
                    if(depth == 0) {
                        mode = TOP;
                        cond_frame_settle(tframes, tframe_count);
                        fn = NULL;
                    } else {
                        KirFunctionAddStmt(fn, KIR_STMT_BLOCK_CLOSE, t, "",
                                           KirSpan(rel, line_no, 1));
                    }
                }
            } else {
                KirStmtKind kind = classify_stmt(t);
                const char *widget = kind == KIR_STMT_EXPR ? t : "";
                int brace_delta = net_block_braces(t);

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
