/**
 * @file css_generator.c
 * @brief CSS Generation System Implementation
 */

#include "css_generator.h"
#include <stdio.h>
#include <string.h>

// =============================================================================
// COLOR CONVERSION
// =============================================================================

void kryon_css_color_to_string(KryonColor color, char* out_buffer, size_t buffer_size) {
    snprintf(out_buffer, buffer_size, "rgba(%d, %d, %d, %.2f)",
            (int)(color.r * 255),
            (int)(color.g * 255),
            (int)(color.b * 255),
            color.a);
}

// =============================================================================
// COMMAND-SPECIFIC CSS GENERATION
// =============================================================================

size_t kryon_css_generate_rect(const KryonDrawRectData* data,
                               const char* element_id,
                               char* out_buffer,
                               size_t buffer_size) {
    char bg_color[64];
    char border_color[64];
    kryon_css_color_to_string(data->color, bg_color, sizeof(bg_color));
    kryon_css_color_to_string(data->border_color, border_color, sizeof(border_color));

    return snprintf(out_buffer, buffer_size,
                   "#%s {\n"
                   "  position: absolute;\n"
                   "  left: %.2fpx;\n"
                   "  top: %.2fpx;\n"
                   "  width: %.2fpx;\n"
                   "  height: %.2fpx;\n"
                   "  background-color: %s;\n"
                   "  border-radius: %.2fpx;\n"
                   "  border: %.2fpx solid %s;\n"
                   "}\n",
                   element_id,
                   data->position.x, data->position.y,
                   data->size.x, data->size.y,
                   bg_color,
                   data->border_radius,
                   data->border_width, border_color);
}

size_t kryon_css_generate_text(const KryonDrawTextData* data,
                               const char* element_id,
                               char* out_buffer,
                               size_t buffer_size) {
    char text_color[64];
    kryon_css_color_to_string(data->color, text_color, sizeof(text_color));

    const char* text_align_css = "left";
    if (data->text_align == 1) text_align_css = "center";
    else if (data->text_align == 2) text_align_css = "right";

    const char* font_weight = data->bold ? "bold" : "normal";
    const char* font_style = data->italic ? "italic" : "normal";

    return snprintf(out_buffer, buffer_size,
                   "#%s {\n"
                   "  position: absolute;\n"
                   "  left: %.2fpx;\n"
                   "  top: %.2fpx;\n"
                   "  color: %s;\n"
                   "  font-size: %.2fpx;\n"
                   "  font-family: %s;\n"
                   "  font-weight: %s;\n"
                   "  font-style: %s;\n"
                   "  text-align: %s;\n"
                   "  white-space: pre-wrap;\n"
                   "}\n",
                   element_id,
                   data->position.x, data->position.y,
                   text_color,
                   data->font_size,
                   data->font_family ? data->font_family : "inherit",
                   font_weight,
                   font_style,
                   text_align_css);
}

size_t kryon_css_generate_button(const KryonDrawButtonData* data,
                                 const char* element_id,
                                 char* out_buffer,
                                 size_t buffer_size) {
    char bg_color[64];
    char text_color[64];
    char border_color[64];
    kryon_css_color_to_string(data->background_color, bg_color, sizeof(bg_color));
    kryon_css_color_to_string(data->text_color, text_color, sizeof(text_color));
    kryon_css_color_to_string(data->border_color, border_color, sizeof(border_color));

    return snprintf(out_buffer, buffer_size,
                   "#%s {\n"
                   "  position: absolute;\n"
                   "  left: %.2fpx;\n"
                   "  top: %.2fpx;\n"
                   "  width: %.2fpx;\n"
                   "  height: %.2fpx;\n"
                   "  background-color: %s;\n"
                   "  color: %s;\n"
                   "  border: %.2fpx solid %s;\n"
                   "  border-radius: %.2fpx;\n"
                   "  cursor: pointer;\n"
                   "  font-size: 16px;\n"
                   "  font-family: inherit;\n"
                   "  display: flex;\n"
                   "  align-items: center;\n"
                   "  justify-content: center;\n"
                   "  transition: background-color 0.2s;\n"
                   "}\n"
                   "#%s:hover {\n"
                   "  filter: brightness(0.9);\n"
                   "}\n"
                   "#%s:active {\n"
                   "  filter: brightness(0.8);\n"
                   "}\n",
                   element_id,
                   data->position.x, data->position.y,
                   data->size.x, data->size.y,
                   bg_color, text_color,
                   data->border_width, border_color,
                   data->border_radius,
                   element_id,
                   element_id);
}

size_t kryon_css_generate_input(const KryonDrawInputData* data,
                               const char* element_id,
                               char* out_buffer,
                               size_t buffer_size) {
    char bg_color[64];
    char text_color[64];
    char border_color[64];
    kryon_css_color_to_string(data->background_color, bg_color, sizeof(bg_color));
    kryon_css_color_to_string(data->text_color, text_color, sizeof(text_color));
    kryon_css_color_to_string(data->border_color, border_color, sizeof(border_color));

    return snprintf(out_buffer, buffer_size,
                   "#%s {\n"
                   "  position: absolute;\n"
                   "  left: %.2fpx;\n"
                   "  top: %.2fpx;\n"
                   "  width: %.2fpx;\n"
                   "  height: %.2fpx;\n"
                   "  background-color: %s;\n"
                   "  color: %s;\n"
                   "  border: %.2fpx solid %s;\n"
                   "  border-radius: %.2fpx;\n"
                   "  font-size: %.2fpx;\n"
                   "  font-family: inherit;\n"
                   "  padding: 0 8px;\n"
                   "  outline: none;\n"
                   "}\n"
                   "#%s:focus {\n"
                   "  border-color: #6495ed;\n"
                   "}\n"
                   "#%s::placeholder {\n"
                   "  color: #999;\n"
                   "}\n",
                   element_id,
                   data->position.x, data->position.y,
                   data->size.x, data->size.y,
                   bg_color, text_color,
                   data->border_width, border_color,
                   data->border_radius,
                   data->font_size,
                   element_id,
                   element_id);
}

size_t kryon_css_generate_container(const KryonBeginContainerData* data,
                                   const char* element_id,
                                   char* out_buffer,
                                   size_t buffer_size) {
    char bg_color[64];
    kryon_css_color_to_string(data->background_color, bg_color, sizeof(bg_color));

    return snprintf(out_buffer, buffer_size,
                   "#%s {\n"
                   "  position: absolute;\n"
                   "  left: %.2fpx;\n"
                   "  top: %.2fpx;\n"
                   "  width: %.2fpx;\n"
                   "  height: %.2fpx;\n"
                   "  background-color: %s;\n"
                   "  opacity: %.2f;\n"
                   "  overflow: %s;\n"
                   "}\n",
                   element_id,
                   data->position.x, data->position.y,
                   data->size.x, data->size.y,
                   bg_color,
                   data->opacity,
                   data->clip_children ? "hidden" : "visible");
}

// =============================================================================
// MAIN CSS GENERATION
// =============================================================================

size_t kryon_css_generate_for_command(const KryonRenderCommand* cmd,
                                     const char* element_id,
                                     char* out_buffer,
                                     size_t buffer_size) {
    if (!cmd || !element_id || !out_buffer) {
        return 0;
    }

    switch (cmd->type) {
        case KRYON_CMD_DRAW_RECT:
            return kryon_css_generate_rect(&cmd->data.draw_rect, element_id, out_buffer, buffer_size);

        case KRYON_CMD_DRAW_TEXT:
            return kryon_css_generate_text(&cmd->data.draw_text, element_id, out_buffer, buffer_size);

        case KRYON_CMD_DRAW_BUTTON:
            return kryon_css_generate_button(&cmd->data.draw_button, element_id, out_buffer, buffer_size);

        case KRYON_CMD_DRAW_INPUT:
            return kryon_css_generate_input(&cmd->data.draw_input, element_id, out_buffer, buffer_size);

        case KRYON_CMD_BEGIN_CONTAINER:
            return kryon_css_generate_container(&cmd->data.begin_container, element_id, out_buffer, buffer_size);

        case KRYON_CMD_DRAW_CHECKBOX: {
            const KryonDrawCheckboxData* data = &cmd->data.draw_checkbox;
            char bg_color[64];
            char border_color[64];
            kryon_css_color_to_string(data->background_color, bg_color, sizeof(bg_color));
            kryon_css_color_to_string(data->border_color, border_color, sizeof(border_color));

            return snprintf(out_buffer, buffer_size,
                           "#%s {\n"
                           "  position: absolute;\n"
                           "  left: %.2fpx;\n"
                           "  top: %.2fpx;\n"
                           "  display: flex;\n"
                           "  align-items: center;\n"
                           "  cursor: pointer;\n"
                           "}\n"
                           "#%s input[type='checkbox'] {\n"
                           "  width: 20px;\n"
                           "  height: 20px;\n"
                           "  margin-right: 8px;\n"
                           "}\n",
                           element_id,
                           data->position.x, data->position.y,
                           element_id);
        }

        case KRYON_CMD_DRAW_DROPDOWN: {
            const KryonDrawDropdownData* data = &cmd->data.draw_dropdown;
            char bg_color[64];
            char border_color[64];
            kryon_css_color_to_string(data->background_color, bg_color, sizeof(bg_color));
            kryon_css_color_to_string(data->border_color, border_color, sizeof(border_color));

            return snprintf(out_buffer, buffer_size,
                           "#%s {\n"
                           "  position: absolute;\n"
                           "  left: %.2fpx;\n"
                           "  top: %.2fpx;\n"
                           "  width: %.2fpx;\n"
                           "  height: %.2fpx;\n"
                           "  background-color: %s;\n"
                           "  border: %.2fpx solid %s;\n"
                           "  border-radius: %.2fpx;\n"
                           "  padding: 0 8px;\n"
                           "  font-size: 14px;\n"
                           "  cursor: pointer;\n"
                           "}\n",
                           element_id,
                           data->position.x, data->position.y,
                           data->size.x, data->size.y,
                           bg_color,
                           data->border_width, border_color,
                           data->border_radius);
        }

        default:
            return 0;
    }
}