#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

enum {
    KC_PATH_MAX = 1024,
    KC_TEXT_MAX = 1024 * 1024,
    KC_NAME_MAX = 128,
    KC_INCLUDE_MAX = 64,
    KC_USE_MAX = 32,
    KC_CALL_MAX = 64,
    KC_CONST_MAX = 64,
    KC_TYPE_MAX = 64,
    KC_RAW_MAX = 1024,
    KC_STATE_MAX = 128,
    KC_BODY_MAX = 512,
    KC_BODY_LINE_MAX = 1024,
};

typedef struct KryFunction {
    char screen[KC_NAME_MAX];
    char args[512];
    char return_type[KC_NAME_MAX];
    int exact_name;
    int is_public;
    char calls[KC_CALL_MAX][512];
    char body[KC_BODY_MAX][KC_BODY_LINE_MAX];
    int body_line[KC_BODY_MAX];
    int call_count;
    int body_count;
} KryFunction;

typedef struct KryFile {
    char *path;
    char *text;
    char app_title[256];
    int app_width;
    int app_height;
    int app_fps;
    int app_font_examples;
    char app_theme[KC_NAME_MAX];
    int app_dark_mode;
    int no_main;
    char raw[KC_RAW_MAX][KC_BODY_LINE_MAX];
    char public_types[KC_TYPE_MAX][KC_BODY_LINE_MAX];
    char private_types[KC_TYPE_MAX][KC_BODY_LINE_MAX];
    char state[KC_STATE_MAX][KC_BODY_LINE_MAX];
    char includes[KC_INCLUDE_MAX][KC_PATH_MAX];
    char const_names[KC_CONST_MAX][KC_NAME_MAX];
    char const_exprs[KC_CONST_MAX][KC_BODY_LINE_MAX];
    char module[KC_NAME_MAX];
    char use_aliases[KC_USE_MAX][KC_NAME_MAX];
    char use_modules[KC_USE_MAX][KC_NAME_MAX];
    KryFunction *functions;
    int raw_count;
    int public_type_count;
    int private_type_count;
    int state_count;
    int include_count;
    int const_count;
    int use_count;
    int function_count;
    int function_cap;
    int current_line;
    KryFunction *current;
} KryFile;

static void die(const char *fmt, ...);
static char *trim(char *s);
static int is_ident_text(const char *text);
static void rewrite_nil_tokens(char *dst, size_t dst_size, const char *src);
static void strip_kry_ext(char *dst, size_t dst_size, const char *path);
static void module_symbol(char *dst, size_t dst_size, const char *module);

static void
add_raw_line(KryFile *file, const char *line)
{
    if(file->raw_count >= KC_RAW_MAX)
        die("%s: top-level raw block is too large", file->path);
    snprintf(file->raw[file->raw_count],
             sizeof(file->raw[file->raw_count]), "%s", line);
    file->raw_count++;
}

static void
add_type_line(KryFile *file, int is_public, const char *fmt, ...)
{
    char (*types)[KC_BODY_LINE_MAX];
    int *count;
    va_list ap;

    if(is_public) {
        types = file->public_types;
        count = &file->public_type_count;
    } else {
        types = file->private_types;
        count = &file->private_type_count;
    }
    if(*count >= KC_TYPE_MAX)
        die("%s: type block is too large", file->path);
    va_start(ap, fmt);
    vsnprintf(types[*count], sizeof(types[*count]), fmt, ap);
    va_end(ap);
    (*count)++;
}

static void
add_state_line(KryFile *file, const char *line)
{
    if(file->state_count >= KC_STATE_MAX)
        die("%s: state block is too large", file->path);
    snprintf(file->state[file->state_count],
             sizeof(file->state[file->state_count]), "%s", line);
    file->state_count++;
}

static void
add_const(KryFile *file, int line_no, const char *name, const char *expr)
{
    if(!is_ident_text(name))
        die("%s:%d: invalid compile-time constant name '%s'",
            file->path, line_no, name);
    if(expr == NULL || expr[0] == '\0')
        die("%s:%d: expected compile-time constant expression",
            file->path, line_no);
    if(file->const_count >= KC_CONST_MAX)
        die("%s:%d: too many compile-time constants", file->path, line_no);
    for(int i = 0; i < file->const_count; i++) {
        if(strcmp(file->const_names[i], name) == 0)
            die("%s:%d: duplicate compile-time constant '%s'",
                file->path, line_no, name);
    }
    snprintf(file->const_names[file->const_count],
             sizeof(file->const_names[file->const_count]), "%s", name);
    snprintf(file->const_exprs[file->const_count],
             sizeof(file->const_exprs[file->const_count]), "%s", expr);
    file->const_count++;
}

static KryFunction *
add_function(KryFile *file)
{
    KryFunction *next;
    KryFunction *fn;
    int next_cap;

    if(file == NULL)
        return NULL;
    if(file->function_count >= file->function_cap) {
        next_cap = file->function_cap == 0 ? 32 : file->function_cap * 2;
        next = realloc(file->functions, (size_t)next_cap * sizeof(*next));
        if(next == NULL)
            die("%s: out of memory while growing function list", file->path);
        file->functions = next;
        file->function_cap = next_cap;
    }
    fn = &file->functions[file->function_count++];
    memset(fn, 0, sizeof(*fn));
    return fn;
}

static void
add_body_line(KryFile *file, int source_line, const char *fmt, ...)
{
    KryFunction *fn;
    va_list args;

    fn = file->current;
    if(fn == NULL)
        die("%s: statement outside function", file->path);
    if(fn->body_count >= KC_BODY_MAX)
        die("%s: generated body is too large", file->path);
    va_start(args, fmt);
    vsnprintf(fn->body[fn->body_count],
              sizeof(fn->body[fn->body_count]), fmt, args);
    va_end(args);
    fn->body_line[fn->body_count] = source_line;
    fn->body_count++;
}

static void
add_body(KryFile *file, const char *fmt, ...)
{
    KryFunction *fn;
    va_list args;

    fn = file->current;
    if(fn == NULL)
        die("%s: statement outside function", file->path);
    if(fn->body_count >= KC_BODY_MAX)
        die("%s: generated body is too large", file->path);
    va_start(args, fmt);
    vsnprintf(fn->body[fn->body_count],
              sizeof(fn->body[fn->body_count]), fmt, args);
    va_end(args);
    fn->body_line[fn->body_count] = file->current_line;
    fn->body_count++;
}

static void
die(const char *fmt, ...)
{
    va_list args;
    char message[2048];
    char *p1;
    char *p2;
    char *p3;
    int line = 0;
    int column = 1;

    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    p1 = strchr(message, ':');
    p2 = p1 != NULL ? strchr(p1 + 1, ':') : NULL;
    if(p1 != NULL && p2 != NULL) {
        int all_digits = 1;

        for(char *p = p1 + 1; p < p2; p++) {
            if(*p < '0' || *p > '9') {
                all_digits = 0;
                break;
            }
        }
        if(all_digits) {
            *p1 = '\0';
            *p2 = '\0';
            line = atoi(p1 + 1);
            p3 = strchr(p2 + 1, ':');
            if(p3 != NULL) {
                int column_digits = 1;

                for(char *p = p2 + 1; p < p3; p++) {
                    if(*p < '0' || *p > '9') {
                        column_digits = 0;
                        break;
                    }
                }
                if(column_digits) {
                    *p3 = '\0';
                    column = atoi(p2 + 1);
                    p2 = p3;
                }
            }
            if(column <= 0)
                column = 1;
            fprintf(stderr, "%s:%d:%d: error: %s\n",
                    message, line, column, trim(p2 + 1));
            exit(1);
        }
    }
    fprintf(stderr, "kc: error: %s\n", message);
    exit(1);
}

static char *
trim(char *s)
{
    char *end;

    while(*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        s++;
    end = s + strlen(s);
    while(end > s &&
          (end[-1] == ' ' || end[-1] == '\t' ||
           end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';
    return s;
}

static int
starts_word(const char *s, const char *word)
{
    size_t n = strlen(word);

    return strncmp(s, word, n) == 0 &&
           (s[n] == '\0' || isspace((unsigned char)s[n]));
}

static int
parse_ident(char **sp, char *dst, size_t dst_size)
{
    char *s = *sp;
    size_t n = 0;

    while(*s == ' ' || *s == '\t')
        s++;
    if(!((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || *s == '_'))
        return 0;
    while((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') ||
          (*s >= '0' && *s <= '9') || *s == '_') {
        if(n + 1 < dst_size)
            dst[n++] = *s;
        s++;
    }
    dst[n] = '\0';
    *sp = s;
    return 1;
}

static int
is_ident_text(const char *text)
{
    char tmp[KC_NAME_MAX];
    char *p;

    if(text == NULL || text[0] == '\0')
        return 0;
    snprintf(tmp, sizeof(tmp), "%s", text);
    p = tmp;
    if(!parse_ident(&p, tmp, sizeof(tmp)))
        return 0;
    p = trim(p);
    return p[0] == '\0';
}

static int
parse_module_name(char **sp, char *dst, size_t dst_size)
{
    char part[KC_NAME_MAX];
    char *s = *sp;
    size_t n = 0;

    if(!parse_ident(&s, part, sizeof(part)))
        return 0;
    while(1) {
        size_t part_len = strlen(part);

        if(n + part_len + 1 >= dst_size)
            return 0;
        memcpy(dst + n, part, part_len);
        n += part_len;
        if(*s != '.')
            break;
        dst[n++] = '.';
        s++;
        if(!parse_ident(&s, part, sizeof(part)))
            return 0;
    }
    dst[n] = '\0';
    *sp = s;
    return 1;
}

static int
parse_quoted(char **sp, char *dst, size_t dst_size)
{
    char *s = *sp;
    size_t n = 0;

    while(*s == ' ' || *s == '\t')
        s++;
    if(*s != '"')
        return 0;
    s++;
    while(*s != '\0' && *s != '"') {
        if(*s == '\\' && s[1] != '\0') {
            if(n + 1 < dst_size)
                dst[n++] = *s;
            s++;
        }
        if(n + 1 < dst_size)
            dst[n++] = *s;
        s++;
    }
    if(*s != '"')
        return 0;
    dst[n] = '\0';
    *sp = s + 1;
    return 1;
}

static void
c_string_literal(char *dst, size_t dst_size, const char *src)
{
    size_t n = 0;

    if(dst_size == 0)
        return;
    dst[n++] = '"';
    for(const char *p = src != NULL ? src : ""; *p != '\0' && n + 2 < dst_size; p++) {
        if(*p == '"' || *p == '\\') {
            if(n + 2 >= dst_size)
                break;
            dst[n++] = '\\';
            dst[n++] = *p;
        } else if(*p == '\n') {
            if(n + 2 >= dst_size)
                break;
            dst[n++] = '\\';
            dst[n++] = 'n';
        } else {
            dst[n++] = *p;
        }
    }
    if(n + 1 < dst_size)
        dst[n++] = '"';
    dst[n] = '\0';
}

static void
emit_source_push(KryFile *file, int line_no)
{
    char path[KC_PATH_MAX + 8];

    c_string_literal(path, sizeof(path), file->path);
    add_body_line(file, 0, "    PushUIInspectSource(%s, %d);", path, line_no);
}

static void
emit_source_pop(KryFile *file)
{
    add_body_line(file, 0, "    PopUIInspectSource();");
}

static const char *
native_widget_name(const char *name)
{
    if(strcmp(name, "CenteredColumn") == 0)
        return "GetUICenteredColumn";
    if(strcmp(name, "ScrollEnd") == 0)
        return "EndUIScrollPage";
    if(strcmp(name, "Text") == 0)
        return "DrawUIText";
    if(strcmp(name, "TextButton") == 0)
        return "DrawUITextButton";
    return name;
}

static void
emit_call(KryFile *file, const char *prefix, const char *expr,
          const char *suffix)
{
    char name[KC_NAME_MAX];
    const char *open = strchr(expr, '(');
    size_t n;

    if(open == NULL) {
        add_body(file, "%s%s%s", prefix, expr, suffix);
        return;
    }
    n = (size_t)(open - expr);
    if(n >= sizeof(name))
        n = sizeof(name) - 1;
    memcpy(name, expr, n);
    name[n] = '\0';
    add_body(file, "%s%s%s%s", prefix, native_widget_name(name), open, suffix);
}

static void
module_symbol(char *dst, size_t dst_size, const char *module)
{
    size_t n = 0;

    if(dst_size == 0)
        return;
    for(const char *p = module != NULL ? module : ""; *p != '\0' && n + 1 < dst_size; p++) {
        if(*p == '.')
            dst[n++] = '_';
        else
            dst[n++] = *p;
    }
    dst[n] = '\0';
}

static void
module_header(char *dst, size_t dst_size, const char *module)
{
    char sym[KC_NAME_MAX];

    module_symbol(sym, sizeof(sym), module);
    snprintf(dst, dst_size, "%s.h", sym);
}

static void
convert_var_decl(char *dst, size_t dst_size, const char *name,
                 const char *type)
{
    char tmp[512];
    char *t;

    snprintf(tmp, sizeof(tmp), "%s", type != NULL ? type : "");
    t = trim(tmp);
    if(t[0] == '[') {
        char *end = strchr(t, ']');

        if(end != NULL) {
            char *size;
            char *element;

            *end = '\0';
            size = trim(t + 1);
            element = trim(end + 1);
            snprintf(dst, dst_size, "%s %s[%s]", element, name, size);
            return;
        }
    }
    if(t[0] == '*') {
        snprintf(dst, dst_size, "%s *%s", trim(t + 1), name);
        return;
    }
    snprintf(dst, dst_size, "%s %s", t, name);
}

static void
convert_arg_list(char *dst, size_t dst_size, const char *src)
{
    char tmp[512];
    char out[512] = "";
    char *part;
    char *save = NULL;

    snprintf(tmp, sizeof(tmp), "%s", src);
    for(part = strtok_r(tmp, ",", &save); part != NULL;
        part = strtok_r(NULL, ",", &save)) {
        char *name;
        char *type;
        char *colon;
        char item[256];

        part = trim(part);
        colon = strchr(part, ':');
        if(colon == NULL) {
            snprintf(item, sizeof(item), "%s", part);
        } else {
            *colon = '\0';
            name = trim(part);
            type = trim(colon + 1);
            convert_var_decl(item, sizeof(item), name, type);
        }
        if(out[0] != '\0')
            strncat(out, ", ", sizeof(out) - strlen(out) - 1);
        strncat(out, item, sizeof(out) - strlen(out) - 1);
    }
    snprintf(dst, dst_size, "%s", out);
}

static int
read_prop_value(const char *src, const char *key, char *dst, size_t dst_size)
{
    char pattern[64];
    const char *p;
    const char *end;
    int depth = 0;
    int in_string = 0;

    snprintf(pattern, sizeof(pattern), "%s:", key);
    p = strstr(src, pattern);
    if(p == NULL)
        return 0;
    p += strlen(pattern);
    while(*p == ' ' || *p == '\t')
        p++;
    end = p;
    while(*end != '\0') {
        if(*end == '"' && (end == p || end[-1] != '\\'))
            in_string = !in_string;
        if(!in_string) {
            if(*end == '(' || *end == '{' || *end == '[')
                depth++;
            else if(*end == ')' || *end == '}' || *end == ']')
                depth--;
            else if(depth <= 0 && (*end == ' ' || *end == '\t')) {
                const char *next = end;

                while(*next == ' ' || *next == '\t')
                    next++;
                if(isalpha((unsigned char)*next) || *next == '_') {
                    const char *colon = next;

                    while(isalnum((unsigned char)*colon) || *colon == '_')
                        colon++;
                    if(*colon == ':')
                        break;
                }
            }
        }
        end++;
    }
    while(end > p && (end[-1] == ' ' || end[-1] == '\t'))
        end--;
    if((size_t)(end - p) >= dst_size)
        end = p + dst_size - 1;
    memcpy(dst, p, (size_t)(end - p));
    dst[end - p] = '\0';
    return dst[0] != '\0';
}

static int
parse_label_token(char **sp, char *dst, size_t dst_size)
{
    char *s = *sp;
    size_t n = 0;

    while(*s == ' ' || *s == '\t')
        s++;
    if(*s == '"') {
        char text[512];

        if(!parse_quoted(sp, text, sizeof(text)))
            return 0;
        c_string_literal(dst, dst_size, text);
        return 1;
    }
    int depth = 0;
    int in_string = 0;

    while(*s != '\0') {
        if(*s == '"' && (s == *sp || s[-1] != '\\'))
            in_string = !in_string;
        if(!in_string) {
            if(*s == '(' || *s == '{' || *s == '[')
                depth++;
            else if(*s == ')' || *s == '}' || *s == ']')
                depth--;
            else if(depth <= 0 && isspace((unsigned char)*s)) {
                char *next = s;

                while(*next == ' ' || *next == '\t')
                    next++;
                if(isalpha((unsigned char)*next) || *next == '_') {
                    char *colon = next;

                    while(isalnum((unsigned char)*colon) || *colon == '_')
                        colon++;
                    if(*colon == ':')
                        break;
                }
            }
        }
        if(n + 1 < dst_size)
            dst[n++] = *s;
        s++;
    }
    while(n > 0 && isspace((unsigned char)dst[n - 1]))
        n--;
    dst[n] = '\0';
    *sp = s;
    return n > 0;
}

static char *
find_inferred_decl_op(char *line)
{
    int depth = 0;
    int in_string = 0;
    int escaped = 0;

    for(char *p = line; p != NULL && *p != '\0'; p++) {
        if(in_string) {
            if(escaped)
                escaped = 0;
            else if(*p == '\\')
                escaped = 1;
            else if(*p == '"')
                in_string = 0;
            continue;
        }
        if(*p == '#')
            return NULL;
        if(*p == '"') {
            in_string = 1;
            continue;
        }
        if(*p == '(' || *p == '{' || *p == '[')
            depth++;
        else if(*p == ')' || *p == '}' || *p == ']')
            depth--;
        else if(depth == 0 && p[0] == ':' && p[1] == '=')
            return p;
    }
    return NULL;
}

static int
split_top_level_list(const char *src, char parts[][KC_BODY_LINE_MAX], int max)
{
    int count = 0;
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    const char *start = src;

    for(const char *p = src; p != NULL; p++) {
        int at_end = *p == '\0';

        if(!at_end && in_string) {
            if(escaped)
                escaped = 0;
            else if(*p == '\\')
                escaped = 1;
            else if(*p == '"')
                in_string = 0;
            continue;
        }
        if(!at_end) {
            if(*p == '"') {
                in_string = 1;
                continue;
            }
            if(*p == '(' || *p == '{' || *p == '[')
                depth++;
            else if(*p == ')' || *p == '}' || *p == ']')
                depth--;
        }
        if((at_end || (*p == ',' && depth == 0)) && count < max) {
            size_t n = (size_t)(p - start);

            while(n > 0 && isspace((unsigned char)start[n - 1]))
                n--;
            while(n > 0 && isspace((unsigned char)*start)) {
                start++;
                n--;
            }
            if(n >= KC_BODY_LINE_MAX)
                n = KC_BODY_LINE_MAX - 1;
            memcpy(parts[count], start, n);
            parts[count][n] = '\0';
            count++;
            start = p + 1;
        }
        if(at_end)
            break;
    }
    return count;
}

static int
line_is_inferred_decl(char *line)
{
    char tmp[KC_BODY_LINE_MAX];
    char names[KC_CALL_MAX][KC_BODY_LINE_MAX];
    char *op;
    int count;

    snprintf(tmp, sizeof(tmp), "%s", line);
    op = find_inferred_decl_op(tmp);
    if(op == NULL)
        return 0;
    *op = '\0';
    count = split_top_level_list(trim(tmp), names, KC_CALL_MAX);
    if(count <= 0)
        return 0;
    for(int i = 0; i < count; i++) {
        if(!is_ident_text(names[i]))
            return 0;
    }
    return 1;
}

static char *
find_typed_decl_colon(char *line)
{
    char name[KC_NAME_MAX];
    char *p = line;

    if(!parse_ident(&p, name, sizeof(name)))
        return NULL;
    p = trim(p);
    if(p[0] != ':' || p[1] == '=' || p[1] == ':')
        return NULL;
    return p;
}

static int
line_is_typed_decl(char *line)
{
    char tmp[KC_BODY_LINE_MAX];
    char *colon;
    char *type;

    snprintf(tmp, sizeof(tmp), "%s", line);
    colon = find_typed_decl_colon(tmp);
    if(colon == NULL)
        return 0;
    type = trim(colon + 1);
    return type[0] != '\0';
}

static void
emit_inferred_decl(KryFile *file, int line_no, char *line, int is_state)
{
    char names[KC_CALL_MAX][KC_BODY_LINE_MAX];
    char exprs[KC_CALL_MAX][KC_BODY_LINE_MAX];
    char *op;
    char *lhs;
    char *rhs;
    int name_count;
    int expr_count;

    op = find_inferred_decl_op(line);
    if(op == NULL)
        die("%s:%d: expected ':=' in inferred declaration", file->path,
            line_no);
    *op = '\0';
    lhs = trim(line);
    rhs = trim(op + 2);
    if(lhs[0] == '\0' || rhs[0] == '\0')
        die("%s:%d: expected names and values around ':='", file->path,
            line_no);
    name_count = split_top_level_list(lhs, names, KC_CALL_MAX);
    expr_count = split_top_level_list(rhs, exprs, KC_CALL_MAX);
    if(name_count != expr_count && expr_count != 1)
        die("%s:%d: inferred declaration count mismatch: %d names, %d values",
            file->path, line_no, name_count, expr_count);
    for(int i = 0; i < name_count; i++) {
        char out[KC_BODY_LINE_MAX];
        const char *expr = expr_count == 1 ? exprs[0] : exprs[i];

        if(!is_ident_text(names[i]))
            die("%s:%d: invalid inferred variable name '%s'", file->path,
                line_no, names[i]);
        if(expr[0] == '\0')
            die("%s:%d: expected inferred value for '%s'", file->path,
                line_no, names[i]);
        snprintf(out, sizeof(out), "%s__auto_type %s = %s;",
                 is_state ? "static " : "", names[i], expr);
        if(is_state) {
            add_state_line(file, out);
        } else {
            if(strchr(expr, '(') != NULL)
                emit_source_push(file, line_no);
            add_body(file, "    %s", out);
            if(strchr(expr, '(') != NULL)
                emit_source_pop(file);
        }
    }
}

static void
emit_typed_decl(KryFile *file, int line_no, char *line)
{
    char name[KC_NAME_MAX];
    char decl[512];
    char *colon;
    char *type;
    char *eq;
    char *expr = NULL;
    char *p = line;

    if(!parse_ident(&p, name, sizeof(name)))
        die("%s:%d: expected variable name", file->path, line_no);
    colon = trim(p);
    if(colon[0] != ':' || colon[1] == '=' || colon[1] == ':')
        die("%s:%d: expected ':' in typed declaration", file->path, line_no);
    type = trim(colon + 1);
    eq = strchr(type, '=');
    if(eq != NULL) {
        *eq = '\0';
        expr = trim(eq + 1);
    }
    type = trim(type);
    if(type[0] == '\0')
        die("%s:%d: expected variable type", file->path, line_no);
    convert_var_decl(decl, sizeof(decl), name, type);
    if(expr != NULL && expr[0] != '\0' && strchr(expr, '(') != NULL)
        emit_source_push(file, line_no);
    if(expr != NULL && expr[0] != '\0')
        add_body(file, "    %s = %s;", decl, expr);
    else
        add_body(file, "    %s = {0};", decl);
    if(expr != NULL && expr[0] != '\0' && strchr(expr, '(') != NULL)
        emit_source_pop(file);
}

static char *
find_assignment_op(char *line)
{
    int depth = 0;
    int in_string = 0;
    int escaped = 0;

    for(char *p = line; p != NULL && *p != '\0'; p++) {
        if(in_string) {
            if(escaped)
                escaped = 0;
            else if(*p == '\\')
                escaped = 1;
            else if(*p == '"')
                in_string = 0;
            continue;
        }
        if(*p == '#')
            return NULL;
        if(*p == '"') {
            in_string = 1;
            continue;
        }
        if(*p == '(' || *p == '{' || *p == '[')
            depth++;
        else if(*p == ')' || *p == '}' || *p == ']')
            depth--;
        else if(depth == 0 && *p == '=') {
            if(p > line && (p[-1] == ':' || p[-1] == '=' || p[-1] == '<' ||
                            p[-1] == '>' || p[-1] == '!'))
                continue;
            if(p[1] == '=')
                continue;
            return p;
        }
    }
    return NULL;
}

static void
emit_assignment(KryFile *file, int line_no, char *line)
{
    char names[KC_CALL_MAX][KC_BODY_LINE_MAX];
    char exprs[KC_CALL_MAX][KC_BODY_LINE_MAX];
    char *op;
    char *lhs;
    char *rhs;
    int name_count;
    int expr_count;

    op = find_assignment_op(line);
    if(op == NULL)
        die("%s:%d: expected '=' in assignment", file->path, line_no);
    *op = '\0';
    lhs = trim(line);
    rhs = trim(op + 1);
    if(lhs[0] == '\0' || rhs[0] == '\0')
        die("%s:%d: expected names and values around '='", file->path,
            line_no);
    name_count = split_top_level_list(lhs, names, KC_CALL_MAX);
    expr_count = split_top_level_list(rhs, exprs, KC_CALL_MAX);
    if(name_count != expr_count && expr_count != 1)
        die("%s:%d: assignment count mismatch: %d names, %d values",
            file->path, line_no, name_count, expr_count);
    if(expr_count == 1 && name_count == 1) {
        if(names[0][0] == '\0' || exprs[0][0] == '\0')
            die("%s:%d: expected assignment target and value", file->path,
                line_no);
        if(strchr(exprs[0], '(') != NULL)
            emit_source_push(file, line_no);
        add_body(file, "    %s = %s;", names[0], exprs[0]);
        if(strchr(exprs[0], '(') != NULL)
            emit_source_pop(file);
        return;
    }

    for(int i = 0; i < name_count; i++) {
        if(names[i][0] == '\0')
            die("%s:%d: expected assignment target", file->path, line_no);
    }
    for(int i = 0; i < expr_count; i++) {
        if(exprs[i][0] == '\0')
            die("%s:%d: expected assignment value", file->path, line_no);
    }

    if(expr_count == 1) {
        if(strchr(exprs[0], '(') != NULL)
            emit_source_push(file, line_no);
        add_body(file, "    __auto_type __kryon_assign_%d_0 = %s;", line_no,
                 exprs[0]);
        if(strchr(exprs[0], '(') != NULL)
            emit_source_pop(file);
        for(int i = 0; i < name_count; i++)
            add_body(file, "    %s = __kryon_assign_%d_0;", names[i], line_no);
        return;
    }

    for(int i = 0; i < expr_count; i++) {
        if(strchr(exprs[i], '(') != NULL)
            emit_source_push(file, line_no);
        add_body(file, "    __auto_type __kryon_assign_%d_%d = %s;", line_no,
                 i, exprs[i]);
        if(strchr(exprs[i], '(') != NULL)
            emit_source_pop(file);
    }
    for(int i = 0; i < name_count; i++)
        add_body(file, "    %s = __kryon_assign_%d_%d;", names[i], line_no, i);
}

static void
emit_state_decl(KryFile *file, int line_no, char *line)
{
    char *colon;
    char *eq;
    char name[KC_NAME_MAX];
    char decl[512];
    char *type;
    char *expr = NULL;

    if(line_is_inferred_decl(line)) {
        emit_inferred_decl(file, line_no, line, 1);
        return;
    }
    if(starts_word(line, "let"))
        die("%s:%d: legacy 'let' syntax was removed; use 'name: type = value'",
            file->path, line_no);
    colon = strchr(line, ':');
    if(colon == NULL)
        die("%s:%d: expected ':' in state declaration", file->path, line_no);
    *colon = '\0';
    snprintf(name, sizeof(name), "%s", trim(line));
    if(!is_ident_text(name))
        die("%s:%d: invalid state variable name '%s'", file->path,
            line_no, name);
    type = trim(colon + 1);
    eq = strchr(type, '=');
    if(eq != NULL) {
        *eq = '\0';
        expr = trim(eq + 1);
    }
    convert_var_decl(decl, sizeof(decl), name, trim(type));
    if(expr != NULL && expr[0] != '\0') {
        char out[KC_BODY_LINE_MAX];
        char rewritten[KC_BODY_LINE_MAX];

        rewrite_nil_tokens(rewritten, sizeof(rewritten), expr);
        snprintf(out, sizeof(out), "static %s = %s;", decl, rewritten);
        add_state_line(file, out);
    } else {
        char out[KC_BODY_LINE_MAX];

        snprintf(out, sizeof(out), "static %s;", decl);
        add_state_line(file, out);
    }
}

static int
emit_state_decl_start(KryFile *file, int line_no, char *line)
{
    char *colon;
    char *eq;
    char name[KC_NAME_MAX];
    char decl[512];
    char *type;
    char *expr;
    char out[KC_BODY_LINE_MAX];

    colon = strchr(line, ':');
    if(colon == NULL)
        die("%s:%d: expected ':' in state declaration", file->path, line_no);
    eq = strchr(colon + 1, '=');
    if(eq == NULL)
        return 0;

    *colon = '\0';
    snprintf(name, sizeof(name), "%s", trim(line));
    if(!is_ident_text(name))
        die("%s:%d: invalid state variable name '%s'", file->path,
            line_no, name);

    *eq = '\0';
    type = trim(colon + 1);
    expr = trim(eq + 1);
    convert_var_decl(decl, sizeof(decl), name, type);
    {
        char rewritten[KC_BODY_LINE_MAX];

        rewrite_nil_tokens(rewritten, sizeof(rewritten), expr);
        snprintf(out, sizeof(out), "static %s = %s", decl, rewritten);
    }
    add_state_line(file, out);
    return 1;
}

static void
rewrite_nil_tokens(char *dst, size_t dst_size, const char *src)
{
    size_t n = 0;
    int in_string = 0;
    int escaped = 0;

    if(dst_size == 0)
        return;
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
        if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
           *p == '_') {
            char ident[KC_NAME_MAX];
            size_t ident_len = 0;
            const char *start = p;

            while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                  (*p >= '0' && *p <= '9') || *p == '_') {
                if(ident_len + 1 < sizeof(ident))
                    ident[ident_len++] = *p;
                p++;
            }
            ident[ident_len] = '\0';
            if(strcmp(ident, "nil") == 0) {
                const char *text = "NULL";
                size_t len = strlen(text);

                if(n + len >= dst_size)
                    break;
                memcpy(dst + n, text, len);
                n += len;
            } else {
                size_t len = (size_t)(p - start);

                if(n + len >= dst_size)
                    break;
                memcpy(dst + n, start, len);
                n += len;
            }
            continue;
        }
        dst[n++] = *p++;
    }
    dst[n] = '\0';
}

static void
add_state_continuation_line(KryFile *file, const char *line, int is_last)
{
    char out[KC_BODY_LINE_MAX];
    char rewritten[KC_BODY_LINE_MAX];
    size_t len;

    rewrite_nil_tokens(rewritten, sizeof(rewritten), line);
    snprintf(out, sizeof(out), "%s", rewritten);
    if(is_last) {
        len = strlen(out);
        while(len > 0 && isspace((unsigned char)out[len - 1]))
            out[--len] = '\0';
        if(len > 0 && out[len - 1] == ',')
            out[--len] = '\0';
        if(len + 2 >= sizeof(out))
            die("%s: state declaration is too large", file->path);
        out[len++] = ';';
        out[len] = '\0';
    }
    add_state_line(file, out);
}

static void
emit_top_static_decl(KryFile *file, int line_no, char *line)
{
    char *q = trim(line + strlen("static"));

    if(q[0] == '\0')
        die("%s:%d: expected static declaration", file->path, line_no);
    emit_state_decl(file, line_no, q);
}

static int
emit_top_static_decl_start(KryFile *file, int line_no, char *line)
{
    char *q = trim(line + strlen("static"));

    if(q[0] == '\0')
        die("%s:%d: expected static declaration", file->path, line_no);
    return emit_state_decl_start(file, line_no, q);
}

static void
emit_struct_field(KryFile *file, int line_no, int is_public, const char *line)
{
    char tmp[KC_BODY_LINE_MAX];
    char out[KC_BODY_LINE_MAX];
    char *colon;
    char *name;
    char *type;

    snprintf(tmp, sizeof(tmp), "%s", line);
    colon = strchr(tmp, ':');
    if(colon == NULL)
        die("%s:%d: expected ':' in struct field declaration",
            file->path, line_no);
    *colon = '\0';
    name = trim(tmp);
    type = trim(colon + 1);
    if(!is_ident_text(name))
        die("%s:%d: invalid struct field name '%s'", file->path,
            line_no, name);
    if(type[0] == '\0')
        die("%s:%d: expected struct field type", file->path, line_no);
    convert_var_decl(out, sizeof(out), name, type);
    add_type_line(file, is_public, "    %s;", out);
}

static void
emit_struct_start(KryFile *file, int line_no, int is_public, char *line,
                  char *name, size_t name_size)
{
    char *q = trim(line);
    size_t n;

    if(is_public)
        q = trim(q + strlen("pub"));
    q = trim(q + strlen("struct"));
    n = strlen(q);
    if(n == 0 || q[n - 1] != '{')
        die("%s:%d: expected '{' after struct name", file->path, line_no);
    q[n - 1] = '\0';
    q = trim(q);
    if(!is_ident_text(q))
        die("%s:%d: invalid struct name '%s'", file->path, line_no, q);
    snprintf(name, name_size, "%s", q);
    add_type_line(file, is_public, "typedef struct %s {", name);
}

static void
emit_struct_end(KryFile *file, int is_public, const char *name)
{
    add_type_line(file, is_public, "} %s;", name);
}

static void
emit_enum_item(KryFile *file, int line_no, int is_public, const char *line)
{
    char tmp[KC_BODY_LINE_MAX];
    char *q;
    size_t n;

    snprintf(tmp, sizeof(tmp), "%s", line);
    q = trim(tmp);
    n = strlen(q);
    if(n > 0 && q[n - 1] == ',')
        q[--n] = '\0';
    if(n == 0)
        die("%s:%d: expected enum item", file->path, line_no);
    add_type_line(file, is_public, "    %s,", q);
}

static void
emit_enum_start(KryFile *file, int line_no, int is_public, char *line)
{
    char *q = trim(line);

    if(is_public)
        q = trim(q + strlen("pub"));
    q = trim(q + strlen("enum"));
    if(strcmp(q, "{") != 0)
        die("%s:%d: expected anonymous enum block", file->path, line_no);
    add_type_line(file, is_public, "enum {");
}

static void
emit_enum_end(KryFile *file, int is_public)
{
    add_type_line(file, is_public, "};");
}

static int
line_is_close(const char *line)
{
    return strcmp(line, "}") == 0;
}

static int
line_is_else(const char *line)
{
    return strcmp(line, "else {") == 0 || strcmp(line, "} else {") == 0;
}

static int
line_is_hash_compile(const char *line)
{
    return strncmp(line, "#if", 3) == 0 ||
           strncmp(line, "#else_if", 8) == 0 ||
           strncmp(line, "#else", 5) == 0 ||
           strncmp(line, "} #else_if", 10) == 0 ||
           strncmp(line, "} #else", 7) == 0;
}

static int
starts_else_if(const char *line)
{
    return starts_word(line, "else if") || starts_word(line, "} else if");
}

static int
line_is_assignment_statement(const char *line)
{
    const char *p;

    if(line == NULL || line[0] == '\0')
        return 0;
    if(strstr(line, "+=") != NULL || strstr(line, "-=") != NULL ||
       strstr(line, "*=") != NULL || strstr(line, "/=") != NULL ||
       strstr(line, "++") != NULL || strstr(line, "--") != NULL)
        return 1;
    for(p = line; *p != '\0'; p++) {
        if(*p != '=')
            continue;
        if(p > line && (p[-1] == '=' || p[-1] == '<' || p[-1] == '>' ||
                        p[-1] == '!'))
            continue;
        if(p[1] == '=')
            continue;
        return 1;
    }
    return 0;
}

static int
line_is_mutation_statement(const char *line)
{
    return line != NULL &&
           (strstr(line, "+=") != NULL || strstr(line, "-=") != NULL ||
            strstr(line, "*=") != NULL || strstr(line, "/=") != NULL ||
            strstr(line, "++") != NULL || strstr(line, "--") != NULL);
}

static void
count_line_braces(const char *line, int *opens, int *closes)
{
    int in_string = 0;
    int escaped = 0;

    *opens = 0;
    *closes = 0;
    for(const char *p = line; p != NULL && *p != '\0'; p++) {
        if(in_string) {
            if(escaped) {
                escaped = 0;
            } else if(*p == '\\') {
                escaped = 1;
            } else if(*p == '"') {
                in_string = 0;
            }
            continue;
        }
        if(*p == '#')
            break;
        if(*p == '"') {
            in_string = 1;
            continue;
        }
        if(*p == '{')
            (*opens)++;
        else if(*p == '}')
            (*closes)++;
    }
}

static int
line_delim_delta(const char *line)
{
    int delta = 0;
    int in_string = 0;
    int escaped = 0;

    for(const char *p = line; p != NULL && *p != '\0'; p++) {
        if(in_string) {
            if(escaped)
                escaped = 0;
            else if(*p == '\\')
                escaped = 1;
            else if(*p == '"')
                in_string = 0;
            continue;
        }
        if(*p == '#')
            break;
        if(*p == '"') {
            in_string = 1;
            continue;
        }
        if(*p == '(' || *p == '[' || *p == '{')
            delta++;
        else if(*p == ')' || *p == ']' || *p == '}')
            delta--;
    }
    return delta;
}

static int
line_group_delta(const char *line)
{
    int delta = 0;
    int in_string = 0;
    int escaped = 0;

    for(const char *p = line; p != NULL && *p != '\0'; p++) {
        if(in_string) {
            if(escaped)
                escaped = 0;
            else if(*p == '\\')
                escaped = 1;
            else if(*p == '"')
                in_string = 0;
            continue;
        }
        if(*p == '#')
            break;
        if(*p == '"') {
            in_string = 1;
            continue;
        }
        if(*p == '(' || *p == '[')
            delta++;
        else if(*p == ')' || *p == ']')
            delta--;
    }
    return delta;
}

static int
line_starts_block_statement(const char *line)
{
    return starts_word(line, "if") ||
           starts_word(line, "for") ||
           starts_word(line, "while") ||
           starts_word(line, "button") ||
           starts_word(line, "event") ||
           starts_word(line, "on") ||
           starts_word(line, "icon_button") ||
           starts_word(line, "c") ||
           line_is_hash_compile(line) ||
           starts_else_if(line) ||
           line_is_else(line);
}

static int
line_needs_continuation(const char *line)
{
    const char *end;

    if(line == NULL)
        return 0;
    end = line + strlen(line);
    while(end > line && isspace((unsigned char)end[-1]))
        end--;
    if(end == line)
        return 0;
    if(end[-1] == ',')
        return 1;
    if(end[-1] == '+' || end[-1] == '-') {
        if(end - line >= 2 && end[-2] == end[-1])
            return 0;
        return 1;
    }
    if(end[-1] == '*' || end[-1] == '/' || end[-1] == '%' ||
       end[-1] == '?' || end[-1] == ':' || end[-1] == '=')
        return 1;
    if(end - line >= 2) {
        const char *op = end - 2;

        if(strcmp(op, "&&") == 0 || strcmp(op, "||") == 0 ||
           strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
           strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0)
            return 1;
    }
    return 0;
}

static void
append_statement_line(KryFile *file, char *dst, size_t dst_size,
                      const char *line)
{
    size_t used = strlen(dst);
    size_t need = strlen(line);

    if(used != 0) {
        if(used + 2 >= dst_size)
            die("%s: continued statement is too large", file->path);
        dst[used++] = ' ';
        dst[used] = '\0';
    }
    if(used + need + 1 >= dst_size)
        die("%s: continued statement is too large", file->path);
    memcpy(dst + used, line, need + 1);
}

static int
split_hash_if_suffix(char *line, char **condition)
{
    char *hash_if = strstr(line, " #if ");

    *condition = NULL;
    if(hash_if == NULL)
        return 0;
    *hash_if = '\0';
    *condition = trim(hash_if + strlen(" #if "));
    return (*condition)[0] != '\0';
}

static char *
trim_trailing_open_brace(char *q)
{
    size_t n;

    q = trim(q);
    n = strlen(q);
    if(n == 0 || q[n - 1] != '{')
        return NULL;
    q[n - 1] = '\0';
    return trim(q);
}

static int
parse_hash_if_start(char *line, char **condition)
{
    char *q;

    if(strncmp(line, "#if", 3) == 0 &&
       (line[3] == '\0' || isspace((unsigned char)line[3]))) {
        q = trim(line + 3);
    } else if(strncmp(line, "} #else_if", 10) == 0 &&
              (line[10] == '\0' || isspace((unsigned char)line[10]))) {
        q = trim(line + 10);
    } else if(strncmp(line, "#else_if", 8) == 0 &&
              (line[8] == '\0' || isspace((unsigned char)line[8]))) {
        q = trim(line + 8);
    } else {
        return 0;
    }
    q = trim_trailing_open_brace(q);
    if(q == NULL || q[0] == '\0')
        return 0;
    *condition = q;
    return 1;
}

static int
line_is_hash_else(char *line)
{
    return strcmp(line, "} #else {") == 0 || strcmp(line, "#else {") == 0;
}

static void
expand_compile_expr(char *dst, size_t dst_size, const KryFile *file,
                    const char *src)
{
    size_t n = 0;
    int in_string = 0;
    int escaped = 0;

    if(dst_size == 0)
        return;
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
                const char *text = "defined";
                size_t len = strlen(text);

                if(n + len >= dst_size)
                    break;
                memcpy(dst + n, text, len);
                n += len;
                p += 8;
                continue;
            }
        }
        if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
           *p == '_') {
            char ident[KC_NAME_MAX];
            size_t ident_len = 0;
            int found = 0;

            while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                  (*p >= '0' && *p <= '9') || *p == '_') {
                if(ident_len + 1 < sizeof(ident))
                    ident[ident_len++] = *p;
                p++;
            }
            ident[ident_len] = '\0';
            for(int i = 0; i < file->const_count; i++) {
                if(strcmp(file->const_names[i], ident) == 0) {
                    char expanded[KC_BODY_LINE_MAX];
                    int written;

                    expand_compile_expr(expanded, sizeof(expanded), file,
                                        file->const_exprs[i]);
                    written = snprintf(dst + n, dst_size - n, "(%s)",
                                       expanded);
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
                size_t len = strlen(ident);

                if(n + len >= dst_size)
                    break;
                memcpy(dst + n, ident, len);
                n += len;
            }
            continue;
        }
        dst[n++] = *p++;
    }
    dst[n] = '\0';
}

static void
add_raw_conditional_line(KryFile *file, const char *condition,
                         const char *line)
{
    if(condition != NULL && condition[0] != '\0') {
        char guard[KC_BODY_LINE_MAX];
        char expanded[KC_BODY_LINE_MAX];

        expand_compile_expr(expanded, sizeof(expanded), file, condition);
        snprintf(guard, sizeof(guard), "#if %s", expanded);
        add_raw_line(file, guard);
    }
    add_raw_line(file, line);
    if(condition != NULL && condition[0] != '\0')
        add_raw_line(file, "#endif");
}

static void
emit_c_block_line(KryFile *file, int line_no, char *line)
{
    char *condition = NULL;

    split_hash_if_suffix(line, &condition);
    if(starts_word(line, "include")) {
        char *q = trim(line + strlen("include"));
        char header[KC_PATH_MAX];
        char out[KC_BODY_LINE_MAX];

        if(!parse_quoted(&q, header, sizeof(header)))
            die("%s:%d: expected quoted include path", file->path, line_no);
        snprintf(out, sizeof(out), "#include \"%s\"", header);
        add_raw_conditional_line(file, condition, out);
    } else if(starts_word(line, "define")) {
        char *q = trim(line + strlen("define"));
        char *eq = strchr(q, '=');
        char name[KC_NAME_MAX];
        char out[KC_BODY_LINE_MAX];

        if(eq == NULL)
            die("%s:%d: expected '=' in define", file->path, line_no);
        *eq = '\0';
        snprintf(name, sizeof(name), "%s", trim(q));
        if(!is_ident_text(name))
            die("%s:%d: invalid define name '%s'", file->path, line_no, name);
        snprintf(out, sizeof(out), "#define %s %s", name, trim(eq + 1));
        add_raw_conditional_line(file, condition, out);
    } else if(starts_word(line, "extern")) {
        char *q = trim(line + strlen("extern"));
        char name[KC_NAME_MAX];
        char args[512] = "";
        char ret[KC_NAME_MAX] = "void";
        char out[KC_BODY_LINE_MAX];

        if(!starts_word(q, "fn"))
            die("%s:%d: expected extern fn", file->path, line_no);
        q = trim(q + strlen("fn"));
        if(!parse_ident(&q, name, sizeof(name)))
            die("%s:%d: expected extern function name", file->path, line_no);
        q = trim(q);
        if(q[0] == '(') {
            char *end = strrchr(q, ')');

            if(end == NULL)
                die("%s:%d: expected ')' in extern function", file->path,
                    line_no);
            *end = '\0';
            convert_arg_list(args, sizeof(args), trim(q + 1));
            q = trim(end + 1);
        }
        if(q[0] == '-' && q[1] == '>') {
            q = trim(q + 2);
            if(q[0] == '\0')
                die("%s:%d: expected extern return type", file->path,
                    line_no);
            snprintf(ret, sizeof(ret), "%s", q);
        }
        snprintf(out, sizeof(out), "%s %s(%s);", ret, name,
                 args[0] != '\0' ? args : "void");
        add_raw_conditional_line(file, condition, out);
    } else {
        die("%s:%d: unknown c block statement: %s", file->path, line_no,
            line);
    }
}

static void
parse_statement(KryFile *file, int line_no, char *line)
{
    if(line_is_close(line)) {
        add_body(file, "    }");
    } else if(starts_word(line, "let")) {
        die("%s:%d: legacy 'let' syntax was removed; use 'name: type = value'",
            file->path, line_no);
    } else if(line_is_inferred_decl(line)) {
        emit_inferred_decl(file, line_no, line, 0);
    } else if(line_is_typed_decl(line)) {
        emit_typed_decl(file, line_no, line);
    } else if(starts_word(line, "background")) {
        char *q = trim(line + strlen("background"));

        if(q[0] == '\0')
            die("%s:%d: expected background color", file->path, line_no);
        add_body(file,
                 "    DrawRectangleRec((Rectangle){0, 0, GetUIViewWidth(), GetUIViewHeight()}, %s);",
                 q);
    } else if(starts_word(line, "set_theme")) {
        char *q = trim(line + strlen("set_theme"));
        char theme[KC_NAME_MAX];
        char *mode;

        if(!parse_ident(&q, theme, sizeof(theme)))
            die("%s:%d: expected theme id", file->path, line_no);
        mode = trim(q);
        if(mode[0] == '\0')
            mode = "0";
        add_body(file, "    SetCurrentTheme(%s, %s);", theme, mode);
    } else if(starts_word(line, "text")) {
        char *q = trim(line + strlen("text"));
        char label[512];
        char x[128] = "0";
        char y[128] = "0";
        char size[128] = "UI_TEXT_16";
        char color[256] = "GetThemeText()";

        if(!parse_label_token(&q, label, sizeof(label)))
            die("%s:%d: expected text label", file->path, line_no);
        q = trim(q);
        read_prop_value(q, "x", x, sizeof(x));
        read_prop_value(q, "y", y, sizeof(y));
        read_prop_value(q, "size", size, sizeof(size));
        read_prop_value(q, "color", color, sizeof(color));
        emit_source_push(file, line_no);
        add_body(file, "    DrawUIText(%s, %s, %s, %s, %s);",
                 label[0] == '"' ? label : label, x, y, size, color);
        emit_source_pop(file);
    } else if(starts_word(line, "rect")) {
        char *q = trim(line + strlen("rect"));
        char x[128] = "0";
        char y[128] = "0";
        char w[128] = "0";
        char h[128] = "0";
        char fill[256] = "GetThemeSurface()";
        char border[256] = "";

        if(strchr(q, '=') != NULL)
            die("%s:%d: rect draws a rectangle; use 'var name: Rectangle = {...}' for declarations",
                file->path, line_no);
        if(!read_prop_value(q, "x", x, sizeof(x)) ||
           !read_prop_value(q, "y", y, sizeof(y)) ||
           !read_prop_value(q, "w", w, sizeof(w)) ||
           !read_prop_value(q, "h", h, sizeof(h)))
            die("%s:%d: rect requires x:, y:, w:, and h:", file->path,
                line_no);
        read_prop_value(q, "fill", fill, sizeof(fill));
        read_prop_value(q, "border", border, sizeof(border));
        add_body(file,
                 "    DrawRectangleRec((Rectangle){%s, %s, %s, %s}, %s);",
                 x, y, w, h, fill);
        if(border[0] != '\0')
            add_body(file,
                     "    DrawRectangleLinesEx((Rectangle){%s, %s, %s, %s}, 1, %s);",
                     x, y, w, h, border);
    } else if(starts_word(line, "line")) {
        char *q = trim(line + strlen("line"));
        char x1[128] = "0";
        char y1[128] = "0";
        char x2[128] = "0";
        char y2[128] = "0";
        char color[256] = "GetThemeText()";

        read_prop_value(q, "x1", x1, sizeof(x1));
        read_prop_value(q, "y1", y1, sizeof(y1));
        read_prop_value(q, "x2", x2, sizeof(x2));
        read_prop_value(q, "y2", y2, sizeof(y2));
        read_prop_value(q, "color", color, sizeof(color));
        add_body(file, "    DrawLine(%s, %s, %s, %s, %s);",
                 x1, y1, x2, y2, color);
    } else if(starts_word(line, "swatch")) {
        char *q = trim(line + strlen("swatch"));
        char label[512];
        char x[128] = "0";
        char y[128] = "0";
        char size[128] = "ScaleUIPx(48)";
        char color[256] = "GetThemeButton()";
        char text_color[256] = "GetThemeText()";

        if(!parse_label_token(&q, label, sizeof(label)))
            die("%s:%d: expected swatch label", file->path, line_no);
        q = trim(q);
        read_prop_value(q, "x", x, sizeof(x));
        read_prop_value(q, "y", y, sizeof(y));
        read_prop_value(q, "size", size, sizeof(size));
        read_prop_value(q, "color", color, sizeof(color));
        read_prop_value(q, "text_color", text_color, sizeof(text_color));
        add_body(file,
                 "    DrawRectangleRec((Rectangle){%s, %s, %s, %s}, %s);",
                 x, y, size, size, color);
        add_body(file,
                 "    DrawRectangleLinesEx((Rectangle){%s, %s, %s, %s}, 1, %s);",
                 x, y, size, size, text_color);
        add_body(file, "    DrawUIText(%s, (%s) + (%s) + ScaleUIPx(10), (%s) + ScaleUIPx(10), UI_TEXT_16, %s);",
                 label, x, size, y, text_color);
    } else if(starts_word(line, "on key_down")) {
        char *q = trim(line + strlen("on key_down"));
        size_t n = strlen(q);

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected key_down block ending with {",
                file->path, line_no);
        q[n - 1] = '\0';
        add_body(file, "    if(UIKeyDown(%s)) {", trim(q));
    } else if(starts_word(line, "on key")) {
        char *q = trim(line + strlen("on key"));
        size_t n = strlen(q);

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected key block ending with {", file->path,
                line_no);
        q[n - 1] = '\0';
        add_body(file, "    if(UIKeyPressed(%s)) {", trim(q));
    } else if(starts_else_if(line)) {
        char *q = starts_word(line, "else if")
                      ? trim(line + strlen("else if"))
                      : trim(line + strlen("} else if"));
        size_t n = strlen(q);

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected else if condition ending with {",
                file->path, line_no);
        q[n - 1] = '\0';
        q = trim(q);
        if(q[0] == '\0')
            die("%s:%d: expected else if condition", file->path, line_no);
        add_body(file, "    } else if(%s) {", q);
    } else if(line_is_else(line)) {
        add_body(file, "    } else {");
    } else if(starts_word(line, "guard")) {
        char *q = trim(line + strlen("guard"));

        if(q[0] == '\0')
            die("%s:%d: expected guard condition", file->path, line_no);
        add_body(file, "    if(%s)", q);
        add_body(file, "        return;");
    } else if(starts_word(line, "return")) {
        char *q = trim(line + strlen("return"));

        if(q[0] == '\0') {
            add_body(file, "    return;");
        } else {
            if(strchr(q, '(') != NULL)
                emit_source_push(file, line_no);
            add_body(file, "    return %s;", q);
            if(strchr(q, '(') != NULL)
                emit_source_pop(file);
        }
    } else if(strcmp(line, "continue") == 0) {
        add_body(file, "    continue;");
    } else if(strcmp(line, "break") == 0) {
        add_body(file, "    break;");
    } else if(starts_word(line, "var")) {
        die("%s:%d: 'var' syntax was removed; use 'name: type = value'",
            file->path, line_no);
    } else if(starts_word(line, "set")) {
        char *q = trim(line + strlen("set"));

        if(q[0] == '\0')
            die("%s:%d: expected assignment", file->path, line_no);
        if(strchr(q, '(') != NULL)
            emit_source_push(file, line_no);
        add_body(file, "    %s;", q);
        if(strchr(q, '(') != NULL)
            emit_source_pop(file);
    } else if(starts_word(line, "native")) {
        char *q = trim(line + strlen("native"));

        if(q[0] == '\0')
            die("%s:%d: expected native expression", file->path, line_no);
        add_body(file, "    %s;", q);
    } else if(starts_word(line, "c")) {
        char *q = trim(line + strlen("c"));

        if(q[0] == '\0')
            die("%s:%d: expected raw C line", file->path, line_no);
        add_body(file, "    %s", q);
    } else if(starts_word(line, "draw") || starts_word(line, "do") ||
              starts_word(line, "widget")) {
        char *q = starts_word(line, "draw")
                      ? trim(line + strlen("draw"))
                      : (starts_word(line, "widget")
                             ? trim(line + strlen("widget"))
                             : trim(line + strlen("do")));

        if(q[0] == '\0')
            die("%s:%d: expected expression", file->path, line_no);
        emit_source_push(file, line_no);
        emit_call(file, "    ", q, ";");
        emit_source_pop(file);
    } else if(starts_word(line, "advance")) {
        char *q = trim(line + strlen("advance"));
        char name[KC_NAME_MAX];

        if(!parse_ident(&q, name, sizeof(name)))
            die("%s:%d: expected variable name", file->path, line_no);
        q = trim(q);
        if(!starts_word(q, "by"))
            die("%s:%d: expected 'by' in advance statement", file->path,
                line_no);
        q = trim(q + strlen("by"));
        if(q[0] == '\0')
            die("%s:%d: expected advance expression", file->path, line_no);
        add_body(file, "    %s += %s;", name, q);
    } else if(starts_word(line, "clamp_min") ||
              starts_word(line, "clamp_max")) {
        int is_min = starts_word(line, "clamp_min");
        char *q = trim(line + strlen(is_min ? "clamp_min" : "clamp_max"));
        char name[KC_NAME_MAX];

        if(!parse_ident(&q, name, sizeof(name)))
            die("%s:%d: expected variable name", file->path, line_no);
        q = trim(q);
        if(q[0] == '\0')
            die("%s:%d: expected clamp expression", file->path, line_no);
        add_body(file, "    if(%s %c %s) {", name, is_min ? '<' : '>', q);
        add_body(file, "        %s = %s;", name, q);
        add_body(file, "    }");
    } else if(starts_word(line, "c_rect")) {
        char *q = trim(line + strlen("c_rect"));
        char name[KC_NAME_MAX];

        if(!parse_ident(&q, name, sizeof(name)))
            die("%s:%d: expected rectangle name", file->path, line_no);
        q = trim(q);
        if(*q != '=')
            die("%s:%d: expected '=' in rect statement", file->path,
                line_no);
        q = trim(q + 1);
        if(q[0] == '\0')
            die("%s:%d: expected rectangle expression list", file->path,
                line_no);
        add_body(file, "    Rectangle %s = (Rectangle){%s};", name, q);
    } else if(starts_word(line, "texture")) {
        char *q = trim(line + strlen("texture"));
        char name[KC_NAME_MAX];

        if(!parse_ident(&q, name, sizeof(name)))
            die("%s:%d: expected texture name", file->path, line_no);
        q = trim(q);
        if(*q != '=')
            die("%s:%d: expected '=' in texture statement", file->path,
                line_no);
        q = trim(q + 1);
        if(q[0] == '\0')
            die("%s:%d: expected texture expression", file->path, line_no);
        add_body(file, "    Texture2D %s = %s;", name, q);
    } else if(starts_word(line, "if")) {
        char *q = trim(line + strlen("if"));
        size_t n = strlen(q);

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected if condition ending with {", file->path,
                line_no);
        q[n - 1] = '\0';
        q = trim(q);
        if(q[0] == '\0')
            die("%s:%d: expected if condition", file->path, line_no);
        add_body(file, "    if(%s) {", q);
    } else if(starts_word(line, "for")) {
        char *q = trim(line + strlen("for"));
        size_t n = strlen(q);
        char *in;

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected for expression ending with {", file->path,
                line_no);
        q[n - 1] = '\0';
        q = trim(q);
        if(q[0] == '\0')
            die("%s:%d: expected for expression", file->path, line_no);
        in = strstr(q, " in ");
        if(in != NULL) {
            char name[KC_NAME_MAX];
            char *range;
            char *dots;

            *in = '\0';
            range = trim(in + strlen(" in "));
            dots = strstr(range, "..");
            if(!parse_ident(&q, name, sizeof(name)) || dots == NULL)
                die("%s:%d: expected for NAME in START..END", file->path,
                    line_no);
            *dots = '\0';
            add_body(file, "    for(int %s = %s; %s < %s; %s++) {",
                     name, trim(range), name, trim(dots + 2), name);
            return;
        }
        add_body(file, "    for(%s) {", q);
    } else if(starts_word(line, "while")) {
        char *q = trim(line + strlen("while"));
        size_t n = strlen(q);

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected while condition ending with {", file->path,
                line_no);
        q[n - 1] = '\0';
        q = trim(q);
        if(q[0] == '\0')
            die("%s:%d: expected while condition", file->path, line_no);
        add_body(file, "    while(%s) {", q);
    } else if(starts_word(line, "button")) {
        char *q = trim(line + strlen("button"));
        size_t n = strlen(q);
        char hit[KC_NAME_MAX];
        char label[512];
        char x[128] = "0";
        char y[128] = "0";
        char w[128] = "ScaleUIPx(120)";
        char h[128] = "ScaleUIPx(34)";
        char style[128] = "UI_BUTTON_STYLE_PRIMARY";

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected button arguments ending with {", file->path,
                line_no);
        q[n - 1] = '\0';
        q = trim(q);
        if(strchr(q, '(') != NULL && q[0] != '"') {
            snprintf(hit, sizeof(hit), "__kry_hit_%d", line_no);
            add_body_line(file, 0, "    int %s;", hit);
            emit_source_push(file, line_no);
            add_body_line(file, 0, "    %s = DrawUIGenericButton(%s);", hit, q);
            emit_source_pop(file);
            add_body_line(file, 0, "    if(%s) {", hit);
            return;
        }
        if(!parse_label_token(&q, label, sizeof(label)))
            die("%s:%d: expected button label", file->path, line_no);
        q = trim(q);
        read_prop_value(q, "x", x, sizeof(x));
        read_prop_value(q, "y", y, sizeof(y));
        read_prop_value(q, "w", w, sizeof(w));
        read_prop_value(q, "h", h, sizeof(h));
        read_prop_value(q, "style", style, sizeof(style));
        snprintf(hit, sizeof(hit), "__kry_hit_%d", line_no);
        add_body_line(file, 0, "    int %s;", hit);
        emit_source_push(file, line_no);
        add_body_line(file, 0,
                      "    %s = DrawUIGenericButton(%s, %s, %s, %s, %s, %s, 0, NULL);",
                      hit, x, y, w, h, label, style);
        emit_source_pop(file);
        add_body_line(file, 0, "    if(%s) {", hit);
    } else if(starts_word(line, "event") || starts_word(line, "on")) {
        char *q = starts_word(line, "event")
                      ? trim(line + strlen("event"))
                      : trim(line + strlen("on"));
        size_t n = strlen(q);
        char hit[KC_NAME_MAX];

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected event expression ending with {", file->path,
                line_no);
        q[n - 1] = '\0';
        q = trim(q);
        snprintf(hit, sizeof(hit), "__kry_hit_%d", line_no);
        add_body_line(file, 0, "    int %s;", hit);
        emit_source_push(file, line_no);
        {
            char prefix[KC_NAME_MAX + 8];

            snprintf(prefix, sizeof(prefix), "    %s = ", hit);
            emit_call(file, prefix, q, ";");
            file->current->body_line[file->current->body_count - 1] = 0;
        }
        emit_source_pop(file);
        add_body_line(file, 0, "    if(%s) {", hit);
    } else if(starts_word(line, "icon_button")) {
        char *q = trim(line + strlen("icon_button"));
        size_t n = strlen(q);
        char hit[KC_NAME_MAX];

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected icon_button arguments ending with {",
                file->path, line_no);
        q[n - 1] = '\0';
        q = trim(q);
        snprintf(hit, sizeof(hit), "__kry_hit_%d", line_no);
        add_body_line(file, 0, "    int %s;", hit);
        emit_source_push(file, line_no);
        add_body_line(file, 0,
                      "    %s = DrawUIIconButton((UIIconButton){%s});",
                      hit, q);
        emit_source_pop(file);
        add_body_line(file, 0, "    if(%s) {", hit);
    } else if(line_is_mutation_statement(line)) {
        add_body(file, "    %s;", line);
    } else if(find_assignment_op(line) != NULL) {
        emit_assignment(file, line_no, line);
    } else {
        size_t n = strlen(line);

        if(n > 2 && line[n - 1] == ')' && strchr(line, '(') != NULL) {
            die("%s:%d: implicit call statements are not allowed; use 'do %s' or 'draw %s'",
                file->path, line_no, line, line);
        }
        if(line_is_assignment_statement(line)) {
            die("%s:%d: invalid assignment syntax: %s", file->path, line_no,
                line);
        }
        die("%s:%d: unknown statement: %s", file->path, line_no, line);
    }
}

static char *
read_text_file(const char *path)
{
    FILE *file;
    long size;
    char *text;

    file = fopen(path, "rb");
    if(file == NULL)
        die("%s: open failed: %s", path, strerror(errno));
    if(fseek(file, 0, SEEK_END) != 0)
        die("%s: seek failed", path);
    size = ftell(file);
    if(size < 0 || size > KC_TEXT_MAX)
        die("%s: file too large", path);
    if(fseek(file, 0, SEEK_SET) != 0)
        die("%s: seek failed", path);
    text = calloc((size_t)size + 1, 1);
    if(text == NULL)
        die("out of memory");
    if(fread(text, 1, (size_t)size, file) != (size_t)size)
        die("%s: read failed", path);
    fclose(file);
    return text;
}

static void
parse_kry(KryFile *file)
{
    char *text;
    char *line_start;
    int line_no = 1;
    int depth = 0;
    int in_screen = 0;
    int in_app = 0;
    int in_state = 0;
    int in_state_decl = 0;
    int state_decl_depth = 0;
    int in_top_static_decl = 0;
    int top_static_decl_depth = 0;
    int in_struct = 0;
    int struct_is_public = 0;
    char struct_name[KC_NAME_MAX] = "";
    int in_enum = 0;
    int enum_is_public = 0;
    int in_c_block = 0;
    int in_raw = 0;
    char pending_stmt[KC_BODY_LINE_MAX * 4] = "";
    char pending_decl[KC_BODY_LINE_MAX * 4] = "";
    int pending_line = 0;
    int pending_delta = 0;
    int pending_is_block = 0;
    int pending_decl_line = 0;
    int pending_decl_delta = 0;
    int macro_depths[64];
    int macro_count = 0;

    file->text = read_text_file(file->path);
    text = file->text;
    line_start = text;
    for(char *p = text;; p++) {
        if(*p != '\n' && *p != '\0')
            continue;
        char saved = *p;
        char *line;
        char completed_decl[KC_BODY_LINE_MAX * 4];

        *p = '\0';
        line = trim(line_start);
        if(in_raw) {
            if(strcmp(line, "endraw") == 0) {
                in_raw = 0;
            } else if(in_screen && depth > 0) {
                add_body_line(file, line_no, "%s", line_start);
            } else {
                add_raw_line(file, line_start);
            }
            *p = saved;
            if(saved == '\0')
                break;
            line_start = p + 1;
            line_no++;
            continue;
        }
        if(line[0] != '\0' && (line[0] != '#' || line_is_hash_compile(line))) {
            int opens = 0;
            int closes = 0;

            if(pending_decl[0] != '\0') {
                append_statement_line(file, pending_decl,
                                      sizeof(pending_decl), line);
                pending_decl_delta += line_group_delta(line);
                if(pending_decl_delta > 0 || line_needs_continuation(line)) {
                    *p = saved;
                    if(saved == '\0')
                        break;
                    line_start = p + 1;
                    line_no++;
                    continue;
                }
                snprintf(completed_decl, sizeof(completed_decl), "%s",
                         pending_decl);
                line = completed_decl;
                pending_decl[0] = '\0';
                pending_decl_line = 0;
                pending_decl_delta = 0;
            } else if(!in_screen && depth == 0 &&
                      (starts_word(line, "fn") || starts_word(line, "pub") ||
                       starts_word(line, "screen") ||
                       starts_word(line, "preview") ||
                       starts_word(line, "page")) &&
                      (line_group_delta(line) > 0 ||
                       line_needs_continuation(line))) {
                append_statement_line(file, pending_decl,
                                      sizeof(pending_decl), line);
                pending_decl_line = line_no;
                pending_decl_delta = line_group_delta(line);
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
            }
            count_line_braces(line, &opens, &closes);
            if(in_struct) {
                if(line_is_close(line)) {
                    emit_struct_end(file, struct_is_public, struct_name);
                    in_struct = 0;
                    struct_is_public = 0;
                    struct_name[0] = '\0';
                    depth += opens;
                    depth -= closes;
                    if(depth < 0)
                        die("%s:%d: unexpected }", file->path, line_no);
                    *p = saved;
                    if(saved == '\0')
                        break;
                    line_start = p + 1;
                    line_no++;
                    continue;
                }
                emit_struct_field(file, line_no, struct_is_public, line);
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
            } else if(in_enum) {
                if(line_is_close(line)) {
                    emit_enum_end(file, enum_is_public);
                    in_enum = 0;
                    enum_is_public = 0;
                    depth += opens;
                    depth -= closes;
                    if(depth < 0)
                        die("%s:%d: unexpected }", file->path, line_no);
                    *p = saved;
                    if(saved == '\0')
                        break;
                    line_start = p + 1;
                    line_no++;
                    continue;
                }
                emit_enum_item(file, line_no, enum_is_public, line);
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
            } else if(in_c_block) {
                if(line_is_close(line)) {
                    in_c_block = 0;
                    depth += opens;
                    depth -= closes;
                    if(depth < 0)
                        die("%s:%d: unexpected }", file->path, line_no);
                    *p = saved;
                    if(saved == '\0')
                        break;
                    line_start = p + 1;
                    line_no++;
                    continue;
                }
                emit_c_block_line(file, line_no, line);
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
            } else if(in_app) {
                if(!(line_is_close(line) && depth == 1)) {
                    if(starts_word(line, "size")) {
                        char *q = trim(line + strlen("size"));

                        file->app_width = atoi(q);
                        while(*q != '\0' && !isspace((unsigned char)*q))
                            q++;
                        file->app_height = atoi(trim(q));
                    } else if(starts_word(line, "fps")) {
                        file->app_fps = atoi(trim(line + strlen("fps")));
                    } else if(starts_word(line, "font")) {
                        char *q = trim(line + strlen("font"));

                        if(strcmp(q, "examples") == 0)
                            file->app_font_examples = 1;
                    } else if(starts_word(line, "theme")) {
                        char *q = trim(line + strlen("theme"));
                        char mode[KC_NAME_MAX] = "";

                        if(!parse_ident(&q, file->app_theme,
                                        sizeof(file->app_theme)))
                            die("%s:%d: expected theme id", file->path,
                                line_no);
                        parse_ident(&q, mode, sizeof(mode));
                        file->app_dark_mode = strcmp(mode, "dark") == 0;
                    } else {
                        die("%s:%d: unknown app property: %s",
                            file->path, line_no, line);
                    }
                }
                depth += opens;
                depth -= closes;
                if(depth < 0)
                    die("%s:%d: unexpected }", file->path, line_no);
                if(depth == 0)
                    in_app = 0;
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
            } else if(in_state) {
                if(!(line_is_close(line) && depth == 1)) {
                    int decl_opens = 0;
                    int decl_closes = 0;

                    file->current_line = line_no;
                    count_line_braces(line, &decl_opens, &decl_closes);
                    if(in_state_decl) {
                        state_decl_depth += decl_opens;
                        state_decl_depth -= decl_closes;
                        add_state_continuation_line(file, line,
                                                    state_decl_depth <= 0);
                        if(state_decl_depth <= 0) {
                            in_state_decl = 0;
                            state_decl_depth = 0;
                        }
                    } else if(strchr(line, '=') != NULL &&
                              decl_opens > decl_closes) {
                        emit_state_decl_start(file, line_no, line);
                        in_state_decl = 1;
                        state_decl_depth = decl_opens - decl_closes;
                    } else {
                        emit_state_decl(file, line_no, line);
                    }
                    file->current_line = 0;
                }
                depth += opens;
                depth -= closes;
                if(depth < 0)
                    die("%s:%d: unexpected }", file->path, line_no);
                if(depth == 0)
                    in_state = 0;
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
            } else if(in_top_static_decl) {
                int decl_opens = 0;
                int decl_closes = 0;

                count_line_braces(line, &decl_opens, &decl_closes);
                top_static_decl_depth += decl_opens;
                top_static_decl_depth -= decl_closes;
                add_state_continuation_line(file, line,
                                            top_static_decl_depth <= 0);
                if(top_static_decl_depth <= 0) {
                    in_top_static_decl = 0;
                    top_static_decl_depth = 0;
                }
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
            } else if(!in_screen && strstr(line, "::") != NULL) {
                char *op = strstr(line, "::");
                char name[KC_NAME_MAX];
                char *lhs;
                char *rhs;

                if(depth != 0)
                    die("%s:%d: compile-time constant must be top-level",
                        file->path, line_no);
                *op = '\0';
                lhs = trim(line);
                rhs = trim(op + 2);
                snprintf(name, sizeof(name), "%s", lhs);
                add_const(file, line_no, name, rhs);
            } else if(!in_screen && starts_word(line, "app")) {
                char *q = trim(line + strlen("app"));

                if(depth != 0)
                    die("%s:%d: nested app blocks are not supported",
                        file->path, line_no);
                if(!parse_quoted(&q, file->app_title,
                                 sizeof(file->app_title)))
                    die("%s:%d: expected app title", file->path, line_no);
                in_app = 1;
                depth += opens;
                depth -= closes;
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
            } else if(starts_word(line, "state")) {
                if(depth != 0)
                    die("%s:%d: nested state blocks are not supported",
                        file->path, line_no);
                in_state = 1;
                depth += opens;
                depth -= closes;
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
            } else if(!in_screen && starts_word(line, "static")) {
                int decl_opens = 0;
                int decl_closes = 0;

                if(depth != 0)
                    die("%s:%d: static declaration must be top-level",
                        file->path, line_no);
                count_line_braces(line, &decl_opens, &decl_closes);
                if(strchr(line, '=') != NULL && decl_opens > decl_closes) {
                    emit_top_static_decl_start(file, line_no, line);
                    in_top_static_decl = 1;
                    top_static_decl_depth = decl_opens - decl_closes;
                } else {
                    emit_top_static_decl(file, line_no, line);
                }
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
            } else if(!in_screen &&
                      (starts_word(line, "struct") ||
                       (starts_word(line, "pub") &&
                        starts_word(trim(line + strlen("pub")), "struct")))) {
                if(depth != 0)
                    die("%s:%d: struct declaration must be top-level",
                        file->path, line_no);
                struct_is_public = starts_word(line, "pub");
                emit_struct_start(file, line_no, struct_is_public, line,
                                  struct_name, sizeof(struct_name));
                in_struct = 1;
                depth += opens;
                depth -= closes;
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
            } else if(!in_screen &&
                      (starts_word(line, "enum") ||
                       (starts_word(line, "pub") &&
                        starts_word(trim(line + strlen("pub")), "enum")))) {
                if(depth != 0)
                    die("%s:%d: enum declaration must be top-level",
                        file->path, line_no);
                enum_is_public = starts_word(line, "pub");
                emit_enum_start(file, line_no, enum_is_public, line);
                in_enum = 1;
                depth += opens;
                depth -= closes;
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
            } else if(starts_word(line, "raw")) {
                in_raw = 1;
            } else if(!in_screen && starts_word(line, "c")) {
                char *q = trim(line + strlen("c"));

                if(depth != 0)
                    die("%s:%d: c block must be top-level", file->path,
                        line_no);
                if(strcmp(q, "{") != 0)
                    die("%s:%d: expected c block", file->path, line_no);
                in_c_block = 1;
            } else if(starts_word(line, "mod")) {
                char *q = trim(line + strlen("mod"));
                char module[KC_NAME_MAX];

                if(depth != 0)
                    die("%s:%d: mod must be top-level", file->path, line_no);
                if(file->module[0] != '\0')
                    die("%s:%d: duplicate mod declaration", file->path, line_no);
                if(!parse_module_name(&q, module, sizeof(module)) ||
                   trim(q)[0] != '\0')
                    die("%s:%d: expected module name", file->path, line_no);
                module_symbol(file->module, sizeof(file->module), module);
            } else if(starts_word(line, "cimport")) {
                char *q = line + strlen("cimport");

                if(file->include_count >= KC_INCLUDE_MAX)
                    die("%s:%d: too many includes", file->path, line_no);
                if(!parse_quoted(&q, file->includes[file->include_count],
                                 sizeof(file->includes[file->include_count])))
                    die("%s:%d: expected quoted C header path", file->path,
                        line_no);
                file->include_count++;
            } else if(starts_word(line, "import")) {
                char *q = line + strlen("import");
                char import_path[KC_PATH_MAX];
                char import_base[KC_PATH_MAX];

                if(file->include_count >= KC_INCLUDE_MAX)
                    die("%s:%d: too many imports", file->path, line_no);
                if(!parse_quoted(&q, import_path, sizeof(import_path)))
                    die("%s:%d: expected quoted import path", file->path,
                        line_no);
                strip_kry_ext(import_base, sizeof(import_base), import_path);
                snprintf(file->includes[file->include_count],
                         sizeof(file->includes[file->include_count]),
                         "%s.h", import_base);
                file->include_count++;
            } else if(starts_word(line, "use") ||
                      strstr(line, ":= use") != NULL) {
                char *q = line;
                char alias[KC_NAME_MAX] = "";
                char module[KC_NAME_MAX];
                char module_sym[KC_NAME_MAX];
                char header[KC_PATH_MAX];
                char *op;

                if(file->use_count >= KC_USE_MAX)
                    die("%s:%d: too many uses", file->path, line_no);
                if(file->include_count >= KC_INCLUDE_MAX)
                    die("%s:%d: too many includes", file->path, line_no);
                op = find_inferred_decl_op(line);
                if(op != NULL) {
                    *op = '\0';
                    q = trim(line);
                    if(!is_ident_text(q))
                        die("%s:%d: invalid use binding '%s'", file->path,
                            line_no, q);
                    snprintf(alias, sizeof(alias), "%s", q);
                    q = trim(op + 2);
                    if(!starts_word(q, "use"))
                        die("%s:%d: expected use after ':='", file->path,
                            line_no);
                    q = trim(q + strlen("use"));
                } else {
                    q = trim(q + strlen("use"));
                }
                if(!parse_quoted(&q, module, sizeof(module)) ||
                   trim(q)[0] != '\0')
                    die("%s:%d: expected quoted module name", file->path,
                        line_no);
                module_symbol(module_sym, sizeof(module_sym), module);
                if(alias[0] == '\0')
                    snprintf(alias, sizeof(alias), "%s", module_sym);
                snprintf(file->use_aliases[file->use_count],
                         sizeof(file->use_aliases[file->use_count]),
                         "%s", alias);
                snprintf(file->use_modules[file->use_count],
                         sizeof(file->use_modules[file->use_count]),
                         "%s", module_sym);
                file->use_count++;
                module_header(header, sizeof(header), module);
                snprintf(file->includes[file->include_count],
                         sizeof(file->includes[file->include_count]),
                         "%s", header);
                file->include_count++;
            } else if(!in_screen &&
                      (starts_word(line, "screen") ||
                       starts_word(line, "preview") ||
                       starts_word(line, "page") ||
                       starts_word(line, "fn") ||
                       starts_word(line, "pub"))) {
                int is_pub = 0;
                int is_fn;
                KryFunction *fn;
                char *decl = line;
                char *q;

                if(starts_word(decl, "pub")) {
                    is_pub = 1;
                    decl = trim(decl + strlen("pub"));
                    if(!starts_word(decl, "fn"))
                        die("%s:%d: expected fn after pub", file->path,
                            line_no);
                }
                is_fn = starts_word(decl, "fn");
                q = starts_word(decl, "screen")
                        ? decl + strlen("screen")
                        : (starts_word(decl, "preview")
                               ? decl + strlen("preview")
                        : (starts_word(decl, "page")
                               ? decl + strlen("page")
                               : decl + strlen("fn")));

                if(depth != 0)
                    die("%s:%d: nested functions are not supported",
                        file->path, line_no);
                fn = add_function(file);
                file->current = fn;
                if(!parse_ident(&q, fn->screen, sizeof(fn->screen)))
                    die("%s:%d: expected screen name", file->path, line_no);
                fn->exact_name = is_fn;
                fn->is_public = is_pub || !is_fn;
                q = trim(q);
                if(q[0] == '(') {
                    char *end = strrchr(q, ')');

                    if(end == NULL)
                        die("%s:%d: expected ) in page argument list",
                            file->path, line_no);
                    *end = '\0';
                    convert_arg_list(fn->args, sizeof(fn->args), trim(q + 1));
                    q = trim(end + 1);
                }
                if(q[0] == '-' && q[1] == '>') {
                    q = trim(q + 2);
                    if(q[0] == '\0')
                        die("%s:%d: expected return type", file->path,
                            line_no);
                    if(q[strlen(q) - 1] == '{')
                        q[strlen(q) - 1] = '\0';
                    snprintf(fn->return_type, sizeof(fn->return_type),
                             "%s", trim(q));
                }
                in_screen = 1;
            } else if(in_screen && depth > 0 && starts_word(line, "args")) {
                char *q = trim(line + strlen("args"));
                KryFunction *fn = file->current;

                if(fn == NULL)
                    die("%s:%d: args outside function", file->path, line_no);
                snprintf(fn->args, sizeof(fn->args), "%s", q);
            } else if(in_screen && depth > 0 && starts_word(line, "returns")) {
                char *q = trim(line + strlen("returns"));
                KryFunction *fn = file->current;

                if(fn == NULL)
                    die("%s:%d: returns outside function", file->path, line_no);
                if(q[0] == '\0')
                    die("%s:%d: expected return type", file->path, line_no);
                snprintf(fn->return_type, sizeof(fn->return_type), "%s", q);
            } else if(in_screen && depth > 0 && starts_word(line, "call")) {
                char *q = trim(line + strlen("call"));
                KryFunction *fn = file->current;

                if(fn == NULL)
                    die("%s:%d: call outside function", file->path, line_no);
                if(fn->call_count >= KC_CALL_MAX)
                    die("%s:%d: too many calls", file->path, line_no);
                if(q[0] == '\0')
                    die("%s:%d: expected call expression", file->path,
                        line_no);
                snprintf(fn->calls[fn->call_count],
                         sizeof(fn->calls[fn->call_count]), "%s", q);
                fn->call_count++;
            } else if(in_screen && depth > 0) {
                if(line_is_close(line) && depth == 1 &&
                   !(macro_count > 0 &&
                     depth == macro_depths[macro_count - 1])) {
                    /* closing the page */
                } else {
                    int stmt_line = line_no;
                    char *stmt = line;
                    int is_block_stmt = line_starts_block_statement(line);
                    int stmt_delta = line_delim_delta(line);
                    int group_delta = line_group_delta(line);
                    int parsed_pending = 0;
                    char *macro_condition = NULL;

                    if(pending_stmt[0] == '\0' && macro_count > 0 &&
                       line_is_close(line) &&
                       depth == macro_depths[macro_count - 1]) {
                        add_body(file, "    #endif");
                        macro_count--;
                        if(depth < 0)
                            die("%s:%d: unexpected }", file->path, line_no);
                        *p = saved;
                        if(saved == '\0')
                            break;
                        line_start = p + 1;
                        line_no++;
                        continue;
                    }
                    if(pending_stmt[0] == '\0' &&
                       parse_hash_if_start(line, &macro_condition)) {
                        char expanded[KC_BODY_LINE_MAX];

                        expand_compile_expr(expanded, sizeof(expanded), file,
                                            macro_condition);
                        if(strncmp(line, "} #else_if", 10) == 0 ||
                           strncmp(line, "#else_if", 8) == 0) {
                            if(macro_count <= 0 ||
                               depth != macro_depths[macro_count - 1])
                                die("%s:%d: #else_if without matching #if",
                                    file->path, line_no);
                            add_body(file, "    #elif %s", expanded);
                        } else {
                            if(macro_count >= (int)(sizeof(macro_depths) /
                                                   sizeof(macro_depths[0])))
                                die("%s:%d: too many nested #if blocks",
                                    file->path, line_no);
                            macro_depths[macro_count++] = depth;
                            add_body(file, "    #if %s", expanded);
                        }
                        /* } #else { keeps the same compile-time block depth. */
                        if(depth < 0)
                            die("%s:%d: unexpected }", file->path, line_no);
                        *p = saved;
                        if(saved == '\0')
                            break;
                        line_start = p + 1;
                        line_no++;
                        continue;
                    }
                    if(pending_stmt[0] == '\0' && line_is_hash_else(line)) {
                        if(macro_count <= 0 ||
                           depth != macro_depths[macro_count - 1])
                            die("%s:%d: #else without matching #if",
                                file->path, line_no);
                        add_body(file, "    #else");
                        if(depth < 0)
                            die("%s:%d: unexpected }", file->path, line_no);
                        *p = saved;
                        if(saved == '\0')
                            break;
                        line_start = p + 1;
                        line_no++;
                        continue;
                    }

                    if(pending_stmt[0] != '\0') {
                        append_statement_line(file, pending_stmt,
                                              sizeof(pending_stmt), line);
                        pending_delta += pending_is_block ? group_delta
                                                           : stmt_delta;
                        if(pending_delta > 0 || line_needs_continuation(line)) {
                            depth += opens;
                            depth -= closes;
                            if(depth < 0)
                                die("%s:%d: unexpected }", file->path, line_no);
                            *p = saved;
                            if(saved == '\0')
                                break;
                            line_start = p + 1;
                            line_no++;
                            continue;
                        }
                        stmt = pending_stmt;
                        stmt_line = pending_line;
                        parsed_pending = 1;
                    } else if((is_block_stmt ? group_delta : stmt_delta) > 0 ||
                              line_needs_continuation(line)) {
                        append_statement_line(file, pending_stmt,
                                              sizeof(pending_stmt), line);
                        pending_line = line_no;
                        pending_delta = is_block_stmt ? group_delta
                                                      : stmt_delta;
                        pending_is_block = is_block_stmt;
                        depth += opens;
                        depth -= closes;
                        if(depth < 0)
                            die("%s:%d: unexpected }", file->path, line_no);
                        *p = saved;
                        if(saved == '\0')
                            break;
                        line_start = p + 1;
                        line_no++;
                        continue;
                    }

                    file->current_line = stmt_line;
                    parse_statement(file, stmt_line, stmt);
                    file->current_line = 0;
                    if(parsed_pending) {
                        pending_stmt[0] = '\0';
                        pending_line = 0;
                        pending_delta = 0;
                        pending_is_block = 0;
                    }
                }
            } else {
                die("%s:%d: unknown top-level statement: %s",
                    file->path, line_no, line);
            }
            depth += opens;
            depth -= closes;
            if(depth < 0)
                die("%s:%d: unexpected }", file->path, line_no);
            if(depth == 0) {
                in_screen = 0;
                file->current = NULL;
            }
        }
        *p = saved;
        if(saved == '\0')
            break;
        line_start = p + 1;
        line_no++;
    }
    if(file->function_count == 0)
        die("%s: missing screen declaration", file->path);
    if(in_raw)
        die("%s: unterminated raw block", file->path);
    if(in_c_block)
        die("%s: unterminated c block", file->path);
    if(in_struct)
        die("%s:%d: unterminated struct declaration", file->path, line_no);
    if(macro_count != 0)
        die("%s:%d: unterminated #if block", file->path, line_no);
    if(pending_decl[0] != '\0')
        die("%s:%d: unterminated function declaration", file->path,
            pending_decl_line);
    if(pending_stmt[0] != '\0')
        die("%s:%d: unterminated continued statement", file->path,
            pending_line);
    if(depth != 0)
        die("%s:%d: unbalanced braces", file->path, line_no);
}

static const char *
relative_path(const char *root, const char *path)
{
    size_t n;

    if(root == NULL || root[0] == '\0')
        return path;
    n = strlen(root);
    if(strncmp(path, root, n) == 0 && path[n] == '/')
        return path + n + 1;
    return path;
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
strip_kry_ext(char *dst, size_t dst_size, const char *path)
{
    size_t len;

    if(path == NULL) {
        if(dst_size > 0)
            dst[0] = '\0';
        return;
    }
    len = strlen(path);
    if(len > 4 && strcmp(path + len - 4, ".kry") == 0)
        len -= 4;
    if(len >= dst_size)
        len = dst_size - 1;
    memcpy(dst, path, len);
    dst[len] = '\0';
}

static void
mkdir_parent(const char *path)
{
    char tmp[KC_PATH_MAX];

    snprintf(tmp, sizeof(tmp), "%s", path);
    for(char *p = tmp + 1; *p != '\0'; p++) {
        if(*p != '/')
            continue;
        *p = '\0';
        if(mkdir(tmp, 0755) != 0 && errno != EEXIST)
            die("%s: mkdir failed: %s", tmp, strerror(errno));
        *p = '/';
    }
}

static void
header_guard(char *dst, size_t dst_size, const char *rel)
{
    size_t n = 0;

    if(dst_size > 2) {
        dst[n++] = 'K';
        dst[n++] = '_';
    }
    for(const char *p = rel; *p != '\0' && n + 1 < dst_size; p++) {
        unsigned char ch = (unsigned char)*p;

        if((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))
            dst[n++] = (char)ch;
        else if(ch >= 'a' && ch <= 'z')
            dst[n++] = (char)(ch - 'a' + 'A');
        else
            dst[n++] = '_';
    }
    if(n + 7 < dst_size) {
        memcpy(dst + n, "_H", 3);
        n += 2;
    }
    dst[n] = '\0';
}

static const char *
skip_indent(const char *line)
{
    while(*line == ' ' || *line == '\t')
        line++;
    return line;
}

static int
brace_delta(const char *line)
{
    int delta = 0;

    if(line[0] == '#')
        return 0;
    for(const char *p = line; *p != '\0'; p++) {
        if(*p == '{')
            delta++;
        else if(*p == '}')
            delta--;
    }
    return delta;
}

static int
kry_function_c_name(char *dst, size_t dst_size, const KryFile *file,
                    const KryFunction *fn)
{
    char base[KC_NAME_MAX + 16];

    if(fn->exact_name)
        snprintf(base, sizeof(base), "%s", fn->screen);
    else
        snprintf(base, sizeof(base), "%s_kry_draw", fn->screen);
    if(file->module[0] != '\0') {
        snprintf(dst, dst_size, "%s_%s", file->module, base);
        return 1;
    }
    snprintf(dst, dst_size, "%s", base);
    return 1;
}

static const KryFunction *
find_local_function(const KryFile *file, const char *name)
{
    for(int i = 0; i < file->function_count; i++) {
        const KryFunction *fn = &file->functions[i];
        char base[KC_NAME_MAX + 16];

        if(fn->exact_name)
            snprintf(base, sizeof(base), "%s", fn->screen);
        else
            snprintf(base, sizeof(base), "%s_kry_draw", fn->screen);
        if(strcmp(name, base) == 0 || strcmp(name, fn->screen) == 0)
            return fn;
    }
    return NULL;
}

static int
module_for_alias(const KryFile *file, const char *alias, char *module,
                 size_t module_size)
{
    for(int i = 0; i < file->use_count; i++) {
        if(strcmp(alias, file->use_aliases[i]) == 0) {
            snprintf(module, module_size, "%s", file->use_modules[i]);
            return 1;
        }
    }
    return 0;
}

static void
rewrite_kry_expr(char *dst, size_t dst_size, const KryFile *file,
                 const char *src)
{
    size_t n = 0;
    int in_string = 0;
    int escaped = 0;

    if(dst_size == 0)
        return;
    for(const char *p = src; *p != '\0' && n + 1 < dst_size;) {
        if(in_string) {
            dst[n++] = *p;
            if(escaped) {
                escaped = 0;
            } else if(*p == '\\') {
                escaped = 1;
            } else if(*p == '"') {
                in_string = 0;
            }
            p++;
            continue;
        }
        if(*p == '"') {
            in_string = 1;
            dst[n++] = *p++;
            continue;
        }
        if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
           *p == '_') {
            char ident[KC_NAME_MAX];
            const char *start = p;
            const char *after;
            size_t ident_len = 0;

            while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                  (*p >= '0' && *p <= '9') || *p == '_') {
                if(ident_len + 1 < sizeof(ident))
                    ident[ident_len++] = *p;
                p++;
            }
            ident[ident_len] = '\0';
            after = p;
            while(*after == ' ' || *after == '\t')
                after++;
            if(strcmp(ident, "nil") == 0) {
                int written = snprintf(dst + n, dst_size - n, "NULL");

                if(written < 0)
                    written = 0;
                if((size_t)written >= dst_size - n)
                    n = dst_size - 1;
                else
                    n += (size_t)written;
                continue;
            }
            if(*after == '.') {
                const char *member_start;
                char member[KC_NAME_MAX];
                char module[KC_NAME_MAX];
                size_t member_len = 0;

                after++;
                while(*after == ' ' || *after == '\t')
                    after++;
                member_start = after;
                if(((*after >= 'A' && *after <= 'Z') ||
                    (*after >= 'a' && *after <= 'z') || *after == '_')) {
                    while((*after >= 'A' && *after <= 'Z') ||
                          (*after >= 'a' && *after <= 'z') ||
                          (*after >= '0' && *after <= '9') ||
                          *after == '_') {
                        if(member_len + 1 < sizeof(member))
                            member[member_len++] = *after;
                        after++;
                    }
                    member[member_len] = '\0';
                    if(module_for_alias(file, ident, module, sizeof(module))) {
                        int written = snprintf(dst + n, dst_size - n,
                                               "%s_%s", module, member);
                        if(written < 0)
                            written = 0;
                        if((size_t)written >= dst_size - n)
                            n = dst_size - 1;
                        else
                            n += (size_t)written;
                        p = after;
                        continue;
                    }
                    (void)member_start;
                }
            } else if(*after == '(') {
                const KryFunction *fn = NULL;

                if(start == src ||
                   (start[-1] != '.' && start[-1] != '>'))
                    fn = find_local_function(file, ident);

                if(fn != NULL) {
                    char cname[KC_NAME_MAX * 2];
                    int written;

                    kry_function_c_name(cname, sizeof(cname), file, fn);
                    written = snprintf(dst + n, dst_size - n, "%s", cname);
                    if(written < 0)
                        written = 0;
                    if((size_t)written >= dst_size - n)
                        n = dst_size - 1;
                    else
                        n += (size_t)written;
                    continue;
                }
            }
            while(start < p && n + 1 < dst_size)
                dst[n++] = *start++;
            continue;
        }
        dst[n++] = *p++;
    }
    dst[n] = '\0';
}

static void
write_body_line(FILE *out, const KryFile *file, const char *line, int *indent)
{
    const char *text = skip_indent(line);
    char rewritten[KC_BODY_LINE_MAX];
    int delta;

    if(text[0] == '}')
        (*indent)--;
    if(*indent < 0)
        *indent = 0;
    if(text[0] != '#') {
        for(int i = 0; i < *indent; i++)
            fputs("    ", out);
    }
    if(text[0] == '#') {
        fprintf(out, "%s\n", text);
    } else {
        rewrite_kry_expr(rewritten, sizeof(rewritten), file, text);
        fprintf(out, "%s\n", rewritten);
        text = rewritten;
    }
    delta = brace_delta(text);
    if(text[0] == '}')
        delta++;
    *indent += delta;
    if(*indent < 0)
        *indent = 0;
}

static void
function_name(char *dst, size_t dst_size, const KryFile *file,
              const KryFunction *fn)
{
    kry_function_c_name(dst, dst_size, file, fn);
}

static int
function_takes_rectangle(const KryFunction *fn)
{
    return fn != NULL && strstr(fn->args, "Rectangle") != NULL;
}

static void
write_app_main(FILE *out, const KryFile *file)
{
    const KryFunction *screen = NULL;
    char screen_name[KC_NAME_MAX + 16];
    char title[512];
    int width = file->app_width > 0 ? file->app_width : 800;
    int height = file->app_height > 0 ? file->app_height : 600;
    int fps = file->app_fps > 0 ? file->app_fps : 60;

    if(file->app_title[0] == '\0')
        return;
    for(int i = 0; i < file->function_count; i++) {
        if(file->functions[i].is_public && !file->functions[i].exact_name) {
            screen = &file->functions[i];
            break;
        }
    }
    if(screen == NULL)
        return;
    function_name(screen_name, sizeof(screen_name), file, screen);
    c_string_literal(title, sizeof(title), file->app_title);
    fprintf(out, "\nint\nmain(void)\n{\n");
    fprintf(out, "    InitWindow(%d, %d, %s);\n", width, height, title);
    fprintf(out, "    SetTargetFPS(%d);\n", fps);
    if(file->app_font_examples)
        fprintf(out, "    LoadExampleUIFont();\n");
    fprintf(out, "    InitUI(%d, %d, GetUIScale());\n", width, height);
    if(file->app_theme[0] != '\0')
        fprintf(out, "    SetCurrentTheme(%s, %d);\n",
                file->app_theme, file->app_dark_mode);
    fprintf(out, "    while(!WindowShouldClose()) {\n");
    fprintf(out, "        BeginDrawing();\n");
    fprintf(out, "        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());\n");
    if(function_takes_rectangle(screen))
        fprintf(out, "        %s((Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()});\n",
                screen_name);
    else
        fprintf(out, "        %s();\n", screen_name);
    fprintf(out, "        EndDrawing();\n");
    fprintf(out, "    }\n");
    if(file->app_font_examples)
        fprintf(out, "    UnloadExampleUIFont();\n");
    fprintf(out, "    CloseWindow();\n");
    fprintf(out, "    return 0;\n");
    fprintf(out, "}\n");
}

static void
write_generated(const KryFile *file, const char *root, const char *out_dir)
{
    const char *rel = relative_path(root, file->path);
    char gen_rel[KC_PATH_MAX];
    char crel[KC_PATH_MAX];
    char hrel[KC_PATH_MAX];
    char cpath[KC_PATH_MAX];
    char hpath[KC_PATH_MAX];
    char guard[KC_NAME_MAX * 2];
    char hbase[KC_PATH_MAX];
    FILE *out;

    strip_kry_ext(gen_rel, sizeof(gen_rel), rel);
    snprintf(crel, sizeof(crel), "%s.c", gen_rel);
    snprintf(hrel, sizeof(hrel), "%s.h", gen_rel);
    path_join(cpath, sizeof(cpath), out_dir, crel);
    path_join(hpath, sizeof(hpath), out_dir, hrel);
    mkdir_parent(cpath);
    mkdir_parent(hpath);
    header_guard(guard, sizeof(guard), gen_rel);
    snprintf(hbase, sizeof(hbase), "%s.h", gen_rel);

    out = fopen(hpath, "wb");
    if(out == NULL)
        die("%s: open failed: %s", hpath, strerror(errno));
    fprintf(out, "/* Generated by kc from %s. */\n", rel);
    fprintf(out, "#ifndef %s\n#define %s\n\n", guard, guard);
    for(int i = 0; i < file->include_count; i++)
        fprintf(out, "#include \"%s\"\n", file->includes[i]);
    if(file->include_count > 0)
        fputc('\n', out);
    for(int i = 0; i < file->public_type_count; i++)
        fprintf(out, "%s\n", file->public_types[i]);
    if(file->public_type_count > 0)
        fputc('\n', out);
    for(int i = 0; i < file->function_count; i++) {
        const KryFunction *fn = &file->functions[i];
        const char *args = fn->args[0] != '\0' ? fn->args : "void";
        const char *return_type = fn->return_type[0] != '\0'
                                      ? fn->return_type
                                      : "void";
        char name[KC_NAME_MAX + 16];

        if(!fn->is_public)
            continue;
        function_name(name, sizeof(name), file, fn);
        fprintf(out, "%s %s(%s);\n", return_type, name, args);
    }
    fputc('\n', out);
    fprintf(out, "#endif\n");
    fclose(out);

    out = fopen(cpath, "wb");
    if(out == NULL)
        die("%s: open failed: %s", cpath, strerror(errno));
    fprintf(out, "/* Generated by kc from %s. */\n", rel);
    fprintf(out, "#include \"%s\"\n", hbase);
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include \"ui_inspect.h\"\n");
    if(file->app_font_examples)
        fprintf(out, "#include \"example_ui_font.h\"\n");
    if(file->raw_count > 0) {
        fputc('\n', out);
        for(int i = 0; i < file->raw_count; i++)
            fprintf(out, "%s\n", file->raw[i]);
    }
    if(file->private_type_count > 0) {
        fputc('\n', out);
        for(int i = 0; i < file->private_type_count; i++)
            fprintf(out, "%s\n", file->private_types[i]);
    }
    for(int i = 0; i < file->function_count; i++) {
        const KryFunction *fn = &file->functions[i];
        const char *args = fn->args[0] != '\0' ? fn->args : "void";
        const char *return_type = fn->return_type[0] != '\0'
                                      ? fn->return_type
                                      : "void";
        char name[KC_NAME_MAX + 16];

        if(fn->is_public)
            continue;
        function_name(name, sizeof(name), file, fn);
        fprintf(out, "\nstatic %s %s(%s);\n", return_type, name, args);
    }
    if(file->state_count > 0) {
        fputc('\n', out);
        for(int i = 0; i < file->state_count; i++)
            fprintf(out, "%s\n", file->state[i]);
    }
    for(int i = 0; i < file->function_count; i++) {
        const KryFunction *fn = &file->functions[i];
        const char *args = fn->args[0] != '\0' ? fn->args : "void";
        const char *return_type = fn->return_type[0] != '\0'
                                      ? fn->return_type
                                      : "void";
        char name[KC_NAME_MAX + 16];
        int indent = 1;

        function_name(name, sizeof(name), file, fn);
        fprintf(out, "\n%s%s\n%s(%s)\n{\n",
                fn->is_public ? "" : "static ", return_type, name, args);
        for(int j = 0; j < fn->body_count; j++)
            write_body_line(out, file, fn->body[j], &indent);
        for(int j = 0; j < fn->call_count; j++) {
            const char *call = fn->calls[j];
            size_t len = strlen(call);

            for(int k = 0; k < indent; k++)
                fputs("    ", out);
            fprintf(out, "%s", call);
            if(len == 0 || call[len - 1] != ';')
                fputc(';', out);
            fputc('\n', out);
        }
        fprintf(out, "}\n");
    }
    if(!file->no_main)
        write_app_main(out, file);
    fclose(out);
}

static void
usage(void)
{
    fprintf(stderr, "usage: kc [--no-main] --root DIR -o DIR file.kry ...\n");
}

int
main(int argc, char **argv)
{
    const char *root = ".";
    const char *out_dir = NULL;
    int no_main = 0;
    int first_file = 0;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--root") == 0) {
            if(++i >= argc)
                die("--root needs a directory");
            root = argv[i];
        } else if(strcmp(argv[i], "-o") == 0) {
            if(++i >= argc)
                die("-o needs a directory");
            out_dir = argv[i];
        } else if(strcmp(argv[i], "--no-main") == 0) {
            no_main = 1;
        } else if(argv[i][0] == '-') {
            usage();
            return 1;
        } else {
            first_file = i;
            break;
        }
    }
    if(out_dir == NULL || first_file == 0) {
        usage();
        return 1;
    }
    for(int i = first_file; i < argc; i++) {
        KryFile *file;

        file = calloc(1, sizeof(*file));
        if(file == NULL)
            die("out of memory");
        file->path = argv[i];
        file->no_main = no_main;
        parse_kry(file);
        write_generated(file, root, out_dir);
        free(file->text);
        free(file);
    }
    return 0;
}
