#include "kir_expr.h"
#include "kir_token.h"
#include "kir_text.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct ExprParser {
    KirLexer lexer;
    KirToken token;
    KirFunction *fn;
    const KirModule *module;
    KirSourceSpan span;
    const char *source;
    size_t begin;
    int failed, depth;
} ExprParser;

static void
next(ExprParser *p)
{
    p->token = KirLexerNext(&p->lexer);
    p->begin = p->lexer.pos - strlen(p->token.text);
}

static int
is(ExprParser *p, const char *s)
{
    return strcmp(p->token.text, s) == 0;
}

static int
take(ExprParser *p, const char *s)
{
    if(!is(p, s)) return 0;
    next(p);
    return 1;
}

static void
expect(ExprParser *p, const char *s)
{
    if(!take(p, s)) p->failed = 1;
}

static int
node(ExprParser *p, KirExprKind kind, size_t start, const char *name,
     const char *op, int left, int right)
{
    char text[KIR_TEXT_MAX];
    size_t n = p->begin > start ? p->begin - start : 0;
    KirSourceSpan span = p->span;
    KirExpr *e;
    if(n >= sizeof(text)) { p->failed = 1; return -1; }
    memcpy(text, p->source + start, n);
    text[n] = 0;
    kir_trim_in_place(text);
    span.column += (int)start;
    e = KirFunctionAddExpr(p->fn, kind, text, span);
    if(!e) { p->failed = 1; return -1; }
    kir_copy(e->name, sizeof(e->name), name);
    kir_copy(e->op, sizeof(e->op), op);
    e->left = left;
    e->right = right;
    return p->fn->expr_count - 1;
}

static int
type_name(const ExprParser *p, const char *s)
{
    static const char *const names[] = {
        "void", "bool", "char", "int", "float", "double", "short", "long",
        "signed", "unsigned", "const", "volatile", "size_t", "ptrdiff_t",
        "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t",
        "uint32_t", "uint64_t", "intptr_t", "uintptr_t", "i8", "i16", "i32",
        "i64", "u8", "u16", "u32", "u64", "isize", "usize", "f32", "f64", NULL
    };
    for(int i = 0; names[i]; i++) if(!strcmp(s, names[i])) return 1;
    if(p->module)
        for(int i = 0; i < p->module->type_count; i++)
            if(!strcmp(s, p->module->types[i].name)) return 1;
    return 0;
}

static int expression(ExprParser *p, int minimum);

static int
prefix(ExprParser *p)
{
    size_t start = p->begin;
    KirToken tok = p->token;
    int result = -1;
    if(++p->depth > 128) { p->failed = 1; p->depth--; return -1; }
    if(take(p, "sizeof")) {
        int right;
        if(take(p, "(")) {
            if(type_name(p, p->token.text)) {
                size_t ts = p->begin;
                while(p->token.kind != KIR_TOKEN_EOF && !is(p, ")")) next(p);
                right = node(p, KIR_EXPR_IDENT, ts, "", "", -1, -1);
                expect(p, ")");
            } else {
                right = expression(p, 1);
                expect(p, ")");
            }
        } else right = prefix(p);
        result = node(p, KIR_EXPR_SIZEOF, start, "", "", -1, right);
    } else if(is(p, "+") || is(p, "-") || is(p, "!") || is(p, "~") ||
              is(p, "*") || is(p, "&") || is(p, "++") || is(p, "--")) {
        int right;
        next(p);
        right = prefix(p);
        result = node(p, KIR_EXPR_UNARY, start, "", tok.text, -1, right);
    } else if(take(p, "(")) {
        if(type_name(p, p->token.text)) {
            char type[KIR_NAME_MAX];
            size_t ts = p->begin, length;
            while(p->token.kind == KIR_TOKEN_IDENT || is(p, "*")) next(p);
            length = p->begin - ts;
            if(length >= sizeof(type)) { p->failed = 1; length = 0; }
            memcpy(type, p->source + ts, length); type[length] = 0;
            kir_trim_in_place(type);
            expect(p, ")");
            if(is(p, "{")) {
                /* Compound initializers remain explicitly opaque until their
                 * designated-field representation is available. */
                int depth = 0;
                do {
                    if(is(p, "{")) depth++;
                    if(is(p, "}")) depth--;
                    next(p);
                } while(depth && p->token.kind != KIR_TOKEN_EOF);
                if(depth) p->failed = 1;
                result = node(p, KIR_EXPR_COMPOUND, start, "", "", -1, -1);
            } else {
                int right = prefix(p);
                result = node(p, KIR_EXPR_CAST, start, type, "", -1, right);
            }
        } else {
            result = expression(p, 1);
            expect(p, ")");
            /* Unknown imported C types still preserve compound literals. */
            if(is(p, "{")) {
                int depth = 0;
                do {
                    if(is(p, "{")) depth++;
                    if(is(p, "}")) depth--;
                    next(p);
                } while(depth && p->token.kind != KIR_TOKEN_EOF);
                if(depth) p->failed = 1;
                result = node(p, KIR_EXPR_COMPOUND, start, "", "", -1, -1);
            }
        }
    } else {
        KirExprKind kind;
        switch(tok.kind) {
        case KIR_TOKEN_IDENT: kind = KIR_EXPR_IDENT; break;
        case KIR_TOKEN_INT: kind = KIR_EXPR_INT; break;
        case KIR_TOKEN_FLOAT: kind = KIR_EXPR_FLOAT; break;
        case KIR_TOKEN_STRING: kind = KIR_EXPR_STRING; break;
        case KIR_TOKEN_CHAR: kind = KIR_EXPR_CHAR; break;
        default: p->failed = 1; p->depth--; return -1;
        }
        next(p);
        result = node(p, kind, start, kind == KIR_EXPR_IDENT ? tok.text : "", "", -1, -1);
    }
    while(!p->failed) {
        if(take(p, "[")) {
            int index = expression(p, 1);
            expect(p, "]");
            result = node(p, KIR_EXPR_INDEX, start, "", "", result, index);
        } else if(is(p, ".") || is(p, "->")) {
            int pointer = is(p, "->");
            char name[KIR_NAME_MAX];
            next(p);
            if(p->token.kind != KIR_TOKEN_IDENT) p->failed = 1;
            kir_copy(name, sizeof(name), p->token.text);
            next(p);
            result = node(p, pointer ? KIR_EXPR_POINTER_MEMBER : KIR_EXPR_MEMBER,
                          start, name, pointer ? "->" : ".", result, -1);
        } else if(take(p, "(")) {
            int first = -1, last = -1;
            char name[KIR_NAME_MAX] = "";
            int callee = result;
            if(callee >= 0 && p->fn->exprs[callee].kind == KIR_EXPR_IDENT)
                kir_copy(name, sizeof(name), p->fn->exprs[callee].name);
            if(!is(p, ")")) do {
                int child = expression(p, 1);
                if(child < 0) { p->failed = 1; break; }
                if(last >= 0) p->fn->exprs[last].next_sibling = child;
                else first = child;
                last = child;
            } while(take(p, ","));
            expect(p, ")");
            result = node(p, KIR_EXPR_CALL, start, name, "", name[0] ? -1 : callee, -1);
            if(result >= 0) p->fn->exprs[result].first_child = first;
        } else if(is(p, "++") || is(p, "--")) {
            char op[8]; kir_copy(op, sizeof(op), p->token.text); next(p);
            result = node(p, KIR_EXPR_POSTFIX, start, "", op, result, -1);
        } else break;
    }
    p->depth--;
    return result;
}

static int
precedence(const char *op)
{
    if(!strcmp(op, "?")) return 1;
    if(!strcmp(op, "||")) return 2;
    if(!strcmp(op, "&&")) return 3;
    if(!strcmp(op, "|")) return 4;
    if(!strcmp(op, "^")) return 5;
    if(!strcmp(op, "&")) return 6;
    if(!strcmp(op, "==") || !strcmp(op, "!=")) return 7;
    if(!strcmp(op, "<") || !strcmp(op, "<=") || !strcmp(op, ">") || !strcmp(op, ">=")) return 8;
    if(!strcmp(op, "<<") || !strcmp(op, ">>")) return 9;
    if(!strcmp(op, "+") || !strcmp(op, "-")) return 10;
    if(!strcmp(op, "*") || !strcmp(op, "/") || !strcmp(op, "%")) return 11;
    return 0;
}

static int
expression(ExprParser *p, int minimum)
{
    size_t start = p->begin;
    int left = prefix(p), prec;
    if(++p->depth > 128) { p->failed = 1; p->depth--; return -1; }
    while(!p->failed && (prec = precedence(p->token.text)) >= minimum) {
        char op[8];
        int right;
        kir_copy(op, sizeof(op), p->token.text);
        next(p);
        right = expression(p, !strcmp(op, "?") ? 1 : prec + 1);
        if(!strcmp(op, "?")) {
            int third;
            expect(p, ":");
            third = expression(p, 1);
            left = node(p, KIR_EXPR_CONDITIONAL, start, "", "?", left, right);
            if(left >= 0) p->fn->exprs[left].third = third;
        } else left = node(p, KIR_EXPR_BINARY, start, "", op, left, right);
    }
    p->depth--;
    return left;
}

int
KirParseExpr(KirFunction *fn, const KirModule *module, const char *text, KirSourceSpan span)
{
    ExprParser p = {0};
    int initial = fn->expr_count, result;
    if(!*kir_skip_ws(text)) return -1;
    p.fn = fn; p.module = module; p.span = span; p.source = text;
    KirLexerInit(&p.lexer, text, span.path);
    next(&p);
    result = expression(&p, 1);
    take(&p, ";");
    if(p.failed || p.token.kind != KIR_TOKEN_EOF || result < 0) {
        fn->expr_count = initial;
        if(!KirFunctionAddExpr(fn, KIR_EXPR_UNKNOWN, text, span)) return -1;
        return fn->expr_count - 1;
    }
    return result;
}

void
KirStructureFunction(KirFunction *fn, const KirModule *module)
{
    free(fn->exprs); fn->exprs = NULL; fn->expr_count = fn->expr_cap = 0;
    for(int i = 0; i < fn->stmt_count; i++) {
        KirStmt *st = &fn->stmts[i];
        char text[KIR_TEXT_MAX];
        char *value = NULL;
        kir_copy(text, sizeof(text), st->text);
        st->expr_root = st->lhs_root = -1;
        if(st->kind == KIR_STMT_DECL) {
            char *colon = strchr(text, ':');
            if(colon) {
                *colon++ = 0; kir_trim_in_place(text);
                kir_copy(st->name, sizeof(st->name), text);
                value = strchr(colon, '=');
                if(value) *value++ = 0;
                kir_trim_in_place(colon);
                kir_copy(st->type, sizeof(st->type), colon);
            }
        } else if(st->kind == KIR_STMT_ASSIGN) {
            KirLexer lexer; KirToken tok;
            KirLexerInit(&lexer, text, st->span.path);
            do {
                tok = KirLexerNext(&lexer);
                if(!strcmp(tok.text, "=") || !strcmp(tok.text, "+=") ||
                   !strcmp(tok.text, "-=") || !strcmp(tok.text, "*=") ||
                   !strcmp(tok.text, "/=") || !strcmp(tok.text, "%=") ||
                   !strcmp(tok.text, "&=") || !strcmp(tok.text, "|=") ||
                   !strcmp(tok.text, "^=") || !strcmp(tok.text, "<<=") || !strcmp(tok.text, ">>=")) {
                    value = text + lexer.pos;
                    text[lexer.pos - strlen(tok.text)] = 0;
                    st->lhs_root = KirParseExpr(fn, module, text, st->span);
                    break;
                }
            } while(tok.kind != KIR_TOKEN_EOF);
        } else if(st->kind == KIR_STMT_RETURN) value = text + 6;
        else if(st->kind == KIR_STMT_UNUSED) value = text + 6;
        else if(st->kind == KIR_STMT_EXPR || st->kind == KIR_STMT_WIDGET) value = text;
        else if(st->kind == KIR_STMT_WHILE || st->kind == KIR_STMT_IF || st->kind == KIR_STMT_SWITCH) {
            kir_strip_block_brace(text);
            value = text;
            if(!strncmp(value, "else", 4)) value = (char *)kir_skip_ws(value + 4);
            while(*value && !isspace((unsigned char)*value) && *value != '(') value++;
        }
        if(value && strcmp(kir_skip_ws(value), ";"))
            st->expr_root = KirParseExpr(fn, module, value, st->span);
    }
}
