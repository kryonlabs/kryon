/*
 * kir.h - Kryon intermediate representation.
 *
 * KIR is the shared compiler representation between .kry source and the C/KRB
 * backends. This module owns the tree shape, explicit allocation, source spans,
 * and deterministic dumps used by tools and tests.
 */
#ifndef KRYON_KIR_H
#define KRYON_KIR_H

#include <stdio.h>

enum {
    KIR_PATH_MAX = 1024,
    KIR_NAME_MAX = 128,
    KIR_TEXT_MAX = 4096
};

typedef enum KirImportKind {
    KIR_IMPORT_HEADER = 1,
    KIR_IMPORT_MODULE,
    KIR_IMPORT_EXTERN,
    KIR_IMPORT_INTRINSIC,
    KIR_IMPORT_CAPABILITY,
    KIR_IMPORT_HOST
} KirImportKind;

typedef enum KirStmtKind {
    KIR_STMT_UNKNOWN = 0,
    KIR_STMT_BLOCK_OPEN,
    KIR_STMT_BLOCK_CLOSE,
    KIR_STMT_DECL,
    KIR_STMT_ASSIGN,
    KIR_STMT_EXPR,
    KIR_STMT_IF,
    KIR_STMT_WHILE,
    KIR_STMT_FOR,
    KIR_STMT_SWITCH,
    KIR_STMT_CASE,
    KIR_STMT_RETURN,
    KIR_STMT_BREAK,
    KIR_STMT_CONTINUE,
    KIR_STMT_GOTO,
    KIR_STMT_LABEL,
    KIR_STMT_DEFER,
    KIR_STMT_UNUSED,
    KIR_STMT_RAW,
    KIR_STMT_WIDGET
} KirStmtKind;

typedef enum KirExprKind {
    KIR_EXPR_UNKNOWN = 0,
    KIR_EXPR_IDENT,
    KIR_EXPR_INT,
    KIR_EXPR_STRING,
    KIR_EXPR_CALL,
    KIR_EXPR_BINARY
} KirExprKind;

typedef struct KirSourceSpan {
    char path[KIR_PATH_MAX];
    int line;
    int column;
} KirSourceSpan;

typedef struct KirStateField {
    char name[KIR_NAME_MAX];
    char type[KIR_NAME_MAX];
    char init[KIR_TEXT_MAX];
    char guard[KIR_TEXT_MAX];   /* enclosing '#if' condition (expanded) */
    KirSourceSpan span;
} KirStateField;

typedef struct KirImport {
    KirImportKind kind;
    char name[KIR_NAME_MAX];
    char target[KIR_PATH_MAX];
    char signature[KIR_TEXT_MAX];
    int required;
    char guard[KIR_TEXT_MAX];   /* enclosing '#if' condition (expanded) */
    KirSourceSpan span;
} KirImport;

typedef struct KirStmt {
    KirStmtKind kind;
    char text[KIR_TEXT_MAX];
    char widget[KIR_NAME_MAX];
    char args[KIR_TEXT_MAX];
    int expr_root;      /* index into enclosing function exprs, or -1 */
    KirSourceSpan span;
} KirStmt;

typedef struct KirExpr {
    KirExprKind kind;
    char text[KIR_TEXT_MAX];
    char name[KIR_NAME_MAX];
    char op[8];
    int left;
    int right;
    int first_child;
    int next_sibling;
    KirSourceSpan span;
} KirExpr;

typedef struct KirFunction {
    char name[KIR_NAME_MAX];
    char args[KIR_TEXT_MAX];
    char return_type[KIR_NAME_MAX];
    int exported;
    int is_extern;
    int is_colon;   /* 'Name :: (...) {' form: C name has no _kry_draw suffix */
    int is_public;  /* screen/preview/page keyword: a project route */
    char extern_target[KIR_NAME_MAX];   /* '#extern "pkg.Fn"' quoted symbol */
    char guard[KIR_TEXT_MAX];   /* enclosing '#if' condition (expanded) */
    KirSourceSpan span;
    KirStmt *stmts;
    int stmt_count;
    int stmt_cap;
    KirExpr *exprs;
    int expr_count;
    int expr_cap;
} KirFunction;

typedef struct KirGlobal {
    char name[KIR_NAME_MAX];
    char type[KIR_TEXT_MAX];
    char init[KIR_TEXT_MAX];
    int is_static;   /* 'static name: T = init' — internal linkage */
    char guard[KIR_TEXT_MAX];   /* enclosing '#if' condition (expanded) */
    KirSourceSpan span;
} KirGlobal;

/* A `Name :: #define value` module constant — emitted as a real C #define. */
typedef struct KirDefine {
    char name[KIR_NAME_MAX];
    char value[KIR_TEXT_MAX];
    char guard[KIR_TEXT_MAX];   /* enclosing '#if' condition (expanded) */
    KirSourceSpan span;
} KirDefine;

typedef struct KirAssert {
    char condition[KIR_TEXT_MAX]; /* expanded C preprocessor condition */
    char message[KIR_TEXT_MAX];   /* C #error payload */
    int known;                    /* condition resolved by Kry frontend */
    int value;                    /* boolean value when known */
    char guard[KIR_TEXT_MAX];     /* enclosing '#if' condition (expanded) */
    KirSourceSpan span;
} KirAssert;

/* A `Name :: struct { fields }` type declaration. body holds the raw field
 * lines (one per line, no braces). */
typedef struct KirType {
    char name[KIR_NAME_MAX];
    char body[KIR_TEXT_MAX * 2];
    int is_enum;   /* 'Name :: enum' — emit typedef enum, not struct */
    char guard[KIR_TEXT_MAX];   /* enclosing '#if' condition (expanded) */
    KirSourceSpan span;
} KirType;

typedef struct KirAppMeta {
    int has_app;
    char title[KIR_NAME_MAX];
    int width;
    int height;
    int fps;
    char theme[KIR_NAME_MAX];
    int dark_mode;
    int font_examples;
    char frame[KIR_NAME_MAX];
    char init[KIR_NAME_MAX];
    char scene[KIR_NAME_MAX];
    char shutdown[KIR_NAME_MAX];
} KirAppMeta;

typedef struct KirModule {
    char name[KIR_NAME_MAX];
    char source_path[KIR_PATH_MAX];
    KirSourceSpan span;
    KirAppMeta app;
    KirGlobal *globals;
    int global_count;
    int global_cap;
    KirDefine *defines;
    int define_count;
    int define_cap;
    KirAssert *asserts;
    int assert_count;
    int assert_cap;
    KirType *types;
    int type_count;
    int type_cap;
    KirStateField *state_fields;
    int state_count;
    int state_cap;
    KirImport *imports;
    int import_count;
    int import_cap;
    KirFunction *functions;
    int function_count;
    int function_cap;
} KirModule;

typedef struct KirProgram {
    KirModule *modules;
    int module_count;
    int module_cap;
} KirProgram;

KirProgram *KirProgramNew(void);
void KirProgramFree(KirProgram *program);
KirSourceSpan KirSpan(const char *path, int line, int column);
KirModule *KirProgramAddModule(KirProgram *program, const char *name,
                               const char *source_path, KirSourceSpan span);
KirStateField *KirModuleAddStateField(KirModule *module, const char *name,
                                      const char *type, const char *init,
                                      KirSourceSpan span);
KirImport *KirModuleAddImport(KirModule *module, KirImportKind kind,
                              const char *name, const char *target,
                              const char *signature, int required,
                              KirSourceSpan span);
KirFunction *KirModuleAddFunction(KirModule *module, const char *name,
                                  const char *args, const char *return_type,
                                  int exported, KirSourceSpan span);
void KirModuleAddGlobal(KirModule *module, const char *name, const char *type,
                        const char *init, KirSourceSpan span);
void KirModuleAddStatic(KirModule *module, const char *name, const char *type,
                        const char *init, KirSourceSpan span);
KirDefine *KirModuleAddDefine(KirModule *module, const char *name,
                              const char *value, KirSourceSpan span);
KirAssert *KirModuleAddAssert(KirModule *module, const char *condition,
                              const char *message, KirSourceSpan span);
KirType *KirModuleAddType(KirModule *module, const char *name,
                          KirSourceSpan span);
KirStmt *KirFunctionAddStmt(KirFunction *fn, KirStmtKind kind,
                            const char *text, const char *widget,
                            KirSourceSpan span);
KirStmt *KirFunctionAddWidget(KirFunction *fn, const char *widget,
                              const char *args, const char *text,
                              KirSourceSpan span);
const char *KirImportKindName(KirImportKind kind);
const char *KirStmtKindName(KirStmtKind kind);
const char *KirExprKindName(KirExprKind kind);
KirExpr *KirFunctionAddExpr(KirFunction *fn, KirExprKind kind,
                            const char *text, KirSourceSpan span);
void KirProgramDump(const KirProgram *program, FILE *out);

#endif /* KRYON_KIR_H */
