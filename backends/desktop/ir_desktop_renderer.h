#ifndef IR_DESKTOP_RENDERER_H
#define IR_DESKTOP_RENDERER_H

#include "../../ir/ir_core.h"
#include <stdbool.h>

// Desktop Rendering Backend Types
typedef enum {
    DESKTOP_BACKEND_SDL3,
    DESKTOP_BACKEND_GLFW,
    DESKTOP_BACKEND_WIN32,
    DESKTOP_BACKEND_COCOA,
    DESKTOP_BACKEND_X11
} DesktopBackendType;

// Desktop Renderer Context (opaque)
typedef struct DesktopIRRenderer DesktopIRRenderer;

// Desktop-specific rendering configuration
typedef struct DesktopRendererConfig {
    DesktopBackendType backend_type;
    int window_width;
    int window_height;
    const char* window_title;
    bool resizable;
    bool fullscreen;
    bool vsync_enabled;
    int target_fps;
} DesktopRendererConfig;

// Desktop IR Renderer Functions
DesktopIRRenderer* desktop_ir_renderer_create(const DesktopRendererConfig* config);
void desktop_ir_renderer_destroy(DesktopIRRenderer* renderer);

// Rendering control
bool desktop_ir_renderer_initialize(DesktopIRRenderer* renderer);
bool desktop_ir_renderer_render_frame(DesktopIRRenderer* renderer, IRComponent* root);
bool desktop_ir_renderer_run_main_loop(DesktopIRRenderer* renderer, IRComponent* root);
void desktop_ir_renderer_stop(DesktopIRRenderer* renderer);

// Event handling
typedef struct DesktopEvent {
    enum {
        DESKTOP_EVENT_QUIT,
        DESKTOP_EVENT_MOUSE_CLICK,
        DESKTOP_EVENT_MOUSE_MOVE,
        DESKTOP_EVENT_KEY_PRESS,
        DESKTOP_EVENT_KEY_RELEASE,
        DESKTOP_EVENT_WINDOW_RESIZE,
        DESKTOP_EVENT_FOCUS_GAINED,
        DESKTOP_EVENT_FOCUS_LOST
    } type;

    union {
        struct {
            int x, y;
            uint32_t button;
        } mouse;
        struct {
            int key_code;
            bool shift, ctrl, alt;
        } keyboard;
        struct {
            int width, height;
        } resize;
    } data;

    uint64_t timestamp;
} DesktopEvent;

// Event callback function type
typedef bool (*DesktopEventCallback)(const DesktopEvent* event, void* user_data);
void desktop_ir_renderer_set_event_callback(DesktopIRRenderer* renderer, DesktopEventCallback callback, void* user_data);

// Resource management
bool desktop_ir_renderer_load_font(DesktopIRRenderer* renderer, const char* font_path, float size);
bool desktop_ir_renderer_load_image(DesktopIRRenderer* renderer, const char* image_path);
void desktop_ir_renderer_clear_resources(DesktopIRRenderer* renderer);

// Resource registration (global, before renderer init)
void desktop_ir_register_font(const char* name, const char* path);
void desktop_ir_set_default_font(const char* name);

// Debug and utilities
bool desktop_ir_renderer_validate_ir(DesktopIRRenderer* renderer, IRComponent* root);
void desktop_ir_renderer_print_tree_info(DesktopIRRenderer* renderer, IRComponent* root);
void desktop_ir_renderer_print_performance_stats(DesktopIRRenderer* renderer);

// Page navigation support
void desktop_renderer_reload_tree(void* renderer_ptr, IRComponent* new_root);
void desktop_init_router(const char* initial_page, const char* ir_dir, IRComponent* root, void* renderer);

// Convenience functions
DesktopRendererConfig desktop_renderer_config_default(void);
DesktopRendererConfig desktop_renderer_config_sdl3(int width, int height, const char* title);
bool desktop_render_ir_component(IRComponent* root, const DesktopRendererConfig* config);

#endif // IR_DESKTOP_RENDERER_H
