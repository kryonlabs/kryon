/*
 * k2b — standalone .kry -> .krb cartridge compiler. Fully independent of kc
 * (kc is .kry -> .c). This header is private to the cmd/k2b/ translation units.
 */
#ifndef K2B_H
#define K2B_H

#include <stddef.h>

enum {
    K2B_PATH_MAX = 1024,
    K2B_NAME_MAX = 128,
    K2B_LINE_MAX = 1024,
    K2B_STATE_MAX = 128,
    K2B_FUNC_MAX = 32,
    K2B_STMT_MAX = 256
};

/* Statement classification of raw .kry lines. Only IF and BLOCK_CLOSE are
 * load-bearing for the cartridge codegen (button-handler capture); the rest
 * are recorded so the body shape is recoverable. */
enum {
    K2B_STMT_UNKNOWN = 0,
    K2B_STMT_BLOCK_OPEN,
    K2B_STMT_BLOCK_CLOSE,
    K2B_STMT_DECL,
    K2B_STMT_ASSIGN,
    K2B_STMT_EXPR,
    K2B_STMT_IF,
    K2B_STMT_ELSE,
    K2B_STMT_WHILE,
    K2B_STMT_FOR,
    K2B_STMT_SWITCH,
    K2B_STMT_CASE,
    K2B_STMT_RETURN,
    K2B_STMT_BREAK,
    K2B_STMT_CONTINUE,
    K2B_STMT_RAW
};

typedef struct K2bStmt {
    int kind;
    int depth;
    char text[K2B_LINE_MAX];
} K2bStmt;

typedef struct K2bFunction {
    char screen[K2B_NAME_MAX];   /* function name (screen/frame/fn/...) */
    K2bStmt stmts[K2B_STMT_MAX];
    int stmt_count;
} K2bFunction;

typedef struct K2bFile {
    char path[K2B_PATH_MAX];
    char root[K2B_PATH_MAX];
    char module_file[K2B_PATH_MAX];
    int no_main;
    char app_title[512];
    int app_width;
    int app_height;
    int app_fps;
    char app_theme[K2B_NAME_MAX];
    int app_dark_mode;
    int app_font_examples;
    char app_frame[K2B_NAME_MAX];
    char app_init[K2B_NAME_MAX];
    char app_scene[K2B_NAME_MAX];
    char app_shutdown[K2B_NAME_MAX];
    char state[K2B_STATE_MAX][K2B_LINE_MAX];   /* C-style decl strings */
    int state_count;
    K2bFunction functions[K2B_FUNC_MAX];
    int function_count;
} K2bFile;

/* k2b_util.c — same names the moved codegen already calls. */
void die(const char *fmt, ...);
void path_join(char *dst, size_t dst_size, const char *a, const char *b);
void mkdir_parent(const char *path);
void header_guard(char *dst, size_t dst_size, const char *rel);
void strip_kry_ext(char *dst, size_t dst_size, const char *path);
void replace_path_basename(char *dst, size_t dst_size, const char *path,
                           const char *base);
const char *relative_path(const char *root, const char *path);
int is_ident_text(const char *text);
void c_string_literal(char *dst, size_t dst_size, const char *src);

/* k2b_parse.c — read a .kry source into file (state/app/functions). */
int k2b_parse_file(K2bFile *file, const char *path, const char *root);

/* k2b_krb.c — emit the .krb + .krb.c/.h host for a parsed file. */
void write_krb(const K2bFile *file, const char *root, const char *out_dir);

#endif /* K2B_H */
