#ifndef ANDROID_BACKEND_INTERNAL_H
#define ANDROID_BACKEND_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include "../../core/include/kryon.h"
#include "../../ir/ir_core.h"
#include "../../ir/ir_builder.h"
#include "../../ir/ir_animation.h"
#include "../../ir/ir_hot_reload.h"
#include "../../ir/ir_style_vars.h"
#include "../../ir/ir_serialization.h"
#include "../../renderers/android/android_renderer_internal.h"
#include "../../platforms/android/android_platform.h"
#include "ir_android_renderer.h"

// ============================================================================
// SHARED TYPE DEFINITIONS
// ============================================================================

// Android IR Renderer Context (main renderer structure)
struct AndroidIRRenderer {
    AndroidIRRendererConfig config;
    bool initialized;
    bool running;

    // Low-level renderer and platform
    AndroidRenderer* renderer;
    void* platform_context;  // kryon_android_platform_context

    // Event handling
    AndroidEventCallback event_callback;
    void* event_user_data;

    // Performance tracking
    uint64_t frame_count;
    double last_frame_time;
    double fps;

    // Component rendering cache
    IRComponent* last_root;
    bool needs_relayout;

    // Animation system
    IRAnimationContext* animation_ctx;

    // Hot reload system
    IRHotReloadContext* hot_reload_ctx;
    bool hot_reload_enabled;

    // Input state
    float touch_x;
    float touch_y;
    bool touch_down;
    uint32_t hovered_component_id;
    uint32_t active_component_id;

    // Window dimensions
    int window_width;
    int window_height;
};

// Font registry entry
typedef struct {
    char name[128];
    char path[512];
} RegisteredFont;

// ============================================================================
// GLOBAL STATE
// ============================================================================

extern RegisteredFont g_font_registry[32];
extern int g_font_registry_count;
extern char g_default_font_name[128];
extern char g_default_font_path[512];

// ============================================================================
// INTERNAL FUNCTION DECLARATIONS
// ============================================================================

// Font management
void android_ir_register_font_internal(const char* name, const char* path);

// Component rendering
void render_component_android(AndroidIRRenderer* ir_renderer,
                              IRComponent* component,
                              float parent_x, float parent_y,
                              float parent_opacity);

// Event handling
void android_handle_touch_event(AndroidIRRenderer* ir_renderer,
                                float x, float y, bool down);

// Layout integration
void android_register_text_measurement(void);
void android_layout_set_renderer(AndroidIRRenderer* renderer);

#endif // ANDROID_BACKEND_INTERNAL_H
