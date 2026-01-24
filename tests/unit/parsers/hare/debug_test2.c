#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations and token types from hare_parser.c
typedef enum {
    HARE_TOKEN_EOF,
    HARE_TOKEN_IDENTIFIER,
    HARE_TOKEN_STRING,
    HARE_TOKEN_NUMBER,
    HARE_TOKEN_LBRACE,
    HARE_TOKEN_RBRACE,
    HARE_TOKEN_LPAREN,
    HARE_TOKEN_RPAREN,
    HARE_TOKEN_LBRACKET,
    HARE_TOKEN_RBRACKET,
    HARE_TOKEN_SEMICOLON,
    HARE_TOKEN_COMMA,
    HARE_TOKEN_DOT,
    HARE_TOKEN_ARROW,
    HARE_TOKEN_EQUAL,
    HARE_TOKEN_USE,
    HARE_TOKEN_EXPORT,
    HARE_TOKEN_FN,
    HARE_TOKEN_RETURN,
    HARE_TOKEN_VOID,
    HARE_TOKEN_UNKNOWN
} HareTokenType;

const char* token_name(HareTokenType type) {
    switch (type) {
        case HARE_TOKEN_EOF: return "EOF";
        case HARE_TOKEN_IDENTIFIER: return "IDENTIFIER";
        case HARE_TOKEN_STRING: return "STRING";
        case HARE_TOKEN_NUMBER: return "NUMBER";
        case HARE_TOKEN_LBRACE: return "LBRACE";
        case HARE_TOKEN_RBRACE: return "RBRACE";
        case HARE_TOKEN_LPAREN: return "LPAREN";
        case HARE_TOKEN_RPAREN: return "RPAREN";
        case HARE_TOKEN_LBRACKET: return "LBRACKET";
        case HARE_TOKEN_RBRACKET: return "RBRACKET";
        case HARE_TOKEN_SEMICOLON: return "SEMICOLON";
        case HARE_TOKEN_COMMA: return "COMMA";
        case HARE_TOKEN_DOT: return "DOT";
        case HARE_TOKEN_ARROW: return "ARROW";
        case HARE_TOKEN_EQUAL: return "EQUAL";
        case HARE_TOKEN_USE: return "USE";
        case HARE_TOKEN_EXPORT: return "EXPORT";
        case HARE_TOKEN_FN: return "FN";
        case HARE_TOKEN_RETURN: return "RETURN";
        case HARE_TOKEN_VOID: return "VOID";
        case HARE_TOKEN_UNKNOWN: return "UNKNOWN";
        default: return "?";
    }
}

// Minimal lexer test
typedef struct {
    const char* input;
    size_t pos;
    size_t line;
    size_t col;
    size_t length;
} TestLexer;

typedef struct {
    HareTokenType type;
    char* value;
    size_t line;
    size_t col;
} TestToken;

void test_lexer_init(TestLexer* lex, const char* input) {
    lex->input = input;
    lex->pos = 0;
    lex->line = 1;
    lex->col = 1;
    lex->length = strlen(input);
}

char test_lexer_peek(TestLexer* lex) {
    if (lex->pos >= lex->length) return '\0';
    return lex->input[lex->pos];
}

char test_lexer_advance(TestLexer* lex) {
    if (lex->pos >= lex->length) return '\0';
    char ch = lex->input[lex->pos++];
    if (ch == '\n') {
        lex->line++;
        lex->col = 1;
    } else {
        lex->col++;
    }
    return ch;
}

void test_lexer_skip_whitespace(TestLexer* lex) {
    while (lex->pos < lex->length) {
        char ch = test_lexer_peek(lex);
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            test_lexer_advance(lex);
        } else if (ch == '/' && lex->pos + 1 < lex->length && lex->input[lex->pos + 1] == '/') {
            while (test_lexer_peek(lex) != '\n' && test_lexer_peek(lex) != '\0') {
                test_lexer_advance(lex);
            }
        } else {
            break;
        }
    }
}

TestToken* test_lexer_next_token(TestLexer* lex) {
    test_lexer_skip_whitespace(lex);
    if (lex->pos >= lex->length) {
        TestToken* tok = calloc(1, sizeof(TestToken));
        tok->type = HARE_TOKEN_EOF;
        return tok;
    }
    size_t start_line = lex->line;
    size_t start_col = lex->col;
    char ch = test_lexer_peek(lex);
    
    // String
    if (ch == '"') {
        test_lexer_advance(lex);
        char buf[4096] = {0};
        size_t i = 0;
        while ((ch = test_lexer_peek(lex)) != '"' && ch != '\0' && ch != '\n') {
            if (ch == '\\') test_lexer_advance(lex);
            ch = test_lexer_peek(lex);
            buf[i++] = ch;
            test_lexer_advance(lex);
        }
        test_lexer_advance(lex);
        TestToken* tok = calloc(1, sizeof(TestToken));
        tok->type = HARE_TOKEN_STRING;
        tok->value = strdup(buf);
        tok->line = start_line;
        tok->col = start_col;
        return tok;
    }
    
    // Arrow =>
    if (ch == '=' && lex->pos + 1 < lex->length && lex->input[lex->pos + 1] == '>') {
        test_lexer_advance(lex);
        test_lexer_advance(lex);
        TestToken* tok = calloc(1, sizeof(TestToken));
        tok->type = HARE_TOKEN_ARROW;
        return tok;
    }
    
    // Single char tokens
    if (ch == '{') { test_lexer_advance(lex); TestToken* t = calloc(1,sizeof(TestToken)); t->type=HARE_TOKEN_LBRACE; return t; }
    if (ch == '}') { test_lexer_advance(lex); TestToken* t = calloc(1,sizeof(TestToken)); t->type=HARE_TOKEN_RBRACE; return t; }
    if (ch == '(') { test_lexer_advance(lex); TestToken* t = calloc(1,sizeof(TestToken)); t->type=HARE_TOKEN_LPAREN; return t; }
    if (ch == ')') { test_lexer_advance(lex); TestToken* t = calloc(1,sizeof(TestToken)); t->type=HARE_TOKEN_RPAREN; return t; }
    if (ch == '*') { test_lexer_advance(lex); TestToken* t = calloc(1,sizeof(TestToken)); t->type=HARE_TOKEN_UNKNOWN; t->value=strdup("*"); return t; }
    if (ch == ';') { test_lexer_advance(lex); TestToken* t = calloc(1,sizeof(TestToken)); t->type=HARE_TOKEN_SEMICOLON; return t; }
    if (ch == '.') { test_lexer_advance(lex); TestToken* t = calloc(1,sizeof(TestToken)); t->type=HARE_TOKEN_DOT; return t; }
    if (ch == '=') { test_lexer_advance(lex); TestToken* t = calloc(1,sizeof(TestToken)); t->type=HARE_TOKEN_EQUAL; return t; }
    
    // Identifier
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_') {
        char buf[256] = {0};
        size_t i = 0;
        while ((test_lexer_peek(lex) >= 'a' && test_lexer_peek(lex) <= 'z') ||
               (test_lexer_peek(lex) >= 'A' && test_lexer_peek(lex) <= 'Z') ||
               test_lexer_peek(lex) == '_') {
            buf[i++] = test_lexer_advance(lex);
        }
        TestToken* tok = calloc(1, sizeof(TestToken));
        tok->value = strdup(buf);
        if (strcmp(buf, "use") == 0) tok->type = HARE_TOKEN_USE;
        else if (strcmp(buf, "export") == 0) tok->type = HARE_TOKEN_EXPORT;
        else if (strcmp(buf, "fn") == 0) tok->type = HARE_TOKEN_FN;
        else if (strcmp(buf, "void") == 0) tok->type = HARE_TOKEN_VOID;
        else tok->type = HARE_TOKEN_IDENTIFIER;
        tok->line = start_line;
        tok->col = start_col;
        return tok;
    }
    
    test_lexer_advance(lex);
    TestToken* tok = calloc(1, sizeof(TestToken));
    tok->type = HARE_TOKEN_UNKNOWN;
    return tok;
}

int main(void) {
    const char* source =
        "use kryon::*;\n"
        "export fn main() void = {\n"
        "    container { b => b.width(\"100%\"); }\n"
        "};\n";
    
    printf("=== Lexer Test ===\n");
    TestLexer lex;
    test_lexer_init(&lex, source);
    
    for (int i = 0; i < 30; i++) {
        TestToken* tok = test_lexer_next_token(&lex);
        printf("%2d: %-15s", i, token_name(tok->type));
        if (tok->value) printf(" '%s'", tok->value);
        printf("\n");
        if (tok->type == HARE_TOKEN_EOF) {
            free(tok->value);
            free(tok);
            break;
        }
        free(tok->value);
        free(tok);
    }
    return 0;
}
