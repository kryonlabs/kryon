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
parse_route_header(const char *s, char *out, size_t out_size)
{
    const char *p;
    size_t n = 0;

    if(!starts_word(s, "route"))
        return 0;
    p = s + 5;
    while(*p == ' ' || *p == '\t')
        p++;
    while((isalnum((unsigned char)*p) || *p == '_') && n + 1 < out_size)
        out[n++] = *p++;
    out[n] = '\0';
    while(*p == ' ' || *p == '\t')
        p++;
    return out[0] != '\0' && strchr(p, '{') != NULL;
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

static int
is_c_ident(const char *s)
{
    if(s == NULL || s[0] == '\0')
        return 0;
    if(!(isalpha((unsigned char)s[0]) || s[0] == '_'))
        return 0;
    for(const char *p = s + 1; *p != '\0'; p++) {
        if(!(isalnum((unsigned char)*p) || *p == '_'))
            return 0;
    }
    return 1;
}

static KirExternKind
classify_extern_target(const char *target, char *symbol, size_t symbol_size,
                       const char *path, int line_no)
{
    const char *dot;
    const char *slash;

    symbol[0] = '\0';
    if(target == NULL || target[0] == '\0')
        return KIR_EXTERN_HOST;
    if(strncmp(target, "c.", 2) == 0) {
        if(!is_c_ident(target + 2))
            die("%s:%d: C extern target must be c.<symbol>", path, line_no);
        snprintf(symbol, symbol_size, "%s", target + 2);
        return KIR_EXTERN_C;
    }
    dot = strrchr(target, '.');
    slash = strrchr(target, '/');
    if(dot != NULL && slash != NULL && slash < dot)
        return KIR_EXTERN_GO;
    return KIR_EXTERN_HOST;
}

/* Net block braces: only '{'/'}' at paren/bracket depth 0 open/close
 * blocks. Braces inside parens (compound literals like (Props){...}) are
 * expression braces, not blocks. */
static int
net_block_braces(const char *s)
{
    int pd = 0;
    int in_s = 0;
    int in_c = 0;
    int delta = 0;

    for(const char *p = s; *p != '\0'; p++) {
        if(in_s) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == '"')
                in_s = 0;
        } else if(in_c) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == '\'')
                in_c = 0;
        } else if(*p == '"') {
            in_s = 1;
        } else if(*p == '\'') {
            in_c = 1;
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
    /* goto label: a bare 'name:' / 'name: ;' — a following type marks a
     * declaration ('scalar: int = 5'), not a label */
    {
        size_t n = 0;

        while(isalnum((unsigned char)s[n]) || s[n] == '_')
            n++;
        if(n > 0 && s[n] == ':') {
            const char *rest = s + n + 1;

            while(*rest == ' ' || *rest == '\t')
                rest++;
            if(*rest == '\0' || strcmp(rest, ";") == 0)
                return KIR_STMT_LABEL;
        }
    }
    if(starts_word(s, "defer"))
        return KIR_STMT_DEFER;
    if(starts_word(s, "unused"))
        return KIR_STMT_UNUSED;
    if(strstr(s, ":=") != NULL)
        return KIR_STMT_DECL;   /* ':=' wins over the raw 'c' prefix (a
                                   variable may be named 'c') */
    if(starts_word(s, "c") && s[1] != ':') {
        /* 'c <raw C line>' glue — but 'c = ...' / 'c += ...' is an
         * assignment to a local named c, which needs the normal statement
         * path (raw lines emit without a trailing semicolon). 'c:' is a
         * typed decl of a variable named c. */
        const char *rest = s + 1;

        while(*rest == ' ' || *rest == '\t')
            rest++;
        if(*rest != '=' && strncmp(rest, "+=", 2) != 0 &&
           strncmp(rest, "-=", 2) != 0 && strncmp(rest, "*=", 2) != 0 &&
           strncmp(rest, "/=", 2) != 0 && strncmp(rest, "%=", 2) != 0 &&
           strncmp(rest, "&=", 2) != 0 && strncmp(rest, "|=", 2) != 0 &&
           strncmp(rest, "^=", 2) != 0 && strncmp(rest, "<<=", 3) != 0 &&
           strncmp(rest, ">>=", 3) != 0)
            return KIR_STMT_RAW;
    }
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
    /* C-style locals remain accepted at the language boundary. Lowerers
     * already treat declaration statements without ':' as an opaque typed
     * declaration and add the target terminator. */
    {
        static const char *const types[] = {
            "int ", "unsigned ", "long ", "float ", "double ",
            "char ", "bool ", "const ", "struct ", NULL
        };
        int i;

        for(i = 0; types[i] != NULL; i++)
            if(strncmp(s, types[i], strlen(types[i])) == 0)
                return KIR_STMT_DECL;
    }
    /* An '=' inside a call's compound literal is a designated initializer,
     * not an assignment statement (TextField((Props){.text = value})). */
    {
        int depth = 0;
        int quote = 0;
        const char *p;

        for(p = s; *p != '\0'; p++) {
            if(quote) {
                if(*p == '\\' && p[1] != '\0')
                    p++;
                else if(*p == quote)
                    quote = 0;
                continue;
            }
            if(*p == '"' || *p == '\'')
                quote = *p;
            else if(*p == '(' || *p == '[' || *p == '{')
                depth++;
            else if(*p == ')' || *p == ']' || *p == '}') {
                if(depth > 0)
                    depth--;
            } else if(*p == '=' && depth == 0)
                return KIR_STMT_ASSIGN;
        }
    }
    if(strchr(s, '(') != NULL || strchr(s, '+') != NULL ||
       strchr(s, '-') != NULL)
        return KIR_STMT_EXPR;
    return KIR_STMT_UNKNOWN;
}

static int
parse_widget_statement(const char *text, char *name, size_t name_size,
                       char *args, size_t args_size)
{
    static const char *const widgets[] = {
        "Background", "Text", "TextInRect", "Paragraph", "TextLines",
        "Rect", "Line", "Bevel", "IconTexture", "Picture", "Button",
        "IconButton", "Href", "TextField", "Dropdown", "Slider",
        "Toggle", "Checkbox", "Progress", "Screen", "Column", "Row", "Stack",
        "End", "Scroll", "Canvas", "Modal", "TitleBar", "TabBar",
        "BottomNav", "TopNav", "Toolbar"
    };
    const char *p = text;
    const char *open;
    const char *close;
    size_t length;
    size_t i;
    int known = 0;
    int depth = 0;
    int in_string = 0;

    while(*p == ' ' || *p == '\t')
        p++;
    open = p;
    while(isalnum((unsigned char)*p) || *p == '_')
        p++;
    length = (size_t)(p - open);
    if(length == 0 || length >= name_size)
        return 0;
    memcpy(name, open, length);
    name[length] = '\0';
    for(i = 0; i < sizeof(widgets) / sizeof(widgets[0]); i++)
        if(strcmp(name, widgets[i]) == 0) {
            known = 1;
            break;
        }
    if(!known)
        return 0;
    while(*p == ' ' || *p == '\t')
        p++;
    if(*p != '(')
        return 0;
    open = p++;
    close = NULL;
    depth = 1;
    while(*p != '\0') {
        if(in_string) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == '"')
                in_string = 0;
        } else if(*p == '"') {
            in_string = 1;
        } else if(*p == '(') {
            depth++;
        } else if(*p == ')' && --depth == 0) {
            close = p;
            break;
        }
        p++;
    }
    if(close == NULL || (size_t)(close - open) >= args_size)
        return 0;
    p = close + 1;
    while(*p == ' ' || *p == '\t' || *p == ';')
        p++;
    if(*p != '\0')
        return 0;
    memcpy(args, open + 1, (size_t)(close - open - 1));
    args[close - open - 1] = '\0';
    return 1;
}

typedef struct UiBlock {
    char widget[KIR_NAME_MAX];
    char name[KIR_NAME_MAX];
    char path[KIR_TEXT_MAX];
    char props[KIR_TEXT_MAX];
    int close_depth;
    int opened;
    int prop_count;
} UiBlock;

static int
is_layout_widget(const char *name)
{
    return strcmp(name, "Screen") == 0 || strcmp(name, "Column") == 0 ||
           strcmp(name, "Row") == 0 || strcmp(name, "Stack") == 0;
}

static int
parse_ui_block_header(const char *text, char *widget, size_t widget_size,
                      char *name, size_t name_size)
{
    const char *p = text;
    const char *start;
    size_t n = 0;

    while(*p == ' ' || *p == '\t')
        p++;
    start = p;
    while(isalnum((unsigned char)*p) || *p == '_')
        p++;
    n = (size_t)(p - start);
    if(n == 0 || n >= widget_size)
        return 0;
    memcpy(widget, start, n);
    widget[n] = '\0';
    if(!is_layout_widget(widget))
        return 0;
    while(*p == ' ' || *p == '\t')
        p++;
    start = p;
    n = 0;
    while(isalnum((unsigned char)*p) || *p == '_') {
        if(n + 1 < name_size)
            name[n++] = *p;
        p++;
    }
    name[n] = '\0';
    if(name[0] == '\0')
        return 0;
    while(*p == ' ' || *p == '\t')
        p++;
    if(*p == ':')
        p++;
    while(*p == ' ' || *p == '\t')
        p++;
    return p[0] == '{' && p[1] == '\0';
}

static int
parse_ui_prop_line(char *text, char *field, size_t field_size,
                   char *value, size_t value_size)
{
    char *eq;
    char *name;
    char *expr;
    size_t n;

    eq = strchr(text, '=');
    if(eq == NULL || strstr(text, "==") != NULL || strstr(text, "!=") != NULL ||
       strstr(text, "<=") != NULL || strstr(text, ">=") != NULL)
        return 0;
    *eq = '\0';
    name = trim(text);
    expr = trim(eq + 1);
    if(name[0] == '\0' || expr[0] == '\0')
        return 0;
    for(const char *p = name; *p != '\0'; p++)
        if(!isalnum((unsigned char)*p) && *p != '_')
            return 0;
    n = strlen(expr);
    if(n > 0 && expr[n - 1] == ';') {
        expr[n - 1] = '\0';
        expr = trim(expr);
    }
    snprintf(field, field_size, "%s", name);
    snprintf(value, value_size, "%s", expr);
    return 1;
}

static void
ui_block_append_prop(UiBlock *block, const char *field, const char *value)
{
    size_t used = strlen(block->props);

    if(used + strlen(field) + strlen(value) + 8 >= sizeof(block->props))
        return;
    snprintf(block->props + used, sizeof(block->props) - used, ".%.120s = %.3000s, ",
             field, value);
    block->prop_count++;
}

static const char *
ui_block_prop_type(const char *widget)
{
    if(strcmp(widget, "Row") == 0)
        return "RowProps";
    return "ColumnProps";
}

static void
ui_block_open(KirFunction *fn, UiBlock *block, KirSourceSpan span)
{
    char call[KIR_TEXT_MAX];
    char args[KIR_TEXT_MAX];
    const char *prop_type;
    int has_key;

    if(block == NULL || block->opened)
        return;
    prop_type = ui_block_prop_type(block->widget);
    has_key = strstr(block->props, ".key") != NULL ||
              strstr(block->props, ".Key") != NULL;
    if(has_key)
        snprintf(args, sizeof(args), "(%s){%.3800s}", prop_type, block->props);
    else
        snprintf(args, sizeof(args),
                 "(%s){%.3000s.key = Key(\"%.900s\")}",
                 prop_type, block->props, block->path);
    snprintf(call, sizeof(call), "%.120s(%.3900s)", block->widget, args);
    KirFunctionAddWidget(fn, block->widget, args, call, span);
    block->opened = 1;
}

static void
copy_trim_expr(char *dst, size_t dst_size, const char *src)
{
    char tmp[KIR_TEXT_MAX];
    char *t;

    snprintf(tmp, sizeof(tmp), "%s", src != NULL ? src : "");
    t = trim(tmp);
    snprintf(dst, dst_size, "%s", t);
}

static int
is_simple_ident(const char *s)
{
    if(!(isalpha((unsigned char)s[0]) || s[0] == '_'))
        return 0;
    for(const char *p = s + 1; *p != '\0'; p++)
        if(!(isalnum((unsigned char)*p) || *p == '_'))
            return 0;
    return 1;
}

static int
is_int_literal_text(const char *s)
{
    char *end;

    if(!isdigit((unsigned char)s[0]))
        return 0;
    (void)strtol(s, &end, 0);
    if(end == s)
        return 0;
    while(*end != '\0') {
        if(*end != 'u' && *end != 'U' && *end != 'l' && *end != 'L')
            return 0;
        end++;
    }
    return 1;
}

static int
find_matching_close(const char *s, int open_pos)
{
    int depth = 0;
    int in_string = 0;
    int in_char = 0;

    for(int i = open_pos; s[i] != '\0'; i++) {
        if(in_string) {
            if(s[i] == '\\' && s[i + 1] != '\0')
                i++;
            else if(s[i] == '"')
                in_string = 0;
        } else if(in_char) {
            if(s[i] == '\\' && s[i + 1] != '\0')
                i++;
            else if(s[i] == '\'')
                in_char = 0;
        } else if(s[i] == '"') {
            in_string = 1;
        } else if(s[i] == '\'') {
            in_char = 1;
        } else if(s[i] == '(') {
            depth++;
        } else if(s[i] == ')') {
            depth--;
            if(depth == 0)
                return i;
        }
    }
    return -1;
}

static int
find_top_op(const char *s, const char *const *ops, int op_count,
            const char **op_out)
{
    int depth = 0;
    int in_string = 0;
    int in_char = 0;
    int last = -1;
    const char *last_op = NULL;

    for(int i = 0; s[i] != '\0'; i++) {
        if(in_string) {
            if(s[i] == '\\' && s[i + 1] != '\0')
                i++;
            else if(s[i] == '"')
                in_string = 0;
        } else if(in_char) {
            if(s[i] == '\\' && s[i + 1] != '\0')
                i++;
            else if(s[i] == '\'')
                in_char = 0;
        } else if(s[i] == '"') {
            in_string = 1;
        } else if(s[i] == '\'') {
            in_char = 1;
        } else if(s[i] == '(' || s[i] == '[' || s[i] == '{') {
            depth++;
        } else if(s[i] == ')' || s[i] == ']' || s[i] == '}') {
            if(depth > 0)
                depth--;
        } else if(depth == 0) {
            for(int op = 0; op < op_count; op++) {
                size_t len = strlen(ops[op]);

                if(strncmp(s + i, ops[op], len) == 0) {
                    if((ops[op][0] == '+' || ops[op][0] == '-') &&
                       (i == 0 || strchr("(!=<>+-*/%,", s[i - 1]) != NULL))
                        continue;
                    last = i;
                    last_op = ops[op];
                }
            }
        }
    }
    if(last >= 0 && op_out != NULL)
        *op_out = last_op;
    return last;
}

static int parse_expr_to_kir(KirFunction *fn, const char *text,
                             KirSourceSpan span);

static void
append_call_arg(KirFunction *fn, int call_index, int child)
{
    int *slot;

    if(fn == NULL || call_index < 0 || call_index >= fn->expr_count ||
       child < 0)
        return;
    slot = &fn->exprs[call_index].first_child;
    while(*slot >= 0 && *slot < fn->expr_count)
        slot = &fn->exprs[*slot].next_sibling;
    *slot = child;
}

static int
parse_expr_to_kir(KirFunction *fn, const char *text, KirSourceSpan span)
{
    static const char *const or_ops[] = { "||" };
    static const char *const and_ops[] = { "&&" };
    static const char *const eq_ops[] = { "==", "!=" };
    static const char *const rel_ops[] = { "<=", ">=", "<", ">" };
    static const char *const add_ops[] = { "+", "-" };
    static const char *const mul_ops[] = { "*", "/", "%" };
    const char *op = NULL;
    const char *expr_ops[6][4];
    int expr_counts[6] = { 1, 1, 2, 4, 2, 3 };
    char s[KIR_TEXT_MAX];
    KirExpr *expr;
    int pos = -1;

    copy_trim_expr(s, sizeof(s), text);
    if(s[0] == '\0')
        return -1;
    expr_ops[0][0] = or_ops[0];
    expr_ops[1][0] = and_ops[0];
    expr_ops[2][0] = eq_ops[0];
    expr_ops[2][1] = eq_ops[1];
    expr_ops[3][0] = rel_ops[0];
    expr_ops[3][1] = rel_ops[1];
    expr_ops[3][2] = rel_ops[2];
    expr_ops[3][3] = rel_ops[3];
    expr_ops[4][0] = add_ops[0];
    expr_ops[4][1] = add_ops[1];
    expr_ops[5][0] = mul_ops[0];
    expr_ops[5][1] = mul_ops[1];
    expr_ops[5][2] = mul_ops[2];
    for(int group = 0; group < 6; group++) {
        pos = find_top_op(s, expr_ops[group], expr_counts[group], &op);
        if(pos >= 0)
            break;
    }
    if(pos > 0 && op != NULL) {
        char left[KIR_TEXT_MAX];
        char right[KIR_TEXT_MAX];
        int idx;

        snprintf(left, sizeof(left), "%.*s", pos, s);
        snprintf(right, sizeof(right), "%s", s + pos + strlen(op));
        expr = KirFunctionAddExpr(fn, KIR_EXPR_BINARY, s, span);
        if(expr == NULL)
            return -1;
        idx = fn->expr_count - 1;
        snprintf(expr->op, sizeof(expr->op), "%s", op);
        {
            int left_idx = parse_expr_to_kir(fn, left, span);
            int right_idx = parse_expr_to_kir(fn, right, span);

            if(idx >= 0 && idx < fn->expr_count) {
                fn->exprs[idx].left = left_idx;
                fn->exprs[idx].right = right_idx;
            }
        }
        return idx;
    }
    {
        char *open = strchr(s, '(');

        if(open != NULL && open > s) {
            int open_pos = (int)(open - s);
            int close = find_matching_close(s, open_pos);
            char *tail = close >= 0 ? s + close + 1 : NULL;

            if(close >= 0 && tail != NULL) {
                while(*tail == ' ' || *tail == '\t')
                    tail++;
                if(*tail == '\0') {
                    char name[KIR_NAME_MAX];
                    char args[KIR_TEXT_MAX];
                    int idx;
                    size_t nl = (size_t)(open - s);

                    if(nl >= sizeof(name))
                        nl = sizeof(name) - 1;
                    memcpy(name, s, nl);
                    name[nl] = '\0';
                    if(is_simple_ident(name)) {
                        expr = KirFunctionAddExpr(fn, KIR_EXPR_CALL, s, span);
                        if(expr == NULL)
                            return -1;
                        idx = fn->expr_count - 1;
                        snprintf(expr->name, sizeof(expr->name), "%s", name);
                        snprintf(args, sizeof(args), "%.*s",
                                 close - open_pos - 1, s + open_pos + 1);
                        {
                            char *start = args;
                            int depth = 0;
                            int in_string = 0;

                            for(char *p = args;; p++) {
                                if(in_string) {
                                    if(*p == '\\' && p[1] != '\0')
                                        p++;
                                    else if(*p == '"')
                                        in_string = 0;
                                } else if(*p == '"') {
                                    in_string = 1;
                                } else if(*p == '(' || *p == '[' || *p == '{') {
                                    depth++;
                                } else if(*p == ')' || *p == ']' || *p == '}') {
                                    if(depth > 0)
                                        depth--;
                                }
                                if((*p == ',' && depth == 0) || *p == '\0') {
                                    char save = *p;
                                    int child;

                                    *p = '\0';
                                    if(trim(start)[0] != '\0') {
                                        child = parse_expr_to_kir(fn, start, span);
                                        append_call_arg(fn, idx, child);
                                    }
                                    if(save == '\0')
                                        break;
                                    start = p + 1;
                                }
                            }
                        }
                        return idx;
                    }
                }
            }
        }
    }
    if(s[0] == '"' && s[strlen(s) - 1] == '"') {
        expr = KirFunctionAddExpr(fn, KIR_EXPR_STRING, s, span);
        return expr != NULL ? fn->expr_count - 1 : -1;
    }
    if(is_int_literal_text(s)) {
        expr = KirFunctionAddExpr(fn, KIR_EXPR_INT, s, span);
        return expr != NULL ? fn->expr_count - 1 : -1;
    }
    if(is_simple_ident(s)) {
        expr = KirFunctionAddExpr(fn, KIR_EXPR_IDENT, s, span);
        if(expr != NULL)
            snprintf(expr->name, sizeof(expr->name), "%s", s);
        return expr != NULL ? fn->expr_count - 1 : -1;
    }
    expr = KirFunctionAddExpr(fn, KIR_EXPR_UNKNOWN, s, span);
    return expr != NULL ? fn->expr_count - 1 : -1;
}

static void
strip_expr_block_brace(char *s)
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

static const char *
stmt_expr_source(KirStmtKind kind, const char *text)
{
    const char *p;

    if(kind == KIR_STMT_DECL) {
        p = strstr(text, ":=");
        if(p != NULL)
            return p + 2;
        p = strstr(text, " = ");
        if(p != NULL)
            return p + 3;
        return NULL;
    }
    if(kind == KIR_STMT_ASSIGN) {
        p = strrchr(text, '=');
        if(p != NULL)
            return p + 1;
        return NULL;
    }
    if(kind == KIR_STMT_RETURN) {
        p = text;
        while(*p == ' ' || *p == '\t')
            p++;
        if(strncmp(p, "return", 6) == 0)
            return p + 6;
        return NULL;
    }
    if(kind == KIR_STMT_IF || kind == KIR_STMT_WHILE ||
       kind == KIR_STMT_SWITCH) {
        static char buf[KIR_TEXT_MAX];
        char *b;

        snprintf(buf, sizeof(buf), "%s", text);
        strip_expr_block_brace(buf);
        b = trim(buf);
        if(strncmp(b, "else if ", 8) == 0)
            return b + 8;
        if(strncmp(b, "if ", 3) == 0)
            return b + 3;
        if(strncmp(b, "while ", 6) == 0)
            return b + 6;
        if(strncmp(b, "switch ", 7) == 0)
            return b + 7;
        return b;
    }
    if(kind == KIR_STMT_EXPR || kind == KIR_STMT_WIDGET)
        return text;
    return NULL;
}

/* 'if cond { body }' (also else/while/for/switch/case/default/guard) written
 * on one logical line: locate the block-open brace at paren/bracket depth 0
 * outside string/char literals, and report the header (up to and including
 * the '{') plus the body (between '{ ' and the trailing '}'). Returns 0 when
 * the line is not a one-line control block. */
static int
split_oneline_block(const char *t, char *head, size_t hsz,
                    char *body, size_t bsz)
{
    static const char *kws[] = { "if", "else", "while", "for", "switch",
                                 "case", "default", "guard", "do" };
    size_t n = strlen(t);
    size_t brace_pos = 0;
    int depth = 0;
    int in_str = 0;
    int in_chr = 0;
    char w0[16];
    size_t wl = 0;
    size_t i;

    if(n < 8 || t[n - 1] != '}')
        return 0;
    for(i = 0; t[i] != '\0' && (isalnum((unsigned char)t[i]) || t[i] == '_') &&
        wl + 1 < sizeof(w0); i++)
        w0[wl++] = t[i];
    w0[wl] = '\0';
    {
        int is_kw = 0;

        for(size_t k = 0; k < sizeof(kws) / sizeof(kws[0]); k++)
            if(strcmp(w0, kws[k]) == 0)
                is_kw = 1;
        if(!is_kw)
            return 0;
    }
    for(i = 0; i < n; i++) {
        char ch = t[i];

        if(in_str) {
            if(ch == '\\' && i + 1 < n)
                i++;
            else if(ch == '"')
                in_str = 0;
        } else if(in_chr) {
            if(ch == '\\' && i + 1 < n)
                i++;
            else if(ch == '\'')
                in_chr = 0;
        } else if(ch == '"') {
            in_str = 1;
        } else if(ch == '\'') {
            in_chr = 1;
        } else if(ch == '(' || ch == '[') {
            depth++;
        } else if(ch == ')' || ch == ']') {
            depth--;
        } else if(ch == '{' && depth == 0) {
            /* first top-level '{' preceded by a space opens the block */
            if(i > 0 && t[i - 1] == ' ' && i + 2 < n && t[i + 1] == ' ' &&
                t[i + 2] != '}') {
                brace_pos = i;
                break;
            }
            return 0;   /* '{' used as expression on a control line */
        }
    }
    if(brace_pos == 0)
        return 0;
    /* no other top-level brace may appear before the trailing closer */
    depth = 0;
    in_str = in_chr = 0;
    for(i = brace_pos + 1; i + 1 < n; i++) {
        char ch = t[i];

        if(in_str) {
            if(ch == '\\' && i + 1 < n)
                i++;
            else if(ch == '"')
                in_str = 0;
        } else if(in_chr) {
            if(ch == '\\' && i + 1 < n)
                i++;
            else if(ch == '\'')
                in_chr = 0;
        } else if(ch == '"') {
            in_str = 1;
        } else if(ch == '\'') {
            in_chr = 1;
        } else if(ch == '(' || ch == '[') {
            depth++;
        } else if(ch == ')' || ch == ']') {
            depth--;
        } else if((ch == '{' || ch == '}') && depth == 0) {
            return 0;
        }
    }
    snprintf(head, hsz, "%.*s", (int)(brace_pos + 1), t);
    snprintf(body, bsz, "%.*s", (int)(n - brace_pos - 3), t + brace_pos + 2);
    return 1;
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
    char target[KIR_PATH_MAX];
    char symbol[KIR_NAME_MAX];
    KirImport *imp;
    KirExternKind extern_kind;
    const char *intrinsic = strstr(line, "#intrinsic");

    if(intrinsic == NULL && strstr(line, "#extern") == NULL)
        return 0;
    if(!parse_symbol_before_colons(line, name, sizeof(name)))
        return 0;
    target[0] = '\0';
    symbol[0] = '\0';
    if(intrinsic != NULL) {
        /* 'name :: (args) -> int #intrinsic "web"' — lowered by k2c to a
         * static EM_ASM wrapper on web builds. Only the two known web
         * intrinsics exist. */
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
    } else {
        const char *dir = strstr(line, "#extern");

        if(dir != NULL)
            parse_quoted(dir + 7, target, sizeof(target));
    }
    extern_kind = intrinsic != NULL
                      ? KIR_EXTERN_NONE
                      : classify_extern_target(target, symbol, sizeof(symbol),
                                               path, line_no);
    imp = KirModuleAddImport(module,
                             intrinsic != NULL ? KIR_IMPORT_INTRINSIC
                                               : KIR_IMPORT_EXTERN,
                             name, target[0] ? target : name, line, 1,
                             KirSpan(path, line_no, 1));
    if(imp != NULL) {
        imp->extern_kind = extern_kind;
        snprintf(imp->extern_symbol, sizeof(imp->extern_symbol), "%s",
                 symbol);
    }
    return 1;
}

static int
looks_like_function_header(const char *line)
{
    char tmp[K2IR_LINE_MAX];
    char *p;
    char *body;

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
 * '#if COND { ... } #else { ... }' regions use one model: top-level
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

static char *
find_top_comma(char *s)
{
    int depth = 0;
    int in_string = 0;
    int in_char = 0;

    for(char *p = s; *p != '\0'; p++) {
        if(in_string) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == '"')
                in_string = 0;
        } else if(in_char) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == '\'')
                in_char = 0;
        } else if(*p == '"') {
            in_string = 1;
        } else if(*p == '\'') {
            in_char = 1;
        } else if(*p == '(' || *p == '[' || *p == '{') {
            depth++;
        } else if(*p == ')' || *p == ']' || *p == '}') {
            if(depth > 0)
                depth--;
        } else if(*p == ',' && depth == 0) {
            return p;
        }
    }
    return NULL;
}

typedef struct KirEval {
    const char *p;
    int known;
    long value;
} KirEval;

static void
eval_skip(KirEval *ev)
{
    while(*ev->p == ' ' || *ev->p == '\t')
        ev->p++;
}

static long eval_or(KirEval *ev);

static long
eval_primary(KirEval *ev)
{
    char *end;
    long value;

    eval_skip(ev);
    if(*ev->p == '(') {
        ev->p++;
        value = eval_or(ev);
        eval_skip(ev);
        if(*ev->p == ')')
            ev->p++;
        else
            ev->known = 0;
        return value;
    }
    if(isdigit((unsigned char)*ev->p)) {
        value = strtol(ev->p, &end, 0);
        if(end == ev->p) {
            ev->known = 0;
            return 0;
        }
        ev->p = end;
        while(isalnum((unsigned char)*ev->p) || *ev->p == '_')
            ev->p++;   /* integer suffixes: U, L, UL */
        return value;
    }
    if(isalpha((unsigned char)*ev->p) || *ev->p == '_') {
        while(isalnum((unsigned char)*ev->p) || *ev->p == '_')
            ev->p++;
        ev->known = 0;
        return 0;
    }
    ev->known = 0;
    return 0;
}

static long
eval_unary(KirEval *ev)
{
    eval_skip(ev);
    if(*ev->p == '!') {
        long v;

        ev->p++;
        v = eval_unary(ev);
        return ev->known ? !v : 0;
    }
    if(*ev->p == '-') {
        ev->p++;
        return -eval_unary(ev);
    }
    if(*ev->p == '+') {
        ev->p++;
        return eval_unary(ev);
    }
    return eval_primary(ev);
}

static long
eval_mul(KirEval *ev)
{
    long left = eval_unary(ev);

    while(1) {
        char op;
        long right;
        int left_known;

        eval_skip(ev);
        if(*ev->p != '*' && *ev->p != '/' && *ev->p != '%')
            return left;
        op = *ev->p++;
        left_known = ev->known;
        right = eval_unary(ev);
        if(!left_known || !ev->known || (right == 0 && op != '*')) {
            ev->known = 0;
            left = 0;
        } else if(op == '*') {
            left *= right;
        } else if(op == '/') {
            left /= right;
        } else {
            left %= right;
        }
    }
}

static long
eval_add(KirEval *ev)
{
    long left = eval_mul(ev);

    while(1) {
        char op;
        long right;
        int left_known;

        eval_skip(ev);
        if(*ev->p != '+' && *ev->p != '-')
            return left;
        op = *ev->p++;
        left_known = ev->known;
        right = eval_mul(ev);
        if(!left_known || !ev->known) {
            ev->known = 0;
            left = 0;
        } else if(op == '+') {
            left += right;
        } else {
            left -= right;
        }
    }
}

static long
eval_rel(KirEval *ev)
{
    long left = eval_add(ev);

    while(1) {
        const char *op = NULL;
        long right;
        int left_known;

        eval_skip(ev);
        if(strncmp(ev->p, "<=", 2) == 0 || strncmp(ev->p, ">=", 2) == 0)
            op = ev->p, ev->p += 2;
        else if(*ev->p == '<' || *ev->p == '>')
            op = ev->p, ev->p++;
        else
            return left;
        left_known = ev->known;
        right = eval_add(ev);
        if(!left_known || !ev->known) {
            ev->known = 0;
            left = 0;
        } else if(op[0] == '<' && op[1] == '=') {
            left = left <= right;
        } else if(op[0] == '>' && op[1] == '=') {
            left = left >= right;
        } else if(op[0] == '<') {
            left = left < right;
        } else {
            left = left > right;
        }
    }
}

static long
eval_eq(KirEval *ev)
{
    long left = eval_rel(ev);

    while(1) {
        int neq = 0;
        long right;
        int left_known;

        eval_skip(ev);
        if(strncmp(ev->p, "==", 2) == 0) {
            ev->p += 2;
        } else if(strncmp(ev->p, "!=", 2) == 0) {
            ev->p += 2;
            neq = 1;
        } else {
            return left;
        }
        left_known = ev->known;
        right = eval_rel(ev);
        if(!left_known || !ev->known) {
            ev->known = 0;
            left = 0;
        } else {
            left = neq ? left != right : left == right;
        }
    }
}

static long
eval_and(KirEval *ev)
{
    long left = eval_eq(ev);

    while(1) {
        long right;
        int left_known;
        long left_value;

        eval_skip(ev);
        if(strncmp(ev->p, "&&", 2) != 0)
            return left;
        ev->p += 2;
        left_known = ev->known;
        left_value = left;
        right = eval_eq(ev);
        if(left_known && !left_value) {
            ev->known = 1;
            left = 0;
        } else if(left_known && ev->known) {
            left = left_value && right;
        } else {
            ev->known = 0;
            left = 0;
        }
    }
}

static long
eval_or(KirEval *ev)
{
    long left = eval_and(ev);

    while(1) {
        long right;
        int left_known;
        long left_value;

        eval_skip(ev);
        if(strncmp(ev->p, "||", 2) != 0)
            return left;
        ev->p += 2;
        left_known = ev->known;
        left_value = left;
        right = eval_and(ev);
        if(left_known && left_value) {
            ev->known = 1;
            left = 1;
        } else if(left_known && ev->known) {
            left = left_value || right;
        } else {
            ev->known = 0;
            left = 0;
        }
    }
}

static int
eval_const_condition(const char *src, long *value)
{
    KirEval ev;

    ev.p = src;
    ev.known = 1;
    ev.value = eval_or(&ev);
    eval_skip(&ev);
    if(*ev.p != '\0')
        ev.known = 0;
    if(value != NULL)
        *value = ev.value;
    return ev.known;
}

static int
parse_compile_check(KirModule *module, const char *path, int line_no,
                    char *line, const KirConsts *consts, const char *guard)
{
    char cond[KIR_TEXT_MAX];
    char msg[KIR_TEXT_MAX];
    KirAssert *a;

    if(strncmp(line, "#assert", 7) == 0 &&
       (line[7] == '\0' || isspace((unsigned char)line[7]))) {
        char *body = trim(line + 7);
        char *comma;

        if(body[0] == '\0')
            die("%s:%d: #assert needs a condition", path, line_no);
        comma = find_top_comma(body);
        if(comma != NULL) {
            *comma = '\0';
            snprintf(msg, sizeof(msg), "%s", trim(comma + 1));
            if(msg[0] == '\0')
                snprintf(msg, sizeof(msg), "\"Kry #assert failed\"");
        } else {
            snprintf(msg, sizeof(msg), "\"Kry #assert failed\"");
        }
        expand_compile_expr(cond, sizeof(cond), consts, trim(body));
        {
            long value = 0;
            int known = eval_const_condition(cond, &value);

            if(guard[0] == '\0' && known && !value)
                die("%s:%d: #assert failed: %s", path, line_no, msg);
            a = KirModuleAddAssert(module, cond, msg, KirSpan(path, line_no, 1));
            if(a != NULL) {
                a->known = known;
                a->value = value != 0;
                snprintf(a->guard, sizeof(a->guard), "%s", guard);
            }
        }
        return 1;
    }
    if(strncmp(line, "#error", 6) == 0 &&
       (line[6] == '\0' || isspace((unsigned char)line[6]))) {
        char *body = trim(line + 6);

        if(body[0] == '\0')
            die("%s:%d: #error needs a message", path, line_no);
        a = KirModuleAddAssert(module, "0", body, KirSpan(path, line_no, 1));
        if(a != NULL) {
            a->known = 1;
            a->value = 0;
            snprintf(a->guard, sizeof(a->guard), "%s", guard);
        }
        return 1;
    }
    return 0;
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

/* Strip C-style block comments in place, preserving newlines so line
 * numbers stay honest. *in_comment carries the state across lines (a
 * comment opened on one line keeps stripping on the next). String and
 * char literals are respected, and reset at each newline since Kry
 * literals never span lines. Without this, a comment close at end of
 * line trips the trailing-slash continuation rule and glues the comment
 * onto the next function header, silently dropping the function. */
static void
strip_block_comments(char *s, int *in_comment)
{
    char *w = s;
    char *r = s;
    int in_str = 0;
    int in_chr = 0;

    while(*r != '\0') {
        if(*in_comment) {
            while(*r != '\0' && !(*r == '*' && r[1] == '/')) {
                if(*r == '\n')
                    *w++ = '\n';
                r++;
            }
            if(*r != '\0') {
                r += 2;
                *in_comment = 0;
                *w++ = ' ';   /* keep tokens on either side apart */
            }
            continue;
        }
        if(in_str || in_chr) {
            if(*r == '\\' && r[1] != '\0') {
                *w++ = *r++;
                *w++ = *r++;
                continue;
            }
            if((in_str && *r == '"') || (in_chr && *r == '\''))
                in_str = in_chr = 0;
            else if(*r == '\n')
                in_str = in_chr = 0;
            *w++ = *r++;
        } else if(*r == '"') {
            in_str = 1;
            *w++ = *r++;
        } else if(*r == '\'') {
            in_chr = 1;
            *w++ = *r++;
        } else if(*r == '/' && r[1] == '*') {
            *in_comment = 1;
            r += 2;
        } else if(*r == '\n') {
            *w++ = *r++;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
}

KirProgram *
kir_parse_file(const char *path, const char *root)
{
    FILE *in;
    KirProgram *program;
    KirModule *module;
    KirFunction *fn = NULL;
    KirRoute *route = NULL;
    char line[K2IR_LINE_MAX];
    char module_name[KIR_NAME_MAX] = "main";
    char rel[K2IR_PATH_MAX];
    char stem[K2IR_PATH_MAX];
    char out_rel[K2IR_PATH_MAX];
    char out_path[K2IR_PATH_MAX];
    int line_no = 0;
    enum { TOP, APP, STATE, ROUTE, TYPE, ENUM, FUNCTION } mode = TOP;
    int enum_return = TOP;
    int depth = 0;
    char pending[K2IR_LINE_MAX * 4];
    pending[0] = '\0';
    char lookahead[K2IR_LINE_MAX];
    int have_look = 0;
    /* One-line control blocks ('if cond { body }') are split into header /
     * body / '}' logical lines; the body and closer re-enter the main loop
     * through this FIFO so they flow through the normal join machinery. */
    char onelineq[16][K2IR_LINE_MAX * 2];
    int onelineq_count = 0;
    int from_queue = 0;
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
    int in_block_comment = 0;
    UiBlock ui_blocks[64];
    int ui_block_count = 0;

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

    while(have_look || onelineq_count > 0 || fgets(line, sizeof(line), in) != NULL) {
        char raw[K2IR_LINE_MAX];
        char *t;

        /* Queued one-liner parts outrank the stashed lookahead: they belong
         * before the next source line, and have_look persists until the
         * queue drains. */
        from_queue = onelineq_count > 0;
        if(onelineq_count > 0) {
            snprintf(line, sizeof(line), "%s", onelineq[0]);
            memmove(onelineq[0], onelineq[1],
                    sizeof(onelineq[0]) * (size_t)(onelineq_count - 1));
            onelineq_count--;
        } else if(have_look) {
            snprintf(line, sizeof(line), "%s", lookahead);
            have_look = 0;
        }

        line_no++;
        strip_block_comments(line, &in_block_comment);
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
                    char uiw[KIR_NAME_MAX];
                    char uin[KIR_NAME_MAX];

                    header_line =
                        pending[0] == '#' ||
                        strcmp(pending, "{") == 0 ||   /* bare scope-open */
                        parse_ui_block_header(pending, uiw, sizeof(uiw),
                                              uin, sizeof(uin)) ||
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
                          strcmp(w0, "struct") == 0 ||
                          strcmp(w0, "enum") == 0 ||
                          strcmp(w0, "state") == 0 ||
                          strcmp(w0, "route") == 0 ||
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
                {
                int in_chr = 0;

                for(const char *p = trimmed; *p != '\0'; p++) {
                    if(in_string) {
                        if(*p == '\\' && p[1] != '\0')
                            p++;
                        else if(*p == '"')
                            in_string = 0;
                    } else if(in_chr) {
                        if(*p == '\\' && p[1] != '\0')
                            p++;
                        else if(*p == '\'')
                            in_chr = 0;
                    } else if(*p == '"') {
                        in_string = 1;
                    } else if(*p == '\'') {
                        in_chr = 1;
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
            }
            if(paren_depth > 0 || bracket_depth > 0 || in_string ||
               expr_brace > 0)
                continue;
            /* Continuation: a line ending in a binary operator or comma
             * continues onto the next. Exclude ++/-- (they end statements). */
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
                 * binary operators) continues this statement. Skipped for queued one-liner
                 * parts and while a stash is pending: their "next line" is
                 * not the next physical source line, and reading ahead here
                 * would overwrite/lose the stashed one. */
                if(!from_queue && !have_look) {
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

                        strip_block_comments(la, &in_block_comment);
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
            /* One-line control block: keep the header as this logical line
             * and queue the body + closer for the next iterations (nested
             * one-liners split again when their body is finalized). */
            if(mode != TOP) {
                char head[K2IR_LINE_MAX * 2];
                char body[K2IR_LINE_MAX * 2];

                if(split_oneline_block(t, head, sizeof(head),
                                       body, sizeof(body))) {
                    if(onelineq_count + 2 <=
                       (int)(sizeof(onelineq) / sizeof(onelineq[0]))) {
                        snprintf(onelineq[onelineq_count++],
                                 sizeof(onelineq[0]), "%s", body);
                        snprintf(onelineq[onelineq_count++],
                                 sizeof(onelineq[0]), "}");
                        snprintf(logical, sizeof(logical), "%s", head);
                        t = logical;
                    }
                }
            }
        }
        pending[0] = '\0';
        pending_len = 0;
        if(mode == TOP &&
           cond_top_step(t, tframes, &tframe_count, cur_guard,
                         sizeof(cur_guard), &consts, rel, line_no)) {
            continue;
        } else if(mode == TOP &&
                  parse_compile_check(module, rel, line_no, t, &consts,
                                      cur_guard)) {
            continue;
        } else if(mode == TOP && t[0] == '#' &&
                  strncmp(t, "#if", 3) != 0 && strncmp(t, "#else", 5) != 0 &&
                  strncmp(t, "#endif", 6) != 0 &&
                  strncmp(t, "#defined", 8) != 0 &&
                  strncmp(t, "#enum", 5) != 0 &&
                  strncmp(t, "#module", 7) != 0 &&
                  strncmp(t, "#import", 7) != 0) {
            /* plain # comment at top level — never a header; real
             * directives (#module/#import/#if...) fall through below */
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
        } else if(mode == TOP && starts_word(t, "route") &&
                  strchr(t, '{') != NULL) {
            char route_id[KIR_NAME_MAX];

            if(!parse_route_header(t, route_id, sizeof(route_id)))
                die("%s:%d: route block must be `route name {`", rel, line_no);
            route = KirModuleAddRoute(module, route_id, KirSpan(rel, line_no, 1));
            if(route != NULL)
                snprintf(route->guard, sizeof(route->guard), "%s", cur_guard);
            mode = ROUTE;
        } else if(mode == ROUTE) {
            if(t[0] == '}') {
                route = NULL;
                mode = TOP;
                cond_frame_settle(tframes, tframe_count);
            } else if(route != NULL && starts_word(t, "title")) {
                parse_quoted(t, route->title, sizeof(route->title));
            } else if(route != NULL && starts_word(t, "group")) {
                parse_quoted(t, route->group, sizeof(route->group));
            } else if(route != NULL && starts_word(t, "page")) {
                char page[KIR_NAME_MAX];

                page[0] = '\0';
                sscanf(t, "page %127s", page);
                if(page[0] != '\0')
                    snprintf(route->page, sizeof(route->page), "%s", page);
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
                die("%s:%d: app frame property is not supported; declare a #ui function",
                    rel, line_no);
            } else if(starts_word(t, "init")) {
                sscanf(t, "init %127s", module->app.init);
            } else if(starts_word(t, "scene")) {
                sscanf(t, "scene %127s", module->app.scene);
            } else if(starts_word(t, "shutdown")) {
                sscanf(t, "shutdown %127s", module->app.shutdown);
            }
        } else if(mode == TOP &&
                  (starts_word(t, "screen") || starts_word(t, "preview") ||
                   starts_word(t, "page") || starts_word(t, "frame") ||
                   starts_word(t, "fn"))) {
            die("%s:%d: declare UI with Name :: (...) #ui", rel, line_no);
        } else if(mode == TOP && looks_like_function_header(t)) {
            char name[KIR_NAME_MAX];
            char args[KIR_TEXT_MAX];
            char ret[KIR_NAME_MAX];
            int is_extern = strstr(t, "#extern") != NULL;
            int is_ui = strstr(t, "#ui") != NULL;
            int has_body = strchr(t, '{') != NULL;

            parse_function_header(name, sizeof(name), args, sizeof(args),
                                  ret, sizeof(ret), t);
            if(name[0] != '\0') {
                fn = KirModuleAddFunction(module, name, args, ret, 0,
                                          KirSpan(rel, line_no, 1));
                snprintf(fn->guard, sizeof(fn->guard), "%s", cur_guard);
                fn->is_extern = is_extern;
                /* '#extern "pkg.Fn"' — keep the quoted host symbol so the
                 * Go backend can bridge the call instead of emitting C. */
                if(is_extern) {
                    const char *dir = strstr(t, "#extern");

                    if(dir != NULL) {
                        const char *q = strchr(dir + 7, '"');

                        if(q != NULL) {
                            size_t n = 0;
                            const char *r = q + 1;

                            while(*r != '\0' && *r != '"' &&
                                  n + 1 < sizeof(fn->extern_target))
                                fn->extern_target[n++] = *r++;
                            fn->extern_target[n] = '\0';
                        }
                    }
                    fn->extern_kind = classify_extern_target(
                        fn->extern_target, fn->extern_symbol,
                        sizeof(fn->extern_symbol), rel, line_no);
                }
                fn->is_colon = strstr(t, "::") != NULL;
                fn->is_ui = is_ui;
                /* '#export' on a colon function keeps the plain Kry name as
                 * the C symbol so handwritten C and JNI entry points can call
                 * it directly. */
                fn->exported = fn->is_colon && strstr(t, "#export") != NULL;
                /* Public functions are emitted in headers; #ui functions are
                 * also project routes. */
                fn->is_public = !is_extern &&
                                strstr(t, "#private") == NULL &&
                                (is_ui || fn->is_colon);
                if(has_body && !is_extern) {
                    mode = FUNCTION;
                    depth = 1;
                    ui_block_count = 0;
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
                    char run_value[KIR_TEXT_MAX];

                    if(consts.count >= 16)
                        die("%s:%d: too many compile-time constants",
                            rel, line_no);
                    if(starts_word(expr, "#run")) {
                        char expanded[KIR_TEXT_MAX];
                        long value = 0;

                        expand_compile_expr(expanded, sizeof(expanded),
                                            &consts, trim((char *)(expr + 4)));
                        if(!eval_const_condition(expanded, &value))
                            die("%s:%d: #run expression is not a constant: %s",
                                rel, line_no, expanded);
                        snprintf(run_value, sizeof(run_value), "%ld", value);
                        expr = run_value;
                    }
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

            if(bck != 0 || line_is_hash_else(t)) {
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
            } else if(t[0] == '}' && ui_block_count > 0 &&
                      depth == ui_blocks[ui_block_count - 1].close_depth) {
                UiBlock *block = &ui_blocks[ui_block_count - 1];

                ui_block_open(fn, block, KirSpan(rel, line_no, 1));
                KirFunctionAddWidget(fn, "End", "", "End()",
                                     KirSpan(rel, line_no, 1));
                ui_block_count--;
                if(depth > 0)
                    depth--;
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
                        ui_block_count = 0;
                    } else {
                        KirFunctionAddStmt(fn, KIR_STMT_BLOCK_CLOSE, t, "",
                                           KirSpan(rel, line_no, 1));
                    }
                }
            } else {
                KirStmtKind kind = classify_stmt(t);
                char widget[KIR_NAME_MAX] = "";
                char widget_args[KIR_TEXT_MAX] = "";
                KirStmt *added;
                int brace_delta = net_block_braces(t);
                char block_widget[KIR_NAME_MAX];
                char block_name[KIR_NAME_MAX];
                char prop_field[KIR_NAME_MAX];
                char prop_value[KIR_TEXT_MAX];
                char prop_line[K2IR_LINE_MAX];

                if(parse_ui_block_header(t, block_widget,
                                         sizeof(block_widget),
                                         block_name, sizeof(block_name))) {
                    UiBlock *block;

                    if(ui_block_count > 0)
                        ui_block_open(fn, &ui_blocks[ui_block_count - 1],
                                      KirSpan(rel, line_no, 1));
                    if(ui_block_count >=
                       (int)(sizeof(ui_blocks) / sizeof(ui_blocks[0])))
                        die("%s:%d: too many nested UI blocks", rel, line_no);
                    block = &ui_blocks[ui_block_count++];
                    memset(block, 0, sizeof(*block));
                    snprintf(block->widget, sizeof(block->widget), "%s",
                             block_widget);
                    snprintf(block->name, sizeof(block->name), "%s",
                             block_name);
                    if(ui_block_count > 1)
                    {
                        char parent_path[KIR_TEXT_MAX];

                        snprintf(parent_path, sizeof(parent_path), "%s",
                                 ui_blocks[ui_block_count - 2].path);
                        snprintf(block->path, sizeof(block->path), "%.3000s/%.900s",
                                 parent_path, block_name);
                    }
                    else
                        snprintf(block->path, sizeof(block->path), "%.3000s/%.900s",
                                 fn != NULL ? fn->name : "ui", block_name);
                    block->close_depth = depth + 1;
                    depth++;
                    continue;
                }

                snprintf(prop_line, sizeof(prop_line), "%s", t);
                if(ui_block_count > 0 &&
                   !ui_blocks[ui_block_count - 1].opened &&
                   depth == ui_blocks[ui_block_count - 1].close_depth &&
                   parse_ui_prop_line(prop_line, prop_field,
                                      sizeof(prop_field), prop_value,
                                      sizeof(prop_value))) {
                    ui_block_append_prop(&ui_blocks[ui_block_count - 1],
                                         prop_field, prop_value);
                    continue;
                }

                if(ui_block_count > 0 &&
                   !ui_blocks[ui_block_count - 1].opened &&
                   depth == ui_blocks[ui_block_count - 1].close_depth)
                    ui_block_open(fn, &ui_blocks[ui_block_count - 1],
                                  KirSpan(rel, line_no, 1));

                if(kind == KIR_STMT_EXPR &&
                   parse_widget_statement(t, widget, sizeof(widget),
                                          widget_args,
                                          sizeof(widget_args)))
                    kind = KIR_STMT_WIDGET;
                if(kind == KIR_STMT_WIDGET)
                    added = KirFunctionAddWidget(fn, widget, widget_args, t,
                                                 KirSpan(rel, line_no, 1));
                else
                    added = KirFunctionAddStmt(fn, kind, t, widget,
                                               KirSpan(rel, line_no, 1));
                if(added != NULL) {
                    const char *expr_src = stmt_expr_source(kind, t);

                    if(expr_src != NULL)
                        added->expr_root =
                            parse_expr_to_kir(fn, expr_src,
                                              KirSpan(rel, line_no, 1));
                }
                depth += brace_delta;
                if(depth < 0)
                    depth = 0;
            }
        }
    }
    fclose(in);
    return program;
}
