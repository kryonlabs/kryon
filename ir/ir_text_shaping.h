/**
 * @file ir_text_shaping.h
 * @brief Text shaping and complex script support using HarfBuzz
 *
 * Provides text shaping capabilities for:
 * - Complex scripts (Arabic, Devanagari, Thai, etc.)
 * - Ligatures and glyph substitution
 * - Proper glyph positioning
 * - Font features (kerning, contextual alternates, etc.)
 */

#ifndef IR_TEXT_SHAPING_H
#define IR_TEXT_SHAPING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct IRFont IRFont;
typedef struct IRShapedText IRShapedText;
typedef struct IRGlyphInfo IRGlyphInfo;

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
// Font Management
// ============================================================================

/**
 * @brief Load a font from a file path
 * @param font_path Path to the font file (TTF/OTF)
 * @param size Font size in points
 * @return Font handle, or NULL on failure
 */
IRFont* ir_font_load(const char* font_path, float size);

/**
 * @brief Destroy a font and free resources
 * @param font Font handle
 */
void ir_font_destroy(IRFont* font);

/**
 * @brief Set font size
 * @param font Font handle
 * @param size New font size in points
 */
void ir_font_set_size(IRFont* font, float size);

// ============================================================================
// Text Shaping
// ============================================================================

/**
 * @brief Shape text using HarfBuzz
 *
 * Converts text to positioned glyphs with proper shaping for complex scripts.
 *
 * @param font Font to use for shaping
 * @param text UTF-8 encoded text
 * @param text_length Length of text in bytes
 * @param options Shaping options (direction, language, script, features)
 * @return Shaped text with glyph information, or NULL on failure
 */
IRShapedText* ir_shape_text(
    IRFont* font,
    const char* text,
    uint32_t text_length,
    const IRShapeOptions* options
);

/**
 * @brief Destroy shaped text and free resources
 * @param shaped_text Shaped text handle
 */
void ir_shaped_text_destroy(IRShapedText* shaped_text);

/**
 * @brief Get the width of shaped text
 * @param shaped_text Shaped text handle
 * @return Width in pixels
 */
float ir_shaped_text_get_width(const IRShapedText* shaped_text);

/**
 * @brief Get the height of shaped text
 * @param shaped_text Shaped text handle
 * @return Height in pixels
 */
float ir_shaped_text_get_height(const IRShapedText* shaped_text);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Initialize the text shaping system
 * @return true on success, false on failure
 */
bool ir_text_shaping_init(void);

/**
 * @brief Shutdown the text shaping system
 */
void ir_text_shaping_shutdown(void);

/**
 * @brief Check if text shaping is available
 * @return true if HarfBuzz is available, false otherwise
 */
bool ir_text_shaping_available(void);

// ============================================================================
// Bidirectional Text (BiDi) Support
// ============================================================================

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
 * @brief Detect the base direction of text
 *
 * Uses the Unicode Bidirectional Algorithm to determine the base direction.
 *
 * @param text UTF-8 encoded text
 * @param length Length in bytes
 * @return Detected base direction
 */
IRBidiDirection ir_bidi_detect_direction(const char* text, uint32_t length);

/**
 * @brief Reorder text for visual display using BiDi algorithm
 *
 * Applies the Unicode Bidirectional Algorithm to reorder text containing
 * mixed LTR and RTL characters for correct visual display.
 *
 * @param text UTF-32 text (use ir_bidi_utf8_to_utf32 to convert)
 * @param length Number of UTF-32 characters
 * @param base_dir Base paragraph direction
 * @return BiDi result with reordering info, or NULL on failure
 */
IRBidiResult* ir_bidi_reorder(
    const uint32_t* text,
    uint32_t length,
    IRBidiDirection base_dir
);

/**
 * @brief Convert UTF-8 to UTF-32 for BiDi processing
 *
 * @param utf8_text UTF-8 encoded text
 * @param utf8_length Length in bytes
 * @param out_length Output: number of UTF-32 characters
 * @return UTF-32 text, or NULL on failure (caller must free)
 */
uint32_t* ir_bidi_utf8_to_utf32(
    const char* utf8_text,
    uint32_t utf8_length,
    uint32_t* out_length
);

/**
 * @brief Destroy BiDi result and free resources
 * @param result BiDi result handle
 */
void ir_bidi_result_destroy(IRBidiResult* result);

/**
 * @brief Check if BiDi support is available
 * @return true if FriBidi is available, false otherwise
 */
bool ir_bidi_available(void);

#ifdef __cplusplus
}
#endif

#endif // IR_TEXT_SHAPING_H
