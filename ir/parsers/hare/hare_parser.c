/**
 * Hare Parser Implementation
 *
 * Parses Hare DSL syntax and converts to KIR JSON.
 * This is a simplified parser that handles the Kryon DSL subset.
 */

#define _POSIX_C_SOURCE 200809L

#include "hare_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ============================================================================
// Error Handling
// ============================================================================

static char* g_last_error = NULL;

static void set_error(const char* error) {
    if (g_last_error) {
        free(g_last_error);
    }
    g_last_error = error ? strdup(error) : NULL;
}

const char* ir_hare_get_error(void) {
    return g_last_error;
}

void ir_hare_clear_error(void) {
    if (g_last_error) {
        free(g_last_error);
        g_last_error = NULL;
    }
}

// ============================================================================
// String Builder for JSON Output
// ============================================================================

typedef struct {
    char* buffer;
    size_t size;
    size_t capacity;
} JSONBuilder;

static bool json_init(JSONBuilder* jb) {
    jb->capacity = 8192;
    jb->size = 0;
    jb->buffer = malloc(jb->capacity);
    if (!jb->buffer) return false;
    jb->buffer[0] = '\0';
    return true;
}

static bool json_append(JSONBuilder* jb, const char* str) {
    size_t len = strlen(str);
    while (jb->size + len >= jb->capacity) {
        jb->capacity *= 2;
        char* new_buf = realloc(jb->buffer, jb->capacity);
        if (!new_buf) return false;
        jb->buffer = new_buf;
    }
    strcpy(jb->buffer + jb->size, str);
    jb->size += len;
    return true;
}

static bool json_append_escaped(JSONBuilder* jb, const char* str) {
    json_append(jb, "\"");
    for (size_t i = 0; str[i]; i++) {
        switch (str[i]) {
            case '"':  json_append(jb, "\\\""); break;
            case '\\': json_append(jb, "\\\\"); break;
            case '\n': json_append(jb, "\\n"); break;
            case '\r': json_append(jb, "\\r"); break;
            case '\t': json_append(jb, "\\t"); break;
            default:   {
                char buf[2] = {str[i], '\0'};
                json_append(jb, buf);
                break;
            }
        }
    }
    json_append(jb, "\"");
    return true;
}

static char* json_finish(JSONBuilder* jb) {
    json_append(jb, "\0");
    char* result = jb->buffer;
    jb->buffer = NULL;  // Transfer ownership - don't free in json_free
    return result;
}

static void json_free(JSONBuilder* jb) {
    if (jb && jb->buffer) {
        free(jb->buffer);
        jb->buffer = NULL;
    }
}

// ============================================================================
// Hare Lexer
// ============================================================================

typedef enum {
    HARE_TOKEN_EOF,
    HARE_TOKEN_IDENTIFIER,
    HARE_TOKEN_STRING,
    HARE_TOKEN_NUMBER,
    HARE_TOKEN_LBRACE,       // {
    HARE_TOKEN_RBRACE,       // }
    HARE_TOKEN_LPAREN,       // (
    HARE_TOKEN_RPAREN,       // )
    HARE_TOKEN_LBRACKET,     // [
    HARE_TOKEN_RBRACKET,     // ]
    HARE_TOKEN_SEMICOLON,    // ;
    HARE_TOKEN_COMMA,        // ,
    HARE_TOKEN_DOT,          // .
    HARE_TOKEN_COLON,        // :
    HARE_TOKEN_DCOLON,       // ::
    HARE_TOKEN_ASTERISK,     // *
    HARE_TOKEN_ARROW,        // =>
    HARE_TOKEN_EQUAL,        // =
    HARE_TOKEN_USE,
    HARE_TOKEN_EXPORT,
    HARE_TOKEN_FN,
    HARE_TOKEN_RETURN,
    HARE_TOKEN_VOID,
    HARE_TOKEN_UNKNOWN
} HareTokenType;

typedef struct {
    HareTokenType type;
    char* value;
    size_t line;
    size_t col;
} HareToken;

static HareToken* hare_token_create(HareTokenType type, const char* value, size_t line, size_t col) {
    HareToken* tok = calloc(1, sizeof(HareToken));
    if (tok) {
        tok->type = type;
        tok->value = value ? strdup(value) : NULL;
        tok->line = line;
        tok->col = col;
    }
    return tok;
}

static void hare_token_free(HareToken* tok) {
    if (tok) {
        free(tok->value);
        free(tok);
    }
}

typedef struct {
    const char* input;
    size_t pos;
    size_t line;
    size_t col;
    size_t length;
} HareLexer;

static void hare_lexer_init(HareLexer* lex, const char* input) {
    lex->input = input;
    lex->pos = 0;
    lex->line = 1;
    lex->col = 1;
    lex->length = strlen(input);
}

static char hare_lexer_peek(HareLexer* lex) {
    if (lex->pos >= lex->length) return '\0';
    return lex->input[lex->pos];
}

static char hare_lexer_advance(HareLexer* lex) {
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

static void hare_lexer_skip_whitespace(HareLexer* lex) {
    while (lex->pos < lex->length) {
        char ch = hare_lexer_peek(lex);
        if (isspace(ch)) {
            hare_lexer_advance(lex);
        } else if (ch == '/' && lex->pos + 1 < lex->length && lex->input[lex->pos + 1] == '/') {
            // Single-line comment
            while (hare_lexer_peek(lex) != '\n' && hare_lexer_peek(lex) != '\0') {
                hare_lexer_advance(lex);
            }
        } else {
            break;
        }
    }
}

static HareToken* hare_lexer_next_token(HareLexer* lex) {
    hare_lexer_skip_whitespace(lex);

    if (lex->pos >= lex->length) {
        return hare_token_create(HARE_TOKEN_EOF, NULL, lex->line, lex->col);
    }

    size_t start_line = lex->line;
    size_t start_col = lex->col;
    char ch = hare_lexer_peek(lex);

    // String literal
    if (ch == '"') {
        hare_lexer_advance(lex); // skip opening quote
        char buf[4096];
        size_t i = 0;
        while ((ch = hare_lexer_peek(lex)) != '"' && ch != '\0' && ch != '\n') {
            if (ch == '\\') {
                hare_lexer_advance(lex);
                ch = hare_lexer_peek(lex);
            }
            buf[i++] = ch;
            hare_lexer_advance(lex);
        }
        buf[i] = '\0';
        hare_lexer_advance(lex); // skip closing quote
        return hare_token_create(HARE_TOKEN_STRING, buf, start_line, start_col);
    }

    // Number
    if (isdigit(ch) || (ch == '.' && isdigit(lex->input[lex->pos + 1]))) {
        char buf[64];
        size_t i = 0;
        while (isdigit(hare_lexer_peek(lex)) || hare_lexer_peek(lex) == '.') {
            buf[i++] = hare_lexer_advance(lex);
        }
        buf[i] = '\0';
        return hare_token_create(HARE_TOKEN_NUMBER, buf, start_line, start_col);
    }

    // Double colon ::
    if (ch == ':' && lex->pos + 1 < lex->length && lex->input[lex->pos + 1] == ':') {
        hare_lexer_advance(lex);
        hare_lexer_advance(lex);
        return hare_token_create(HARE_TOKEN_DCOLON, "::", start_line, start_col);
    }

    // Arrow =>
    if (ch == '=' && lex->pos + 1 < lex->length && lex->input[lex->pos + 1] == '>') {
        hare_lexer_advance(lex);
        hare_lexer_advance(lex);
        return hare_token_create(HARE_TOKEN_ARROW, "=>", start_line, start_col);
    }

    // Single character tokens
    switch (ch) {
        case '{': hare_lexer_advance(lex); return hare_token_create(HARE_TOKEN_LBRACE, "{", start_line, start_col);
        case '}': hare_lexer_advance(lex); return hare_token_create(HARE_TOKEN_RBRACE, "}", start_line, start_col);
        case '(': hare_lexer_advance(lex); return hare_token_create(HARE_TOKEN_LPAREN, "(", start_line, start_col);
        case ')': hare_lexer_advance(lex); return hare_token_create(HARE_TOKEN_RPAREN, ")", start_line, start_col);
        case '[': hare_lexer_advance(lex); return hare_token_create(HARE_TOKEN_LBRACKET, "[", start_line, start_col);
        case ']': hare_lexer_advance(lex); return hare_token_create(HARE_TOKEN_RBRACKET, "]", start_line, start_col);
        case ';': hare_lexer_advance(lex); return hare_token_create(HARE_TOKEN_SEMICOLON, ";", start_line, start_col);
        case ',': hare_lexer_advance(lex); return hare_token_create(HARE_TOKEN_COMMA, ",", start_line, start_col);
        case '.': hare_lexer_advance(lex); return hare_token_create(HARE_TOKEN_DOT, ".", start_line, start_col);
        case ':': hare_lexer_advance(lex); return hare_token_create(HARE_TOKEN_COLON, ":", start_line, start_col);
        case '*': hare_lexer_advance(lex); return hare_token_create(HARE_TOKEN_ASTERISK, "*", start_line, start_col);
        case '=': hare_lexer_advance(lex); return hare_token_create(HARE_TOKEN_EQUAL, "=", start_line, start_col);
    }

    // Identifier or keyword
    if (isalpha(ch) || ch == '_') {
        char buf[256];
        size_t i = 0;
        while (isalnum(hare_lexer_peek(lex)) || hare_lexer_peek(lex) == '_') {
            buf[i++] = hare_lexer_advance(lex);
        }
        buf[i] = '\0';

        // Check for keywords
        if (strcmp(buf, "use") == 0) return hare_token_create(HARE_TOKEN_USE, "use", start_line, start_col);
        if (strcmp(buf, "export") == 0) return hare_token_create(HARE_TOKEN_EXPORT, "export", start_line, start_col);
        if (strcmp(buf, "fn") == 0) return hare_token_create(HARE_TOKEN_FN, "fn", start_line, start_col);
        if (strcmp(buf, "return") == 0) return hare_token_create(HARE_TOKEN_RETURN, "return", start_line, start_col);
        if (strcmp(buf, "void") == 0) return hare_token_create(HARE_TOKEN_VOID, "void", start_line, start_col);

        return hare_token_create(HARE_TOKEN_IDENTIFIER, buf, start_line, start_col);
    }

    hare_lexer_advance(lex);
    return hare_token_create(HARE_TOKEN_UNKNOWN, NULL, start_line, start_col);
}

// ============================================================================
// Parser State
// ============================================================================

typedef struct {
    HareLexer lexer;
    HareToken* current;
    JSONBuilder json;  // Embedded, not pointer
} HareParser;

static void hare_parser_init(HareParser* p, const char* input) {
    hare_lexer_init(&p->lexer, input);
    p->current = hare_lexer_next_token(&p->lexer);
    json_init(&p->json);
}

static void hare_parser_free(HareParser* p) {
    if (p->current) {
        hare_token_free(p->current);
    }
    json_free(&p->json);
}

static HareToken* hare_parser_peek(HareParser* p) {
    return p->current;
}

static HareToken* hare_parser_next(HareParser* p) {
    if (p->current) {
        hare_token_free(p->current);
    }
    p->current = hare_lexer_next_token(&p->lexer);
    return p->current;
}

static bool hare_parser_consume(HareParser* p, HareTokenType type) {
    if (p->current->type == type) {
        hare_parser_next(p);
        return true;
    }
    return false;
}

// ============================================================================
// Parser Implementation
// ============================================================================

/**
 * Parse a component builder call like:
 * container { b => b.width("100%"); b.child(text { t => t.text("hi") }); }
 */
static bool hare_parse_component_builder(HareParser* p, const char* component_type) {
    // Expected: LBRACE IDENTIFIER ARROW (then builder.method calls)
    // E.g., container { b => b.width("100%"); }

    if (!hare_parser_consume(p, HARE_TOKEN_LBRACE)) {
        set_error("Expected '{' after component type");
        return false;
    }

    if (!hare_parser_consume(p, HARE_TOKEN_IDENTIFIER)) {
        set_error("Expected builder parameter name");
        return false;
    }
    if (!hare_parser_consume(p, HARE_TOKEN_ARROW)) {
        set_error("Expected '=>' after parameter name");
        return false;
    }

    // Start component object
    json_append(&p->json, "{\"type\":\"");
    // Capitalize first letter of component type for KIR
    char capitalized_type[256];
    size_t len = strlen(component_type);
    if (len > 0 && len < sizeof(capitalized_type) - 1) {
        capitalized_type[0] = component_type[0] >= 'a' && component_type[0] <= 'z'
            ? component_type[0] - 32 : component_type[0];
        for (size_t i = 1; i < len; i++) {
            capitalized_type[i] = component_type[i];
        }
        capitalized_type[len] = '\0';
        json_append(&p->json, capitalized_type);
    } else {
        json_append(&p->json, component_type);
    }
    json_append(&p->json, "\"");

    // Parse builder method calls: builder.method(arg);
    while (p->current->type != HARE_TOKEN_RBRACE && p->current->type != HARE_TOKEN_EOF) {
        // builder.method(...)
        if (!hare_parser_consume(p, HARE_TOKEN_IDENTIFIER)) break; // builder name (unused)
        if (!hare_parser_consume(p, HARE_TOKEN_DOT)) break;        // dot operator

        // Save method name before consuming token
        char* method_name = strdup(p->current->value);
        if (!hare_parser_consume(p, HARE_TOKEN_IDENTIFIER)) {
            free(method_name);
            break; // method name
        }

        // Check for method call
        if (p->current->type == HARE_TOKEN_LPAREN) {
            hare_parser_consume(p, HARE_TOKEN_LPAREN);

            // Parse argument (could be string, number, or nested component)
            if (p->current->type == HARE_TOKEN_STRING) {
                // Property value like b.width("100%")
                const char* prop_name = method_name;

                // Map camelCase to snake_case for KIR
                char prop_key[256];
                size_t j = 0;
                for (size_t i = 0; prop_name[i] && j < sizeof(prop_key) - 1; i++) {
                    if (prop_name[i] >= 'A' && prop_name[i] <= 'Z') {
                        prop_key[j++] = '_';
                        prop_key[j++] = prop_name[i] + 32;
                    } else {
                        prop_key[j++] = prop_name[i];
                    }
                }
                prop_key[j] = '\0';

                json_append(&p->json, ",\"");
                json_append(&p->json, prop_key);
                json_append(&p->json, "\":");
                json_append_escaped(&p->json, p->current->value);

                hare_parser_consume(p, HARE_TOKEN_STRING);
                hare_parser_consume(p, HARE_TOKEN_RPAREN);
                hare_parser_consume(p, HARE_TOKEN_SEMICOLON);
                free(method_name);
            } else if (p->current->type == HARE_TOKEN_NUMBER) {
                // Numeric property value like r.gap(10)
                const char* prop_name = method_name;

                // Map camelCase to snake_case for KIR
                char prop_key[256];
                size_t j = 0;
                for (size_t i = 0; prop_name[i] && j < sizeof(prop_key) - 1; i++) {
                    if (prop_name[i] >= 'A' && prop_name[i] <= 'Z') {
                        prop_key[j++] = '_';
                        prop_key[j++] = prop_name[i] + 32;
                    } else {
                        prop_key[j++] = prop_name[i];
                    }
                }
                prop_key[j] = '\0';

                json_append(&p->json, ",\"");
                json_append(&p->json, prop_key);
                json_append(&p->json, "\":");
                json_append(&p->json, p->current->value);  // Numbers don't need escaping

                hare_parser_consume(p, HARE_TOKEN_NUMBER);
                hare_parser_consume(p, HARE_TOKEN_RPAREN);
                hare_parser_consume(p, HARE_TOKEN_SEMICOLON);
                free(method_name);
            } else if (p->current->type == HARE_TOKEN_IDENTIFIER) {
                // Nested component like b.child(container { ... })

                if (strcmp(method_name, "child") == 0 || strcmp(method_name, "add_child") == 0) {
                    json_append(&p->json, ",\"children\":[");

                    // Duplicate child_type since token will be freed
                    char* child_type = strdup(p->current->value);
                    hare_parser_consume(p, HARE_TOKEN_IDENTIFIER);

                    if (!hare_parse_component_builder(p, child_type)) {
                        free(child_type);
                        free(method_name);
                        return false;
                    }
                    free(child_type);

                    json_append(&p->json, "]");

                    // Consume closing ) and ;
                    hare_parser_consume(p, HARE_TOKEN_RPAREN);
                    hare_parser_consume(p, HARE_TOKEN_SEMICOLON);
                } else {
                    // Unknown method
                    char err[256];
                    snprintf(err, sizeof(err), "Unknown builder method: %s", method_name);
                    set_error(err);
                    free(method_name);
                    return false;
                }
                free(method_name);
            } else {
                free(method_name);
            }
        } else {
            free(method_name);
        }
    }

    json_append(&p->json, "}");

    if (!hare_parser_consume(p, HARE_TOKEN_RBRACE)) {
        set_error("Expected '}' at end of component builder");
        return false;
    }
    return true;
}

/**
 * Parse the main function:
 * export fn main() void = { ... }
 */
static bool hare_parse_main(HareParser* p) {
    // export fn main ( ) void = { body }
    if (!hare_parser_consume(p, HARE_TOKEN_EXPORT)) {
        set_error("Expected 'export' keyword");
        return false;
    }
    if (!hare_parser_consume(p, HARE_TOKEN_FN)) {
        set_error("Expected 'fn' keyword");
        return false;
    }
    if (!hare_parser_consume(p, HARE_TOKEN_IDENTIFIER)) {
        set_error("Expected function name");
        return false;
    } // main
    if (!hare_parser_consume(p, HARE_TOKEN_LPAREN)) {
        set_error("Expected '('");
        return false;
    }
    if (!hare_parser_consume(p, HARE_TOKEN_RPAREN)) {
        set_error("Expected ')'");
        return false;
    }
    if (!hare_parser_consume(p, HARE_TOKEN_VOID)) {
        set_error("Expected 'void'");
        return false;
    }
    if (!hare_parser_consume(p, HARE_TOKEN_EQUAL)) {
        set_error("Expected '='");
        return false;
    }
    if (!hare_parser_consume(p, HARE_TOKEN_LBRACE)) {
        set_error("Expected '{'");
        return false;
    }

    // Parse body - should contain component builder calls
    json_append(&p->json, "{\"root\":");

    if (p->current->type == HARE_TOKEN_IDENTIFIER) {
        // Duplicate the component type string since the token will be freed
        char* component_type = strdup(p->current->value);
        hare_parser_next(p); // consume component type identifier
        if (!hare_parse_component_builder(p, component_type)) {
            free(component_type);
            return false;
        }
        free(component_type);
    }

    json_append(&p->json, "}");

    if (!hare_parser_consume(p, HARE_TOKEN_RBRACE)) {
        set_error("Expected '}' at end of main");
        return false;
    }
    return true;
}

// ============================================================================
// Public API
// ============================================================================

char* ir_hare_to_kir(const char* source, size_t length) {
    (void)length; // We use strlen instead
    if (!source || !*source) {
        set_error("Empty source");
        return NULL;
    }

    HareParser parser;
    hare_parser_init(&parser, source);

    // Parse use statements (skip for now, just validate)
    while (parser.current->type == HARE_TOKEN_USE) {
        hare_parser_next(&parser); // use
        if (parser.current->type == HARE_TOKEN_IDENTIFIER) {
            hare_parser_next(&parser); // module name
        }
        // Handle ::* patterns in use statements
        while (parser.current->type == HARE_TOKEN_DCOLON) {
            hare_parser_next(&parser); // ::
            if (parser.current->type == HARE_TOKEN_IDENTIFIER) {
                hare_parser_next(&parser); // nested module name
            } else if (parser.current->type == HARE_TOKEN_ASTERISK) {
                hare_parser_next(&parser); // *
            }
        }
        if (parser.current->type == HARE_TOKEN_SEMICOLON) {
            hare_parser_next(&parser);
        }
    }

    // Parse main function
    if (!hare_parse_main(&parser)) {
        hare_parser_free(&parser);
        return NULL;
    }

    // Validate we reached EOF (trailing semicolons are OK)
    while (parser.current->type == HARE_TOKEN_SEMICOLON) {
        hare_parser_next(&parser);
    }
    if (parser.current->type != HARE_TOKEN_EOF) {
        set_error("Unexpected token at end of input");
        hare_parser_free(&parser);
        return NULL;
    }

    char* result = json_finish(&parser.json);
    hare_parser_free(&parser);
    return result;
}

char* ir_hare_file_to_kir(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        char err[256];
        snprintf(err, sizeof(err), "Failed to open file: %s", filepath);
        set_error(err);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        set_error("Memory allocation failed");
        return NULL;
    }

    size_t read = fread(source, 1, size, f);
    source[read] = '\0';
    fclose(f);

    char* result = ir_hare_to_kir(source, read);
    free(source);
    return result;
}
