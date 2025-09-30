/**
 * @file dom_manager.c
 * @brief DOM Element Generation Implementation
 */

#include "dom_manager.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

size_t kryon_dom_escape_html(const char* input,
                            char* out_buffer,
                            size_t buffer_size) {
    if (!input || !out_buffer) return 0;

    size_t pos = 0;
    for (const char* p = input; *p && pos < buffer_size - 1; p++) {
        switch (*p) {
            case '<':
                if (pos + 4 < buffer_size) {
                    memcpy(out_buffer + pos, "&lt;", 4);
                    pos += 4;
                }
                break;
            case '>':
                if (pos + 4 < buffer_size) {
                    memcpy(out_buffer + pos, "&gt;", 4);
                    pos += 4;
                }
                break;
            case '&':
                if (pos + 5 < buffer_size) {
                    memcpy(out_buffer + pos, "&amp;", 5);
                    pos += 5;
                }
                break;
            case '"':
                if (pos + 6 < buffer_size) {
                    memcpy(out_buffer + pos, "&quot;", 6);
                    pos += 6;
                }
                break;
            case '\'':
                if (pos + 6 < buffer_size) {
                    memcpy(out_buffer + pos, "&#39;", 5);
                    pos += 5;
                }
                break;
            default:
                out_buffer[pos++] = *p;
                break;
        }
    }
    out_buffer[pos] = '\0';
    return pos;
}

size_t kryon_dom_open_tag(const char* tag_name,
                         const char* element_id,
                         const char* class_name,
                         char* out_buffer,
                         size_t buffer_size) {
    if (class_name) {
        return snprintf(out_buffer, buffer_size, "<%s id=\"%s\" class=\"%s\">",
                       tag_name, element_id, class_name);
    } else {
        return snprintf(out_buffer, buffer_size, "<%s id=\"%s\">",
                       tag_name, element_id);
    }
}

size_t kryon_dom_close_tag(const char* tag_name,
                          char* out_buffer,
                          size_t buffer_size) {
    return snprintf(out_buffer, buffer_size, "</%s>\n", tag_name);
}

// =============================================================================
// ELEMENT-SPECIFIC HTML GENERATION
// =============================================================================

size_t kryon_dom_generate_button(const KryonDrawButtonData* data,
                                const char* element_id,
                                char* out_buffer,
                                size_t buffer_size) {
    char escaped_text[512];
    if (data->text) {
        kryon_dom_escape_html(data->text, escaped_text, sizeof(escaped_text));
    } else {
        escaped_text[0] = '\0';
    }

    const char* onclick_attr = "";
    char onclick_buf[256] = "";
    if (data->element_id && strlen(data->element_id) > 0) {
        snprintf(onclick_buf, sizeof(onclick_buf),
                " onclick=\"kryonHandleClick('%s')\"", data->element_id);
        onclick_attr = onclick_buf;
    }

    return snprintf(out_buffer, buffer_size,
                   "<button id=\"%s\" class=\"kryon-button\"%s>%s</button>\n",
                   element_id, onclick_attr, escaped_text);
}

size_t kryon_dom_generate_input(const KryonDrawInputData* data,
                               const char* element_id,
                               char* out_buffer,
                               size_t buffer_size) {
    char escaped_text[512] = "";
    char escaped_placeholder[512] = "";

    if (data->text && strlen(data->text) > 0) {
        kryon_dom_escape_html(data->text, escaped_text, sizeof(escaped_text));
    }
    if (data->placeholder && strlen(data->placeholder) > 0) {
        kryon_dom_escape_html(data->placeholder, escaped_placeholder, sizeof(escaped_placeholder));
    }

    const char* input_type = data->is_password ? "password" : "text";

    char oninput_buf[256] = "";
    if (data->element_id && strlen(data->element_id) > 0) {
        snprintf(oninput_buf, sizeof(oninput_buf),
                " oninput=\"kryonHandleInput('%s', this.value)\"", data->element_id);
    }

    return snprintf(out_buffer, buffer_size,
                   "<input type=\"%s\" id=\"%s\" class=\"kryon-input\" "
                   "value=\"%s\" placeholder=\"%s\"%s>\n",
                   input_type, element_id, escaped_text, escaped_placeholder, oninput_buf);
}

size_t kryon_dom_generate_text(const KryonDrawTextData* data,
                              const char* element_id,
                              char* out_buffer,
                              size_t buffer_size) {
    char escaped_text[1024];
    if (data->text) {
        kryon_dom_escape_html(data->text, escaped_text, sizeof(escaped_text));
    } else {
        escaped_text[0] = '\0';
    }

    return snprintf(out_buffer, buffer_size,
                   "<span id=\"%s\" class=\"kryon-text\">%s</span>\n",
                   element_id, escaped_text);
}

size_t kryon_dom_generate_container(const KryonBeginContainerData* data,
                                   const char* element_id,
                                   char* out_buffer,
                                   size_t buffer_size) {
    return snprintf(out_buffer, buffer_size,
                   "<div id=\"%s\" class=\"kryon-container\">\n",
                   element_id);
}

size_t kryon_dom_generate_checkbox(const KryonDrawCheckboxData* data,
                                  const char* element_id,
                                  char* out_buffer,
                                  size_t buffer_size) {
    char escaped_label[512] = "";
    if (data->label) {
        kryon_dom_escape_html(data->label, escaped_label, sizeof(escaped_label));
    }

    const char* checked = data->checked ? " checked" : "";

    char onchange_buf[256] = "";
    if (data->element_id && strlen(data->element_id) > 0) {
        snprintf(onchange_buf, sizeof(onchange_buf),
                " onchange=\"kryonHandleCheckbox('%s', this.checked)\"", data->element_id);
    }

    return snprintf(out_buffer, buffer_size,
                   "<label id=\"%s\" class=\"kryon-checkbox\">"
                   "<input type=\"checkbox\"%s%s>%s</label>\n",
                   element_id, checked, onchange_buf, escaped_label);
}

size_t kryon_dom_generate_dropdown(const KryonDrawDropdownData* data,
                                  const char* element_id,
                                  char* out_buffer,
                                  size_t buffer_size) {
    char onchange_buf[256] = "";
    if (data->element_id && strlen(data->element_id) > 0) {
        snprintf(onchange_buf, sizeof(onchange_buf),
                " onchange=\"kryonHandleDropdown('%s', this.value)\"", data->element_id);
    }

    size_t pos = snprintf(out_buffer, buffer_size,
                         "<select id=\"%s\" class=\"kryon-dropdown\"%s>\n",
                         element_id, onchange_buf);

    if (data->items && data->item_count > 0) {
        for (size_t i = 0; i < data->item_count && pos < buffer_size; i++) {
            const char* selected = (i == (size_t)data->selected_index) ? " selected" : "";
            char escaped_text[256];
            if (data->items[i].text) {
                kryon_dom_escape_html(data->items[i].text, escaped_text, sizeof(escaped_text));
            } else {
                escaped_text[0] = '\0';
            }

            pos += snprintf(out_buffer + pos, buffer_size - pos,
                           "  <option value=\"%zu\"%s>%s</option>\n",
                           i, selected, escaped_text);
        }
    }

    pos += snprintf(out_buffer + pos, buffer_size - pos, "</select>\n");
    return pos;
}

// =============================================================================
// MAIN DOM GENERATION
// =============================================================================

size_t kryon_dom_generate_for_command(const KryonRenderCommand* cmd,
                                     const char* element_id,
                                     char* out_buffer,
                                     size_t buffer_size) {
    if (!cmd || !element_id || !out_buffer) {
        return 0;
    }

    switch (cmd->type) {
        case KRYON_CMD_DRAW_RECT:
            // Draw rect as div
            return snprintf(out_buffer, buffer_size,
                           "<div id=\"%s\" class=\"kryon-rect\"></div>\n",
                           element_id);

        case KRYON_CMD_DRAW_TEXT:
            return kryon_dom_generate_text(&cmd->data.draw_text, element_id, out_buffer, buffer_size);

        case KRYON_CMD_DRAW_BUTTON:
            return kryon_dom_generate_button(&cmd->data.draw_button, element_id, out_buffer, buffer_size);

        case KRYON_CMD_DRAW_INPUT:
            return kryon_dom_generate_input(&cmd->data.draw_input, element_id, out_buffer, buffer_size);

        case KRYON_CMD_BEGIN_CONTAINER:
            return kryon_dom_generate_container(&cmd->data.begin_container, element_id, out_buffer, buffer_size);

        case KRYON_CMD_END_CONTAINER:
            return snprintf(out_buffer, buffer_size, "</div>\n");

        case KRYON_CMD_DRAW_CHECKBOX:
            return kryon_dom_generate_checkbox(&cmd->data.draw_checkbox, element_id, out_buffer, buffer_size);

        case KRYON_CMD_DRAW_DROPDOWN:
            return kryon_dom_generate_dropdown(&cmd->data.draw_dropdown, element_id, out_buffer, buffer_size);

        case KRYON_CMD_DRAW_IMAGE: {
            const KryonDrawImageData* data = &cmd->data.draw_image;
            return snprintf(out_buffer, buffer_size,
                           "<img id=\"%s\" class=\"kryon-image\" src=\"%s\" "
                           "style=\"opacity: %.2f;\">\n",
                           element_id, data->source ? data->source : "", data->opacity);
        }

        default:
            return 0;
    }
}