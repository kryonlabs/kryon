// desktop_renderer_common.c
//
// Common desktop renderer implementation
// Shared command execution logic for all desktop backends
//

#include "desktop_renderer_common.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Common Command Execution
// ============================================================================

void desktop_execute_commands_common(kryon_renderer_t* renderer,
                                     kryon_cmd_buf_t* buf,
                                     const DesktopBackendDrawFunctions* draw_funcs,
                                     void* user_data) {
    if (!renderer || !buf || !draw_funcs) {
        return;
    }

    // Check if we have commands to execute
    int cmd_count = kryon_cmd_buf_count(buf);
    if (cmd_count == 0) {
        return;
    }

    // Execute all commands in the buffer
    kryon_cmd_iterator_t iter = kryon_cmd_iter_create(buf);
    kryon_command_t cmd;

    int executed_count = 0;

    while (kryon_cmd_iter_has_next(&iter)) {
        executed_count++;
        if (kryon_cmd_iter_next(&iter, &cmd)) {
            switch (cmd.type) {
                case KRYON_CMD_DRAW_RECT: {
                    if (draw_funcs->draw_rect) {
                        draw_funcs->draw_rect(
                            cmd.data.draw_rect.x,
                            cmd.data.draw_rect.y,
                            cmd.data.draw_rect.w,
                            cmd.data.draw_rect.h,
                            cmd.data.draw_rect.color
                        );
                    }
                    break;
                }

                case KRYON_CMD_DRAW_TEXT: {
                    if (draw_funcs->draw_text) {
                        draw_funcs->draw_text(
                            cmd.data.draw_text.text,
                            cmd.data.draw_text.x,
                            cmd.data.draw_text.y,
                            cmd.data.draw_text.font_size,
                            cmd.data.draw_text.color
                        );
                    }
                    break;
                }

                case KRYON_CMD_DRAW_LINE: {
                    if (draw_funcs->draw_line) {
                        draw_funcs->draw_line(
                            cmd.data.draw_line.x1,
                            cmd.data.draw_line.y1,
                            cmd.data.draw_line.x2,
                            cmd.data.draw_line.y2,
                            cmd.data.draw_line.color
                        );
                    }
                    break;
                }

                case KRYON_CMD_DRAW_ARC: {
                    if (draw_funcs->draw_circle) {
                        draw_funcs->draw_circle(
                            cmd.data.draw_arc.cx,
                            cmd.data.draw_arc.cy,
                            cmd.data.draw_arc.radius,
                            cmd.data.draw_arc.color
                        );
                    }
                    break;
                }

                case KRYON_CMD_SET_CLIP: {
                    if (draw_funcs->set_clip) {
                        draw_funcs->set_clip(
                            cmd.data.set_clip.x,
                            cmd.data.set_clip.y,
                            cmd.data.set_clip.w,
                            cmd.data.set_clip.h
                        );
                    }
                    break;
                }

                case KRYON_CMD_PUSH_CLIP: {
                    if (draw_funcs->push_clip) {
                        draw_funcs->push_clip();
                    }
                    break;
                }

                case KRYON_CMD_POP_CLIP: {
                    if (draw_funcs->pop_clip) {
                        draw_funcs->pop_clip();
                    }
                    break;
                }

                case KRYON_CMD_DRAW_ROUNDED_RECT: {
                    if (draw_funcs->draw_rounded_rect) {
                        draw_funcs->draw_rounded_rect(
                            cmd.data.draw_rounded_rect.x,
                            cmd.data.draw_rounded_rect.y,
                            cmd.data.draw_rounded_rect.w,
                            cmd.data.draw_rounded_rect.h,
                            cmd.data.draw_rounded_rect.radius,
                            cmd.data.draw_rounded_rect.color
                        );
                    }
                    break;
                }

                case KRYON_CMD_DRAW_POLYGON: {
                    if (draw_funcs->draw_polygon) {
                        draw_funcs->draw_polygon(
                            cmd.data.draw_polygon.vertices,
                            cmd.data.draw_polygon.vertex_count,
                            cmd.data.draw_polygon.color,
                            cmd.data.draw_polygon.filled
                        );
                    }
                    break;
                }

                case KRYON_CMD_DRAW_IMAGE: {
                    if (draw_funcs->draw_image) {
                        draw_funcs->draw_image(
                            cmd.data.draw_image.image_data,
                            cmd.data.draw_image.x,
                            cmd.data.draw_image.y,
                            cmd.data.draw_image.w,
                            cmd.data.draw_image.h,
                            cmd.data.draw_image.tint
                        );
                    }
                    break;
                }

                default:
                    fprintf(stderr, "[desktop_common] Warning: Unknown command type %d\n", cmd.type);
                    break;
            }
        }
    }
}

// ============================================================================
// Clip Stack Management
// ============================================================================

void desktop_clip_stack_init(DesktopClipStack* stack) {
    if (!stack) return;

    stack->depth = 0;
    stack->current = (DesktopClipRect){0, 0, 0, 0};

    for (int i = 0; i < 8; i++) {
        stack->clips[i] = (DesktopClipRect){0, 0, 0, 0};
    }
}

void desktop_clip_stack_push(DesktopClipStack* stack, float x, float y, float w, float h) {
    if (!stack) return;

    if (stack->depth < 8) {
        stack->clips[stack->depth] = (DesktopClipRect){x, y, w, h};
        stack->depth++;
        stack->current = (DesktopClipRect){x, y, w, h};
    } else {
        fprintf(stderr, "[desktop_common] Warning: Clip stack overflow (max 8 levels)\n");
    }
}

void desktop_clip_stack_pop(DesktopClipStack* stack) {
    if (!stack) return;

    if (stack->depth > 0) {
        stack->depth--;
        if (stack->depth > 0) {
            // Restore previous clip
            stack->current = stack->clips[stack->depth - 1];
        } else {
            // No more clips
            stack->current = (DesktopClipRect){0, 0, 0, 0};
        }
    } else {
        fprintf(stderr, "[desktop_common] Warning: Clip stack underflow\n");
    }
}

bool desktop_clip_stack_get_current(const DesktopClipStack* stack, DesktopClipRect* out) {
    if (!stack || !out) return false;

    if (stack->depth > 0) {
        *out = stack->current;
        return true;
    }

    return false;
}

// ============================================================================
// Color Conversion Helpers
// ============================================================================

void desktop_color_to_floats(uint32_t rgba, float* out) {
    if (!out) return;

    out[0] = desktop_color_r(rgba) / 255.0f;
    out[1] = desktop_color_g(rgba) / 255.0f;
    out[2] = desktop_color_b(rgba) / 255.0f;
    out[3] = desktop_color_a(rgba) / 255.0f;
}
