#ifndef CANVAS_SDL3_H
#define CANVAS_SDL3_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL3/SDL.h>

// Canvas drawing API for SDL3 backend
// This provides Love2D-style immediate mode drawing

// Types
typedef enum {
    CANVAS_DRAW_FILL = 0,
    CANVAS_DRAW_LINE = 1
} CanvasDrawMode;

// Global canvas context management
void canvas_sdl3_set_renderer(SDL_Renderer* renderer);
SDL_Renderer* canvas_sdl3_get_renderer(void);

// Color management
void canvas_sdl3_background(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void canvas_sdl3_fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void canvas_sdl3_stroke(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void canvas_sdl3_stroke_weight(float weight);

// Basic shapes
void canvas_sdl3_rect(float x, float y, float width, float height);
void canvas_sdl3_rectangle(CanvasDrawMode mode, float x, float y, float width, float height);
void canvas_sdl3_circle(CanvasDrawMode mode, float x, float y, float radius);
void canvas_sdl3_ellipse(CanvasDrawMode mode, float x, float y, float rx, float ry);
void canvas_sdl3_polygon(CanvasDrawMode mode, float* vertices, int vertex_count);

// Lines
void canvas_sdl3_line(float x1, float y1, float x2, float y2);
void canvas_sdl3_lines(float* points, int point_count);

// Arcs
void canvas_sdl3_arc(CanvasDrawMode mode, float cx, float cy, float radius, float start_angle, float end_angle);

// Text (stub - would need SDL_ttf)
void canvas_sdl3_print(const char* text, float x, float y);

#endif // CANVAS_SDL3_H
