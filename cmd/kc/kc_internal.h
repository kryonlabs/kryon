#ifndef KRYON_KC_INTERNAL_H
#define KRYON_KC_INTERNAL_H

#include <stddef.h>

enum {
    KC_PATH_MAX = 1024,
    KC_TEXT_MAX = 1024 * 1024,
    KC_NAME_MAX = 128,
    KC_INCLUDE_MAX = 64,
    KC_USE_MAX = 32,
    KC_CALL_MAX = 64,
    KC_CONST_MAX = 64,
    KC_DEFINE_MAX = 64,
    KC_TYPE_MAX = 256,
    KC_RAW_MAX = 1024,
    KC_STATE_MAX = 128,
    KC_BODY_MAX = 512,
    KC_BODY_LINE_MAX = 1024,
};

/* A recorded diagnostic (Phase 4 error recovery). kc fills these during parse
 * instead of aborting on the first error, so the IDE can show many at once. */
#define KC_DIAGNOSTIC_MAX 64

typedef struct KryDiagnostic {
    char path[KC_PATH_MAX];
    int line;
    int column;
    char message[KC_BODY_LINE_MAX];
} KryDiagnostic;

typedef struct KryFunction {
    char screen[KC_NAME_MAX];
    char args[512];
    char return_type[KC_NAME_MAX];
    char guard[KC_BODY_LINE_MAX];
    int exact_name;
    int is_public;
    int global_name;
    char calls[KC_CALL_MAX][512];
    char body[KC_BODY_MAX][KC_BODY_LINE_MAX];
    int body_line[KC_BODY_MAX];
    int call_count;
    int body_count;
} KryFunction;

typedef struct KryFile {
    char *path;
    const char *root;
    char display_path[KC_PATH_MAX];
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
    char public_globals[KC_STATE_MAX][KC_BODY_LINE_MAX];
    char globals[KC_STATE_MAX][KC_BODY_LINE_MAX];
    char includes[KC_INCLUDE_MAX][KC_PATH_MAX];
    char include_guards[KC_INCLUDE_MAX][KC_BODY_LINE_MAX];
    char const_names[KC_CONST_MAX][KC_NAME_MAX];
    char const_exprs[KC_CONST_MAX][KC_BODY_LINE_MAX];
    char define_names[KC_DEFINE_MAX][KC_NAME_MAX];
    char define_values[KC_DEFINE_MAX][KC_BODY_LINE_MAX];
    char define_guards[KC_DEFINE_MAX][KC_BODY_LINE_MAX];
    char module[KC_NAME_MAX];
    char module_file[KC_NAME_MAX];
    char use_aliases[KC_USE_MAX][KC_NAME_MAX];
    char use_modules[KC_USE_MAX][KC_NAME_MAX];
    KryFunction *functions;
    int raw_count;
    int public_type_count;
    int private_type_count;
    int state_count;
    int public_global_count;
    int global_count;
    int include_count;
    int const_count;
    int define_count;
    int use_count;
    int function_count;
    int function_cap;
    int current_line;
    KryFunction *current;
    KryDiagnostic diagnostics[KC_DIAGNOSTIC_MAX];
    int diagnostic_count;
} KryFile;

typedef struct KryMacroFrame {
    char condition[KC_BODY_LINE_MAX];
    char excluded[KC_BODY_LINE_MAX];
} KryMacroFrame;

void add_raw_line(KryFile *file, const char *line);
void add_type_line(KryFile *file, int is_public, const char *fmt, ...);
void add_state_line(KryFile *file, const char *line);
void expand_compile_expr(char *dst, size_t dst_size, const KryFile *file,
                         const char *src);

void add_guard_line(KryFile *file, const char *guard, int is_public_type,
                    int is_type, int is_state);
void add_guard_end(KryFile *file, int is_public_type, int is_type,
                   int is_state);
void combine_compile_guards(char *dst, size_t dst_size, const char *left,
                            const char *right);
void current_macro_guard(char *dst, size_t dst_size,
                         const KryMacroFrame *macros, int macro_count);
void append_macro_excluded(char *dst, size_t dst_size, const char *current,
                           const char *next);

/* --- error recovery (Phase 4) ------------------------------------------- */

/* Record a diagnostic and continue parsing. Use in place of die() for
 * recoverable parse errors (bad statement, missing token, etc.). Fatal errors
 * (out of memory, bad CLI args) still use die(). */
void kc_error(KryFile *file, int line_no, const char *fmt, ...);

/* Print all accumulated diagnostics to stderr and return the count. Caller
 * exits nonzero if any were printed. */
int kc_flush_diagnostics(const KryFile *file);

#endif
