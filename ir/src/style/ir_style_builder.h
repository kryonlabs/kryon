#ifndef IR_STYLE_BUILDER_H
#define IR_STYLE_BUILDER_H

#include <stdint.h>
#include <stdbool.h>

// Include core IR header which defines all necessary types
#include "../include/ir_core.h"

// ============================================================================
// Style Creation and Destruction
// ============================================================================
IRStyle* ir_create_style(void);
void ir_destroy_style(IRStyle* style);
void ir_set_style(IRComponent* component, IRStyle* style);
IRStyle* ir_get_style(IRComponent* component);

// ============================================================================
// Visible and Z-Index
// ============================================================================
void ir_set_visible(IRStyle* style, bool visible);
void ir_set_z_index(IRStyle* style, uint32_t z_index);

// ============================================================================
// Text Effect Helpers
// ============================================================================
void ir_set_text_overflow(IRStyle* style, IRTextOverflowType overflow);
void ir_set_text_fade(IRStyle* style, IRTextFadeType fade_type, float fade_length);
void ir_set_text_shadow(IRStyle* style, float offset_x, float offset_y, float blur_radius,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// ============================================================================
// Opacity
// ============================================================================
void ir_set_opacity(IRStyle* style, float opacity);
void ir_set_disabled(IRComponent* component, bool disabled);

// ============================================================================
// Text Layout
// ============================================================================
void ir_set_text_max_width(IRStyle* style, IRDimensionType type, float value);

// ============================================================================
// Box Shadow and Filters
// ============================================================================
void ir_set_box_shadow(IRStyle* style, float offset_x, float offset_y, float blur_radius,
                       float spread_radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool inset);
void ir_add_filter(IRStyle* style, IRFilterType type, float value);
void ir_clear_filters(IRStyle* style);

// ============================================================================
// Style Property Helpers - Component Size
// ============================================================================
void ir_set_width(IRComponent* component, IRDimensionType type, float value);
void ir_set_height(IRComponent* component, IRDimensionType type, float value);

// ============================================================================
// Background Color
// ============================================================================
void ir_set_background_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// ============================================================================
// Border Properties
// ============================================================================
void ir_set_border(IRStyle* style, float width, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t radius);
void ir_set_border_width(IRStyle* style, float width);
void ir_set_border_radius(IRStyle* style, uint8_t radius);
void ir_set_border_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// ============================================================================
// Margin and Padding
// ============================================================================
void ir_set_margin(IRComponent* component, float top, float right, float bottom, float left);
void ir_set_padding(IRComponent* component, float top, float right, float bottom, float left);

// ============================================================================
// Font Properties
// ============================================================================
void ir_set_font(IRStyle* style, float size, const char* family, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool bold, bool italic);
void ir_set_font_size(IRStyle* style, float size);
void ir_set_font_family(IRStyle* style, const char* family);
void ir_set_font_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_set_font_style(IRStyle* style, bool bold, bool italic);

// ============================================================================
// Extended Typography (Phase 3)
// ============================================================================
void ir_set_font_weight(IRStyle* style, uint16_t weight);
void ir_set_line_height(IRStyle* style, float line_height);
void ir_set_letter_spacing(IRStyle* style, float spacing);
void ir_set_word_spacing(IRStyle* style, float spacing);
void ir_set_text_align(IRStyle* style, IRTextAlign align);
void ir_set_text_decoration(IRStyle* style, uint8_t decoration);

// ============================================================================
// Style Variable Reference Setters (for theme support)
// ============================================================================
void ir_set_background_color_var(IRStyle* style, IRStyleVarId var_id);
void ir_set_text_color_var(IRStyle* style, IRStyleVarId var_id);
void ir_set_border_color_var(IRStyle* style, IRStyleVarId var_id);

#endif // IR_STYLE_BUILDER_H
