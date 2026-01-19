#ifndef IR_ANDROID_RENDERER_H
#define IR_ANDROID_RENDERER_H

#include <stdint.h>
#include <stdbool.h>
#include "../../ir/include/ir_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct AndroidIRRenderer AndroidIRRenderer;

// ============================================================================
// CONFIGURATION
// ============================================================================

typedef struct {
    int window_width;
    int window_height;
    const char* title;
    bool enable_hot_reload;
    const char* hot_reload_watch_path;
    bool enable_animations;
    float target_fps;
} AndroidIRRendererConfig;

// ============================================================================
// EVENT HANDLING
// ============================================================================

typedef enum {
    ANDROID_EVENT_CLICK,
    ANDROID_EVENT_HOVER_START,
    ANDROID_EVENT_HOVER_END,
    ANDROID_EVENT_TOUCH_DOWN,
    ANDROID_EVENT_TOUCH_UP,
    ANDROID_EVENT_INPUT_CHANGE,
    ANDROID_EVENT_CHECKBOX_TOGGLE,
    ANDROID_EVENT_DROPDOWN_SELECT
} AndroidEventType;

typedef struct {
    AndroidEventType type;
    uint32_t component_id;
    float x, y;
    const char* value;
} AndroidEvent;

typedef void (*AndroidEventCallback)(AndroidEvent* event, void* user_data);

// ============================================================================
// LIFECYCLE
// ============================================================================

// Create renderer with configuration
AndroidIRRenderer* android_ir_renderer_create(const AndroidIRRendererConfig* config);

// Initialize renderer (called after platform/window setup)
bool android_ir_renderer_initialize(AndroidIRRenderer* ir_renderer, void* platform_context);

// Load and set root component from KIR file
bool android_ir_renderer_load_kir(AndroidIRRenderer* ir_renderer, const char* kir_path);

// Set root component from IR tree
void android_ir_renderer_set_root(AndroidIRRenderer* ir_renderer, IRComponent* root);

// Render current frame
void android_ir_renderer_render(AndroidIRRenderer* ir_renderer);

// Process input events
void android_ir_renderer_handle_event(AndroidIRRenderer* ir_renderer, void* event);

// Update animations and timers
void android_ir_renderer_update(AndroidIRRenderer* ir_renderer, float delta_time);

// Cleanup and destroy
void android_ir_renderer_destroy(AndroidIRRenderer* ir_renderer);

// ============================================================================
// EVENT CALLBACKS
// ============================================================================

void android_ir_renderer_set_event_callback(AndroidIRRenderer* ir_renderer,
                                            AndroidEventCallback callback,
                                            void* user_data);

// ============================================================================
// FONT REGISTRATION
// ============================================================================

void android_ir_register_font(const char* name, const char* path);
void android_ir_set_default_font(const char* name);

// ============================================================================
// PERFORMANCE
// ============================================================================

float android_ir_renderer_get_fps(AndroidIRRenderer* ir_renderer);
uint64_t android_ir_renderer_get_frame_count(AndroidIRRenderer* ir_renderer);

#ifdef __cplusplus
}
#endif

#endif // IR_ANDROID_RENDERER_H
