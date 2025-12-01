/**
 * @file ir_text_shaping.c
 * @brief Text shaping implementation using HarfBuzz
 */

#define _POSIX_C_SOURCE 200809L

#include "ir_text_shaping.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// HarfBuzz headers
#include <hb.h>
#include <hb-ft.h>

// FreeType headers (used by HarfBuzz)
#include <ft2build.h>
#include FT_FREETYPE_H

// Global FreeType library instance
static FT_Library g_ft_library = NULL;
static bool g_shaping_initialized = false;

// ============================================================================
// Initialization and Cleanup
// ============================================================================

bool ir_text_shaping_init(void) {
    if (g_shaping_initialized) {
        return true;
    }

    FT_Error error = FT_Init_FreeType(&g_ft_library);
    if (error) {
        fprintf(stderr, "[IR Text Shaping] Failed to initialize FreeType: %d\n", error);
        return false;
    }

    g_shaping_initialized = true;
    printf("[IR Text Shaping] Initialized HarfBuzz text shaping\n");
    return true;
}

void ir_text_shaping_shutdown(void) {
    if (!g_shaping_initialized) {
        return;
    }

    if (g_ft_library) {
        FT_Done_FreeType(g_ft_library);
        g_ft_library = NULL;
    }

    g_shaping_initialized = false;
}

bool ir_text_shaping_available(void) {
    return g_shaping_initialized;
}

// ============================================================================
// Font Management
// ============================================================================

IRFont* ir_font_load(const char* font_path, float size) {
    if (!g_shaping_initialized) {
        if (!ir_text_shaping_init()) {
            return NULL;
        }
    }

    IRFont* font = (IRFont*)calloc(1, sizeof(IRFont));
    if (!font) {
        return NULL;
    }

    // Load font with FreeType
    FT_Face ft_face;
    FT_Error error = FT_New_Face(g_ft_library, font_path, 0, &ft_face);
    if (error) {
        fprintf(stderr, "[IR Text Shaping] Failed to load font '%s': %d\n", font_path, error);
        free(font);
        return NULL;
    }

    // Set font size (convert points to 26.6 fixed-point)
    error = FT_Set_Char_Size(ft_face, 0, (FT_F26Dot6)(size * 64), 0, 0);
    if (error) {
        fprintf(stderr, "[IR Text Shaping] Failed to set font size: %d\n", error);
        FT_Done_Face(ft_face);
        free(font);
        return NULL;
    }

    // Create HarfBuzz font from FreeType face
    hb_font_t* hb_font = hb_ft_font_create(ft_face, NULL);
    if (!hb_font) {
        fprintf(stderr, "[IR Text Shaping] Failed to create HarfBuzz font\n");
        FT_Done_Face(ft_face);
        free(font);
        return NULL;
    }

    // Store font information
    font->hb_font = hb_font;
    font->hb_face = hb_font_get_face(hb_font);
    font->font_path = strdup(font_path);
    font->size = size;

    // Calculate scale factor (font units to pixels)
    // HarfBuzz uses font units, we need to scale to pixels
    font->scale = size / (float)ft_face->units_per_EM;

    printf("[IR Text Shaping] Loaded font: %s (%.1fpt)\n", font_path, size);
    return font;
}

void ir_font_destroy(IRFont* font) {
    if (!font) {
        return;
    }

    if (font->hb_font) {
        // Get FreeType face before destroying HarfBuzz font
        FT_Face ft_face = hb_ft_font_get_face((hb_font_t*)font->hb_font);

        hb_font_destroy((hb_font_t*)font->hb_font);

        if (ft_face) {
            FT_Done_Face(ft_face);
        }
    }

    if (font->font_path) {
        free(font->font_path);
    }

    free(font);
}

void ir_font_set_size(IRFont* font, float size) {
    if (!font || !font->hb_font) {
        return;
    }

    font->size = size;

    // Get FreeType face and update size
    FT_Face ft_face = hb_ft_font_get_face((hb_font_t*)font->hb_font);
    if (ft_face) {
        FT_Set_Char_Size(ft_face, 0, (FT_F26Dot6)(size * 64), 0, 0);
        font->scale = size / (float)ft_face->units_per_EM;
    }

    // Update HarfBuzz font scale
    hb_ft_font_changed((hb_font_t*)font->hb_font);
}

// ============================================================================
// Text Shaping
// ============================================================================

IRShapedText* ir_shape_text(
    IRFont* font,
    const char* text,
    uint32_t text_length,
    const IRShapeOptions* options
) {
    if (!font || !font->hb_font || !text || text_length == 0) {
        return NULL;
    }

    IRShapedText* shaped = (IRShapedText*)calloc(1, sizeof(IRShapedText));
    if (!shaped) {
        return NULL;
    }

    // Create HarfBuzz buffer
    hb_buffer_t* hb_buf = hb_buffer_create();
    if (!hb_buf) {
        free(shaped);
        return NULL;
    }

    // Add text to buffer
    hb_buffer_add_utf8(hb_buf, text, text_length, 0, text_length);

    // Set buffer properties
    if (options) {
        // Set direction
        hb_direction_t hb_dir = HB_DIRECTION_LTR;
        switch (options->direction) {
            case IR_SHAPE_DIRECTION_LTR: hb_dir = HB_DIRECTION_LTR; break;
            case IR_SHAPE_DIRECTION_RTL: hb_dir = HB_DIRECTION_RTL; break;
            case IR_SHAPE_DIRECTION_TTB: hb_dir = HB_DIRECTION_TTB; break;
            case IR_SHAPE_DIRECTION_BTT: hb_dir = HB_DIRECTION_BTT; break;
            case IR_SHAPE_DIRECTION_AUTO: hb_dir = HB_DIRECTION_INVALID; break;
        }
        hb_buffer_set_direction(hb_buf, hb_dir);

        // Set language
        if (options->language) {
            hb_buffer_set_language(hb_buf, hb_language_from_string(options->language, -1));
        }

        // Set script
        if (options->script) {
            hb_buffer_set_script(hb_buf, hb_script_from_string(options->script, -1));
        }
    } else {
        // Default to LTR
        hb_buffer_set_direction(hb_buf, HB_DIRECTION_LTR);
    }

    // Guess segment properties if not set
    hb_buffer_guess_segment_properties(hb_buf);

    // Prepare font features
    hb_feature_t* features = NULL;
    unsigned int feature_count = 0;

    if (options && options->features && options->feature_count > 0) {
        features = (hb_feature_t*)malloc(sizeof(hb_feature_t) * options->feature_count);
        for (uint32_t i = 0; i < options->feature_count; i++) {
            if (hb_feature_from_string(
                    options->features[i],
                    strlen(options->features[i]),
                    &features[feature_count])) {
                feature_count++;
            }
        }
    }

    // Shape the text
    hb_shape((hb_font_t*)font->hb_font, hb_buf, features, feature_count);

    if (features) {
        free(features);
    }

    // Get glyph information
    unsigned int glyph_count = 0;
    hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(hb_buf, &glyph_count);
    hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(hb_buf, &glyph_count);

    if (glyph_count == 0) {
        hb_buffer_destroy(hb_buf);
        free(shaped);
        return NULL;
    }

    // Allocate glyph array
    shaped->glyphs = (IRGlyphInfo*)malloc(sizeof(IRGlyphInfo) * glyph_count);
    if (!shaped->glyphs) {
        hb_buffer_destroy(hb_buf);
        free(shaped);
        return NULL;
    }

    shaped->glyph_count = glyph_count;

    // Convert HarfBuzz glyphs to our format
    float cursor_x = 0.0f;
    float max_height = 0.0f;

    for (unsigned int i = 0; i < glyph_count; i++) {
        IRGlyphInfo* glyph = &shaped->glyphs[i];

        glyph->glyph_id = glyph_info[i].codepoint;
        glyph->cluster = glyph_info[i].cluster;

        // Convert from 26.6 fixed-point to float pixels
        glyph->x_advance = glyph_pos[i].x_advance / 64.0f;
        glyph->y_advance = glyph_pos[i].y_advance / 64.0f;
        glyph->x_offset = glyph_pos[i].x_offset / 64.0f;
        glyph->y_offset = glyph_pos[i].y_offset / 64.0f;

        cursor_x += glyph->x_advance;

        // Approximate height (this is simplified)
        float glyph_height = font->size;
        if (glyph_height > max_height) {
            max_height = glyph_height;
        }
    }

    shaped->total_width = cursor_x;
    shaped->total_height = max_height;
    shaped->hb_buffer = hb_buf;

    return shaped;
}

void ir_shaped_text_destroy(IRShapedText* shaped_text) {
    if (!shaped_text) {
        return;
    }

    if (shaped_text->glyphs) {
        free(shaped_text->glyphs);
    }

    if (shaped_text->hb_buffer) {
        hb_buffer_destroy((hb_buffer_t*)shaped_text->hb_buffer);
    }

    free(shaped_text);
}

float ir_shaped_text_get_width(const IRShapedText* shaped_text) {
    return shaped_text ? shaped_text->total_width : 0.0f;
}

float ir_shaped_text_get_height(const IRShapedText* shaped_text) {
    return shaped_text ? shaped_text->total_height : 0.0f;
}

// ============================================================================
// Bidirectional Text (BiDi) Support with FriBidi
// ============================================================================

#include <fribidi.h>

bool ir_bidi_available(void) {
    // FriBidi is always available if compiled in
    return true;
}

IRBidiDirection ir_bidi_detect_direction(const char* text, uint32_t length) {
    if (!text || length == 0) {
        return IR_BIDI_DIR_LTR;
    }

    // Convert UTF-8 to UTF-32 for FriBidi
    uint32_t utf32_len = 0;
    uint32_t* utf32_text = ir_bidi_utf8_to_utf32(text, length, &utf32_len);
    if (!utf32_text) {
        return IR_BIDI_DIR_LTR;
    }

    // Get bidirectional types
    FriBidiCharType* bidi_types = (FriBidiCharType*)malloc(sizeof(FriBidiCharType) * utf32_len);
    if (!bidi_types) {
        free(utf32_text);
        return IR_BIDI_DIR_LTR;
    }

    fribidi_get_bidi_types((FriBidiChar*)utf32_text, utf32_len, bidi_types);

    // Get paragraph direction
    FriBidiParType par_type = fribidi_get_par_direction(bidi_types, utf32_len);

    free(utf32_text);
    free(bidi_types);

    // Convert FriBidi direction to our enum
    switch (par_type) {
        case FRIBIDI_PAR_LTR:
        case FRIBIDI_PAR_WLTR:
            return IR_BIDI_DIR_LTR;
        case FRIBIDI_PAR_RTL:
        case FRIBIDI_PAR_WRTL:
            return IR_BIDI_DIR_RTL;
        default:
            return IR_BIDI_DIR_NEUTRAL;
    }
}

IRBidiResult* ir_bidi_reorder(
    const uint32_t* text,
    uint32_t length,
    IRBidiDirection base_dir
) {
    if (!text || length == 0) {
        return NULL;
    }

    IRBidiResult* result = (IRBidiResult*)calloc(1, sizeof(IRBidiResult));
    if (!result) {
        return NULL;
    }

    result->length = length;

    // Allocate arrays
    result->visual_to_logical = (uint32_t*)malloc(sizeof(uint32_t) * length);
    result->logical_to_visual = (uint32_t*)malloc(sizeof(uint32_t) * length);
    result->embedding_levels = (uint8_t*)malloc(sizeof(uint8_t) * length);

    if (!result->visual_to_logical || !result->logical_to_visual || !result->embedding_levels) {
        ir_bidi_result_destroy(result);
        return NULL;
    }

    // Convert base direction
    FriBidiParType par_type;
    switch (base_dir) {
        case IR_BIDI_DIR_LTR:      par_type = FRIBIDI_PAR_LTR; break;
        case IR_BIDI_DIR_RTL:      par_type = FRIBIDI_PAR_RTL; break;
        case IR_BIDI_DIR_WEAK_LTR: par_type = FRIBIDI_PAR_WLTR; break;
        case IR_BIDI_DIR_WEAK_RTL: par_type = FRIBIDI_PAR_WRTL; break;
        default:                   par_type = FRIBIDI_PAR_ON; break;
    }

    // Get bidirectional types
    FriBidiCharType* bidi_types = (FriBidiCharType*)malloc(sizeof(FriBidiCharType) * length);
    if (!bidi_types) {
        ir_bidi_result_destroy(result);
        return NULL;
    }

    fribidi_get_bidi_types((FriBidiChar*)text, length, bidi_types);

    // Get embedding levels
    FriBidiLevel* levels = (FriBidiLevel*)malloc(sizeof(FriBidiLevel) * length);
    if (!levels) {
        free(bidi_types);
        ir_bidi_result_destroy(result);
        return NULL;
    }

    FriBidiLevel max_level = fribidi_get_par_embedding_levels(
        bidi_types,
        length,
        &par_type,
        levels
    );

    if (max_level == 0) {
        // Failed to get embedding levels
        free(bidi_types);
        free(levels);
        ir_bidi_result_destroy(result);
        return NULL;
    }

    // Copy embedding levels
    for (uint32_t i = 0; i < length; i++) {
        result->embedding_levels[i] = (uint8_t)levels[i];
    }

    // Initialize position maps
    FriBidiStrIndex* position_map = (FriBidiStrIndex*)malloc(sizeof(FriBidiStrIndex) * length);

    if (!position_map) {
        free(bidi_types);
        free(levels);
        ir_bidi_result_destroy(result);
        return NULL;
    }

    // Initialize map to identity [0, 1, 2, 3, ...]
    for (uint32_t i = 0; i < length; i++) {
        position_map[i] = i;
    }

    // Reorder the position map according to BiDi levels
    FriBidiLevel max_level_reorder = fribidi_reorder_line(
        0,              // Flags
        bidi_types,
        length,
        0,              // Start of line
        par_type,
        levels,
        NULL,           // Don't need visual string
        position_map    // Position map (gets reordered)
    );

    if (max_level_reorder == 0) {
        // Reordering failed
        free(bidi_types);
        free(levels);
        free(position_map);
        ir_bidi_result_destroy(result);
        return NULL;
    }

    // position_map is now visual-to-logical (visual position i maps to logical position position_map[i])
    // Calculate both mappings
    for (uint32_t i = 0; i < length; i++) {
        uint32_t logical_pos = (uint32_t)position_map[i];
        result->visual_to_logical[i] = logical_pos;
        result->logical_to_visual[logical_pos] = i;
    }

    // Store resolved direction
    if (par_type == FRIBIDI_PAR_LTR) {
        result->base_direction = IR_BIDI_DIR_LTR;
    } else if (par_type == FRIBIDI_PAR_RTL) {
        result->base_direction = IR_BIDI_DIR_RTL;
    } else {
        result->base_direction = IR_BIDI_DIR_NEUTRAL;
    }

    // Cleanup
    free(bidi_types);
    free(levels);
    free(position_map);

    return result;
}

uint32_t* ir_bidi_utf8_to_utf32(
    const char* utf8_text,
    uint32_t utf8_length,
    uint32_t* out_length
) {
    if (!utf8_text || utf8_length == 0 || !out_length) {
        return NULL;
    }

    // Allocate buffer (worst case: same number of UTF-32 chars as UTF-8 bytes)
    uint32_t* utf32_text = (uint32_t*)malloc(sizeof(uint32_t) * utf8_length);
    if (!utf32_text) {
        return NULL;
    }

    // Convert using FriBidi's charset conversion
    FriBidiStrIndex utf32_len = fribidi_charset_to_unicode(
        FRIBIDI_CHAR_SET_UTF8,
        utf8_text,
        utf8_length,
        (FriBidiChar*)utf32_text
    );

    if (utf32_len <= 0) {
        free(utf32_text);
        return NULL;
    }

    *out_length = (uint32_t)utf32_len;
    return utf32_text;
}

void ir_bidi_result_destroy(IRBidiResult* result) {
    if (!result) {
        return;
    }

    if (result->visual_to_logical) {
        free(result->visual_to_logical);
    }

    if (result->logical_to_visual) {
        free(result->logical_to_visual);
    }

    if (result->embedding_levels) {
        free(result->embedding_levels);
    }

    free(result);
}
