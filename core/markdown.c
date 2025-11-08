#include "include/kryon_markdown.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// Built-in Themes
// ============================================================================

const md_theme_t md_theme_light = {
    // Colors
    .text_color = 0x202020FF,         // Dark gray
    .heading_colors = {
        0x000000FF,                    // H1 - Black
        0x1a1a1aFF,                    // H2
        0x2d2d2dFF,                    // H3
        0x404040FF,                    // H4
        0x535353FF,                    // H5
        0x666666FF                     // H6
    },
    .link_color = 0x0066CCFF,          // Blue
    .code_text_color = 0x333333FF,     // Dark gray
    .code_bg_color = 0xF0F0F0FF,       // Light gray
    .blockquote_text_color = 0x333333FF,
    .blockquote_border_color = 0xDDDDDDFF,
    .table_border_color = 0xCCCCCCFF,
    .table_header_bg_color = 0xF5F5F5FF,

    // Typography
    .base_font_size = 16,
    .heading_font_sizes = { 32, 28, 24, 20, 18, 16 },
    .code_font_size = 14,
    .line_height = 24,
    .paragraph_spacing = 16,
    .heading_spacing = { 16, 14, 12, 10, 8, 6 },

    // Layout
    .max_width = 800,
    .padding = 16,
    .margin = 16,
    .list_indent = 24,
    .code_block_indent = 16
};

const md_theme_t md_theme_dark = {
    // Colors
    .text_color = 0xE5E5E5FF,         // Light gray
    .heading_colors = {
        0xFFFFFFFF,                    // H1 - White
        0xF0F0F0FF,                    // H2
        0xE0E0E0FF,                    // H3
        0xD0D0D0FF,                    // H4
        0xB0B0B0FF,                    // H5
        0x909090FF                     // H6
    },
    .link_color = 0x60A5FAFF,          // Light blue
    .code_text_color = 0xA0A0A0FF,     // Medium gray
    .code_bg_color = 0x1F1F1FFF,       // Dark gray
    .blockquote_text_color = 0xD0D0D0FF,
    .blockquote_border_color = 0x404040FF,
    .table_border_color = 0x333333FF,
    .table_header_bg_color = 0x2D2D2DFF,

    // Typography
    .base_font_size = 16,
    .heading_font_sizes = { 32, 28, 24, 20, 18, 16 },
    .code_font_size = 14,
    .line_height = 24,
    .paragraph_spacing = 16,
    .heading_spacing = { 16, 14, 12, 10, 8, 6 },

    // Layout
    .max_width = 800,
    .padding = 16,
    .margin = 16,
    .list_indent = 24,
    .code_block_indent = 16
};

const md_theme_t md_theme_github = {
    // Colors (GitHub-like)
    .text_color = 0x24292EFF,         // GitHub text
    .heading_colors = {
        0x24292EFF,                    // H1
        0x24292EFF,                    // H2
        0x24292EFF,                    // H3
        0x24292EFF,                    // H4
        0x24292EFF,                    // H5
        0x24292EFF                     // H6
    },
    .link_color = 0x0366D6FF,          // GitHub blue
    .code_text_color = 0xE1E4E8FF,     // GitHub code text
    .code_bg_color = 0xF6F8FAFF,       // GitHub code background
    .blockquote_text_color = 0x6A737DFF,
    .blockquote_border_color = 0xD0D7DEFF,
    .table_border_color = 0xD0D7DEFF,
    .table_header_bg_color = 0xF6F8FAFF,

    // Typography
    .base_font_size = 16,
    .heading_font_sizes = { 32, 28, 24, 20, 18, 16 },
    .code_font_size = 14,
    .line_height = 24,
    .paragraph_spacing = 16,
    .heading_spacing = { 16, 14, 12, 10, 8, 6 },

    // Layout
    .max_width = 880,
    .padding = 16,
    .margin = 16,
    .list_indent = 24,
    .code_block_indent = 16
};

// ============================================================================
// Parser Implementation
// ============================================================================

md_parser_t* md_parser_create(const char* input, size_t length) {
    if (input == NULL || length == 0) return NULL;

#if KRYON_NO_HEAP
    // For MCU, use static allocation
    static md_parser_t static_parser;
    md_parser_t* parser = &static_parser;
    memset(parser, 0, sizeof(md_parser_t));
#else
    md_parser_t* parser = (md_parser_t*)malloc(sizeof(md_parser_t));
    if (parser == NULL) return NULL;
    memset(parser, 0, sizeof(md_parser_t));
#endif

    parser->input = input;
    parser->input_length = length;
    parser->position = 0;

    // Initialize memory pools
#if KRYON_NO_HEAP
    static md_node_t node_pool[128];
    static md_inline_t inline_pool[256];
    static char text_buffer[4096];

    parser->memory.nodes = node_pool;
    parser->memory.inlines = inline_pool;
    parser->memory.text_buffer = text_buffer;
    parser->memory.node_count = 128;
    parser->memory.inline_count = 256;
    parser->memory.text_buffer_size = 4096;
#else
    parser->memory.node_count = 256;
    parser->memory.inline_count = 512;
    parser->memory.text_buffer_size = 8192;

    parser->memory.nodes = (md_node_t*)malloc(sizeof(md_node_t) * parser->memory.node_count);
    parser->memory.inlines = (md_inline_t*)malloc(sizeof(md_inline_t) * parser->memory.inline_count);
    parser->memory.text_buffer = (char*)malloc(parser->memory.text_buffer_size);

    if (!parser->memory.nodes || !parser->memory.inlines || !parser->memory.text_buffer) {
        md_parser_destroy(parser);
        return NULL;
    }
#endif

    return parser;
}

void md_parser_destroy(md_parser_t* parser) {
    if (parser == NULL) return;

#if !KRYON_NO_HEAP
    free(parser->memory.nodes);
    free(parser->memory.inlines);
    free(parser->memory.text_buffer);
    free(parser);
#endif
}

md_node_t* md_create_node(md_parser_t* parser, md_block_type_t type) {
    if (parser == NULL || parser->memory.node_count <= parser->memory.node_used) return NULL;

    md_node_t* node = &parser->memory.nodes[parser->memory.node_used++];
    memset(node, 0, sizeof(md_node_t));
    node->type = type;
    return node;
}

md_inline_t* md_create_inline(md_parser_t* parser, md_inline_type_t type) {
    if (parser == NULL || parser->memory.inline_count <= parser->memory.inline_used) return NULL;

    md_inline_t* inline = &parser->memory.inlines[parser->memory.inline_used++];
    memset(inline, 0, sizeof(md_inline_t));
    inline->type = type;
    return inline;
}

char* md_alloc_text(md_parser_t* parser, const char* text, uint16_t length) {
    if (parser == NULL || text == NULL || length == 0) return NULL;
    if (parser->memory.text_buffer_used + length + 1 > parser->memory.text_buffer_size) return NULL;

    char* result = &parser->memory.text_buffer[parser->memory.text_buffer_used];
    strncpy(result, text, length);
    result[length] = '\0';
    parser->memory.text_buffer_used += length + 1;
    return result;
}

// ============================================================================
// Text Processing Utilities
// ============================================================================

uint16_t md_strip_line_endings(const char* text, uint16_t length) {
    uint16_t result = length;
    while (result > 0 && (text[result - 1] == '\n' || text[result - 1] == '\r')) {
        result--;
    }
    return result;
}

bool md_is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool md_is_space_or_tab(char c) {
    return c == ' ' || c == '\t';
}

uint16_t md_count_leading_spaces(const char* text, uint16_t length) {
    uint16_t count = 0;
    for (uint16_t i = 0; i < length && md_is_space_or_tab(text[i]); i++) {
        count++;
    }
    return count;
}

bool md_is_ascii_punctuation(char c) {
    return (c >= 33 && c <= 47) ||   // !"#$%&'()*+,-./
           (c >= 58 && c <= 64) ||   // :;<=>?@
           (c >= 91 && c <= 96) ||   // [\]^_`
           (c >= 123 && c <= 126);  // {|}~
}

bool md_is_ascii_digit(char c) {
    return c >= '0' && c <= '9';
}

bool md_is_ascii_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool md_is_ascii_alpha_numeric(char c) {
    return md_is_ascii_letter(c) || md_is_ascii_digit(c);
}

// ============================================================================
// String Utilities
// ============================================================================

uint16_t md_strlen(const char* str) {
    if (str == NULL) return 0;
    uint16_t len = 0;
    while (str[len] != '\0') len++;
    return len;
}

int md_strcmp(const char* a, const char* b) {
    if (a == NULL || b == NULL) {
        return a == b ? 0 : (a == NULL ? -1 : 1);
    }

    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

void md_strcpy(char* dest, const char* src, uint16_t max_len) {
    if (dest == NULL || src == NULL || max_len == 0) return;

    uint16_t i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

char* md_strdup(const char* str, uint16_t length) {
    if (str == NULL || length == 0) return NULL;

    char* result = (char*)malloc(length + 1);
    if (result == NULL) return NULL;

    strncpy(result, str, length);
    result[length] = '\0';
    return result;
}

uint32_t md_hash_string(const char* str, uint16_t length) {
    if (str == NULL || length == 0) return 0;

    uint32_t hash = 5381;
    for (uint16_t i = 0; i < length; i++) {
        hash = ((hash << 5) + hash) + str[i];
    }
    return hash;
}

// ============================================================================
// Simple Parser Implementation
// ============================================================================

md_node_t* md_parse(const char* input, size_t length) {
    if (input == NULL || length == 0) return NULL;

    md_parser_t* parser = md_parser_create(input, length);
    if (parser == NULL) return NULL;

    // Create root document node
    md_node_t* document = md_create_node(parser, MD_BLOCK_DOCUMENT);
    if (document == NULL) {
        md_parser_destroy(parser);
        return NULL;
    }

    // Simple parsing - just split into paragraphs for now
    md_node_t* current_parent = document;
    uint16_t line_start = 0;
    uint16_t position = 0;

    while (position < length) {
        // Find end of current line
        uint16_t line_end = position;
        while (line_end < length && input[line_end] != '\n' && input[line_end] != '\r') {
            line_end++;
        }

        // Extract line content
        uint16_t line_length = line_end - position;
        uint16_t trimmed_length = line_length;
        while (trimmed_length > 0 && md_is_space_or_tab(input[position + trimmed_length - 1])) {
            trimmed_length--;
        }

        if (trimmed_length > 0) {
            // Check for heading
            if (input[position] == '#') {
                uint8_t heading_level = 0;
                while (heading_level < 6 && position + heading_level < line_end && input[position + heading_level] == '#') {
                    heading_level++;
                }

                if (heading_level > 0 && (position + heading_level >= line_end || md_is_space_or_tab(input[position + heading_level]))) {
                    // Skip spaces after #
                    uint16_t text_start = position + heading_level;
                    while (text_start < line_end && md_is_space_or_tab(input[text_start])) {
                        text_start++;
                    }

                    uint16_t text_length = line_end - text_start;
                    md_node_t* heading = md_create_node(parser, (md_block_type_t)(MD_BLOCK_HEADING_1 + heading_level - 1));
                    if (heading) {
                        heading->data.heading.text = md_alloc_text(parser, &input[text_start], text_length);
                        heading->data.heading.length = text_length;
                        heading->data.heading.level = heading_level;
                        heading->parent = current_parent;

                        // Add to parent's children list
                        if (current_parent->first_child == NULL) {
                            current_parent->first_child = heading;
                            current_parent->last_child = heading;
                        } else {
                            current_parent->last_child->next = heading;
                            heading->prev = current_parent->last_child;
                            current_parent->last_child = heading;
                        }
                    }
                }
            }
            // Check for horizontal rule
            else if (line_length >= 3 &&
                     (input[position] == '-' || input[position] == '_' || input[position] == '*')) {
                bool is_rule = true;
                for (uint16_t i = 0; i < line_length; i++) {
                    if (input[position + i] != input[position] && !md_is_space_or_tab(input[position + i])) {
                        is_rule = false;
                        break;
                    }
                }

                if (is_rule) {
                    md_node_t* rule = md_create_node(parser, MD_BLOCK_THEMATIC_BREAK);
                    if (rule) {
                        rule->parent = current_parent;
                        if (current_parent->first_child == NULL) {
                            current_parent->first_child = rule;
                            current_parent->last_child = rule;
                        } else {
                            current_parent->last_child->next = rule;
                            rule->prev = current_parent->last_child;
                            current_parent->last_child = rule;
                        }
                    }
                }
            }
            // Default to paragraph
            else {
                md_node_t* paragraph = md_create_node(parser, MD_BLOCK_PARAGRAPH);
                if (paragraph) {
                    paragraph->data.paragraph.text = md_alloc_text(parser, &input[position], trimmed_length);
                    paragraph->data.paragraph.length = trimmed_length;
                    paragraph->parent = current_parent;

                    if (current_parent->first_child == NULL) {
                        current_parent->first_child = paragraph;
                        current_parent->last_child = paragraph;
                    } else {
                        current_parent->last_child->next = paragraph;
                        paragraph->prev = current_parent->last_child;
                        current_parent->last_child = paragraph;
                    }
                }
            }
        }

        // Move to next line
        position = line_end + 1;
        if (position < length && (input[position - 1] == '\r' && input[position] == '\n')) {
            position++; // Skip \n in \r\n
        }
    }

    md_parser_destroy(parser);
    return document;
}

void md_free(md_node_t* root) {
    // For now, memory is managed by the parser
    // In a full implementation, we'd need to track and free allocated memory
    (void)root;
}

// ============================================================================
// Renderer Implementation
// ============================================================================

md_renderer_t* md_renderer_create(kryon_cmd_buf_t* cmd_buf, uint16_t width, uint16_t height) {
    if (cmd_buf == NULL) return NULL;

    md_renderer_t* renderer = (md_renderer_t*)malloc(sizeof(md_renderer_t));
    if (renderer == NULL) return NULL;

    memset(renderer, 0, sizeof(md_renderer_t));
    renderer->cmd_buf = cmd_buf;
    renderer->canvas_width = width;
    renderer->canvas_height = height;
    renderer->theme = &md_theme_light;

    return renderer;
}

void md_renderer_destroy(md_renderer_t* renderer) {
    if (renderer == NULL) return;
    free(renderer);
}

void md_renderer_set_theme(md_renderer_t* renderer, const md_theme_t* theme) {
    if (renderer == NULL || theme == NULL) return;
    renderer->theme = theme;
}

// ============================================================================
// Layout and Rendering Implementation
// ============================================================================

uint16_t md_measure_text(const char* text, uint16_t length, const md_theme_t* theme) {
    if (text == NULL || length == 0 || theme == NULL) return 0;

    // Simple approximation: 6 pixels per character for monospace fonts
    // In a real implementation, this would use font metrics
    return length * 6;
}

void md_layout_context_init(md_layout_context_t* ctx, uint16_t width, uint16_t height, const md_theme_t* theme) {
    if (ctx == NULL) return;

    memset(ctx, 0, sizeof(md_layout_context_t));
    ctx->width = width;
    ctx->height = height;
    ctx->max_width = theme->max_width;
    ctx->line_height = theme->line_height;
    ctx->theme = theme;
}

void md_render_document(md_node_t* root, md_renderer_t* renderer) {
    if (root == NULL || renderer == NULL) return;

    md_layout_context_t ctx;
    md_layout_context_init(&ctx, renderer->canvas_width, renderer->canvas_height, renderer->theme);
    ctx.renderer = renderer;

    // Simple rendering of all nodes
    md_node_t* node = root->first_child;
    uint16_t y = renderer->theme->margin;

    while (node != NULL) {
        switch (node->type) {
            case MD_BLOCK_HEADING_1:
            case MD_BLOCK_HEADING_2:
            case MD_BLOCK_HEADING_3:
            case MD_BLOCK_HEADING_4:
            case MD_BLOCK_HEADING_5:
            case MD_BLOCK_HEADING_6:
                if (node->data.heading.text && node->data.heading.length > 0) {
                    uint8_t level = node->type - MD_BLOCK_HEADING_1 + 1;
                    uint32_t color = renderer->theme->heading_colors[level - 1];
                    uint16_t font_size = renderer->theme->heading_font_sizes[level - 1];

                    kryon_draw_text(renderer->cmd_buf,
                                   node->data.heading.text,
                                   renderer->theme->margin, y,
                                   0, color);  // Use default font for now

                    y += font_size + renderer->theme->heading_spacing[level - 1];
                }
                break;

            case MD_BLOCK_PARAGRAPH:
                if (node->data.paragraph.text && node->data.paragraph.length > 0) {
                    kryon_draw_text(renderer->cmd_buf,
                                   node->data.paragraph.text,
                                   renderer->theme->margin, y,
                                   0, renderer->theme->text_color);

                    y += renderer->theme->line_height + renderer->theme->paragraph_spacing;
                }
                break;

            case MD_BLOCK_THEMATIC_BREAK:
                // Draw horizontal rule
                kryon_draw_line(renderer->cmd_buf,
                               renderer->theme->margin, y + 8,
                               renderer->canvas_width - renderer->theme->margin, y + 8,
                               renderer->theme->blockquote_border_color);
                y += 16;
                break;

            default:
                // Skip unknown node types
                break;
        }

        node = node->next;
    }
}

// ============================================================================
// File Loading
// ============================================================================

md_node_t* md_parse_file(const char* filename) {
    if (filename == NULL) return NULL;

    FILE* file = fopen(filename, "r");
    if (file == NULL) return NULL;

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(file);
        return NULL;
    }

    // Allocate buffer
    char* buffer = (char*)malloc(file_size + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    // Read file
    size_t read_size = fread(buffer, 1, file_size, file);
    fclose(file);

    if (read_size != (size_t)file_size) {
        free(buffer);
        return NULL;
    }

    buffer[file_size] = '\0';
    md_node_t* result = md_parse(buffer, file_size);
    free(buffer);

    return result;
}