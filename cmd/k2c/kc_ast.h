/*
 * kc_ast.h - Abstract Syntax Tree node types for the kc compiler.
 *
 * Phase 1 of the parser migration (docs/KC_PARSER_PLAN.md): the AST is built
 * alongside the existing string-based body[] path. Today the nodes are
 * reconstructed from the already-emitted body[] fragments to prove the
 * structure is fully recoverable; Phase 2 will build them directly during
 * parsing and emit from them.
 *
 * Expressions are held as opaque C text. kc does not parse expressions today;
 * a future phase may tokenize them for real type inference.
 */
#ifndef KRYON_KC_AST_H
#define KRYON_KC_AST_H

#include "kc_internal.h"  /* KryFunction, KC_* limits */

/* Statement kinds, matching the parse_statement branch inventory. Every
 * non-removed statement form maps to exactly one kind. */
typedef enum {
    AST_STMT_UNKNOWN = 0,   /* unclassified (shouldn't happen post-Phase-1) */
    AST_STMT_BLOCK_OPEN,    /* anonymous `{` scope open */
    AST_STMT_BLOCK_CLOSE,   /* `}` close of any scope */
    AST_STMT_DECL,          /* x := expr  |  x: Type = expr  |  x: Type */
    AST_STMT_ASSIGN,        /* x = expr   (incl. compound += %= etc.) */
    AST_STMT_EXPR,          /* bare call / expression statement */
    AST_STMT_IF,            /* if(...) {  (also else-if, else) */
    AST_STMT_WHILE,         /* while(...) { */
    AST_STMT_FOR,           /* for(...) {  (C-style or NAME in A..B) */
    AST_STMT_SWITCH,        /* switch(...) { */
    AST_STMT_CASE,          /* case X: / default: */
    AST_STMT_RETURN,        /* return [expr]; */
    AST_STMT_BREAK,
    AST_STMT_CONTINUE,
    AST_STMT_GOTO,          /* goto label; */
    AST_STMT_LABEL,         /* label: */
    AST_STMT_DEFER,         /* defer stmt  (lowered later) */
    AST_STMT_UNUSED,        /* unused expr; */
    AST_STMT_RAW,           /* c <line>  escape hatch */
    AST_STMT_ENUM,          /* inline enum { ... } */
} AstStmtKind;

/* A reconstructed statement node. Phase 1 holds the source text fragment and
 * the scope depth at which it sits; Phase 2 will add structured fields. */
typedef struct AstStmt {
    AstStmtKind kind;
    int source_line;        /* the .kry line (from fn->body_line, or 0) */
    int depth;              /* scope depth (1 = function body) */
    const char *text;       /* the body[] fragment this was built from */
} AstStmt;

/* A reconstructed function: its statements with computed depths. */
typedef struct AstFunction {
    const KryFunction *fn;  /* the source function */
    AstStmt *stmts;         /* array of reconstructed statements */
    int stmt_count;
} AstFunction;

/* Reconstruct an AST from a function's body[] strings. Returns a malloc'd
 * AstFunction (NULL if fn is NULL or empty); caller frees with
 * ast_function_free. The kind classification is best-effort from the text
 * fragment — anything unclassifiable becomes AST_STMT_UNKNOWN so callers can
 * detect incomplete capture. */
AstFunction *ast_function_from_body(const KryFunction *fn);

/* Free an AstFunction built by ast_function_from_body. */
void ast_function_free(AstFunction *af);

/* Print a human-readable dump of the AST to stdout, one statement per line
 * with depth indentation and the kind label. For the --dump-ast debug mode. */
void ast_function_dump(const AstFunction *af);

/* Count how many statements of a given kind appear in the function. Useful
 * for tests that assert the AST captured expected constructs. */
int ast_function_count_kind(const AstFunction *af, AstStmtKind kind);

#endif /* KRYON_KC_AST_H */
