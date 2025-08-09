/**
 * @file renderer_interface.h
 * @brief Kryon Renderer Interface - Common interface for all renderer backends
 * 
 * Based on kryon-rust renderer architecture with command-based rendering
 */

#ifndef KRYON_INTERNAL_RENDERER_INTERFACE_H
#define KRYON_INTERNAL_RENDERER_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "internal/types.h"
#include "internal/events.h"

#include "internal/elements.h"

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonRenderer KryonRenderer;
typedef struct KryonRenderContext KryonRenderContext;
typedef struct KryonElementTree KryonElementTree;

// Event callback function type
typedef void (*KryonEventCallback)(const KryonEvent* event, void* userData);



// =============================================================================
// RENDER COMMANDS (Based on kryon-rust architecture)
// =============================================================================

typedef enum {
    KRYON_CMD_SET_CANVAS_SIZE,
    KRYON_CMD_DRAW_RECT,
    KRYON_CMD_DRAW_TEXT,
    KRYON_CMD_DRAW_IMAGE,
    KRYON_CMD_BEGIN_CONTAINER,
    KRYON_CMD_END_CONTAINER,
    KRYON_CMD_BEGIN_CANVAS,
    KRYON_CMD_BEGIN_CANVAS_3D,
    KRYON_CMD_EMBED_VIEW,
    KRYON_CMD_SET_TRANSFORM,
    KRYON_CMD_PUSH_CLIP,
    KRYON_CMD_POP_CLIP,
    KRYON_CMD_DRAW_LINE,
    KRYON_CMD_DRAW_CIRCLE,
    KRYON_CMD_DRAW_GRADIENT,
    KRYON_CMD_BEGIN_PATH,
    KRYON_CMD_END_PATH,
    // Element commands
    KRYON_CMD_DRAW_BUTTON,
    KRYON_CMD_DRAW_INPUT,
    KRYON_CMD_DRAW_DROPDOWN,
    KRYON_CMD_DRAW_CHECKBOX,
    KRYON_CMD_DRAW_SLIDER,
    KRYON_CMD_UPDATE_ELEMENT_STATE
} KryonRenderCommandType;

typedef struct {
    KryonVec2 position;
    KryonVec2 size;
    KryonColor color;
    float border_radius;
    float border_width;
    KryonColor border_color;
} KryonDrawRectData;

typedef struct {
    KryonVec2 position;
    char* text;
    float font_size;
    KryonColor color;
    char* font_family;
    bool bold;
    bool italic;
    float max_width;
    int text_align; // 0=left, 1=center, 2=right
} KryonDrawTextData;

typedef struct {
    KryonVec2 position;
    KryonVec2 size;
    char* source;
    float opacity;
    bool maintain_aspect;
} KryonDrawImageData;

typedef struct {
    char* element_id;
    KryonVec2 position;
    KryonVec2 size;
    KryonColor background_color;
    float opacity;
    bool clip_children;
} KryonBeginContainerData;

typedef struct {
    char* canvas_id;
    KryonVec2 position;
    KryonVec2 size;
} KryonBeginCanvasData;

typedef struct {
    KryonVec2 canvas_size;
} KryonSetCanvasSizeData;



typedef struct {
    char* element_id;
    KryonVec2 position;
    KryonVec2 size;
    char* text;
    KryonColor background_color;
    KryonColor text_color;
    KryonColor border_color;
    float border_width;
    float border_radius;
    KryonElementState state;
    bool enabled;
    char* onclick_handler;
} KryonDrawButtonData;

typedef struct {
    char* element_id;
    KryonVec2 position;
    KryonVec2 size;
    char* text;
    char* placeholder;
    KryonColor background_color;
    KryonColor text_color;
    KryonColor border_color;
    float border_width;
    float border_radius;
    float font_size;          // Add font size for consistent text measurement
    KryonElementState state;
    bool enabled;
    bool is_password;
    int cursor_position;
    bool has_focus;
} KryonDrawInputData;



typedef struct {
    char* element_id;
    KryonVec2 position;
    KryonVec2 size;
    KryonDropdownItem* items;
    size_t item_count;
    int selected_index;
    int hovered_index;
    bool is_open;
    KryonColor background_color;
    KryonColor text_color;
    KryonColor border_color;
    KryonColor hover_color;
    KryonColor selected_color;
    float border_width;
    float border_radius;
    KryonElementState state;
    bool enabled;
} KryonDrawDropdownData;

typedef struct {
    char* element_id;
    KryonVec2 position;
    KryonVec2 size;
    char* label;
    bool checked;
    KryonColor background_color;
    KryonColor check_color;
    KryonColor border_color;
    float border_width;
    KryonElementState state;
    bool enabled;
} KryonDrawCheckboxData;

typedef struct {
    char* element_id;
    KryonVec2 position;
    KryonVec2 size;
    float min_value;
    float max_value;
    float current_value;
    KryonColor track_color;
    KryonColor thumb_color;
    KryonColor border_color;
    float border_width;
    KryonElementState state;
    bool enabled;
    bool is_dragging;
} KryonDrawSliderData;

typedef struct {
    char* element_id;
    KryonElementState new_state;
    union {
        struct {
            char* new_text;
            int cursor_pos;
        } input_data;
        struct {
            int selected_index;
            bool is_open;
        } dropdown_data;
        struct {
            bool checked;
        } checkbox_data;
        struct {
            float value;
        } slider_data;
    } state_data;
} KryonUpdateElementStateData;

typedef struct {
    KryonRenderCommandType type;
    int z_index;
    union {
        KryonSetCanvasSizeData set_canvas_size;
        KryonDrawRectData draw_rect;
        KryonDrawTextData draw_text;
        KryonDrawImageData draw_image;
        KryonBeginContainerData begin_container;
        KryonBeginCanvasData begin_canvas;
        // Element commands
        KryonDrawButtonData draw_button;
        KryonDrawInputData draw_input;
        KryonDrawDropdownData draw_dropdown;
        KryonDrawCheckboxData draw_checkbox;
        KryonDrawSliderData draw_slider;
        KryonUpdateElementStateData update_element_state;
    } data;
} KryonRenderCommand;

// =============================================================================
// RENDERER RESULT TYPES
// =============================================================================

typedef enum {
    KRYON_RENDER_SUCCESS = 0,
    KRYON_RENDER_ERROR_INVALID_PARAM,
    KRYON_RENDER_ERROR_OUT_OF_MEMORY,
    KRYON_RENDER_ERROR_BACKEND_INIT_FAILED,
    KRYON_RENDER_ERROR_SURFACE_LOST,
    KRYON_RENDER_ERROR_COMMAND_FAILED,
    KRYON_RENDER_ERROR_UNSUPPORTED_OPERATION,
    KRYON_RENDER_ERROR_WINDOW_CLOSED
} KryonRenderResult;

// =============================================================================
// INPUT HANDLING
// =============================================================================

typedef enum {
    KRYON_KEY_UNKNOWN = 0,
    KRYON_KEY_SPACE = 32,
    KRYON_KEY_ENTER = 257,
    KRYON_KEY_TAB = 258,
    KRYON_KEY_BACKSPACE = 259,
    KRYON_KEY_DELETE = 261,
    KRYON_KEY_RIGHT = 262,
    KRYON_KEY_LEFT = 263,
    KRYON_KEY_DOWN = 264,
    KRYON_KEY_UP = 265,
    KRYON_KEY_ESCAPE = 256,
    KRYON_KEY_A = 65,
    KRYON_KEY_C = 67,
    KRYON_KEY_V = 86,
    KRYON_KEY_X = 88,
    KRYON_KEY_Z = 90
} KryonKey;

typedef enum {
    KRYON_MOUSE_LEFT = 0,
    KRYON_MOUSE_RIGHT = 1,
    KRYON_MOUSE_MIDDLE = 2
} KryonMouseButton;

typedef struct {
    KryonVec2 position;
    KryonVec2 delta;
    float wheel;
    bool left_pressed;
    bool right_pressed;
    bool middle_pressed;
    bool left_released;
    bool right_released;
    bool middle_released;
} KryonMouseState;

typedef struct {
    bool keys_pressed[512];
    bool keys_released[512]; 
    bool keys_down[512];
    char text_input[32];
    size_t text_input_length;
} KryonKeyboardState;

typedef struct {
    KryonMouseState mouse;
    KryonKeyboardState keyboard;
} KryonInputState;

// =============================================================================
// RENDERER CONFIGURATION
// =============================================================================

typedef struct {
    KryonEventCallback event_callback;
    void* callback_data;
    void* platform_context;  // Window handle, etc.
} KryonRendererConfig;

// =============================================================================
// RENDERER INTERFACE (Based on kryon-rust Renderer trait)
// =============================================================================

typedef struct {
    /**
     * Initialize the renderer with a surface
     * @param surface Platform-specific surface data
     * @return KRYON_RENDER_SUCCESS on success
     */
    KryonRenderResult (*initialize)(void* surface);
    
    /**
     * Begin a new frame
     * @param context Output render context
     * @param clear_color Background color
     * @return KRYON_RENDER_SUCCESS on success
     */
    KryonRenderResult (*begin_frame)(KryonRenderContext** context, KryonColor clear_color);
    
    /**
     * End the current frame and present
     * @param context Render context from begin_frame
     * @return KRYON_RENDER_SUCCESS on success
     */
    KryonRenderResult (*end_frame)(KryonRenderContext* context);
    
    /**
     * Execute a batch of render commands
     * @param context Active render context
     * @param commands Array of render commands
     * @param command_count Number of commands
     * @return KRYON_RENDER_SUCCESS on success
     */
    KryonRenderResult (*execute_commands)(KryonRenderContext* context, 
                                         const KryonRenderCommand* commands, 
                                         size_t command_count);
    
    /**
     * Resize the renderer
     * @param new_size New viewport size
     * @return KRYON_RENDER_SUCCESS on success
     */
    KryonRenderResult (*resize)(KryonVec2 new_size);
    
    /**
     * Get current viewport size
     * @return Current viewport size
     */
    KryonVec2 (*viewport_size)(void);
    
    /**
     * Clean up and destroy the renderer
     */
    void (*destroy)(void);
    
    
    /**
     * Check if a point is inside a element
     * @param point Point to check
     * @param element_bounds Element bounds
     * @return true if point is inside element
     */
    bool (*point_in_element)(KryonVec2 point, KryonRect element_bounds);
    
    /**
     * Handle platform events
     * @param event Platform event
     * @return true if event was handled
     */
    bool (*handle_event)(const KryonEvent* event);
    
    /**
     * Measure text width with current font
     * @param text Text to measure
     * @param font_size Font size (ignored if renderer uses fixed size)
     * @return Text width in pixels
     */
    float (*measure_text_width)(const char* text, float font_size);
    
    /**
     * Get platform-specific window handle
     * @return Platform window handle or NULL
     */
    void* (*get_native_window)(void);
    
} KryonRendererVTable;

// =============================================================================
// RENDERER IMPLEMENTATION
// =============================================================================

struct KryonRenderer {
    KryonRendererVTable* vtable;
    void* impl_data;  // Backend-specific implementation data
    char* name;       // Human-readable name
    char* backend;    // Backend identifier (sdl2, raylib, html)
};

// =============================================================================
// RENDERER FACTORY FUNCTIONS
// =============================================================================

/**
 * Create SDL2 renderer
 * @param config Renderer configuration with event callback
 * @return Renderer instance or NULL on failure
 */
KryonRenderer* kryon_sdl2_renderer_create(const KryonRendererConfig* config);

/**
 * Create Raylib renderer
 * @param config Renderer configuration with event callback  
 * @return Renderer instance or NULL on failure
 */
KryonRenderer* kryon_raylib_renderer_create(const KryonRendererConfig* config);

/**
 * Create HTML renderer
 * @param config Renderer configuration with event callback
 * @return Renderer instance or NULL on failure
 */
KryonRenderer* kryon_html_renderer_create(const KryonRendererConfig* config);

// =============================================================================
// EVENT CALLBACK HELPER FUNCTIONS
// =============================================================================

/**
 * Set event callback on existing renderer
 * @param renderer The renderer instance
 * @param callback The event callback function
 * @param userData User data to pass to callback
 * @return true on success, false on failure
 */
bool kryon_renderer_set_event_callback(KryonRenderer* renderer, 
                                       KryonEventCallback callback, 
                                       void* userData);

/**
 * Push an event from renderer to its callback
 * @param renderer The renderer instance
 * @param event The event to push
 */
void kryon_renderer_push_event(KryonRenderer* renderer, const KryonEvent* event);

// =============================================================================
// CONVENIENCE FUNCTIONS
// =============================================================================

/**
 * Execute render commands on a renderer
 */
static inline KryonRenderResult kryon_renderer_execute_commands(
    KryonRenderer* renderer, 
    KryonRenderContext* context,
    const KryonRenderCommand* commands, 
    size_t count) {
    
    if (!renderer || !renderer->vtable || !renderer->vtable->execute_commands) {
        return KRYON_RENDER_ERROR_INVALID_PARAM;
    }
    
    return renderer->vtable->execute_commands(context, commands, count);
}

/**
 * Begin frame on a renderer
 */
static inline KryonRenderResult kryon_renderer_begin_frame(
    KryonRenderer* renderer,
    KryonRenderContext** context, 
    KryonColor clear_color) {
    
    if (!renderer || !renderer->vtable || !renderer->vtable->begin_frame) {
        return KRYON_RENDER_ERROR_INVALID_PARAM;
    }
    
    return renderer->vtable->begin_frame(context, clear_color);
}

/**
 * End frame on a renderer
 */
static inline KryonRenderResult kryon_renderer_end_frame(
    KryonRenderer* renderer,
    KryonRenderContext* context) {
    
    if (!renderer || !renderer->vtable || !renderer->vtable->end_frame) {
        return KRYON_RENDER_ERROR_INVALID_PARAM;
    }
    
    return renderer->vtable->end_frame(context);
}

/**
 * Destroy a renderer
 */
static inline void kryon_renderer_destroy(KryonRenderer* renderer) {
    if (renderer) {
        if (renderer->vtable && renderer->vtable->destroy) {
            renderer->vtable->destroy();
        }
        free(renderer->name);
        free(renderer->backend);
        free(renderer);
    }
}

// =============================================================================
// HELPER FUNCTIONS FOR COMMAND CREATION
// =============================================================================

/**
 * Create a draw rect command
 */
static inline KryonRenderCommand kryon_cmd_draw_rect(
    KryonVec2 pos, KryonVec2 size, KryonColor color, float border_radius) {
    
    KryonRenderCommand cmd = {0};
    cmd.type = KRYON_CMD_DRAW_RECT;
    cmd.z_index = 0;
    cmd.data.draw_rect = (KryonDrawRectData){
        .position = pos,
        .size = size,
        .color = color,
        .border_radius = border_radius,
        .border_width = 0,
        .border_color = {0, 0, 0, 0}
    };
    return cmd;
}

/**
 * Create a draw text command
 */
static inline KryonRenderCommand kryon_cmd_draw_text(
    KryonVec2 pos, const char* text, float font_size, KryonColor color) {
    
    KryonRenderCommand cmd = {0};
    cmd.type = KRYON_CMD_DRAW_TEXT;
    cmd.z_index = 0;
    cmd.data.draw_text = (KryonDrawTextData){
        .position = pos,
        .text = strdup(text),
        .font_size = font_size,
        .color = color,
        .font_family = strdup("Arial"),
        .bold = false,
        .italic = false,
        .max_width = 0,
        .text_align = 0
    };
    return cmd;
}

/**
 * Create a draw button command
 */
static inline KryonRenderCommand kryon_cmd_draw_button(
    const char* element_id, KryonVec2 pos, KryonVec2 size, const char* text,
    KryonColor bg_color, KryonColor text_color) {
    
    KryonRenderCommand cmd = {0};
    cmd.type = KRYON_CMD_DRAW_BUTTON;
    cmd.z_index = 0;
    cmd.data.draw_button = (KryonDrawButtonData){
        .element_id = strdup(element_id),
        .position = pos,
        .size = size,
        .text = strdup(text),
        .background_color = bg_color,
        .text_color = text_color,
        .border_color = {0.5f, 0.5f, 0.5f, 1.0f},
        .border_width = 1.0f,
        .border_radius = 4.0f,
        .state = KRYON_ELEMENT_STATE_NORMAL,
        .enabled = true,
        .onclick_handler = NULL
    };
    return cmd;
}

/**
 * Create a draw dropdown command
 */
static inline KryonRenderCommand kryon_cmd_draw_dropdown(
    const char* element_id, KryonVec2 pos, KryonVec2 size,
    KryonDropdownItem* items, size_t item_count, int selected_index) {
    
    KryonRenderCommand cmd = {0};
    cmd.type = KRYON_CMD_DRAW_DROPDOWN;
    cmd.z_index = 0;
    cmd.data.draw_dropdown = (KryonDrawDropdownData){
        .element_id = strdup(element_id),
        .position = pos,
        .size = size,
        .items = items,
        .item_count = item_count,
        .selected_index = selected_index,
        .hovered_index = -1,
        .is_open = false,
        .background_color = {0.9f, 0.9f, 0.9f, 1.0f},
        .text_color = {0.0f, 0.0f, 0.0f, 1.0f},
        .border_color = {0.5f, 0.5f, 0.5f, 1.0f},
        .hover_color = {0.8f, 0.9f, 1.0f, 1.0f},
        .selected_color = {0.7f, 0.8f, 1.0f, 1.0f},
        .border_width = 1.0f,
        .border_radius = 4.0f,
        .state = KRYON_ELEMENT_STATE_NORMAL,
        .enabled = true
    };
    return cmd;
}

/**
 * Create a draw input command
 */
static inline KryonRenderCommand kryon_cmd_draw_input(
    const char* element_id, KryonVec2 pos, KryonVec2 size, 
    const char* text, const char* placeholder, float font_size) {
    
    KryonRenderCommand cmd = {0};
    cmd.type = KRYON_CMD_DRAW_INPUT;
    cmd.z_index = 0;
    cmd.data.draw_input = (KryonDrawInputData){
        .element_id = strdup(element_id),
        .position = pos,
        .size = size,
        .text = strdup(text ? text : ""),
        .placeholder = strdup(placeholder ? placeholder : ""),
        .background_color = {1.0f, 1.0f, 1.0f, 1.0f},
        .text_color = {0.0f, 0.0f, 0.0f, 1.0f},
        .border_color = {0.5f, 0.5f, 0.5f, 1.0f},
        .border_width = 1.0f,
        .border_radius = 4.0f,
        .font_size = font_size,
        .state = KRYON_ELEMENT_STATE_NORMAL,
        .enabled = true,
        .is_password = false,
        .cursor_position = strlen(text ? text : ""),
        .has_focus = false
    };
    return cmd;
}

/**
 * Create a draw checkbox command
 */
static inline KryonRenderCommand kryon_cmd_draw_checkbox(
    const char* element_id, KryonVec2 pos, KryonVec2 size,
    const char* label, bool checked) {
    
    KryonRenderCommand cmd = {0};
    cmd.type = KRYON_CMD_DRAW_CHECKBOX;
    cmd.z_index = 0;
    cmd.data.draw_checkbox = (KryonDrawCheckboxData){
        .element_id = strdup(element_id),
        .position = pos,
        .size = size,
        .label = strdup(label ? label : ""),
        .checked = checked,
        .background_color = {1.0f, 1.0f, 1.0f, 1.0f},
        .check_color = {0.0f, 0.6f, 0.0f, 1.0f},
        .border_color = {0.5f, 0.5f, 0.5f, 1.0f},
        .border_width = 1.0f,
        .state = KRYON_ELEMENT_STATE_NORMAL,
        .enabled = true
    };
    return cmd;
}

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_RENDERER_INTERFACE_H