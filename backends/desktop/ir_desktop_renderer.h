#ifndef IR_DESKTOP_RENDERER_H
#define IR_DESKTOP_RENDERER_H

#include "../../ir/ir_core.h"
#include "desktop_platform.h"
#include <stdbool.h>

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

// Event callback
void desktop_ir_renderer_set_event_callback(DesktopIRRenderer* renderer, DesktopEventCallback callback, void* user_data);

// Lua event callback (for LuaJIT FFI bindings)
void desktop_ir_renderer_set_lua_event_callback(DesktopIRRenderer* renderer,
                                                void (*callback)(uint32_t component_id, int event_type));

// Dynamic root update (for reactive UI rebuilds)
void desktop_ir_renderer_update_root(DesktopIRRenderer* renderer, IRComponent* new_root);

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
