/**
 * @file dropdown_renderer.c
 * @brief Kryon Dropdown Renderer - HTML Select Mapping
 */

#include "internal/widgets.h"
#include "internal/renderer_interface.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// HTML SELECT ELEMENT MAPPING
// =============================================================================

typedef struct {
    char* html_output;
    size_t html_capacity;
    size_t html_length;
    int indent_level;
} KryonHTMLContext;

static bool html_ensure_capacity(KryonHTMLContext* ctx, size_t needed) {
    if (ctx->html_capacity >= needed) return true;
    
    size_t new_capacity = ctx->html_capacity == 0 ? 1024 : ctx->html_capacity;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }
    
    char* new_html = kryon_alloc(new_capacity);
    if (!new_html) return false;
    
    if (ctx->html_output && ctx->html_length > 0) {
        memcpy(new_html, ctx->html_output, ctx->html_length);
        kryon_free(ctx->html_output);
    }
    
    ctx->html_output = new_html;
    ctx->html_capacity = new_capacity;
    return true;
}

static void html_append(KryonHTMLContext* ctx, const char* text) {
    if (!ctx || !text) return;
    
    size_t text_len = strlen(text);
    if (!html_ensure_capacity(ctx, ctx->html_length + text_len + 1)) return;
    
    strcpy(ctx->html_output + ctx->html_length, text);
    ctx->html_length += text_len;
}

static void html_append_formatted(KryonHTMLContext* ctx, const char* format, ...) {
    if (!ctx || !format) return;
    
    va_list args;
    va_start(args, format);
    
    // Calculate needed size
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (needed <= 0) {
        va_end(args);
        return;
    }
    
    if (!html_ensure_capacity(ctx, ctx->html_length + needed + 1)) {
        va_end(args);
        return;
    }
    
    vsnprintf(ctx->html_output + ctx->html_length, needed + 1, format, args);
    ctx->html_length += needed;
    
    va_end(args);
}

static void html_indent(KryonHTMLContext* ctx) {
    for (int i = 0; i < ctx->indent_level; i++) {
        html_append(ctx, "  ");
    }
}

// =============================================================================
// HTML SELECT GENERATION
// =============================================================================

static char* escape_html_attribute(const char* text) {
    if (!text) return NULL;
    
    size_t len = strlen(text);
    size_t escaped_len = len * 6 + 1; // Worst case: all chars need escaping
    char* escaped = kryon_alloc(escaped_len);
    if (!escaped) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (text[i]) {
            case '"':
                strcpy(&escaped[j], "&quot;");
                j += 6;
                break;
            case '&':
                strcpy(&escaped[j], "&amp;");
                j += 5;
                break;
            case '<':
                strcpy(&escaped[j], "&lt;");
                j += 4;
                break;
            case '>':
                strcpy(&escaped[j], "&gt;");
                j += 4;
                break;
            default:
                escaped[j++] = text[i];
                break;
        }
    }
    escaped[j] = '\0';
    
    return escaped;
}

static char* color_to_css(KryonColor color) {
    char* css_color = kryon_alloc(32);
    if (!css_color) return NULL;
    
    if (color.a < 1.0f) {
        snprintf(css_color, 32, "rgba(%d,%d,%d,%.2f)", 
                (int)(color.r * 255), (int)(color.g * 255), 
                (int)(color.b * 255), color.a);
    } else {
        snprintf(css_color, 32, "rgb(%d,%d,%d)", 
                (int)(color.r * 255), (int)(color.g * 255), (int)(color.b * 255));
    }
    
    return css_color;
}

char* kryon_dropdown_to_html_select(KryonDropdownWidget* dropdown) {
    if (!dropdown) return NULL;
    
    KryonHTMLContext ctx = {0};
    
    // Generate CSS classes for styling
    html_append(&ctx, "<style>\n");
    html_append(&ctx, ".kryon-dropdown {\n");
    
    char* bg_color = color_to_css(dropdown->base.style.background_color);
    char* text_color = color_to_css(dropdown->base.style.text_color);
    char* border_color = color_to_css(dropdown->base.style.border_color);
    
    html_append_formatted(&ctx, "  background-color: %s;\n", bg_color);
    html_append_formatted(&ctx, "  color: %s;\n", text_color);
    html_append_formatted(&ctx, "  border: %.1fpx solid %s;\n", 
                         dropdown->base.style.border_width, border_color);
    html_append_formatted(&ctx, "  border-radius: %.1fpx;\n", dropdown->base.style.border_radius);
    html_append_formatted(&ctx, "  font-size: %.1fpx;\n", dropdown->base.style.font_size);
    html_append_formatted(&ctx, "  padding: %.1fpx %.1fpx %.1fpx %.1fpx;\n",
                         dropdown->base.style.padding.top, dropdown->base.style.padding.right,
                         dropdown->base.style.padding.bottom, dropdown->base.style.padding.left);
    
    if (dropdown->base.bounds.size.x > 0) {
        html_append_formatted(&ctx, "  width: %.1fpx;\n", dropdown->base.bounds.size.x);
    }
    if (dropdown->base.bounds.size.y > 0) {
        html_append_formatted(&ctx, "  height: %.1fpx;\n", dropdown->base.bounds.size.y);
    }
    
    html_append(&ctx, "  appearance: none;\n");
    html_append(&ctx, "  -webkit-appearance: none;\n");
    html_append(&ctx, "  -moz-appearance: none;\n");
    html_append(&ctx, "  cursor: pointer;\n");
    html_append(&ctx, "}\n");
    
    // Hover state
    html_append(&ctx, ".kryon-dropdown:hover {\n");
    html_append(&ctx, "  filter: brightness(1.05);\n");
    html_append(&ctx, "}\n");
    
    // Focus state
    html_append(&ctx, ".kryon-dropdown:focus {\n");
    html_append(&ctx, "  outline: 2px solid #4285f4;\n");
    html_append(&ctx, "  outline-offset: 1px;\n");
    html_append(&ctx, "}\n");
    
    // Disabled state
    html_append(&ctx, ".kryon-dropdown:disabled {\n");
    html_append(&ctx, "  opacity: 0.5;\n");
    html_append(&ctx, "  cursor: not-allowed;\n");
    html_append(&ctx, "}\n");
    
    html_append(&ctx, "</style>\n\n");
    
    // Generate select element
    html_append(&ctx, "<select");
    
    // Add ID attribute
    if (dropdown->base.id) {
        char* escaped_id = escape_html_attribute(dropdown->base.id);
        if (escaped_id) {
            html_append_formatted(&ctx, " id=\"%s\"", escaped_id);
            kryon_free(escaped_id);
        }
    }
    
    // Add name attribute (same as ID if not specified)
    if (dropdown->base.id) {
        char* escaped_name = escape_html_attribute(dropdown->base.id);
        if (escaped_name) {
            html_append_formatted(&ctx, " name=\"%s\"", escaped_name);
            kryon_free(escaped_name);
        }
    }
    
    // Add CSS class
    html_append(&ctx, " class=\"kryon-dropdown\"");
    
    // Add disabled attribute if needed
    if (dropdown->base.state == KRYON_WIDGET_STATE_DISABLED) {
        html_append(&ctx, " disabled");
    }
    
    // Add multiple attribute for multiselect
    if (dropdown->is_multiselect) {
        html_append(&ctx, " multiple");
        
        if (dropdown->max_height > 0) {
            int visible_options = (int)(dropdown->max_height / dropdown->item_height);
            html_append_formatted(&ctx, " size=\"%d\"", visible_options);
        }
    }
    
    // Add change event handler
    html_append(&ctx, " onchange=\"handleDropdownChange(this)\"");
    
    html_append(&ctx, ">\n");
    ctx.indent_level++;
    
    // Generate option elements
    for (size_t i = 0; i < dropdown->option_count; i++) {
        KryonDropdownOption* option = &dropdown->options[i];
        
        html_indent(&ctx);
        html_append(&ctx, "<option");
        
        // Add value attribute
        if (option->value) {
            char* escaped_value = escape_html_attribute(option->value);
            if (escaped_value) {
                html_append_formatted(&ctx, " value=\"%s\"", escaped_value);
                kryon_free(escaped_value);
            }
        }
        
        // Add selected attribute
        if ((int)i == dropdown->selected_index) {
            html_append(&ctx, " selected");
        }
        
        // Add disabled attribute if needed
        if (!option->enabled) {
            html_append(&ctx, " disabled");
        }
        
        html_append(&ctx, ">");
        
        // Add option text (escaped)
        if (option->text) {
            char* escaped_text = escape_html_attribute(option->text);
            if (escaped_text) {
                html_append(&ctx, escaped_text);
                kryon_free(escaped_text);
            }
        }
        
        html_append(&ctx, "</option>\n");
    }
    
    ctx.indent_level--;
    html_append(&ctx, "</select>\n");
    
    // Add JavaScript for event handling
    html_append(&ctx, "\n<script>\n");
    html_append(&ctx, "function handleDropdownChange(select) {\n");
    html_append(&ctx, "  // Kryon dropdown change handler\n");
    html_append(&ctx, "  const selectedIndex = select.selectedIndex;\n");
    html_append(&ctx, "  const selectedValue = select.value;\n");
    html_append(&ctx, "  const selectedText = select.options[selectedIndex].text;\n");
    html_append(&ctx, "  \n");
    html_append(&ctx, "  // Dispatch custom event\n");
    html_append(&ctx, "  const event = new CustomEvent('kryonDropdownChange', {\n");
    html_append(&ctx, "    detail: {\n");
    html_append(&ctx, "      element: select,\n");
    html_append(&ctx, "      selectedIndex: selectedIndex,\n");
    html_append(&ctx, "      selectedValue: selectedValue,\n");
    html_append(&ctx, "      selectedText: selectedText\n");
    html_append(&ctx, "    }\n");
    html_append(&ctx, "  });\n");
    html_append(&ctx, "  \n");
    html_append(&ctx, "  select.dispatchEvent(event);\n");
    html_append(&ctx, "  \n");
    html_append(&ctx, "  // Call Kryon callback if available\n");
    
    if (dropdown->base.id) {
        html_append_formatted(&ctx, "  if (window.kryonCallbacks && window.kryonCallbacks['%s']) {\n", dropdown->base.id);
        html_append_formatted(&ctx, "    window.kryonCallbacks['%s'](selectedIndex, selectedValue, selectedText);\n", dropdown->base.id);
        html_append(&ctx, "  }\n");
    }
    
    html_append(&ctx, "}\n");
    html_append(&ctx, "</script>\n");
    
    // Clean up
    kryon_free(bg_color);
    kryon_free(text_color);
    kryon_free(border_color);
    
    return ctx.html_output;
}

// =============================================================================
// REACT/VUE COMPONENT GENERATION
// =============================================================================

char* kryon_dropdown_to_react_select(KryonDropdownWidget* dropdown) {
    if (!dropdown) return NULL;
    
    KryonHTMLContext ctx = {0};
    
    html_append(&ctx, "import React, { useState } from 'react';\n\n");
    
    // Component function
    char* component_name = dropdown->base.id ? dropdown->base.id : "KryonDropdown";
    html_append_formatted(&ctx, "const %s = ({ onChange, ...props }) => {\n", component_name);
    ctx.indent_level++;
    
    // State
    html_indent(&ctx);
    html_append_formatted(&ctx, "const [selectedValue, setSelectedValue] = useState('%s');\n",
                         dropdown->selected_index >= 0 ? dropdown->options[dropdown->selected_index].value : "");
    
    // Event handler
    html_append(&ctx, "\n");
    html_indent(&ctx);
    html_append(&ctx, "const handleChange = (event) => {\n");
    ctx.indent_level++;
    html_indent(&ctx);
    html_append(&ctx, "const newValue = event.target.value;\n");
    html_indent(&ctx);
    html_append(&ctx, "const selectedIndex = event.target.selectedIndex;\n");
    html_indent(&ctx);
    html_append(&ctx, "setSelectedValue(newValue);\n");
    html_indent(&ctx);
    html_append(&ctx, "if (onChange) {\n");
    ctx.indent_level++;
    html_indent(&ctx);
    html_append(&ctx, "onChange(selectedIndex, newValue, event.target.options[selectedIndex].text);\n");
    ctx.indent_level--;
    html_indent(&ctx);
    html_append(&ctx, "}\n");
    ctx.indent_level--;
    html_indent(&ctx);
    html_append(&ctx, "};\n");
    
    // Style object
    html_append(&ctx, "\n");
    html_indent(&ctx);
    html_append(&ctx, "const dropdownStyle = {\n");
    ctx.indent_level++;
    
    char* bg_color = color_to_css(dropdown->base.style.background_color);
    char* text_color = color_to_css(dropdown->base.style.text_color);
    char* border_color = color_to_css(dropdown->base.style.border_color);
    
    html_indent(&ctx);
    html_append_formatted(&ctx, "backgroundColor: '%s',\n", bg_color);
    html_indent(&ctx);
    html_append_formatted(&ctx, "color: '%s',\n", text_color);
    html_indent(&ctx);
    html_append_formatted(&ctx, "border: '%.1fpx solid %s',\n", 
                         dropdown->base.style.border_width, border_color);
    html_indent(&ctx);
    html_append_formatted(&ctx, "borderRadius: '%.1fpx',\n", dropdown->base.style.border_radius);
    html_indent(&ctx);
    html_append_formatted(&ctx, "fontSize: '%.1fpx',\n", dropdown->base.style.font_size);
    html_indent(&ctx);
    html_append_formatted(&ctx, "padding: '%.1fpx %.1fpx %.1fpx %.1fpx',\n",
                         dropdown->base.style.padding.top, dropdown->base.style.padding.right,
                         dropdown->base.style.padding.bottom, dropdown->base.style.padding.left);
    
    if (dropdown->base.bounds.size.x > 0) {
        html_indent(&ctx);
        html_append_formatted(&ctx, "width: '%.1fpx',\n", dropdown->base.bounds.size.x);
    }
    if (dropdown->base.bounds.size.y > 0) {
        html_indent(&ctx);
        html_append_formatted(&ctx, "height: '%.1fpx',\n", dropdown->base.bounds.size.y);
    }
    
    html_indent(&ctx);
    html_append(&ctx, "appearance: 'none',\n");
    html_indent(&ctx);
    html_append(&ctx, "cursor: 'pointer'\n");
    
    ctx.indent_level--;
    html_indent(&ctx);
    html_append(&ctx, "};\n");
    
    // Return JSX
    html_append(&ctx, "\n");
    html_indent(&ctx);
    html_append(&ctx, "return (\n");
    ctx.indent_level++;
    html_indent(&ctx);
    html_append(&ctx, "<select\n");
    ctx.indent_level++;
    html_indent(&ctx);
    html_append(&ctx, "value={selectedValue}\n");
    html_indent(&ctx);
    html_append(&ctx, "onChange={handleChange}\n");
    html_indent(&ctx);
    html_append(&ctx, "style={dropdownStyle}\n");
    html_indent(&ctx);
    html_append(&ctx, "disabled={");
    html_append(&ctx, dropdown->base.state == KRYON_WIDGET_STATE_DISABLED ? "true" : "false");
    html_append(&ctx, "}\n");
    
    if (dropdown->is_multiselect) {
        html_indent(&ctx);
        html_append(&ctx, "multiple\n");
    }
    
    html_indent(&ctx);
    html_append(&ctx, "{...props}\n");
    ctx.indent_level--;
    html_indent(&ctx);
    html_append(&ctx, ">\n");
    
    // Options
    ctx.indent_level++;
    for (size_t i = 0; i < dropdown->option_count; i++) {
        KryonDropdownOption* option = &dropdown->options[i];
        
        html_indent(&ctx);
        html_append_formatted(&ctx, "<option value=\"%s\"", option->value ? option->value : "");
        
        if (!option->enabled) {
            html_append(&ctx, " disabled");
        }
        
        html_append(&ctx, ">\n");
        ctx.indent_level++;
        html_indent(&ctx);
        html_append_formatted(&ctx, "%s\n", option->text ? option->text : "");
        ctx.indent_level--;
        html_indent(&ctx);
        html_append(&ctx, "</option>\n");
    }
    
    ctx.indent_level--;
    html_indent(&ctx);
    html_append(&ctx, "</select>\n");
    
    ctx.indent_level--;
    html_indent(&ctx);
    html_append(&ctx, ");\n");
    
    ctx.indent_level--;
    html_append(&ctx, "};\n\n");
    
    html_append_formatted(&ctx, "export default %s;\n", component_name);
    
    // Clean up
    kryon_free(bg_color);
    kryon_free(text_color);
    kryon_free(border_color);
    
    return ctx.html_output;
}

// =============================================================================
// RENDER COMMAND GENERATION
// =============================================================================

KryonRenderCommand kryon_dropdown_to_render_command(KryonDropdownWidget* dropdown) {
    KryonRenderCommand cmd = {0};
    
    if (!dropdown) return cmd;
    
    cmd.type = KRYON_CMD_DRAW_DROPDOWN;
    cmd.z_index = 0;
    
    // Copy dropdown data
    KryonDrawDropdownData* data = &cmd.data.draw_dropdown;
    
    if (dropdown->base.id) {
        data->widget_id = kryon_alloc(strlen(dropdown->base.id) + 1);
        if (data->widget_id) {
            strcpy(data->widget_id, dropdown->base.id);
        }
    }
    
    data->position = dropdown->base.bounds.position;
    data->size = dropdown->base.bounds.size;
    data->selected_index = dropdown->selected_index;
    data->hovered_index = dropdown->hovered_index;
    data->is_open = dropdown->is_open;
    
    // Copy style properties
    data->background_color = dropdown->base.style.background_color;
    data->text_color = dropdown->base.style.text_color;
    data->border_color = dropdown->base.style.border_color;
    data->hover_color = (KryonColor){0.9f, 0.95f, 1.0f, 1.0f};
    data->selected_color = (KryonColor){0.2f, 0.4f, 0.8f, 1.0f};
    data->border_width = dropdown->base.style.border_width;
    data->border_radius = dropdown->base.style.border_radius;
    data->state = dropdown->base.state;
    data->enabled = (dropdown->base.state != KRYON_WIDGET_STATE_DISABLED);
    
    // Copy options
    if (dropdown->option_count > 0) {
        data->items = kryon_alloc(sizeof(KryonDropdownItem) * dropdown->option_count);
        if (data->items) {
            for (size_t i = 0; i < dropdown->option_count; i++) {
                KryonDropdownItem* item = &data->items[i];
                KryonDropdownOption* option = &dropdown->options[i];
                
                if (option->text) {
                    item->text = kryon_alloc(strlen(option->text) + 1);
                    if (item->text) {
                        strcpy(item->text, option->text);
                    }
                }
                
                if (option->value) {
                    item->value = kryon_alloc(strlen(option->value) + 1);
                    if (item->value) {
                        strcpy(item->value, option->value);
                    }
                }
            }
            data->item_count = dropdown->option_count;
        }
    }
    
    return cmd;
}