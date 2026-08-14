/* KryFile and KryFunction mutators — the "IR builder" for kc's intermediate
 * representation. Each top-level construct (raw C block, type decl, state
 * slot, global, compile-time const, #define, function, route) and each body
 * statement is appended here. These are the only functions that grow the
 * per-file arrays, so the capacity invariants live here too. */
#include "kc_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
add_raw_line(KryFile *file, const char *line)
{
    if(file->raw_count >= KC_RAW_MAX)
        die("%s: top-level raw block is too large", file->path);
    snprintf(file->raw[file->raw_count],
             sizeof(file->raw[file->raw_count]), "%s", line);
    file->raw_count++;
}

void
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

void
add_state_line(KryFile *file, const char *line)
{
    if(file->state_count >= KC_STATE_MAX)
        die("%s: state block is too large", file->path);
    snprintf(file->state[file->state_count],
             sizeof(file->state[file->state_count]), "%s", line);
    file->state_count++;
}

void
add_global_line(KryFile *file, int is_public, const char *line)
{
    char (*globals)[KC_BODY_LINE_MAX];
    int *count;

    if(is_public) {
        globals = file->public_globals;
        count = &file->public_global_count;
    } else {
        globals = file->globals;
        count = &file->global_count;
    }
    if(*count >= KC_STATE_MAX)
        die("%s: global declaration block is too large", file->path);
    snprintf(globals[*count], sizeof(globals[*count]), "%s", line);
    (*count)++;
}

void
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

void
add_define(KryFile *file, int line_no, const char *name, const char *value,
           const char *guard)
{
    if(!is_ident_text(name))
        die("%s:%d: invalid define name '%s'", file->path, line_no, name);
    if(value == NULL || value[0] == '\0')
        die("%s:%d: expected define value", file->path, line_no);
    if(file->define_count >= KC_DEFINE_MAX)
        die("%s:%d: too many defines", file->path, line_no);
    for(int i = 0; i < file->define_count; i++) {
        if(strcmp(file->define_names[i], name) == 0)
            die("%s:%d: duplicate define '%s'", file->path, line_no, name);
    }
    snprintf(file->define_names[file->define_count],
             sizeof(file->define_names[file->define_count]), "%s", name);
    snprintf(file->define_values[file->define_count],
             sizeof(file->define_values[file->define_count]), "%s", value);
    snprintf(file->define_guards[file->define_count],
             sizeof(file->define_guards[file->define_count]), "%s",
             guard != NULL ? guard : "");
    file->define_count++;
}

KryFunction *
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

KryRoute *
add_route(KryFile *file, int line_no, const char *id,
          const KryMacroFrame *macros, int macro_count)
{
    KryRoute *next;
    KryRoute *route;
    int next_cap;

    if(file == NULL)
        return NULL;
    if(!is_ident_text(id))
        die("%s:%d: invalid route id '%s'", file->path, line_no, id);
    for(int i = 0; i < file->route_count; i++) {
        if(strcmp(file->routes[i].id, id) == 0)
            die("%s:%d: duplicate route '%s'", file->path, line_no, id);
    }
    if(file->route_count >= file->route_cap) {
        next_cap = file->route_cap == 0 ? 16 : file->route_cap * 2;
        next = realloc(file->routes, (size_t)next_cap * sizeof(*next));
        if(next == NULL)
            die("%s: out of memory while growing route list", file->path);
        file->routes = next;
        file->route_cap = next_cap;
    }
    route = &file->routes[file->route_count++];
    memset(route, 0, sizeof(*route));
    snprintf(route->id, sizeof(route->id), "%s", id);
    snprintf(route->title, sizeof(route->title), "%s", id);
    snprintf(route->group, sizeof(route->group), "Project");
    snprintf(route->page, sizeof(route->page), "%s", id);
    snprintf(route->source_path, sizeof(route->source_path), "%s",
             file->display_path);
    current_macro_guard(route->guard, sizeof(route->guard), macros,
                        macro_count);
    return route;
}

void
parse_route_property(KryFile *file, int line_no, KryRoute *route, char *line)
{
    char *q;

    if(route == NULL)
        die("%s:%d: route property outside route", file->path, line_no);
    if(starts_word(line, "title")) {
        q = trim(line + strlen("title"));
        if(!parse_quoted(&q, route->title, sizeof(route->title)) ||
           trim(q)[0] != '\0')
            die("%s:%d: expected quoted route title", file->path, line_no);
    } else if(starts_word(line, "group")) {
        q = trim(line + strlen("group"));
        if(!parse_quoted(&q, route->group, sizeof(route->group)) ||
           trim(q)[0] != '\0')
            die("%s:%d: expected quoted route group", file->path, line_no);
    } else if(starts_word(line, "page")) {
        q = trim(line + strlen("page"));
        if(!parse_ident(&q, route->page, sizeof(route->page)) ||
           trim(q)[0] != '\0')
            die("%s:%d: expected route page id", file->path, line_no);
    } else {
        die("%s:%d: unknown route property: %s", file->path, line_no, line);
    }
}

/* Ensure fn->body/body_line have room for one more statement, growing the
 * buffers geometrically. Body lines are heap-allocated char[KC_BODY_LINE_MAX]
 * each; there is no fixed per-function cap. */
void
grow_body(KryFunction *fn)
{
    int next_cap;
    char **next_body;
    int *next_line;

    if(fn->body_count < fn->body_cap)
        return;
    next_cap = fn->body_cap == 0 ? 128 : fn->body_cap * 2;
    next_body = realloc(fn->body, (size_t)next_cap * sizeof(*next_body));
    next_line = realloc(fn->body_line, (size_t)next_cap * sizeof(*next_line));
    if(next_body == NULL || next_line == NULL)
        die("out of memory while growing function body");
    /* Zero the newly grown slots so vsnprintf writes into a valid buffer. */
    for(int i = fn->body_cap; i < next_cap; i++) {
        next_body[i] = calloc(KC_BODY_LINE_MAX, 1);
        if(next_body[i] == NULL)
            die("out of memory while growing function body");
    }
    fn->body = next_body;
    fn->body_line = next_line;
    fn->body_cap = next_cap;
}

void
add_body_line(KryFile *file, int source_line, const char *fmt, ...)
{
    KryFunction *fn;
    va_list args;

    fn = file->current;
    if(fn == NULL)
        die("%s: statement outside function", file->path);
    grow_body(fn);
    va_start(args, fmt);
    vsnprintf(fn->body[fn->body_count], KC_BODY_LINE_MAX, fmt, args);
    va_end(args);
    fn->body_line[fn->body_count] = source_line;
    fn->body_count++;
}

void
add_body(KryFile *file, const char *fmt, ...)
{
    KryFunction *fn;
    va_list args;

    fn = file->current;
    if(fn == NULL)
        die("%s: statement outside function", file->path);
    grow_body(fn);
    va_start(args, fmt);
    vsnprintf(fn->body[fn->body_count], KC_BODY_LINE_MAX, fmt, args);
    va_end(args);
    fn->body_line[fn->body_count] = file->current_line;
    fn->body_count++;
}
