/**
 * KRY @universal Transpiler Implementation
 *
 * Transpiles platform-agnostic code to Lua and JavaScript
 */

#define _POSIX_C_SOURCE 200809L
#include "kry_universal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================================
// Error Handling
// ============================================================================

static char* g_last_error = NULL;

static void set_error(const char* message) {
    if (g_last_error) free(g_last_error);
    g_last_error = strdup(message);
}

// ============================================================================
// String Builder for Efficient Code Generation
// ============================================================================

typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} StringBuilder;

static void sb_init(StringBuilder* sb, size_t initial_capacity) {
    sb->capacity = initial_capacity;
    sb->size = 0;
    sb->data = malloc(initial_capacity);
    if (sb->data) sb->data[0] = '\0';
}

static void sb_append(StringBuilder* sb, const char* str) {
    if (!str) return;
    size_t len = strlen(str);
    if (sb->size + len + 1 > sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        if (new_capacity < sb->size + len + 1) new_capacity = sb->size + len + 1;
        sb->data = realloc(sb->data, new_capacity);
        sb->capacity = new_capacity;
    }
    if (sb->data) {
        strcpy(sb->data + sb->size, str);
        sb->size += len;
    }
}

static void sb_append_char(StringBuilder* sb, char c) {
    char str[2] = {c, '\0'};
    sb_append(sb, str);
}

static void sb_free(StringBuilder* sb) {
    free(sb->data);
    sb->data = NULL;
    sb->size = sb->capacity = 0;
}

// ============================================================================
// Token Types for Parsing
// ============================================================================

typedef enum {
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_OPERATOR,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_DOT,
    TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_SEMICOLON,
    TOKEN_EQUAL,
    TOKEN_ARROW,      // ->
    TOKEN_EOF,
    TOKEN_UNKNOWN
} TokenType;

typedef struct {
    TokenType type;
    char* value;
    size_t length;
} Token;

// ============================================================================
// Lexer
// ============================================================================

static const char* g_source = NULL;
static size_t g_pos = 0;
static size_t g_source_len = 0;

static char peek_char(void) {
    return (g_pos < g_source_len) ? g_source[g_pos] : '\0';
}

static char next_char(void) {
    if (g_pos >= g_source_len) return '\0';
    return g_source[g_pos++];
}

static void skip_whitespace(void) {
    while (isspace(peek_char())) {
        next_char();
    }
}

static Token create_token(TokenType type, const char* value, size_t length) {
    Token token;
    token.type = type;
    token.length = length;
    if (value && length > 0) {
        token.value = malloc(length + 1);
        memcpy(token.value, value, length);
        token.value[length] = '\0';
    } else {
        token.value = NULL;
    }
    return token;
}

static void free_token(Token* token) {
    if (token->value) free(token->value);
    token->value = NULL;
}

static Token next_token(void) {
    skip_whitespace();

    if (g_pos >= g_source_len) {
        return create_token(TOKEN_EOF, NULL, 0);
    }

    char c = peek_char();

    // Single character tokens
    switch (c) {
        case '(': return create_token(TOKEN_LPAREN, NULL, 1);
        case ')': return create_token(TOKEN_RPAREN, NULL, 1);
        case '[': return create_token(TOKEN_LBRACKET, NULL, 1);
        case ']': return create_token(TOKEN_RBRACKET, NULL, 1);
        case '{': return create_token(TOKEN_LBRACE, NULL, 1);
        case '}': return create_token(TOKEN_RBRACE, NULL, 1);
        case '.': return create_token(TOKEN_DOT, NULL, 1);
        case ',': return create_token(TOKEN_COMMA, NULL, 1);
        case ':': return create_token(TOKEN_COLON, NULL, 1);
        case ';': return create_token(TOKEN_SEMICOLON, NULL, 1);
        case '=': next_char(); return create_token(TOKEN_EQUAL, "=", 1);
    }

    // Arrow ->
    if (c == '-' && g_pos + 1 < g_source_len && g_source[g_pos + 1] == '>') {
        g_pos += 2;
        return create_token(TOKEN_ARROW, "->", 2);
    }

    // Operators
    if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
        c == '!' || c == '<' || c == '>' || c == '|' || c == '&') {
        next_char();
        char op[2] = {c, '\0'};
        return create_token(TOKEN_OPERATOR, op, 1);
    }

    // Number
    if (isdigit(c) || (c == '.' && isdigit(g_source[g_pos + 1]))) {
        size_t start = g_pos;
        while (isdigit(peek_char()) || peek_char() == '.') {
            next_char();
        }
        return create_token(TOKEN_NUMBER, g_source + start, g_pos - start);
    }

    // String literal
    if (c == '"' || c == '\'' || c == '`') {
        char quote = c;
        next_char(); // Skip opening quote
        size_t start = g_pos;
        while (peek_char() != quote && peek_char() != '\0') {
            if (peek_char() == '\\' && g_pos + 1 < g_source_len) {
                next_char(); // Skip escape char
            }
            next_char();
        }
        next_char(); // Skip closing quote
        return create_token(TOKEN_STRING, g_source + start, g_pos - start - 1);
    }

    // Identifier or keyword
    if (isalpha(c) || c == '_') {
        size_t start = g_pos;
        while (isalnum(peek_char()) || peek_char() == '_') {
            next_char();
        }
        return create_token(TOKEN_IDENTIFIER, g_source + start, g_pos - start);
    }

    // Unknown
    next_char();
    return create_token(TOKEN_UNKNOWN, NULL, 0);
}

// ============================================================================
// Standard Library Function Transpilation
// ============================================================================

typedef struct {
    const char* universal;
    const char* lua;
    const char* javascript;
} StdLibMapping;

static const StdLibMapping g_stdlib_mappings[] = {
    // Array functions
    {"array.length(", "#", "arr.length"},
    {"array.push(", "table.insert(", "arr.push("},
    {"array.pop(", "table.remove(", "arr.pop("},
    {"array.map(", "custom_map(", "arr.map("},
    {"array.filter(", "custom_filter(", "arr.filter("},
    {"array.reduce(", "custom_reduce(", "arr.reduce("},

    // Math functions
    {"math.random()", "math.random()", "Math.random()"},
    {"math.floor(", "math.floor(", "Math.floor("},
    {"math.ceil(", "math.ceil(", "Math.ceil("},
    {"math.abs(", "math.abs(", "Math.abs("},
    {"math.min(", "math.min(", "Math.min("},
    {"math.max(", "math.max(", "Math.max("},

    // String functions
    {"string.match(", ":match(", ".match("},
    {"string.find(", ":find(", ".indexOf("},
    {"string.sub(", ":sub(", ".substring("},
    {"string.length(", "#", ".length"},
    {"string.upper(", ":upper()", ".toUpperCase()"},
    {"string.lower(", ":lower()", ".toLowerCase()"},

    // Object functions
    {"object.keys(", "custom_keys(", "Object.keys("},
    {"object.values(", "custom_values(", "Object.values("},
    {"object.entries(", "custom_entries(", "Object.entries("},

    {NULL, NULL, NULL}
};

// ============================================================================
// Transpiler Core
// ============================================================================

static void transpile_identifier(StringBuilder* sb, const char* identifier, KryTargetPlatform platform) {
    // Check if identifier is a standard library function call
    for (const StdLibMapping* mapping = g_stdlib_mappings; mapping->universal; mapping++) {
        if (strncmp(identifier, mapping->universal, strlen(mapping->universal)) == 0) {
            const char* replacement = (platform == KRY_TARGET_LUA) ? mapping->lua : mapping->javascript;

            // Special handling for array.length(arr) -> #arr or arr.length
            if (strcmp(mapping->universal, "array.length(") == 0) {
                if (platform == KRY_TARGET_LUA) {
                    // Lua: array.length(arr) -> #arr
                    // We need to get the argument
                    const char* arg_start = identifier + strlen(mapping->universal);
                    sb_append(sb, "#");
                    // Find matching closing paren
                    int depth = 1;
                    const char* p = arg_start;
                    while (*p && depth > 0) {
                        if (*p == '(') depth++;
                        else if (*p == ')') depth--;
                        if (depth > 0) sb_append_char(sb, *p);
                        p++;
                    }
                    return;
                } else {
                    // JavaScript: array.length(arr) -> arr.length
                    // Get the argument name
                    const char* arg_start = identifier + strlen(mapping->universal);
                    const char* arg_end = strchr(arg_start, ')');
                    if (arg_end) {
                        sb_append(sb, "(");
                        // Append substring
                        char* arg_name = strndup(arg_start, arg_end - arg_start);
                        sb_append(sb, arg_name);
                        free(arg_name);
                        sb_append(sb, ").length");
                        return;
                    }
                }
            }

            sb_append(sb, replacement);
            return;
        }
    }

    // Not a standard library function, just output as-is
    sb_append(sb, identifier);
}

static void transpile_code(StringBuilder* sb, const char* source, KryTargetPlatform platform) {
    g_source = source;
    g_pos = 0;
    g_source_len = strlen(source);

    while (g_pos < g_source_len) {
        skip_whitespace();
        if (g_pos >= g_source_len) break;

        Token token = next_token();

        switch (token.type) {
            case TOKEN_IDENTIFIER:
                transpile_identifier(sb, token.value, platform);
                break;

            case TOKEN_NUMBER:
            case TOKEN_STRING:
            case TOKEN_OPERATOR:
            case TOKEN_LPAREN:
            case TOKEN_RPAREN:
            case TOKEN_LBRACKET:
            case TOKEN_RBRACKET:
            case TOKEN_LBRACE:
            case TOKEN_RBRACE:
            case TOKEN_DOT:
            case TOKEN_COMMA:
            case TOKEN_COLON:
            case TOKEN_SEMICOLON:
            case TOKEN_EQUAL:
            case TOKEN_ARROW:
                if (token.value) {
                    sb_append(sb, token.value);
                } else if (token.type == TOKEN_DOT) {
                    sb_append(sb, ".");
                } else if (token.type == TOKEN_LPAREN) {
                    sb_append(sb, "(");
                } else if (token.type == TOKEN_RPAREN) {
                    sb_append(sb, ")");
                } else if (token.type == TOKEN_LBRACKET) {
                    sb_append(sb, "[");
                } else if (token.type == TOKEN_RBRACKET) {
                    sb_append(sb, "]");
                } else if (token.type == TOKEN_LBRACE) {
                    sb_append(sb, "{");
                } else if (token.type == TOKEN_RBRACE) {
                    sb_append(sb, "}");
                } else if (token.type == TOKEN_COMMA) {
                    sb_append(sb, ",");
                } else if (token.type == TOKEN_COLON) {
                    sb_append(sb, ":");
                } else if (token.type == TOKEN_SEMICOLON) {
                    sb_append(sb, ";");
                } else if (token.type == TOKEN_ARROW) {
                    // Arrow function: (args) -> expr
                    // Lua: function(args) return expr end
                    // JavaScript: (args) => expr
                    if (platform == KRY_TARGET_LUA) {
                        sb_append(sb, " function");
                    } else {
                        sb_append(sb, " =>");
                    }
                }
                break;

            case TOKEN_EOF:
                goto done;

            default:
                // Unknown token - just skip
                break;
        }

        free_token(&token);
    }

done:
    if (sb->size > 0) {
        sb->data[sb->size] = '\0';
    }
}

// ============================================================================
// Public API Implementation
// ============================================================================

char* kry_universal_transpile(const char* source,
                              KryTargetPlatform platform,
                              size_t* output_length) {
    KryTranspilerOptions options = {
        .platform = platform,
        .minify = false,
        .include_comments = true,
        .use_strict = true
    };
    return kry_universal_transpile_ex(source, &options, output_length);
}

char* kry_universal_transpile_ex(const char* source,
                                 const KryTranspilerOptions* options,
                                 size_t* output_length) {
    if (!source || !options) {
        set_error("Invalid arguments: source and options must not be NULL");
        return NULL;
    }

    // Reset error state
    kry_universal_clear_error();

    // Create string builder
    StringBuilder sb;
    sb_init(&sb, strlen(source) * 2);  // Start with 2x capacity

    // Transpile the code
    transpile_code(&sb, source, options->platform);

    // Add null terminator
    if (sb.size > 0) {
        sb.data[sb.size] = '\0';
    } else {
        sb_append(&sb, "");
    }

    if (output_length) {
        *output_length = sb.size;
    }

    return sb.data;  // Caller must free
}

const char* kry_universal_version(void) {
    return "1.0.0";
}

const char* kry_universal_get_error(void) {
    return g_last_error;
}

void kry_universal_clear_error(void) {
    if (g_last_error) {
        free(g_last_error);
        g_last_error = NULL;
    }
}
