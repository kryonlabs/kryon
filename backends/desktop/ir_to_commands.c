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

#include "ir_to_commands.h"
#include "desktop_internal.h"
#include "../../ir/ir_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Resolve IRColor to RGBA components.
 * Returns true if color was successfully resolved to solid RGBA.
 * Returns false for gradients or unresolved variable references.
 */
bool ir_color_resolve(const IRColor* color, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) {
    if (!color) return false;

    switch (color->type) {
        case IR_COLOR_SOLID:
            *r = color->data.r;
            *g = color->data.g;
            *b = color->data.b;
            *a = color->data.a;
            return true;

        case IR_COLOR_TRANSPARENT:
            /* Fully transparent */
            *r = 0;
            *g = 0;
            *b = 0;
            *a = 0;
            return true;

        case IR_COLOR_GRADIENT:
            /* Gradients need special handling - return false to indicate non-solid color */
            return false;

        case IR_COLOR_VAR_REF:
            /* TODO: Resolve style variable references */
            /* For now, return false - variable resolution not yet implemented */
            return false;

        default:
            return false;
    }
}

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

bool ir_should_defer_to_overlay(IRComponent* comp) {
    /* Defer open dropdowns and dragged tabs to overlay pass */
    if (comp->type == IR_COMPONENT_DROPDOWN) {
        /* TODO: Check if dropdown is open */
        return false; /* For now, render inline */
    }

    /* TODO: Check for dragged tabs */

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
    if (!comp->style) return true;

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
        if (ir_color_resolve(&style->background, &bg_r, &bg_g, &bg_b, &bg_a) && bg_a > 0) {
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
            fprintf(stderr, "[IR2CMD][id=%u] RECT: x=%.1f y=%.1f w=%.1f h=%.1f rgba(%d,%d,%d,%d) color=0x%08x\n",
                    comp->id, bounds->x, bounds->y, bounds->width, bounds->height,
                    bg_r, bg_g, bg_b, bg_a, bg_color);
            fflush(stderr);

            cmd.type = KRYON_CMD_DRAW_RECT;
            cmd.data.draw_rect.x = bounds->x;
            cmd.data.draw_rect.y = bounds->y;
            cmd.data.draw_rect.w = bounds->width;
            cmd.data.draw_rect.h = bounds->height;
            cmd.data.draw_rect.color = bg_color;
        }

        kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
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
    if (!comp->text_content) return true;

    if (getenv("KRYON_DEBUG_TAB_LAYOUT")) {
        fprintf(stderr, "[TEXT_RENDER] ID=%u text='%s' bounds=(%.1f, %.1f) layout_state=%p\n",
               comp->id, comp->text_content, bounds->x, bounds->y, comp->layout_state);
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

    kryon_command_t cmd = {0};  // CRITICAL FIX: Zero-initialize
    cmd.type = KRYON_CMD_DRAW_TEXT;
    cmd.data.draw_text.x = bounds->x;
    cmd.data.draw_text.y = bounds->y;
    cmd.data.draw_text.font_id = 0; /* TODO: Resolve font from style */
    cmd.data.draw_text.font_size = style && style->font.size > 0 ? style->font.size : 16;
    cmd.data.draw_text.font_weight = style && style->font.bold ? 1 : 0;
    cmd.data.draw_text.font_style = style && style->font.italic ? 1 : 0;
    cmd.data.draw_text.color = text_color;

    /* Copy text to inline storage */
    strncpy(cmd.data.draw_text.text_storage, comp->text_content, 127);
    cmd.data.draw_text.text_storage[127] = '\0';
    cmd.data.draw_text.text = NULL;  /* NULL indicates text is in text_storage */
    cmd.data.draw_text.max_length = strlen(cmd.data.draw_text.text_storage);

    kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

    return true;
}

bool ir_gen_button_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds) {
    /* Check if button is being hovered */
    bool is_hovered = (g_hovered_component == comp);

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

    /* Render selected text */
    if (comp->text_content && comp->text_content[0] != '\0') {
        LayoutRect text_bounds = {
            .x = bounds->x + 12,
            .y = bounds->y,
            .width = bounds->width - 40,  /* Leave space for arrow */
            .height = bounds->height
        };
        ir_gen_text_commands(comp, ctx, &text_bounds);
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

    /* TODO: Render dropdown menu if open (deferred to overlay pass) */

    return true;
}

/* Image Component Generator */
bool ir_gen_image_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds) {
    /* Render image background if any */
    if (comp->style) {
        ir_gen_container_commands(comp, ctx, bounds);
    }

    /* TODO: Load and render image from custom_data or text_content (URL/path) */
    /* Would need image resource loading system */

    (void)comp;
    (void)ctx;
    (void)bounds;

    return true;
}

/* Table Component Generator */
bool ir_gen_table_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds) {
    /* Render table background and border */
    ir_gen_container_commands(comp, ctx, bounds);

    /* Tables are hierarchical - children handle their own rendering */
    /* Parent table just provides the container */

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
    /* Render canvas background */
    ir_gen_container_commands(comp, ctx, bounds);

    /* BACKUP FIX: Set rendered bounds from layout bounds
     * This ensures bounds are set even if plugin renderer doesn't execute.
     * Provides safety net for hit testing.
     */
    ir_set_rendered_bounds(comp, bounds->x, bounds->y, bounds->width, bounds->height);

    /* Invoke canvas plugin renderer to execute onDraw commands
     * The plugin renderer will execute canvas drawing commands from the buffer
     * that was filled by the onDraw callback (invoked before rendering).
     */
    extern bool ir_plugin_dispatch_component_render(void* backend_ctx, uint32_t component_type,
                                                     const IRComponent* component,
                                                     float x, float y, float width, float height);

    if (ctx->backend_ctx) {
        ir_plugin_dispatch_component_render(ctx->backend_ctx, comp->type, comp,
                                            bounds->x, bounds->y, bounds->width, bounds->height);
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
    if (ir_should_defer_to_overlay(component)) {
        ir_defer_to_overlay(ctx, component);
        ir_pop_opacity(ctx);
        return true;
    }

    /* Generate commands based on component type */
    bool success = true;

    switch (component->type) {
        case IR_COMPONENT_CONTAINER:
        case IR_COMPONENT_ROW:
        case IR_COMPONENT_COLUMN:
        case IR_COMPONENT_CENTER:
            success = ir_gen_container_commands(component, ctx, render_bounds);
            break;

        case IR_COMPONENT_TEXT:
            success = ir_gen_text_commands(component, ctx, render_bounds);
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

        default:
            /* Unknown component type - render as container */
            success = ir_gen_container_commands(component, ctx, render_bounds);
            break;
    }

    /* Render children */
    if (success && component->child_count > 0) {
        for (int i = 0; i < component->child_count; i++) {
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
    void* backend_ctx
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

    /* Pass 1: Main rendering */
    kryon_command_t cmd = {
        .type = KRYON_CMD_BEGIN_PASS,
        .data.begin_pass = { .pass_id = 0 }
    };
    kryon_cmd_buf_push(cmd_buf, &cmd);

    bool success = ir_generate_component_commands(component, &ctx, bounds, opacity);

    cmd.type = KRYON_CMD_END_PASS;
    kryon_cmd_buf_push(cmd_buf, &cmd);

    /* Pass 2: Overlay rendering (dropdowns, dragged tabs) */
    if (ctx.overlay_count > 0) {
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

    // Debug: Check buffer before returning
    fprintf(stderr, "[IR2CMD_RETURN] Buffer at pos 0: %02x %02x %02x %02x\n",
            cmd_buf->buffer[0], cmd_buf->buffer[1], cmd_buf->buffer[2], cmd_buf->buffer[3]);
    fprintf(stderr, "[IR2CMD_RETURN] Buffer at pos 1632: %02x %02x %02x %02x\n",
            cmd_buf->buffer[1632], cmd_buf->buffer[1633], cmd_buf->buffer[1634], cmd_buf->buffer[1635]);
    fflush(stderr);

    return success;
}
