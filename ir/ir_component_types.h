#ifndef IR_COMPONENT_TYPES_H
#define IR_COMPONENT_TYPES_H

#include <stdint.h>

// IR Component Types
typedef enum {
    IR_COMPONENT_CONTAINER,
    IR_COMPONENT_TEXT,
    IR_COMPONENT_BUTTON,
    IR_COMPONENT_INPUT,
    IR_COMPONENT_CHECKBOX,
    IR_COMPONENT_DROPDOWN,
    IR_COMPONENT_TEXTAREA,          // Multi-line text input
    IR_COMPONENT_ROW,
    IR_COMPONENT_COLUMN,
    IR_COMPONENT_CENTER,
    IR_COMPONENT_IMAGE,
    IR_COMPONENT_CANVAS,
    IR_COMPONENT_NATIVE_CANVAS,
    IR_COMPONENT_MARKDOWN,
    IR_COMPONENT_SPRITE,
    IR_COMPONENT_TAB_GROUP,
    IR_COMPONENT_TAB_BAR,
    IR_COMPONENT_TAB,
    IR_COMPONENT_TAB_CONTENT,
    IR_COMPONENT_TAB_PANEL,
    // Modal/Overlay components
    IR_COMPONENT_MODAL,
    // Table components
    IR_COMPONENT_TABLE,
    IR_COMPONENT_TABLE_HEAD,
    IR_COMPONENT_TABLE_BODY,
    IR_COMPONENT_TABLE_FOOT,
    IR_COMPONENT_TABLE_ROW,
    IR_COMPONENT_TABLE_CELL,
    IR_COMPONENT_TABLE_HEADER_CELL,
    // Markdown document components
    IR_COMPONENT_HEADING,             // H1-H6 with semantic level
    IR_COMPONENT_PARAGRAPH,           // Text paragraph with wrapping
    IR_COMPONENT_BLOCKQUOTE,          // Quoted text block
    IR_COMPONENT_CODE_BLOCK,          // Fenced code with language tag
    IR_COMPONENT_HORIZONTAL_RULE,     // Thematic break (---, ***, ___)
    IR_COMPONENT_LIST,                // Ordered/unordered list container
    IR_COMPONENT_LIST_ITEM,           // Individual list item
    IR_COMPONENT_LINK,                // Hyperlink <a href="...">
    // Inline semantic components (for rich text)
    IR_COMPONENT_SPAN,                // <span> - inline container
    IR_COMPONENT_STRONG,              // <strong> - bold/important text
    IR_COMPONENT_EM,                  // <em> - emphasized/italic text
    IR_COMPONENT_CODE_INLINE,         // <code> - inline code
    IR_COMPONENT_SMALL,               // <small> - smaller text
    IR_COMPONENT_MARK,                // <mark> - highlighted text
    IR_COMPONENT_CUSTOM,
    // Source structure types (for round-trip codegen)
    IR_COMPONENT_STATIC_BLOCK,        // Static block (compile-time code execution)
    IR_COMPONENT_FOR_LOOP,            // For loop template (compile-time iteration)
    IR_COMPONENT_FOR_EACH,            // ForEach (runtime dynamic list rendering)
    IR_COMPONENT_VAR_DECL,            // Variable declaration (const/let/var)
    // Template placeholder (for docs layout templates)
    IR_COMPONENT_PLACEHOLDER,         // Template placeholder ({{name}})
    // Flowchart/diagram components (for Mermaid support)
    IR_COMPONENT_FLOWCHART,           // Flowchart container
    IR_COMPONENT_FLOWCHART_NODE,      // Individual flowchart node
    IR_COMPONENT_FLOWCHART_EDGE,      // Connection between nodes
    IR_COMPONENT_FLOWCHART_SUBGRAPH,  // Grouped nodes (subgraph)
    IR_COMPONENT_FLOWCHART_LABEL      // Text label for nodes/edges
} IRComponentType;

// Max value for registry array sizing
#define IR_COMPONENT_MAX IR_COMPONENT_FLOWCHART_LABEL

// Type conversion functions
const char* ir_component_type_to_string(IRComponentType type);
IRComponentType ir_component_type_from_string(const char* str);
IRComponentType ir_component_type_from_string_insensitive(const char* str);

/**
 * Convert component name to type (snake_case or PascalCase).
 * Used by the plugin capability API for string-based registration.
 *
 * @param name Component name (e.g., "code_block" or "CodeBlock")
 * @return Component type, or IR_COMPONENT_CONTAINER if unknown
 */
IRComponentType ir_component_type_from_snake_case(const char* name);

/**
 * Check if a component name is valid.
 *
 * @param name Component name to validate
 * @return true if name maps to a known component type
 */
#include <stdbool.h>
bool ir_component_type_name_valid(const char* name);

#endif // IR_COMPONENT_TYPES_H
