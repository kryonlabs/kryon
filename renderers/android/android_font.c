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

// stb_truetype for font rendering
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION
#include "../../third_party/stb/stb_rect_pack.h"
#include "../../third_party/stb/stb_truetype.h"

// ============================================================================
// Font Management
// ============================================================================

#define GLYPH_ATLAS_SIZE 512
#define FIRST_CHAR 32
#define CHAR_COUNT 96

bool android_font_init(AndroidRenderer* renderer) {
    if (!renderer) return false;

    LOGI("Initializing font system...\n");

    renderer->font_registry_count = 0;
    strncpy(renderer->default_font_name, "sans-serif", sizeof(renderer->default_font_name) - 1);
    renderer->default_font_size = 16;

    // Initialize all font slots
    for (int i = 0; i < MAX_FONT_REGISTRY; i++) {
        renderer->font_registry[i].loaded = false;
        renderer->font_registry[i].font_info = NULL;
        renderer->font_registry[i].font_buffer = NULL;
        renderer->font_registry[i].atlas_texture = 0;
    }

    LOGI("Font system initialized\n");
    return true;
}

void android_font_cleanup(AndroidRenderer* renderer) {
    if (!renderer) return;

    LOGI("Cleaning up font system...\n");

    // Cleanup font resources
    for (int i = 0; i < MAX_FONT_REGISTRY; i++) {
        if (renderer->font_registry[i].loaded) {
            if (renderer->font_registry[i].font_info) {
                free(renderer->font_registry[i].font_info);
                renderer->font_registry[i].font_info = NULL;
            }
            if (renderer->font_registry[i].font_buffer) {
                free(renderer->font_registry[i].font_buffer);
                renderer->font_registry[i].font_buffer = NULL;
            }
#ifdef __ANDROID__
            if (renderer->font_registry[i].atlas_texture) {
                glDeleteTextures(1, &renderer->font_registry[i].atlas_texture);
                renderer->font_registry[i].atlas_texture = 0;
            }
#endif
            renderer->font_registry[i].loaded = false;
        }
    }

    renderer->font_registry_count = 0;
    LOGI("Font system cleaned up\n");
}

static bool build_glyph_atlas(FontInfo* font, int size) {
    if (!font || !font->font_info) return false;

    stbtt_fontinfo* info = (stbtt_fontinfo*)font->font_info;

    // Calculate scale for desired size
    font->scale = stbtt_ScaleForPixelHeight(info, (float)size);
    font->size = size;

    // Get font metrics
    stbtt_GetFontVMetrics(info, &font->ascent, &font->descent, &font->line_gap);

    // Create glyph atlas bitmap
    int atlas_size = GLYPH_ATLAS_SIZE;
    unsigned char* atlas_bitmap = (unsigned char*)calloc(atlas_size * atlas_size, 1);
    if (!atlas_bitmap) {
        LOGE("Failed to allocate atlas bitmap\n");
        return false;
    }

    // Pack glyphs using stb_rect_pack
    stbrp_context pack_ctx;
    stbrp_node* pack_nodes = (stbrp_node*)malloc(sizeof(stbrp_node) * atlas_size);
    if (!pack_nodes) {
        free(atlas_bitmap);
        return false;
    }

    stbrp_init_target(&pack_ctx, atlas_size, atlas_size, pack_nodes, atlas_size);

    // Render glyphs and pack into atlas
    int pen_x = 1, pen_y = 1;
    for (int ch = FIRST_CHAR; ch < FIRST_CHAR + CHAR_COUNT; ch++) {
        int glyph_index = stbtt_FindGlyphIndex(info, ch);
        if (glyph_index == 0) continue;

        // Get glyph metrics
        int advance, lsb, x0, y0, x1, y1;
        stbtt_GetGlyphHMetrics(info, glyph_index, &advance, &lsb);
        stbtt_GetGlyphBitmapBox(info, glyph_index, font->scale, font->scale,
                                 &x0, &y0, &x1, &y1);

        int glyph_w = x1 - x0;
        int glyph_h = y1 - y0;

        // Check if glyph fits in current row
        if (pen_x + glyph_w + 1 >= atlas_size) {
            pen_x = 1;
            pen_y += size + 1;
        }

        if (pen_y + glyph_h + 1 < atlas_size) {
            // Render glyph into atlas
            stbtt_MakeGlyphBitmap(info,
                                  atlas_bitmap + pen_y * atlas_size + pen_x,
                                  glyph_w, glyph_h, atlas_size,
                                  font->scale, font->scale, glyph_index);

            // Store glyph info
            GlyphInfo* glyph = &font->glyphs[ch];
            glyph->width = glyph_w;
            glyph->height = glyph_h;
            glyph->bearing_x = x0;
            glyph->bearing_y = -y0;
            glyph->advance = (int)(advance * font->scale);
            glyph->u0 = (float)pen_x / atlas_size;
            glyph->v0 = (float)pen_y / atlas_size;
            glyph->u1 = (float)(pen_x + glyph_w) / atlas_size;
            glyph->v1 = (float)(pen_y + glyph_h) / atlas_size;

            pen_x += glyph_w + 1;
        }
    }

#ifdef __ANDROID__
    // Create OpenGL texture for atlas
    glGenTextures(1, &font->atlas_texture);
    glBindTexture(GL_TEXTURE_2D, font->atlas_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlas_size, atlas_size, 0,
                 GL_RED, GL_UNSIGNED_BYTE, atlas_bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
#endif

    font->atlas_width = atlas_size;
    font->atlas_height = atlas_size;

    free(pack_nodes);
    free(atlas_bitmap);

    LOGI("Built glyph atlas for font %s size %d\n", font->name, size);
    return true;
}

FontInfo* android_font_get(AndroidRenderer* renderer, const char* name, int size) {
    if (!renderer || !name) return NULL;

    // Search for existing font with matching size
    for (int i = 0; i < renderer->font_registry_count; i++) {
        if (strcmp(renderer->font_registry[i].name, name) == 0 &&
            renderer->font_registry[i].size == size &&
            renderer->font_registry[i].atlas_texture != 0) {
            return &renderer->font_registry[i];
        }
    }

    // Find font by name and build atlas for requested size
    for (int i = 0; i < renderer->font_registry_count; i++) {
        if (strcmp(renderer->font_registry[i].name, name) == 0) {
            FontInfo* font = &renderer->font_registry[i];
            if (font->atlas_texture == 0 || font->size != size) {
                if (build_glyph_atlas(font, size)) {
                    return font;
                }
            }
        }
    }

    LOGI("Font not found: %s @ %d\n", name, size);
    return NULL;
}

GlyphInfo* android_font_get_glyph(FontInfo* font, char c) {
    if (!font) return NULL;

    int ch = (int)(unsigned char)c;
    if (ch < 0 || ch >= 256) return NULL;

    if (font->glyphs[ch].width > 0) {
        return &font->glyphs[ch];
    }

    return NULL;
}

bool android_renderer_register_font(AndroidRenderer* renderer,
                                     const char* font_name,
                                     const char* font_path) {
    if (!renderer || !font_name || !font_path) return false;

    LOGI("Registering font: %s -> %s\n", font_name, font_path);

    // Find free slot
    if (renderer->font_registry_count >= MAX_FONT_REGISTRY) {
        LOGE("Font registry full\n");
        return false;
    }

    FontInfo* font = &renderer->font_registry[renderer->font_registry_count];

    // Load font file
    FILE* file = fopen(font_path, "rb");
    if (!file) {
        LOGE("Failed to open font file: %s\n", font_path);
        return false;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        LOGE("Invalid font file size\n");
        fclose(file);
        return false;
    }

    // Allocate buffer for font data
    font->font_buffer = malloc(file_size);
    if (!font->font_buffer) {
        LOGE("Failed to allocate font buffer\n");
        fclose(file);
        return false;
    }

    // Read font file
    size_t bytes_read = fread(font->font_buffer, 1, file_size, file);
    fclose(file);

    if (bytes_read != (size_t)file_size) {
        LOGE("Failed to read font file\n");
        free(font->font_buffer);
        font->font_buffer = NULL;
        return false;
    }

    // Initialize stb_truetype
    font->font_info = malloc(sizeof(stbtt_fontinfo));
    if (!font->font_info) {
        LOGE("Failed to allocate font info\n");
        free(font->font_buffer);
        font->font_buffer = NULL;
        return false;
    }

    if (!stbtt_InitFont((stbtt_fontinfo*)font->font_info,
                        (unsigned char*)font->font_buffer, 0)) {
        LOGE("Failed to initialize font\n");
        free(font->font_info);
        free(font->font_buffer);
        font->font_info = NULL;
        font->font_buffer = NULL;
        return false;
    }

    // Store font info
    strncpy(font->name, font_name, sizeof(font->name) - 1);
    strncpy(font->path, font_path, sizeof(font->path) - 1);
    font->size = renderer->default_font_size;
    font->loaded = true;

    renderer->font_registry_count++;

    LOGI("Font registered: %s (%ld bytes)\n", font_name, file_size);
    return true;
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

    // Get font (use default if name is NULL or empty)
    const char* name = (font_name && font_name[0] != '\0') ? font_name : renderer->default_font_name;
    int size = font_size > 0 ? font_size : renderer->default_font_size;

    FontInfo* font = android_font_get(renderer, name, size);
    if (!font) {
        // Fallback to estimate
        size_t len = strlen(text);
        if (width) *width = (float)(len * size * 0.6f);
        if (height) *height = (float)size;
        return false;
    }

    // Measure text width
    float text_width = 0.0f;
    for (const char* p = text; *p; p++) {
        GlyphInfo* glyph = android_font_get_glyph(font, *p);
        if (glyph) {
            text_width += glyph->advance;
        }
    }

    if (width) *width = text_width;
    if (height) *height = (float)size;

    return true;
}

void android_renderer_draw_text(AndroidRenderer* renderer,
                                 const char* text,
                                 float x, float y,
                                 const char* font_name,
                                 int font_size,
                                 uint32_t color) {
    if (!renderer || !text) return;

    // Get font (use default if name is NULL or empty)
    const char* name = (font_name && font_name[0] != '\0') ? font_name : renderer->default_font_name;
    int size = font_size > 0 ? font_size : renderer->default_font_size;

    FontInfo* font = android_font_get(renderer, name, size);
    if (!font || !font->atlas_texture) {
        LOGE("❌ FONT MISSING: Cannot render text '%s' - font '%s' @ %dpx not registered!\n",
             text, name, size);
        LOGE("   Font registry has %d fonts registered\n", renderer->font_registry_count);
        for (int i = 0; i < renderer->font_registry_count; i++) {
            LOGE("   Font[%d]: %s @ %dpx from %s\n",
                 i, renderer->font_registry[i].name,
                 renderer->font_registry[i].size,
                 renderer->font_registry[i].path);
        }
        return;
    }

    // Switch to text shader
    android_shader_use(renderer, SHADER_PROGRAM_TEXT);

#ifdef __ANDROID__
    // Bind font atlas texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font->atlas_texture);

    // Track current texture so it can be rebound during batch flush
    renderer->current_texture = font->atlas_texture;

    // Set texture uniform - check if location is valid first
    GLint u_tex_loc = renderer->shader_programs[SHADER_PROGRAM_TEXT].u_texture;
    if (u_tex_loc >= 0) {
        glUniform1i(u_tex_loc, 0);
    } else {
        LOGE("Text shader u_texture uniform not found! location=%d\n", u_tex_loc);
    }

    // Check GL errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        LOGE("GL error after texture setup: 0x%x (atlas=%u, program=%u, location=%d)\n",
             err, font->atlas_texture,
             renderer->shader_programs[SHADER_PROGRAM_TEXT].program, u_tex_loc);

        // Get current program to verify
        GLint current_program = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
        LOGE("Current active program: %d (expected %u)\n",
             current_program, renderer->shader_programs[SHADER_PROGRAM_TEXT].program);
    }

    LOGI("Text shader setup: atlas_texture=%u, shader_program=%u, u_texture_location=%d\n",
         font->atlas_texture, renderer->shader_programs[SHADER_PROGRAM_TEXT].program, u_tex_loc);
#endif

    // Extract color components
    uint8_t a = (color >> 24) & 0xFF;
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    // Apply global opacity
    a = (uint8_t)(a * renderer->global_opacity);

    // Render each character
    float cursor_x = x;
    float baseline_y = y + (font->ascent * font->scale);

    LOGI("Drawing text '%s' with %d chars, font=%s, size=%d, atlas_texture=%u\n",
         text, (int)strlen(text), name, size, font->atlas_texture);

    for (const char* p = text; *p; p++) {
        GlyphInfo* glyph = android_font_get_glyph(font, *p);
        if (!glyph || glyph->width == 0) {
            LOGE("Glyph for char '%c' (0x%02x) not found or has width 0 (glyph=%p, width=%d)\n",
                 *p, (unsigned char)*p, glyph, glyph ? glyph->width : 0);
            cursor_x += glyph ? glyph->advance : 0;
            continue;
        }

        LOGI("Rendering glyph '%c': pos=(%.1f,%.1f) size=%dx%d\n",
             *p, cursor_x + glyph->bearing_x, baseline_y + glyph->bearing_y,
             glyph->width, glyph->height);

        // Check if we need to flush batch
        if (renderer->vertex_count + 4 > MAX_VERTICES ||
            renderer->index_count + 6 > MAX_INDICES) {
            android_renderer_flush_batch(renderer);
#ifdef __ANDROID__
            // Re-bind texture after flush
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, font->atlas_texture);
            glUniform1i(renderer->shader_programs[SHADER_PROGRAM_TEXT].u_texture, 0);
#endif
        }

        // Calculate glyph position
        float glyph_x = cursor_x + glyph->bearing_x;
        float glyph_y = baseline_y + glyph->bearing_y;
        float glyph_w = (float)glyph->width;
        float glyph_h = (float)glyph->height;

        // Add 4 vertices for the glyph quad
        uint32_t base_index = renderer->vertex_count;

        // Top-left
        renderer->vertices[renderer->vertex_count].x = glyph_x;
        renderer->vertices[renderer->vertex_count].y = glyph_y;
        renderer->vertices[renderer->vertex_count].u = glyph->u0;
        renderer->vertices[renderer->vertex_count].v = glyph->v0;
        renderer->vertices[renderer->vertex_count].r = r;
        renderer->vertices[renderer->vertex_count].g = g;
        renderer->vertices[renderer->vertex_count].b = b;
        renderer->vertices[renderer->vertex_count].a = a;
        renderer->vertex_count++;

        // Top-right
        renderer->vertices[renderer->vertex_count].x = glyph_x + glyph_w;
        renderer->vertices[renderer->vertex_count].y = glyph_y;
        renderer->vertices[renderer->vertex_count].u = glyph->u1;
        renderer->vertices[renderer->vertex_count].v = glyph->v0;
        renderer->vertices[renderer->vertex_count].r = r;
        renderer->vertices[renderer->vertex_count].g = g;
        renderer->vertices[renderer->vertex_count].b = b;
        renderer->vertices[renderer->vertex_count].a = a;
        renderer->vertex_count++;

        // Bottom-right
        renderer->vertices[renderer->vertex_count].x = glyph_x + glyph_w;
        renderer->vertices[renderer->vertex_count].y = glyph_y + glyph_h;
        renderer->vertices[renderer->vertex_count].u = glyph->u1;
        renderer->vertices[renderer->vertex_count].v = glyph->v1;
        renderer->vertices[renderer->vertex_count].r = r;
        renderer->vertices[renderer->vertex_count].g = g;
        renderer->vertices[renderer->vertex_count].b = b;
        renderer->vertices[renderer->vertex_count].a = a;
        renderer->vertex_count++;

        // Bottom-left
        renderer->vertices[renderer->vertex_count].x = glyph_x;
        renderer->vertices[renderer->vertex_count].y = glyph_y + glyph_h;
        renderer->vertices[renderer->vertex_count].u = glyph->u0;
        renderer->vertices[renderer->vertex_count].v = glyph->v1;
        renderer->vertices[renderer->vertex_count].r = r;
        renderer->vertices[renderer->vertex_count].g = g;
        renderer->vertices[renderer->vertex_count].b = b;
        renderer->vertices[renderer->vertex_count].a = a;
        renderer->vertex_count++;

        // Add indices for 2 triangles (6 indices)
        renderer->indices[renderer->index_count++] = base_index + 0;
        renderer->indices[renderer->index_count++] = base_index + 1;
        renderer->indices[renderer->index_count++] = base_index + 2;
        renderer->indices[renderer->index_count++] = base_index + 0;
        renderer->indices[renderer->index_count++] = base_index + 2;
        renderer->indices[renderer->index_count++] = base_index + 3;

        LOGI("Added glyph '%c': vertices=%d, indices=%d, uv=(%.3f,%.3f)-(%.3f,%.3f)\n",
             *p, renderer->vertex_count, renderer->index_count,
             glyph->u0, glyph->v0, glyph->u1, glyph->v1);

        // Advance cursor
        cursor_x += glyph->advance;
    }

    LOGI("Text '%s' rendered with %d vertices, %d indices\n",
         text, renderer->vertex_count, renderer->index_count);
}
