/**
 * @file css_generator_core.h
 * @brief Core CSS generation functions
 *
 * Provides the core CSS generation functionality using IRStringBuilder
 * for efficient string building.
 */

#ifndef CSS_GENERATOR_CORE_H
#define CSS_GENERATOR_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include "ir_core.h"
#include "ir_properties.h"
#include "ir_layout.h"
#include "ir_string_builder.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CSS Generator Write Context
// ============================================================================

/**
 * CSS generation context for writing output
 * Wraps IRStringBuilder with CSS-specific helpers
 */
typedef struct CSSGenContext {
    IRStringBuilder* builder;      // String builder for output
    bool pretty_print;             // Enable formatted output
    bool has_output;               // Track if any properties were written
} CSSGenContext;

/**
 * Create a CSS generator context
 * @param builder Existing string builder to use (or NULL to create new)
 * @return New context or NULL on failure
 */
CSSGenContext* css_gen_context_create(IRStringBuilder* builder);

/**
 * Free a CSS generator context
 * If the builder was created by the context, it will be freed too.
 * @param ctx Context to free
 */
void css_gen_context_free(CSSGenContext* ctx);

/**
 * Get the string builder from the context
 * @param ctx CSS generator context
 * @return String builder (never NULL)
 */
IRStringBuilder* css_gen_get_builder(CSSGenContext* ctx);

/**
 * Check if any output was written since the last check
 * @param ctx CSS generator context
 * @return true if output was written
 */
bool css_gen_has_output(CSSGenContext* ctx);

/**
 * Reset the output tracking flag
 * @param ctx CSS generator context
 */
void css_gen_reset_output(CSSGenContext* ctx);

/**
 * Set pretty print mode
 * @param ctx CSS generator context
 * @param pretty Enable pretty print
 */
void css_gen_set_pretty_print(CSSGenContext* ctx, bool pretty);

// ============================================================================
// Core Write Functions
// ============================================================================

/**
 * Write a string to the CSS output
 * @param ctx CSS generator context
 * @param str String to write
 * @return true on success, false on failure
 */
bool css_gen_write(CSSGenContext* ctx, const char* str);

/**
 * Write a formatted string to the CSS output
 * @param ctx CSS generator context
 * @param format Printf-style format string
 * @return true on success, false on failure
 */
bool css_gen_writef(CSSGenContext* ctx, const char* format, ...);

/**
 * Write a line (string + newline) to the CSS output
 * @param ctx CSS generator context
 * @param line Line to write (without newline)
 * @return true on success, false on failure
 */
bool css_gen_write_line(CSSGenContext* ctx, const char* line);

/**
 * Write a formatted line to the CSS output
 * @param ctx CSS generator context
 * @param format Printf-style format string
 * @return true on success, false on failure
 */
bool css_gen_write_linef(CSSGenContext* ctx, const char* format, ...);

/**
 * Write an indented line to the CSS output
 * @param ctx CSS generator context
 * @param level Indent level (2 spaces per level)
 * @param format Printf-style format string
 * @return true on success, false on failure
 */
bool css_gen_write_indented(CSSGenContext* ctx, int level, const char* format, ...);

/**
 * Write a CSS property with value
 * @param ctx CSS generator context
 * @param property CSS property name
 * @param value Property value
 * @return true on success, false on failure
 */
bool css_gen_write_property(CSSGenContext* ctx, const char* property, const char* value);

/**
 * Write a CSS property with formatted value
 * @param ctx CSS generator context
 * @param property CSS property name
 * @param format Printf-style format for value
 * @return true on success, false on failure
 */
bool css_gen_write_propertyf(CSSGenContext* ctx, const char* property, const char* format, ...);

/**
 * Write a CSS declaration block opening
 * @param ctx CSS generator context
 * @param selector CSS selector (without braces)
 * @return true on success, false on failure
 */
bool css_gen_write_block_open(CSSGenContext* ctx, const char* selector);

/**
 * Write a CSS declaration block closing
 * @param ctx CSS generator context
 * @param newline_after Add trailing newline after closing brace
 * @return true on success, false on failure
 */
bool css_gen_write_block_close(CSSGenContext* ctx, bool newline_after);

/**
 * Write a CSS comment
 * @param ctx CSS generator context
 * @param comment Comment text (without delimiters)
 * @return true on success, false on failure
 */
bool css_gen_write_comment(CSSGenContext* ctx, const char* comment);

// ============================================================================
// Property Generation Functions
// ============================================================================

/**
 * Generate CSS for IRDimension (width, height, etc.)
 * @param ctx CSS generator context
 * @param property CSS property name
 * @param dimension IR dimension value
 * @return true if property was written (non-auto value)
 */
bool css_gen_generate_dimension(CSSGenContext* ctx, const char* property, IRDimension* dimension);

/**
 * Generate CSS for IRColor (background, color, etc.)
 * @param ctx CSS generator context
 * @param property CSS property name
 * @param color IR color value
 * @param skip_fully_transparent Skip if color is fully transparent
 * @return true if property was written
 */
bool css_gen_generate_color(CSSGenContext* ctx, const char* property, IRColor* color,
                            bool skip_fully_transparent);

/**
 * Generate CSS for IRAlignment (justify-content, align-items, etc.)
 * @param ctx CSS generator context
 * @param property CSS property name
 * @param alignment IR alignment value
 * @param skip_default Skip if value is default (flex-start)
 * @return true if property was written
 */
bool css_gen_generate_alignment(CSSGenContext* ctx, const char* property, IRAlignment alignment,
                                bool skip_default);

/**
 * Generate CSS for IRSpacing (margin, padding)
 * @param ctx CSS generator context
 * @param property_base Base property name ("margin" or "padding")
 * @param spacing IR spacing value
 * @return true if any property was written
 */
bool css_gen_generate_spacing(CSSGenContext* ctx, const char* property_base, IRSpacing* spacing);

/**
 * Generate CSS for IRBorder
 * @param ctx CSS generator context
 * @param border IR border value
 * @return true if any property was written
 */
bool css_gen_generate_border(CSSGenContext* ctx, IRBorder* border);

/**
 * Generate CSS for IRTypography
 * @param ctx CSS generator context
 * @param font IR typography value
 * @return true if any property was written
 */
bool css_gen_generate_typography(CSSGenContext* ctx, IRTypography* font);

/**
 * Generate CSS for IRTransform
 * @param ctx CSS generator context
 * @param transform IR transform value
 * @return true if property was written
 */
bool css_gen_generate_transform(CSSGenContext* ctx, IRTransform* transform);

/**
 * Generate CSS for IRBoxShadow
 * @param ctx CSS generator context
 * @param shadow IR box shadow value
 * @return true if property was written
 */
bool css_gen_generate_box_shadow(CSSGenContext* ctx, IRBoxShadow* shadow);

/**
 * Generate CSS for IRTextShadow
 * @param ctx CSS generator context
 * @param shadow IR text shadow value
 * @return true if property was written
 */
bool css_gen_generate_text_shadow(CSSGenContext* ctx, IRTextShadow* shadow);

/**
 * Generate CSS for IRFilter array
 * @param ctx CSS generator context
 * @param filters Array of filters
 * @param count Number of filters
 * @return true if property was written
 */
bool css_gen_generate_filters(CSSGenContext* ctx, IRFilter* filters, uint8_t count);

/**
 * Generate CSS for IRFlexbox
 * @param ctx CSS generator context
 * @param flex IR flexbox value
 * @param component_type Component type for direction inference
 * @return true if any property was written
 */
bool css_gen_generate_flexbox(CSSGenContext* ctx, IRFlexbox* flex, IRComponentType component_type);

/**
 * Generate CSS for IRGrid
 * @param ctx CSS generator context
 * @param grid IR grid value
 * @param is_inline True for inline-grid
 * @return true if any property was written
 */
bool css_gen_generate_grid(CSSGenContext* ctx, IRGrid* grid, bool is_inline);

/**
 * Generate CSS for IRGridItem
 * @param ctx CSS generator context
 * @param item IR grid item value
 * @return true if any property was written
 */
bool css_gen_generate_grid_item(CSSGenContext* ctx, IRGridItem* item);

/**
 * Generate CSS for IROverflowMode
 * @param ctx CSS generator context
 * @param property CSS property name ("overflow", "overflow-x", or "overflow-y")
 * @param overflow IR overflow value
 * @param skip_default Skip if value is default (visible)
 * @return true if property was written
 */
bool css_gen_generate_overflow(CSSGenContext* ctx, const char* property, IROverflowMode overflow,
                               bool skip_default);

/**
 * Generate CSS for IRLayoutMode
 * @param ctx CSS generator context
 * @param mode IR layout mode value
 * @return true if property was written
 */
bool css_gen_generate_layout_mode(CSSGenContext* ctx, IRLayoutMode mode);

/**
 * Generate CSS for animations list
 * @param ctx CSS generator context
 * @param animations Array of animation pointers
 * @param count Number of animations
 * @return true if property was written
 */
bool css_gen_generate_animations(CSSGenContext* ctx, IRAnimation** animations, uint32_t count);

/**
 * Generate CSS for transitions list
 * @param ctx CSS generator context
 * @param transitions Array of transition pointers
 * @param count Number of transitions
 * @return true if property was written
 */
bool css_gen_generate_transitions(CSSGenContext* ctx, IRTransition** transitions, uint32_t count);

/**
 * Generate CSS for IRTextEffect
 * @param ctx CSS generator context
 * @param effect IR text effect value
 * @return true if any property was written
 */
bool css_gen_generate_text_effect(CSSGenContext* ctx, IRTextEffect* effect);

/**
 * Generate CSS for media query
 * @param ctx CSS generator context
 * @param conditions Query conditions array
 * @param count Number of conditions
 * @param content_callback Callback to write query content
 * @param user_data User data for callback
 * @return true if media query was written
 */
typedef bool (*CSSMediaQueryContentFn)(CSSGenContext* ctx, void* user_data);

bool css_gen_generate_media_query(CSSGenContext* ctx, IRQueryCondition* conditions, uint8_t count,
                                   CSSMediaQueryContentFn content_callback, void* user_data);

/**
 * Generate CSS for container query
 * @param ctx CSS generator context
 * @param conditions Query conditions array
 * @param count Number of conditions
 * @param container_name Container name (or NULL)
 * @param content_callback Callback to write query content
 * @param user_data User data for callback
 * @return true if container query was written
 */
bool css_gen_generate_container_query(CSSGenContext* ctx, IRQueryCondition* conditions, uint8_t count,
                                       const char* container_name,
                                       CSSMediaQueryContentFn content_callback, void* user_data);

/**
 * Generate CSS for pseudo-class rule
 * @param ctx CSS generator context
 * @param selector Base selector
 * @param pseudo_class Pseudo-class name (without colon)
 * @param style_callback Callback to write pseudo-class content
 * @param user_data User data for callback
 * @return true if rule was written
 */
typedef bool (*CSSPseudoStyleFn)(CSSGenContext* ctx, void* user_data);

bool css_gen_generate_pseudo_class(CSSGenContext* ctx, const char* selector, const char* pseudo_class,
                                    CSSPseudoStyleFn style_callback, void* user_data);

/**
 * Generate CSS for @keyframes animation
 * @param ctx CSS generator context
 * @param animation IR animation definition
 * @return true if keyframes were written
 */
bool css_gen_generate_keyframes(CSSGenContext* ctx, IRAnimation* animation);

/**
 * Generate CSS for :root variables block
 * @param ctx CSS generator context
 * @param variables Array of CSS variable entries
 * @param count Number of variables
 * @return true if block was written
 */
typedef struct {
    const char* name;
    const char* value;
} CSSVariableEntry;

bool css_gen_generate_root_variables(CSSGenContext* ctx, CSSVariableEntry* variables, size_t count);

#ifdef __cplusplus
}
#endif

#endif // CSS_GENERATOR_CORE_H
