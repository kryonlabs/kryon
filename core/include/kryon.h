#ifndef KRYON_H
#define KRYON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// Memory Constraints Platform Configuration
// ============================================================================

// Platform constants - only define if not already defined
#ifndef KRYON_PLATFORM_DESKTOP
#define KRYON_PLATFORM_DESKTOP  0
#define KRYON_PLATFORM_MCU      1
#define KRYON_PLATFORM_WEB      2
#endif

// Target platform - only set if not already set by build system
#ifndef KRYON_TARGET_PLATFORM
#define KRYON_TARGET_PLATFORM KRYON_PLATFORM_DESKTOP
#endif

// Maximum text length for text inputs
#define KRYON_MAX_TEXT_LENGTH 256

// Command buffer size - DEFAULT TO LARGE for desktop builds
// Only override to small if explicitly building for MCU
#ifndef KRYON_CMD_BUF_SIZE
#define KRYON_CMD_BUF_SIZE      131072   // Default: 128KB buffer for desktop
#endif

// Memory constraint configuration
#if KRYON_TARGET_PLATFORM == KRYON_PLATFORM_MCU
#ifndef KRYON_NO_HEAP
#define KRYON_NO_HEAP           1       // FORBIDDEN heap allocations
#endif
#ifndef KRYON_NO_FLOAT
#define KRYON_NO_FLOAT          1       // FORBIDDEN floating point
#endif
#define KRYON_MAX_COMPONENTS    64      // Maximum component count
#undef KRYON_CMD_BUF_SIZE
#define KRYON_CMD_BUF_SIZE      2048    // Override to 2KB for MCU
#else
#ifndef KRYON_NO_HEAP
#define KRYON_NO_HEAP           0       // Allow heap on desktop
#endif
#ifndef KRYON_NO_FLOAT
#define KRYON_NO_FLOAT          0       // Allow floating point
#endif
#define KRYON_MAX_COMPONENTS    1024    // Higher limit for desktop
// Keep the default 128KB buffer size for desktop - DO NOT OVERRIDE
#endif


// ============================================================================
// Fixed-Point Arithmetic (16.16 format) - Required for MCUs
// ============================================================================

#if KRYON_NO_FLOAT
typedef int32_t kryon_fp_t;
#define KRYON_FP_16_16(value)   ((kryon_fp_t)((value) * 65536.0f))
#define KRYON_FP_TO_INT(value)  ((value) >> 16)
#define KRYON_FP_TO_FLOAT(value) ((float)((value) >> 16))
#define KRYON_FP_FROM_INT(value) ((value) << 16)
#define KRYON_FP_MUL(a, b)      (((int64_t)(a) * (b)) >> 16)
#define KRYON_FP_DIV(a, b)      (((int64_t)(a) << 16) / (b))
#else
typedef float kryon_fp_t;
#define KRYON_FP_16_16(value)   ((kryon_fp_t)(value))
#define KRYON_FP_TO_INT(value)  ((int32_t)(value))
#define KRYON_FP_TO_FLOAT(value) ((float)(value))
#define KRYON_FP_FROM_INT(value) ((kryon_fp_t)(value))
#define KRYON_FP_MUL(a, b)      ((a) * (b))
#define KRYON_FP_DIV(a, b)      ((a) / (b))
#endif

// ============================================================================
// Unified Event System
// ============================================================================

typedef enum {
    KRYON_EVT_CLICK      = 0,
    KRYON_EVT_TOUCH      = 1,
    KRYON_EVT_KEY        = 2,
    KRYON_EVT_TIMER      = 3,
    KRYON_EVT_HOVER      = 4,
    KRYON_EVT_SCROLL     = 5,
    KRYON_EVT_FOCUS      = 6,
    KRYON_EVT_BLUR       = 7,
    KRYON_EVT_CUSTOM     = 8
} kryon_event_type_t;

typedef struct {
    kryon_event_type_t type;
    int16_t x, y;                          // Coordinates (fixed-point 8.8 for MCUs)
    uint32_t param;                        // Key code, timer ID, etc.
    void* data;                           // Optional event-specific data
    uint32_t timestamp;                   // Event timestamp (ms)
} kryon_event_t;

// ============================================================================
// Component System (VTable Pattern)
// ============================================================================

// Forward declarations (use only when needed to avoid C11 typedef redefinition warnings)
struct kryon_component;
struct kryon_cmd_buf;
struct kryon_renderer;

// Component function table (vtable)
typedef struct kryon_component_ops {
    void (*render)(struct kryon_component* self, struct kryon_cmd_buf* buf);
    bool (*on_event)(struct kryon_component* self, kryon_event_t* evt);
    void (*destroy)(struct kryon_component* self);
    void (*layout)(struct kryon_component* self, kryon_fp_t available_width, kryon_fp_t available_height);
} kryon_component_ops_t;

// Forward declarations for style system
typedef struct kryon_style_rule kryon_style_rule_t;
typedef struct kryon_selector_group kryon_selector_group_t;
typedef struct kryon_style_prop kryon_style_prop_t;

// Alignment options
typedef enum {
    KRYON_ALIGN_START = 0,
    KRYON_ALIGN_CENTER = 1,
    KRYON_ALIGN_END = 2,
    KRYON_ALIGN_STRETCH = 3,
    KRYON_ALIGN_SPACE_EVENLY = 4,
    KRYON_ALIGN_SPACE_AROUND = 5,
    KRYON_ALIGN_SPACE_BETWEEN = 6
} kryon_alignment_t;

// Dimension types for width/height
typedef enum {
    KRYON_DIM_PX = 0,       // Pixels (default)
    KRYON_DIM_PERCENT = 1,  // Percentage of parent (0-100)
    KRYON_DIM_AUTO = 2,     // Auto-size based on content
    KRYON_DIM_FLEX = 3      // Flex units (grow/shrink)
} kryon_dimension_type_t;

typedef enum {
    KRYON_COMPONENT_FLAG_HAS_X      = 1 << 0,
    KRYON_COMPONENT_FLAG_HAS_Y      = 1 << 1,
    KRYON_COMPONENT_FLAG_HAS_WIDTH  = 1 << 2,
    KRYON_COMPONENT_FLAG_HAS_HEIGHT = 1 << 3,
    KRYON_COMPONENT_FLAG_HAS_ABSOLUTE = 1 << 4  // Set when layout system sets absolute coordinates
} kryon_component_flag_t;

// Component state structure
typedef struct kryon_component {
    const kryon_component_ops_t* ops;      // VTable pointer
    void* state;                          // Language-specific state pointer
    struct kryon_component* parent;       // Parent component
    struct kryon_component** children;    // Child components array
    uint8_t child_count;                  // Number of children
    uint8_t child_capacity;               // Capacity of children array

    // Layout properties
    kryon_fp_t x, y;                     // Position
    kryon_fp_t width, height;            // Dimensions
    kryon_fp_t min_width, min_height;    // Minimum dimensions
    kryon_fp_t max_width, max_height;    // Maximum dimensions
    kryon_fp_t explicit_x, explicit_y;   // Declarative offsets for layout

    // Style properties (simplified for core)
    uint32_t background_color;           // RGBA color
    uint32_t text_color;                 // RGBA color
    uint32_t border_color;               // RGBA color
    uint8_t padding_top, padding_right;  // Padding (reduced for memory)
    uint8_t padding_bottom, padding_left;
    uint8_t margin_top, margin_right;    // Margin (reduced for memory)
    uint8_t margin_bottom, margin_left;
    uint8_t border_width;                // Border thickness in pixels

    // Layout properties
    uint8_t flex_grow;                   // Flex grow factor
    uint8_t flex_shrink;                 // Flex shrink factor
    uint8_t align_self;                  // Self alignment
    uint8_t align_items;                 // Cross-axis alignment for children
    uint8_t justify_content;             // Main-axis alignment for children
    uint8_t layout_direction;            // Layout direction: 0=column, 1=row
    uint8_t gap;                         // Gap between children (pixels)
    uint8_t width_type;                  // Dimension type: 0=px, 1=percent, 2=auto, 3=flex
    uint8_t height_type;                 // Dimension type: 0=px, 1=percent, 2=auto, 3=flex
    kryon_fp_t aspect_ratio;             // Width/height ratio (0 = no constraint)
    bool dirty;                          // Needs layout recalculation
    bool visible;                        // Visibility flag
    bool scrollable;                     // Scrollable container flag
    uint16_t z_index;                    // Z-order index (0-65535)
    uint8_t layout_flags;                // Explicit property bitmask

    // Scrolling
    kryon_fp_t scroll_offset_x;          // Horizontal scroll offset
    kryon_fp_t scroll_offset_y;          // Vertical scroll offset
    kryon_fp_t content_width;            // Content width (for scrolling)
    kryon_fp_t content_height;           // Content height (for scrolling)

    // Event handling
    void (*event_handlers[8])(struct kryon_component*, kryon_event_t*);  // Limited handler array
    uint8_t handler_count;
} kryon_component_t;

// ============================================================================
// Command Buffer System (Ring Buffer)
// ============================================================================

// Command types
typedef enum {
    // Core commands (0-99)
    KRYON_CMD_DRAW_RECT      = 0,
    KRYON_CMD_DRAW_TEXT      = 1,
    KRYON_CMD_DRAW_LINE      = 2,
    KRYON_CMD_DRAW_ARC       = 3,
    KRYON_CMD_DRAW_TEXTURE   = 4,
    KRYON_CMD_SET_CLIP       = 5,
    KRYON_CMD_PUSH_CLIP      = 6,
    KRYON_CMD_POP_CLIP       = 7,
    KRYON_CMD_SET_TRANSFORM  = 8,
    KRYON_CMD_PUSH_TRANSFORM = 9,
    KRYON_CMD_POP_TRANSFORM  = 10,
    KRYON_CMD_DRAW_POLYGON   = 11,

    // Advanced rendering commands (12-27)
    KRYON_CMD_DRAW_ROUNDED_RECT   = 12,  // Rounded rectangle with radius
    KRYON_CMD_DRAW_GRADIENT        = 13,  // Linear/radial/conic gradients
    KRYON_CMD_DRAW_SHADOW          = 14,  // Box shadow (before main shape)
    KRYON_CMD_DRAW_TEXT_SHADOW     = 15,  // Text shadow effect
    KRYON_CMD_DRAW_IMAGE           = 16,  // Image with transforms
    KRYON_CMD_SET_OPACITY          = 17,  // Global opacity modifier
    KRYON_CMD_PUSH_OPACITY         = 18,  // Save opacity state
    KRYON_CMD_POP_OPACITY          = 19,  // Restore opacity state
    KRYON_CMD_SET_BLEND_MODE       = 20,  // Blend mode
    KRYON_CMD_BEGIN_PASS           = 21,  // Start deferred render pass
    KRYON_CMD_END_PASS             = 22,  // Execute deferred pass
    KRYON_CMD_DRAW_TEXT_WRAPPED    = 23,  // Text with word wrapping
    KRYON_CMD_DRAW_TEXT_FADE       = 24,  // Text with fade effect
    KRYON_CMD_DRAW_TEXT_STYLED     = 25,  // Rich text with inline styles
    KRYON_CMD_LOAD_FONT            = 26,  // Register font resource
    KRYON_CMD_LOAD_IMAGE           = 27,  // Register image resource

    // Plugin commands (100-255)
    KRYON_CMD_PLUGIN_START   = 100,

    // Canvas plugin commands
    KRYON_CMD_CANVAS_CIRCLE  = 100,
    KRYON_CMD_CANVAS_ELLIPSE = 101,
    KRYON_CMD_CANVAS_ARC     = 102,

    // Tilemap plugin commands
    KRYON_CMD_TILEMAP_CREATE = 103,
    KRYON_CMD_TILEMAP_SET    = 104,
    KRYON_CMD_TILEMAP_RENDER = 105,

    KRYON_CMD_PLUGIN_END     = 255
} kryon_command_type_t;

// Command structures
typedef struct {
    kryon_command_type_t type;
    union {
        struct {
            int16_t x, y;
            uint16_t w, h;
            uint32_t color;
        } draw_rect;
        struct {
            const char* text;
            int16_t x, y;
            uint16_t font_id;
            uint8_t font_size;      // Font size in pixels (0 = use default)
            uint8_t font_weight;    // 0=normal, 1=bold
            uint8_t font_style;     // 0=normal, 1=italic
            uint32_t color;
            uint8_t max_length;
            char text_storage[128]; // Text storage to fix pointer issue
        } draw_text;
        struct {
            int16_t x1, y1;
            int16_t x2, y2;
            uint32_t color;
        } draw_line;
        struct {
            int16_t cx, cy;
            uint16_t radius;
            int16_t start_angle, end_angle;
            uint32_t color;
        } draw_arc;
        struct {
            uint16_t texture_id;
            int16_t x, y;
        } draw_texture;
        struct {
            int16_t x, y;
            uint16_t w, h;
        } set_clip;
        struct {
            // Simple 2D transform: matrix[6] = [a, b, c, d, tx, ty]
            kryon_fp_t matrix[6];
        } set_transform;
        struct {
            const kryon_fp_t* vertices;     // Pointer to vertex array (x,y pairs)
            uint16_t vertex_count;          // Number of vertices
            uint32_t color;                 // Fill color
            bool filled;                    // true = filled, false = outline only
            // Store vertices inline to avoid pointer issues (max 64 vertices = 128 floats)
            kryon_fp_t vertex_storage[128];
        } draw_polygon;

        // Plugin command structures
        struct {
            kryon_fp_t cx, cy;      // Center position
            kryon_fp_t radius;       // Circle radius
            uint32_t color;          // Fill/stroke color
            bool filled;             // true = filled, false = outline
        } canvas_circle;
        struct {
            kryon_fp_t cx, cy;      // Center position
            kryon_fp_t rx, ry;       // X and Y radii
            uint32_t color;          // Fill/stroke color
            bool filled;             // true = filled, false = outline
        } canvas_ellipse;
        struct {
            kryon_fp_t cx, cy;      // Center position
            kryon_fp_t radius;       // Arc radius
            kryon_fp_t start_angle;  // Start angle in radians
            kryon_fp_t end_angle;    // End angle in radians
            uint32_t color;          // Stroke color
        } canvas_arc;

        // Tilemap plugin structures
        struct {
            int32_t x, y;            // Render position in pixels
            uint8_t layer;           // Layer index
            uint16_t width, height;  // Tilemap dimensions in tiles
            uint16_t tile_width, tile_height;  // Tile size in pixels
        } tilemap_render;

        // Advanced rendering command structures
        struct {
            int16_t x, y;
            uint16_t w, h;
            uint16_t radius;         // Corner radius in pixels
            uint32_t color;
        } draw_rounded_rect;
        struct {
            int16_t x, y;
            uint16_t w, h;
            uint8_t gradient_type;   // 0=linear, 1=radial, 2=conic
            int16_t angle;           // For linear gradients (degrees)
            uint8_t stop_count;      // Number of gradient stops (max 8)
            struct {
                float position;      // 0.0 to 1.0
                uint32_t color;
            } stops[8];
        } draw_gradient;
        struct {
            int16_t x, y;
            uint16_t w, h;
            int16_t offset_x, offset_y;
            uint16_t blur_radius;    // For future use
            uint16_t spread_radius;
            uint32_t color;
            bool inset;
        } draw_shadow;
        struct {
            const char* text;
            int16_t x, y;
            int16_t offset_x, offset_y;
            uint16_t blur_radius;    // For future use
            uint32_t color;
            char text_storage[128];  // Inline text storage
        } draw_text_shadow;
        struct {
            uint16_t image_id;
            int16_t x, y;
            uint16_t w, h;           // 0 = use original size
            float opacity;
        } draw_image;
        struct {
            float opacity;           // 0.0 to 1.0
        } set_opacity;
        struct {
            uint8_t blend_mode;      // 0=normal, 1=add, 2=multiply, etc.
        } set_blend_mode;
        struct {
            uint8_t pass_id;         // 0=main, 1=overlay, etc.
        } begin_pass;
        struct {
            const char* text;
            int16_t x, y;
            uint16_t max_width;
            uint16_t font_id;
            uint8_t font_size;
            uint8_t font_weight;
            uint8_t font_style;
            uint32_t color;
            uint16_t line_height;    // 0 = auto
            char text_storage[128];
        } draw_text_wrapped;
        struct {
            const char* text;
            int16_t x, y;
            uint16_t w, h;           // Fade region
            uint16_t font_id;
            uint8_t font_size;
            uint8_t font_weight;
            uint8_t font_style;
            uint32_t color;
            float fade_ratio;        // 0.0 to 1.0 (how much to fade)
            char text_storage[128];
        } draw_text_fade;
        struct {
            uint16_t font_id;
            const char* path;
            uint8_t size;
            uint8_t weight;          // 0=normal, 1=bold
            uint8_t style;           // 0=normal, 1=italic
            char path_storage[256];  // Inline path storage
        } load_font;
        struct {
            uint16_t image_id;
            const char* path;
            char path_storage[256];  // Inline path storage
        } load_image;
    } data;
} kryon_command_t;

// Command buffer (ring buffer)
typedef struct kryon_cmd_buf {
    uint8_t buffer[KRYON_CMD_BUF_SIZE];  // Raw command buffer
    uint16_t head;                       // Write position
    uint16_t tail;                       // Read position
    uint16_t count;                      // Number of commands
    bool overflow;                       // Buffer overflow flag
} kryon_cmd_buf_t;

// ============================================================================
// Renderer Abstraction
// ============================================================================

// Text measurement callback for accurate rich text layout
// Renderer backend provides this to measure text with actual font metrics
typedef void (*kryon_measure_text_callback_t)(
    const char* text,           // Text to measure
    uint16_t font_size,         // Font size in pixels (0 = default)
    uint8_t font_weight,        // KRYON_FONT_WEIGHT_NORMAL or KRYON_FONT_WEIGHT_BOLD
    uint8_t font_style,         // KRYON_FONT_STYLE_NORMAL or KRYON_FONT_STYLE_ITALIC
    uint16_t font_id,           // Font ID (0 = default, 1 = monospace)
    uint16_t* width,            // OUT: measured width in pixels
    uint16_t* height,           // OUT: measured height in pixels
    void* user_data             // Renderer backend data
);

// Renderer backend interface
typedef struct kryon_renderer_ops {
    bool (*init)(struct kryon_renderer* renderer, void* native_window);
    void (*shutdown)(struct kryon_renderer* renderer);
    void (*begin_frame)(struct kryon_renderer* renderer);
    void (*end_frame)(struct kryon_renderer* renderer);
    void (*execute_commands)(struct kryon_renderer* renderer, struct kryon_cmd_buf* buf);
    void (*swap_buffers)(struct kryon_renderer* renderer);
    void (*get_dimensions)(struct kryon_renderer* renderer, uint16_t* width, uint16_t* height);
    void (*set_clear_color)(struct kryon_renderer* renderer, uint32_t color);
} kryon_renderer_ops_t;

// Renderer structure
typedef struct kryon_renderer {
    const kryon_renderer_ops_t* ops;     // Backend operations
    void* backend_data;                  // Backend-specific data
    uint16_t width, height;              // Renderer dimensions
    uint32_t clear_color;                // Background clear color
    bool vsync_enabled;                  // VSync enabled flag

    // Text measurement callback (NULL = use fallback approximation)
    kryon_measure_text_callback_t measure_text;
} kryon_renderer_t;

// ============================================================================
// Core API Functions
// ============================================================================

// Component lifecycle
kryon_component_t* kryon_component_create(const kryon_component_ops_t* ops, void* initial_state);
void kryon_component_destroy(kryon_component_t* component);
bool kryon_component_add_child(kryon_component_t* parent, kryon_component_t* child);
void kryon_component_remove_child(kryon_component_t* parent, kryon_component_t* child);
kryon_component_t* kryon_component_get_parent(kryon_component_t* component);
kryon_component_t* kryon_component_get_child(kryon_component_t* component, uint8_t index);
uint8_t kryon_component_get_child_count(kryon_component_t* component);

// Coordinate transformation
void calculate_absolute_position(const kryon_component_t* component,
                                 kryon_fp_t* abs_x, kryon_fp_t* abs_y);
void kryon_component_get_absolute_bounds(kryon_component_t* component,
                                         kryon_fp_t* abs_x, kryon_fp_t* abs_y,
                                         kryon_fp_t* width, kryon_fp_t* height);

// Event system
void kryon_component_send_event(kryon_component_t* component, kryon_event_t* event);
bool kryon_component_add_event_handler(kryon_component_t* component,
                                      void (*handler)(kryon_component_t*, kryon_event_t*));
bool kryon_component_is_button(kryon_component_t* component);
kryon_component_t* kryon_event_find_target_at_point(kryon_component_t* root, int16_t x, int16_t y);
void kryon_event_dispatch_to_point(kryon_component_t* root, int16_t x, int16_t y, kryon_event_t* event);
bool kryon_event_is_point_in_component(kryon_component_t* component, int16_t x, int16_t y);

// Event utility functions
uint32_t kryon_event_get_key_code(kryon_event_t* event);
bool kryon_event_is_key_pressed(kryon_event_t* event);
void kryon_event_get_scroll_delta(kryon_event_t* event, int16_t* delta_x, int16_t* delta_y);

// Layout system
void kryon_component_set_bounds_mask(kryon_component_t* component, kryon_fp_t x, kryon_fp_t y,
                                     kryon_fp_t width, kryon_fp_t height, uint8_t explicit_mask);
void kryon_component_set_bounds(kryon_component_t* component, kryon_fp_t x, kryon_fp_t y,
                               kryon_fp_t width, kryon_fp_t height);
void kryon_component_set_z_index(kryon_component_t* component, uint16_t z_index);
uint16_t kryon_component_get_z_index(kryon_component_t* component);
uint8_t kryon_component_get_layout_flags(kryon_component_t* component);
void kryon_component_set_padding(kryon_component_t* component, uint8_t top, uint8_t right,
                                uint8_t bottom, uint8_t left);
void kryon_component_set_margin(kryon_component_t* component, uint8_t top, uint8_t right,
                               uint8_t bottom, uint8_t left);
void kryon_component_set_background_color(kryon_component_t* component, uint32_t color);
void kryon_component_set_border_color(kryon_component_t* component, uint32_t color);
void kryon_component_set_border_width(kryon_component_t* component, uint8_t width);
void kryon_component_set_visible(kryon_component_t* component, bool visible);
void kryon_component_set_scrollable(kryon_component_t* component, bool scrollable);
void kryon_component_set_scroll_offset(kryon_component_t* component, kryon_fp_t offset_x, kryon_fp_t offset_y);
void kryon_component_get_scroll_offset(kryon_component_t* component, kryon_fp_t* offset_x, kryon_fp_t* offset_y);
void kryon_component_set_flex(kryon_component_t* component, uint8_t flex_grow, uint8_t flex_shrink);
void kryon_component_mark_dirty(kryon_component_t* component);
void kryon_component_mark_clean(kryon_component_t* component);
bool kryon_component_is_dirty(kryon_component_t* component);
void kryon_component_set_text_color(kryon_component_t* component, uint32_t color);
void kryon_component_set_text(kryon_component_t* component, const char* text);
void kryon_component_set_layout_alignment(kryon_component_t* component,
                                         kryon_alignment_t justify,
                                         kryon_alignment_t align);
void kryon_component_set_layout_direction(kryon_component_t* component, uint8_t direction);
void kryon_component_set_gap(kryon_component_t* component, uint8_t gap);

// Color inheritance helpers
uint32_t kryon_component_get_effective_text_color(kryon_component_t* component);
uint32_t kryon_component_get_effective_background_color(kryon_component_t* component);

// Command buffer operations
void kryon_cmd_buf_init(kryon_cmd_buf_t* buf);
void kryon_cmd_buf_clear(kryon_cmd_buf_t* buf);
bool kryon_cmd_buf_push(kryon_cmd_buf_t* buf, const kryon_command_t* cmd);
bool kryon_cmd_buf_pop(kryon_cmd_buf_t* buf, kryon_command_t* cmd);
uint16_t kryon_cmd_buf_count(kryon_cmd_buf_t* buf);
bool kryon_cmd_buf_is_full(kryon_cmd_buf_t* buf);
bool kryon_cmd_buf_is_empty(kryon_cmd_buf_t* buf);

// Command iterator (internal)
typedef struct {
    kryon_cmd_buf_t* buf;
    uint16_t position;
    uint16_t remaining;
} kryon_cmd_iterator_t;

kryon_cmd_iterator_t kryon_cmd_iter_create(kryon_cmd_buf_t* buf);
bool kryon_cmd_iter_has_next(kryon_cmd_iterator_t* iter);
bool kryon_cmd_iter_next(kryon_cmd_iterator_t* iter, kryon_command_t* cmd);

// Drawing commands (these go into command buffer)
bool kryon_draw_rect(kryon_cmd_buf_t* buf, int16_t x, int16_t y, uint16_t w, uint16_t h, uint32_t color);
bool kryon_draw_text(kryon_cmd_buf_t* buf, const char* text, int16_t x, int16_t y, uint16_t font_id, uint8_t font_size, uint8_t font_weight, uint8_t font_style, uint32_t color);
bool kryon_draw_line(kryon_cmd_buf_t* buf, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color);
bool kryon_draw_arc(kryon_cmd_buf_t* buf, int16_t cx, int16_t cy, uint16_t radius,
                   int16_t start_angle, int16_t end_angle, uint32_t color);
bool kryon_draw_polygon(kryon_cmd_buf_t* buf, const kryon_fp_t* vertices, uint16_t vertex_count,
                       uint32_t color, bool filled);
bool kryon_draw_texture(kryon_cmd_buf_t* buf, uint16_t texture_id, int16_t x, int16_t y);

// Clipping operations
bool kryon_set_clip(kryon_cmd_buf_t* buf, int16_t x, int16_t y, uint16_t w, uint16_t h);
bool kryon_push_clip(kryon_cmd_buf_t* buf);
bool kryon_pop_clip(kryon_cmd_buf_t* buf);

// Transform operations
bool kryon_set_transform(kryon_cmd_buf_t* buf, const kryon_fp_t matrix[6]);
bool kryon_push_transform(kryon_cmd_buf_t* buf);
bool kryon_pop_transform(kryon_cmd_buf_t* buf);

// Command buffer statistics
typedef struct {
    uint16_t total_commands;
    uint16_t draw_rect_count;
    uint16_t draw_text_count;
    uint16_t draw_line_count;
    uint16_t draw_arc_count;
    uint16_t draw_texture_count;
    uint16_t draw_polygon_count;
    uint16_t clip_operations;
    uint16_t transform_operations;
    uint16_t buffer_utilization;  // Percentage of buffer used
    bool overflow_detected;
} kryon_cmd_stats_t;

kryon_cmd_stats_t kryon_cmd_buf_get_stats(kryon_cmd_buf_t* buf);

// Layout engine
void kryon_layout_tree(kryon_component_t* root, kryon_fp_t available_width, kryon_fp_t available_height);
void kryon_layout_component(kryon_component_t* component, kryon_fp_t available_width, kryon_fp_t available_height);
void kryon_layout_apply_column(kryon_component_t* container, kryon_fp_t available_width, kryon_fp_t available_height);

// Renderer management
kryon_renderer_t* kryon_renderer_create(const kryon_renderer_ops_t* ops);
void kryon_renderer_destroy(kryon_renderer_t* renderer);
bool kryon_renderer_init(kryon_renderer_t* renderer, void* native_window);
void kryon_renderer_shutdown(kryon_renderer_t* renderer);

// Global renderer for text measurement (set during initialization)
void kryon_set_global_renderer(kryon_renderer_t* renderer);
kryon_renderer_t* kryon_get_global_renderer(void);

// Main rendering loop
void kryon_render_frame(kryon_renderer_t* renderer, kryon_component_t* root_component);

// Built-in renderer helpers
const kryon_renderer_ops_t* kryon_get_framebuffer_renderer_ops(void);
kryon_renderer_t* kryon_framebuffer_renderer_create(uint16_t width, uint16_t height, uint8_t bytes_per_pixel);

// Command execution (for renderer backends)
typedef struct {
    void (*execute_draw_rect)(int16_t x, int16_t y, uint16_t w, uint16_t h, uint32_t color);
    void (*execute_draw_text)(const char* text, int16_t x, int16_t y, uint16_t font_id, uint32_t color);
    void (*execute_draw_line)(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color);
    void (*execute_draw_arc)(int16_t cx, int16_t cy, uint16_t radius, int16_t start_angle, int16_t end_angle, uint32_t color);
    void (*execute_draw_texture)(uint16_t texture_id, int16_t x, int16_t y);
    void (*execute_set_clip)(int16_t x, int16_t y, uint16_t w, uint16_t h);
    void (*execute_push_clip)(void);
    void (*execute_pop_clip)(void);
    void (*execute_set_transform)(const kryon_fp_t matrix[6]);
    void (*execute_push_transform)(void);
    void (*execute_pop_transform)(void);
} kryon_command_executor_t;

void kryon_execute_commands(kryon_cmd_buf_t* buf, const kryon_command_executor_t* executor);

// Utility functions
uint32_t kryon_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
uint32_t kryon_color_rgb(uint8_t r, uint8_t g, uint8_t b);
void kryon_color_get_components(uint32_t color, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);
uint8_t kryon_color_get_red(uint32_t color);
uint8_t kryon_color_get_green(uint32_t color);
uint8_t kryon_color_get_blue(uint32_t color);
uint8_t kryon_color_get_alpha(uint32_t color);
uint32_t kryon_color_lerp(uint32_t color1, uint32_t color2, kryon_fp_t t);

// Geometry utilities
bool kryon_point_in_rect(int16_t x, int16_t y, int16_t rx, int16_t ry, uint16_t rw, uint16_t rh);
bool kryon_rect_intersect(int16_t x1, int16_t y1, uint16_t w1, uint16_t h1,
                         int16_t x2, int16_t y2, uint16_t w2, uint16_t h2);
void kryon_rect_union(int16_t x1, int16_t y1, uint16_t w1, uint16_t h1,
                     int16_t x2, int16_t y2, uint16_t w2, uint16_t h2,
                     int16_t* ux, int16_t* uy, uint16_t* uw, uint16_t* uh);

// Memory utilities
void* kryon_memcpy(void* dest, const void* src, uint32_t size);
void* kryon_memset(void* ptr, int value, uint32_t size);
int32_t kryon_memcmp(const void* ptr1, const void* ptr2, uint32_t size);

// String utilities
uint32_t kryon_str_len(const char* str);
int32_t kryon_str_cmp(const char* str1, const char* str2);
void kryon_str_copy(char* dest, const char* src, uint32_t max_len);
bool kryon_str_equals(const char* str1, const char* str2);

// Fixed-point utilities (always available)
kryon_fp_t kryon_fp_from_float(float f);
float kryon_fp_to_float(kryon_fp_t fp);
kryon_fp_t kryon_fp_from_int(int32_t i);
int32_t kryon_fp_to_int_round(kryon_fp_t fp);

// Fixed-point math operations
kryon_fp_t kryon_fp_add(kryon_fp_t a, kryon_fp_t b);
kryon_fp_t kryon_fp_sub(kryon_fp_t a, kryon_fp_t b);
kryon_fp_t kryon_fp_mul(kryon_fp_t a, kryon_fp_t b);
kryon_fp_t kryon_fp_div(kryon_fp_t a, kryon_fp_t b);
kryon_fp_t kryon_fp_sqrt(kryon_fp_t value);
kryon_fp_t kryon_fp_abs(kryon_fp_t value);
kryon_fp_t kryon_fp_min(kryon_fp_t a, kryon_fp_t b);
kryon_fp_t kryon_fp_max(kryon_fp_t a, kryon_fp_t b);
kryon_fp_t kryon_fp_clamp(kryon_fp_t value, kryon_fp_t min, kryon_fp_t max);
bool kryon_fp_equal(kryon_fp_t a, kryon_fp_t b, kryon_fp_t epsilon);

// Additional math utilities
kryon_fp_t kryon_lerp(kryon_fp_t a, kryon_fp_t b, kryon_fp_t t);
kryon_fp_t kryon_smoothstep(kryon_fp_t edge0, kryon_fp_t edge1, kryon_fp_t x);

// ============================================================================
// Built-in Component Types (Core components)
// ============================================================================

// Container component
extern const kryon_component_ops_t kryon_container_ops;

// Font weight constants
#define KRYON_FONT_WEIGHT_NORMAL  0
#define KRYON_FONT_WEIGHT_BOLD    1

// Font style constants
#define KRYON_FONT_STYLE_NORMAL   0
#define KRYON_FONT_STYLE_ITALIC   1

// ============================================================================
// Rich Text System - Text Runs and Inline Formatting
// ============================================================================

// Text style flags (bitfield)
typedef enum {
    KRYON_TEXT_STYLE_BOLD          = 1 << 0,
    KRYON_TEXT_STYLE_ITALIC        = 1 << 1,
    KRYON_TEXT_STYLE_UNDERLINE     = 1 << 2,
    KRYON_TEXT_STYLE_STRIKETHROUGH = 1 << 3,
    KRYON_TEXT_STYLE_CODE          = 1 << 4,
    KRYON_TEXT_STYLE_LINK          = 1 << 5,
    KRYON_TEXT_STYLE_SUBSCRIPT     = 1 << 6,
    KRYON_TEXT_STYLE_SUPERSCRIPT   = 1 << 7
} kryon_text_style_flags_t;

// Single text run/span with formatting
typedef struct kryon_text_run {
    const char* text;              // Pointer to text content (in shared buffer)
    uint16_t offset;               // Byte offset in shared text buffer
    uint16_t length;               // Length in bytes (NOT characters - handles UTF-8)

    // Styling
    uint8_t style_flags;           // Bitfield of kryon_text_style_flags_t
    uint32_t fg_color;             // Foreground color (0 = inherit)
    uint32_t bg_color;             // Background color (0 = transparent)

    // Font properties
    uint16_t font_id;              // Font family ID (0 = default)
    uint8_t font_size;             // Font size in pixels (0 = inherit)

    // Link data (if KRYON_TEXT_STYLE_LINK is set)
    const char* link_url;          // URL for clickable links
    uint16_t link_url_offset;      // Offset in URL buffer
    uint16_t link_url_length;      // URL length

    // Layout cache (calculated during layout pass)
    uint16_t measured_width;       // Measured pixel width
    uint16_t measured_height;      // Measured pixel height (line height)
    uint16_t baseline;             // Distance from top to baseline

    struct kryon_text_run* next;   // Linked list for layout
} kryon_text_run_t;

// Represents a single wrapped line of text
typedef struct kryon_text_line {
    kryon_text_run_t** runs;       // Array of pointers to runs in this line
    uint16_t run_count;            // Number of runs in this line
    uint16_t run_capacity;         // Capacity of runs array

    // Measured dimensions
    uint16_t width;                // Total line width in pixels
    uint16_t height;               // Line height (max of run heights)
    uint16_t baseline;             // Distance from top to baseline

    // Word breaking info
    uint16_t word_count;           // Number of words in this line
    bool ends_with_soft_break;     // True if line ends with soft wrap
    bool ends_with_hard_break;     // True if line ends with \n or </p>

    // Justification cache (for JUSTIFY alignment)
    uint16_t space_count;          // Number of spaces in line
    kryon_fp_t extra_space_per_gap;// Extra pixels per word gap (for justify)
} kryon_text_line_t;

// Collection of text runs forming rich text content
typedef struct kryon_rich_text {
    // Memory pools (NO heap allocation during render)
    kryon_text_run_t* runs;        // Array of text runs
    uint16_t run_count;            // Number of runs
    uint16_t run_capacity;         // Capacity of runs array

    // Shared text buffer (all runs point into this)
    char* text_buffer;             // Shared UTF-8 text buffer
    uint16_t text_buffer_size;     // Total buffer size
    uint16_t text_buffer_used;     // Bytes used

    // Shared URL buffer (for links)
    char* url_buffer;              // Shared URL buffer
    uint16_t url_buffer_size;      // Total URL buffer size
    uint16_t url_buffer_used;      // Bytes used in URL buffer

    // Layout cache (calculated once, reused until invalidated)
    kryon_text_line_t* lines;      // Array of wrapped lines
    uint16_t line_count;           // Number of wrapped lines
    uint16_t line_capacity;        // Capacity of lines array
    bool layout_valid;             // Layout cache validity flag

    // Paragraph-level properties
    uint8_t text_align;            // KRYON_ALIGN_START/CENTER/END/JUSTIFY
    uint16_t line_height;          // Line height in pixels (0 = auto = 1.2x font size)
    uint16_t paragraph_spacing;    // Space after paragraph
    uint16_t first_line_indent;    // First line indentation
    kryon_fp_t max_width;          // Maximum width for wrapping (0 = no limit)
} kryon_rich_text_t;

// Text component (now supports rich text)
typedef struct {
    const char* text;              // Simple text (for backward compat)
    uint16_t font_id;
    uint8_t font_size;             // Font size in pixels (0 = use default)
    uint8_t font_weight;           // 0=normal, 1=bold (for simple text)
    uint8_t font_style;            // 0=normal, 1=italic (for simple text)
    uint16_t max_length;
    bool word_wrap;

    // Rich text support
    kryon_rich_text_t* rich_text;  // Rich text content (NULL = simple text)
    bool uses_rich_text;           // True if using rich text mode
} kryon_text_state_t;

extern const kryon_component_ops_t kryon_text_ops;

// ============================================================================
// Inline Text Components - Native styled text elements
// ============================================================================

// Span component state (inline container)
typedef struct {
    uint32_t fg_color;             // Foreground color (0 = inherit)
    uint32_t bg_color;             // Background color (0 = transparent)
    uint8_t font_size;             // Font size override (0 = inherit)
} kryon_span_state_t;

extern const kryon_component_ops_t kryon_span_ops;

// Bold component (no state needed - just sets style flag)
extern const kryon_component_ops_t kryon_bold_ops;

// Italic component (no state needed - just sets style flag)
extern const kryon_component_ops_t kryon_italic_ops;

// Underline component (no state needed - just sets style flag)
extern const kryon_component_ops_t kryon_underline_ops;

// Strikethrough component (no state needed - just sets style flag)
extern const kryon_component_ops_t kryon_strikethrough_ops;

// Code component (inline code span)
typedef struct {
    uint32_t bg_color;             // Background color (default: light gray)
    uint32_t fg_color;             // Foreground color (default: dark gray)
} kryon_code_state_t;

extern const kryon_component_ops_t kryon_code_ops;

// Link component (clickable hyperlink)
typedef struct {
    const char* url;               // Link URL
    bool underline;                // Show underline
    uint32_t color;                // Link color (default: blue)
    void (*on_click)(kryon_component_t*, const char* url, kryon_event_t*);
} kryon_link_state_t;

extern const kryon_component_ops_t kryon_link_ops;

// Highlight component (background highlight)
typedef struct {
    uint32_t bg_color;             // Highlight color (default: yellow)
    uint32_t fg_color;             // Text color (0 = inherit)
} kryon_highlight_state_t;

extern const kryon_component_ops_t kryon_highlight_ops;

// ============================================================================
// Rich Text API
// ============================================================================

// Create and destroy rich text
kryon_rich_text_t* kryon_rich_text_create(uint16_t text_capacity,
                                          uint16_t run_capacity,
                                          uint16_t line_capacity);
void kryon_rich_text_destroy(kryon_rich_text_t* rich_text);

// Add text runs
kryon_text_run_t* kryon_rich_text_add_run(kryon_rich_text_t* rich_text,
                                          const char* text,
                                          uint16_t length);

// Modify run styling
void kryon_text_run_set_bold(kryon_text_run_t* run, bool enable);
void kryon_text_run_set_italic(kryon_text_run_t* run, bool enable);
void kryon_text_run_set_underline(kryon_text_run_t* run, bool enable);
void kryon_text_run_set_strikethrough(kryon_text_run_t* run, bool enable);
void kryon_text_run_set_code(kryon_text_run_t* run, bool enable);
void kryon_text_run_set_color(kryon_text_run_t* run, uint32_t fg, uint32_t bg);
void kryon_text_run_set_font(kryon_text_run_t* run, uint16_t font_id, uint8_t size);
void kryon_text_run_set_link(kryon_text_run_t* run, const char* url);

// Paragraph-level properties
void kryon_rich_text_set_alignment(kryon_rich_text_t* rich_text, uint8_t align);
void kryon_rich_text_set_line_height(kryon_rich_text_t* rich_text, uint16_t height);
void kryon_rich_text_set_first_line_indent(kryon_rich_text_t* rich_text, uint16_t indent);
void kryon_rich_text_set_max_width(kryon_rich_text_t* rich_text, kryon_fp_t max_width);

// Layout invalidation
void kryon_rich_text_invalidate_layout(kryon_rich_text_t* rich_text);

// Flatten component tree to text runs (internal)
void kryon_text_flatten_children_to_runs(kryon_component_t* text_component,
                                         kryon_rich_text_t* rich_text);

// Find run at pixel coordinates (for link clicks)
kryon_text_run_t* kryon_rich_text_find_run_at_point(kryon_rich_text_t* rich_text,
                                                     uint16_t x, uint16_t y,
                                                     uint16_t base_x, uint16_t base_y);

// Handle click event on rich text (for links)
bool kryon_rich_text_handle_click(kryon_component_t* component,
                                   kryon_rich_text_t* rich_text,
                                   kryon_event_t* event);

// Layout and rendering (internal)
void kryon_rich_text_layout_lines(kryon_rich_text_t* rich_text,
                                  uint16_t max_width,
                                  uint8_t default_font_size,
                                  kryon_renderer_t* renderer);
void kryon_rich_text_render(kryon_component_t* component,
                            kryon_rich_text_t* rich_text,
                            kryon_cmd_buf_t* buf);

// Heading components (H1-H6) - Native first-class components
extern const kryon_component_ops_t kryon_h1_ops;
extern const kryon_component_ops_t kryon_h2_ops;
extern const kryon_component_ops_t kryon_h3_ops;
extern const kryon_component_ops_t kryon_h4_ops;
extern const kryon_component_ops_t kryon_h5_ops;
extern const kryon_component_ops_t kryon_h6_ops;

// Button component
typedef struct {
    const char* text;
    uint16_t font_id;
    uint8_t font_size;
    bool pressed;
    bool hovered;
    bool center_text;
    bool ellipsize;
    void (*on_click)(kryon_component_t*, kryon_event_t*);
} kryon_button_state_t;

extern const kryon_component_ops_t kryon_button_ops;
void kryon_button_set_center_text(kryon_component_t* component, bool center);
void kryon_button_set_ellipsize(kryon_component_t* component, bool ellipsize);
void kryon_button_set_font_size(kryon_component_t* component, uint8_t font_size);

// Input component
typedef struct {
    char text[KRYON_MAX_TEXT_LENGTH];
    char placeholder[KRYON_MAX_TEXT_LENGTH];
    uint16_t font_id;
    uint32_t text_color;
    uint32_t placeholder_color;
    uint32_t background_color;
    uint32_t border_color;
    uint8_t border_width;
    uint8_t padding;
    bool focused;
    bool password_mode;
    uint8_t cursor_position;
    float cursor_blink_timer;
    bool cursor_visible;
    void (*on_change)(kryon_component_t* input, const char* text);
    void (*on_submit)(kryon_component_t* input, const char* text);
} kryon_input_state_t;

extern const kryon_component_ops_t kryon_input_ops;

// Checkbox component
typedef struct {
    bool checked;
    uint16_t check_size;
    uint32_t check_color;
    uint32_t box_color;
    uint32_t text_color;
    char label[KRYON_MAX_TEXT_LENGTH];
    uint16_t font_id;
    void (*on_change)(kryon_component_t* checkbox, bool checked);
} kryon_checkbox_state_t;

extern const kryon_component_ops_t kryon_checkbox_ops;

// Spacer component
typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t flex_grow;
} kryon_spacer_state_t;

extern const kryon_component_ops_t kryon_spacer_ops;

// Dropdown component
#define KRYON_MAX_DROPDOWN_OPTIONS 16
typedef struct {
    char placeholder[KRYON_MAX_TEXT_LENGTH];
    const char* options[KRYON_MAX_DROPDOWN_OPTIONS];
    uint8_t option_count;
    int8_t selected_index;
    bool is_open;
    int8_t hovered_index;
    uint16_t font_id;
    uint8_t font_size;
    uint32_t text_color;
    uint32_t background_color;
    uint32_t border_color;
    uint32_t hover_color;
    uint8_t border_width;
    void (*on_change)(kryon_component_t* dropdown, int8_t selected_index);
} kryon_dropdown_state_t;

extern const kryon_component_ops_t kryon_dropdown_ops;

// Component factory functions
kryon_component_t* kryon_input_create(const char* placeholder, const char* initial_text,
                                         uint16_t font_id, bool password_mode);
kryon_component_t* kryon_checkbox_create(const char* label, bool initial_checked,
                                            uint16_t check_size, void (*on_change)(kryon_component_t*, bool));
kryon_component_t* kryon_spacer_create(uint16_t width, uint16_t height, uint8_t flex_grow);

// ============================================================================
// Style System API
// ============================================================================

// Style system initialization
void kryon_style_init(void);
void kryon_style_clear(void);

// Style rule management
uint16_t kryon_style_add_rule(const kryon_selector_group_t* selector, const kryon_style_rule_t* rule);
void kryon_style_remove_rule(uint16_t rule_index);

// Style resolution
void kryon_style_resolve_tree(kryon_component_t* root);
void kryon_style_resolve_component(kryon_component_t* component);

// Style system state
bool kryon_style_is_dirty(void);
void kryon_style_mark_clean(void);

// Style rule creation - types already declared above

kryon_style_rule_t kryon_style_create_rule(void);
void kryon_style_add_property(kryon_style_rule_t* rule, const kryon_style_prop_t* prop);

// Style property creation - functions only, struct is opaque

kryon_style_prop_t kryon_style_color(uint32_t color);
kryon_style_prop_t kryon_style_background_color(uint32_t color);
kryon_style_prop_t kryon_style_width(kryon_fp_t width);
kryon_style_prop_t kryon_style_height(kryon_fp_t height);
kryon_style_prop_t kryon_style_margin(uint8_t margin);
kryon_style_prop_t kryon_style_padding(uint8_t padding);
kryon_style_prop_t kryon_style_visible(bool visible);
kryon_style_prop_t kryon_style_z_index(uint16_t z_index);

// Selector creation
kryon_selector_group_t kryon_style_selector_type(const char* type);
kryon_selector_group_t kryon_style_selector_class(const char* class_name);
kryon_selector_group_t kryon_style_selector_id(const char* id);
kryon_selector_group_t kryon_style_selector_pseudo(const char* pseudo);

// Convenience style functions
uint16_t kryon_style_add_button_style(const char* class_name, uint32_t bg_color, uint32_t text_color,
                                     uint8_t padding, uint16_t width, uint16_t height);
uint16_t kryon_style_add_container_style(const char* class_name, uint32_t bg_color,
                                        uint8_t margin, uint8_t padding);
uint16_t kryon_style_add_text_style(const char* class_name, uint32_t text_color,
                                   uint16_t font_size);

// ============================================================================
// Error Codes
// ============================================================================

typedef enum {
    KRYON_OK               = 0,
    KRYON_ERROR_NULL_PTR   = -1,
    KRYON_ERROR_INVALID_PARAM = -2,
    KRYON_ERROR_OUT_OF_MEMORY = -3,
    KRYON_ERROR_BUFFER_FULL    = -4,
    KRYON_ERROR_INVALID_COMPONENT = -5,
    KRYON_ERROR_RENDERER_INIT   = -6,
    KRYON_ERROR_COMMAND_OVERFLOW = -7
} kryon_error_t;

// ============================================================================
// Platform-specific includes and configurations
// ============================================================================

#ifdef KRYON_PLATFORM_MCU
// MCU-specific configurations - override global setting
#undef KRYON_MAX_TEXT_LENGTH
#define KRYON_MAX_TEXT_LENGTH 64
#define KRYON_FONT_CACHE_SIZE  4
#else
// Desktop-specific configurations - keep global setting
#define KRYON_FONT_CACHE_SIZE  32
#endif

// ============================================================================
// Generic Platform Abstraction APIs
// ============================================================================

// Generic Event System (implemented by platform backends)
bool kryon_poll_event(kryon_event_t* event);
bool kryon_is_mouse_button_down(uint8_t button);
void kryon_get_mouse_position(int16_t* x, int16_t* y);
bool kryon_is_key_down(uint32_t key_code);

// Generic Font Management (implemented by platform backends)
void kryon_add_font_search_path(const char* path);
uint16_t kryon_load_font(const char* name, uint16_t size);
void kryon_get_font_metrics(uint16_t font_id, uint16_t* width, uint16_t* height);

// Generic Renderer Factory (implemented by platform backends)
kryon_renderer_t* kryon_create_renderer(uint16_t width, uint16_t height, const char* title);

// Version information
#define KRYON_VERSION_MAJOR  1
#define KRYON_VERSION_MINOR  0
#define KRYON_VERSION_PATCH  0
#define KRYON_VERSION_STRING "1.0.0"

#ifdef __cplusplus
}
#endif

#endif // KRYON_H
