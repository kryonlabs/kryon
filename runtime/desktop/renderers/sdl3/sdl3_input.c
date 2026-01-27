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
#include <ir_executor.h>
#include "../../ir/include/ir_state.h"
#include <ir_builder.h>
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

// Forward declaration from C bindings event bridge
extern void kryon_c_event_bridge(const char* logic_id);

// Forward declaration for two-way input binding sync
extern void kryon_sync_input_to_signal(IRComponent* component);

// Helper to get executor from global state manager
static IRExecutorContext* get_executor_from_state_mgr(void) {
    extern IRStateManager* ir_state_get_global(void);
    IRStateManager* state_mgr = ir_state_get_global();
    return state_mgr ? ir_state_manager_get_executor(state_mgr) : NULL;
}

// ============================================================================
// MODAL HANDLING
// ============================================================================

/**
 * Find an open modal in the component tree
 * Returns the first open modal component found, or NULL if none
 */
static IRComponent* find_open_modal(IRComponent* root) {
    if (!root) return NULL;

    if (root->type == IR_COMPONENT_MODAL) {
        /* Parse modal state from custom_data string: "open|title" or "closed|title" */
        const char* state_str = root->custom_data;
        if (state_str && strncmp(state_str, "open", 4) == 0) {
            return root;
        }
    }

    for (uint32_t i = 0; i < root->child_count; i++) {
        IRComponent* found = find_open_modal(root->children[i]);
        if (found) return found;
    }

    return NULL;
}

/**
 * Check if a point is inside the modal's content area (not the backdrop)
 * Returns true if click is on backdrop (outside modal content)
 */
static bool is_click_on_modal_backdrop(IRComponent* modal, float x, float y) {
    if (!modal || modal->type != IR_COMPONENT_MODAL) return false;

    // The modal's rendered_bounds contains the centered modal content area
    IRRenderedBounds bounds = modal->rendered_bounds;
    if (!bounds.valid) return false;

    // If click is outside the modal content area, it's on the backdrop
    bool inside = (x >= bounds.x && x < bounds.x + bounds.width &&
                   y >= bounds.y && y < bounds.y + bounds.height);

    return !inside;
}

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

/**
 * Public wrapper for rendering dropdown menus
 * Called from main rendering loop to draw all open dropdown overlays
 */
void render_dropdown_menu_sdl3(DesktopIRRenderer* renderer, IRComponent* component) {
    render_dropdown_menu_sdl3_internal(renderer, component);
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
    static IRComponent* prev_hovered = NULL;  // Track hover state for transitions

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
                    // Check for modal backdrop click first (highest priority)
                    IRComponent* open_modal = find_open_modal(renderer->last_root);

                    if (open_modal && is_click_on_modal_backdrop(open_modal,
                            (float)event.button.x, (float)event.button.y)) {
                        // Clicked on modal backdrop - close the modal
                        // Mark the modal as closed by updating custom_data string
                        // This will cause the modal to not render on next frame
                        // Note: The reactive system should rebuild UI when state changes
                        if (open_modal->custom_data) {
                            const char* state_str = (const char*)open_modal->custom_data;
                            if (strncmp(state_str, "open", 4) == 0) {
                                // Format: "open|title" -> "closed|title"
                                const char* title_part = strchr(state_str, '|');
                                if (title_part) {
                                    // Construct "closed|title"
                                    size_t title_len = strlen(title_part);  // includes the '|'
                                    char* new_state = malloc(6 + title_len + 1);  // "closed" + "|title" + null
                                    if (new_state) {
                                        memcpy(new_state, "closed", 6);
                                        memcpy(new_state + 6, title_part, title_len + 1);
                                        ir_set_custom_data(open_modal, new_state);
                                        free(new_state);
                                    }
                                } else {
                                    // No title, just set to "closed"
                                    ir_set_custom_data(open_modal, "closed");
                                }
                            }
                        }
                        break;  // Don't process click further
                    }

                    IRComponent* clicked = NULL;

                    // Check if click is in an open dropdown menu first
                    IRComponent* dropdown_at_point = find_dropdown_menu_at_point(
                        renderer->last_root,
                        (float)event.button.x,
                        (float)event.button.y
                    );

                    if (dropdown_at_point) {
                        clicked = dropdown_at_point;
                    } else if (open_modal) {
                        // When modal is open, ONLY search within the modal's children
                        // Don't search the rest of the tree (which is behind the modal)
                        clicked = ir_find_component_at_point(
                            open_modal,
                            (float)event.button.x,
                            (float)event.button.y
                        );
                    } else {
                        clicked = ir_find_component_at_point(
                            renderer->last_root,
                            (float)event.button.x,
                            (float)event.button.y
                        );
                    }

                    if (clicked) {
                        // Don't process clicks on disabled components
                        if (clicked->is_disabled) {
                            break;
                        }

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
                        if (ir_event && ir_event->logic_id) {
                            // C event (c_click_*, c_change_*, etc.)
                            if (strncmp(ir_event->logic_id, "c_", 2) == 0) {
                                kryon_c_event_bridge(ir_event->logic_id);
                            }
                            // Generic fallback - handle via executor
                            else {
                                printf("Click on component ID %u (logic: %s)\n",
                                       clicked->id, ir_event->logic_id);
                                IRExecutorContext* executor = get_executor_from_state_mgr();
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
                                IRLinkData* link_data = (IRLinkData*)clicked->custom_data;
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
                        // Check if modal is open - search only modal subtree for hover
                        IRComponent* open_modal = find_open_modal(renderer->last_root);
                        if (open_modal) {
                            hovered = ir_find_component_at_point(
                                open_modal,
                                (float)event.motion.x,
                                (float)event.motion.y
                            );
                        } else {
                            hovered = ir_find_component_at_point(
                                renderer->last_root,
                                (float)event.motion.x,
                                (float)event.motion.y
                            );
                        }

                        // Clear hover state for any open dropdowns
                        #define MAX_DROPDOWNS_TO_CLEAR 10
                        IRComponent* all_dropdowns[MAX_DROPDOWNS_TO_CLEAR];
                        int count = 0;
                        collect_open_dropdowns(renderer->last_root, all_dropdowns, &count, MAX_DROPDOWNS_TO_CLEAR);
                        for (int i = 0; i < count; i++) {
                            ir_set_dropdown_hovered_index(all_dropdowns[i], -1);
                        }
                    }

                    // Validate hovered component is visible before using it
                    bool hovered_is_valid = (hovered != NULL &&
                                             (!hovered->style || hovered->style->visible));

                    // Update global hover state (used for hover effects in rendering)
                    g_hovered_component = hovered_is_valid ? hovered : NULL;

                    // Update pseudo-state bitmask for hover transitions
                    // Clear hover bit from previous component
                    if (prev_hovered && prev_hovered != g_hovered_component && prev_hovered->style) {
                        prev_hovered->style->current_pseudo_states &= ~(1u << 0);  // Clear bit 0 (hover)
                    }
                    // Set hover bit on new hovered component
                    if (g_hovered_component && g_hovered_component->style && g_hovered_component != prev_hovered) {
                        g_hovered_component->style->current_pseudo_states |= (1u << 0);  // Set bit 0 (hover)
                    }
                    prev_hovered = g_hovered_component;

                    // Set cursor to hand for clickable components
                    SDL_Cursor* desired_cursor;
                    if (is_in_dropdown_menu ||
                        (hovered_is_valid &&
                         !hovered->is_disabled &&
                         (hovered->type == IR_COMPONENT_BUTTON ||
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

                        // Sync to bound variable
                        IRExecutorContext* exec_ctx = get_executor_from_state_mgr();
                        if (exec_ctx) {
                            ir_executor_sync_input_to_var(exec_ctx, focused_input);
                        }

                        // Sync to bound reactive signal (two-way binding)
                        kryon_sync_input_to_signal(focused_input);

                        free(combined);
                    }
                }
                break;

            case SDL_EVENT_KEY_UP:
                // ESC to quit (disabled by default, enable with KRYON_ENABLE_ESCAPE_QUIT=1)
                if (event.key.scancode == SDL_SCANCODE_ESCAPE && getenv("KRYON_ENABLE_ESCAPE_QUIT")) {
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

                // ESC to close open modal (highest priority)
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    IRComponent* open_modal = find_open_modal(renderer->last_root);
                    if (open_modal) {
                        // Close the modal by updating custom_data properly
                        if (open_modal->custom_data) {
                            const char* state_str = (const char*)open_modal->custom_data;
                            if (strncmp(state_str, "open", 4) == 0) {
                                // Format: "open|title" -> "closed|title"
                                const char* title_part = strchr(state_str, '|');
                                if (title_part) {
                                    size_t title_len = strlen(title_part);
                                    char* new_state = malloc(6 + title_len + 1);
                                    if (new_state) {
                                        memcpy(new_state, "closed", 6);
                                        memcpy(new_state + 6, title_part, title_len + 1);
                                        ir_set_custom_data(open_modal, new_state);
                                        free(new_state);
                                    }
                                } else {
                                    ir_set_custom_data(open_modal, "closed");
                                }
                            }
                        }
                        break;  // Don't process ESC further
                    }
                }

                // ESC to quit (disabled by default, enable with KRYON_ENABLE_ESCAPE_QUIT=1)
                if (event.key.scancode == SDL_SCANCODE_ESCAPE && getenv("KRYON_ENABLE_ESCAPE_QUIT")) {
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

                            // Sync to bound variable
                            IRExecutorContext* exec_ctx = get_executor_from_state_mgr();
                            if (exec_ctx) {
                                ir_executor_sync_input_to_var(exec_ctx, focused_input);
                            }

                            // Sync to bound reactive signal (two-way binding)
                            kryon_sync_input_to_signal(focused_input);

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
