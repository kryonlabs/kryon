/**
 * SDL3 Input - SDL3-Specific Event Handling
 *
 * This file implements SDL3 event handling, including:
 * - SDL3 event loop processing (keyboard, mouse, window events)
 * - Input field text editing with caret visibility
 * - Dropdown menu rendering with SDL3
 * - Component hit testing and event dispatch
 */

#define _POSIX_C_SOURCE 200809L

#include "sdl3_renderer.h"
#include "../../desktop_internal.h"
#include "../../../../ir/ir_executor.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Forward declarations from desktop_input.c (platform-agnostic functions)
extern InputRuntimeState* get_input_state(uint32_t id);
extern void collect_open_dropdowns(IRComponent* component, IRComponent** dropdown_list, int* count, int max_count);
extern IRComponent* find_dropdown_menu_at_point(IRComponent* root, float x, float y);

// Forward declarations from desktop_fonts.c (SDL3-specific)
extern float measure_text_width(TTF_Font* font, const char* text);
extern TTF_Font* desktop_ir_resolve_font(DesktopIRRenderer* renderer, IRComponent* component, float fallback_size);
extern SDL_Color ir_color_to_sdl(IRColor color);

// Nim bridge functions (optional, weak symbols for Lua compatibility)
__attribute__((weak)) void nimButtonBridge(uint32_t componentId);
__attribute__((weak)) void nimCheckboxBridge(uint32_t componentId);
__attribute__((weak)) void nimDropdownBridge(uint32_t componentId, int32_t selectedIndex);
__attribute__((weak)) bool nimInputBridge(IRComponent* component, const char* text);

// ============================================================================
// INPUT CARET VISIBILITY
// ============================================================================

/**
 * Ensure caret is visible within input field's visible area
 *
 * Adjusts scroll_x to keep the caret within view, with a small margin.
 * Accounts for input field padding and available space.
 */
static void ensure_caret_visible_sdl3(DesktopIRRenderer* renderer,
                          IRComponent* input,
                          InputRuntimeState* istate,
                          TTF_Font* font,
                          float pad_left,
                          float pad_right) {
    if (!renderer || !input || !istate) return;

    SDL3RendererData* data = sdl3_get_data(renderer);
    if (!data) return;

    const char* txt = input->text_content ? input->text_content : "";
    size_t cur = istate->cursor_index;
    size_t len = strlen(txt);
    if (cur > len) cur = len;
    char* prefix = strndup(txt, cur);
    float caret_local = measure_text_width(font ? font : data->default_font, prefix);
    free(prefix);

    float avail = input->rendered_bounds.width - pad_left - pad_right;
    if (avail < 0) avail = 0;

    // Clamp scroll to content width
    float total_width = measure_text_width(font ? font : data->default_font, txt);
    float max_scroll = total_width > avail ? (total_width - avail) : 0.0f;
    if (istate->scroll_x > max_scroll) istate->scroll_x = max_scroll;

    float visible_pos = caret_local - istate->scroll_x;
    const float margin = 8.0f;
    if (visible_pos > avail - margin) {
        istate->scroll_x = caret_local - (avail - margin);
        if (istate->scroll_x > max_scroll) istate->scroll_x = max_scroll;
    } else if (visible_pos < margin) {
        istate->scroll_x = caret_local - margin;
        if (istate->scroll_x < 0) istate->scroll_x = 0;
    }
}

// ============================================================================
// DROPDOWN MENU RENDERING
// ============================================================================

/**
 * Render dropdown menu overlay with SDL3
 *
 * Draws dropdown menu below the dropdown button with all options.
 * Highlights hovered or selected options.
 */
static void render_dropdown_menu_sdl3_internal(DesktopIRRenderer* renderer, IRComponent* component) {
    if (!renderer || !component || component->type != IR_COMPONENT_DROPDOWN) return;

    SDL3RendererData* data = sdl3_get_data(renderer);
    if (!data || !data->renderer) return;

    IRDropdownState* dropdown_state = ir_get_dropdown_state(component);
    if (!dropdown_state || !dropdown_state->is_open || dropdown_state->option_count == 0) return;

    IRRenderedBounds bounds = component->rendered_bounds;
    if (!bounds.valid) return;

    // Get colors from component style
    SDL_Color bg_color = component->style ?
        ir_color_to_sdl(component->style->background) :
        (SDL_Color){255, 255, 255, 255};
    SDL_Color text_color = component->style ?
        ir_color_to_sdl(component->style->font.color) :
        (SDL_Color){0, 0, 0, 255};
    SDL_Color border_color = component->style ?
        ir_color_to_sdl(component->style->border.color) :
        (SDL_Color){209, 213, 219, 255};

    float menu_y = bounds.y + bounds.height;
    float menu_height = fminf(dropdown_state->option_count * 35.0f, 200.0f);

    SDL_FRect menu_rect = {
        .x = bounds.x,
        .y = menu_y,
        .w = bounds.width,
        .h = menu_height
    };

    // Draw menu background with shadow effect
    SDL_SetRenderDrawColor(data->renderer, 50, 50, 50, 100);
    SDL_FRect shadow_rect = {menu_rect.x + 2, menu_rect.y + 2, menu_rect.w, menu_rect.h};
    SDL_RenderFillRect(data->renderer, &shadow_rect);

    SDL_SetRenderDrawColor(data->renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
    SDL_RenderFillRect(data->renderer, &menu_rect);

    SDL_SetRenderDrawColor(data->renderer, border_color.r, border_color.g, border_color.b, border_color.a);
    SDL_RenderRect(data->renderer, &menu_rect);

    // Render each option
    float option_height = 35.0f;
    for (uint32_t i = 0; i < dropdown_state->option_count; i++) {
        float option_y = menu_y + i * option_height;

        // Highlight hovered or selected option
        if ((int32_t)i == dropdown_state->hovered_index || (int32_t)i == dropdown_state->selected_index) {
            SDL_Color hover_color = {224, 224, 224, 255};
            SDL_SetRenderDrawColor(data->renderer, hover_color.r, hover_color.g, hover_color.b, hover_color.a);
            SDL_FRect option_rect = {menu_rect.x, option_y, menu_rect.w, option_height};
            SDL_RenderFillRect(data->renderer, &option_rect);
        }

        // Render option text
        if (dropdown_state->options[i] && data->default_font) {
            SDL_Surface* opt_surface = TTF_RenderText_Blended(data->default_font,
                                                              dropdown_state->options[i],
                                                              strlen(dropdown_state->options[i]),
                                                              text_color);
            if (opt_surface) {
                SDL_Texture* opt_texture = SDL_CreateTextureFromSurface(data->renderer, opt_surface);
                if (opt_texture) {
                    SDL_SetTextureScaleMode(opt_texture, SDL_SCALEMODE_NEAREST);  // Crisp text
                    SDL_FRect opt_text_rect = {
                        .x = roundf(menu_rect.x + 10),
                        .y = roundf(option_y + (option_height - opt_surface->h) / 2.0f),
                        .w = (float)opt_surface->w,
                        .h = (float)opt_surface->h
                    };
                    SDL_RenderTexture(data->renderer, opt_texture, NULL, &opt_text_rect);
                    SDL_DestroyTexture(opt_texture);
                }
                SDL_DestroySurface(opt_surface);
            }
        }
    }
}

// ============================================================================
// SDL3 EVENT HANDLING
// ============================================================================

// Forward declarations for helper functions from desktop_input.c (platform-agnostic)
extern void desktop_init_router(const char* initial_page, const char* ir_dir, IRComponent* root, void* renderer);
extern void desktop_renderer_reload_tree(void* renderer, IRComponent* new_root);

// These functions are implemented in desktop_input.c and are platform-agnostic
// They will be linked when building the final library/executable
__attribute__((weak)) bool is_external_url(const char* url);
__attribute__((weak)) void navigate_to_page(const char* path);
__attribute__((weak)) void open_external_url(const char* url);
__attribute__((weak)) bool try_handle_as_tab_click(DesktopIRRenderer* renderer, IRComponent* clicked);

/**
 * Main SDL3 event loop
 *
 * Processes all pending SDL3 events and dispatches them to IR components.
 */
void handle_sdl3_events(DesktopIRRenderer* renderer) {
    if (!renderer) return;

    SDL3RendererData* data = sdl3_get_data(renderer);
    if (!data || !data->window) return;

    // Static state (NOT thread-safe)
    static TabGroupState* dragging_tabgroup = NULL;
    static IRComponent* focused_input = NULL;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        DesktopEvent desktop_event = {0};
        desktop_event.timestamp = SDL_GetTicks();

        switch (event.type) {
            case SDL_EVENT_QUIT:
                desktop_event.type = DESKTOP_EVENT_QUIT;
                if (renderer->event_callback) {
                    renderer->event_callback(&desktop_event, renderer->event_user_data);
                }
                renderer->running = false;
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                desktop_event.type = DESKTOP_EVENT_MOUSE_CLICK;
                desktop_event.data.mouse.x = event.button.x;
                desktop_event.data.mouse.y = event.button.y;
                desktop_event.data.mouse.button = event.button.button;

                if (renderer->last_root) {
                    IRComponent* clicked = NULL;

                    // Check if click is in an open dropdown menu first
                    IRComponent* dropdown_at_point = find_dropdown_menu_at_point(
                        renderer->last_root,
                        (float)event.button.x,
                        (float)event.button.y
                    );

                    if (dropdown_at_point) {
                        clicked = dropdown_at_point;
                    } else {
                        clicked = ir_find_component_at_point(
                            renderer->last_root,
                            (float)event.button.x,
                            (float)event.button.y
                        );
                    }

                    printf("[DEBUG] Click at (%.0f, %.0f) found: %s (ID: %u type=%d)\n",
                           (float)event.button.x, (float)event.button.y,
                           clicked ? (clicked->type == IR_COMPONENT_LINK ? "LINK" :
                                     clicked->type == IR_COMPONENT_BUTTON ? "BUTTON" :
                                     clicked->type == IR_COMPONENT_CHECKBOX ? "CHECKBOX" :
                                     clicked->type == IR_COMPONENT_TEXT ? "TEXT" :
                                     clicked->type == IR_COMPONENT_ROW ? "ROW" :
                                     clicked->type == IR_COMPONENT_COLUMN ? "COLUMN" :
                                     clicked->type == IR_COMPONENT_TAB ? "TAB" :
                                     clicked->type == IR_COMPONENT_TAB_BAR ? "TAB_BAR" :
                                     clicked->type == IR_COMPONENT_TAB_CONTENT ? "TAB_CONTENT" :
                                     clicked->type == IR_COMPONENT_TAB_PANEL ? "TAB_PANEL" : "OTHER") : "NULL",
                           clicked ? clicked->id : 0,
                           clicked ? clicked->type : -1);
                    if (clicked) {
                        printf("[DEBUG]   rendered_bounds: valid=%d [%.1f, %.1f, %.1f, %.1f]\n",
                               clicked->rendered_bounds.valid,
                               clicked->rendered_bounds.x, clicked->rendered_bounds.y,
                               clicked->rendered_bounds.width, clicked->rendered_bounds.height);
                    }
                    if (clicked && clicked->parent) {
                        printf("[DEBUG]   parent: ID=%u type=%d\n", clicked->parent->id, clicked->parent->type);
                    }

                    if (clicked) {
                        // Handle input focus
                        if (clicked->type == IR_COMPONENT_INPUT) {
                            focused_input = clicked;
                            InputRuntimeState* istate = get_input_state(clicked->id);
                            if (istate) {
                                istate->focused = true;
                                istate->caret_visible = true;
                                istate->last_blink_ms = SDL_GetTicks();
                                size_t len = clicked->text_content ? strlen(clicked->text_content) : 0;
                                istate->cursor_index = len;
                            }
                            SDL_StartTextInput(data->window);
                        } else if (focused_input) {
                            InputRuntimeState* istate = get_input_state(focused_input->id);
                            if (istate) istate->focused = false;
                            focused_input = NULL;
                            SDL_StopTextInput(data->window);
                        }

                        // Handle IR-level tab clicks
                        if (clicked->type == IR_COMPONENT_TAB) {
                            IRComponent* tab_group = clicked->parent;
                            while (tab_group && tab_group->type != IR_COMPONENT_TAB_GROUP) {
                                tab_group = tab_group->parent;
                            }

                            if (tab_group && tab_group->custom_data) {
                                TabGroupState* tg_state = (TabGroupState*)tab_group->custom_data;
                                IRComponent* tab_bar = clicked->parent;
                                if (tab_bar && tab_bar->type == IR_COMPONENT_TAB_BAR) {
                                    for (uint32_t i = 0; i < tab_bar->child_count; i++) {
                                        if (tab_bar->children[i] == clicked) {
                                            ir_tabgroup_select(tg_state, (int)i);
                                            break;
                                        }
                                    }
                                }
                            }
                        }

                        // Start tab drag if parent has TabGroupState
                        if (clicked->parent && clicked->parent->custom_data) {
                            TabGroupState* tg_state = (TabGroupState*)clicked->parent->custom_data;
                            if (tg_state) {
                                ir_tabgroup_handle_drag(tg_state, (float)event.button.x, (float)event.button.y, true, false);
                                dragging_tabgroup = tg_state;
                            }
                        }

                        // Find and trigger IR_EVENT_CLICK handler
                        IREvent* ir_event = ir_find_event(clicked, IR_EVENT_CLICK);
                        printf("[DEBUG] Component %u has ir_event: %s\n", clicked->id, ir_event ? "YES" : "NO");
                        if (ir_event && ir_event->logic_id) {
                            // Check if this is a Lua event handler
                            if (strncmp(ir_event->logic_id, "lua_event_", 10) == 0) {
                                bool handled_as_tab = try_handle_as_tab_click(renderer, clicked);
                                uint32_t handler_id = 0;
                                if (sscanf(ir_event->logic_id + 10, "%u", &handler_id) == 1) {
                                    if (renderer->lua_event_callback) {
                                        fprintf(stderr, "[LUA_EVENT] Calling Lua handler %u for component %u%s\n",
                                                handler_id, clicked->id, handled_as_tab ? " (TAB)" : "");
                                        renderer->lua_event_callback(handler_id, IR_EVENT_CLICK);
                                    } else if (!handled_as_tab) {
                                        printf("⚠️ Lua event detected but no callback registered\n");
                                        printf("  💡 Hint: Run .lua files directly (don't compile to .kir first)\n");
                                    }
                                } else if (!handled_as_tab) {
                                    fprintf(stderr, "⚠️ Failed to parse handler ID from logic_id: %s\n",
                                            ir_event->logic_id);
                                }
                            }
                            // Check if this is a Nim button handler
                            else if (strncmp(ir_event->logic_id, "nim_button_", 11) == 0) {
                                bool handled_as_tab = try_handle_as_tab_click(renderer, clicked);
                                IRExecutorContext* executor = ir_executor_get_global();
                                if (executor) {
                                    ir_executor_set_root(executor, renderer->last_root);
                                    ir_executor_handle_event_by_logic_id(executor, clicked->id, ir_event->logic_id);
                                } else if (nimButtonBridge) {
                                    nimButtonBridge(clicked->id);
                                }
                            }
                            // Check if this is a Nim checkbox handler
                            else if (strncmp(ir_event->logic_id, "nim_checkbox_", 13) == 0) {
                                if (clicked->type == IR_COMPONENT_CHECKBOX) {
                                    ir_toggle_checkbox_state(clicked);
                                }
                                IRExecutorContext* executor = ir_executor_get_global();
                                if (executor) {
                                    ir_executor_set_root(executor, renderer->last_root);
                                    ir_executor_handle_event_by_logic_id(executor, clicked->id, ir_event->logic_id);
                                } else if (nimCheckboxBridge) {
                                    nimCheckboxBridge(clicked->id);
                                }
                            }
                            // Check if this is a Nim dropdown handler
                            else if (strncmp(ir_event->logic_id, "nim_dropdown_", 13) == 0) {
                                if (clicked->type == IR_COMPONENT_DROPDOWN) {
                                    IRDropdownState* state = ir_get_dropdown_state(clicked);
                                    if (state) {
                                        float click_y = (float)event.button.y;
                                        IRRenderedBounds bounds = clicked->rendered_bounds;

                                        if (state->is_open) {
                                            float menu_y = bounds.y + bounds.height;
                                            float menu_height = fminf(state->option_count * 35.0f, 200.0f);

                                            if (click_y >= menu_y && click_y < menu_y + menu_height) {
                                                uint32_t option_index = (uint32_t)((click_y - menu_y) / 35.0f);
                                                if (option_index < state->option_count) {
                                                    ir_set_dropdown_selected_index(clicked, (int32_t)option_index);
                                                    ir_set_dropdown_open_state(clicked, false);
                                                    if (nimDropdownBridge) {
                                                        nimDropdownBridge(clicked->id, (int32_t)option_index);
                                                    }
                                                }
                                            } else {
                                                ir_set_dropdown_open_state(clicked, false);
                                            }
                                        } else {
                                            ir_set_dropdown_open_state(clicked, true);
                                        }
                                    }
                                }
                            }
                            else {
                                // Generic logic_id - handle via executor
                                printf("Click on component ID %u (logic: %s)\n",
                                       clicked->id, ir_event->logic_id);
                                IRExecutorContext* executor = ir_executor_get_global();
                                if (executor) {
                                    ir_executor_set_root(executor, renderer->last_root);
                                    ir_executor_handle_event_by_logic_id(executor, clicked->id, ir_event->logic_id);
                                }
                            }
                        } else {
                            // No event handler - handle built-in behaviors
                            if (clicked->type == IR_COMPONENT_CHECKBOX) {
                                ir_toggle_checkbox_state(clicked);
                            } else if (clicked->type == IR_COMPONENT_DROPDOWN) {
                                IRDropdownState* state = ir_get_dropdown_state(clicked);
                                if (state) {
                                    float click_y = (float)event.button.y;
                                    IRRenderedBounds bounds = clicked->rendered_bounds;

                                    if (state->is_open) {
                                        float menu_y = bounds.y + bounds.height;
                                        float menu_height = fminf(state->option_count * 35.0f, 200.0f);

                                        if (click_y >= menu_y && click_y < menu_y + menu_height) {
                                            uint32_t option_index = (uint32_t)((click_y - menu_y) / 35.0f);
                                            if (option_index < state->option_count) {
                                                ir_set_dropdown_selected_index(clicked, (int32_t)option_index);
                                            }
                                        }
                                        ir_set_dropdown_open_state(clicked, false);
                                    } else {
                                        ir_set_dropdown_open_state(clicked, true);
                                    }
                                }
                            }
                            // Handle Link clicks - navigate internally or open externally
                            else if (clicked->type == IR_COMPONENT_LINK) {
                                printf("[DEBUG] Link handler reached! custom_data: %p\n", clicked->custom_data);
                                IRLinkData* link_data = (IRLinkData*)clicked->custom_data;
                                if (link_data) {
                                    printf("[DEBUG] link_data exists, url: %s\n", link_data->url ? link_data->url : "NULL");
                                }
                                if (link_data && link_data->url) {
                                    if (link_data->title) {
                                        printf("🔗 Link clicked: %s (%s)\n", link_data->url, link_data->title);
                                    } else {
                                        printf("🔗 Link clicked: %s\n", link_data->url);
                                    }

                                    // Route based on link type (calls platform-agnostic functions)
                                    if (is_external_url(link_data->url)) {
                                        open_external_url(link_data->url);
                                    } else {
                                        navigate_to_page(link_data->url);
                                    }
                                }
                            }
                        }
                    }
                }

                if (renderer->event_callback) {
                    renderer->event_callback(&desktop_event, renderer->event_user_data);
                }
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                // Mouse wheel scrolling handled by plugins
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    if (dragging_tabgroup) {
                        ir_tabgroup_handle_drag(dragging_tabgroup, (float)event.button.x, (float)event.button.y, false, true);
                        dragging_tabgroup = NULL;
                    }
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                desktop_event.type = DESKTOP_EVENT_MOUSE_MOVE;
                desktop_event.data.mouse.x = event.motion.x;
                desktop_event.data.mouse.y = event.motion.y;

                // Dragging tabs
                if (dragging_tabgroup) {
                    ir_tabgroup_handle_drag(dragging_tabgroup, (float)event.motion.x, (float)event.motion.y, false, false);
                }

                // Update cursor based on component under mouse
                if (renderer->last_root) {
                    IRComponent* hovered = NULL;
                    bool is_in_dropdown_menu = false;

                    IRComponent* dropdown_at_point = find_dropdown_menu_at_point(
                        renderer->last_root,
                        (float)event.motion.x,
                        (float)event.motion.y
                    );

                    if (dropdown_at_point) {
                        hovered = dropdown_at_point;
                        is_in_dropdown_menu = true;

                        IRDropdownState* state = ir_get_dropdown_state(dropdown_at_point);
                        if (state && state->is_open) {
                            IRRenderedBounds bounds = dropdown_at_point->rendered_bounds;
                            float menu_y = bounds.y + bounds.height;
                            float mouse_y = (float)event.motion.y;
                            int32_t new_hover = (int32_t)((mouse_y - menu_y) / 35.0f);
                            if (new_hover >= 0 && new_hover < (int32_t)state->option_count) {
                                ir_set_dropdown_hovered_index(dropdown_at_point, new_hover);
                            }
                        }
                    } else {
                        hovered = ir_find_component_at_point(
                            renderer->last_root,
                            (float)event.motion.x,
                            (float)event.motion.y
                        );

                        // Clear hover state for any open dropdowns
                        #define MAX_DROPDOWNS_TO_CLEAR 10
                        IRComponent* all_dropdowns[MAX_DROPDOWNS_TO_CLEAR];
                        int count = 0;
                        collect_open_dropdowns(renderer->last_root, all_dropdowns, &count, MAX_DROPDOWNS_TO_CLEAR);
                        for (int i = 0; i < count; i++) {
                            ir_set_dropdown_hovered_index(all_dropdowns[i], -1);
                        }
                    }

                    // Set cursor to hand for clickable components
                    SDL_Cursor* desired_cursor;
                    if (is_in_dropdown_menu ||
                        (hovered && (hovered->type == IR_COMPONENT_BUTTON ||
                                     hovered->type == IR_COMPONENT_INPUT ||
                                     hovered->type == IR_COMPONENT_CHECKBOX ||
                                     hovered->type == IR_COMPONENT_DROPDOWN ||
                                     hovered->type == IR_COMPONENT_LINK))) {
                        desired_cursor = data->cursor_hand;
                    } else {
                        desired_cursor = data->cursor_default;
                    }

                    if (desired_cursor != data->current_cursor) {
                        SDL_SetCursor(desired_cursor);
                        data->current_cursor = desired_cursor;
                    }
                }

                if (renderer->event_callback) {
                    renderer->event_callback(&desktop_event, renderer->event_user_data);
                }
                break;

            case SDL_EVENT_TEXT_INPUT:
                if (focused_input && event.text.text[0] != '\0') {
                    const char* incoming = event.text.text;
                    size_t incoming_len = strlen(incoming);
                    const char* current = focused_input->text_content ? focused_input->text_content : "";
                    size_t current_len = strlen(current);
                    InputRuntimeState* istate = get_input_state(focused_input->id);
                    float pad_left = focused_input->style ? focused_input->style->padding.left : 8.0f;
                    float pad_right = focused_input->style ? focused_input->style->padding.right : 8.0f;
                    TTF_Font* font = desktop_ir_resolve_font(renderer, focused_input,
                        (focused_input->style && focused_input->style->font.size > 0) ? focused_input->style->font.size : 16.0f);
                    size_t cursor = istate ? istate->cursor_index : current_len;
                    if (cursor > current_len) cursor = current_len;
                    char* combined = malloc(current_len + incoming_len + 1);
                    if (combined) {
                        memcpy(combined, current, cursor);
                        memcpy(combined + cursor, incoming, incoming_len);
                        memcpy(combined + cursor + incoming_len, current + cursor, current_len - cursor);
                        combined[current_len + incoming_len] = '\0';
                        ir_set_text_content(focused_input, combined);
                        if (istate) {
                            istate->cursor_index = cursor + incoming_len;
                            ensure_caret_visible_sdl3(renderer, focused_input, istate, font, pad_left, pad_right);
                        }
                        if (nimInputBridge) {
                            nimInputBridge(focused_input, focused_input->text_content);
                        }

                        // Fire IR_EVENT_TEXT_CHANGE for Lua handlers
                        IREvent* text_event = ir_find_event(focused_input, IR_EVENT_TEXT_CHANGE);
                        if (text_event && text_event->logic_id) {
                            if (strncmp(text_event->logic_id, "lua_event_", 10) == 0) {
                                uint32_t handler_id = 0;
                                if (sscanf(text_event->logic_id + 10, "%u", &handler_id) == 1) {
                                    if (renderer->lua_event_callback) {
                                        fprintf(stderr, "[LUA_EVENT] Firing onTextChange handler %u (text input)\n", handler_id);
                                        renderer->lua_event_callback(handler_id, IR_EVENT_TEXT_CHANGE);
                                    }
                                }
                            }
                        }

                        // Sync to bound variable
                        IRExecutorContext* exec_ctx = ir_executor_get_global();
                        if (exec_ctx) {
                            ir_executor_sync_input_to_var(exec_ctx, focused_input);
                        }

                        free(combined);
                    }
                }
                break;

            case SDL_EVENT_KEY_UP:
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    renderer->running = false;
                    break;
                }
                if (event.key.scancode == SDL_SCANCODE_Q && (event.key.mod & SDL_KMOD_CTRL)) {
                    renderer->running = false;
                    break;
                }
                break;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.repeat) break;

                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    renderer->running = false;
                    break;
                }

                // Backspace handling for focused input
                if (focused_input && event.key.key == SDLK_BACKSPACE) {
                    const char* current = focused_input->text_content ? focused_input->text_content : "";
                    size_t len = strlen(current);
                    InputRuntimeState* istate = get_input_state(focused_input->id);
                    float pad_left = focused_input->style ? focused_input->style->padding.left : 8.0f;
                    float pad_right = focused_input->style ? focused_input->style->padding.right : 8.0f;
                    TTF_Font* font = desktop_ir_resolve_font(renderer, focused_input,
                        (focused_input->style && focused_input->style->font.size > 0) ? focused_input->style->font.size : 16.0f);
                    size_t cursor = istate ? istate->cursor_index : len;
                    if (cursor > len) cursor = len;
                    if (len > 0 && cursor > 0) {
                        size_t cut = cursor - 1;
                        while (cut > 0 && ((current[cut] & 0xC0) == 0x80)) cut -= 1;
                        size_t prefix_len = cut;
                        size_t suffix_len = len - cursor;
                        char* combined = malloc(prefix_len + suffix_len + 1);
                        if (combined) {
                            memcpy(combined, current, prefix_len);
                            memcpy(combined + prefix_len, current + cursor, suffix_len);
                            combined[prefix_len + suffix_len] = '\0';
                            ir_set_text_content(focused_input, combined);
                            if (istate) {
                                istate->cursor_index = prefix_len;
                                ensure_caret_visible_sdl3(renderer, focused_input, istate, font, pad_left, pad_right);
                            }
                            if (nimInputBridge) {
                                nimInputBridge(focused_input, focused_input->text_content);
                            }

                            // Fire IR_EVENT_TEXT_CHANGE
                            IREvent* text_event = ir_find_event(focused_input, IR_EVENT_TEXT_CHANGE);
                            if (text_event && text_event->logic_id) {
                                if (strncmp(text_event->logic_id, "lua_event_", 10) == 0) {
                                    uint32_t handler_id = 0;
                                    if (sscanf(text_event->logic_id + 10, "%u", &handler_id) == 1) {
                                        if (renderer->lua_event_callback) {
                                            fprintf(stderr, "[LUA_EVENT] Firing onTextChange handler %u (backspace)\n", handler_id);
                                            renderer->lua_event_callback(handler_id, IR_EVENT_TEXT_CHANGE);
                                        }
                                    }
                                }
                            }

                            // Sync to bound variable
                            IRExecutorContext* exec_ctx = ir_executor_get_global();
                            if (exec_ctx) {
                                ir_executor_sync_input_to_var(exec_ctx, focused_input);
                            }

                            free(combined);
                        }
                    }
                    break;
                }

                desktop_event.type = DESKTOP_EVENT_KEY_PRESS;
                desktop_event.data.keyboard.key_code = event.key.key;
                desktop_event.data.keyboard.shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
                desktop_event.data.keyboard.ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;
                desktop_event.data.keyboard.alt = (event.key.mod & SDL_KMOD_ALT) != 0;
                if (renderer->event_callback) {
                    renderer->event_callback(&desktop_event, renderer->event_user_data);
                }
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                desktop_event.type = DESKTOP_EVENT_WINDOW_RESIZE;
                desktop_event.data.resize.width = event.window.data1;
                desktop_event.data.resize.height = event.window.data2;
                renderer->needs_relayout = true;
                if (renderer->event_callback) {
                    renderer->event_callback(&desktop_event, renderer->event_user_data);
                }
                break;

            case SDL_EVENT_WINDOW_EXPOSED:
            case SDL_EVENT_WINDOW_RESTORED:
            case SDL_EVENT_WINDOW_SHOWN:
                renderer->needs_relayout = true;
                break;

            default:
                break;
        }
    }
}
