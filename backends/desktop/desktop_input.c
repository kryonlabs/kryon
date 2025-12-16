/*
 * desktop_input.c - Input/Event Handling Module
 *
 * This module handles input events and event processing for the desktop renderer.
 * It manages:
 * - SDL3 event loop processing (keyboard, mouse, window events)
 * - Input field focus management and text editing
 * - Dropdown menu hit testing and selection
 * - Tab group drag-and-drop handling
 * - Cursor management and visual feedback
 * - Input state tracking (caret position, scroll offset, focus state)
 *
 * Architecture:
 * The input handling is tightly integrated with the IR component tree.
 * Events are dispatched to components via hit testing, and component-specific
 * handlers (buttons, checkboxes, dropdowns, inputs) are invoked through
 * Nim bridge functions when available.
 */

#define _GNU_SOURCE
#include "desktop_internal.h"
#include "../../ir/ir_executor.h"

#ifdef ENABLE_SDL3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// ============================================================================
// INPUT STATE MANAGEMENT
// ============================================================================
// NOTE: input_states and input_state_count are defined in desktop_globals.c

/**
 * Get or create input runtime state for a component
 *
 * Maintains a table of input states indexed by component ID.
 * Input state tracks caret position, scroll offset, focus, and blink state.
 *
 * Args:
 *   id - Component ID to get state for
 *
 * Returns:
 *   Pointer to InputRuntimeState, or NULL if table is full
 */
InputRuntimeState* get_input_state(uint32_t id) {
    for (size_t i = 0; i < input_state_count; i++) {
        if (input_states[i].id == id) return &input_states[i];
    }
    if (input_state_count < sizeof(input_states) / sizeof(input_states[0])) {
        input_states[input_state_count] = (InputRuntimeState){
            .id = id,
            .cursor_index = 0,
            .scroll_x = 0.0f,
            .focused = false,
            .last_blink_ms = 0,
            .caret_visible = true
        };
        input_state_count++;
        return &input_states[input_state_count - 1];
    }
    return NULL;
}

/**
 * Ensure caret is visible within input field's visible area
 *
 * Adjusts scroll_x to keep the caret within view, with a small margin.
 * Accounts for input field padding and available space.
 *
 * Args:
 *   renderer - Desktop renderer for font measurement
 *   input - Input component to adjust scroll for
 *   istate - Input state (contains cursor_index and scroll_x)
 *   font - Font for measuring text width
 *   pad_left - Left padding of input field
 *   pad_right - Right padding of input field
 */
void ensure_caret_visible(DesktopIRRenderer* renderer,
                          IRComponent* input,
                          InputRuntimeState* istate,
                          TTF_Font* font,
                          float pad_left,
                          float pad_right) {
    if (!renderer || !input || !istate) return;
    const char* txt = input->text_content ? input->text_content : "";
    size_t cur = istate->cursor_index;
    size_t len = strlen(txt);
    if (cur > len) cur = len;
    char* prefix = strndup(txt, cur);
    float caret_local = measure_text_width(font ? font : renderer->default_font, prefix);
    free(prefix);

    float avail = input->rendered_bounds.width - pad_left - pad_right;
    if (avail < 0) avail = 0;

    // Clamp scroll to content width
    float total_width = measure_text_width(font ? font : renderer->default_font, txt);
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
// DROPDOWN MENU RENDERING AND HIT TESTING
// ============================================================================

/**
 * Render dropdown menu overlay
 *
 * Draws dropdown menu below the dropdown button with all options.
 * Highlights hovered or selected options.
 * Used in a second rendering pass to ensure menus appear above other content.
 *
 * Args:
 *   renderer - Desktop renderer context
 *   component - Dropdown component to render menu for
 */
void render_dropdown_menu_sdl3(DesktopIRRenderer* renderer, IRComponent* component) {
    if (!renderer || !component || component->type != IR_COMPONENT_DROPDOWN) return;

    IRDropdownState* dropdown_state = ir_get_dropdown_state(component);
    if (!dropdown_state || !dropdown_state->is_open || dropdown_state->option_count == 0) return;

    // Get cached rendered bounds from the component
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
    SDL_SetRenderDrawColor(renderer->renderer, 50, 50, 50, 100);
    SDL_FRect shadow_rect = {menu_rect.x + 2, menu_rect.y + 2, menu_rect.w, menu_rect.h};
    SDL_RenderFillRect(renderer->renderer, &shadow_rect);

    SDL_SetRenderDrawColor(renderer->renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
    SDL_RenderFillRect(renderer->renderer, &menu_rect);

    SDL_SetRenderDrawColor(renderer->renderer, border_color.r, border_color.g, border_color.b, border_color.a);
    SDL_RenderRect(renderer->renderer, &menu_rect);

    // Render each option
    float option_height = 35.0f;
    for (uint32_t i = 0; i < dropdown_state->option_count; i++) {
        float option_y = menu_y + i * option_height;

        // Highlight hovered or selected option
        if ((int32_t)i == dropdown_state->hovered_index || (int32_t)i == dropdown_state->selected_index) {
            SDL_Color hover_color = {224, 224, 224, 255};
            SDL_SetRenderDrawColor(renderer->renderer, hover_color.r, hover_color.g, hover_color.b, hover_color.a);
            SDL_FRect option_rect = {menu_rect.x, option_y, menu_rect.w, option_height};
            SDL_RenderFillRect(renderer->renderer, &option_rect);
        }

        // Render option text
        if (dropdown_state->options[i] && renderer->default_font) {
            SDL_Surface* opt_surface = TTF_RenderText_Blended(renderer->default_font,
                                                              dropdown_state->options[i],
                                                              strlen(dropdown_state->options[i]),
                                                              text_color);
            if (opt_surface) {
                SDL_Texture* opt_texture = SDL_CreateTextureFromSurface(renderer->renderer, opt_surface);
                if (opt_texture) {
                    SDL_SetTextureScaleMode(opt_texture, SDL_SCALEMODE_NEAREST);  // Crisp text
                    SDL_FRect opt_text_rect = {
                        .x = roundf(menu_rect.x + 10),
                        .y = roundf(option_y + (option_height - opt_surface->h) / 2.0f),
                        .w = (float)opt_surface->w,
                        .h = (float)opt_surface->h
                    };
                    SDL_RenderTexture(renderer->renderer, opt_texture, NULL, &opt_text_rect);
                    SDL_DestroyTexture(opt_texture);
                }
                SDL_DestroySurface(opt_surface);
            }
        }
    }
}

/**
 * Collect all open dropdowns from component tree
 *
 * Recursively traverses the component tree and collects all open dropdowns.
 * Used for hit testing and state management.
 *
 * Args:
 *   component - Component to search (root on first call)
 *   dropdown_list - Array to fill with open dropdown components
 *   count - Pointer to current count (incremented as dropdowns are added)
 *   max_count - Maximum entries in dropdown_list
 */
void collect_open_dropdowns(IRComponent* component, IRComponent** dropdown_list, int* count, int max_count) {
    if (!component || !dropdown_list || !count || *count >= max_count) return;

    // Check if this component is an open dropdown
    if (component->type == IR_COMPONENT_DROPDOWN) {
        IRDropdownState* state = ir_get_dropdown_state(component);
        if (state && state->is_open && component->rendered_bounds.valid) {
            dropdown_list[*count] = component;
            (*count)++;
        }
    }

    // Recursively check children
    for (uint32_t i = 0; i < component->child_count && *count < max_count; i++) {
        collect_open_dropdowns(component->children[i], dropdown_list, count, max_count);
    }
}

/**
 * Hit test to find dropdown menu at point
 *
 * Checks if a point (typically mouse position) is within an open dropdown menu.
 * Respects z-index by checking dropdowns in reverse order.
 *
 * Args:
 *   root - Root component to search from
 *   x - X coordinate to test
 *   y - Y coordinate to test
 *
 * Returns:
 *   Dropdown component if point is in menu, NULL otherwise
 */
IRComponent* find_dropdown_menu_at_point(IRComponent* root, float x, float y) {
    if (!root) return NULL;

    // Collect all open dropdowns
    #define MAX_OPEN_DROPDOWNS_CHECK 10
    IRComponent* open_dropdowns[MAX_OPEN_DROPDOWNS_CHECK];
    int dropdown_count = 0;
    collect_open_dropdowns(root, open_dropdowns, &dropdown_count, MAX_OPEN_DROPDOWNS_CHECK);

    // Check each open dropdown menu (in reverse order for proper z-index)
    for (int i = dropdown_count - 1; i >= 0; i--) {
        IRComponent* dropdown = open_dropdowns[i];
        IRDropdownState* state = ir_get_dropdown_state(dropdown);
        if (!state || !state->is_open) continue;

        IRRenderedBounds bounds = dropdown->rendered_bounds;
        if (!bounds.valid) continue;

        // Calculate menu area
        float menu_y = bounds.y + bounds.height;
        float menu_height = fminf(state->option_count * 35.0f, 200.0f);

        // Check if point is in menu area
        if (x >= bounds.x && x < bounds.x + bounds.width &&
            y >= menu_y && y < menu_y + menu_height) {
            return dropdown;
        }
    }

    return NULL;
}

// ============================================================================
// SDL3 EVENT HANDLING
// ============================================================================

/**
 * Main SDL3 event loop
 *
 * Processes all pending SDL3 events and dispatches them to IR components.
 * Handles:
 * - Mouse clicks (hit testing, input focus, button/checkbox handlers)
 * - Mouse movement (cursor changes, dropdown hover states)
 * - Mouse wheel (markdown scroll)
 * - Text input (adds to focused input field with unicode support)
 * - Keyboard (backspace for input, escape/ctrl+q for quit)
 * - Window resize
 *
 * Static state (NOT thread-safe):
 * - dragging_tabgroup: Currently dragged tab group (for drag-and-drop)
 * - focused_input: Currently focused input field (for text input)
 *
 * Args:
 *   renderer - Desktop renderer context
 */
void handle_sdl3_events(DesktopIRRenderer* renderer) {
    // NOTE: These static variables are NOT thread-safe
    // This renderer assumes single-threaded usage
    // If multi-threading is needed, move these to DesktopIRRenderer struct
    static TabGroupState* dragging_tabgroup = NULL;
    static IRComponent* focused_input = NULL;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        DesktopEvent desktop_event = {0};

#ifdef ENABLE_SDL3
        desktop_event.timestamp = SDL_GetTicks();
#else
        desktop_event.timestamp = 0;
#endif

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

                // Dispatch click event to IR component at mouse position
                if (renderer->last_root) {
                    IRComponent* clicked = NULL;

                    // FIRST: Check if click is in an open dropdown menu (higher priority due to z-index)
                    IRComponent* dropdown_at_point = find_dropdown_menu_at_point(
                        renderer->last_root,
                        (float)event.button.x,
                        (float)event.button.y
                    );

                    if (dropdown_at_point) {
                        // Click is in a dropdown menu - use the dropdown component
                        clicked = dropdown_at_point;
                    } else {
                        // No dropdown menu at this point - do normal hit testing
                        clicked = ir_find_component_at_point(
                            renderer->last_root,
                            (float)event.button.x,
                            (float)event.button.y
                        );
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
                            SDL_StartTextInput(renderer->window);
                        } else if (focused_input) {
                            InputRuntimeState* istate = get_input_state(focused_input->id);
                            if (istate) istate->focused = false;
                            focused_input = NULL;
                            SDL_StopTextInput(renderer->window);
                        }

                        // Handle IR-level tab clicks (new system)
                        if (clicked->type == IR_COMPONENT_TAB) {
                            // Find the TabGroup ancestor
                            IRComponent* tab_group = clicked->parent;
                            while (tab_group && tab_group->type != IR_COMPONENT_TAB_GROUP) {
                                tab_group = tab_group->parent;
                            }

                            if (tab_group && tab_group->custom_data) {
                                // Get the TabGroupState from custom_data
                                TabGroupState* tg_state = (TabGroupState*)tab_group->custom_data;

                                // Find which tab was clicked
                                IRComponent* tab_bar = clicked->parent;
                                if (tab_bar && tab_bar->type == IR_COMPONENT_TAB_BAR) {
                                    for (uint32_t i = 0; i < tab_bar->child_count; i++) {
                                        if (tab_bar->children[i] == clicked) {
                                            // Switch to the clicked tab (triggers panel switching!)
                                            printf("[tabs] Clicked tab %u in TabGroup %u\n", i, tab_group->id);
                                            ir_tabgroup_select(tg_state, (int)i);
                                            break;
                                        }
                                    }
                                }
                            }
                        }

                        // Start tab drag if parent carries TabGroupState in custom_data
                        if (clicked->parent && clicked->parent->custom_data) {
                            TabGroupState* tg_state = (TabGroupState*)clicked->parent->custom_data;
                            if (tg_state) {
                                ir_tabgroup_handle_drag(tg_state, (float)event.button.x, (float)event.button.y, true, false);
                                dragging_tabgroup = tg_state;
                            }
                        }

                        // Find and trigger IR_EVENT_CLICK handler
                        IREvent* ir_event = ir_find_event(clicked, IR_EVENT_CLICK);
                        if (ir_event) {
                            // Use legacy logic_id system
                            if (ir_event->logic_id) {
                                printf("Click on component ID %u (logic: %s)\n",
                                       clicked->id, ir_event->logic_id);

                                // Check if this is a Nim button handler
                                if (strncmp(ir_event->logic_id, "nim_button_", 11) == 0) {
                                // Check if this button is actually a tab in a TabGroup
                                // by looking at parent's custom_data for TabGroupState
                                bool handled_as_tab = false;
                                if (clicked->parent && clicked->parent->custom_data) {
                                    TabGroupState* tg_state = (TabGroupState*)clicked->parent->custom_data;

                                    // Verify this looks like a valid TabGroupState (tab_bar matches parent)
                                    if (tg_state && tg_state->tab_bar == clicked->parent) {
                                        // Find which tab was clicked
                                        for (uint32_t i = 0; i < tg_state->tab_count; i++) {
                                            if (tg_state->tabs[i] == clicked) {
                                                // C handles tab selection (panel switching, visuals)
                                                ir_tabgroup_handle_tab_click(tg_state, i);
                                                // Also call handler for user's onClick
                                                IRExecutorContext* executor = ir_executor_get_global();
                                                if (executor) {
                                                    ir_executor_handle_event(executor, clicked->id, "click");
                                                } else {
                                                    extern void nimButtonBridge(uint32_t componentId);
                                                    nimButtonBridge(clicked->id);
                                                }
                                                handled_as_tab = true;
                                                break;
                                            }
                                        }
                                    }
                                }

                                // If not a tab, use normal button bridge
                                if (!handled_as_tab) {
                                    // Try executor first (for .kir file execution)
                                    IRExecutorContext* executor = ir_executor_get_global();
                                    if (executor) {
                                        // Set root for UI updates
                                        ir_executor_set_root(executor, renderer->last_root);
                                        // Execute via IR executor using logic_id (handles .kir logic blocks)
                                        ir_executor_handle_event_by_logic_id(executor, clicked->id, ir_event->logic_id);
                                    } else {
                                        // Fallback to Nim bridge (for compiled Nim binaries)
                                        extern void nimButtonBridge(uint32_t componentId);
                                        nimButtonBridge(clicked->id);
                                    }
                                }
                            }
                            // Check if this is a Nim checkbox handler
                            else if (strncmp(ir_event->logic_id, "nim_checkbox_", 13) == 0) {
                                // Toggle checkbox state BEFORE calling user handler
                                if (clicked->type == IR_COMPONENT_CHECKBOX) {
                                    ir_toggle_checkbox_state(clicked);
                                }
                                // Try executor first (for .kir file execution)
                                IRExecutorContext* executor = ir_executor_get_global();
                                if (executor) {
                                    // Set root for UI updates
                                    ir_executor_set_root(executor, renderer->last_root);
                                    ir_executor_handle_event_by_logic_id(executor, clicked->id, ir_event->logic_id);
                                } else {
                                    // Fallback to Nim bridge
                                    extern void nimCheckboxBridge(uint32_t componentId);
                                    nimCheckboxBridge(clicked->id);
                                }
                            }
                            // Check if this is a Nim dropdown handler
                            else if (strncmp(ir_event->logic_id, "nim_dropdown_", 13) == 0) {
                                if (clicked->type == IR_COMPONENT_DROPDOWN) {
                                    IRDropdownState* state = ir_get_dropdown_state(clicked);
                                    if (state) {
                                        // Get click coordinates
                                        float click_y = (float)event.button.y;
                                        IRRenderedBounds bounds = clicked->rendered_bounds;

                                        if (state->is_open) {
                                            // Check if click is in menu area
                                            float menu_y = bounds.y + bounds.height;
                                            float menu_height = fminf(state->option_count * 35.0f, 200.0f);

                                            if (click_y >= menu_y && click_y < menu_y + menu_height) {
                                                // Click in menu - select option
                                                uint32_t option_index = (uint32_t)((click_y - menu_y) / 35.0f);
                                                if (option_index < state->option_count) {
                                                    ir_set_dropdown_selected_index(clicked, (int32_t)option_index);
                                                    ir_set_dropdown_open_state(clicked, false);

                                                    // Call Nim handler with selected index
                                                    extern void nimDropdownBridge(uint32_t componentId, int32_t selectedIndex);
                                                    nimDropdownBridge(clicked->id, (int32_t)option_index);
                                                }
                                            } else {
                                                // Click outside menu - just close it
                                                ir_set_dropdown_open_state(clicked, false);
                                            }
                                        } else {
                                            // Dropdown closed - toggle it open
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
                                    // Set root for UI updates
                                    ir_executor_set_root(executor, renderer->last_root);
                                    // Use logic_id directly since component IDs may be remapped
                                    ir_executor_handle_event_by_logic_id(executor, clicked->id, ir_event->logic_id);
                                }
                            }
                            }  // end else if (ir_event->logic_id)
                        }  // end if (ir_event)
                        else {
                            // No event handler registered - handle built-in behaviors
                            // Checkboxes should toggle on click even without a handler
                            if (clicked->type == IR_COMPONENT_CHECKBOX) {
                                ir_toggle_checkbox_state(clicked);
                            }
                            // Dropdowns should toggle open/closed on click even without a handler
                            else if (clicked->type == IR_COMPONENT_DROPDOWN) {
                                IRDropdownState* state = ir_get_dropdown_state(clicked);
                                if (state) {
                                    // Get click coordinates
                                    float click_y = (float)event.button.y;
                                    IRRenderedBounds bounds = clicked->rendered_bounds;

                                    if (state->is_open) {
                                        // Check if click is in menu area
                                        float menu_y = bounds.y + bounds.height;
                                        float menu_height = fminf(state->option_count * 35.0f, 200.0f);

                                        if (click_y >= menu_y && click_y < menu_y + menu_height) {
                                            // Click in menu - select option
                                            uint32_t option_index = (uint32_t)((click_y - menu_y) / 35.0f);
                                            if (option_index < state->option_count) {
                                                ir_set_dropdown_selected_index(clicked, (int32_t)option_index);
                                            }
                                        }
                                        // Close dropdown after any click when open
                                        ir_set_dropdown_open_state(clicked, false);
                                    } else {
                                        // Dropdown closed - toggle it open
                                        ir_set_dropdown_open_state(clicked, true);
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

            case SDL_EVENT_MOUSE_WHEEL: {
                // Mouse wheel scrolling (now handled by plugins for custom components)
                // TODO: Add generic scrollable container support
                break;
            }

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

                // Dragging tabs (if active)
                if (dragging_tabgroup) {
                    ir_tabgroup_handle_drag(dragging_tabgroup, (float)event.motion.x, (float)event.motion.y, false, false);
                }

                // Update cursor based on what component is under the mouse
                if (renderer->last_root) {
                    IRComponent* hovered = NULL;
                    bool is_in_dropdown_menu = false;

                    // FIRST: Check if mouse is over an open dropdown menu (higher priority due to z-index)
                    IRComponent* dropdown_at_point = find_dropdown_menu_at_point(
                        renderer->last_root,
                        (float)event.motion.x,
                        (float)event.motion.y
                    );

                    if (dropdown_at_point) {
                        // Mouse is over a dropdown menu
                        hovered = dropdown_at_point;
                        is_in_dropdown_menu = true;

                        // Update hovered index within the menu
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
                        // No dropdown menu at this point - do normal hit testing
                        hovered = ir_find_component_at_point(
                            renderer->last_root,
                            (float)event.motion.x,
                            (float)event.motion.y
                        );

                        // Clear hover state for any open dropdowns since mouse is not over menu
                        #define MAX_DROPDOWNS_TO_CLEAR 10
                        IRComponent* all_dropdowns[MAX_DROPDOWNS_TO_CLEAR];
                        int count = 0;
                        collect_open_dropdowns(renderer->last_root, all_dropdowns, &count, MAX_DROPDOWNS_TO_CLEAR);
                        for (int i = 0; i < count; i++) {
                            ir_set_dropdown_hovered_index(all_dropdowns[i], -1);
                        }
                    }

                    // Set cursor to hand for clickable components or dropdown menu
                    SDL_Cursor* desired_cursor;
                    if (is_in_dropdown_menu ||
                        (hovered && (hovered->type == IR_COMPONENT_BUTTON ||
                                     hovered->type == IR_COMPONENT_INPUT ||
                                     hovered->type == IR_COMPONENT_CHECKBOX ||
                                     hovered->type == IR_COMPONENT_DROPDOWN))) {
                        desired_cursor = renderer->cursor_hand;
                    } else {
                        desired_cursor = renderer->cursor_default;
                    }

                    // Only update if cursor changed
                    if (desired_cursor != renderer->current_cursor) {
                        SDL_SetCursor(desired_cursor);
                        renderer->current_cursor = desired_cursor;
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
                            ensure_caret_visible(renderer, focused_input, istate, font, pad_left, pad_right);
                        }
                        extern bool nimInputBridge(IRComponent* component, const char* text);
                        nimInputBridge(focused_input, focused_input->text_content);

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
                // Quit on key RELEASE to prevent stuck key state (SDL bug #5550)
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    renderer->running = false;
                    break;
                }
                if (event.key.scancode == SDL_SCANCODE_Q && (event.key.mod & SDL_KMOD_CTRL)) {
                    renderer->running = false;
                    break;
                }
                break;

            case SDL_EVENT_KEY_DOWN: {
                // Ignore key repeat events
                if (event.key.repeat) {
                    break;
                }

                // Handle ESC on key DOWN for more responsive exit (in addition to KEY_UP)
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
                                ensure_caret_visible(renderer, focused_input, istate, font, pad_left, pad_right);
                            }
                            extern bool nimInputBridge(IRComponent* component, const char* text);
                            nimInputBridge(focused_input, focused_input->text_content);

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
            }

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
                // Force redraw when window becomes visible after occlusion
                renderer->needs_relayout = true;
                break;

            default:
                break;
        }
    }
}

#endif // ENABLE_SDL3
