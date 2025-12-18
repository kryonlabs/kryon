/*
 * ir_markdown_parser.c
 *
 * AST-based markdown parser for Kryon core.
 * Builds a tree of MdNode structures from markdown source text.
 *
 * This parser supports:
 * - Headings (# through ######) with auto-generated anchor IDs
 * - Paragraphs with inline formatting (bold, italic, code, links)
 * - Unordered lists (-, *, +)
 * - Ordered lists (1., 2., etc.)
 * - Fenced code blocks (```)
 * - Blockquotes (>)
 * - Tables (GFM style with | delimiters)
 * - Thematic breaks (---, ***, ___)
 * - Inline elements: bold, italic, code spans, links, images
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#include "ir_markdown_ast.h"
#include "ir_markdown_parser.h"

// Forward declaration for AST to IR converter (defined in ir_markdown_to_ir.c)
extern IRComponent* ir_markdown_ast_to_ir(MdNode* ast_root);

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

// Default allocation sizes
#define DEFAULT_NODE_POOL_SIZE 256
#define DEFAULT_INLINE_POOL_SIZE 512
#define DEFAULT_TEXT_BUFFER_SIZE 8192

// Parser state for memory management
typedef struct {
    const char* input;
    size_t input_length;
    size_t position;

    // Node allocation
    MdNode* node_pool;
    size_t node_pool_size;
    size_t node_pool_used;

    // Inline allocation
    MdInline* inline_pool;
    size_t inline_pool_size;
    size_t inline_pool_used;

    // Text buffer for string copies
    char* text_buffer;
    size_t text_buffer_size;
    size_t text_buffer_used;
} parser_state_t;

static parser_state_t* parser_create(const char* input, size_t length) {
    parser_state_t* p = (parser_state_t*)malloc(sizeof(parser_state_t));
    if (!p) return NULL;

    p->input = input;
    p->input_length = length;
    p->position = 0;

    p->node_pool = (MdNode*)calloc(DEFAULT_NODE_POOL_SIZE, sizeof(MdNode));
    p->node_pool_size = DEFAULT_NODE_POOL_SIZE;
    p->node_pool_used = 0;

    p->inline_pool = (MdInline*)calloc(DEFAULT_INLINE_POOL_SIZE, sizeof(MdInline));
    p->inline_pool_size = DEFAULT_INLINE_POOL_SIZE;
    p->inline_pool_used = 0;

    p->text_buffer = (char*)malloc(DEFAULT_TEXT_BUFFER_SIZE);
    p->text_buffer_size = DEFAULT_TEXT_BUFFER_SIZE;
    p->text_buffer_used = 0;

    if (!p->node_pool || !p->inline_pool || !p->text_buffer) {
        free(p->node_pool);
        free(p->inline_pool);
        free(p->text_buffer);
        free(p);
        return NULL;
    }

    return p;
}

static void parser_destroy(parser_state_t* p) {
    if (!p) return;
    free(p->node_pool);
    free(p->inline_pool);
    free(p->text_buffer);
    free(p);
}

static MdNode* alloc_node(parser_state_t* p, MdBlockType type) {
    if (p->node_pool_used >= p->node_pool_size) {
        // Expand pool
        size_t new_size = p->node_pool_size * 2;
        MdNode* new_pool = (MdNode*)realloc(p->node_pool, new_size * sizeof(MdNode));
        if (!new_pool) return NULL;
        memset(new_pool + p->node_pool_size, 0, (new_size - p->node_pool_size) * sizeof(MdNode));
        p->node_pool = new_pool;
        p->node_pool_size = new_size;
    }

    MdNode* node = &p->node_pool[p->node_pool_used++];
    memset(node, 0, sizeof(MdNode));
    node->type = type;
    return node;
}

static MdInline* alloc_inline(parser_state_t* p, MdInlineType type) {
    if (p->inline_pool_used >= p->inline_pool_size) {
        // Expand pool
        size_t new_size = p->inline_pool_size * 2;
        MdInline* new_pool = (MdInline*)realloc(p->inline_pool, new_size * sizeof(MdInline));
        if (!new_pool) return NULL;
        memset(new_pool + p->inline_pool_size, 0, (new_size - p->inline_pool_size) * sizeof(MdInline));
        p->inline_pool = new_pool;
        p->inline_pool_size = new_size;
    }

    MdInline* inl = &p->inline_pool[p->inline_pool_used++];
    memset(inl, 0, sizeof(MdInline));
    inl->type = type;
    return inl;
}

static char* alloc_text(parser_state_t* p, const char* text, size_t length) {
    if (p->text_buffer_used + length + 1 > p->text_buffer_size) {
        // Expand buffer
        size_t new_size = p->text_buffer_size * 2;
        while (p->text_buffer_used + length + 1 > new_size) new_size *= 2;
        char* new_buffer = (char*)realloc(p->text_buffer, new_size);
        if (!new_buffer) return NULL;
        p->text_buffer = new_buffer;
        p->text_buffer_size = new_size;
    }

    char* result = p->text_buffer + p->text_buffer_used;
    memcpy(result, text, length);
    result[length] = '\0';
    p->text_buffer_used += length + 1;
    return result;
}

// ============================================================================
// TREE OPERATIONS
// ============================================================================

static void add_child(MdNode* parent, MdNode* child) {
    if (!parent || !child) return;

    child->parent = parent;
    child->next = NULL;

    if (parent->last_child) {
        child->prev = parent->last_child;
        parent->last_child->next = child;
        parent->last_child = child;
    } else {
        child->prev = NULL;
        parent->first_child = child;
        parent->last_child = child;
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

static bool is_newline(char c) {
    return c == '\n';
}

static size_t skip_whitespace(const char* text, size_t len, size_t pos) {
    while (pos < len && is_whitespace(text[pos])) pos++;
    return pos;
}

static size_t get_line_length(const char* text, size_t len, size_t pos) {
    size_t start = pos;
    while (pos < len && text[pos] != '\n') pos++;
    return pos - start;
}

static size_t skip_to_next_line(const char* text, size_t len, size_t pos) {
    while (pos < len && text[pos] != '\n') pos++;
    if (pos < len && text[pos] == '\n') pos++;
    return pos;
}

// ============================================================================
// TABLE PARSING HELPERS
// ============================================================================

static bool is_table_separator_line(const char* line, size_t len, MdAlignment* alignments, int* column_count) {
    size_t pos = 0;
    *column_count = 0;

    // Skip leading whitespace
    while (pos < len && is_whitespace(line[pos])) pos++;

    // Must start with pipe
    if (pos >= len || line[pos] != '|') return false;
    pos++;

    while (pos < len && *column_count < 64) {
        // Skip whitespace
        while (pos < len && is_whitespace(line[pos])) pos++;
        if (pos >= len || line[pos] == '\n') break;

        // Check for colon (left align marker)
        bool left_colon = (line[pos] == ':');
        if (left_colon) pos++;

        // Must have at least one dash
        size_t dash_count = 0;
        while (pos < len && line[pos] == '-') {
            dash_count++;
            pos++;
        }
        if (dash_count == 0) return false;

        // Check for colon (right align marker)
        bool right_colon = (pos < len && line[pos] == ':');
        if (right_colon) pos++;

        // Determine alignment
        if (left_colon && right_colon) {
            alignments[*column_count] = MD_ALIGN_CENTER;
        } else if (right_colon) {
            alignments[*column_count] = MD_ALIGN_RIGHT;
        } else {
            alignments[*column_count] = MD_ALIGN_LEFT;
        }
        (*column_count)++;

        // Skip whitespace
        while (pos < len && is_whitespace(line[pos])) pos++;

        // Check for pipe or end of line
        if (pos >= len || line[pos] == '\n') break;
        if (line[pos] != '|') return false;
        pos++;
    }

    return *column_count > 0;
}

static bool is_table_row_line(const char* line, size_t len) {
    size_t pos = 0;
    while (pos < len && is_whitespace(line[pos])) pos++;
    return pos < len && line[pos] == '|';
}

// ============================================================================
// INLINE PARSING
// ============================================================================

static MdInline* parse_inlines(parser_state_t* p, const char* text, size_t len) {
    MdInline* first = NULL;
    MdInline* last = NULL;

    size_t pos = 0;
    size_t text_start = 0;

    while (pos < len) {
        // Check for inline code
        if (text[pos] == '`') {
            // Emit pending text
            if (pos > text_start) {
                MdInline* inl = alloc_inline(p, MD_INLINE_TEXT);
                if (inl) {
                    inl->data.text.text = alloc_text(p, text + text_start, pos - text_start);
                    inl->data.text.length = pos - text_start;
                    if (last) { last->next = inl; last = inl; }
                    else { first = last = inl; }
                }
            }

            pos++;
            size_t code_start = pos;
            while (pos < len && text[pos] != '`') pos++;

            MdInline* inl = alloc_inline(p, MD_INLINE_CODE_SPAN);
            if (inl) {
                inl->data.text.text = alloc_text(p, text + code_start, pos - code_start);
                inl->data.text.length = pos - code_start;
                if (last) { last->next = inl; last = inl; }
                else { first = last = inl; }
            }

            if (pos < len) pos++;  // Skip closing `
            text_start = pos;
        }
        // Check for bold (**text**)
        else if (pos + 1 < len && text[pos] == '*' && text[pos + 1] == '*') {
            // Emit pending text
            if (pos > text_start) {
                MdInline* inl = alloc_inline(p, MD_INLINE_TEXT);
                if (inl) {
                    inl->data.text.text = alloc_text(p, text + text_start, pos - text_start);
                    inl->data.text.length = pos - text_start;
                    if (last) { last->next = inl; last = inl; }
                    else { first = last = inl; }
                }
            }

            pos += 2;
            size_t bold_start = pos;
            while (pos + 1 < len && !(text[pos] == '*' && text[pos + 1] == '*')) pos++;

            MdInline* inl = alloc_inline(p, MD_INLINE_STRONG);
            if (inl) {
                inl->data.text.text = alloc_text(p, text + bold_start, pos - bold_start);
                inl->data.text.length = pos - bold_start;
                if (last) { last->next = inl; last = inl; }
                else { first = last = inl; }
            }

            if (pos + 1 < len) pos += 2;  // Skip closing **
            text_start = pos;
        }
        // Check for italic (*text*)
        else if (text[pos] == '*') {
            // Emit pending text
            if (pos > text_start) {
                MdInline* inl = alloc_inline(p, MD_INLINE_TEXT);
                if (inl) {
                    inl->data.text.text = alloc_text(p, text + text_start, pos - text_start);
                    inl->data.text.length = pos - text_start;
                    if (last) { last->next = inl; last = inl; }
                    else { first = last = inl; }
                }
            }

            pos++;
            size_t italic_start = pos;
            while (pos < len && text[pos] != '*') pos++;

            MdInline* inl = alloc_inline(p, MD_INLINE_EMPHASIS);
            if (inl) {
                inl->data.text.text = alloc_text(p, text + italic_start, pos - italic_start);
                inl->data.text.length = pos - italic_start;
                if (last) { last->next = inl; last = inl; }
                else { first = last = inl; }
            }

            if (pos < len) pos++;  // Skip closing *
            text_start = pos;
        }
        // Check for link [text](url)
        else if (text[pos] == '[') {
            // Emit pending text
            if (pos > text_start) {
                MdInline* inl = alloc_inline(p, MD_INLINE_TEXT);
                if (inl) {
                    inl->data.text.text = alloc_text(p, text + text_start, pos - text_start);
                    inl->data.text.length = pos - text_start;
                    if (last) { last->next = inl; last = inl; }
                    else { first = last = inl; }
                }
            }

            pos++;
            size_t link_text_start = pos;
            while (pos < len && text[pos] != ']') pos++;
            size_t link_text_len = pos - link_text_start;

            if (pos + 1 < len && text[pos] == ']' && text[pos + 1] == '(') {
                pos += 2;  // Skip ](
                size_t url_start = pos;
                while (pos < len && text[pos] != ')') pos++;
                size_t url_len = pos - url_start;

                MdInline* inl = alloc_inline(p, MD_INLINE_LINK);
                if (inl) {
                    inl->data.link.url = alloc_text(p, text + url_start, url_len);
                    inl->data.link.url_length = url_len;
                    // Store link text in title field (repurposed)
                    inl->data.link.title = alloc_text(p, text + link_text_start, link_text_len);
                    inl->data.link.title_length = link_text_len;
                    if (last) { last->next = inl; last = inl; }
                    else { first = last = inl; }
                }

                if (pos < len) pos++;  // Skip closing )
            } else {
                // Not a valid link, treat [ as text
                MdInline* inl = alloc_inline(p, MD_INLINE_TEXT);
                if (inl) {
                    inl->data.text.text = alloc_text(p, "[", 1);
                    inl->data.text.length = 1;
                    if (last) { last->next = inl; last = inl; }
                    else { first = last = inl; }
                }
                pos = link_text_start;
            }
            text_start = pos;
        }
        // Check for image ![alt](url)
        else if (pos + 1 < len && text[pos] == '!' && text[pos + 1] == '[') {
            // Emit pending text
            if (pos > text_start) {
                MdInline* inl = alloc_inline(p, MD_INLINE_TEXT);
                if (inl) {
                    inl->data.text.text = alloc_text(p, text + text_start, pos - text_start);
                    inl->data.text.length = pos - text_start;
                    if (last) { last->next = inl; last = inl; }
                    else { first = last = inl; }
                }
            }

            pos += 2;  // Skip ![
            size_t alt_start = pos;
            while (pos < len && text[pos] != ']') pos++;
            size_t alt_len = pos - alt_start;

            if (pos + 1 < len && text[pos] == ']' && text[pos + 1] == '(') {
                pos += 2;  // Skip ](
                size_t url_start = pos;
                while (pos < len && text[pos] != ')') pos++;
                size_t url_len = pos - url_start;

                MdInline* inl = alloc_inline(p, MD_INLINE_IMAGE);
                if (inl) {
                    inl->data.image.url = alloc_text(p, text + url_start, url_len);
                    inl->data.image.url_length = url_len;
                    inl->data.image.alt = alloc_text(p, text + alt_start, alt_len);
                    inl->data.image.alt_length = alt_len;
                    if (last) { last->next = inl; last = inl; }
                    else { first = last = inl; }
                }

                if (pos < len) pos++;  // Skip closing )
            }
            text_start = pos;
        }
        else {
            pos++;
        }
    }

    // Emit remaining text
    if (pos > text_start) {
        MdInline* inl = alloc_inline(p, MD_INLINE_TEXT);
        if (inl) {
            inl->data.text.text = alloc_text(p, text + text_start, pos - text_start);
            inl->data.text.length = pos - text_start;
            if (last) { last->next = inl; last = inl; }
            else { first = last = inl; }
        }
    }

    return first;
}

// ============================================================================
// BLOCK PARSING
// ============================================================================

static MdNode* parse_table(parser_state_t* p, MdNode* parent,
                              const char* first_line, size_t first_line_len,
                              MdAlignment* alignments, int column_count) {
    MdNode* table = alloc_node(p, MD_BLOCK_TABLE);
    if (!table) return NULL;

    table->data.table.columns = column_count;
    table->data.table.rows = 0;
    table->data.table.alignments = (MdAlignment*)malloc(column_count * sizeof(MdAlignment));
    if (table->data.table.alignments) {
        memcpy(table->data.table.alignments, alignments, column_count * sizeof(MdAlignment));
    }
    table->data.table.header = true;

    // Parse header row
    MdNode* header_row = alloc_node(p, MD_BLOCK_TABLE_ROW);
    if (header_row) {
        size_t pos = 0;

        // Skip leading whitespace and pipe
        while (pos < first_line_len && is_whitespace(first_line[pos])) pos++;
        if (pos < first_line_len && first_line[pos] == '|') pos++;

        for (int col = 0; col < column_count && pos < first_line_len; col++) {
            // Find cell content
            size_t cell_start = pos;
            while (pos < first_line_len && first_line[pos] != '|' && first_line[pos] != '\n') pos++;

            // Trim whitespace
            size_t cell_end = pos;
            while (cell_end > cell_start && is_whitespace(first_line[cell_end - 1])) cell_end--;
            while (cell_start < cell_end && is_whitespace(first_line[cell_start])) cell_start++;

            MdNode* cell = alloc_node(p, MD_BLOCK_TABLE_CELL);
            if (cell) {
                // Add paragraph with text content
                MdNode* para = alloc_node(p, MD_BLOCK_PARAGRAPH);
                if (para) {
                    para->data.paragraph.text = alloc_text(p, first_line + cell_start, cell_end - cell_start);
                    para->data.paragraph.length = cell_end - cell_start;
                    para->data.paragraph.first_inline = parse_inlines(p, first_line + cell_start, cell_end - cell_start);
                    add_child(cell, para);
                }
                add_child(header_row, cell);
            }

            // Skip pipe
            if (pos < first_line_len && first_line[pos] == '|') pos++;
        }

        add_child(table, header_row);
        table->data.table.rows++;
    }

    return table;
}

static void parse_table_data_row(parser_state_t* p, MdNode* table,
                                 const char* line, size_t line_len, int column_count) {
    MdNode* row = alloc_node(p, MD_BLOCK_TABLE_ROW);
    if (!row) return;

    size_t pos = 0;

    // Skip leading whitespace and pipe
    while (pos < line_len && is_whitespace(line[pos])) pos++;
    if (pos < line_len && line[pos] == '|') pos++;

    for (int col = 0; col < column_count && pos < line_len; col++) {
        // Find cell content
        size_t cell_start = pos;
        while (pos < line_len && line[pos] != '|' && line[pos] != '\n') pos++;

        // Trim whitespace
        size_t cell_end = pos;
        while (cell_end > cell_start && is_whitespace(line[cell_end - 1])) cell_end--;
        while (cell_start < cell_end && is_whitespace(line[cell_start])) cell_start++;

        MdNode* cell = alloc_node(p, MD_BLOCK_TABLE_CELL);
        if (cell) {
            // Add paragraph with text content
            MdNode* para = alloc_node(p, MD_BLOCK_PARAGRAPH);
            if (para) {
                para->data.paragraph.text = alloc_text(p, line + cell_start, cell_end - cell_start);
                para->data.paragraph.length = cell_end - cell_start;
                para->data.paragraph.first_inline = parse_inlines(p, line + cell_start, cell_end - cell_start);
                add_child(cell, para);
            }
            add_child(row, cell);
        }

        // Skip pipe
        if (pos < line_len && line[pos] == '|') pos++;
    }

    add_child(table, row);
    table->data.table.rows++;
}

static MdNode* parse_document(parser_state_t* p) {
    MdNode* doc = alloc_node(p, MD_BLOCK_DOCUMENT);
    if (!doc) return NULL;

    const char* text = p->input;
    size_t len = p->input_length;
    size_t pos = 0;

    bool in_code_block = false;
    char* code_buffer = NULL;
    size_t code_len = 0;
    size_t code_cap = 0;
    char* code_language = NULL;

    while (pos < len) {
        // Skip leading whitespace on line
        size_t line_start = pos;
        while (pos < len && is_whitespace(text[pos])) pos++;

        size_t line_len = get_line_length(text, len, pos);

        // Check for fenced code block (```)
        if (pos + 2 < len && text[pos] == '`' && text[pos + 1] == '`' && text[pos + 2] == '`') {
            if (in_code_block) {
                // End code block
                MdNode* code = alloc_node(p, MD_BLOCK_CODE_BLOCK);
                if (code) {
                    code->data.code_block.code = code_buffer ? alloc_text(p, code_buffer, code_len) : alloc_text(p, "", 0);
                    code->data.code_block.length = code_len;
                    code->data.code_block.language = code_language;
                    code->data.code_block.fenced = true;
                    add_child(doc, code);
                }
                free(code_buffer);
                code_buffer = NULL;
                code_len = 0;
                code_cap = 0;
                code_language = NULL;
                in_code_block = false;
            } else {
                // Start code block - capture language
                pos += 3;
                size_t lang_start = pos;
                while (pos < len && text[pos] != '\n') pos++;
                if (pos > lang_start) {
                    code_language = alloc_text(p, text + lang_start, pos - lang_start);
                }
                in_code_block = true;
            }
            pos = skip_to_next_line(text, len, line_start);
            continue;
        }

        if (in_code_block) {
            // Accumulate code block content
            if (!code_buffer) {
                code_cap = 1024;
                code_buffer = (char*)malloc(code_cap);
                code_len = 0;
            }
            if (code_len + line_len + 2 > code_cap) {
                code_cap = (code_len + line_len + 2) * 2;
                code_buffer = (char*)realloc(code_buffer, code_cap);
            }
            if (code_buffer) {
                memcpy(code_buffer + code_len, text + pos, line_len);
                code_len += line_len;
                code_buffer[code_len++] = '\n';
                code_buffer[code_len] = '\0';
            }
            pos = skip_to_next_line(text, len, pos);
            continue;
        }

        // Empty line
        if (line_len == 0 || (line_len == 1 && text[pos] == '\n')) {
            pos = skip_to_next_line(text, len, pos);
            continue;
        }

        // Heading (# through ######)
        if (text[pos] == '#') {
            int level = 0;
            size_t h_pos = pos;
            while (h_pos < len && text[h_pos] == '#' && level < 6) {
                level++;
                h_pos++;
            }

            // Must have space after #
            if (h_pos < len && is_whitespace(text[h_pos])) {
                while (h_pos < len && is_whitespace(text[h_pos])) h_pos++;

                size_t heading_start = h_pos;
                size_t heading_end = pos + line_len;

                // Trim trailing # and whitespace
                while (heading_end > heading_start && (text[heading_end - 1] == '#' || is_whitespace(text[heading_end - 1]))) {
                    heading_end--;
                }

                MdBlockType type = (MdBlockType)(MD_BLOCK_HEADING_1 + level - 1);
                MdNode* heading = alloc_node(p, type);
                if (heading) {
                    heading->data.heading.text = alloc_text(p, text + heading_start, heading_end - heading_start);
                    heading->data.heading.length = heading_end - heading_start;
                    heading->data.heading.level = level;
                    add_child(doc, heading);
                }

                pos = skip_to_next_line(text, len, pos);
                continue;
            }
        }

        // Thematic break (--- or *** or ___)
        if (line_len >= 3) {
            char break_char = text[pos];
            if (break_char == '-' || break_char == '*' || break_char == '_') {
                size_t count = 0;
                size_t check_pos = pos;
                while (check_pos < pos + line_len) {
                    if (text[check_pos] == break_char) count++;
                    else if (!is_whitespace(text[check_pos])) break;
                    check_pos++;
                }

                if (count >= 3 && check_pos >= pos + line_len) {
                    MdNode* hr = alloc_node(p, MD_BLOCK_THEMATIC_BREAK);
                    if (hr) add_child(doc, hr);
                    pos = skip_to_next_line(text, len, pos);
                    continue;
                }
            }
        }

        // Unordered list (-, *, +)
        if ((text[pos] == '-' || text[pos] == '*' || text[pos] == '+') &&
            pos + 1 < len && is_whitespace(text[pos + 1])) {

            MdNode* list = alloc_node(p, MD_BLOCK_LIST_UNORDERED);
            if (list) {
                list->data.list.ordered = false;

                while (pos < len) {
                    // Check if current line is a list item
                    size_t item_pos = pos;
                    while (item_pos < len && is_whitespace(text[item_pos])) item_pos++;

                    if (item_pos >= len) break;

                    if ((text[item_pos] == '-' || text[item_pos] == '*' || text[item_pos] == '+') &&
                        item_pos + 1 < len && is_whitespace(text[item_pos + 1])) {

                        item_pos += 2;  // Skip marker and space
                        size_t item_line_len = get_line_length(text, len, item_pos);

                        MdNode* item = alloc_node(p, MD_BLOCK_LIST_ITEM);
                        if (item) {
                            MdNode* para = alloc_node(p, MD_BLOCK_PARAGRAPH);
                            if (para) {
                                para->data.paragraph.text = alloc_text(p, text + item_pos, item_line_len);
                                para->data.paragraph.length = item_line_len;
                                para->data.paragraph.first_inline = parse_inlines(p, text + item_pos, item_line_len);
                                add_child(item, para);
                            }
                            add_child(list, item);
                        }

                        pos = skip_to_next_line(text, len, item_pos);
                    } else {
                        break;
                    }
                }

                add_child(doc, list);
            }
            continue;
        }

        // Ordered list (1. 2. etc.)
        if (text[pos] >= '0' && text[pos] <= '9') {
            size_t num_start = pos;
            size_t num_pos = pos;
            while (num_pos < len && text[num_pos] >= '0' && text[num_pos] <= '9') num_pos++;

            if (num_pos < len && text[num_pos] == '.' &&
                num_pos + 1 < len && is_whitespace(text[num_pos + 1])) {

                int start_num = atoi(text + num_start);

                MdNode* list = alloc_node(p, MD_BLOCK_LIST_ORDERED);
                if (list) {
                    list->data.list.ordered = true;
                    list->data.list.start = start_num;

                    while (pos < len) {
                        // Check if current line is a list item
                        size_t item_pos = pos;
                        while (item_pos < len && is_whitespace(text[item_pos])) item_pos++;

                        if (item_pos >= len) break;

                        size_t digit_start = item_pos;
                        while (item_pos < len && text[item_pos] >= '0' && text[item_pos] <= '9') item_pos++;

                        if (item_pos > digit_start && item_pos < len && text[item_pos] == '.' &&
                            item_pos + 1 < len && is_whitespace(text[item_pos + 1])) {

                            item_pos += 2;  // Skip . and space
                            size_t item_line_len = get_line_length(text, len, item_pos);

                            MdNode* item = alloc_node(p, MD_BLOCK_LIST_ITEM);
                            if (item) {
                                MdNode* para = alloc_node(p, MD_BLOCK_PARAGRAPH);
                                if (para) {
                                    para->data.paragraph.text = alloc_text(p, text + item_pos, item_line_len);
                                    para->data.paragraph.length = item_line_len;
                                    para->data.paragraph.first_inline = parse_inlines(p, text + item_pos, item_line_len);
                                    add_child(item, para);
                                }
                                add_child(list, item);
                            }

                            pos = skip_to_next_line(text, len, item_pos);
                        } else {
                            break;
                        }
                    }

                    add_child(doc, list);
                }
                continue;
            }
        }

        // Blockquote (>)
        if (text[pos] == '>') {
            MdNode* quote = alloc_node(p, MD_BLOCK_BLOCKQUOTE);
            if (quote) {
                size_t quote_pos = pos + 1;
                if (quote_pos < len && is_whitespace(text[quote_pos])) quote_pos++;

                size_t quote_line_len = get_line_length(text, len, quote_pos);

                quote->data.blockquote.text = alloc_text(p, text + quote_pos, quote_line_len);
                quote->data.blockquote.length = quote_line_len;

                add_child(doc, quote);
            }
            pos = skip_to_next_line(text, len, pos);
            continue;
        }

        // Table (line starts with |)
        if (text[pos] == '|') {
            size_t next_line_start = skip_to_next_line(text, len, pos);

            if (next_line_start < len) {
                size_t next_line_len = get_line_length(text, len, next_line_start);
                MdAlignment alignments[64];
                int column_count = 0;

                if (is_table_separator_line(text + next_line_start, next_line_len, alignments, &column_count)) {
                    // Valid table
                    MdNode* table = parse_table(p, doc, text + pos, line_len, alignments, column_count);
                    if (table) {
                        add_child(doc, table);

                        // Skip header and separator lines
                        pos = skip_to_next_line(text, len, next_line_start);

                        // Parse data rows
                        while (pos < len) {
                            size_t data_pos = pos;
                            while (data_pos < len && is_whitespace(text[data_pos])) data_pos++;

                            size_t data_line_len = get_line_length(text, len, data_pos);

                            if (!is_table_row_line(text + data_pos, data_line_len)) {
                                break;
                            }

                            parse_table_data_row(p, table, text + data_pos, data_line_len, column_count);
                            pos = skip_to_next_line(text, len, data_pos);
                        }
                    }
                    continue;
                }
            }
        }

        // Default: paragraph
        {
            MdNode* para = alloc_node(p, MD_BLOCK_PARAGRAPH);
            if (para) {
                para->data.paragraph.text = alloc_text(p, text + pos, line_len);
                para->data.paragraph.length = line_len;
                para->data.paragraph.first_inline = parse_inlines(p, text + pos, line_len);
                add_child(doc, para);
            }
        }

        pos = skip_to_next_line(text, len, pos);
    }

    // Handle unclosed code block
    if (in_code_block && code_buffer) {
        MdNode* code = alloc_node(p, MD_BLOCK_CODE_BLOCK);
        if (code) {
            code->data.code_block.code = alloc_text(p, code_buffer, code_len);
            code->data.code_block.length = code_len;
            code->data.code_block.language = code_language;
            code->data.code_block.fenced = true;
            add_child(doc, code);
        }
        free(code_buffer);
    }

    return doc;
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

// Global parser state for memory management
static parser_state_t* g_parser = NULL;

// Internal helper that returns MdNode AST
static MdNode* md_parse_internal(const char* input, size_t length) {
    fprintf(stderr, "=== md_parse_internal: START, input=%p, length=%zu\n", (void*)input, length);
    fflush(stderr);

    if (!input) {
        fprintf(stderr, "=== md_parse_internal: input is NULL!\n");
        fflush(stderr);
        return NULL;
    }
    if (length == 0) length = strlen(input);

    fprintf(stderr, "=== md_parse_internal: Creating parser...\n");
    fflush(stderr);

    parser_state_t* p = parser_create(input, length);
    if (!p) {
        fprintf(stderr, "=== md_parse_internal: parser_create failed!\n");
        fflush(stderr);
        return NULL;
    }

    fprintf(stderr, "=== md_parse_internal: Checking g_parser...\n");
    fflush(stderr);

    // Store globally for cleanup
    if (g_parser) {
        fprintf(stderr, "=== md_parse_internal: Destroying old parser at %p\n", (void*)g_parser);
        fflush(stderr);
        parser_destroy(g_parser);
    }
    g_parser = p;

    fprintf(stderr, "=== md_parse_internal: Parsing document...\n");
    fflush(stderr);

    return parse_document(p);
}

// Stub implementation - calls internal parser
IRComponent* ir_markdown_parse(const char* source, size_t length) {
    fprintf(stderr, ">>> ENTERED ir_markdown_parse <<<\n");
    fflush(stderr);
    fprintf(stderr, "=== ir_markdown_parse: Parsing AST...\n");
    fflush(stderr);

    // Parse markdown source to AST
    MdNode* ast = md_parse_internal(source, length);
    if (!ast) {
        fprintf(stderr, "=== ir_markdown_parse: AST parse failed\n");
        fflush(stderr);
        return NULL;
    }

    fprintf(stderr, "=== ir_markdown_parse: Converting AST to IR...\n");
    fflush(stderr);

    // Convert AST to IR component tree
    IRComponent* root = ir_markdown_ast_to_ir(ast);

    fprintf(stderr, "=== ir_markdown_parse: Done (AST will be freed with parser)\n");
    fprintf(stderr, "=== ir_markdown_parse: root=%p, type=%d\n", (void*)root, root ? root->type : -1);
    fflush(stderr);

    // NOTE: Do NOT free the AST! All AST memory (nodes, inlines, text) is allocated
    // from pools owned by the parser (node_pool, inline_pool, text_buffer).
    // These pools will be freed when parser_destroy() is called.
    // Calling md_node_free() tries to free pool memory and causes crashes.
    // md_node_free(ast);  // DO NOT CALL THIS

    fprintf(stderr, "=== ir_markdown_parse: About to return...\n");
    fflush(stderr);

    return root;
}

// Convert markdown source to KIR JSON string
char* ir_markdown_to_kir(const char* source, size_t length) {
    fprintf(stderr, "=== ir_markdown_to_kir: Starting parse...\n");
    fflush(stderr);

    // Parse markdown to IR
    fprintf(stderr, "=== ir_markdown_to_kir: Calling ir_markdown_parse...\n");
    fflush(stderr);

    IRComponent* root = ir_markdown_parse(source, length);

    fprintf(stderr, "=== ir_markdown_to_kir: ir_markdown_parse returned! root=%p\n", (void*)root);
    fflush(stderr);

    if (!root) {
        fprintf(stderr, "=== ir_markdown_to_kir: Parse failed, root is NULL\n");
        fflush(stderr);
        return NULL;
    }

    fprintf(stderr, "=== ir_markdown_to_kir: Parse successful, root type=%d\n", root->type);
    fprintf(stderr, "=== ir_markdown_to_kir: Starting serialization...\n");
    fflush(stderr);

    // Serialize to JSON string (defined in ir_json_v2.c)
    extern char* ir_serialize_json_v2(IRComponent* root);
    char* json_str = ir_serialize_json_v2(root);

    if (!json_str) {
        fprintf(stderr, "=== ir_markdown_to_kir: Serialization failed\n");
        fflush(stderr);
        return NULL;
    }

    fprintf(stderr, "=== ir_markdown_to_kir: Serialization successful, length=%zu\n", strlen(json_str));
    fflush(stderr);

    // TODO: Free the IR tree (currently leaking to avoid crash)
    // The markdown parser creates components with uninitialized fields
    // that cause ir_destroy_component to crash. Need to fix parser first.
    // extern void ir_destroy_component(IRComponent* component);
    // ir_destroy_component(root);

    return json_str;
}

// Stub implementation - calls ir_markdown_parse for now
IRComponent* ir_markdown_parse_with_warnings(const char* source, size_t length,
                                              char*** warnings) {
    if (warnings) {
        *warnings = NULL;
    }
    return ir_markdown_parse(source, length);
}

// Returns version string
const char* ir_markdown_parser_version(void) {
    return "1.0.0-commonmark-0.31.2";
}

// Stub implementation - returns false for all features for now
bool ir_markdown_feature_supported(const char* feature_name) {
    (void)feature_name;
    return false;
}

// Cleanup function for freeing AST memory
void ir_markdown_free_ast(MdNode* root) {
    // The nodes are allocated from pools, so we clean up the parser state
    if (g_parser) {
        // Free any alignment arrays in table nodes
        if (g_parser->node_pool) {
            for (size_t i = 0; i < g_parser->node_pool_used; i++) {
                MdNode* node = &g_parser->node_pool[i];
                if (node->type == MD_BLOCK_TABLE && node->data.table.alignments) {
                    free(node->data.table.alignments);
                    node->data.table.alignments = NULL;
                }
            }
        }
        parser_destroy(g_parser);
        g_parser = NULL;
    }
}
