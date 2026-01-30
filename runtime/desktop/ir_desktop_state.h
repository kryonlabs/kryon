/**
 * Kryon Desktop State Management
 *
 * Per-instance desktop rendering state for multi-instance support.
 * Each instance can have its own window, fonts, input state, etc.
 */

#ifndef IR_DESKTOP_STATE_H
#define IR_DESKTOP_STATE_H

#include "desktop_internal.h"
#include "../../ir/include/ir_instance.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Configuration
// ============================================================================

#define IR_MAX_DESKTOP_STATES 32

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct IRDesktopState IRDesktopState;

// ============================================================================
// Per-Instance Desktop State
// ============================================================================

/**
 * Per-instance desktop rendering state
 *
 * Each instance has its own:
 * - Renderer (if using multiple windows)
 * - Font cache
 * - Text texture cache
 * - Input states
 * - Hover/focus state
 */
typedef struct IRDesktopState {
    uint32_t instance_id;        // Owner instance ID

    // Renderer (optional - can share global renderer)
    struct DesktopIRRenderer* renderer;

    // Window/Render target
    struct {
        void* window;            // Platform window handle (SDL_Window*, etc.)
        void* renderer;           // Platform renderer handle
        int width;
        int height;
        bool owns_window;        // True if we created the window
    } render_target;

    // Font cache (can be shared with global state)
    struct {
#ifdef ENABLE_SDL3
        struct {
            char path[512];
            int size;
            TTF_Font* font;
        } cache[64];
        int count;
        int registry_count;
        RegisteredFont registry[32];
        char default_name[128];
        char default_path[512];
        struct {
            char family[128];
            uint16_t weight;
            bool italic;
            char path[512];
            bool valid;
        } path_cache[128];
        int path_cache_count;
#endif
    } fonts;

    // Input states
    struct {
        InputRuntimeState states[64];
        size_t count;
    } input;

    // Hover/focus state
    struct {
        IRComponent* hovered;
        IRComponent* focused;
    } interaction;

    // Command buffer for rendering
    struct IRCommandBuffer* commands;

    // Frame tracking
    uint64_t frame_count;

    // Initialized flag
    bool initialized;

} IRDesktopState;

// ============================================================================
// Desktop State API
// ============================================================================

/**
 * Create a new desktop state for an instance
 * @param instance_id Instance ID
 * @return New state, or NULL on failure
 */
IRDesktopState* ir_desktop_state_create(uint32_t instance_id);

/**
 * Destroy a desktop state
 * @param state State to destroy (can be NULL)
 */
void ir_desktop_state_destroy(IRDesktopState* state);

/**
 * Get the current thread's desktop state
 * @return Current state, or NULL if not set
 */
IRDesktopState* ir_desktop_get_current_state(void);

/**
 * Set the current thread's desktop state
 * @param state State to set as current (NULL for global)
 * @return Previous state (for restoring)
 */
IRDesktopState* ir_desktop_set_current_state(IRDesktopState* state);

/**
 * Get desktop state by instance ID
 * @param instance_id Instance ID
 * @return State, or NULL if not found
 */
IRDesktopState* ir_desktop_get_state_by_instance(uint32_t instance_id);

/**
 * Initialize desktop state for an instance
 * Creates window and renderer if needed
 * @param state State to initialize
 * @param width Window width (0 to use default)
 * @param height Window height (0 to use default)
 * @param title Window title
 * @return true on success
 */
bool ir_desktop_state_initialize(IRDesktopState* state, int width, int height, const char* title);

/**
 * Render a frame for a specific instance
 * @param state Desktop state
 * @param root Root component to render
 * @return true on success
 */
bool ir_desktop_state_render(IRDesktopState* state, IRComponent* root);

/**
 * Handle input for a specific instance
 * @param state Desktop state
 * @param timeout_ms Event timeout (0 for non-blocking)
 * @return true if should continue running
 */
bool ir_desktop_state_handle_input(IRDesktopState* state, int timeout_ms);

// ============================================================================
// Render Target Binding
// ============================================================================

/**
 * Bind instance to a render target
 * @param state Desktop state
 * @param window Window handle
 * @param renderer Renderer handle
 * @param owns_window True if instance should destroy window on cleanup
 */
void ir_desktop_bind_render_target(IRDesktopState* state, void* window, void* renderer, bool owns_window);

/**
 * Unbind instance from render target
 * @param state Desktop state
 */
void ir_desktop_unbind_render_target(IRDesktopState* state);

/**
 * Get render target for an instance
 * @param state Desktop state
 * @return Window handle, or NULL
 */
void* ir_desktop_get_render_target_window(IRDesktopState* state);

// ============================================================================
// Input Routing
// ============================================================================

/**
 * Route input event to correct instance based on window/position
 * @param x Mouse X position
 * @param y Mouse Y position
 * @param window Window handle (can be NULL)
 * @return Instance that should receive the input, or NULL
 */
IRInstanceContext* ir_desktop_route_input(float x, float y, void* window);

/**
 * Set focused instance for keyboard input
 * @param inst Instance to focus
 */
void ir_desktop_set_focus_instance(IRInstanceContext* inst);

/**
 * Get focused instance for keyboard input
 * @return Focused instance, or NULL
 */
IRInstanceContext* ir_desktop_get_focus_instance(void);

// ============================================================================
// Shared Resource Management
// ============================================================================

/**
 * Check if fonts should be shared across instances
 * By default, fonts are shared to save memory
 * @return true if fonts are shared
 */
bool ir_desktop_fonts_shared(void);

/**
 * Set whether fonts should be shared across instances
 * @param shared True to share fonts (default), false for per-instance fonts
 */
void ir_desktop_set_fonts_shared(bool shared);

// ============================================================================
// Debug/Statistics
// ============================================================================

/**
 * Print desktop state info for debugging
 * @param state State to print
 */
void ir_desktop_state_print(IRDesktopState* state);

/**
 * Get desktop state statistics
 */
typedef struct {
    uint32_t font_cache_count;
    uint32_t input_state_count;
    uint32_t frame_count;
    bool has_window;
    bool has_renderer;
} IRDesktopStats;

void ir_desktop_get_stats(IRDesktopState* state, IRDesktopStats* stats);

#ifdef __cplusplus
}
#endif

#endif // IR_DESKTOP_STATE_H
