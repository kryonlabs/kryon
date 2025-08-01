/**
 * @file widget_renderer.c
 * @brief Render command generation for Flutter-inspired widget system
 */

#include "kryon/widget_system.h"
#include "internal/memory.h"
#include <math.h>

// =============================================================================
// RENDER COMMAND TYPES (FROM OLD LAYOUT ENGINE)
// =============================================================================

/// Render command types
typedef enum {
    KRYON_RENDER_NONE,
    KRYON_RENDER_RECTANGLE,
    KRYON_RENDER_TEXT,
    KRYON_RENDER_IMAGE,
    KRYON_RENDER_BORDER,
    KRYON_RENDER_CLIP_START,
    KRYON_RENDER_CLIP_END,
    KRYON_RENDER_CUSTOM,
} KryonRenderCommandType;

/// Rectangle render data
typedef struct {
    KryonColor color;
} KryonRenderRectangle;

/// Text render data
typedef struct {
    char* text;
    KryonColor color;
    int font_id;
    float font_size;
} KryonRenderText;

/// Image render data
typedef struct {
    void* image_data;
    KryonColor tint;
} KryonRenderImage;

/// Border render data
typedef struct {
    KryonColor color;
    float width;
    float radius;
} KryonRenderBorder;

/// Custom render data
typedef struct {
    void* user_data;
} KryonRenderCustom;

/// Union of all render data types
typedef union {
    KryonRenderRectangle rectangle;
    KryonRenderText text;
    KryonRenderImage image;
    KryonRenderBorder border;
    KryonRenderCustom custom;
} KryonRenderData;

/// Complete render command
typedef struct {
    KryonRenderCommandType type;
    KryonRect bounding_box;
    KryonRenderData data;
    uint32_t widget_id;
    uint16_t z_index;
} KryonRenderCommand;

/// Dynamic array of render commands
typedef struct {
    KryonRenderCommand* commands;
    int count;
    int capacity;
} KryonRenderCommandArray;

// =============================================================================
// RENDER COMMAND GENERATION
// =============================================================================

/// Initialize render command array
static KryonRenderCommandArray kryon_render_commands_init(int initial_capacity) {
    KryonRenderCommandArray commands = {0};
    commands.commands = kryon_malloc(initial_capacity * sizeof(KryonRenderCommand));
    commands.capacity = initial_capacity;
    commands.count = 0;
    return commands;
}

/// Add render command to array
static void kryon_render_commands_add(KryonRenderCommandArray* commands, KryonRenderCommand command) {
    if (commands->count >= commands->capacity) {
        commands->capacity *= 2;
        commands->commands = kryon_realloc(commands->commands, 
                                          commands->capacity * sizeof(KryonRenderCommand));
    }
    
    commands->commands[commands->count++] = command;
}

/// Free render command array
static void kryon_render_commands_free(KryonRenderCommandArray* commands) {
    // Free any allocated strings in commands
    for (int i = 0; i < commands->count; i++) {
        if (commands->commands[i].type == KRYON_RENDER_TEXT) {
            kryon_free(commands->commands[i].data.text.text);
        }
    }
    
    kryon_free(commands->commands);
    commands->commands = NULL;
    commands->count = 0;
    commands->capacity = 0;
}

/// Generate render commands for a single widget
static void generate_widget_render_commands(KryonWidget* widget, KryonRenderCommandArray* commands, uint16_t z_index) {
    if (!widget || !widget->visible) {
        return;
    }
    
    KryonRect bounds = widget->computed_rect;
    
    // Generate background/container render commands first
    switch (widget->type) {
        case KRYON_WIDGET_CONTAINER: {
            // Render background
            if (widget->props.container_props.background_color.a > 0) {
                KryonRenderCommand bg_cmd = {0};
                bg_cmd.type = KRYON_RENDER_RECTANGLE;
                bg_cmd.bounding_box = bounds;
                bg_cmd.data.rectangle.color = widget->props.container_props.background_color;
                bg_cmd.z_index = z_index;
                
                kryon_render_commands_add(commands, bg_cmd);
            }
            
            // Render border
            if (widget->props.container_props.border.width > 0) {
                KryonRenderCommand border_cmd = {0};
                border_cmd.type = KRYON_RENDER_BORDER;
                border_cmd.bounding_box = bounds;
                border_cmd.data.border.color = widget->props.container_props.border.color;
                border_cmd.data.border.width = widget->props.container_props.border.width;
                border_cmd.data.border.radius = widget->props.container_props.border.radius;
                border_cmd.z_index = z_index;
                
                kryon_render_commands_add(commands, border_cmd);
            }
            break;
        }
        
        case KRYON_WIDGET_BUTTON: {
            // Render button background
            KryonRenderCommand bg_cmd = {0};
            bg_cmd.type = KRYON_RENDER_RECTANGLE;
            bg_cmd.bounding_box = bounds;
            bg_cmd.data.rectangle.color = widget->props.button_props.background_color;
            bg_cmd.z_index = z_index;
            
            kryon_render_commands_add(commands, bg_cmd);
            
            // Render button border
            if (widget->props.button_props.border.width > 0) {
                KryonRenderCommand border_cmd = {0};
                border_cmd.type = KRYON_RENDER_BORDER;
                border_cmd.bounding_box = bounds;
                border_cmd.data.border.color = widget->props.button_props.border.color;
                border_cmd.data.border.width = widget->props.button_props.border.width;
                border_cmd.data.border.radius = widget->props.button_props.border.radius;
                border_cmd.z_index = z_index;
                
                kryon_render_commands_add(commands, border_cmd);
            }
            
            // Render button text
            if (widget->props.button_props.label) {
                KryonRenderCommand text_cmd = {0};
                text_cmd.type = KRYON_RENDER_TEXT;
                text_cmd.bounding_box = bounds;
                text_cmd.data.text.text = kryon_strdup(widget->props.button_props.label);
                text_cmd.data.text.color = widget->props.button_props.text_color;
                text_cmd.data.text.font_id = 0; // Default font
                text_cmd.data.text.font_size = 16.0f; // Default size
                text_cmd.z_index = z_index + 1;
                
                kryon_render_commands_add(commands, text_cmd);
            }
            break;
        }
        
        case KRYON_WIDGET_TEXT: {
            // Render text
            if (widget->props.text_props.text) {
                KryonRenderCommand text_cmd = {0};
                text_cmd.type = KRYON_RENDER_TEXT;
                text_cmd.bounding_box = bounds;
                text_cmd.data.text.text = kryon_strdup(widget->props.text_props.text);
                text_cmd.data.text.color = widget->props.text_props.style.color;
                text_cmd.data.text.font_id = widget->props.text_props.style.font_id;
                text_cmd.data.text.font_size = widget->props.text_props.style.font_size;
                text_cmd.z_index = z_index;
                
                kryon_render_commands_add(commands, text_cmd);
            }
            break;
        }
        
        case KRYON_WIDGET_INPUT: {
            // Render input background
            KryonRenderCommand bg_cmd = {0};
            bg_cmd.type = KRYON_RENDER_RECTANGLE;
            bg_cmd.bounding_box = bounds;
            bg_cmd.data.rectangle.color = (KryonColor){255, 255, 255, 255}; // White background
            bg_cmd.z_index = z_index;
            
            kryon_render_commands_add(commands, bg_cmd);
            
            // Render input border
            if (widget->props.input_props.border.width > 0) {
                KryonRenderCommand border_cmd = {0};
                border_cmd.type = KRYON_RENDER_BORDER;
                border_cmd.bounding_box = bounds;
                border_cmd.data.border.color = widget->props.input_props.border.color;
                border_cmd.data.border.width = widget->props.input_props.border.width;
                border_cmd.data.border.radius = widget->props.input_props.border.radius;
                border_cmd.z_index = z_index;
                
                kryon_render_commands_add(commands, border_cmd);
            }
            
            // Render input text or placeholder
            const char* display_text = widget->props.input_props.value;
            KryonColor text_color = {0, 0, 0, 255}; // Black text
            
            if (!display_text || strlen(display_text) == 0) {
                display_text = widget->props.input_props.placeholder;
                text_color = (KryonColor){108, 117, 125, 255}; // Gray placeholder
            }
            
            if (display_text) {
                KryonRenderCommand text_cmd = {0};
                text_cmd.type = KRYON_RENDER_TEXT;
                
                // Add padding to text position
                KryonRect text_bounds = bounds;
                text_bounds.position.x += 8;
                text_bounds.position.y += 8;
                text_bounds.size.width -= 16;
                text_bounds.size.height -= 16;
                
                text_cmd.bounding_box = text_bounds;
                text_cmd.data.text.text = kryon_strdup(display_text);
                text_cmd.data.text.color = text_color;
                text_cmd.data.text.font_id = 0;
                text_cmd.data.text.font_size = 16.0f;
                text_cmd.z_index = z_index + 1;
                
                kryon_render_commands_add(commands, text_cmd);
            }
            break;
        }
        
        case KRYON_WIDGET_IMAGE: {
            // Render image
            if (widget->props.image_props.image_data) {
                KryonRenderCommand img_cmd = {0};
                img_cmd.type = KRYON_RENDER_IMAGE;
                img_cmd.bounding_box = bounds;
                img_cmd.data.image.image_data = widget->props.image_props.image_data;
                img_cmd.data.image.tint = widget->props.image_props.tint;
                img_cmd.z_index = z_index;
                
                kryon_render_commands_add(commands, img_cmd);
            }
            break;
        }
        
        default:
            // Layout widgets (Column, Row, Stack, etc.) don't render themselves
            // but may have debug visualization in debug mode
            #ifdef KRYON_DEBUG
            if (kryon_widget_is_layout_type(widget->type)) {
                // Render debug outline
                KryonRenderCommand debug_cmd = {0};
                debug_cmd.type = KRYON_RENDER_BORDER;
                debug_cmd.bounding_box = bounds;
                debug_cmd.data.border.color = (KryonColor){255, 0, 0, 64}; // Semi-transparent red
                debug_cmd.data.border.width = 1.0f;
                debug_cmd.data.border.radius = 0.0f;
                debug_cmd.z_index = z_index + 1000; // High z-index for debug
                
                kryon_render_commands_add(commands, debug_cmd);
            }
            #endif
            break;
    }
}

/// Recursively generate render commands for widget tree
static void generate_render_commands_recursive(KryonWidget* widget, KryonRenderCommandArray* commands, uint16_t z_index) {
    if (!widget || !widget->visible) {
        return;
    }
    
    // Generate commands for this widget
    generate_widget_render_commands(widget, commands, z_index);
    
    // Generate commands for children (with higher z-index)
    for (size_t i = 0; i < widget->child_count; i++) {
        uint16_t child_z_index = z_index + 1;
        
        // Stack widgets layer children on top of each other
        if (widget->type == KRYON_WIDGET_STACK) {
            child_z_index = z_index + (uint16_t)(i + 1);
        }
        
        generate_render_commands_recursive(widget->children[i], commands, child_z_index);
    }
}

// =============================================================================
// PUBLIC API
// =============================================================================

/// Generate render commands for widget tree
KryonRenderCommandArray kryon_widget_generate_render_commands(KryonWidget* root) {
    KryonRenderCommandArray commands = kryon_render_commands_init(64);
    
    if (root) {
        generate_render_commands_recursive(root, &commands, 0);
    }
    
    return commands;
}

/// Sort render commands by z-index
void kryon_render_commands_sort_by_z_index(KryonRenderCommandArray* commands) {
    if (!commands || commands->count <= 1) {
        return;
    }
    
    // Simple bubble sort (good enough for most UI rendering)
    for (int i = 0; i < commands->count - 1; i++) {
        for (int j = 0; j < commands->count - i - 1; j++) {
            if (commands->commands[j].z_index > commands->commands[j + 1].z_index) {
                // Swap commands
                KryonRenderCommand temp = commands->commands[j];
                commands->commands[j] = commands->commands[j + 1];
                commands->commands[j + 1] = temp;
            }
        }
    }
}

/// Cull render commands outside viewport
void kryon_render_commands_cull(KryonRenderCommandArray* commands, KryonRect viewport) {
    int write_index = 0;
    
    for (int read_index = 0; read_index < commands->count; read_index++) {
        KryonRenderCommand* cmd = &commands->commands[read_index];
        KryonRect* bbox = &cmd->bounding_box;
        
        // Check if bounding box intersects with viewport
        bool intersects = (bbox->position.x < viewport.position.x + viewport.size.width) &&
                         (bbox->position.x + bbox->size.width > viewport.position.x) &&
                         (bbox->position.y < viewport.position.y + viewport.size.height) &&
                         (bbox->position.y + bbox->size.height > viewport.position.y);
        
        if (intersects) {
            if (write_index != read_index) {
                commands->commands[write_index] = commands->commands[read_index];
            }
            write_index++;
        } else {
            // Free text data for culled commands
            if (cmd->type == KRYON_RENDER_TEXT) {
                kryon_free(cmd->data.text.text);
            }
        }
    }
    
    commands->count = write_index;
}

/// Get render command at index
KryonRenderCommand* kryon_render_commands_get(KryonRenderCommandArray* commands, int index) {
    if (!commands || index < 0 || index >= commands->count) {
        return NULL;
    }
    
    return &commands->commands[index];
}

/// Get number of render commands
int kryon_render_commands_count(KryonRenderCommandArray* commands) {
    return commands ? commands->count : 0;
}

/// Free render command array
void kryon_render_commands_destroy(KryonRenderCommandArray* commands) {
    if (commands) {
        kryon_render_commands_free(commands);
    }
}