/**
 * Desktop Platform - Platform Abstraction Layer Implementation
 *
 * This file implements the backend registry and platform utilities
 */

#include "desktop_platform.h"
#include "../../ir/ir_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Backend Registry
// ============================================================================

// Global backend registry (one entry per backend type)
static DesktopRendererOps* g_backends[7] = {NULL};  // 7 = max backend types

void desktop_register_backend(DesktopBackendType type, DesktopRendererOps* ops) {
    if (type < 0 || type >= 7) {
        IR_LOG_ERROR("DESKTOP", "Invalid backend type: %d", type);
        return;
    }

    // Only register Raylib backend if explicitly enabled via environment variable
    if (type == DESKTOP_BACKEND_RAYLIB && !getenv("KRYON_ENABLE_RAYLIB")) {
        return;
    }

    if (g_backends[type] != NULL) {
        IR_LOG_WARN("DESKTOP", "Backend %d already registered, overwriting", type);
    }

    g_backends[type] = ops;

    const char* backend_names[] = {"SDL3", "Raylib", "GLFW", "Win32", "Cocoa", "X11", "Plan9"};
    IR_LOG_INFO("DESKTOP", "Registered backend: %s", backend_names[type]);
}

DesktopRendererOps* desktop_get_backend_ops(DesktopBackendType type) {
    if (type < 0 || type >= 7) {
        IR_LOG_ERROR("DESKTOP", "Invalid backend type: %d", type);
        return NULL;
    }

    return g_backends[type];
}

// ============================================================================
// Font Registry (Shared Across Backends)
// ============================================================================

// Global font registry
static RegisteredFont g_font_registry[32];
static int g_font_registry_count = 0;

void desktop_register_font(const char* name, const char* path) {
    if (!name || !path) {
        IR_LOG_ERROR("DESKTOP", "Invalid font registration: name=%p, path=%p",
                (void*)name, (void*)path);
        return;
    }

    if (g_font_registry_count >= 32) {
        IR_LOG_ERROR("DESKTOP", "Font registry full (max 32 fonts)");
        return;
    }

    // Check if already registered
    for (int i = 0; i < g_font_registry_count; i++) {
        if (strcmp(g_font_registry[i].name, name) == 0) {
            // Update path if different
            if (strcmp(g_font_registry[i].path, path) != 0) {
                strncpy(g_font_registry[i].path, path, sizeof(g_font_registry[i].path) - 1);
                g_font_registry[i].path[sizeof(g_font_registry[i].path) - 1] = '\0';
            }
            return;
        }
    }

    // Add new font
    RegisteredFont* font = &g_font_registry[g_font_registry_count++];
    strncpy(font->name, name, sizeof(font->name) - 1);
    font->name[sizeof(font->name) - 1] = '\0';
    strncpy(font->path, path, sizeof(font->path) - 1);
    font->path[sizeof(font->path) - 1] = '\0';

    IR_LOG_INFO("DESKTOP", "Registered font: %s -> %s", name, path);
}

const char* desktop_find_font_path(const char* name) {
    if (!name) return NULL;

    for (int i = 0; i < g_font_registry_count; i++) {
        if (strcmp(g_font_registry[i].name, name) == 0) {
            return g_font_registry[i].path;
        }
    }

    return NULL;
}

int desktop_get_font_registry_count(void) {
    return g_font_registry_count;
}

const RegisteredFont* desktop_get_registered_font(int index) {
    if (index < 0 || index >= g_font_registry_count) {
        return NULL;
    }
    return &g_font_registry[index];
}

// ============================================================================
// Platform Utilities
// ============================================================================

float desktop_dimension_to_pixels(IRDimension dimension, float container_size) {
    switch (dimension.type) {
        case IR_DIMENSION_PX:
            return dimension.value;

        case IR_DIMENSION_PERCENT:
            return (dimension.value / 100.0f) * container_size;

        case IR_DIMENSION_AUTO:
            return 0.0f;

        case IR_DIMENSION_FLEX:
            return dimension.value;

        default:
            return 0.0f;
    }
}

uint32_t desktop_color_to_rgba32(IRColor color) {
    // Extract RGBA from IRColor
    uint8_t r, g, b, a;

    switch (color.type) {
        case IR_COLOR_SOLID:
            r = color.data.r;
            g = color.data.g;
            b = color.data.b;
            a = color.data.a;
            break;

        case IR_COLOR_TRANSPARENT:
            return 0x00000000;

        case IR_COLOR_VAR_REF:
            // Style variable - would need to resolve
            // For now, return transparent black
            return 0x00000000;

        case IR_COLOR_GRADIENT:
            // Gradient - would need special handling
            // For now, return first stop color if available
            return 0x00000000;

        default:
            return 0x00000000;
    }

    // Pack into RGBA32 format (0xRRGGBBAA)
    return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)a;
}

// ============================================================================
// Text Utilities
// ============================================================================

void desktop_text_wrap_result_free(DesktopTextWrapResult* result) {
    if (!result) return;

    if (result->lines) {
        for (int i = 0; i < result->line_count; i++) {
            free(result->lines[i]);
        }
        free(result->lines);
    }

    memset(result, 0, sizeof(DesktopTextWrapResult));
}
