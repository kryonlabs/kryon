#include "kir_token.h"

#include <ctype.h>
#include <string.h>

static int
is_ident_start(int c)
{
    return isalpha((unsigned char)c) || c == '_';
}

static int
is_ident_continue(int c)
{
    return isalnum((unsigned char)c) || c == '_';
}

static void
lexer_advance(KirLexer *lx)
{
    if(lx->src[lx->pos] == '\n') {
        lx->line++;
        lx->column = 1;
    } else {
        lx->column++;
    }
    lx->pos++;
}

static void
token_copy(KirToken *tok, const char *src, size_t begin, size_t end)
{
    size_t n = end > begin ? end - begin : 0;

    if(n >= sizeof(tok->text))
        n = sizeof(tok->text) - 1;
    memcpy(tok->text, src + begin, n);
    tok->text[n] = '\0';
}

void
KirLexerInit(KirLexer *lx, const char *src, const char *path)
{
    memset(lx, 0, sizeof(*lx));
    lx->src = src != NULL ? src : "";
    lx->path = path != NULL ? path : "";
    lx->line = 1;
    lx->column = 1;
}

KirToken
KirLexerNext(KirLexer *lx)
{
    KirToken tok;
    size_t begin;
    int start_line;
    int start_column;
    int c;

    memset(&tok, 0, sizeof(tok));
    while(isspace((unsigned char)lx->src[lx->pos]))
        lexer_advance(lx);
    begin = lx->pos;
    start_line = lx->line;
    start_column = lx->column;
    c = (unsigned char)lx->src[lx->pos];
    tok.span = KirSpan(lx->path, start_line, start_column);
    if(c == '\0') {
        tok.kind = KIR_TOKEN_EOF;
        return tok;
    }
    if(c == '#') {
        lexer_advance(lx);
        while(is_ident_continue((unsigned char)lx->src[lx->pos]))
            lexer_advance(lx);
        tok.kind = KIR_TOKEN_DIRECTIVE;
        token_copy(&tok, lx->src, begin, lx->pos);
        return tok;
    }
    if(is_ident_start(c)) {
        lexer_advance(lx);
        while(is_ident_continue((unsigned char)lx->src[lx->pos]))
            lexer_advance(lx);
        tok.kind = KIR_TOKEN_IDENT;
        token_copy(&tok, lx->src, begin, lx->pos);
        return tok;
    }
    if(isdigit(c)) {
        int seen_dot = 0;

        lexer_advance(lx);
        while(isalnum((unsigned char)lx->src[lx->pos]) ||
              lx->src[lx->pos] == '_' || lx->src[lx->pos] == '.') {
            if(lx->src[lx->pos] == '.')
                seen_dot = 1;
            lexer_advance(lx);
        }
        tok.kind = seen_dot ? KIR_TOKEN_FLOAT : KIR_TOKEN_INT;
        token_copy(&tok, lx->src, begin, lx->pos);
        return tok;
    }
    if(c == '"' || c == '\'') {
        int quote = c;

        lexer_advance(lx);
        while(lx->src[lx->pos] != '\0') {
            if(lx->src[lx->pos] == '\\' && lx->src[lx->pos + 1] != '\0') {
                lexer_advance(lx);
                lexer_advance(lx);
                continue;
            }
            if(lx->src[lx->pos] == quote) {
                lexer_advance(lx);
                break;
            }
            lexer_advance(lx);
        }
        tok.kind = quote == '"' ? KIR_TOKEN_STRING : KIR_TOKEN_CHAR;
        token_copy(&tok, lx->src, begin, lx->pos);
        return tok;
    }
    if(strchr("{}()[],:;", c) != NULL) {
        lexer_advance(lx);
        tok.kind = KIR_TOKEN_PUNCT;
        token_copy(&tok, lx->src, begin, lx->pos);
        return tok;
    }
    {
        static const char *const ops[] = {
            "::", "->", "==", "!=", "<=", ">=", "&&", "||", "+=", "-=",
            "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", "<<", ">>",
            NULL
        };

        for(int i = 0; ops[i] != NULL; i++) {
            size_t n = strlen(ops[i]);

            if(strncmp(lx->src + lx->pos, ops[i], n) == 0) {
                while(n-- > 0)
                    lexer_advance(lx);
                tok.kind = KIR_TOKEN_OPERATOR;
                token_copy(&tok, lx->src, begin, lx->pos);
                return tok;
            }
        }
    }
    if(strchr("+-*/%=!<>.&|^?~", c) != NULL) {
        lexer_advance(lx);
        tok.kind = KIR_TOKEN_OPERATOR;
        token_copy(&tok, lx->src, begin, lx->pos);
        return tok;
    }
    lexer_advance(lx);
    tok.kind = KIR_TOKEN_UNKNOWN;
    token_copy(&tok, lx->src, begin, lx->pos);
    return tok;
}

const char *
KirTokenKindName(KirTokenKind kind)
{
    switch(kind) {
    case KIR_TOKEN_EOF: return "eof";
    case KIR_TOKEN_IDENT: return "ident";
    case KIR_TOKEN_INT: return "int";
    case KIR_TOKEN_FLOAT: return "float";
    case KIR_TOKEN_STRING: return "string";
    case KIR_TOKEN_CHAR: return "char";
    case KIR_TOKEN_DIRECTIVE: return "directive";
    case KIR_TOKEN_OPERATOR: return "operator";
    case KIR_TOKEN_PUNCT: return "punct";
    default: return "unknown";
    }
}
