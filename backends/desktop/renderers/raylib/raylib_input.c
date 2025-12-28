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
#include "../../../../ir/ir_builder.h"
#include "../../../../ir/ir_executor.h"

// Forward declarations from desktop_input_helpers.c (platform-agnostic)
extern InputRuntimeState* get_input_state(uint32_t id);
extern IRComponent* find_dropdown_menu_at_point(IRComponent* root, float x, float y);
extern void collect_open_dropdowns(IRComponent* component, IRComponent** dropdown_list, int* count, int max_count);

// Forward declaration from C bindings event bridge
extern void kryon_c_event_bridge(const char* logic_id);

// ============================================================================
// HIT TESTING
// ============================================================================

/**
 * Find component at screen coordinates using pre-computed layout
 */
static IRComponent* find_component_at_point(IRComponent* component, float x, float y) {
    if (!component || !component->rendered_bounds.valid) {
        return NULL;
    }

    // Check if point is within this component's bounds
    if (x < component->rendered_bounds.x ||
        x > component->rendered_bounds.x + component->rendered_bounds.width ||
        y < component->rendered_bounds.y ||
        y > component->rendered_bounds.y + component->rendered_bounds.height) {
        return NULL;
    }

    // Check children first (reverse order for proper z-index)
    for (int i = component->child_count - 1; i >= 0; i--) {
        IRComponent* child = find_component_at_point(component->children[i], x, y);
        if (child) return child;
    }

    // Return this component if it's interactive
    if (component->type == IR_COMPONENT_BUTTON ||
        component->type == IR_COMPONENT_CHECKBOX ||
        component->type == IR_COMPONENT_DROPDOWN ||
        component->type == IR_COMPONENT_INPUT) {
        return component;
    }

    return NULL;
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
        clicked = find_component_at_point(root, x, y);
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
                // Nim button handler
                if (strncmp(ir_event->logic_id, "nim_button_", 11) == 0) {
                    IRExecutorContext* executor = ir_executor_get_global();
                    if (executor) {
                        ir_executor_set_root(executor, renderer->last_root);
                        ir_executor_handle_event_by_logic_id(executor, clicked->id, ir_event->logic_id);
                    }
                }
                // Lua event handler
                else if (strncmp(ir_event->logic_id, "lua_event_", 10) == 0) {
                    uint32_t handler_id = 0;
                    if (sscanf(ir_event->logic_id + 10, "%u", &handler_id) == 1) {
                        if (renderer->lua_event_callback) {
                            renderer->lua_event_callback(handler_id, IR_EVENT_CLICK);
                        }
                    }
                }
                // C event (c_click_*, c_change_*, etc.)
                else if (strncmp(ir_event->logic_id, "c_", 2) == 0) {
                    kryon_c_event_bridge(ir_event->logic_id);
                }
                // Generic fallback - try executor
                else {
                    IRExecutorContext* executor = ir_executor_get_global();
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

                // Nim checkbox handler
                if (strncmp(ir_event->logic_id, "nim_checkbox_", 13) == 0) {
                    IRExecutorContext* executor = ir_executor_get_global();
                    if (executor) {
                        ir_executor_set_root(executor, renderer->last_root);
                        ir_executor_handle_event_by_logic_id(executor, clicked->id, ir_event->logic_id);
                    }
                }
                // Lua event handler (ADDED - was missing!)
                else if (strncmp(ir_event->logic_id, "lua_event_", 10) == 0) {
                    uint32_t handler_id = 0;
                    if (sscanf(ir_event->logic_id + 10, "%u", &handler_id) == 1) {
                        if (renderer->lua_event_callback) {
                            renderer->lua_event_callback(handler_id, IR_EVENT_CLICK);
                        }
                    }
                }
                // C event
                else if (strncmp(ir_event->logic_id, "c_", 2) == 0) {
                    kryon_c_event_bridge(ir_event->logic_id);
                }
                // Generic fallback - try executor
                else {
                    IRExecutorContext* executor = ir_executor_get_global();
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
                            // Nim dropdown handler
                            if (strncmp(ir_event->logic_id, "nim_dropdown_", 13) == 0) {
                                IRExecutorContext* executor = ir_executor_get_global();
                                if (executor) {
                                    ir_executor_set_root(executor, renderer->last_root);
                                    ir_executor_handle_event_by_logic_id(executor, clicked->id, ir_event->logic_id);
                                }
                            }
                            // Lua event handler (ADDED - was missing!)
                            else if (strncmp(ir_event->logic_id, "lua_event_", 10) == 0) {
                                uint32_t handler_id = 0;
                                if (sscanf(ir_event->logic_id + 10, "%u", &handler_id) == 1) {
                                    if (renderer->lua_event_callback) {
                                        renderer->lua_event_callback(handler_id, IR_EVENT_CLICK);
                                    }
                                }
                            }
                            // C event
                            else if (strncmp(ir_event->logic_id, "c_", 2) == 0) {
                                kryon_c_event_bridge(ir_event->logic_id);
                            }
                            // Generic fallback - try executor
                            else {
                                IRExecutorContext* executor = ir_executor_get_global();
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
                // Nim dropdown handler
                if (strncmp(ir_event->logic_id, "nim_dropdown_", 13) == 0) {
                    IRExecutorContext* executor = ir_executor_get_global();
                    if (executor) {
                        ir_executor_set_root(executor, renderer->last_root);
                        ir_executor_handle_event_by_logic_id(executor, clicked->id, ir_event->logic_id);
                    }
                }
                // Lua event handler
                else if (strncmp(ir_event->logic_id, "lua_event_", 10) == 0) {
                    uint32_t handler_id = 0;
                    if (sscanf(ir_event->logic_id + 10, "%u", &handler_id) == 1) {
                        if (renderer->lua_event_callback) {
                            renderer->lua_event_callback(handler_id, IR_EVENT_CLICK);
                        }
                    }
                }
                // C event
                else if (strncmp(ir_event->logic_id, "c_", 2) == 0) {
                    kryon_c_event_bridge(ir_event->logic_id);
                }
                // Generic fallback - try executor
                else {
                    IRExecutorContext* executor = ir_executor_get_global();
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
 */
static void handle_text_input(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return;

    // Get the character pressed
    int key = GetCharPressed();
    if (key == 0) return;

    // Find focused input field
    // For now, we'll need to iterate through components to find the focused one
    // This is a simplified version - in production you'd track focused component
    printf("[raylib] Character pressed: %c (%d)\n", (char)key, key);

    // TODO: Append character to focused input field
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

    static int frame_count = 0;
    if (frame_count++ % 60 == 0) {
        printf("[raylib] Event polling active (frame %d)\n", frame_count);
    }

    // Check for window close
    if (WindowShouldClose()) {
        printf("[raylib] Window close requested\n");
        renderer->running = false;
        return;
    }

    // Handle mouse button clicks
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        printf("[raylib] Mouse button pressed detected\n");
        Vector2 mouse_pos = GetMousePosition();
        handle_mouse_click(renderer, root, mouse_pos.x, mouse_pos.y);
    }

    // Handle text input
    handle_text_input(renderer, root);

    // Handle keyboard special keys
    handle_keyboard_keys(renderer, root);

    // TODO: Handle mouse wheel for scrolling
    // TODO: Handle window resize events
}
