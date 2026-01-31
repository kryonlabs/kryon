/**
 * Kryon IR - Layout Helper Utilities Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "layout_helpers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// External text measurement function (from ir_layout.c)
extern float ir_get_text_width_estimate(const char* text, float font_size);

// ============================================================================
// Layout State Management
// ============================================================================

IRLayoutState* layout_ensure_state(IRComponent* c) {
    if (!c) return NULL;

    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
        if (!c->layout_state) return NULL;
        layout_state_init(c->layout_state);
    }

    return c->layout_state;
}

void layout_state_init(IRLayoutState* state) {
    if (!state) return;

    memset(state, 0, sizeof(IRLayoutState));
    state->layout_valid = false;
    state->computed.valid = false;
}

void layout_mark_valid(IRLayoutState* state) {
    if (!state) return;

    state->layout_valid = true;
    state->computed.valid = true;
}

// ============================================================================
// Style Access Helpers
// ============================================================================

float layout_get_font_size(IRComponent* c, float default_size) {
    if (!c || !c->style) return default_size;

    float size = c->style->font.size;
    return (size > 0) ? size : default_size;
}

IRSpacing layout_get_padding(IRComponent* c) {
    IRSpacing padding = {0};

    if (c && c->style) {
        padding = c->style->padding;
    }

    return padding;
}

IRSpacing layout_get_margin(IRComponent* c) {
    IRSpacing margin = {0};

    if (c && c->style) {
        margin = c->style->margin;
    }

    return margin;
}

bool layout_get_explicit_width(IRComponent* c, float* out_width) {
    if (!c || !c->style || !out_width) return false;

    if (c->style->width.type == IR_DIMENSION_PX) {
        *out_width = c->style->width.value;
        return true;
    }

    return false;
}

bool layout_get_explicit_height(IRComponent* c, float* out_height) {
    if (!c || !c->style || !out_height) return false;

    if (c->style->height.type == IR_DIMENSION_PX) {
        *out_height = c->style->height.value;
        return true;
    }

    return false;
}

// ============================================================================
// Constraint Application
// ============================================================================

void layout_apply_constraints(float* width, float* height,
                               IRLayoutConstraints constraints) {
    layout_apply_min_constraints(width, height, constraints);
    layout_apply_max_constraints(width, height, constraints);
}

void layout_apply_min_constraints(float* width, float* height,
                                   IRLayoutConstraints constraints) {
    if (width) {
        *width = fmaxf(*width, constraints.min_width);
    }
    if (height) {
        *height = fmaxf(*height, constraints.min_height);
    }
}

void layout_apply_max_constraints(float* width, float* height,
                                   IRLayoutConstraints constraints) {
    if (width && constraints.max_width > 0) {
        *width = fminf(*width, constraints.max_width);
    }
    if (height && constraints.max_height > 0) {
        *height = fminf(*height, constraints.max_height);
    }
}

// ============================================================================
// Final Layout Computation
// ============================================================================

void layout_set_final(IRComponent* c, float x, float y, float width, float height) {
    if (!c || !c->layout_state) return;

    c->layout_state->computed.x = x;
    c->layout_state->computed.y = y;
    c->layout_state->computed.width = width;
    c->layout_state->computed.height = height;

    layout_mark_valid(c->layout_state);
}

void layout_set_final_with_parent(IRComponent* c, float parent_x, float parent_y,
                                  float width, float height) {
    if (!c || !c->layout_state) return;

    c->layout_state->computed.x = parent_x;
    c->layout_state->computed.y = parent_y;
    c->layout_state->computed.width = width;
    c->layout_state->computed.height = height;

    layout_mark_valid(c->layout_state);
}

void layout_set_final_dimensions(IRComponent* c, float width, float height) {
    if (!c || !c->layout_state) return;

    c->layout_state->computed.width = width;
    c->layout_state->computed.height = height;
}

// ============================================================================
// Debug Logging
// ============================================================================

bool layout_is_debug_enabled(const char* component_name) {
    if (!component_name) return false;

    char env_var[64];
    snprintf(env_var, sizeof(env_var), "KRYON_DEBUG_%s", component_name);

    // Convert to uppercase
    for (char* p = env_var; *p; p++) {
        if (*p >= 'a' && *p <= 'z') {
            *p = *p - 'a' + 'A';
        }
    }

    return getenv(env_var) != NULL;
}

void layout_debug_log(const char* component_name, IRComponent* c, const char* extra_info) {
    if (!component_name || !c || !c->layout_state) return;

    if (layout_is_debug_enabled(component_name)) {
        char env_var[64];
        snprintf(env_var, sizeof(env_var), "KRYON_DEBUG_%s", component_name);

        // Convert to uppercase
        for (char* p = env_var; *p; p++) {
            if (*p >= 'a' && *p <= 'z') {
                *p = *p - 'a' + 'A';
            }
        }

        if (getenv(env_var)) {
            fprintf(stderr, "[%s] Final: x=%.1f, y=%.1f, w=%.1f, h=%.1f%s%s\n",
                    component_name,
                    c->layout_state->computed.x,
                    c->layout_state->computed.y,
                    c->layout_state->computed.width,
                    c->layout_state->computed.height,
                    extra_info ? " " : "",
                    extra_info ? extra_info : "");
        }
    }
}

// ============================================================================
// Intrinsic Size Computation Helpers
// ============================================================================

void layout_compute_text_size(const char* text, float font_size,
                               IRSpacing padding, float min_width,
                               float* out_width, float* out_height) {
    if (!out_width || !out_height) return;

    float text_width = 0;
    float text_height = font_size * 1.2f; // Default line height

    if (text) {
        text_width = ir_get_text_width_estimate(text, font_size);
    }

    *out_width = text_width + padding.left + padding.right;
    *out_height = text_height + padding.top + padding.bottom;

    // Apply minimum width
    if (min_width > 0 && *out_width < min_width) {
        *out_width = min_width;
    }
}

void layout_compute_with_explicit(IRComponent* c,
                                  float intrinsic_width, float intrinsic_height,
                                  float* out_width, float* out_height) {
    if (!out_width || !out_height) return;

    // Start with intrinsic dimensions
    *out_width = intrinsic_width;
    *out_height = intrinsic_height;

    // Override with explicit dimensions if set
    float explicit_width;
    if (layout_get_explicit_width(c, &explicit_width)) {
        *out_width = explicit_width;
    }

    float explicit_height;
    if (layout_get_explicit_height(c, &explicit_height)) {
        *out_height = explicit_height;
    }
}

void layout_compute_full_pipeline(IRComponent* c,
                                   IRLayoutConstraints constraints,
                                   float parent_x, float parent_y,
                                   float intrinsic_width,
                                   float intrinsic_height) {
    if (!c) return;

    // Ensure layout state exists
    if (!layout_ensure_state(c)) return;

    // Get font size and padding
    float font_size = layout_get_font_size(c, LAYOUT_DEFAULT_FONT_SIZE);
    IRSpacing padding = layout_get_padding(c);
    (void)font_size; // May be used by caller
    (void)padding;   // May be used by caller

    // Compute dimensions with explicit values taking precedence
    float final_width, final_height;
    layout_compute_with_explicit(c, intrinsic_width, intrinsic_height,
                                &final_width, &final_height);

    // Apply constraints
    layout_apply_constraints(&final_width, &final_height, constraints);

    // Set final layout
    layout_set_final_with_parent(c, parent_x, parent_y, final_width, final_height);
}
