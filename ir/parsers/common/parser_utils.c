/**
 * Kryon Parser - Common Utilities Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "parser_utils.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

// ============================================================================
// Error Handling System
// ============================================================================

void parser_error_set(ParserError* error, ParserErrorCode code, ParserErrorLevel level,
                      int line, int column, const char* format, ...) {
    if (!error) return;

    error->code = code;
    error->level = level;
    error->line = line;
    error->column = column;
    error->source_file = NULL;

    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
}

void parser_error_set_v(ParserError* error, ParserErrorCode code, ParserErrorLevel level,
                        int line, int column, const char* format, va_list args) {
    if (!error) return;

    error->code = code;
    error->level = level;
    error->line = line;
    error->column = column;
    error->source_file = NULL;

    vsnprintf(error->message, sizeof(error->message), format, args);
}

const char* parser_error_string(ParserErrorCode code) {
    switch (code) {
        case PARSER_OK: return "No error";
        case PARSER_ERROR_SYNTAX: return "Syntax error";
        case PARSER_ERROR_LEXICAL: return "Lexical error";
        case PARSER_ERROR_MEMORY: return "Memory allocation error";
        case PARSER_ERROR_IO: return "I/O error";
        case PARSER_ERROR_UNKNOWN: return "Unknown error";
        case PARSER_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case PARSER_ERROR_UNTERMINATED_STRING: return "Unterminated string literal";
        default: return "Invalid error code";
    }
}

const char* parser_error_format(const ParserError* error) {
    if (!error) return "NULL error";

    static char buffer[1024];
    if (error->line > 0) {
        snprintf(buffer, sizeof(buffer), "%s at line %d, column %d: %s",
                 parser_error_string(error->code), error->line, error->column, error->message);
    } else {
        snprintf(buffer, sizeof(buffer), "%s: %s",
                 parser_error_string(error->code), error->message);
    }
    return buffer;
}

// ============================================================================
// Memory Management Utilities
// ============================================================================

char* parser_strdup(const char* str) {
    if (!str) return NULL;
    return strdup(str);
}

char* parser_strndup(const char* str, size_t n) {
    if (!str) return NULL;

    size_t len = strlen(str);
    if (len > n) len = n;

    char* result = malloc(len + 1);
    if (!result) return NULL;

    memcpy(result, str, len);
    result[len] = '\0';
    return result;
}

void* parser_calloc(size_t size) {
    if (size == 0) return NULL;
    return calloc(1, size);
}

void* parser_realloc(void* ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

// ============================================================================
// String Parsing Utilities
// ============================================================================

char* parser_parse_identifier(const char* source, size_t* pos, size_t max_len) {
    if (!source || !pos || *pos >= max_len) return NULL;

    size_t start = *pos;

    // First character must be alpha or underscore
    char ch = source[*pos];
    if (!isalpha(ch) && ch != '_') {
        return NULL;
    }
    (*pos)++;

    // Subsequent characters can be alphanumeric, underscore, or hyphen
    while (*pos < max_len) {
        ch = source[*pos];
        if (isalnum(ch) || ch == '_' || ch == '-') {
            (*pos)++;
        } else {
            break;
        }
    }

    return parser_strndup(source + start, *pos - start);
}

char* parser_parse_string(const char* source, size_t* pos, size_t max_len,
                          ParserError* out_error) {
    if (!source || !pos || *pos >= max_len) return NULL;

    // Check for opening quote
    if (source[*pos] != '"') {
        if (out_error) {
            parser_error_set(out_error, PARSER_ERROR_SYNTAX, PARSER_ERROR_ERROR,
                            0, 0, "Expected string literal");
        }
        return NULL;
    }
    (*pos)++; // Skip opening quote

    ParserBuffer buf;
    if (!parser_buffer_init(&buf, 64)) {
        if (out_error) {
            parser_error_set(out_error, PARSER_ERROR_MEMORY, PARSER_ERROR_ERROR,
                            0, 0, "Failed to allocate string buffer");
        }
        return NULL;
    }

    while (*pos < max_len) {
        char ch = source[*pos];

        if (ch == '"') {
            (*pos)++; // Skip closing quote
            char* result = parser_buffer_get(&buf);
            parser_buffer_free(&buf);
            return result;
        }

        if (ch == '\\') {
            (*pos)++; // Skip escape char
            if (*pos >= max_len) {
                if (out_error) {
                    parser_error_set(out_error, PARSER_ERROR_UNTERMINATED_STRING,
                                    PARSER_ERROR_ERROR, 0, 0,
                                    "Unterminated string literal");
                }
                parser_buffer_free(&buf);
                return NULL;
            }

            char escaped = source[*pos];
            switch (escaped) {
                case 'n': parser_buffer_append(&buf, '\n'); break;
                case 't': parser_buffer_append(&buf, '\t'); break;
                case 'r': parser_buffer_append(&buf, '\r'); break;
                case '\\': parser_buffer_append(&buf, '\\'); break;
                case '"': parser_buffer_append(&buf, '"'); break;
                case '\'': parser_buffer_append(&buf, '\''); break;
                default:
                    // Unknown escape - just copy the character
                    parser_buffer_append(&buf, escaped);
                    break;
            }
            (*pos)++;
        } else {
            parser_buffer_append(&buf, ch);
            (*pos)++;
        }
    }

    // Reached end without closing quote
    if (out_error) {
        parser_error_set(out_error, PARSER_ERROR_UNTERMINATED_STRING,
                        PARSER_ERROR_ERROR, 0, 0,
                        "Unterminated string literal");
    }
    parser_buffer_free(&buf);
    return NULL;
}

double parser_parse_number(const char* source, size_t* pos, size_t max_len) {
    if (!source || !pos || *pos >= max_len) return 0.0;

    size_t start = *pos;

    // Handle negative numbers
    if (source[*pos] == '-') {
        (*pos)++;
    }

    // Parse integer part
    while (*pos < max_len && isdigit(source[*pos])) {
        (*pos)++;
    }

    // Parse decimal part
    if (*pos < max_len && source[*pos] == '.') {
        (*pos)++;
        while (*pos < max_len && isdigit(source[*pos])) {
            (*pos)++;
        }
    }

    // Extract the number string
    char num_str[128];
    size_t len = *pos - start;
    if (len >= sizeof(num_str)) {
        len = sizeof(num_str) - 1;
    }
    memcpy(num_str, source + start, len);
    num_str[len] = '\0';

    return atof(num_str);
}

void parser_skip_whitespace(const char* source, size_t* pos, size_t max_len) {
    if (!source || !pos) return;

    while (*pos < max_len && isspace(source[*pos])) {
        (*pos)++;
    }
}

char parser_peek(const char* source, size_t pos, size_t max_len) {
    if (!source || pos >= max_len) return '\0';
    return source[pos];
}

bool parser_match(const char* source, size_t* pos, size_t max_len, char expected) {
    if (!source || !pos) return false;
    if (*pos >= max_len) return false;

    if (source[*pos] == expected) {
        (*pos)++;
        return true;
    }
    return false;
}

char parser_advance(const char* source, size_t* pos, size_t max_len) {
    if (!source || !pos) return '\0';
    if (*pos >= max_len) return '\0';

    return source[(*pos)++];
}

// ============================================================================
// AST Node Creation Helpers
// ============================================================================

void parser_ast_node_init(ParserASTNode* node, int node_type, int line, int column) {
    if (!node) return;

    node->node_type = node_type;
    node->line = line;
    node->column = column;
    node->children = NULL;
    node->child_count = 0;
    node->child_capacity = 0;
}

bool parser_ast_add_child(ParserASTNode* node, ParserASTNode* child) {
    if (!node || !child) return false;

    // Grow capacity if needed
    if (node->child_count >= node->child_capacity) {
        size_t new_capacity = node->child_capacity == 0 ? 8 : node->child_capacity * 2;
        ParserASTNode** new_children = realloc(node->children,
                                                new_capacity * sizeof(ParserASTNode*));
        if (!new_children) return false;

        node->children = new_children;
        node->child_capacity = new_capacity;
    }

    node->children[node->child_count++] = child;
    return true;
}

void parser_ast_free_children(ParserASTNode* node,
                              void (*child_free)(ParserASTNode* child)) {
    if (!node) return;

    for (size_t i = 0; i < node->child_count; i++) {
        if (child_free) {
            child_free(node->children[i]);
        } else {
            free(node->children[i]);
        }
    }

    free(node->children);
    node->children = NULL;
    node->child_count = 0;
    node->child_capacity = 0;
}

// ============================================================================
// File Reading Utilities
// ============================================================================

char* parser_read_file(const char* filepath, size_t* out_size, ParserError* out_error) {
    if (!filepath) {
        if (out_error) {
            parser_error_set(out_error, PARSER_ERROR_IO, PARSER_ERROR_ERROR,
                            0, 0, "NULL file path");
        }
        return NULL;
    }

    FILE* f = fopen(filepath, "r");
    if (!f) {
        if (out_error) {
            parser_error_set(out_error, PARSER_ERROR_IO, PARSER_ERROR_ERROR,
                            0, 0, "Failed to open file: %s (%s)", filepath, strerror(errno));
        }
        return NULL;
    }

    // Get file size
    if (fseek(f, 0, SEEK_END) != 0) {
        if (out_error) {
            parser_error_set(out_error, PARSER_ERROR_IO, PARSER_ERROR_ERROR,
                            0, 0, "Failed to seek in file: %s", filepath);
        }
        fclose(f);
        return NULL;
    }

    long size = ftell(f);
    if (size < 0) {
        if (out_error) {
            parser_error_set(out_error, PARSER_ERROR_IO, PARSER_ERROR_ERROR,
                            0, 0, "Failed to get file size: %s", filepath);
        }
        fclose(f);
        return NULL;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        if (out_error) {
            parser_error_set(out_error, PARSER_ERROR_IO, PARSER_ERROR_ERROR,
                            0, 0, "Failed to seek to start: %s", filepath);
        }
        fclose(f);
        return NULL;
    }

    // Allocate buffer
    char* buffer = malloc(size + 1);
    if (!buffer) {
        if (out_error) {
            parser_error_set(out_error, PARSER_ERROR_MEMORY, PARSER_ERROR_ERROR,
                            0, 0, "Failed to allocate buffer for file: %s", filepath);
        }
        fclose(f);
        return NULL;
    }

    // Read file
    size_t bytes_read = fread(buffer, 1, (size_t)size, f);
    if (bytes_read != (size_t)size) {
        if (out_error) {
            parser_error_set(out_error, PARSER_ERROR_IO, PARSER_ERROR_ERROR,
                            0, 0, "Failed to read complete file: %s", filepath);
        }
        free(buffer);
        fclose(f);
        return NULL;
    }

    buffer[size] = '\0';
    fclose(f);

    if (out_size) {
        *out_size = (size_t)size;
    }

    return buffer;
}

long parser_get_file_size(const char* filepath) {
    if (!filepath) return -1;

    FILE* f = fopen(filepath, "r");
    if (!f) return -1;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }

    long size = ftell(f);
    fclose(f);

    return size;
}

// ============================================================================
// Buffer Management
// ============================================================================

bool parser_buffer_init(ParserBuffer* buf, size_t initial_capacity) {
    if (!buf) return false;

    buf->data = malloc(initial_capacity);
    if (!buf->data) return false;

    buf->size = 0;
    buf->capacity = initial_capacity;
    return true;
}

bool parser_buffer_append(ParserBuffer* buf, char ch) {
    if (!buf) return false;

    // Grow capacity if needed
    if (buf->size >= buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        char* new_data = realloc(buf->data, new_capacity);
        if (!new_data) return false;

        buf->data = new_data;
        buf->capacity = new_capacity;
    }

    buf->data[buf->size++] = ch;
    return true;
}

bool parser_buffer_append_str(ParserBuffer* buf, const char* str) {
    if (!buf || !str) return false;

    size_t len = strlen(str);

    // Grow capacity if needed
    while (buf->size + len >= buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        char* new_data = realloc(buf->data, new_capacity);
        if (!new_data) return false;

        buf->data = new_data;
        buf->capacity = new_capacity;
    }

    memcpy(buf->data + buf->size, str, len);
    buf->size += len;
    return true;
}

char* parser_buffer_get(ParserBuffer* buf) {
    if (!buf) return NULL;

    // Ensure null termination
    if (buf->size >= buf->capacity) {
        if (!parser_buffer_append(buf, '\0')) {
            return NULL;
        }
    } else {
        buf->data[buf->size] = '\0';
    }

    return buf->data;
}

void parser_buffer_free(ParserBuffer* buf) {
    if (!buf) return;

    free(buf->data);
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

void parser_buffer_reset(ParserBuffer* buf) {
    if (!buf) return;
    buf->size = 0;
}
