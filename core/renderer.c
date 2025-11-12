#include "include/kryon.h"
#include "include/kryon_canvas.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static bool kryon_core_trace_enabled(void) {
    static bool initialized = false;
    static bool enabled = false;

    if (!initialized) {
        initialized = true;
#if defined(__unix__) || defined(__APPLE__) || defined(_WIN32)
        const char* env = getenv("KRYON_TRACE_COMMANDS");
        enabled = (env != NULL && env[0] != '\0' && env[0] != '0');
#endif
    }

    return enabled;
}

// ============================================================================
// Renderer Management
// ============================================================================

kryon_renderer_t* kryon_renderer_create(const kryon_renderer_ops_t* ops) {
    if (ops == NULL) {
        return NULL;
    }

#if KRYON_NO_HEAP
    // For MCU, use static allocation or require caller to provide memory
    static kryon_renderer_t static_renderer;
    kryon_renderer_t* renderer = &static_renderer;
    memset(renderer, 0, sizeof(kryon_renderer_t));
#else
    // For desktop, use dynamic allocation
    kryon_renderer_t* renderer = (kryon_renderer_t*)malloc(sizeof(kryon_renderer_t));
    if (renderer == NULL) {
        return NULL;
    }
    memset(renderer, 0, sizeof(kryon_renderer_t));
#endif

    renderer->ops = ops;
    renderer->clear_color = kryon_color_rgb(0, 0, 0); // Black default
    renderer->vsync_enabled = true;

    return renderer;
}

void kryon_renderer_destroy(kryon_renderer_t* renderer) {
    if (renderer == NULL) {
        return;
    }

    // Shutdown renderer first
    kryon_renderer_shutdown(renderer);

#if !KRYON_NO_HEAP
    // Only free on desktop platforms
    free(renderer);
#endif
}

bool kryon_renderer_init(kryon_renderer_t* renderer, void* native_window) {
    if (renderer == NULL || renderer->ops == NULL) {
        return false;
    }

    if (renderer->ops->init == NULL) {
        return false;
    }

    return renderer->ops->init(renderer, native_window);
}

void kryon_renderer_shutdown(kryon_renderer_t* renderer) {
    if (renderer == NULL || renderer->ops == NULL) {
        return;
    }

    if (renderer->ops->shutdown != NULL) {
        renderer->ops->shutdown(renderer);
    }
}

void kryon_renderer_get_dimensions(kryon_renderer_t* renderer, uint16_t* width, uint16_t* height) {
    if (renderer == NULL) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }

    if (width) *width = renderer->width;
    if (height) *height = renderer->height;

    // If backend provides dynamic dimensions, use those
    if (renderer->ops && renderer->ops->get_dimensions) {
        renderer->ops->get_dimensions(renderer, width, height);
        if (width) renderer->width = *width;
        if (height) renderer->height = *height;
    }
}

void kryon_renderer_set_clear_color(kryon_renderer_t* renderer, uint32_t color) {
    if (renderer == NULL) {
        return;
    }
    renderer->clear_color = color;
}

void kryon_renderer_set_vsync(kryon_renderer_t* renderer, bool enabled) {
    if (renderer == NULL) {
        return;
    }
    renderer->vsync_enabled = enabled;
}

// ============================================================================
// Main Rendering Loop
// ============================================================================

void kryon_render_frame(kryon_renderer_t* renderer, kryon_component_t* root_component) {
    if (renderer == NULL || root_component == NULL) {
        return;
    }

    bool trace_render = false;
#if defined(__unix__) || defined(__APPLE__) || defined(_WIN32)
    {
        static bool init = false;
        static bool enabled = false;
        if (!init) {
            init = true;
            const char* env = getenv("KRYON_TRACE_RENDER");
            enabled = (env != NULL && env[0] != '\0' && env[0] != '0');
        }
        trace_render = enabled;
    }
#endif

    if (trace_render) {
        fprintf(stderr, "[kryon][render] begin\n");
    }

    // Begin frame
    if (renderer->ops && renderer->ops->begin_frame) {
        renderer->ops->begin_frame(renderer);
    }

    // Create command buffer (reverting to local allocation to restore basic rendering)
    kryon_cmd_buf_t* cmd_buf = (kryon_cmd_buf_t*)malloc(sizeof(kryon_cmd_buf_t));
    if (cmd_buf == NULL) {
        return;
    }
    memset(cmd_buf, 0, sizeof(kryon_cmd_buf_t));
    kryon_cmd_buf_init(cmd_buf);

    // Render component tree
    if (root_component->ops && root_component->ops->render) {
        root_component->ops->render(root_component, cmd_buf);
    }

    // Execute commands
    if (renderer->ops && renderer->ops->execute_commands) {
        renderer->ops->execute_commands(renderer, cmd_buf);
    }

    // End frame
    if (renderer->ops && renderer->ops->end_frame) {
        renderer->ops->end_frame(renderer);
    }

    // Swap buffers
    if (renderer->ops && renderer->ops->swap_buffers) {
        renderer->ops->swap_buffers(renderer);
    }

    if (trace_render) {
        fprintf(stderr, "[kryon][render] end\n");
    }
    free(cmd_buf);
}

// ============================================================================
// Command Processing Utilities
// ============================================================================

void kryon_execute_commands(kryon_cmd_buf_t* buf, const kryon_command_executor_t* executor) {
    if (buf == NULL || executor == NULL) {
        return;
    }

    kryon_cmd_iterator_t iter = kryon_cmd_iter_create(buf);
    kryon_command_t cmd;

    while (kryon_cmd_iter_has_next(&iter)) {
        if (kryon_cmd_iter_next(&iter, &cmd)) {
            if (kryon_core_trace_enabled()) {
                fprintf(stderr, "[kryon][core] cmd=%d\n", cmd.type);
            }
            switch (cmd.type) {
                case KRYON_CMD_DRAW_RECT:
                    if (kryon_core_trace_enabled()) {
                        fprintf(stderr,
                                "[kryon][core] rect x=%d y=%d w=%u h=%u color=%08X\n",
                                cmd.data.draw_rect.x,
                                cmd.data.draw_rect.y,
                                cmd.data.draw_rect.w,
                                cmd.data.draw_rect.h,
                                cmd.data.draw_rect.color);
                    }
                    if (executor->execute_draw_rect) {
                        executor->execute_draw_rect(
                            cmd.data.draw_rect.x, cmd.data.draw_rect.y,
                            cmd.data.draw_rect.w, cmd.data.draw_rect.h,
                            cmd.data.draw_rect.color
                        );
                    }
                    break;

                case KRYON_CMD_DRAW_TEXT:
                    if (kryon_core_trace_enabled()) {
                        fprintf(stderr,
                                "[kryon][core] text x=%d y=%d font=%u color=%08X ptr=%p\n",
                                cmd.data.draw_text.x,
                                cmd.data.draw_text.y,
                                cmd.data.draw_text.font_id,
                                cmd.data.draw_text.color,
                                (void*)cmd.data.draw_text.text);
                    }
                    if (executor->execute_draw_text) {
                        executor->execute_draw_text(
                            cmd.data.draw_text.text, cmd.data.draw_text.x, cmd.data.draw_text.y,
                            cmd.data.draw_text.font_id, cmd.data.draw_text.color
                        );
                    }
                    break;

                case KRYON_CMD_DRAW_LINE:
                    if (executor->execute_draw_line) {
                        executor->execute_draw_line(
                            cmd.data.draw_line.x1, cmd.data.draw_line.y1,
                            cmd.data.draw_line.x2, cmd.data.draw_line.y2,
                            cmd.data.draw_line.color
                        );
                    }
                    break;

                case KRYON_CMD_DRAW_ARC:
                    if (executor->execute_draw_arc) {
                        executor->execute_draw_arc(
                            cmd.data.draw_arc.cx, cmd.data.draw_arc.cy,
                            cmd.data.draw_arc.radius,
                            cmd.data.draw_arc.start_angle, cmd.data.draw_arc.end_angle,
                            cmd.data.draw_arc.color
                        );
                    }
                    break;

                case KRYON_CMD_DRAW_TEXTURE:
                    if (executor->execute_draw_texture) {
                        executor->execute_draw_texture(
                            cmd.data.draw_texture.texture_id,
                            cmd.data.draw_texture.x, cmd.data.draw_texture.y
                        );
                    }
                    break;

                case KRYON_CMD_SET_CLIP:
                    if (executor->execute_set_clip) {
                        executor->execute_set_clip(
                            cmd.data.set_clip.x, cmd.data.set_clip.y,
                            cmd.data.set_clip.w, cmd.data.set_clip.h
                        );
                    }
                    break;

                case KRYON_CMD_PUSH_CLIP:
                    if (executor->execute_push_clip) {
                        executor->execute_push_clip();
                    }
                    break;

                case KRYON_CMD_POP_CLIP:
                    if (executor->execute_pop_clip) {
                        executor->execute_pop_clip();
                    }
                    break;

                case KRYON_CMD_SET_TRANSFORM:
                    if (executor->execute_set_transform) {
                        executor->execute_set_transform(cmd.data.set_transform.matrix);
                    }
                    break;

                case KRYON_CMD_PUSH_TRANSFORM:
                    if (executor->execute_push_transform) {
                        executor->execute_push_transform();
                    }
                    break;

                case KRYON_CMD_POP_TRANSFORM:
                    if (executor->execute_pop_transform) {
                        executor->execute_pop_transform();
                    }
                    break;

                default:
                    // Unknown command - skip
                    break;
            }
        }
    }
}

// ============================================================================
// Transform Stack for Renderers
// ============================================================================

#define KRYON_MAX_TRANSFORM_STACK 16

typedef struct {
    kryon_fp_t stack[KRYON_MAX_TRANSFORM_STACK][6];
    uint8_t top;
    kryon_fp_t current[6];
} kryon_transform_stack_t;

void kryon_transform_stack_init(kryon_transform_stack_t* stack) {
    if (stack == NULL) return;

    // Initialize with identity matrix
    stack->top = 0;
    stack->current[0] = KRYON_FP_FROM_INT(1); // a
    stack->current[1] = KRYON_FP_FROM_INT(0); // b
    stack->current[2] = KRYON_FP_FROM_INT(0); // c
    stack->current[3] = KRYON_FP_FROM_INT(1); // d
    stack->current[4] = KRYON_FP_FROM_INT(0); // tx
    stack->current[5] = KRYON_FP_FROM_INT(0); // ty

    // Copy to bottom of stack
    kryon_memcpy(stack->stack[0], stack->current, sizeof(stack->current));
}

void kryon_transform_stack_push(kryon_transform_stack_t* stack) {
    if (stack == NULL || stack->top >= KRYON_MAX_TRANSFORM_STACK - 1) {
        return; // Stack overflow
    }

    stack->top++;
    kryon_memcpy(stack->stack[stack->top], stack->current, sizeof(stack->current));
}

void kryon_transform_stack_pop(kryon_transform_stack_t* stack) {
    if (stack == NULL || stack->top == 0) {
        return; // Stack underflow
    }

    stack->top--;
    kryon_memcpy(stack->current, stack->stack[stack->top], sizeof(stack->current));
}

void kryon_transform_stack_set(kryon_transform_stack_t* stack, const kryon_fp_t matrix[6]) {
    if (stack == NULL || matrix == NULL) return;

    kryon_memcpy(stack->current, matrix, sizeof(stack->current));
}

void kryon_transform_stack_get(kryon_transform_stack_t* stack, kryon_fp_t matrix[6]) {
    if (stack == NULL || matrix == NULL) return;

    kryon_memcpy(matrix, stack->current, sizeof(stack->current));
}

void kryon_transform_stack_multiply(kryon_transform_stack_t* stack, const kryon_fp_t matrix[6]) {
    if (stack == NULL || matrix == NULL) return;

    // Matrix multiplication: current = current * matrix
    kryon_fp_t result[6];
    result[0] = KRYON_FP_MUL(stack->current[0], matrix[0]) + KRYON_FP_MUL(stack->current[2], matrix[1]);
    result[1] = KRYON_FP_MUL(stack->current[1], matrix[0]) + KRYON_FP_MUL(stack->current[3], matrix[1]);
    result[2] = KRYON_FP_MUL(stack->current[0], matrix[2]) + KRYON_FP_MUL(stack->current[2], matrix[3]);
    result[3] = KRYON_FP_MUL(stack->current[1], matrix[2]) + KRYON_FP_MUL(stack->current[3], matrix[3]);
    result[4] = KRYON_FP_MUL(stack->current[0], matrix[4]) + KRYON_FP_MUL(stack->current[2], matrix[5]) + stack->current[4];
    result[5] = KRYON_FP_MUL(stack->current[1], matrix[4]) + KRYON_FP_MUL(stack->current[3], matrix[5]) + stack->current[5];

    kryon_memcpy(stack->current, result, sizeof(result));
}

void kryon_transform_stack_translate(kryon_transform_stack_t* stack, kryon_fp_t x, kryon_fp_t y) {
    if (stack == NULL) return;

    kryon_fp_t translate_matrix[6] = {
        KRYON_FP_FROM_INT(1), KRYON_FP_FROM_INT(0),
        KRYON_FP_FROM_INT(0), KRYON_FP_FROM_INT(1),
        x, y
    };

    kryon_transform_stack_multiply(stack, translate_matrix);
}

void kryon_transform_stack_scale(kryon_transform_stack_t* stack, kryon_fp_t sx, kryon_fp_t sy) {
    if (stack == NULL) return;

    kryon_fp_t scale_matrix[6] = {
        sx, KRYON_FP_FROM_INT(0),
        KRYON_FP_FROM_INT(0), sy,
        KRYON_FP_FROM_INT(0), KRYON_FP_FROM_INT(0)
    };

    kryon_transform_stack_multiply(stack, scale_matrix);
}

void kryon_transform_stack_rotate(kryon_transform_stack_t* stack, kryon_fp_t angle) {
    if (stack == NULL) return;

#if !KRYON_NO_FLOAT
    float angle_rad = kryon_fp_to_float(angle) * 3.14159f / 180.0f;
    float cos_a = cosf(angle_rad);
    float sin_a = sinf(angle_rad);

    kryon_fp_t rotate_matrix[6] = {
        kryon_fp_from_float(cos_a), kryon_fp_from_float(sin_a),
        kryon_fp_from_float(-sin_a), kryon_fp_from_float(cos_a),
        KRYON_FP_FROM_INT(0), KRYON_FP_FROM_INT(0)
    };
#else
    // For MCU, use integer approximation or lookup table
    // Simplified rotation for common angles
    int32_t angle_int = KRYON_FP_TO_INT(angle);
    kryon_fp_t rotate_matrix[6] = {
        KRYON_FP_FROM_INT(1), KRYON_FP_FROM_INT(0),
        KRYON_FP_FROM_INT(0), KRYON_FP_FROM_INT(1),
        KRYON_FP_FROM_INT(0), KRYON_FP_FROM_INT(0)
    };

    // Handle 90-degree rotations
    if (angle_int % 90 == 0) {
        int32_t quadrant = (angle_int / 90) % 4;
        switch (quadrant) {
            case 0: // 0 degrees
                // Identity matrix already set
                break;
            case 1: // 90 degrees
                rotate_matrix[0] = KRYON_FP_FROM_INT(0);
                rotate_matrix[1] = KRYON_FP_FROM_INT(1);
                rotate_matrix[2] = KRYON_FP_FROM_INT(-1);
                rotate_matrix[3] = KRYON_FP_FROM_INT(0);
                break;
            case 2: // 180 degrees
                rotate_matrix[0] = KRYON_FP_FROM_INT(-1);
                rotate_matrix[1] = KRYON_FP_FROM_INT(0);
                rotate_matrix[2] = KRYON_FP_FROM_INT(0);
                rotate_matrix[3] = KRYON_FP_FROM_INT(-1);
                break;
            case 3: // 270 degrees
                rotate_matrix[0] = KRYON_FP_FROM_INT(0);
                rotate_matrix[1] = KRYON_FP_FROM_INT(-1);
                rotate_matrix[2] = KRYON_FP_FROM_INT(1);
                rotate_matrix[3] = KRYON_FP_FROM_INT(0);
                break;
        }
    }
#endif

    kryon_transform_stack_multiply(stack, rotate_matrix);
}

void kryon_transform_stack_transform_point(kryon_transform_stack_t* stack, kryon_fp_t* x, kryon_fp_t* y) {
    if (stack == NULL || x == NULL || y == NULL) return;

    kryon_fp_t new_x = KRYON_FP_MUL(stack->current[0], *x) + KRYON_FP_MUL(stack->current[2], *y) + stack->current[4];
    kryon_fp_t new_y = KRYON_FP_MUL(stack->current[1], *x) + KRYON_FP_MUL(stack->current[3], *y) + stack->current[5];

    *x = new_x;
    *y = new_y;
}

// ============================================================================
// Clip Stack for Renderers
// ============================================================================

#define KRYON_MAX_CLIP_STACK 8

typedef struct {
    int16_t x, y, w, h;
} kryon_clip_rect_t;

typedef struct {
    kryon_clip_rect_t stack[KRYON_MAX_CLIP_STACK];
    uint8_t top;
    kryon_clip_rect_t current;
} kryon_clip_stack_t;

void kryon_clip_stack_init(kryon_clip_stack_t* stack, uint16_t width, uint16_t height) {
    if (stack == NULL) return;

    stack->top = 0;
    stack->current.x = 0;
    stack->current.y = 0;
    stack->current.w = width;
    stack->current.h = height;
    stack->stack[0] = stack->current;
}

void kryon_clip_stack_push(kryon_clip_stack_t* stack) {
    if (stack == NULL || stack->top >= KRYON_MAX_CLIP_STACK - 1) {
        return; // Stack overflow
    }

    stack->top++;
    stack->stack[stack->top] = stack->current;
}

void kryon_clip_stack_pop(kryon_clip_stack_t* stack) {
    if (stack == NULL || stack->top == 0) {
        return; // Stack underflow
    }

    stack->top--;
    stack->current = stack->stack[stack->top];
}

void kryon_clip_stack_intersect(kryon_clip_stack_t* stack, int16_t x, int16_t y, uint16_t w, uint16_t h) {
    if (stack == NULL) return;

    // Calculate intersection with current clip
    int16_t current_right = stack->current.x + stack->current.w;
    int16_t current_bottom = stack->current.y + stack->current.h;
    int16_t new_right = x + w;
    int16_t new_bottom = y + h;

    int16_t intersect_left = (stack->current.x > x) ? stack->current.x : x;
    int16_t intersect_top = (stack->current.y > y) ? stack->current.y : y;
    int16_t intersect_right = (current_right < new_right) ? current_right : new_right;
    int16_t intersect_bottom = (current_bottom < new_bottom) ? current_bottom : new_bottom;

    // Update current clip
    stack->current.x = intersect_left;
    stack->current.y = intersect_top;
    stack->current.w = (intersect_right > intersect_left) ? (intersect_right - intersect_left) : 0;
    stack->current.h = (intersect_bottom > intersect_top) ? (intersect_bottom - intersect_top) : 0;
}

void kryon_clip_stack_get(kryon_clip_stack_t* stack, int16_t* x, int16_t* y, uint16_t* w, uint16_t* h) {
    if (stack == NULL) {
        if (x) *x = 0;
        if (y) *y = 0;
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }

    if (x) *x = stack->current.x;
    if (y) *y = stack->current.y;
    if (w) *w = stack->current.w;
    if (h) *h = stack->current.h;
}

bool kryon_clip_stack_is_point_clipped(kryon_clip_stack_t* stack, int16_t x, int16_t y) {
    if (stack == NULL) return true;

    return !(x >= stack->current.x && x < stack->current.x + stack->current.w &&
             y >= stack->current.y && y < stack->current.y + stack->current.h);
}
