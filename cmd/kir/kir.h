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
    KIR_TEXT_MAX = 1024
};

typedef enum KirImportKind {
    KIR_IMPORT_HEADER = 1,
    KIR_IMPORT_MODULE,
    KIR_IMPORT_EXTERN,
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

typedef struct KirSourceSpan {
    char path[KIR_PATH_MAX];
    int line;
    int column;
} KirSourceSpan;

typedef struct KirStateField {
    char name[KIR_NAME_MAX];
    char type[KIR_NAME_MAX];
    char init[KIR_TEXT_MAX];
    KirSourceSpan span;
} KirStateField;

typedef struct KirImport {
    KirImportKind kind;
    char name[KIR_NAME_MAX];
    char target[KIR_PATH_MAX];
    char signature[KIR_TEXT_MAX];
    int required;
    KirSourceSpan span;
} KirImport;

typedef struct KirStmt {
    KirStmtKind kind;
    char text[KIR_TEXT_MAX];
    char widget[KIR_NAME_MAX];
    KirSourceSpan span;
} KirStmt;

typedef struct KirFunction {
    char name[KIR_NAME_MAX];
    char args[KIR_TEXT_MAX];
    char return_type[KIR_NAME_MAX];
    int exported;
    int is_extern;
    int is_colon;   /* 'Name :: (...) {' form: C name has no _kry_draw suffix */
    int is_public;  /* screen/preview/page keyword: a project route */
    KirSourceSpan span;
    KirStmt *stmts;
    int stmt_count;
    int stmt_cap;
} KirFunction;

typedef struct KirGlobal {
    char name[KIR_NAME_MAX];
    char type[KIR_TEXT_MAX];
    char init[KIR_TEXT_MAX];
    int is_static;   /* 'static name: T = init' — internal linkage */
    KirSourceSpan span;
} KirGlobal;

/* A `Name :: struct { fields }` type declaration. body holds the raw field
 * lines (one per line, no braces). */
typedef struct KirType {
    char name[KIR_NAME_MAX];
    char body[KIR_TEXT_MAX * 2];
    int is_enum;   /* 'Name :: enum' — emit typedef enum, not struct */
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
KirType *KirModuleAddType(KirModule *module, const char *name,
                          KirSourceSpan span);
KirStmt *KirFunctionAddStmt(KirFunction *fn, KirStmtKind kind,
                            const char *text, const char *widget,
                            KirSourceSpan span);
const char *KirImportKindName(KirImportKind kind);
const char *KirStmtKindName(KirStmtKind kind);
void KirProgramDump(const KirProgram *program, FILE *out);

#endif /* KRYON_KIR_H */
