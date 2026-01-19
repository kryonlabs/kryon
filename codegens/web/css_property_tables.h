/**
 * @file css_property_tables.h
 * @brief Data-driven CSS property mapping tables
 *
 * Defines formatter functions and property mapping tables for CSS generation.
 * This module separates CSS generation logic from data structures.
 */

#ifndef CSS_PROPERTY_TABLES_H
#define CSS_PROPERTY_TABLES_H

#include <stdbool.h>
#include <stddef.h>
#include "ir_properties.h"
#include "ir_style.h"
#include "ir_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Formatter Functions
// ============================================================================

/**
 * Format a dimension value (px, %, vw, vh, etc.)
 * @param value The dimension value to format
 * @return Static buffer containing formatted value (e.g., "16px", "50%")
 */
const char* css_format_dimension(IRDimension* value);

/**
 * Format a color value (rgba, hex, var(), gradient)
 * @param color The color to format
 * @return Static buffer containing formatted color string
 */
const char* css_format_color(IRColor* color);

/**
 * Format an alignment value (flex-start, center, flex-end, stretch)
 * @param alignment The alignment to format
 * @return CSS alignment string
 */
const char* css_format_alignment(IRAlignment alignment);

/**
 * Format an overflow value (visible, hidden, scroll, auto)
 * @param overflow The overflow mode to format
 * @return CSS overflow string
 */
const char* css_format_overflow(IROverflowMode overflow);

/**
 * Format a text alignment value (left, center, right, justify)
 * @param align The text alignment to format
 * @return CSS text-align string
 */
const char* css_format_text_align(IRTextAlign align);

/**
 * Format a position mode value (relative, absolute, fixed)
 * @param mode The position mode to format
 * @return CSS position string
 */
const char* css_format_position(IRPositionMode mode);

/**
 * Format a grid track value (auto, px, %, fr, min-content, max-content)
 * @param track The grid track to format
 * @return Static buffer containing formatted track string
 */
const char* css_format_grid_track(IRGridTrack* track);

/**
 * Format a layout mode value (flex, grid, block, inline, etc.)
 * @param mode The layout mode to format
 * @return CSS display string
 */
const char* css_format_layout_mode(IRLayoutMode mode);

/**
 * Format a grid track minmax value
 * @param type The track type
 * @param value The track value
 * @param buffer Output buffer
 * @param size Buffer size
 * @return The buffer pointer
 */
const char* css_format_grid_track_minmax(IRGridTrackType type, float value,
                                         char* buffer, size_t size);

/**
 * Format an easing type value (linear, ease-in, ease-out, etc.)
 * @param easing The easing type to format
 * @return CSS easing function string
 */
const char* css_format_easing(IREasingType easing);

/**
 * Format an animation property name (opacity, transform, width, etc.)
 * @param prop The animation property to format
 * @return CSS property name string
 */
const char* css_format_animation_property(IRAnimationProperty prop);

/**
 * Format a pseudo-class state name (hover, active, focus, etc.)
 * @param state The pseudo state to format
 * @return CSS pseudo-class string (without colon)
 */
const char* css_format_pseudo_class(IRPseudoState state);

/**
 * Format a direction value (ltr, rtl, auto)
 * @param direction The direction to format
 * @return CSS direction string
 */
const char* css_format_direction(IRDirection direction);

/**
 * Format a unicode-bidi value (normal, embed, isolate, plaintext)
 * @param bidi The unicode-bidi value to format
 * @return CSS unicode-bidi string
 */
const char* css_format_unicode_bidi(IRUnicodeBidi bidi);

/**
 * Format an object-fit value (fill, contain, cover, none, scale-down)
 * @param fit The object-fit value to format
 * @return CSS object-fit string
 */
const char* css_format_object_fit(IRObjectFit fit);

/**
 * Format a background-clip value (border-box, padding-box, content-box, text)
 * @param clip The background-clip value to format
 * @return CSS background-clip string
 */
const char* css_format_background_clip(IRBackgroundClip clip);

// ============================================================================
// Value Formatting Helpers
// ============================================================================

/**
 * Format a pixel value without trailing zeros
 * @param value The value in pixels
 * @return Static buffer containing formatted value (e.g., "16px")
 */
const char* css_format_px(float value);

/**
 * Format a unitless float value (for line-height, etc.)
 * @param value The float value
 * @return Static buffer containing formatted value
 */
const char* css_format_float(float value);

/**
 * Format a CSS numeric value with unit, stripping trailing zeros
 * @param buffer Output buffer
 * @param size Buffer size
 * @param value The numeric value
 * @param unit The unit string (e.g., "px", "%")
 */
void css_format_value(char* buffer, size_t size, float value, const char* unit);

// ============================================================================
// Property Mapping Tables
// ============================================================================

/**
 * CSS property category for grouping related properties
 */
typedef enum {
    CSS_PROP_CATEGORY_LAYOUT,
    CSS_PROP_CATEGORY_SPACING,
    CSS_PROP_CATEGORY_FLEXBOX,
    CSS_PROP_CATEGORY_GRID,
    CSS_PROP_CATEGORY_COLOR,
    CSS_PROP_CATEGORY_BORDER,
    CSS_PROP_CATEGORY_ALIGNMENT,
    CSS_PROP_CATEGORY_TYPOGRAPHY,
    CSS_PROP_CATEGORY_TRANSFORM,
    CSS_PROP_CATEGORY_EFFECTS,
    CSS_PROP_CATEGORY_ANIMATION,
    CSS_PROP_CATEGORY_POSITION,
    CSS_PROP_CATEGORY_RESPONSIVE,
    CSS_PROP_CATEGORY_MISC
} CSSPropertyCategory;

/**
 * CSS property type for value formatting
 */
typedef enum {
    CSS_PROP_TYPE_DIMENSION,
    CSS_PROP_TYPE_COLOR,
    CSS_PROP_TYPE_ALIGNMENT,
    CSS_PROP_TYPE_INTEGER,
    CSS_PROP_TYPE_FLOAT,
    CSS_PROP_TYPE_STRING,
    CSS_PROP_TYPE_BOOLEAN,
    CSS_PROP_TYPE_ENUM
} CSSPropertyType;

/**
 * CSS property formatter function type
 */
typedef const char* (*CSSFormatterFn)(void);

/**
 * CSS property mapping structure
 * Maps IR style properties to CSS property names and formatters
 */
typedef struct {
    const char* css_property;      // CSS property name (e.g., "width")
    CSSPropertyCategory category;  // Property category
    CSSPropertyType type;          // Property type for formatting
    size_t offset;                 // Offset in IRStyle struct
    CSSFormatterFn formatter;      // Formatter function (may be NULL)
    const char* default_value;     // Default value (skip if matches)
} CSSPropertyMapping;

// Layout property mappings (display, min/max-width, etc.)
extern const CSSPropertyMapping css_layout_properties[];
extern const size_t css_layout_property_count;

// Spacing property mappings (margin, padding)
extern const CSSPropertyMapping css_spacing_properties[];
extern const size_t css_spacing_property_count;

// Flexbox property mappings (flex-direction, justify-content, etc.)
extern const CSSPropertyMapping css_flexbox_properties[];
extern const size_t css_flexbox_property_count;

// Grid property mappings (grid-template-columns, gap, etc.)
extern const CSSPropertyMapping css_grid_properties[];
extern const size_t css_grid_property_count;

// Color property mappings (background-color, color, etc.)
extern const CSSPropertyMapping css_color_properties[];
extern const size_t css_color_property_count;

// Border property mappings (border-width, border-radius, etc.)
extern const CSSPropertyMapping css_border_properties[];
extern const size_t css_border_property_count;

// Alignment property mappings (text-align, vertical-align, etc.)
extern const CSSPropertyMapping css_alignment_properties[];
extern const size_t css_alignment_property_count;

// Typography property mappings (font-size, font-weight, etc.)
extern const CSSPropertyMapping css_typography_properties[];
extern const size_t css_typography_property_count;

// Transform property mappings (transform, translate, scale, rotate)
extern const CSSPropertyMapping css_transform_properties[];
extern const size_t css_transform_property_count;

// Effects property mappings (opacity, filter, box-shadow, text-shadow)
extern const CSSPropertyMapping css_effects_properties[];
extern const size_t css_effects_property_count;

// Animation property mappings (animation, transition)
extern const CSSPropertyMapping css_animation_properties[];
extern const size_t css_animation_property_count;

// Position property mappings (position, z-index, overflow)
extern const CSSPropertyMapping css_position_properties[];
extern const size_t css_position_property_count;

// Miscellaneous property mappings
extern const CSSPropertyMapping css_misc_properties[];
extern const size_t css_misc_property_count;

// ============================================================================
// Property Category Names
// ============================================================================

/**
 * Get the name of a property category
 * @param category The property category
 * @return Category name string
 */
const char* css_property_category_name(CSSPropertyCategory category);

#ifdef __cplusplus
}
#endif

#endif // CSS_PROPERTY_TABLES_H
