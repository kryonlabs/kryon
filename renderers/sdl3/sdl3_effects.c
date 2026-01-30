/*
 * SDL3 Effects Rendering Module
 *
 * Implements gradient rendering, shadows, and rounded rectangles for SDL3.
 * Optimized for performance with fixed ring counts and strip limits.
 */

#include "sdl3_effects.h"
#include <math.h>
#include <stdio.h>

/* Global blend mode cache flag (reset per frame) */
static bool g_blend_mode_set = false;

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

SDL_Color sdl3_unpack_color(uint32_t color) {
    return (SDL_Color){
        .r = (color >> 24) & 0xFF,
        .g = (color >> 16) & 0xFF,
        .b = (color >> 8) & 0xFF,
        .a = color & 0xFF
    };
}

uint32_t sdl3_apply_opacity(uint32_t color, float opacity) {
    uint8_t a = color & 0xFF;
    a = (uint8_t)(a * opacity);
    return (color & 0xFFFFFF00) | a;
}

void sdl3_ensure_blend_mode(SDL_Renderer* renderer) {
    if (g_blend_mode_set) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    g_blend_mode_set = true;
}

/* Reset blend mode cache (call at start of frame) */
void sdl3_reset_blend_mode_cache(void) {
    g_blend_mode_set = false;
}

/* ============================================================================
 * Gradient Utilities
 * ============================================================================ */

static int find_gradient_stop_binary(SDL3_GradientStop* stops, uint8_t count, float t) {
    if (count < 2) return 0;

    int left = 0;
    int right = count - 2;

    while (left < right) {
        int mid = (left + right) / 2;
        if (t < stops[mid].position) {
            right = mid - 1;
        } else if (t > stops[mid + 1].position) {
            left = mid + 1;
        } else {
            return mid;
        }
    }

    if (left < 0) return 0;
    if (left >= count - 1) return count - 2;
    return left;
}

static void interpolate_color(
    SDL3_GradientStop* from,
    SDL3_GradientStop* to,
    float t,
    uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a
) {
    *r = (uint8_t)(from->r + (to->r - from->r) * t);
    *g = (uint8_t)(from->g + (to->g - from->g) * t);
    *b = (uint8_t)(from->b + (to->b - from->b) * t);
    *a = (uint8_t)(from->a + (to->a - from->a) * t);
}

/* ============================================================================
 * Linear Gradient
 * ============================================================================ */

void sdl3_render_linear_gradient(
    SDL_Renderer* renderer,
    SDL_FRect* rect,
    int16_t angle,
    SDL3_GradientStop* stops,
    uint8_t stop_count,
    float opacity
) {
    if (!renderer || !rect || !stops || stop_count < 2) return;

    sdl3_ensure_blend_mode(renderer);

    /* Convert angle to radians */
    float angle_rad = angle * (3.14159265f / 180.0f);
    float cos_a = cosf(angle_rad);
    float sin_a = sinf(angle_rad);

    /* Calculate gradient vector */
    float dx = cos_a * rect->w;
    float dy = sin_a * rect->h;
    float length = sqrtf(dx * dx + dy * dy);

    /* Draw gradient using horizontal strips */
    int strips = (int)(length > 0 ? length : rect->h);
    if (strips < 1) strips = 1;
    if (strips > 256) strips = 256;  /* Performance limit */

    for (int i = 0; i < strips; i++) {
        float t = (float)i / (float)strips;

        /* Find gradient stops to interpolate */
        int stop_idx = find_gradient_stop_binary(stops, stop_count, t);

        /* Interpolate color */
        SDL3_GradientStop* from = &stops[stop_idx];
        SDL3_GradientStop* to = &stops[stop_idx + 1];
        float local_t = (t - from->position) / (to->position - from->position);
        if (local_t < 0.0f) local_t = 0.0f;
        if (local_t > 1.0f) local_t = 1.0f;

        uint8_t r, g, b, a;
        interpolate_color(from, to, local_t, &r, &g, &b, &a);

        /* Apply opacity */
        uint8_t final_a = (uint8_t)(a * opacity);

        /* Draw strip */
        SDL_SetRenderDrawColor(renderer, r, g, b, final_a);
        float strip_y = rect->y + (t * rect->h);
        SDL_FRect strip_rect = {
            .x = rect->x,
            .y = strip_y,
            .w = rect->w,
            .h = rect->h / strips + 1.0f
        };
        SDL_RenderFillRect(renderer, &strip_rect);
    }
}

/* ============================================================================
 * Radial Gradient
 * ============================================================================ */

void sdl3_render_radial_gradient(
    SDL_Renderer* renderer,
    SDL_FRect* rect,
    SDL3_GradientStop* stops,
    uint8_t stop_count,
    float opacity
) {
    if (!renderer || !rect || !stops || stop_count < 2) return;

    sdl3_ensure_blend_mode(renderer);

    float center_x = rect->x + rect->w * 0.5f;
    float center_y = rect->y + rect->h * 0.5f;
    float max_radius = sqrtf(rect->w * rect->w + rect->h * rect->h) / 2.0f;

    /* Fixed ring count for consistent performance */
    int rings = 30;

    for (int i = rings; i >= 0; i--) {
        float t = (float)i / (float)rings;

        /* Find gradient stops */
        int stop_idx = find_gradient_stop_binary(stops, stop_count, t);

        /* Interpolate color */
        SDL3_GradientStop* from = &stops[stop_idx];
        SDL3_GradientStop* to = &stops[stop_idx + 1];
        float local_t = (t - from->position) / (to->position - from->position);
        if (local_t < 0.0f) local_t = 0.0f;
        if (local_t > 1.0f) local_t = 1.0f;

        uint8_t r, g, b, a;
        interpolate_color(from, to, local_t, &r, &g, &b, &a);

        /* Apply opacity */
        uint8_t final_a = (uint8_t)(a * opacity);

        /* Draw filled circle */
        SDL_SetRenderDrawColor(renderer, r, g, b, final_a);
        float radius = t * max_radius;

        /* Circle approximation using polygon */
        int steps = (int)(radius * 2.0f);
        if (steps < 8) steps = 8;
        if (steps > 64) steps = 64;

        for (int j = 0; j < steps; j++) {
            float angle = (float)j / (float)steps * 2.0f * 3.14159265f;
            float x = center_x + cosf(angle) * radius;
            float y = center_y + sinf(angle) * radius;
            SDL_FRect point = { x - 1, y - 1, 2, 2 };
            SDL_RenderFillRect(renderer, &point);
        }
    }
}

/* ============================================================================
 * Conic Gradient
 * ============================================================================ */

void sdl3_render_conic_gradient(
    SDL_Renderer* renderer,
    SDL_FRect* rect,
    SDL3_GradientStop* stops,
    uint8_t stop_count,
    float opacity
) {
    if (!renderer || !rect || !stops || stop_count < 2) return;

    sdl3_ensure_blend_mode(renderer);

    float center_x = rect->x + rect->w * 0.5f;
    float center_y = rect->y + rect->h * 0.5f;

    /* Fixed section count - 10° per section */
    int sections = 36;

    for (int deg = 0; deg < sections; deg++) {
        float t = (float)deg / (float)sections;

        /* Find gradient stops */
        int stop_idx = find_gradient_stop_binary(stops, stop_count, t);

        /* Interpolate color */
        SDL3_GradientStop* from = &stops[stop_idx];
        SDL3_GradientStop* to = &stops[stop_idx + 1];
        float local_t = (t - from->position) / (to->position - from->position);
        if (local_t < 0.0f) local_t = 0.0f;
        if (local_t > 1.0f) local_t = 1.0f;

        uint8_t r, g, b, a;
        interpolate_color(from, to, local_t, &r, &g, &b, &a);

        /* Apply opacity */
        uint8_t final_a = (uint8_t)(a * opacity);

        SDL_SetRenderDrawColor(renderer, r, g, b, final_a);

        /* Draw triangle slice from center */
        float angle1 = (float)deg * 3.14159265f / 18.0f;  /* sections/2 = 18 */
        float max_dist = sqrtf(rect->w * rect->w + rect->h * rect->h);

        float x1 = center_x + cosf(angle1) * max_dist;
        float y1 = center_y + sinf(angle1) * max_dist;

        SDL_RenderLine(renderer, center_x, center_y, x1, y1);
    }
}

/* ============================================================================
 * Gradient Dispatcher
 * ============================================================================ */

void sdl3_render_gradient(
    SDL_Renderer* renderer,
    SDL_FRect* rect,
    SDL3_GradientType type,
    int16_t angle,
    SDL3_GradientStop* stops,
    uint8_t stop_count,
    float opacity
) {
    switch (type) {
        case SDL3_GRADIENT_LINEAR:
            sdl3_render_linear_gradient(renderer, rect, angle, stops, stop_count, opacity);
            break;
        case SDL3_GRADIENT_RADIAL:
            sdl3_render_radial_gradient(renderer, rect, stops, stop_count, opacity);
            break;
        case SDL3_GRADIENT_CONIC:
            sdl3_render_conic_gradient(renderer, rect, stops, stop_count, opacity);
            break;
    }
}

/* ============================================================================
 * Rounded Rectangle
 * ============================================================================ */

void sdl3_render_rounded_rect(
    SDL_Renderer* renderer,
    SDL_FRect* rect,
    uint16_t radius,
    uint32_t color
) {
    if (!renderer || !rect) return;

    SDL_Color c = sdl3_unpack_color(color);
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);

    if (radius == 0) {
        /* No rounding - just fill rect */
        SDL_RenderFillRect(renderer, rect);
        return;
    }

    sdl3_ensure_blend_mode(renderer);

    /* Clamp radius */
    float max_radius = (rect->w < rect->h ? rect->w : rect->h) / 2.0f;
    if (radius > max_radius) radius = (uint16_t)max_radius;

    /* Draw center rectangle */
    SDL_FRect center = {
        .x = rect->x,
        .y = rect->y + radius,
        .w = rect->w,
        .h = rect->h - 2 * radius
    };
    SDL_RenderFillRect(renderer, &center);

    /* Draw top and bottom rectangles */
    SDL_FRect top = {
        .x = rect->x + radius,
        .y = rect->y,
        .w = rect->w - 2 * radius,
        .h = radius
    };
    SDL_RenderFillRect(renderer, &top);

    SDL_FRect bottom = {
        .x = rect->x + radius,
        .y = rect->y + rect->h - radius,
        .w = rect->w - 2 * radius,
        .h = radius
    };
    SDL_RenderFillRect(renderer, &bottom);

    /* Draw rounded corners using horizontal spans */
    for (int y = 0; y < radius; y++) {
        float dy = radius - y - 0.5f;
        float dx = sqrtf(radius * radius - dy * dy);
        float x_start = radius - dx;
        float x_end = radius + dx;

        /* Top-left */
        SDL_FRect tl = {
            .x = rect->x + x_start,
            .y = rect->y + y,
            .w = x_end - x_start,
            .h = 1
        };
        SDL_RenderFillRect(renderer, &tl);

        /* Top-right */
        SDL_FRect tr = {
            .x = rect->x + rect->w - x_end,
            .y = rect->y + y,
            .w = x_end - x_start,
            .h = 1
        };
        SDL_RenderFillRect(renderer, &tr);

        /* Bottom-left */
        SDL_FRect bl = {
            .x = rect->x + x_start,
            .y = rect->y + rect->h - y - 1,
            .w = x_end - x_start,
            .h = 1
        };
        SDL_RenderFillRect(renderer, &bl);

        /* Bottom-right */
        SDL_FRect br = {
            .x = rect->x + rect->w - x_end,
            .y = rect->y + rect->h - y - 1,
            .w = x_end - x_start,
            .h = 1
        };
        SDL_RenderFillRect(renderer, &br);
    }
}

/* ============================================================================
 * Shadow Rendering
 * ============================================================================ */

void sdl3_render_shadow(
    SDL_Renderer* renderer,
    SDL_FRect* rect,
    int16_t offset_x,
    int16_t offset_y,
    uint16_t blur_radius,
    uint16_t spread_radius,
    uint32_t color,
    bool inset
) {
    if (!renderer || !rect) return;

    /* TODO: Blur not implemented yet - just render solid shadow */
    (void)blur_radius;
    (void)inset;

    SDL_Color c = sdl3_unpack_color(color);
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);

    sdl3_ensure_blend_mode(renderer);

    SDL_FRect shadow = {
        .x = rect->x + offset_x - spread_radius,
        .y = rect->y + offset_y - spread_radius,
        .w = rect->w + 2 * spread_radius,
        .h = rect->h + 2 * spread_radius
    };

    SDL_RenderFillRect(renderer, &shadow);
}
