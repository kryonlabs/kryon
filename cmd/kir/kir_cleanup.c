#include "kir_cleanup.h"
#include "kir_text.h"
#include "kir_token.h"
#include "kir_expr.h"

#include <stdlib.h>
#include <string.h>

typedef struct Cleanup {
    KirStmt statement;
    int depth;
} Cleanup;

static int append(KirFunction *out, const KirStmt *st);

static int
fail(const KirStmt *st, const char *message)
{
    fprintf(stderr, "%s:%d:%d: %s\n", st->span.path, st->span.line,
            st->span.column, message);
    return 0;
}

static int
mentions(const char *text, const char *name)
{
    KirLexer lexer;
    KirToken token;
    KirLexerInit(&lexer, text, "");
    do {
        token = KirLexerNext(&lexer);
        if(token.kind == KIR_TOKEN_IDENT && !strcmp(token.text, name)) return 1;
    } while(token.kind != KIR_TOKEN_EOF);
    return 0;
}

static int
action_kind(const char *text, KirStmt *action)
{
    KirLexer lexer;
    KirToken token;
    KirFunction temporary = {0};
    int valid;
    action->kind = KIR_STMT_EXPR;
    KirLexerInit(&lexer, text, action->span.path);
    token = KirLexerNext(&lexer);
    if(!strcmp(token.text, "return") || !strcmp(token.text, "break") ||
       !strcmp(token.text, "continue") || !strcmp(token.text, "goto") ||
       !strcmp(token.text, "defer") || !strcmp(token.text, "if") ||
       !strcmp(token.text, "while") || !strcmp(token.text, "for")) return 0;
    KirLexerInit(&lexer, text, action->span.path);
    do {
        token = KirLexerNext(&lexer);
        if(!strcmp(token.text, "=") || !strcmp(token.text, "+=") ||
           !strcmp(token.text, "-=") || !strcmp(token.text, "*=") ||
           !strcmp(token.text, "/=") || !strcmp(token.text, "%=") ||
           !strcmp(token.text, "&=") || !strcmp(token.text, "|=") ||
           !strcmp(token.text, "^=") || !strcmp(token.text, "<<=") || !strcmp(token.text, ">>="))
            action->kind = KIR_STMT_ASSIGN;
    } while(token.kind != KIR_TOKEN_EOF);
    kir_copy(action->text, sizeof(action->text), text);
    if(!append(&temporary, action)) return 0;
    KirStructureFunction(&temporary, NULL);
    valid = temporary.stmts[0].expr_root >= 0 &&
            temporary.exprs[temporary.stmts[0].expr_root].kind != KIR_EXPR_UNKNOWN;
    free(temporary.stmts); free(temporary.exprs);
    return valid;
}

static int
append(KirFunction *out, const KirStmt *st)
{
    KirStmt *copy = KirFunctionAddStmt(out, st->kind, st->text, st->widget,
                                      st->span);
    if(!copy)
        return 0;
    *copy = *st;
    return 1;
}

static int
emit(KirFunction *out, Cleanup *entries, int count, int minimum)
{
    for(int i = count - 1; i >= 0 && entries[i].depth >= minimum; i--)
        if(!append(out, &entries[i].statement))
            return 0;
    return 1;
}

static int
opens(KirStmtKind kind)
{
    return kind == KIR_STMT_BLOCK_OPEN || kind == KIR_STMT_IF ||
           kind == KIR_STMT_WHILE || kind == KIR_STMT_FOR || kind == KIR_STMT_SWITCH;
}

static int
end_block(const KirFunction *fn, int start, int end)
{
    int depth = 1;
    for(int i = start + 1; i < end; i++) {
        if(opens(fn->stmts[i].kind)) depth++;
        if(fn->stmts[i].kind == KIR_STMT_BLOCK_CLOSE && !--depth) return i;
    }
    return end;
}

static int
falls_through(const KirFunction *fn, int start, int end)
{
    for(int i = start; i < end; i++) {
        KirStmtKind kind = fn->stmts[i].kind;
        if(kind == KIR_STMT_RETURN || kind == KIR_STMT_BREAK || kind == KIR_STMT_CONTINUE)
            return 0;
        if(opens(kind)) {
            int close = end_block(fn, i, end);
            int falls = falls_through(fn, i + 1, close);
            if(kind == KIR_STMT_BLOCK_OPEN && !falls) return 0;
            if(kind == KIR_STMT_IF) {
                int has_else = 0;
                while(close + 1 < end && fn->stmts[close + 1].kind == KIR_STMT_IF &&
                      strncmp(fn->stmts[close + 1].text, "else", 4) == 0) {
                    int next = close + 1;
                    has_else = strncmp(kir_skip_ws(fn->stmts[next].text + 4), "if", 2) != 0;
                    close = end_block(fn, next, end);
                    falls |= falls_through(fn, next + 1, close);
                }
                if(has_else && !falls) return 0;
            }
            i = close;
        }
    }
    return 1;
}

int
KirLowerCleanup(KirFunction *fn)
{
    int has_cleanup = 0, depth = 0, count = 0, serial = 0, ok = 0;
    Cleanup *entries;
    KirStmtKind *scopes;
    int *scope_start;
    KirFunction out = {0};

    for(int i = 0; i < fn->stmt_count; i++)
        has_cleanup |= fn->stmts[i].kind == KIR_STMT_DEFER;
    if(!has_cleanup)
        return 1;
    entries = calloc((size_t)fn->stmt_count + 1, sizeof(*entries));
    scopes = calloc((size_t)fn->stmt_count + 1, sizeof(*scopes));
    scope_start = calloc((size_t)fn->stmt_count + 1, sizeof(*scope_start));
    if(!entries || !scopes || !scope_start)
        goto done;
    for(int i = 0; i < fn->stmt_count; i++) {
        const KirStmt *st = &fn->stmts[i];
        if(st->kind == KIR_STMT_IF && !strncmp(st->text, "guard ", 6)) {
            fail(st, "guard with defer requires an explicit if and return");
            goto done;
        }
        if(st->kind == KIR_STMT_GOTO || st->kind == KIR_STMT_LABEL) {
            fail(st, "goto and labels in functions with defer are not supported");
            goto done;
        }
        if(st->kind == KIR_STMT_RAW) {
            fail(st, "raw C and conditional preprocessing in functions with defer are not supported");
            goto done;
        }
        if(st->kind == KIR_STMT_DECL) {
            KirLexer lexer;
            KirToken name;
            KirLexerInit(&lexer, st->text, st->span.path);
            name = KirLexerNext(&lexer);
            for(int d = 0; d < count; d++) {
                if(mentions(entries[d].statement.text, name.text)) {
                    fail(st, "a declaration cannot shadow a name referenced by an active defer");
                    goto done;
                }
            }
        }
        if(st->kind == KIR_STMT_FOR) {
            for(int d = 0; d < count; d++) {
                KirLexer lexer;
                KirToken token;
                KirLexerInit(&lexer, st->text, st->span.path);
                do {
                    token = KirLexerNext(&lexer);
                    if(token.kind == KIR_TOKEN_IDENT &&
                       mentions(entries[d].statement.text, token.text)) {
                        fail(st, "a for header cannot reuse a name referenced by an active defer; use a while loop");
                        goto done;
                    }
                } while(token.kind != KIR_TOKEN_EOF);
            }
        }
        if(st->kind == KIR_STMT_DEFER) {
            const char *body = kir_skip_ws(st->text + 5);
            KirStmt *action = &entries[count].statement;
            if(scopes[depth] == KIR_STMT_SWITCH) {
                fail(st, "defer in a switch case requires an explicit block");
                goto done;
            }
            /* Cleanup actions are expressions or assignments, never jumps or
             * declarations. Keep their source span for backend diagnostics. */
            *action = *st;
            if(!action_kind(body, action)) {
                fail(st, "defer requires one expression or assignment");
                goto done;
            }
            action->expr_root = -1;
            kir_copy(action->text, sizeof(action->text), body);
            entries[count++].depth = depth;
            continue;
        }
        if(st->kind == KIR_STMT_RETURN && count) {
            KirStmt result = *st;
            const char *value = kir_skip_ws(st->text + 6);
            if(*value && strcmp(value, ";")) {
                char name[KIR_NAME_MAX];
                int collision;
                /* Reserve a fresh local without reserving user identifiers. */
                do {
                    snprintf(name, sizeof(name), "cleanup_return_%d", serial++);
                    collision = strstr(fn->args, name) != NULL;
                    for(int k = 0; k < fn->stmt_count; k++)
                        collision |= strstr(fn->stmts[k].text, name) != NULL;
                } while(collision);
                result.kind = KIR_STMT_DECL;
                result.expr_root = st->expr_root;
                if(snprintf(result.text, sizeof(result.text), "%s: %s = %s",
                            name, fn->return_type, value) >= (int)sizeof(result.text)) {
                    fail(st, "return expression exceeds cleanup lowering limit");
                    goto done;
                }
                if(!append(&out, &result))
                    goto done;
                result.kind = KIR_STMT_RETURN;
                result.expr_root = -1;
                snprintf(result.text, sizeof(result.text), "return %s", name);
            }
            if(!emit(&out, entries, count, 0) || !append(&out, &result))
                goto done;
            continue;
        }
        if(st->kind == KIR_STMT_BREAK || st->kind == KIR_STMT_CONTINUE) {
            int target = depth;
            while(target > 0 && scopes[target] != KIR_STMT_FOR &&
                  scopes[target] != KIR_STMT_WHILE &&
                  !(st->kind == KIR_STMT_BREAK && scopes[target] == KIR_STMT_SWITCH))
                target--;
            if(!target) {
                fail(st, "loop control outside a loop or switch");
                goto done;
            }
            if(!emit(&out, entries, count, target))
                goto done;
        }
        if(st->kind == KIR_STMT_BLOCK_CLOSE) {
            if(!depth) {
                fail(st, "unbalanced cleanup scope");
                goto done;
            }
            if(falls_through(fn, scope_start[depth], i) &&
               !emit(&out, entries, count, depth))
                goto done;
            while(count && entries[count - 1].depth >= depth)
                count--;
            depth--;
        }
        if(!append(&out, st))
            goto done;
        if(opens(st->kind)) {
            scopes[++depth] = st->kind;
            scope_start[depth] = i + 1;
        }
    }
    if(falls_through(fn, 0, fn->stmt_count) && !emit(&out, entries, count, 0))
        goto done;
    free(fn->stmts);
    fn->stmts = out.stmts;
    fn->stmt_count = out.stmt_count;
    fn->stmt_cap = out.stmt_cap;
    out.stmts = NULL;
    ok = 1;
done:
    free(out.stmts);
    free(entries);
    free(scopes);
    free(scope_start);
    return ok;
}
