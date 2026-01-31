/**
 * Limbo Lexer Implementation
 *
 * Basic lexical analyzer for Limbo source code
 */

#define _POSIX_C_SOURCE 200809L

#include "limbo_lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/**
 * Reserved keywords
 */
static const struct {
    const char* name;
    LimboTokenType type;
} keywords[] = {
    {"module", TOKEN_MODULE},
    {"implement", TOKEN_IMPLEMENT},
    {"import", TOKEN_IMPORT},
    {"con", TOKEN_CON},
    {"fn", TOKEN_FN},
    {"type", TOKEN_TYPE},
    {"adt", TOKEN_ADT},
    {"pick", TOKEN_PICK},
    {"within", TOKEN_WITHIN},
    {"if", TOKEN_IF},
    {"else", TOKEN_ELSE},
    {"while", TOKEN_WHILE},
    {"do", TOKEN_DO},
    {"for", TOKEN_FOR},
    {"case", TOKEN_CASE},
    {"of", TOKEN_OF},
    {"alt", TOKEN_ALT},
    {"tag", TOKEN_TAG},
    {"const", TOKEN_CONST},
    {"var", TOKEN_VAR},
    {"int", TOKEN_INT},
    {"real", TOKEN_REAL},
    {"float", TOKEN_FLOAT},
    {"string", TOKEN_STRING_TYPE},
    {"byte", TOKEN_BYTE},
    {"list", TOKEN_LIST},
    {"array", TOKEN_ARRAY},
    {"chan", TOKEN_CHAN},
    {"ref", TOKEN_REF},
    {"self", TOKEN_IDENTIFIER},  // Special identifier
    {NULL, TOKEN_IDENTIFIER}
};

/**
 * Create lexer
 */
LimboLexer* limbo_lex_create(const char* source, size_t source_len) {
    if (!source) return NULL;

    LimboLexer* lexer = calloc(1, sizeof(LimboLexer));
    if (!lexer) return NULL;

    lexer->source = source;
    lexer->source_len = source_len > 0 ? source_len : strlen(source);
    lexer->position = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->has_peek = false;

    return lexer;
}

/**
 * Free lexer
 */
void limbo_lex_free(LimboLexer* lexer) {
    if (lexer) {
        free(lexer);
    }
}

/**
 * Peek at current character
 */
static char peek_char(LimboLexer* lexer) {
    if (lexer->position >= lexer->source_len) {
        return '\0';
    }
    return lexer->source[lexer->position];
}

/**
 * Get current character and advance
 */
static char next_char(LimboLexer* lexer) {
    if (lexer->position >= lexer->source_len) {
        return '\0';
    }

    char c = lexer->source[lexer->position];
    lexer->position++;

    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }

    return c;
}

/**
 * Skip whitespace
 */
static void skip_whitespace(LimboLexer* lexer) {
    while (lexer->position < lexer->source_len) {
        char c = peek_char(lexer);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            next_char(lexer);
        } else if (c == '#') {
            // Skip line comment
            while (peek_char(lexer) != '\0' && peek_char(lexer) != '\n') {
                next_char(lexer);
            }
        } else {
            break;
        }
    }
}

/**
 * Read identifier or keyword
 */
static LimboToken read_identifier(LimboLexer* lexer, char first_char) {
    LimboToken token = {0};
    token.line = lexer->line;
    token.column = lexer->column;
    token.type = TOKEN_IDENTIFIER;
    token.text = lexer->source + lexer->position - 1;

    // Build identifier
    while (lexer->position < lexer->source_len) {
        char c = peek_char(lexer);
        if (isalnum((unsigned char)c) || c == '_') {
            next_char(lexer);
        } else {
            break;
        }
    }

    token.length = (lexer->source + lexer->position) - token.text;

    // Check if it's a keyword
    for (int i = 0; keywords[i].name != NULL; i++) {
        if (strlen(keywords[i].name) == token.length &&
            strncmp(token.text, keywords[i].name, token.length) == 0) {
            token.type = keywords[i].type;
            break;
        }
    }

    return token;
}

/**
 * Read number literal
 */
static LimboToken read_number(LimboLexer* lexer, char first_char) {
    LimboToken token = {0};
    token.line = lexer->line;
    token.column = lexer->column;
    token.text = lexer->source + lexer->position - 1;
    token.type = TOKEN_INTEGER;

    bool is_real = false;

    while (lexer->position < lexer->source_len) {
        char c = peek_char(lexer);
        if (isdigit((unsigned char)c)) {
            next_char(lexer);
        } else if (c == '.' && !is_real) {
            is_real = true;
            token.type = TOKEN_REAL;
            next_char(lexer);
        } else if ((c == 'e' || c == 'E') && !is_real) {
            is_real = true;
            token.type = TOKEN_REAL;
            next_char(lexer);
            // Optional sign
            if (peek_char(lexer) == '+' || peek_char(lexer) == '-') {
                next_char(lexer);
            }
        } else {
            break;
        }
    }

    token.length = (lexer->source + lexer->position) - token.text;
    return token;
}

/**
 * Read string literal
 */
static LimboToken read_string(LimboLexer* lexer) {
    LimboToken token = {0};
    token.line = lexer->line;
    token.column = lexer->column;
    token.text = lexer->source + lexer->position;
    token.type = TOKEN_STRING;

    next_char(lexer);  // Skip opening quote

    while (lexer->position < lexer->source_len) {
        char c = next_char(lexer);

        if (c == '\\') {
            // Escape sequence
            if (lexer->position < lexer->source_len) {
                next_char(lexer);
            }
        } else if (c == '"') {
            break;
        }
    }

    token.length = (lexer->source + lexer->position) - token.text;
    return token;
}

/**
 * Read character literal
 */
static LimboToken read_char(LimboLexer* lexer) {
    LimboToken token = {0};
    token.line = lexer->line;
    token.column = lexer->column;
    token.text = lexer->source + lexer->position;
    token.type = TOKEN_CHAR;

    next_char(lexer);  // Skip opening quote

    while (lexer->position < lexer->source_len) {
        char c = next_char(lexer);

        if (c == '\\') {
            // Escape sequence
            if (lexer->position < lexer->source_len) {
                next_char(lexer);
            }
        } else if (c == '\'') {
            break;
        }
    }

    token.length = (lexer->source + lexer->position) - token.text;
    return token;
}

/**
 * Get next token
 */
LimboToken limbo_next_token(LimboLexer* lexer) {
    skip_whitespace(lexer);

    if (lexer->position >= lexer->source_len) {
        LimboToken eof = {0};
        eof.type = TOKEN_EOF;
        eof.line = lexer->line;
        eof.column = lexer->column;
        return eof;
    }

    char c = next_char(lexer);

    // Identifiers and keywords
    if (isalpha((unsigned char)c) || c == '_') {
        return read_identifier(lexer, c);
    }

    // Numbers
    if (isdigit((unsigned char)c)) {
        return read_number(lexer, c);
    }

    // String literal
    if (c == '"') {
        return read_string(lexer);
    }

    // Character literal
    if (c == '\'') {
        return read_char(lexer);
    }

    // Operators and delimiters
    LimboToken token = {0};
    token.line = lexer->line;
    token.column = lexer->column;
    token.text = lexer->source + lexer->position - 1;
    token.length = 1;

    switch (c) {
        case '+':
            if (peek_char(lexer) == '+') {
                next_char(lexer);
                token.type = TOKEN_INCREMENT;
                token.length = 2;
            } else if (peek_char(lexer) == '=') {
                next_char(lexer);
                token.type = TOKEN_PLUS_ASSIGN;
                token.length = 2;
            } else {
                token.type = TOKEN_PLUS;
            }
            break;

        case '-':
            if (peek_char(lexer) == '-') {
                next_char(lexer);
                token.type = TOKEN_DECREMENT;
                token.length = 2;
            } else if (peek_char(lexer) == '=') {
                next_char(lexer);
                token.type = TOKEN_MINUS_ASSIGN;
                token.length = 2;
            } else if (peek_char(lexer) == '>') {
                next_char(lexer);
                token.type = TOKEN_ARROW;
                token.length = 2;
            } else {
                token.type = TOKEN_MINUS;
            }
            break;

        case '*':
            if (peek_char(lexer) == '=') {
                next_char(lexer);
                token.type = TOKEN_MUL_ASSIGN;
                token.length = 2;
            } else {
                token.type = TOKEN_MULTIPLY;
            }
            break;

        case '/':
            if (peek_char(lexer) == '=') {
                next_char(lexer);
                token.type = TOKEN_DIV_ASSIGN;
                token.length = 2;
            } else {
                token.type = TOKEN_DIVIDE;
            }
            break;

        case '%':
            token.type = TOKEN_MOD;
            break;

        case '&':
            if (peek_char(lexer) == '=') {
                next_char(lexer);
                token.type = TOKEN_AND_ASSIGN;
                token.length = 2;
            } else {
                token.type = TOKEN_AND;
            }
            break;

        case '|':
            if (peek_char(lexer) == '=') {
                next_char(lexer);
                token.type = TOKEN_OR_ASSIGN;
                token.length = 2;
            } else if (peek_char(lexer) == '|') {
                next_char(lexer);
                token.type = TOKEN_PIPE;
                token.length = 2;
            } else {
                token.type = TOKEN_OR;
            }
            break;

        case '^':
            token.type = TOKEN_NOT;
            break;

        case '<':
            if (peek_char(lexer) == '<') {
                next_char(lexer);
                if (peek_char(lexer) == '=') {
                    next_char(lexer);
                    token.type = TOKEN_LSHIFT_ASSIGN;
                    token.length = 3;
                } else {
                    token.type = TOKEN_LSHIFT;
                    token.length = 2;
                }
            } else if (peek_char(lexer) == '=') {
                next_char(lexer);
                token.type = TOKEN_LESS_EQUAL;
                token.length = 2;
            } else {
                token.type = TOKEN_LESS;
            }
            break;

        case '>':
            if (peek_char(lexer) == '>') {
                next_char(lexer);
                if (peek_char(lexer) == '=') {
                    next_char(lexer);
                    token.type = TOKEN_RSHIFT_ASSIGN;
                    token.length = 3;
                } else {
                    token.type = TOKEN_RSHIFT;
                    token.length = 2;
                }
            } else if (peek_char(lexer) == '=') {
                next_char(lexer);
                token.type = TOKEN_GREATER_EQUAL;
                token.length = 2;
            } else {
                token.type = TOKEN_GREATER;
            }
            break;

        case '=':
            if (peek_char(lexer) == '=') {
                next_char(lexer);
                token.type = TOKEN_EQUAL;
                token.length = 2;
            } else {
                token.type = TOKEN_ASSIGN;
            }
            break;

        case '!':
            if (peek_char(lexer) == '=') {
                next_char(lexer);
                token.type = TOKEN_NOT_EQUAL;
                token.length = 2;
            } else {
                token.type = TOKEN_NOT;
            }
            break;

        case '(':
            token.type = TOKEN_LPAREN;
            break;

        case ')':
            token.type = TOKEN_RPAREN;
            break;

        case '[':
            token.type = TOKEN_LBRACKET;
            break;

        case ']':
            token.type = TOKEN_RBRACKET;
            break;

        case '{':
            token.type = TOKEN_LBRACE;
            break;

        case '}':
            token.type = TOKEN_RBRACE;
            break;

        case ',':
            token.type = TOKEN_COMMA;
            break;

        case ';':
            token.type = TOKEN_SEMICOLON;
            break;

        case ':':
            if (peek_char(lexer) == ':') {
                next_char(lexer);
                token.type = TOKEN_DOUBLE_COLON;
                token.length = 2;
            } else {
                token.type = TOKEN_COLON;
            }
            break;

        case '.':
            if (peek_char(lexer) == '.') {
                next_char(lexer);
                token.type = TOKEN_DOT_DOT;
                token.length = 2;
            } else {
                token.type = TOKEN_DOT;
            }
            break;

        default:
            token.type = TOKEN_INVALID;
            break;
    }

    return token;
}

/**
 * Peek at next token
 */
LimboToken limbo_peek_token(LimboLexer* lexer) {
    if (!lexer->has_peek) {
        lexer->peek_token = limbo_next_token(lexer);
        lexer->has_peek = true;
    }
    return lexer->peek_token;
}

/**
 * Get token name as string
 */
const char* limbo_token_name(LimboTokenType type) {
    switch (type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_INTEGER: return "INTEGER";
        case TOKEN_REAL: return "REAL";
        case TOKEN_CHAR: return "CHAR";
        case TOKEN_MODULE: return "MODULE";
        case TOKEN_IMPLEMENT: return "IMPLEMENT";
        case TOKEN_IMPORT: return "IMPORT";
        case TOKEN_CON: return "CON";
        case TOKEN_FN: return "FN";
        case TOKEN_TYPE: return "TYPE";
        case TOKEN_ADT: return "ADT";
        case TOKEN_PICK: return "PICK";
        case TOKEN_ADT_TAG: return "ADT_TAG";
        case TOKEN_WITHIN: return "WITHIN";
        case TOKEN_IF: return "IF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_DO: return "DO";
        case TOKEN_FOR: return "FOR";
        case TOKEN_CASE: return "CASE";
        case TOKEN_OF: return "OF";
        case TOKEN_ALT: return "ALT";
        case TOKEN_TAG: return "TAG";
        case TOKEN_CONST: return "CONST";
        case TOKEN_VAR: return "VAR";
        case TOKEN_INT: return "INT";
        case TOKEN_FLOAT: return "FLOAT";
        case TOKEN_STRING_TYPE: return "STRING_TYPE";
        case TOKEN_BYTE: return "BYTE";
        case TOKEN_LIST: return "LIST";
        case TOKEN_ARRAY: return "ARRAY";
        case TOKEN_CHAN: return "CHAN";
        case TOKEN_REF: return "REF";
        case TOKEN_MODULE_TYPE: return "MODULE_TYPE";
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_MULTIPLY: return "MULTIPLY";
        case TOKEN_DIVIDE: return "DIVIDE";
        case TOKEN_MOD: return "MOD";
        case TOKEN_AND: return "AND";
        case TOKEN_OR: return "OR";
        case TOKEN_NOT: return "NOT";
        case TOKEN_LSHIFT: return "LSHIFT";
        case TOKEN_RSHIFT: return "RSHIFT";
        case TOKEN_EQUAL: return "EQUAL";
        case TOKEN_NOT_EQUAL: return "NOT_EQUAL";
        case TOKEN_LESS: return "LESS";
        case TOKEN_LESS_EQUAL: return "LESS_EQUAL";
        case TOKEN_GREATER: return "GREATER";
        case TOKEN_GREATER_EQUAL: return "GREATER_EQUAL";
        case TOKEN_ASSIGN: return "ASSIGN";
        case TOKEN_PLUS_ASSIGN: return "PLUS_ASSIGN";
        case TOKEN_MINUS_ASSIGN: return "MINUS_ASSIGN";
        case TOKEN_MUL_ASSIGN: return "MUL_ASSIGN";
        case TOKEN_DIV_ASSIGN: return "DIV_ASSIGN";
        case TOKEN_AND_ASSIGN: return "AND_ASSIGN";
        case TOKEN_OR_ASSIGN: return "OR_ASSIGN";
        case TOKEN_LSHIFT_ASSIGN: return "LSHIFT_ASSIGN";
        case TOKEN_RSHIFT_ASSIGN: return "RSHIFT_ASSIGN";
        case TOKEN_INCREMENT: return "INCREMENT";
        case TOKEN_DECREMENT: return "DECREMENT";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_LBRACKET: return "LBRACKET";
        case TOKEN_RBRACKET: return "RBRACKET";
        case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_COLON: return "COLON";
        case TOKEN_DOT: return "DOT";
        case TOKEN_DOT_DOT: return "DOT_DOT";
        case TOKEN_PIPE: return "PIPE";
        case TOKEN_ARROW: return "ARROW";
        case TOKEN_DOUBLE_COLON: return "DOUBLE_COLON";
        default: return "INVALID";
    }
}
