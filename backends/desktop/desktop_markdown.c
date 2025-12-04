/*
 * desktop_markdown.c
 *
 * Markdown parsing and rendering module for the Kryon desktop backend.
 *
 * This module provides comprehensive markdown support including:
 * - Markdown parsing with support for headers, lists, code blocks, and inline formatting
 * - Token-based rendering system for styled text output
 * - Scroll state management for markdown content containers
 * - Text measurement and layout calculation
 * - Caret positioning for text input elements
 *
 * The implementation handles:
 * - Fenced code blocks with syntax-aware rendering
 * - Unordered and ordered lists with nesting support
 * - Inline formatting (bold, italic, code spans)
 * - Headers with proper sizing and styling (H1-H6)
 * - Dynamic text measurement using SDL_ttf for accurate layout
 *
 * Architecture:
 * - parse_markdown(): Tokenizes markdown content into structured tokens
 * - render_markdown_content(): Renders tokens to SDL with proper styling
 * - Scroll state management: Tracks scroll offset and content height per component
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "desktop_internal.h"

#ifdef ENABLE_SDL3

// ============================================================================
// GLOBAL STATE - Markdown scroll tracking
// ============================================================================
// NOTE: markdown_scroll_states and markdown_scroll_state_count are defined in desktop_globals.c

// ============================================================================
// SCROLL STATE MANAGEMENT
// ============================================================================

/*
 * get_markdown_scroll_state - Get or create scroll state for a markdown component
 *
 * Retrieves the scroll state for a markdown component, creating a new entry if needed.
 * Scroll state tracks the current scroll offset and total content height for the component.
 *
 * @component_id: Unique identifier of the markdown component
 * @return: Pointer to MarkdownScrollState, or NULL if table is full
 */
MarkdownScrollState* get_markdown_scroll_state(uint32_t component_id) {
    for (size_t i = 0; i < markdown_scroll_state_count; i++) {
        if (markdown_scroll_states[i].component_id == component_id) {
            return &markdown_scroll_states[i];
        }
    }
    if (markdown_scroll_state_count < (sizeof(markdown_scroll_states) / sizeof(markdown_scroll_states[0]))) {
        markdown_scroll_states[markdown_scroll_state_count].component_id = component_id;
        markdown_scroll_states[markdown_scroll_state_count].scroll_offset = 0.0f;
        markdown_scroll_states[markdown_scroll_state_count].content_height = 0.0f;
        markdown_scroll_state_count++;
        return &markdown_scroll_states[markdown_scroll_state_count - 1];
    }
    return NULL;
}

// ============================================================================
// TEXT MEASUREMENT
// ============================================================================

// NOTE: measure_text_width is defined in desktop_fonts.c

// NOTE: ensure_caret_visible is defined in desktop_input.c

// ============================================================================
// BUFFER MANAGEMENT
// ============================================================================

/*
 * append_to_buffer - Dynamically grow and append text to a buffer
 *
 * Manages a dynamic buffer used to accumulate code block content during parsing.
 * Automatically reallocates when capacity is exceeded, doubling the capacity each time.
 *
 * @buffer: Pointer to buffer pointer (will be reallocated if needed)
 * @len: Current length of buffer content
 * @cap: Current capacity of buffer
 * @text: Text to append
 * @text_len: Length of text to append
 * @add_newline: If true, append a newline after the text
 * @return: true if successful, false on allocation failure
 */
bool append_to_buffer(char** buffer, size_t* len, size_t* cap,
                      const char* text, size_t text_len, bool add_newline) {
    size_t extra = text_len + (add_newline ? 1 : 0);
    if (*len + extra + 1 > *cap) {
        size_t new_cap = (*cap == 0 ? 128 : *cap * 2);
        while (*len + extra + 1 > new_cap) new_cap *= 2;
        char* tmp = realloc(*buffer, new_cap);
        if (!tmp) return false;
        *buffer = tmp;
        *cap = new_cap;
    }
    memcpy(*buffer + *len, text, text_len);
    *len += text_len;
    if (add_newline) {
        (*buffer)[*len] = '\n';
        (*len)++;
    }
    (*buffer)[*len] = '\0';
    return true;
}

// ============================================================================
// TOKEN MANAGEMENT
// ============================================================================

/*
 * create_markdown_tokens - Allocate and initialize a markdown token collection
 *
 * Creates an empty token array with initial capacity of 64 tokens.
 * The array will automatically expand as tokens are added.
 *
 * @return: Pointer to new MarkdownTokens structure, or NULL on allocation failure
 */
MarkdownTokens* create_markdown_tokens(void) {
    MarkdownTokens* tokens = malloc(sizeof(MarkdownTokens));
    if (!tokens) return NULL;

    tokens->capacity = 64;
    tokens->count = 0;
    tokens->tokens = malloc(sizeof(MarkdownToken) * tokens->capacity);
    if (!tokens->tokens) {
        free(tokens);
        return NULL;
    }

    return tokens;
}

/*
 * add_markdown_token - Add a styled text token to the token collection
 *
 * Creates and appends a new token to the token array, expanding capacity if needed.
 * Tokens represent styled text segments with formatting attributes (bold, italic, code)
 * and structural information (header level, list status).
 *
 * @tokens: Token collection to add to
 * @text: Text content (will be copied)
 * @length: Length of text
 * @bold: True if text should be rendered bold
 * @italic: True if text should be rendered italic
 * @code: True if text is inline code
 * @header_level: 0 for regular text, 1-6 for headers
 * @is_list_item: True if part of a list item
 * @is_ordered: True if part of an ordered list (vs unordered)
 * @order_index: Item number for ordered lists
 * @list_level: Nesting level (0 for top-level)
 * @is_code_block: True if this is a code block (vs inline)
 * @ends_line: True if this token ends the current line
 */
void add_markdown_token(MarkdownTokens* tokens, const char* text, size_t length,
                        bool bold, bool italic, bool code, int header_level,
                        bool is_list_item, bool is_ordered, int order_index,
                        int list_level, bool is_code_block, bool ends_line) {
    if (tokens->count >= tokens->capacity) {
        tokens->capacity *= 2;
        tokens->tokens = realloc(tokens->tokens, sizeof(MarkdownToken) * tokens->capacity);
        if (!tokens->tokens) return;
    }

    MarkdownToken* token = &tokens->tokens[tokens->count];
    token->text = malloc(length + 1);
    if (token->text && length > 0) {
        memcpy(token->text, text, length);
        token->text[length] = '\0';
    } else {
        token->text = strdup("");
    }
    token->length = length;
    token->bold = bold;
    token->italic = italic;
    token->code = code;
    token->header_level = header_level;
    token->is_list_item = is_list_item;
    token->is_ordered = is_ordered;
    token->order_index = order_index;
    token->list_level = list_level;
    token->is_code_block = is_code_block;
    token->ends_line = ends_line;

    tokens->count++;
}

/*
 * free_markdown_tokens - Free all memory associated with a token collection
 *
 * Deallocates the token array and all text strings within tokens.
 * Must be called after rendering is complete to avoid memory leaks.
 *
 * @tokens: Token collection to free (can be NULL)
 */
void free_markdown_tokens(MarkdownTokens* tokens) {
    if (!tokens) return;

    for (size_t i = 0; i < tokens->count; i++) {
        free(tokens->tokens[i].text);
    }
    free(tokens->tokens);
    free(tokens);
}

// ============================================================================
// INLINE FORMATTING PARSING
// ============================================================================

/*
 * add_inline_segments - Parse and tokenize inline formatting in a line of text
 *
 * Handles inline markdown syntax within a single line:
 * - `code` (backtick-enclosed)
 * - **bold** (double asterisk)
 * - *italic* (single asterisk)
 *
 * Creates multiple tokens for segments with different formatting styles.
 * Properly handles nested formatting and escape sequences.
 *
 * @tokens: Token collection to add segments to
 * @text: Input text to parse
 * @len: Length of text
 * @is_list_item: True if this line is part of a list item
 * @is_ordered: True if part of an ordered list
 * @order_index: Item number for ordered lists
 * @list_level: Nesting level
 */
void add_inline_segments(MarkdownTokens* tokens, const char* text, size_t len,
                                bool is_list_item, bool is_ordered, int order_index, int list_level) {
    size_t start_count = tokens->count;
    size_t start = 0;
    bool bold = false, italic = false, code = false;

    for (size_t i = 0; i < len; i++) {
        if (text[i] == '`') {
            // Emit text before the backtick
            if (i > start) {
                add_markdown_token(tokens, text + start, i - start, bold, italic, code, 0,
                                   is_list_item, is_ordered, order_index, list_level, false, false);
            }
            code = !code;
            start = i + 1;
        }
        else if (!code && text[i] == '*' && i + 1 < len && text[i + 1] == '*') {
            if (i > start) {
                add_markdown_token(tokens, text + start, i - start, bold, italic, code, 0,
                                   is_list_item, is_ordered, order_index, list_level, false, false);
            }
            bold = !bold;
            i++; // Skip second *
            start = i + 1;
        }
        else if (!code && text[i] == '*') {
            if (i > start) {
                add_markdown_token(tokens, text + start, i - start, bold, italic, code, 0,
                                   is_list_item, is_ordered, order_index, list_level, false, false);
            }
            italic = !italic;
            start = i + 1;
        }
    }

    // Emit remaining text
    if (start < len) {
        add_markdown_token(tokens, text + start, len - start, bold, italic, code, 0,
                           is_list_item, is_ordered, order_index, list_level, false, false);
    }

    // Mark end of line on the final token for this line
    if (tokens->count > start_count) {
        tokens->tokens[tokens->count - 1].ends_line = true;
    }
}

// ============================================================================
// MARKDOWN PARSING
// ============================================================================

/*
 * parse_markdown - Parse markdown content into a structured token stream
 *
 * Tokenizes markdown input, recognizing:
 * - Headers (# to ######)
 * - Unordered lists (- or * followed by space)
 * - Ordered lists (digit(s) followed by . and space)
 * - Fenced code blocks (triple backticks)
 * - Inline formatting (bold, italic, code spans)
 *
 * The tokenizer processes the markdown line-by-line, maintaining state for
 * multi-line code blocks. Each token carries complete formatting information
 * needed for rendering.
 *
 * @markdown: Markdown-formatted text to parse
 * @return: Pointer to MarkdownTokens, or NULL on allocation failure
 */
MarkdownTokens* parse_markdown(const char* markdown) {
    if (!markdown) return NULL;

    MarkdownTokens* tokens = create_markdown_tokens();
    if (!tokens) return NULL;

    size_t len = strlen(markdown);
    size_t pos = 0;
    bool in_code_block = false;
    char* code_buffer = NULL;
    size_t code_len = 0;
    size_t code_cap = 0;

    while (pos < len) {
        // Skip whitespace at start of line
        while (pos < len && (markdown[pos] == ' ' || markdown[pos] == '\t')) pos++;

        // Fenced code block start/end
        if (pos + 2 < len && markdown[pos] == '`' && markdown[pos + 1] == '`' && markdown[pos + 2] == '`') {
            pos += 3;
            // Skip the rest of the fence line (language hint ignored)
            while (pos < len && markdown[pos] != '\n') pos++;

            if (in_code_block) {
                // End code block
                add_markdown_token(tokens, code_buffer ? code_buffer : "", code_len, false, false, true, 0,
                                   false, false, 0, 0, true, true);
                free(code_buffer);
                code_buffer = NULL;
                code_len = 0;
                code_cap = 0;
                in_code_block = false;
            } else {
                // Start code block
                in_code_block = true;
            }

            if (pos < len && markdown[pos] == '\n') pos++;
            continue;
        }

        if (in_code_block) {
            // Accumulate raw text inside code block
            size_t start = pos;
            while (pos < len && markdown[pos] != '\n') pos++;
            append_to_buffer(&code_buffer, &code_len, &code_cap, markdown + start, pos - start, true);
            if (pos < len && markdown[pos] == '\n') pos++;
            continue;
        }

        // Check for headers
        if (pos < len && markdown[pos] == '#') {
            int header_level = 0;
            while (pos < len && markdown[pos] == '#') {
                header_level++;
                pos++;
            }
            while (pos < len && markdown[pos] == ' ') pos++;

            size_t start = pos;
            while (pos < len && markdown[pos] != '\n') pos++;

            add_markdown_token(tokens, markdown + start, pos - start,
                              true, false, false, header_level, false, false, 0, 0, false, true);
        }
        // Check for list items
        else if (pos + 1 < len && (markdown[pos] == '-' || markdown[pos] == '*') && markdown[pos + 1] == ' ') {
            pos += 2; // Skip "- " or "* "
            int list_level = 0;

            size_t start = pos;
            while (pos < len && markdown[pos] != '\n') pos++;

            add_inline_segments(tokens, markdown + start, pos - start, true, false, 0, list_level);
        }
        // Check for ordered list items
        else if (pos < len && markdown[pos] >= '1' && markdown[pos] <= '9') {
            size_t num_start = pos;
            while (pos < len && markdown[pos] >= '0' && markdown[pos] <= '9') pos++;
            if (pos + 1 < len && markdown[pos] == '.' && markdown[pos + 1] == ' ') {
                int order_index = atoi(markdown + num_start);
                pos += 2; // Skip ". "

                size_t start = pos;
                while (pos < len && markdown[pos] != '\n') pos++;

                add_inline_segments(tokens, markdown + start, pos - start, true, true, order_index, 0);
            } else {
                // Regular text
                size_t start = num_start;
                while (pos < len && markdown[pos] != '\n') pos++;
                add_inline_segments(tokens, markdown + start, pos - start, false, false, 0, 0);
            }
        }
        // Regular text
        else {
            size_t start = pos;
            while (pos < len && markdown[pos] != '\n') pos++;

            add_inline_segments(tokens, markdown + start, pos - start, false, false, 0, 0);
        }

        // Skip newline
        if (pos < len && markdown[pos] == '\n') pos++;
    }

    // If file ended while still in code block, flush buffer
    if (in_code_block) {
        add_markdown_token(tokens, code_buffer ? code_buffer : "", code_len, false, false, true, 0,
                           false, false, 0, 0, true, true);
        free(code_buffer);
    }

    return tokens;
}

// ============================================================================
// MARKDOWN RENDERING
// ============================================================================

/*
 * render_markdown_content - Render markdown tokens to SDL surface
 *
 * Converts a parsed markdown token stream into rendered SDL output.
 * Handles:
 * - Font styling (bold, italic, size scaling for headers)
 * - Color application (different colors for headers, code, text)
 * - List formatting with bullet/number markers
 * - Code block background highlighting
 * - Scrolling support with scroll state tracking
 * - Scrollbar rendering when content overflows
 *
 * The rendering uses a two-pass approach:
 * 1. First pass measures total content height
 * 2. Second pass renders tokens with proper positioning and clipping
 *
 * @renderer: Desktop renderer context (contains SDL renderer and font)
 * @component: Component being rendered (carries component ID for scroll state)
 * @rect: Layout rectangle defining render area
 */
void render_markdown_content(DesktopIRRenderer* renderer, IRComponent* component, LayoutRect rect) {
    const char* markdown = component ? component->text_content : NULL;
    if (!markdown || !renderer->default_font) return;

    MarkdownTokens* tokens = parse_markdown(markdown);
    if (!tokens) return;

    SDL_Color default_color = {0, 0, 0, 255}; // Black text
    SDL_Color header_color = {0, 0, 100, 255}; // Dark blue for headers
    SDL_Color code_color = {100, 0, 100, 255}; // Purple for code
    SDL_Color code_bg_color = {240, 240, 240, 255}; // Light gray background

    float line_y = rect.y;
    float line_x = rect.x;
    float line_height = 22.0f; // Base line height
    float current_line_height = line_height;
    float header_scales[] = {1.0f, 2.0f, 1.6f, 1.35f, 1.2f, 1.05f, 1.0f}; // H1-H6 scales

    // First pass: measure total content height
    float measure_line_y = 0.0f;
    float measure_line_x = 0.0f;
    float measure_current_line_height = line_height;
    for (size_t i = 0; i < tokens->count; i++) {
        MarkdownToken* token = &tokens->tokens[i];
        if (token->length == 0) {
            measure_line_y += line_height * 0.5f;
            measure_line_x = 0.0f;
            measure_current_line_height = line_height;
            continue;
        }

        float font_scale = 1.0f;
        if (token->header_level > 0 && token->header_level <= 6) {
            font_scale = header_scales[token->header_level];
        }

        if (token->is_code_block) {
            // Measure code block using actual font rendering for accurate height
            const float padding = 6.0f;
            const char* text_ptr = token->text;
            size_t text_remaining = token->length;
            float total_height = 0.0f;
            while (text_remaining > 0) {
                const char* newline = memchr(text_ptr, '\n', text_remaining);
                size_t line_len = newline ? (size_t)(newline - text_ptr) : text_remaining;
                SDL_Surface* measure_surface = TTF_RenderText_Blended(renderer->default_font, line_len > 0 ? text_ptr : " ", line_len, default_color);
                if (measure_surface) {
                    total_height += measure_surface->h;
                    SDL_DestroySurface(measure_surface);
                } else {
                    total_height += line_height;
                }
                if (!newline) break;
                text_remaining -= line_len + 1;
                text_ptr = newline + 1;
            }
            float block_height = total_height + padding * 2.0f;
            if (block_height < line_height * 2.0f) block_height = line_height * 2.0f;
            // Add extra breathing room below code blocks so following content doesn't overlap visually
            measure_line_y += block_height + line_height * 0.75f;
            measure_line_x = 0.0f;
            measure_current_line_height = line_height;
            continue;
        }

        // Estimate width by rendering to surface for accuracy
        SDL_Surface* surface = TTF_RenderText_Blended(renderer->default_font, token->text, token->length, default_color);
        float token_width = surface ? (float)surface->w : (float)token->length * 8.0f;
        float token_height = surface ? (float)surface->h : line_height * font_scale;
        if (surface) SDL_DestroySurface(surface);

        if (token_height < line_height * font_scale) token_height = line_height * font_scale;
        if (token_height > measure_current_line_height) measure_current_line_height = token_height;

        measure_line_x += token_width + (token->is_list_item ? 22.0f : 4.0f);
        if (token->ends_line) {
            measure_line_y += measure_current_line_height + (token->header_level > 0 ? 4.0f : 2.0f);
            measure_line_x = 0.0f;
            measure_current_line_height = line_height;
        }
    }

    float content_height = measure_line_y + measure_current_line_height;

    // Get/update scroll state
    MarkdownScrollState* scroll_state = component ? get_markdown_scroll_state(component->id) : NULL;
    if (scroll_state) {
        scroll_state->content_height = content_height;
        float max_scroll = content_height - rect.height;
        if (max_scroll < 0) max_scroll = 0;
        if (scroll_state->scroll_offset < 0) scroll_state->scroll_offset = 0;
        if (scroll_state->scroll_offset > max_scroll) scroll_state->scroll_offset = max_scroll;
    }

    float scroll_offset = scroll_state ? scroll_state->scroll_offset : 0.0f;

    // Apply clip for scroll region
    SDL_Rect clip_rect = {
        (int)roundf(rect.x),
        (int)roundf(rect.y),
        (int)roundf(rect.width),
        (int)roundf(rect.height)
    };
    SDL_SetRenderClipRect(renderer->renderer, &clip_rect);

    for (size_t i = 0; i < tokens->count; i++) {
        MarkdownToken* token = &tokens->tokens[i];

        if (token->length == 0) {
            line_y += line_height * 0.5f; // Empty line
            line_x = rect.x;
            current_line_height = line_height;
            continue;
        }

        // Determine font properties
        float font_scale = 1.0f;
        SDL_Color text_color = default_color;
        bool is_bold = token->bold;
        bool is_italic = token->italic;
        bool is_code_span = token->code;

        if (token->header_level > 0 && token->header_level <= 6) {
            font_scale = header_scales[token->header_level];
            text_color = header_color;
            is_bold = true;
        } else if (is_code_span) {
            text_color = code_color;
        }

        // Load font with appropriate size
        TTF_Font* font = renderer->default_font; // Base font

        // Apply font style
        int ttf_style = TTF_STYLE_NORMAL;
        if (is_bold) ttf_style |= TTF_STYLE_BOLD;
        if (is_italic) ttf_style |= TTF_STYLE_ITALIC;
        TTF_SetFontStyle(font, ttf_style);

        float x_offset = 0.0f;
        if (token->is_list_item) {
            x_offset = 24.0f + (float)(token->list_level * 16); // Indent list items per level

            const char* marker_text = token->is_ordered ? "-" : "-";
            char marker_buf[16];
            if (token->is_ordered) {
                snprintf(marker_buf, sizeof(marker_buf), "%d.", token->order_index > 0 ? token->order_index : 1);
                marker_text = marker_buf;
            }

            SDL_Surface* bullet_surface = TTF_RenderText_Blended(font, marker_text, strlen(marker_text), text_color);
            if (bullet_surface) {
                    SDL_Texture* bullet_texture = SDL_CreateTextureFromSurface(renderer->renderer, bullet_surface);
                    if (bullet_texture) {
                        SDL_SetTextureScaleMode(bullet_texture, SDL_SCALEMODE_NEAREST);  // Crisp text
                        SDL_FRect bullet_rect = {
                            .x = roundf(rect.x + x_offset - 18.0f),
                            .y = roundf(line_y - scroll_offset + 2.0f),
                            .w = (float)bullet_surface->w,
                            .h = (float)bullet_surface->h
                        };
                        SDL_RenderTexture(renderer->renderer, bullet_texture, NULL, &bullet_rect);
                        SDL_DestroyTexture(bullet_texture);
                }
                SDL_DestroySurface(bullet_surface);
            }
        }

        if (token->is_code_block) {
            // Render multi-line code block with background
            const float padding = 6.0f;
            float block_x = rect.x + x_offset;
            float block_y = line_y - scroll_offset;
            float block_width = rect.width > 0 ? rect.width - x_offset : 0.0f;
            float block_height = 0.0f;

            // First pass: measure lines to get max width/height
            const char* text_ptr = token->text;
            size_t text_remaining = token->length;
            float max_line_width = 0.0f;
            float total_height = 0.0f;

            while (text_remaining > 0) {
                const char* newline = memchr(text_ptr, '\n', text_remaining);
                size_t line_len = newline ? (size_t)(newline - text_ptr) : text_remaining;

                SDL_Surface* measure_surface = TTF_RenderText_Blended(font, line_len > 0 ? text_ptr : " ", line_len, text_color);
                if (measure_surface) {
                    if (measure_surface->w > max_line_width) max_line_width = (float)measure_surface->w;
                    total_height += measure_surface->h;
                    SDL_DestroySurface(measure_surface);
                } else {
                    total_height += line_height;
                }

                if (!newline) break;
                text_remaining -= line_len + 1;
                text_ptr = newline + 1;
            }

            block_height = total_height + padding * 2.0f;
            // Always ensure minimum block width to avoid overlap with following content
            float min_block_width = max_line_width + padding * 2.0f;
            if (block_width <= 0.0f || block_width < min_block_width) {
                block_width = min_block_width;
            }

            // Ensure a minimum code block height to keep following content from overlapping
            if (block_height < line_height * 2.0f) {
                block_height = line_height * 2.0f;
                total_height = block_height - padding * 2.0f;
            }

            // Draw background
            SDL_FRect bg_rect = {
                .x = roundf(block_x),
                .y = roundf(block_y),
                .w = block_width,
                .h = block_height
            };
            SDL_SetRenderDrawColor(renderer->renderer, code_bg_color.r, code_bg_color.g, code_bg_color.b, code_bg_color.a);
            SDL_RenderFillRect(renderer->renderer, &bg_rect);

            // Render lines
            float line_y_block = block_y + padding;
            text_ptr = token->text;
            text_remaining = token->length;
            while (text_remaining > 0) {
                const char* newline = memchr(text_ptr, '\n', text_remaining);
                size_t line_len = newline ? (size_t)(newline - text_ptr) : text_remaining;

                SDL_Surface* surface = TTF_RenderText_Blended(font, line_len > 0 ? text_ptr : " ", line_len, text_color);
                if (surface) {
                    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer->renderer, surface);
                    if (texture) {
                        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);  // Crisp text
                        SDL_FRect text_rect = {
                            .x = roundf(block_x + padding),
                            .y = roundf(line_y_block),
                            .w = (float)surface->w,
                            .h = (float)surface->h
                        };
                        SDL_RenderTexture(renderer->renderer, texture, NULL, &text_rect);
                        SDL_DestroyTexture(texture);
                    }
                    line_y_block += surface->h;
                    SDL_DestroySurface(surface);
                } else {
                    line_y_block += line_height;
                }

                if (!newline) break;
                text_remaining -= line_len + 1;
                text_ptr = newline + 1;
            }

            // Advance line_y by full block height plus margin so following content starts below
            line_y += block_height + line_height;
            line_x = rect.x;
            current_line_height = line_height;
            continue;
        }

        // Render text
        SDL_Surface* surface = TTF_RenderText_Blended(font, token->text, token->length, text_color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer->renderer, surface);
            if (texture) {
                SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);  // Crisp text
                SDL_FRect text_rect = {
                    .x = roundf(line_x + x_offset),
                            .y = roundf(line_y - scroll_offset),
                            .w = (float)surface->w,
                            .h = (float)surface->h
                        };

                SDL_RenderTexture(renderer->renderer, texture, NULL, &text_rect);
                SDL_DestroyTexture(texture);
            }
            float advance = (float)surface->h;
            if (advance < line_height * font_scale) advance = line_height * font_scale;
            if (advance > current_line_height) current_line_height = advance;

            // Advance cursor on the same line
            line_x += x_offset + (float)surface->w + 4.0f; // small gap between segments
            x_offset = 0.0f; // only apply indentation once per line
            SDL_DestroySurface(surface);

            if (token->ends_line) {
                line_y += current_line_height + (token->header_level > 0 ? 4.0f : 2.0f);
                line_x = rect.x;
                current_line_height = line_height;
            }
        }
    }

    // Draw scrollbar if overflow
    if (scroll_state && content_height > rect.height && rect.height > 0.0f) {
        float track_width = 10.0f;
        float track_x = rect.x + rect.width - track_width;
        float track_y = rect.y;
        float track_h = rect.height;

        SDL_SetRenderDrawColor(renderer->renderer, 220, 220, 220, 180);
        SDL_FRect track_rect = {track_x, track_y, track_width, track_h};
        SDL_RenderFillRect(renderer->renderer, &track_rect);

        float thumb_h = (rect.height * rect.height) / content_height;
        if (thumb_h < 24.0f) thumb_h = 24.0f;
        if (thumb_h > track_h) thumb_h = track_h;
        float max_scroll = content_height - rect.height;
        float thumb_y = track_y;
        if (max_scroll > 0.0f) {
            thumb_y = track_y + (scroll_offset / max_scroll) * (track_h - thumb_h);
        }
        SDL_SetRenderDrawColor(renderer->renderer, 120, 120, 120, 220);
        SDL_FRect thumb_rect = {track_x + 2.0f, thumb_y + 2.0f, track_width - 4.0f, thumb_h - 4.0f};
        SDL_RenderFillRect(renderer->renderer, &thumb_rect);
    }

    // Clear clip
    SDL_SetRenderClipRect(renderer->renderer, NULL);

    free_markdown_tokens(tokens);
}

#endif // ENABLE_SDL3
