#define _GNU_SOURCE
#include "ir_wireframe.h"
#include "ir_to_commands.h"
#include "desktop_internal.h"
#include "../ir/ir_core.h"
#include "../ir/ir_log.h"
#include "../../core/include/kryon.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static ir_wireframe_config_t g_config = {0};
static bool config_loaded = false;

/* Parse hex color string (#RRGGBB or #RRGGBBAA) */
static bool parse_color(const char* str, uint32_t* out_color) {
    if (!str || str[0] != '#') return false;

    size_t len = strlen(str);
    if (len != 7 && len != 9) return false;

    uint32_t color = 0;
    for (size_t i = 1; i < len; i++) {
        char c = str[i];
        color <<= 4;
        if (c >= '0' && c <= '9') color |= (c - '0');
        else if (c >= 'a' && c <= 'f') color |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') color |= (c - 'A' + 10);
        else return false;
    }

    /* Add alpha if not specified */
    if (len == 7) color = (color << 8) | 0xFF;

    *out_color = color;
    return true;
}

void ir_wireframe_load_config(ir_wireframe_config_t* config) {
    memset(config, 0, sizeof(*config));

    /* Check if wireframe mode is enabled */
    const char* env = getenv("KRYON_WIREFRAME");
    if (env && (strcmp(env, "1") == 0 || strcmp(env, "true") == 0 ||
                 strcmp(env, "yes") == 0)) {
        config->enabled = true;
    }

    if (!config->enabled) return;

    /* Parse wireframe color */
    const char* color_str = getenv("KRYON_WIREFRAME_COLOR");
    if (color_str) {
        if (!parse_color(color_str, &config->color)) {
            IR_LOG_WARN("DESKTOP", "Invalid wireframe color: %s", color_str);
            config->color = 0xFF0000FF; /* Default red */
        }
    } else {
        config->color = 0xFF0000FF;
    }

    /* Parse display options */
    env = getenv("KRYON_WIREFRAME_SHOW_FOR_EACH");
    config->show_for_each = (env && (strcmp(env, "1") == 0 ||
                                     strcmp(env, "true") == 0));

    env = getenv("KRYON_WIREFRAME_SHOW_REACTIVE");
    config->show_reactive = (env && (strcmp(env, "1") == 0 ||
                                     strcmp(env, "true") == 0));

    env = getenv("KRYON_WIREFRAME_SHOW_TEXT_EXPR");
    config->show_text_expr = (env && (strcmp(env, "1") == 0 ||
                                     strcmp(env, "true") == 0));

    env = getenv("KRYON_WIREFRAME_SHOW_CONDITIONAL");
    config->show_conditional = (env && (strcmp(env, "1") == 0 ||
                                       strcmp(env, "true") == 0));
}

bool ir_wireframe_is_enabled(void) {
    if (!config_loaded) {
        ir_wireframe_load_config(&g_config);
        config_loaded = true;
    }
    return g_config.enabled;
}

uint32_t ir_wireframe_get_color(void) {
    if (!config_loaded) {
        ir_wireframe_load_config(&g_config);
        config_loaded = true;
    }
    return g_config.color;
}

bool ir_wireframe_should_render(void* component_ptr, ir_wireframe_config_t* config) {
    if (!config->enabled) return false;

    IRComponent* component = (IRComponent*)component_ptr;

    /* Check for ForEach components */
    if (config->show_for_each) {
        if (component->type == IR_COMPONENT_FOR_EACH ||
            component->type == IR_COMPONENT_FOR_LOOP) {
            return true;
        }
        /* Also check for foreach_def field (new system) */
        if (component->foreach_def) {
            return true;
        }
    }

    /* Check for reactive text expressions */
    if (config->show_text_expr && component->text_expression) {
        return true;
    }

    /* Check for conditional rendering */
    if (config->show_conditional && component->visible_condition) {
        return true;
    }

    /* Check for property bindings (reactive state) */
    if (config->show_reactive && component->property_binding_count > 0) {
        return true;
    }

    return false;
}

bool ir_wireframe_render_outline(
    void* component_ptr,
    void* ctx_ptr,
    void* bounds_ptr,
    uint32_t color,
    const char* label
) {
    (void)component_ptr; /* May be used for additional info in future */
    IRCommandContext* ctx = (IRCommandContext*)ctx_ptr;
    LayoutRect* bounds = (LayoutRect*)bounds_ptr;

    kryon_command_t cmd = {0};

    /* Draw rectangle outline using 4 lines */
    cmd.type = KRYON_CMD_DRAW_LINE;
    cmd.data.draw_line.color = color;

    /* Top edge */
    cmd.data.draw_line.x1 = bounds->x;
    cmd.data.draw_line.y1 = bounds->y;
    cmd.data.draw_line.x2 = bounds->x + bounds->width;
    cmd.data.draw_line.y2 = bounds->y;
    kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

    /* Bottom edge */
    cmd.data.draw_line.x1 = bounds->x;
    cmd.data.draw_line.y1 = bounds->y + bounds->height;
    cmd.data.draw_line.x2 = bounds->x + bounds->width;
    cmd.data.draw_line.y2 = bounds->y + bounds->height;
    kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

    /* Left edge */
    cmd.data.draw_line.x1 = bounds->x;
    cmd.data.draw_line.y1 = bounds->y;
    cmd.data.draw_line.x2 = bounds->x;
    cmd.data.draw_line.y2 = bounds->y + bounds->height;
    kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

    /* Right edge */
    cmd.data.draw_line.x1 = bounds->x + bounds->width;
    cmd.data.draw_line.y1 = bounds->y;
    cmd.data.draw_line.x2 = bounds->x + bounds->width;
    cmd.data.draw_line.y2 = bounds->y + bounds->height;
    kryon_cmd_buf_push(ctx->cmd_buf, &cmd);

    /* Draw label if provided */
    if (label) {
        cmd.type = KRYON_CMD_DRAW_TEXT;
        cmd.data.draw_text.x = (int16_t)(bounds->x + 5);
        cmd.data.draw_text.y = (int16_t)(bounds->y + 5);
        cmd.data.draw_text.color = color;
        strncpy(cmd.data.draw_text.text_storage, label, 127);
        cmd.data.draw_text.text_storage[127] = '\0';
        cmd.data.draw_text.text = cmd.data.draw_text.text_storage;
        kryon_cmd_buf_push(ctx->cmd_buf, &cmd);
    }

    return true;
}
