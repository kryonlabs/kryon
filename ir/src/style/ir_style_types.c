/**
 * @file ir_style_types.c
 * @brief Color and dimension utility functions
 */

#define _GNU_SOURCE
#include "../include/ir_style.h"
#include "../include/ir_layout.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Color Utilities
// ============================================================================

IRColor ir_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    IRColor color = { .type = IR_COLOR_SOLID };
    color.data.r = r;
    color.data.g = g;
    color.data.b = b;
    color.data.a = a;
    color.var_name = NULL;
    return color;
}

bool ir_color_equals(IRColor a, IRColor b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case IR_COLOR_SOLID:
            return a.data.r == b.data.r &&
                   a.data.g == b.data.g &&
                   a.data.b == b.data.b &&
                   a.data.a == b.data.a;
        case IR_COLOR_TRANSPARENT:
            return true;
        case IR_COLOR_VAR_REF:
            return a.data.var_id == b.data.var_id;
        case IR_COLOR_GRADIENT:
            return a.data.gradient == b.data.gradient;
        default:
            return false;
    }
}

void ir_color_free(IRColor* color) {
    if (color) {
        free(color->var_name);
        color->var_name = NULL;
    }
}

// ============================================================================
// Dimension Utilities
// ============================================================================

IRDimension ir_dimension_px(float value) {
    IRDimension dim = { .type = IR_DIMENSION_PX, .value = value };
    return dim;
}

IRDimension ir_dimension_percent(float value) {
    IRDimension dim = { .type = IR_DIMENSION_PERCENT, .value = value };
    return dim;
}

IRDimension ir_dimension_auto(void) {
    IRDimension dim = { .type = IR_DIMENSION_AUTO, .value = 0 };
    return dim;
}

float ir_dimension_resolve(IRDimension dim, float parent_size) {
    switch (dim.type) {
        case IR_DIMENSION_PX:
        case IR_DIMENSION_REM:
        case IR_DIMENSION_EM:
            return dim.value;
        case IR_DIMENSION_PERCENT:
            return (dim.value / 100.0f) * parent_size;
        case IR_DIMENSION_AUTO:
        default:
            return 0;  // Let layout engine decide
    }
}
