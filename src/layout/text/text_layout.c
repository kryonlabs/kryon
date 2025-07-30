/**
 * @file text_layout.c
 * @brief Kryon Text Layout Engine Implementation
 */

#include "internal/layout.h"
#include "internal/graphics.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

// =============================================================================
// TEXT LAYOUT STRUCTURES
// =============================================================================

typedef struct {
    size_t start;        // Character index in text
    size_t length;       // Length in characters
    float width;         // Width in pixels
    float height;        // Height in pixels
    float baseline;      // Baseline position
    bool is_line_break;  // True if this word ends with a line break
    bool is_whitespace;  // True if this is whitespace
} KryonTextWord;

typedef struct {
    KryonTextWord* words;
    size_t word_count;
    float width;         // Total line width
    float height;        // Line height
    float baseline;      // Baseline position
    float x_offset;      // X offset for alignment
    float y_offset;      // Y offset from top
} KryonTextLine;

typedef struct {
    const char* text;
    size_t text_length;
    KryonFont* font;
    
    // Style properties
    float font_size;
    float line_height;
    KryonTextAlign text_align;
    KryonTextWrap text_wrap;
    float letter_spacing;
    float word_spacing;
    
    // Layout constraints
    float max_width;
    float max_height;
    
    // Output
    KryonTextLine* lines;
    size_t line_count;
    size_t line_capacity;
    
    // Computed dimensions
    float total_width;
    float total_height;
    float content_width;
    float content_height;
    
} KryonTextLayoutContext;

// =============================================================================
// TEXT MEASUREMENT UTILITIES
// =============================================================================

static float measure_text_width(const char* text, size_t length, KryonFont* font, float letter_spacing) {
    if (!text || length == 0 || !font) return 0.0f;
    
    // Create null-terminated substring for measurement
    char* temp_text = kryon_alloc(length + 1);
    if (!temp_text) return 0.0f;
    
    strncpy(temp_text, text, length);
    temp_text[length] = '\0';
    
    KryonTextMetrics metrics = kryon_font_measure_text(font, temp_text);
    kryon_free(temp_text);
    
    // Add letter spacing
    float total_width = metrics.width;
    if (length > 1 && letter_spacing != 0.0f) {
        total_width += letter_spacing * (length - 1);
    }
    
    return total_width;
}

static float get_line_height(KryonFont* font, float line_height_multiplier) {
    if (!font) return 16.0f; // Default line height
    
    float base_height = font->line_height;
    return base_height * line_height_multiplier;
}

// =============================================================================
// WORD BREAKING
// =============================================================================

static bool is_word_break_character(char c) {
    return isspace(c) || c == '-' || c == '\0';
}

static bool is_line_break_character(char c) {
    return c == '\n' || c == '\r';
}

static void tokenize_text(KryonTextLayoutContext* ctx) {
    if (!ctx->text || ctx->text_length == 0) return;
    
    // Estimate word count
    size_t estimated_words = ctx->text_length / 5 + 1; // Rough estimate
    KryonTextWord* words = kryon_alloc(sizeof(KryonTextWord) * estimated_words);
    if (!words) return;
    
    size_t word_count = 0;
    size_t word_capacity = estimated_words;
    
    size_t i = 0;
    while (i < ctx->text_length) {
        // Expand word array if needed
        if (word_count >= word_capacity) {
            word_capacity *= 2;
            KryonTextWord* new_words = kryon_alloc(sizeof(KryonTextWord) * word_capacity);
            if (!new_words) break;
            
            memcpy(new_words, words, sizeof(KryonTextWord) * word_count);
            kryon_free(words);
            words = new_words;
        }
        
        KryonTextWord* word = &words[word_count];
        memset(word, 0, sizeof(KryonTextWord));
        
        word->start = i;
        
        // Skip leading whitespace
        while (i < ctx->text_length && isspace(ctx->text[i]) && !is_line_break_character(ctx->text[i])) {
            i++;
        }
        
        if (i >= ctx->text_length) break;
        
        // Handle line breaks
        if (is_line_break_character(ctx->text[i])) {
            word->start = i;
            word->length = 1;
            word->is_line_break = true;
            word->is_whitespace = true;
            word->width = 0.0f;
            word->height = get_line_height(ctx->font, ctx->line_height);
            i++;
            word_count++;
            continue;
        }
        
        // Find end of word
        size_t word_start = i;
        while (i < ctx->text_length && !is_word_break_character(ctx->text[i])) {
            i++;
        }
        
        word->start = word_start;
        word->length = i - word_start;
        word->is_whitespace = false;
        
        // Measure word
        word->width = measure_text_width(&ctx->text[word->start], word->length, ctx->font, ctx->letter_spacing);
        word->height = get_line_height(ctx->font, ctx->line_height);
        word->baseline = ctx->font->size * 0.8f; // Approximate baseline
        
        word_count++;
        
        // Handle trailing whitespace as separate word
        if (i < ctx->text_length && isspace(ctx->text[i]) && !is_line_break_character(ctx->text[i])) {
            if (word_count >= word_capacity) {
                word_capacity *= 2;
                KryonTextWord* new_words = kryon_alloc(sizeof(KryonTextWord) * word_capacity);
                if (!new_words) break;
                
                memcpy(new_words, words, sizeof(KryonTextWord) * word_count);
                kryon_free(words);
                words = new_words;
            }
            
            KryonTextWord* space_word = &words[word_count];
            memset(space_word, 0, sizeof(KryonTextWord));
            
            size_t space_start = i;
            while (i < ctx->text_length && isspace(ctx->text[i]) && !is_line_break_character(ctx->text[i])) {
                i++;
            }
            
            space_word->start = space_start;
            space_word->length = i - space_start;
            space_word->is_whitespace = true;
            space_word->width = measure_text_width(&ctx->text[space_word->start], space_word->length, ctx->font, 0.0f);
            if (ctx->word_spacing != 0.0f) {
                space_word->width += ctx->word_spacing;
            }
            space_word->height = get_line_height(ctx->font, ctx->line_height);
            
            word_count++;
        }
    }
    
    // Store words in context
    ctx->words = words;
    ctx->word_count = word_count;
}

// =============================================================================
// LINE BREAKING
// =============================================================================

static KryonTextLine* add_line(KryonTextLayoutContext* ctx) {
    if (ctx->line_count >= ctx->line_capacity) {
        size_t new_capacity = ctx->line_capacity == 0 ? 4 : ctx->line_capacity * 2;
        KryonTextLine* new_lines = kryon_alloc(sizeof(KryonTextLine) * new_capacity);
        if (!new_lines) return NULL;
        
        if (ctx->lines) {
            memcpy(new_lines, ctx->lines, sizeof(KryonTextLine) * ctx->line_count);
            kryon_free(ctx->lines);
        }
        
        ctx->lines = new_lines;
        ctx->line_capacity = new_capacity;
    }
    
    KryonTextLine* line = &ctx->lines[ctx->line_count];
    memset(line, 0, sizeof(KryonTextLine));
    ctx->line_count++;
    
    return line;
}

static void break_lines(KryynTextLayoutContext* ctx) {
    if (!ctx->words || ctx->word_count == 0) return;
    
    KryonTextLine* current_line = add_line(ctx);
    if (!current_line) return;
    
    // Allocate words for the first line
    size_t words_per_line_estimate = ctx->word_count / 4 + 1;
    current_line->words = kryon_alloc(sizeof(KryonTextWord) * words_per_line_estimate);
    if (!current_line->words) return;
    
    size_t current_line_capacity = words_per_line_estimate;
    float current_line_width = 0.0f;
    float current_line_height = 0.0f;
    
    for (size_t i = 0; i < ctx->word_count; i++) {
        KryonTextWord* word = &ctx->words[i];
        
        // Handle forced line breaks
        if (word->is_line_break) {
            // Finish current line
            current_line->width = current_line_width;
            current_line->height = fmaxf(current_line_height, word->height);
            
            // Start new line
            current_line = add_line(ctx);
            if (!current_line) break;
            
            current_line->words = kryon_alloc(sizeof(KryonTextWord) * words_per_line_estimate);
            if (!current_line->words) break;
            
            current_line_capacity = words_per_line_estimate;
            current_line_width = 0.0f;
            current_line_height = 0.0f;
            continue;
        }
        
        // Check if word fits on current line
        float word_width = word->width;
        bool fits_on_line = (current_line_width + word_width <= ctx->max_width) || current_line->word_count == 0;
        
        if (!fits_on_line && ctx->text_wrap == KRYON_TEXT_WRAP_WRAP) {
            // Word doesn't fit, start new line
            current_line->width = current_line_width;
            current_line->height = current_line_height;
            
            current_line = add_line(ctx);
            if (!current_line) break;
            
            current_line->words = kryon_alloc(sizeof(KryonTextWord) * words_per_line_estimate);
            if (!current_line->words) break;
            
            current_line_capacity = words_per_line_estimate;
            current_line_width = 0.0f;
            current_line_height = 0.0f;
        }
        
        // Add word to current line
        if (current_line->word_count >= current_line_capacity) {
            current_line_capacity *= 2;
            KryonTextWord* new_words = kryon_alloc(sizeof(KryonTextWord) * current_line_capacity);
            if (!new_words) break;
            
            memcpy(new_words, current_line->words, sizeof(KryonTextWord) * current_line->word_count);
            kryon_free(current_line->words);
            current_line->words = new_words;
        }
        
        current_line->words[current_line->word_count] = *word;
        current_line->word_count++;
        
        current_line_width += word_width;
        current_line_height = fmaxf(current_line_height, word->height);
    }
    
    // Finish last line
    if (current_line) {
        current_line->width = current_line_width;
        current_line->height = current_line_height;
    }
}

// =============================================================================
// TEXT ALIGNMENT
// =============================================================================

static void align_lines(KryonTextLayoutContext* ctx) {
    for (size_t i = 0; i < ctx->line_count; i++) {
        KryonTextLine* line = &ctx->lines[i];
        
        switch (ctx->text_align) {
            case KRYON_TEXT_ALIGN_LEFT:
                line->x_offset = 0.0f;
                break;
                
            case KRYON_TEXT_ALIGN_CENTER:
                line->x_offset = (ctx->max_width - line->width) / 2.0f;
                break;
                
            case KRYON_TEXT_ALIGN_RIGHT:
                line->x_offset = ctx->max_width - line->width;
                break;
                
            case KRYON_TEXT_ALIGN_JUSTIFY:
                line->x_offset = 0.0f;
                // TODO: Implement text justification
                break;
        }
        
        // Calculate Y offset
        if (i == 0) {
            line->y_offset = 0.0f;
        } else {
            line->y_offset = ctx->lines[i - 1].y_offset + ctx->lines[i - 1].height;
        }
    }
}

// =============================================================================
// LAYOUT COMPUTATION
// =============================================================================

static void compute_layout_dimensions(KryonTextLayoutContext* ctx) {
    ctx->total_width = 0.0f;
    ctx->total_height = 0.0f;
    ctx->content_width = 0.0f;
    ctx->content_height = 0.0f;
    
    for (size_t i = 0; i < ctx->line_count; i++) {
        KryonTextLine* line = &ctx->lines[i];
        
        ctx->content_width = fmaxf(ctx->content_width, line->width);
        ctx->content_height += line->height;
        
        ctx->total_width = fmaxf(ctx->total_width, line->x_offset + line->width);
        ctx->total_height += line->height;
    }
    
    // Ensure minimum dimensions
    if (ctx->total_width < ctx->content_width) {
        ctx->total_width = ctx->content_width;
    }
    if (ctx->total_height < ctx->content_height) {
        ctx->total_height = ctx->content_height;
    }
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool krylon_layout_text(const char* text, KryonFont* font, float max_width, float max_height,
                        const KryonTextStyle* style, KryonTextLayoutResult* result) {
    if (!text || !font || !result) return false;
    
    // Initialize context
    KryonTextLayoutContext ctx = {0};
    ctx.text = text;
    ctx.text_length = strlen(text);
    ctx.font = font;
    ctx.max_width = max_width > 0 ? max_width : FLT_MAX;
    ctx.max_height = max_height > 0 ? max_height : FLT_MAX;
    
    // Apply style or use defaults
    if (style) {
        ctx.font_size = style->font_size;
        ctx.line_height = style->line_height;
        ctx.text_align = style->text_align;
        ctx.text_wrap = style->text_wrap;
        ctx.letter_spacing = style->letter_spacing;
        ctx.word_spacing = style->word_spacing;
    } else {
        ctx.font_size = font->size;
        ctx.line_height = 1.2f;
        ctx.text_align = KRYON_TEXT_ALIGN_LEFT;
        ctx.text_wrap = KRYON_TEXT_WRAP_WRAP;
        ctx.letter_spacing = 0.0f;
        ctx.word_spacing = 0.0f;
    }
    
    // Step 1: Tokenize text into words
    tokenize_text(&ctx);
    
    // Step 2: Break words into lines
    break_lines(&ctx);
    
    // Step 3: Align lines
    align_lines(&ctx);
    
    // Step 4: Compute final dimensions
    compute_layout_dimensions(&ctx);
    
    // Fill result
    result->width = ctx.total_width;
    result->height = ctx.total_height;
    result->content_width = ctx.content_width;
    result->content_height = ctx.content_height;
    result->line_count = ctx.line_count;
    result->baseline = ctx.line_count > 0 ? ctx.lines[0].height * 0.8f : 0.0f;
    
    // Store layout data for rendering (simplified)
    result->layout_data = NULL; // Would store line/word layout info for rendering
    
    // Clean up
    kryon_free(ctx.words);
    for (size_t i = 0; i < ctx.line_count; i++) {
        kryon_free(ctx.lines[i].words);
    }
    kryon_free(ctx.lines);
    
    return true;
}

float kryon_text_measure_width(const char* text, KryonFont* font, float letter_spacing) {
    if (!text || !font) return 0.0f;
    
    return measure_text_width(text, strlen(text), font, letter_spacing);
}

float kryon_text_measure_height(KryonFont* font, float line_height_multiplier) {
    if (!font) return 16.0f;
    
    return get_line_height(font, line_height_multiplier);
}

size_t kryon_text_find_break_position(const char* text, size_t start, size_t max_length, 
                                     KryonFont* font, float max_width, float letter_spacing) {
    if (!text || !font || max_length == 0) return start;
    
    size_t best_break = start;
    float current_width = 0.0f;
    
    for (size_t i = start; i < start + max_length && text[i] != '\0'; i++) {
        char c = text[i];
        float char_width = measure_text_width(&c, 1, font, 0.0f);
        
        if (current_width + char_width > max_width) {
            break;
        }
        
        current_width += char_width;
        if (letter_spacing != 0.0ف && i > start) {
            current_width += letter_spacing;
        }
        
        if (is_word_break_character(c)) {
            best_break = i;
        }
        
        if (current_width + char_width <= max_width) {
            if (best_break == start) { // No word break found yet
                best_break = i + 1;
            }
        }
    }
    
    return best_break;
}

bool kryon_text_layout_fits(const char* text, KryonFont* font, float width, float height,
                           const KryonTextStyle* style) {
    if (!text || !font) return false;
    
    KryonTextLayoutResult result;
    if (!kryon_layout_text(text, font, width, height, style, &result)) {
        return false;
    }
    
    return result.width <= width && result.height <= height;
}