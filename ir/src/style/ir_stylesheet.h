/**
 * @file ir_stylesheet.h
 * @brief Global stylesheet support for Kryon IR
 *
 * This module provides CSS-like stylesheet functionality with support for:
 * - Complex selectors (descendant, child, sibling combinators)
 * - Selector specificity calculation
 * - Rule matching against component trees
 * - CSS variable storage
 */

#ifndef IR_STYLESHEET_H
#define IR_STYLESHEET_H

#include <stdint.h>
#include <stdbool.h>
#include "../include/ir_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// Types from ir_core.h are already available via include

// ============================================================================
// Selector Types and Combinators
// ============================================================================

/**
 * CSS Combinator types for complex selectors
 */
typedef enum {
    IR_COMBINATOR_NONE = 0,     // Simple selector (no combinator)
    IR_COMBINATOR_DESCENDANT,   // " " (space) - any descendant
    IR_COMBINATOR_CHILD,        // ">" - direct child only
    IR_COMBINATOR_ADJACENT,     // "+" - immediately following sibling
    IR_COMBINATOR_GENERAL       // "~" - any following sibling
} IRCombinator;

/**
 * Selector part types
 */
typedef enum {
    IR_SELECTOR_PART_ELEMENT = 0,  // Element/tag name: div, header, nav
    IR_SELECTOR_PART_CLASS,        // Class selector: .button, .container
    IR_SELECTOR_PART_ID,           // ID selector: #header, #main
    IR_SELECTOR_PART_UNIVERSAL     // Universal selector: *
} IRSelectorPartType;

/**
 * A single part of a selector (e.g., "header" or ".container")
 */
typedef struct {
    IRSelectorPartType type;
    char* name;                    // Element name, class name (without .), or ID (without #)
    char** classes;                // For compound class selectors: .btn.primary
    uint32_t class_count;
} IRSelectorPart;

/**
 * A parsed selector chain (e.g., "header .container > nav")
 * Stores parts and combinators between them
 */
typedef struct {
    IRSelectorPart* parts;         // Array of selector parts
    IRCombinator* combinators;     // Combinators between parts (length = part_count - 1)
    uint32_t part_count;
    char* original_selector;       // Original selector string for round-trip
    uint32_t specificity;          // Calculated specificity (IDs*100 + classes*10 + elements*1)
} IRSelectorChain;

// ============================================================================
// Style Properties (standalone, without component reference)
// ============================================================================

// IRObjectFit is now defined in ir_core.h

/**
 * Style properties that can be applied via stylesheet rules
 * This is a subset of IRStyle that can be serialized independently
 */
typedef struct IRStyleProperties {
    // Dimensions
    IRDimension width;
    IRDimension height;
    IRDimension min_width;
    IRDimension max_width;
    IRDimension min_height;
    IRDimension max_height;

    // Colors
    IRColor background;
    IRColor color;               // Text color
    IRColor border_color;
    IRColor text_fill_color;     // For -webkit-text-fill-color

    // Background
    char* background_image;      // For gradient strings like "linear-gradient(...)"
    uint8_t background_clip;     // IRBackgroundClip enum value

    // Border
    float border_width;
    float border_width_top;
    float border_width_right;
    float border_width_bottom;
    float border_width_left;
    uint8_t border_radius;             // Legacy single radius (for backward compatibility)
    uint8_t border_radius_top_left;    // Individual corner radius values
    uint8_t border_radius_top_right;
    uint8_t border_radius_bottom_right;
    uint8_t border_radius_bottom_left;
    uint8_t border_radius_flags;        // Bitmask for which corners are explicitly set

    // Spacing
    IRSpacing padding;
    IRSpacing margin;

    // Typography
    IRDimension font_size;  // Use IRDimension to preserve rem/em units
    uint16_t font_weight;
    bool font_bold;
    bool font_italic;
    IRTextAlign text_align;
    float line_height;
    float letter_spacing;
    char* font_family;

    // Layout
    IRLayoutMode display;          // flex, grid, block
    bool display_explicit;         // Was display explicitly set?
    uint8_t flex_direction;        // 0=column, 1=row (matches IRFlexbox.direction)
    bool flex_wrap;                // matches IRFlexbox.wrap
    IRAlignment justify_content;
    IRAlignment align_items;
    IRAlignment align_content;
    float gap;
    float row_gap;
    float column_gap;
    uint8_t flex_grow;
    uint8_t flex_shrink;

    // Grid layout
    char* grid_template_columns;   // Raw CSS string: "repeat(auto-fit, minmax(240px, 1fr))"
    char* grid_template_rows;      // Raw CSS string for rows

    // Transforms (uses IRTransform from ir_core.h)
    IRTransform transform;         // Transform: translate, scale, rotate

    // Transitions (array of IRTransition from ir_core.h)
    IRTransition* transitions;     // Array of transition definitions
    uint32_t transition_count;     // Number of transitions

    // Text decoration (uses IR_TEXT_DECORATION_* flags from ir_core.h)
    uint8_t text_decoration;       // Bitmask: IR_TEXT_DECORATION_NONE, _UNDERLINE, _LINE_THROUGH

    // Box model
    uint8_t box_sizing;            // 0 = content-box, 1 = border-box

    // Image/video properties
    IRObjectFit object_fit;        // object-fit: fill, contain, cover, none, scale-down

    // Other
    float opacity;
    uint32_t z_index;

    // Flags for which properties are set (vs default)
    uint64_t set_flags;            // Bitmask of which properties are explicitly set
} IRStyleProperties;

// Property flags for set_flags bitmask
#define IR_PROP_WIDTH           (1ULL << 0)
#define IR_PROP_HEIGHT          (1ULL << 1)
#define IR_PROP_BACKGROUND      (1ULL << 2)
#define IR_PROP_COLOR           (1ULL << 3)
#define IR_PROP_BORDER          (1ULL << 4)
#define IR_PROP_BORDER_RADIUS   (1ULL << 5)
#define IR_PROP_PADDING         (1ULL << 6)
#define IR_PROP_MARGIN          (1ULL << 7)
#define IR_PROP_FONT_SIZE       (1ULL << 8)
#define IR_PROP_FONT_WEIGHT     (1ULL << 9)
#define IR_PROP_TEXT_ALIGN      (1ULL << 10)
#define IR_PROP_DISPLAY         (1ULL << 11)
#define IR_PROP_FLEX_DIRECTION  (1ULL << 12)
#define IR_PROP_JUSTIFY_CONTENT (1ULL << 13)
#define IR_PROP_ALIGN_ITEMS     (1ULL << 14)
#define IR_PROP_GAP             (1ULL << 15)
#define IR_PROP_OPACITY         (1ULL << 16)
#define IR_PROP_Z_INDEX         (1ULL << 17)
#define IR_PROP_MIN_WIDTH       (1ULL << 18)
#define IR_PROP_MAX_WIDTH       (1ULL << 19)
#define IR_PROP_MIN_HEIGHT      (1ULL << 20)
#define IR_PROP_MAX_HEIGHT      (1ULL << 21)
#define IR_PROP_FLEX_WRAP       (1ULL << 22)
#define IR_PROP_FLEX_GROW       (1ULL << 23)
#define IR_PROP_FLEX_SHRINK     (1ULL << 24)
#define IR_PROP_LINE_HEIGHT     (1ULL << 25)
#define IR_PROP_LETTER_SPACING  (1ULL << 26)
#define IR_PROP_FONT_FAMILY     (1ULL << 27)
#define IR_PROP_BORDER_TOP      (1ULL << 28)
#define IR_PROP_BORDER_RIGHT    (1ULL << 29)
#define IR_PROP_BORDER_BOTTOM   (1ULL << 30)
#define IR_PROP_BORDER_LEFT     (1ULL << 31)
#define IR_PROP_BACKGROUND_IMAGE (1ULL << 32)
#define IR_PROP_BACKGROUND_CLIP  (1ULL << 33)
#define IR_PROP_TEXT_FILL_COLOR  (1ULL << 34)
#define IR_PROP_GRID_TEMPLATE_COLUMNS (1ULL << 35)
#define IR_PROP_GRID_TEMPLATE_ROWS    (1ULL << 36)
#define IR_PROP_TRANSITION            (1ULL << 37)
#define IR_PROP_TRANSFORM             (1ULL << 38)
#define IR_PROP_TEXT_DECORATION       (1ULL << 39)
#define IR_PROP_BOX_SIZING            (1ULL << 40)
#define IR_PROP_OBJECT_FIT            (1ULL << 41)
#define IR_PROP_BORDER_COLOR          (1ULL << 42)

// ============================================================================
// Style Rules
// ============================================================================

/**
 * A single CSS rule (selector + properties)
 */
typedef struct {
    IRSelectorChain* selector;     // Parsed selector
    IRStyleProperties properties;  // Style properties to apply
} IRStyleRule;

// ============================================================================
// CSS Variables
// ============================================================================

/**
 * A CSS custom property (variable)
 */
typedef struct {
    char* name;                    // Variable name without -- prefix
    char* value;                   // Resolved value
} IRCSSVariable;

// ============================================================================
// Media Queries (stored as raw CSS for roundtrip)
// ============================================================================

/**
 * A media query block with condition and CSS content
 */
typedef struct {
    char* condition;   // e.g., "max-width: 640px"
    char* css_content; // Raw CSS rules inside the media query
} IRMediaQuery;

// ============================================================================
// Global Stylesheet
// ============================================================================

/**
 * A stylesheet containing rules, CSS variables, and media queries
 * Attached to IRContext for global access
 */
typedef struct IRStylesheet {
    IRStyleRule* rules;
    uint32_t rule_count;
    uint32_t rule_capacity;

    IRCSSVariable* variables;
    uint32_t variable_count;
    uint32_t variable_capacity;

    IRMediaQuery* media_queries;
    uint32_t media_query_count;
    uint32_t media_query_capacity;
} IRStylesheet;

// ============================================================================
// Stylesheet Management Functions
// ============================================================================

/**
 * Create a new empty stylesheet
 */
IRStylesheet* ir_stylesheet_create(void);

/**
 * Free a stylesheet and all its contents
 */
void ir_stylesheet_free(IRStylesheet* stylesheet);

/**
 * Add a rule to the stylesheet
 * @param stylesheet The stylesheet to add to
 * @param selector Original selector string (will be parsed)
 * @param properties Style properties to apply (copied)
 * @return true on success
 */
bool ir_stylesheet_add_rule(IRStylesheet* stylesheet, const char* selector,
                            const IRStyleProperties* properties);

/**
 * Add a CSS variable to the stylesheet
 */
bool ir_stylesheet_add_variable(IRStylesheet* stylesheet, const char* name, const char* value);

/**
 * Get a CSS variable value by name
 */
const char* ir_stylesheet_get_variable(IRStylesheet* stylesheet, const char* name);

/**
 * Add a media query to the stylesheet
 * @param condition The media query condition (e.g., "max-width: 640px")
 * @param css_content The raw CSS content inside the media query
 */
bool ir_stylesheet_add_media_query(IRStylesheet* stylesheet, const char* condition, const char* css_content);

// ============================================================================
// Selector Parsing Functions
// ============================================================================

/**
 * Parse a selector string into a selector chain
 * @param selector CSS selector string (e.g., "header .container > nav")
 * @return Parsed selector chain, or NULL on error
 */
IRSelectorChain* ir_selector_parse(const char* selector);

/**
 * Free a selector chain
 */
void ir_selector_chain_free(IRSelectorChain* chain);

/**
 * Calculate specificity of a selector chain
 * Returns: IDs*100 + classes*10 + elements*1
 */
uint32_t ir_selector_specificity(const IRSelectorChain* chain);

// ============================================================================
// Selector Matching Functions
// ============================================================================

/**
 * Check if a selector chain matches a component
 * @param chain The parsed selector chain
 * @param component The target component to check
 * @param root The root component (for ancestor traversal)
 * @return true if the selector matches the component
 */
bool ir_selector_matches(const IRSelectorChain* chain, IRComponent* component, IRComponent* root);

/**
 * Get all matching rules for a component, sorted by specificity
 * @param stylesheet The stylesheet to search
 * @param component The target component
 * @param root The root component
 * @param out_rules Output array of matching rules (caller must free)
 * @param out_count Output count of matching rules
 */
void ir_stylesheet_get_matching_rules(IRStylesheet* stylesheet, IRComponent* component,
                                       IRComponent* root, IRStyleRule*** out_rules,
                                       uint32_t* out_count);

// ============================================================================
// Style Application Functions
// ============================================================================

/**
 * Apply stylesheet rules to a component tree (parse-time resolution)
 * @param stylesheet The stylesheet to apply
 * @param root The root component of the tree
 */
void ir_stylesheet_apply_to_tree(IRStylesheet* stylesheet, IRComponent* root);

/**
 * Apply style properties to a component's IRStyle
 */
void ir_style_properties_apply(const IRStyleProperties* props, IRStyle* style, IRLayout* layout);

/**
 * Initialize style properties to defaults
 */
void ir_style_properties_init(IRStyleProperties* props);

/**
 * Free any allocated memory in style properties
 */
void ir_style_properties_cleanup(IRStyleProperties* props);

/**
 * Deep copy style properties (properly handles all pointer fields)
 * Avoids the shallow copy + selective deep copy pattern that causes double-frees
 */
void ir_style_properties_deep_copy(const IRStyleProperties* src, IRStyleProperties* dst);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get combinator name for debugging/serialization
 */
const char* ir_combinator_to_string(IRCombinator combinator);

/**
 * Get selector part type name for debugging/serialization
 */
const char* ir_selector_part_type_to_string(IRSelectorPartType type);

#ifdef __cplusplus
}
#endif

#endif // IR_STYLESHEET_H
