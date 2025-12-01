#define _GNU_SOURCE
#include "ir_serialization.h"
#include "ir_builder.h"
#include "third_party/cJSON/cJSON.h"
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

static bool write_float32(IRBuffer* buffer, float value) {
    return ir_buffer_write(buffer, &value, sizeof(float));
}

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

static bool read_float32(IRBuffer* buffer, float* value) {
    return ir_buffer_read(buffer, value, sizeof(float));
}

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

// Buffer Management
IRBuffer* ir_buffer_create(size_t initial_capacity) {
    IRBuffer* buffer = malloc(sizeof(IRBuffer));
    if (!buffer) return NULL;

    buffer->data = malloc(initial_capacity);
    if (!buffer->data) {
        free(buffer);
        return NULL;
    }

    buffer->base = buffer->data;  // Track original pointer for free
    buffer->size = 0;
    buffer->capacity = initial_capacity;

    return buffer;
}

IRBuffer* ir_buffer_create_from_file(const char* filename) {
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

    return buffer;
}

void ir_buffer_destroy(IRBuffer* buffer) {
    if (!buffer) return;

    if (buffer->base) {
        free(buffer->base);  // Free original pointer, not current position
    }
    free(buffer);
}

bool ir_buffer_write(IRBuffer* buffer, const void* data, size_t size) {
    if (!buffer || !data) return false;

    // Check if we need to expand the buffer
    if (buffer->size + size > buffer->capacity) {
        size_t new_capacity = buffer->capacity * 2;
        if (new_capacity < buffer->size + size) {
            new_capacity = buffer->size + size + 1024;  // Add some extra space
        }

        uint8_t* new_data = realloc(buffer->base, new_capacity);
        if (!new_data) return false;

        buffer->base = new_data;
        buffer->data = new_data;  // Reset to base since we're writing
        buffer->capacity = new_capacity;
    }

    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;

    return true;
}

bool ir_buffer_read(IRBuffer* buffer, void* data, size_t size) {
    if (!buffer || !data) return false;

    if (buffer->size < size) return false;  // Not enough data

    memcpy(data, buffer->data, size);
    buffer->data += size;
    buffer->size -= size;

    return true;
}

bool ir_buffer_seek(IRBuffer* buffer, size_t position) {
    if (!buffer) return false;

    // This is a simplified implementation - in practice you'd need to track the original data pointer
    return false;  // Not implemented for this simple version
}

size_t ir_buffer_tell(IRBuffer* buffer) {
    return buffer ? buffer->size : 0;
}

size_t ir_buffer_size(IRBuffer* buffer) {
    return buffer ? buffer->size : 0;
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

static bool serialize_color(IRBuffer* buffer, IRColor color) {
    if (!write_uint8(buffer, color.type)) return false;
    if (!write_uint8(buffer, color.data.r)) return false;
    if (!write_uint8(buffer, color.data.g)) return false;
    if (!write_uint8(buffer, color.data.b)) return false;
    if (!write_uint8(buffer, color.data.a)) return false;
    return true;
}

static bool deserialize_color(IRBuffer* buffer, IRColor* color) {
    if (!read_uint8(buffer, (uint8_t*)&color->type)) return false;
    if (!read_uint8(buffer, &color->data.r)) return false;
    if (!read_uint8(buffer, &color->data.g)) return false;
    if (!read_uint8(buffer, &color->data.b)) return false;
    if (!read_uint8(buffer, &color->data.a)) return false;
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
    if (!write_uint8(buffer, flex->main_axis)) return false;
    if (!write_uint8(buffer, flex->cross_axis)) return false;
    if (!write_uint8(buffer, flex->justify_content)) return false;
    if (!write_uint8(buffer, flex->grow)) return false;
    if (!write_uint8(buffer, flex->shrink)) return false;
    if (!write_uint8(buffer, flex->direction)) return false;
    return true;
}

static bool deserialize_flexbox(IRBuffer* buffer, IRFlexbox* flex) {
    if (!read_uint8(buffer, (uint8_t*)&flex->wrap)) return false;
    if (!read_uint32(buffer, &flex->gap)) return false;
    if (!read_uint8(buffer, (uint8_t*)&flex->main_axis)) return false;
    if (!read_uint8(buffer, (uint8_t*)&flex->cross_axis)) return false;
    if (!read_uint8(buffer, (uint8_t*)&flex->justify_content)) return false;
    if (!read_uint8(buffer, &flex->grow)) return false;
    if (!read_uint8(buffer, &flex->shrink)) return false;
    if (!read_uint8(buffer, &flex->direction)) return false;
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
// Reactive Manifest Serialization
// ============================================================================

// Header flags
#define IR_HEADER_FLAG_HAS_MANIFEST 0x01

// Serialize a reactive variable
static bool serialize_reactive_var(IRBuffer* buffer, IRReactiveVarDescriptor* var) {
    if (!write_uint32(buffer, var->id)) return false;
    if (!write_string(buffer, var->name)) return false;
    if (!write_uint8(buffer, (uint8_t)var->type)) return false;
    if (!write_uint32(buffer, var->version)) return false;
    if (!write_string(buffer, var->source_location)) return false;
    if (!write_uint32(buffer, var->binding_count)) return false;

    // Serialize value based on type
    switch (var->type) {
        case IR_REACTIVE_TYPE_INT:
            if (!write_uint32(buffer, (uint32_t)var->value.as_int)) return false;
            break;
        case IR_REACTIVE_TYPE_FLOAT:
            if (!write_float64(buffer, var->value.as_float)) return false;
            break;
        case IR_REACTIVE_TYPE_STRING:
            if (!write_string(buffer, var->value.as_string)) return false;
            break;
        case IR_REACTIVE_TYPE_BOOL:
            if (!write_uint8(buffer, var->value.as_bool ? 1 : 0)) return false;
            break;
        case IR_REACTIVE_TYPE_CUSTOM:
            if (!write_string(buffer, var->value.as_string)) return false;
            break;
    }

    return true;
}

// Deserialize a reactive variable
static bool deserialize_reactive_var(IRBuffer* buffer, IRReactiveVarDescriptor* var) {
    if (!read_uint32(buffer, &var->id)) return false;
    if (!read_string(buffer, &var->name)) return false;

    uint8_t type;
    if (!read_uint8(buffer, &type)) return false;
    var->type = (IRReactiveVarType)type;

    if (!read_uint32(buffer, &var->version)) return false;
    if (!read_string(buffer, &var->source_location)) return false;
    if (!read_uint32(buffer, &var->binding_count)) return false;

    // Deserialize value based on type
    switch (var->type) {
        case IR_REACTIVE_TYPE_INT: {
            uint32_t val;
            if (!read_uint32(buffer, &val)) return false;
            var->value.as_int = (int32_t)val;
            break;
        }
        case IR_REACTIVE_TYPE_FLOAT:
            if (!read_float64(buffer, &var->value.as_float)) return false;
            break;
        case IR_REACTIVE_TYPE_STRING:
            if (!read_string(buffer, &var->value.as_string)) return false;
            break;
        case IR_REACTIVE_TYPE_BOOL: {
            uint8_t val;
            if (!read_uint8(buffer, &val)) return false;
            var->value.as_bool = (val != 0);
            break;
        }
        case IR_REACTIVE_TYPE_CUSTOM:
            if (!read_string(buffer, &var->value.as_string)) return false;
            break;
    }

    return true;
}

// Serialize a reactive binding
static bool serialize_reactive_binding(IRBuffer* buffer, IRReactiveBinding* binding) {
    if (!write_uint32(buffer, binding->component_id)) return false;
    if (!write_uint32(buffer, binding->reactive_var_id)) return false;
    if (!write_uint8(buffer, (uint8_t)binding->binding_type)) return false;
    if (!write_string(buffer, binding->expression)) return false;
    if (!write_string(buffer, binding->update_code)) return false;
    return true;
}

// Deserialize a reactive binding
static bool deserialize_reactive_binding(IRBuffer* buffer, IRReactiveBinding* binding) {
    if (!read_uint32(buffer, &binding->component_id)) return false;
    if (!read_uint32(buffer, &binding->reactive_var_id)) return false;

    uint8_t type;
    if (!read_uint8(buffer, &type)) return false;
    binding->binding_type = (IRBindingType)type;

    if (!read_string(buffer, &binding->expression)) return false;
    if (!read_string(buffer, &binding->update_code)) return false;
    return true;
}

// Serialize a reactive conditional
static bool serialize_reactive_conditional(IRBuffer* buffer, IRReactiveConditional* cond) {
    if (!write_uint32(buffer, cond->component_id)) return false;
    if (!write_string(buffer, cond->condition)) return false;
    if (!write_uint8(buffer, cond->last_eval_result ? 1 : 0)) return false;
    if (!write_uint8(buffer, cond->suspended ? 1 : 0)) return false;
    if (!write_uint32(buffer, cond->dependent_var_count)) return false;

    for (uint32_t i = 0; i < cond->dependent_var_count; i++) {
        if (!write_uint32(buffer, cond->dependent_var_ids[i])) return false;
    }

    return true;
}

// Deserialize a reactive conditional
static bool deserialize_reactive_conditional(IRBuffer* buffer, IRReactiveConditional* cond) {
    if (!read_uint32(buffer, &cond->component_id)) return false;
    if (!read_string(buffer, &cond->condition)) return false;

    uint8_t val;
    if (!read_uint8(buffer, &val)) return false;
    cond->last_eval_result = (val != 0);

    if (!read_uint8(buffer, &val)) return false;
    cond->suspended = (val != 0);

    if (!read_uint32(buffer, &cond->dependent_var_count)) return false;

    if (cond->dependent_var_count > 0) {
        cond->dependent_var_ids = malloc(sizeof(uint32_t) * cond->dependent_var_count);
        if (!cond->dependent_var_ids) return false;

        for (uint32_t i = 0; i < cond->dependent_var_count; i++) {
            if (!read_uint32(buffer, &cond->dependent_var_ids[i])) return false;
        }
    } else {
        cond->dependent_var_ids = NULL;
    }

    return true;
}

// Serialize a reactive for-loop
static bool serialize_reactive_for_loop(IRBuffer* buffer, IRReactiveForLoop* loop) {
    if (!write_uint32(buffer, loop->parent_component_id)) return false;
    if (!write_string(buffer, loop->collection_expr)) return false;
    if (!write_uint32(buffer, loop->collection_var_id)) return false;
    if (!write_string(buffer, loop->item_template)) return false;
    if (!write_uint32(buffer, loop->child_count)) return false;

    for (uint32_t i = 0; i < loop->child_count; i++) {
        if (!write_uint32(buffer, loop->child_component_ids[i])) return false;
    }

    return true;
}

// Deserialize a reactive for-loop
static bool deserialize_reactive_for_loop(IRBuffer* buffer, IRReactiveForLoop* loop) {
    if (!read_uint32(buffer, &loop->parent_component_id)) return false;
    if (!read_string(buffer, &loop->collection_expr)) return false;
    if (!read_uint32(buffer, &loop->collection_var_id)) return false;
    if (!read_string(buffer, &loop->item_template)) return false;
    if (!read_uint32(buffer, &loop->child_count)) return false;

    if (loop->child_count > 0) {
        loop->child_component_ids = malloc(sizeof(uint32_t) * loop->child_count);
        if (!loop->child_component_ids) return false;

        for (uint32_t i = 0; i < loop->child_count; i++) {
            if (!read_uint32(buffer, &loop->child_component_ids[i])) return false;
        }
    } else {
        loop->child_component_ids = NULL;
    }

    return true;
}

// Serialize complete reactive manifest
static bool serialize_reactive_manifest(IRBuffer* buffer, IRReactiveManifest* manifest) {
    if (!manifest) return true;  // NULL manifest is OK

    // Write manifest header
    if (!write_uint32(buffer, manifest->format_version)) return false;
    if (!write_uint32(buffer, manifest->next_var_id)) return false;

    // Write variables
    if (!write_uint32(buffer, manifest->variable_count)) return false;
    for (uint32_t i = 0; i < manifest->variable_count; i++) {
        if (!serialize_reactive_var(buffer, &manifest->variables[i])) return false;
    }

    // Write bindings
    if (!write_uint32(buffer, manifest->binding_count)) return false;
    for (uint32_t i = 0; i < manifest->binding_count; i++) {
        if (!serialize_reactive_binding(buffer, &manifest->bindings[i])) return false;
    }

    // Write conditionals
    if (!write_uint32(buffer, manifest->conditional_count)) return false;
    for (uint32_t i = 0; i < manifest->conditional_count; i++) {
        if (!serialize_reactive_conditional(buffer, &manifest->conditionals[i])) return false;
    }

    // Write for-loops
    if (!write_uint32(buffer, manifest->for_loop_count)) return false;
    for (uint32_t i = 0; i < manifest->for_loop_count; i++) {
        if (!serialize_reactive_for_loop(buffer, &manifest->for_loops[i])) return false;
    }

    return true;
}

// Deserialize complete reactive manifest
static bool deserialize_reactive_manifest(IRBuffer* buffer, IRReactiveManifest** manifest_ptr) {
    IRReactiveManifest* manifest = ir_reactive_manifest_create();
    if (!manifest) return false;

    // Read manifest header
    if (!read_uint32(buffer, &manifest->format_version)) goto error;
    if (!read_uint32(buffer, &manifest->next_var_id)) goto error;

    // Read variables
    uint32_t var_count;
    if (!read_uint32(buffer, &var_count)) goto error;

    if (var_count > 0) {
        // Ensure capacity
        while (manifest->variable_capacity < var_count) {
            manifest->variable_capacity *= 2;
            IRReactiveVarDescriptor* new_vars = realloc(manifest->variables,
                sizeof(IRReactiveVarDescriptor) * manifest->variable_capacity);
            if (!new_vars) goto error;
            manifest->variables = new_vars;
        }

        for (uint32_t i = 0; i < var_count; i++) {
            if (!deserialize_reactive_var(buffer, &manifest->variables[i])) goto error;
            manifest->variable_count++;
        }
    }

    // Read bindings
    uint32_t binding_count;
    if (!read_uint32(buffer, &binding_count)) goto error;

    if (binding_count > 0) {
        while (manifest->binding_capacity < binding_count) {
            manifest->binding_capacity *= 2;
            IRReactiveBinding* new_bindings = realloc(manifest->bindings,
                sizeof(IRReactiveBinding) * manifest->binding_capacity);
            if (!new_bindings) goto error;
            manifest->bindings = new_bindings;
        }

        for (uint32_t i = 0; i < binding_count; i++) {
            if (!deserialize_reactive_binding(buffer, &manifest->bindings[i])) goto error;
            manifest->binding_count++;
        }
    }

    // Read conditionals
    uint32_t conditional_count;
    if (!read_uint32(buffer, &conditional_count)) goto error;

    if (conditional_count > 0) {
        while (manifest->conditional_capacity < conditional_count) {
            manifest->conditional_capacity *= 2;
            IRReactiveConditional* new_conds = realloc(manifest->conditionals,
                sizeof(IRReactiveConditional) * manifest->conditional_capacity);
            if (!new_conds) goto error;
            manifest->conditionals = new_conds;
        }

        for (uint32_t i = 0; i < conditional_count; i++) {
            if (!deserialize_reactive_conditional(buffer, &manifest->conditionals[i])) goto error;
            manifest->conditional_count++;
        }
    }

    // Read for-loops
    uint32_t for_loop_count;
    if (!read_uint32(buffer, &for_loop_count)) goto error;

    if (for_loop_count > 0) {
        while (manifest->for_loop_capacity < for_loop_count) {
            manifest->for_loop_capacity *= 2;
            IRReactiveForLoop* new_loops = realloc(manifest->for_loops,
                sizeof(IRReactiveForLoop) * manifest->for_loop_capacity);
            if (!new_loops) goto error;
            manifest->for_loops = new_loops;
        }

        for (uint32_t i = 0; i < for_loop_count; i++) {
            if (!deserialize_reactive_for_loop(buffer, &manifest->for_loops[i])) goto error;
            manifest->for_loop_count++;
        }
    }

    *manifest_ptr = manifest;
    return true;

error:
    ir_reactive_manifest_destroy(manifest);
    return false;
}

// ============================================================================
// Binary Serialization Functions
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

    if (magic == IR_MAGIC_NUMBER_LEGACY) {
        // Legacy .kir file (old binary format)
        printf("Warning: Loading legacy .kir format. Consider converting to .kirb format.\n");
    } else if (magic != IR_MAGIC_NUMBER) {
        printf("Error: Invalid magic number in IR file (expected KIRB format)\n");
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

// ============================================================================
// Binary Serialization with Reactive Manifest
// ============================================================================

IRBuffer* ir_serialize_binary_with_manifest(IRComponent* root, IRReactiveManifest* manifest) {
    IRBuffer* buffer = ir_buffer_create(4096);  // Start with 4KB
    if (!buffer) return NULL;

    // Write header with flags
    uint8_t flags = 0;
    if (manifest) flags |= IR_HEADER_FLAG_HAS_MANIFEST;

    if (!write_uint32(buffer, IR_MAGIC_NUMBER)) goto error;
    if (!write_uint16(buffer, IR_FORMAT_VERSION_MAJOR)) goto error;
    if (!write_uint16(buffer, IR_FORMAT_VERSION_MINOR)) goto error;
    if (!write_uint32(buffer, IR_ENDIANNESS_CHECK)) goto error;
    if (!write_uint8(buffer, flags)) goto error;

    // Serialize root component
    if (!serialize_component(buffer, root)) goto error;

    // Serialize reactive manifest if present
    if (manifest) {
        if (!serialize_reactive_manifest(buffer, manifest)) goto error;
    }

    return buffer;

error:
    ir_buffer_destroy(buffer);
    return NULL;
}

IRComponent* ir_deserialize_binary_with_manifest(IRBuffer* buffer, IRReactiveManifest** manifest) {
    if (!buffer) return NULL;

    // Read and validate header
    uint32_t magic;
    uint16_t version_major, version_minor;
    uint32_t endianness_check;
    uint8_t flags;

    if (!read_uint32(buffer, &magic)) return NULL;
    if (!read_uint16(buffer, &version_major)) return NULL;
    if (!read_uint16(buffer, &version_minor)) return NULL;
    if (!read_uint32(buffer, &endianness_check)) return NULL;
    if (!read_uint8(buffer, &flags)) return NULL;

    if (magic == IR_MAGIC_NUMBER_LEGACY) {
        // Legacy .kir file (old binary format)
        printf("Warning: Loading legacy .kir format. Consider converting to .kirb format.\n");
    } else if (magic != IR_MAGIC_NUMBER) {
        printf("Error: Invalid magic number in IR file (expected KIRB format)\n");
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

    // Deserialize reactive manifest if present
    if (manifest) {
        if (flags & IR_HEADER_FLAG_HAS_MANIFEST) {
            if (!deserialize_reactive_manifest(buffer, manifest)) {
                ir_destroy_component(root);
                return NULL;
            }
        } else {
            *manifest = NULL;
        }
    }

    return root;
}

bool ir_write_binary_file_with_manifest(IRComponent* root, IRReactiveManifest* manifest, const char* filename) {
    IRBuffer* buffer = ir_serialize_binary_with_manifest(root, manifest);
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

IRComponent* ir_read_binary_file_with_manifest(const char* filename, IRReactiveManifest** manifest) {
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
    IRComponent* root = ir_deserialize_binary_with_manifest(buffer, manifest);
    ir_buffer_destroy(buffer);

    return root;
}

// JSON Serialization Helper - SAFE version without strcpy
static void append_json(char** json, size_t* size, size_t* capacity, const char* str) {
    size_t len = strlen(str);
    while (*size + len + 1 > *capacity) {
        *capacity *= 2;
        char* new_json = realloc(*json, *capacity);
        if (!new_json) {
            fprintf(stderr, "Error: Failed to allocate memory for JSON serialization\n");
            return;
        }
        *json = new_json;
    }
    // Use memcpy instead of strcpy for safety
    memcpy(*json + *size, str, len);
    *size += len;
    (*json)[*size] = '\0';
}

#define MAX_JSON_DEPTH 64

static void serialize_component_json_recursive(IRComponent* component, char** json, size_t* size, size_t* capacity, int depth) {
    if (!component) return;

    // Depth limit to prevent stack overflow
    if (depth > MAX_JSON_DEPTH) {
        fprintf(stderr, "Error: JSON serialization depth limit exceeded (%d)\n", MAX_JSON_DEPTH);
        return;
    }

    char buffer[1024];
    char indent[256] = "";  // Increased size for safety

    // Build indent string safely
    int indent_len = 0;
    for (int i = 0; i < depth && indent_len < (int)sizeof(indent) - 3; i++) {
        indent[indent_len++] = ' ';
        indent[indent_len++] = ' ';
    }
    indent[indent_len] = '\0';

    // Start component object
    snprintf(buffer, sizeof(buffer), "%s{\n", indent);
    append_json(json, size, capacity, buffer);

    // ID and type
    snprintf(buffer, sizeof(buffer), "%s  \"id\": %u,\n", indent, component->id);
    append_json(json, size, capacity, buffer);

    snprintf(buffer, sizeof(buffer), "%s  \"type\": \"%s\",\n", indent, ir_component_type_to_string(component->type));
    append_json(json, size, capacity, buffer);

    // Text content
    if (component->text_content) {
        snprintf(buffer, sizeof(buffer), "%s  \"text\": \"%s\",\n", indent, component->text_content);
        append_json(json, size, capacity, buffer);
    }

    // Style (simplified - just visible and z_index)
    if (component->style) {
        snprintf(buffer, sizeof(buffer), "%s  \"style\": {\n", indent);
        append_json(json, size, capacity, buffer);

        snprintf(buffer, sizeof(buffer), "%s    \"visible\": %s,\n", indent, component->style->visible ? "true" : "false");
        append_json(json, size, capacity, buffer);

        snprintf(buffer, sizeof(buffer), "%s    \"z_index\": %u\n", indent, component->style->z_index);
        append_json(json, size, capacity, buffer);

        snprintf(buffer, sizeof(buffer), "%s  },\n", indent);
        append_json(json, size, capacity, buffer);
    }

    // Rendered bounds
    if (component->rendered_bounds.valid) {
        snprintf(buffer, sizeof(buffer), "%s  \"bounds\": { \"x\": %.2f, \"y\": %.2f, \"width\": %.2f, \"height\": %.2f },\n",
                indent, component->rendered_bounds.x, component->rendered_bounds.y,
                component->rendered_bounds.width, component->rendered_bounds.height);
        append_json(json, size, capacity, buffer);
    }

    // Children
    if (component->child_count > 0) {
        snprintf(buffer, sizeof(buffer), "%s  \"children\": [\n", indent);
        append_json(json, size, capacity, buffer);

        for (uint32_t i = 0; i < component->child_count; i++) {
            serialize_component_json_recursive(component->children[i], json, size, capacity, depth + 2);
            if (i < component->child_count - 1) {
                append_json(json, size, capacity, ",\n");
            } else {
                append_json(json, size, capacity, "\n");
            }
        }

        snprintf(buffer, sizeof(buffer), "%s  ]\n", indent);
        append_json(json, size, capacity, buffer);
    } else {
        // Remove trailing comma if no children
        if ((*json)[*size - 2] == ',') {
            (*json)[*size - 2] = '\n';
            (*size)--;
        }
    }

    snprintf(buffer, sizeof(buffer), "%s}", indent);
    append_json(json, size, capacity, buffer);
}

// JSON Serialization
char* ir_serialize_json(IRComponent* root) {
    if (!root) return strdup("null");

    size_t capacity = 4096;
    size_t size = 0;
    char* json = malloc(capacity);
    if (!json) return NULL;

    json[0] = '\0';

    serialize_component_json_recursive(root, &json, &size, &capacity, 0);

    append_json(&json, &size, &capacity, "\n");

    return json;
}

IRComponent* ir_deserialize_json(const char* json_string) {
    // This is a placeholder for JSON deserialization
    // In a full implementation, you'd parse the JSON string and build IR components
    return NULL;
}

// File I/O for JSON
bool ir_write_json_file(IRComponent* root, const char* filename) {
    char* json = ir_serialize_json(root);
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
    if (!file) return NULL;

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

    IRComponent* root = ir_deserialize_json(json);
    free(json);

    return root;
}

// Utility Functions
size_t ir_calculate_serialized_size(IRComponent* root) {
    IRBuffer* buffer = ir_serialize_binary(root);
    if (!buffer) return 0;

    size_t size = buffer->size;
    ir_buffer_destroy(buffer);
    return size;
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

void ir_print_buffer_info(IRBuffer* buffer) {
    if (!buffer) {
        printf("Buffer: NULL\n");
        return;
    }

    printf("Buffer Info:\n");
    printf("  Size: %zu bytes\n", buffer->size);
    printf("  Capacity: %zu bytes\n", buffer->capacity);
    printf("  Usage: %.1f%%\n", buffer->capacity > 0 ? (100.0 * buffer->size / buffer->capacity) : 0.0);
}

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