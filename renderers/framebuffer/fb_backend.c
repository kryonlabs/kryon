#include "../../core/include/kryon.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// ============================================================================
// Framebuffer Backend Configuration
// ============================================================================

typedef struct {
    uint8_t* buffer;                     // Framebuffer memory
    uint16_t width;                      // Framebuffer width
    uint16_t height;                     // Framebuffer height
    uint16_t stride;                     // Bytes per row
    uint8_t bytes_per_pixel;             // Usually 1 (8-bit), 2 (16-bit), or 4 (32-bit)
    bool double_buffered;                // Double buffering support
    uint8_t* back_buffer;                // Back buffer for double buffering
} kryon_fb_state_t;

// Font data structure (1-bit monochrome font)
typedef struct {
    uint8_t width;                       // Character width
    uint8_t height;                      // Character height
    uint8_t first_char;                  // First ASCII character in font
    uint8_t char_count;                  // Number of characters
    const uint8_t* data;                 // Font bitmap data
} kryon_fb_font_t;

static void fb_draw_rect(kryon_fb_state_t* state, int16_t x, int16_t y, uint16_t w, uint16_t h, uint32_t color);
static void fb_draw_text(kryon_fb_state_t* state, const char* text, int16_t x, int16_t y, uint16_t font_id, uint32_t color);
static void fb_draw_line(kryon_fb_state_t* state, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color);
static void fb_draw_circle_filled(kryon_fb_state_t* state, int16_t cx, int16_t cy, uint16_t radius, uint32_t color);

// Font cache
#define KRYON_FB_MAX_FONTS 4
static kryon_fb_font_t font_cache[KRYON_FB_MAX_FONTS];
static uint8_t font_count = 0;

static kryon_fb_state_t* fb_exec_state = NULL;

static bool fb_trace_text_enabled(void) {
    static bool initialized = false;
    static bool enabled = false;

    if (!initialized) {
        initialized = true;
        const char* env = getenv("KRYON_TRACE_FB_TEXT");
        enabled = (env != NULL && env[0] != '\0' && env[0] != '0');
    }

    return enabled;
}

static void fb_exec_draw_rect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint32_t color) {
    if (fb_exec_state != NULL) {
        fb_draw_rect(fb_exec_state, x, y, w, h, color);
    }
}

static void fb_exec_draw_text(const char* text, int16_t x, int16_t y, uint16_t font_id, uint32_t color) {
    if (fb_exec_state != NULL) {
        fb_draw_text(fb_exec_state, text, x, y, font_id, color);
    }
}

static void fb_exec_draw_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color) {
    if (fb_exec_state != NULL) {
        fb_draw_line(fb_exec_state, x1, y1, x2, y2, color);
    }
}

static void fb_exec_draw_arc(int16_t cx, int16_t cy, uint16_t radius, int16_t start_angle, int16_t end_angle, uint32_t color) {
    if (fb_exec_state != NULL) {
        fb_draw_circle_filled(fb_exec_state, cx, cy, radius, color);
    }
}

// ============================================================================
// Color Conversion for Framebuffer
// ============================================================================

static uint8_t rgb_to_8bit(uint32_t color) {
    uint8_t r = kryon_color_get_red(color);
    uint8_t g = kryon_color_get_green(color);
    uint8_t b = kryon_color_get_blue(color);

    // Convert RGB 8:8:8 to 3:3:2 (common for 8-bit framebuffers)
    uint8_t result = ((r & 0xE0) >> 5) << 5;   // Red (3 bits)
    result |= ((g & 0xE0) >> 5) << 2;          // Green (3 bits)
    result |= (b & 0xC0) >> 6;                 // Blue (2 bits)

    return result;
}

static uint16_t rgb_to_16bit(uint32_t color) {
    uint8_t r = kryon_color_get_red(color);
    uint8_t g = kryon_color_get_green(color);
    uint8_t b = kryon_color_get_blue(color);

    // Convert RGB 8:8:8 to 5:6:5 (common for 16-bit framebuffers)
    uint16_t result = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return result;
}

static uint32_t rgb_to_32bit(uint32_t color) {
    // Already in 32-bit format (assuming RGBA)
    return color;
}

// ============================================================================
// Pixel Operations
// ============================================================================

static void set_pixel_8bit(kryon_fb_state_t* state, int16_t x, int16_t y, uint32_t color) {
    if (state == NULL || state->buffer == NULL) return;

    if (x < 0 || x >= state->width || y < 0 || y >= state->height) return;

    uint32_t index = y * state->stride + x;
    state->buffer[index] = rgb_to_8bit(color);
}

static void set_pixel_16bit(kryon_fb_state_t* state, int16_t x, int16_t y, uint32_t color) {
    if (state == NULL || state->buffer == NULL) return;

    if (x < 0 || x >= state->width || y < 0 || y >= state->height) return;

    uint32_t index = y * state->stride + x * 2;
    uint16_t pixel = rgb_to_16bit(color);

    // Handle endianness (assuming little-endian)
    state->buffer[index] = pixel & 0xFF;
    state->buffer[index + 1] = (pixel >> 8) & 0xFF;
}

static void set_pixel_32bit(kryon_fb_state_t* state, int16_t x, int16_t y, uint32_t color) {
    if (state == NULL || state->buffer == NULL) return;

    if (x < 0 || x >= state->width || y < 0 || y >= state->height) return;

    uint32_t index = y * state->stride + x * 4;
    uint32_t pixel = rgb_to_32bit(color);

    // Handle endianness (assuming little-endian)
    state->buffer[index] = pixel & 0xFF;
    state->buffer[index + 1] = (pixel >> 8) & 0xFF;
    state->buffer[index + 2] = (pixel >> 16) & 0xFF;
    state->buffer[index + 3] = (pixel >> 24) & 0xFF;
}

static void (*set_pixel_func)(kryon_fb_state_t*, int16_t, int16_t, uint32_t) = NULL;

// ============================================================================
// Drawing Primitives
// ============================================================================

static void fb_draw_rect(kryon_fb_state_t* state, int16_t x, int16_t y, uint16_t w, uint16_t h, uint32_t color) {
    if (state == NULL || set_pixel_func == NULL) return;

    for (int16_t py = y; py < y + h; py++) {
        for (int16_t px = x; px < x + w; px++) {
            set_pixel_func(state, px, py, color);
        }
    }
}

static void fb_draw_line(kryon_fb_state_t* state, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color) {
    if (state == NULL || set_pixel_func == NULL) return;

    // Bresenham's line algorithm
    int16_t dx = abs(x2 - x1);
    int16_t dy = abs(y2 - y1);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;

    while (true) {
        set_pixel_func(state, x1, y1, color);

        if (x1 == x2 && y1 == y2) break;

        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

static void fb_draw_circle(kryon_fb_state_t* state, int16_t cx, int16_t cy, uint16_t radius, uint32_t color) {
    if (state == NULL || set_pixel_func == NULL) return;

    // Midpoint circle algorithm
    int16_t x = 0;
    int16_t y = radius;
    int16_t d = 1 - radius;

    while (x <= y) {
        // Draw 8 octants
        set_pixel_func(state, cx + x, cy + y, color);
        set_pixel_func(state, cx - x, cy + y, color);
        set_pixel_func(state, cx + x, cy - y, color);
        set_pixel_func(state, cx - x, cy - y, color);
        set_pixel_func(state, cx + y, cy + x, color);
        set_pixel_func(state, cx - y, cy + x, color);
        set_pixel_func(state, cx + y, cy - x, color);
        set_pixel_func(state, cx - y, cy - x, color);

        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

static void fb_draw_circle_filled(kryon_fb_state_t* state, int16_t cx, int16_t cy, uint16_t radius, uint32_t color) {
    if (state == NULL || set_pixel_func == NULL) return;

    // Fill circles by drawing horizontal lines
    for (int16_t y = -radius; y <= radius; y++) {
        for (int16_t x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                set_pixel_func(state, cx + x, cy + y, color);
            }
        }
    }
}

// ============================================================================
// Font Rendering
// ============================================================================

static void fb_draw_char(kryon_fb_state_t* state, char c, int16_t x, int16_t y, uint32_t color, const kryon_fb_font_t* font) {
    if (state == NULL || font == NULL || font->data == NULL) return;

    if (c < font->first_char || c >= font->first_char + font->char_count) {
        return;
    }

    uint8_t char_index = c - font->first_char;
    uint32_t bitmap_offset = char_index * font->width * font->height / 8;
    const uint8_t* bitmap = &font->data[bitmap_offset];

    for (uint8_t row = 0; row < font->height; row++) {
        for (uint8_t col = 0; col < font->width; col++) {
            uint32_t bit_index = row * font->width + col;
            uint32_t byte_index = bit_index / 8;
            uint8_t bit_mask = 1 << (7 - (bit_index % 8));

            if (bitmap[byte_index] & bit_mask) {
                set_pixel_func(state, x + col, y + row, color);
            }
        }
    }
}

static void fb_draw_text(kryon_fb_state_t* state, const char* text, int16_t x, int16_t y, uint16_t font_id, uint32_t color) {
    if (fb_trace_text_enabled()) {
        fprintf(stderr, "[kryon][fb][text] state=%p text=%p fontId=%u fontCount=%u\n",
                (void*)state, (const void*)text, (unsigned)font_id, (unsigned)font_count);
    }

    if (state == NULL || text == NULL || font_id >= font_count) {
        if (fb_trace_text_enabled()) {
            fprintf(stderr, "[kryon][fb][text] skipped\n");
        }
        return;
    }

    const kryon_fb_font_t* font = &font_cache[font_id];
    int16_t current_x = x;

    if (fb_trace_text_enabled()) {
        fprintf(stderr, "[kryon][fb][text] enter font=%u len=%zu charCount=%u\n",
                (unsigned)font_id, strlen(text), (unsigned)font->char_count);
    }

    while (*text != '\0') {
        if (*text == '\n') {
            current_x = x;
            y += font->height + 1;
        } else if (*text == '\t') {
            current_x += font->width * 4; // 4 spaces for tab
        } else {
            fb_draw_char(state, *text, current_x, y, color, font);
            current_x += font->width + 1; // 1 pixel spacing between characters
        }
        text++;
    }

    if (fb_trace_text_enabled()) {
        fprintf(stderr, "[kryon][fb][text] exit\n");
    }
}

// ============================================================================
// Built-in 6x8 Font
// ============================================================================

static const uint8_t font_6x8_data[] = {
    // This is a minimal font - in a real implementation, this would be much larger
    // For now, just include a few essential characters for testing
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ' '
    0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, // '!'
    0x36, 0x36, 0x00, 0x00, 0x00, 0x00, // '"'
    0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, // '#'
    0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, // '$'
    // ... more characters would be added here
};

static const kryon_fb_font_t font_6x8 = {
    .width = 6,
    .height = 8,
    .first_char = 32,
    .char_count = sizeof(font_6x8_data) / 6,
    .data = font_6x8_data
};

// ============================================================================
// Backend Implementation Functions
// ============================================================================

static bool fb_init(kryon_renderer_t* renderer, void* native_window) {
    if (renderer == NULL) return false;

    // Allocate state
    kryon_fb_state_t* state = (kryon_fb_state_t*)renderer->backend_data;
    if (state == NULL) {
#if KRYON_NO_HEAP
        static kryon_fb_state_t static_state;
        state = &static_state;
#else
        state = (kryon_fb_state_t*)malloc(sizeof(kryon_fb_state_t));
        if (state == NULL) return false;
#endif
        renderer->backend_data = state;
    }

    // Initialize state with default values
    state->width = 320;    // Default resolution for small displays
    state->height = 240;
    state->bytes_per_pixel = 2; // 16-bit color
    state->stride = state->width * state->bytes_per_pixel;
    state->double_buffered = false;

    // Allocate framebuffer
    uint32_t buffer_size = state->height * state->stride;
#if KRYON_NO_HEAP
    static uint8_t static_buffer[320 * 240 * 2]; // Max size for static allocation
    if (buffer_size > sizeof(static_buffer)) {
        return false; // Buffer too large for static allocation
    }
    state->buffer = static_buffer;
#else
    state->buffer = (uint8_t*)malloc(buffer_size);
    if (state->buffer == NULL) {
#if !KRYON_NO_HEAP
        free(state);
#endif
        return false;
    }
#endif

    // Clear framebuffer
    memset(state->buffer, 0, buffer_size);

    // Set pixel function based on color depth
    switch (state->bytes_per_pixel) {
        case 1:
            set_pixel_func = set_pixel_8bit;
            break;
        case 2:
            set_pixel_func = set_pixel_16bit;
            break;
        case 4:
            set_pixel_func = set_pixel_32bit;
            break;
        default:
            set_pixel_func = set_pixel_16bit; // Default to 16-bit
            break;
    }

    // Initialize font cache
    font_count = 0;
    if (font_count < KRYON_FB_MAX_FONTS) {
        font_cache[font_count] = font_6x8;
        font_count++;
    }

    // Update renderer dimensions
    renderer->width = state->width;
    renderer->height = state->height;

    return true;
}

static void fb_shutdown(kryon_renderer_t* renderer) {
    if (renderer == NULL || renderer->backend_data == NULL) return;

    kryon_fb_state_t* state = (kryon_fb_state_t*)renderer->backend_data;

#if !KRYON_NO_HEAP
    if (state->buffer != NULL) {
        free(state->buffer);
    }
    free(state);
#endif

    renderer->backend_data = NULL;
}

static void fb_begin_frame(kryon_renderer_t* renderer) {
    if (renderer == NULL || renderer->backend_data == NULL) return;

    kryon_fb_state_t* state = (kryon_fb_state_t*)renderer->backend_data;

    // Clear framebuffer with clear color
    fb_draw_rect(state, 0, 0, state->width, state->height, renderer->clear_color);
}

static void fb_end_frame(kryon_renderer_t* renderer) {
    // Nothing to do for framebuffer backend
    (void)renderer;
}

static void fb_execute_commands(kryon_renderer_t* renderer, kryon_cmd_buf_t* buf) {
    if (renderer == NULL || renderer->backend_data == NULL || buf == NULL) return;

    kryon_fb_state_t* state = (kryon_fb_state_t*)renderer->backend_data;

    fb_exec_state = state;

    kryon_command_executor_t executor = (kryon_command_executor_t){0};
    executor.execute_draw_rect = fb_exec_draw_rect;
    executor.execute_draw_text = fb_exec_draw_text;
    executor.execute_draw_line = fb_exec_draw_line;
    executor.execute_draw_arc = fb_exec_draw_arc;

    kryon_execute_commands(buf, &executor);

    fb_exec_state = NULL;
}

static void fb_swap_buffers(kryon_renderer_t* renderer) {
    if (renderer == NULL || renderer->backend_data == NULL) return;

    kryon_fb_state_t* state = (kryon_fb_state_t*)renderer->backend_data;

    // For single buffering, nothing to do
    if (!state->double_buffered) {
        return;
    }

    // Swap buffers (implementation would be platform-specific)
    // For now, just copy back buffer to front buffer
    if (state->back_buffer != NULL && state->buffer != NULL) {
        uint32_t buffer_size = state->height * state->stride;
        memcpy(state->buffer, state->back_buffer, buffer_size);
    }
}

static void fb_get_dimensions(kryon_renderer_t* renderer, uint16_t* width, uint16_t* height) {
    if (renderer == NULL || renderer->backend_data == NULL) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }

    kryon_fb_state_t* state = (kryon_fb_state_t*)renderer->backend_data;
    if (width) *width = state->width;
    if (height) *height = state->height;
}

// ============================================================================
// Backend Operations Structure
// ============================================================================

const kryon_renderer_ops_t kryon_framebuffer_ops = {
    .init = fb_init,
    .shutdown = fb_shutdown,
    .begin_frame = fb_begin_frame,
    .end_frame = fb_end_frame,
    .execute_commands = fb_execute_commands,
    .swap_buffers = fb_swap_buffers,
    .get_dimensions = fb_get_dimensions,
    .set_clear_color = NULL // Use default implementation
};

// ============================================================================
// Public API Functions
// ============================================================================

// Forward declaration
bool kryon_framebuffer_renderer_init(kryon_renderer_t* renderer, uint16_t width, uint16_t height,
                                    uint8_t bytes_per_pixel, void* external_buffer);

kryon_renderer_t* kryon_framebuffer_renderer_create(uint16_t width, uint16_t height, uint8_t bytes_per_pixel) {
    kryon_renderer_t* renderer = kryon_renderer_create(&kryon_framebuffer_ops);
    if (renderer == NULL) return NULL;

    if (!kryon_renderer_init(renderer, NULL)) {
        kryon_renderer_destroy(renderer);
        return NULL;
    }

    // Initialize with custom parameters
    if (!kryon_framebuffer_renderer_init(renderer, width, height, bytes_per_pixel, NULL)) {
        kryon_renderer_destroy(renderer);
        return NULL;
    }

    return renderer;
}

bool kryon_framebuffer_renderer_init(kryon_renderer_t* renderer, uint16_t width, uint16_t height,
                                    uint8_t bytes_per_pixel, void* external_buffer) {
    if (renderer == NULL || renderer->backend_data == NULL) return false;

    kryon_fb_state_t* state = (kryon_fb_state_t*)renderer->backend_data;
    if (bytes_per_pixel == 0) {
        bytes_per_pixel = state->bytes_per_pixel == 0 ? 2 : state->bytes_per_pixel;
    }

    state->width = width;
    state->height = height;
    state->bytes_per_pixel = bytes_per_pixel;
    state->stride = width * bytes_per_pixel;

    uint32_t buffer_size = (uint32_t)state->stride * state->height;

#if KRYON_NO_HEAP
    if (external_buffer != NULL) {
        state->buffer = (uint8_t*)external_buffer;
    } else {
        static uint8_t static_buffer[320 * 240 * 4];
        if (buffer_size > sizeof(static_buffer)) {
            return false;
        }
        state->buffer = static_buffer;
    }
#else
    if (external_buffer != NULL) {
        if (state->buffer != external_buffer && state->buffer != NULL) {
            free(state->buffer);
        }
        state->buffer = (uint8_t*)external_buffer;
    } else {
        if (state->buffer != NULL) {
            free(state->buffer);
        }
        state->buffer = (uint8_t*)malloc(buffer_size);
        if (state->buffer == NULL) {
            return false;
        }
    }
#endif

    if (state->buffer != NULL) {
        memset(state->buffer, 0, buffer_size);
    }
    state->double_buffered = false;
    state->back_buffer = NULL;

    // Update pixel function
    switch (bytes_per_pixel) {
        case 1:
            set_pixel_func = set_pixel_8bit;
            break;
        case 2:
            set_pixel_func = set_pixel_16bit;
            break;
        case 4:
            set_pixel_func = set_pixel_32bit;
            break;
        default:
            set_pixel_func = set_pixel_16bit;
            break;
    }

    renderer->width = width;
    renderer->height = height;

    if (getenv("KRYON_TRACE_COMMANDS")) {
        fprintf(stderr, "[kryon][fb] init width=%u height=%u bpp=%u stride=%u\n",
                state->width, state->height, state->bytes_per_pixel, state->stride);
    }

    return true;
}

void kryon_framebuffer_get_buffer(kryon_renderer_t* renderer, uint8_t** buffer, uint16_t* width, uint16_t* height, uint16_t* stride) {
    if (renderer == NULL || renderer->backend_data == NULL) {
        if (buffer) *buffer = NULL;
        if (width) *width = 0;
        if (height) *height = 0;
        if (stride) *stride = 0;
        return;
    }

    kryon_fb_state_t* state = (kryon_fb_state_t*)renderer->backend_data;
    if (buffer) *buffer = state->buffer;
    if (width) *width = state->width;
    if (height) *height = state->height;
    if (stride) *stride = state->stride;
}

// Helper function for C bindings
const kryon_renderer_ops_t* kryon_get_framebuffer_renderer_ops(void) {
    return &kryon_framebuffer_ops;
}
