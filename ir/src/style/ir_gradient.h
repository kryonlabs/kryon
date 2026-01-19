#ifndef IR_GRADIENT_H
#define IR_GRADIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "../include/ir_core.h"

// ============================================================================
// Gradient Creation
// ============================================================================

IRGradient* ir_gradient_create_linear(float angle);
IRGradient* ir_gradient_create_radial(float center_x, float center_y);
IRGradient* ir_gradient_create_conic(float center_x, float center_y);

// Unified gradient creation
IRGradient* ir_gradient_create(IRGradientType type);

void ir_gradient_destroy(IRGradient* gradient);

// ============================================================================
// Gradient Configuration
// ============================================================================

void ir_gradient_add_stop(IRGradient* gradient, float position, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_gradient_set_angle(IRGradient* gradient, float angle);
void ir_gradient_set_center(IRGradient* gradient, float x, float y);

// ============================================================================
// Gradient to Color Conversion
// ============================================================================

IRColor ir_color_from_gradient(IRGradient* gradient);

// ============================================================================
// Style Gradient Setters
// ============================================================================

void ir_set_background_gradient(IRStyle* style, IRGradient* gradient);

#endif // IR_GRADIENT_H
