/**
 * @file lexer.c
 * @brief Kryon Lexer Implementation
 */

#include "internal/lexer.h"
#include "internal/memory.h"
#include "internal/error.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

// =============================================================================
// STATIC TABLES
// =============================================================================

static const char *token_type_names[] = {
    [KRYON_TOKEN_EOF] = "EOF",
    [KRYON_TOKEN_STRING] = "STRING",
    [KRYON_TOKEN_INTEGER] = "INTEGER", 
    [KRYON_TOKEN_FLOAT] = "FLOAT",
    [KRYON_TOKEN_BOOLEAN_TRUE] = "TRUE",
    [KRYON_TOKEN_BOOLEAN_FALSE] = "FALSE",
    [KRYON_TOKEN_NULL] = "NULL",
    [KRYON_TOKEN_IDENTIFIER] = "IDENTIFIER",
    [KRYON_TOKEN_VARIABLE] = "VARIABLE",
    [KRYON_TOKEN_ELEMENT_TYPE] = "ELEMENT_TYPE",
    [KRYON_TOKEN_COLON] = "COLON",
    [KRYON_TOKEN_SEMICOLON] = "SEMICOLON",
    [KRYON_TOKEN_DOT] = "DOT",
    [KRYON_TOKEN_COMMA] = "COMMA",
    [KRYON_TOKEN_EQUALS] = "EQUALS",
    [KRYON_TOKEN_NOT_EQUALS] = "NOT_EQUALS",
    [KRYON_TOKEN_LESS_THAN] = "LESS_THAN",
    [KRYON_TOKEN_LESS_EQUAL] = "LESS_EQUAL",
    [KRYON_TOKEN_GREATER_THAN] = "GREATER_THAN",
    [KRYON_TOKEN_GREATER_EQUAL] = "GREATER_EQUAL",
    [KRYON_TOKEN_LOGICAL_AND] = "LOGICAL_AND",
    [KRYON_TOKEN_LOGICAL_OR] = "LOGICAL_OR",
    [KRYON_TOKEN_LOGICAL_NOT] = "LOGICAL_NOT",
    [KRYON_TOKEN_PLUS] = "PLUS",
    [KRYON_TOKEN_MINUS] = "MINUS",
    [KRYON_TOKEN_MULTIPLY] = "MULTIPLY",
    [KRYON_TOKEN_DIVIDE] = "DIVIDE",
    [KRYON_TOKEN_MODULO] = "MODULO",
    [KRYON_TOKEN_QUESTION] = "QUESTION",
    [KRYON_TOKEN_AMPERSAND] = "AMPERSAND",
    [KRYON_TOKEN_LEFT_BRACE] = "LEFT_BRACE",
    [KRYON_TOKEN_RIGHT_BRACE] = "RIGHT_BRACE",
    [KRYON_TOKEN_LEFT_BRACKET] = "LEFT_BRACKET",
    [KRYON_TOKEN_RIGHT_BRACKET] = "RIGHT_BRACKET",
    [KRYON_TOKEN_LEFT_PAREN] = "LEFT_PAREN",
    [KRYON_TOKEN_RIGHT_PAREN] = "RIGHT_PAREN",
    [KRYON_TOKEN_AT] = "AT",
    [KRYON_TOKEN_HASH] = "HASH",
    [KRYON_TOKEN_DOLLAR] = "DOLLAR",
    [KRYON_TOKEN_STYLE_KEYWORD] = "STYLE_KEYWORD",
    [KRYON_TOKEN_EXTENDS_KEYWORD] = "EXTENDS_KEYWORD",
    [KRYON_TOKEN_STYLE_DIRECTIVE] = "STYLE_DIRECTIVE",
    [KRYON_TOKEN_STYLES_DIRECTIVE] = "STYLES_DIRECTIVE",
    [KRYON_TOKEN_THEME_DIRECTIVE] = "THEME_DIRECTIVE",
    [KRYON_TOKEN_VARIABLE_DIRECTIVE] = "VARIABLE_DIRECTIVE",
    [KRYON_TOKEN_VARIABLES_DIRECTIVE] = "VARIABLES_DIRECTIVE",
    [KRYON_TOKEN_FUNCTION_DIRECTIVE] = "FUNCTION_DIRECTIVE",
    [KRYON_TOKEN_STORE_DIRECTIVE] = "STORE_DIRECTIVE",
    [KRYON_TOKEN_WATCH_DIRECTIVE] = "WATCH_DIRECTIVE",
    [KRYON_TOKEN_ON_MOUNT_DIRECTIVE] = "ON_MOUNT_DIRECTIVE",
    [KRYON_TOKEN_ON_UNMOUNT_DIRECTIVE] = "ON_UNMOUNT_DIRECTIVE",
    [KRYON_TOKEN_IMPORT_DIRECTIVE] = "IMPORT_DIRECTIVE",
    [KRYON_TOKEN_EXPORT_DIRECTIVE] = "EXPORT_DIRECTIVE",
    [KRYON_TOKEN_INCLUDE_DIRECTIVE] = "INCLUDE_DIRECTIVE",
    [KRYON_TOKEN_METADATA_DIRECTIVE] = "METADATA_DIRECTIVE",
    [KRYON_TOKEN_EVENT_DIRECTIVE] = "EVENT_DIRECTIVE",
    [KRYON_TOKEN_COMPONENT_DIRECTIVE] = "COMPONENT_DIRECTIVE",
    [KRYON_TOKEN_PROPS_DIRECTIVE] = "PROPS_DIRECTIVE",
    [KRYON_TOKEN_SLOTS_DIRECTIVE] = "SLOTS_DIRECTIVE",
    [KRYON_TOKEN_LIFECYCLE_DIRECTIVE] = "LIFECYCLE_DIRECTIVE",
    [KRYON_TOKEN_STATE_DIRECTIVE] = "STATE_DIRECTIVE",
    [KRYON_TOKEN_CONST_DIRECTIVE] = "CONST_DIRECTIVE",
    [KRYON_TOKEN_CONST_FOR_DIRECTIVE] = "CONST_FOR_DIRECTIVE",
    [KRYON_TOKEN_IN_KEYWORD] = "IN_KEYWORD",
    [KRYON_TOKEN_TEMPLATE_START] = "TEMPLATE_START",
    [KRYON_TOKEN_TEMPLATE_END] = "TEMPLATE_END",
    [KRYON_TOKEN_UNIT_PX] = "UNIT_PX",
    [KRYON_TOKEN_UNIT_PERCENT] = "UNIT_PERCENT",
    [KRYON_TOKEN_UNIT_EM] = "UNIT_EM",
    [KRYON_TOKEN_UNIT_REM] = "UNIT_REM",
    [KRYON_TOKEN_UNIT_VW] = "UNIT_VW",
    [KRYON_TOKEN_UNIT_VH] = "UNIT_VH",
    [KRYON_TOKEN_UNIT_PT] = "UNIT_PT",
    [KRYON_TOKEN_SCRIPT_CONTENT] = "SCRIPT_CONTENT",
    [KRYON_TOKEN_LINE_COMMENT] = "LINE_COMMENT",
    [KRYON_TOKEN_BLOCK_COMMENT] = "BLOCK_COMMENT",
    [KRYON_TOKEN_WHITESPACE] = "WHITESPACE",
    [KRYON_TOKEN_NEWLINE] = "NEWLINE",
    [KRYON_TOKEN_ERROR] = "ERROR"
};

// Keywords table
typedef struct {
    const char *keyword;
    KryonTokenType type;
} KeywordEntry;

static const KeywordEntry keywords[] = {
    {"true", KRYON_TOKEN_BOOLEAN_TRUE},
    {"false", KRYON_TOKEN_BOOLEAN_FALSE},
    {"null", KRYON_TOKEN_NULL},
    {"style", KRYON_TOKEN_STYLE_KEYWORD},
    {"extends", KRYON_TOKEN_EXTENDS_KEYWORD},
    {"in", KRYON_TOKEN_IN_KEYWORD},
    {"px", KRYON_TOKEN_UNIT_PX},
    {"%", KRYON_TOKEN_UNIT_PERCENT},
    {"em", KRYON_TOKEN_UNIT_EM},
    {"rem", KRYON_TOKEN_UNIT_REM},
    {"vw", KRYON_TOKEN_UNIT_VW},
    {"vh", KRYON_TOKEN_UNIT_VH},
    {"pt", KRYON_TOKEN_UNIT_PT},
};

static const size_t keywords_count = sizeof(keywords) / sizeof(keywords[0]);

// Directive keywords (without @)
static const KeywordEntry directives[] = {
    {"style", KRYON_TOKEN_STYLE_DIRECTIVE},
    {"styles", KRYON_TOKEN_STYLES_DIRECTIVE},
    {"theme", KRYON_TOKEN_THEME_DIRECTIVE},
    {"var", KRYON_TOKEN_VARIABLE_DIRECTIVE},
    {"variables", KRYON_TOKEN_VARIABLES_DIRECTIVE},
    {"function", KRYON_TOKEN_FUNCTION_DIRECTIVE},
    {"store", KRYON_TOKEN_STORE_DIRECTIVE},
    {"watch", KRYON_TOKEN_WATCH_DIRECTIVE},
    {"on_mount", KRYON_TOKEN_ON_MOUNT_DIRECTIVE},
    {"on_unmount", KRYON_TOKEN_ON_UNMOUNT_DIRECTIVE},
    {"import", KRYON_TOKEN_IMPORT_DIRECTIVE},
    {"export", KRYON_TOKEN_EXPORT_DIRECTIVE},
    {"include", KRYON_TOKEN_INCLUDE_DIRECTIVE},
    {"metadata", KRYON_TOKEN_METADATA_DIRECTIVE},
    {"event", KRYON_TOKEN_EVENT_DIRECTIVE},
    {"component", KRYON_TOKEN_COMPONENT_DIRECTIVE},
    {"props", KRYON_TOKEN_PROPS_DIRECTIVE},
    {"slots", KRYON_TOKEN_SLOTS_DIRECTIVE},
    {"lifecycle", KRYON_TOKEN_LIFECYCLE_DIRECTIVE},
    {"state", KRYON_TOKEN_STATE_DIRECTIVE},
    {"const", KRYON_TOKEN_CONST_DIRECTIVE},
    {"const_for", KRYON_TOKEN_CONST_FOR_DIRECTIVE},
};

static const size_t directives_count = sizeof(directives) / sizeof(directives[0]);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

static bool is_at_end(const KryonLexer *lexer) {
    return lexer->current >= (lexer->source + lexer->source_length) || 
           *lexer->current == '\0';
}

static char advance(KryonLexer *lexer) {
    if (is_at_end(lexer)) return '\0';
    
    char c = *lexer->current;
    lexer->current++;
    lexer->offset++;
    
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    
    return c;
}

static char peek(const KryonLexer *lexer) {
    if (is_at_end(lexer)) return '\0';
    return *lexer->current;
}

static char peek_next(const KryonLexer *lexer) {
    if (lexer->current + 1 >= lexer->source + lexer->source_length) return '\0';
    return lexer->current[1];
}

static bool match(KryonLexer *lexer, char expected) {
    if (is_at_end(lexer)) return false;
    if (*lexer->current != expected) return false;
    advance(lexer);
    return true;
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

static bool is_hex_digit(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// =============================================================================
// TOKEN MANAGEMENT
// =============================================================================

static bool add_token_capacity(KryonLexer *lexer) {
    if (lexer->token_count >= lexer->token_capacity) {
        size_t new_capacity = lexer->token_capacity == 0 ? 64 : lexer->token_capacity * 2;
        KryonToken *new_tokens = kryon_realloc(lexer->tokens, new_capacity * sizeof(KryonToken));
        if (!new_tokens) {
            strcpy(lexer->error_message, "Failed to allocate token memory");
            lexer->has_error = true;
            return false;
        }
        lexer->tokens = new_tokens;
        lexer->token_capacity = new_capacity;
    }
    return true;
}

static KryonToken *add_token(KryonLexer *lexer, KryonTokenType type) {
    if (!add_token_capacity(lexer)) return NULL;
    
    KryonToken *token = &lexer->tokens[lexer->token_count++];
    memset(token, 0, sizeof(KryonToken));
    
    token->type = type;
    token->lexeme = lexer->start;
    token->lexeme_length = (size_t)(lexer->current - lexer->start);
    token->has_value = false;
    
    // Set location
    token->location.filename = lexer->filename;
    token->location.line = lexer->line;
    token->location.column = lexer->column - (uint32_t)token->lexeme_length;
    token->location.offset = (uint32_t)(lexer->start - lexer->source);
    token->location.length = (uint32_t)token->lexeme_length;
    
    
    return token;
}

static void set_error(KryonLexer *lexer, const char *message) {
    lexer->has_error = true;
    strncpy(lexer->error_message, message, sizeof(lexer->error_message) - 1);
    lexer->error_message[sizeof(lexer->error_message) - 1] = '\0';
    
    lexer->error_location.filename = lexer->filename;
    lexer->error_location.line = lexer->line;
    lexer->error_location.column = lexer->column;
    lexer->error_location.offset = lexer->offset;
    lexer->error_location.length = 1;
}

// =============================================================================
// LEXER CREATION AND DESTRUCTION
// =============================================================================

KryonLexer *kryon_lexer_create(const char *source, size_t source_length,
                              const char *filename, const KryonLexerConfig *config) {
    if (!source) {
        KRYON_LOG_ERROR("Source cannot be NULL");
        return NULL;
    }
    
    if (source_length == 0) {
        source_length = strlen(source);
    }
    
    KryonLexer *lexer = kryon_alloc_type(KryonLexer);
    if (!lexer) {
        KRYON_LOG_ERROR("Failed to allocate lexer");
        return NULL;
    }
    
    memset(lexer, 0, sizeof(KryonLexer));
    
    // Set source
    lexer->source = source;
    lexer->source_length = source_length;
    lexer->filename = filename;
    
    // Initialize position
    lexer->current = source;
    lexer->start = source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->offset = 0;
    
    // Set configuration
    if (config) {
        lexer->config = *config;
    } else {
        lexer->config = kryon_lexer_default_config();
    }
    
    // Initialize token array
    lexer->tokens = NULL;
    lexer->token_count = 0;
    lexer->token_capacity = 0;
    lexer->current_token = 0;
    
    return lexer;
}

void kryon_lexer_destroy(KryonLexer *lexer) {
    if (!lexer) return;
    
    // Free token values
    if (lexer->tokens) {
        for (size_t i = 0; i < lexer->token_count; i++) {
            KryonToken *token = &lexer->tokens[i];
            if (token->has_value && (token->type == KRYON_TOKEN_STRING || token->type == KRYON_TOKEN_SCRIPT_CONTENT) && token->value.string_value) {
                kryon_free(token->value.string_value);
            }
        }
        kryon_free(lexer->tokens);
    }
    
    kryon_free(lexer);
}

// =============================================================================
// KEYWORD CLASSIFICATION
// =============================================================================

KryonTokenType kryon_lexer_classify_keyword(const char *str, size_t length) {
    for (size_t i = 0; i < keywords_count; i++) {
        if (strlen(keywords[i].keyword) == length &&
            memcmp(keywords[i].keyword, str, length) == 0) {
            return keywords[i].type;
        }
    }
    return KRYON_TOKEN_IDENTIFIER;
}

KryonTokenType kryon_lexer_classify_directive(const char *str, size_t length) {
    for (size_t i = 0; i < directives_count; i++) {
        if (strlen(directives[i].keyword) == length &&
            memcmp(directives[i].keyword, str, length) == 0) {
            return directives[i].type;
        }
    }
    return KRYON_TOKEN_IDENTIFIER;
}

// =============================================================================
// STRING LEXING
// =============================================================================

static bool lex_string(KryonLexer *lexer) {
    char quote_char = advance(lexer); // consume opening quote
    
    size_t value_capacity = 64;
    char *value = kryon_alloc(value_capacity);
    if (!value) {
        set_error(lexer, "Failed to allocate string memory");
        return false;
    }
    
    size_t value_length = 0;
    
    while (!is_at_end(lexer) && peek(lexer) != quote_char) {
        char c = peek(lexer);
        
        // Handle escape sequences
        if (c == '\\') {
            advance(lexer); // consume backslash
            if (is_at_end(lexer)) {
                kryon_free(value);
                set_error(lexer, "Unterminated string escape");
                return false;
            }
            
            char escaped = advance(lexer);
            switch (escaped) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case '\\': c = '\\'; break;
                case '"': c = '"'; break;
                case '\'': c = '\''; break;
                case '0': c = '\0'; break;
                default:
                    // Unknown escape, keep as-is
                    if (value_length + 1 >= value_capacity) {
                        value_capacity *= 2;
                        char *new_value = kryon_realloc(value, value_capacity);
                        if (!new_value) {
                            kryon_free(value);
                            set_error(lexer, "Failed to reallocate string memory");
                            return false;
                        }
                        value = new_value;
                    }
                    value[value_length++] = '\\';
                    c = escaped;
                    break;
            }
        } else {
            advance(lexer); // consume regular character
        }
        
        // Add character to value
        if (value_length >= value_capacity) {
            value_capacity *= 2;
            char *new_value = kryon_realloc(value, value_capacity);
            if (!new_value) {
                kryon_free(value);
                set_error(lexer, "Failed to reallocate string memory");
                return false;
            }
            value = new_value;
        }
        value[value_length++] = c;
    }
    
    if (is_at_end(lexer)) {
        kryon_free(value);
        set_error(lexer, "Unterminated string");
        return false;
    }
    
    advance(lexer); // consume closing quote
    
    // Null-terminate value
    if (value_length >= value_capacity) {
        char *new_value = kryon_realloc(value, value_length + 1);
        if (!new_value) {
            kryon_free(value);
            set_error(lexer, "Failed to reallocate string memory");
            return false;
        }
        value = new_value;
    }
    value[value_length] = '\0';
    
    KryonToken *token = add_token(lexer, KRYON_TOKEN_STRING);
    if (token) {
        token->has_value = true;
        token->value.string_value = value;
    } else {
        kryon_free(value);
        return false;
    }
    
    return true;
}

// =============================================================================
// NUMBER LEXING
// =============================================================================

static bool lex_number(KryonLexer *lexer) {
    // Consume integer part
    while (is_digit(peek(lexer))) {
        advance(lexer);
    }
    
    bool is_float = false;
    
    // Check for decimal point
    if (peek(lexer) == '.' && is_digit(peek_next(lexer))) {
        is_float = true;
        advance(lexer); // consume '.'
        
        while (is_digit(peek(lexer))) {
            advance(lexer);
        }
    }
    
    // Check for scientific notation
    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        is_float = true;
        advance(lexer); // consume 'e'/'E'
        
        if (peek(lexer) == '+' || peek(lexer) == '-') {
            advance(lexer); // consume sign
        }
        
        if (!is_digit(peek(lexer))) {
            set_error(lexer, "Invalid number format");
            return false;
        }
        
        while (is_digit(peek(lexer))) {
            advance(lexer);
        }
    }
    
    // Convert to number
    size_t length = (size_t)(lexer->current - lexer->start);
    char *number_str = kryon_alloc(length + 1);
    if (!number_str) {
        set_error(lexer, "Failed to allocate number memory");
        return false;
    }
    
    memcpy(number_str, lexer->start, length);
    number_str[length] = '\0';
    
    KryonToken *token;
    if (is_float) {
        token = add_token(lexer, KRYON_TOKEN_FLOAT);
        if (token) {
            token->has_value = true;
            token->value.float_value = strtod(number_str, NULL);
        }
    } else {
        token = add_token(lexer, KRYON_TOKEN_INTEGER);
        if (token) {
            token->has_value = true;
            token->value.int_value = strtoll(number_str, NULL, 10);
        }
    }
    
    kryon_free(number_str);
    return token != NULL;
}

// =============================================================================
// IDENTIFIER LEXING
// =============================================================================

static bool lex_identifier(KryonLexer *lexer) {
    while (is_alnum(peek(lexer))) {
        advance(lexer);
    }
    
    size_t length = (size_t)(lexer->current - lexer->start);
    
    // Check for keywords
    KryonTokenType type = kryon_lexer_classify_keyword(lexer->start, length);
    if (type != KRYON_TOKEN_IDENTIFIER) {
        KryonToken *token = add_token(lexer, type);
        if (token && (type == KRYON_TOKEN_BOOLEAN_TRUE || type == KRYON_TOKEN_BOOLEAN_FALSE)) {
            token->has_value = true;
            token->value.bool_value = (type == KRYON_TOKEN_BOOLEAN_TRUE);
        }
        return token != NULL;
    }
    
    // Check if it's an element type (starts with uppercase)
    if (lexer->start[0] >= 'A' && lexer->start[0] <= 'Z') {
        type = KRYON_TOKEN_ELEMENT_TYPE;
    }
    
    return add_token(lexer, type) != NULL;
}

static bool lex_script_content(KryonLexer *lexer) {
    // We're at the opening brace, consume it
    advance(lexer); // consume '{'
    
    size_t value_capacity = 256;
    char *value = kryon_alloc(value_capacity);
    if (!value) {
        set_error(lexer, "Failed to allocate script content memory");
        return false;
    }
    
    size_t value_length = 0;
    int brace_depth = 1; // We already consumed the opening brace
    
    // Capture everything until matching closing brace
    while (!is_at_end(lexer) && brace_depth > 0) {
        char c = peek(lexer);
        
        if (c == '{') {
            brace_depth++;
        } else if (c == '}') {
            brace_depth--;
        }
        
        // Don't include the final closing brace in the content
        if (brace_depth > 0) {
            // Expand buffer if needed
            if (value_length >= value_capacity) {
                value_capacity *= 2;
                char *new_value = kryon_realloc(value, value_capacity);
                if (!new_value) {
                    kryon_free(value);
                    set_error(lexer, "Failed to reallocate script content memory");
                    return false;
                }
                value = new_value;
            }
            value[value_length++] = c;
        }
        
        advance(lexer);
    }
    
    if (brace_depth > 0) {
        kryon_free(value);
        set_error(lexer, "Unterminated script content");
        return false;
    }
    
    // Null-terminate value
    if (value_length >= value_capacity) {
        char *new_value = kryon_realloc(value, value_length + 1);
        if (!new_value) {
            kryon_free(value);
            set_error(lexer, "Failed to reallocate script content memory");
            return false;
        }
        value = new_value;
    }
    value[value_length] = '\0';
    
    KryonToken *token = add_token(lexer, KRYON_TOKEN_SCRIPT_CONTENT);
    if (token) {
        token->has_value = true;
        token->value.string_value = value;
    } else {
        kryon_free(value);
        return false;
    }
    
    return true;
}

static bool is_function_body_start(KryonLexer *lexer) {
    // Look at the recent tokens to see if we match the pattern:
    // @function "language" identifier ( [params...] ) {
    // We need at least 5 tokens for empty params, but could have more with parameters
    
    if (lexer->token_count < 5) {
        return false;
    }
    
    // Look backwards to find the pattern
    // Last token should be RIGHT_PAREN
    if (lexer->tokens[lexer->token_count - 1].type != KRYON_TOKEN_RIGHT_PAREN) {
        return false;
    }
    
    // Search backwards for the matching LEFT_PAREN
    int paren_depth = 1;
    int left_paren_index = -1;
    for (int i = lexer->token_count - 2; i >= 0 && paren_depth > 0; i--) {
        if (lexer->tokens[i].type == KRYON_TOKEN_RIGHT_PAREN) {
            paren_depth++;
        } else if (lexer->tokens[i].type == KRYON_TOKEN_LEFT_PAREN) {
            paren_depth--;
            if (paren_depth == 0) {
                left_paren_index = i;
                break;
            }
        }
    }
    
    if (left_paren_index < 2) {
        return false; // Need at least 3 tokens before LEFT_PAREN
    }
    
    // Check the tokens before LEFT_PAREN: should be identifier, string, @function
    KryonToken *identifier = &lexer->tokens[left_paren_index - 1];
    KryonToken *language_string = &lexer->tokens[left_paren_index - 2];
    KryonToken *function_directive = &lexer->tokens[left_paren_index - 3];
    
    return (function_directive->type == KRYON_TOKEN_FUNCTION_DIRECTIVE &&
            language_string->type == KRYON_TOKEN_STRING &&
            identifier->type == KRYON_TOKEN_IDENTIFIER);
}

// =============================================================================
// MAIN TOKENIZATION
// =============================================================================


static bool scan_token(KryonLexer *lexer) {
    lexer->start = lexer->current;
    
    if (is_at_end(lexer)) {
        add_token(lexer, KRYON_TOKEN_EOF);
        return false; // End tokenization
    }
    
    char c = advance(lexer);
    
    switch (c) {
        // Single character tokens
        case '(': add_token(lexer, KRYON_TOKEN_LEFT_PAREN); break;
        case ')': add_token(lexer, KRYON_TOKEN_RIGHT_PAREN); break;
        case '{': 
            // Check if this is the start of a script body
            if (is_function_body_start(lexer)) {
                lexer->current--; // Back up to include { in the script content
                return lex_script_content(lexer);
            } else {
                add_token(lexer, KRYON_TOKEN_LEFT_BRACE); 
            }
            break;
        case '}': add_token(lexer, KRYON_TOKEN_RIGHT_BRACE); break;
        case '[': add_token(lexer, KRYON_TOKEN_LEFT_BRACKET); break;
        case ']': add_token(lexer, KRYON_TOKEN_RIGHT_BRACKET); break;
        case ',': add_token(lexer, KRYON_TOKEN_COMMA); break;
        case '.': add_token(lexer, KRYON_TOKEN_DOT); break;
        case ';': add_token(lexer, KRYON_TOKEN_SEMICOLON); break;
        case '+': add_token(lexer, KRYON_TOKEN_PLUS); break;
        case '-': add_token(lexer, KRYON_TOKEN_MINUS); break;
        case '*': add_token(lexer, KRYON_TOKEN_MULTIPLY); break;
        case '/':
            if (match(lexer, '/')) {
                // Line comment - skip until end of line
                while (peek(lexer) != '\n' && !is_at_end(lexer)) {
                    advance(lexer);
                }
                if (lexer->config.preserve_comments) {
                    add_token(lexer, KRYON_TOKEN_LINE_COMMENT);
                }
                // If not preserving comments, just skip them (don't add token)
            } else if (match(lexer, '*')) {
                // Block comment - skip until */
                while (!is_at_end(lexer)) {
                    if (peek(lexer) == '*' && peek_next(lexer) == '/') {
                        advance(lexer); // consume *
                        advance(lexer); // consume /
                        break;
                    }
                    advance(lexer);
                }
                if (lexer->config.preserve_comments) {
                    add_token(lexer, KRYON_TOKEN_BLOCK_COMMENT);
                }
                // If not preserving comments, just skip them (don't add token)
            } else {
                add_token(lexer, KRYON_TOKEN_DIVIDE);
            }
            break;
        case '%': {
            // Context-aware % lexing: unit vs modulo
            bool is_unit = false;
            if (lexer->token_count > 0) {
                KryonToken *prev = &lexer->tokens[lexer->token_count - 1];
                bool is_number = (prev->type == KRYON_TOKEN_INTEGER || prev->type == KRYON_TOKEN_FLOAT);
                bool adjacent = (prev->lexeme + prev->lexeme_length == lexer->current - 1);
                is_unit = is_number && adjacent;
            }
            add_token(lexer, is_unit ? KRYON_TOKEN_UNIT_PERCENT : KRYON_TOKEN_MODULO);
            break;
        }
        case '?': add_token(lexer, KRYON_TOKEN_QUESTION); break;
        case '#': add_token(lexer, KRYON_TOKEN_HASH); break;
        
        // Two character tokens
        case '!':
            add_token(lexer, match(lexer, '=') ? KRYON_TOKEN_NOT_EQUALS : KRYON_TOKEN_LOGICAL_NOT);
            break;
        case '=':
            add_token(lexer, match(lexer, '=') ? KRYON_TOKEN_EQUALS : KRYON_TOKEN_ERROR);
            break;
        case '<':
            add_token(lexer, match(lexer, '=') ? KRYON_TOKEN_LESS_EQUAL : KRYON_TOKEN_LESS_THAN);
            break;
        case '>':
            add_token(lexer, match(lexer, '=') ? KRYON_TOKEN_GREATER_EQUAL : KRYON_TOKEN_GREATER_THAN);
            break;
        case '&':
            if (match(lexer, '&')) {
                add_token(lexer, KRYON_TOKEN_LOGICAL_AND);
            } else {
                add_token(lexer, KRYON_TOKEN_AMPERSAND);
            }
            break;
        case '|':
            add_token(lexer, match(lexer, '|') ? KRYON_TOKEN_LOGICAL_OR : KRYON_TOKEN_ERROR);
            break;
            
        // Colon (check for special case)
        case ':':
            add_token(lexer, KRYON_TOKEN_COLON);
            break;
            
        // @ directive
        case '@':
            if (is_alpha(peek(lexer))) {
                // Read directive name
                while (is_alnum(peek(lexer))) {
                    advance(lexer);
                }
                
                size_t length = (size_t)(lexer->current - lexer->start - 1); // -1 for @
                KryonTokenType type = kryon_lexer_classify_directive(lexer->start + 1, length);
                add_token(lexer, type);
            } else {
                add_token(lexer, KRYON_TOKEN_AT);
            }
            break;
            
        // $ variable or theme variable
        case '$':
            if (is_alpha(peek(lexer))) {
                // Read variable name, including dot notation for theme variables
                while (is_alnum(peek(lexer)) || peek(lexer) == '.') {
                    advance(lexer);
                }
                add_token(lexer, KRYON_TOKEN_VARIABLE);
            } else if (peek(lexer) == '{') {
                advance(lexer); // consume {
                add_token(lexer, KRYON_TOKEN_TEMPLATE_START);
            } else {
                add_token(lexer, KRYON_TOKEN_DOLLAR);
            }
            break;
            
        // String literals
        case '"':
        case '\'':
            lexer->current--; // Back up to include quote in lexeme
            return lex_string(lexer);
            
        // Whitespace
        case ' ':
        case '\r':
        case '\t':
            if (lexer->config.preserve_whitespace) {
                while (peek(lexer) == ' ' || peek(lexer) == '\r' || peek(lexer) == '\t') {
                    advance(lexer);
                }
                add_token(lexer, KRYON_TOKEN_WHITESPACE);
            }
            break;
            
        case '\n':
            if (lexer->config.preserve_whitespace) {
                add_token(lexer, KRYON_TOKEN_NEWLINE);
            }
            break;
            
        default:
            if (is_digit(c)) {
                lexer->current--; // Back up to include digit in lexeme
                return lex_number(lexer);
            } else if (is_alpha(c)) {
                lexer->current--; // Back up to include letter in lexeme
                return lex_identifier(lexer);
            } else if ((unsigned char)c >= 128) {
                // Handle UTF-8 multi-byte characters
                // For now, just skip UTF-8 characters gracefully instead of erroring
                // This allows emojis and other unicode in strings and comments
                // We advance past the UTF-8 sequence
                if ((unsigned char)c >= 0xF0) {
                    // 4-byte UTF-8 sequence (like 🎯)
                    advance(lexer); // 2nd byte
                    advance(lexer); // 3rd byte
                    advance(lexer); // 4th byte
                } else if ((unsigned char)c >= 0xE0) {
                    // 3-byte UTF-8 sequence
                    advance(lexer); // 2nd byte
                    advance(lexer); // 3rd byte
                } else if ((unsigned char)c >= 0xC0) {
                    // 2-byte UTF-8 sequence
                    advance(lexer); // 2nd byte
                }
                // Continue tokenizing after UTF-8 character
                return true;
            } else {
                set_error(lexer, "Unexpected character");
                add_token(lexer, KRYON_TOKEN_ERROR);
                return false;
            }
    }
    
    return true;
}

bool kryon_lexer_tokenize(KryonLexer *lexer) {
    if (!lexer) {
        KRYON_LOG_ERROR("Lexer cannot be NULL");
        return false;
    }
    
    clock_t start_time = clock();
    
    lexer->has_error = false;
    lexer->token_count = 0;
    lexer->current_token = 0;
    
    // Reset position
    lexer->current = lexer->source;
    lexer->start = lexer->source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->offset = 0;
    
    while (!is_at_end(lexer) && !lexer->has_error) {
        if (!scan_token(lexer)) {
            break; // EOF or error
        }
    }
    
    // Add EOF token if not already added
    if (lexer->token_count == 0 || 
        lexer->tokens[lexer->token_count - 1].type != KRYON_TOKEN_EOF) {
        lexer->start = lexer->current;
        add_token(lexer, KRYON_TOKEN_EOF);
    }
    
    clock_t end_time = clock();
    lexer->processing_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    lexer->total_lines = lexer->line;
    lexer->total_tokens = (uint32_t)lexer->token_count;
    
    KRYON_LOG_DEBUG("Lexed %zu tokens in %.2fms", 
                   lexer->token_count, lexer->processing_time * 1000.0);
    
    
    return !lexer->has_error;
}

// =============================================================================
// TOKEN ACCESS API
// =============================================================================

const KryonToken *kryon_lexer_next_token(KryonLexer *lexer) {
    if (!lexer || lexer->current_token >= lexer->token_count) {
        return NULL;
    }
    
    return &lexer->tokens[lexer->current_token++];
}

const KryonToken *kryon_lexer_peek_token(const KryonLexer *lexer, size_t offset) {
    if (!lexer) return NULL;
    
    size_t index = lexer->current_token + offset;
    if (index >= lexer->token_count) return NULL;
    
    return &lexer->tokens[index];
}

const KryonToken *kryon_lexer_current_token(const KryonLexer *lexer) {
    if (!lexer || lexer->current_token == 0 || lexer->current_token > lexer->token_count) {
        return NULL;
    }
    
    return &lexer->tokens[lexer->current_token - 1];
}

void kryon_lexer_reset(KryonLexer *lexer) {
    if (lexer) {
        lexer->current_token = 0;
    }
}

bool kryon_lexer_has_more_tokens(const KryonLexer *lexer) {
    return lexer && lexer->current_token < lexer->token_count;
}

const KryonToken *kryon_lexer_get_tokens(const KryonLexer *lexer, size_t *out_count) {
    if (!lexer || !out_count) return NULL;
    
    *out_count = lexer->token_count;
    
    
    return lexer->tokens;
}

const char *kryon_lexer_get_error(const KryonLexer *lexer) {
    return (lexer && lexer->has_error) ? lexer->error_message : NULL;
}

void kryon_lexer_get_stats(const KryonLexer *lexer, uint32_t *out_lines,
                          uint32_t *out_tokens, double *out_time) {
    if (!lexer) return;
    
    if (out_lines) *out_lines = lexer->total_lines;
    if (out_tokens) *out_tokens = lexer->total_tokens;
    if (out_time) *out_time = lexer->processing_time;
}

// =============================================================================
// CONFIGURATION HELPERS
// =============================================================================

KryonLexerConfig kryon_lexer_default_config(void) {
    KryonLexerConfig config = {0};
    config.preserve_whitespace = false;
    config.preserve_comments = false;
    config.track_positions = true;
    config.unicode_support = true;
    config.strict_mode = false;
    return config;
}

KryonLexerConfig kryon_lexer_minimal_config(void) {
    KryonLexerConfig config = {0};
    config.preserve_whitespace = false;
    config.preserve_comments = false;
    config.track_positions = false;
    config.unicode_support = false;
    config.strict_mode = false;
    return config;
}

KryonLexerConfig kryon_lexer_ide_config(void) {
    KryonLexerConfig config = {0};
    config.preserve_whitespace = true;
    config.preserve_comments = true;
    config.track_positions = true;
    config.unicode_support = true;
    config.strict_mode = true;
    return config;
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

const char *kryon_token_type_name(KryonTokenType type) {
    if (type >= 0 && (size_t)type < sizeof(token_type_names) / sizeof(token_type_names[0])) {
        return token_type_names[type];
    }
    return "UNKNOWN";
}

bool kryon_token_is_literal(KryonTokenType type) {
    return type == KRYON_TOKEN_STRING ||
           type == KRYON_TOKEN_INTEGER ||
           type == KRYON_TOKEN_FLOAT ||
           type == KRYON_TOKEN_BOOLEAN_TRUE ||
           type == KRYON_TOKEN_BOOLEAN_FALSE ||
           type == KRYON_TOKEN_NULL;
}

bool kryon_token_is_operator(KryonTokenType type) {
    return type >= KRYON_TOKEN_COLON && type <= KRYON_TOKEN_AMPERSAND;
}

bool kryon_token_is_directive(KryonTokenType type) {
    return type >= KRYON_TOKEN_STYLE_DIRECTIVE && type <= KRYON_TOKEN_LIFECYCLE_DIRECTIVE;
}

bool kryon_token_is_unit(KryonTokenType type) {
    return type >= KRYON_TOKEN_UNIT_PX && type <= KRYON_TOKEN_UNIT_PT;
}

bool kryon_token_is_bracket(KryonTokenType type) {
    return type == KRYON_TOKEN_LEFT_BRACE || type == KRYON_TOKEN_RIGHT_BRACE ||
           type == KRYON_TOKEN_LEFT_BRACKET || type == KRYON_TOKEN_RIGHT_BRACKET ||
           type == KRYON_TOKEN_LEFT_PAREN || type == KRYON_TOKEN_RIGHT_PAREN;
}

char *kryon_token_copy_lexeme(const KryonToken *token) {
    if (!token) return NULL;
    
    char *copy = kryon_alloc(token->lexeme_length + 1);
    if (!copy) return NULL;
    
    memcpy(copy, token->lexeme, token->lexeme_length);
    copy[token->lexeme_length] = '\0';
    
    return copy;
}

bool kryon_token_lexeme_equals(const KryonToken *token, const char *str) {
    if (!token || !str) return false;
    
    size_t str_len = strlen(str);
    return token->lexeme_length == str_len &&
           memcmp(token->lexeme, str, str_len) == 0;
}

void kryon_token_print(const KryonToken *token, FILE *file) {
    if (!token) return;
    if (!file) file = stdout;
    
    fprintf(file, "%s", kryon_token_type_name(token->type));
    
    if (token->lexeme_length > 0) {
        fprintf(file, " '%.*s'", (int)token->lexeme_length, token->lexeme);
    }
    
    if (token->has_value) {
        switch (token->type) {
            case KRYON_TOKEN_STRING:
                fprintf(file, " = \"%s\"", token->value.string_value);
                break;
            case KRYON_TOKEN_SCRIPT_CONTENT:
                fprintf(file, " = {%s}", token->value.string_value);
                break;
            case KRYON_TOKEN_INTEGER:
                fprintf(file, " = %lld", (long long)token->value.int_value);
                break;
            case KRYON_TOKEN_FLOAT:
                fprintf(file, " = %g", token->value.float_value);
                break;
            case KRYON_TOKEN_BOOLEAN_TRUE:
            case KRYON_TOKEN_BOOLEAN_FALSE:
                fprintf(file, " = %s", token->value.bool_value ? "true" : "false");
                break;
            default:
                break;
        }
    }
    
    fprintf(file, " @ %u:%u", token->location.line, token->location.column);
}

size_t kryon_source_location_format(const KryonSourceLocation *location,
                                   char *buffer, size_t buffer_size) {
    if (!location || !buffer || buffer_size == 0) return 0;
    
    if (location->filename) {
        return snprintf(buffer, buffer_size, "%s:%u:%u", 
                       location->filename, location->line, location->column);
    } else {
        return snprintf(buffer, buffer_size, "%u:%u", 
                       location->line, location->column);
    }
}