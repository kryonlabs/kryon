#include "kc_internal.h"

#include <stdio.h>
#include <string.h>

void
add_guard_line(KryFile *file, const char *guard, int is_public_type,
               int is_type, int is_state)
{
    char line[KC_BODY_LINE_MAX];

    if(guard == NULL || guard[0] == '\0')
        return;
    snprintf(line, sizeof(line), "#if %s", guard);
    if(is_type)
        add_type_line(file, is_public_type, "%s", line);
    else if(is_state)
        add_state_line(file, line);
    else
        add_raw_line(file, line);
}

void
add_guard_end(KryFile *file, int is_public_type, int is_type, int is_state)
{
    if(is_type)
        add_type_line(file, is_public_type, "#endif");
    else if(is_state)
        add_state_line(file, "#endif");
    else
        add_raw_line(file, "#endif");
}

void
combine_compile_guards(char *dst, size_t dst_size, const char *left,
                       const char *right)
{
    const char *a = left != NULL ? left : "";
    const char *b = right != NULL ? right : "";

    if(a[0] == '\0') {
        snprintf(dst, dst_size, "%s", b);
    } else if(b[0] == '\0') {
        snprintf(dst, dst_size, "%s", a);
    } else {
        snprintf(dst, dst_size, "(%s) && (%s)", a, b);
    }
}

void
current_macro_guard(char *dst, size_t dst_size, const KryMacroFrame *macros,
                    int macro_count)
{
    char guard[KC_BODY_LINE_MAX] = "";

    for(int i = 0; i < macro_count; i++) {
        char next[KC_BODY_LINE_MAX];

        combine_compile_guards(next, sizeof(next), guard,
                               macros[i].condition);
        snprintf(guard, sizeof(guard), "%s", next);
    }
    snprintf(dst, dst_size, "%s", guard);
}

void
append_macro_excluded(char *dst, size_t dst_size, const char *current,
                      const char *next)
{
    if(current == NULL || current[0] == '\0')
        snprintf(dst, dst_size, "%s", next);
    else
        snprintf(dst, dst_size, "(%s) || (%s)", current, next);
}
