#ifndef KRYON_MARKDOWN_H
#define KRYON_MARKDOWN_H

#include "kryon.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Markdown Parser Types
// ============================================================================

// Block element types
typedef enum {
    MD_BLOCK_DOCUMENT = 0,
    MD_BLOCK_HEADING_1 = 1,
    MD_BLOCK_HEADING_2 = 2,
    MD_BLOCK_HEADING_3 = 3,
    MD_BLOCK_HEADING_4 = 4,
    MD_BLOCK_HEADING_5 = 5,
    MD_BLOCK_HEADING_6 = 6,
    MD_BLOCK_PARAGRAPH = 7,
    MD_BLOCK_LIST_ITEM = 8,
    MD_BLOCK_LIST_UNORDERED = 9,
    MD_BLOCK_LIST_ORDERED = 10,
    MD_BLOCK_CODE_BLOCK = 11,
    MD_BLOCK_HTML_BLOCK = 12,
    MD_BLOCK_BLOCKQUOTE = 13,
    MD_BLOCK_TABLE = 14,
    MD_BLOCK_TABLE_ROW = 15,
    MD_BLOCK_TABLE_CELL = 16,
    MD_BLOCK_THEMATIC_BREAK = 17
} md_block_type_t;

// Inline element types
typedef enum {
    MD_INLINE_TEXT = 0,
    MD_INLINE_CODE_SPAN = 1,
    MD_INLINE_EMPHASIS = 2,
    MD_INLINE_STRONG = 3,
    MD_INLINE_LINK = 4,
    MD_INLINE_IMAGE = 5,
    MD_INLINE_LINE_BREAK = 6,
    MD_INLINE_SOFT_BREAK = 7,
    MD_INLINE_RAW_HTML = 8,
    MD_INLINE_STRIKETHROUGH = 9
} md_inline_type_t;

// Text alignment
typedef enum {
    MD_ALIGN_LEFT = 0,
    MD_ALIGN_CENTER = 1,
    MD_ALIGN_RIGHT = 2,
    MD_ALIGN_NONE = 3
} md_alignment_t;

// Forward declarations
struct md_node;
struct md_inline;
struct md_renderer;
struct md_theme;

// ============================================================================
// Node Structure
// ============================================================================

typedef struct md_node {
    md_block_type_t type;
    struct md_node* parent;
    struct md_node* first_child;
    struct md_node* last_child;
    struct md_node* prev;
    struct md_node* next;

    union {
        struct {
            char* text;
            uint16_t length;
            uint8_t level;
            bool setext;
        } heading;

        struct {
            char* text;
            uint16_t length;
            struct md_inline* first_inline;  // Linked list of inline elements
        } paragraph;

        struct {
            bool ordered;
            uint8_t start;
            bool tight;
            char* marker;
        } list;

        struct {
            char* code;
            uint16_t length;
            char* language;
            uint8_t indent;
            bool fenced;
        } code_block;

        struct {
            char* text;
            uint16_t length;
            uint8_t level;
        } blockquote;

        struct {
            uint16_t columns;
            uint16_t rows;
            md_alignment_t* alignments;
            bool header;
        } table;
    } data;
} md_node_t;

// Inline element structure
typedef struct md_inline {
    md_inline_type_t type;
    struct md_inline* next;

    union {
        struct {
            char* text;
            uint16_t length;
        } text;

        struct {
            char* url;
            char* title;
            uint16_t url_length;
            uint16_t title_length;
        } link;

        struct {
            char* url;
            char* alt;
            uint16_t url_length;
            uint16_t alt_length;
        } image;
    } data;
} md_inline_t;

// ============================================================================
// Theme System
// ============================================================================

typedef struct md_theme {
    // Colors
    uint32_t text_color;
    uint32_t heading_colors[6];
    uint32_t link_color;
    uint32_t code_text_color;
    uint32_t code_bg_color;
    uint32_t blockquote_text_color;
    uint32_t blockquote_border_color;
    uint32_t table_border_color;
    uint32_t table_header_bg_color;

    // Typography
    uint16_t base_font_size;
    uint16_t heading_font_sizes[6];
    uint16_t code_font_size;
    uint16_t line_height;
    uint16_t paragraph_spacing;
    uint16_t heading_spacing[6];

    // Layout
    uint16_t max_width;
    uint16_t padding;
    uint16_t margin;
    uint16_t list_indent;
    uint16_t code_block_indent;
} md_theme_t;

// Built-in themes
extern const md_theme_t md_theme_light;
extern const md_theme_t md_theme_dark;
extern const md_theme_t md_theme_github;

// ============================================================================
// Parser API
// ============================================================================

typedef struct md_parser {
    const char* input;
    size_t input_length;
    size_t position;

    // Memory management
    struct {
        md_node_t* nodes;
        md_inline_t* inlines;
        char* text_buffer;
        uint16_t node_count;
        uint16_t node_used;
        uint16_t inline_count;
        uint16_t inline_used;
        uint16_t text_buffer_size;
        uint16_t text_buffer_used;
    } memory;

    // Parser state
    uint8_t indent;
    bool in_list;
    bool in_blockquote;
    uint8_t list_depth;
} md_parser_t;

// Parser lifecycle
md_parser_t* md_parser_create(const char* input, size_t length);
void md_parser_destroy(md_parser_t* parser);
md_node_t* md_parse(const char* input, size_t length);
md_node_t* md_parse_file(const char* filename);
void md_free(md_node_t* root);

// Node creation
md_node_t* md_create_node(md_parser_t* parser, md_block_type_t type);
md_inline_t* md_create_inline(md_parser_t* parser, md_inline_type_t type);
char* md_alloc_text(md_parser_t* parser, const char* text, uint16_t length);

// ============================================================================
// Layout Engine
// ============================================================================

typedef struct md_layout_context {
    uint16_t x, y;
    uint16_t width, height;
    uint16_t max_width;
    uint16_t line_height;
    uint16_t baseline;

    // Current line state
    uint16_t line_x;
    uint16_t line_y;
    uint16_t line_height_current;

    // Theme and rendering
    const md_theme_t* theme;
    struct md_renderer* renderer;
} md_layout_context_t;

// Layout functions
void md_layout_node(md_node_t* node, md_layout_context_t* ctx);
void md_layout_document(md_node_t* root, md_layout_context_t* ctx);

// Layout utilities
uint16_t md_measure_text(const char* text, uint16_t length, const md_theme_t* theme);
uint16_t md_wrap_text(const char* text, uint16_t length, uint16_t max_width, const md_theme_t* theme);
uint16_t md_measure_inline(md_inline_t* inl, const md_theme_t* theme);

// ============================================================================
// Renderer System
// ============================================================================

typedef struct md_renderer {
    kryon_cmd_buf_t* cmd_buf;
    uint16_t canvas_width;
    uint16_t canvas_height;
    const md_theme_t* theme;

    // State
    uint32_t current_color;
    uint16_t current_font_size;
    uint16_t current_x, current_y;

    // Font cache
    struct {
        uint16_t font_id;
        uint16_t code_font_id;
    } fonts;

    // Image cache
    struct {
        uint16_t* texture_ids;
        uint16_t count;
        uint16_t capacity;
    } images;

    // Text measurement callback (for accurate spacing)
    void (*measure_text)(const char* text, uint16_t font_size,
                        uint8_t font_weight, uint8_t font_style,
                        uint16_t* width, uint16_t* height, void* user_data);
    void* measure_text_user_data;
} md_renderer_t;

// Renderer lifecycle
md_renderer_t* md_renderer_create(kryon_cmd_buf_t* cmd_buf, uint16_t width, uint16_t height);
void md_renderer_destroy(md_renderer_t* renderer);
void md_renderer_set_theme(md_renderer_t* renderer, const md_theme_t* theme);

// Set text measurement callback for accurate spacing
void md_renderer_set_measure_callback(md_renderer_t* renderer,
                                      void (*measure_text)(const char*, uint16_t, uint8_t, uint8_t, uint16_t*, uint16_t*, void*),
                                      void* user_data);

// Rendering functions
void md_render_node(md_node_t* node, md_renderer_t* renderer, md_layout_context_t* ctx);
void md_render_document(md_node_t* root, md_renderer_t* renderer);

// Element rendering
void md_render_heading(md_node_t* heading, md_renderer_t* renderer, md_layout_context_t* ctx);
void md_render_paragraph(md_node_t* paragraph, md_renderer_t* renderer, md_layout_context_t* ctx);
void md_render_list(md_node_t* list, md_renderer_t* renderer, md_layout_context_t* ctx);
void md_render_code_block(md_node_t* code_block, md_renderer_t* renderer, md_layout_context_t* ctx);
void md_render_blockquote(md_node_t* blockquote, md_renderer_t* renderer, md_layout_context_t* ctx);
void md_render_table(md_node_t* table, md_renderer_t* renderer, md_layout_context_t* ctx);

// Inline rendering
void md_render_inlines(md_inline_t* inlines, md_renderer_t* renderer, md_layout_context_t* ctx);
void md_render_text_inline(md_inline_t* inl, md_renderer_t* renderer, md_layout_context_t* ctx);
void md_render_code_inline(md_inline_t* inl, md_renderer_t* renderer, md_layout_context_t* ctx);
void md_render_link_inline(md_inline_t* inl, md_renderer_t* renderer, md_layout_context_t* ctx);
void md_render_image_inline(md_inline_t* inl, md_renderer_t* renderer, md_layout_context_t* ctx);

// ============================================================================
// Utility Functions
// ============================================================================

// Text processing
uint16_t md_strip_line_endings(const char* text, uint16_t length);
bool md_is_whitespace(char c);
bool md_is_space_or_tab(char c);
uint16_t md_count_leading_spaces(const char* text, uint16_t length);

// Character classification
bool md_is_ascii_punctuation(char c);
bool md_is_ascii_digit(char c);
bool md_is_ascii_letter(char c);
bool md_is_ascii_alpha_numeric(char c);

// URL encoding/decoding
bool md_is_url_safe(char c);
uint16_t md_encode_url(const char* input, char* output, uint16_t max_output);

// String utilities
uint16_t md_strlen(const char* str);
int md_strcmp(const char* a, const char* b);
void md_strcpy(char* dest, const char* src, uint16_t max_len);
char* md_strdup(const char* str, uint16_t length);

// Hash functions
uint32_t md_hash_string(const char* str, uint16_t length);

// ============================================================================
// Constants
// ============================================================================

#define MD_MAX_INDENT 32
#define MD_TAB_SIZE 4
#define MD_MAX_LINK_URL 2048
#define MD_MAX_LINK_TITLE 1024
#define MD_MAX_IMAGE_URL 2048
#define MD_MAX_IMAGE_ALT 1024

// Special characters
#define MD_ASCII_BACKTICK '`'
#define MD_ASCII_ASTERISK '*'
#define MD_ASCII_UNDERSCORE '_'
#define MD_ASCII_OPEN_BRACKET '['
#define MD_ASCII_CLOSE_BRACKET ']'
#define MD_ASCII_OPEN_PAREN '('
#define MD_ASCII_CLOSE_PAREN ')'
#define MD_ASCII_HASH '#'
#define MD_ASCII_PLUS '+'
#define MD_ASCII_DASH '-'
#define MD_ASCII_DOT '.'
#define MD_ASCII_EXCLAMATION '!'
#define MD_ASCII_PIPE '|'
#define MD_ASCII_GREATER '>'
#define MD_ASCII_LESSER '<'
#define MD_ASCII_SINGLE_QUOTE '\''
#define MD_ASCII_DOUBLE_QUOTE '"'

#ifdef __cplusplus
}
#endif

#endif // KRYON_MARKDOWN_H