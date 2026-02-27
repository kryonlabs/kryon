/*
 * Kryon DSL Parser Implementation
 * C89/C90 compliant
 *
 * Parses .kryon files and creates windows/widgets
 */

#include "parser.h"
#include "../include/window.h"
#include "../include/widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * snprintf prototype for C89 compatibility
 */
extern int snprintf(char *str, size_t size, const char *format, ...);

/*
 * Token types
 */
typedef enum {
    TOKEN_WORD,
    TOKEN_STRING,
    TOKEN_LBRACE,     /* { */
    TOKEN_RBRACE,     /* } */
    TOKEN_EQUALS,     /* = */
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

/*
 * Token structure
 */
typedef struct {
    TokenType type;
    char *value;
    int line;
    int column;
} Token;

/*
 * Parser context
 */
typedef struct {
    const char *filename;
    char *source;
    int source_len;
    int pos;
    int line;
    int column;
    Token current_token;
    int has_error;
    char error_msg[256];
} Parser;

/*
 * Forward declarations
 */
static Token next_token(Parser *p);
static Token peek_token(Parser *p);
static int expect_token(Parser *p, TokenType type);
static void free_token(Token *t);
static KryonNode* parse_window(Parser *p);
static KryonNode* parse_layout(Parser *p, const char *layout_type);
static KryonNode* parse_widget(Parser *p, const char *widget_type);
static KryonNode* parse_property_block(Parser *p, KryonNode *node);
static int execute_window(KryonNode *node);
static int execute_widget(KryonNode *node, struct KryonWindow *win);

/*
 * ========== Tokenizer ==========
 */

/*
 * Skip whitespace and comments
 */
static void skip_whitespace(Parser *p)
{
    while (p->pos < p->source_len) {
        char c = p->source[p->pos];

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (c == '\n') {
                p->line++;
                p->column = 1;
            } else {
                p->column++;
            }
            p->pos++;
        } else if (c == '#') {
            /* Skip comment until end of line */
            while (p->pos < p->source_len && p->source[p->pos] != '\n') {
                p->pos++;
            }
        } else {
            break;
        }
    }
}

/*
 * Read a quoted string
 */
static Token read_string(Parser *p)
{
    Token t;
    char quote = p->source[p->pos];
    int start_pos = p->pos + 1;
    int start_line = p->line;
    int start_col = p->column;

    t.type = TOKEN_STRING;
    t.line = start_line;
    t.column = start_col;
    t.value = NULL;

    p->pos++;  /* Skip opening quote */
    p->column++;

    /* Allocate buffer for string */
    {
        char *buf = NULL;
        int buf_len = 0;
        int buf_cap = 64;

        buf = (char *)malloc(buf_cap);
        if (buf == NULL) {
            t.type = TOKEN_ERROR;
            return t;
        }

        while (p->pos < p->source_len) {
            char c = p->source[p->pos];

            if (c == quote) {
                /* End of string */
                p->pos++;
                p->column++;
                break;
            } else if (c == '\\' && p->pos + 1 < p->source_len) {
                /* Escape sequence */
                p->pos++;
                p->column++;
                c = p->source[p->pos];

                switch (c) {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case 'r': c = '\r'; break;
                    default: break;
                }
            }

            /* Add character to buffer */
            if (buf_len >= buf_cap - 1) {
                buf_cap *= 2;
                {
                    char *new_buf = (char *)realloc(buf, buf_cap);
                    if (new_buf == NULL) {
                        free(buf);
                        t.type = TOKEN_ERROR;
                        return t;
                    }
                    buf = new_buf;
                }
            }

            buf[buf_len++] = c;
            p->pos++;
            p->column++;
        }

        buf[buf_len] = '\0';
        t.value = buf;
    }

    return t;
}

/*
 * Read a word (identifier or number)
 */
static Token read_word(Parser *p)
{
    Token t;
    int start_pos = p->pos;
    int start_line = p->line;
    int start_col = p->column;

    t.type = TOKEN_WORD;
    t.line = start_line;
    t.column = start_col;
    t.value = NULL;

    while (p->pos < p->source_len) {
        char c = p->source[p->pos];

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
            c == '{' || c == '}' || c == '=' || c == '#') {
            break;
        }

        p->pos++;
        p->column++;
    }

    /* Copy word */
    {
        int len = p->pos - start_pos;
        t.value = (char *)malloc(len + 1);
        if (t->value != NULL) {
            memcpy(t.value, p->source + start_pos, len);
            t.value[len] = '\0';
        } else {
            t.type = TOKEN_ERROR;
        }
    }

    return t;
}

/*
 * Get next token
 */
static Token next_token(Parser *p)
{
    Token t;

    skip_whitespace(p);

    if (p->pos >= p->source_len) {
        t.type = TOKEN_EOF;
        t.value = NULL;
        t.line = p->line;
        t.column = p->column;
        return t;
    }

    char c = p->source[p->pos];

    if (c == '"' || c == '\'') {
        t = read_string(p);
    } else if (c == '{') {
        t.type = TOKEN_LBRACE;
        t.value = NULL;
        t.line = p->line;
        t.column = p->column;
        p->pos++;
        p->column++;
    } else if (c == '}') {
        t.type = TOKEN_RBRACE;
        t.value = NULL;
        t.line = p->line;
        t.column = p->column;
        p->pos++;
        p->column++;
    } else if (c == '=') {
        t.type = TOKEN_EQUALS;
        t.value = NULL;
        t.line = p->line;
        t.column = p->column;
        p->pos++;
        p->column++;
    } else {
        t = read_word(p);
    }

    return t;
}

/*
 * Peek at next token without consuming
 */
static Token peek_token(Parser *p)
{
    int save_pos = p->pos;
    int save_line = p->line;
    int save_column = p->column;
    Token t = next_token(p);

    /* Restore position */
    p->pos = save_pos;
    p->line = save_line;
    p->column = save_column;

    return t;
}

/*
 * Expect a specific token type
 */
static int expect_token(Parser *p, TokenType type)
{
    if (p->current_token.type != type) {
        p->has_error = 1;
        snprintf(p->error_msg, sizeof(p->error_msg),
                "Expected %d at line %d, got %d",
                type, p->line, p->current_token.type);
        return -1;
    }
    return 0;
}

/*
 * Free token value
 */
static void free_token(Token *t)
{
    if (t != NULL && t->value != NULL) {
        free(t->value);
        t->value = NULL;
    }
}

/*
 * ========== AST Node Management ==========
 */

/*
 * Create new AST node
 */
static KryonNode* create_node(NodeType type)
{
    KryonNode *node = (KryonNode *)calloc(1, sizeof(KryonNode));
    if (node != NULL) {
        node->type = type;
        node->child_capacity = 64;
        node->children = (KryonNode **)malloc(
            node->child_capacity * sizeof(KryonNode *));
        if (node->children == NULL) {
            free(node);
            return NULL;
        }
    }
    return node;
}

/*
 * Free AST node recursively
 */
static void free_node(KryonNode *node)
{
    int i;

    if (node == NULL) {
        return;
    }

    /* Free children */
    for (i = 0; i < node->nchildren; i++) {
        free_node(node->children[i]);
    }

    /* Free strings */
    if (node->value != NULL) free(node->value);
    if (node->id != NULL) free(node->id);
    if (node->text != NULL) free(node->text);
    if (node->title != NULL) free(node->title);
    if (node->layout_type != NULL) free(node->layout_type);
    if (node->layout_align != NULL) free(node->layout_align);
    if (node->widget_type != NULL) free(node->widget_type);
    if (node->prop_rect != NULL) free(node->prop_rect);
    if (node->prop_color != NULL) free(node->prop_color);

    /* Free children array */
    if (node->children != NULL) {
        free(node->children);
    }

    free(node);
}

/*
 * Add child to node
 */
static int add_child(KryonNode *parent, KryonNode *child)
{
    if (parent == NULL || child == NULL) {
        return -1;
    }

    if (parent->nchildren >= parent->child_capacity) {
        return -1;
    }

    parent->children[parent->nchildren++] = child;
    return 0;
}

/*
 * ========== Parser ==========
 */

/*
 * Parse property block: { key=value key="value" }
 */
static KryonNode* parse_property_block(Parser *p, KryonNode *node)
{
    /* Check if there's a property block */
    {
        Token peek = peek_token(p);
        if (peek.type != TOKEN_LBRACE) {
            return node;  /* No property block */
        }
    }

    /* Consume { */
    free_token(&p->current_token);
    p->current_token = next_token(p);

    while (p->current_token.type != TOKEN_RBRACE &&
           p->current_token.type != TOKEN_EOF) {
        /* Parse property */
        if (p->current_token.type == TOKEN_WORD) {
            char *prop_name = p->current_token.value;
            free_token(&p->current_token);

            /* Expect = */
            p->current_token = next_token(p);
            if (p->current_token.type == TOKEN_EQUALS) {
                free_token(&p->current_token);

                /* Get value */
                p->current_token = next_token(p);

                if (p->current_token.type == TOKEN_WORD ||
                    p->current_token.type == TOKEN_STRING) {
                    char *prop_value = p->current_token.value;
                    free_token(&p->current_token);

                    /* Store property in node */
                    if (strcmp(prop_name, "rect") == 0) {
                        free(node->prop_rect);
                        node->prop_rect = prop_value;
                    } else if (strcmp(prop_name, "color") == 0) {
                        free(node->prop_color);
                        node->prop_color = prop_value;
                    } else if (strcmp(prop_name, "gap") == 0) {
                        node->layout_gap = atoi(prop_value);
                        free(prop_value);
                    } else if (strcmp(prop_name, "padding") == 0) {
                        node->layout_padding = atoi(prop_value);
                        free(prop_value);
                    } else if (strcmp(prop_name, "align") == 0) {
                        free(node->layout_align);
                        node->layout_align = prop_value;
                    } else if (strcmp(prop_name, "cols") == 0) {
                        node->layout_cols = atoi(prop_value);
                        free(prop_value);
                    } else if (strcmp(prop_name, "rows") == 0) {
                        node->layout_rows = atoi(prop_value);
                        free(prop_value);
                    } else {
                        free(prop_value);
                    }

                    free(prop_name);
                } else {
                    free(prop_name);
                    break;
                }
            } else {
                free(prop_name);
            }
        } else {
            break;
        }

        p->current_token = next_token(p);
    }

    /* Expect } */
    if (p->current_token.type != TOKEN_RBRACE) {
        return NULL;
    }
    free_token(&p->current_token);
    p->current_token = next_token(p);

    return node;
}

/*
 * Parse widget: widget_type id "text" { properties }
 *             OR widget_type id "text"
 *             OR widget_type id { properties }
 */
static KryonNode* parse_widget(Parser *p, const char *widget_type)
{
    KryonNode *node;

    node = create_node(NODE_WIDGET);
    if (node == NULL) {
        return NULL;
    }

    node->widget_type = (char *)malloc(strlen(widget_type) + 1);
    if (node->widget_type != NULL) {
        strcpy(node->widget_type, widget_type);
    }

    /* Get widget ID (optional, next token) */
    if (p->current_token.type == TOKEN_WORD ||
        p->current_token.type == TOKEN_STRING) {
        node->id = p->current_token.value;
        free_token(&p->current_token);
        p->current_token = next_token(p);
    }

    /* Get text (optional string) */
    if (p->current_token.type == TOKEN_STRING) {
        node->text = p->current_token.value;
        free_token(&p->current_token);
        p->current_token = next_token(p);
    }

    /* Parse property block if present */
    node = parse_property_block(p, node);

    return node;
}

/*
 * Parse layout: layout_type params { children }
 *             OR layout_type { children }
 */
static KryonNode* parse_layout(Parser *p, const char *layout_type)
{
    KryonNode *node;
    Token peek;

    node = create_node(NODE_LAYOUT);
    if (node == NULL) {
        return NULL;
    }

    node->layout_type = (char *)malloc(strlen(layout_type) + 1);
    if (node->layout_type != NULL) {
        strcpy(node->layout_type, layout_type);
    }

    /* Check for inline params: gap=10 padding=20 */
    peek = peek_token(p);
    if (peek.type == TOKEN_WORD) {
        while (peek.type == TOKEN_WORD) {
            /* Consume param name */
            char *param_name = p->current_token.value;
            free_token(&p->current_token);
            p->current_token = next_token(p);

            /* Expect = */
            if (p->current_token.type == TOKEN_EQUALS) {
                free_token(&p->current_token);
                p->current_token = next_token(p);

                /* Get value */
                if (p->current_token.type == TOKEN_WORD ||
                    p->current_token.type == TOKEN_STRING) {
                    char *param_value = p->current_token.value;
                    free_token(&p->current_token);

                    /* Store param */
                    if (strcmp(param_name, "gap") == 0) {
                        node->layout_gap = atoi(param_value);
                    } else if (strcmp(param_name, "padding") == 0) {
                        node->layout_padding = atoi(param_value);
                    } else if (strcmp(param_name, "align") == 0) {
                        free(node->layout_align);
                        node->layout_align = param_value;
                    } else if (strcmp(param_name, "cols") == 0) {
                        node->layout_cols = atoi(param_value);
                    } else if (strcmp(param_name, "rows") == 0) {
                        node->layout_rows = atoi(param_value);
                    } else {
                        free(param_value);
                    }

                    free(param_name);

                    peek = peek_token(p);
                    continue;
                }
            }

            free(param_name);
            break;
        }
    }

    /* Expect { */
    if (p->current_token.type != TOKEN_LBRACE) {
        free_node(node);
        return NULL;
    }
    free_token(&p->current_token);
    p->current_token = next_token(p);

    /* Parse children */
    while (p->current_token.type != TOKEN_RBRACE &&
           p->current_token.type != TOKEN_EOF) {
        KryonNode *child = NULL;

        if (p->current_token.type == TOKEN_WORD) {
            char *word = p->current_token.value;

            /* Check if it's a layout type */
            if (strcmp(word, "vbox") == 0 ||
                strcmp(word, "hbox") == 0 ||
                strcmp(word, "grid") == 0 ||
                strcmp(word, "absolute") == 0 ||
                strcmp(word, "stack") == 0) {
                child = parse_layout(p, word);
            } else {
                /* It's a widget */
                child = parse_widget(p, word);
            }

            if (child != NULL) {
                add_child(node, child);
            }
        } else {
            break;
        }
    }

    /* Expect } */
    if (p->current_token.type != TOKEN_RBRACE) {
        free_node(node);
        return NULL;
    }
    free_token(&p->current_token);
    p->current_token = next_token(p);

    return node;
}

/*
 * Parse window: window id "title" width height { content }
 *            OR window id "title" width height layout { content }
 */
static KryonNode* parse_window(Parser *p)
{
    KryonNode *node;

    /* Create window node */
    node = create_node(NODE_WINDOW);
    if (node == NULL) {
        return NULL;
    }

    /* Get window ID */
    if (p->current_token.type != TOKEN_WORD &&
        p->current_token.type != TOKEN_STRING) {
        free_node(node);
        return NULL;
    }

    node->id = p->current_token.value;
    free_token(&p->current_token);
    p->current_token = next_token(p);

    /* Get title */
    if (p->current_token.type != TOKEN_STRING) {
        free_node(node);
        return NULL;
    }

    node->title = p->current_token.value;
    free_token(&p->current_token);
    p->current_token = next_token(p);

    /* Get width */
    if (p->current_token.type != TOKEN_WORD) {
        free_node(node);
        return NULL;
    }

    node->width = atoi(p->current_token.value);
    free_token(&p->current_token);
    p->current_token = next_token(p);

    /* Get height */
    if (p->current_token.type != TOKEN_WORD) {
        free_node(node);
        return NULL;
    }

    node->height = atoi(p->current_token.value);
    free_token(&p->current_token);
    p->current_token = next_token(p);

    /* Expect { */
    if (p->current_token.type != TOKEN_LBRACE) {
        free_node(node);
        return NULL;
    }
    free_token(&p->current_token);
    p->current_token = next_token(p);

    /* Parse content - should be a layout or widgets */
    while (p->current_token.type != TOKEN_RBRACE &&
           p->current_token.type != TOKEN_EOF) {
        KryonNode *child = NULL;

        if (p->current_token.type == TOKEN_WORD) {
            char *word = p->current_token.value;

            /* Check if it's a layout type */
            if (strcmp(word, "vbox") == 0 ||
                strcmp(word, "hbox") == 0 ||
                strcmp(word, "grid") == 0 ||
                strcmp(word, "absolute") == 0 ||
                strcmp(word, "stack") == 0) {
                child = parse_layout(p, word);
            } else {
                /* It's a widget */
                child = parse_widget(p, word);
            }

            if (child != NULL) {
                add_child(node, child);
            }
        } else {
            break;
        }
    }

    /* Expect } */
    if (p->current_token.type != TOKEN_RBRACE) {
        free_node(node);
        return NULL;
    }
    free_token(&p->current_token);
    p->current_token = next_token(p);

    return node;
}

/*
 * Parse .kryon file
 */
KryonNode* kryon_parse_file(const char *filename)
{
    FILE *f;
    char *source;
    int len;
    KryonNode *ast;

    f = fopen(filename, "r");
    if (f == NULL) {
        fprintf(stderr, "Error: cannot open file: %s\n", filename);
        return NULL;
    }

    /* Read file */
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);

    source = (char *)malloc(len + 1);
    if (source == NULL) {
        fclose(f);
        return NULL;
    }

    if (fread(source, 1, len, f) != (size_t)len) {
        free(source);
        fclose(f);
        return NULL;
    }
    source[len] = '\0';
    fclose(f);

    /* Parse */
    ast = kryon_parse_string(source);

    free(source);

    return ast;
}

/*
 * Parse .kryon from string
 */
KryonNode* kryon_parse_string(const char *source)
{
    Parser p;
    KryonNode *root;

    /* Initialize parser */
    memset(&p, 0, sizeof(p));
    p.source_len = strlen(source);
    p.source = (char *)malloc(p.source_len + 1);
    if (p.source == NULL) {
        return NULL;
    }
    strcpy(p.source, source);
    p.pos = 0;
    p.line = 1;
    p.column = 1;
    p.has_error = 0;

    /* Create root node */
    root = create_node(NODE_ROOT);
    if (root == NULL) {
        free(p.source);
        return NULL;
    }

    /* Get first token */
    p.current_token = next_token(&p);

    /* Parse windows */
    while (p.current_token.type != TOKEN_EOF && !p.has_error) {
        if (p.current_token.type == TOKEN_WORD) {
            if (strcmp(p.current_token.value, "window") == 0) {
                free_token(&p.current_token);
                p.current_token = next_token(&p);

                {
                    KryonNode *window = parse_window(&p);
                    if (window != NULL) {
                        add_child(root, window);
                    }
                }
            } else {
                /* Unknown top-level keyword */
                free_token(&p.current_token);
                p.current_token = next_token(&p);
            }
        } else {
            break;
        }
    }

    /* Cleanup */
    free(p.source);
    free_token(&p.current_token);

    if (p.has_error) {
        fprintf(stderr, "Parse error: %s\n", p.error_msg);
        free_node(root);
        return NULL;
    }

    return root;
}

/*
 * Free parsed AST
 */
void kryon_free_ast(KryonNode *ast)
{
    free_node(ast);
}

/*
 * ========== Executor ==========
 */

/*
 * Execute widget node
 */
static int execute_widget(KryonNode *node, struct KryonWindow *win)
{
    struct KryonWidget *widget;
    WidgetType type;

    if (node == NULL || win == NULL) {
        return -1;
    }

    /* Get widget type */
    type = widget_type_from_string(node->widget_type);
    if (type == WIDGET_UNKNOWN) {
        fprintf(stderr, "Warning: unknown widget type: %s\n",
                node->widget_type);
        return -1;
    }

    /* Create widget */
    widget = widget_create(type, node->id, win);
    if (widget == NULL) {
        fprintf(stderr, "Error: failed to create widget: %s\n",
                node->widget_type);
        return -1;
    }

    /* Set text */
    if (node->text != NULL) {
        free(widget->prop_text);
        widget->prop_text = (char *)malloc(strlen(node->text) + 1);
        if (widget->prop_text != NULL) {
            strcpy(widget->prop_text, node->text);
        }
    }

    /* Set rect */
    if (node->prop_rect != NULL) {
        free(widget->prop_rect);
        widget->prop_rect = (char *)malloc(strlen(node->prop_rect) + 1);
        if (widget->prop_rect != NULL) {
            strcpy(widget->prop_rect, node->prop_rect);
        }
    }

    /* Add widget to window */
    window_add_widget(win, widget);

    fprintf(stderr, "  Created widget %u: %s (text='%s')\n",
            widget->id, node->widget_type,
            widget->prop_text ? widget->prop_text : "");

    return 0;
}

/*
 * Recursively execute layout node
 */
static int execute_layout(KryonNode *node, struct KryonWindow *win)
{
    int i;

    if (node == NULL || win == NULL) {
        return -1;
    }

    /* Execute all children (widgets and nested layouts) */
    for (i = 0; i < node->nchildren; i++) {
        KryonNode *child = node->children[i];

        if (child->type == NODE_WIDGET) {
            execute_widget(child, win);
        } else if (child->type == NODE_LAYOUT) {
            execute_layout(child, win);
        }
    }

    return 0;
}

/*
 * Execute window node
 */
static int execute_window(KryonNode *node)
{
    struct KryonWindow *win;
    int i;

    if (node == NULL || node->type != NODE_WINDOW) {
        return -1;
    }

    /* Create window */
    win = window_create(node->title, node->width, node->height);
    if (win == NULL) {
        fprintf(stderr, "Error: failed to create window: %s\n",
                node->title ? node->title : "(null)");
        return -1;
    }

    fprintf(stderr, "Created window %u: %s (%dx%d)\n",
            win->id, win->title, node->width, node->height);

    /* Execute children (layouts/widgets) */
    for (i = 0; i < node->nchildren; i++) {
        KryonNode *child = node->children[i];

        if (child->type == NODE_LAYOUT) {
            execute_layout(child, win);
        } else if (child->type == NODE_WIDGET) {
            execute_widget(child, win);
        }
    }

    return 0;
}

/*
 * Execute parsed AST
 */
int kryon_execute_ast(KryonNode *ast)
{
    int i;
    int count = 0;

    if (ast == NULL) {
        return -1;
    }

    /* Execute all top-level windows */
    for (i = 0; i < ast->nchildren; i++) {
        KryonNode *child = ast->children[i];

        if (child->type == NODE_WINDOW) {
            if (execute_window(child) == 0) {
                count++;
            }
        }
    }

    return count;
}

/*
 * Convenience: load and execute .kryon file
 */
int kryon_load_file(const char *filename)
{
    KryonNode *ast;
    int result;

    fprintf(stderr, "Loading .kryon file: %s\n", filename);

    ast = kryon_parse_file(filename);
    if (ast == NULL) {
        return -1;
    }

    result = kryon_execute_ast(ast);

    kryon_free_ast(ast);

    return result;
}
