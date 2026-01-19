/**
 * Raylib Input - Event Handling for Raylib Backend
 *
 * Handles mouse, keyboard, and window events for the Raylib backend.
 * Converts Raylib input to appropriate component interactions.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>

#include "../../desktop_internal.h"
#include "raylib_renderer.h"
#include <ir_builder.h>
#include <ir_executor.h>
#include <ir_state_manager.h>

// Forward declarations from desktop_input_helpers.c (platform-agnostic)
extern InputRuntimeState* get_input_state(uint32_t id);
extern IRComponent* find_dropdown_menu_at_point(IRComponent* root, float x, float y);
extern void collect_open_dropdowns(IRComponent* component, IRComponent** dropdown_list, int* count, int max_count);

// Forward declaration from C bindings event bridge
extern void kryon_c_event_bridge(const char* logic_id);

// Forward declarations for shutdown API
extern bool kryon_request_shutdown(DesktopIRRenderer* renderer, KryonShutdownReason reason);

// Helper to get executor from global state manager
static IRExecutorContext* get_executor_from_state_mgr(void) {
    extern IRStateManager* ir_state_get_global(void);
    IRStateManager* state_mgr = ir_state_get_global();
    return state_mgr ? ir_state_manager_get_executor(state_mgr) : NULL;
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

/**
 * Handle mouse button click
 */
static void handle_mouse_click(DesktopIRRenderer* renderer, IRComponent* root, float x, float y) {
    if (!renderer || !root) return;

    printf("[raylib] Mouse click at (%.0f, %.0f)\n", x, y);

    // FIRST: Check if clicking on any open dropdown menu overlay
    IRComponent* clicked = find_dropdown_menu_at_point(root, x, y);

    if (!clicked) {
        // SECOND: Find regular component at click point
        clicked = ir_find_component_at_point(root, x, y);
    }

    if (!clicked) {
        printf("[raylib] No component found at click point\n");
        return;
    }

    printf("[raylib] Clicked component ID=%u type=%d\n", clicked->id, clicked->type);

    // Find and trigger IR_EVENT_CLICK handler
    IREvent* ir_event = ir_find_event(clicked, IR_EVENT_CLICK);

    // Handle different component types
    switch (clicked->type) {
        case IR_COMPONENT_BUTTON: {
            if (ir_event && ir_event->logic_id) {
                // Lua event handler
                if (strncmp(ir_event->logic_id, "lua_event_", 10) == 0) {
                    uint32_t handler_id = 0;
                    if (sscanf(ir_event->logic_id + 10, "%u", &handler_id) == 1) {
                        if (renderer->lua_event_callback) {
                            renderer->lua_event_callback(handler_id, IR_EVENT_CLICK, NULL);
                        }
                    }
                }
                // C event (c_click_*, c_change_*, etc.)
                else if (strncmp(ir_event->logic_id, "c_", 2) == 0) {
                    kryon_c_event_bridge(ir_event->logic_id);
                }
                // Generic fallback - try executor
                else {
                    IRExecutorContext* executor = get_executor_from_state_mgr();
                    if (executor) {
                        ir_executor_set_root(executor, renderer->last_root);
                        ir_executor_handle_event_by_logic_id(executor, clicked->id, ir_event->logic_id);
                    }
                }
            }
            // No else clause - buttons without events are valid
            break;
        }

        case IR_COMPONENT_CHECKBOX: {
            if (ir_event && ir_event->logic_id) {
                // Toggle checkbox state FIRST (before calling handlers)
                ir_toggle_checkbox_state(clicked);

                // Lua event handler
                if (strncmp(ir_event->logic_id, "lua_event_", 10) == 0) {
                    uint32_t handler_id = 0;
                    if (sscanf(ir_event->logic_id + 10, "%u", &handler_id) == 1) {
                        if (renderer->lua_event_callback) {
                            renderer->lua_event_callback(handler_id, IR_EVENT_CLICK, NULL);
                        }
                    }
                }
                // C event
                else if (strncmp(ir_event->logic_id, "c_", 2) == 0) {
                    kryon_c_event_bridge(ir_event->logic_id);
                }
                // Generic fallback - try executor
                else {
                    IRExecutorContext* executor = get_executor_from_state_mgr();
                    if (executor) {
                        ir_executor_set_root(executor, renderer->last_root);
                        ir_executor_handle_event_by_logic_id(executor, clicked->id, ir_event->logic_id);
                    }
                }
            } else {
                // No event handler - just toggle state
                ir_toggle_checkbox_state(clicked);
            }
            break;
        }

        case IR_COMPONENT_DROPDOWN: {
            IRDropdownState* state = ir_get_dropdown_state(clicked);
            if (!state) break;

            // Check if clicking on dropdown menu overlay (when open)
            if (state->is_open) {
                IRRenderedBounds bounds = clicked->rendered_bounds;
                float menu_y = bounds.y + bounds.height;
                float menu_height = state->option_count * 35.0f; // Standard option height

                // Check if click is within menu bounds
                if (y >= menu_y && y < menu_y + menu_height) {
                    // Calculate which option was clicked
                    uint32_t option_index = (uint32_t)((y - menu_y) / 35.0f);

                    if (option_index < state->option_count) {
                        // Valid option clicked - select it
                        ir_set_dropdown_selected_index(clicked, (int32_t)option_index);
                        ir_set_dropdown_open_state(clicked, false); // Close menu

                        // Fire event if handler exists
                        if (ir_event && ir_event->logic_id) {
                            // Lua event handler
                            if (strncmp(ir_event->logic_id, "lua_event_", 10) == 0) {
                                uint32_t handler_id = 0;
                                if (sscanf(ir_event->logic_id + 10, "%u", &handler_id) == 1) {
                                    if (renderer->lua_event_callback) {
                                        renderer->lua_event_callback(handler_id, IR_EVENT_CLICK, NULL);
                                    }
                                }
                            }
                            // C event
                            else if (strncmp(ir_event->logic_id, "c_", 2) == 0) {
                                kryon_c_event_bridge(ir_event->logic_id);
                            }
                            // Generic fallback - try executor
                            else {
                                IRExecutorContext* executor = get_executor_from_state_mgr();
                                if (executor) {
                                    ir_executor_set_root(executor, renderer->last_root);
                                    ir_executor_handle_event_by_logic_id(executor, clicked->id, ir_event->logic_id);
                                }
                            }
                        }
                        return; // Done - option selected
                    }
                } else {
                    // Click outside menu - just close it
                    ir_set_dropdown_open_state(clicked, false);
                    return;
                }
            }

            // Clicking on dropdown header (not menu) - toggle open/closed
            if (state->is_open) {
                ir_set_dropdown_open_state(clicked, false);
            } else {
                ir_set_dropdown_open_state(clicked, true);
            }

            // Fire event for toggle action
            if (ir_event && ir_event->logic_id) {
                // Lua event handler
                if (strncmp(ir_event->logic_id, "lua_event_", 10) == 0) {
                    uint32_t handler_id = 0;
                    if (sscanf(ir_event->logic_id + 10, "%u", &handler_id) == 1) {
                        if (renderer->lua_event_callback) {
                            renderer->lua_event_callback(handler_id, IR_EVENT_CLICK, NULL);
                        }
                    }
                }
                // C event
                else if (strncmp(ir_event->logic_id, "c_", 2) == 0) {
                    kryon_c_event_bridge(ir_event->logic_id);
                }
                // Generic fallback - try executor
                else {
                    IRExecutorContext* executor = get_executor_from_state_mgr();
                    if (executor) {
                        ir_executor_set_root(executor, renderer->last_root);
                        ir_executor_handle_event_by_logic_id(executor, clicked->id, ir_event->logic_id);
                    }
                }
            }
            break;
        }

        case IR_COMPONENT_INPUT:
            printf("[raylib] Input field %u focused\n", clicked->id);
            // Set focus
            InputRuntimeState* state = get_input_state(clicked->id);
            if (state) {
                state->focused = true;
            }
            break;

        default:
            break;
    }
}

/**
 * Handle keyboard text input for focused input field
 * NOTE: Only consume characters if there's a focused Kryon input field
 * to avoid stealing input from native raylib code (like game pause menus)
 */
static void handle_text_input(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return;

    // IMPORTANT: Don't call GetCharPressed() unless we have a focused Kryon input field
    // GetCharPressed() consumes the character, so calling it here would steal
    // input from native raylib applications
    // TODO: Find focused input and only then consume characters

    // For now, disable this to let game code handle text input
    // int key = GetCharPressed();
    // if (key == 0) return;
    // printf("[raylib] Character pressed: %c (%d)\n", (char)key, key);
}

/**
 * Handle keyboard special keys (backspace, delete, arrows, etc.)
 */
static void handle_keyboard_keys(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return;

    // Handle backspace
    if (IsKeyPressed(KEY_BACKSPACE)) {
        printf("[raylib] Backspace pressed\n");
        // TODO: Delete character before cursor in focused input
    }

    // Handle delete
    if (IsKeyPressed(KEY_DELETE)) {
        printf("[raylib] Delete pressed\n");
        // TODO: Delete character at cursor in focused input
    }

    // Handle arrow keys
    if (IsKeyPressed(KEY_LEFT)) {
        printf("[raylib] Left arrow pressed\n");
        // TODO: Move cursor left in focused input
    }

    if (IsKeyPressed(KEY_RIGHT)) {
        printf("[raylib] Right arrow pressed\n");
        // TODO: Move cursor right in focused input
    }

    // Handle enter
    if (IsKeyPressed(KEY_ENTER)) {
        printf("[raylib] Enter pressed\n");
        // TODO: Submit focused input or trigger button
    }

    // Handle escape
    if (IsKeyPressed(KEY_ESCAPE)) {
        printf("[raylib] Escape pressed\n");
        // TODO: Close dropdown menus, unfocus inputs
    }
}

/**
 * Handle mouse motion and cursor updates
 */
static void handle_mouse_motion(DesktopIRRenderer* renderer, IRComponent* root, float x, float y) {
    if (!renderer || !root) return;

    // Check if mouse is over a dropdown menu
    IRComponent* dropdown_at_point = find_dropdown_menu_at_point(root, x, y);
    bool is_in_dropdown_menu = (dropdown_at_point != NULL);

    IRComponent* hovered = NULL;

    if (is_in_dropdown_menu) {
        // Update dropdown hover state
        IRDropdownState* state = ir_get_dropdown_state(dropdown_at_point);
        if (state && state->is_open) {
            IRRenderedBounds bounds = dropdown_at_point->rendered_bounds;
            float menu_y = bounds.y + bounds.height;
            int32_t new_hover = (int32_t)((y - menu_y) / 35.0f);
            if (new_hover >= 0 && new_hover < (int32_t)state->option_count) {
                ir_set_dropdown_hovered_index(dropdown_at_point, new_hover);
            }
        }
    } else {
        // Find regular component at mouse position
        hovered = ir_find_component_at_point(root, x, y);

        // Clear hover state for any open dropdowns
        #define MAX_DROPDOWNS_TO_CLEAR 10
        IRComponent* all_dropdowns[MAX_DROPDOWNS_TO_CLEAR];
        int count = 0;
        collect_open_dropdowns(root, all_dropdowns, &count, MAX_DROPDOWNS_TO_CLEAR);
        for (int i = 0; i < count; i++) {
            ir_set_dropdown_hovered_index(all_dropdowns[i], -1);
        }
    }

    // Update global hover state
    g_hovered_component = hovered;

    // Validate hovered component is visible before using it
    bool hovered_is_valid = (hovered != NULL &&
                             (!hovered->style || hovered->style->visible));

    // Change cursor based on hovered component
    // Raylib cursor types: MOUSE_CURSOR_DEFAULT, MOUSE_CURSOR_POINTING_HAND
    if (is_in_dropdown_menu ||
        (hovered_is_valid && (hovered->type == IR_COMPONENT_BUTTON ||
                              hovered->type == IR_COMPONENT_INPUT ||
                              hovered->type == IR_COMPONENT_CHECKBOX ||
                              hovered->type == IR_COMPONENT_DROPDOWN))) {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    } else {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }
}

// ============================================================================
// MAIN EVENT LOOP
// ============================================================================

/**
 * Poll and handle Raylib events
 *
 * This is called from raylib_poll_events in raylib_renderer.c
 */
void raylib_handle_input_events(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return;

    // Check if window is still valid BEFORE calling any GLFW functions
    if (!IsWindowReady()) {
        // Window destroyed unexpectedly - signal shutdown
        renderer->running = false;
        return;
    }

    // Check for window close - use graceful shutdown API
    if (WindowShouldClose()) {
        // Signal main loop that window close was requested
        // The main loop will call kryon_request_shutdown with WINDOW reason
        renderer->running = false;
        return;
    }

    // Handle mouse motion and cursor updates
    Vector2 mouse_pos = GetMousePosition();
    handle_mouse_motion(renderer, root, mouse_pos.x, mouse_pos.y);

    // Handle mouse button clicks
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        printf("[raylib] Mouse button pressed detected\n");
        handle_mouse_click(renderer, root, mouse_pos.x, mouse_pos.y);
    }

    // Handle text input
    handle_text_input(renderer, root);

    // Handle keyboard special keys
    handle_keyboard_keys(renderer, root);

    // TODO: Handle mouse wheel for scrolling

    // Handle window resize events
    if (IsWindowResized()) {
        int new_width = GetScreenWidth();
        int new_height = GetScreenHeight();

        printf("[raylib] Window resized to %dx%d\n", new_width, new_height);

        // Update config (used by ir_layout_compute_tree)
        renderer->config.window_width = new_width;
        renderer->config.window_height = new_height;

        // Update backend data cache
        RaylibRendererData* data = raylib_get_data(renderer);
        if (data) {
            data->window_width = new_width;
            data->window_height = new_height;
        }

        // Emit resize event
        DesktopEvent desktop_event = {0};
        desktop_event.type = DESKTOP_EVENT_WINDOW_RESIZE;
        desktop_event.data.resize.width = new_width;
        desktop_event.data.resize.height = new_height;
        renderer->needs_relayout = true;

        if (renderer->event_callback) {
            renderer->event_callback(&desktop_event, renderer->event_user_data);
        }
    }
}
