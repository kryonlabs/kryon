// IR Gradient Module
// Gradient helper functions extracted from ir_builder.c

#define _GNU_SOURCE
#include "ir_gradient.h"
#include "ir_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Gradient Creation
// ============================================================================

IRGradient* ir_gradient_create_linear(float angle) {
    IRGradient* gradient = (IRGradient*)calloc(1, sizeof(IRGradient));
    if (!gradient) return NULL;

    gradient->type = IR_GRADIENT_LINEAR;
    gradient->angle = angle;
    gradient->stop_count = 0;
    gradient->center_x = 0.5f;
    gradient->center_y = 0.5f;

    return gradient;
}

IRGradient* ir_gradient_create_radial(float center_x, float center_y) {
    IRGradient* gradient = (IRGradient*)calloc(1, sizeof(IRGradient));
    if (!gradient) return NULL;

    gradient->type = IR_GRADIENT_RADIAL;
    gradient->center_x = center_x;
    gradient->center_y = center_y;
    gradient->stop_count = 0;
    gradient->angle = 0.0f;

    return gradient;
}

IRGradient* ir_gradient_create_conic(float center_x, float center_y) {
    IRGradient* gradient = (IRGradient*)calloc(1, sizeof(IRGradient));
    if (!gradient) return NULL;

    gradient->type = IR_GRADIENT_CONIC;
    gradient->center_x = center_x;
    gradient->center_y = center_y;
    gradient->stop_count = 0;
    gradient->angle = 0.0f;

    return gradient;
}

// Unified gradient creation
IRGradient* ir_gradient_create(IRGradientType type) {
    IRGradient* gradient = (IRGradient*)calloc(1, sizeof(IRGradient));
    if (!gradient) return NULL;

    gradient->type = type;
    gradient->stop_count = 0;
    gradient->center_x = 0.5f;
    gradient->center_y = 0.5f;
    gradient->angle = 0.0f;

    return gradient;
}

void ir_gradient_destroy(IRGradient* gradient) {
    if (gradient) {
        free(gradient);
    }
}

// ============================================================================
// Gradient Configuration
// ============================================================================

void ir_gradient_add_stop(IRGradient* gradient, float position, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!gradient) return;

    if (gradient->stop_count >= 8) {
        fprintf(stderr, "Warning: Gradient stop limit (8) exceeded, ignoring additional stop\n");
        return;
    }

    IRGradientStop* stop = &gradient->stops[gradient->stop_count];
    stop->position = position;
    stop->r = r;
    stop->g = g;
    stop->b = b;
    stop->a = a;

    gradient->stop_count++;
}

void ir_gradient_set_angle(IRGradient* gradient, float angle) {
    if (!gradient) return;
    gradient->angle = angle;
}

void ir_gradient_set_center(IRGradient* gradient, float x, float y) {
    if (!gradient) return;
    gradient->center_x = x;
    gradient->center_y = y;
}

// ============================================================================
// Gradient to Color Conversion
// ============================================================================

IRColor ir_color_from_gradient(IRGradient* gradient) {
    IRColor color;
    color.type = IR_COLOR_GRADIENT;
    color.data.gradient = gradient;
    return color;
}

// ============================================================================
// Style Gradient Setters
// ============================================================================

void ir_set_background_gradient(IRStyle* style, IRGradient* gradient) {
    if (!style || !gradient) return;
    style->background = ir_color_from_gradient(gradient);
}
