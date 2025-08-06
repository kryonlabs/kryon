/**
 * @file dropdown_widget.c
 * @brief Kryon Dropdown Widget Implementation
 */

#include "internal/widgets.h"
#include "internal/renderer_interface.h"
#include "internal/events.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// =============================================================================
// DROPDOWN WIDGET STRUCTURES
// =============================================================================

typedef struct {
    char* text;
    char* value;
    bool enabled;
    void* user_data;
} KryonDropdownOption;

typedef struct {
    // Base widget
    KryonWidget base;
    
    // Dropdown-specific properties
    KryonDropdownOption* options;
    size_t option_count;
    size_t option_capacity;
    
    // State
    int selected_index;
    int hovered_index;
    bool is_open;
    bool is_multiselect;
    
    // Visual properties
    float max_height;
    float item_height;
    bool show_icons;
    bool searchable;
    char* search_text;
    
    // Callbacks
    KryonDropdownChangeCallback on_change;
    KryonDropdownOpenCallback on_open;
    KryonDropdownCloseCallback on_close;
    void* callback_user_data;
    
    // Animation state
    float open_animation;
    float target_animation;
    bool animating;
    
    // Scroll state for long lists
    float scroll_offset;
    float max_scroll;
    
} KryonDropdownWidget;

// =============================================================================
// DROPDOWN OPTION MANAGEMENT
// =============================================================================

static bool dropdown_ensure_capacity(KryonDropdownWidget* dropdown, size_t needed_capacity) {
    if (dropdown->option_capacity >= needed_capacity) return true;
    
    size_t new_capacity = dropdown->option_capacity == 0 ? 8 : dropdown->option_capacity;
    while (new_capacity < needed_capacity) {
        new_capacity *= 2;
    }
    
    KryonDropdownOption* new_options = kryon_alloc(sizeof(KryonDropdownOption) * new_capacity);
    if (!new_options) return false;
    
    if (dropdown->options && dropdown->option_count > 0) {
        memcpy(new_options, dropdown->options, sizeof(KryonDropdownOption) * dropdown->option_count);
        kryon_free(dropdown->options);
    }
    
    dropdown->options = new_options;
    dropdown->option_capacity = new_capacity;
    return true;
}

static void dropdown_clear_options(KryonDropdownWidget* dropdown) {
    if (!dropdown->options) return;
    
    for (size_t i = 0; i < dropdown->option_count; i++) {
        kryon_free(dropdown->options[i].text);
        kryon_free(dropdown->options[i].value);
    }
    
    dropdown->option_count = 0;
    dropdown->selected_index = -1;
    dropdown->hovered_index = -1;
}

// =============================================================================
// DROPDOWN RENDERING
// =============================================================================

static void render_dropdown_button(KryonDropdownWidget* dropdown, KryonRenderContext* ctx) {
    KryonWidget* widget = &dropdown->base;
    
    // Determine button state
    KryonWidgetState state = widget->state;
    if (dropdown->is_open) {
        state = KRYON_WIDGET_STATE_PRESSED;
    }
    
    // Calculate colors based on state
    KryonColor bg_color = widget->style.background_color;
    KryonColor border_color = widget->style.border_color;
    KryonColor text_color = widget->style.text_color;
    
    switch (state) {
        case KRYON_WIDGET_STATE_HOVERED:
            bg_color.r = fminf(bg_color.r + 0.05f, 1.0f);
            bg_color.g = fminf(bg_color.g + 0.05f, 1.0f);
            bg_color.b = fminf(bg_color.b + 0.05f, 1.0f);
            break;
        case KRYON_WIDGET_STATE_PRESSED:
            bg_color.r = fmaxf(bg_color.r - 0.1f, 0.0f);
            bg_color.g = fmaxf(bg_color.g - 0.1f, 0.0f);
            bg_color.b = fmaxf(bg_color.b - 0.1f, 0.0f);
            break;
        case KRYON_WIDGET_STATE_DISABLED:
            bg_color.a *= 0.5f;
            text_color.a *= 0.5f;
            border_color.a *= 0.5f;
            break;
        default:
            break;
    }
    
    // Draw button background
    KryonRenderCommand bg_cmd = kryon_cmd_draw_rect(
        widget->bounds.position, 
        widget->bounds.size,
        bg_color,
        widget->style.border_radius
    );
    bg_cmd.data.draw_rect.border_width = widget->style.border_width;
    bg_cmd.data.draw_rect.border_color = border_color;
    
    kryon_renderer_execute_command(ctx, &bg_cmd);
    
    // Draw selected text
    const char* display_text = "Select...";
    if (dropdown->selected_index >= 0 && dropdown->selected_index < (int)dropdown->option_count) {
        display_text = dropdown->options[dropdown->selected_index].text;
    }
    
    KryonVec2 text_pos = {
        widget->bounds.position.x + widget->style.padding.left,
        widget->bounds.position.y + (widget->bounds.size.y - widget->style.font_size) / 2.0f
    };
    
    float max_text_width = widget->bounds.size.x - widget->style.padding.left - 
                          widget->style.padding.right - 20.0f; // Reserve space for arrow
    
    KryonRenderCommand text_cmd = kryon_cmd_draw_text(text_pos, display_text, widget->style.font_size, text_color);
    text_cmd.data.draw_text.max_width = max_text_width;
    
    kryon_renderer_execute_command(ctx, &text_cmd);
    
    // Draw dropdown arrow
    KryonVec2 arrow_pos = {
        widget->bounds.position.x + widget->bounds.size.x - 20.0f,
        widget->bounds.position.y + widget->bounds.size.y / 2.0f
    };
    
    // Draw simple triangle arrow
    const char* arrow_symbol = dropdown->is_open ? "▲" : "▼";
    KryonRenderCommand arrow_cmd = kryon_cmd_draw_text(arrow_pos, arrow_symbol, widget->style.font_size * 0.8f, text_color);
    
    kryon_renderer_execute_command(ctx, &arrow_cmd);
}

static void render_dropdown_list(KryonDropdownWidget* dropdown, KryonRenderContext* ctx) {
    if (!dropdown->is_open || dropdown->option_count == 0) return;
    
    KryonWidget* widget = &dropdown->base;
    
    // Calculate dropdown list position and size
    KryonVec2 list_pos = {
        widget->bounds.position.x,
        widget->bounds.position.y + widget->bounds.size.y + 1.0f
    };
    
    float visible_height = fminf(dropdown->max_height, dropdown->option_count * dropdown->item_height);
    KryonVec2 list_size = {
        widget->bounds.size.x,
        visible_height
    };
    
    // Draw list background with shadow
    KryonColor shadow_color = {0.0f, 0.0f, 0.0f, 0.2f};
    KryonVec2 shadow_pos = {list_pos.x + 2.0f, list_pos.y + 2.0f};
    KryonRenderCommand shadow_cmd = kryon_cmd_draw_rect(shadow_pos, list_size, shadow_color, 4.0f);
    kryon_renderer_execute_command(ctx, &shadow_cmd);
    
    // Draw list background
    KryonColor list_bg = {1.0f, 1.0f, 1.0f, 1.0f};
    KryonRenderCommand list_bg_cmd = kryon_cmd_draw_rect(list_pos, list_size, list_bg, 4.0f);
    list_bg_cmd.data.draw_rect.border_width = 1.0f;
    list_bg_cmd.data.draw_rect.border_color = widget->style.border_color;
    kryon_renderer_execute_command(ctx, &list_bg_cmd);
    
    // Calculate visible range based on scroll
    int start_index = (int)(dropdown->scroll_offset / dropdown->item_height);
    int end_index = (int)((dropdown->scroll_offset + visible_height) / dropdown->item_height) + 1;
    
    start_index = fmaxf(0, start_index);
    end_index = fminf(dropdown->option_count, end_index);
    
    // Draw visible options
    for (int i = start_index; i < end_index; i++) {
        KryonDropdownOption* option = &dropdown->options[i];
        
        float item_y = list_pos.y + (i * dropdown->item_height) - dropdown->scroll_offset;
        KryonVec2 item_pos = {list_pos.x, item_y};
        KryonVec2 item_size = {list_size.x, dropdown->item_height};
        
        // Skip items outside visible area
        if (item_y + dropdown->item_height < list_pos.y || item_y > list_pos.y + visible_height) {
            continue;
        }
        
        // Determine item state and colors
        KryonColor item_bg = {0.0f, 0.0f, 0.0f, 0.0f}; // Transparent by default
        KryonColor item_text_color = {0.0f, 0.0f, 0.0f, 1.0f};
        
        if (i == dropdown->selected_index) {
            item_bg = (KryonColor){0.2f, 0.4f, 0.8f, 1.0f}; // Blue for selected
            item_text_color = (KryonColor){1.0f, 1.0f, 1.0f, 1.0f}; // White text
        } else if (i == dropdown->hovered_index) {
            item_bg = (KryonColor){0.9f, 0.95f, 1.0f, 1.0f}; // Light blue for hover
        }
        
        if (!option->enabled) {
            item_text_color.a = 0.5f;
        }
        
        // Draw item background
        if (item_bg.a > 0.0f) {
            KryonRenderCommand item_bg_cmd = kryon_cmd_draw_rect(item_pos, item_size, item_bg, 0.0f);
            kryon_renderer_execute_command(ctx, &item_bg_cmd);
        }
        
        // Draw item text
        KryonVec2 text_pos = {
            item_pos.x + widget->style.padding.left,
            item_pos.y + (dropdown->item_height - widget->style.font_size) / 2.0f
        };
        
        KryonRenderCommand item_text_cmd = kryon_cmd_draw_text(text_pos, option->text, widget->style.font_size, item_text_color);
        item_text_cmd.data.draw_text.max_width = item_size.x - widget->style.padding.left - widget->style.padding.right;
        
        kryon_renderer_execute_command(ctx, &item_text_cmd);
    }
    
    // Draw scrollbar if needed
    if (dropdown->option_count * dropdown->item_height > visible_height) {
        float scrollbar_height = (visible_height / (dropdown->option_count * dropdown->item_height)) * visible_height;
        float scrollbar_y = list_pos.y + (dropdown->scroll_offset / (dropdown->option_count * dropdown->item_height - visible_height)) * (visible_height - scrollbar_height);
        
        KryonVec2 scrollbar_pos = {list_pos.x + list_size.x - 8.0f, scrollbar_y};
        KryonVec2 scrollbar_size = {6.0f, scrollbar_height};
        KryonColor scrollbar_color = {0.6f, 0.6f, 0.6f, 0.8f};
        
        KryonRenderCommand scrollbar_cmd = kryon_cmd_draw_rect(scrollbar_pos, scrollbar_size, scrollbar_color, 3.0f);
        kryon_renderer_execute_command(ctx, &scrollbar_cmd);
    }
}

// =============================================================================
// EVENT HANDLING
// =============================================================================

static bool dropdown_handle_mouse_click(KryonDropdownWidget* dropdown, KryonVec2 click_pos) {
    KryonWidget* widget = &dropdown->base;
    
    // Check if click is on the dropdown button
    if (kryon_point_in_rect(click_pos, widget->bounds)) {
        if (!dropdown->is_open) {
            // Open dropdown
            dropdown->is_open = true;
            dropdown->target_animation = 1.0f;
            dropdown->animating = true;
            
            if (dropdown->on_open) {
                dropdown->on_open(dropdown, dropdown->callback_user_data);
            }
        } else {
            // Close dropdown
            dropdown->is_open = false;
            dropdown->target_animation = 0.0f;
            dropdown->animating = true;
            
            if (dropdown->on_close) {
                dropdown->on_close(dropdown, dropdown->callback_user_data);
            }
        }
        return true;
    }
    
    // Check if click is on dropdown list
    if (dropdown->is_open) {
        KryonVec2 list_pos = {
            widget->bounds.position.x,
            widget->bounds.position.y + widget->bounds.size.y + 1.0f
        };
        
        float visible_height = fminf(dropdown->max_height, dropdown->option_count * dropdown->item_height);
        KryonRect list_bounds = {list_pos.x, list_pos.y, widget->bounds.size.x, visible_height};
        
        if (kryon_point_in_rect(click_pos, list_bounds)) {
            // Calculate clicked item
            float relative_y = click_pos.y - list_pos.y + dropdown->scroll_offset;
            int clicked_index = (int)(relative_y / dropdown->item_height);
            
            if (clicked_index >= 0 && clicked_index < (int)dropdown->option_count && 
                dropdown->options[clicked_index].enabled) {
                
                int old_selected = dropdown->selected_index;
                dropdown->selected_index = clicked_index;
                
                // Close dropdown
                dropdown->is_open = false;
                dropdown->target_animation = 0.0f;
                dropdown->animating = true;
                
                // Trigger change callback
                if (dropdown->on_change && old_selected != dropdown->selected_index) {
                    dropdown->on_change(dropdown, dropdown->selected_index, 
                                      dropdown->options[dropdown->selected_index].value,
                                      dropdown->callback_user_data);
                }
                
                if (dropdown->on_close) {
                    dropdown->on_close(dropdown, dropdown->callback_user_data);
                }
            }
            return true;
        } else {
            // Click outside dropdown - close it
            dropdown->is_open = false;
            dropdown->target_animation = 0.0f;
            dropdown->animating = true;
            
            if (dropdown->on_close) {
                dropdown->on_close(dropdown, dropdown->callback_user_data);
            }
            return true;
        }
    }
    
    return false;
}

static bool dropdown_handle_mouse_move(KryonDropdownWidget* dropdown, KryonVec2 mouse_pos) {
    KryonWidget* widget = &dropdown->base;
    
    // Update button hover state
    bool was_hovered = (widget->state == KRYON_WIDGET_STATE_HOVERED);
    bool is_hovered = kryon_point_in_rect(mouse_pos, widget->bounds);
    
    if (is_hovered && !was_hovered && widget->state != KRYON_WIDGET_STATE_PRESSED) {
        widget->state = KRYON_WIDGET_STATE_HOVERED;
    } else if (!is_hovered && was_hovered) {
        widget->state = KRYON_WIDGET_STATE_NORMAL;
    }
    
    // Update list hover state if open
    if (dropdown->is_open) {
        KryonVec2 list_pos = {
            widget->bounds.position.x,
            widget->bounds.position.y + widget->bounds.size.y + 1.0f
        };
        
        float visible_height = fminf(dropdown->max_height, dropdown->option_count * dropdown->item_height);
        KryonRect list_bounds = {list_pos.x, list_pos.y, widget->bounds.size.x, visible_height};
        
        if (kryon_point_in_rect(mouse_pos, list_bounds)) {
            float relative_y = mouse_pos.y - list_pos.y + dropdown->scroll_offset;
            int hovered_index = (int)(relative_y / dropdown->item_height);
            
            if (hovered_index >= 0 && hovered_index < (int)dropdown->option_count) {
                dropdown->hovered_index = hovered_index;
            } else {
                dropdown->hovered_index = -1;
            }
        } else {
            dropdown->hovered_index = -1;
        }
    }
    
    return is_hovered || dropdown->is_open;
}

static bool dropdown_handle_key_press(KryonDropdownWidget* dropdown, KryonKey key) {
    if (!dropdown->is_open) {
        if (key == KRYON_KEY_SPACE || key == KRYON_KEY_ENTER) {
            dropdown->is_open = true;
            dropdown->target_animation = 1.0f;
            dropdown->animating = true;
            
            if (dropdown->on_open) {
                dropdown->on_open(dropdown, dropdown->callback_user_data);
            }
            return true;
        }
        return false;
    }
    
    switch (key) {
        case KRYON_KEY_ESCAPE:
            dropdown->is_open = false;
            dropdown->target_animation = 0.0f;
            dropdown->animating = true;
            
            if (dropdown->on_close) {
                dropdown->on_close(dropdown, dropdown->callback_user_data);
            }
            return true;
            
        case KRYON_KEY_ENTER:
            if (dropdown->hovered_index >= 0 && dropdown->hovered_index < (int)dropdown->option_count &&
                dropdown->options[dropdown->hovered_index].enabled) {
                
                int old_selected = dropdown->selected_index;
                dropdown->selected_index = dropdown->hovered_index;
                dropdown->is_open = false;
                dropdown->target_animation = 0.0f;
                dropdown->animating = true;
                
                if (dropdown->on_change && old_selected != dropdown->selected_index) {
                    dropdown->on_change(dropdown, dropdown->selected_index,
                                      dropdown->options[dropdown->selected_index].value,
                                      dropdown->callback_user_data);
                }
                
                if (dropdown->on_close) {
                    dropdown->on_close(dropdown, dropdown->callback_user_data);
                }
            }
            return true;
            
        case KRYON_KEY_UP:
            if (dropdown->hovered_index > 0) {
                dropdown->hovered_index--;
            } else {
                dropdown->hovered_index = dropdown->option_count - 1;
            }
            return true;
            
        case KRYON_KEY_DOWN:
            if (dropdown->hovered_index < (int)dropdown->option_count - 1) {
                dropdown->hovered_index++;
            } else {
                dropdown->hovered_index = 0;
            }
            return true;
            
        default:
            return false;
    }
}

// =============================================================================
// ANIMATION UPDATE
// =============================================================================

static void dropdown_update_animation(KryonDropdownWidget* dropdown, float delta_time) {
    if (!dropdown->animating) return;
    
    float animation_speed = 8.0f; // Animation speed
    float diff = dropdown->target_animation - dropdown->open_animation;
    
    if (fabsf(diff) < 0.01f) {
        dropdown->open_animation = dropdown->target_animation;
        dropdown->animating = false;
    } else {
        dropdown->open_animation += diff * animation_speed * delta_time;
    }
}

// =============================================================================
// PUBLIC API
// =============================================================================

KryonDropdownWidget* kryon_dropdown_create(const char* id) {
    KryonDropdownWidget* dropdown = kryon_alloc(sizeof(KryonDropdownWidget));
    if (!dropdown) return NULL;
    
    memset(dropdown, 0, sizeof(KryonDropdownWidget));
    
    // Initialize base widget
    kryon_widget_init(&dropdown->base, KRYON_WIDGET_DROPDOWN);
    if (id) {
        dropdown->base.id = kryon_alloc(strlen(id) + 1);
        if (dropdown->base.id) {
            strcpy(dropdown->base.id, id);
        }
    }
    
    // Set default properties
    dropdown->selected_index = -1;
    dropdown->hovered_index = -1;
    dropdown->is_open = false;
    dropdown->max_height = 200.0f;
    dropdown->item_height = 32.0f;
    dropdown->show_icons = false;
    dropdown->searchable = false;
    dropdown->is_multiselect = false;
    
    // Set default styling
    dropdown->base.style.background_color = (KryonColor){0.95f, 0.95f, 0.95f, 1.0f};
    dropdown->base.style.text_color = (KryonColor){0.0f, 0.0f, 0.0f, 1.0f};
    dropdown->base.style.border_color = (KryonColor){0.7f, 0.7f, 0.7f, 1.0f};
    dropdown->base.style.border_width = 1.0f;
    dropdown->base.style.border_radius = 4.0f;
    dropdown->base.style.font_size = 14.0f;
    dropdown->base.style.padding = (KryonPadding){8.0f, 8.0f, 8.0f, 8.0f};
    
    return dropdown;
}

void kryon_dropdown_destroy(KryonDropdownWidget* dropdown) {
    if (!dropdown) return;
    
    dropdown_clear_options(dropdown);
    kryon_free(dropdown->options);
    kryon_free(dropdown->search_text);
    kryon_widget_cleanup(&dropdown->base);
    kryon_free(dropdown);
}

bool kryon_dropdown_add_option(KryonDropdownWidget* dropdown, const char* text, const char* value) {
    if (!dropdown || !text) return false;
    
    if (!dropdown_ensure_capacity(dropdown, dropdown->option_count + 1)) {
        return false;
    }
    
    KryonDropdownOption* option = &dropdown->options[dropdown->option_count];
    memset(option, 0, sizeof(KryonDropdownOption));
    
    option->text = kryon_alloc(strlen(text) + 1);
    if (!option->text) return false;
    strcpy(option->text, text);
    
    if (value) {
        option->value = kryon_alloc(strlen(value) + 1);
        if (!option->value) {
            kryon_free(option->text);
            return false;
        }
        strcpy(option->value, value);
    } else {
        option->value = kryon_alloc(strlen(text) + 1);
        if (option->value) {
            strcpy(option->value, text);
        }
    }
    
    option->enabled = true;
    dropdown->option_count++;
    
    return true;
}

void kryon_dropdown_clear_options(KryonDropdownWidget* dropdown) {
    if (!dropdown) return;
    
    dropdown_clear_options(dropdown);
}

bool kryon_dropdown_set_selected_index(KryonDropdownWidget* dropdown, int index) {
    if (!dropdown || index < -1 || index >= (int)dropdown->option_count) {
        return false;
    }
    
    dropdown->selected_index = index;
    return true;
}

int kryon_dropdown_get_selected_index(KryonDropdownWidget* dropdown) {
    return dropdown ? dropdown->selected_index : -1;
}

const char* kryon_dropdown_get_selected_value(KryonDropdownWidget* dropdown) {
    if (!dropdown || dropdown->selected_index < 0 || dropdown->selected_index >= (int)dropdown->option_count) {
        return NULL;
    }
    
    return dropdown->options[dropdown->selected_index].value;
}

const char* kryon_dropdown_get_selected_text(KryonDropdownWidget* dropdown) {
    if (!dropdown || dropdown->selected_index < 0 || dropdown->selected_index >= (int)dropdown->option_count) {
        return NULL;
    }
    
    return dropdown->options[dropdown->selected_index].text;
}

void kryon_dropdown_set_callbacks(KryonDropdownWidget* dropdown,
                                 KryonDropdownChangeCallback on_change,
                                 KryonDropdownOpenCallback on_open,
                                 KryonDropdownCloseCallback on_close,
                                 void* user_data) {
    if (!dropdown) return;
    
    dropdown->on_change = on_change;
    dropdown->on_open = on_open;
    dropdown->on_close = on_close;
    dropdown->callback_user_data = user_data;
}

void kryon_dropdown_set_max_height(KryonDropdownWidget* dropdown, float height) {
    if (dropdown && height > 0) {
        dropdown->max_height = height;
    }
}

void kryon_dropdown_set_item_height(KryonDropdownWidget* dropdown, float height) {
    if (dropdown && height > 0) {
        dropdown->item_height = height;
    }
}

bool kryon_dropdown_is_open(KryonDropdownWidget* dropdown) {
    return dropdown ? dropdown->is_open : false;
}

void kryon_dropdown_open(KryonDropdownWidget* dropdown) {
    if (dropdown && !dropdown->is_open) {
        dropdown->is_open = true;
        dropdown->target_animation = 1.0f;
        dropdown->animating = true;
        
        if (dropdown->on_open) {
            dropdown->on_open(dropdown, dropdown->callback_user_data);
        }
    }
}

void kryon_dropdown_close(KryonDropdownWidget* dropdown) {
    if (dropdown && dropdown->is_open) {
        dropdown->is_open = false;
        dropdown->target_animation = 0.0f;
        dropdown->animating = true;
        
        if (dropdown->on_close) {
            dropdown->on_close(dropdown, dropdown->callback_user_data);
        }
    }
}

// Widget interface implementation
void kryon_dropdown_render(KryonDropdownWidget* dropdown, KryonRenderContext* ctx) {
    if (!dropdown) return;
    
    render_dropdown_button(dropdown, ctx);
    render_dropdown_list(dropdown, ctx);
}

bool kryon_dropdown_handle_event(KryonDropdownWidget* dropdown, const KryonEvent* event) {
    if (!dropdown || !event) return false;
    
    switch (event->type) {
        case KRYON_EVENT_MOUSE_MOVED:
            return dropdown_handle_mouse_move(dropdown, (KryonVec2){event->data.mouse.x, event->data.mouse.y});
            
        case KRYON_EVENT_MOUSE_CLICKED:
            if (event->data.mouse.button == KRYON_MOUSE_LEFT) {
                return dropdown_handle_mouse_click(dropdown, (KryonVec2){event->data.mouse.x, event->data.mouse.y});
            }
            break;
            
        case KRYON_EVENT_KEY_DOWN:
            return dropdown_handle_key_press(dropdown, event->data.keyboard.key);
            
        default:
            break;
    }
    
    return false;
}

void kryon_dropdown_update(KryonDropdownWidget* dropdown, float delta_time) {
    if (!dropdown) return;
    
    dropdown_update_animation(dropdown, delta_time);
}