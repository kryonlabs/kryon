/**
 * Kryon Markdown AST (Abstract Syntax Tree)
 *
 * Internal AST representation for markdown parsing.
 * The parser first builds this AST, then converts it to IR components.
 *
 * This is an internal implementation detail - external code should
 * use ir_markdown_parse() which returns IRComponent* directly.
 */

#ifndef IR_MARKDOWN_AST_H
#define IR_MARKDOWN_AST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Block Element Types
// ============================================================================

typedef enum {
    MD_BLOCK_DOCUMENT = 0,          // Root document node
    MD_BLOCK_HEADING_1 = 1,         // # Heading level 1
    MD_BLOCK_HEADING_2 = 2,         // ## Heading level 2
    MD_BLOCK_HEADING_3 = 3,         // ### Heading level 3
    MD_BLOCK_HEADING_4 = 4,         // #### Heading level 4
    MD_BLOCK_HEADING_5 = 5,         // ##### Heading level 5
    MD_BLOCK_HEADING_6 = 6,         // ###### Heading level 6
    MD_BLOCK_PARAGRAPH = 7,         // Text paragraph
    MD_BLOCK_LIST_ITEM = 8,         // List item (child of list)
    MD_BLOCK_LIST_UNORDERED = 9,    // Unordered list (-, *, +)
    MD_BLOCK_LIST_ORDERED = 10,     // Ordered list (1., 2., ...)
    MD_BLOCK_CODE_BLOCK = 11,       // Fenced or indented code block
    MD_BLOCK_HTML_BLOCK = 12,       // Raw HTML block (not rendered)
    MD_BLOCK_BLOCKQUOTE = 13,       // Blockquote (>)
    MD_BLOCK_TABLE = 14,            // Table container
    MD_BLOCK_TABLE_ROW = 15,        // Table row (child of table)
    MD_BLOCK_TABLE_CELL = 16,       // Table cell (th or td)
    MD_BLOCK_THEMATIC_BREAK = 17    // Horizontal rule (---, ***, ___)
} MdBlockType;

// ============================================================================
// Inline Element Types
// ============================================================================

typedef enum {
    MD_INLINE_TEXT = 0,             // Plain text
    MD_INLINE_CODE_SPAN = 1,        // Inline code (`code`)
    MD_INLINE_EMPHASIS = 2,         // Italic (*text* or _text_)
    MD_INLINE_STRONG = 3,           // Bold (**text** or __text__)
    MD_INLINE_LINK = 4,             // Link [text](url)
    MD_INLINE_IMAGE = 5,            // Image ![alt](url)
    MD_INLINE_LINE_BREAK = 6,       // Hard line break (two spaces + newline)
    MD_INLINE_SOFT_BREAK = 7,       // Soft line break (single newline)
    MD_INLINE_RAW_HTML = 8,         // Raw HTML inline (not rendered)
    MD_INLINE_STRIKETHROUGH = 9     // Strikethrough (~~text~~, GFM extension)
} MdInlineType;

// ============================================================================
// Text Alignment (for table cells)
// ============================================================================

typedef enum {
    MD_ALIGN_LEFT = 0,              // Left-aligned (default)
    MD_ALIGN_CENTER = 1,            // Center-aligned (:---:)
    MD_ALIGN_RIGHT = 2,             // Right-aligned (---:)
    MD_ALIGN_NONE = 3               // No alignment specified
} MdAlignment;

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct MdNode MdNode;
typedef struct MdInline MdInline;

// ============================================================================
// AST Node Structure (Block-level elements)
// ============================================================================

struct MdNode {
    MdBlockType type;
    MdNode* parent;
    MdNode* first_child;
    MdNode* last_child;
    MdNode* prev;
    MdNode* next;

    // Type-specific data
    union {
        // Heading (MD_BLOCK_HEADING_*)
        struct {
            char* text;             // Heading text content
            uint16_t length;        // Text length
            uint8_t level;          // 1-6 for H1-H6
            bool setext;            // Setext style (underline with === or ---)
        } heading;

        // Paragraph (MD_BLOCK_PARAGRAPH)
        struct {
            char* text;             // Raw paragraph text
            uint16_t length;        // Text length
            MdInline* first_inline; // Linked list of inline elements
        } paragraph;

        // List (MD_BLOCK_LIST_*)
        struct {
            bool ordered;           // True for ordered (1.), false for unordered (-)
            uint8_t start;          // Starting number for ordered lists
            bool tight;             // Tight spacing (no blank lines between items)
            char* marker;           // List marker character (-, *, +, or NULL)
        } list;

        // Code block (MD_BLOCK_CODE_BLOCK)
        struct {
            char* code;             // Code content
            uint16_t length;        // Code length
            char* language;         // Language tag (NULL if none)
            uint8_t indent;         // Indentation level
            bool fenced;            // Fenced (```) vs indented (4 spaces)
        } code_block;

        // Blockquote (MD_BLOCK_BLOCKQUOTE)
        struct {
            char* text;             // Blockquote text (for debug)
            uint16_t length;        // Text length
            uint8_t level;          // Nesting level
        } blockquote;

        // Table (MD_BLOCK_TABLE)
        struct {
            uint16_t columns;       // Number of columns
            uint16_t rows;          // Number of rows
            MdAlignment* alignments;// Column alignments (array of size columns)
            bool header;            // Has header row
        } table;

        // Table cell (MD_BLOCK_TABLE_CELL)
        struct {
            bool is_header;         // TH vs TD
            MdAlignment align;      // Cell alignment
            MdInline* first_inline; // Cell content (inline elements)
        } table_cell;
    } data;
};

// ============================================================================
// Inline Element Structure
// ============================================================================

struct MdInline {
    MdInlineType type;
    MdInline* next;

    // Type-specific data
    union {
        // Text, code span, emphasis, strong (MD_INLINE_TEXT, CODE_SPAN, EMPHASIS, STRONG)
        struct {
            char* text;             // Text content
            uint16_t length;        // Text length
        } text;

        // Link (MD_INLINE_LINK)
        struct {
            char* url;              // Link URL
            char* title;            // Optional title attribute
            uint16_t url_length;    // URL length
            uint16_t title_length;  // Title length
            MdInline* first_child;  // Link text (inline elements)
        } link;

        // Image (MD_INLINE_IMAGE)
        struct {
            char* url;              // Image source URL
            char* alt;              // Alt text
            uint16_t url_length;    // URL length
            uint16_t alt_length;    // Alt text length
        } image;
    } data;
};

// ============================================================================
// AST Node Creation (internal use only)
// ============================================================================

/**
 * Create a new AST node
 *
 * @param type Block type
 * @return MdNode* Allocated node (caller must free), or NULL on error
 */
MdNode* md_node_new(MdBlockType type);

/**
 * Create a new inline element
 *
 * @param type Inline type
 * @return MdInline* Allocated inline (caller must free), or NULL on error
 */
MdInline* md_inline_new(MdInlineType type);

/**
 * Append a child node to a parent
 *
 * @param parent Parent node
 * @param child Child node to append
 */
void md_node_append_child(MdNode* parent, MdNode* child);

/**
 * Free an AST node and all its children
 *
 * @param node Root node to free
 */
void md_node_free(MdNode* node);

/**
 * Free an inline element and all its siblings
 *
 * @param inl Root inline element to free
 */
void md_inline_free(MdInline* inl);

#ifdef __cplusplus
}
#endif

#endif // IR_MARKDOWN_AST_H
