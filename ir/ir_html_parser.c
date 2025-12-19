#define _POSIX_C_SOURCE 200809L
#include "ir_html_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================================
// Tokenizer
// ============================================================================

typedef enum {
    TOKEN_TAG_OPEN,      // <tag
    TOKEN_TAG_CLOSE,     // </tag>
    TOKEN_TAG_SELF_CLOSE,// <tag />
    TOKEN_ATTRIBUTE,     // attr="value"
    TOKEN_TEXT,          // text content
    TOKEN_COMMENT,       // <!-- comment -->
    TOKEN_END,           // End of input
} TokenType;

typedef struct {
    TokenType type;
    char* value;         // Tag name, attribute name, or text content
    char* attr_value;    // Attribute value (for TOKEN_ATTRIBUTE)
    bool is_closing_tag; // True for </tag>
} Token;

typedef struct {
    const char* input;
    size_t length;
    size_t pos;
    Token current;
} Tokenizer;

static void tokenizer_init(Tokenizer* tok, const char* input, size_t length) {
    tok->input = input;
    tok->length = length;
    tok->pos = 0;
    memset(&tok->current, 0, sizeof(Token));
}

static void token_free(Token* token) {
    if (token->value) {
        free(token->value);
        token->value = NULL;
    }
    if (token->attr_value) {
        free(token->attr_value);
        token->attr_value = NULL;
    }
}

static char peek_char(Tokenizer* tok) {
    if (tok->pos >= tok->length) return '\0';
    return tok->input[tok->pos];
}

static char next_char(Tokenizer* tok) {
    if (tok->pos >= tok->length) return '\0';
    return tok->input[tok->pos++];
}

static void skip_whitespace(Tokenizer* tok) {
    while (tok->pos < tok->length && isspace(peek_char(tok))) {
        tok->pos++;
    }
}

static char* extract_string(Tokenizer* tok, size_t start, size_t end) {
    if (end <= start || end > tok->length) return NULL;
    size_t len = end - start;
    char* str = (char*)malloc(len + 1);
    if (!str) return NULL;
    memcpy(str, tok->input + start, len);
    str[len] = '\0';
    return str;
}

static char* parse_quoted_string(Tokenizer* tok, char quote) {
    size_t start = tok->pos;
    while (tok->pos < tok->length && peek_char(tok) != quote) {
        if (peek_char(tok) == '\\') tok->pos++;  // Escape character
        tok->pos++;
    }

    char* value = extract_string(tok, start, tok->pos);
    if (peek_char(tok) == quote) tok->pos++;  // Skip closing quote
    return value;
}

static char* parse_until(Tokenizer* tok, const char* delimiters) {
    size_t start = tok->pos;
    while (tok->pos < tok->length) {
        char ch = peek_char(tok);
        bool is_delimiter = false;
        for (const char* d = delimiters; *d; d++) {
            if (ch == *d) {
                is_delimiter = true;
                break;
            }
        }
        if (is_delimiter) break;
        tok->pos++;
    }
    return extract_string(tok, start, tok->pos);
}

static bool next_token(Tokenizer* tok) {
    token_free(&tok->current);
    skip_whitespace(tok);

    if (tok->pos >= tok->length) {
        tok->current.type = TOKEN_END;
        return false;
    }

    char ch = peek_char(tok);

    // Check for tag
    if (ch == '<') {
        tok->pos++;  // Skip '<'

        // Check for comment
        if (tok->pos + 2 < tok->length &&
            tok->input[tok->pos] == '!' &&
            tok->input[tok->pos + 1] == '-' &&
            tok->input[tok->pos + 2] == '-') {
            tok->pos += 3;  // Skip '!--'

            // Find end of comment
            while (tok->pos + 2 < tok->length) {
                if (tok->input[tok->pos] == '-' &&
                    tok->input[tok->pos + 1] == '-' &&
                    tok->input[tok->pos + 2] == '>') {
                    tok->pos += 3;
                    break;
                }
                tok->pos++;
            }

            tok->current.type = TOKEN_COMMENT;
            return true;
        }

        // Check for closing tag
        if (peek_char(tok) == '/') {
            tok->pos++;  // Skip '/'
            tok->current.is_closing_tag = true;
        } else {
            tok->current.is_closing_tag = false;
        }

        // Parse tag name
        skip_whitespace(tok);
        tok->current.value = parse_until(tok, " \t\r\n>/");

        if (!tok->current.value) {
            tok->current.type = TOKEN_END;
            return false;
        }

        // Skip to end of tag
        skip_whitespace(tok);

        // Check for self-closing tag
        if (peek_char(tok) == '/' && tok->pos + 1 < tok->length && tok->input[tok->pos + 1] == '>') {
            tok->pos += 2;  // Skip '/>'
            tok->current.type = TOKEN_TAG_SELF_CLOSE;
            return true;
        }

        // Regular tag (attributes will be parsed separately)
        tok->current.type = tok->current.is_closing_tag ? TOKEN_TAG_CLOSE : TOKEN_TAG_OPEN;
        return true;

    } else {
        // Parse text content
        size_t start = tok->pos;
        while (tok->pos < tok->length && peek_char(tok) != '<') {
            tok->pos++;
        }

        char* text = extract_string(tok, start, tok->pos);
        if (text) {
            // Trim whitespace from text
            char* trimmed_start = text;
            char* trimmed_end = text + strlen(text);

            while (*trimmed_start && isspace(*trimmed_start)) trimmed_start++;
            while (trimmed_end > trimmed_start && isspace(*(trimmed_end - 1))) trimmed_end--;

            if (trimmed_end > trimmed_start) {
                *trimmed_end = '\0';
                tok->current.value = strdup(trimmed_start);
                free(text);
                tok->current.type = TOKEN_TEXT;
                return true;
            }
            free(text);
        }

        return next_token(tok);  // Skip empty text, get next token
    }
}

static bool parse_attribute(Tokenizer* tok, char** attr_name, char** attr_value) {
    skip_whitespace(tok);

    char ch = peek_char(tok);
    if (ch == '>' || ch == '/' || ch == '\0') return false;

    // Parse attribute name
    *attr_name = parse_until(tok, " \t\r\n>=");
    if (!*attr_name || strlen(*attr_name) == 0) {
        if (*attr_name) free(*attr_name);
        return false;
    }

    skip_whitespace(tok);

    // Check for '='
    if (peek_char(tok) == '=') {
        tok->pos++;  // Skip '='
        skip_whitespace(tok);

        // Parse attribute value
        char quote = peek_char(tok);
        if (quote == '"' || quote == '\'') {
            tok->pos++;  // Skip opening quote
            *attr_value = parse_quoted_string(tok, quote);
        } else {
            *attr_value = parse_until(tok, " \t\r\n>");
        }
    } else {
        // Boolean attribute (no value)
        *attr_value = strdup("true");
    }

    return true;
}

// ============================================================================
// Parser
// ============================================================================

typedef struct {
    HtmlNode** stack;
    uint32_t stack_count;
    uint32_t stack_capacity;
} ParseStack;

static void stack_init(ParseStack* stack) {
    stack->stack_capacity = 16;
    stack->stack = (HtmlNode**)calloc(stack->stack_capacity, sizeof(HtmlNode*));
    stack->stack_count = 0;
}

static void stack_push(ParseStack* stack, HtmlNode* node) {
    if (stack->stack_count >= stack->stack_capacity) {
        stack->stack_capacity *= 2;
        stack->stack = (HtmlNode**)realloc(stack->stack, stack->stack_capacity * sizeof(HtmlNode*));
    }
    stack->stack[stack->stack_count++] = node;
}

static HtmlNode* stack_pop(ParseStack* stack) {
    if (stack->stack_count == 0) return NULL;
    return stack->stack[--stack->stack_count];
}

static HtmlNode* stack_peek(ParseStack* stack) {
    if (stack->stack_count == 0) return NULL;
    return stack->stack[stack->stack_count - 1];
}

static void stack_free(ParseStack* stack) {
    if (stack->stack) free(stack->stack);
    stack->stack = NULL;
    stack->stack_count = 0;
}

HtmlNode* ir_html_parse(const char* html, size_t length) {
    if (!html || length == 0) return NULL;

    Tokenizer tok;
    tokenizer_init(&tok, html, length);

    ParseStack stack;
    stack_init(&stack);

    HtmlNode* root = html_node_create_element("root");  // Wrapper root node
    stack_push(&stack, root);

    while (next_token(&tok)) {
        Token* token = &tok.current;

        switch (token->type) {
            case TOKEN_TAG_OPEN: {
                // Create element node
                HtmlNode* element = html_node_create_element(token->value);
                if (!element) continue;

                // Parse attributes
                while (peek_char(&tok) != '>' && peek_char(&tok) != '\0') {
                    char* attr_name = NULL;
                    char* attr_value = NULL;

                    if (!parse_attribute(&tok, &attr_name, &attr_value)) {
                        break;
                    }

                    // Set attribute (handles data-* attributes)
                    if (strncmp(attr_name, "data-", 5) == 0) {
                        html_node_set_data_attribute(element, attr_name, attr_value);
                    } else {
                        html_node_set_attribute(element, attr_name, attr_value);
                    }

                    if (attr_name) free(attr_name);
                    if (attr_value) free(attr_value);
                }

                // Skip '>'
                if (peek_char(&tok) == '>') tok.pos++;

                // Add to parent
                HtmlNode* parent = stack_peek(&stack);
                if (parent) html_node_add_child(parent, element);

                // Push to stack (for children)
                stack_push(&stack, element);
                break;
            }

            case TOKEN_TAG_SELF_CLOSE: {
                // Create element node
                HtmlNode* element = html_node_create_element(token->value);
                if (!element) continue;

                // Parse attributes (same as TAG_OPEN)
                while (peek_char(&tok) != '>' && peek_char(&tok) != '/' && peek_char(&tok) != '\0') {
                    char* attr_name = NULL;
                    char* attr_value = NULL;

                    if (!parse_attribute(&tok, &attr_name, &attr_value)) {
                        break;
                    }

                    if (strncmp(attr_name, "data-", 5) == 0) {
                        html_node_set_data_attribute(element, attr_name, attr_value);
                    } else {
                        html_node_set_attribute(element, attr_name, attr_value);
                    }

                    if (attr_name) free(attr_name);
                    if (attr_value) free(attr_value);
                }

                // Add to parent
                HtmlNode* parent = stack_peek(&stack);
                if (parent) html_node_add_child(parent, element);

                // Don't push to stack (self-closing, no children)
                break;
            }

            case TOKEN_TAG_CLOSE: {
                // Pop from stack
                HtmlNode* current = stack_peek(&stack);
                if (current && current->tag_name && strcmp(current->tag_name, token->value) == 0) {
                    stack_pop(&stack);
                } else {
                    // Mismatched closing tag - try to recover by finding matching open tag
                    fprintf(stderr, "Warning: Mismatched closing tag </%s>, expected </%s>\n",
                            token->value, current && current->tag_name ? current->tag_name : "?");
                }

                // Skip '>'
                while (peek_char(&tok) != '>' && peek_char(&tok) != '\0') {
                    tok.pos++;
                }
                if (peek_char(&tok) == '>') tok.pos++;
                break;
            }

            case TOKEN_TEXT: {
                // Create text node
                HtmlNode* text_node = html_node_create_text(token->value);
                if (!text_node) continue;

                // Add to parent
                HtmlNode* parent = stack_peek(&stack);
                if (parent) html_node_add_child(parent, text_node);
                break;
            }

            case TOKEN_COMMENT:
                // Ignore comments
                break;

            case TOKEN_ATTRIBUTE:
            case TOKEN_END:
                break;
        }
    }

    token_free(&tok.current);
    stack_free(&stack);

    // Return first child of root (skip wrapper)
    HtmlNode* result = NULL;
    if (root && root->child_count > 0) {
        result = root->children[0];
        root->children[0] = NULL;  // Prevent double-free
        root->child_count = 0;
    }

    html_node_free(root);
    return result;
}

HtmlNode* ir_html_parse_file(const char* filepath) {
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file: %s\n", filepath);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = (char*)malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    buffer[bytes_read] = '\0';
    fclose(file);

    HtmlNode* ast = ir_html_parse(buffer, bytes_read);
    free(buffer);

    return ast;
}

// Stub implementations (to be completed with HTML-to-IR conversion)
char* ir_html_to_kir(const char* html, size_t length) {
    // TODO: Implement after HTML-to-IR conversion is done
    (void)html;
    (void)length;
    fprintf(stderr, "Error: ir_html_to_kir not yet implemented\n");
    return NULL;
}

char* ir_html_file_to_kir(const char* filepath) {
    // TODO: Implement after HTML-to-IR conversion is done
    (void)filepath;
    fprintf(stderr, "Error: ir_html_file_to_kir not yet implemented\n");
    return NULL;
}
