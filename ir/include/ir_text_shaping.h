/**
 * @file ir_text_shaping.h
 * @brief Text shaping and complex script support using HarfBuzz
 *
 * This header provides function declarations for text shaping.
 * Implementations are in ir_text_shaping.c:
 * - For TaijiOS/Inferno: stub implementations that return NULL/false
 * - For other platforms: full implementations using HarfBuzz/FreeType
 *
 * To enable full text shaping on non-TaijiOS platforms:
 * 1. Install HarfBuzz: sudo apt-get install libharfbuzz-dev
 * 2. Install FriBidi: sudo apt-get install libfribidi-dev
 * 3. Rebuild with: make clean && make
 */

#ifndef IR_TEXT_SHAPING_H
#define IR_TEXT_SHAPING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct IRFont IRFont;
typedef struct IRShapedText IRShapedText;
typedef struct IRGlyphInfo IRGlyphInfo;

/**
 * @brief Base direction for bidirectional text
 */
typedef enum {
    IR_BIDI_DIR_LTR = 0,     // Left-to-right
    IR_BIDI_DIR_RTL,         // Right-to-left
    IR_BIDI_DIR_WEAK_LTR,    // Weak LTR (auto-detect, prefer LTR)
    IR_BIDI_DIR_WEAK_RTL,    // Weak RTL (auto-detect, prefer RTL)
    IR_BIDI_DIR_NEUTRAL      // Neutral (auto-detect from content)
} IRBidiDirection;

/**
 * @brief Bidirectional text reordering result
 */
typedef struct IRBidiResult {
    uint32_t* visual_to_logical;  // Visual position -> logical position mapping
    uint32_t* logical_to_visual;  // Logical position -> visual position mapping
    uint8_t* embedding_levels;    // Embedding levels for each character
    IRBidiDirection base_direction; // Resolved base direction
    uint32_t length;              // Number of characters
} IRBidiResult;

/**
 * @brief Information about a single shaped glyph
 */
typedef struct IRGlyphInfo {
    uint32_t glyph_id;         // Glyph index in the font
    uint32_t cluster;          // Cluster index (maps to UTF-8 byte offset)
    float x_advance;           // Horizontal advance
    float y_advance;           // Vertical advance
    float x_offset;            // Horizontal positioning offset
    float y_offset;            // Vertical positioning offset
} IRGlyphInfo;

/**
 * @brief Shaped text containing glyph information
 */
typedef struct IRShapedText {
    IRGlyphInfo* glyphs;       // Array of shaped glyphs
    uint32_t glyph_count;      // Number of glyphs
    float total_width;         // Total width of shaped text
    float total_height;        // Total height of shaped text

    // Private HarfBuzz data (opaque pointer)
    void* hb_buffer;           // HarfBuzz buffer (hb_buffer_t*)
} IRShapedText;

/**
 * @brief Font handle for text shaping
 */
typedef struct IRFont {
    void* hb_font;             // HarfBuzz font (hb_font_t*)
    void* hb_face;             // HarfBuzz face (hb_face_t*)
    void* hb_blob;             // HarfBuzz blob (hb_blob_t*)

    char* font_path;           // Path to font file
    float size;                // Font size in points
    float scale;               // Scale factor for font units to pixels
} IRFont;

/**
 * @brief Text shaping direction
 */
typedef enum {
    IR_SHAPE_DIRECTION_LTR = 0,    // Left-to-right
    IR_SHAPE_DIRECTION_RTL,        // Right-to-left
    IR_SHAPE_DIRECTION_TTB,        // Top-to-bottom
    IR_SHAPE_DIRECTION_BTT,        // Bottom-to-top
    IR_SHAPE_DIRECTION_AUTO        // Auto-detect from script
} IRShapeDirection;

/**
 * @brief Text shaping options
 */
typedef struct IRShapeOptions {
    IRShapeDirection direction;    // Text direction
    const char* language;          // Language code (e.g., "en", "ar", "hi")
    const char* script;            // Script code (e.g., "Latn", "Arab", "Deva")
    const char** features;         // OpenType features (e.g., "liga", "kern")
    uint32_t feature_count;        // Number of features
} IRShapeOptions;

// ============================================================================
// Function Declarations
// ============================================================================
// All functions are implemented in ir_text_shaping.c
// For TaijiOS/Inferno: stub implementations that return NULL/false
// For other platforms: full implementations using HarfBuzz/FreeType

// Bidirectional text functions
bool ir_bidi_available(void);
IRBidiDirection ir_bidi_detect_direction(const char* text, uint32_t length);
IRBidiResult* ir_bidi_reorder(const uint32_t* text, uint32_t length, IRBidiDirection base_dir);
uint32_t* ir_bidi_utf8_to_utf32(const char* utf8_text, uint32_t utf8_length, uint32_t* out_length);
void ir_bidi_result_destroy(IRBidiResult* result);

// Text shaping functions
bool ir_text_shaping_init(void);
void ir_text_shaping_shutdown(void);
bool ir_text_shaping_available(void);

// Font management
IRFont* ir_font_load(const char* font_path, float size);
void ir_font_destroy(IRFont* font);
void ir_font_set_size(IRFont* font, float size);

// Text shaping
IRShapedText* ir_shape_text(IRFont* font, const char* text, uint32_t text_length, const IRShapeOptions* options);
void ir_shaped_text_destroy(IRShapedText* shaped_text);
float ir_shaped_text_get_width(const IRShapedText* shaped_text);
float ir_shaped_text_get_height(const IRShapedText* shaped_text);

#ifdef __cplusplus
}
#endif

#endif // IR_TEXT_SHAPING_H
