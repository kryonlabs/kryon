/**
 * @file css_property_tables.c
 * @brief Data-driven CSS property mapping tables implementation
 *
 * Implements formatter functions and property mapping tables for CSS generation.
 */

#define _GNU_SOURCE
#include "css_property_tables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// Thread-Local Buffers for Value Formatting
// ============================================================================

static __thread char px_buffer[8][32];
static __thread int px_buffer_idx = 0;

// ============================================================================
// Value Formatting Helpers
// ============================================================================

void css_format_value(char* buffer, size_t size, float value, const char* unit) {
    if (value == 0.0f) {
        strcpy(buffer, "0");
        return;
    }

    // Check if value is effectively an integer
    if (floorf(value) == value) {
        snprintf(buffer, size, "%.0f%s", value, unit);
    } else {
        // Format with precision, then strip trailing zeros
        snprintf(buffer, size, "%.2f%s", value, unit);
        // Find the end of the numeric part (before unit) and strip trailing zeros
        size_t len = strlen(buffer);
        size_t unit_len = strlen(unit);
        char* p = buffer + len - unit_len - 1;
        while (p > buffer && *p == '0') p--;
        if (*p == '.') p--;
        memmove(p + 1, buffer + len - unit_len, unit_len + 1);
    }
}

const char* css_format_px(float value) {
    char* buf = px_buffer[px_buffer_idx++ & 7];
    css_format_value(buf, 32, value, "px");
    return buf;
}

const char* css_format_float(float value) {
    char* buf = px_buffer[px_buffer_idx++ & 7];
    css_format_value(buf, 32, value, "");
    return buf;
}

// ============================================================================
// Formatter Functions
// ============================================================================

const char* css_format_dimension(IRDimension* value) {
    static char dim_buffer[32];

    if (!value) return "auto";

    switch (value->type) {
        case IR_DIMENSION_PX:
            css_format_value(dim_buffer, sizeof(dim_buffer), value->value, "px");
            break;
        case IR_DIMENSION_PERCENT:
            css_format_value(dim_buffer, sizeof(dim_buffer), value->value, "%");
            break;
        case IR_DIMENSION_VW:
            css_format_value(dim_buffer, sizeof(dim_buffer), value->value, "vw");
            break;
        case IR_DIMENSION_VH:
            css_format_value(dim_buffer, sizeof(dim_buffer), value->value, "vh");
            break;
        case IR_DIMENSION_VMIN:
            css_format_value(dim_buffer, sizeof(dim_buffer), value->value, "vmin");
            break;
        case IR_DIMENSION_VMAX:
            css_format_value(dim_buffer, sizeof(dim_buffer), value->value, "vmax");
            break;
        case IR_DIMENSION_REM:
            css_format_value(dim_buffer, sizeof(dim_buffer), value->value, "rem");
            break;
        case IR_DIMENSION_EM:
            css_format_value(dim_buffer, sizeof(dim_buffer), value->value, "em");
            break;
        case IR_DIMENSION_FLEX:
            css_format_value(dim_buffer, sizeof(dim_buffer), value->value, "fr");
            break;
        case IR_DIMENSION_AUTO:
        default:
            strcpy(dim_buffer, "auto");
            break;
    }
    return dim_buffer;
}

const char* css_format_color(IRColor* color) {
    static char color_buffer[512];

    if (!color) return "transparent";

    // Check for CSS variable name first (for roundtrip preservation)
    if (color->var_name) {
        snprintf(color_buffer, sizeof(color_buffer), "%s", color->var_name);
        return color_buffer;
    }

    switch (color->type) {
        case IR_COLOR_SOLID: {
            snprintf(color_buffer, sizeof(color_buffer), "rgba(%u, %u, %u, %.2f)",
                     color->data.r, color->data.g, color->data.b,
                     color->data.a / 255.0f);
            return color_buffer;
        }
        case IR_COLOR_TRANSPARENT:
            return "transparent";
        case IR_COLOR_VAR_REF: {
            snprintf(color_buffer, sizeof(color_buffer), "var(--color-%u)",
                     color->data.var_id);
            return color_buffer;
        }
        case IR_COLOR_GRADIENT: {
            if (!color->data.gradient || color->data.gradient->stop_count < 2) {
                return "transparent";
            }

            IRGradient* grad = color->data.gradient;

            if (grad->type == IR_GRADIENT_LINEAR) {
                int offset = snprintf(color_buffer, sizeof(color_buffer),
                                     "linear-gradient(%.0fdeg", grad->angle);

                for (int i = 0; i < grad->stop_count && offset < (int)sizeof(color_buffer) - 50; i++) {
                    IRGradientStop* stop = &grad->stops[i];
                    offset += snprintf(color_buffer + offset, sizeof(color_buffer) - offset,
                                     ", rgba(%u, %u, %u, %.2f) %.1f%%",
                                     stop->r, stop->g, stop->b, stop->a / 255.0f,
                                     stop->position * 100.0f);
                }

                if (offset < (int)sizeof(color_buffer) - 1) {
                    color_buffer[offset++] = ')';
                    color_buffer[offset] = '\0';
                }
                return color_buffer;

            } else if (grad->type == IR_GRADIENT_RADIAL) {
                int offset = snprintf(color_buffer, sizeof(color_buffer),
                                     "radial-gradient(circle at %.1f%% %.1f%%",
                                     grad->center_x * 100.0f, grad->center_y * 100.0f);

                for (int i = 0; i < grad->stop_count && offset < (int)sizeof(color_buffer) - 50; i++) {
                    IRGradientStop* stop = &grad->stops[i];
                    offset += snprintf(color_buffer + offset, sizeof(color_buffer) - offset,
                                     ", rgba(%u, %u, %u, %.2f) %.1f%%",
                                     stop->r, stop->g, stop->b, stop->a / 255.0f,
                                     stop->position * 100.0f);
                }

                if (offset < (int)sizeof(color_buffer) - 1) {
                    color_buffer[offset++] = ')';
                    color_buffer[offset] = '\0';
                }
                return color_buffer;

            } else if (grad->type == IR_GRADIENT_CONIC) {
                int offset = snprintf(color_buffer, sizeof(color_buffer),
                                     "conic-gradient(from 0deg at %.1f%% %.1f%%",
                                     grad->center_x * 100.0f, grad->center_y * 100.0f);

                for (int i = 0; i < grad->stop_count && offset < (int)sizeof(color_buffer) - 50; i++) {
                    IRGradientStop* stop = &grad->stops[i];
                    offset += snprintf(color_buffer + offset, sizeof(color_buffer) - offset,
                                     ", rgba(%u, %u, %u, %.2f) %.1f%%",
                                     stop->r, stop->g, stop->b, stop->a / 255.0f,
                                     stop->position * 100.0f);
                }

                if (offset < (int)sizeof(color_buffer) - 1) {
                    color_buffer[offset++] = ')';
                    color_buffer[offset] = '\0';
                }
                return color_buffer;
            }
            return "transparent";
        }
        default:
            return "transparent";
    }
}

const char* css_format_alignment(IRAlignment alignment) {
    switch (alignment) {
        case IR_ALIGNMENT_START: return "flex-start";
        case IR_ALIGNMENT_CENTER: return "center";
        case IR_ALIGNMENT_END: return "flex-end";
        case IR_ALIGNMENT_STRETCH: return "stretch";
        case IR_ALIGNMENT_SPACE_BETWEEN: return "space-between";
        case IR_ALIGNMENT_SPACE_AROUND: return "space-around";
        case IR_ALIGNMENT_SPACE_EVENLY: return "space-evenly";
        default: return "flex-start";
    }
}

const char* css_format_overflow(IROverflowMode overflow) {
    switch (overflow) {
        case IR_OVERFLOW_HIDDEN: return "hidden";
        case IR_OVERFLOW_SCROLL: return "scroll";
        case IR_OVERFLOW_AUTO: return "auto";
        case IR_OVERFLOW_VISIBLE:
        default: return "visible";
    }
}

const char* css_format_text_align(IRTextAlign align) {
    switch (align) {
        case IR_TEXT_ALIGN_LEFT: return "left";
        case IR_TEXT_ALIGN_RIGHT: return "right";
        case IR_TEXT_ALIGN_CENTER: return "center";
        case IR_TEXT_ALIGN_JUSTIFY: return "justify";
        default: return "left";
    }
}

const char* css_format_position(IRPositionMode mode) {
    switch (mode) {
        case IR_POSITION_ABSOLUTE: return "absolute";
        case IR_POSITION_FIXED: return "fixed";
        case IR_POSITION_RELATIVE:
        default: return "relative";
    }
}

const char* css_format_grid_track(IRGridTrack* track) {
    static char track_buffer[32];

    if (!track) return "1fr";

    switch (track->type) {
        case IR_GRID_TRACK_PX:
            snprintf(track_buffer, sizeof(track_buffer), "%.0fpx", track->value);
            return track_buffer;
        case IR_GRID_TRACK_PERCENT:
            snprintf(track_buffer, sizeof(track_buffer), "%.0f%%", track->value);
            return track_buffer;
        case IR_GRID_TRACK_FR:
            snprintf(track_buffer, sizeof(track_buffer), "%.0ffr", track->value);
            return track_buffer;
        case IR_GRID_TRACK_AUTO:
            return "auto";
        case IR_GRID_TRACK_MIN_CONTENT:
            return "min-content";
        case IR_GRID_TRACK_MAX_CONTENT:
            return "max-content";
        default:
            return "1fr";
    }
}

const char* css_format_grid_track_minmax(IRGridTrackType type, float value,
                                         char* buffer, size_t size) {
    switch (type) {
        case IR_GRID_TRACK_PX:
            snprintf(buffer, size, "%.0fpx", value);
            break;
        case IR_GRID_TRACK_PERCENT:
            snprintf(buffer, size, "%.0f%%", value);
            break;
        case IR_GRID_TRACK_FR:
            snprintf(buffer, size, "%.0ffr", value);
            break;
        case IR_GRID_TRACK_AUTO:
            strncpy(buffer, "auto", size);
            break;
        case IR_GRID_TRACK_MIN_CONTENT:
            strncpy(buffer, "min-content", size);
            break;
        case IR_GRID_TRACK_MAX_CONTENT:
            strncpy(buffer, "max-content", size);
            break;
        default:
            strncpy(buffer, "auto", size);
            break;
    }
    return buffer;
}

const char* css_format_layout_mode(IRLayoutMode mode) {
    switch (mode) {
        case IR_LAYOUT_MODE_FLEX: return "flex";
        case IR_LAYOUT_MODE_INLINE_FLEX: return "inline-flex";
        case IR_LAYOUT_MODE_GRID: return "grid";
        case IR_LAYOUT_MODE_INLINE_GRID: return "inline-grid";
        case IR_LAYOUT_MODE_BLOCK: return "block";
        case IR_LAYOUT_MODE_INLINE: return "inline";
        case IR_LAYOUT_MODE_INLINE_BLOCK: return "inline-block";
        case IR_LAYOUT_MODE_NONE: return "none";
        default: return "flex";
    }
}

const char* css_format_easing(IREasingType easing) {
    switch (easing) {
        case IR_EASING_LINEAR: return "linear";
        case IR_EASING_EASE_IN: return "ease-in";
        case IR_EASING_EASE_OUT: return "ease-out";
        case IR_EASING_EASE_IN_OUT: return "ease-in-out";
        case IR_EASING_EASE_IN_QUAD: return "cubic-bezier(0.55, 0.085, 0.68, 0.53)";
        case IR_EASING_EASE_OUT_QUAD: return "cubic-bezier(0.25, 0.46, 0.45, 0.94)";
        case IR_EASING_EASE_IN_OUT_QUAD: return "cubic-bezier(0.455, 0.03, 0.515, 0.955)";
        case IR_EASING_EASE_IN_CUBIC: return "cubic-bezier(0.55, 0.055, 0.675, 0.19)";
        case IR_EASING_EASE_OUT_CUBIC: return "cubic-bezier(0.215, 0.61, 0.355, 1)";
        case IR_EASING_EASE_IN_OUT_CUBIC: return "cubic-bezier(0.645, 0.045, 0.355, 1)";
        case IR_EASING_EASE_IN_BOUNCE: return "cubic-bezier(0.6, -0.28, 0.735, 0.045)";
        case IR_EASING_EASE_OUT_BOUNCE: return "cubic-bezier(0.68, -0.55, 0.265, 1.55)";
        default: return "ease";
    }
}

const char* css_format_animation_property(IRAnimationProperty prop) {
    switch (prop) {
        case IR_ANIM_PROP_OPACITY: return "opacity";
        case IR_ANIM_PROP_TRANSLATE_X: return "transform";
        case IR_ANIM_PROP_TRANSLATE_Y: return "transform";
        case IR_ANIM_PROP_SCALE_X: return "transform";
        case IR_ANIM_PROP_SCALE_Y: return "transform";
        case IR_ANIM_PROP_ROTATE: return "transform";
        case IR_ANIM_PROP_WIDTH: return "width";
        case IR_ANIM_PROP_HEIGHT: return "height";
        case IR_ANIM_PROP_BACKGROUND_COLOR: return "background-color";
        default: return NULL;
    }
}

const char* css_format_pseudo_class(IRPseudoState state) {
    switch (state) {
        case IR_PSEUDO_HOVER: return "hover";
        case IR_PSEUDO_ACTIVE: return "active";
        case IR_PSEUDO_FOCUS: return "focus";
        case IR_PSEUDO_DISABLED: return "disabled";
        case IR_PSEUDO_CHECKED: return "checked";
        case IR_PSEUDO_FIRST_CHILD: return "first-child";
        case IR_PSEUDO_LAST_CHILD: return "last-child";
        case IR_PSEUDO_VISITED: return "visited";
        default: return NULL;
    }
}

const char* css_format_direction(IRDirection direction) {
    switch (direction) {
        case IR_DIRECTION_LTR: return "ltr";
        case IR_DIRECTION_RTL: return "rtl";
        case IR_DIRECTION_INHERIT: return "inherit";
        case IR_DIRECTION_AUTO:
        default: return "auto";
    }
}

const char* css_format_unicode_bidi(IRUnicodeBidi bidi) {
    switch (bidi) {
        case IR_UNICODE_BIDI_NORMAL: return "normal";
        case IR_UNICODE_BIDI_EMBED: return "embed";
        case IR_UNICODE_BIDI_ISOLATE: return "isolate";
        case IR_UNICODE_BIDI_PLAINTEXT: return "plaintext";
        default: return "normal";
    }
}

const char* css_format_object_fit(IRObjectFit fit) {
    switch (fit) {
        case IR_OBJECT_FIT_CONTAIN: return "contain";
        case IR_OBJECT_FIT_COVER: return "cover";
        case IR_OBJECT_FIT_NONE: return "none";
        case IR_OBJECT_FIT_SCALE_DOWN: return "scale-down";
        case IR_OBJECT_FIT_FILL:
        default: return "fill";
    }
}

const char* css_format_background_clip(IRBackgroundClip clip) {
    switch (clip) {
        case IR_BACKGROUND_CLIP_PADDING_BOX: return "padding-box";
        case IR_BACKGROUND_CLIP_CONTENT_BOX: return "content-box";
        case IR_BACKGROUND_CLIP_TEXT: return "text";
        case IR_BACKGROUND_CLIP_BORDER_BOX:
        default: return "border-box";
    }
}

// ============================================================================
// Property Category Names
// ============================================================================

const char* css_property_category_name(CSSPropertyCategory category) {
    switch (category) {
        case CSS_PROP_CATEGORY_LAYOUT: return "Layout";
        case CSS_PROP_CATEGORY_SPACING: return "Spacing";
        case CSS_PROP_CATEGORY_FLEXBOX: return "Flexbox";
        case CSS_PROP_CATEGORY_GRID: return "Grid";
        case CSS_PROP_CATEGORY_COLOR: return "Color";
        case CSS_PROP_CATEGORY_BORDER: return "Border";
        case CSS_PROP_CATEGORY_ALIGNMENT: return "Alignment";
        case CSS_PROP_CATEGORY_TYPOGRAPHY: return "Typography";
        case CSS_PROP_CATEGORY_TRANSFORM: return "Transform";
        case CSS_PROP_CATEGORY_EFFECTS: return "Effects";
        case CSS_PROP_CATEGORY_ANIMATION: return "Animation";
        case CSS_PROP_CATEGORY_POSITION: return "Position";
        case CSS_PROP_CATEGORY_RESPONSIVE: return "Responsive";
        case CSS_PROP_CATEGORY_MISC: return "Miscellaneous";
        default: return "Unknown";
    }
}

// ============================================================================
// Property Mapping Tables
// ============================================================================

// Note: These tables provide metadata about CSS properties.
// The offsets are illustrative - actual property generation uses direct struct access.

// Layout properties
const CSSPropertyMapping css_layout_properties[] = {
    { "width", CSS_PROP_CATEGORY_LAYOUT, CSS_PROP_TYPE_DIMENSION, 0, NULL, "auto" },
    { "height", CSS_PROP_CATEGORY_LAYOUT, CSS_PROP_TYPE_DIMENSION, 0, NULL, "auto" },
    { "min-width", CSS_PROP_CATEGORY_LAYOUT, CSS_PROP_TYPE_DIMENSION, 0, NULL, "auto" },
    { "min-height", CSS_PROP_CATEGORY_LAYOUT, CSS_PROP_TYPE_DIMENSION, 0, NULL, "auto" },
    { "max-width", CSS_PROP_CATEGORY_LAYOUT, CSS_PROP_TYPE_DIMENSION, 0, NULL, "none" },
    { "max-height", CSS_PROP_CATEGORY_LAYOUT, CSS_PROP_TYPE_DIMENSION, 0, NULL, "none" },
    { "aspect-ratio", CSS_PROP_CATEGORY_LAYOUT, CSS_PROP_TYPE_FLOAT, 0, NULL, "auto" },
};
const size_t css_layout_property_count = sizeof(css_layout_properties) / sizeof(css_layout_properties[0]);

// Spacing properties
const CSSPropertyMapping css_spacing_properties[] = {
    { "margin", CSS_PROP_CATEGORY_SPACING, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "margin-top", CSS_PROP_CATEGORY_SPACING, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "margin-right", CSS_PROP_CATEGORY_SPACING, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "margin-bottom", CSS_PROP_CATEGORY_SPACING, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "margin-left", CSS_PROP_CATEGORY_SPACING, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "padding", CSS_PROP_CATEGORY_SPACING, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "padding-top", CSS_PROP_CATEGORY_SPACING, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "padding-right", CSS_PROP_CATEGORY_SPACING, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "padding-bottom", CSS_PROP_CATEGORY_SPACING, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "padding-left", CSS_PROP_CATEGORY_SPACING, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "gap", CSS_PROP_CATEGORY_SPACING, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
};
const size_t css_spacing_property_count = sizeof(css_spacing_properties) / sizeof(css_spacing_properties[0]);

// Flexbox properties
const CSSPropertyMapping css_flexbox_properties[] = {
    { "display", CSS_PROP_CATEGORY_FLEXBOX, CSS_PROP_TYPE_STRING, 0, NULL, "block" },
    { "flex-direction", CSS_PROP_CATEGORY_FLEXBOX, CSS_PROP_TYPE_STRING, 0, NULL, "column" },
    { "flex-wrap", CSS_PROP_CATEGORY_FLEXBOX, CSS_PROP_TYPE_BOOLEAN, 0, NULL, "nowrap" },
    { "flex-grow", CSS_PROP_CATEGORY_FLEXBOX, CSS_PROP_TYPE_INTEGER, 0, NULL, "0" },
    { "flex-shrink", CSS_PROP_CATEGORY_FLEXBOX, CSS_PROP_TYPE_INTEGER, 0, NULL, "1" },
    { "flex-basis", CSS_PROP_CATEGORY_FLEXBOX, CSS_PROP_TYPE_STRING, 0, NULL, "auto" },
    { "justify-content", CSS_PROP_CATEGORY_FLEXBOX, CSS_PROP_TYPE_ENUM, 0, (CSSFormatterFn)css_format_alignment, "flex-start" },
    { "align-items", CSS_PROP_CATEGORY_FLEXBOX, CSS_PROP_TYPE_ENUM, 0, (CSSFormatterFn)css_format_alignment, "stretch" },
    { "align-content", CSS_PROP_CATEGORY_FLEXBOX, CSS_PROP_TYPE_ENUM, 0, (CSSFormatterFn)css_format_alignment, "flex-start" },
};
const size_t css_flexbox_property_count = sizeof(css_flexbox_properties) / sizeof(css_flexbox_properties[0]);

// Grid properties
const CSSPropertyMapping css_grid_properties[] = {
    { "display", CSS_PROP_CATEGORY_GRID, CSS_PROP_TYPE_STRING, 0, NULL, "block" },
    { "grid-template-columns", CSS_PROP_CATEGORY_GRID, CSS_PROP_TYPE_STRING, 0, NULL, "none" },
    { "grid-template-rows", CSS_PROP_CATEGORY_GRID, CSS_PROP_TYPE_STRING, 0, NULL, "none" },
    { "grid-column", CSS_PROP_CATEGORY_GRID, CSS_PROP_TYPE_STRING, 0, NULL, "auto" },
    { "grid-row", CSS_PROP_CATEGORY_GRID, CSS_PROP_TYPE_STRING, 0, NULL, "auto" },
    { "grid-column-gap", CSS_PROP_CATEGORY_GRID, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "grid-row-gap", CSS_PROP_CATEGORY_GRID, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "justify-items", CSS_PROP_CATEGORY_GRID, CSS_PROP_TYPE_ENUM, 0, (CSSFormatterFn)css_format_alignment, "start" },
    { "align-items", CSS_PROP_CATEGORY_GRID, CSS_PROP_TYPE_ENUM, 0, (CSSFormatterFn)css_format_alignment, "start" },
    { "justify-self", CSS_PROP_CATEGORY_GRID, CSS_PROP_TYPE_ENUM, 0, (CSSFormatterFn)css_format_alignment, "auto" },
    { "align-self", CSS_PROP_CATEGORY_GRID, CSS_PROP_TYPE_ENUM, 0, (CSSFormatterFn)css_format_alignment, "auto" },
};
const size_t css_grid_property_count = sizeof(css_grid_properties) / sizeof(css_grid_properties[0]);

// Color properties
const CSSPropertyMapping css_color_properties[] = {
    { "background-color", CSS_PROP_CATEGORY_COLOR, CSS_PROP_TYPE_COLOR, 0, NULL, "transparent" },
    { "background", CSS_PROP_CATEGORY_COLOR, CSS_PROP_TYPE_COLOR, 0, NULL, "transparent" },
    { "color", CSS_PROP_CATEGORY_COLOR, CSS_PROP_TYPE_COLOR, 0, NULL, "inherit" },
    { "opacity", CSS_PROP_CATEGORY_COLOR, CSS_PROP_TYPE_FLOAT, 0, NULL, "1" },
};
const size_t css_color_property_count = sizeof(css_color_properties) / sizeof(css_color_properties[0]);

// Border properties
const CSSPropertyMapping css_border_properties[] = {
    { "border", CSS_PROP_CATEGORY_BORDER, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "border-top", CSS_PROP_CATEGORY_BORDER, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "border-right", CSS_PROP_CATEGORY_BORDER, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "border-bottom", CSS_PROP_CATEGORY_BORDER, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "border-left", CSS_PROP_CATEGORY_BORDER, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "border-width", CSS_PROP_CATEGORY_BORDER, CSS_PROP_TYPE_STRING, 0, NULL, "0" },
    { "border-color", CSS_PROP_CATEGORY_BORDER, CSS_PROP_TYPE_COLOR, 0, NULL, "currentColor" },
    { "border-radius", CSS_PROP_CATEGORY_BORDER, CSS_PROP_TYPE_INTEGER, 0, NULL, "0" },
};
const size_t css_border_property_count = sizeof(css_border_properties) / sizeof(css_border_properties[0]);

// Alignment properties
const CSSPropertyMapping css_alignment_properties[] = {
    { "text-align", CSS_PROP_CATEGORY_ALIGNMENT, CSS_PROP_TYPE_ENUM, 0, (CSSFormatterFn)css_format_text_align, "left" },
    { "vertical-align", CSS_PROP_CATEGORY_ALIGNMENT, CSS_PROP_TYPE_STRING, 0, NULL, "baseline" },
};
const size_t css_alignment_property_count = sizeof(css_alignment_properties) / sizeof(css_alignment_properties[0]);

// Typography properties
const CSSPropertyMapping css_typography_properties[] = {
    { "font-family", CSS_PROP_CATEGORY_TYPOGRAPHY, CSS_PROP_TYPE_STRING, 0, NULL, "inherit" },
    { "font-size", CSS_PROP_CATEGORY_TYPOGRAPHY, CSS_PROP_TYPE_DIMENSION, 0, NULL, "medium" },
    { "font-weight", CSS_PROP_CATEGORY_TYPOGRAPHY, CSS_PROP_TYPE_INTEGER, 0, NULL, "normal" },
    { "font-style", CSS_PROP_CATEGORY_TYPOGRAPHY, CSS_PROP_TYPE_STRING, 0, NULL, "normal" },
    { "line-height", CSS_PROP_CATEGORY_TYPOGRAPHY, CSS_PROP_TYPE_FLOAT, 0, NULL, "normal" },
    { "letter-spacing", CSS_PROP_CATEGORY_TYPOGRAPHY, CSS_PROP_TYPE_STRING, 0, NULL, "normal" },
    { "word-spacing", CSS_PROP_CATEGORY_TYPOGRAPHY, CSS_PROP_TYPE_STRING, 0, NULL, "normal" },
    { "text-decoration", CSS_PROP_CATEGORY_TYPOGRAPHY, CSS_PROP_TYPE_STRING, 0, NULL, "none" },
    { "text-overflow", CSS_PROP_CATEGORY_TYPOGRAPHY, CSS_PROP_TYPE_STRING, 0, NULL, "clip" },
};
const size_t css_typography_property_count = sizeof(css_typography_properties) / sizeof(css_typography_properties[0]);

// Transform properties
const CSSPropertyMapping css_transform_properties[] = {
    { "transform", CSS_PROP_CATEGORY_TRANSFORM, CSS_PROP_TYPE_STRING, 0, NULL, "none" },
    { "transform-origin", CSS_PROP_CATEGORY_TRANSFORM, CSS_PROP_TYPE_STRING, 0, NULL, "50% 50%" },
};
const size_t css_transform_property_count = sizeof(css_transform_properties) / sizeof(css_transform_properties[0]);

// Effects properties
const CSSPropertyMapping css_effects_properties[] = {
    { "opacity", CSS_PROP_CATEGORY_EFFECTS, CSS_PROP_TYPE_FLOAT, 0, NULL, "1" },
    { "filter", CSS_PROP_CATEGORY_EFFECTS, CSS_PROP_TYPE_STRING, 0, NULL, "none" },
    { "box-shadow", CSS_PROP_CATEGORY_EFFECTS, CSS_PROP_TYPE_STRING, 0, NULL, "none" },
    { "text-shadow", CSS_PROP_CATEGORY_EFFECTS, CSS_PROP_TYPE_STRING, 0, NULL, "none" },
};
const size_t css_effects_property_count = sizeof(css_effects_properties) / sizeof(css_effects_properties[0]);

// Animation properties
const CSSPropertyMapping css_animation_properties[] = {
    { "animation", CSS_PROP_CATEGORY_ANIMATION, CSS_PROP_TYPE_STRING, 0, NULL, "none" },
    { "animation-name", CSS_PROP_CATEGORY_ANIMATION, CSS_PROP_TYPE_STRING, 0, NULL, "none" },
    { "animation-duration", CSS_PROP_CATEGORY_ANIMATION, CSS_PROP_TYPE_STRING, 0, NULL, "0s" },
    { "animation-timing-function", CSS_PROP_CATEGORY_ANIMATION, CSS_PROP_TYPE_ENUM, 0, (CSSFormatterFn)css_format_easing, "ease" },
    { "animation-delay", CSS_PROP_CATEGORY_ANIMATION, CSS_PROP_TYPE_STRING, 0, NULL, "0s" },
    { "animation-iteration-count", CSS_PROP_CATEGORY_ANIMATION, CSS_PROP_TYPE_STRING, 0, NULL, "1" },
    { "animation-direction", CSS_PROP_CATEGORY_ANIMATION, CSS_PROP_TYPE_STRING, 0, NULL, "normal" },
    { "transition", CSS_PROP_CATEGORY_ANIMATION, CSS_PROP_TYPE_STRING, 0, NULL, "none" },
    { "transition-property", CSS_PROP_CATEGORY_ANIMATION, CSS_PROP_TYPE_STRING, 0, NULL, "all" },
    { "transition-duration", CSS_PROP_CATEGORY_ANIMATION, CSS_PROP_TYPE_STRING, 0, NULL, "0s" },
    { "transition-timing-function", CSS_PROP_CATEGORY_ANIMATION, CSS_PROP_TYPE_ENUM, 0, (CSSFormatterFn)css_format_easing, "ease" },
    { "transition-delay", CSS_PROP_CATEGORY_ANIMATION, CSS_PROP_TYPE_STRING, 0, NULL, "0s" },
};
const size_t css_animation_property_count = sizeof(css_animation_properties) / sizeof(css_animation_properties[0]);

// Position properties
const CSSPropertyMapping css_position_properties[] = {
    { "position", CSS_PROP_CATEGORY_POSITION, CSS_PROP_TYPE_ENUM, 0, (CSSFormatterFn)css_format_position, "relative" },
    { "top", CSS_PROP_CATEGORY_POSITION, CSS_PROP_TYPE_STRING, 0, NULL, "auto" },
    { "right", CSS_PROP_CATEGORY_POSITION, CSS_PROP_TYPE_STRING, 0, NULL, "auto" },
    { "bottom", CSS_PROP_CATEGORY_POSITION, CSS_PROP_TYPE_STRING, 0, NULL, "auto" },
    { "left", CSS_PROP_CATEGORY_POSITION, CSS_PROP_TYPE_STRING, 0, NULL, "auto" },
    { "z-index", CSS_PROP_CATEGORY_POSITION, CSS_PROP_TYPE_INTEGER, 0, NULL, "auto" },
    { "overflow", CSS_PROP_CATEGORY_POSITION, CSS_PROP_TYPE_ENUM, 0, (CSSFormatterFn)css_format_overflow, "visible" },
    { "overflow-x", CSS_PROP_CATEGORY_POSITION, CSS_PROP_TYPE_ENUM, 0, (CSSFormatterFn)css_format_overflow, "visible" },
    { "overflow-y", CSS_PROP_CATEGORY_POSITION, CSS_PROP_TYPE_ENUM, 0, (CSSFormatterFn)css_format_overflow, "visible" },
};
const size_t css_position_property_count = sizeof(css_position_properties) / sizeof(css_position_properties[0]);

// Miscellaneous properties
const CSSPropertyMapping css_misc_properties[] = {
    { "cursor", CSS_PROP_CATEGORY_MISC, CSS_PROP_TYPE_STRING, 0, NULL, "auto" },
    { "pointer-events", CSS_PROP_CATEGORY_MISC, CSS_PROP_TYPE_STRING, 0, NULL, "auto" },
    { "user-select", CSS_PROP_CATEGORY_MISC, CSS_PROP_TYPE_STRING, 0, NULL, "auto" },
    { "visibility", CSS_PROP_CATEGORY_MISC, CSS_PROP_TYPE_STRING, 0, NULL, "visible" },
    { "box-sizing", CSS_PROP_CATEGORY_MISC, CSS_PROP_TYPE_STRING, 0, NULL, "content-box" },
    { "object-fit", CSS_PROP_CATEGORY_MISC, CSS_PROP_TYPE_ENUM, 0, (CSSFormatterFn)css_format_object_fit, "fill" },
};
const size_t css_misc_property_count = sizeof(css_misc_properties) / sizeof(css_misc_properties[0]);
