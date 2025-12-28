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

// Forward declarations from desktop_input_helpers.c (platform-agnostic)
extern InputRuntimeState* get_input_state(uint32_t id);
extern IRComponent* find_dropdown_menu_at_point(IRComponent* root, float x, float y);
extern void collect_open_dropdowns(IRComponent* component, IRComponent** dropdown_list, int* count, int max_count);

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

    // Find component at click point
    IRComponent* clicked = find_component_at_point(root, x, y);

    if (!clicked) {
        printf("[raylib] Click at (%.0f, %.0f) - no component\n", x, y);
        return;
    }

    printf("[raylib] Clicked component ID=%u type=%d at (%.0f, %.0f)\n",
           clicked->id, clicked->type, x, y);

    // Handle different component types
    switch (clicked->type) {
        case IR_COMPONENT_BUTTON:
            printf("[raylib] Button %u clicked\n", clicked->id);
            // TODO: Trigger button callback
            break;

        case IR_COMPONENT_CHECKBOX:
            printf("[raylib] Checkbox %u clicked\n", clicked->id);
            // TODO: Toggle checkbox state
            break;

        case IR_COMPONENT_DROPDOWN:
            printf("[raylib] Dropdown %u clicked\n", clicked->id);
            // TODO: Toggle dropdown menu
            break;

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

    // Check for window close
    if (WindowShouldClose()) {
        renderer->running = false;
        return;
    }

    // Handle mouse button clicks
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
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
