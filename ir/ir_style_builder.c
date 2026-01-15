// IR Style Builder Module
// Style property setters extracted from ir_builder.c

#define _GNU_SOURCE
#include "ir_style_builder.h"
#include "ir_builder.h"
#include "ir_memory.h"
#include "ir_animation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for destroy functions
extern void ir_animation_destroy(struct IRAnimation* anim);
extern void ir_transition_destroy(struct IRTransition* transition);
extern void ir_gradient_destroy(struct IRGradient* gradient);

// Helper function to mark component dirty when style changes
static void mark_style_dirty(IRComponent* component) {
    if (!component) return;
    ir_layout_mark_dirty(component);
    if (component->layout_state) {
        component->layout_state->dirty_flags |= IR_DIRTY_STYLE | IR_DIRTY_LAYOUT;
    }
}

// ============================================================================
// Style Management
// ============================================================================

IRStyle* ir_create_style(void) {
    IRStyle* style = malloc(sizeof(IRStyle));
    if (!style) return NULL;

    memset(style, 0, sizeof(IRStyle));

    // Set sensible defaults
    style->visible = true;
    style->opacity = 1.0f;
    style->position_mode = IR_POSITION_RELATIVE;
    style->absolute_x = 0.0f;
    style->absolute_y = 0.0f;

    // Initialize transforms to identity (no transformation)
    style->transform.scale_x = 1.0f;
    style->transform.scale_y = 1.0f;

    // Set default line height (1.5 is CSS standard)
    style->font.line_height = 1.5f;
    style->transform.translate_x = 0.0f;
    style->transform.translate_y = 0.0f;
    style->transform.rotate = 0.0f;

    // Default dimensions to AUTO so components can be stretched by align-items: stretch
    style->width.type = IR_DIMENSION_AUTO;
    style->width.value = 0.0f;
    style->height.type = IR_DIMENSION_AUTO;
    style->height.value = 0.0f;

    // Grid item defaults (-1 means auto placement)
    style->grid_item.row_start = -1;
    style->grid_item.row_end = -1;
    style->grid_item.column_start = -1;
    style->grid_item.column_end = -1;

    // Text effect defaults
    style->text_effect.max_width.type = IR_DIMENSION_PX;
    style->text_effect.max_width.value = 0.0f;  // 0 means no wrapping
    style->text_effect.text_direction = IR_TEXT_DIR_AUTO;  // Auto-detect direction
    style->text_effect.language = NULL;  // No language specified
    style->grid_item.justify_self = IR_ALIGNMENT_START;
    style->grid_item.align_self = IR_ALIGNMENT_START;

    return style;
}

void ir_destroy_style(IRStyle* style) {
    if (!style) return;

    // Free animations
    if (style->animations) {
        for (uint32_t i = 0; i < style->animation_count; i++) {
            if (style->animations[i]) {
                ir_animation_destroy(style->animations[i]);
            }
        }
        free(style->animations);
    }

    // Free transitions
    if (style->transitions) {
        for (uint32_t i = 0; i < style->transition_count; i++) {
            if (style->transitions[i]) {
                ir_transition_destroy(style->transitions[i]);
            }
        }
        free(style->transitions);
    }

    // Free gradient objects
    if (style->background.type == IR_COLOR_GRADIENT && style->background.data.gradient) {
        ir_gradient_destroy(style->background.data.gradient);
    }
    if (style->border_color.type == IR_COLOR_GRADIENT && style->border_color.data.gradient) {
        ir_gradient_destroy(style->border_color.data.gradient);
    }

    if (style->font.family) free(style->font.family);

    free(style);
}

void ir_set_style(IRComponent* component, IRStyle* style) {
    if (!component) return;

    if (component->style) {
        ir_destroy_style(component->style);
    }
    component->style = style;
}

IRStyle* ir_get_style(IRComponent* component) {
    if (!component) return NULL;

    if (!component->style) {
        component->style = ir_create_style();
    }

    return component->style;
}

// ============================================================================
// Visible and Z-Index
// ============================================================================

void ir_set_visible(IRStyle* style, bool visible) {
    if (!style) return;
    style->visible = visible;
}

void ir_set_z_index(IRStyle* style, uint32_t z_index) {
    if (!style) return;
    style->z_index = z_index;
}

// ============================================================================
// Text Effect Helpers
// ============================================================================

void ir_set_text_overflow(IRStyle* style, IRTextOverflowType overflow) {
    if (!style) return;
    style->text_effect.overflow = overflow;
}

void ir_set_text_fade(IRStyle* style, IRTextFadeType fade_type, float fade_length) {
    if (!style) return;
    style->text_effect.fade_type = fade_type;
    style->text_effect.fade_length = fade_length;
}

void ir_set_text_shadow(IRStyle* style, float offset_x, float offset_y, float blur_radius,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!style) return;
    style->text_effect.shadow.offset_x = offset_x;
    style->text_effect.shadow.offset_y = offset_y;
    style->text_effect.shadow.blur_radius = blur_radius;
    style->text_effect.shadow.color.type = IR_COLOR_SOLID;
    style->text_effect.shadow.color.data.r = r;
    style->text_effect.shadow.color.data.g = g;
    style->text_effect.shadow.color.data.b = b;
    style->text_effect.shadow.color.data.a = a;
    style->text_effect.shadow.enabled = true;
}

// ============================================================================
// Opacity
// ============================================================================

void ir_set_opacity(IRStyle* style, float opacity) {
    if (!style) return;
    style->opacity = opacity;
}

void ir_set_disabled(IRComponent* component, bool disabled) {
    if (!component) return;

    component->is_disabled = disabled;

    // Apply visual styling for disabled state
    if (disabled) {
        // Ensure style exists
        if (!component->style) {
            component->style = ir_create_style();
        }

        // Set 50% opacity for disabled appearance
        ir_set_opacity(component->style, 0.5f);
    }

    // Mark component as dirty to trigger re-render
    mark_style_dirty(component);
}

// ============================================================================
// Text Layout
// ============================================================================

void ir_set_text_max_width(IRStyle* style, IRDimensionType type, float value) {
    if (!style) return;
    style->text_effect.max_width.type = type;
    style->text_effect.max_width.value = value;
}

// ============================================================================
// Box Shadow and Filters
// ============================================================================

void ir_set_box_shadow(IRStyle* style, float offset_x, float offset_y, float blur_radius,
                       float spread_radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool inset) {
    if (!style) return;
    style->box_shadow.offset_x = offset_x;
    style->box_shadow.offset_y = offset_y;
    style->box_shadow.blur_radius = blur_radius;
    style->box_shadow.spread_radius = spread_radius;
    style->box_shadow.color.type = IR_COLOR_SOLID;
    style->box_shadow.color.data.r = r;
    style->box_shadow.color.data.g = g;
    style->box_shadow.color.data.b = b;
    style->box_shadow.color.data.a = a;
    style->box_shadow.inset = inset;
    style->box_shadow.enabled = true;
}

void ir_add_filter(IRStyle* style, IRFilterType type, float value) {
    if (!style || style->filter_count >= IR_MAX_FILTERS) return;
    style->filters[style->filter_count].type = type;
    style->filters[style->filter_count].value = value;
    style->filter_count++;
}

void ir_clear_filters(IRStyle* style) {
    if (!style) return;
    style->filter_count = 0;
}

// ============================================================================
// Style Property Helpers - Component Size
// ============================================================================

void ir_set_width(IRComponent* component, IRDimensionType type, float value) {
    if (!component || !component->style) return;
    component->style->width.type = type;
    component->style->width.value = value;
    ir_layout_mark_dirty(component);
}

void ir_set_height(IRComponent* component, IRDimensionType type, float value) {
    if (!component || !component->style) return;
    component->style->height.type = type;
    component->style->height.value = value;
    ir_layout_mark_dirty(component);
}

// ============================================================================
// Background Color
// ============================================================================

void ir_set_background_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!style) return;
    style->background.type = IR_COLOR_SOLID;
    style->background.data.r = r;
    style->background.data.g = g;
    style->background.data.b = b;
    style->background.data.a = a;
}

// ============================================================================
// Border Properties
// ============================================================================

void ir_set_border(IRStyle* style, float width, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t radius) {
    if (!style) return;
    style->border.width = width;
    style->border.color.type = IR_COLOR_SOLID;
    style->border.color.data.r = r;
    style->border.color.data.g = g;
    style->border.color.data.b = b;
    style->border.color.data.a = a;
    style->border.radius = radius;
}

// Individual border property setters (preserve other properties)
void ir_set_border_width(IRStyle* style, float width) {
    if (!style) return;
    style->border.width = width;
}

void ir_set_border_radius(IRStyle* style, uint8_t radius) {
    if (!style) return;
    style->border.radius = radius;
}

void ir_set_border_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!style) return;
    style->border.color.type = IR_COLOR_SOLID;
    style->border.color.data.r = r;
    style->border.color.data.g = g;
    style->border.color.data.b = b;
    style->border.color.data.a = a;
}

// ============================================================================
// Margin and Padding
// ============================================================================

void ir_set_margin(IRComponent* component, float top, float right, float bottom, float left) {
    if (!component || !component->style) return;
    component->style->margin.top = top;
    component->style->margin.right = right;
    component->style->margin.bottom = bottom;
    component->style->margin.left = left;
    ir_layout_mark_dirty(component);
}

void ir_set_padding(IRComponent* component, float top, float right, float bottom, float left) {
    if (!component || !component->style) return;
    component->style->padding.top = top;
    component->style->padding.right = right;
    component->style->padding.bottom = bottom;
    component->style->padding.left = left;
    ir_layout_mark_dirty(component);
}

// ============================================================================
// Font Properties
// ============================================================================

void ir_set_font(IRStyle* style, float size, const char* family, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool bold, bool italic) {
    if (!style) return;

    // Free old font family before replacing
    if (style->font.family) {
        free((void*)style->font.family);
        style->font.family = NULL;
    }

    style->font.size = size;
    style->font.family = family ? strdup(family) : NULL;
    style->font.color.type = IR_COLOR_SOLID;
    style->font.color.data.r = r;
    style->font.color.data.g = g;
    style->font.color.data.b = b;
    style->font.color.data.a = a;
    style->font.bold = bold;
    style->font.italic = italic;
}

// Individual font property setters (preserve other properties)
void ir_set_font_size(IRStyle* style, float size) {
    if (!style) return;
    style->font.size = size;
}

void ir_set_font_family(IRStyle* style, const char* family) {
    if (!style) return;

    // Free old font family before replacing
    if (style->font.family) {
        free((void*)style->font.family);
        style->font.family = NULL;
    }

    style->font.family = family ? strdup(family) : NULL;
}

void ir_set_font_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!style) return;
    style->font.color.type = IR_COLOR_SOLID;
    style->font.color.data.r = r;
    style->font.color.data.g = g;
    style->font.color.data.b = b;
    style->font.color.data.a = a;
}

void ir_set_font_style(IRStyle* style, bool bold, bool italic) {
    if (!style) return;
    style->font.bold = bold;
    style->font.italic = italic;
}

// ============================================================================
// Extended Typography (Phase 3)
// ============================================================================

void ir_set_font_weight(IRStyle* style, uint16_t weight) {
    if (!style) return;
    style->font.weight = weight;
}

void ir_set_line_height(IRStyle* style, float line_height) {
    if (!style) return;
    style->font.line_height = line_height;
}

void ir_set_letter_spacing(IRStyle* style, float spacing) {
    if (!style) return;
    style->font.letter_spacing = spacing;
}

void ir_set_word_spacing(IRStyle* style, float spacing) {
    if (!style) return;
    style->font.word_spacing = spacing;
}

void ir_set_text_align(IRStyle* style, IRTextAlign align) {
    if (!style) return;
    style->font.align = align;
}

void ir_set_text_decoration(IRStyle* style, uint8_t decoration) {
    if (!style) return;
    style->font.decoration = decoration;
}

// ============================================================================
// Style Variable Reference Setters (for theme support)
// ============================================================================

void ir_set_background_color_var(IRStyle* style, IRStyleVarId var_id) {
    if (!style) return;
    style->background.type = IR_COLOR_VAR_REF;
    style->background.data.var_id = var_id;
}

void ir_set_text_color_var(IRStyle* style, IRStyleVarId var_id) {
    if (!style) return;
    style->font.color.type = IR_COLOR_VAR_REF;
    style->font.color.data.var_id = var_id;
}

void ir_set_border_color_var(IRStyle* style, IRStyleVarId var_id) {
    if (!style) return;
    style->border.color.type = IR_COLOR_VAR_REF;
    style->border.color.data.var_id = var_id;
}
