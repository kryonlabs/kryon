/**
 * Kryon .kry Parser Implementation
 *
 * Tokenizes and parses .kry DSL files into an AST, which is then
 * converted to IR components.
 */

#include "ir_kry_parser.h"
#include "ir_kry_ast.h"
#include "ir_serialization.h"
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

    // Identifier
    if (isalpha(c) || c == '_') {
        char* id = parse_identifier(p);
        if (!id) return NULL;
        return kry_value_create_identifier(p, id);
    }

    // Expression block { }
    if (c == '{') {
        size_t start = p->pos;
        advance(p);  // Skip {

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

    kry_parser_error(p, "Expected value (string, number, identifier, or expression)");
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

        // Parse identifier
        if (!isalpha(peek(p)) && peek(p) != '_') {
            kry_parser_error(p, "Expected property or component name");
            return NULL;
        }

        char* name = parse_identifier(p);
        if (!name) return NULL;

        skip_whitespace(p);

        // Check if it's a property (=) or child component ({)
        if (peek(p) == '=') {
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
        } else {
            kry_parser_error(p, "Expected '=' or '{' after identifier");
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
    skip_whitespace(p);

    if (!isalpha(peek(p)) && peek(p) != '_') {
        kry_parser_error(p, "Expected component name");
        return NULL;
    }

    char* name = parse_identifier(p);
    if (!name) return NULL;

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

    parser->root = parse_component(parser);

    if (parser->has_error) {
        return NULL;
    }

    return parser->root;
}

// ============================================================================
// Public API
// ============================================================================

IRComponent* ir_kry_parse(const char* source, size_t length);  // Implemented in ir_kry_to_ir.c
extern char* ir_serialize_json_v2(IRComponent* root);  // From ir_json_v2.c

char* ir_kry_to_kir(const char* source, size_t length) {
    IRComponent* root = ir_kry_parse(source, length);
    if (!root) return NULL;

    // Serialize to JSON string
    char* json = ir_serialize_json_v2(root);
    ir_destroy_component(root);

    return json;
}

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
