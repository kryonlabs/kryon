#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "kir.h"
#include "kir_parse.h"
#include "k2c_lower.h"

#include "kc_internal.h"
#include "kc_ast.h"

static void resolve_use_module(char *dst, size_t dst_size,
                               const KryFile *file, const char *module);
static void add_raw_conditional_line(KryFile *file, const char *active_guard,
                                     const char *condition, const char *line);
static void emit_web_intrinsic_wrapper(KryFile *file, int line_no,
                                       const char *name, const char *args,
                                       const char *ret,
                                       const char *active_guard);
/* Ensure fn->body/body_line have room for one more statement, growing the
 * buffers geometrically. Body lines are heap-allocated char[KC_BODY_LINE_MAX]
 * each; there is no fixed per-function cap. */
static void
emit_source_push(KryFile *file, int line_no)
{
    char path[KC_PATH_MAX + 8];

    c_string_literal(path, sizeof(path),
                     file->display_path[0] != '\0' ? file->display_path
                                                    : file->path);
    add_body_line(file, 0, "    PushUIInspectSource(%s, %d);", path, line_no);
}

static void
emit_source_pop(KryFile *file)
{
    add_body_line(file, 0, "    PopUIInspectSource();");
}

static void
emit_call(KryFile *file, const char *prefix, const char *expr,
          const char *suffix)
{
    /*
     * Calls pass through verbatim. The compiler does not rewrite function
     * names: a widget is whatever library function the source calls, and
     * source-inspection wrapping is applied uniformly to statement calls.
     */
    add_body(file, "%s%s%s", prefix, expr, suffix);
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

static int
c_decl_type_allowed(const char *type)
{
    char first[KC_NAME_MAX];
    const char *p = type;
    size_t n = 0;

    while(*p == ' ' || *p == '\t')
        p++;
    while((isalnum((unsigned char)*p) || *p == '_') && n + 1 < sizeof(first))
        first[n++] = *p++;
    first[n] = '\0';
    return strcmp(first, "char") == 0 ||
           strcmp(first, "const") == 0 ||
           strcmp(first, "double") == 0 ||
           strcmp(first, "float") == 0 ||
           strcmp(first, "int") == 0 ||
           strcmp(first, "long") == 0 ||
           strcmp(first, "short") == 0 ||
           strcmp(first, "size_t") == 0 ||
           strcmp(first, "unsigned") == 0 ||
           strcmp(first, "void") == 0 ||
           (first[0] >= 'A' && first[0] <= 'Z') ||
           strstr(first, "_t") != NULL;
}

static int
line_is_c_uninit_decl(const char *line)
{
    char tmp[KC_BODY_LINE_MAX];
    char *s;
    char *name;
    char *type_end;
    char *array;

    if(line == NULL || line[0] == '\0')
        return 0;
    if(strpbrk(line, "=(){}:,") != NULL)
        return 0;
    snprintf(tmp, sizeof(tmp), "%s", line);
    s = trim(tmp);
    if(s[0] == '\0')
        return 0;
    array = strchr(s, '[');
    if(array != NULL) {
        name = array;
        while(name > s &&
              (isalnum((unsigned char)name[-1]) || name[-1] == '_'))
            name--;
        *array = '\0';
    } else {
        name = s + strlen(s);
        while(name > s &&
              (isalnum((unsigned char)name[-1]) || name[-1] == '_'))
            name--;
    }
    if(!is_ident_text(name))
        return 0;
    type_end = name;
    while(type_end > s && (type_end[-1] == ' ' || type_end[-1] == '\t'))
        *--type_end = '\0';
    while(type_end > s && type_end[-1] == '*')
        *--type_end = '\0';
    return c_decl_type_allowed(s);
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
    convert_var_decl_file(decl, sizeof(decl), name, type, file);
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
        die("%s:%d: 'let' syntax was removed; use 'name: type = value'",
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
    convert_var_decl_file(decl, sizeof(decl), name, trim(type), file);
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
    convert_var_decl_file(decl, sizeof(decl), name, type, file);
    {
        char rewritten[KC_BODY_LINE_MAX];

        rewrite_nil_tokens(rewritten, sizeof(rewritten), expr);
        snprintf(out, sizeof(out), "static %s = %s", decl, rewritten);
    }
    add_state_line(file, out);
    return 1;
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

static int split_hash_if_suffix(char *line, char **condition);

static int
extract_quoted_tag(KryFile *file, int line_no, char *text, const char *tag,
                   char *dst, size_t dst_size)
{
    char *p = strstr(text, tag);
    char *q;

    if(p == NULL)
        return 0;
    q = trim(p + strlen(tag));
    if(!parse_quoted(&q, dst, dst_size))
        die("%s:%d: expected quoted value after %s", file->path, line_no,
            tag);
    memmove(p, q, strlen(q) + 1);
    return 1;
}

static int
emit_colon_extern_decl(KryFile *file, int line_no, char *name, char *rhs,
                       const char *guard)
{
    char *q = trim(rhs);
    char *end;
    char *suffix;
    char *extern_tag;
    char *intrinsic_tag;
    char *condition = NULL;
    char args[512] = "";
    char ret[KC_NAME_MAX] = "void";
    char storage[KC_NAME_MAX] = "";
    char abi[KC_NAME_MAX] = "";
    char attr[KC_NAME_MAX] = "";
    char out[KC_BODY_LINE_MAX];

    if(q[0] != '(')
        return 0;
    split_hash_if_suffix(q, &condition);
    extract_quoted_tag(file, line_no, q, "#storage", storage,
                       sizeof(storage));
    extract_quoted_tag(file, line_no, q, "#abi", abi, sizeof(abi));
    extract_quoted_tag(file, line_no, q, "#attr", attr, sizeof(attr));
    extern_tag = strstr(q, "#extern");
    intrinsic_tag = strstr(q, "#intrinsic");
    if(extern_tag == NULL && intrinsic_tag == NULL)
        return 0;
    if(extern_tag != NULL && intrinsic_tag != NULL)
        die("%s:%d: extern declaration cannot also be intrinsic",
            file->path, line_no);
    if(!is_ident_text(name))
        die("%s:%d: invalid extern function name '%s'", file->path,
            line_no, name);
    end = strrchr(q, ')');
    if(end == NULL)
        die("%s:%d: expected ')' in extern function", file->path, line_no);
    *end = '\0';
    convert_arg_list_file(args, sizeof(args), trim(q + 1), file);

    suffix = trim(end + 1);
    if(suffix[0] == '-' && suffix[1] == '>') {
        char *tag;

        suffix = trim(suffix + 2);
        tag = strstr(suffix, "#extern");
        if(tag == NULL)
            tag = strstr(suffix, "#intrinsic");
        if(tag != NULL)
            *tag = '\0';
        suffix = trim(suffix);
        if(suffix[0] == '\0')
            die("%s:%d: expected extern return type", file->path, line_no);
        snprintf(ret, sizeof(ret), "%s", suffix);
    }

    if(intrinsic_tag != NULL) {
        char backend[KC_NAME_MAX];
        char active_guard[KC_BODY_LINE_MAX];
        char suffix_guard[KC_BODY_LINE_MAX] = "";
        char *b = trim(intrinsic_tag + strlen("#intrinsic"));

        if(!parse_quoted(&b, backend, sizeof(backend)) &&
           !parse_ident(&b, backend, sizeof(backend)))
            die("%s:%d: expected intrinsic backend", file->path, line_no);
        if(trim(b)[0] != '\0')
            die("%s:%d: unexpected intrinsic suffix", file->path, line_no);
        if(strcmp(backend, "web") != 0)
            die("%s:%d: unknown intrinsic backend '%s'",
                file->path, line_no, backend);
        if(condition != NULL && condition[0] != '\0')
            expand_compile_expr(suffix_guard, sizeof(suffix_guard), file,
                                condition);
        combine_compile_guards(active_guard, sizeof(active_guard), guard,
                               suffix_guard);
        emit_web_intrinsic_wrapper(file, line_no, name, args, ret,
                                   active_guard);
        return 1;
    }

    snprintf(out, sizeof(out), "%s%s%s%s%s %s(%s)%s%s;",
             storage[0] != '\0' ? storage : "",
             storage[0] != '\0' ? " " : "",
             ret,
             abi[0] != '\0' ? " " : "",
             abi[0] != '\0' ? abi : "",
             name,
             args[0] != '\0' ? args : "void",
             attr[0] != '\0' ? " " : "",
             attr[0] != '\0' ? attr : "");
    add_raw_conditional_line(file, guard, condition, out);
    return 1;
}

static int
emit_colon_function_decl(KryFile *file, int line_no, char *name, char *rhs,
                         const char *guard, int *in_screen)
{
    KryFunction *fn;
    char *q = trim(rhs);
    char *end;
    char *suffix;

    if(q[0] != '(')
        return 0;
    if(!is_ident_text(name))
        die("%s:%d: invalid function name '%s'", file->path, line_no, name);
    end = strrchr(q, ')');
    if(end == NULL)
        die("%s:%d: expected ')' in function declaration", file->path,
            line_no);
    *end = '\0';

    fn = add_function(file);
    file->current = fn;
    fn->exact_name = 1;
    fn->is_public = 1;
    snprintf(fn->screen, sizeof(fn->screen), "%s", name);
    convert_arg_list_file(fn->args, sizeof(fn->args), trim(q + 1), file);
    snprintf(fn->guard, sizeof(fn->guard), "%s", guard != NULL ? guard : "");

    suffix = trim(end + 1);
    if(suffix[0] == '-' && suffix[1] == '>') {
        char *ret = trim(suffix + 2);
        char *export_tag = strstr(ret, "#export");
        char *private_tag = strstr(ret, "#private");

        if(export_tag != NULL) {
            *export_tag = '\0';
            fn->global_name = 1;
        }
        if(private_tag != NULL) {
            *private_tag = '\0';
            fn->is_public = 0;
        }
        if(ret[strlen(ret) - 1] == '{')
            ret[strlen(ret) - 1] = '\0';
        ret = trim(ret);
        if(ret[0] == '\0')
            die("%s:%d: expected return type", file->path, line_no);
        strip_module_alias(fn->return_type, sizeof(fn->return_type), file, ret);
    } else {
        char *export_tag = strstr(suffix, "#export");
        char *private_tag = strstr(suffix, "#private");

        if(export_tag != NULL)
            fn->global_name = 1;
        if(private_tag != NULL)
            fn->is_public = 0;
    }
    *in_screen = 1;
    return 1;
}

static int
path_has_suffix(const char *path, const char *suffix)
{
    size_t path_len = strlen(path);
    size_t suffix_len = strlen(suffix);

    return path_len >= suffix_len &&
           strcmp(path + path_len - suffix_len, suffix) == 0;
}

static void
add_import_include(KryFile *file, int line_no, const char *header,
                   const char *guard)
{
    if(file->include_count >= KC_INCLUDE_MAX)
        die("%s:%d: too many imports", file->path, line_no);
    snprintf(file->includes[file->include_count],
             sizeof(file->includes[file->include_count]), "%s", header);
    snprintf(file->include_guards[file->include_count],
             sizeof(file->include_guards[file->include_count]), "%s",
             guard != NULL ? guard : "");
    file->include_count++;
}

static int
emit_import_decl(KryFile *file, int line_no, const char *alias, char *rhs,
                 const char *guard)
{
    char *q = trim(rhs);
    char module[KC_PATH_MAX];
    char header[KC_PATH_MAX];
    char out[KC_BODY_LINE_MAX];
    int private_import = 0;

    if(!starts_word(q, "#import"))
        return 0;
    q = trim(q + strlen("#import"));
    if(*q == '<') {
        if(alias != NULL && alias[0] != '\0')
            die("%s:%d: system header import cannot have an alias",
                file->path, line_no);
        if(!parse_c_header_token(&q, header, sizeof(header)))
            die("%s:%d: expected system header import path", file->path,
                line_no);
        q = trim(q);
        if(q[0] != '\0') {
            if(strcmp(q, "#private") != 0)
                die("%s:%d: unexpected import suffix", file->path, line_no);
        }
        snprintf(out, sizeof(out), "#include %s", header);
        add_raw_conditional_line(file, guard, NULL, out);
        return 1;
    }
    if(!parse_quoted(&q, module, sizeof(module)))
        die("%s:%d: expected import path", file->path, line_no);
    q = trim(q);
    if(q[0] != '\0') {
        if(strcmp(q, "#private") != 0)
            die("%s:%d: unexpected import suffix", file->path, line_no);
        private_import = 1;
    }

    if(alias == NULL || alias[0] == '\0') {
        if(!path_has_suffix(module, ".h")) {
            if(private_import)
                die("%s:%d: module import cannot be private",
                    file->path, line_no);
            if(file->use_count >= KC_USE_MAX)
                die("%s:%d: too many imports", file->path, line_no);
            resolve_use_module(file->use_modules[file->use_count],
                               sizeof(file->use_modules[file->use_count]),
                               file, module);
            snprintf(file->use_aliases[file->use_count],
                     sizeof(file->use_aliases[file->use_count]), "%s",
                     file->use_modules[file->use_count]);
            module_header(header, sizeof(header), module);
            add_import_include(file, line_no, header, guard);
            file->use_count++;
            return 1;
        }
        if(private_import) {
            snprintf(out, sizeof(out), "#include \"%s\"", module);
            add_raw_conditional_line(file, guard, NULL, out);
            return 1;
        }
        add_import_include(file, line_no, module, guard);
        return 1;
    }

    if(private_import)
        die("%s:%d: module import cannot be private", file->path, line_no);
    if(!is_ident_text(alias))
        die("%s:%d: invalid import alias '%s'", file->path, line_no, alias);
    if(file->use_count >= KC_USE_MAX)
        die("%s:%d: too many imports", file->path, line_no);
    resolve_use_module(file->use_modules[file->use_count],
                       sizeof(file->use_modules[file->use_count]), file,
                       module);
    snprintf(file->use_aliases[file->use_count],
             sizeof(file->use_aliases[file->use_count]), "%s", alias);
    module_header(header, sizeof(header), module);
    add_import_include(file, line_no, header, guard);
    file->use_count++;
    return 1;
}

static void
add_global_guard_line(KryFile *file, const char *guard, int is_public)
{
    char line[KC_BODY_LINE_MAX];

    if(guard == NULL || guard[0] == '\0')
        return;
    snprintf(line, sizeof(line), "#if %s", guard);
    add_global_line(file, is_public, line);
}

static void
add_global_guard_end(KryFile *file, const char *guard, int is_public)
{
    if(guard == NULL || guard[0] == '\0')
        return;
    add_global_line(file, is_public, "#endif");
}

static int
emit_colon_global_decl(KryFile *file, int line_no, const char *name, char *rhs,
                       const char *guard)
{
    char type_buf[KC_BODY_LINE_MAX];
    char *type;
    char *eq;
    char *global_tag;
    char *export_tag;
    char decl[512];
    char out[KC_BODY_LINE_MAX];
    int is_public = 0;

    global_tag = strstr(rhs, "#global");
    if(global_tag == NULL)
        return 0;
    *global_tag = '\0';
    export_tag = strstr(global_tag + strlen("#global"), "#export");
    if(export_tag != NULL)
        is_public = 1;
    if(!is_ident_text(name))
        die("%s:%d: invalid global variable name '%s'", file->path, line_no,
            name);
    snprintf(type_buf, sizeof(type_buf), "%s", trim(rhs));
    type = trim(type_buf);
    eq = strchr(type, '=');
    if(eq != NULL)
        *eq = '\0';
    if(type[0] == '\0')
        die("%s:%d: expected global type", file->path, line_no);

    convert_var_decl_file(decl, sizeof(decl), name, trim(type), file);
    if(eq != NULL) {
        char rewritten[KC_BODY_LINE_MAX];

        rewrite_nil_tokens(rewritten, sizeof(rewritten), trim(eq + 1));
        snprintf(out, sizeof(out), "%s = %s;", decl, rewritten);
    } else {
        snprintf(out, sizeof(out), "%s;", decl);
    }
    add_global_guard_line(file, guard, 0);
    add_global_line(file, 0, out);
    add_global_guard_end(file, guard, 0);
    if(is_public) {
        snprintf(out, sizeof(out), "extern %s;", decl);
        add_global_guard_line(file, guard, 1);
        add_global_line(file, 1, out);
        add_global_guard_end(file, guard, 1);
    }
    return 1;
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
    convert_var_decl_file(out, sizeof(out), name, type, file);
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
emit_enum_start(KryFile *file, int line_no, int is_public, char *line,
                char *name, size_t name_size)
{
    char *q = trim(line);
    size_t n;

    if(is_public)
        q = trim(q + strlen("pub"));
    q = trim(q + strlen("enum"));
    n = strlen(q);
    if(n == 0 || q[n - 1] != '{')
        die("%s:%d: expected '{' after enum name", file->path, line_no);
    q[n - 1] = '\0';
    q = trim(q);
    if(q[0] == '\0') {
        name[0] = '\0';
        add_type_line(file, is_public, "enum {");
        return;
    }
    if(!is_ident_text(q))
        die("%s:%d: invalid enum name '%s'", file->path, line_no, q);
    snprintf(name, name_size, "%s", q);
    add_type_line(file, is_public, "typedef enum %s {", name);
}

static void
emit_enum_end(KryFile *file, int is_public, const char *name)
{
    if(name != NULL && name[0] != '\0')
        add_type_line(file, is_public, "} %s;", name);
    else
        add_type_line(file, is_public, "};");
}

static void
emit_type_alias_value(KryFile *file, int line_no, int is_public,
                      const char *name, char *rhs)
{
    char out[KC_BODY_LINE_MAX];
    char *end;
    char *slot;
    size_t head_len;

    if(!is_ident_text(name))
        die("%s:%d: invalid type alias name '%s'", file->path, line_no,
            name);
    rhs = trim(rhs);
    if(rhs[0] == '\0')
        die("%s:%d: expected type alias value", file->path, line_no);
    end = rhs + strlen(rhs);
    while(end > rhs && isspace((unsigned char)end[-1]))
        *--end = '\0';
    if(end > rhs && end[-1] == ';')
        *--end = '\0';

    slot = strstr(rhs, "(*)");
    if(slot != NULL) {
        head_len = (size_t)(slot - rhs);
        snprintf(out, sizeof(out), "typedef %.*s(*%s)%s;",
                 (int)head_len, rhs, name, slot + 3);
    } else {
        snprintf(out, sizeof(out), "typedef %s %s;", rhs, name);
    }
    add_type_line(file, is_public, "%s", out);
}

static int
emit_colon_type_alias(KryFile *file, int line_no, const char *name, char *rhs,
                      const char *guard)
{
    char *type_tag;
    char *private_tag;
    int is_public = 1;

    type_tag = strstr(rhs, "#type");
    if(type_tag == NULL)
        return 0;
    *type_tag = '\0';
    private_tag = strstr(type_tag + strlen("#type"), "#private");
    if(private_tag != NULL)
        is_public = 0;
    add_guard_line(file, guard, is_public, 1, 0);
    emit_type_alias_value(file, line_no, is_public, name, rhs);
    if(guard != NULL && guard[0] != '\0')
        add_guard_end(file, is_public, 1, 0);
    return 1;
}

static int
strip_tag(char *text, const char *tag)
{
    char *p = strstr(text, tag);

    if(p == NULL)
        return 0;
    memmove(p, p + strlen(tag), strlen(p + strlen(tag)) + 1);
    return 1;
}

static void
parse_module_directive(KryFile *file, int line_no, char *line, int depth)
{
    char *q = trim(line + strlen("#module"));
    char module[KC_NAME_MAX];

    if(depth != 0)
        die("%s:%d: module directive must be top-level", file->path, line_no);
    if(file->module[0] != '\0')
        die("%s:%d: duplicate module directive", file->path, line_no);
    if(!parse_quoted(&q, module, sizeof(module)) || trim(q)[0] != '\0')
        die("%s:%d: expected quoted module name", file->path, line_no);
    module_symbol(file->module, sizeof(file->module), module);
}

static void
parse_output_directive(KryFile *file, int line_no, char *line, int depth)
{
    char *q = trim(line + strlen("#output"));
    char name[KC_NAME_MAX];

    if(depth != 0)
        die("%s:%d: output directive must be top-level", file->path, line_no);
    if(file->module_file[0] != '\0')
        die("%s:%d: duplicate output directive", file->path, line_no);
    if(!parse_quoted(&q, name, sizeof(name)) || trim(q)[0] != '\0')
        die("%s:%d: expected quoted output name", file->path, line_no);
    validate_output_name(file, line_no, name);
    snprintf(file->module_file, sizeof(file->module_file), "%s", name);
}

static void
emit_pragma_directive(KryFile *file, int line_no, char *line,
                      const KryMacroFrame *top_macros, int top_macro_count,
                      int depth)
{
    char guard[KC_BODY_LINE_MAX];
    char value[KC_BODY_LINE_MAX];
    char out[KC_BODY_LINE_MAX];
    char *q = trim(line + strlen("#pragma"));

    if(depth != 0)
        die("%s:%d: pragma directive must be top-level", file->path, line_no);
    if(!parse_quoted(&q, value, sizeof(value)) || trim(q)[0] != '\0')
        die("%s:%d: expected quoted pragma text", file->path, line_no);
    snprintf(out, sizeof(out), "#pragma %s", value);
    current_macro_guard(guard, sizeof(guard), top_macros, top_macro_count);
    add_raw_conditional_line(file, guard, NULL, out);
}

static void
emit_error_directive(KryFile *file, int line_no, char *line,
                     const KryMacroFrame *top_macros, int top_macro_count,
                     int depth)
{
    char guard[KC_BODY_LINE_MAX];
    char value[KC_BODY_LINE_MAX];
    char quoted[KC_BODY_LINE_MAX];
    char out[KC_BODY_LINE_MAX];
    char *q = trim(line + strlen("#error"));

    if(depth != 0)
        die("%s:%d: error directive must be top-level", file->path, line_no);
    if(!parse_quoted(&q, value, sizeof(value)) || trim(q)[0] != '\0')
        die("%s:%d: expected quoted error text", file->path, line_no);
    c_string_literal(quoted, sizeof(quoted), value);
    snprintf(out, sizeof(out), "#error %s", quoted);
    current_macro_guard(guard, sizeof(guard), top_macros, top_macro_count);
    add_raw_conditional_line(file, guard, NULL, out);
}

static void
begin_hash_enum(KryFile *file, int line_no, char *line,
                const KryMacroFrame *top_macros, int top_macro_count,
                int *enum_is_public, char *type_guard, size_t type_guard_size,
                char *enum_name, size_t enum_name_size)
{
    char decl[KC_BODY_LINE_MAX];
    char body[KC_BODY_LINE_MAX];
    int is_public;

    snprintf(body, sizeof(body), "%s", trim(line + strlen("#enum")));
    is_public = !strip_tag(body, "#private");
    snprintf(decl, sizeof(decl), "%senum %s",
             is_public ? "pub " : "", trim(body));
    *enum_is_public = is_public;
    current_macro_guard(type_guard, type_guard_size, top_macros,
                        top_macro_count);
    add_guard_line(file, type_guard, is_public, 1, 0);
    emit_enum_start(file, line_no, is_public, decl, enum_name,
                    enum_name_size);
}

static void
begin_colon_struct(KryFile *file, int line_no, const char *name, char *rhs,
                   const KryMacroFrame *top_macros, int top_macro_count,
                   int *struct_is_public, char *type_guard,
                   size_t type_guard_size, char *struct_name,
                   size_t struct_name_size)
{
    char decl[KC_BODY_LINE_MAX];
    char body[KC_BODY_LINE_MAX];
    int is_public;

    snprintf(body, sizeof(body), "%s", trim(rhs + strlen("struct")));
    is_public = !strip_tag(body, "#private");
    snprintf(decl, sizeof(decl), "%sstruct %s %s",
             is_public ? "pub " : "", name, trim(body));
    *struct_is_public = is_public;
    current_macro_guard(type_guard, type_guard_size, top_macros,
                        top_macro_count);
    add_guard_line(file, type_guard, is_public, 1, 0);
    emit_struct_start(file, line_no, is_public, decl, struct_name,
                      struct_name_size);
}

static void
begin_colon_enum(KryFile *file, int line_no, const char *name, char *rhs,
                 const KryMacroFrame *top_macros, int top_macro_count,
                 int *enum_is_public, char *type_guard,
                 size_t type_guard_size, char *enum_name,
                 size_t enum_name_size)
{
    char decl[KC_BODY_LINE_MAX];
    char body[KC_BODY_LINE_MAX];
    int is_public;

    snprintf(body, sizeof(body), "%s", trim(rhs + strlen("enum")));
    is_public = !strip_tag(body, "#private");
    snprintf(decl, sizeof(decl), "%senum %s %s",
             is_public ? "pub " : "", name, trim(body));
    *enum_is_public = is_public;
    current_macro_guard(type_guard, type_guard_size, top_macros,
                        top_macro_count);
    add_guard_line(file, type_guard, is_public, 1, 0);
    emit_enum_start(file, line_no, is_public, decl, enum_name,
                    enum_name_size);
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
           strncmp(line, "#import", 7) == 0 ||
           strncmp(line, "#module", 7) == 0 ||
           strncmp(line, "#output", 7) == 0 ||
           strncmp(line, "#enum", 5) == 0 ||
           strncmp(line, "#pragma", 7) == 0 ||
           strncmp(line, "#error", 6) == 0 ||
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
       strstr(line, "%=") != NULL || strstr(line, "&=") != NULL ||
       strstr(line, "|=") != NULL || strstr(line, "^=") != NULL ||
       strstr(line, "<<=") != NULL || strstr(line, ">>=") != NULL ||
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
            strstr(line, "%=") != NULL || strstr(line, "&=") != NULL ||
            strstr(line, "|=") != NULL || strstr(line, "^=") != NULL ||
            strstr(line, "<<=") != NULL || strstr(line, ">>=") != NULL ||
            strstr(line, "++") != NULL || strstr(line, "--") != NULL);
}

static void
count_line_braces(const char *line, int *opens, int *closes)
{
    int in_string = 0;
    int escaped = 0;
    const char *q = line;

    *opens = 0;
    *closes = 0;
    while(q != NULL && (*q == ' ' || *q == '\t'))
        q++;
    if(q != NULL && q[0] == '#')
        return;
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
           starts_word(line, "switch") ||
           starts_word(line, "for") ||
           starts_word(line, "while") ||
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
    if(starts_word(line, "case") || strcmp(skip_indent(line), "default:") == 0 ||
       line_is_goto_label(line, NULL, 0))
        return 0;
    end = line + strlen(line);
    while(end > line && isspace((unsigned char)end[-1]))
        end--;
    if(end == line)
        return 0;
    if(strchr(line, '=') == NULL && find_typed_decl_colon((char *)line) != NULL)
        return 0;
    if(end[-1] == ',')
        return 1;
    if(end[-1] == ';')
        return 1;
    if(end[-1] == '+' || end[-1] == '-') {
        if(end - line >= 2 && end[-2] == end[-1])
            return 0;
        return 1;
    }
    if(end[-1] == '*' && strstr(line, "->") != NULL)
        return 0;
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

static int
line_starts_continuation(const char *line)
{
    const char *q = line;

    if(q == NULL)
        return 0;
    while(*q == ' ' || *q == '\t')
        q++;
    if(q[0] == '?' || q[0] == ':' || q[0] == '.' || q[0] == ',' ||
       q[0] == '+' || q[0] == '/' || q[0] == '%')
        return 1;
    if(q[0] == '-' && q[1] != '>')
        return 1;
    if((q[0] == '&' && q[1] == '&') ||
       (q[0] == '|' && q[1] == '|') ||
       (q[0] == '=' && q[1] == '=') ||
       (q[0] == '!' && q[1] == '=') ||
       (q[0] == '<' && q[1] == '=') ||
       (q[0] == '>' && q[1] == '='))
        return 1;
    return 0;
}

static int
line_starts_char(const char *line, char c)
{
    const char *q = line;

    if(q == NULL)
        return 0;
    while(*q == ' ' || *q == '\t')
        q++;
    return q[0] == c;
}

static int
line_can_accept_leading_continuation(const char *line)
{
    return line != NULL &&
           (starts_word(line, "return") ||
            line_is_inferred_decl((char *)line) ||
            find_typed_decl_colon((char *)line) != NULL ||
            line_is_assignment_statement(line));
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
expand_compile_expr_depth(char *dst, size_t dst_size, const KryFile *file,
                          const char *src, int depth)
{
    size_t n = 0;
    int in_string = 0;
    int escaped = 0;

    if(dst_size == 0)
        return;
    if(depth > 32) {
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

                    expand_compile_expr_depth(expanded, sizeof(expanded), file,
                                              file->const_exprs[i], depth + 1);
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

void
expand_compile_expr(char *dst, size_t dst_size, const KryFile *file,
                    const char *src)
{
    expand_compile_expr_depth(dst, dst_size, file, src, 0);
}

static void
add_raw_conditional_line(KryFile *file, const char *active_guard,
                         const char *condition,
                         const char *line)
{
    char guard[KC_BODY_LINE_MAX] = "";

    if(condition != NULL && condition[0] != '\0') {
        char suffix_guard[KC_BODY_LINE_MAX];

        expand_compile_expr(suffix_guard, sizeof(suffix_guard), file,
                            condition);
        combine_compile_guards(guard, sizeof(guard), active_guard,
                               suffix_guard);
    } else {
        combine_compile_guards(guard, sizeof(guard), active_guard, "");
    }
    add_guard_line(file, guard, 0, 0, 0);
    add_raw_line(file, line);
    if(guard[0] != '\0')
        add_guard_end(file, 0, 0, 0);
}

static void
emit_c_block_line(KryFile *file, int line_no, char *line,
                  const char *active_guard)
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
        add_raw_conditional_line(file, active_guard, condition, out);
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
        add_raw_conditional_line(file, active_guard, condition, out);
    } else if(starts_word(line, "extern")) {
        char *q = trim(line + strlen("extern"));
        char name[KC_NAME_MAX];
        char args[512] = "";
        char ret[KC_NAME_MAX] = "void";
        char out[KC_BODY_LINE_MAX];

        if(!starts_word(q, "fn"))
            die("%s:%d: expected external function declaration",
                file->path, line_no);
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
            convert_arg_list_file(args, sizeof(args), trim(q + 1), file);
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
        add_raw_conditional_line(file, active_guard, condition, out);
    } else {
        die("%s:%d: unknown c block statement: %s", file->path, line_no,
            line);
    }
}

static void
emit_web_download_file_body(KryFile *file)
{
    add_raw_line(file, "    return EM_ASM_INT({");
    add_raw_line(file, "        try {");
    add_raw_line(file, "            const path = UTF8ToString($0);");
    add_raw_line(file, "            const filename = UTF8ToString($1);");
    add_raw_line(file, "            const mime = UTF8ToString($2);");
    add_raw_line(file, "            const bytes = FS.readFile(path);");
    add_raw_line(file, "            const blob = new Blob([bytes], {type: mime || \"application/octet-stream\"});");
    add_raw_line(file, "            const url = URL.createObjectURL(blob);");
    add_raw_line(file, "            const a = document.createElement(\"a\");");
    add_raw_line(file, "            a.href = url;");
    add_raw_line(file, "            a.download = filename || \"download\";");
    add_raw_line(file, "            a.style.display = \"none\";");
    add_raw_line(file, "            document.body.appendChild(a);");
    add_raw_line(file, "            a.click();");
    add_raw_line(file, "            a.remove();");
    add_raw_line(file, "            setTimeout(() => URL.revokeObjectURL(url), 1000);");
    add_raw_line(file, "            return 1;");
    add_raw_line(file, "        } catch(e) {");
    add_raw_line(file, "            console.error(\"Kry web download failed:\", e);");
    add_raw_line(file, "            return 0;");
    add_raw_line(file, "        }");
    add_raw_line(file, "    }, path, filename, mime);");
}

static void
emit_web_context_click_body(KryFile *file)
{
    add_raw_line(file, "    return EM_ASM_INT({");
    add_raw_line(file, "        const click = Module.__kryonContextClick;");
    add_raw_line(file, "        if(!click)");
    add_raw_line(file, "            return 0;");
    add_raw_line(file, "        if(Date.now() - click.time > 750) {");
    add_raw_line(file, "            Module.__kryonContextClick = null;");
    add_raw_line(file, "            return 0;");
    add_raw_line(file, "        }");
    add_raw_line(file, "        if(click.x >= $0 && click.x <= $2 && click.y >= $1 && click.y <= $3) {");
    add_raw_line(file, "            Module.__kryonContextClick = null;");
    add_raw_line(file, "            return 1;");
    add_raw_line(file, "        }");
    add_raw_line(file, "        return 0;");
    add_raw_line(file, "    }, x0, y0, x1, y1);");
}

static void
emit_web_intrinsic_wrapper(KryFile *file, int line_no, const char *name,
                           const char *args, const char *ret,
                           const char *active_guard)
{
    char guard[KC_BODY_LINE_MAX];
    char backend_guard[KC_BODY_LINE_MAX];

    if(strcmp(ret, "int") != 0)
        die("%s:%d: web intrinsic '%s' must return int",
            file->path, line_no, name);
    combine_compile_guards(guard, sizeof(guard), active_guard,
                           "defined(PLATFORM_WEB)");
    snprintf(backend_guard, sizeof(backend_guard), "%s", guard);
    add_guard_line(file, backend_guard, 0, 0, 0);
    add_raw_line(file, "static int");
    {
        char line[KC_BODY_LINE_MAX];

        snprintf(line, sizeof(line), "%s(%s)", name,
                 args[0] != '\0' ? args : "void");
        add_raw_line(file, line);
    }
    add_raw_line(file, "{");
    if(strcmp(name, "web_download_file") == 0)
        emit_web_download_file_body(file);
    else if(strcmp(name, "web_context_click_in_bounds") == 0)
        emit_web_context_click_body(file);
    else
        die("%s:%d: unknown web intrinsic '%s'", file->path, line_no, name);
    add_raw_line(file, "}");
    if(backend_guard[0] != '\0')
        add_guard_end(file, 0, 0, 0);
}

static int
parse_top_macro_line(KryFile *file, int line_no, char *line,
                     KryMacroFrame *macros, int *macro_count)
{
    char *macro_condition = NULL;

    if(parse_hash_if_start(line, &macro_condition)) {
        char expanded[KC_BODY_LINE_MAX];

        expand_compile_expr(expanded, sizeof(expanded), file,
                            macro_condition);
        if(strncmp(line, "} #else_if", 10) == 0 ||
           strncmp(line, "#else_if", 8) == 0) {
            KryMacroFrame *frame;
            char next_condition[KC_BODY_LINE_MAX];
            char next_excluded[KC_BODY_LINE_MAX];

            if(*macro_count <= 0)
                die("%s:%d: #else_if without matching #if",
                    file->path, line_no);
            frame = &macros[*macro_count - 1];
            snprintf(next_condition, sizeof(next_condition),
                     "!(%s) && (%s)", frame->excluded, expanded);
            append_macro_excluded(next_excluded, sizeof(next_excluded),
                                  frame->excluded, expanded);
            snprintf(frame->condition, sizeof(frame->condition),
                     "%s", next_condition);
            snprintf(frame->excluded, sizeof(frame->excluded),
                     "%s", next_excluded);
        } else {
            KryMacroFrame *frame;

            if(*macro_count >= 64)
                die("%s:%d: too many nested #if blocks",
                    file->path, line_no);
            frame = &macros[(*macro_count)++];
            snprintf(frame->condition, sizeof(frame->condition),
                     "%s", expanded);
            snprintf(frame->excluded, sizeof(frame->excluded),
                     "%s", expanded);
        }
        return 1;
    }
    if(line_is_hash_else(line)) {
        KryMacroFrame *frame;

        if(*macro_count <= 0)
            die("%s:%d: #else without matching #if", file->path, line_no);
        frame = &macros[*macro_count - 1];
        snprintf(frame->condition, sizeof(frame->condition),
                 "!(%s)", frame->excluded);
        return 1;
    }
    return 0;
}

static void
parse_statement(KryFile *file, int line_no, char *line)
{
    if(line_is_close(line)) {
        add_body(file, "    }");
    } else if(strcmp(line, "{") == 0) {
        add_body(file, "    {");
    } else if(line_is_goto_label(line, NULL, 0)) {
        char label[KC_NAME_MAX];

        line_is_goto_label(line, label, sizeof(label));
        add_body(file, "%s:", label);
    } else if(starts_word(line, "let")) {
        die("%s:%d: 'let' syntax was removed; use 'name: type = value'",
            file->path, line_no);
    } else if(starts_word(line, "background")) {
        die("%s:%d: 'background' widget keyword was removed; call Background(color) instead",
            file->path, line_no);
    } else if(starts_word(line, "set_theme")) {
        die("%s:%d: 'set_theme' was removed; call SetCurrentTheme(id, mode) directly",
            file->path, line_no);
    } else if(line_is_inferred_decl(line)) {
        emit_inferred_decl(file, line_no, line, 0);
    } else if(line_is_typed_decl(line)) {
        emit_typed_decl(file, line_no, line);
    } else if(line_is_c_uninit_decl(line)) {
        add_body(file, "    %s;", line);
    } else if(starts_statement_word(line, "text")) {
        die("%s:%d: 'text' widget keyword was removed; call Text(label, x, y, size, color) instead",
            file->path, line_no);
    } else if(starts_word(line, "rect")) {
        die("%s:%d: 'rect' widget keyword was removed; call Rect(x, y, w, h, fill, border) instead",
            file->path, line_no);
    } else if(starts_word(line, "line")) {
        die("%s:%d: 'line' widget keyword was removed; call UILine(x1, y1, x2, y2, color) instead",
            file->path, line_no);
    } else if(starts_word(line, "swatch")) {
        die("%s:%d: 'swatch' widget keyword was removed; draw a rect + text with Rect/Text instead",
            file->path, line_no);
    } else if(starts_word(line, "on key_down")) {
        die("%s:%d: 'on key_down' was removed; use 'if (IsKeyDown(KEY)) {'",
            file->path, line_no);
    } else if(starts_word(line, "on key")) {
        die("%s:%d: 'on key' was removed; use 'if (IsKeyPressed(KEY)) {'",
            file->path, line_no);
    } else if(starts_else_if(line)) {
        char *q = starts_word(line, "else if")
                      ? trim(line + strlen("else if"))
                      : trim(line + strlen("} else if"));
        size_t n = strlen(q);
        int split_else = starts_word(line, "else if");

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected else if condition ending with {",
                file->path, line_no);
        q[n - 1] = '\0';
        q = trim(q);
        if(q[0] == '\0')
            die("%s:%d: expected else if condition", file->path, line_no);
        add_body(file, split_else ? "    else if(%s) {" : "    } else if(%s) {",
                 q);
    } else if(line_is_else(line)) {
        add_body(file, strcmp(line, "else {") == 0 ? "    else {"
                                                   : "    } else {");
    } else if(starts_word(line, "guard")) {
        char *q = trim(line + strlen("guard"));

        if(q[0] == '\0')
            die("%s:%d: expected guard condition", file->path, line_no);
        add_body(file, "    if(%s)", q);
        add_body(file, "        return;");
    } else if(starts_word(line, "defer")) {
        char *q = trim(line + strlen("defer"));

        if(q[0] == '\0')
            die("%s:%d: expected defer statement", file->path, line_no);
        add_body(file, "    defer %s", q);
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
    } else if(starts_word(line, "goto")) {
        char *q = trim(line + strlen("goto"));
        char label[KC_NAME_MAX];

        if(!parse_ident(&q, label, sizeof(label)) || trim(q)[0] != '\0')
            die("%s:%d: expected goto label name", file->path, line_no);
        add_body(file, "    goto %s;", label);
    } else if(starts_word(line, "var")) {
        die("%s:%d: 'var' syntax was removed; use 'name: type = value'",
            file->path, line_no);
    } else if(starts_word(line, "set")) {
        char *q = trim(line + strlen("set"));

        if(q[0] == '\0')
            die("%s:%d: expected assignment", file->path, line_no);
        die("%s:%d: 'set' was removed; write the assignment directly: %s",
            file->path, line_no, q);
    } else if(starts_word(line, "native")) {
        char *q = trim(line + strlen("native"));

        if(q[0] == '\0')
            die("%s:%d: expected native expression", file->path, line_no);
        die("%s:%d: 'native' was removed; call it directly: %s",
            file->path, line_no, q);
    } else if(starts_word(line, "unused")) {
        char *q = trim(line + strlen("unused"));

        if(q[0] == '\0')
            die("%s:%d: expected unused expression", file->path, line_no);
        add_body(file, "    (void)%s;", q);
    } else if(starts_word(line, "c")) {
        char *q = trim(line + strlen("c"));

        if(q[0] == '\0')
            die("%s:%d: expected raw C line", file->path, line_no);
        add_body(file, "    %s", q);
    } else if(starts_word(line, "do")) {
        char *q = trim(line + strlen("do"));

        if(q[0] == '\0')
            die("%s:%d: 'do' syntax was removed; use a plain call",
                file->path, line_no);
        die("%s:%d: 'do' syntax was removed; use '%s'",
            file->path, line_no, q);
    } else if(starts_word(line, "draw") || starts_word(line, "widget")) {
        char *q = starts_word(line, "draw")
                      ? trim(line + strlen("draw"))
                      : trim(line + strlen("widget"));

        if(q[0] == '\0')
            die("%s:%d: expected expression", file->path, line_no);
        die("%s:%d: 'draw'/'widget' was removed; call it directly: %s",
            file->path, line_no, q);
    } else if(starts_word(line, "advance")) {
        die("%s:%d: 'advance x by N' was removed; write 'x += N' directly",
            file->path, line_no);
    } else if(starts_word(line, "clamp_min") ||
              starts_word(line, "clamp_max")) {
        die("%s:%d: 'clamp_min/max' was removed; write 'if (x < N) x = N' directly",
            file->path, line_no);
    } else if(starts_word(line, "c_rect")) {
        die("%s:%d: 'c_rect' was removed; declare with 'name: Rectangle = (Rectangle){...}'",
            file->path, line_no);
    } else if(starts_word(line, "texture")) {
        die("%s:%d: 'texture' was removed; declare with 'name: Texture2D = ...'",
            file->path, line_no);
    } else if(starts_word(line, "enum")) {
        char *q = trim(line + strlen("enum"));
        size_t n = strlen(q);

        if(n == 0 || q[n - 1] != '}')
            die("%s:%d: expected enum block ending with }", file->path,
                line_no);
        add_body(file, "    enum %s;", q);
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
        /* If the condition is a function call (e.g. `Button(...)`),
         * wrap it so the call registers its source location for click-to-
         * source inspection. Evaluating into a temp and popping before the
         * branch keeps the Push/Pop balanced regardless of else/return. */
        {
            size_t cn = strlen(q);
            if(strchr(q, '(') != NULL && cn > 0 && q[cn - 1] == ')') {
                emit_source_push(file, line_no);
                add_body(file, "    __auto_type __kryon_cond_%d = %s;",
                         line_no, q);
                emit_source_pop(file);
                add_body(file, "    if(__kryon_cond_%d) {", line_no);
            } else {
                add_body(file, "    if(%s) {", q);
            }
        }
    } else if(starts_word(line, "switch")) {
        char *q = trim(line + strlen("switch"));
        size_t n = strlen(q);

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected switch expression ending with {",
                file->path, line_no);
        q[n - 1] = '\0';
        q = trim(q);
        if(q[0] == '\0')
            die("%s:%d: expected switch expression", file->path, line_no);
        add_body(file, "    switch(%s) {", q);
    } else if(starts_word(line, "case")) {
        char *q = trim(line + strlen("case"));
        size_t n = strlen(q);

        if(n == 0 || q[n - 1] != ':')
            die("%s:%d: expected case label ending with ':'", file->path,
                line_no);
        q[n - 1] = '\0';
        q = trim(q);
        if(q[0] == '\0')
            die("%s:%d: expected case label", file->path, line_no);
        add_body(file, "    case %s:", q);
    } else if(strcmp(line, "default:") == 0) {
        add_body(file, "    default:");
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
        die("%s:%d: 'button' block keyword was removed; use 'if Button((ButtonProps){...}) {'",
            file->path, line_no);
    } else if(starts_word(line, "event") || starts_word(line, "on")) {
        die("%s:%d: 'event'/'on' block keyword was removed; use 'if (expr) {' with a bool-returning call",
            file->path, line_no);
    } else if(starts_word(line, "icon_button")) {
        die("%s:%d: 'icon_button' block keyword was removed; use 'if IconButton((IconButtonProps){...}) {'",
            file->path, line_no);
    } else if(starts_word(line, "connect")) {
        /* connect emitter.signal to target.handler
         * lowers to KrySignalConnect(scene, emitter, "signal", target, "handler") */
        char *q = trim(line + strlen("connect"));
        char emitter_expr[256];
        char signal_name[KC_NAME_MAX];
        char target_expr[256];
        char handler_name[KC_NAME_MAX];
        char call[768];
        char *dot;
        char *to;

        dot = strchr(q, '.');
        if(dot == NULL)
            die("%s:%d: expected 'connect emitter.signal to target.handler'",
                file->path, line_no);
        snprintf(emitter_expr, sizeof(emitter_expr), "%.*s", (int)(dot - q), q);
        q = dot + 1;
        if(!parse_ident(&q, signal_name, sizeof(signal_name)))
            die("%s:%d: expected signal name after '.'", file->path, line_no);
        q = trim(q);
        to = starts_word(q, "to") ? q + strlen("to") : NULL;
        if(to == NULL)
            die("%s:%d: expected 'to' in connect", file->path, line_no);
        to = trim(to);
        dot = strchr(to, '.');
        if(dot == NULL)
            die("%s:%d: expected 'target.handler' after 'to'", file->path, line_no);
        snprintf(target_expr, sizeof(target_expr), "%.*s", (int)(dot - to), to);
        to = dot + 1;
        if(!parse_ident(&to, handler_name, sizeof(handler_name)) || trim(to)[0] != '\0')
            die("%s:%d: expected handler name after '.'", file->path, line_no);
        snprintf(call, sizeof(call),
                 "KrySignalConnect(scene, %s, \"%s\", %s, \"%s\")",
                 emitter_expr, signal_name, target_expr, handler_name);
        emit_source_push(file, line_no);
        emit_call(file, "    ", call, ";");
        emit_source_pop(file);
    } else if(starts_word(line, "emit")) {
        /* emit emitter.signal
         * lowers to KrySignalEmit(scene, emitter, "signal", KryonPropertyInt(0)) */
        char *q = trim(line + strlen("emit"));
        char emitter_expr[256];
        char signal_name[KC_NAME_MAX];
        char call[512];
        char *dot;

        dot = strchr(q, '.');
        if(dot == NULL)
            die("%s:%d: expected 'emit emitter.signal'", file->path, line_no);
        snprintf(emitter_expr, sizeof(emitter_expr), "%.*s", (int)(dot - q), q);
        q = dot + 1;
        if(!parse_ident(&q, signal_name, sizeof(signal_name)) || trim(q)[0] != '\0')
            die("%s:%d: expected signal name after '.'", file->path, line_no);
        snprintf(call, sizeof(call),
                 "KrySignalEmit(scene, %s, \"%s\", KryonPropertyInt(0))",
                 emitter_expr, signal_name);
        emit_source_push(file, line_no);
        emit_call(file, "    ", call, ";");
        emit_source_pop(file);
    } else if(line[strlen(line) - 1] == ')' && strchr(line, '(') != NULL &&
              find_assignment_op(line) == NULL) {
        emit_source_push(file, line_no);
        emit_call(file, "    ", line, ";");
        emit_source_pop(file);
    } else if(line_is_mutation_statement(line)) {
        add_body(file, "    %s;", line);
    } else if(find_assignment_op(line) != NULL) {
        emit_assignment(file, line_no, line);
    } else {
        size_t n = strlen(line);

        if(n > 2 && line[n - 1] == ')' && strchr(line, '(') != NULL) {
            die("%s:%d: invalid implicit call statement: %s",
                file->path, line_no, line);
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
resolve_use_module(char *dst, size_t dst_size, const KryFile *file,
                   const char *module)
{
    char header[KC_PATH_MAX];
    char rel[KC_PATH_MAX];
    char path[KC_PATH_MAX];
    FILE *in;
    char line[KC_BODY_LINE_MAX];

    module_symbol(dst, dst_size, module);
    module_header(header, sizeof(header), module);
    snprintf(rel, sizeof(rel), "%s", header);
    if(strlen(rel) > 2 && strcmp(rel + strlen(rel) - 2, ".h") == 0)
        snprintf(rel + strlen(rel) - 2, sizeof(rel) - strlen(rel) + 2,
                 ".kry");
    if(file->root != NULL && file->root[0] != '\0')
        snprintf(path, sizeof(path), "%s/%s", file->root, rel);
    else
        snprintf(path, sizeof(path), "%s", rel);

    in = fopen(path, "rb");
    if(in == NULL)
        return;
    while(fgets(line, sizeof(line), in) != NULL) {
        char *q = trim(line);
        char name[KC_NAME_MAX];

        if(starts_word(q, "#module")) {
            q = trim(q + strlen("#module"));
            if(parse_quoted(&q, name, sizeof(name)) &&
               trim(q)[0] == '\0')
                module_symbol(dst, dst_size, name);
        }
        break;
    }
    fclose(in);
}

static void
parse_kry(KryFile *file)
{
    char *text;
    char *line_start;

    /* Enable per-statement error recovery for this file. Located die() calls
     * reached while parsing record a diagnostic and longjmp to the nearest
     * statement boundary instead of aborting the compile. */
    kc_set_recovery_file(file);
    int line_no = 1;
    int depth = 0;
    int in_screen = 0;
    int in_route = 0;
    int in_app = 0;
    int in_state = 0;
    int in_state_decl = 0;
    int state_decl_depth = 0;
    int in_top_static_decl = 0;
    int top_static_decl_depth = 0;
    char top_static_guard[KC_BODY_LINE_MAX] = "";
    int in_struct = 0;
    int struct_is_public = 0;
    char struct_name[KC_NAME_MAX] = "";
    char type_guard[KC_BODY_LINE_MAX] = "";
    int in_enum = 0;
    int enum_is_public = 0;
    char enum_name[KC_NAME_MAX] = "";
    int in_c_block = 0;
    char c_block_guard[KC_BODY_LINE_MAX] = "";
    char state_block_guard[KC_BODY_LINE_MAX] = "";
    int in_raw = 0;
    char pending_stmt[KC_BODY_LINE_MAX * 4] = "";
    char pending_decl[KC_BODY_LINE_MAX * 4] = "";
    int pending_line = 0;
    int pending_delta = 0;
    int pending_is_block = 0;
    int pending_can_lead_continue = 0;
    int pending_decl_line = 0;
    int pending_decl_delta = 0;
    int macro_depths[64];
    int macro_count = 0;
    KryMacroFrame top_macros[64];
    int top_macro_count = 0;
    KryRoute *current_route = NULL;

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
                      (starts_word(line, "screen") ||
                       starts_word(line, "preview") ||
                       starts_word(line, "page") ||
                       starts_word(line, "scene") ||
                       strstr(line, "::") != NULL) &&
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
                    if(type_guard[0] != '\0') {
                        add_guard_end(file, struct_is_public, 1, 0);
                        type_guard[0] = '\0';
                    }
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
                    emit_enum_end(file, enum_is_public, enum_name);
                    if(type_guard[0] != '\0') {
                        add_guard_end(file, enum_is_public, 1, 0);
                        type_guard[0] = '\0';
                    }
                    in_enum = 0;
                    enum_is_public = 0;
                    enum_name[0] = '\0';
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
                    c_block_guard[0] = '\0';
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
                emit_c_block_line(file, line_no, line, c_block_guard);
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
                    } else if(starts_word(line, "init")) {
                        char *q = trim(line + strlen("init"));

                        if(!parse_ident(&q, file->app_init,
                                        sizeof(file->app_init)))
                            die("%s:%d: expected init function name",
                                file->path, line_no);
                    } else if(starts_word(line, "frame")) {
                        char *q = trim(line + strlen("frame"));

                        if(!parse_ident(&q, file->app_frame,
                                        sizeof(file->app_frame)))
                            die("%s:%d: expected frame function name",
                                file->path, line_no);
                    } else if(starts_word(line, "shutdown")) {
                        char *q = trim(line + strlen("shutdown"));

                        if(!parse_ident(&q, file->app_shutdown,
                                        sizeof(file->app_shutdown)))
                            die("%s:%d: expected shutdown function name",
                                file->path, line_no);
                    } else if(starts_word(line, "scene")) {
                        char *q = trim(line + strlen("scene"));

                        if(!parse_ident(&q, file->app_scene,
                                        sizeof(file->app_scene)))
                            die("%s:%d: expected scene name",
                                file->path, line_no);
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
            } else if(in_route) {
                if(!(line_is_close(line) && depth == 1))
                    parse_route_property(file, line_no, current_route, line);
                depth += opens;
                depth -= closes;
                if(depth < 0)
                    die("%s:%d: unexpected }", file->path, line_no);
                if(depth == 0) {
                    in_route = 0;
                    current_route = NULL;
                }
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
                if(depth == 0) {
                    in_state = 0;
                    if(state_block_guard[0] != '\0') {
                        add_guard_end(file, 0, 0, 1);
                        state_block_guard[0] = '\0';
                    }
                }
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
                    if(top_static_guard[0] != '\0') {
                        add_guard_end(file, 0, 0, 1);
                        top_static_guard[0] = '\0';
                    }
                }
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
            } else if(!in_screen && depth == 0 && line_is_close(line) &&
                      top_macro_count > 0) {
                top_macro_count--;
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
            } else if(!in_screen && depth == 0 &&
                      parse_top_macro_line(file, line_no, line, top_macros,
                                           &top_macro_count)) {
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
            } else if(!in_screen && starts_word(line, "#module")) {
                parse_module_directive(file, line_no, line, depth);
            } else if(!in_screen && starts_word(line, "#output")) {
                parse_output_directive(file, line_no, line, depth);
            } else if(!in_screen && starts_word(line, "#pragma")) {
                emit_pragma_directive(file, line_no, line, top_macros,
                                      top_macro_count, depth);
            } else if(!in_screen && starts_word(line, "#error")) {
                emit_error_directive(file, line_no, line, top_macros,
                                     top_macro_count, depth);
            } else if(!in_screen && starts_word(line, "#import")) {
                char guard[KC_BODY_LINE_MAX];

                if(depth != 0)
                    die("%s:%d: import must be top-level", file->path,
                        line_no);
                current_macro_guard(guard, sizeof(guard), top_macros,
                                    top_macro_count);
                emit_import_decl(file, line_no, NULL, line, guard);
            } else if(!in_screen && starts_word(line, "#enum")) {
                if(depth != 0)
                    die("%s:%d: enum declaration must be top-level",
                        file->path, line_no);
                begin_hash_enum(file, line_no, line, top_macros,
                                top_macro_count, &enum_is_public,
                                type_guard, sizeof(type_guard),
                                enum_name, sizeof(enum_name));
                in_enum = 1;
                depth++;
                depth -= closes;
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
            } else if(!in_screen && strstr(line, "::") != NULL) {
                char *op = strstr(line, "::");
                char name[KC_NAME_MAX];
                char guard[KC_BODY_LINE_MAX];
                char *lhs;
                char *rhs;

                if(depth != 0)
                    die("%s:%d: declaration must be top-level",
                        file->path, line_no);
                *op = '\0';
                lhs = trim(line);
                rhs = trim(op + 2);
                snprintf(name, sizeof(name), "%s", lhs);
                current_macro_guard(guard, sizeof(guard), top_macros,
                                    top_macro_count);
                if(emit_import_decl(file, line_no, name, rhs, guard)) {
                } else if(starts_word(rhs, "struct")) {
                    begin_colon_struct(file, line_no, name, rhs, top_macros,
                                       top_macro_count, &struct_is_public,
                                       type_guard, sizeof(type_guard),
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
                } else if(starts_word(rhs, "enum")) {
                    begin_colon_enum(file, line_no, name, rhs, top_macros,
                                     top_macro_count, &enum_is_public,
                                     type_guard, sizeof(type_guard),
                                     enum_name, sizeof(enum_name));
                    in_enum = 1;
                    depth += opens;
                    depth -= closes;
                    *p = saved;
                    if(saved == '\0')
                        break;
                    line_start = p + 1;
                    line_no++;
                    continue;
                } else if(emit_colon_extern_decl(file, line_no, name, rhs,
                                                 guard)) {
                } else if(emit_colon_function_decl(file, line_no, name, rhs,
                                                   guard, &in_screen)) {
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
                } else if(emit_colon_global_decl(file, line_no, name, rhs,
                                                 guard)) {
                } else if(emit_colon_type_alias(file, line_no, name, rhs,
                                                guard)) {
                } else if(starts_word(rhs, "#define")) {
                    char *value = trim(rhs + strlen("#define"));

                    add_define(file, line_no, name, value, guard);
                } else {
                    add_const(file, line_no, name, rhs);
                }
            } else if(!in_screen && starts_word(line, "route")) {
                char *q = trim(line + strlen("route"));
                char id[KC_NAME_MAX];

                if(depth != 0)
                    die("%s:%d: nested route blocks are not supported",
                        file->path, line_no);
                if(!parse_ident(&q, id, sizeof(id)))
                    die("%s:%d: expected route id", file->path, line_no);
                q = trim(q);
                if(strcmp(q, "{") != 0)
                    die("%s:%d: expected route block", file->path, line_no);
                current_route = add_route(file, line_no, id, top_macros,
                                          top_macro_count);
                in_route = 1;
                depth += opens;
                depth -= closes;
                *p = saved;
                if(saved == '\0')
                    break;
                line_start = p + 1;
                line_no++;
                continue;
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
                current_macro_guard(state_block_guard,
                                    sizeof(state_block_guard),
                                    top_macros, top_macro_count);
                add_guard_line(file, state_block_guard, 0, 0, 1);
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
                char guard[KC_BODY_LINE_MAX];

                if(depth != 0)
                    die("%s:%d: static declaration must be top-level",
                        file->path, line_no);
                current_macro_guard(guard, sizeof(guard), top_macros,
                                    top_macro_count);
                count_line_braces(line, &decl_opens, &decl_closes);
                if(strchr(line, '=') != NULL && decl_opens > decl_closes) {
                    add_guard_line(file, guard, 0, 0, 1);
                    snprintf(top_static_guard, sizeof(top_static_guard),
                             "%s", guard);
                    emit_top_static_decl_start(file, line_no, line);
                    in_top_static_decl = 1;
                    top_static_decl_depth = decl_opens - decl_closes;
                } else {
                    add_guard_line(file, guard, 0, 0, 1);
                    emit_top_static_decl(file, line_no, line);
                    if(guard[0] != '\0')
                        add_guard_end(file, 0, 0, 1);
                }
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
                current_macro_guard(c_block_guard, sizeof(c_block_guard),
                                    top_macros, top_macro_count);
                in_c_block = 1;
            } else if(!in_screen &&
                      (starts_word(line, "screen") ||
                       starts_word(line, "preview") ||
                       starts_word(line, "page") ||
                       starts_word(line, "scene") ||
                       starts_word(line, "frame"))) {
                KryFunction *fn;
                char *decl = line;
                char *q;
                int matched_scene = 0;
                int is_frame = 0;

                if(starts_word(decl, "screen"))
                    q = decl + strlen("screen");
                else if(starts_word(decl, "preview"))
                    q = decl + strlen("preview");
                else if(starts_word(decl, "page"))
                    q = decl + strlen("page");
                else if(starts_word(decl, "scene")) {
                    matched_scene = 1;
                    q = decl + strlen("scene");
                } else if(starts_word(decl, "frame")) {
                    is_frame = 1;
                    q = decl + strlen("frame");
                } else
                    q = decl + strlen("fn");

                if(depth != 0)
                    die("%s:%d: nested functions are not supported",
                        file->path, line_no);
                fn = add_function(file);
                file->current = fn;
                current_macro_guard(fn->guard, sizeof(fn->guard),
                                    top_macros, top_macro_count);
                if(!parse_ident(&q, fn->screen, sizeof(fn->screen)))
                    die("%s:%d: expected screen name", file->path, line_no);
                fn->is_public = 1;
                if(is_frame)
                    fn->exact_name = 1;
                if(matched_scene) {
                    /* scene builders are retained-tree constructors: their C
                     * signature is always void Name_kry_scene(KryScene *scene),
                     * and the body adds nodes to that scene. */
                    fn->is_scene = 1;
                    convert_arg_list_file(fn->args, sizeof(fn->args),
                                          "scene: KryScene*", file);
                }
                q = trim(q);
                if(q[0] == '(') {
                    char *end = strrchr(q, ')');

                    if(end == NULL)
                        die("%s:%d: expected ) in page argument list",
                            file->path, line_no);
                    *end = '\0';
                    convert_arg_list_file(fn->args, sizeof(fn->args), trim(q + 1), file);
                    q = trim(end + 1);
                }
                if(q[0] == '-' && q[1] == '>') {
                    q = trim(q + 2);
                    if(q[0] == '\0')
                        die("%s:%d: expected return type", file->path,
                            line_no);
                    if(q[strlen(q) - 1] == '{')
                        q[strlen(q) - 1] = '\0';
                    strip_module_alias(fn->return_type, sizeof(fn->return_type),
                                        file, trim(q));
                }
                in_screen = 1;
            } else if(in_screen && depth > 0 &&
                      starts_header_directive(line, "args")) {
                char *q = trim(line + strlen("args"));
                KryFunction *fn = file->current;

                if(fn == NULL)
                    die("%s:%d: args outside function", file->path, line_no);
                strip_module_alias(fn->args, sizeof(fn->args), file, q);
            } else if(in_screen && depth > 0 &&
                      starts_header_directive(line, "returns")) {
                char *q = trim(line + strlen("returns"));
                KryFunction *fn = file->current;

                if(fn == NULL)
                    die("%s:%d: returns outside function", file->path, line_no);
                if(q[0] == '\0')
                    die("%s:%d: expected return type", file->path, line_no);
                strip_module_alias(fn->return_type, sizeof(fn->return_type),
                                    file, q);
            } else if(in_screen && depth > 0 &&
                      starts_header_directive(line, "call")) {
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
                    if(pending_stmt[0] != '\0') {
                        file->current_line = pending_line;
                        if(setjmp(*kc_recover_buf()) == 0) {
                            kc_set_recovering(1);
                            parse_statement(file, pending_line, pending_stmt);
                        }
                        kc_set_recovering(0);
                        file->current_line = 0;
                        pending_stmt[0] = '\0';
                        pending_line = 0;
                        pending_delta = 0;
                        pending_is_block = 0;
                        pending_can_lead_continue = 0;
                    }
                } else {
                    int stmt_line = line_no;
                    char *stmt = line;
                    int is_block_stmt = line_starts_block_statement(line);
                    int stmt_delta = line_delim_delta(line);
                    int group_delta = line_group_delta(line);
                    int parsed_pending = 0;
                    char *macro_condition = NULL;

process_screen_line:
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
                        if(pending_can_lead_continue &&
                           pending_delta == 0 &&
                           !line_starts_continuation(line)) {
                            file->current_line = pending_line;
                            if(setjmp(*kc_recover_buf()) == 0) {
                                kc_set_recovering(1);
                                parse_statement(file, pending_line,
                                                pending_stmt);
                            }
                            kc_set_recovering(0);
                            file->current_line = 0;
                            pending_stmt[0] = '\0';
                            pending_line = 0;
                            pending_delta = 0;
                            pending_is_block = 0;
                            pending_can_lead_continue = 0;
                            goto process_screen_line;
                        } else {
                            append_statement_line(file, pending_stmt,
                                                  sizeof(pending_stmt), line);
                            pending_delta += pending_is_block ? group_delta
                                                               : stmt_delta;
                            pending_can_lead_continue =
                                pending_delta == 0 && line_starts_char(line, '?');
                            if(pending_delta > 0 ||
                               pending_can_lead_continue ||
                               line_needs_continuation(line)) {
                                depth += opens;
                                depth -= closes;
                                if(depth < 0)
                                    die("%s:%d: unexpected }", file->path,
                                        line_no);
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
                        }
                    } else if(strcmp(line, "{") != 0 &&
                              ((is_block_stmt ? group_delta : stmt_delta) > 0 ||
                               line_needs_continuation(line))) {
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
                    } else if(!is_block_stmt &&
                              line_can_accept_leading_continuation(line)) {
                        append_statement_line(file, pending_stmt,
                                              sizeof(pending_stmt), line);
                        pending_line = line_no;
                        pending_delta = 0;
                        pending_is_block = 0;
                        pending_can_lead_continue = 1;
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

                    if(stmt[0] != '\0') {
                        file->current_line = stmt_line;
                        if(setjmp(*kc_recover_buf()) == 0) {
                            kc_set_recovering(1);
                            parse_statement(file, stmt_line, stmt);
                        }
                        kc_set_recovering(0);
                        file->current_line = 0;
                        if(parsed_pending) {
                            pending_stmt[0] = '\0';
                            pending_line = 0;
                            pending_delta = 0;
                            pending_is_block = 0;
                            pending_can_lead_continue = 0;
                        }
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
    if(file->function_count == 0 && file->public_type_count == 0 &&
       file->private_type_count == 0 && file->route_count == 0)
        die("%s: missing declarations", file->path);
    if(in_raw)
        die("%s: unterminated raw block", file->path);
    if(in_c_block)
        die("%s: unterminated c block", file->path);
    if(in_route)
        die("%s:%d: unterminated route block", file->path, line_no);
    if(in_struct)
        die("%s:%d: unterminated struct declaration", file->path, line_no);
    if(macro_count != 0)
        die("%s:%d: unterminated #if block", file->path, line_no);
    if(top_macro_count != 0)
        die("%s:%d: unterminated top-level #if block", file->path, line_no);
    if(pending_decl[0] != '\0')
        die("%s:%d: unterminated function declaration", file->path,
            pending_decl_line);
    if(depth != 0)
        die("%s:%d: unbalanced braces", file->path, line_no);
    if(pending_stmt[0] != '\0')
        die("%s:%d: unterminated continued statement", file->path,
            pending_line);
}
static void
usage(void)
{
    fprintf(stderr,
            "usage: k2c [--no-main] [--dump-ast] [--kir] --root DIR -o DIR file.kry ...\n");
}

int
main(int argc, char **argv)
{
    const char *root = ".";
    const char *out_dir = NULL;
    KryFile **files = NULL;
    int file_count = 0;
    int no_main = 0;
    int dump_ast = 0;
    int kir_mode = 0;
    int had_errors = 0;
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
        } else if(strcmp(argv[i], "--dump-ast") == 0) {
            /* Debug: reconstruct and print the AST for each function without
             * writing files. Phase 1 of the parser migration. */
            dump_ast = 1;
        } else if(strcmp(argv[i], "--kir") == 0) {
            kir_mode = 1;
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
    file_count = argc - first_file;
    files = calloc((size_t)file_count, sizeof(*files));
    if(files == NULL)
        die("out of memory");
    for(int i = first_file; i < argc; i++) {
        if(kir_mode) {
            KirProgram *prog = kir_parse_file(argv[i], root);
            if(prog != NULL) {
                k2c_lower(prog, root, out_dir);
                KirProgramFree(prog);
            }
            continue;
        }
        KryFile *file;
        int index = i - first_file;

        file = calloc(1, sizeof(*file));
        if(file == NULL)
            die("out of memory");
        file->path = argv[i];
        file->root = root;
        snprintf(file->display_path, sizeof(file->display_path), "%s",
                 relative_path(root, file->path));
        file->no_main = no_main;
        parse_kry(file);
        if(file->diagnostic_count > 0) {
            kc_flush_diagnostics(file);
            had_errors = 1;
        } else if(dump_ast) {
            for(int j = 0; j < file->function_count; j++) {
                AstFunction *af = ast_function_from_body(&file->functions[j]);
                ast_function_dump(af);
                ast_function_free(af);
            }
        } else {
            write_generated(file, root, out_dir);
        }
        files[index] = file;
    }
    if(!had_errors && !kir_mode) {
        write_project_header(files, file_count, root, out_dir);
        write_project_source(files, file_count, root, out_dir);
    }
    if(kir_mode) {
        free(files);
        return had_errors ? 1 : 0;
    }
    for(int i = 0; i < file_count; i++) {
        KryFile *file = files[i];

        for(int j = 0; j < file->function_count; j++) {
            KryFunction *fn = &file->functions[j];

            for(int k = 0; k < fn->body_cap; k++)
                free(fn->body[k]);
            free(fn->body);
            free(fn->body_line);
        }
        free(file->functions);
        free(file->routes);
        free(file->text);
        free(file);
    }
    free(files);
    return had_errors ? 1 : 0;
}
