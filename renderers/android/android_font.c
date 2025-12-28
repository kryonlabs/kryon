#include "android_renderer_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "KryonFont"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) fprintf(stdout, "[INFO] " __VA_ARGS__)
#define LOGE(...) fprintf(stderr, "[ERROR] " __VA_ARGS__)
#endif

// ============================================================================
// Font Management (Stub Implementation)
// ============================================================================
// TODO: Integrate FreeType2 for actual font rendering

bool android_font_init(AndroidRenderer* renderer) {
    if (!renderer) return false;

    LOGI("Initializing font system (stub)...\n");

    renderer->font_registry_count = 0;
    strncpy(renderer->default_font_name, "sans-serif", sizeof(renderer->default_font_name) - 1);
    renderer->default_font_size = 16;

    // TODO: Initialize FreeType library
    // FT_Init_FreeType(&renderer->ft_library);

    LOGI("Font system initialized (stub)\n");
    return true;
}

void android_font_cleanup(AndroidRenderer* renderer) {
    if (!renderer) return;

    LOGI("Cleaning up font system (stub)...\n");

    // TODO: Cleanup FreeType resources
    // for (int i = 0; i < renderer->font_registry_count; i++) {
    //     if (renderer->font_registry[i].face) {
    //         FT_Done_Face(renderer->font_registry[i].face);
    //     }
    // }
    // FT_Done_FreeType(renderer->ft_library);

    renderer->font_registry_count = 0;

    LOGI("Font system cleaned up (stub)\n");
}

FontInfo* android_font_get(AndroidRenderer* renderer, const char* name, int size) {
    if (!renderer || !name) return NULL;

    // Search for existing font
    for (int i = 0; i < renderer->font_registry_count; i++) {
        if (strcmp(renderer->font_registry[i].name, name) == 0 &&
            renderer->font_registry[i].size == size) {
            return &renderer->font_registry[i];
        }
    }

    // TODO: Load new font with FreeType
    LOGI("Font not found: %s (stub)\n", name);
    return NULL;
}

GlyphInfo* android_font_get_glyph(FontInfo* font, char c) {
    if (!font) return NULL;

    // TODO: Render glyph with FreeType
    (void)c;
    return NULL;
}

bool android_renderer_register_font(AndroidRenderer* renderer,
                                     const char* font_name,
                                     const char* font_path) {
    if (!renderer || !font_name || !font_path) return false;

    LOGI("Register font (stub): %s -> %s\n", font_name, font_path);

    // TODO: Load font with FreeType
    return false;
}

bool android_renderer_set_default_font(AndroidRenderer* renderer,
                                        const char* font_name,
                                        int size) {
    if (!renderer || !font_name) return false;

    strncpy(renderer->default_font_name, font_name, sizeof(renderer->default_font_name) - 1);
    renderer->default_font_size = size;

    LOGI("Default font set (stub): %s @ %d\n", font_name, size);
    return true;
}

bool android_renderer_measure_text(AndroidRenderer* renderer,
                                    const char* text,
                                    const char* font_name,
                                    int font_size,
                                    float* width,
                                    float* height) {
    if (!renderer || !text) return false;

    // Stub implementation: rough estimate
    size_t len = strlen(text);
    if (width) *width = (float)(len * font_size * 0.6f);
    if (height) *height = (float)font_size;

    (void)font_name;

    return true;
}

void android_renderer_draw_text(AndroidRenderer* renderer,
                                 const char* text,
                                 float x, float y,
                                 const char* font_name,
                                 int font_size,
                                 uint32_t color) {
    if (!renderer || !text) return;

    // TODO: Implement text rendering
    // 1. Get font and glyphs
    // 2. Generate glyph textures
    // 3. Render textured quads

    (void)x;
    (void)y;
    (void)font_name;
    (void)font_size;
    (void)color;
}
