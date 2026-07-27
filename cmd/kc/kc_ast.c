/*
 * kc_ast.c - AST reconstruction (Phase 1 of the parser migration).
 *
 * Builds typed AstStmt nodes from a function's already-emitted body[] string
 * fragments, proving the structure is fully recoverable from the line-matcher
 * output. Scope depth is computed by replaying write_body_line's brace
 * accounting (the same logic apply_defers uses).
 *
 * Phase 2 will build nodes directly during parsing; this reconstruction pass
 * is the validation that such a migration is safe — if every body[] fragment
 * classifies into a known kind, the AST captures 100% of what kc emits.
 */
#include "kc_ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Skip leading whitespace (mirrors skip_indent in kc.c, kept local to avoid a
 * cross-module static dependency). */
static const char *
skip_ws(const char *s)
{
    while(s != NULL && (*s == ' ' || *s == '\t'))
        s++;
    return s;
}

/* Count `{` minus `}` in a line, skipping preprocessor lines (which start
 * with `#`). Mirrors brace_delta in kc.c. */
static int
brace_delta(const char *text)
{
    int delta = 0;

    if(text[0] == '#')
        return 0;
    for(const char *p = text; *p != '\0'; p++) {
        if(*p == '{')
            delta++;
        else if(*p == '}')
            delta--;
    }
    return delta;
}

/* Classify a single body[] fragment into an AstStmtKind by its leading token.
 * The fragments are the exact strings parse_statement emits (e.g.
 * "    return;", "    if(x) {", "    }", "loop_start:"). */
static AstStmtKind
classify_fragment(const char *raw)
{
    const char *t = skip_ws(raw);

    if(t[0] == '\0')
        return AST_STMT_UNKNOWN;

    /* Closing brace closes any scope. */
    if(t[0] == '}')
        return AST_STMT_BLOCK_CLOSE;

    /* Control-flow headers and keywords — match by leading token. */
    if(strncmp(t, "for(", 4) == 0 || strncmp(t, "for (", 5) == 0)
        return AST_STMT_FOR;
    if(strncmp(t, "while(", 6) == 0 || strncmp(t, "while (", 7) == 0)
        return AST_STMT_WHILE;
    if(strncmp(t, "switch(", 7) == 0 || strncmp(t, "switch (", 8) == 0)
        return AST_STMT_SWITCH;
    if(strncmp(t, "case ", 5) == 0 || strcmp(t, "default:") == 0)
        return AST_STMT_CASE;
    if(strncmp(t, "if(", 3) == 0 || strncmp(t, "if (", 4) == 0 ||
       strncmp(t, "else if(", 8) == 0 || strncmp(t, "else if (", 9) == 0 ||
       strncmp(t, "else {", 5) == 0 || strncmp(t, "} else", 6) == 0)
        return AST_STMT_IF;
    if(strncmp(t, "return", 6) == 0 &&
       (t[6] == ';' || t[6] == ' ' || t[6] == '\0'))
        return AST_STMT_RETURN;
    if(strcmp(t, "break;") == 0)
        return AST_STMT_BREAK;
    if(strcmp(t, "continue;") == 0)
        return AST_STMT_CONTINUE;
    if(strncmp(t, "goto ", 5) == 0)
        return AST_STMT_GOTO;

    /* defer marker: parse_statement emits "    defer <stmt>" which survives
     * in body[] only until apply_defers runs. During reconstruction we may
     * see it if dump runs pre-lowering. */
    if(strncmp(t, "defer ", 6) == 0 || strcmp(t, "defer") == 0)
        return AST_STMT_DEFER;

    /* PushUIInspectSource markers wrap expr statements; the wrapped call is
     * the real statement. Treat the push as a side-channel marker and
     * classify the following non-push line. */
    if(strncmp(t, "PushUIInspectSource(", 20) == 0 ||
       strncmp(t, "PopUIInspectSource();", 21) == 0)
        return AST_STMT_EXPR;   /* inspection scaffolding, not a user stmt */

    /* A bare `{` opens an anonymous block scope. */
    if(strcmp(t, "{") == 0)
        return AST_STMT_BLOCK_OPEN;

    /* Inline enum block: "enum {" or "enum { TAG, }". */
    if(strncmp(t, "enum {", 6) == 0 || strncmp(t, "enum{", 5) == 0)
        return AST_STMT_ENUM;

    /* A line ending in ':' with no leading space and no trailing code is a
     * goto label (kc emits "label:" with no indent). */
    {
        size_t n = strlen(t);
        if(n > 1 && t[n - 1] == ':' && strchr(t, ' ') == NULL &&
           strchr(t, ';') == NULL && t[0] != '#')
            return AST_STMT_LABEL;
    }

    /* unused: "(void)name;" — emitted as "(void)expr;". */
    if(strncmp(t, "(void)", 6) == 0)
        return AST_STMT_UNUSED;

    /* Mutation statements: x++, x--, x += y, etc. (no `=` as declaration). */
    if(strstr(t, "++") != NULL || strstr(t, "--") != NULL ||
       strstr(t, "+=") != NULL || strstr(t, "-=") != NULL ||
       strstr(t, "*=") != NULL || strstr(t, "/=") != NULL ||
       strstr(t, "%=") != NULL || strstr(t, "&=") != NULL ||
       strstr(t, "|=") != NULL || strstr(t, "^=") != NULL ||
       strstr(t, "<<=") != NULL || strstr(t, ">>=") != NULL)
        return AST_STMT_ASSIGN;

    /* Declarations vs assignments. kc emits:
     *   __auto_type name = expr;           → inferred decl
     *   Type name = expr;                  → typed decl (Type is >=1 word)
     *   Type name[N] = {...};              → typed array decl
     *   name = expr;                       → assignment
     *   name.field = expr; / name[i] = ... → assignment
     * A typed decl has a multi-token prefix (the type) before the lvalue. */
    if(strstr(t, "=") != NULL) {
        /* __auto_type ... = ... is an inferred decl. */
        if(strncmp(t, "__auto_type", 11) == 0)
            return AST_STMT_DECL;

        /* Heuristic: a typed declaration's first token is a C type keyword or
         * a capitalized identifier (a typedef/struct name like Color,
         * Rectangle). An assignment's first token is a lowercase variable
         * name. If the first token looks like a type and there's a second
         * token (the name), classify as DECL. */
        {
            char first[128];
            const char *p = t;
            size_t fn = 0;

            while((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                  *p == '_') {
                if(fn + 1 < sizeof(first))
                    first[fn++] = *p;
                p++;
            }
            first[fn] = '\0';
            /* Skip whitespace/type-qualifiers to see if there's a name after. */
            while(*p == ' ' || *p == '\t')
                p++;
            /* Known C type keywords that start a typed declaration. */
            if(fn > 0 && *p != '\0' && *p != '=' &&
               (strcmp(first, "int") == 0 || strcmp(first, "char") == 0 ||
                strcmp(first, "float") == 0 || strcmp(first, "double") == 0 ||
                strcmp(first, "void") == 0 || strcmp(first, "long") == 0 ||
                strcmp(first, "short") == 0 || strcmp(first, "unsigned") == 0 ||
                strcmp(first, "signed") == 0 || strcmp(first, "const") == 0 ||
                strcmp(first, "static") == 0 || strcmp(first, "bool") == 0 ||
                (first[0] >= 'A' && first[0] <= 'Z')))
                return AST_STMT_DECL;
        }
        return AST_STMT_ASSIGN;
    }

    /* Bare statement ending in ';' or ')' is an expression/call. */
    return AST_STMT_EXPR;
}

AstFunction *
ast_function_from_body(const KryFunction *fn)
{
    AstFunction *af;
    int depth;
    int i;

    if(fn == NULL || fn->body_count <= 0)
        return NULL;
    af = calloc(1, sizeof(*af));
    if(af == NULL)
        return NULL;
    af->fn = fn;
    af->stmts = calloc((size_t)fn->body_count, sizeof(*af->stmts));
    if(af->stmts == NULL) {
        free(af);
        return NULL;
    }

    /* Replay write_body_line's brace accounting to compute per-line depth. */
    depth = 1;  /* function body starts at depth 1 */
    for(i = 0; i < fn->body_count; i++) {
        const char *raw = fn->body[i];
        const char *text = skip_ws(raw);
        AstStmt *s = &af->stmts[i];
        int delta;

        /* A closing brace sits at the depth of the scope it closes. */
        if(text[0] == '}')
            depth--;
        if(depth < 1)
            depth = 1;
        s->depth = depth;
        s->text = raw;
        s->source_line = (i < fn->body_count && fn->body_line[i] != 0)
                             ? fn->body_line[i]
                             : 0;
        s->kind = classify_fragment(raw);
        /* Advance depth for braces opened on this line. */
        delta = brace_delta(text);
        if(text[0] == '}')
            delta++;   /* cancel the decrement above for net counting */
        depth += delta;
        if(depth < 1)
            depth = 1;
    }
    af->stmt_count = fn->body_count;
    return af;
}

void
ast_function_free(AstFunction *af)
{
    if(af == NULL)
        return;
    free(af->stmts);
    free(af);
}

static const char *
kind_name(AstStmtKind k)
{
    switch(k) {
    case AST_STMT_UNKNOWN:     return "UNKNOWN";
    case AST_STMT_BLOCK_OPEN:  return "BLOCK_OPEN";
    case AST_STMT_BLOCK_CLOSE: return "BLOCK_CLOSE";
    case AST_STMT_DECL:        return "DECL";
    case AST_STMT_ASSIGN:      return "ASSIGN";
    case AST_STMT_EXPR:        return "EXPR";
    case AST_STMT_IF:          return "IF";
    case AST_STMT_WHILE:       return "WHILE";
    case AST_STMT_FOR:         return "FOR";
    case AST_STMT_SWITCH:      return "SWITCH";
    case AST_STMT_CASE:        return "CASE";
    case AST_STMT_RETURN:      return "RETURN";
    case AST_STMT_BREAK:       return "BREAK";
    case AST_STMT_CONTINUE:    return "CONTINUE";
    case AST_STMT_GOTO:        return "GOTO";
    case AST_STMT_LABEL:       return "LABEL";
    case AST_STMT_DEFER:       return "DEFER";
    case AST_STMT_UNUSED:      return "UNUSED";
    case AST_STMT_RAW:         return "RAW";
    case AST_STMT_ENUM:        return "ENUM";
    }
    return "?";
}

void
ast_function_dump(const AstFunction *af)
{
    int i;

    if(af == NULL || af->fn == NULL) {
        printf("(empty ast)\n");
        return;
    }
    printf("function %s body_count=%d\n",
           af->fn->screen[0] != '\0' ? af->fn->screen : "(screen)",
           af->stmt_count);
    for(i = 0; i < af->stmt_count; i++) {
        const AstStmt *s = &af->stmts[i];
        int j;

        for(j = 1; j < s->depth; j++)
            fputs("  ", stdout);
        printf("%-12s | %s\n", kind_name(s->kind), s->text ? s->text : "");
    }
}

int
ast_function_count_kind(const AstFunction *af, AstStmtKind kind)
{
    int n = 0;
    int i;

    if(af == NULL)
        return 0;
    for(i = 0; i < af->stmt_count; i++)
        if(af->stmts[i].kind == kind)
            n++;
    return n;
}
