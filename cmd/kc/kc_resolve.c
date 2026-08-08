/* Symbol and module resolution plus expression rewriting. Kry lets apps import
 * a module under an alias (`alias :: #import "mod/path"`) and then write
 * `alias.fn(...)` for calls or `alias.Type` for cross-module type references.
 * The codegen and parser both need these rewoven into the C spelling:
 * qualified calls become `module_fn(...)`, qualified types drop the alias,
 * bare screen-local names resolve to their mangled C names, and `nil`
 * becomes `NULL`.
 *
 * find_local_function / module_for_alias / function_value_context /
 * name_in_c_parameter_list / kry_declares_name / function_has_local_name are
 * file-local: they are only reached from the public rewriters below. */
#include "kc_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const KryFunction *
find_local_function(const KryFile *file, const char *name)
{
    for(int i = 0; i < file->function_count; i++) {
        const KryFunction *fn = &file->functions[i];
        char base[KC_NAME_MAX + 16];

        kc_function_base_name(base, sizeof(base), fn);
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
            if(module != NULL && module_size > 0)
                snprintf(module, module_size, "%s", file->use_modules[i]);
            return 1;
        }
    }
    return 0;
}

/* Rewrite qualified type references in C type text. Kry imports another module
 * with `alias :: #import "mod/path"` and then writes `alias.Type` to reference a
 * struct/enum declared there. In C the type uses its bare declared name across
 * translation units (kc emits `typedef struct Counter {...} Counter;` without a
 * module prefix), so `alias.Type` must become just `Type`. This is distinct from
 * qualified *calls* (`alias.fn(...)`), which kc rewrites to `module_fn(...)`.
 *
 * Walks the text, and for each `alias.member` pair where alias is a known module
 * alias, emits `member` verbatim and skips past `alias.`. Everything else
 * (including string literals, since type text never contains them in practice)
 * is copied unchanged. */
void
strip_module_alias(char *dst, size_t dst_size, const KryFile *file,
                   const char *src)
{
    size_t n = 0;

    if(dst_size == 0)
        return;
    for(const char *p = src; *p != '\0' && n + 1 < dst_size;) {
        if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
           *p == '_') {
            char ident[KC_NAME_MAX];
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
            if(*after == '.' &&
               module_for_alias(file, ident, NULL, 0)) {
                const char *member = after + 1;

                while(*member == ' ' || *member == '\t')
                    member++;
                if((*member >= 'A' && *member <= 'Z') ||
                   (*member >= 'a' && *member <= 'z') ||
                   *member == '_') {
                    /* Drop `alias.` and copy only the member name. */
                    while((*member >= 'A' && *member <= 'Z') ||
                          (*member >= 'a' && *member <= 'z') ||
                          (*member >= '0' && *member <= '9') ||
                          *member == '_') {
                        if(n + 1 < dst_size)
                            dst[n++] = *member;
                        member++;
                    }
                    p = member;
                    continue;
                }
            }
            /* Not a qualified type: copy the identifier as-is. */
            {
                size_t i;

                for(i = 0; i < ident_len && n + 1 < dst_size; i++)
                    dst[n++] = ident[i];
            }
            continue;
        }
        dst[n++] = *p++;
    }
    dst[n] = '\0';
}

static int
function_value_context(const char *src, const char *start, const char *after)
{
    const char *prev = start;

    while(prev > src && (prev[-1] == ' ' || prev[-1] == '\t'))
        prev--;
    while(*after == ' ' || *after == '\t')
        after++;
    if(!(*after == '\0' || *after == ',' || *after == '}' || *after == ')'))
        return 0;
    if(prev == src)
        return 1;
    return prev[-1] == '=' || prev[-1] == ',' || prev[-1] == '{' ||
           prev[-1] == '(' || prev[-1] == '?';
}

static int
name_in_c_parameter_list(const char *args, const char *name)
{
    size_t name_len = strlen(name);

    for(const char *p = args; *p != '\0'; p++) {
        if(((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
            *p == '_') && strncmp(p, name, name_len) == 0 &&
           !((p[name_len] >= 'A' && p[name_len] <= 'Z') ||
             (p[name_len] >= 'a' && p[name_len] <= 'z') ||
             (p[name_len] >= '0' && p[name_len] <= '9') ||
             p[name_len] == '_')) {
            const char *after = p + name_len;

            while(*after == ' ' || *after == '\t')
                after++;
            if(*after == ',' || *after == '\0')
                return 1;
        }
    }
    return 0;
}

static int
kry_declares_name(const char *line, const char *name)
{
    char ident[KC_NAME_MAX];
    size_t name_len = strlen(name);
    const char *p = skip_indent(line);
    size_t ident_len = 0;

    if(strncmp(p, "for ", 4) == 0)
        p = skip_indent(p + 4);
    if(strncmp(p, "int ", 4) == 0)
        p = skip_indent(p + 4);
    while(((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
           (*p >= '0' && *p <= '9') || *p == '_' || *p == ',' ||
           *p == ' ' || *p == '\t') && *p != '\0') {
        if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
           *p == '_') {
            ident_len = 0;
            while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                  (*p >= '0' && *p <= '9') || *p == '_') {
                if(ident_len + 1 < sizeof(ident))
                    ident[ident_len++] = *p;
                p++;
            }
            ident[ident_len] = '\0';
            if(strcmp(ident, name) == 0) {
                const char *after = p;

                while(*after == ' ' || *after == '\t')
                    after++;
                if(*after == ':' || *after == ',' || *after == '=')
                    return 1;
            }
            continue;
        }
        if(*p == ':' || *p == '=')
            break;
        p++;
    }

    (void)name_len;
    return 0;
}

static int
function_has_local_name(const KryFunction *fn, const char *name)
{
    if(fn == NULL)
        return 0;
    if(name_in_c_parameter_list(fn->args, name))
        return 1;
    for(int i = 0; i < fn->body_count; i++) {
        if(kry_declares_name(fn->body[i], name))
            return 1;
    }
    return 0;
}

void
rewrite_kry_expr(char *dst, size_t dst_size, const KryFile *file,
                 const KryFunction *current_fn, const char *src)
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
                        const char *peek = after;
                        int written;

                        while(*peek == ' ' || *peek == '\t')
                            peek++;
                        if(*peek == '(') {
                            /* Qualified call: alias.fn(...) -> module_fn(...).
                             * Module functions are emitted with the module
                             * prefix. */
                            written = snprintf(dst + n, dst_size - n,
                                               "%s_%s", module, member);
                        } else {
                            /* Qualified type/value: alias.Type -> Type.
                             * Structs and enums use their bare declared name
                             * in C, so drop the module alias. */
                            written = snprintf(dst + n, dst_size - n,
                                               "%s", member);
                        }
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
            } else if((start == src ||
                       (start[-1] != '.' && start[-1] != '>')) &&
                      (*after == '(' ||
                       function_value_context(src, start, after))) {
                const KryFunction *fn = NULL;

                if(!function_has_local_name(current_fn, ident))
                    fn = find_local_function(file, ident);

                if(fn != NULL) {
                    char cname[KC_NAME_MAX * 2];
                    int written;

                    kc_function_name(cname, sizeof(cname), file, fn);
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

/* Token-level nil -> NULL rewrite used by the state/decl emitters. Unlike
 * rewrite_kry_expr this does no symbol resolution — it only swaps the literal
 * token, and is string-literal aware so nil inside a string is left alone. */
void
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

void
convert_var_decl_file(char *dst, size_t dst_size, const char *name,
                      const char *type, const KryFile *file)
{
    char tmp[512];
    char stripped[512];
    char *t;

    if(file != NULL)
        strip_module_alias(stripped, sizeof(stripped), file,
                           type != NULL ? type : "");
    else
        snprintf(stripped, sizeof(stripped), "%s",
                 type != NULL ? type : "");
    snprintf(tmp, sizeof(tmp), "%s", stripped);
    t = trim(tmp);
    if(t[0] == '[') {
        char dims[256] = "";
        char *p = t;

        while(*p == '[') {
            char *end = strchr(p, ']');
            char *size;

            if(end == NULL)
                break;
            *end = '\0';
            size = trim(p + 1);
            strncat(dims, "[", sizeof(dims) - strlen(dims) - 1);
            strncat(dims, size, sizeof(dims) - strlen(dims) - 1);
            strncat(dims, "]", sizeof(dims) - strlen(dims) - 1);
            p = trim(end + 1);
        }
        if(dims[0] != '\0') {
            snprintf(dst, dst_size, "%s %s%s", p, name, dims);
            return;
        }
    }
    if(t[0] == '*') {
        snprintf(dst, dst_size, "%s *%s", trim(t + 1), name);
        return;
    }
    snprintf(dst, dst_size, "%s %s", t, name);
}

void
convert_arg_list_file(char *dst, size_t dst_size, const char *src,
                      const KryFile *file)
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
        if(strcmp(part, "...") == 0) {
            snprintf(item, sizeof(item), "...");
        } else {
            colon = strchr(part, ':');
            if(colon == NULL) {
                snprintf(item, sizeof(item), "%s", part);
            } else {
                *colon = '\0';
                name = trim(part);
                type = trim(colon + 1);
                convert_var_decl_file(item, sizeof(item), name, type, file);
            }
        }
        if(out[0] != '\0')
            strncat(out, ", ", sizeof(out) - strlen(out) - 1);
        strncat(out, item, sizeof(out) - strlen(out) - 1);
    }
    snprintf(dst, dst_size, "%s", out);
}
