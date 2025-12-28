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

bool ir_gen_container_commands(IRComponent* comp, IRCommandContext* ctx, IRRect* bounds) {
    if (!comp->style) return true;

    IRStyle* style = comp->style;
    kryon_command_t cmd;

    /* Render shadow if present */
    if (style->has_shadow && !style->shadow.inset && style->shadow.color != 0) {
        uint32_t shadow_color = ir_apply_opacity_to_color(style->shadow.color, ctx->current_opacity);

        cmd.type = KRYON_CMD_DRAW_SHADOW;
        cmd.data.draw_shadow.x = bounds->x + style->shadow.offset_x;
        cmd.data.draw_shadow.y = bounds->y + style->shadow.offset_y;
        cmd.data.draw_shadow.w = bounds->w;
        cmd.data.draw_shadow.h = bounds->h;
        cmd.data.draw_shadow.offset_x = style->shadow.offset_x;
        cmd.data.draw_shadow.offset_y = style->shadow.offset_y;
        cmd.data.draw_shadow.blur_radius = style->shadow.blur_radius;
        cmd.data.draw_shadow.spread_radius = style->shadow.spread_radius;
        cmd.data.draw_shadow.color = shadow_color;
        cmd.data.draw_shadow.inset = false;

        kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
    }

    /* Render background */
    if (style->background_type == IR_BACKGROUND_COLOR && style->background_color != 0) {
        uint32_t bg_color = ir_apply_opacity_to_color(style->background_color, ctx->current_opacity);

        if (style->border_radius > 0) {
            cmd.type = KRYON_CMD_DRAW_ROUNDED_RECT;
            cmd.data.draw_rounded_rect.x = bounds->x;
            cmd.data.draw_rounded_rect.y = bounds->y;
            cmd.data.draw_rounded_rect.w = bounds->w;
            cmd.data.draw_rounded_rect.h = bounds->h;
            cmd.data.draw_rounded_rect.radius = style->border_radius;
            cmd.data.draw_rounded_rect.color = bg_color;
        } else {
            cmd.type = KRYON_CMD_DRAW_RECT;
            cmd.data.draw_rect.x = bounds->x;
            cmd.data.draw_rect.y = bounds->y;
            cmd.data.draw_rect.w = bounds->w;
            cmd.data.draw_rect.h = bounds->h;
            cmd.data.draw_rect.color = bg_color;
        }

        kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
    }

    /* Render gradient background */
    if (style->background_type == IR_BACKGROUND_GRADIENT && style->gradient.stop_count > 0) {
        cmd.type = KRYON_CMD_DRAW_GRADIENT;
        cmd.data.draw_gradient.x = bounds->x;
        cmd.data.draw_gradient.y = bounds->y;
        cmd.data.draw_gradient.w = bounds->w;
        cmd.data.draw_gradient.h = bounds->h;
        cmd.data.draw_gradient.gradient_type = style->gradient.type;
        cmd.data.draw_gradient.angle = style->gradient.angle;
        cmd.data.draw_gradient.stop_count = style->gradient.stop_count;

        for (int i = 0; i < style->gradient.stop_count && i < 8; i++) {
            cmd.data.draw_gradient.stops[i].position = style->gradient.stops[i].position;
            cmd.data.draw_gradient.stops[i].color = ir_apply_opacity_to_color(
                style->gradient.stops[i].color, ctx->current_opacity
            );
        }

        kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
    }

    /* Render border */
    if (style->border_width > 0 && style->border_color != 0) {
        uint32_t border_color = ir_apply_opacity_to_color(style->border_color, ctx->current_opacity);

        /* Draw border as 4 lines (top, right, bottom, left) */
        for (int i = 0; i < style->border_width; i++) {
            /* Top */
            cmd.type = KRYON_CMD_DRAW_LINE;
            cmd.data.draw_line.x1 = bounds->x + i;
            cmd.data.draw_line.y1 = bounds->y + i;
            cmd.data.draw_line.x2 = bounds->x + bounds->w - i;
            cmd.data.draw_line.y2 = bounds->y + i;
            cmd.data.draw_line.color = border_color;
            kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

            /* Right */
            cmd.data.draw_line.x1 = bounds->x + bounds->w - i - 1;
            cmd.data.draw_line.y1 = bounds->y + i;
            cmd.data.draw_line.x2 = bounds->x + bounds->w - i - 1;
            cmd.data.draw_line.y2 = bounds->y + bounds->h - i;
            kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

            /* Bottom */
            cmd.data.draw_line.x1 = bounds->x + i;
            cmd.data.draw_line.y1 = bounds->y + bounds->h - i - 1;
            cmd.data.draw_line.x2 = bounds->x + bounds->w - i;
            cmd.data.draw_line.y2 = bounds->y + bounds->h - i - 1;
            kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

            /* Left */
            cmd.data.draw_line.x1 = bounds->x + i;
            cmd.data.draw_line.y1 = bounds->y + i;
            cmd.data.draw_line.x2 = bounds->x + i;
            cmd.data.draw_line.y2 = bounds->y + bounds->h - i;
            kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
        }
    }

    return true;
}

bool ir_gen_text_commands(IRComponent* comp, IRCommandContext* ctx, IRRect* bounds) {
    if (!comp->text_content || !comp->text_content->value) return true;

    IRTextContent* text = comp->text_content;
    IRStyle* style = comp->style;

    uint32_t text_color = style ? ir_apply_opacity_to_color(style->color, ctx->current_opacity) :
                                   ir_apply_opacity_to_color(0x000000FF, ctx->current_opacity);

    kryon_command_t cmd;
    cmd.type = KRYON_CMD_DRAW_TEXT;
    cmd.data.draw_text.x = bounds->x;
    cmd.data.draw_text.y = bounds->y;
    cmd.data.draw_text.font_id = 0; /* TODO: Resolve font from style */
    cmd.data.draw_text.font_size = style && style->font_size > 0 ? style->font_size : 16;
    cmd.data.draw_text.font_weight = style && style->font_weight == IR_FONT_WEIGHT_BOLD ? 1 : 0;
    cmd.data.draw_text.font_style = style && style->font_style == IR_FONT_STYLE_ITALIC ? 1 : 0;
    cmd.data.draw_text.color = text_color;

    /* Copy text to inline storage */
    strncpy(cmd.data.draw_text.text_storage, text->value, 127);
    cmd.data.draw_text.text_storage[127] = '\0';
    cmd.data.draw_text.text = cmd.data.draw_text.text_storage;
    cmd.data.draw_text.max_length = strlen(cmd.data.draw_text.text_storage);

    kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

    return true;
}

bool ir_gen_button_commands(IRComponent* comp, IRCommandContext* ctx, IRRect* bounds) {
    /* Render button background and border first */
    ir_gen_container_commands(comp, ctx, bounds);

    /* Render button text */
    if (comp->text_content && comp->text_content->value) {
        ir_gen_text_commands(comp, ctx, bounds);
    }

    return true;
}

bool ir_gen_input_commands(IRComponent* comp, IRCommandContext* ctx, IRRect* bounds) {
    /* Render input background and border */
    ir_gen_container_commands(comp, ctx, bounds);

    /* TODO: Render input text, caret, selection */

    return true;
}

bool ir_gen_checkbox_commands(IRComponent* comp, IRCommandContext* ctx, IRRect* bounds) {
    /* TODO: Implement checkbox rendering */
    return true;
}

bool ir_gen_dropdown_commands(IRComponent* comp, IRCommandContext* ctx, IRRect* bounds) {
    /* TODO: Implement dropdown rendering */
    return true;
}

bool ir_gen_image_commands(IRComponent* comp, IRCommandContext* ctx, IRRect* bounds) {
    /* TODO: Implement image rendering */
    return true;
}

bool ir_gen_table_commands(IRComponent* comp, IRCommandContext* ctx, IRRect* bounds) {
    /* TODO: Implement table rendering */
    return true;
}

bool ir_gen_markdown_commands(IRComponent* comp, IRCommandContext* ctx, IRRect* bounds) {
    /* TODO: Implement markdown rendering */
    return true;
}

bool ir_gen_canvas_commands(IRComponent* comp, IRCommandContext* ctx, IRRect* bounds) {
    /* TODO: Implement canvas rendering */
    return true;
}

/* ============================================================================
 * Main Component Rendering - Recursive Traversal
 * ============================================================================ */

bool ir_generate_component_commands(
    IRComponent* component,
    IRCommandContext* ctx,
    IRRect* bounds,
    float inherited_opacity
) {
    if (!component || !ctx || !bounds) return false;
    if (!component->visible) return true;

    /* Apply opacity cascading */
    float comp_opacity = component->style ? component->style->opacity : 1.0f;
    ir_push_opacity(ctx, comp_opacity);

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
            success = ir_gen_container_commands(component, ctx, bounds);
            break;

        case IR_COMPONENT_TEXT:
            success = ir_gen_text_commands(component, ctx, bounds);
            break;

        case IR_COMPONENT_BUTTON:
            success = ir_gen_button_commands(component, ctx, bounds);
            break;

        case IR_COMPONENT_INPUT:
            success = ir_gen_input_commands(component, ctx, bounds);
            break;

        case IR_COMPONENT_CHECKBOX:
            success = ir_gen_checkbox_commands(component, ctx, bounds);
            break;

        case IR_COMPONENT_DROPDOWN:
            success = ir_gen_dropdown_commands(component, ctx, bounds);
            break;

        case IR_COMPONENT_IMAGE:
            success = ir_gen_image_commands(component, ctx, bounds);
            break;

        case IR_COMPONENT_TABLE:
        case IR_COMPONENT_TABLE_HEAD:
        case IR_COMPONENT_TABLE_BODY:
        case IR_COMPONENT_TABLE_FOOT:
        case IR_COMPONENT_TABLE_ROW:
        case IR_COMPONENT_TABLE_CELL:
        case IR_COMPONENT_TABLE_HEADER_CELL:
            success = ir_gen_table_commands(component, ctx, bounds);
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
            success = ir_gen_markdown_commands(component, ctx, bounds);
            break;

        case IR_COMPONENT_CANVAS:
        case IR_COMPONENT_NATIVE_CANVAS:
            success = ir_gen_canvas_commands(component, ctx, bounds);
            break;

        default:
            /* Unknown component type - render as container */
            success = ir_gen_container_commands(component, ctx, bounds);
            break;
    }

    /* Render children */
    if (success && component->child_count > 0) {
        for (int i = 0; i < component->child_count; i++) {
            IRComponent* child = component->children[i];
            if (!child) continue;

            /* Get child bounds from layout */
            IRRect child_bounds = {
                .x = child->layout.x,
                .y = child->layout.y,
                .w = child->layout.width,
                .h = child->layout.height
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
    IRRect* bounds,
    float opacity
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
            IRRect overlay_bounds = {
                .x = overlay->layout.x,
                .y = overlay->layout.y,
                .w = overlay->layout.width,
                .h = overlay->layout.height
            };

            success = ir_generate_component_commands(overlay, &ctx, &overlay_bounds, opacity) && success;
        }

        cmd.type = KRYON_CMD_END_PASS;
        kryon_cmd_buf_push(cmd_buf, &cmd);
    }

    return success;
}
