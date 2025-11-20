#include "canvas_sdl3.h"
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Global canvas state
static SDL_Renderer* g_renderer = NULL;
static SDL_Color g_fill_color = {255, 255, 255, 255};
static SDL_Color g_stroke_color = {0, 0, 0, 255};
static float g_line_width = 1.0f;

// Context management
void canvas_sdl3_set_renderer(SDL_Renderer* renderer) {
    g_renderer = renderer;
}

SDL_Renderer* canvas_sdl3_get_renderer(void) {
    return g_renderer;
}

// Color management
void canvas_sdl3_background(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!g_renderer) return;
    SDL_SetRenderDrawColor(g_renderer, r, g, b, a);
    SDL_RenderClear(g_renderer);
}

void canvas_sdl3_fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    g_fill_color.r = r;
    g_fill_color.g = g;
    g_fill_color.b = b;
    g_fill_color.a = a;
}

void canvas_sdl3_stroke(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    g_stroke_color.r = r;
    g_stroke_color.g = g;
    g_stroke_color.b = b;
    g_stroke_color.a = a;
}

void canvas_sdl3_stroke_weight(float weight) {
    g_line_width = weight;
}

// Basic shapes
void canvas_sdl3_rect(float x, float y, float width, float height) {
    if (!g_renderer) return;
    SDL_SetRenderDrawColor(g_renderer, g_fill_color.r, g_fill_color.g, g_fill_color.b, g_fill_color.a);
    SDL_FRect rect = {x, y, width, height};
    SDL_RenderFillRect(g_renderer, &rect);
}

void canvas_sdl3_rectangle(CanvasDrawMode mode, float x, float y, float width, float height) {
    if (!g_renderer) return;
    SDL_FRect rect = {x, y, width, height};

    if (mode == CANVAS_DRAW_FILL) {
        SDL_SetRenderDrawColor(g_renderer, g_fill_color.r, g_fill_color.g, g_fill_color.b, g_fill_color.a);
        SDL_RenderFillRect(g_renderer, &rect);
    } else {
        SDL_SetRenderDrawColor(g_renderer, g_stroke_color.r, g_stroke_color.g, g_stroke_color.b, g_stroke_color.a);
        SDL_RenderRect(g_renderer, &rect);
    }
}

void canvas_sdl3_circle(CanvasDrawMode mode, float x, float y, float radius) {
    if (!g_renderer) return;

    int segments = (int)(radius * 2.0f);
    if (segments < 16) segments = 16;

    SDL_FPoint* points = malloc((segments + 1) * sizeof(SDL_FPoint));
    if (!points) return;

    for (int i = 0; i <= segments; i++) {
        float angle = ((float)i / (float)segments) * 2.0f * M_PI;
        points[i].x = x + radius * cosf(angle);
        points[i].y = y + radius * sinf(angle);
    }

    if (mode == CANVAS_DRAW_FILL) {
        SDL_SetRenderDrawColor(g_renderer, g_fill_color.r, g_fill_color.g, g_fill_color.b, g_fill_color.a);
        // Draw fan triangles from center
        for (int i = 0; i < segments; i++) {
            SDL_RenderLine(g_renderer, x, y, points[i].x, points[i].y);
            SDL_RenderLine(g_renderer, points[i].x, points[i].y, points[i+1].x, points[i+1].y);
        }
    } else {
        SDL_SetRenderDrawColor(g_renderer, g_stroke_color.r, g_stroke_color.g, g_stroke_color.b, g_stroke_color.a);
        SDL_RenderLines(g_renderer, points, segments + 1);
    }

    free(points);
}

void canvas_sdl3_ellipse(CanvasDrawMode mode, float x, float y, float rx, float ry) {
    if (!g_renderer) return;

    int segments = 32;
    SDL_FPoint* points = malloc((segments + 1) * sizeof(SDL_FPoint));
    if (!points) return;

    for (int i = 0; i <= segments; i++) {
        float angle = ((float)i / (float)segments) * 2.0f * M_PI;
        points[i].x = x + rx * cosf(angle);
        points[i].y = y + ry * sinf(angle);
    }

    if (mode == CANVAS_DRAW_FILL) {
        SDL_SetRenderDrawColor(g_renderer, g_fill_color.r, g_fill_color.g, g_fill_color.b, g_fill_color.a);
        for (int i = 0; i < segments; i++) {
            SDL_RenderLine(g_renderer, x, y, points[i].x, points[i].y);
            SDL_RenderLine(g_renderer, points[i].x, points[i].y, points[i+1].x, points[i+1].y);
        }
    } else {
        SDL_SetRenderDrawColor(g_renderer, g_stroke_color.r, g_stroke_color.g, g_stroke_color.b, g_stroke_color.a);
        SDL_RenderLines(g_renderer, points, segments + 1);
    }

    free(points);
}

void canvas_sdl3_polygon(CanvasDrawMode mode, float* vertices, int vertex_count) {
    if (!g_renderer || vertex_count < 3) return;

    SDL_FPoint* points = malloc((vertex_count + 1) * sizeof(SDL_FPoint));
    if (!points) return;

    for (int i = 0; i < vertex_count; i++) {
        points[i].x = vertices[i * 2];
        points[i].y = vertices[i * 2 + 1];
    }
    points[vertex_count] = points[0];  // Close polygon

    if (mode == CANVAS_DRAW_FILL) {
        SDL_SetRenderDrawColor(g_renderer, g_fill_color.r, g_fill_color.g, g_fill_color.b, g_fill_color.a);
        SDL_RenderLines(g_renderer, points, vertex_count + 1);
    } else {
        SDL_SetRenderDrawColor(g_renderer, g_stroke_color.r, g_stroke_color.g, g_stroke_color.b, g_stroke_color.a);
        SDL_RenderLines(g_renderer, points, vertex_count + 1);
    }

    free(points);
}

// Lines
void canvas_sdl3_line(float x1, float y1, float x2, float y2) {
    if (!g_renderer) return;
    SDL_SetRenderDrawColor(g_renderer, g_stroke_color.r, g_stroke_color.g, g_stroke_color.b, g_stroke_color.a);
    SDL_RenderLine(g_renderer, x1, y1, x2, y2);
}

void canvas_sdl3_lines(float* points, int point_count) {
    if (!g_renderer || point_count < 2) return;

    SDL_FPoint* sdl_points = malloc(point_count * sizeof(SDL_FPoint));
    if (!sdl_points) return;

    for (int i = 0; i < point_count; i++) {
        sdl_points[i].x = points[i * 2];
        sdl_points[i].y = points[i * 2 + 1];
    }

    SDL_SetRenderDrawColor(g_renderer, g_stroke_color.r, g_stroke_color.g, g_stroke_color.b, g_stroke_color.a);
    SDL_RenderLines(g_renderer, sdl_points, point_count);

    free(sdl_points);
}

// Arcs
void canvas_sdl3_arc(CanvasDrawMode mode, float cx, float cy, float radius, float start_angle, float end_angle) {
    if (!g_renderer) return;

    int segments = 32;
    int point_count = segments + 1;
    if (mode == CANVAS_DRAW_FILL) point_count++; // Add center point

    SDL_FPoint* points = malloc(point_count * sizeof(SDL_FPoint));
    if (!points) return;

    int idx = 0;
    if (mode == CANVAS_DRAW_FILL) {
        points[idx++] = (SDL_FPoint){cx, cy}; // Center point
    }

    for (int i = 0; i <= segments; i++) {
        float t = (float)i / (float)segments;
        float angle = start_angle + (end_angle - start_angle) * t;
        points[idx++] = (SDL_FPoint){
            cx + radius * cosf(angle),
            cy + radius * sinf(angle)
        };
    }

    if (mode == CANVAS_DRAW_FILL) {
        SDL_SetRenderDrawColor(g_renderer, g_fill_color.r, g_fill_color.g, g_fill_color.b, g_fill_color.a);
        // Draw triangles from center
        for (int i = 1; i < point_count - 1; i++) {
            SDL_RenderLine(g_renderer, points[0].x, points[0].y, points[i].x, points[i].y);
            SDL_RenderLine(g_renderer, points[i].x, points[i].y, points[i+1].x, points[i+1].y);
            SDL_RenderLine(g_renderer, points[i+1].x, points[i+1].y, points[0].x, points[0].y);
        }
    } else {
        SDL_SetRenderDrawColor(g_renderer, g_stroke_color.r, g_stroke_color.g, g_stroke_color.b, g_stroke_color.a);
        SDL_RenderLines(g_renderer, points, point_count);
    }

    free(points);
}

// Text (stub)
void canvas_sdl3_print(const char* text, float x, float y) {
    // TODO: Implement text rendering with SDL_ttf
    (void)text;
    (void)x;
    (void)y;
}
