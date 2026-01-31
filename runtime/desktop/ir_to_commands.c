/*
 * ============================================================================
 * IR Component to Command Buffer Translation
 * ============================================================================
 *
 * This module translates IR components into command buffer commands for
 * renderer-agnostic rendering. It handles component tree traversal, opacity
 * cascading, transform accumulation, z-index sorting, and multi-pass
 * rendering for overlays.
 */

#define _POSIX_C_SOURCE 200809L

#include "ir_to_commands.h"
#include "ir_wireframe.h"
#include "desktop_internal.h"
#include "../../ir/include/ir_core.h"
#include "../../ir/include/ir_executor.h"
#include "../../ir/include/ir_style_vars.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
bool ir_generate_component_commands(IRComponent* component, IRCommandContext* ctx,
                                     LayoutRect* bounds, float inherited_opacity);
bool ir_gen_tab_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Resolve IRColor to RGBA components.
 * Returns true if color was successfully resolved to solid RGBA.
 * Returns false for gradients or unresolved variable references.
 * NOTE: This function is now defined in ir/src/style/ir_style_vars.c
 */

uint32_t ir_apply_opacity_to_color(uint32_t color, float opacity) {
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;

    a = (uint8_t)(a * opacity);

    return (r << 24) | (g << 16) | (b << 8) | a;
}

void ir_push_opacity(IRCommandContext* ctx, float opacity) {
    if (ctx->opacity_depth >= 16) {
        fprintf(stderr, "Warning: Opacity stack overflow\n");
        return;
    }

    ctx->opacity_stack[ctx->opacity_depth++] = ctx->current_opacity;
    ctx->current_opacity *= opacity;

    kryon_command_t cmd = {
        .type = KRYON_CMD_SET_OPACITY,
        .data.set_opacity = { .opacity = ctx->current_opacity }
    };
    kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
}

void ir_pop_opacity(IRCommandContext* ctx) {
    if (ctx->opacity_depth <= 0) {
        fprintf(stderr, "Warning: Opacity stack underflow\n");
        return;
    }

    ctx->current_opacity = ctx->opacity_stack[--ctx->opacity_depth];

    kryon_command_t cmd = {
        .type = KRYON_CMD_POP_OPACITY
    };
    kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
}

bool ir_should_defer_to_overlay(IRComponent* comp, IRCommandContext* ctx) {
    /* Never defer if we're already in the overlay pass */
    if (ctx && ctx->current_pass == 1) {
        return false;
    }

    /* Defer open modals to overlay pass */
    if (comp->type == IR_COMPONENT_MODAL) {
        /* Parse modal state from custom_data string: "open|title" or "closed|title" */
        const char* state_str = comp->custom_data;
        if (state_str && strncmp(state_str, "open", 4) == 0) {
            return true;
        }
        return false;
    }

    /* Defer open dropdowns and dragged tabs to overlay pass */
    if (comp->type == IR_COMPONENT_DROPDOWN) {
        /* Check if dropdown is open - defer to overlay if so */
        if (comp->custom_data) {
            IRDropdownState* state = (IRDropdownState*)comp->custom_data;
            if (state->is_open) {
                return true;  /* Defer to overlay pass */
            }
        }
        return false; /* Render inline when closed */
    }

    /* Check for dragged tabs - defer to overlay if any tab is being dragged */
    if (comp->type == IR_COMPONENT_TAB_GROUP || comp->type == IR_COMPONENT_TAB_BAR) {
        /* Check if any tab in this group is being dragged */
        /* For now, defer all tab groups to overlay to handle dragging properly */
        /* TODO: Implement actual drag state checking */
        return true;
    }

    return false;
}

void ir_defer_to_overlay(IRCommandContext* ctx, IRComponent* comp) {
    if (ctx->overlay_count >= 16) {
        fprintf(stderr, "Warning: Overlay buffer overflow\n");
        return;
    }

    ctx->overlays[ctx->overlay_count++] = comp;
}

/* ============================================================================
 * Component-Specific Command Generators
 * ============================================================================ */

bool ir_gen_container_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds) {
    // fprintf(stderr, "[CONTAINER_GEN] Component %u type=%d has_style=%d is_root=%d\n",
    //         comp->id, comp->type, comp->style != NULL, ctx->is_root_component);
    if (!comp->style) {
        // fprintf(stderr, "[CONTAINER_GEN]   No style, returning early\n");
        return true;
    }

    /* Skip root background rendering - SDL_RenderClear already handles it */
    if (ctx->is_root_component) {
        // fprintf(stderr, "[CONTAINER_GEN]   Is root component, skipping background\n");
        return true;
    }

    // fprintf(stderr, "[CONTAINER_GEN]   About to check gradient vs solid\n");

    IRStyle* style = comp->style;
    kryon_command_t cmd = {0};  // CRITICAL FIX: Zero-initialize to prevent stack garbage

    /* Render shadow if present */
    if (style->box_shadow.enabled && !style->box_shadow.inset) {
        /* Convert IRColor to uint32_t */
        uint8_t r, g, b, a;
        uint32_t shadow_color = 0x00000080;  /* Default semi-transparent black */
        if (ir_color_resolve(&style->box_shadow.color, &r, &g, &b, &a)) {
            shadow_color = (r << 24) | (g << 16) | (b << 8) | a;
        }
        shadow_color = ir_apply_opacity_to_color(shadow_color, ctx->current_opacity);

        cmd.type = KRYON_CMD_DRAW_SHADOW;
        cmd.data.draw_shadow.x = bounds->x + style->box_shadow.offset_x;
        cmd.data.draw_shadow.y = bounds->y + style->box_shadow.offset_y;
        cmd.data.draw_shadow.w = bounds->width;
        cmd.data.draw_shadow.h = bounds->height;
        cmd.data.draw_shadow.offset_x = style->box_shadow.offset_x;
        cmd.data.draw_shadow.offset_y = style->box_shadow.offset_y;
        cmd.data.draw_shadow.blur_radius = style->box_shadow.blur_radius;
        cmd.data.draw_shadow.spread_radius = style->box_shadow.spread_radius;
        cmd.data.draw_shadow.color = shadow_color;
        cmd.data.draw_shadow.inset = false;

        kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
    }

    /* Render gradient background if present */
    if (style->background.type == IR_COLOR_GRADIENT && style->background.data.gradient) {
        IRGradient* gradient = style->background.data.gradient;

        cmd.type = KRYON_CMD_DRAW_GRADIENT;
        cmd.data.draw_gradient.x = bounds->x;
        cmd.data.draw_gradient.y = bounds->y;
        cmd.data.draw_gradient.w = bounds->width;
        cmd.data.draw_gradient.h = bounds->height;
        cmd.data.draw_gradient.gradient_type = gradient->type;
        cmd.data.draw_gradient.angle = gradient->angle;
        cmd.data.draw_gradient.stop_count = gradient->stop_count < 8 ? gradient->stop_count : 8;

        for (int i = 0; i < cmd.data.draw_gradient.stop_count; i++) {
            cmd.data.draw_gradient.stops[i].position = gradient->stops[i].position;

            /* Convert gradient stop RGBA to uint32_t */
            uint32_t color = (gradient->stops[i].r << 24) |
                            (gradient->stops[i].g << 16) |
                            (gradient->stops[i].b << 8) |
                            gradient->stops[i].a;
            cmd.data.draw_gradient.stops[i].color = ir_apply_opacity_to_color(color, ctx->current_opacity);
        }

        kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
    }
    /* Render solid color background */
    else {
        uint8_t bg_r, bg_g, bg_b, bg_a;

        // DEBUG: Print background color info
        // fprintf(stderr, "[BG_DEBUG] Component %u type=%d background.type=%d\n",
        //         comp->id, comp->type, style->background.type);

        bool resolved = ir_color_resolve(&style->background, &bg_r, &bg_g, &bg_b, &bg_a);

        // fprintf(stderr, "[BG_DEBUG]   ir_color_resolve returned %d, RGBA=(%u,%u,%u,%u)\n",
        //         resolved, bg_r, bg_g, bg_b, bg_a);

        if (resolved && bg_a > 0) {
        uint32_t bg_color = (bg_r << 24) | (bg_g << 16) | (bg_b << 8) | bg_a;
        bg_color = ir_apply_opacity_to_color(bg_color, ctx->current_opacity);
        if (style->border.radius > 0) {
            cmd.type = KRYON_CMD_DRAW_ROUNDED_RECT;
            cmd.data.draw_rounded_rect.x = bounds->x;
            cmd.data.draw_rounded_rect.y = bounds->y;
            cmd.data.draw_rounded_rect.w = bounds->width;
            cmd.data.draw_rounded_rect.h = bounds->height;
            cmd.data.draw_rounded_rect.radius = style->border.radius;
            cmd.data.draw_rounded_rect.color = bg_color;
        } else {
            cmd.type = KRYON_CMD_DRAW_RECT;
            cmd.data.draw_rect.x = bounds->x;
            cmd.data.draw_rect.y = bounds->y;
            cmd.data.draw_rect.w = bounds->width;
            cmd.data.draw_rect.h = bounds->height;
            cmd.data.draw_rect.color = bg_color;
        }

        kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
        } else {
            // fprintf(stderr, "[BG_DEBUG]   SKIPPING background (resolved=%d, a=%u)\n",
            //         resolved, bg_a);
        }
    }

    /* Render border */
    uint8_t border_r, border_g, border_b, border_a;
    if (style->border.width > 0 && ir_color_resolve(&style->border.color, &border_r, &border_g, &border_b, &border_a) && border_a > 0) {
        uint32_t border_color = (border_r << 24) | (border_g << 16) | (border_b << 8) | border_a;
        border_color = ir_apply_opacity_to_color(border_color, ctx->current_opacity);

        /* Draw border as 4 lines (top, right, bottom, left) */
        for (int i = 0; i < (int)style->border.width; i++) {
            /* Top */
            cmd.type = KRYON_CMD_DRAW_LINE;
            cmd.data.draw_line.x1 = bounds->x + i;
            cmd.data.draw_line.y1 = bounds->y + i;
            cmd.data.draw_line.x2 = bounds->x + bounds->width - i;
            cmd.data.draw_line.y2 = bounds->y + i;
            cmd.data.draw_line.color = border_color;
            kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

            /* Right */
            cmd.data.draw_line.x1 = bounds->x + bounds->width - i - 1;
            cmd.data.draw_line.y1 = bounds->y + i;
            cmd.data.draw_line.x2 = bounds->x + bounds->width - i - 1;
            cmd.data.draw_line.y2 = bounds->y + bounds->height - i;
            kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

            /* Bottom */
            cmd.data.draw_line.x1 = bounds->x + i;
            cmd.data.draw_line.y1 = bounds->y + bounds->height - i - 1;
            cmd.data.draw_line.x2 = bounds->x + bounds->width - i;
            cmd.data.draw_line.y2 = bounds->y + bounds->height - i - 1;
            kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

            /* Left */
            cmd.data.draw_line.x1 = bounds->x + i;
            cmd.data.draw_line.y1 = bounds->y + i;
            cmd.data.draw_line.x2 = bounds->x + i;
            cmd.data.draw_line.y2 = bounds->y + bounds->height - i;
            kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
        }
    }

    return true;
}

bool ir_gen_text_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds) {
    // Evaluate text expression if present, otherwise use text_content
    char* display_text = NULL;
    bool needs_free = false;

    if (comp->text_expression) {
        // Runtime evaluation of reactive text expression
        // Pass component's scope for correct variable lookup
        display_text = ir_executor_eval_text_expression(comp->text_expression, comp->scope);
        if (display_text) {
            needs_free = true;
        } else {
            // Fallback to text_content if expression evaluation fails
            display_text = comp->text_content;
        }
    } else {
        display_text = comp->text_content;
    }

    if (!display_text) {
        return true;
    }

    if (getenv("KRYON_DEBUG_TAB_LAYOUT")) {
        fprintf(stderr, "[TEXT_RENDER] ID=%u text='%s' bounds=(%.1f, %.1f) layout_state=%p\n",
               comp->id, display_text, bounds->x, bounds->y, comp->layout_state);
        if (comp->layout_state) {
            fprintf(stderr, "[TEXT_RENDER]   layout_state->computed=(%.1f, %.1f, %.1f, %.1f) valid=%d\n",
                   comp->layout_state->computed.x, comp->layout_state->computed.y,
                   comp->layout_state->computed.width, comp->layout_state->computed.height,
                   comp->layout_state->computed.valid);
        }
    }

    IRStyle* style = comp->style;

    /* Extract color from IRColor struct and convert to uint32_t */
    uint32_t base_color = 0x000000FF;  /* Default black */
    if (style) {
        uint8_t r, g, b, a;
        if (ir_color_resolve(&style->font.color, &r, &g, &b, &a)) {
            base_color = (r << 24) | (g << 16) | (b << 8) | a;
        }
    }
    uint32_t text_color = ir_apply_opacity_to_color(base_color, ctx->current_opacity);

    /* Apply semantic styling based on component type */
    bool is_bold = style && style->font.bold;
    bool is_italic = style && style->font.italic;
    float font_size = style && style->font.size > 0 ? style->font.size : 16;
    uint32_t bg_color = 0;  /* Background highlight color (for Mark) */

    switch (comp->type) {
        case IR_COMPONENT_STRONG:
            is_bold = true;
            break;
        case IR_COMPONENT_EM:
            is_italic = true;
            break;
        case IR_COMPONENT_CODE_INLINE:
            /* TODO: Use monospace font_id when font system supports it */
            break;
        case IR_COMPONENT_SMALL:
            font_size *= 0.85f;  /* Smaller text */
            break;
        case IR_COMPONENT_MARK:
            bg_color = 0xFFFF00FF;  /* Yellow highlight */
            break;
        default:
            break;
    }

    /* Draw highlight background for Mark component */
    if (bg_color != 0) {
        kryon_command_t bg_cmd = {0};
        bg_cmd.type = KRYON_CMD_DRAW_RECT;
        bg_cmd.data.draw_rect.x = bounds->x;
        bg_cmd.data.draw_rect.y = bounds->y;
        bg_cmd.data.draw_rect.w = bounds->width;
        bg_cmd.data.draw_rect.h = bounds->height;
        bg_cmd.data.draw_rect.color = ir_apply_opacity_to_color(bg_color, ctx->current_opacity);
        kryon_cmd_buf_push(ctx->cmd_buf, &bg_cmd);
    }

    kryon_command_t cmd = {0};  // CRITICAL FIX: Zero-initialize
    cmd.type = KRYON_CMD_DRAW_TEXT;
    cmd.data.draw_text.x = bounds->x;
    cmd.data.draw_text.y = bounds->y;
    cmd.data.draw_text.font_id = 0; /* TODO: Resolve font from style */
    cmd.data.draw_text.font_size = font_size;
    cmd.data.draw_text.font_weight = is_bold ? 1 : 0;
    cmd.data.draw_text.font_style = is_italic ? 1 : 0;
    cmd.data.draw_text.color = text_color;

    /* Copy text to inline storage */
    strncpy(cmd.data.draw_text.text_storage, display_text, 127);
    cmd.data.draw_text.text_storage[127] = '\0';
    cmd.data.draw_text.text = NULL;  /* NULL indicates text is in text_storage */
    cmd.data.draw_text.max_length = strlen(cmd.data.draw_text.text_storage);

    kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

    // Free evaluated text if needed
    if (needs_free) {
        free(display_text);
    }

    return true;
}

bool ir_gen_button_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds) {
    if (getenv("KRYON_DEBUG_BUTTONS")) {
        fprintf(stderr, "[BUTTON] ID=%u text='%s' bounds=(%.0f,%.0f,%.0fx%.0f)\n",
                comp->id,
                comp->text_content ? comp->text_content : "(null)",
                bounds->x, bounds->y, bounds->width, bounds->height);
    }

    /* Check if button is being hovered */
    bool is_hovered = (g_hovered_component == comp);

    /* Disabled buttons don't show hover effects */
    if (comp->is_disabled) {
        is_hovered = false;
    }

    /* Add default border for buttons if not set */
    if (comp->style && comp->style->border.width == 0.0f) {
        comp->style->border.width = 2.0f;  /* 2px for visibility */
        /* Set default border color to bright white if not set or transparent */
        if (comp->style->border.color.type == IR_COLOR_TRANSPARENT ||
            (comp->style->border.color.type == IR_COLOR_SOLID && comp->style->border.color.data.a == 0)) {
            comp->style->border.color.type = IR_COLOR_SOLID;
            comp->style->border.color.data.r = 255;
            comp->style->border.color.data.g = 255;
            comp->style->border.color.data.b = 255;
            comp->style->border.color.data.a = 255;
        }
    }

    /* Apply hover effect - brighten background */
    uint8_t original_bg_r = 0, original_bg_g = 0, original_bg_b = 0, original_bg_a = 0;
    if (is_hovered && comp->style) {
        /* Save original background color */
        if (ir_color_resolve(&comp->style->background, &original_bg_r, &original_bg_g, &original_bg_b, &original_bg_a)) {
            /* Brighten by 30% */
            comp->style->background.data.r = (uint8_t)(original_bg_r + (255 - original_bg_r) * 0.3f);
            comp->style->background.data.g = (uint8_t)(original_bg_g + (255 - original_bg_g) * 0.3f);
            comp->style->background.data.b = (uint8_t)(original_bg_b + (255 - original_bg_b) * 0.3f);
        }
    }

    /* Render button background and border first */
    ir_gen_container_commands(comp, ctx, bounds);

    /* Restore original background color if it was modified */
    if (is_hovered && comp->style && original_bg_a > 0) {
        comp->style->background.data.r = original_bg_r;
        comp->style->background.data.g = original_bg_g;
        comp->style->background.data.b = original_bg_b;
        comp->style->background.data.a = original_bg_a;
    }

    /* Render button text centered */
    if (comp->text_content) {
        IRStyle* style = comp->style;

        /* Get text color */
        uint32_t base_color = 0x000000FF;  /* Default black */
        if (style) {
            uint8_t r, g, b, a;
            if (ir_color_resolve(&style->font.color, &r, &g, &b, &a)) {
                base_color = (r << 24) | (g << 16) | (b << 8) | a;
            }
        }
        uint32_t text_color = ir_apply_opacity_to_color(base_color, ctx->current_opacity);

        /* Get font size */
        float font_size = style && style->font.size > 0 ? style->font.size : 16;

        /* Estimate text dimensions for centering */
        float text_width = ir_get_text_width_estimate(comp->text_content, font_size);
        float text_height = font_size;

        /* Calculate centered position */
        float text_x = bounds->x + (bounds->width - text_width) / 2.0f;
        float text_y = bounds->y + (bounds->height - text_height) / 2.0f;

        /* Create centered text command */
        kryon_command_t cmd = {0};  // CRITICAL FIX: Zero-initialize
        cmd.type = KRYON_CMD_DRAW_TEXT;
        cmd.data.draw_text.x = text_x;
        cmd.data.draw_text.y = text_y;
        cmd.data.draw_text.font_id = 0;
        cmd.data.draw_text.font_size = font_size;
        cmd.data.draw_text.font_weight = style && style->font.bold ? 1 : 0;
        cmd.data.draw_text.font_style = style && style->font.italic ? 1 : 0;
        cmd.data.draw_text.color = text_color;

        strncpy(cmd.data.draw_text.text_storage, comp->text_content, 127);
        cmd.data.draw_text.text_storage[127] = '\0';
        cmd.data.draw_text.text = NULL;
        cmd.data.draw_text.max_length = strlen(cmd.data.draw_text.text_storage);

        kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
    }

    return true;
}

/* Tab Component Generator - renders individual tabs in the tab bar */
bool ir_gen_tab_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds) {
    /* Check if tab is being hovered */
    bool is_hovered = (g_hovered_component == comp);

    /* Save original background color for hover effect */
    uint8_t original_bg_r = 0, original_bg_g = 0, original_bg_b = 0, original_bg_a = 0;
    if (is_hovered && comp->style) {
        if (ir_color_resolve(&comp->style->background, &original_bg_r, &original_bg_g, &original_bg_b, &original_bg_a)) {
            comp->style->background.data.r = (uint8_t)(original_bg_r + (255 - original_bg_r) * 0.3f);
            comp->style->background.data.g = (uint8_t)(original_bg_g + (255 - original_bg_g) * 0.3f);
            comp->style->background.data.b = (uint8_t)(original_bg_b + (255 - original_bg_b) * 0.3f);
        }
    }

    /* Render tab background */
    ir_gen_container_commands(comp, ctx, bounds);

    /* Restore original background color */
    if (is_hovered && comp->style && original_bg_a > 0) {
        comp->style->background.data.r = original_bg_r;
        comp->style->background.data.g = original_bg_g;
        comp->style->background.data.b = original_bg_b;
        comp->style->background.data.a = original_bg_a;
    }

    /* Get tab title - check tab_data first, then text_content */
    const char* title = NULL;
    if (comp->tab_data && comp->tab_data->title) {
        title = comp->tab_data->title;
    } else if (comp->text_content) {
        title = comp->text_content;
    }

    /* Render tab title text centered */
    if (title && title[0] != '\0') {
        IRStyle* style = comp->style;

        /* Get text color - default to white for dark tabs */
        uint32_t base_color = 0xFFFFFFFF;
        if (style) {
            uint8_t r, g, b, a;
            if (ir_color_resolve(&style->font.color, &r, &g, &b, &a) && a > 0) {
                base_color = (r << 24) | (g << 16) | (b << 8) | a;
            }
        }
        uint32_t text_color = ir_apply_opacity_to_color(base_color, ctx->current_opacity);

        /* Get font size */
        float font_size = style && style->font.size > 0 ? style->font.size : 14;

        /* Estimate text dimensions for centering */
        float text_width = ir_get_text_width_estimate(title, font_size);
        float text_height = font_size;

        /* Calculate centered position */
        float text_x = bounds->x + (bounds->width - text_width) / 2.0f;
        float text_y = bounds->y + (bounds->height - text_height) / 2.0f;

        kryon_command_t cmd = {0};
        cmd.type = KRYON_CMD_DRAW_TEXT;
        cmd.data.draw_text.x = text_x;
        cmd.data.draw_text.y = text_y;
        cmd.data.draw_text.font_id = 0;
        cmd.data.draw_text.font_size = font_size;
        cmd.data.draw_text.font_weight = style && style->font.bold ? 1 : 0;
        cmd.data.draw_text.font_style = style && style->font.italic ? 1 : 0;
        cmd.data.draw_text.color = text_color;

        strncpy(cmd.data.draw_text.text_storage, title, 127);
        cmd.data.draw_text.text_storage[127] = '\0';
        cmd.data.draw_text.text = NULL;
        cmd.data.draw_text.max_length = strlen(cmd.data.draw_text.text_storage);

        kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
    }

    return true;
}

/* Input Component Generator */
bool ir_gen_input_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds) {
    /* Render input background and border */
    ir_gen_container_commands(comp, ctx, bounds);

    /* Render input text content if present */
    if (comp->text_content && comp->text_content[0] != '\0') {
        IRStyle* style = comp->style;
        uint8_t r, g, b, a;

        /* Get text color (default to black) */
        if (style && ir_color_resolve(&style->font.color, &r, &g, &b, &a)) {
            uint32_t text_color = (r << 24) | (g << 16) | (b << 8) | a;
            text_color = ir_apply_opacity_to_color(text_color, ctx->current_opacity);

            kryon_command_t cmd = {0};  // CRITICAL FIX: Zero-initialize
            cmd.type = KRYON_CMD_DRAW_TEXT;
            cmd.data.draw_text.x = bounds->x + 8; /* 8px padding */
            cmd.data.draw_text.y = bounds->y + (bounds->height - 16) / 2;
            cmd.data.draw_text.font_id = 0;
            cmd.data.draw_text.font_size = style->font.size > 0 ? style->font.size : 14;
            cmd.data.draw_text.font_weight = style->font.bold ? 1 : 0;
            cmd.data.draw_text.font_style = style->font.italic ? 1 : 0;
            cmd.data.draw_text.color = text_color;

            strncpy(cmd.data.draw_text.text_storage, comp->text_content, 127);
            cmd.data.draw_text.text_storage[127] = '\0';
            cmd.data.draw_text.text = NULL;  /* NULL indicates text is in text_storage */
            cmd.data.draw_text.max_length = strlen(cmd.data.draw_text.text_storage);

            kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
        }
    }

    return true;
}

/* Checkbox Component Generator */
bool ir_gen_checkbox_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds) {
    /* Add default styling for checkbox if not set */
    if (comp->style) {
        /* Set default white background for checkbox box */
        if (comp->style->background.type == IR_COLOR_TRANSPARENT ||
            (comp->style->background.type == IR_COLOR_SOLID && comp->style->background.data.a == 0)) {
            comp->style->background.type = IR_COLOR_SOLID;
            comp->style->background.data.r = 255;
            comp->style->background.data.g = 255;
            comp->style->background.data.b = 255;
            comp->style->background.data.a = 255;
        }

        /* Set default border for checkbox box */
        if (comp->style->border.width == 0.0f) {
            comp->style->border.width = 2.0f;
            /* Set default border color to gray if not set or transparent */
            if (comp->style->border.color.type == IR_COLOR_TRANSPARENT ||
                (comp->style->border.color.type == IR_COLOR_SOLID && comp->style->border.color.data.a == 0)) {
                comp->style->border.color.type = IR_COLOR_SOLID;
                comp->style->border.color.data.r = 128;
                comp->style->border.color.data.g = 128;
                comp->style->border.color.data.b = 128;
                comp->style->border.color.data.a = 255;
            }
        }
    }

    /* Checkbox dimensions */
    float checkbox_size = bounds->height * 0.6f < 20.0f ? bounds->height * 0.6f : 20.0f;
    float checkbox_y = bounds->y + (bounds->height - checkbox_size) / 2;

    LayoutRect checkbox_bounds = {
        .x = bounds->x + 5,
        .y = checkbox_y,
        .width = checkbox_size,
        .height = checkbox_size
    };

    /* Render checkbox box (background + border) */
    ir_gen_container_commands(comp, ctx, &checkbox_bounds);

    /* Check if checkbox is checked (stored in custom_data as "checked" or "unchecked") */
    bool is_checked = comp->custom_data && strcmp(comp->custom_data, "checked") == 0;

    if (is_checked) {
        /* Draw checkmark as two lines forming a check */
        uint32_t check_color = 0x22C55EFF; /* Default green */
        if (comp->style) {
            uint8_t r, g, b, a;
            if (ir_color_resolve(&comp->style->font.color, &r, &g, &b, &a) && a > 0) {
                /* Only use font color if it's not transparent */
                check_color = (r << 24) | (g << 16) | (b << 8) | a;
            }
        }
        check_color = ir_apply_opacity_to_color(check_color, ctx->current_opacity);

        kryon_command_t cmd = {0};  // CRITICAL FIX: Zero-initialize
        cmd.type = KRYON_CMD_DRAW_LINE;
        cmd.data.draw_line.color = check_color;

        /* First line of checkmark */
        cmd.data.draw_line.x1 = checkbox_bounds.x + checkbox_size * 0.2f;
        cmd.data.draw_line.y1 = checkbox_bounds.y + checkbox_size * 0.5f;
        cmd.data.draw_line.x2 = checkbox_bounds.x + checkbox_size * 0.4f;
        cmd.data.draw_line.y2 = checkbox_bounds.y + checkbox_size * 0.7f;
        kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

        /* Second line of checkmark */
        cmd.data.draw_line.x1 = checkbox_bounds.x + checkbox_size * 0.4f;
        cmd.data.draw_line.y1 = checkbox_bounds.y + checkbox_size * 0.7f;
        cmd.data.draw_line.x2 = checkbox_bounds.x + checkbox_size * 0.8f;
        cmd.data.draw_line.y2 = checkbox_bounds.y + checkbox_size * 0.3f;
        kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
    }

    /* Render label text if present */
    if (comp->text_content && comp->text_content[0] != '\0') {
        LayoutRect text_bounds = {
            .x = bounds->x + checkbox_size + 15,
            .y = bounds->y,
            .width = bounds->width - checkbox_size - 15,
            .height = bounds->height
        };
        ir_gen_text_commands(comp, ctx, &text_bounds);
    }

    return true;
}

/* Dropdown Component Generator */
bool ir_gen_dropdown_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds) {
    /* Render dropdown box background and border */
    ir_gen_container_commands(comp, ctx, bounds);

    /* Get dropdown state */
    IRDropdownState* dropdown = (IRDropdownState*)comp->custom_data;

    /* Determine what text to display */
    const char* display_text = NULL;
    if (dropdown && dropdown->selected_index >= 0 && dropdown->selected_index < (int32_t)dropdown->option_count) {
        /* Show selected option */
        display_text = dropdown->options[dropdown->selected_index];
    } else if (dropdown && dropdown->placeholder) {
        /* Show placeholder */
        display_text = dropdown->placeholder;
    }

    /* Render text if we have something to display */
    if (display_text && display_text[0] != '\0') {
        /* Temporarily set text_content for rendering */
        char* original_text = comp->text_content;
        comp->text_content = (char*)display_text;

        /* Get font size for vertical centering */
        float font_size = 14.0f;
        if (comp->style && comp->style->font.size > 0) {
            font_size = comp->style->font.size;
        }

        /* Calculate vertical offset to center text */
        float text_y_offset = (bounds->height - font_size) / 2.0f;

        LayoutRect text_bounds = {
            .x = bounds->x + 12,  /* 12px left padding */
            .y = bounds->y + text_y_offset,  /* Vertically centered */
            .width = bounds->width - 40,  /* Leave space for arrow */
            .height = font_size  /* Height should match font size */
        };
        ir_gen_text_commands(comp, ctx, &text_bounds);

        /* Restore original text_content */
        comp->text_content = original_text;
    }

    /* Draw dropdown arrow on the right */
    uint32_t arrow_color = 0x6B7280FF; /* Gray */
    if (comp->style) {
        uint8_t r, g, b, a;
        if (ir_color_resolve(&comp->style->font.color, &r, &g, &b, &a)) {
            arrow_color = (r << 24) | (g << 16) | (b << 8) | a;
        }
    }
    arrow_color = ir_apply_opacity_to_color(arrow_color, ctx->current_opacity);

    float arrow_x = bounds->x + bounds->width - 20;
    float arrow_y = bounds->y + bounds->height / 2;
    float arrow_size = 6;

    kryon_command_t cmd = {0};  // CRITICAL FIX: Zero-initialize
    cmd.type = KRYON_CMD_DRAW_LINE;
    cmd.data.draw_line.color = arrow_color;

    /* Left line of down arrow */
    cmd.data.draw_line.x1 = arrow_x - arrow_size / 2;
    cmd.data.draw_line.y1 = arrow_y - 2;
    cmd.data.draw_line.x2 = arrow_x;
    cmd.data.draw_line.y2 = arrow_y + 2;
    kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

    /* Right line of down arrow */
    cmd.data.draw_line.x1 = arrow_x;
    cmd.data.draw_line.y1 = arrow_y + 2;
    cmd.data.draw_line.x2 = arrow_x + arrow_size / 2;
    cmd.data.draw_line.y2 = arrow_y - 2;
    kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

    /* Dropdown menu is rendered in overlay pass when open */
    /* See ir_gen_dropdown_overlay_commands() */

    return true;
}

/* Image Component Generator */
bool ir_gen_image_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds) {
    /* Render image background if any */
    if (comp->style) {
        ir_gen_container_commands(comp, ctx, bounds);
    }

    /* Get image source path from text_content */
    const char* image_source = comp->text_content;

    if (image_source && strlen(image_source) > 0) {
        /* Generate draw image command */
        kryon_command_t draw_cmd = {0};
        draw_cmd.type = KRYON_CMD_DRAW_IMAGE;
        draw_cmd.data.draw_image.image_id = 0;  /* Will be resolved by renderer from path */
        draw_cmd.data.draw_image.x = (int16_t)bounds->x;
        draw_cmd.data.draw_image.y = (int16_t)bounds->y;
        draw_cmd.data.draw_image.w = (uint16_t)bounds->width;
        draw_cmd.data.draw_image.h = (uint16_t)bounds->height;
        draw_cmd.data.draw_image.opacity = comp->style ? comp->style->opacity : 1.0f;

        /* Store image path in custom_data for renderer access */
        if (!comp->custom_data && image_source) {
            comp->custom_data = (void*)strdup(image_source);
        }

        kryon_cmd_buf_push(ctx->cmd_buf, &draw_cmd);
    } else {
        /* Fallback: render placeholder rectangle with text */
        kryon_command_t placeholder_cmd = {0};
        placeholder_cmd.type = KRYON_CMD_DRAW_RECT;
        placeholder_cmd.data.draw_rect.x = (int16_t)bounds->x;
        placeholder_cmd.data.draw_rect.y = (int16_t)bounds->y;
        placeholder_cmd.data.draw_rect.w = (uint16_t)bounds->width;
        placeholder_cmd.data.draw_rect.h = (uint16_t)bounds->height;
        placeholder_cmd.data.draw_rect.color = 0xCCCCCCFF;  /* Light gray placeholder */
        kryon_cmd_buf_push(ctx->cmd_buf, &placeholder_cmd);

        /* Draw "Image" text in center if there's enough space */
        if (bounds->height > 20) {
            kryon_command_t text_cmd = {0};
            text_cmd.type = KRYON_CMD_DRAW_TEXT;
            text_cmd.data.draw_text.x = (int16_t)(bounds->x + 5);
            text_cmd.data.draw_text.y = (int16_t)(bounds->y + bounds->height / 2 - 6);
            text_cmd.data.draw_text.font_id = 0;  /* Default font */
            text_cmd.data.draw_text.font_size = 12;
            text_cmd.data.draw_text.font_weight = 0;
            text_cmd.data.draw_text.font_style = 0;
            text_cmd.data.draw_text.color = 0x666666FF;
            text_cmd.data.draw_text.max_length = 127;

            /* Store text inline */
            strncpy(text_cmd.data.draw_text.text_storage, "Image", 127);
            text_cmd.data.draw_text.text = text_cmd.data.draw_text.text_storage;

            kryon_cmd_buf_push(ctx->cmd_buf, &text_cmd);
        }
    }

    return true;
}

/* Table Component Generator */
bool ir_gen_table_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds) {
    /* Render table background */
    ir_gen_container_commands(comp, ctx, bounds);

    if (getenv("KRYON_DEBUG_TABLE")) {
        fprintf(stderr, "[TABLE_RENDER] Component %u type=%d bounds=(%.1f,%.1f,%.1fx%.1f)\n",
                comp->id, comp->type, bounds->x, bounds->y, bounds->width, bounds->height);
    }

    /* Handle table-specific rendering based on component type */
    if (comp->type == IR_COMPONENT_TABLE) {
        /* Draw table borders if enabled */
        IRTableState* state = ir_get_table_state(comp);

        if (getenv("KRYON_DEBUG_TABLE")) {
            fprintf(stderr, "[TABLE_RENDER]   state=%p\n", (void*)state);
            if (state) {
                fprintf(stderr, "[TABLE_RENDER]   show_borders=%d calculated_widths=%p calculated_heights=%p\n",
                        state->style.show_borders, (void*)state->calculated_widths, (void*)state->calculated_heights);
                fprintf(stderr, "[TABLE_RENDER]   row_count=%u column_count=%u\n",
                        state->row_count, state->column_count);
            }
        }
        if (state && state->style.show_borders) {
            uint32_t border_color = 0xC8C8C8FF; /* Default gray */
            uint8_t r, g, b, a;
            if (ir_color_resolve(&state->style.border_color, &r, &g, &b, &a)) {
                border_color = (r << 24) | (g << 16) | (b << 8) | a;
            }
            border_color = ir_apply_opacity_to_color(border_color, ctx->current_opacity);

            float border_width = state->style.border_width;
            float x = bounds->x;
            float y = bounds->y;

            /* Draw table border grid lines */
            if (state->calculated_heights && state->calculated_widths) {
                uint32_t num_rows = state->row_count;
                uint32_t num_cols = state->column_count;

                /* Horizontal lines */
                float current_y = y;
                for (uint32_t r = 0; r <= num_rows; r++) {
                    kryon_command_t cmd = {0};
                    cmd.type = KRYON_CMD_DRAW_LINE;
                    cmd.data.draw_line.x1 = x;
                    cmd.data.draw_line.y1 = current_y;
                    cmd.data.draw_line.x2 = x + bounds->width;
                    cmd.data.draw_line.y2 = current_y;
                    cmd.data.draw_line.color = border_color;
                    kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

                    if (r < num_rows) {
                        current_y += state->calculated_heights[r] + border_width;
                    }
                }

                /* Vertical lines */
                float current_x = x;
                for (uint32_t c = 0; c <= num_cols; c++) {
                    kryon_command_t cmd = {0};
                    cmd.type = KRYON_CMD_DRAW_LINE;
                    cmd.data.draw_line.x1 = current_x;
                    cmd.data.draw_line.y1 = y;
                    cmd.data.draw_line.x2 = current_x;
                    cmd.data.draw_line.y2 = y + bounds->height;
                    cmd.data.draw_line.color = border_color;
                    kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

                    if (c < num_cols) {
                        current_x += state->calculated_widths[c] + border_width;
                    }
                }
            }
        }
    } else if (comp->type == IR_COMPONENT_TABLE_CELL || comp->type == IR_COMPONENT_TABLE_HEADER_CELL) {
        /* Render cell background and text */
        if (comp->style && comp->style->background.type == IR_COLOR_SOLID) {
            uint8_t r, g, b, a;
            if (ir_color_resolve(&comp->style->background, &r, &g, &b, &a) && a > 0) {
                uint32_t bg_color = (r << 24) | (g << 16) | (b << 8) | a;
                bg_color = ir_apply_opacity_to_color(bg_color, ctx->current_opacity);

                kryon_command_t cmd = {0};
                cmd.type = KRYON_CMD_DRAW_RECT;
                cmd.data.draw_rect.x = bounds->x;
                cmd.data.draw_rect.y = bounds->y;
                cmd.data.draw_rect.w = bounds->width;
                cmd.data.draw_rect.h = bounds->height;
                cmd.data.draw_rect.color = bg_color;
                kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
            }
        }

        /* Render cell text with padding */
        if (comp->text_content) {
            /* Find parent table to get cell padding */
            float padding = 8.0f; /* Default */
            IRComponent* parent = comp->parent;
            while (parent) {
                if (parent->type == IR_COMPONENT_TABLE) {
                    IRTableState* state = ir_get_table_state(parent);
                    if (state) {
                        padding = state->style.cell_padding;
                    }
                    break;
                }
                parent = parent->parent;
            }

            /* Adjust bounds for padding */
            LayoutRect text_bounds = *bounds;
            text_bounds.x += padding;
            text_bounds.y += padding;
            text_bounds.width -= 2 * padding;
            text_bounds.height -= 2 * padding;

            ir_gen_text_commands(comp, ctx, &text_bounds);
        }
    }

    return true;
}

/* Markdown Component Generator */
bool ir_gen_markdown_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds) {
    /* Markdown components are hierarchical - render as container */
    /* Specific markdown elements (heading, paragraph, etc.) are child components */
    
    ir_gen_container_commands(comp, ctx, bounds);

    /* Special handling for specific markdown types */
    switch (comp->type) {
        case IR_COMPONENT_HORIZONTAL_RULE: {
            /* Draw horizontal line */
            uint32_t line_color = 0xE5E7EBFF; /* Light gray */
            if (comp->style) {
                uint8_t r, g, b, a;
                if (ir_color_resolve(&comp->style->border.color, &r, &g, &b, &a)) {
                    line_color = (r << 24) | (g << 16) | (b << 8) | a;
                }
            }
            line_color = ir_apply_opacity_to_color(line_color, ctx->current_opacity);

            kryon_command_t cmd = {0};  // CRITICAL FIX: Zero-initialize
            cmd.type = KRYON_CMD_DRAW_LINE;
            cmd.data.draw_line.x1 = bounds->x;
            cmd.data.draw_line.y1 = bounds->y + bounds->height / 2;
            cmd.data.draw_line.x2 = bounds->x + bounds->width;
            cmd.data.draw_line.y2 = bounds->y + bounds->height / 2;
            cmd.data.draw_line.color = line_color;
            kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
            break;
        }

        case IR_COMPONENT_CODE_BLOCK: {
            /* Code blocks have monospace font and distinct background */
            /* Rendering handled by container + text content */
            if (comp->text_content) {
                ir_gen_text_commands(comp, ctx, bounds);
            }
            break;
        }

        default:
            /* Other markdown elements render their text content */
            if (comp->text_content) {
                ir_gen_text_commands(comp, ctx, bounds);
            }
            break;
    }

    return true;
}

/* Canvas Component Generator */
bool ir_gen_canvas_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds) {
    /* Canvas background is rendered directly by plugin renderer (not via command buffer)
     * Skip ir_gen_container_commands to avoid background overdraw */

    /* BACKUP FIX: Set rendered bounds from layout bounds
     * This ensures bounds are set even if plugin renderer doesn't execute.
     * Provides safety net for hit testing.
     */
    ir_set_rendered_bounds(comp, bounds->x, bounds->y, bounds->width, bounds->height);

    /* Invoke canvas plugin renderer to execute onDraw commands
     * The capability system provides a different API for component rendering.
     * For canvas components, we use the capability API to check for renderers
     * and dispatch appropriately.
     *
     * TODO: Implement canvas rendering through capability API
     */

    /* Canvas rendering is now handled through the capability system
     * The plugin will need to register a component renderer which can be
     * invoked via ir_capability_has_component_renderer() and a dispatch function.
     * For now, skip canvas rendering if no capability renderer is registered.
     */

    (void)ctx;  // Suppress unused warning
    (void)comp;  // Suppress unused warning
    (void)bounds;  // Suppress unused warning
    (void)ir_set_rendered_bounds;  // Suppress unused warning

    return true;
}

/* Modal Component Generator */
bool ir_gen_modal_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds) {
    /* Parse modal state from custom_data string: "open|title" or "closed|title" */
    const char* state_str = comp->custom_data;
    bool is_open = state_str && strncmp(state_str, "open", 4) == 0;

    /* If modal is not open, skip rendering entirely */
    if (!is_open) {
        return true;
    }

    /* Extract title from state string (after the '|') */
    const char* title = NULL;
    if (state_str) {
        const char* pipe = strchr(state_str, '|');
        if (pipe && pipe[1] != '\0') {
            title = pipe + 1;
        }
    }

    /* Get viewport dimensions from the root component's layout state */
    /* For simplicity, use reasonable defaults if not available */
    float viewport_width = 800.0f;
    float viewport_height = 600.0f;

    /* Try to get actual viewport size from the root component */
    IRComponent* root = comp;
    while (root->parent) {
        root = root->parent;
    }
    if (root->layout_state && root->layout_state->computed.valid) {
        viewport_width = root->layout_state->computed.width;
        viewport_height = root->layout_state->computed.height;
    }

    /* 1. Render semi-transparent backdrop covering entire viewport */
    /* Default: semi-transparent black (rgba(0,0,0,0.5)) */
    uint32_t backdrop_color = 0x00000080;
    backdrop_color = ir_apply_opacity_to_color(backdrop_color, ctx->current_opacity);

    kryon_command_t cmd = {0};
    cmd.type = KRYON_CMD_DRAW_RECT;
    cmd.data.draw_rect.x = 0;
    cmd.data.draw_rect.y = 0;
    cmd.data.draw_rect.w = viewport_width;
    cmd.data.draw_rect.h = viewport_height;
    cmd.data.draw_rect.color = backdrop_color;
    kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

    /* 2. Calculate modal container bounds (centered) */
    /* Get modal size from style since layout gives it 0 size */
    float modal_width = 400.0f;  /* Default */
    float modal_height = 300.0f; /* Default */

    if (comp->style) {
        if (comp->style->width.type == IR_DIMENSION_PX && comp->style->width.value > 0) {
            modal_width = comp->style->width.value;
        }
        if (comp->style->height.type == IR_DIMENSION_PX && comp->style->height.value > 0) {
            modal_height = comp->style->height.value;
        }
    }

    /* Center the modal in the viewport */
    float modal_x = (viewport_width - modal_width) / 2.0f;
    float modal_y = (viewport_height - modal_height) / 2.0f;

    /* Update rendered bounds for hit testing */
    ir_set_rendered_bounds(comp, modal_x, modal_y, modal_width, modal_height);

    /* Create centered bounds for the modal container */
    LayoutRect modal_bounds = {
        .x = modal_x,
        .y = modal_y,
        .width = modal_width,
        .height = modal_height
    };

    /* 3. Render modal background (as container) */
    ir_gen_container_commands(comp, ctx, &modal_bounds);

    /* 4. Render title bar if title is provided */
    float content_y = modal_y;
    if (title && title[0] != '\0') {
        float title_height = 40.0f;

        /* Title bar background */
        cmd.type = KRYON_CMD_DRAW_RECT;
        cmd.data.draw_rect.x = modal_x;
        cmd.data.draw_rect.y = modal_y;
        cmd.data.draw_rect.w = modal_width;
        cmd.data.draw_rect.h = title_height;
        cmd.data.draw_rect.color = 0x3d3d3dFF;  /* Slightly lighter than modal bg */
        kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

        /* Title text */
        cmd.type = KRYON_CMD_DRAW_TEXT;
        cmd.data.draw_text.x = modal_x + 16;
        cmd.data.draw_text.y = modal_y + 10;
        cmd.data.draw_text.font_id = 0;
        cmd.data.draw_text.font_size = 16;
        cmd.data.draw_text.font_weight = 1;  /* Bold */
        cmd.data.draw_text.font_style = 0;
        cmd.data.draw_text.color = 0xFFFFFFFF;

        strncpy(cmd.data.draw_text.text_storage, title, 127);
        cmd.data.draw_text.text_storage[127] = '\0';
        cmd.data.draw_text.text = NULL;
        cmd.data.draw_text.max_length = strlen(cmd.data.draw_text.text_storage);
        kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

        content_y += title_height;
    }

    /* 5. Render children in the modal's content area */
    if (comp->child_count > 0) {
        float padding = 24.0f;  /* Default modal padding */
        if (comp->style && comp->style->padding.top > 0) {
            padding = comp->style->padding.top;
        }

        /* Re-layout children with correct modal position so all descendants get proper positions */
        float content_x = modal_x + padding;
        float content_start_y = content_y + padding;
        float content_width = modal_width - 2 * padding;
        float content_height = modal_height - (content_y - modal_y) - 2 * padding;

        IRLayoutConstraints modal_constraints = {
            .min_width = 0,
            .max_width = content_width,
            .min_height = 0,
            .max_height = content_height
        };

        for (int i = 0; i < (int)comp->child_count; i++) {
            IRComponent* child = comp->children[i];
            if (!child) continue;

            /* Re-layout child subtree with correct absolute position */
            ir_layout_single_pass(child, modal_constraints, content_x, content_start_y);

            /* Now render with the newly computed layout */
            LayoutRect child_bounds;
            if (child->layout_state && child->layout_state->computed.valid) {
                child_bounds.x = child->layout_state->computed.x;
                child_bounds.y = child->layout_state->computed.y;
                child_bounds.width = child->layout_state->computed.width;
                child_bounds.height = child->layout_state->computed.height;
            } else {
                /* Fallback */
                child_bounds.x = content_x;
                child_bounds.y = content_start_y;
                child_bounds.width = content_width;
                child_bounds.height = 40.0f;
            }

            ir_generate_component_commands(child, ctx, &child_bounds, ctx->current_opacity);
        }
    }

    return true;
}

/* ============================================================================
 * Main Component Rendering - Recursive Traversal
 * ============================================================================ */

bool ir_generate_component_commands(
    IRComponent* component,
    IRCommandContext* ctx,
    LayoutRect* bounds,
    float inherited_opacity
) {
    if (!component || !ctx || !bounds) return false;
    if (component->style && !component->style->visible) return true;

    // Check conditional visibility
    if (component->visible_condition && component->visible_condition[0] != '\0') {
        extern int64_t ir_executor_get_var_int(IRExecutorContext* ctx, const char* name, uint32_t instance_id);

        IRExecutorContext* executor = ctx->state_mgr ? ir_state_manager_get_executor(ctx->state_mgr) : NULL;
        if (executor) {
            // Get variable value as int (0 = false, non-zero = true)
            int64_t var_value = ir_executor_get_var_int(executor, component->visible_condition, component->owner_instance_id);
            bool condition_value = (var_value != 0);

            // Check if component should be visible
            bool should_render = (condition_value == component->visible_when_true);

            if (!should_render) {
                // Component is hidden by condition - skip rendering
                return true;
            }
        }
    }

    /* Set rendered bounds for hit testing and positioning (dropdowns, etc) */
    component->rendered_bounds.x = bounds->x;
    component->rendered_bounds.y = bounds->y;
    component->rendered_bounds.width = bounds->width;
    component->rendered_bounds.height = bounds->height;
    component->rendered_bounds.valid = true;

    /* Apply opacity cascading */
    float comp_opacity = component->style ? component->style->opacity : 1.0f;
    ir_push_opacity(ctx, comp_opacity);

    /* Apply transforms (scale, translate, rotate) to bounds */
    LayoutRect transformed_bounds = *bounds;
    if (component->style) {
        IRTransform* t = &component->style->transform;

        /* Apply scale */
        if (t->scale_x != 1.0f || t->scale_y != 1.0f) {
            float center_x = bounds->x + bounds->width / 2.0f;
            float center_y = bounds->y + bounds->height / 2.0f;
            transformed_bounds.width *= t->scale_x;
            transformed_bounds.height *= t->scale_y;
            transformed_bounds.x = center_x - transformed_bounds.width / 2.0f;
            transformed_bounds.y = center_y - transformed_bounds.height / 2.0f;
        }

        /* Apply translate */
        transformed_bounds.x += t->translate_x;
        transformed_bounds.y += t->translate_y;

        /* TODO: Apply rotate (requires full 2D transform matrix) */
    }
    /* Use transformed bounds for rendering this component */
    LayoutRect* render_bounds = &transformed_bounds;

    /* Defer to overlay pass if needed */
    if (ir_should_defer_to_overlay(component, ctx)) {
        ir_defer_to_overlay(ctx, component);
        ir_pop_opacity(ctx);
        return true;
    }

    /* Check wireframe mode - render reactive components as outlines */
    ir_wireframe_config_t wf_config = {0};
    ir_wireframe_load_config(&wf_config);

    if (wf_config.enabled && ir_wireframe_should_render(component, &wf_config)) {
        const char* label = NULL;
        char label_buf[64];

        /* Generate appropriate label */
        if (component->type == IR_COMPONENT_FOR_LOOP) {
            label = "[FOR]";
            label = "[FORLOOP]";
        } else if (component->text_expression) {
            snprintf(label_buf, sizeof(label_buf), "{{%s}}", component->text_expression);
            label = label_buf;
        } else if (component->visible_condition) {
            snprintf(label_buf, sizeof(label_buf), "?%s", component->visible_condition);
            label = label_buf;
        } else if (component->property_binding_count > 0) {
            label = "[REACTIVE]";
        }

        /* Render wireframe outline */
        bool wireframe_result = ir_wireframe_render_outline(
            component, ctx, render_bounds,
            wf_config.color, label
        );

        ir_pop_opacity(ctx);
        return wireframe_result;
    }

    /* Generate commands based on component type */
    bool success = true;

    // fprintf(stderr, "[DISPATCH] Component %u type=%d (%s)\n",
    //         component->id, component->type,
    //         component->type == 8 ? "BUTTON" : "other");

    switch (component->type) {
        case IR_COMPONENT_ROW:
            if (getenv("KRYON_DEBUG_LAYOUT")) {
                fprintf(stderr, "[ROW] ID=%u children=%u\n",
                        component->id, component->child_count);
            }
            success = ir_gen_container_commands(component, ctx, render_bounds);
            break;

        case IR_COMPONENT_CONTAINER:
        case IR_COMPONENT_COLUMN:
        case IR_COMPONENT_CENTER:
            success = ir_gen_container_commands(component, ctx, render_bounds);
            break;

        case IR_COMPONENT_TEXT:
        case IR_COMPONENT_STRONG:
        case IR_COMPONENT_EM:
        case IR_COMPONENT_CODE_INLINE:
        case IR_COMPONENT_SMALL:
        case IR_COMPONENT_MARK:
            success = ir_gen_text_commands(component, ctx, render_bounds);
            break;

        case IR_COMPONENT_SPAN:
            success = ir_gen_container_commands(component, ctx, render_bounds);
            break;

        case IR_COMPONENT_BUTTON:
            success = ir_gen_button_commands(component, ctx, render_bounds);
            break;

        case IR_COMPONENT_INPUT:
            success = ir_gen_input_commands(component, ctx, render_bounds);
            break;

        case IR_COMPONENT_CHECKBOX:
            success = ir_gen_checkbox_commands(component, ctx, render_bounds);
            break;

        case IR_COMPONENT_DROPDOWN:
            success = ir_gen_dropdown_commands(component, ctx, render_bounds);
            break;

        case IR_COMPONENT_IMAGE:
            success = ir_gen_image_commands(component, ctx, render_bounds);
            break;

        case IR_COMPONENT_TABLE:
        case IR_COMPONENT_TABLE_HEAD:
        case IR_COMPONENT_TABLE_BODY:
        case IR_COMPONENT_TABLE_FOOT:
        case IR_COMPONENT_TABLE_ROW:
        case IR_COMPONENT_TABLE_CELL:
        case IR_COMPONENT_TABLE_HEADER_CELL:
            success = ir_gen_table_commands(component, ctx, render_bounds);
            break;

        case IR_COMPONENT_MARKDOWN:
        case IR_COMPONENT_HEADING:
        case IR_COMPONENT_PARAGRAPH:
        case IR_COMPONENT_BLOCKQUOTE:
        case IR_COMPONENT_CODE_BLOCK:
        case IR_COMPONENT_HORIZONTAL_RULE:
        case IR_COMPONENT_LIST:
        case IR_COMPONENT_LIST_ITEM:
        case IR_COMPONENT_LINK:
            success = ir_gen_markdown_commands(component, ctx, render_bounds);
            break;

        case IR_COMPONENT_CANVAS:
        case IR_COMPONENT_NATIVE_CANVAS:
            success = ir_gen_canvas_commands(component, ctx, render_bounds);
            break;

        case IR_COMPONENT_MODAL:
            success = ir_gen_modal_commands(component, ctx, render_bounds);
            break;

        case IR_COMPONENT_FOR_LOOP:
            /* For loop is layout-transparent - like CSS display: contents */
            /* Skip rendering the For wrapper itself, just render children */
            if (getenv("KRYON_DEBUG_LAYOUT")) {
                fprintf(stderr, "[FOR] ID=%u children=%d\n",
                        component->id, component->child_count);
            }
            success = true;
            break;

        case IR_COMPONENT_TAB:
            success = ir_gen_tab_commands(component, ctx, render_bounds);
            break;

        case IR_COMPONENT_TAB_GROUP:
        case IR_COMPONENT_TAB_BAR:
        case IR_COMPONENT_TAB_CONTENT:
        case IR_COMPONENT_TAB_PANEL:
            /* Tab containers render as normal containers - children handle their own rendering */
            success = ir_gen_container_commands(component, ctx, render_bounds);
            break;

        default:
            /* Unknown component type - render as container */
            success = ir_gen_container_commands(component, ctx, render_bounds);
            break;
    }

    /* Reset root flag after rendering root component itself */
    if (ctx->is_root_component) {
        ctx->is_root_component = false;
    }

    /* Render children */
    /* Skip children for Modal components - they render their own children when open */
    bool skip_children = (component->type == IR_COMPONENT_MODAL);
    if (success && component->child_count > 0 && !skip_children) {
        for (int i = 0; i < (int)component->child_count; i++) {
            IRComponent* child = component->children[i];
            if (!child || !child->layout_state) continue;

            /* Get child bounds from layout */
            LayoutRect child_bounds = {
                .x = child->layout_state->computed.x,
                .y = child->layout_state->computed.y,
                .width = child->layout_state->computed.width,
                .height = child->layout_state->computed.height
            };

            success = ir_generate_component_commands(child, ctx, &child_bounds, ctx->current_opacity);
            if (!success) break;
        }
    }

    /* Restore opacity */
    ir_pop_opacity(ctx);

    return success;
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

bool ir_component_to_commands(
    IRComponent* component,
    kryon_cmd_buf_t* cmd_buf,
    LayoutRect* bounds,
    float opacity,
    void* backend_ctx,
    IRStateManager* state_mgr
) {
    if (!component || !cmd_buf || !bounds) return false;

    /* Initialize command generation context */
    IRCommandContext ctx = {0};
    ctx.cmd_buf = cmd_buf;
    ctx.current_opacity = opacity;
    ctx.opacity_depth = 0;
    ctx.transform_depth = 0;
    ctx.current_pass = 0;
    ctx.next_font_id = 1;
    ctx.next_image_id = 1;
    ctx.overlay_count = 0;
    ctx.backend_ctx = backend_ctx;
    ctx.state_mgr = state_mgr;
    ctx.is_root_component = true;  // Root component - skip background (SDL_RenderClear handles it)

    /* Pass 1: Main rendering */
    kryon_command_t cmd = {
        .type = KRYON_CMD_BEGIN_PASS,
        .data.begin_pass = { .pass_id = 0 }
    };
    kryon_cmd_buf_push(cmd_buf, &cmd);

    bool success = ir_generate_component_commands(component, &ctx, bounds, opacity);
    ctx.is_root_component = false;  // Reset after root is processed

    cmd.type = KRYON_CMD_END_PASS;
    kryon_cmd_buf_push(cmd_buf, &cmd);

    /* Pass 2: Overlay rendering (dropdowns, dragged tabs, modals) */
    if (ctx.overlay_count > 0) {
        ctx.current_pass = 1;  /* Mark that we're in overlay pass */

        cmd.type = KRYON_CMD_BEGIN_PASS;
        cmd.data.begin_pass.pass_id = 1;
        kryon_cmd_buf_push(cmd_buf, &cmd);

        for (int i = 0; i < ctx.overlay_count; i++) {
            IRComponent* overlay = ctx.overlays[i];
            if (!overlay || !overlay->layout_state) continue;

            LayoutRect overlay_bounds = {
                .x = overlay->layout_state->computed.x,
                .y = overlay->layout_state->computed.y,
                .width = overlay->layout_state->computed.width,
                .height = overlay->layout_state->computed.height
            };

            success = ir_generate_component_commands(overlay, &ctx, &overlay_bounds, opacity) && success;
        }

        cmd.type = KRYON_CMD_END_PASS;
        kryon_cmd_buf_push(cmd_buf, &cmd);
    }

    return success;
}
