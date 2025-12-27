#define _POSIX_C_SOURCE 200809L
#include "ir_html_parser.h"
#include "ir_html_to_ir.h"
#include "ir_serialization.h"
#include "ir_logic.h"
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

// Parser context for tracking logic during HTML parsing
typedef struct {
    IRLogicBlock* logic_block;
    uint32_t next_handler_id;
    uint32_t next_component_id;
    char** script_contents;  // Collected <script> blocks
    int script_count;
    int script_capacity;
} HtmlParserContext;

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

// ============================================================================
// Parser Context Helpers
// ============================================================================

static void parser_ctx_init(HtmlParserContext* ctx) {
    ctx->logic_block = ir_logic_block_create();
    ctx->next_handler_id = 1;
    ctx->next_component_id = 1;
    ctx->script_capacity = 4;
    ctx->script_contents = (char**)calloc(ctx->script_capacity, sizeof(char*));
    ctx->script_count = 0;
}

static void parser_ctx_free(HtmlParserContext* ctx) {
    if (ctx->logic_block) {
        ir_logic_block_free(ctx->logic_block);
        ctx->logic_block = NULL;
    }
    if (ctx->script_contents) {
        for (int i = 0; i < ctx->script_count; i++) {
            free(ctx->script_contents[i]);
        }
        free(ctx->script_contents);
        ctx->script_contents = NULL;
    }
}

static void parser_ctx_add_script(HtmlParserContext* ctx, const char* script) {
    if (ctx->script_count >= ctx->script_capacity) {
        ctx->script_capacity *= 2;
        ctx->script_contents = (char**)realloc(ctx->script_contents,
                                               ctx->script_capacity * sizeof(char*));
    }
    ctx->script_contents[ctx->script_count++] = strdup(script);
}

// Check if attribute is an event handler (onclick, onchange, etc.)
static bool is_event_attribute(const char* attr_name) {
    if (!attr_name || strlen(attr_name) < 3) return false;
    return (attr_name[0] == 'o' && attr_name[1] == 'n');
}

// Extract event type from attribute name: "onclick" -> "click"
static char* get_event_type(const char* attr_name) {
    if (!is_event_attribute(attr_name)) return NULL;
    return strdup(attr_name + 2);  // Skip "on" prefix
}

// Extract event handler from attribute value and add to logic block
static void extract_event_handler(HtmlParserContext* ctx,
                                   uint32_t component_id,
                                   const char* attr_name,
                                   const char* attr_value) {
    if (!is_event_attribute(attr_name)) return;

    // Generate handler name
    char* event_type = get_event_type(attr_name);
    if (!event_type) return;

    char handler_name[128];
    snprintf(handler_name, sizeof(handler_name), "handler_%u_%s",
             ctx->next_handler_id++, event_type);

    // Create logic function with JavaScript source
    IRLogicFunction* func = ir_logic_function_create(handler_name);
    ir_logic_function_add_source(func, "javascript", attr_value);

    // Add function to logic block
    ir_logic_block_add_function(ctx->logic_block, func);

    // Create event binding
    IREventBinding* binding = ir_event_binding_create(component_id, event_type, handler_name);
    ir_logic_block_add_binding(ctx->logic_block, binding);

    free(event_type);
}

HtmlNode* ir_html_parse(const char* html, size_t length) {
    if (!html || length == 0) return NULL;

    Tokenizer tok;
    tokenizer_init(&tok, html, length);

    ParseStack stack;
    stack_init(&stack);

    HtmlNode* root = html_node_create_element("root");  // Wrapper root node
    stack_push(&stack, root);

    bool inside_style = false;
    bool inside_script = false;

    while (next_token(&tok)) {
        Token* token = &tok.current;

        switch (token->type) {
            case TOKEN_TAG_OPEN: {
                // Check if we're entering <style> or <script> tags - skip them entirely
                if (strcmp(token->value, "style") == 0) {
                    inside_style = true;
                    // Skip attributes and '>'
                    while (peek_char(&tok) != '>' && peek_char(&tok) != '\0') {
                        tok.pos++;
                    }
                    if (peek_char(&tok) == '>') tok.pos++;
                    break;
                } else if (strcmp(token->value, "script") == 0) {
                    inside_script = true;
                    // Skip attributes and '>'
                    while (peek_char(&tok) != '>' && peek_char(&tok) != '\0') {
                        tok.pos++;
                    }
                    if (peek_char(&tok) == '>') tok.pos++;
                    break;
                }

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
                // Check if we're exiting <style> or <script> tags
                if (strcmp(token->value, "style") == 0) {
                    inside_style = false;
                } else if (strcmp(token->value, "script") == 0) {
                    inside_script = false;
                }

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
                // Skip text content inside <style> and <script> tags
                if (inside_style || inside_script) {
                    break;
                }

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

// Internal function that parses HTML with logic extraction
static HtmlNode* ir_html_parse_with_logic(const char* html, size_t length,
                                           HtmlParserContext* ctx) {
    if (!html || length == 0) return NULL;

    Tokenizer tok;
    tokenizer_init(&tok, html, length);

    ParseStack stack;
    stack_init(&stack);

    HtmlNode* root = html_node_create_element("root");
    stack_push(&stack, root);

    bool inside_style = false;
    bool inside_script = false;
    size_t script_start = 0;

    while (next_token(&tok)) {
        Token* token = &tok.current;

        switch (token->type) {
            case TOKEN_TAG_OPEN: {
                if (strcmp(token->value, "style") == 0) {
                    inside_style = true;
                    while (peek_char(&tok) != '>' && peek_char(&tok) != '\0') {
                        tok.pos++;
                    }
                    if (peek_char(&tok) == '>') tok.pos++;
                    break;
                } else if (strcmp(token->value, "script") == 0) {
                    inside_script = true;
                    // Skip attributes and '>'
                    while (peek_char(&tok) != '>' && peek_char(&tok) != '\0') {
                        tok.pos++;
                    }
                    if (peek_char(&tok) == '>') tok.pos++;
                    script_start = tok.pos;  // Mark start of script content
                    break;
                }

                HtmlNode* element = html_node_create_element(token->value);
                if (!element) continue;

                // Assign component ID if not already present
                if (ctx && element->data_ir_id == 0) {
                    element->data_ir_id = ctx->next_component_id++;
                }

                // Parse attributes and extract event handlers
                while (peek_char(&tok) != '>' && peek_char(&tok) != '\0') {
                    char* attr_name = NULL;
                    char* attr_value = NULL;

                    if (!parse_attribute(&tok, &attr_name, &attr_value)) {
                        break;
                    }

                    // Extract event handler if this is an event attribute
                    if (ctx && is_event_attribute(attr_name)) {
                        extract_event_handler(ctx, element->data_ir_id, attr_name, attr_value);
                    } else {
                        // Set regular attribute
                        if (strncmp(attr_name, "data-", 5) == 0) {
                            html_node_set_data_attribute(element, attr_name, attr_value);
                        } else {
                            html_node_set_attribute(element, attr_name, attr_value);
                        }
                    }

                    if (attr_name) free(attr_name);
                    if (attr_value) free(attr_value);
                }

                if (peek_char(&tok) == '>') tok.pos++;

                HtmlNode* parent = stack_peek(&stack);
                if (parent) html_node_add_child(parent, element);
                stack_push(&stack, element);
                break;
            }

            case TOKEN_TAG_SELF_CLOSE: {
                HtmlNode* element = html_node_create_element(token->value);
                if (!element) continue;

                // Assign component ID if not already present
                if (ctx && element->data_ir_id == 0) {
                    element->data_ir_id = ctx->next_component_id++;
                }

                while (peek_char(&tok) != '>' && peek_char(&tok) != '/' && peek_char(&tok) != '\0') {
                    char* attr_name = NULL;
                    char* attr_value = NULL;

                    if (!parse_attribute(&tok, &attr_name, &attr_value)) {
                        break;
                    }

                    if (ctx && is_event_attribute(attr_name)) {
                        extract_event_handler(ctx, element->data_ir_id, attr_name, attr_value);
                    } else {
                        if (strncmp(attr_name, "data-", 5) == 0) {
                            html_node_set_data_attribute(element, attr_name, attr_value);
                        } else {
                            html_node_set_attribute(element, attr_name, attr_value);
                        }
                    }

                    if (attr_name) free(attr_name);
                    if (attr_value) free(attr_value);
                }

                HtmlNode* parent = stack_peek(&stack);
                if (parent) html_node_add_child(parent, element);
                break;
            }

            case TOKEN_TAG_CLOSE: {
                if (strcmp(token->value, "style") == 0) {
                    inside_style = false;
                } else if (strcmp(token->value, "script") == 0) {
                    inside_script = false;
                    // Extract script content
                    if (ctx && script_start < tok.pos) {
                        size_t script_end = tok.pos;
                        // Find the actual </script> position by searching backwards
                        const char* script_tag = "</script>";
                        for (size_t i = tok.pos; i > script_start; i--) {
                            if (strncmp(tok.input + i, script_tag, strlen(script_tag)) == 0) {
                                script_end = i;
                                break;
                            }
                        }

                        if (script_end > script_start) {
                            char* script_content = extract_string(&tok, script_start, script_end);
                            if (script_content) {
                                parser_ctx_add_script(ctx, script_content);
                                free(script_content);
                            }
                        }
                    }
                }

                HtmlNode* current = stack_peek(&stack);
                if (current && current->tag_name && strcmp(current->tag_name, token->value) == 0) {
                    stack_pop(&stack);
                }

                while (peek_char(&tok) != '>' && peek_char(&tok) != '\0') {
                    tok.pos++;
                }
                if (peek_char(&tok) == '>') tok.pos++;
                break;
            }

            case TOKEN_TEXT: {
                if (inside_style || inside_script) {
                    break;
                }

                HtmlNode* text_node = html_node_create_text(token->value);
                if (!text_node) continue;

                HtmlNode* parent = stack_peek(&stack);
                if (parent) html_node_add_child(parent, text_node);
                break;
            }

            case TOKEN_COMMENT:
            case TOKEN_ATTRIBUTE:
            case TOKEN_END:
                break;
        }
    }

    token_free(&tok.current);
    stack_free(&stack);

    HtmlNode* result = NULL;
    if (root && root->child_count > 0) {
        result = root->children[0];
        root->children[0] = NULL;
        root->child_count = 0;
    }

    html_node_free(root);
    return result;
}

// HTML to KIR JSON conversion
char* ir_html_to_kir(const char* html, size_t length) {
    if (!html) return NULL;

    // Create parser context for logic extraction
    HtmlParserContext ctx;
    parser_ctx_init(&ctx);

    // Step 1: Parse HTML to AST with logic extraction
    HtmlNode* ast = ir_html_parse_with_logic(html, length, &ctx);
    if (!ast) {
        fprintf(stderr, "Error: Failed to parse HTML\n");
        parser_ctx_free(&ctx);
        return NULL;
    }

    // Step 2: Convert AST to IR
    IRComponent* root = ir_html_to_ir_convert(ast);
    html_node_free(ast);

    if (!root) {
        fprintf(stderr, "Error: Failed to convert HTML to IR\n");
        parser_ctx_free(&ctx);
        return NULL;
    }

    // Step 3: Serialize IR to JSON with logic_block
    char* json = ir_serialize_json_complete(root, NULL, ctx.logic_block, NULL);
    ir_destroy_component(root);
    parser_ctx_free(&ctx);

    if (!json) {
        fprintf(stderr, "Error: Failed to serialize IR to JSON\n");
        return NULL;
    }

    return json;
}

char* ir_html_file_to_kir(const char* filepath) {
    if (!filepath) return NULL;

    // Read HTML file
    FILE* f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file: %s\n", filepath);
        return NULL;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        fprintf(stderr, "Error: Empty or invalid file: %s\n", filepath);
        return NULL;
    }

    // Read content
    char* html = (char*)malloc((size_t)size + 1);
    if (!html) {
        fclose(f);
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }

    size_t bytes_read = fread(html, 1, (size_t)size, f);
    fclose(f);

    if ((long)bytes_read != size) {
        free(html);
        fprintf(stderr, "Error: Failed to read file: %s\n", filepath);
        return NULL;
    }

    html[bytes_read] = '\0';

    // Convert HTML to KIR
    char* kir = ir_html_to_kir(html, bytes_read);
    free(html);

    return kir;
}
