#ifndef KRYON_CANVAS_H
#define KRYON_CANVAS_H

#include "kryon.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Enhanced Canvas System - Love2D Inspired API
// ============================================================================

// Drawing modes for shapes
typedef enum {
    KRYON_DRAW_FILL = 0,   // Fill the shape
    KRYON_DRAW_LINE = 1    // Draw outline only
} kryon_draw_mode_t;

// Line styles
typedef enum {
    KRYON_LINE_SOLID = 0,
    KRYON_LINE_ROUGH = 1,
    KRYON_LINE_SMOOTH = 2
} kryon_line_style_t;

// Line join styles
typedef enum {
    KRYON_LINE_JOIN_MITER = 0,
    KRYON_LINE_JOIN_BEVEL = 1,
    KRYON_LINE_JOIN_ROUND = 2
} kryon_line_join_t;

// Blend modes
typedef enum {
    KRYON_BLEND_ALPHA = 0,        // Standard alpha blending
    KRYON_BLEND_ADDITIVE = 1,     // Additive blending
    KRYON_BLEND_MULTIPLY = 2,     // Multiplicative blending
    KRYON_BLEND_SUBTRACT = 3,     // Subtractive blending
    KRYON_BLEND_SCREEN = 4,       // Screen blending
    KRYON_BLEND_REPLACE = 5       // Replace (no blending)
} kryon_blend_mode_t;

// Canvas drawing state (Love2D style global state)
// NOTE: This is separate from kryon_canvas_state_t which is the component state
typedef struct {
    // Canvas dimensions and offset
    uint16_t width;              // Canvas width
    uint16_t height;             // Canvas height
    int16_t offset_x;            // X offset for drawing (canvas position)
    int16_t offset_y;            // Y offset for drawing (canvas position)

    // Drawing colors
    uint32_t color;               // Current drawing color (RGBA)
    uint32_t background_color;    // Background color

    // Line properties
    kryon_fp_t line_width;        // Line width in pixels
    kryon_line_style_t line_style;    // Line style (solid, rough, smooth)
    kryon_line_join_t line_join;      // Line join style for polygons

    // Font properties
    uint16_t font_id;            // Current font ID
    kryon_fp_t font_size;        // Current font size

    // Blend mode
    kryon_blend_mode_t blend_mode;    // Current blend mode

    // State flags for dirty tracking
    struct {
        bool color : 1;
        bool line_width : 1;
        bool font : 1;
        bool blend_mode : 1;
    } dirty;
} kryon_canvas_draw_state_t;

// Global canvas instance (Love2D style)
extern kryon_canvas_draw_state_t* g_canvas;

// ============================================================================
// Canvas Lifecycle and State Management
// ============================================================================

// Initialize canvas system
void kryon_canvas_init(uint16_t width, uint16_t height);

// Shutdown canvas system
void kryon_canvas_shutdown(void);

// Resize canvas
void kryon_canvas_resize(uint16_t width, uint16_t height);

// Get current canvas drawing state
kryon_canvas_draw_state_t* kryon_canvas_get_state(void);

// Set the active command buffer for canvas operations
// This allows canvas to write to a specific buffer (e.g., component's buffer)
void kryon_canvas_set_command_buffer(kryon_cmd_buf_t* buf);

// Set the drawing offset (for positioning canvas within a component)
void kryon_canvas_set_offset(int16_t x, int16_t y);

// Clear canvas with background color
void kryon_canvas_clear(void);

// Clear canvas with specific color
void kryon_canvas_clear_color(uint32_t color);

// ============================================================================
// Drawing State Management (Love2D Style)
// ============================================================================

// Color management
void kryon_canvas_set_color(uint32_t color);
void kryon_canvas_set_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void kryon_canvas_set_color_rgb(uint8_t r, uint8_t g, uint8_t b);
uint32_t kryon_canvas_get_color(void);

void kryon_canvas_set_background_color(uint32_t color);
void kryon_canvas_set_background_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// Line properties
void kryon_canvas_set_line_width(kryon_fp_t width);
kryon_fp_t kryon_canvas_get_line_width(void);

void kryon_canvas_set_line_style(kryon_line_style_t style);
kryon_line_style_t kryon_canvas_get_line_style(void);

void kryon_canvas_set_line_join(kryon_line_join_t join);
kryon_line_join_t kryon_canvas_get_line_join(void);

// Font management
void kryon_canvas_set_font(uint16_t font_id);
uint16_t kryon_canvas_get_font(void);

// Blend modes
void kryon_canvas_set_blend_mode(kryon_blend_mode_t mode);
kryon_blend_mode_t kryon_canvas_get_blend_mode(void);

// ============================================================================
// Transform System (Love2D Style)
// ============================================================================

// Matrix operations
void kryon_canvas_origin(void);
void kryon_canvas_translate(kryon_fp_t x, kryon_fp_t y);
void kryon_canvas_rotate(kryon_fp_t angle);
void kryon_canvas_scale(kryon_fp_t sx, kryon_fp_t sy);
void kryon_canvas_shear(kryon_fp_t kx, kryon_fp_t ky);

// Transform stack
void kryon_canvas_push(void);
void kryon_canvas_pop(void);

// Get current transform matrix
void kryon_canvas_get_transform(kryon_fp_t matrix[6]);

// ============================================================================
// Basic Drawing Primitives (Love2D Style)
// ============================================================================

// Rectangle drawing
void kryon_canvas_rectangle(kryon_draw_mode_t mode, kryon_fp_t x, kryon_fp_t y, kryon_fp_t width, kryon_fp_t height);

// Circle drawing
void kryon_canvas_circle(kryon_draw_mode_t mode, kryon_fp_t x, kryon_fp_t y, kryon_fp_t radius);

// Ellipse drawing
void kryon_canvas_ellipse(kryon_draw_mode_t mode, kryon_fp_t x, kryon_fp_t y, kryon_fp_t rx, kryon_fp_t ry);

// Polygon drawing
void kryon_canvas_polygon(kryon_draw_mode_t mode, const kryon_fp_t* vertices, uint16_t vertex_count);

// Line drawing
void kryon_canvas_line_points(const kryon_fp_t* points, uint16_t point_count);
void kryon_canvas_line(kryon_fp_t x1, kryon_fp_t y1, kryon_fp_t x2, kryon_fp_t y2);

// Point drawing
void kryon_canvas_point(kryon_fp_t x, kryon_fp_t y);
void kryon_canvas_points(const kryon_fp_t* points, uint16_t point_count);

// Arc and sector drawing
void kryon_canvas_arc(kryon_draw_mode_t mode, kryon_fp_t cx, kryon_fp_t cy, kryon_fp_t radius,
                     kryon_fp_t start_angle, kryon_fp_t end_angle);

// ============================================================================
// Text Rendering (Love2D Style)
// ============================================================================

// Basic text printing
void kryon_canvas_print(const char* text, kryon_fp_t x, kryon_fp_t y);

// Formatted text printing with wrapping and alignment
void kryon_canvas_printf(const char* text, kryon_fp_t x, kryon_fp_t y, kryon_fp_t wrap_limit, int32_t align);

// Text measurement
kryon_fp_t kryon_canvas_get_text_width(const char* text);
kryon_fp_t kryon_canvas_get_text_height(void);
kryon_fp_t kryon_canvas_get_font_size(void);

// ============================================================================
// Geometry Utilities for Canvas
// ============================================================================

// Circle tessellation for rendering
uint16_t kryon_canvas_tessellate_circle(kryon_fp_t radius, kryon_fp_t** vertices);

// Ellipse tessellation for rendering
uint16_t kryon_canvas_tessellate_ellipse(kryon_fp_t rx, kryon_fp_t ry, kryon_fp_t** vertices);

// Arc tessellation for rendering
uint16_t kryon_canvas_tessellate_arc(kryon_fp_t radius, kryon_fp_t start_angle, kryon_fp_t end_angle,
                                    kryon_fp_t** vertices);

// Triangle fan generation from vertices
void kryon_canvas_triangle_fan(const kryon_fp_t* vertices, uint16_t vertex_count, kryon_cmd_buf_t* buf);

// Line loop generation from vertices
void kryon_canvas_line_loop(const kryon_fp_t* vertices, uint16_t vertex_count, kryon_cmd_buf_t* buf);

// Polygon triangulation (ear clipping method)
bool kryon_canvas_triangulate_polygon(const kryon_fp_t* vertices, uint16_t vertex_count,
                                     uint16_t** triangle_indices, uint16_t* triangle_count);

// ============================================================================
// Constants and Utilities
// ============================================================================

// PI constant for angle calculations
#ifndef KRYON_PI
#define KRYON_PI 3.14159265358979323846f
#endif

// Default circle segments for tessellation
#define KRYON_DEFAULT_CIRCLE_SEGMENTS 32

// Helper macros for common colors
#define KRYON_COLOR_RED     0xFF0000FF
#define KRYON_COLOR_GREEN   0x00FF00FF
#define KRYON_COLOR_BLUE    0x0000FFFF
#define KRYON_COLOR_WHITE   0xFFFFFFFF
#define KRYON_COLOR_BLACK   0x000000FF
#define KRYON_COLOR_YELLOW  0xFFFF00FF
#define KRYON_COLOR_CYAN    0x00FFFFFF
#define KRYON_COLOR_MAGENTA 0xFF00FFFF
#define KRYON_COLOR_GRAY    0x808080FF
#define KRYON_COLOR_ORANGE  0xFFA500FF
#define KRYON_COLOR_PURPLE  0x800080FF

// Helper macros for drawing modes
#define KRYON_FILL  KRYON_DRAW_FILL
#define KRYON_LINE  KRYON_DRAW_LINE

// ============================================================================
// Canvas Command Buffer Access
// ============================================================================

// Get the canvas command buffer for rendering
kryon_cmd_buf_t* kryon_canvas_get_command_buffer(void);

#ifdef __cplusplus
}
#endif

#endif // KRYON_CANVAS_H