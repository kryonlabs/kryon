/**
 * Kryon Desktop State Management Implementation
 *
 * Per-instance desktop rendering state for multi-instance support.
 */

// _POSIX_C_SOURCE defined by included headers (200809L from ir_transition.h)

#include "ir_desktop_state.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Global State Management
// ============================================================================

// Thread-local current desktop state (for instance-specific operations)
static __thread IRDesktopState* t_current_state = NULL;

// Per-instance state tracking (for lookup by instance_id)
static IRDesktopState* g_desktop_states[IR_MAX_DESKTOP_STATES] = {0};
static uint32_t g_desktop_state_count = 0;

// Focused instance for keyboard input
static IRInstanceContext* g_focused_instance = NULL;

// Configuration
static bool g_fonts_shared = true;

// ============================================================================
// Desktop State Lifecycle
// ============================================================================

IRDesktopState* ir_desktop_state_create(uint32_t instance_id) {
    // Check if state already exists for this instance
    for (uint32_t i = 0; i < g_desktop_state_count; i++) {
        if (g_desktop_states[i] && g_desktop_states[i]->instance_id == instance_id) {
            return g_desktop_states[i];
        }
    }

    // Check capacity
    if (g_desktop_state_count >= IR_MAX_DESKTOP_STATES) {
        fprintf(stderr, "[Desktop] Maximum desktop states reached (%d)\n", IR_MAX_DESKTOP_STATES);
        return NULL;
    }

    // Allocate new state
    IRDesktopState* state = calloc(1, sizeof(IRDesktopState));
    if (!state) {
        fprintf(stderr, "[Desktop] Failed to allocate desktop state\n");
        return NULL;
    }

    state->instance_id = instance_id;
    state->initialized = false;
    state->frame_count = 0;

    // Register the state
    g_desktop_states[g_desktop_state_count++] = state;

    return state;
}

void ir_desktop_state_destroy(IRDesktopState* state) {
    if (!state) return;

    // Destroy render target if we own it
    if (state->render_target.owns_window) {
#ifdef ENABLE_SDL3
        if (state->render_target.renderer) {
            SDL_DestroyRenderer((SDL_Renderer*)state->render_target.renderer);
        }
        if (state->render_target.window) {
            SDL_DestroyWindow((SDL_Window*)state->render_target.window);
        }
#endif
    }

    // Free command buffer if allocated
    if (state->commands) {
        // Command buffer cleanup would go here
        // For now, we don't own it
    }

    // Remove from state list
    for (uint32_t i = 0; i < g_desktop_state_count; i++) {
        if (g_desktop_states[i] == state) {
            // Shift remaining entries down
            for (uint32_t j = i; j < g_desktop_state_count - 1; j++) {
                g_desktop_states[j] = g_desktop_states[j + 1];
            }
            g_desktop_states[g_desktop_state_count - 1] = NULL;
            g_desktop_state_count--;
            break;
        }
    }

    free(state);
}

IRDesktopState* ir_desktop_get_current_state(void) {
    return t_current_state;
}

IRDesktopState* ir_desktop_set_current_state(IRDesktopState* state) {
    IRDesktopState* prev = t_current_state;
    t_current_state = state;
    return prev;
}

IRDesktopState* ir_desktop_get_state_by_instance(uint32_t instance_id) {
    for (uint32_t i = 0; i < g_desktop_state_count; i++) {
        if (g_desktop_states[i] && g_desktop_states[i]->instance_id == instance_id) {
            return g_desktop_states[i];
        }
    }
    return NULL;
}

// ============================================================================
// Desktop State Initialization
// ============================================================================

bool ir_desktop_state_initialize(IRDesktopState* state, int width, int height, const char* title) {
    if (!state) return false;

    if (state->initialized) {
        return true;  // Already initialized
    }

    // If width/height not specified, use defaults
    if (width <= 0) width = 800;
    if (height <= 0) height = 600;
    if (!title) title = "Kryon Application";

#ifdef ENABLE_SDL3
    // Create window
    SDL_Window* window = SDL_CreateWindow(
        title,
        width, height,
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        fprintf(stderr, "[Desktop] Failed to create window: %s\n", SDL_GetError());
        return false;
    }

    // Create renderer (SDL3 simplified API - no flags parameter)
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);

    if (!renderer) {
        fprintf(stderr, "[Desktop] Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return false;
    }

    // Bind to state
    state->render_target.window = window;
    state->render_target.renderer = renderer;
    state->render_target.width = width;
    state->render_target.height = height;
    state->render_target.owns_window = true;

    state->initialized = true;
    return true;

#else
    // For non-SDL3 backends, just mark as initialized
    // The actual renderer will be set up elsewhere
    state->initialized = true;
    return true;
#endif
}

bool ir_desktop_state_render(IRDesktopState* state, IRComponent* root) {
    if (!state || !root) return false;

    // Set current state
    IRDesktopState* prev = ir_desktop_set_current_state(state);

    // Render would happen here using the instance's renderer
    // For now, delegate to global renderer
    // TODO: Implement proper per-instance rendering

    // Increment frame counter
    state->frame_count++;

    // Restore previous state
    ir_desktop_set_current_state(prev);

    return true;
}

bool ir_desktop_state_handle_input(IRDesktopState* state, int timeout_ms) {
    if (!state) return false;

    // Set current state
    IRDesktopState* prev = ir_desktop_set_current_state(state);

    // Handle input - would process events for this instance's window
    // TODO: Implement proper per-instance input handling
    (void)timeout_ms;

    // Restore previous state
    ir_desktop_set_current_state(prev);

    return true;
}

// ============================================================================
// Render Target Binding
// ============================================================================

void ir_desktop_bind_render_target(IRDesktopState* state, void* window, void* renderer, bool owns_window) {
    if (!state) return;

    state->render_target.window = window;
    state->render_target.renderer = renderer;
    state->render_target.owns_window = owns_window;

#ifdef ENABLE_SDL3
    if (window) {
        int w, h;
        SDL_GetWindowSize((SDL_Window*)window, &w, &h);
        state->render_target.width = w;
        state->render_target.height = h;
    }
#endif
}

void ir_desktop_unbind_render_target(IRDesktopState* state) {
    if (!state) return;

    // Only destroy if we own it
    if (state->render_target.owns_window) {
#ifdef ENABLE_SDL3
        if (state->render_target.renderer) {
            SDL_DestroyRenderer((SDL_Renderer*)state->render_target.renderer);
        }
        if (state->render_target.window) {
            SDL_DestroyWindow((SDL_Window*)state->render_target.window);
        }
#endif
    }

    state->render_target.window = NULL;
    state->render_target.renderer = NULL;
    state->render_target.owns_window = false;
}

void* ir_desktop_get_render_target_window(IRDesktopState* state) {
    return state ? state->render_target.window : NULL;
}

// ============================================================================
// Input Routing
// ============================================================================

IRInstanceContext* ir_desktop_route_input(float x, float y, void* window) {
    (void)x;  // Unused parameters - reserved for future hit testing
    (void)y;
    // Find instance with matching window
    for (uint32_t i = 0; i < g_desktop_state_count; i++) {
        IRDesktopState* state = g_desktop_states[i];
        if (state && state->render_target.window == window) {
            // Found matching window, get instance
            // TODO: This needs to access the instance registry
            // For now, return NULL
            return NULL;
        }
    }

    // No matching window, route to focused instance
    return g_focused_instance;
}

void ir_desktop_set_focus_instance(IRInstanceContext* inst) {
    g_focused_instance = inst;
}

IRInstanceContext* ir_desktop_get_focus_instance(void) {
    return g_focused_instance;
}

// ============================================================================
// Shared Resource Management
// ============================================================================

bool ir_desktop_fonts_shared(void) {
    return g_fonts_shared;
}

void ir_desktop_set_fonts_shared(bool shared) {
    g_fonts_shared = shared;
}

// ============================================================================
// Debug/Statistics
// ============================================================================

void ir_desktop_state_print(IRDesktopState* state) {
    if (!state) {
        printf("Desktop State: NULL\n");
        return;
    }

    printf("--- Desktop State %u ---\n", state->instance_id);
    printf("  Initialized: %s\n", state->initialized ? "yes" : "no");
    printf("  Has Window: %s\n", state->render_target.window ? "yes" : "no");
    printf("  Frame Count: %lu\n", (unsigned long)state->frame_count);
    printf("  Input States: %zu\n", state->input.count);
    printf("-----------------------\n");
}

void ir_desktop_get_stats(IRDesktopState* state, IRDesktopStats* stats) {
    if (!state || !stats) return;

    memset(stats, 0, sizeof(IRDesktopStats));

#ifdef ENABLE_SDL3
    stats->font_cache_count = state->fonts.count;
#endif
    stats->input_state_count = (uint32_t)state->input.count;
    stats->frame_count = (uint32_t)state->frame_count;
    stats->has_window = state->render_target.window != NULL;
    stats->has_renderer = state->render_target.renderer != NULL;
}
