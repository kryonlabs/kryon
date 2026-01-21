/**
 * Kryon .kry Parser Implementation
 *
 * Tokenizes and parses .kry DSL files into an AST, which is then
 * converted to IR components.
 */

#include "kry_parser.h"
#include "kry_ast.h"
#include "kry_struct_parser.h"
#include "../include/ir_serialization.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

// ============================================================================
// Parser Constants
// ============================================================================

// Buffer sizes for parsing
#define KRY_IDENTIFIER_BUFFER_SIZE 256
#define KRY_STRING_BUFFER_SIZE 512
#define KRY_NUMBER_BUFFER_SIZE 256
#define KRY_EXPRESSION_BUFFER_SIZE 512
#define KRY_ERROR_MESSAGE_BUFFER_SIZE 512

// Collection limits
#define KRY_MAX_ARRAY_ELEMENTS 256
#define KRY_MAX_OBJECT_ENTRIES 128

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

    // Initialize error collection
    parser->errors.first = NULL;
    parser->errors.last = NULL;
    parser->errors.error_count = 0;
    parser->errors.warning_count = 0;
    parser->errors.has_fatal = false;

    // Legacy error fields
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

// Add error to collection
void kry_parser_add_error(
    KryParser* parser,
    KryErrorLevel level,
    KryErrorCategory category,
    const char* message
) {
    if (!parser) return;

    // Allocate error from chunk
    KryError* error = (KryError*)kry_alloc(parser, sizeof(KryError));
    if (!error) return;

    error->level = level;
    error->category = category;
    error->message = kry_strdup(parser, message);
    error->line = parser->line;
    error->column = parser->column;
    error->context = NULL;
    error->next = NULL;

    // Add to list
    if (!parser->errors.first) {
        parser->errors.first = error;
        parser->errors.last = error;
    } else {
        parser->errors.last->next = error;
        parser->errors.last = error;
    }

    // Update counts
    if (level == KRY_ERROR_WARNING) {
        parser->errors.warning_count++;
    } else {
        parser->errors.error_count++;
        if (level == KRY_ERROR_FATAL) {
            parser->errors.has_fatal = true;
        }
    }

    // Update backward compatibility fields (point to first error)
    parser->has_error = (parser->errors.error_count > 0);
    if (!parser->error_message) {
        parser->error_message = error->message;
        parser->error_line = error->line;
        parser->error_column = error->column;
    }
}

// Printf-style error formatting
void kry_parser_add_error_fmt(
    KryParser* parser,
    KryErrorLevel level,
    KryErrorCategory category,
    const char* fmt,
    ...
) {
    if (!parser) return;

    char buffer[KRY_ERROR_MESSAGE_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    kry_parser_add_error(parser, level, category, buffer);
}

// Check if should stop (only on fatal errors)
bool kry_parser_should_stop(KryParser* parser) {
    return parser && parser->errors.has_fatal;
}

// Legacy error function - modified to use new infrastructure
void kry_parser_error(KryParser* parser, const char* message) {
    // Modified to add error instead of stopping
    kry_parser_add_error(parser, KRY_ERROR_ERROR, KRY_ERROR_SYNTAX, message);
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

// Balanced delimiter capture (replaces 6 duplicated depth tracking loops)
typedef struct {
    char open;      // Opening delimiter: '(', '[', '{'
    char close;     // Closing delimiter: ')', ']', '}'
    size_t start;   // Start position (after opening delimiter)
    size_t end;     // End position (before closing delimiter)
} KryBalancedCapture;

// Skip balanced delimiters and return captured range
static bool kry_skip_balanced(KryParser* p, KryBalancedCapture* capture) {
    if (!p || !capture) return false;

    char open = peek(p);
    if (open != '(' && open != '[' && open != '{') return false;

    char close = (open == '(') ? ')' : (open == '[') ? ']' : '}';
    capture->open = open;
    capture->close = close;

    advance(p);  // Skip opening delimiter
    capture->start = p->pos;

    int depth = 1;

    while (depth > 0 && peek(p) != '\0') {
        char c = peek(p);
        if (c == open) depth++;
        else if (c == close) depth--;

        if (depth > 0) advance(p);
    }

    capture->end = p->pos;

    if (depth != 0) {
        kry_parser_add_error_fmt(p, KRY_ERROR_ERROR, KRY_ERROR_SYNTAX,
                                 "Unbalanced '%c' (missing '%c')", open, close);
        return false;
    }

    if (peek(p) == close) advance(p);  // Consume closing delimiter
    return true;
}

// Parser checkpoint for lookahead/backtracking
typedef struct {
    size_t pos;
    uint32_t line;
    uint32_t column;
} KryParserCheckpoint;

static inline KryParserCheckpoint kry_checkpoint_save(KryParser* p) {
    KryParserCheckpoint cp = { p->pos, p->line, p->column };
    return cp;
}

static inline void kry_checkpoint_restore(KryParser* p, KryParserCheckpoint cp) {
    p->pos = cp.pos;
    p->line = cp.line;
    p->column = cp.column;
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
    node->import_module = NULL;
    node->import_name = NULL;
    node->func_name = NULL;
    node->func_return_type = NULL;
    node->func_params = NULL;
    node->param_count = 0;
    node->func_body = NULL;
    node->return_expr = NULL;
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

KryValue* kry_value_create_struct_instance(KryParser* parser, const char* struct_name,
                                            char** field_names, KryValue** field_values, size_t count) {
    KryValue* value = (KryValue*)kry_alloc(parser, sizeof(KryValue));
    if (!value) return NULL;

    value->type = KRY_VALUE_STRUCT_INSTANCE;
    value->struct_instance.struct_name = kry_strdup(parser, struct_name);
    value->struct_instance.field_names = field_names;
    value->struct_instance.field_values = field_values;
    value->struct_instance.field_count = count;
    value->is_percentage = false;
    return value;
}

KryValue* kry_value_create_range(KryParser* parser, KryValue* start, KryValue* end) {
    KryValue* value = (KryValue*)kry_alloc(parser, sizeof(KryValue));
    if (!value) return NULL;

    value->type = KRY_VALUE_RANGE;
    value->range.start = start;
    value->range.end = end;
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

// ============================================================================
// Parsing Functions
// ============================================================================

static char* parse_identifier(KryParser* p) {
    size_t start = p->pos;

    while (peek(p) != '\0' && (isalnum(peek(p)) || peek(p) == '_' || peek(p) == '-')) {
        advance(p);
    }

    size_t len = p->pos - start;
    char* result = kry_strndup(p, p->source + start, len);

    return result;
}

// Peek at the next identifier without consuming it

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
    // BUT NOT if this is the start of a range operator (..)
    if (peek(p) == '.' && p->pos + 1 < p->length && p->source[p->pos + 1] != '.') {
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
    char clean_num[KRY_NUMBER_BUFFER_SIZE];
    size_t clean_len = 0;
    for (size_t i = 0; i < len; i++) {
        if (clean_len >= sizeof(clean_num) - 1) {
            kry_parser_add_error(p, KRY_ERROR_ERROR, KRY_ERROR_BUFFER_OVERFLOW,
                                 "Number literal too long (max 255 characters)");
            clean_num[sizeof(clean_num) - 1] = '\0';
            break;
        }
        if (num_str[i] != '%') {
            clean_num[clean_len++] = num_str[i];
        }
    }
    clean_num[clean_len] = '\0';

    double value = atof(clean_num);

    return value;
}

static KryValue* parse_value(KryParser* p);  // Forward declaration
static bool keyword_match(const char* id, const char* keyword);  // Forward declaration

// ============================================================================
// Expression Component Parsers
// ============================================================================

// Parse property access: .property
static bool parse_property_access_into_buffer(KryParser* p, char* buffer, size_t* pos, size_t max_size) {
    if (peek(p) != '.') return false;

    // Add '.' to buffer
    if (*pos >= max_size - 1) {
        kry_parser_add_error(p, KRY_ERROR_ERROR, KRY_ERROR_BUFFER_OVERFLOW,
                            "Expression too long (max 511 characters)");
        return false;
    }
    buffer[(*pos)++] = '.';
    advance(p);
    skip_whitespace(p);

    // Parse property name
    if (!isalpha(peek(p)) && peek(p) != '_') {
        kry_parser_error(p, "Expected property name after '.'");
        return false;
    }

    char* prop = parse_identifier(p);
    if (!prop) return false;

    size_t prop_len = strlen(prop);
    if (*pos + prop_len >= max_size) {
        kry_parser_add_error(p, KRY_ERROR_ERROR, KRY_ERROR_BUFFER_OVERFLOW,
                            "Expression too long (max 511 characters)");
        return false;
    }

    memcpy(buffer + *pos, prop, prop_len);
    *pos += prop_len;

    return true;
}

// Parse array indexing: [index]
static bool parse_array_indexing_into_buffer(KryParser* p, char* buffer, size_t* pos, size_t max_size) {
    if (peek(p) != '[') return false;

    // Add '[' to buffer
    if (*pos >= max_size - 1) {
        kry_parser_add_error(p, KRY_ERROR_ERROR, KRY_ERROR_BUFFER_OVERFLOW,
                            "Expression too long (max 511 characters)");
        return false;
    }
    buffer[(*pos)++] = '[';
    advance(p);
    skip_whitespace(p);

    // Parse index content (balanced brackets)
    int bracket_depth = 1;
    while (bracket_depth > 0 && peek(p) != '\0') {
        if (peek(p) == '[') bracket_depth++;
        else if (peek(p) == ']') bracket_depth--;

        if (bracket_depth > 0) {
            if (*pos >= max_size - 1) {
                kry_parser_add_error(p, KRY_ERROR_ERROR, KRY_ERROR_BUFFER_OVERFLOW,
                                    "Expression too long (max 511 characters)");
                return false;
            }
            buffer[(*pos)++] = peek(p);
            advance(p);
        }
    }

    if (!match(p, ']')) {
        kry_parser_error(p, "Expected ']' to close array index");
        return false;
    }

    if (*pos < max_size - 1) {
        buffer[(*pos)++] = ']';
    }

    return true;
}

// Parse function call: (args)
static bool parse_function_call_into_buffer(KryParser* p, char* buffer, size_t* pos, size_t max_size) {
    if (peek(p) != '(') return false;

    // Add '(' to buffer
    if (*pos >= max_size - 1) {
        kry_parser_add_error(p, KRY_ERROR_ERROR, KRY_ERROR_BUFFER_OVERFLOW,
                            "Expression too long (max 511 characters)");
        return false;
    }
    buffer[(*pos)++] = '(';
    advance(p);
    skip_whitespace(p);

    // Parse arguments (balanced parentheses)
    int paren_depth = 1;
    while (paren_depth > 0 && peek(p) != '\0') {
        if (peek(p) == '(') paren_depth++;
        else if (peek(p) == ')') paren_depth--;

        if (paren_depth > 0) {
            if (*pos >= max_size - 1) {
                kry_parser_add_error(p, KRY_ERROR_ERROR, KRY_ERROR_BUFFER_OVERFLOW,
                                    "Expression too long (max 511 characters)");
                return false;
            }
            buffer[(*pos)++] = peek(p);
            advance(p);
        }
    }

    if (!match(p, ')')) {
        kry_parser_error(p, "Expected ')' to close function call");
        return false;
    }

    if (*pos < max_size - 1) {
        buffer[(*pos)++] = ')';
    }

    return true;
}

// Build complex expression from identifier and chained operations
static KryValue* parse_complex_expression(KryParser* p, char* id) {
    char expr_buffer[KRY_EXPRESSION_BUFFER_SIZE];
    size_t expr_len = 0;

    // Copy initial identifier
    size_t id_len = strlen(id);
    if (id_len >= sizeof(expr_buffer)) {
        kry_parser_add_error(p, KRY_ERROR_ERROR, KRY_ERROR_BUFFER_OVERFLOW,
                            "Expression too long (max 511 characters)");
        id_len = sizeof(expr_buffer) - 1;
    }
    memcpy(expr_buffer, id, id_len);
    expr_len = id_len;

    // Parse chained operations (., [, ()
    bool success = true;
    while ((peek(p) == '.' || peek(p) == '[' || peek(p) == '(') && success) {
        if (peek(p) == '.') {
            success = parse_property_access_into_buffer(p, expr_buffer, &expr_len, sizeof(expr_buffer));
        } else if (peek(p) == '[') {
            success = parse_array_indexing_into_buffer(p, expr_buffer, &expr_len, sizeof(expr_buffer));
        } else if (peek(p) == '(') {
            success = parse_function_call_into_buffer(p, expr_buffer, &expr_len, sizeof(expr_buffer));
        }

        if (!success) {
            return NULL;
        }

        skip_whitespace(p);
    }

    expr_buffer[expr_len] = '\0';
    return kry_value_create_expression(p, expr_buffer);
}

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
    KryValue* elements[KRY_MAX_ARRAY_ELEMENTS];
    size_t count = 0;

    while (peek(p) != ']' && peek(p) != '\0') {
        // Check array size limit
        if (count >= KRY_MAX_ARRAY_ELEMENTS) {
            kry_parser_add_error_fmt(p, KRY_ERROR_ERROR, KRY_ERROR_LIMIT_EXCEEDED,
                                     "Array exceeds maximum size of %d elements", KRY_MAX_ARRAY_ELEMENTS);
            // Skip to closing bracket
            while (peek(p) != ']' && peek(p) != '\0') {
                advance(p);
            }
            break;
        }

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
            (void)key;  // Unused - lookahead only
            skip_whitespace(p);
            if (peek(p) == ':') {
                is_object = true;
            }
        } else {
            // Identifier key
            char* key = parse_identifier(p);
            (void)key;  // Unused - lookahead only
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
    char* keys[KRY_MAX_OBJECT_ENTRIES];
    KryValue* values[KRY_MAX_OBJECT_ENTRIES];
    size_t count = 0;

    while (peek(p) != '}' && peek(p) != '\0') {
        // Check object size limit
        if (count >= KRY_MAX_OBJECT_ENTRIES) {
            kry_parser_add_error_fmt(p, KRY_ERROR_ERROR, KRY_ERROR_LIMIT_EXCEEDED,
                                     "Object exceeds maximum of %d properties", KRY_MAX_OBJECT_ENTRIES);
            // Skip to closing brace
            while (peek(p) != '}' && peek(p) != '\0') {
                advance(p);
            }
            break;
        }

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

// ============================================================================
// Struct Instantiation Parsing
// ============================================================================

// Parse struct instantiation: StructName { field = value; field2 = value2 }
// This is different from object literals which use ':' as separator
// Struct instantiations use '=' for field assignment and ';' for field separator
static KryValue* parse_struct_instantiation(KryParser* p, char* struct_name) {
    if (!match(p, '{')) {
        kry_parser_error(p, "Expected '{' for struct instantiation");
        return NULL;
    }

    skip_whitespace(p);

    // Empty struct: Habit {}
    if (peek(p) == '}') {
        advance(p);
        return kry_value_create_struct_instance(p, struct_name, NULL, NULL, 0);
    }

    // Parse field assignments
    char* field_names[KRY_MAX_OBJECT_ENTRIES];
    KryValue* field_values[KRY_MAX_OBJECT_ENTRIES];
    size_t count = 0;

    while (peek(p) != '}' && peek(p) != '\0') {
        if (count >= KRY_MAX_OBJECT_ENTRIES) {
            kry_parser_add_error_fmt(p, KRY_ERROR_ERROR, KRY_ERROR_LIMIT_EXCEEDED,
                                     "Struct exceeds maximum of %d fields", KRY_MAX_OBJECT_ENTRIES);
            // Skip to closing brace
            while (peek(p) != '}' && peek(p) != '\0') {
                advance(p);
            }
            break;
        }

        // Parse field name
        char* field_name = parse_identifier(p);
        if (!field_name) {
            kry_parser_error(p, "Expected field name in struct instantiation");
            return NULL;
        }

        skip_whitespace(p);

        // Expect '=' (not ':' like JSON)
        if (!match(p, '=')) {
            kry_parser_error(p, "Expected '=' after field name in struct instantiation");
            return NULL;
        }

        skip_whitespace(p);

        // Parse field value
        KryValue* value = parse_value(p);
        if (!value) {
            return NULL;
        }

        field_names[count] = field_name;
        field_values[count] = value;
        count++;

        skip_whitespace(p);

        // Check for semicolon separator or end (NOT comma like JSON)
        if (peek(p) == ';') {
            advance(p);
            skip_whitespace(p);
        } else if (peek(p) != '}') {
            kry_parser_error(p, "Expected ';' or '}' in struct instantiation");
            return NULL;
        }
    }

    if (!match(p, '}')) {
        kry_parser_error(p, "Expected '}' to close struct instantiation");
        return NULL;
    }

    // Allocate field name and value arrays from parser memory
    char** name_array = NULL;
    KryValue** value_array = NULL;

    if (count > 0) {
        name_array = (char**)kry_alloc(p, sizeof(char*) * count);
        value_array = (KryValue**)kry_alloc(p, sizeof(KryValue*) * count);

        if (!name_array || !value_array) return NULL;

        for (size_t i = 0; i < count; i++) {
            name_array[i] = field_names[i];
            value_array[i] = field_values[i];
        }
    }

    return kry_value_create_struct_instance(p, struct_name, name_array, value_array, count);
}

// ============================================================================
// Expression Text Parsing (for native operator support in functions)
// ============================================================================

// Parse expression text from current position until statement boundary
// This captures expressions like "count + 1" or "x * 2 + y" as text
// which will be parsed by kry_expr_parse() in the IR conversion phase
static KryValue* parse_expression_text(KryParser* p) {
    skip_whitespace(p);

    size_t start = p->pos;
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;
    bool in_string = false;
    char string_char = 0;

    // Capture until we hit a statement boundary (newline, }, or end of file)
    // while respecting nested structures
    while (peek(p) != '\0') {
        char c = peek(p);

        // Handle string literals
        if (!in_string && (c == '"' || c == '\'')) {
            in_string = true;
            string_char = c;
            advance(p);
            continue;
        }
        if (in_string) {
            if (c == '\\') {
                advance(p);  // Skip escape char
                if (peek(p) != '\0') advance(p);  // Skip escaped char
            } else if (c == string_char) {
                in_string = false;
                advance(p);  // Skip closing quote
            } else {
                advance(p);
            }
            continue;
        }

        // Track nested structures
        if (c == '(') { paren_depth++; advance(p); continue; }
        if (c == '[') { bracket_depth++; advance(p); continue; }
        // For '{', stop at depth 0 (for conditions like `if !x {`)
        if (c == '{') {
            if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
                break;  // Unmatched { at depth 0 - end of expression
            }
            brace_depth++; advance(p); continue;
        }

        if (c == ')') {
            if (paren_depth > 0) { paren_depth--; advance(p); continue; }
            break;  // Unmatched ) - end of expression
        }
        if (c == ']') {
            if (bracket_depth > 0) { bracket_depth--; advance(p); continue; }
            break;  // Unmatched ] - end of expression
        }
        if (c == '}') {
            if (brace_depth > 0) { brace_depth--; advance(p); continue; }
            break;  // Unmatched } - end of statement block
        }

        // Statement boundaries (only at depth 0)
        if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
            if (c == '\n' || c == ';') {
                break;
            }
        }

        advance(p);
    }

    size_t len = p->pos - start;

    // Trim trailing whitespace
    while (len > 0 && (p->source[start + len - 1] == ' ' ||
                       p->source[start + len - 1] == '\t' ||
                       p->source[start + len - 1] == '\r')) {
        len--;
    }

    if (len == 0) {
        return NULL;
    }

    char* expr_text = kry_strndup(p, p->source + start, len);
    return kry_value_create_expression(p, expr_text);
}

// Check if the next characters form a binary operator
static bool peek_is_binary_operator(KryParser* p) {
    char c = peek(p);

    // Single-char operators (including ? for ternary)
    if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
        c == '<' || c == '>' || c == '?') {
        return true;
    }

    // Two-char operators: ==, !=, <=, >=, &&, ||
    if (p->pos + 1 < p->length) {
        char c2 = p->source[p->pos + 1];
        if ((c == '=' && c2 == '=') ||
            (c == '!' && c2 == '=') ||
            (c == '<' && c2 == '=') ||
            (c == '>' && c2 == '=') ||
            (c == '&' && c2 == '&') ||
            (c == '|' && c2 == '|')) {
            return true;
        }
    }

    return false;
}

// Check if current position starts with a unary operator
// Returns: length of operator if found, 0 otherwise
static int peek_is_unary_operator(KryParser* p) {
    if (p->pos >= p->length) return 0;

    char c = peek(p);

    // ! operator (but not !=)
    if (c == '!') {
        char next = (p->pos + 1 < p->length) ? p->source[p->pos + 1] : '\0';
        if (next != '=') return 1;
        return 0;
    }

    // - operator as unary (not -= or ->)
    // Note: Negative numbers are handled separately in parse_value
    // Only treat as unary if followed by identifier or (
    if (c == '-') {
        char next = (p->pos + 1 < p->length) ? p->source[p->pos + 1] : '\0';
        if (next == '=' || next == '>') return 0;  // -= or ->
        if (isdigit(next) || next == '.') return 0;  // Negative number, handled elsewhere
        if (isalpha(next) || next == '_' || next == '(' || next == '!') return 1;
        return 0;
    }

    // + operator as unary (not += or ++)
    if (c == '+') {
        char next = (p->pos + 1 < p->length) ? p->source[p->pos + 1] : '\0';
        if (next == '=' || next == '+') return 0;  // += or ++
        if (isalpha(next) || next == '_' || next == '(' || next == '+') return 1;
        return 0;
    }

    // typeof keyword
    if (c == 't' && p->pos + 6 <= p->length) {
        if (strncmp(p->source + p->pos, "typeof", 6) == 0) {
            char after = (p->pos + 6 < p->length) ? p->source[p->pos + 6] : '\0';
            if (!isalnum(after) && after != '_') {
                return 6;
            }
        }
    }

    return 0;
}

// Parse arrow function text from current position
// Captures patterns like: () => expr, (a, b) => expr, x => expr, () => { block }
static KryValue* parse_arrow_function_text(KryParser* p) {
    skip_whitespace(p);

    size_t start = p->pos;
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;
    bool in_string = false;
    char string_char = 0;

    // First, consume the parameter part
    char c = peek(p);
    if (c == '(') {
        // Parenthesized params: () or (a, b, ...)
        advance(p);  // Skip '('
        paren_depth = 1;
        while (paren_depth > 0 && peek(p) != '\0') {
            c = peek(p);
            if (c == '(') paren_depth++;
            else if (c == ')') paren_depth--;
            advance(p);
        }
    } else if (isalpha(c) || c == '_' || c == '$') {
        // Single identifier param: x => expr
        while (isalnum(peek(p)) || peek(p) == '_' || peek(p) == '$') {
            advance(p);
        }
    }

    skip_whitespace(p);

    // Expect '=>'
    if (peek(p) != '=' || (p->pos + 1 < p->length && p->source[p->pos + 1] != '>')) {
        // Not an arrow function, this shouldn't happen if caller checked properly
        return NULL;
    }
    advance(p);  // Skip '='
    advance(p);  // Skip '>'

    skip_whitespace(p);

    // Now parse the body
    c = peek(p);
    if (c == '{') {
        // Block body: capture until matching }
        advance(p);  // Skip '{'
        brace_depth = 1;
        while (brace_depth > 0 && peek(p) != '\0') {
            c = peek(p);

            // Handle string literals
            if (!in_string && (c == '"' || c == '\'')) {
                in_string = true;
                string_char = c;
                advance(p);
                continue;
            }
            if (in_string) {
                if (c == '\\') {
                    advance(p);  // Skip escape char
                    if (peek(p) != '\0') advance(p);  // Skip escaped char
                } else if (c == string_char) {
                    in_string = false;
                    advance(p);
                } else {
                    advance(p);
                }
                continue;
            }

            if (c == '{') brace_depth++;
            else if (c == '}') brace_depth--;
            advance(p);
        }
    } else {
        // Expression body: capture until statement boundary
        paren_depth = 0;
        bracket_depth = 0;
        brace_depth = 0;

        while (peek(p) != '\0') {
            c = peek(p);

            // Handle string literals
            if (!in_string && (c == '"' || c == '\'')) {
                in_string = true;
                string_char = c;
                advance(p);
                continue;
            }
            if (in_string) {
                if (c == '\\') {
                    advance(p);
                    if (peek(p) != '\0') advance(p);
                } else if (c == string_char) {
                    in_string = false;
                    advance(p);
                } else {
                    advance(p);
                }
                continue;
            }

            // Track nested structures
            if (c == '(') { paren_depth++; advance(p); continue; }
            if (c == '[') { bracket_depth++; advance(p); continue; }
            if (c == '{') { brace_depth++; advance(p); continue; }

            if (c == ')') {
                if (paren_depth > 0) { paren_depth--; advance(p); continue; }
                break;  // End of expression (probably function call arg)
            }
            if (c == ']') {
                if (bracket_depth > 0) { bracket_depth--; advance(p); continue; }
                break;
            }
            if (c == '}') {
                if (brace_depth > 0) { brace_depth--; advance(p); continue; }
                break;
            }

            // Statement boundaries (only at depth 0)
            if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
                if (c == '\n' || c == ';' || c == ',') {
                    break;
                }
            }

            advance(p);
        }
    }

    size_t len = p->pos - start;

    // Trim trailing whitespace
    while (len > 0 && (p->source[start + len - 1] == ' ' ||
                       p->source[start + len - 1] == '\t' ||
                       p->source[start + len - 1] == '\r')) {
        len--;
    }

    if (len == 0) {
        return NULL;
    }

    char* expr_text = kry_strndup(p, p->source + start, len);
    return kry_value_create_expression(p, expr_text);
}

static KryValue* parse_value(KryParser* p) {
    skip_whitespace(p);

    // Check for unary operators - capture full expression for later parsing
    if (peek_is_unary_operator(p) > 0) {
        return parse_expression_text(p);
    }

    char c = peek(p);

    // Check for arrow function patterns: () => expr, (a, b) => expr
    if (c == '(') {
        KryParserCheckpoint saved = kry_checkpoint_save(p);
        advance(p);  // Skip '('
        skip_whitespace(p);

        // Check for () => pattern (parameterless)
        if (peek(p) == ')') {
            advance(p);  // Skip ')'
            skip_whitespace(p);
            if (peek(p) == '=' && p->pos + 1 < p->length && p->source[p->pos + 1] == '>') {
                // This is an arrow function, capture everything
                kry_checkpoint_restore(p, saved);
                return parse_arrow_function_text(p);
            }
        } else {
            // Check for (a, b, ...) => pattern
            // Try to parse as comma-separated identifiers
            bool valid_params = true;
            while (valid_params && peek(p) != ')' && peek(p) != '\0') {
                skip_whitespace(p);
                char ch = peek(p);
                if (isalpha(ch) || ch == '_' || ch == '$') {
                    // Skip identifier
                    while (isalnum(peek(p)) || peek(p) == '_' || peek(p) == '$') {
                        advance(p);
                    }
                    skip_whitespace(p);
                    // Check for comma or end
                    if (peek(p) == ',') {
                        advance(p);
                    } else if (peek(p) != ')') {
                        valid_params = false;
                    }
                } else {
                    valid_params = false;
                }
            }
            if (valid_params && peek(p) == ')') {
                advance(p);  // Skip ')'
                skip_whitespace(p);
                if (peek(p) == '=' && p->pos + 1 < p->length && p->source[p->pos + 1] == '>') {
                    // This is an arrow function
                    kry_checkpoint_restore(p, saved);
                    return parse_arrow_function_text(p);
                }
            }
        }
        // Not an arrow function, restore and continue
        kry_checkpoint_restore(p, saved);

        // Handle parenthesized expressions: (expr)
        advance(p);  // Skip '('
        skip_whitespace(p);

        // Parse the inner expression
        KryValue* inner = parse_value(p);
        if (!inner) {
            kry_parser_error(p, "Expected expression inside parentheses");
            return NULL;
        }

        skip_whitespace(p);
        if (!match(p, ')')) {
            kry_parser_error(p, "Expected ')' after expression");
            return NULL;
        }

        return inner;  // Return the inner value
    }

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

        KryValue* start_value = kry_value_create_number(p, num);
        if (start_value) {
            start_value->is_percentage = is_percentage;
        }

        // Check for range syntax: 0..n
        if (peek(p) == '.' && p->pos + 1 < p->length && p->source[p->pos + 1] == '.') {
            advance(p);  // Skip first '.'
            advance(p);  // Skip second '.'

            // Parse end value (can be number, identifier, or expression)
            KryValue* end_value = parse_value(p);
            if (!end_value) {
                kry_parser_error(p, "Expected value after '..' in range expression");
                return NULL;
            }
            return kry_value_create_range(p, start_value, end_value);
        }

        return start_value;
    }

    // Identifier (with optional function call, property access, array indexing, or struct instantiation)
    if (isalpha(c) || c == '_') {
        // Check for single-param arrow function: identifier => expr
        KryParserCheckpoint saved_id = kry_checkpoint_save(p);
        char* id = parse_identifier(p);
        if (!id) return NULL;

        // Check for arrow function: identifier => expr
        skip_whitespace(p);
        if (peek(p) == '=' && p->pos + 1 < p->length && p->source[p->pos + 1] == '>') {
            // This is a single-param arrow function, restore and parse as full arrow function
            kry_checkpoint_restore(p, saved_id);
            return parse_arrow_function_text(p);
        }

        // Check for function call, property access (.property), or array indexing ([index])
        if (peek(p) == '.' || peek(p) == '[' || peek(p) == '(') {
            // Build full expression (delegate to helper)
            return parse_complex_expression(p, id);
        }

        // Struct instantiation: StructName { field = value; field2 = value2 }
        // Only recognize as struct instantiation if we see { identifier = pattern
        // This prevents confusion with block bodies like: for x in items { ... }
        if (peek(p) == '{') {
            // Save position to peek ahead
            KryParserCheckpoint saved = kry_checkpoint_save(p);
            advance(p);  // Skip '{'
            skip_whitespace(p);

            // Check if this looks like a struct instantiation (field = value pattern)
            // or just a block body (keywords like if, for, var, return, etc.)
            bool is_struct = false;
            char c = peek(p);
            if (c == '}') {
                // Empty struct: StructName {}
                is_struct = true;
            } else if (isalpha(c) || c == '_') {
                // Peek at the identifier
                char peek_id[256];
                size_t i = 0;
                while (i < 255 && (isalnum(peek(p)) || peek(p) == '_')) {
                    peek_id[i++] = peek(p);
                    advance(p);
                }
                peek_id[i] = '\0';

                // Check if it's a keyword that starts a statement (not struct field)
                bool is_keyword = keyword_match(peek_id, "if") ||
                                  keyword_match(peek_id, "for") ||
                                  keyword_match(peek_id, "var") ||
                                  keyword_match(peek_id, "return") ||
                                  keyword_match(peek_id, "while") ||
                                  keyword_match(peek_id, "func") ||
                                  keyword_match(peek_id, "component") ||
                                  keyword_match(peek_id, "struct") ||
                                  keyword_match(peek_id, "import");

                if (!is_keyword) {
                    skip_whitespace(p);
                    // Struct fields use '=' for assignment
                    if (peek(p) == '=') {
                        is_struct = true;
                    }
                }
            }

            // Restore and proceed based on detection
            kry_checkpoint_restore(p, saved);

            if (is_struct) {
                return parse_struct_instantiation(p, id);
            }
        }

        // Simple identifier (no function call, property access, or array indexing)
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

    // Save position to potentially re-parse as full expression
    KryParserCheckpoint saved = kry_checkpoint_save(p);

    // Try parsing as simple value first
    prop->value = parse_value(p);
    if (!prop->value) {
        return NULL;
    }

    // Check if there's a binary operator following - if so, this is a full expression
    skip_whitespace(p);
    if (peek_is_binary_operator(p)) {
        // Restore position and parse as full expression
        kry_checkpoint_restore(p, saved);
        prop->value = parse_expression_text(p);
        if (!prop->value) {
            kry_parser_error(p, "Failed to parse expression");
            return NULL;
        }
    }

    return prop;
}

// Parse property access assignment or method call: object.prop1.prop2 = value OR object.method(args)
static KryNode* parse_property_access_assignment(KryParser* p, char* base_name) {
    // Build the full property access expression (e.g., "habit.completions" or "habits.push")
    char buffer[1024];
    size_t len = strlen(base_name);
    if (len >= sizeof(buffer) - 1) {
        kry_parser_error(p, "Property access path too long");
        return NULL;
    }
    memcpy(buffer, base_name, len);

    // Consume dot-separated property chain
    while (peek(p) == '.') {
        advance(p);  // consume '.'
        if (len >= sizeof(buffer) - 1) {
            kry_parser_error(p, "Property access path too long");
            return NULL;
        }
        buffer[len++] = '.';

        char* prop = parse_identifier(p);
        if (!prop) {
            kry_parser_error(p, "Expected property name after '.'");
            return NULL;
        }
        size_t prop_len = strlen(prop);
        if (len + prop_len >= sizeof(buffer) - 1) {
            kry_parser_error(p, "Property access path too long");
            return NULL;
        }
        memcpy(buffer + len, prop, prop_len);
        len += prop_len;

        skip_whitespace(p);
    }
    buffer[len] = '\0';

    // Check for method call: object.method(args)
    skip_whitespace(p);
    if (peek(p) == '(') {
        // This is a method call statement - capture the full expression
        KryBalancedCapture args;
        if (kry_skip_balanced(p, &args)) {
            // Build the full method call expression
            size_t arg_length = args.end - args.start;
            size_t total_len = len + 1 + arg_length + 1;  // buffer + "(" + args + ")"
            if (total_len >= sizeof(buffer) - 1) {
                kry_parser_error(p, "Method call expression too long");
                return NULL;
            }
            buffer[len++] = '(';
            memcpy(buffer + len, p->source + args.start, arg_length);
            len += arg_length;
            buffer[len++] = ')';
            buffer[len] = '\0';
        }

        // Create an expression node for the method call statement
        KryNode* node = kry_node_create(p, KRY_NODE_EXPRESSION);
        if (!node) return NULL;

        // Store the full method call expression
        KryValue* expr_value = kry_value_create_expression(p, buffer);
        if (!expr_value) return NULL;
        node->value = expr_value;

        return node;
    }

    // Expect '=' for assignment
    if (!match(p, '=')) {
        kry_parser_error(p, "Expected '=' after property access");
        return NULL;
    }

    // Parse value using the same logic as parse_property
    skip_whitespace(p);

    // Save position to potentially re-parse as full expression
    KryParserCheckpoint saved = kry_checkpoint_save(p);

    // Try parsing as simple value first
    KryValue* value = parse_value(p);
    if (!value) {
        return NULL;
    }

    // Check if there's a binary operator following - if so, this is a full expression
    skip_whitespace(p);
    if (peek_is_binary_operator(p)) {
        // Restore position and parse as full expression
        kry_checkpoint_restore(p, saved);
        value = parse_expression_text(p);
        if (!value) {
            kry_parser_error(p, "Failed to parse expression");
            return NULL;
        }
    }

    // Create assignment node
    KryNode* node = kry_node_create(p, KRY_NODE_PROPERTY);
    if (!node) return NULL;

    node->name = kry_strdup(p, buffer);
    node->value = value;

    return node;
}

// Parse indexed assignment: base_name[expr].prop[expr] = value
// Handles chains like: arr[i] = val, arr[i].prop = val, obj.arr[i][j] = val
static KryNode* parse_indexed_assignment(KryParser* p, const char* base_name) {
    char buffer[1024];
    size_t len = strlen(base_name);
    if (len >= sizeof(buffer) - 1) {
        kry_parser_error(p, "Indexed access path too long");
        return NULL;
    }
    memcpy(buffer, base_name, len);

    // Parse chain of [expr] and .prop
    while (peek(p) == '[' || peek(p) == '.') {
        if (peek(p) == '[') {
            // Capture bracket content using balanced delimiter capture
            KryBalancedCapture bracket;
            if (!kry_skip_balanced(p, &bracket)) {
                return NULL;
            }

            // Append [content] to buffer
            size_t content_len = bracket.end - bracket.start;
            if (len + 2 + content_len >= sizeof(buffer) - 1) {
                kry_parser_error(p, "Indexed access path too long");
                return NULL;
            }
            buffer[len++] = '[';
            memcpy(buffer + len, p->source + bracket.start, content_len);
            len += content_len;
            buffer[len++] = ']';

            skip_whitespace(p);
        } else if (peek(p) == '.') {
            advance(p);  // consume '.'
            if (len >= sizeof(buffer) - 1) {
                kry_parser_error(p, "Indexed access path too long");
                return NULL;
            }
            buffer[len++] = '.';

            char* prop = parse_identifier(p);
            if (!prop) {
                kry_parser_error(p, "Expected property name after '.'");
                return NULL;
            }
            size_t prop_len = strlen(prop);
            if (len + prop_len >= sizeof(buffer) - 1) {
                kry_parser_error(p, "Indexed access path too long");
                return NULL;
            }
            memcpy(buffer + len, prop, prop_len);
            len += prop_len;

            skip_whitespace(p);
        }
    }
    buffer[len] = '\0';

    // Check for method call: obj[i].method(args)
    skip_whitespace(p);
    if (peek(p) == '(') {
        KryBalancedCapture args;
        if (kry_skip_balanced(p, &args)) {
            size_t arg_length = args.end - args.start;
            size_t total_len = len + 1 + arg_length + 1;
            if (total_len >= sizeof(buffer) - 1) {
                kry_parser_error(p, "Method call expression too long");
                return NULL;
            }
            buffer[len++] = '(';
            memcpy(buffer + len, p->source + args.start, arg_length);
            len += arg_length;
            buffer[len++] = ')';
            buffer[len] = '\0';
        }

        // Create an expression node for the method call statement
        KryNode* node = kry_node_create(p, KRY_NODE_EXPRESSION);
        if (!node) return NULL;

        KryValue* expr_value = kry_value_create_expression(p, buffer);
        if (!expr_value) return NULL;
        node->value = expr_value;

        return node;
    }

    // Expect '=' for assignment
    if (!match(p, '=')) {
        kry_parser_error(p, "Expected '=' after indexed expression");
        return NULL;
    }

    // Parse value using the same logic as parse_property
    skip_whitespace(p);

    // Save position to potentially re-parse as full expression
    KryParserCheckpoint saved = kry_checkpoint_save(p);

    // Try parsing as simple value first
    KryValue* value = parse_value(p);
    if (!value) {
        return NULL;
    }

    // Check if there's a binary operator following - if so, this is a full expression
    skip_whitespace(p);
    if (peek_is_binary_operator(p)) {
        // Restore position and parse as full expression
        kry_checkpoint_restore(p, saved);
        value = parse_expression_text(p);
        if (!value) {
            kry_parser_error(p, "Failed to parse expression");
            return NULL;
        }
    }

    // Create assignment node
    KryNode* node = kry_node_create(p, KRY_NODE_PROPERTY);
    if (!node) return NULL;

    node->name = kry_strdup(p, buffer);
    node->value = value;

    return node;
}

// Helper function to check if an identifier matches a keyword
static bool keyword_match(const char* id, const char* keyword) {
    if (!id || !keyword) {
        return false;
    }

    return strcmp(id, keyword) == 0;
}

// ============================================================================
// Keyword Dispatch Table
// ============================================================================

// Forward declarations for keyword handlers
static KryNode* parse_var_decl(KryParser* p, const char* keyword);
static KryNode* parse_static_block(KryParser* p);
static KryNode* parse_function_declaration(KryParser* p);
static KryNode* parse_return_statement(KryParser* p);
static KryNode* parse_delete_statement(KryParser* p);
static KryNode* parse_indexed_assignment(KryParser* p, const char* base_name);
static KryNode* parse_for_loop(KryParser* p);
static KryNode* parse_if_statement(KryParser* p);
static KryNode* parse_style_block(KryParser* p);
static KryNode* parse_import_statement(KryParser* p);
KryNode* kry_parse_struct_declaration(KryParser* p);  // External function

// Keyword handler function type
typedef KryNode* (*KryKeywordHandler)(KryParser* p, const char* keyword);

// Keyword dispatch entry
typedef struct {
    const char* keyword;
    KryKeywordHandler handler;
    bool needs_keyword_arg;  // true if handler needs the keyword string
} KryKeywordDispatch;

// Wrapper functions for handlers that don't need keyword argument
static KryNode* handle_static(KryParser* p, const char* keyword) {
    (void)keyword;
    return parse_static_block(p);
}

static KryNode* handle_func(KryParser* p, const char* keyword) {
    (void)keyword;
    return parse_function_declaration(p);
}

static KryNode* handle_return(KryParser* p, const char* keyword) {
    (void)keyword;
    return parse_return_statement(p);
}

static KryNode* handle_for(KryParser* p, const char* keyword) {
    (void)keyword;
    return parse_for_loop(p);
}

static KryNode* handle_if(KryParser* p, const char* keyword) {
    (void)keyword;
    return parse_if_statement(p);
}

static KryNode* handle_style(KryParser* p, const char* keyword) {
    (void)keyword;
    return parse_style_block(p);
}

static KryNode* handle_import(KryParser* p, const char* keyword) {
    (void)keyword;
    return parse_import_statement(p);
}

static KryNode* handle_struct(KryParser* p, const char* keyword) {
    (void)keyword;
    return kry_parse_struct_declaration(p);
}

static KryNode* handle_delete(KryParser* p, const char* keyword) {
    (void)keyword;
    return parse_delete_statement(p);
}

static KryNode* handle_var_decl(KryParser* p, const char* keyword) {
    return parse_var_decl(p, keyword);
}

// Keyword dispatch table (sorted alphabetically for potential binary search)
static const KryKeywordDispatch kry_keyword_table[] = {
    {"const", handle_var_decl, true},
    {"delete", handle_delete, false},
    {"for", handle_for, false},
    {"func", handle_func, false},
    {"if", handle_if, false},
    {"import", handle_import, false},
    {"let", handle_var_decl, true},
    {"return", handle_return, false},
    {"static", handle_static, false},
    {"struct", handle_struct, false},
    {"style", handle_style, false},
    {"var", handle_var_decl, true},
    {NULL, NULL, false}  // Sentinel
};

// Dispatch keyword to appropriate handler
// Returns: node on success, NULL if not a keyword or handler failed
// Sets *handler_failed = true if keyword matched but handler returned NULL
static KryNode* dispatch_keyword(KryParser* p, const char* keyword, bool* handler_failed) {
    if (handler_failed) *handler_failed = false;
    if (!keyword) return NULL;

    for (int i = 0; kry_keyword_table[i].keyword != NULL; i++) {
        if (keyword_match(keyword, kry_keyword_table[i].keyword)) {
            fprintf(stderr, "[DISPATCH] Matched keyword '%s', calling handler\n", keyword);
            KryNode* result = kry_keyword_table[i].handler(p, keyword);
            fprintf(stderr, "[DISPATCH] Handler returned %s for '%s'\n", result ? "node" : "NULL", keyword);
            if (!result && handler_failed) {
                *handler_failed = true;
            }
            return result;
        }
    }

    fprintf(stderr, "[DISPATCH] No match for keyword '%s'\n", keyword);
    return NULL;  // Not a keyword
}

// ============================================================================
// Parse Functions
// ============================================================================

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

// Parse import statement: import Name from "module"
// Returns KRY_NODE_IMPORT node with parsed data
static KryNode* parse_import_statement(KryParser* p) {
    fprintf(stderr, "[PARSER] parse_import_statement() called\n");

    // Skip whitespace after 'import' keyword
    skip_whitespace(p);

    char* import_name = NULL;
    bool is_destructured = false;

    // Check for destructured import: import { name1, name2, ... } from "module"
    if (peek(p) == '{') {
        is_destructured = true;
        advance(p);  // Consume '{'
        skip_whitespace(p);

        // Parse comma-separated names inside braces
        char names_buffer[1024] = {0};
        size_t buffer_pos = 0;

        while (peek(p) != '}' && peek(p) != '\0') {
            skip_whitespace(p);

            // Parse identifier
            if (!isalpha(peek(p)) && peek(p) != '_') {
                kry_parser_error(p, "Expected identifier in destructured import");
                return NULL;
            }

            char* name = parse_identifier(p);
            if (!name) {
                kry_parser_error(p, "Expected identifier in destructured import");
                return NULL;
            }

            // Append to buffer
            size_t name_len = strlen(name);
            if (buffer_pos + name_len + 2 < sizeof(names_buffer)) {
                if (buffer_pos > 0) {
                    names_buffer[buffer_pos++] = ',';
                }
                memcpy(names_buffer + buffer_pos, name, name_len);
                buffer_pos += name_len;
            }

            skip_whitespace(p);

            // Check for comma or closing brace
            if (peek(p) == ',') {
                advance(p);  // Consume ','
                skip_whitespace(p);
            } else if (peek(p) != '}') {
                kry_parser_error(p, "Expected ',' or '}' in destructured import");
                return NULL;
            }
        }

        if (peek(p) != '}') {
            kry_parser_error(p, "Expected '}' to close destructured import");
            return NULL;
        }
        advance(p);  // Consume '}'
        skip_whitespace(p);

        names_buffer[buffer_pos] = '\0';
        import_name = kry_strndup(p, names_buffer, buffer_pos);

        fprintf(stderr, "[PARSER] Parsed destructured import names: %s\n", import_name);
    }
    // Default import: import Name from "module"
    else if (isalpha(peek(p)) || peek(p) == '*') {
        import_name = parse_identifier(p);
        if (!import_name) {
            kry_parser_error(p, "Expected import name after 'import'");
            return NULL;
        }
        skip_whitespace(p);
    }
    else {
        kry_parser_error(p, "Expected import name, '*', or '{' after 'import'");
        return NULL;
    }

    // Check for 'from' keyword
    char* from_keyword = parse_identifier(p);
    if (!from_keyword || !keyword_match(from_keyword, "from")) {
        kry_parser_error(p, "Expected 'from' after import name");
        return NULL;
    }

    skip_whitespace(p);

    // Parse module path (string literal)
    if (peek(p) != '"' && peek(p) != '\'') {
        kry_parser_error(p, "Expected module path string after 'from'");
        return NULL;
    }

    char delim = peek(p);
    advance(p);  // Consume opening quote

    // Parse module path
    const char* module_start = p->source + p->pos;
    while (peek(p) != delim && peek(p) != '\0' && peek(p) != '\n') {
        advance(p);
    }

    if (peek(p) != delim) {
        kry_parser_error(p, "Unterminated module path string");
        return NULL;
    }

    char* module_path = kry_strndup(p, module_start, (size_t)(p->source + p->pos - module_start));
    advance(p);  // Consume closing quote

    // Create import node
    KryNode* import_node = kry_node_create(p, KRY_NODE_IMPORT);
    if (!import_node) {
        return NULL;
    }

    import_node->name = import_name;           // Store imported name(s)
    import_node->import_module = module_path;  // Store module path
    import_node->import_name = import_name;    // Also store in import_name field
    import_node->is_destructured_import = is_destructured;

    fprintf(stderr, "[PARSER] Parsed import: %s from %s (destructured=%d)\n",
            import_name, module_path, is_destructured);

    return import_node;
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

// Parse code block: @lua...@end, @js...@end, @c...@end
static KryNode* parse_code_block(KryParser* p, const char* language) {
    skip_whitespace(p);

    // Read all content until @end marker
    const char* start = p->source + p->pos;
    const char* end_marker = "@end";

    // Search for @end marker
    const char* end_pos = start;
    while (*end_pos != '\0' && (size_t)(end_pos - p->source) < p->length) {
        // Check if we found @end
        if (strncmp(end_pos, end_marker, 4) == 0) {
            // Check it's a standalone @end (preceded by whitespace or start of content)
            if (end_pos == start ||
                *(end_pos - 1) == ' ' ||
                *(end_pos - 1) == '\t' ||
                *(end_pos - 1) == '\n' ||
                *(end_pos - 1) == '\r') {
                // Check what follows @end (should be whitespace, newline, or end of string)
                const char* after_end = end_pos + 4;
                if (*after_end == '\0' ||
                    *after_end == ' ' ||
                    *after_end == '\t' ||
                    *after_end == '\n' ||
                    *after_end == '\r') {
                    break;
                }
            }
        }
        end_pos++;
    }

    if (*end_pos == '\0' || (size_t)(end_pos - p->source) >= p->length) {
        kry_parser_error(p, "Expected @end to close code block");
        return NULL;
    }

    // Extract source code (start to end_pos)
    size_t length = end_pos - start;
    char* source = kry_strndup(p, start, length);

    // Trim leading and trailing whitespace from source
    char* trimmed = source;
    while (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\n' || *trimmed == '\r') {
        trimmed++;
    }

    // Trim trailing whitespace
    size_t trimmed_len = strlen(trimmed);
    if (trimmed_len > 0) {
        char* end_trim = trimmed + trimmed_len - 1;
        while (end_trim > trimmed && (*end_trim == ' ' || *end_trim == '\t' ||
               *end_trim == '\n' || *end_trim == '\r')) {
            *end_trim = '\0';
            end_trim--;
        }
    }

    // Advance parser past @end marker
    // Manually update position and count newlines for accurate line tracking
    const char* scan = start;
    while (scan < end_pos + 4) {  // +4 to skip "@end"
        if (*scan == '\n') {
            p->line++;
            p->column = 1;
        } else {
            p->column++;
        }
        scan++;
    }
    p->pos = (end_pos + 4) - p->source;

    // Create code block node
    KryNode* code_node = kry_node_create_code_block(p, language, trimmed);
    if (!code_node) return NULL;

    fprintf(stderr, "[KRY_PARSE] Parsed code block: language='%s', source_length=%zu\n",
            language, strlen(trimmed));
    fflush(stderr);

    return code_node;
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
        // Error already set by parse_component_body, add context
        kry_parser_error(p, "Failed to parse 'for' loop body");
        return NULL;
    }

    return for_node;
}

// Parse if statement: if condition { ... } else { ... }
static KryNode* parse_if_statement(KryParser* p) {
    skip_whitespace(p);

    // Save position to potentially re-parse as full expression
    KryParserCheckpoint saved = kry_checkpoint_save(p);

    // Parse condition expression (identifier or expression)
    KryValue* condition = parse_value(p);
    if (!condition) {
        kry_parser_error(p, "Expected condition after 'if'");
        return NULL;
    }

    // Check if there's a binary operator following - if so, this is a full expression
    skip_whitespace(p);
    if (peek_is_binary_operator(p)) {
        // Restore position and parse as full expression until '{'
        kry_checkpoint_restore(p, saved);

        // Capture expression until we hit '{'
        size_t start = p->pos;
        int paren_depth = 0;
        while (peek(p) != '\0' && !(peek(p) == '{' && paren_depth == 0)) {
            if (peek(p) == '(') paren_depth++;
            else if (peek(p) == ')') paren_depth--;
            advance(p);
        }

        size_t len = p->pos - start;
        // Trim trailing whitespace
        while (len > 0 && (p->source[start + len - 1] == ' ' ||
                          p->source[start + len - 1] == '\t' ||
                          p->source[start + len - 1] == '\r' ||
                          p->source[start + len - 1] == '\n')) {
            len--;
        }

        if (len == 0) {
            kry_parser_error(p, "Empty condition in if statement");
            return NULL;
        }

        char* expr_text = kry_strndup(p, p->source + start, len);
        if (!expr_text) {
            kry_parser_error(p, "Memory allocation failed for condition expression");
            return NULL;
        }
        condition = kry_value_create_expression(p, expr_text);
        if (!condition) {
            kry_parser_error(p, "Failed to create condition expression");
            return NULL;
        }
    }

    skip_whitespace(p);

    // Create if node
    KryNode* if_node = kry_node_create(p, KRY_NODE_IF);
    if (!if_node) return NULL;

    // Store condition as value
    if_node->value = condition;

    // Parse then block
    if (!parse_component_body(p, if_node)) {
        // Error already set by parse_component_body, add context
        kry_parser_error(p, "Failed to parse 'if' statement body");
        return NULL;
    }

    skip_whitespace(p);

    // Check for else block
    size_t saved_pos = p->pos;
    char* else_keyword = parse_identifier(p);
    if (else_keyword && keyword_match(else_keyword, "else")) {
        skip_whitespace(p);

        // Check for "else if" pattern
        size_t saved_after_else = p->pos;
        char* if_keyword = parse_identifier(p);
        if (if_keyword && keyword_match(if_keyword, "if")) {
            // This is an "else if" - parse recursively as another if statement
            KryNode* else_if_node = parse_if_statement(p);
            if (!else_if_node) {
                kry_parser_error(p, "Failed to parse 'else if' statement");
                return NULL;
            }
            if_node->else_branch = else_if_node;
        } else {
            // Not "else if", restore position and parse as regular else block
            p->pos = saved_after_else;

            // Create else branch node (wrapper for else block children)
            KryNode* else_node = kry_node_create(p, KRY_NODE_COMPONENT);
            if (!else_node) return NULL;
            else_node->name = "ElseBranch";  // Internal marker

            // Parse else block
            if (!parse_component_body(p, else_node)) {
                // Error already set by parse_component_body, add context
                kry_parser_error(p, "Failed to parse 'else' block body");
                return NULL;
            }

            if_node->else_branch = else_node;
        }
    } else {
        // No else block, restore position
        p->pos = saved_pos;
        if_node->else_branch = NULL;
    }

    return if_node;
}

// Parse function declaration: func name(param1: type, param2: type): return_type { ... }
static KryNode* parse_function_declaration(KryParser* p) {
    skip_whitespace(p);

    // Parse function name
    if (!isalpha(peek(p)) && peek(p) != '_') {
        kry_parser_error(p, "Expected function name after 'func'");
        return NULL;
    }

    char* func_name = parse_identifier(p);
    if (!func_name) return NULL;

    skip_whitespace(p);

    // Expect '(' for parameter list
    if (!match(p, '(')) {
        kry_parser_error(p, "Expected '(' after function name");
        return NULL;
    }

    skip_whitespace(p);

    // Create function declaration node
    KryNode* func_node = kry_node_create(p, KRY_NODE_FUNCTION_DECL);
    if (!func_node) return NULL;

    func_node->func_name = func_name;
    func_node->func_params = NULL;
    func_node->param_count = 0;
    func_node->func_return_type = NULL;
    func_node->func_body = NULL;

    // Parse parameter list
    if (peek(p) != ')') {
        // Allocate parameter array (max 32 parameters)
        #define MAX_PARAMS 32
        KryNode* params[MAX_PARAMS];
        int param_idx = 0;

        while (peek(p) != ')' && peek(p) != '\0' && param_idx < MAX_PARAMS) {
            // Parse parameter name
            if (!isalpha(peek(p)) && peek(p) != '_') {
                kry_parser_error(p, "Expected parameter name");
                return NULL;
            }

            char* param_name = parse_identifier(p);
            if (!param_name) return NULL;

            skip_whitespace(p);

            // Optional type annotation: ": type"
            char* param_type = NULL;
            if (peek(p) == ':') {
                match(p, ':');
                skip_whitespace(p);
                param_type = parse_identifier(p);
                if (!param_type) {
                    kry_parser_error(p, "Expected type name after ':'");
                    return NULL;
                }
                skip_whitespace(p);
            }

            // Create a VAR_DECL node for this parameter
            KryNode* param_node = kry_node_create(p, KRY_NODE_VAR_DECL);
            if (!param_node) return NULL;
            param_node->name = param_name;
            param_node->var_type = param_type ? param_type : kry_strdup(p, "any");  // Default to "any"
            param_node->value = NULL;

            params[param_idx++] = param_node;

            // Check for comma or end of parameter list
            if (peek(p) == ',') {
                match(p, ',');
                skip_whitespace(p);
            } else if (peek(p) != ')') {
                kry_parser_error(p, "Expected ',' or ')' in parameter list");
                return NULL;
            }
        }

        // Allocate permanent parameter array from parser memory
        if (param_idx > 0) {
            func_node->func_params = (KryNode**)kry_alloc(p, sizeof(KryNode*) * param_idx);
            if (!func_node->func_params) return NULL;
            for (int i = 0; i < param_idx; i++) {
                func_node->func_params[i] = params[i];
            }
            func_node->param_count = param_idx;
        }
    }

    // Expect closing ')'
    if (!match(p, ')')) {
        kry_parser_error(p, "Expected ')' to close parameter list");
        return NULL;
    }

    skip_whitespace(p);

    // Parse optional return type: ": type"
    if (peek(p) == ':') {
        match(p, ':');
        skip_whitespace(p);

        char* return_type = parse_identifier(p);
        if (!return_type) {
            kry_parser_error(p, "Expected return type after ':'");
            return NULL;
        }

        func_node->func_return_type = return_type;
        skip_whitespace(p);
    } else {
        // Default to void return type
        func_node->func_return_type = kry_strdup(p, "void");
    }

    // Parse function body as a component node (wrapper for statements)
    KryNode* body_node = kry_node_create(p, KRY_NODE_COMPONENT);
    if (!body_node) return NULL;
    body_node->name = "FunctionBody";  // Internal marker

    // Parse function body
    if (!parse_component_body(p, body_node)) {
        // Error already set by parse_component_body, add context
        kry_parser_error(p, "Failed to parse function body");
        return NULL;
    }

    func_node->func_body = body_node;

    return func_node;
}

// Parse module-level return: return { COLORS, DEFAULT_COLOR, getRandomColor }
static KryNode* parse_module_return(KryParser* p) {
    skip_whitespace(p);

    KryNode* return_node = kry_node_create(p, KRY_NODE_MODULE_RETURN);
    if (!return_node) return NULL;

    return_node->export_names = NULL;
    return_node->export_count = 0;

    // Expect object literal with export list
    if (peek(p) != '{') {
        kry_parser_error(p, "Module return requires object literal: return { export1, export2, ... }");
        return NULL;
    }

    match(p, '{');
    skip_whitespace(p);

    // Parse export list
    #define MAX_EXPORTS 64
    char* exports[MAX_EXPORTS];
    int export_idx = 0;

    while (peek(p) != '}' && peek(p) != '\0' && export_idx < MAX_EXPORTS) {
        // Parse export name (identifier)
        if (!isalpha(peek(p)) && peek(p) != '_') {
            kry_parser_error(p, "Expected export identifier");
            return NULL;
        }

        char* export_name = parse_identifier(p);
        if (!export_name) break;

        exports[export_idx++] = export_name;

        skip_whitespace(p);

        // Check for comma or end of list
        if (peek(p) == ',') {
            match(p, ',');
            skip_whitespace(p);
        }
    }

    if (!match(p, '}')) {
        kry_parser_error(p, "Expected '}' to close export list");
        return NULL;
    }

    // Allocate permanent export name array
    if (export_idx > 0) {
        return_node->export_names = (char**)kry_alloc(p, sizeof(char*) * export_idx);
        if (!return_node->export_names) return NULL;

        for (int i = 0; i < export_idx; i++) {
            return_node->export_names[i] = exports[i];
        }
        return_node->export_count = export_idx;
    }

    return return_node;
}

// Parse return statement: return expression (or module-level return)
static KryNode* parse_return_statement(KryParser* p) {
    skip_whitespace(p);

    // Check if this is a module-level return: return { exports }
    // We can detect this by checking if the next non-whitespace character is '{'
    skip_whitespace(p);
    char next_char = peek(p);

    if (next_char == '{') {
        // Module-level return: return { exports }
        return parse_module_return(p);
    }

    // Function return statement
    // Create return statement node
    KryNode* return_node = kry_node_create(p, KRY_NODE_RETURN_STMT);
    if (!return_node) return NULL;

    return_node->return_expr = NULL;

    // Check if there's an expression to return
    // Look ahead to see if we have a semicolon immediately
    size_t saved_pos = p->pos;
    uint32_t saved_line = p->line;
    uint32_t saved_column = p->column;

    skip_whitespace(p);

    // Check if next token is semicolon or end of block (void return)
    if (peek(p) == ';' || peek(p) == '}' || peek(p) == '\n') {
        // Void return - no expression
        p->pos = saved_pos;
        p->line = saved_line;
        p->column = saved_column;
    } else {
        // Restore position and parse expression
        p->pos = saved_pos;
        p->line = saved_line;
        p->column = saved_column;

        // Parse return value expression
        // Save position to potentially re-parse as full expression with binary operators
        KryParserCheckpoint saved = kry_checkpoint_save(p);

        // Try parsing as simple value first
        KryValue* return_value = parse_value(p);
        if (return_value) {
            // Check if there's a binary operator following - if so, this is a full expression
            skip_whitespace(p);
            if (peek_is_binary_operator(p)) {
                // Restore position and parse as full expression
                kry_checkpoint_restore(p, saved);
                return_value = parse_expression_text(p);
                if (!return_value) {
                    kry_parser_error(p, "Failed to parse return expression");
                    return NULL;
                }
            }

            // Store the value in the return_expr field
            KryNode* expr_wrapper = kry_node_create(p, KRY_NODE_EXPRESSION);
            if (expr_wrapper) {
                expr_wrapper->value = return_value;
                return_node->return_expr = expr_wrapper;
            }
        }
    }

    // Semicolon is optional - consume if present
    skip_whitespace(p);
    match(p, ';');  // Don't error if missing

    return return_node;
}

// Parse delete statement: delete <expression>
// Returns KRY_NODE_DELETE_STMT with delete_target set
static KryNode* parse_delete_statement(KryParser* p) {
    // 'delete' keyword already consumed by dispatch
    skip_whitespace(p);

    // Parse the target expression (e.g., habits[index].completions[day.date])
    KryValue* target_value = parse_value(p);
    if (!target_value) {
        kry_parser_error(p, "Expected expression after 'delete'");
        return NULL;
    }

    // Create delete statement node
    KryNode* node = kry_node_create(p, KRY_NODE_DELETE_STMT);
    if (!node) return NULL;

    // Store target as a child node containing the expression
    KryNode* target_node = kry_node_create(p, KRY_NODE_EXPRESSION);
    if (target_node) {
        target_node->value = target_value;
        node->delete_target = target_node;
    }

    // Semicolon is optional - consume if present
    skip_whitespace(p);
    match(p, ';');

    return node;
}

// Parse @ construct (code block like @lua, @js, @c)
// Returns KRY_NODE_CODE_BLOCK or NULL
static KryNode* parse_at_construct(KryParser* p) {
    if (!match(p, '@')) return NULL;

    // Parse the identifier after @
    if (!isalpha(peek(p)) && peek(p) != '_') {
        kry_parser_error(p, "Expected identifier after '@'");
        return NULL;
    }

    char* at_name = parse_identifier(p);
    if (!at_name) return NULL;

    skip_whitespace(p);

    // Check for code blocks (@lua, @js, @c)
    if (keyword_match(at_name, "lua") || keyword_match(at_name, "js") || keyword_match(at_name, "c")) {
        return parse_code_block(p, at_name);
    }

    // Decorators are no longer supported
    kry_parser_error(p, "Decorators (@reactive, @computed, @action, @watch) are no longer supported. Use @lua/@js/@c blocks for platform-specific code.");
    return NULL;
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

        // Check for @ symbols (code blocks)
        if (peek(p) == '@') {
            KryNode* code_block = parse_at_construct(p);
            if (!code_block) return NULL;
            kry_node_append_child(component, code_block);
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

        // Try to dispatch as keyword
        bool handler_failed = false;
        KryNode* keyword_node = dispatch_keyword(p, name, &handler_failed);
        if (keyword_node) {
            kry_node_append_child(component, keyword_node);
        } else if (handler_failed) {
            // Keyword was matched but handler failed - propagate error
            return NULL;
        }
        // Check for property access assignment (object.property = value)
        else if (peek(p) == '.') {
            KryNode* assign = parse_property_access_assignment(p, name);
            if (!assign) return NULL;
            kry_node_append_child(component, assign);
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

            // Capture argument string using balanced delimiter capture
            KryBalancedCapture args;
            if (kry_skip_balanced(p, &args)) {
                size_t arg_length = args.end - args.start;
                if (arg_length > 0) {
                    child->arguments = kry_strndup(p, p->source + args.start, arg_length);
                }
            }
            skip_whitespace(p);
            kry_node_append_child(component, child);
        } else if (peek(p) == '[') {
            // Indexed assignment: arr[idx] = value or arr[idx].prop = value
            KryNode* assign = parse_indexed_assignment(p, name);
            if (!assign) return NULL;
            kry_node_append_child(component, assign);
        } else {
            kry_parser_error(p, "Expected '=', '.', '[', '{', or '(' after identifier");
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

// ============================================================================
// Struct Parsing
// ============================================================================

// Parse struct field: name: type = "default" OR name = "default" (type inferred)
static KryNode* parse_struct_field(KryParser* p) {
    skip_whitespace(p);

    KryNode* field = kry_node_create(p, KRY_NODE_STRUCT_FIELD);
    if (!field) return NULL;

    // Parse field name
    char* field_name = parse_identifier(p);
    if (!field_name) {
        kry_parser_error(p, "Expected field name in struct declaration");
        return NULL;
    }
    field->name = field_name;

    skip_whitespace(p);

    // Check for optional type annotation: ':' type
    if (peek(p) == ':') {
        match(p, ':');
        skip_whitespace(p);

        // Parse type (string, int, int64, float, bool, map, or custom type)
        char* type_name = parse_identifier(p);
        if (!type_name) {
            kry_parser_error(p, "Expected type after ':'");
            return NULL;
        }
        field->field_type = type_name;

        // Note: Type validation is handled by the type system, not the parser.
        // The parser accepts any valid identifier as a type name, including:
        // - Primitive types: string, int, int64, float, bool
        // - Container types: map, [T] arrays
        // - Custom types: user-defined structs

        skip_whitespace(p);
    } else {
        // No explicit type - will be inferred from default value
        field->field_type = NULL;
    }

    // Default value: = expression (required if no type annotation)
    if (peek(p) == '=') {
        match(p, '=');
        skip_whitespace(p);

        // Parse default value (supports expressions like DateTime.today())
        field->field_default = parse_value(p);
    } else if (!field->field_type) {
        // No type and no default value - error
        kry_parser_error(p, "Struct field requires either type annotation or default value");
        return NULL;
    }

    return field;
}

// Parse struct declaration: struct Habit { name: string = ""; ... }
KryNode* kry_parse_struct_declaration(KryParser* p) {
    skip_whitespace(p);

    KryNode* struct_decl = kry_node_create(p, KRY_NODE_STRUCT_DECL);
    if (!struct_decl) return NULL;

    // Parse struct name
    char* struct_name = parse_identifier(p);
    if (!struct_name) {
        kry_parser_error(p, "Expected struct name");
        return NULL;
    }
    struct_decl->struct_name = struct_name;

    skip_whitespace(p);

    // Expect '{'
    if (peek(p) != '{') {
        kry_parser_error(p, "Expected '{' after struct name");
        return NULL;
    }
    match(p, '{');
    skip_whitespace(p);

    // Parse fields
    #define MAX_STRUCT_FIELDS 64
    KryNode* fields[MAX_STRUCT_FIELDS];
    int field_idx = 0;

    while (peek(p) != '}' && peek(p) != '\0' && field_idx < MAX_STRUCT_FIELDS) {
        KryNode* field = parse_struct_field(p);
        if (!field) break;

        fields[field_idx++] = field;

        skip_whitespace(p);

        // Optional comma
        if (peek(p) == ',') {
            match(p, ',');
            skip_whitespace(p);
        }
    }

    if (!match(p, '}')) {
        kry_parser_error(p, "Expected '}' to close struct declaration");
        return NULL;
    }

    // Allocate permanent field array
    if (field_idx > 0) {
        struct_decl->struct_fields = (KryNode**)kry_alloc(p, sizeof(KryNode*) * field_idx);
        if (!struct_decl->struct_fields) return NULL;

        for (int i = 0; i < field_idx; i++) {
            struct_decl->struct_fields[i] = fields[i];
        }
        struct_decl->field_count = field_idx;
    }

    fprintf(stderr, "[STRUCT] Parsed struct declaration '%s' with %d fields\n",
            struct_decl->struct_name, struct_decl->field_count);

    return struct_decl;
}

// Parse struct instantiation: Habit { name = "Exercise" }
KryNode* kry_parse_struct_instantiation(KryParser* p, char* type_name) {
    skip_whitespace(p);

    KryNode* inst = kry_node_create(p, KRY_NODE_STRUCT_INST);
    if (!inst) return NULL;

    inst->instance_type = type_name;

    // Expect '{'
    if (peek(p) != '{') {
        kry_parser_error(p, "Expected '{' for struct instantiation");
        return NULL;
    }
    match(p, '{');
    skip_whitespace(p);

    // Parse field assignments
    #define MAX_INST_FIELDS 64
    char* names[MAX_INST_FIELDS];
    KryValue* values[MAX_INST_FIELDS];
    int field_idx = 0;

    while (peek(p) != '}' && peek(p) != '\0' && field_idx < MAX_INST_FIELDS) {
        // Parse field name
        char* field_name = parse_identifier(p);
        if (!field_name) break;

        skip_whitespace(p);

        // Expect '='
        if (peek(p) != '=') {
            kry_parser_error(p, "Expected '=' after field name");
            break;
        }
        match(p, '=');
        skip_whitespace(p);

        // Parse value
        KryValue* value = parse_value(p);
        if (!value) break;

        names[field_idx] = field_name;
        values[field_idx] = value;
        field_idx++;

        skip_whitespace(p);

        // Optional comma
        if (peek(p) == ',') {
            match(p, ',');
            skip_whitespace(p);
        }
    }

    if (!match(p, '}')) {
        kry_parser_error(p, "Expected '}' to close struct instantiation");
        return NULL;
    }

    // Allocate permanent arrays
    if (field_idx > 0) {
        inst->field_names = (char**)kry_alloc(p, sizeof(char*) * field_idx);
        inst->field_values = (KryValue**)kry_alloc(p, sizeof(KryValue*) * field_idx);

        for (int i = 0; i < field_idx; i++) {
            inst->field_names[i] = names[i];
            inst->field_values[i] = values[i];
        }
        inst->field_value_count = field_idx;
    }

    fprintf(stderr, "[STRUCT] Parsed struct instantiation of type '%s' with %d fields\n",
            inst->instance_type, inst->field_value_count);

    return inst;
}

// ============================================================================
// Main Parse Function
// ============================================================================

// Peek at the next identifier without consuming it
static char* peek_identifier(KryParser* p) {
    skip_whitespace(p);

    size_t start = p->pos;

    // Check if we have an identifier start
    if (!isalpha(peek(p)) && peek(p) != '_') {
        return NULL;
    }

    // Consume identifier characters
    while (isalnum(peek(p)) || peek(p) == '_' || peek(p) == '-') {
        advance(p);
    }

    // Copy identifier to temp buffer
    size_t len = p->pos - start;
    char* ident = kry_strndup(p, p->source + start, len);

    // Reset position (don't consume)
    p->pos = start;

    return ident;
}

KryNode* kry_parse(KryParser* parser) {
    if (!parser) return NULL;

    skip_whitespace(parser);

    // Create a root wrapper node to hold top-level declarations and components
    parser->root = kry_node_create(parser, KRY_NODE_COMPONENT);
    if (!parser->root) return NULL;
    parser->root->name = "Root";  // Special root node

    // Parse top-level declarations (const/let/var/static) and components
    while (peek(parser) != '\0' && !kry_parser_should_stop(parser)) {
        skip_whitespace(parser);
        if (peek(parser) == '\0') break;

        // Check for @ symbols (code blocks) at top level
        if (peek(parser) == '@') {
            KryNode* at_node = parse_at_construct(parser);
            if (at_node) {
                kry_node_append_child(parser->root, at_node);
                fprintf(stderr, "[KRY_PARSE] Added top-level @ node\n");
                fflush(stderr);
            }
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

        fprintf(stderr, "[MAIN_LOOP] Parsed identifier in main loop: '%s'\n", id);

        skip_whitespace(parser);

        KryNode* node = NULL;

        // Try to dispatch as keyword
        bool handler_failed = false;
        node = dispatch_keyword(parser, id, &handler_failed);
        if (handler_failed) {
            // Keyword was matched but handler failed - propagate error
            break;
        }

        // Check for component definition (special handling)
        if (!node && keyword_match(id, "component")) {
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
                KryBalancedCapture args;
                if (kry_skip_balanced(parser, &args)) {
                    size_t arg_length = args.end - args.start;
                    if (arg_length > 0) {
                        node->arguments = kry_strndup(parser, parser->source + args.start, arg_length);
                    }
                }
                skip_whitespace(parser);
            }

            // Check for extends clause
            char* next_identifier = peek_identifier(parser);
            if (next_identifier && keyword_match(next_identifier, "extends")) {
                fprintf(stderr, "[MAIN_LOOP] Found 'extends' keyword\n");

                // Consume "extends" keyword
                parse_identifier(parser);  // This consumes "extends"
                skip_whitespace(parser);

                // Parse parent component name
                char* parent_name = parse_identifier(parser);
                if (!parent_name) {
                    kry_parser_error(parser, "Expected parent component name after 'extends'");
                    // Don't break - continue to try to recover
                } else {
                    node->extends_parent = parent_name;
                    fprintf(stderr, "[MAIN_LOOP] Component extends: '%s'\n", parent_name);
                }
                skip_whitespace(parser);
            }

            // Parse component body
            if (!parse_component_body(parser, node)) {
                break;
            }
        }
        // Must be a component instantiation (only if no node was created by dispatch_keyword)
        else if (!node) {
            fprintf(stderr, "[MAIN_LOOP] No keyword match - treating '%s' as component instantiation\n", id);
            // Create component node
            node = kry_node_create(parser, KRY_NODE_COMPONENT);
            if (!node) break;
            node->name = id;

            // Check for component arguments
            if (peek(parser) == '(') {
                KryBalancedCapture args;
                if (kry_skip_balanced(parser, &args)) {
                    size_t arg_length = args.end - args.start;
                    if (arg_length > 0) {
                        node->arguments = kry_strndup(parser, parser->source + args.start, arg_length);
                    }
                }
                skip_whitespace(parser);
            }

            // Parse component body
            if (!parse_component_body(parser, node)) {
                break;
            }
        }

        // Only stop on fatal errors; continue parsing to collect all errors
        if (kry_parser_should_stop(parser)) {
            break;
        }

        // Skip invalid nodes but continue parsing
        if (!node) {
            continue;
        }

        // Add node as child of root
        kry_node_append_child(parser->root, node);

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
