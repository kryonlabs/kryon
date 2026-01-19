/**
 * Kryon .kry Parser Implementation
 *
 * Tokenizes and parses .kry DSL files into an AST, which is then
 * converted to IR components.
 */

#include "kry_parser.h"
#include "kry_ast.h"
#include "../include/ir_serialization.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

// ============================================================================
// Memory Management
// ============================================================================

void* kry_alloc(KryParser* parser, size_t size) {
    if (!parser) return NULL;

    // Ensure we have a chunk with enough space
    if (!parser->current_chunk || parser->current_chunk->used + size > KRY_CHUNK_SIZE) {
        KryChunk* chunk = (KryChunk*)malloc(sizeof(KryChunk));
        if (!chunk) return NULL;

        chunk->used = 0;
        chunk->next = NULL;

        if (!parser->first_chunk) {
            parser->first_chunk = chunk;
        } else {
            parser->current_chunk->next = chunk;
        }
        parser->current_chunk = chunk;
    }

    void* ptr = parser->current_chunk->data + parser->current_chunk->used;
    parser->current_chunk->used += size;
    return ptr;
}

char* kry_strdup(KryParser* parser, const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* copy = (char*)kry_alloc(parser, len + 1);
    if (copy) {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

char* kry_strndup(KryParser* parser, const char* str, size_t len) {
    if (!str) return NULL;
    char* copy = (char*)kry_alloc(parser, len + 1);
    if (copy) {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

// ============================================================================
// Parser Creation/Destruction
// ============================================================================

KryParser* kry_parser_create(const char* source, size_t length) {
    if (!source) return NULL;

    KryParser* parser = (KryParser*)malloc(sizeof(KryParser));
    if (!parser) return NULL;

    parser->source = source;
    parser->length = length > 0 ? length : strlen(source);
    parser->pos = 0;
    parser->line = 1;
    parser->column = 1;
    parser->first_chunk = NULL;
    parser->current_chunk = NULL;
    parser->root = NULL;
    parser->has_error = false;
    parser->error_message = NULL;
    parser->error_line = 0;
    parser->error_column = 0;

    return parser;
}

void kry_parser_free(KryParser* parser) {
    if (!parser) return;

    // Free chunks
    KryChunk* chunk = parser->first_chunk;
    while (chunk) {
        KryChunk* next = chunk->next;
        free(chunk);
        chunk = next;
    }

    // Free error message if allocated separately
    if (parser->error_message &&
        (parser->error_message < (char*)parser->first_chunk->data ||
         parser->error_message >= (char*)parser->current_chunk->data + parser->current_chunk->used)) {
        free(parser->error_message);
    }

    free(parser);
}

// ============================================================================
// Error Handling
// ============================================================================

void kry_parser_error(KryParser* parser, const char* message) {
    if (!parser || parser->has_error) return;

    parser->has_error = true;
    // Use regular malloc for error message
    size_t len = strlen(message);
    parser->error_message = (char*)malloc(len + 1);
    if (parser->error_message) {
        memcpy(parser->error_message, message, len);
        parser->error_message[len] = '\0';
    }
    parser->error_line = parser->line;
    parser->error_column = parser->column;
}

// ============================================================================
// Character Utilities
// ============================================================================

static inline char peek(KryParser* p) {
    return (p->pos < p->length) ? p->source[p->pos] : '\0';
}

static inline char advance(KryParser* p) {
    if (p->pos >= p->length) return '\0';

    char c = p->source[p->pos++];

    if (c == '\n') {
        p->line++;
        p->column = 1;
    } else {
        p->column++;
    }

    return c;
}

static inline bool match(KryParser* p, char expected) {
    if (peek(p) == expected) {
        advance(p);
        return true;
    }
    return false;
}

static void skip_whitespace(KryParser* p) {
    while (peek(p) != '\0') {
        char c = peek(p);

        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(p);
            continue;
        }

        // Skip // comments
        if (c == '/' && p->pos + 1 < p->length && p->source[p->pos + 1] == '/') {
            advance(p);  // Skip first /
            advance(p);  // Skip second /
            while (peek(p) != '\n' && peek(p) != '\0') {
                advance(p);
            }
            continue;
        }

        // Skip /* */ comments
        if (c == '/' && p->pos + 1 < p->length && p->source[p->pos + 1] == '*') {
            advance(p);  // Skip /
            advance(p);  // Skip *
            while (p->pos + 1 < p->length) {
                if (peek(p) == '*' && p->source[p->pos + 1] == '/') {
                    advance(p);  // Skip *
                    advance(p);  // Skip /
                    break;
                }
                advance(p);
            }
            continue;
        }

        break;
    }
}

// ============================================================================
// Node Creation
// ============================================================================

KryNode* kry_node_create(KryParser* parser, KryNodeType type) {
    KryNode* node = (KryNode*)kry_alloc(parser, sizeof(KryNode));
    if (!node) return NULL;

    node->type = type;
    node->parent = NULL;
    node->first_child = NULL;
    node->last_child = NULL;
    node->prev_sibling = NULL;
    node->next_sibling = NULL;
    node->name = NULL;
    node->value = NULL;
    node->is_component_definition = false;
    node->arguments = NULL;
    node->var_type = NULL;
    node->state_type = NULL;
    node->else_branch = NULL;
    node->code_language = NULL;
    node->code_source = NULL;
    node->decorator_type = NULL;
    node->decorator_target = NULL;
    node->decorator_args = NULL;
    node->line = parser->line;
    node->column = parser->column;

    return node;
}

void kry_node_append_child(KryNode* parent, KryNode* child) {
    if (!parent || !child) return;

    child->parent = parent;
    child->next_sibling = NULL;

    if (!parent->first_child) {
        parent->first_child = child;
        parent->last_child = child;
        child->prev_sibling = NULL;
    } else {
        child->prev_sibling = parent->last_child;
        parent->last_child->next_sibling = child;
        parent->last_child = child;
    }
}

// ============================================================================
// Value Creation
// ============================================================================

KryValue* kry_value_create_string(KryParser* parser, const char* str) {
    KryValue* value = (KryValue*)kry_alloc(parser, sizeof(KryValue));
    if (!value) return NULL;

    value->type = KRY_VALUE_STRING;
    value->string_value = kry_strdup(parser, str);
    value->is_percentage = false;
    return value;
}

KryValue* kry_value_create_number(KryParser* parser, double num) {
    KryValue* value = (KryValue*)kry_alloc(parser, sizeof(KryValue));
    if (!value) return NULL;

    value->type = KRY_VALUE_NUMBER;
    value->number_value = num;
    value->is_percentage = false;
    return value;
}

KryValue* kry_value_create_identifier(KryParser* parser, const char* id) {
    KryValue* value = (KryValue*)kry_alloc(parser, sizeof(KryValue));
    if (!value) return NULL;

    value->type = KRY_VALUE_IDENTIFIER;
    value->identifier = kry_strdup(parser, id);
    value->is_percentage = false;
    return value;
}

KryValue* kry_value_create_expression(KryParser* parser, const char* expr) {
    KryValue* value = (KryValue*)kry_alloc(parser, sizeof(KryValue));
    if (!value) return NULL;

    value->type = KRY_VALUE_EXPRESSION;
    value->expression = kry_strdup(parser, expr);
    value->is_percentage = false;
    return value;
}

KryValue* kry_value_create_array(KryParser* parser, KryValue** elements, size_t count) {
    KryValue* value = (KryValue*)kry_alloc(parser, sizeof(KryValue));
    if (!value) return NULL;

    value->type = KRY_VALUE_ARRAY;
    value->array.elements = elements;
    value->array.count = count;
    value->is_percentage = false;
    return value;
}

KryValue* kry_value_create_object(KryParser* parser, char** keys, KryValue** values, size_t count) {
    KryValue* value = (KryValue*)kry_alloc(parser, sizeof(KryValue));
    if (!value) return NULL;

    value->type = KRY_VALUE_OBJECT;
    value->object.keys = keys;
    value->object.values = values;
    value->object.count = count;
    value->is_percentage = false;
    return value;
}

// ============================================================================
// Code Block and Decorator Creation
// ============================================================================

KryNode* kry_node_create_code_block(KryParser* parser, const char* language, const char* source) {
    KryNode* node = kry_node_create(parser, KRY_NODE_CODE_BLOCK);
    if (!node) return NULL;

    node->code_language = kry_strdup(parser, language);
    node->code_source = kry_strdup(parser, source);
    return node;
}

KryNode* kry_node_create_decorator(KryParser* parser, const char* type, const char* target, const char* args) {
    KryNode* node = kry_node_create(parser, KRY_NODE_DECORATOR);
    if (!node) return NULL;

    node->decorator_type = kry_strdup(parser, type);
    node->decorator_target = target ? kry_strdup(parser, target) : NULL;
    node->decorator_args = args ? kry_strdup(parser, args) : NULL;
    return node;
}

// ============================================================================
// Parsing Functions
// ============================================================================

static char* parse_identifier(KryParser* p) {
    size_t start = p->pos;

    while (peek(p) != '\0' && (isalnum(peek(p)) || peek(p) == '_' || peek(p) == '-')) {
        advance(p);
    }

    size_t len = p->pos - start;
    return kry_strndup(p, p->source + start, len);
}

static char* parse_string(KryParser* p) {
    if (!match(p, '"')) {
        kry_parser_error(p, "Expected string literal");
        return NULL;
    }

    size_t start = p->pos;

    while (peek(p) != '"' && peek(p) != '\0') {
        if (peek(p) == '\\') {
            advance(p);  // Skip escape char
            if (peek(p) != '\0') advance(p);  // Skip escaped char
        } else {
            advance(p);
        }
    }

    size_t len = p->pos - start;
    char* str = kry_strndup(p, p->source + start, len);

    if (!match(p, '"')) {
        kry_parser_error(p, "Unterminated string literal");
        return NULL;
    }

    return str;
}

static double parse_number(KryParser* p) {
    size_t start = p->pos;

    // Handle negative numbers
    if (peek(p) == '-') {
        advance(p);
    }

    // Parse integer part
    while (isdigit(peek(p))) {
        advance(p);
    }

    // Parse decimal part
    if (peek(p) == '.') {
        advance(p);
        while (isdigit(peek(p))) {
            advance(p);
        }
    }

    // Skip optional % suffix (for percentage values like 100%)
    if (peek(p) == '%') {
        advance(p);
    }

    size_t len = p->pos - start;
    char* num_str = kry_strndup(p, p->source + start, len);

    // Remove % from the string before parsing
    char clean_num[256];
    size_t clean_len = 0;
    for (size_t i = 0; i < len && i < sizeof(clean_num) - 1; i++) {
        if (num_str[i] != '%') {
            clean_num[clean_len++] = num_str[i];
        }
    }
    clean_num[clean_len] = '\0';

    double value = atof(clean_num);

    return value;
}

static KryValue* parse_value(KryParser* p);  // Forward declaration

static KryValue* parse_array(KryParser* p) {
    if (!match(p, '[')) {
        kry_parser_error(p, "Expected '[' for array literal");
        return NULL;
    }

    skip_whitespace(p);

    // Empty array
    if (peek(p) == ']') {
        advance(p);
        return kry_value_create_array(p, NULL, 0);
    }

    // Parse array elements
    #define MAX_ARRAY_ELEMENTS 256
    KryValue* elements[MAX_ARRAY_ELEMENTS];
    size_t count = 0;

    while (peek(p) != ']' && peek(p) != '\0' && count < MAX_ARRAY_ELEMENTS) {
        // Parse element value
        KryValue* element = parse_value(p);
        if (!element) {
            return NULL;
        }
        elements[count++] = element;

        skip_whitespace(p);

        // Check for comma or end of array
        if (peek(p) == ',') {
            advance(p);
            skip_whitespace(p);
        } else if (peek(p) != ']') {
            kry_parser_error(p, "Expected ',' or ']' in array literal");
            return NULL;
        }
    }

    if (!match(p, ']')) {
        kry_parser_error(p, "Expected ']' to close array literal");
        return NULL;
    }

    // Allocate element array from parser memory
    KryValue** element_array = (KryValue**)kry_alloc(p, sizeof(KryValue*) * count);
    if (!element_array) return NULL;

    for (size_t i = 0; i < count; i++) {
        element_array[i] = elements[i];
    }

    return kry_value_create_array(p, element_array, count);
}

static KryValue* parse_object(KryParser* p) {
    if (!match(p, '{')) {
        kry_parser_error(p, "Expected '{' for object literal");
        return NULL;
    }

    skip_whitespace(p);

    // Check if this is an object literal or expression block
    // Object literal has pattern: identifier/string ':' value
    // Expression block has any other pattern

    // Look ahead to distinguish object from expression
    size_t saved_pos = p->pos;
    uint32_t saved_line = p->line;
    uint32_t saved_column = p->column;

    bool is_object = false;

    // Try to parse as object: look for 'identifier:' or 'string:'
    if (peek(p) == '"' || isalpha(peek(p)) || peek(p) == '_') {
        // Save position before lookahead
        if (peek(p) == '"') {
            // String key
            char* key = parse_string(p);
            skip_whitespace(p);
            if (peek(p) == ':') {
                is_object = true;
            }
        } else {
            // Identifier key
            char* key = parse_identifier(p);
            skip_whitespace(p);
            if (peek(p) == ':') {
                is_object = true;
            }
        }
    }

    // Restore position
    p->pos = saved_pos;
    p->line = saved_line;
    p->column = saved_column;

    // If not an object literal, treat as expression block
    if (!is_object) {
        // Parse as expression block
        size_t start = p->pos - 1;  // Include opening {
        int depth = 1;
        while (depth > 0 && peek(p) != '\0') {
            if (peek(p) == '{') depth++;
            else if (peek(p) == '}') depth--;
            advance(p);
        }

        size_t len = p->pos - start - 1;  // Exclude closing }
        char* expr = kry_strndup(p, p->source + start + 1, len);
        return kry_value_create_expression(p, expr);
    }

    // Empty object
    if (peek(p) == '}') {
        advance(p);
        return kry_value_create_object(p, NULL, NULL, 0);
    }

    // Parse object key-value pairs
    #define MAX_OBJECT_ENTRIES 128
    char* keys[MAX_OBJECT_ENTRIES];
    KryValue* values[MAX_OBJECT_ENTRIES];
    size_t count = 0;

    while (peek(p) != '}' && peek(p) != '\0' && count < MAX_OBJECT_ENTRIES) {
        // Parse key (identifier or string)
        char* key = NULL;
        if (peek(p) == '"') {
            key = parse_string(p);
        } else if (isalpha(peek(p)) || peek(p) == '_') {
            key = parse_identifier(p);
        } else {
            kry_parser_error(p, "Expected property key (identifier or string)");
            return NULL;
        }

        if (!key) return NULL;
        keys[count] = key;

        skip_whitespace(p);

        // Expect ':'
        if (!match(p, ':')) {
            kry_parser_error(p, "Expected ':' after object key");
            return NULL;
        }

        skip_whitespace(p);

        // Parse value
        KryValue* value = parse_value(p);
        if (!value) return NULL;
        values[count] = value;

        count++;

        skip_whitespace(p);

        // Check for comma or end of object
        if (peek(p) == ',') {
            advance(p);
            skip_whitespace(p);
        } else if (peek(p) != '}') {
            kry_parser_error(p, "Expected ',' or '}' in object literal");
            return NULL;
        }
    }

    if (!match(p, '}')) {
        kry_parser_error(p, "Expected '}' to close object literal");
        return NULL;
    }

    // Allocate key and value arrays from parser memory
    char** key_array = (char**)kry_alloc(p, sizeof(char*) * count);
    KryValue** value_array = (KryValue**)kry_alloc(p, sizeof(KryValue*) * count);

    if (!key_array || !value_array) return NULL;

    for (size_t i = 0; i < count; i++) {
        key_array[i] = keys[i];
        value_array[i] = values[i];
    }

    return kry_value_create_object(p, key_array, value_array, count);
}

static KryValue* parse_value(KryParser* p) {
    skip_whitespace(p);

    char c = peek(p);

    // String literal
    if (c == '"') {
        char* str = parse_string(p);
        if (!str) return NULL;
        return kry_value_create_string(p, str);
    }

    // Number
    if (isdigit(c) || c == '-') {
        size_t before_pos = p->pos;
        double num = parse_number(p);

        // Check if a '%' was consumed
        bool is_percentage = (p->pos > before_pos &&
                             p->source[p->pos - 1] == '%');

        KryValue* value = kry_value_create_number(p, num);
        if (value) {
            value->is_percentage = is_percentage;
        }
        return value;
    }

    // Identifier (with optional property access or array indexing)
    if (isalpha(c) || c == '_') {
        size_t start = p->pos;
        char* id = parse_identifier(p);
        if (!id) return NULL;

        // Check for property access (.property) or array indexing ([index])
        skip_whitespace(p);
        if (peek(p) == '.' || peek(p) == '[') {
            // Build full member access expression
            char expr_buffer[512];
            size_t expr_len = 0;

            // Copy initial identifier
            size_t id_len = strlen(id);
            if (id_len < sizeof(expr_buffer)) {
                memcpy(expr_buffer, id, id_len);
                expr_len = id_len;
            }

            // Parse property chains and array indexing
            while ((peek(p) == '.' || peek(p) == '[') && expr_len < sizeof(expr_buffer) - 1) {
                if (peek(p) == '.') {
                    // Property access
                    expr_buffer[expr_len++] = '.';
                    advance(p);
                    skip_whitespace(p);

                    // Parse property name
                    if (!isalpha(peek(p)) && peek(p) != '_') {
                        kry_parser_error(p, "Expected property name after '.'");
                        return NULL;
                    }

                    char* prop = parse_identifier(p);
                    if (!prop) return NULL;

                    size_t prop_len = strlen(prop);
                    if (expr_len + prop_len < sizeof(expr_buffer)) {
                        memcpy(expr_buffer + expr_len, prop, prop_len);
                        expr_len += prop_len;
                    }
                } else if (peek(p) == '[') {
                    // Array indexing
                    expr_buffer[expr_len++] = '[';
                    advance(p);
                    skip_whitespace(p);

                    // Parse index (number or identifier)
                    size_t index_start = p->pos;
                    int bracket_depth = 1;
                    while (bracket_depth > 0 && peek(p) != '\0') {
                        if (peek(p) == '[') bracket_depth++;
                        else if (peek(p) == ']') bracket_depth--;
                        if (bracket_depth > 0) {
                            if (expr_len < sizeof(expr_buffer) - 1) {
                                expr_buffer[expr_len++] = peek(p);
                            }
                            advance(p);
                        }
                    }

                    if (!match(p, ']')) {
                        kry_parser_error(p, "Expected ']' to close array index");
                        return NULL;
                    }

                    if (expr_len < sizeof(expr_buffer) - 1) {
                        expr_buffer[expr_len++] = ']';
                    }
                }

                skip_whitespace(p);
            }

            expr_buffer[expr_len] = '\0';

            // Return as expression (will be evaluated later)
            return kry_value_create_expression(p, expr_buffer);
        }

        // Simple identifier (no member access)
        return kry_value_create_identifier(p, id);
    }

    // Variable reference $variable (shorthand for {variable})
    if (c == '$') {
        advance(p);  // Skip $
        char* var_name = parse_identifier(p);
        if (!var_name) return NULL;
        return kry_value_create_expression(p, var_name);
    }

    // Array literal [...]
    if (c == '[') {
        return parse_array(p);
    }

    // Object literal {...} or expression block { }
    // parse_object will distinguish between them
    if (c == '{') {
        return parse_object(p);
    }

    kry_parser_error(p, "Expected value (string, number, identifier, array, object, or expression)");
    return NULL;
}

static KryNode* parse_property(KryParser* p, char* name) {
    KryNode* prop = kry_node_create(p, KRY_NODE_PROPERTY);
    if (!prop) return NULL;

    prop->name = name;

    skip_whitespace(p);

    if (!match(p, '=')) {
        kry_parser_error(p, "Expected '=' after property name");
        return NULL;
    }

    skip_whitespace(p);

    prop->value = parse_value(p);
    if (!prop->value) {
        return NULL;
    }

    return prop;
}

static KryNode* parse_component(KryParser* p);  // Forward declaration

// Helper function to check if an identifier matches a keyword
static bool keyword_match(const char* id, const char* keyword) {
    return strcmp(id, keyword) == 0;
}

// Parse state declaration: state value: int = initialValue
static KryNode* parse_state_declaration(KryParser* p) {
    skip_whitespace(p);

    // Parse variable name
    char* var_name = parse_identifier(p);
    if (!var_name) return NULL;

    skip_whitespace(p);

    // Expect ':'
    if (!match(p, ':')) {
        kry_parser_error(p, "Expected ':' after state variable name");
        return NULL;
    }

    skip_whitespace(p);

    // Parse type
    char* type_name = parse_identifier(p);
    if (!type_name) return NULL;

    skip_whitespace(p);

    // Parse optional initializer
    KryValue* initial_value = NULL;
    if (match(p, '=')) {
        skip_whitespace(p);
        initial_value = parse_value(p);
        if (!initial_value) return NULL;
    }

    // Create state node
    KryNode* state_node = kry_node_create(p, KRY_NODE_STATE);
    if (!state_node) return NULL;

    state_node->name = var_name;
    state_node->value = initial_value;
    state_node->state_type = type_name;

    return state_node;
}

// Parse variable declaration: const|let|var name = value
static KryNode* parse_var_decl(KryParser* p, const char* var_type) {
    skip_whitespace(p);

    // Parse variable name
    char* var_name = parse_identifier(p);
    if (!var_name) return NULL;

    skip_whitespace(p);

    // Optional type annotation: ": type"
    char* type_annotation = NULL;
    if (peek(p) == ':') {
        match(p, ':');
        skip_whitespace(p);
        type_annotation = parse_identifier(p);
        if (!type_annotation) {
            kry_parser_error(p, "Expected type name after ':'");
            return NULL;
        }
        skip_whitespace(p);
    }

    // Expect '='
    if (!match(p, '=')) {
        kry_parser_error(p, "Expected '=' after variable name");
        return NULL;
    }

    skip_whitespace(p);

    // Parse value
    KryValue* value = parse_value(p);
    if (!value) return NULL;

    // Create variable declaration node
    KryNode* var_node = kry_node_create(p, KRY_NODE_VAR_DECL);
    if (!var_node) return NULL;

    var_node->name = var_name;
    var_node->value = value;
    var_node->var_type = kry_strdup(p, var_type);
    // Store type annotation if provided (for future type checking)
    (void)type_annotation;  // Currently unused, but parsed for forward compatibility

    return var_node;
}

// Forward declaration
static KryNode* parse_component_body(KryParser* p, KryNode* component);

// Parse style block: style selector { property = value; ... }
// Selector can be:
//   - Simple identifier: style primaryButton { ... }
//   - Quoted complex selector: style "header .container" { ... }
static KryNode* parse_style_block(KryParser* p) {
    skip_whitespace(p);

    // Parse selector - can be identifier or quoted string
    char* selector = NULL;

    if (peek(p) == '"') {
        // Quoted complex selector
        advance(p);  // Skip opening quote
        const char* start = p->source + p->pos;
        size_t len = 0;
        while (p->pos < p->length && peek(p) != '"') {
            len++;
            advance(p);
        }
        if (peek(p) != '"') {
            kry_parser_error(p, "Unterminated string in style selector");
            return NULL;
        }
        advance(p);  // Skip closing quote
        selector = kry_strndup(p, start, len);
    } else {
        // Simple identifier selector (like a class name)
        selector = parse_identifier(p);
    }

    if (!selector || !selector[0]) {
        kry_parser_error(p, "Expected selector after 'style'");
        return NULL;
    }

    skip_whitespace(p);

    // Create style block node
    KryNode* style_node = kry_node_create(p, KRY_NODE_STYLE_BLOCK);
    if (!style_node) return NULL;

    style_node->name = selector;  // Store selector in name field

    // Parse body like a component body (will parse properties as children)
    if (!parse_component_body(p, style_node)) {
        return NULL;
    }

    return style_node;
}

// Parse static block: static { ... }
static KryNode* parse_static_block(KryParser* p) {
    skip_whitespace(p);

    // Create static block node
    KryNode* static_node = kry_node_create(p, KRY_NODE_STATIC_BLOCK);
    if (!static_node) return NULL;

    // Parse body like a component body
    if (!parse_component_body(p, static_node)) {
        return NULL;
    }

    return static_node;
}

// Parse code block: @lua { ... }, @js { ... }
static KryNode* parse_code_block(KryParser* p, const char* language) {
    skip_whitespace(p);

    // Expect opening brace
    if (!match(p, '{')) {
        kry_parser_error(p, "Expected '{' after code block declaration");
        return NULL;
    }

    skip_whitespace(p);

    // Collect source code until closing brace
    size_t start = p->pos;
    int brace_count = 1;
    bool in_string = false;
    char string_char = '\0';

    while (peek(p) != '\0' && brace_count > 0) {
        char c = peek(p);

        // Handle string literals to avoid counting braces inside strings
        if ((c == '"' || c == '\'' || c == '`') && !in_string) {
            in_string = true;
            string_char = c;
            advance(p);
            continue;
        }

        if (in_string) {
            if (c == string_char) {
                // Check for escape character
                if (p->pos > 0 && p->source[p->pos - 1] != '\\') {
                    in_string = false;
                }
            }
            advance(p);
            continue;
        }

        // Count braces (only when not in string)
        if (c == '{') {
            brace_count++;
        } else if (c == '}') {
            brace_count--;
        }

        advance(p);
    }

    if (brace_count > 0) {
        kry_parser_error(p, "Unclosed code block - missing '}'");
        return NULL;
    }

    // Extract source code (without braces)
    size_t end = p->pos - 1;  // -1 to exclude closing brace
    char* source = kry_strndup(p, p->source + start, end - start);

    // Trim leading and trailing whitespace from source
    char* trimmed = source;
    while (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\n' || *trimmed == '\r') {
        trimmed++;
    }

    char* end_trim = trimmed + strlen(trimmed) - 1;
    while (end_trim > trimmed && (*end_trim == ' ' || *end_trim == '\t' ||
           *end_trim == '\n' || *end_trim == '\r')) {
        *end_trim = '\0';
        end_trim--;
    }

    // Create code block node
    KryNode* code_node = kry_node_create_code_block(p, language, trimmed);
    if (!code_node) return NULL;

    fprintf(stderr, "[KRY_PARSE] Parsed code block: language='%s', source_length=%zu\n",
            language, strlen(trimmed));
    fflush(stderr);

    return code_node;
}

// Parse decorator: @reactive, @computed, @action, @watch(path), @context, @use(path)
static KryNode* parse_decorator(KryParser* p, const char* decorator_name) {
    skip_whitespace(p);

    // Check if decorator has arguments (e.g., @watch("state.habits"))
    char* args = NULL;
    if (peek(p) == '(') {
        advance(p);  // Skip '('

        skip_whitespace(p);

        // Parse arguments until closing paren
        size_t start = p->pos;
        int paren_count = 1;
        bool in_string = false;
        char string_char = '\0';

        while (peek(p) != '\0' && paren_count > 0) {
            char c = peek(p);

            // Handle string literals
            if ((c == '"' || c == '\'' || c == '`') && !in_string) {
                in_string = true;
                string_char = c;
                advance(p);
                continue;
            }

            if (in_string) {
                if (c == string_char) {
                    if (p->pos > 0 && p->source[p->pos - 1] != '\\') {
                        in_string = false;
                    }
                }
                advance(p);
                continue;
            }

            if (c == '(') paren_count++;
            if (c == ')') paren_count--;

            advance(p);
        }

        if (paren_count > 0) {
            kry_parser_error(p, "Unclosed decorator arguments - missing ')'");
            return NULL;
        }

        // Extract arguments (without parentheses)
        args = kry_strndup(p, p->source + start, (p->pos - 1) - start);
    }

    // Create decorator node
    KryNode* decorator_node = kry_node_create_decorator(p, decorator_name, NULL, args);
    if (!decorator_node) return NULL;

    fprintf(stderr, "[KRY_PARSE] Parsed decorator: type='%s', args='%s'\n",
            decorator_name, args ? args : "(null)");
    fflush(stderr);

    return decorator_node;
}

// Parse for loop: for item in collection { ... }
static KryNode* parse_for_loop(KryParser* p) {
    skip_whitespace(p);

    // Parse iterator variable name
    char* iter_name = parse_identifier(p);
    if (!iter_name) return NULL;

    skip_whitespace(p);

    // Expect 'in' keyword
    char* in_keyword = parse_identifier(p);
    if (!in_keyword || !keyword_match(in_keyword, "in")) {
        kry_parser_error(p, "Expected 'in' keyword in for loop");
        return NULL;
    }

    skip_whitespace(p);

    // Parse collection expression (can be identifier, array, or expression)
    KryValue* collection = parse_value(p);
    if (!collection) return NULL;

    skip_whitespace(p);

    // Create for loop node
    KryNode* for_node = kry_node_create(p, KRY_NODE_FOR_LOOP);
    if (!for_node) return NULL;

    for_node->name = iter_name;  // Iterator variable name
    for_node->value = collection;  // Collection to iterate over

    // Parse loop body
    if (!parse_component_body(p, for_node)) {
        return NULL;
    }

    return for_node;
}

// Parse for each loop: for each item in collection { ... }
// This creates a runtime ForEach component (IR_COMPONENT_FOR_EACH)
// Unlike 'for', this does NOT expand at compile time
static KryNode* parse_for_each_loop(KryParser* p) {
    skip_whitespace(p);

    // Parse iterator variable name
    char* iter_name = parse_identifier(p);
    if (!iter_name) return NULL;

    skip_whitespace(p);

    // Expect 'in' keyword
    char* in_keyword = parse_identifier(p);
    if (!in_keyword || !keyword_match(in_keyword, "in")) {
        kry_parser_error(p, "Expected 'in' keyword in for each loop");
        return NULL;
    }

    skip_whitespace(p);

    // Parse collection expression (can be identifier, array, or expression)
    KryValue* collection = parse_value(p);
    if (!collection) return NULL;

    skip_whitespace(p);

    // Create for each loop node
    KryNode* for_each_node = kry_node_create(p, KRY_NODE_FOR_EACH);
    if (!for_each_node) return NULL;

    for_each_node->name = iter_name;  // Iterator variable name
    for_each_node->value = collection;  // Collection to iterate over

    // Parse loop body
    if (!parse_component_body(p, for_each_node)) {
        return NULL;
    }

    return for_each_node;
}

// Parse if statement: if condition { ... } else { ... }
static KryNode* parse_if_statement(KryParser* p) {
    skip_whitespace(p);

    // Parse condition expression (identifier or expression)
    KryValue* condition = parse_value(p);
    if (!condition) {
        kry_parser_error(p, "Expected condition after 'if'");
        return NULL;
    }

    skip_whitespace(p);

    // Create if node
    KryNode* if_node = kry_node_create(p, KRY_NODE_IF);
    if (!if_node) return NULL;

    // Store condition as value
    if_node->value = condition;

    // Parse then block
    if (!parse_component_body(p, if_node)) {
        return NULL;
    }

    skip_whitespace(p);

    // Check for else block
    size_t saved_pos = p->pos;
    char* else_keyword = parse_identifier(p);
    if (else_keyword && keyword_match(else_keyword, "else")) {
        skip_whitespace(p);

        // Create else branch node (wrapper for else block children)
        KryNode* else_node = kry_node_create(p, KRY_NODE_COMPONENT);
        if (!else_node) return NULL;
        else_node->name = "ElseBranch";  // Internal marker

        // Parse else block
        if (!parse_component_body(p, else_node)) {
            return NULL;
        }

        if_node->else_branch = else_node;
    } else {
        // No else block, restore position
        p->pos = saved_pos;
        if_node->else_branch = NULL;
    }

    return if_node;
}

static KryNode* parse_component_body(KryParser* p, KryNode* component) {
    skip_whitespace(p);

    if (!match(p, '{')) {
        kry_parser_error(p, "Expected '{' after component name");
        return NULL;
    }

    skip_whitespace(p);

    while (peek(p) != '}' && peek(p) != '\0') {
        skip_whitespace(p);

        if (peek(p) == '}') break;

        // Check for @ symbols (code blocks and decorators)
        if (peek(p) == '@') {
            advance(p);  // Consume '@'

            // Parse the identifier after @
            if (!isalpha(peek(p)) && peek(p) != '_') {
                kry_parser_error(p, "Expected identifier after '@'");
                return NULL;
            }

            char* at_name = parse_identifier(p);
            if (!at_name) return NULL;

            skip_whitespace(p);

            // Check for code blocks (@lua, @js)
            if (keyword_match(at_name, "lua") || keyword_match(at_name, "js")) {

                if (peek(p) == '{') {
                    // It's a code block
                    KryNode* code_block = parse_code_block(p, at_name);
                    if (!code_block) return NULL;
                    kry_node_append_child(component, code_block);
                } else {
                    kry_parser_error(p, "Expected '{' after @lua/@js");
                    return NULL;
                }
            }
            // Check for decorators (@reactive, @computed, @action, @watch, @context, @use)
            else if (keyword_match(at_name, "reactive") || keyword_match(at_name, "computed") ||
                     keyword_match(at_name, "action") || keyword_match(at_name, "watch") ||
                     keyword_match(at_name, "context") || keyword_match(at_name, "use")) {

                KryNode* decorator = parse_decorator(p, at_name);
                if (!decorator) return NULL;
                kry_node_append_child(component, decorator);
            } else {
                kry_parser_error(p, "Unknown @ directive - expected @lua, @js, or decorator");
                return NULL;
            }

            skip_whitespace(p);
            continue;
        }

        // Parse identifier
        if (!isalpha(peek(p)) && peek(p) != '_') {
            kry_parser_error(p, "Expected property or component name");
            return NULL;
        }

        char* name = parse_identifier(p);
        if (!name) return NULL;

        skip_whitespace(p);

        // Check for state declaration
        if (keyword_match(name, "state")) {
            KryNode* state = parse_state_declaration(p);
            if (!state) return NULL;
            kry_node_append_child(component, state);
        }
        // Check for variable declarations (const/let/var)
        else if (keyword_match(name, "const") || keyword_match(name, "let") || keyword_match(name, "var")) {
            KryNode* var_decl = parse_var_decl(p, name);
            if (!var_decl) return NULL;
            kry_node_append_child(component, var_decl);
        }
        // Check for static block
        else if (keyword_match(name, "static")) {
            KryNode* static_block = parse_static_block(p);
            if (!static_block) return NULL;
            kry_node_append_child(component, static_block);
        }
        // Check for for loop / for each loop
        else if (keyword_match(name, "for")) {
            // Look ahead to check if this is "for each" or just "for"
            size_t saved_pos = p->pos;
            uint32_t saved_line = p->line;
            uint32_t saved_column = p->column;

            skip_whitespace(p);
            char* next_id = parse_identifier(p);

            KryNode* loop_node = NULL;
            if (next_id && keyword_match(next_id, "each")) {
                // "for each" - runtime ForEach
                loop_node = parse_for_each_loop(p);
            } else {
                // Just "for" - compile-time for loop
                // Restore position and parse as regular for loop
                p->pos = saved_pos;
                p->line = saved_line;
                p->column = saved_column;
                loop_node = parse_for_loop(p);
            }

            if (!loop_node) return NULL;
            kry_node_append_child(component, loop_node);
        }
        // Check for if statement
        else if (keyword_match(name, "if")) {
            KryNode* if_stmt = parse_if_statement(p);
            if (!if_stmt) return NULL;
            kry_node_append_child(component, if_stmt);
        }
        // Check for style block
        else if (keyword_match(name, "style")) {
            KryNode* style_block = parse_style_block(p);
            if (!style_block) return NULL;
            kry_node_append_child(component, style_block);
        }
        // Check if it's a property (=) or child component ({)
        else if (peek(p) == '=') {
            // Property
            KryNode* prop = parse_property(p, name);
            if (!prop) return NULL;
            kry_node_append_child(component, prop);
        } else if (peek(p) == '{') {
            // Child component
            KryNode* child = kry_node_create(p, KRY_NODE_COMPONENT);
            if (!child) return NULL;
            child->name = name;

            if (!parse_component_body(p, child)) {
                return NULL;
            }

            kry_node_append_child(component, child);
        } else if (peek(p) == '(') {
            // Component instantiation with arguments
            KryNode* child = kry_node_create(p, KRY_NODE_COMPONENT);
            if (!child) return NULL;
            child->name = name;

            // Capture argument string
            match(p, '(');
            size_t arg_start = p->pos;
            int paren_depth = 1;
            while (paren_depth > 0 && peek(p) != '\0') {
                if (peek(p) == '(') paren_depth++;
                else if (peek(p) == ')') paren_depth--;
                if (paren_depth > 0) advance(p);  // Don't advance past final ')'
            }
            size_t arg_length = p->pos - arg_start;

            // Store arguments (empty string if no args)
            if (arg_length > 0) {
                child->arguments = kry_strndup(p, p->source + arg_start, arg_length);
            }

            match(p, ')');  // Consume closing ')'
            skip_whitespace(p);
            kry_node_append_child(component, child);
        } else {
            kry_parser_error(p, "Expected '=', '{', or '(' after identifier");
            return NULL;
        }

        skip_whitespace(p);
    }

    if (!match(p, '}')) {
        kry_parser_error(p, "Expected '}' to close component");
        return NULL;
    }

    return component;
}

static KryNode* parse_component(KryParser* p) {
    fprintf(stderr, "[PARSER] parse_component() called\n");
    fflush(stderr);
    skip_whitespace(p);

    if (!isalpha(peek(p)) && peek(p) != '_') {
        kry_parser_error(p, "Expected component name");
        return NULL;
    }

    char* name = parse_identifier(p);
    if (!name) return NULL;
    fprintf(stderr, "[PARSER] Parsed component name: '%s'\n", name);
    fflush(stderr);

    skip_whitespace(p);

    // Check if this is a component definition
    if (keyword_match(name, "component")) {
        fprintf(stderr, "[PARSER] Detected component definition!\n");
        // Parse component definition name
        if (!isalpha(peek(p)) && peek(p) != '_') {
            kry_parser_error(p, "Expected component definition name after 'component'");
            return NULL;
        }

        char* comp_def_name = parse_identifier(p);
        if (!comp_def_name) return NULL;
        fprintf(stderr, "[PARSER] Component definition name: '%s'\n", comp_def_name);

        skip_whitespace(p);
        fprintf(stderr, "[PARSER] After skip_whitespace, peek() = '%c' (0x%02x)\n",
                peek(p), (unsigned char)peek(p));

        // Parse parameter list if present
        if (peek(p) == '(') {
            fprintf(stderr, "[PARSER] Found '(' after component name, consuming parameters...\n");
            match(p, '(');
            // Consume parameters
            int paren_depth = 1;
            while (paren_depth > 0 && peek(p) != '\0') {
                if (peek(p) == '(') paren_depth++;
                else if (peek(p) == ')') paren_depth--;
                advance(p);
            }
            fprintf(stderr, "[PARSER] After consuming params, peek() = '%c' (0x%02x)\n",
                    peek(p), (unsigned char)peek(p));
            skip_whitespace(p);
            fprintf(stderr, "[PARSER] After skip_whitespace, peek() = '%c' (0x%02x)\n",
                    peek(p), (unsigned char)peek(p));
        }

        // Create component definition node
        KryNode* component = kry_node_create(p, KRY_NODE_COMPONENT);
        if (!component) return NULL;
        component->name = comp_def_name;
        component->is_component_definition = true;

        return parse_component_body(p, component);
    }

    // Regular component
    KryNode* component = kry_node_create(p, KRY_NODE_COMPONENT);
    if (!component) return NULL;

    component->name = name;

    return parse_component_body(p, component);
}

// ============================================================================
// Main Parse Function
// ============================================================================

KryNode* kry_parse(KryParser* parser) {
    if (!parser) return NULL;

    skip_whitespace(parser);

    // Create a root wrapper node to hold top-level declarations and components
    parser->root = kry_node_create(parser, KRY_NODE_COMPONENT);
    if (!parser->root) return NULL;
    parser->root->name = "Root";  // Special root node

    KryNode* current = NULL;

    // Parse top-level declarations (const/let/var/static) and components
    while (peek(parser) != '\0' && !parser->has_error) {
        skip_whitespace(parser);
        if (peek(parser) == '\0') break;

        // Check for @ symbols (code blocks and decorators) at top level
        if (peek(parser) == '@') {
            advance(parser);  // Consume '@'

            // Parse the identifier after @
            if (!isalpha(peek(parser)) && peek(parser) != '_') {
                kry_parser_error(parser, "Expected identifier after '@'");
                break;
            }

            char* at_name = parse_identifier(parser);
            if (!at_name) break;

            skip_whitespace(parser);

            KryNode* at_node = NULL;

            // Check for code blocks (@lua, @js)
            if (keyword_match(at_name, "lua") || keyword_match(at_name, "js")) {

                if (peek(parser) == '{') {
                    at_node = parse_code_block(parser, at_name);
                } else {
                    kry_parser_error(parser, "Expected '{' after @lua/@js");
                    break;
                }
            }
            // Check for decorators (@reactive, @computed, @action, @watch, @context, @use)
            else if (keyword_match(at_name, "reactive") || keyword_match(at_name, "computed") ||
                     keyword_match(at_name, "action") || keyword_match(at_name, "watch") ||
                     keyword_match(at_name, "context") || keyword_match(at_name, "use")) {

                at_node = parse_decorator(parser, at_name);
            } else {
                kry_parser_error(parser, "Unknown @ directive - expected @lua, @js, or decorator");
                break;
            }

            if (!at_node) break;

            // Add to root
            kry_node_append_child(parser->root, at_node);

            fprintf(stderr, "[KRY_PARSE] Added top-level @ node: type='%s'\n", at_name);
            fflush(stderr);

            continue;
        }

        // Check if next character can start a declaration or component (alpha or _)
        if (!isalpha(peek(parser)) && peek(parser) != '_') {
            // Hit something that's not valid
            // Stop parsing top-level declarations
            break;
        }

        // Parse identifier to determine what we're dealing with
        char* id = parse_identifier(parser);
        if (!id) break;

        skip_whitespace(parser);

        KryNode* node = NULL;

        // Check for variable declarations (const/let/var)
        if (keyword_match(id, "const") || keyword_match(id, "let") || keyword_match(id, "var")) {
            node = parse_var_decl(parser, id);
        }
        // Check for static block
        else if (keyword_match(id, "static")) {
            node = parse_static_block(parser);
        }
        // Check for style block at top level
        else if (keyword_match(id, "style")) {
            node = parse_style_block(parser);
        }
        // Check for component definition
        else if (keyword_match(id, "component")) {
            // Parse component definition name
            char* comp_def_name = parse_identifier(parser);
            if (!comp_def_name) {
                kry_parser_error(parser, "Expected component definition name after 'component'");
                break;
            }

            skip_whitespace(parser);

            // Create component definition node
            node = kry_node_create(parser, KRY_NODE_COMPONENT);
            if (!node) break;
            node->name = comp_def_name;
            node->is_component_definition = true;

            // Parse parameter list if present
            if (peek(parser) == '(') {
                match(parser, '(');
                size_t arg_start = parser->pos;
                int paren_depth = 1;
                while (paren_depth > 0 && peek(parser) != '\0') {
                    if (peek(parser) == '(') paren_depth++;
                    else if (peek(parser) == ')') paren_depth--;
                    if (paren_depth > 0) advance(parser);
                }
                size_t arg_length = parser->pos - arg_start;

                if (arg_length > 0) {
                    node->arguments = kry_strndup(parser, parser->source + arg_start, arg_length);
                }

                match(parser, ')');
                skip_whitespace(parser);
            }

            // Parse component body
            if (!parse_component_body(parser, node)) {
                break;
            }
        }
        // Must be a component instantiation
        else {
            // Create component node
            node = kry_node_create(parser, KRY_NODE_COMPONENT);
            if (!node) break;
            node->name = id;

            // Check for component arguments
            if (peek(parser) == '(') {
                match(parser, '(');
                size_t arg_start = parser->pos;
                int paren_depth = 1;
                while (paren_depth > 0 && peek(parser) != '\0') {
                    if (peek(parser) == '(') paren_depth++;
                    else if (peek(parser) == ')') paren_depth--;
                    if (paren_depth > 0) advance(parser);
                }
                size_t arg_length = parser->pos - arg_start;

                if (arg_length > 0) {
                    node->arguments = kry_strndup(parser, parser->source + arg_start, arg_length);
                }

                match(parser, ')');
                skip_whitespace(parser);
            }

            // Parse component body
            if (!parse_component_body(parser, node)) {
                break;
            }
        }

        if (!node || parser->has_error) {
            break;
        }

        // Add node as child of root
        kry_node_append_child(parser->root, node);
        current = node;

        fprintf(stderr, "[KRY_PARSE] Added node: name='%s', is_component_definition=%d\n",
                node->name ? node->name : "(null)", node->is_component_definition);
        fflush(stderr);

        skip_whitespace(parser);
    }

    if (parser->has_error) {
        return NULL;
    }

    // Count children for debug
    uint32_t child_count = 0;
    KryNode* child = parser->root->first_child;
    while (child) {
        child_count++;
        fprintf(stderr, "[KRY_PARSE] Child %u: name='%s', is_component_definition=%d\n",
                child_count, child->name ? child->name : "(null)", child->is_component_definition);
        fflush(stderr);
        child = child->next_sibling;
    }
    fprintf(stderr, "[KRY_PARSE] Total children: %u\n", child_count);
    fflush(stderr);

    // If we only have one child component and no variable declarations, return that directly
    // Otherwise return the root wrapper
    if (parser->root->first_child && !parser->root->first_child->next_sibling &&
        parser->root->first_child->type == KRY_NODE_COMPONENT) {
        fprintf(stderr, "[KRY_PARSE] Returning single child directly\n");
        fflush(stderr);
        return parser->root->first_child;
    }

    fprintf(stderr, "[KRY_PARSE] Returning root wrapper\n");
    fflush(stderr);
    return parser->root;
}

// ============================================================================
// Public API
// ============================================================================

IRComponent* ir_kry_parse(const char* source, size_t length);  // Implemented in ir_kry_to_ir.c
extern char* ir_serialize_json(IRComponent* root, IRReactiveManifest* manifest);  // From ir_json.c

// NOTE: ir_kry_to_kir is now implemented in ir_kry_to_ir.c with manifest support

IRComponent* ir_kry_parse_with_errors(const char* source, size_t length,
                                       char** error_message,
                                       uint32_t* error_line,
                                       uint32_t* error_column) {
    KryParser* parser = kry_parser_create(source, length);
    if (!parser) return NULL;

    kry_parse(parser);

    if (parser->has_error) {
        if (error_message) {
            if (parser->error_message) {
                size_t len = strlen(parser->error_message);
                *error_message = (char*)malloc(len + 1);
                if (*error_message) {
                    memcpy(*error_message, parser->error_message, len);
                    (*error_message)[len] = '\0';
                }
            } else {
                *error_message = NULL;
            }
        }
        if (error_line) *error_line = parser->error_line;
        if (error_column) *error_column = parser->error_column;

        kry_parser_free(parser);
        return NULL;
    }

    // Convert AST to IR (implemented in ir_kry_to_ir.c)
    IRComponent* root = ir_kry_parse(source, length);

    kry_parser_free(parser);
    return root;
}

const char* ir_kry_parser_version(void) {
    return "1.0.0";
}
