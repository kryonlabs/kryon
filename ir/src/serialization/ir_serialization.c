#define _GNU_SOURCE
#include "../include/ir_serialization.h"
#include "../include/ir_buffer.h"
#include "../include/ir_serialize_types.h"
#include "../include/ir_builder.h"
#include "ir_foreach_expand.h"  // New modular ForEach expansion
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

// Binary format magic numbers
#define IR_MAGIC_NUMBER 0x4B495242      // "KIRB" in hex (new binary format)
#define IR_MAGIC_NUMBER_LEGACY 0x4B5259 // "KRY" in hex (old format, for backwards compatibility)
#define IR_ENDIANNESS_CHECK 0x12345678

// Binary Serialization Helper Functions
static bool write_uint8(IRBuffer* buffer, uint8_t value) {
    return ir_buffer_write(buffer, &value, sizeof(uint8_t));
}

static bool write_uint16(IRBuffer* buffer, uint16_t value) {
    return ir_buffer_write(buffer, &value, sizeof(uint16_t));
}

static bool write_uint32(IRBuffer* buffer, uint32_t value) {
    return ir_buffer_write(buffer, &value, sizeof(uint32_t));
}

__attribute__((unused))
static bool write_int32(IRBuffer* buffer, int32_t value) {
    return ir_buffer_write(buffer, &value, sizeof(int32_t));
}

__attribute__((unused))
static bool write_int64(IRBuffer* buffer, int64_t value) {
    return ir_buffer_write(buffer, &value, sizeof(int64_t));
}

__attribute__((unused))
static bool write_double(IRBuffer* buffer, double value) {
    return ir_buffer_write(buffer, &value, sizeof(double));
}

static bool write_float32(IRBuffer* buffer, float value) {
    return ir_buffer_write(buffer, &value, sizeof(float));
}

__attribute__((unused))
static bool write_float64(IRBuffer* buffer, double value) {
    return ir_buffer_write(buffer, &value, sizeof(double));
}

static bool write_string(IRBuffer* buffer, const char* string) {
    if (!string) {
        write_uint32(buffer, 0);
        return true;
    }

    uint32_t length = strlen(string) + 1;  // Include null terminator
    write_uint32(buffer, length);
    return ir_buffer_write(buffer, string, length);
}

static bool read_uint8(IRBuffer* buffer, uint8_t* value) {
    return ir_buffer_read(buffer, value, sizeof(uint8_t));
}

static bool read_uint16(IRBuffer* buffer, uint16_t* value) {
    return ir_buffer_read(buffer, value, sizeof(uint16_t));
}

static bool read_uint32(IRBuffer* buffer, uint32_t* value) {
    return ir_buffer_read(buffer, value, sizeof(uint32_t));
}

__attribute__((unused))
static bool read_int32(IRBuffer* buffer, int32_t* value) {
    return ir_buffer_read(buffer, value, sizeof(int32_t));
}

__attribute__((unused))
static bool read_int64(IRBuffer* buffer, int64_t* value) {
    return ir_buffer_read(buffer, value, sizeof(int64_t));
}

__attribute__((unused))
static bool read_double(IRBuffer* buffer, double* value) {
    return ir_buffer_read(buffer, value, sizeof(double));
}

static bool read_float32(IRBuffer* buffer, float* value) {
    return ir_buffer_read(buffer, value, sizeof(float));
}

__attribute__((unused))
static bool read_float64(IRBuffer* buffer, double* value) {
    return ir_buffer_read(buffer, value, sizeof(double));
}

static bool read_string(IRBuffer* buffer, char** string) {
    uint32_t length;
    if (!read_uint32(buffer, &length)) return false;

    if (length == 0) {
        *string = NULL;
        return true;
    }

    *string = malloc(length);
    if (!*string) return false;

    if (!ir_buffer_read(buffer, *string, length)) {
        free(*string);
        *string = NULL;
        return false;
    }

    return true;
}

// Serialization Helper Functions
static bool serialize_dimension(IRBuffer* buffer, IRDimension dimension) {
    if (!write_uint8(buffer, dimension.type)) return false;
    if (!write_float32(buffer, dimension.value)) return false;
    return true;
}

static bool deserialize_dimension(IRBuffer* buffer, IRDimension* dimension) {
    if (!read_uint8(buffer, (uint8_t*)&dimension->type)) return false;
    if (!read_float32(buffer, &dimension->value)) return false;
    return true;
}

static bool serialize_gradient(IRBuffer* buffer, IRGradient* gradient) {
    if (!gradient) {
        if (!write_uint8(buffer, 0)) return false;  // Null marker
        return true;
    }

    if (!write_uint8(buffer, 1)) return false;  // Non-null marker
    if (!write_uint8(buffer, gradient->type)) return false;
    if (!write_float32(buffer, gradient->angle)) return false;
    if (!write_float32(buffer, gradient->center_x)) return false;
    if (!write_float32(buffer, gradient->center_y)) return false;
    if (!write_uint8(buffer, gradient->stop_count)) return false;

    // Write color stops
    for (int i = 0; i < gradient->stop_count; i++) {
        if (!write_float32(buffer, gradient->stops[i].position)) return false;
        if (!write_uint8(buffer, gradient->stops[i].r)) return false;
        if (!write_uint8(buffer, gradient->stops[i].g)) return false;
        if (!write_uint8(buffer, gradient->stops[i].b)) return false;
        if (!write_uint8(buffer, gradient->stops[i].a)) return false;
    }

    return true;
}

static bool deserialize_gradient(IRBuffer* buffer, IRGradient** gradient_ptr) {
    uint8_t has_gradient;
    if (!read_uint8(buffer, &has_gradient)) return false;

    if (!has_gradient) {
        *gradient_ptr = NULL;
        return true;
    }

    IRGradient* gradient = calloc(1, sizeof(IRGradient));
    if (!gradient) return false;

    if (!read_uint8(buffer, (uint8_t*)&gradient->type)) goto error;
    if (!read_float32(buffer, &gradient->angle)) goto error;
    if (!read_float32(buffer, &gradient->center_x)) goto error;
    if (!read_float32(buffer, &gradient->center_y)) goto error;
    if (!read_uint8(buffer, &gradient->stop_count)) goto error;

    // Validate stop count
    if (gradient->stop_count > 8) {
        gradient->stop_count = 8;
    }

    // Read color stops
    for (int i = 0; i < gradient->stop_count; i++) {
        if (!read_float32(buffer, &gradient->stops[i].position)) goto error;
        if (!read_uint8(buffer, &gradient->stops[i].r)) goto error;
        if (!read_uint8(buffer, &gradient->stops[i].g)) goto error;
        if (!read_uint8(buffer, &gradient->stops[i].b)) goto error;
        if (!read_uint8(buffer, &gradient->stops[i].a)) goto error;
    }

    *gradient_ptr = gradient;
    return true;

error:
    free(gradient);
    return false;
}

static bool serialize_color(IRBuffer* buffer, IRColor color) {
    if (!write_uint8(buffer, color.type)) return false;

    if (color.type == IR_COLOR_GRADIENT) {
        // Serialize gradient data
        if (!serialize_gradient(buffer, color.data.gradient)) return false;
    } else {
        // Serialize RGBA data
        if (!write_uint8(buffer, color.data.r)) return false;
        if (!write_uint8(buffer, color.data.g)) return false;
        if (!write_uint8(buffer, color.data.b)) return false;
        if (!write_uint8(buffer, color.data.a)) return false;
    }

    return true;
}

static bool deserialize_color(IRBuffer* buffer, IRColor* color) {
    if (!read_uint8(buffer, (uint8_t*)&color->type)) return false;

    color->var_name = NULL;  // Initialize

    if (color->type == IR_COLOR_GRADIENT) {
        // Deserialize gradient data
        if (!deserialize_gradient(buffer, &color->data.gradient)) return false;
    } else {
        // Deserialize RGBA data
        if (!read_uint8(buffer, &color->data.r)) return false;
        if (!read_uint8(buffer, &color->data.g)) return false;
        if (!read_uint8(buffer, &color->data.b)) return false;
        if (!read_uint8(buffer, &color->data.a)) return false;
    }

    return true;
}

// ============================================================================
// V2.0 SERIALIZATION HELPERS - Complete IR property support
// ============================================================================

// Grid Track serialization
static bool serialize_grid_track(IRBuffer* buffer, IRGridTrack track) {
    if (!write_uint8(buffer, track.type)) return false;
    if (!write_float32(buffer, track.value)) return false;
    return true;
}

static bool deserialize_grid_track(IRBuffer* buffer, IRGridTrack* track) {
    if (!read_uint8(buffer, (uint8_t*)&track->type)) return false;
    if (!read_float32(buffer, &track->value)) return false;
    return true;
}

// Flexbox serialization
static bool serialize_flexbox(IRBuffer* buffer, IRFlexbox* flex) {
    if (!write_uint8(buffer, flex->wrap)) return false;
    if (!write_uint32(buffer, flex->gap)) return false;
    // NOTE: main_axis was removed - it's the same as justify_content
    if (!write_uint8(buffer, flex->cross_axis)) return false;
    if (!write_uint8(buffer, flex->justify_content)) return false;
    if (!write_uint8(buffer, flex->grow)) return false;
    if (!write_uint8(buffer, flex->shrink)) return false;
    if (!write_uint8(buffer, flex->direction)) return false;
    if (!write_uint8(buffer, flex->base_direction)) return false;  // BiDi: CSS direction
    if (!write_uint8(buffer, flex->unicode_bidi)) return false;    // BiDi: unicode-bidi
    if (!write_uint8(buffer, flex->_padding)) return false;        // BiDi: padding
    return true;
}

static bool deserialize_flexbox(IRBuffer* buffer, IRFlexbox* flex) {
    if (!read_uint8(buffer, (uint8_t*)&flex->wrap)) return false;
    if (!read_uint32(buffer, &flex->gap)) return false;
    // NOTE: main_axis was removed - it's the same as justify_content
    if (!read_uint8(buffer, (uint8_t*)&flex->cross_axis)) return false;
    if (!read_uint8(buffer, (uint8_t*)&flex->justify_content)) return false;
    if (!read_uint8(buffer, &flex->grow)) return false;
    if (!read_uint8(buffer, &flex->shrink)) return false;
    if (!read_uint8(buffer, &flex->direction)) return false;
    if (!read_uint8(buffer, &flex->base_direction)) return false;  // BiDi: CSS direction
    if (!read_uint8(buffer, &flex->unicode_bidi)) return false;    // BiDi: unicode-bidi
    if (!read_uint8(buffer, &flex->_padding)) return false;        // BiDi: padding
    return true;
}

// Grid serialization
static bool serialize_grid(IRBuffer* buffer, IRGrid* grid) {
    // Serialize grid tracks
    if (!write_uint8(buffer, grid->row_count)) return false;
    for (uint8_t i = 0; i < grid->row_count; i++) {
        if (!serialize_grid_track(buffer, grid->rows[i])) return false;
    }

    if (!write_uint8(buffer, grid->column_count)) return false;
    for (uint8_t i = 0; i < grid->column_count; i++) {
        if (!serialize_grid_track(buffer, grid->columns[i])) return false;
    }

    // Serialize gaps and alignment
    if (!write_float32(buffer, grid->row_gap)) return false;
    if (!write_float32(buffer, grid->column_gap)) return false;
    if (!write_uint8(buffer, grid->justify_items)) return false;
    if (!write_uint8(buffer, grid->align_items)) return false;
    if (!write_uint8(buffer, grid->justify_content)) return false;
    if (!write_uint8(buffer, grid->align_content)) return false;
    if (!write_uint8(buffer, grid->auto_flow_row)) return false;
    if (!write_uint8(buffer, grid->auto_flow_dense)) return false;

    return true;
}

static bool deserialize_grid(IRBuffer* buffer, IRGrid* grid) {
    // Deserialize grid tracks
    if (!read_uint8(buffer, &grid->row_count)) return false;
    for (uint8_t i = 0; i < grid->row_count; i++) {
        if (!deserialize_grid_track(buffer, &grid->rows[i])) return false;
    }

    if (!read_uint8(buffer, &grid->column_count)) return false;
    for (uint8_t i = 0; i < grid->column_count; i++) {
        if (!deserialize_grid_track(buffer, &grid->columns[i])) return false;
    }

    // Deserialize gaps and alignment
    if (!read_float32(buffer, &grid->row_gap)) return false;
    if (!read_float32(buffer, &grid->column_gap)) return false;
    if (!read_uint8(buffer, (uint8_t*)&grid->justify_items)) return false;
    if (!read_uint8(buffer, (uint8_t*)&grid->align_items)) return false;
    if (!read_uint8(buffer, (uint8_t*)&grid->justify_content)) return false;
    if (!read_uint8(buffer, (uint8_t*)&grid->align_content)) return false;
    if (!read_uint8(buffer, (uint8_t*)&grid->auto_flow_row)) return false;
    if (!read_uint8(buffer, (uint8_t*)&grid->auto_flow_dense)) return false;

    return true;
}

// Spacing serialization
static bool serialize_spacing(IRBuffer* buffer, IRSpacing spacing) {
    if (!write_float32(buffer, spacing.top)) return false;
    if (!write_float32(buffer, spacing.right)) return false;
    if (!write_float32(buffer, spacing.bottom)) return false;
    if (!write_float32(buffer, spacing.left)) return false;
    return true;
}

static bool deserialize_spacing(IRBuffer* buffer, IRSpacing* spacing) {
    if (!read_float32(buffer, &spacing->top)) return false;
    if (!read_float32(buffer, &spacing->right)) return false;
    if (!read_float32(buffer, &spacing->bottom)) return false;
    if (!read_float32(buffer, &spacing->left)) return false;
    return true;
}

// Layout serialization
static bool serialize_layout(IRBuffer* buffer, IRLayout* layout) {
    if (!layout) {
        write_uint8(buffer, 0);  // Null layout marker
        return true;
    }

    write_uint8(buffer, 1);  // Non-null layout marker

    // Serialize layout mode and constraints
    if (!write_uint8(buffer, layout->mode)) return false;
    if (!serialize_dimension(buffer, layout->min_width)) return false;
    if (!serialize_dimension(buffer, layout->min_height)) return false;
    if (!serialize_dimension(buffer, layout->max_width)) return false;
    if (!serialize_dimension(buffer, layout->max_height)) return false;

    // Serialize flexbox and grid
    if (!serialize_flexbox(buffer, &layout->flex)) return false;
    if (!serialize_grid(buffer, &layout->grid)) return false;

    // Serialize spacing and aspect ratio
    if (!serialize_spacing(buffer, layout->margin)) return false;
    if (!serialize_spacing(buffer, layout->padding)) return false;
    if (!write_float32(buffer, layout->aspect_ratio)) return false;

    return true;
}

static bool deserialize_layout(IRBuffer* buffer, IRLayout** layout_ptr) {
    uint8_t has_layout;
    if (!read_uint8(buffer, &has_layout)) return false;

    if (has_layout == 0) {
        *layout_ptr = NULL;
        return true;
    }

    IRLayout* layout = ir_create_layout();
    if (!layout) return false;

    // Deserialize layout mode and constraints
    if (!read_uint8(buffer, (uint8_t*)&layout->mode)) goto error;
    if (!deserialize_dimension(buffer, &layout->min_width)) goto error;
    if (!deserialize_dimension(buffer, &layout->min_height)) goto error;
    if (!deserialize_dimension(buffer, &layout->max_width)) goto error;
    if (!deserialize_dimension(buffer, &layout->max_height)) goto error;

    // Deserialize flexbox and grid
    if (!deserialize_flexbox(buffer, &layout->flex)) goto error;
    if (!deserialize_grid(buffer, &layout->grid)) goto error;

    // Deserialize spacing and aspect ratio
    if (!deserialize_spacing(buffer, &layout->margin)) goto error;
    if (!deserialize_spacing(buffer, &layout->padding)) goto error;
    if (!read_float32(buffer, &layout->aspect_ratio)) goto error;

    *layout_ptr = layout;
    return true;

error:
    ir_destroy_layout(layout);
    return false;
}

// Grid Item serialization
static bool serialize_grid_item(IRBuffer* buffer, IRGridItem* item) {
    if (!write_uint16(buffer, (uint16_t)item->row_start)) return false;
    if (!write_uint16(buffer, (uint16_t)item->row_end)) return false;
    if (!write_uint16(buffer, (uint16_t)item->column_start)) return false;
    if (!write_uint16(buffer, (uint16_t)item->column_end)) return false;
    if (!write_uint8(buffer, item->justify_self)) return false;
    if (!write_uint8(buffer, item->align_self)) return false;
    return true;
}

static bool deserialize_grid_item(IRBuffer* buffer, IRGridItem* item) {
    uint16_t tmp;
    if (!read_uint16(buffer, &tmp)) return false;
    item->row_start = (int16_t)tmp;
    if (!read_uint16(buffer, &tmp)) return false;
    item->row_end = (int16_t)tmp;
    if (!read_uint16(buffer, &tmp)) return false;
    item->column_start = (int16_t)tmp;
    if (!read_uint16(buffer, &tmp)) return false;
    item->column_end = (int16_t)tmp;
    if (!read_uint8(buffer, (uint8_t*)&item->justify_self)) return false;
    if (!read_uint8(buffer, (uint8_t*)&item->align_self)) return false;
    return true;
}

// Text Shadow serialization
static bool serialize_text_shadow(IRBuffer* buffer, IRTextShadow* shadow) {
    if (!write_float32(buffer, shadow->offset_x)) return false;
    if (!write_float32(buffer, shadow->offset_y)) return false;
    if (!write_float32(buffer, shadow->blur_radius)) return false;
    if (!serialize_color(buffer, shadow->color)) return false;
    if (!write_uint8(buffer, shadow->enabled)) return false;
    return true;
}

static bool deserialize_text_shadow(IRBuffer* buffer, IRTextShadow* shadow) {
    if (!read_float32(buffer, &shadow->offset_x)) return false;
    if (!read_float32(buffer, &shadow->offset_y)) return false;
    if (!read_float32(buffer, &shadow->blur_radius)) return false;
    if (!deserialize_color(buffer, &shadow->color)) return false;
    if (!read_uint8(buffer, (uint8_t*)&shadow->enabled)) return false;
    return true;
}

// Text Effect serialization
static bool serialize_text_effect(IRBuffer* buffer, IRTextEffect* effect) {
    if (!write_uint8(buffer, effect->overflow)) return false;
    if (!write_uint8(buffer, effect->fade_type)) return false;
    if (!write_float32(buffer, effect->fade_length)) return false;
    if (!serialize_text_shadow(buffer, &effect->shadow)) return false;
    if (!serialize_dimension(buffer, effect->max_width)) return false;
    if (!write_uint8(buffer, effect->text_direction)) return false;
    if (!write_string(buffer, effect->language)) return false;
    return true;
}

static bool deserialize_text_effect(IRBuffer* buffer, IRTextEffect* effect) {
    if (!read_uint8(buffer, (uint8_t*)&effect->overflow)) return false;
    if (!read_uint8(buffer, (uint8_t*)&effect->fade_type)) return false;
    if (!read_float32(buffer, &effect->fade_length)) return false;
    if (!deserialize_text_shadow(buffer, &effect->shadow)) return false;
    if (!deserialize_dimension(buffer, &effect->max_width)) return false;
    if (!read_uint8(buffer, (uint8_t*)&effect->text_direction)) return false;
    if (!read_string(buffer, &effect->language)) return false;
    return true;
}

// Box Shadow serialization
static bool serialize_box_shadow(IRBuffer* buffer, IRBoxShadow* shadow) {
    if (!write_float32(buffer, shadow->offset_x)) return false;
    if (!write_float32(buffer, shadow->offset_y)) return false;
    if (!write_float32(buffer, shadow->blur_radius)) return false;
    if (!write_float32(buffer, shadow->spread_radius)) return false;
    if (!serialize_color(buffer, shadow->color)) return false;
    if (!write_uint8(buffer, shadow->inset)) return false;
    if (!write_uint8(buffer, shadow->enabled)) return false;
    return true;
}

static bool deserialize_box_shadow(IRBuffer* buffer, IRBoxShadow* shadow) {
    if (!read_float32(buffer, &shadow->offset_x)) return false;
    if (!read_float32(buffer, &shadow->offset_y)) return false;
    if (!read_float32(buffer, &shadow->blur_radius)) return false;
    if (!read_float32(buffer, &shadow->spread_radius)) return false;
    if (!deserialize_color(buffer, &shadow->color)) return false;
    if (!read_uint8(buffer, (uint8_t*)&shadow->inset)) return false;
    if (!read_uint8(buffer, (uint8_t*)&shadow->enabled)) return false;
    return true;
}

// Filter serialization
static bool serialize_filter(IRBuffer* buffer, IRFilter* filter) {
    if (!write_uint8(buffer, filter->type)) return false;
    if (!write_float32(buffer, filter->value)) return false;
    return true;
}

static bool deserialize_filter(IRBuffer* buffer, IRFilter* filter) {
    if (!read_uint8(buffer, (uint8_t*)&filter->type)) return false;
    if (!read_float32(buffer, &filter->value)) return false;
    return true;
}

// Animation Keyframe serialization
static bool serialize_keyframe(IRBuffer* buffer, IRKeyframe* keyframe) {
    if (!write_float32(buffer, keyframe->offset)) return false;
    if (!write_uint8(buffer, keyframe->easing)) return false;
    if (!write_uint8(buffer, keyframe->property_count)) return false;

    // Serialize each property inline
    for (uint8_t i = 0; i < keyframe->property_count; i++) {
        if (!write_uint8(buffer, keyframe->properties[i].property)) return false;
        if (!write_float32(buffer, keyframe->properties[i].value)) return false;
        if (!serialize_color(buffer, keyframe->properties[i].color_value)) return false;
        if (!write_uint8(buffer, keyframe->properties[i].is_set)) return false;
    }
    return true;
}

static bool deserialize_keyframe(IRBuffer* buffer, IRKeyframe* keyframe) {
    if (!read_float32(buffer, &keyframe->offset)) return false;
    if (!read_uint8(buffer, (uint8_t*)&keyframe->easing)) return false;
    if (!read_uint8(buffer, &keyframe->property_count)) return false;

    // Deserialize each property inline
    for (uint8_t i = 0; i < keyframe->property_count; i++) {
        if (!read_uint8(buffer, (uint8_t*)&keyframe->properties[i].property)) return false;
        if (!read_float32(buffer, &keyframe->properties[i].value)) return false;
        if (!deserialize_color(buffer, &keyframe->properties[i].color_value)) return false;
        if (!read_uint8(buffer, (uint8_t*)&keyframe->properties[i].is_set)) return false;
    }
    return true;
}

// Animation serialization
static bool serialize_animation(IRBuffer* buffer, IRAnimation* animation) {
    if (!write_string(buffer, animation->name)) return false;
    if (!write_float32(buffer, animation->duration)) return false;
    if (!write_float32(buffer, animation->delay)) return false;
    if (!write_uint32(buffer, (uint32_t)animation->iteration_count)) return false;
    if (!write_uint8(buffer, animation->alternate)) return false;
    if (!write_uint8(buffer, animation->default_easing)) return false;
    if (!write_uint8(buffer, animation->keyframe_count)) return false;

    for (uint8_t i = 0; i < animation->keyframe_count; i++) {
        if (!serialize_keyframe(buffer, &animation->keyframes[i])) return false;
    }
    return true;
}

static bool deserialize_animation(IRBuffer* buffer, IRAnimation* animation) {
    if (!read_string(buffer, &animation->name)) return false;
    if (!read_float32(buffer, &animation->duration)) return false;
    if (!read_float32(buffer, &animation->delay)) return false;

    uint32_t tmp;
    if (!read_uint32(buffer, &tmp)) return false;
    animation->iteration_count = (int32_t)tmp;

    if (!read_uint8(buffer, (uint8_t*)&animation->alternate)) return false;
    if (!read_uint8(buffer, (uint8_t*)&animation->default_easing)) return false;
    if (!read_uint8(buffer, &animation->keyframe_count)) return false;

    for (uint8_t i = 0; i < animation->keyframe_count; i++) {
        if (!deserialize_keyframe(buffer, &animation->keyframes[i])) return false;
    }

    // Initialize runtime state (not serialized)
    animation->current_time = 0.0f;
    animation->current_iteration = 0;
    animation->is_paused = false;

    return true;
}

// Transition serialization
static bool serialize_transition(IRBuffer* buffer, IRTransition* transition) {
    if (!write_uint8(buffer, transition->property)) return false;
    if (!write_float32(buffer, transition->duration)) return false;
    if (!write_float32(buffer, transition->delay)) return false;
    if (!write_uint8(buffer, transition->easing)) return false;
    if (!write_uint32(buffer, transition->trigger_state)) return false;
    return true;
}

static bool deserialize_transition(IRBuffer* buffer, IRTransition* transition) {
    if (!read_uint8(buffer, (uint8_t*)&transition->property)) return false;
    if (!read_float32(buffer, &transition->duration)) return false;
    if (!read_float32(buffer, &transition->delay)) return false;
    if (!read_uint8(buffer, (uint8_t*)&transition->easing)) return false;
    if (!read_uint32(buffer, &transition->trigger_state)) return false;
    return true;
}

// Pseudo Style serialization
static bool serialize_pseudo_style(IRBuffer* buffer, IRPseudoStyle* pseudo) {
    if (!write_uint32(buffer, pseudo->state)) return false;
    if (!serialize_color(buffer, pseudo->background)) return false;
    if (!serialize_color(buffer, pseudo->text_color)) return false;
    if (!serialize_color(buffer, pseudo->border_color)) return false;
    if (!write_float32(buffer, pseudo->opacity)) return false;

    // Serialize transform
    if (!write_float32(buffer, pseudo->transform.translate_x)) return false;
    if (!write_float32(buffer, pseudo->transform.translate_y)) return false;
    if (!write_float32(buffer, pseudo->transform.scale_x)) return false;
    if (!write_float32(buffer, pseudo->transform.scale_y)) return false;
    if (!write_float32(buffer, pseudo->transform.rotate)) return false;

    // Serialize has_* flags
    if (!write_uint8(buffer, pseudo->has_background)) return false;
    if (!write_uint8(buffer, pseudo->has_text_color)) return false;
    if (!write_uint8(buffer, pseudo->has_border_color)) return false;
    if (!write_uint8(buffer, pseudo->has_opacity)) return false;
    if (!write_uint8(buffer, pseudo->has_transform)) return false;

    return true;
}

static bool deserialize_pseudo_style(IRBuffer* buffer, IRPseudoStyle* pseudo) {
    if (!read_uint32(buffer, &pseudo->state)) return false;
    if (!deserialize_color(buffer, &pseudo->background)) return false;
    if (!deserialize_color(buffer, &pseudo->text_color)) return false;
    if (!deserialize_color(buffer, &pseudo->border_color)) return false;
    if (!read_float32(buffer, &pseudo->opacity)) return false;

    // Deserialize transform
    if (!read_float32(buffer, &pseudo->transform.translate_x)) return false;
    if (!read_float32(buffer, &pseudo->transform.translate_y)) return false;
    if (!read_float32(buffer, &pseudo->transform.scale_x)) return false;
    if (!read_float32(buffer, &pseudo->transform.scale_y)) return false;
    if (!read_float32(buffer, &pseudo->transform.rotate)) return false;

    // Deserialize has_* flags
    if (!read_uint8(buffer, (uint8_t*)&pseudo->has_background)) return false;
    if (!read_uint8(buffer, (uint8_t*)&pseudo->has_text_color)) return false;
    if (!read_uint8(buffer, (uint8_t*)&pseudo->has_border_color)) return false;
    if (!read_uint8(buffer, (uint8_t*)&pseudo->has_opacity)) return false;
    if (!read_uint8(buffer, (uint8_t*)&pseudo->has_transform)) return false;

    return true;
}

// Query Condition serialization
static bool serialize_query_condition(IRBuffer* buffer, IRQueryCondition* cond) {
    if (!write_uint8(buffer, cond->type)) return false;
    if (!write_float32(buffer, cond->value)) return false;
    return true;
}

static bool deserialize_query_condition(IRBuffer* buffer, IRQueryCondition* cond) {
    if (!read_uint8(buffer, (uint8_t*)&cond->type)) return false;
    if (!read_float32(buffer, &cond->value)) return false;
    return true;
}

// Breakpoint serialization
static bool serialize_breakpoint(IRBuffer* buffer, IRBreakpoint* breakpoint) {
    if (!write_uint8(buffer, breakpoint->condition_count)) return false;
    for (uint8_t i = 0; i < breakpoint->condition_count; i++) {
        if (!serialize_query_condition(buffer, &breakpoint->conditions[i])) return false;
    }

    if (!serialize_dimension(buffer, breakpoint->width)) return false;
    if (!serialize_dimension(buffer, breakpoint->height)) return false;
    if (!write_uint8(buffer, breakpoint->visible)) return false;
    if (!write_float32(buffer, breakpoint->opacity)) return false;
    if (!write_uint8(buffer, breakpoint->layout_mode)) return false;
    if (!write_uint8(buffer, breakpoint->has_layout_mode)) return false;

    return true;
}

static bool deserialize_breakpoint(IRBuffer* buffer, IRBreakpoint* breakpoint) {
    if (!read_uint8(buffer, &breakpoint->condition_count)) return false;
    for (uint8_t i = 0; i < breakpoint->condition_count; i++) {
        if (!deserialize_query_condition(buffer, &breakpoint->conditions[i])) return false;
    }

    if (!deserialize_dimension(buffer, &breakpoint->width)) return false;
    if (!deserialize_dimension(buffer, &breakpoint->height)) return false;
    if (!read_uint8(buffer, (uint8_t*)&breakpoint->visible)) return false;
    if (!read_float32(buffer, &breakpoint->opacity)) return false;
    if (!read_uint8(buffer, (uint8_t*)&breakpoint->layout_mode)) return false;
    if (!read_uint8(buffer, (uint8_t*)&breakpoint->has_layout_mode)) return false;

    return true;
}

static bool serialize_style(IRBuffer* buffer, IRStyle* style) {
    if (!style) {
        write_uint8(buffer, 0);  // Null style marker
        return true;
    }

    write_uint8(buffer, 1);  // Non-null style marker

    // Serialize dimensions
    if (!serialize_dimension(buffer, style->width)) return false;
    if (!serialize_dimension(buffer, style->height)) return false;

    // Serialize colors
    if (!serialize_color(buffer, style->background)) return false;
    if (!serialize_color(buffer, style->border_color)) return false;

    // Serialize border
    if (!write_float32(buffer, style->border.width)) return false;
    if (!serialize_color(buffer, style->border.color)) return false;
    if (!write_uint8(buffer, style->border.radius)) return false;

    // Serialize spacing
    if (!write_float32(buffer, style->margin.top)) return false;
    if (!write_float32(buffer, style->margin.right)) return false;
    if (!write_float32(buffer, style->margin.bottom)) return false;
    if (!write_float32(buffer, style->margin.left)) return false;

    if (!write_float32(buffer, style->padding.top)) return false;
    if (!write_float32(buffer, style->padding.right)) return false;
    if (!write_float32(buffer, style->padding.bottom)) return false;
    if (!write_float32(buffer, style->padding.left)) return false;

    // Serialize typography
    if (!write_float32(buffer, style->font.size)) return false;
    if (!serialize_color(buffer, style->font.color)) return false;
    if (!write_uint8(buffer, style->font.bold)) return false;
    if (!write_uint8(buffer, style->font.italic)) return false;
    if (!write_string(buffer, style->font.family)) return false;

    // V2.0: Extended typography
    if (!write_uint16(buffer, style->font.weight)) return false;
    if (!write_float32(buffer, style->font.line_height)) return false;
    if (!write_float32(buffer, style->font.letter_spacing)) return false;
    if (!write_float32(buffer, style->font.word_spacing)) return false;
    if (!write_uint8(buffer, style->font.align)) return false;
    if (!write_uint8(buffer, style->font.decoration)) return false;

    // Serialize transform
    if (!write_float32(buffer, style->transform.translate_x)) return false;
    if (!write_float32(buffer, style->transform.translate_y)) return false;
    if (!write_float32(buffer, style->transform.scale_x)) return false;
    if (!write_float32(buffer, style->transform.scale_y)) return false;
    if (!write_float32(buffer, style->transform.rotate)) return false;

    // V2.0: Animations (pointer array)
    if (!write_uint32(buffer, style->animation_count)) return false;
    for (uint32_t i = 0; i < style->animation_count; i++) {
        if (style->animations && style->animations[i]) {
            if (!serialize_animation(buffer, style->animations[i])) return false;
        }
    }

    // V2.0: Transitions (pointer array)
    if (!write_uint32(buffer, style->transition_count)) return false;
    for (uint32_t i = 0; i < style->transition_count; i++) {
        if (style->transitions && style->transitions[i]) {
            if (!serialize_transition(buffer, style->transitions[i])) return false;
        }
    }

    // Serialize z-index, visibility, opacity
    if (!write_uint32(buffer, style->z_index)) return false;
    if (!write_uint8(buffer, style->visible)) return false;
    if (!write_float32(buffer, style->opacity)) return false;

    // V2.0: Position mode and coordinates
    if (!write_uint8(buffer, style->position_mode)) return false;
    if (!write_float32(buffer, style->absolute_x)) return false;
    if (!write_float32(buffer, style->absolute_y)) return false;

    // V2.0: Overflow handling
    if (!write_uint8(buffer, style->overflow_x)) return false;
    if (!write_uint8(buffer, style->overflow_y)) return false;

    // V2.0: Grid item placement
    if (!serialize_grid_item(buffer, &style->grid_item)) return false;

    // V2.0: Text effect
    if (!serialize_text_effect(buffer, &style->text_effect)) return false;

    // V2.0: Box shadow
    if (!serialize_box_shadow(buffer, &style->box_shadow)) return false;

    // V2.0: Filters array
    if (!write_uint8(buffer, style->filter_count)) return false;
    for (uint8_t i = 0; i < style->filter_count; i++) {
        if (!serialize_filter(buffer, &style->filters[i])) return false;
    }

    // V2.0: Breakpoints array
    if (!write_uint8(buffer, style->breakpoint_count)) return false;
    for (uint8_t i = 0; i < style->breakpoint_count; i++) {
        if (!serialize_breakpoint(buffer, &style->breakpoints[i])) return false;
    }

    // V2.0: Container query context
    if (!write_uint8(buffer, style->container_type)) return false;
    if (!write_string(buffer, style->container_name)) return false;

    // V2.0: Pseudo-class styles array
    if (!write_uint8(buffer, style->pseudo_style_count)) return false;
    for (uint8_t i = 0; i < style->pseudo_style_count; i++) {
        if (!serialize_pseudo_style(buffer, &style->pseudo_styles[i])) return false;
    }

    // V2.0: Current pseudo-states (runtime, but serialize for state preservation)
    if (!write_uint32(buffer, style->current_pseudo_states)) return false;

    return true;
}

static bool deserialize_style(IRBuffer* buffer, IRStyle** style_ptr) {
    uint8_t has_style;
    if (!read_uint8(buffer, &has_style)) return false;

    if (has_style == 0) {
        *style_ptr = NULL;
        return true;
    }

    IRStyle* style = ir_create_style();
    if (!style) return false;

    // Deserialize dimensions
    if (!deserialize_dimension(buffer, &style->width)) goto error;
    if (!deserialize_dimension(buffer, &style->height)) goto error;

    // Deserialize colors
    if (!deserialize_color(buffer, &style->background)) goto error;
    if (!deserialize_color(buffer, &style->border_color)) goto error;

    // Deserialize border
    if (!read_float32(buffer, &style->border.width)) goto error;
    if (!deserialize_color(buffer, &style->border.color)) goto error;
    if (!read_uint8(buffer, &style->border.radius)) goto error;

    // Deserialize spacing
    if (!read_float32(buffer, &style->margin.top)) goto error;
    if (!read_float32(buffer, &style->margin.right)) goto error;
    if (!read_float32(buffer, &style->margin.bottom)) goto error;
    if (!read_float32(buffer, &style->margin.left)) goto error;

    if (!read_float32(buffer, &style->padding.top)) goto error;
    if (!read_float32(buffer, &style->padding.right)) goto error;
    if (!read_float32(buffer, &style->padding.bottom)) goto error;
    if (!read_float32(buffer, &style->padding.left)) goto error;

    // Deserialize typography
    if (!read_float32(buffer, &style->font.size)) goto error;
    if (!deserialize_color(buffer, &style->font.color)) goto error;
    if (!read_uint8(buffer, (uint8_t*)&style->font.bold)) goto error;
    if (!read_uint8(buffer, (uint8_t*)&style->font.italic)) goto error;
    if (!read_string(buffer, &style->font.family)) goto error;

    // V2.0: Extended typography
    if (!read_uint16(buffer, &style->font.weight)) goto error;
    if (!read_float32(buffer, &style->font.line_height)) goto error;
    if (!read_float32(buffer, &style->font.letter_spacing)) goto error;
    if (!read_float32(buffer, &style->font.word_spacing)) goto error;
    if (!read_uint8(buffer, (uint8_t*)&style->font.align)) goto error;
    if (!read_uint8(buffer, &style->font.decoration)) goto error;

    // Deserialize transform
    if (!read_float32(buffer, &style->transform.translate_x)) goto error;
    if (!read_float32(buffer, &style->transform.translate_y)) goto error;
    if (!read_float32(buffer, &style->transform.scale_x)) goto error;
    if (!read_float32(buffer, &style->transform.scale_y)) goto error;
    if (!read_float32(buffer, &style->transform.rotate)) goto error;

    // V2.0: Animations (pointer array)
    if (!read_uint32(buffer, &style->animation_count)) goto error;
    if (style->animation_count > 0) {
        style->animations = malloc(sizeof(IRAnimation*) * style->animation_count);
        if (!style->animations) goto error;

        for (uint32_t i = 0; i < style->animation_count; i++) {
            style->animations[i] = malloc(sizeof(IRAnimation));
            if (!style->animations[i]) goto error;
            if (!deserialize_animation(buffer, style->animations[i])) goto error;
        }
    }

    // V2.0: Transitions (pointer array)
    if (!read_uint32(buffer, &style->transition_count)) goto error;
    if (style->transition_count > 0) {
        style->transitions = malloc(sizeof(IRTransition*) * style->transition_count);
        if (!style->transitions) goto error;

        for (uint32_t i = 0; i < style->transition_count; i++) {
            style->transitions[i] = malloc(sizeof(IRTransition));
            if (!style->transitions[i]) goto error;
            if (!deserialize_transition(buffer, style->transitions[i])) goto error;
        }
    }

    // Deserialize z-index, visibility, opacity
    if (!read_uint32(buffer, &style->z_index)) goto error;
    if (!read_uint8(buffer, (uint8_t*)&style->visible)) goto error;
    if (!read_float32(buffer, &style->opacity)) goto error;

    // V2.0: Position mode and coordinates
    if (!read_uint8(buffer, (uint8_t*)&style->position_mode)) goto error;
    if (!read_float32(buffer, &style->absolute_x)) goto error;
    if (!read_float32(buffer, &style->absolute_y)) goto error;

    // V2.0: Overflow handling
    if (!read_uint8(buffer, (uint8_t*)&style->overflow_x)) goto error;
    if (!read_uint8(buffer, (uint8_t*)&style->overflow_y)) goto error;

    // V2.0: Grid item placement
    if (!deserialize_grid_item(buffer, &style->grid_item)) goto error;

    // V2.0: Text effect
    if (!deserialize_text_effect(buffer, &style->text_effect)) goto error;

    // V2.0: Box shadow
    if (!deserialize_box_shadow(buffer, &style->box_shadow)) goto error;

    // V2.0: Filters array
    if (!read_uint8(buffer, &style->filter_count)) goto error;
    for (uint8_t i = 0; i < style->filter_count; i++) {
        if (!deserialize_filter(buffer, &style->filters[i])) goto error;
    }

    // V2.0: Breakpoints array
    if (!read_uint8(buffer, &style->breakpoint_count)) goto error;
    for (uint8_t i = 0; i < style->breakpoint_count; i++) {
        if (!deserialize_breakpoint(buffer, &style->breakpoints[i])) goto error;
    }

    // V2.0: Container query context
    if (!read_uint8(buffer, (uint8_t*)&style->container_type)) goto error;
    if (!read_string(buffer, &style->container_name)) goto error;

    // V2.0: Pseudo-class styles array
    if (!read_uint8(buffer, &style->pseudo_style_count)) goto error;
    for (uint8_t i = 0; i < style->pseudo_style_count; i++) {
        if (!deserialize_pseudo_style(buffer, &style->pseudo_styles[i])) goto error;
    }

    // V2.0: Current pseudo-states
    if (!read_uint32(buffer, &style->current_pseudo_states)) goto error;

    *style_ptr = style;
    return true;

error:
    ir_destroy_style(style);
    return false;
}

static bool serialize_event(IRBuffer* buffer, IREvent* event) {
    if (!event) {
        write_uint8(buffer, 0);  // Null event marker
        return true;
    }

    write_uint8(buffer, 1);  // Non-null event marker

    if (!write_uint8(buffer, event->type)) return false;
    if (!write_string(buffer, event->logic_id)) return false;
    if (!write_string(buffer, event->handler_data)) return false;
    if (!write_uint32(buffer, event->bytecode_function_id)) return false;  // IR v2.1: bytecode support

    // IR v2.2: handler_source for Lua source preservation
    if (event->handler_source) {
        if (!write_uint8(buffer, 1)) return false;  // Has handler_source
        if (!write_string(buffer, event->handler_source->language)) return false;
        if (!write_string(buffer, event->handler_source->code)) return false;
        if (!write_string(buffer, event->handler_source->file)) return false;
        if (!write_uint32(buffer, (uint32_t)event->handler_source->line)) return false;
    } else {
        if (!write_uint8(buffer, 0)) return false;  // No handler_source
    }

    return true;
}

static bool deserialize_event(IRBuffer* buffer, IREvent** event_ptr) {
    uint8_t has_event;
    if (!read_uint8(buffer, &has_event)) return false;

    if (has_event == 0) {
        *event_ptr = NULL;
        return true;
    }

    IREvent* event = malloc(sizeof(IREvent));
    if (!event) return false;

    memset(event, 0, sizeof(IREvent));

    if (!read_uint8(buffer, (uint8_t*)&event->type)) goto error;
    if (!read_string(buffer, &event->logic_id)) goto error;
    if (!read_string(buffer, &event->handler_data)) goto error;
    if (!read_uint32(buffer, &event->bytecode_function_id)) goto error;  // IR v2.1: bytecode support

    // IR v2.2: handler_source for Lua source preservation
    uint8_t has_handler_source;
    if (!read_uint8(buffer, &has_handler_source)) goto error;
    if (has_handler_source) {
        event->handler_source = calloc(1, sizeof(IRHandlerSource));
        if (!event->handler_source) goto error;
        if (!read_string(buffer, &event->handler_source->language)) goto error;
        if (!read_string(buffer, &event->handler_source->code)) goto error;
        if (!read_string(buffer, &event->handler_source->file)) goto error;
        uint32_t line;
        if (!read_uint32(buffer, &line)) goto error;
        event->handler_source->line = (int)line;
    }

    *event_ptr = event;
    return true;

error:
    ir_destroy_event(event);
    return false;
}

static bool serialize_logic(IRBuffer* buffer, IRLogic* logic) {
    if (!logic) {
        write_uint8(buffer, 0);  // Null logic marker
        return true;
    }

    write_uint8(buffer, 1);  // Non-null logic marker

    if (!write_string(buffer, logic->id)) return false;
    if (!write_uint8(buffer, logic->type)) return false;
    if (!write_string(buffer, logic->source_code)) return false;

    return true;
}

static bool deserialize_logic(IRBuffer* buffer, IRLogic** logic_ptr) {
    uint8_t has_logic;
    if (!read_uint8(buffer, &has_logic)) return false;

    if (has_logic == 0) {
        *logic_ptr = NULL;
        return true;
    }

    IRLogic* logic = malloc(sizeof(IRLogic));
    if (!logic) return false;

    memset(logic, 0, sizeof(IRLogic));

    if (!read_string(buffer, &logic->id)) goto error;
    if (!read_uint8(buffer, (uint8_t*)&logic->type)) goto error;
    if (!read_string(buffer, &logic->source_code)) goto error;

    *logic_ptr = logic;
    return true;

error:
    ir_destroy_logic(logic);
    return false;
}

static bool serialize_component(IRBuffer* buffer, IRComponent* component) {
    if (!component) {
        write_uint8(buffer, 0);  // Null component marker
        return true;
    }

    write_uint8(buffer, 1);  // Non-null component marker

    // Serialize basic properties
    if (!write_uint32(buffer, component->id)) return false;
    if (!write_uint8(buffer, component->type)) return false;
    if (!write_string(buffer, component->tag)) return false;
    if (!write_string(buffer, component->text_content)) return false;
    if (!write_string(buffer, component->custom_data)) return false;

    // Serialize style
    if (!serialize_style(buffer, component->style)) return false;

    // V2.0: Serialize layout
    if (!serialize_layout(buffer, component->layout)) return false;

    // Serialize events
    uint32_t event_count = 0;
    IREvent* event = component->events;
    while (event) {
        event_count++;
        event = event->next;
    }
    if (!write_uint32(buffer, event_count)) return false;

    event = component->events;
    while (event) {
        if (!serialize_event(buffer, event)) return false;
        event = event->next;
    }

    // Serialize logic
    uint32_t logic_count = 0;
    IRLogic* logic = component->logic;
    while (logic) {
        logic_count++;
        logic = logic->next;
    }
    if (!write_uint32(buffer, logic_count)) return false;

    logic = component->logic;
    while (logic) {
        if (!serialize_logic(buffer, logic)) return false;
        logic = logic->next;
    }

    // Serialize children
    if (!write_uint32(buffer, component->child_count)) return false;
    for (uint32_t i = 0; i < component->child_count; i++) {
        if (!serialize_component(buffer, component->children[i])) return false;
    }

    return true;
}

static bool deserialize_component(IRBuffer* buffer, IRComponent** component_ptr) {
    uint8_t has_component;
    if (!read_uint8(buffer, &has_component)) return false;

    if (has_component == 0) {
        *component_ptr = NULL;
        return true;
    }

    // Read type and ID first
    uint32_t id;
    uint8_t type;
    if (!read_uint32(buffer, &id)) return false;
    if (!read_uint8(buffer, &type)) return false;

    // Create component using proper allocator (uses pool if context exists)
    IRComponent* component = ir_create_component_with_id((IRComponentType)type, id);
    if (!component) return false;

    // Deserialize remaining properties
    if (!read_string(buffer, &component->tag)) goto error;
    if (!read_string(buffer, &component->text_content)) goto error;
    if (!read_string(buffer, &component->custom_data)) goto error;

    // Deserialize style
    if (!deserialize_style(buffer, &component->style)) goto error;

    // V2.0: Deserialize layout
    if (!deserialize_layout(buffer, &component->layout)) goto error;

    // Deserialize events
    uint32_t event_count;
    if (!read_uint32(buffer, &event_count)) goto error;

    IREvent* last_event = NULL;
    for (uint32_t i = 0; i < event_count; i++) {
        IREvent* event;
        if (!deserialize_event(buffer, &event)) goto error;

        if (i == 0) {
            component->events = event;
        } else {
            last_event->next = event;
        }
        last_event = event;
    }

    // Deserialize logic
    uint32_t logic_count;
    if (!read_uint32(buffer, &logic_count)) goto error;

    IRLogic* last_logic = NULL;
    for (uint32_t i = 0; i < logic_count; i++) {
        IRLogic* logic;
        if (!deserialize_logic(buffer, &logic)) goto error;

        if (i == 0) {
            component->logic = logic;
        } else {
            last_logic->next = logic;
        }
        last_logic = logic;
    }

    // Deserialize children
    if (!read_uint32(buffer, &component->child_count)) goto error;

    if (component->child_count > 0) {
        component->children = malloc(sizeof(IRComponent*) * component->child_count);
        if (!component->children) goto error;

        for (uint32_t i = 0; i < component->child_count; i++) {
            if (!deserialize_component(buffer, &component->children[i])) goto error;
            component->children[i]->parent = component;
        }
    }

    *component_ptr = component;
    return true;

error:
    ir_destroy_component(component);
    return false;
}

// ============================================================================
// Validation Functions
// ============================================================================

bool ir_validate_binary_format(IRBuffer* buffer) {
    if (!buffer || buffer->size < 16) return false;

    // Check magic number and version by attempting to read header
    size_t original_size = buffer->size;
    uint8_t* original_data = buffer->data;

    uint32_t magic;
    uint16_t version_major, version_minor;
    uint32_t endianness_check;

    if (!read_uint32(buffer, &magic)) return false;
    if (!read_uint16(buffer, &version_major)) return false;
    if (!read_uint16(buffer, &version_minor)) return false;
    if (!read_uint32(buffer, &endianness_check)) return false;

    // Restore buffer position
    buffer->data = original_data;
    buffer->size = original_size;

    return ((magic == IR_MAGIC_NUMBER || magic == IR_MAGIC_NUMBER_LEGACY) &&
            version_major == IR_FORMAT_VERSION_MAJOR &&
            endianness_check == IR_ENDIANNESS_CHECK);
}

bool ir_validate_json_format(const char* json_string) {
    // Simplified JSON validation - just check if it starts with {
    return json_string && json_string[0] == '{';
}
// ============================================================================
// High-Level Serialization API
// ============================================================================

IRBuffer* ir_serialize_binary(IRComponent* root) {
    IRBuffer* buffer = ir_buffer_create(4096);  // Start with 4KB
    if (!buffer) return NULL;

    // Write header
    if (!write_uint32(buffer, IR_MAGIC_NUMBER)) goto error;
    if (!write_uint16(buffer, IR_FORMAT_VERSION_MAJOR)) goto error;
    if (!write_uint16(buffer, IR_FORMAT_VERSION_MINOR)) goto error;
    if (!write_uint32(buffer, IR_ENDIANNESS_CHECK)) goto error;

    // Serialize root component
    if (!serialize_component(buffer, root)) goto error;

    return buffer;

error:
    ir_buffer_destroy(buffer);
    return NULL;
}

IRComponent* ir_deserialize_binary(IRBuffer* buffer) {
    if (!buffer) return NULL;

    // Read and validate header
    uint32_t magic;
    uint16_t version_major, version_minor;
    uint32_t endianness_check;

    if (!read_uint32(buffer, &magic)) return NULL;
    if (!read_uint16(buffer, &version_major)) return NULL;
    if (!read_uint16(buffer, &version_minor)) return NULL;
    if (!read_uint32(buffer, &endianness_check)) return NULL;

    if (magic != IR_MAGIC_NUMBER) {
        printf("Error: Invalid magic number in IR file\n");
        return NULL;
    }

    if (version_major != IR_FORMAT_VERSION_MAJOR) {
        printf("Error: Unsupported IR format version %d.%d\n", version_major, version_minor);
        return NULL;
    }

    if (endianness_check != IR_ENDIANNESS_CHECK) {
        printf("Error: Endianness mismatch in IR file\n");
        return NULL;
    }

    // Deserialize root component
    IRComponent* root;
    if (!deserialize_component(buffer, &root)) return NULL;

    return root;
}

bool ir_write_binary_file(IRComponent* root, const char* filename) {
    IRBuffer* buffer = ir_serialize_binary(root);
    if (!buffer) return false;

    FILE* file = fopen(filename, "wb");
    if (!file) {
        ir_buffer_destroy(buffer);
        return false;
    }

    bool success = (fwrite(buffer->data, 1, buffer->size, file) == buffer->size);
    fclose(file);
    ir_buffer_destroy(buffer);

    return success;
}

IRComponent* ir_read_binary_file(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;

    // Get file size
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Create buffer
    IRBuffer* buffer = ir_buffer_create(file_size);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    // Read file into buffer
    if (fread(buffer->data, 1, file_size, file) != file_size) {
        fclose(file);
        ir_buffer_destroy(buffer);
        return NULL;
    }
    buffer->size = file_size;
    fclose(file);

    // Deserialize
    IRComponent* root = ir_deserialize_binary(buffer);
    ir_buffer_destroy(buffer);

    return root;
}

void ir_print_component_info(IRComponent* component, int depth) {
    if (!component) return;

    // Print indentation
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }

    printf("%s (ID: %u)", ir_component_type_to_string(component->type), component->id);
    if (component->text_content) {
        printf(" Text: \"%s\"", component->text_content);
    }
    if (component->tag) {
        printf(" Tag: \"%s\"", component->tag);
    }
    printf("\n");

    // Print children recursively
    for (uint32_t i = 0; i < component->child_count; i++) {
        ir_print_component_info(component->children[i], depth + 1);
    }
}

bool ir_write_json_file(IRComponent* root, IRReactiveManifest* manifest, const char* filename) {
    char* json = ir_serialize_json(root, manifest);
    if (!json) return false;

    FILE* file = fopen(filename, "w");
    if (!file) {
        free(json);
        return false;
    }

    bool success = (fprintf(file, "%s", json) >= 0);
    fclose(file);
    free(json);

    return success;
}

IRComponent* ir_read_json_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* json = malloc(file_size + 1);
    if (!json) {
        fclose(file);
        return NULL;
    }

    if (fread(json, 1, file_size, file) != file_size) {
        free(json);
        fclose(file);
        return NULL;
    }
    json[file_size] = '\0';
    fclose(file);

    // Ensure global context exists before parsing metadata
    extern IRContext* g_ir_context;
    if (!g_ir_context) {
        extern IRContext* ir_create_context(void);
        extern void ir_set_context(IRContext* ctx);
        IRContext* ctx = ir_create_context();
        ir_set_context(ctx);
    }

    // Parse JSON to extract source metadata
    extern cJSON* cJSON_Parse(const char* value);
    extern void cJSON_Delete(cJSON* item);
    extern cJSON* cJSON_GetObjectItem(const cJSON* object, const char* string);
    extern int cJSON_IsObject(const cJSON* item);
    extern int cJSON_IsString(const cJSON* item);

    cJSON* root_json = cJSON_Parse(json);
    if (root_json) {
        cJSON* metadataObj = cJSON_GetObjectItem(root_json, "metadata");
        if (metadataObj && cJSON_IsObject(metadataObj)) {
            // Parse source metadata (source_language, compiler_version, etc.)
            IRSourceMetadata* metadata = calloc(1, sizeof(IRSourceMetadata));
            if (metadata) {
                cJSON* lang = cJSON_GetObjectItem(metadataObj, "source_language");
                if (lang && cJSON_IsString(lang)) {
                    metadata->source_language = strdup(lang->valuestring);
                }

                cJSON* version = cJSON_GetObjectItem(metadataObj, "compiler_version");
                if (version && cJSON_IsString(version)) {
                    metadata->compiler_version = strdup(version->valuestring);
                }

                cJSON* ts = cJSON_GetObjectItem(metadataObj, "timestamp");
                if (ts && cJSON_IsString(ts)) {
                    metadata->timestamp = strdup(ts->valuestring);
                }

                cJSON* sf = cJSON_GetObjectItem(metadataObj, "source_file");
                if (sf && cJSON_IsString(sf)) {
                    metadata->source_file = strdup(sf->valuestring);
                }

                // Store in global context
                if (g_ir_context) {
                    // Free old source metadata if exists
                    if (g_ir_context->source_metadata) {
                        if (g_ir_context->source_metadata->source_language)
                            free(g_ir_context->source_metadata->source_language);
                        if (g_ir_context->source_metadata->compiler_version)
                            free(g_ir_context->source_metadata->compiler_version);
                        if (g_ir_context->source_metadata->timestamp)
                            free(g_ir_context->source_metadata->timestamp);
                        if (g_ir_context->source_metadata->source_file)
                            free(g_ir_context->source_metadata->source_file);
                        free(g_ir_context->source_metadata);
                    }
                    g_ir_context->source_metadata = metadata;
                }
            }
        }
        cJSON_Delete(root_json);
    }

    IRComponent* root = ir_deserialize_json(json);
    free(json);

    return root;
}

IRComponent* ir_read_json_file_with_manifest(const char* filename, IRReactiveManifest** out_manifest) {
    if (out_manifest) *out_manifest = NULL;

    FILE* file = fopen(filename, "r");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* json = malloc(file_size + 1);
    if (!json) {
        fclose(file);
        return NULL;
    }

    if (fread(json, 1, file_size, file) != file_size) {
        free(json);
        fclose(file);
        return NULL;
    }
    json[file_size] = '\0';
    fclose(file);

    // Parse JSON to extract reactive_manifest
    extern cJSON* cJSON_Parse(const char* value);
    extern void cJSON_Delete(cJSON* item);
    extern cJSON* cJSON_GetObjectItem(const cJSON* object, const char* string);
    extern int cJSON_IsObject(const cJSON* item);
    extern int cJSON_IsArray(const cJSON* item);
    extern int cJSON_IsString(const cJSON* item);
    extern int cJSON_GetArraySize(const cJSON* array);
    extern cJSON* cJSON_GetArrayItem(const cJSON* array, int index);

    // Ensure global context exists before parsing metadata
    extern IRContext* g_ir_context;
    if (!g_ir_context) {
        extern IRContext* ir_create_context(void);
        extern void ir_set_context(IRContext* ctx);
        IRContext* ctx = ir_create_context();
        ir_set_context(ctx);
    }

    cJSON* root_json = cJSON_Parse(json);
    if (root_json) {
        // Parse source metadata
        cJSON* metadataObj = cJSON_GetObjectItem(root_json, "metadata");
        if (metadataObj && cJSON_IsObject(metadataObj) && g_ir_context) {
            IRSourceMetadata* metadata = calloc(1, sizeof(IRSourceMetadata));
            if (metadata) {
                cJSON* lang = cJSON_GetObjectItem(metadataObj, "source_language");
                if (lang && cJSON_IsString(lang)) {
                    metadata->source_language = strdup(lang->valuestring);
                }

                cJSON* version = cJSON_GetObjectItem(metadataObj, "compiler_version");
                if (version && cJSON_IsString(version)) {
                    metadata->compiler_version = strdup(version->valuestring);
                }

                cJSON* ts = cJSON_GetObjectItem(metadataObj, "timestamp");
                if (ts && cJSON_IsString(ts)) {
                    metadata->timestamp = strdup(ts->valuestring);
                }

                cJSON* sf = cJSON_GetObjectItem(metadataObj, "source_file");
                if (sf && cJSON_IsString(sf)) {
                    metadata->source_file = strdup(sf->valuestring);
                }

                // Free old source metadata if exists
                if (g_ir_context->source_metadata) {
                    if (g_ir_context->source_metadata->source_language)
                        free(g_ir_context->source_metadata->source_language);
                    if (g_ir_context->source_metadata->compiler_version)
                        free(g_ir_context->source_metadata->compiler_version);
                    if (g_ir_context->source_metadata->timestamp)
                        free(g_ir_context->source_metadata->timestamp);
                    if (g_ir_context->source_metadata->source_file)
                        free(g_ir_context->source_metadata->source_file);
                    free(g_ir_context->source_metadata);
                }
                g_ir_context->source_metadata = metadata;
            }
        }

        // Parse reactive manifest
        if (out_manifest) {
            cJSON* reactiveObj = cJSON_GetObjectItem(root_json, "reactive_manifest");
            if (reactiveObj && cJSON_IsObject(reactiveObj)) {
                // Create manifest and extract CSS variables from "variables" array
                IRReactiveManifest* manifest = ir_reactive_manifest_create();
                if (manifest) {
                    cJSON* variablesArray = cJSON_GetObjectItem(reactiveObj, "variables");
                    if (variablesArray && cJSON_IsArray(variablesArray)) {
                        int var_count = cJSON_GetArraySize(variablesArray);
                        for (int i = 0; i < var_count; i++) {
                            cJSON* varObj = cJSON_GetArrayItem(variablesArray, i);
                            if (!varObj || !cJSON_IsObject(varObj)) continue;

                            cJSON* nameItem = cJSON_GetObjectItem(varObj, "name");
                            cJSON* typeItem = cJSON_GetObjectItem(varObj, "type");
                            cJSON* initialValueItem = cJSON_GetObjectItem(varObj, "initial_value");

                            if (nameItem && cJSON_IsString(nameItem)) {
                                const char* name = nameItem->valuestring;
                                const char* type_str = typeItem && cJSON_IsString(typeItem) ?
                                                       typeItem->valuestring : "string";
                                const char* initial_value = initialValueItem && cJSON_IsString(initialValueItem) ?
                                                            initialValueItem->valuestring : "";

                                // Add variable to manifest
                                IRReactiveValue value = {0};
                                value.as_string = strdup(initial_value);

                                uint32_t var_id = ir_reactive_manifest_add_var(manifest, name,
                                                                               IR_REACTIVE_TYPE_STRING, value);
                                ir_reactive_manifest_set_var_metadata(manifest, var_id,
                                                                       type_str, initial_value, "global");
                            }
                        }
                    }

                    // Parse bindings array
                    cJSON* bindingsArray = cJSON_GetObjectItem(reactiveObj, "bindings");
                    if (bindingsArray && cJSON_IsArray(bindingsArray)) {
                        int binding_count = cJSON_GetArraySize(bindingsArray);
                        for (int i = 0; i < binding_count; i++) {
                            cJSON* bindingObj = cJSON_GetArrayItem(bindingsArray, i);
                            if (!bindingObj || !cJSON_IsObject(bindingObj)) continue;

                            cJSON* componentIdItem = cJSON_GetObjectItem(bindingObj, "component_id");
                            cJSON* varIdItem = cJSON_GetObjectItem(bindingObj, "var_id");
                            cJSON* expressionItem = cJSON_GetObjectItem(bindingObj, "expression");

                            if (componentIdItem && varIdItem) {
                                uint32_t component_id = (uint32_t)componentIdItem->valueint;
                                uint32_t var_id = (uint32_t)varIdItem->valueint;
                                const char* expression = expressionItem && cJSON_IsString(expressionItem) ?
                                                        expressionItem->valuestring : "";

                                // Determine binding type from expression
                                // Format: "path" for text, "path:propName" for property
                                IRBindingType binding_type = IR_BINDING_TEXT;
                                if (strstr(expression, ":disabled") != NULL ||
                                    strstr(expression, ":checked") != NULL ||
                                    strstr(expression, ":value") != NULL) {
                                    binding_type = IR_BINDING_ATTRIBUTE;
                                } else if (strstr(expression, ":visible") != NULL) {
                                    binding_type = IR_BINDING_CONDITIONAL;
                                }

                                ir_reactive_manifest_add_binding(manifest, component_id, var_id,
                                                                binding_type, expression);
                            }
                        }
                    }

                    *out_manifest = manifest;
                }
            }
        }
        cJSON_Delete(root_json);
    }

    IRComponent* root = ir_deserialize_json(json);
    free(json);

    return root;
}

// NOTE: ir_serialize_json and ir_deserialize_json are now implemented in ir_json.c

/* ============================================================================
 * ForEach Runtime Expansion
 * ============================================================================*/

/**
 * Deep copy a component tree without using JSON serialization.
 * This preserves ForEach custom_data without double-encoding.
 *
 * This function creates a complete deep copy of a component including:
 * - All primitive fields (id, type, flags, etc.)
 * - String fields (text_content, custom_data, etc.) via strdup
 * - Style, layout, events, logic structures
 * - All children (recursively)
 *
 * IMPORTANT: This does NOT copy parent pointers - those must be set after copying.
 */
static IRComponent* ir_deep_copy_component(IRComponent* src) {
    if (!src) return NULL;

    // Allocate new component using the same allocator as the original
    IRComponent* dest = ir_create_component(src->type);
    if (!dest) {
        return NULL;
    }

    // Copy primitive fields
    dest->selector_type = src->selector_type;
    dest->child_count = src->child_count;
    dest->child_capacity = src->child_count;  // Only allocate what we need
    dest->owner_instance_id = src->owner_instance_id;
    dest->is_disabled = src->is_disabled;
    // Note: dirty_flags now consolidated in layout_state, not copied here
    dest->has_active_animations = src->has_active_animations;

    // Copy string fields
    if (src->tag) dest->tag = strdup(src->tag);
    if (src->css_class) dest->css_class = strdup(src->css_class);
    if (src->text_content) dest->text_content = strdup(src->text_content);
    if (src->text_expression) dest->text_expression = strdup(src->text_expression);
    if (src->custom_data) dest->custom_data = strdup(src->custom_data);  // KEY: Direct copy, no JSON encoding!
    if (src->component_ref) dest->component_ref = strdup(src->component_ref);
    if (src->component_props) dest->component_props = strdup(src->component_props);
    if (src->module_ref) dest->module_ref = strdup(src->module_ref);
    if (src->export_name) dest->export_name = strdup(src->export_name);
    if (src->scope) dest->scope = strdup(src->scope);
    if (src->visible_condition) dest->visible_condition = strdup(src->visible_condition);
    if (src->each_source) dest->each_source = strdup(src->each_source);
    if (src->each_item_name) dest->each_item_name = strdup(src->each_item_name);
    if (src->each_index_name) dest->each_index_name = strdup(src->each_index_name);

    // Copy source_metadata
    if (src->source_metadata.generated_by) {
        dest->source_metadata.generated_by = strdup(src->source_metadata.generated_by);
    }
    dest->source_metadata.iteration_index = src->source_metadata.iteration_index;
    dest->source_metadata.is_template = src->source_metadata.is_template;
    dest->visible_when_true = src->visible_when_true;

    // Copy style if present (deep copy of style structure)
    if (src->style) {
        dest->style = ir_create_style();
        if (dest->style && src->style) {
            memcpy(dest->style, src->style, sizeof(IRStyle));
            // Copy style strings that need deep copying
            if (src->style->background_image) dest->style->background_image = strdup(src->style->background_image);
            if (src->style->background.var_name) dest->style->background.var_name = strdup(src->style->background.var_name);
            if (src->style->text_fill_color.var_name) dest->style->text_fill_color.var_name = strdup(src->style->text_fill_color.var_name);
            if (src->style->border_color.var_name) dest->style->border_color.var_name = strdup(src->style->border_color.var_name);
            if (src->style->font.family) dest->style->font.family = strdup(src->style->font.family);
            if (src->style->container_name) dest->style->container_name = strdup(src->style->container_name);

            // Copy gradient if present
            if (src->style->background.type == IR_COLOR_GRADIENT && src->style->background.data.gradient) {
                dest->style->background.data.gradient = malloc(sizeof(IRGradient));
                if (dest->style->background.data.gradient) {
                    memcpy(dest->style->background.data.gradient, src->style->background.data.gradient, sizeof(IRGradient));
                }
            }

            // Copy animations
            if (src->style->animation_count > 0) {
                dest->style->animations = calloc(src->style->animation_count, sizeof(IRAnimation*));
                if (dest->style->animations) {
                    for (uint32_t i = 0; i < src->style->animation_count; i++) {
                        if (src->style->animations[i]) {
                            dest->style->animations[i] = malloc(sizeof(IRAnimation));
                            if (dest->style->animations[i]) {
                                memcpy(dest->style->animations[i], src->style->animations[i], sizeof(IRAnimation));
                                if (src->style->animations[i]->name) {
                                    dest->style->animations[i]->name = strdup(src->style->animations[i]->name);
                                }
                            }
                        }
                    }
                }
            }

            // Copy transitions
            if (src->style->transition_count > 0) {
                dest->style->transitions = calloc(src->style->transition_count, sizeof(IRTransition*));
                if (dest->style->transitions) {
                    for (uint32_t i = 0; i < src->style->transition_count; i++) {
                        if (src->style->transitions[i]) {
                            dest->style->transitions[i] = malloc(sizeof(IRTransition));
                            if (dest->style->transitions[i]) {
                                memcpy(dest->style->transitions[i], src->style->transitions[i], sizeof(IRTransition));
                            }
                        }
                    }
                }
            }
        }
    }

    // Copy layout if present
    if (src->layout) {
        dest->layout = ir_create_layout();
        if (dest->layout && src->layout) {
            memcpy(dest->layout, src->layout, sizeof(IRLayout));
        }
    }

    // NOTE: Do NOT copy layout_state! ForEach expanded copies need fresh layout calculation.
    // Copying layout_state would cause all expanded copies to overlap in the same position.
    dest->layout_state = NULL;

    // Copy events if present
    if (src->events && src->child_count > 0) {
        // Count events first
        int event_count = 0;
        IREvent* ev = src->events;
        while (ev) {
            event_count++;
            ev = ev->next;
        }

        // Copy each event
        if (event_count > 0) {
            IREvent** dest_next_ptr = &dest->events;
            ev = src->events;
            while (ev) {
                IREvent* new_event = ir_create_event(ev->type, ev->logic_id, NULL);
                if (new_event) {
                    if (ev->handler_source) {
                        new_event->handler_source = calloc(1, sizeof(IRHandlerSource));
                        if (new_event->handler_source) {
                            new_event->handler_source->language = ev->handler_source->language ? strdup(ev->handler_source->language) : NULL;
                            new_event->handler_source->code = ev->handler_source->code ? strdup(ev->handler_source->code) : NULL;
                            new_event->handler_source->file = ev->handler_source->file ? strdup(ev->handler_source->file) : NULL;
                            new_event->handler_source->line = ev->handler_source->line;
                            new_event->handler_source->uses_closures = ev->handler_source->uses_closures;
                            new_event->handler_source->closure_var_count = ev->handler_source->closure_var_count;

                            // Copy closure variables array
                            if (ev->handler_source->closure_vars && ev->handler_source->closure_var_count > 0) {
                                new_event->handler_source->closure_vars = calloc(ev->handler_source->closure_var_count, sizeof(char*));
                                for (int i = 0; i < ev->handler_source->closure_var_count; i++) {
                                    if (ev->handler_source->closure_vars[i]) {
                                        new_event->handler_source->closure_vars[i] = strdup(ev->handler_source->closure_vars[i]);
                                    }
                                }
                            }
                        }
                    }
                    *dest_next_ptr = new_event;
                    dest_next_ptr = &new_event->next;
                }
                ev = ev->next;
            }
        }
    }

    // Recursively copy children
    if (src->child_count > 0) {
        dest->children = calloc(src->child_count, sizeof(IRComponent*));
        if (dest->children) {
            for (uint32_t i = 0; i < src->child_count; i++) {
                dest->children[i] = ir_deep_copy_component(src->children[i]);
                if (dest->children[i]) {
                    dest->children[i]->parent = dest;  // Set parent pointer
                }
            }
        } else {
            dest->child_count = 0;
        }
    }

    return dest;
}

/**
 * Update text content of a component tree based on ForEach iteration data
 * This handles dynamic text like calendar day numbers
 *
 * @param component The component (or subtree) to update
 * @param data_item The JSON data for this iteration (e.g., {"dayNumber": 1, "isCurrentMonth": true})
 * @param item_name The name of the iteration variable (e.g., "day", "weekRow")
 */
__attribute__((unused))
static void update_foreach_component_text(IRComponent* component, cJSON* data_item, const char* item_name) {
    if (!component || !data_item) return;

    // Declare cJSON functions
    extern cJSON* cJSON_GetObjectItem(const cJSON* object, const char* string);
    extern int cJSON_IsNumber(const cJSON* item);
    extern int cJSON_IsBool(const cJSON* item);
    extern int cJSON_IsTrue(const cJSON* item);
    extern int cJSON_IsArray(const cJSON* item);
    extern double cJSON_GetNumberValue(const cJSON* item);

    // Skip if data_item is an array (this is a nested ForEach parent, not the leaf data)
    if (cJSON_IsArray(data_item)) {
        return;
    }

    // Get the dayNumber field for Button/Text components
    cJSON* day_number = cJSON_GetObjectItem(data_item, "dayNumber");
    cJSON* is_current_month = cJSON_GetObjectItem(data_item, "isCurrentMonth");

    // For Button or Text components with dynamic text, update based on calendar data
    if ((component->type == IR_COMPONENT_BUTTON || component->type == IR_COMPONENT_TEXT) && day_number) {
        // Check if this is a calendar button (has dayNumber in data)
        if (cJSON_IsNumber(day_number)) {
            int day_num = (int)cJSON_GetNumberValue(day_number);

            // Check if we should show the number (only for current month)
            bool show_number = true;
            if (is_current_month && cJSON_IsBool(is_current_month)) {
                show_number = cJSON_IsTrue(is_current_month);
            }

            // Update text_content
            if (component->text_content) {
                free(component->text_content);
            }

            if (show_number) {
                // Convert day number to string
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", day_num);
                component->text_content = strdup(buf);
            } else {
                // Empty string for non-current month days
                component->text_content = strdup("");
            }
        }
    }

    // Don't recurse - only update the immediate component
    // Nested ForEach will be expanded separately and their children will be updated then
}

/**
 * Update nested ForEach components' each_source with the correct iteration data.
 * When expanding an outer ForEach (e.g., weeks), each copy of the template contains
 * a nested ForEach (e.g., days). This function finds those nested ForEach components
 * and updates their each_source with the current iteration's data.
 *
 * @param component The component tree to search for nested ForEach
 * @param source_array The cJSON array to set as the nested ForEach's each_source
 */
static void update_nested_foreach_source(IRComponent* component, cJSON* source_array) {
    if (!component || !source_array) return;

    extern char* cJSON_PrintUnformatted(const cJSON* item);

    // If this is a ForEach, update its each_source
    if (component->type == IR_COMPONENT_FOR_EACH) {
        // Free old each_source if present
        if (component->each_source) {
            free(component->each_source);
            component->each_source = NULL;
        }
        // Serialize the new source array to string
        char* json_str = cJSON_PrintUnformatted(source_array);
        component->each_source = json_str;

        // Debug: show first item's dayNumber to verify correct week data
        extern cJSON* cJSON_GetArrayItem(const cJSON* array, int index);
        extern cJSON* cJSON_GetObjectItem(const cJSON* object, const char* string);
        extern double cJSON_GetNumberValue(const cJSON* item);
        cJSON* first_item = cJSON_GetArrayItem(source_array, 0);
        if (first_item) {
            cJSON* day_num = cJSON_GetObjectItem(first_item, "dayNumber");
            if (day_num) {
                // day number found
            }
        }
        return;  // Don't recurse into ForEach children (template)
    }

    // Recurse into children
    for (uint32_t i = 0; i < component->child_count; i++) {
        update_nested_foreach_source(component->children[i], source_array);
    }
}

/**
 * Replace a ForEach component with its expanded children in the parent's children array.
 * This is necessary because ForEach is layout-transparent - if we leave the ForEach
 * in the tree, all children would be laid out at the same position.
 *
 * @param parent The parent component containing the ForEach (NULL for root-level ForEach)
 * @param foreach_index The index of the ForEach in parent's children array
 * @param expanded_children The expanded children to replace the ForEach with
 * @param expanded_count The number of expanded children
 */
static void replace_foreach_with_children(IRComponent* parent, int foreach_index,
                                          IRComponent** expanded_children, int expanded_count) {
    if (!expanded_children || expanded_count == 0) return;

    // Special case: parent is NULL (root-level ForEach)
    // This shouldn't happen in normal usage, but handle it gracefully
    if (!parent) {
        return;
    }

    // Allocate new children array with space for expanded children
    int new_count = parent->child_count - 1 + expanded_count;
    IRComponent** new_children = calloc(new_count, sizeof(IRComponent*));
    if (!new_children) return;

    // Copy children before the ForEach
    int dest_idx = 0;
    for (int i = 0; i < foreach_index; i++) {
        new_children[dest_idx++] = parent->children[i];
    }

    // Add the expanded children
    for (int i = 0; i < expanded_count; i++) {
        new_children[dest_idx++] = expanded_children[i];
        if (expanded_children[i] && !expanded_children[i]->parent) {
            // Only set parent if not already set (deep copy preserves correct parent)
            expanded_children[i]->parent = parent;
        }
    }

    // Copy children after the ForEach
    for (int i = foreach_index + 1; i < (int)parent->child_count; i++) {
        new_children[dest_idx++] = parent->children[i];
    }

    // Free the old children array (but not the components themselves)
    free(parent->children);

    // Update parent
    parent->children = new_children;
    parent->child_count = new_count;
}

/**
 * Expand ForEach components that have serialized each_source data
 * This is called after loading the KIR file to expand ForEach templates
 *
 * For the calendar: Outer ForEach (6 week rows) → Inner ForEach (7 days each)
 */
__attribute__((unused))
static void expand_foreach_component(IRComponent* component) {
    if (!component) return;

    // Check if this is a ForEach component with serialized data
    if (component->type != IR_COMPONENT_FOR_EACH) {
        // Not a ForEach - process children
        for (uint32_t i = 0; i < component->child_count; i++) {
            if (component->children[i]) {
                expand_foreach_component(component->children[i]);
            }
        }
        return;
    }

    // ForEach data can be in two places:
    // 1. custom_data (from initial Lua compilation)
    // 2. each_source (from KIR file deserialization)

    // Check for runtime marker - skip expansion if data should come from runtime
    if (component->each_source &&
        (strcmp(component->each_source, "__runtime__") == 0 ||
         strcmp(component->each_source, "\"__runtime__\"") == 0)) {
        // ForEach has runtime marker - skip expansion, process children as-is
        for (uint32_t i = 0; i < component->child_count; i++) {
            if (component->children[i]) {
                expand_foreach_component(component->children[i]);
            }
        }
        return;
    }

    if (!component->custom_data && !component->each_source) {
        // No data - process children
        for (uint32_t i = 0; i < component->child_count; i++) {
            if (component->children[i]) {
                expand_foreach_component(component->children[i]);
            }
        }
        return;
    }

    cJSON* custom_json = NULL;
    cJSON* each_source_item = NULL;
    int num_items = 0;

    // Declare cJSON functions for use in this function
    extern cJSON* cJSON_Parse(const char* value);
    extern void cJSON_Delete(cJSON* item);
    extern cJSON* cJSON_GetObjectItem(const cJSON* object, const char* string);
    extern int cJSON_IsArray(const cJSON* item);
    extern int cJSON_IsObject(const cJSON* item);
    extern cJSON* cJSON_GetArrayItem(const cJSON* array, int index);
    extern int cJSON_GetArraySize(const cJSON* array);

    // Prefer each_source (from KIR) over custom_data (from initial compilation)
    if (component->each_source) {
        // Parse each_source directly (it should already be a JSON string)
        custom_json = cJSON_Parse(component->each_source);
        if (custom_json && cJSON_IsArray(custom_json)) {
            each_source_item = custom_json;
            num_items = cJSON_GetArraySize(custom_json);
        }
    }

    // Fall back to custom_data (from initial compilation)
    if (!each_source_item && component->custom_data) {
        custom_json = cJSON_Parse(component->custom_data);
        if (!custom_json) {
            for (uint32_t i = 0; i < component->child_count; i++) {
                if (component->children[i]) {
                    expand_foreach_component(component->children[i]);
                }
            }
            return;
        }

        each_source_item = cJSON_GetObjectItem(custom_json, "each_source");
        if (each_source_item && cJSON_IsArray(each_source_item)) {
            num_items = cJSON_GetArraySize(each_source_item);
        }
    }

    if (!each_source_item || !cJSON_IsArray(each_source_item)) {
        if (custom_json) cJSON_Delete(custom_json);
        for (uint32_t i = 0; i < component->child_count; i++) {
            if (component->children[i]) {
                expand_foreach_component(component->children[i]);
            }
        }
        return;
    }

    if (num_items == 0) {
        cJSON_Delete(custom_json);
        return;
    }

    // Get the template child (first child is the template)
    if (component->child_count == 0 || !component->children[0]) {
        cJSON_Delete(custom_json);
        return;
    }

    IRComponent* template = component->children[0];

    // FIXED: Don't expand nested ForEach first!
    // The old code did: expand_foreach_component(template);
    // This caused nested ForEach to be expanded, then serialized to JSON,
    // which corrupted the nested ForEach's custom_data (double-encoded)

    // Allocate new children array
    IRComponent** new_children = calloc(num_items, sizeof(IRComponent*));
    if (!new_children) {
        cJSON_Delete(custom_json);
        return;
    }

    // For each item in each_source, create a DEEP COPY of the template
    // Using deep copy instead of JSON serialize/deserialize preserves
    // nested ForEach custom_data without double-encoding
    int expanded_count = 0;
    for (int i = 0; i < num_items; i++) {
        IRComponent* copy = ir_deep_copy_component(template);
        if (copy) {
            // Set parent pointer
            copy->parent = component;

            // Update the copy's source_metadata with iteration index
            copy->source_metadata.iteration_index = i;

            // Update dynamic text content based on iteration data
            // For calendar buttons: extract dayNumber from the data item
            cJSON* data_item = cJSON_GetArrayItem(each_source_item, i);
            if (data_item && cJSON_IsObject(data_item)) {
                update_foreach_component_text(copy, data_item, component->each_item_name);
            }

            new_children[expanded_count++] = copy;
        }
    }

    if (expanded_count > 0) {
        // Free old children (just the template)
        if (component->children) {
            // Don't free the template here as it might be in use
            // Just replace the children array
        }

        // Update component with expanded children
        component->children = new_children;
        component->child_count = expanded_count;

        // Set parent pointers for all new children
        for (int i = 0; i < expanded_count; i++) {
            if (new_children[i]) {
                new_children[i]->parent = component;
            }
        }

        // IMPORTANT: After copying, expand nested ForEach in each copy
        // This is the KEY FIX for nested ForEach support!
        // We do this AFTER all copies are made so each copy gets its
        // own nested ForEach with its original (uncorrupted) custom_data
        for (int i = 0; i < expanded_count; i++) {
            if (new_children[i]) {
                expand_foreach_component(new_children[i]);
            }
        }
    } else {
        free(new_children);
    }

    cJSON_Delete(custom_json);
}

/**
 * Expand ForEach components with parent tracking
 * This version tracks the parent and child index so we can replace the ForEach
 * with its expanded children, avoiding the layout-overlap issue.
 */
__attribute__((unused))
static void expand_foreach_with_parent(IRComponent* component, IRComponent* parent, int child_index) {
    if (!component) return;

    // Check if this is a ForEach component with serialized data
    if (component->type != IR_COMPONENT_FOR_EACH) {
        // Not a ForEach - process children
        for (uint32_t i = 0; i < component->child_count; i++) {
            if (component->children[i]) {
                expand_foreach_with_parent(component->children[i], component, (int)i);
            }
        }
        return;
    }

    // ForEach data can be in two places:
    // 1. custom_data (from initial Lua compilation)
    // 2. each_source (from KIR file deserialization)

    // Check for runtime marker - skip expansion if data should come from runtime
    if (component->each_source &&
        (strcmp(component->each_source, "__runtime__") == 0 ||
         strcmp(component->each_source, "\"__runtime__\"") == 0)) {
        // ForEach has runtime marker - replace with template child
        if (component->child_count > 0) {
            replace_foreach_with_children(parent, child_index, component->children, component->child_count);
            component->children = NULL;
            component->child_count = 0;
        }
        return;
    }

    if (!component->custom_data && !component->each_source) {
        // No data - replace with template child
        if (component->child_count > 0) {
            replace_foreach_with_children(parent, child_index, component->children, component->child_count);
            component->children = NULL;
            component->child_count = 0;
        }
        return;
    }

    cJSON* custom_json = NULL;
    cJSON* each_source_item = NULL;
    int num_items = 0;

    // Declare cJSON functions for use in this function
    extern cJSON* cJSON_Parse(const char* value);
    extern void cJSON_Delete(cJSON* item);
    extern cJSON* cJSON_GetObjectItem(const cJSON* object, const char* string);
    extern int cJSON_IsArray(const cJSON* item);
    extern int cJSON_IsObject(const cJSON* item);
    extern cJSON* cJSON_GetArrayItem(const cJSON* array, int index);
    extern int cJSON_GetArraySize(const cJSON* array);

    // Prefer each_source (from KIR) over custom_data (from initial compilation)
    if (component->each_source) {
        // Parse each_source directly (it should already be a JSON string)
        custom_json = cJSON_Parse(component->each_source);
        if (custom_json && cJSON_IsArray(custom_json)) {
            each_source_item = custom_json;
            num_items = cJSON_GetArraySize(custom_json);
        }
    }

    // Fall back to custom_data (from initial compilation)
    if (!each_source_item && component->custom_data) {
        custom_json = cJSON_Parse(component->custom_data);
        if (!custom_json) {
            // Replace with template child and return
            if (component->child_count > 0) {
                replace_foreach_with_children(parent, child_index, component->children, component->child_count);
                component->children = NULL;
                component->child_count = 0;
            }
            return;
        }

        each_source_item = cJSON_GetObjectItem(custom_json, "each_source");

        // Check for runtime marker in custom_data
        extern int cJSON_IsString(const cJSON* item);
        extern char* cJSON_GetStringValue(const cJSON* item);
        if (each_source_item && cJSON_IsString(each_source_item)) {
            char* marker = cJSON_GetStringValue(each_source_item);
            if (marker && strcmp(marker, "__runtime__") == 0) {
                cJSON_Delete(custom_json);
                // Replace ForEach with its template children
                if (component->child_count > 0) {
                    replace_foreach_with_children(parent, child_index, component->children, component->child_count);
                    component->children = NULL;
                    component->child_count = 0;
                }
                return;
            }
        }

        if (each_source_item && cJSON_IsArray(each_source_item)) {
            num_items = cJSON_GetArraySize(each_source_item);
        }
    }

    if (!each_source_item || !cJSON_IsArray(each_source_item)) {
        if (custom_json) cJSON_Delete(custom_json);
        // Replace with template child and return
        if (component->child_count > 0) {
            replace_foreach_with_children(parent, child_index, component->children, component->child_count);
            component->children = NULL;
            component->child_count = 0;
        }
        return;
    }

    if (num_items == 0) {
        if (custom_json) cJSON_Delete(custom_json);
        return;
    }

    if (component->child_count == 0 || !component->children[0]) {
        cJSON_Delete(custom_json);
        return;
    }

    IRComponent* template = component->children[0];

    // Allocate new children array
    IRComponent** new_children = calloc(num_items, sizeof(IRComponent*));
    if (!new_children) {
        cJSON_Delete(custom_json);
        return;
    }

    // For each item in each_source, create a DEEP COPY of the template
    int expanded_count = 0;
    for (int i = 0; i < num_items; i++) {
        IRComponent* copy = ir_deep_copy_component(template);
        if (copy) {
            copy->parent = component;
            copy->source_metadata.iteration_index = i;

            // Update dynamic text content based on iteration data
            cJSON* data_item = cJSON_GetArrayItem(each_source_item, i);
            if (data_item) {
                if (cJSON_IsObject(data_item)) {
                    // Leaf data - update text content directly
                    update_foreach_component_text(copy, data_item, component->each_item_name);
                } else if (cJSON_IsArray(data_item)) {
                    // Nested array data - propagate to nested ForEach components
                    // This is crucial for nested ForEach (e.g., weeks containing days)
                    update_nested_foreach_source(copy, data_item);
                }
            }

            new_children[expanded_count++] = copy;
        }
    }

    if (expanded_count > 0) {
        // Update component with expanded children
        component->children = new_children;
        component->child_count = expanded_count;

        // IMPORTANT: Expand nested ForEach in each copy BEFORE replacing in parent
        // This ensures nested ForEach are also expanded and replaced
        for (int i = 0; i < expanded_count; i++) {
            if (new_children[i]) {
                expand_foreach_with_parent(new_children[i], component, i);
            }
        }

        // NOW replace this ForEach with its expanded children in the parent
        // This is the KEY FIX - the ForEach is removed from the tree and its children
        // take its place, so the parent (Row/Column) can layout them correctly
        replace_foreach_with_children(parent, child_index, new_children, expanded_count);

        // Don't free new_children - they're now owned by the parent
        // But mark component's children as NULL so we don't double-free
        component->children = NULL;
        component->child_count = 0;
    } else {
        free(new_children);
    }

    cJSON_Delete(custom_json);
}

/**
 * Expand all ForEach components in the tree
 * Call this after loading a KIR file to expand ForEach templates
 *
 * This function now delegates to the new modular ForEach expansion system
 * in ir_foreach_expand.c which provides:
 * - Generic property binding system (replaces hardcoded field updates)
 * - Clean separation between definition, serialization, and expansion
 * - Better nested ForEach support
 */
void ir_expand_foreach(IRComponent* root) {
    // Delegate to the new modular ForEach expansion
    ir_foreach_expand_tree(root);
}
