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
    int is_scene; /* scene Name { ... } builder: emits Name_kry_scene(KryScene *) */
    char calls[KC_CALL_MAX][512];
    /* Function bodies grow on demand (see grow_body()). body[i] holds one
     * pre-translated C statement line; body_line[i] is its .kry source line.
     * There is no fixed per-function cap — large UI draw functions can emit
     * thousands of lines. */
    char **body;
    int *body_line;
    int body_cap;
    int call_count;
    int body_count;
} KryFunction;

typedef struct KryRoute {
    char id[KC_NAME_MAX];
    char title[KC_NAME_MAX];
    char group[KC_NAME_MAX];
    char page[KC_NAME_MAX];
    char source_path[KC_PATH_MAX];
    char guard[KC_BODY_LINE_MAX];
} KryRoute;

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
    /* Optional lifecycle hooks named in the app{} block. When `frame` is set,
     * kc emits a main() that calls init() once, frame() each iteration, and
     * shutdown() at exit — giving the program full ownership of its loop. When
     * unset, kc falls back to the single-screen main() used by the examples. */
    char app_init[KC_NAME_MAX];
    char app_frame[KC_NAME_MAX];
    char app_shutdown[KC_NAME_MAX];
    char app_scene[KC_NAME_MAX]; /* scene builder to run at startup for the scene-tree main() */
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
    KryRoute *routes;
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
    int route_count;
    int route_cap;
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
void add_global_line(KryFile *file, int is_public, const char *line);
void add_const(KryFile *file, int line_no, const char *name, const char *expr);
void add_define(KryFile *file, int line_no, const char *name, const char *value,
                const char *guard);
KryFunction *add_function(KryFile *file);
KryRoute *add_route(KryFile *file, int line_no, const char *id,
                    const KryMacroFrame *macros, int macro_count);
void parse_route_property(KryFile *file, int line_no, KryRoute *route,
                          char *line);
void grow_body(KryFunction *fn);
void add_body_line(KryFile *file, int source_line, const char *fmt, ...);
void add_body(KryFile *file, const char *fmt, ...);
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
void write_project_source(KryFile **files, int file_count, const char *root,
                          const char *out_dir);
void write_project_header(KryFile **files, int file_count, const char *root,
                          const char *out_dir);
void write_generated(const KryFile *file, const char *root,
                     const char *out_dir);
void generated_header_rel(char *dst, size_t dst_size, const KryFile *file,
                          const char *root);

/* Build the per-function C name base: the screen name, suffixed with
 * "_kry_draw" unless the function declared an exact name. Does not prepend the
 * module prefix — callers do that when the function is module-local. */
void kc_function_base_name(char *dst, size_t dst_size, const KryFunction *fn);

/* Resolve a function's full C name, prepending the module prefix for
 * module-local functions. Shared by codegen and project emission. */
void kc_function_name(char *dst, size_t dst_size, const KryFile *file,
                      const KryFunction *fn);

/* --- kc_util.c: string/path/brace leaf helpers --------------------------- */

/* Universal error path. When a "%s:%d:" location prefix is present and error
 * recovery is active (parse_kry installs a setjmp boundary), the message is
 * recorded as a diagnostic and control returns to the boundary so parsing can
 * continue. Otherwise the message is printed and the process exits. */
void die(const char *fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noreturn))
#endif
    ;

/* --- kc_diag.c: diagnostics + error recovery ----------------------------- */

#include <setjmp.h>

/* Recovery target for parse_kry's per-statement boundary. setjmp() must run in
 * the caller's own frame, so the buffer is handed out by reference and the
 * caller does setjmp(*kc_recover_buf()). */
jmp_buf *kc_recover_buf(void);
void kc_set_recovery_file(KryFile *file);
void kc_set_recovering(int on);

/* Record a diagnostic and continue parsing. Use in place of die() for
 * recoverable parse errors (bad statement, missing token, etc.). Fatal errors
 * (out of memory, bad CLI args) still use die(). */
void kc_error(KryFile *file, int line_no, const char *fmt, ...);

/* Print all accumulated diagnostics to stderr and return the count. Caller
 * exits nonzero if any were printed. */
int kc_flush_diagnostics(const KryFile *file);

char *trim(char *s);
int starts_word(const char *s, const char *word);
int starts_statement_word(const char *s, const char *word);
int starts_header_directive(const char *s, const char *word);
int parse_ident(char **sp, char *dst, size_t dst_size);
int is_ident_text(const char *text);
int line_is_goto_label(const char *line, char *label, size_t label_size);
int parse_quoted(char **sp, char *dst, size_t dst_size);
int parse_c_header_token(char **sp, char *dst, size_t dst_size);
void c_string_literal(char *dst, size_t dst_size, const char *src);
void module_symbol(char *dst, size_t dst_size, const char *module);
void module_header(char *dst, size_t dst_size, const char *module);
void validate_output_name(KryFile *file, int line_no, const char *name);
void replace_path_basename(char *dst, size_t dst_size, const char *path,
                           const char *base);
const char *relative_path(const char *root, const char *path);
void normalize_source_path(char *dst, size_t dst_size, const char *path);
void path_join(char *dst, size_t dst_size, const char *a, const char *b);
void strip_kry_ext(char *dst, size_t dst_size, const char *path);
void mkdir_parent(const char *path);
void header_guard(char *dst, size_t dst_size, const char *rel);
const char *skip_indent(const char *line);
int brace_delta(const char *line);

/* --- kc_resolve.c: symbol/module resolution + expression rewrite --------- */

void strip_module_alias(char *dst, size_t dst_size, const KryFile *file,
                        const char *src);
void rewrite_kry_expr(char *dst, size_t dst_size, const KryFile *file,
                      const KryFunction *current_fn, const char *src);
void rewrite_nil_tokens(char *dst, size_t dst_size, const char *src);
void convert_var_decl_file(char *dst, size_t dst_size, const char *name,
                           const char *type, const KryFile *file);
void convert_arg_list_file(char *dst, size_t dst_size, const char *src,
                           const KryFile *file);

/* --- kc_defer.c: defer lowering ------------------------------------------ */

/* Splice deferred statements into a function body at every scope exit
 * (fall-through, return, break, continue). Rewrites fn->body[] in place. */
void apply_defers(KryFunction *fn);

#endif
