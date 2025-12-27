/*
 * ============================================================================
 * desktop_rendering.c - Main Component Rendering Orchestrator
 * ============================================================================
 *
 * This module contains the primary rendering function that handles the
 * recursive traversal and rendering of all IR components to SDL3 graphics.
 *
 * The render_component_sdl3() function is the heart of the desktop rendering
 * engine, responsible for:
 *
 *   - Rendering all component types (Container, Button, Text, Input,
 *     Checkbox, Dropdown, Canvas, Markdown)
 *   - Computing and applying box shadows and background colors
 *   - Recursively rendering child components with proper layout
 *   - Cascading opacity through the component tree
 *   - Managing z-index sorting for absolute positioned components
 *   - Handling dropdown menus with a second rendering pass
 *   - Managing text overflow, shadows, and styling effects
 *   - Computing flex layout (justify_content, align_items, flex_grow/shrink)
 *   - Handling absolute positioning and tab group dragging
 *
 * The function uses a sophisticated two-pass layout system for ROW and COLUMN
 * components, first measuring total content dimensions, then positioning based
 * on alignment modes (START, CENTER, END, SPACE_BETWEEN, SPACE_AROUND,
 * SPACE_EVENLY).
 *
 * Key rendering features:
 *   - Opacity inheritance and cascading
 *   - CSS-like transforms (scale, translate)
 *   - Border and shadow rendering
 *   - Text rendering with fade effects and shadow support
 *   - Input field caret animation and text scrolling
 *   - Canvas command buffer execution (polygon, rect, line, text, arc)
 *   - Markdown parsing and multi-line text rendering
 *   - Hit testing via rendered bounds caching
 *
 * Performance optimizations:
 *   - Blend mode caching (95% reduction in state changes)
 *   - Text measurement heuristics for layout phase
 *   - Command buffer iteration for canvas operations
 *   - Z-index sorting only for absolutely positioned components
 *   - Deferred dropdown menu rendering to avoid z-index conflicts
 *
 * ============================================================================
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

#include "desktop_internal.h"
#include "../../ir/ir_serialization.h"
#include "../../ir/ir_style_vars.h"
#include "../../ir/ir_plugin.h"
#include "../../ir/ir_native_canvas.h"
#include "../../third_party/stb/stb_image.h"
#include "../../third_party/stb/stb_image_write.h"

#ifdef ENABLE_SDL3

// Image texture cache - simple linked list for now
typedef struct ImageCacheEntry {
    char* path;
    SDL_Texture* texture;
    int width;
    int height;
    struct ImageCacheEntry* next;
} ImageCacheEntry;

static ImageCacheEntry* image_cache = NULL;

static SDL_Texture* load_image_texture(SDL_Renderer* renderer, const char* path, int* out_width, int* out_height) {
    // Check cache first
    for (ImageCacheEntry* entry = image_cache; entry; entry = entry->next) {
        if (strcmp(entry->path, path) == 0) {
            if (out_width) *out_width = entry->width;
            if (out_height) *out_height = entry->height;
            return entry->texture;
        }
    }

    // Load file into memory first (stbi_load requires STDIO which is disabled in IR)
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[kryon][image] Failed to open image file: %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* file_data = malloc(file_size);
    if (!file_data) {
        fclose(f);
        return NULL;
    }
    fread(file_data, 1, file_size, f);
    fclose(f);

    // Load image with stb_image from memory
    int width, height, channels;
    unsigned char* data = stbi_load_from_memory(file_data, file_size, &width, &height, &channels, 4);  // Force RGBA
    free(file_data);
    if (!data) {
        fprintf(stderr, "[kryon][image] Failed to decode image: %s\n", path);
        return NULL;
    }

    // Create SDL surface from pixel data
    SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, data, width * 4);
    if (!surface) {
        fprintf(stderr, "[kryon][image] Failed to create surface: %s\n", SDL_GetError());
        stbi_image_free(data);
        return NULL;
    }

    // Create texture from surface
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    stbi_image_free(data);

    if (!texture) {
        fprintf(stderr, "[kryon][image] Failed to create texture: %s\n", SDL_GetError());
        return NULL;
    }

    // Add to cache
    ImageCacheEntry* entry = malloc(sizeof(ImageCacheEntry));
    entry->path = strdup(path);
    entry->texture = texture;
    entry->width = width;
    entry->height = height;
    entry->next = image_cache;
    image_cache = entry;

    if (out_width) *out_width = width;
    if (out_height) *out_height = height;
    return texture;
}

/*
 * ============================================================================
 * render_component_sdl3()
 * ============================================================================
 *
 * Main recursive component rendering function for SDL3 backend.
 *
 * This is the primary rendering pipeline that traverses the IR component tree
 * and produces visual output to the SDL3 renderer. Each call handles a single
 * component and recursively calls itself for child components.
 *
 * Parameters:
 *   renderer          - The desktop renderer context with SDL3 renderer handle
 *   component         - The IR component to render (NULL-safe)
 *   rect              - The layout rectangle (position + dimensions) for this component
 *   inherited_opacity - Opacity value cascaded from parent (0.0-1.0)
 *
 * Returns:
 *   bool - true on success, false if renderer or component is invalid
 *
 * The function processes components in the following phases:
 *
 * Phase 1: Validation and Bounds Caching
 *   - Validates inputs (renderer, component)
 *   - Caches rendered bounds for hit testing
 *   - Initializes SDL_FRect from layout bounds
 *
 * Phase 2: Transform Application
 *   - Applies CSS-like transforms (scale, translate)
 *   - Recenter scaling to maintain visual appearance
 *
 * Phase 3: Component-Specific Rendering
 *   - Renders component background, borders, shadows
 *   - Renders component text content with styling
 *   - Renders component interactive elements (checkbox, dropdown arrow, etc)
 *   - Executes component canvas commands
 *   - Handles markdown parsing and layout
 *
 * Phase 4: Child Layout and Rendering
 *   - Applies padding and child rect adjustments
 *   - Computes flex layout (justify_content, align_items)
 *   - Handles flex_grow and flex_shrink distribution
 *   - Applies alignment and positioning to each child
 *   - Recursively renders each child component
 *   - Handles absolute positioning with z-index sorting
 *   - Defers dragged tab rendering to appear on top
 *
 * Component Type Handling:
 *
 *   IR_COMPONENT_CONTAINER / ROW / COLUMN
 *   - Renders box shadow, background, and border
 *   - Processes child layout with flex container rules
 *
 *   IR_COMPONENT_CENTER
 *   - Transparent layout container; centers single child
 *
 *   IR_COMPONENT_BUTTON
 *   - Renders button background and border
 *   - Renders button text centered with overflow handling
 *   - Supports text fade effect for overflow
 *
 *   IR_COMPONENT_TEXT
 *   - Renders text with optional shadow effect
 *   - Applies opacity cascading
 *   - Uses positioned text rendering
 *
 *   IR_COMPONENT_INPUT
 *   - Renders background and border
 *   - Renders input text with scrolling for overflow
 *   - Animates caret cursor (blinking at 500ms interval)
 *   - Clips text to input bounds
 *
 *   IR_COMPONENT_CHECKBOX
 *   - Renders checkbox box with border
 *   - Renders checkmark if checked
 *   - Renders label text next to checkbox
 *
 *   IR_COMPONENT_DROPDOWN
 *   - Renders dropdown box, border, and selected text
 *   - Renders dropdown arrow on right side
 *   - Menu rendering deferred to second pass for z-index
 *
 *   IR_COMPONENT_CANVAS
 *   - Executes Nim callback to populate canvas command buffer
 *   - Processes command buffer and renders primitives:
 *     * POLYGON: filled shapes rendered as triangle fans
 *     * RECT: filled rectangles
 *     * LINE: line segments
 *     * TEXT: text rendering via SDL_ttf
 *     * ARC: tessellated into line segments (32 segments)
 *
 *   IR_COMPONENT_MARKDOWN
 *   - Delegates to render_markdown_content() helper
 *   - Parses and formats markdown content with syntax highlighting
 *
 * Flex Layout System:
 *
 * The function implements a sophisticated two-pass layout system:
 *
 * COLUMN Pass 1 (lines 3009-3097):
 *   - Measures total content height
 *   - Computes justify_content (main axis alignment)
 *   - Calculates starting Y position (START, CENTER, END)
 *   - Handles space distribution modes:
 *     * SPACE_BETWEEN: equal space between items
 *     * SPACE_AROUND: equal space around items (half at edges)
 *     * SPACE_EVENLY: equal space everywhere (including edges)
 *
 * ROW Pass 1 (lines 3102-3230):
 *   - Measures total content width
 *   - Computes flex_grow factors and allocates flex_extra_widths array
 *   - Distributes remaining space via flex_grow or flex_shrink
 *   - Calculates starting X position with justify_content
 *   - Handles space distribution modes (same as COLUMN)
 *
 * Child Alignment Phase (lines 3437-3556):
 *   - Applies cross-axis alignment (align_items) for each child:
 *     * COLUMN: align_items controls horizontal centering/stretching
 *     * ROW: align_items controls vertical centering/stretching
 *   - Special handling for CENTER component (always centers single child)
 *   - STRETCH mode sizing for flexible layouts
 *   - Defers treatment of AUTO-sized Row/Column children
 *
 * Child Sizing Phase (lines 3563-3671):
 *   - Applies flex_grow/flex_shrink extra width to ROW children
 *   - Clamps to minWidth and maxWidth constraints
 *   - Handles AUTO-sized Row/Column special cases:
 *     * Use measured content dimensions instead of 0
 *     * Preserve full container dimensions if alignment needed
 *     * Measure actual content for centering calculations
 *
 * Child Rendering and Stacking (lines 3673-3720):
 *   - Defers dragged tab rendering to appear on top
 *   - Recursively renders each child
 *   - Advances current_x or current_y based on container type
 *   - Tracks distributed_gap for space distribution modes
 *
 * Opacity Cascading:
 *
 *   All rendering operations respect cascaded opacity:
 *     child_opacity = (component->style->opacity) * inherited_opacity
 *
 *   This is applied to:
 *   - Background and border colors
 *   - Text colors
 *   - Shadow colors
 *   - All child component rendering
 *
 * Special Features:
 *
 *   Absolute Positioning (lines 3382-3426):
 *     - Shortcuts flex layout for all-absolute-positioned children
 *     - Sorts by z-index and renders in order
 *     - Computed layout via calculate_component_layout()
 *
 *   Text Overflow Handling (lines 2400-2422):
 *     - IR_TEXT_OVERFLOW_CLIP: clips text to visible width
 *     - IR_TEXT_OVERFLOW_FADE: renders with gradient fade at edge
 *     - Fade ratio defaults to 25% of visible width
 *
 *   Input Caret Animation (lines 2591-2606):
 *     - Blinks at 500ms intervals
 *     - Visible state toggled on SDL_GetTicks() milestone
 *     - Rendered as 1px vertical line
 *
 *   Tab Dragging (lines 3673-3686, 3722-3727):
 *     - Tracks dragged_child during iteration
 *     - Defers rendering until after siblings
 *     - Applies cursor-following offset (drag_x - half width)
 *     - Renders on top of siblings (last in render order)
 *
 * Environment Variables (for debugging):
 *
 *   KRYON_TRACE_LAYOUT
 *     - Enables detailed layout computation tracing
 *     - Shows alignment decisions and flex distribution
 *     - Prints child positions and sizing at each phase
 *
 */
bool render_component_sdl3(DesktopIRRenderer* renderer, IRComponent* component, LayoutRect rect, float inherited_opacity) {
    if (!renderer || !component || !renderer->renderer) return false;

    // Skip rendering if component is not visible
    if (component->style && !component->style->visible) {
        return true;  // Successfully "rendered" by not drawing
    }

    // CRITICAL: Layout MUST be pre-computed by ir_layout_compute_tree()
    // NO FALLBACKS - if layout isn't valid, this is a BUG that must be fixed!
    if (!component->layout_state || !component->layout_state->layout_valid) {
        fprintf(stderr, "FATAL ERROR: Layout not computed for component %u type %d\n",
                component->id, component->type);
        fprintf(stderr, "  Call ir_layout_compute_tree() before rendering!\n");
        return false;
    }

    // Read pre-computed layout (NEVER use rect parameter for positioning!)
    IRComputedLayout* layout = &component->layout_state->computed;
    SDL_FRect sdl_rect = {
        .x = layout->x,
        .y = layout->y,
        .w = layout->width,
        .h = layout->height
    };

    // Apply CSS-like transforms (scale, translate)
    if (component->style) {
        IRTransform* t = &component->style->transform;

        // Apply scale from center
        if (t->scale_x != 1.0f || t->scale_y != 1.0f) {
            float orig_w = sdl_rect.w;
            float orig_h = sdl_rect.h;
            sdl_rect.w *= t->scale_x;
            sdl_rect.h *= t->scale_y;
            // Recenter after scaling
            sdl_rect.x -= (sdl_rect.w - orig_w) / 2.0f;
            sdl_rect.y -= (sdl_rect.h - orig_h) / 2.0f;
        }

        // Apply translation
        sdl_rect.x += t->translate_x;
        sdl_rect.y += t->translate_y;
    }

    // Cache rendered bounds for hit testing AFTER transforms
    // This ensures hit testing matches the visual rendering
    // Transforms (scale, translate) affect both rendering AND interaction
    //
    // IMPORTANT: Skip for TABLE children (sections, rows, cells) because their
    // rendered_bounds use RELATIVE coordinates set by ir_layout_compute_table.
    // Writing absolute coordinates here would corrupt the table layout cache.
    bool is_table_child = (component->type == IR_COMPONENT_TABLE_HEAD ||
                           component->type == IR_COMPONENT_TABLE_BODY ||
                           component->type == IR_COMPONENT_TABLE_FOOT ||
                           component->type == IR_COMPONENT_TABLE_ROW ||
                           component->type == IR_COMPONENT_TABLE_CELL ||
                           component->type == IR_COMPONENT_TABLE_HEADER_CELL);
    if (!is_table_child) {
        ir_set_rendered_bounds(component, sdl_rect.x, sdl_rect.y, sdl_rect.w, sdl_rect.h);
    }

    // Render based on component type
    switch (component->type) {
        case IR_COMPONENT_CONTAINER:
        case IR_COMPONENT_ROW:
        case IR_COMPONENT_COLUMN:
            // Render box shadow if present (simplified - no blur)
            if (component->style && component->style->box_shadow.enabled && !component->style->box_shadow.inset) {
                IRBoxShadow* shadow = &component->style->box_shadow;

                // Create shadow rectangle offset by shadow.offset_x/y
                SDL_FRect shadow_rect = {
                    .x = sdl_rect.x + shadow->offset_x,
                    .y = sdl_rect.y + shadow->offset_y,
                    .w = sdl_rect.w + (shadow->spread_radius * 2.0f),
                    .h = sdl_rect.h + (shadow->spread_radius * 2.0f)
                };

                // Adjust position for spread (expand from center)
                shadow_rect.x -= shadow->spread_radius;
                shadow_rect.y -= shadow->spread_radius;

                // Render shadow with shadow color and opacity
                float opacity = component->style->opacity * inherited_opacity;
                SDL_Color shadow_color = ir_color_to_sdl(shadow->color);
                shadow_color = apply_opacity_to_color(shadow_color, opacity);
                ensure_blend_mode(renderer->renderer);
                SDL_SetRenderDrawColor(renderer->renderer,
                    shadow_color.r, shadow_color.g, shadow_color.b, shadow_color.a);
                SDL_RenderFillRect(renderer->renderer, &shadow_rect);
            }

            // Only render background if component has a style with non-transparent background
            if (component->style) {
                float opacity = component->style->opacity * inherited_opacity;
                render_background(renderer->renderer, component, &sdl_rect, opacity);

                // Render border if it has a width > 0
                float border_width = component->style->border.width;
                if (border_width > 0) {
                    SDL_Color border_color = ir_color_to_sdl(component->style->border.color);
                    // Apply opacity to border as well
                    border_color.a = (uint8_t)(border_color.a * opacity);
                    ensure_blend_mode(renderer->renderer);
                    SDL_SetRenderDrawColor(renderer->renderer, border_color.r, border_color.g, border_color.b, border_color.a);

                    // Draw border with specified width (multiple concentric rectangles)
                    for (int i = 0; i < (int)border_width; i++) {
                        SDL_FRect border_rect = {
                            .x = sdl_rect.x + i,
                            .y = sdl_rect.y + i,
                            .w = sdl_rect.w - 2*i,
                            .h = sdl_rect.h - 2*i
                        };
                        SDL_RenderRect(renderer->renderer, &border_rect);
                    }
                }
            }
            break;

        case IR_COMPONENT_CENTER:
            // Center is a layout-only component, completely transparent - don't render background
            break;

        case IR_COMPONENT_BUTTON: {
            TTF_Font* font = desktop_ir_resolve_font(renderer, component, component->style && component->style->font.size > 0 ? component->style->font.size : 16.0f);
            // Render button background with opacity
            if (component->style) {
                float opacity = component->style->opacity * inherited_opacity;
                render_background(renderer->renderer, component, &sdl_rect, opacity);
            } else {
                ensure_blend_mode(renderer->renderer);
                SDL_SetRenderDrawColor(renderer->renderer, 100, 100, 200, 255);
                SDL_RenderFillRect(renderer->renderer, &sdl_rect);
            }

            // Draw button border
            SDL_SetRenderDrawColor(renderer->renderer, 100, 100, 100, 255);
            SDL_RenderRect(renderer->renderer, &sdl_rect);

            // Render button text if present
            if (component->text_content && font) {
                SDL_Color text_color = component->style ?
                    ir_color_to_sdl(component->style->font.color) :
                    (SDL_Color){0, 0, 0, 255};

                SDL_Surface* surface = TTF_RenderText_Blended(font,
                                                              component->text_content,
                                                              strlen(component->text_content),
                                                              text_color);
                if (surface) {
                    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer->renderer, surface);
                    if (texture) {
                        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);  // Crisp text
                        float text_w = (float)surface->w;
                        float text_h = (float)surface->h;
                        float avail_w = sdl_rect.w;
                        float text_y = roundf(sdl_rect.y + (sdl_rect.h - text_h) / 2);

                        // Check text_effect settings for overflow handling
                        IRTextOverflowType overflow = IR_TEXT_OVERFLOW_CLIP;  // Default: clip (no fade)
                        IRTextFadeType fade_type = IR_TEXT_FADE_NONE;
                        float fade_length = 0;

                        if (component->style) {
                            overflow = component->style->text_effect.overflow;
                            fade_type = component->style->text_effect.fade_type;
                            fade_length = component->style->text_effect.fade_length;
                        }

                        if (text_w > avail_w - 16) {
                            // Text overflows - handle based on text_effect settings
                            float text_x = sdl_rect.x + 8;  // Small left padding
                            float visible_w = avail_w - 12;  // Leave margin

                            // Use fade if: overflow is FADE, or fade_type is HORIZONTAL with length > 0
                            bool should_fade = (overflow == IR_TEXT_OVERFLOW_FADE) ||
                                              (fade_type == IR_TEXT_FADE_HORIZONTAL && fade_length > 0);

                            if (should_fade) {
                                // Calculate fade ratio from fade_length or default to 25%
                                float fade_ratio = (fade_length > 0) ? (fade_length / visible_w) : 0.25f;
                                if (fade_ratio > 1.0f) fade_ratio = 1.0f;

                                // Fade not yet implemented - render normally
                                SDL_FRect src_rect = { 0, 0, visible_w, text_h };
                                SDL_FRect dst_rect = { text_x, text_y, visible_w, text_h };
                                SDL_RenderTexture(renderer->renderer, texture, &src_rect, &dst_rect);
                            } else {
                                // Clip: just render what fits, no fade
                                SDL_FRect src_rect = { 0, 0, visible_w, text_h };
                                SDL_FRect dst_rect = { text_x, text_y, visible_w, text_h };
                                SDL_RenderTexture(renderer->renderer, texture, &src_rect, &dst_rect);
                            }
                        } else {
                            // Text fits: render normally, centered
                            SDL_FRect text_rect = {
                                .x = roundf(sdl_rect.x + (sdl_rect.w - text_w) / 2),
                                .y = text_y,
                                .w = text_w,
                                .h = text_h
                            };
                            SDL_RenderTexture(renderer->renderer, texture, NULL, &text_rect);
                        }
                        SDL_DestroyTexture(texture);
                    }
                    SDL_DestroySurface(surface);
                }
            }
            break;
        }

        case IR_COMPONENT_TAB: {
            // Render tab like a button but use tab_data->title
            TTF_Font* font = desktop_ir_resolve_font(renderer, component, component->style && component->style->font.size > 0 ? component->style->font.size : 16.0f);

            // Render tab background with opacity
            if (component->style) {
                float opacity = component->style->opacity * inherited_opacity;
                render_background(renderer->renderer, component, &sdl_rect, opacity);
            }

            // Render tab title text if present
            if (component->tab_data && component->tab_data->title && font) {
                SDL_Color text_color = component->style ?
                    ir_color_to_sdl(component->style->font.color) :
                    (SDL_Color){255, 255, 255, 255};

                // Guard against alpha=0 colors (uninitialized or transparent)
                if (text_color.a == 0) {
                    // Default to white text for tabs (good contrast on dark backgrounds)
                    text_color = (SDL_Color){255, 255, 255, 255};
                }

                // Apply component opacity to text (cascaded with inherited opacity)
                if (component->style) {
                    float opacity = component->style->opacity * inherited_opacity;
                    text_color.a = (uint8_t)(text_color.a * opacity);
                }

                SDL_Surface* surface = TTF_RenderText_Blended(font,
                                                              component->tab_data->title,
                                                              strlen(component->tab_data->title),
                                                              text_color);
                if (surface) {
                    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer->renderer, surface);
                    if (texture) {
                        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
                        float text_w = (float)surface->w;
                        float text_h = (float)surface->h;

                        // Center text in tab
                        SDL_FRect text_rect = {
                            .x = roundf(sdl_rect.x + (sdl_rect.w - text_w) / 2),
                            .y = roundf(sdl_rect.y + (sdl_rect.h - text_h) / 2),
                            .w = text_w,
                            .h = text_h
                        };
                        SDL_RenderTexture(renderer->renderer, texture, NULL, &text_rect);
                        SDL_DestroyTexture(texture);
                    }
                    SDL_DestroySurface(surface);
                }
            }
            break;
        }

        case IR_COMPONENT_TEXT: {
            TTF_Font* font = desktop_ir_resolve_font(renderer, component, component->style && component->style->font.size > 0 ? component->style->font.size : 16.0f);
            if (component->text_content && font) {
                SDL_Color text_color = component->style ?
                    ir_color_to_sdl(component->style->font.color) :
                    (SDL_Color){51, 51, 51, 255};
                if (text_color.a == 0) {
                    // Avoid invisible/alpha=0 colors; default to dark text
                    text_color = (SDL_Color){51, 51, 51, 255};
                }

                // Apply component opacity to text (cascaded with inherited opacity)
                if (component->style) {
                    float opacity = component->style->opacity * inherited_opacity;
                    text_color.a = (uint8_t)(text_color.a * opacity);
                }

                // Check for text shadow effect
                IRTextShadow* shadow = (component->style && component->style->text_effect.shadow.enabled) ?
                    &component->style->text_effect.shadow : NULL;

                // Determine max_width for text wrapping
                // Use explicit max_width if set, otherwise use large value to prevent unwanted wrapping
                float text_max_width = 10000.0f; // Default: no wrapping
                if (component->style && component->style->text_effect.max_width.type == IR_DIMENSION_PX) {
                    text_max_width = component->style->text_effect.max_width.value;
                }

                // Render text with optional shadow
                render_text_with_shadow(renderer->renderer, font,
                                       component->text_content, text_color, component,
                                       sdl_rect.x, sdl_rect.y, text_max_width);
            }
            break;
        }

        case IR_COMPONENT_INPUT:
            // Background
            {
                SDL_Color bg_color = (SDL_Color){255, 255, 255, 255};
                if (component->style) {
                    SDL_Color candidate = ir_color_to_sdl(component->style->background);
                    // Default styles have alpha=0; use fallback if fully transparent
                    if (candidate.a != 0) {
                        bg_color = candidate;
                    }
                }
                float opacity = component->style ? component->style->opacity * inherited_opacity : 1.0f;
                bg_color = apply_opacity_to_color(bg_color, opacity);
                ensure_blend_mode(renderer->renderer);
                SDL_SetRenderDrawColor(renderer->renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
                SDL_RenderFillRect(renderer->renderer, &sdl_rect);
            }

            // Border
            {
                SDL_Color border_color = (SDL_Color){180, 180, 180, 255};
                if (component->style) {
                    SDL_Color candidate = ir_color_to_sdl(component->style->border.color);
                    if (candidate.a != 0) {
                        border_color = candidate;
                    }
                }
                float opacity = component->style ? component->style->opacity * inherited_opacity : 1.0f;
                border_color = apply_opacity_to_color(border_color, opacity);
                ensure_blend_mode(renderer->renderer);
                SDL_SetRenderDrawColor(renderer->renderer, border_color.r, border_color.g, border_color.b, border_color.a);
                SDL_RenderRect(renderer->renderer, &sdl_rect);
            }

            // Text / placeholder with scrolling and caret
            {
                InputRuntimeState* istate = get_input_state(component->id);
                const char* value_text = component->text_content ? component->text_content : "";
                bool has_value = value_text[0] != '\0';
                const char* placeholder = component->custom_data ? component->custom_data : "";
                const char* to_render = has_value ? value_text : placeholder;
                TTF_Font* font = desktop_ir_resolve_font(renderer, component, component->style && component->style->font.size > 0 ? component->style->font.size : 16.0f);

                float pad_left = component->style ? component->style->padding.left : 8.0f;
                float pad_top = component->style ? component->style->padding.top : 8.0f;
                float pad_right = component->style ? component->style->padding.right : 8.0f;
                float avail_width = sdl_rect.w - pad_left - pad_right;

                SDL_Color text_color = has_value ? (SDL_Color){40, 40, 40, 255}
                                                 : (SDL_Color){160, 160, 160, 255};
                if (component->style) {
                    SDL_Color candidate = ir_color_to_sdl(component->style->font.color);
                    if (candidate.a != 0) {
                        text_color = candidate;
                        if (!has_value) {
                            text_color.r = (uint8_t)((text_color.r + 255) / 2);
                            text_color.g = (uint8_t)((text_color.g + 255) / 2);
                            text_color.b = (uint8_t)((text_color.b + 255) / 2);
                        }
                    }
                }

                float caret_x = sdl_rect.x + pad_left;
                float caret_y = sdl_rect.y + pad_top;
                float caret_height = sdl_rect.h - pad_top - (component->style ? component->style->padding.bottom : 8.0f);

                float caret_local = 0.0f;
                if (font && istate) {
                    char* prefix = strndup(value_text, istate->cursor_index);
                    caret_local = measure_text_width(font, prefix);
                    free(prefix);
                    ensure_caret_visible(renderer, component, istate, font, pad_left, pad_right);
                    caret_x = sdl_rect.x + pad_left + (caret_local - istate->scroll_x);
                } else {
                    caret_x = sdl_rect.x + pad_left;
                }

                if (font && to_render && to_render[0] != '\0') {
                    SDL_Surface* surface = TTF_RenderText_Blended(font,
                                                                  to_render,
                                                                  strlen(to_render),
                                                                  text_color);
                    if (surface) {
                        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer->renderer, surface);
                        if (texture) {
                            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);  // Crisp text
                            float text_x = sdl_rect.x + pad_left;
                            float text_y = sdl_rect.y + pad_top;
                            if (!component->style || (component->style->padding.top == 0 && component->style->padding.bottom == 0)) {
                                text_y = sdl_rect.y + (sdl_rect.h - surface->h) / 2;
                            }
                            SDL_FRect dest_rect = {
                                .x = roundf(text_x - (istate ? istate->scroll_x : 0.0f)),
                                .y = roundf(text_y),
                                .w = (float)surface->w,
                                .h = (float)surface->h
                            };
                            // Clip to input rect minus padding
                            SDL_FRect clip_rect = {
                                .x = sdl_rect.x + pad_left,
                                .y = sdl_rect.y + pad_top,
                                .w = fmaxf(0.0f, sdl_rect.w - pad_left - pad_right),
                                .h = sdl_rect.h - pad_top - (component->style ? component->style->padding.bottom : 8.0f)
                            };
                            SDL_Rect clip_px = {
                                .x = (int)clip_rect.x,
                                .y = (int)clip_rect.y,
                                .w = (int)clip_rect.w,
                                .h = (int)clip_rect.h
                            };
                            SDL_SetRenderClipRect(renderer->renderer, &clip_px);
                            SDL_RenderTexture(renderer->renderer, texture, NULL, &dest_rect);
                            SDL_SetRenderClipRect(renderer->renderer, NULL);
                            SDL_DestroyTexture(texture);
                        }
                        SDL_DestroySurface(surface);
                    }
                }

                // Caret rendering
                if (istate && istate->focused && font) {
                    uint32_t now = SDL_GetTicks();
                    if (now - istate->last_blink_ms > 500) {
                        istate->caret_visible = !istate->caret_visible;
                        istate->last_blink_ms = now;
                    }
                    if (istate->caret_visible) {
                        SDL_Color caret_color = text_color;
                        SDL_SetRenderDrawColor(renderer->renderer, caret_color.r, caret_color.g, caret_color.b, caret_color.a);
                        SDL_RenderLine(renderer->renderer,
                            caret_x,
                            caret_y,
                            caret_x,
                            caret_y + caret_height - 2);
                    }
                }
            }
            break;

        case IR_COMPONENT_CHECKBOX: {
            // Checkbox is rendered as a small box on the left + label text
            float checkbox_size = fminf(sdl_rect.h * 0.6f, 20.0f);
            float checkbox_y = sdl_rect.y + (sdl_rect.h - checkbox_size) / 2;

            SDL_FRect checkbox_rect = {
                .x = sdl_rect.x + 5,
                .y = checkbox_y,
                .w = checkbox_size,
                .h = checkbox_size
            };

            // Calculate opacity for all checkbox elements
            float opacity = component->style ? component->style->opacity * inherited_opacity : 1.0f;

            // Draw checkbox background
            SDL_Color bg_color = component->style ?
                ir_color_to_sdl(component->style->background) :
                (SDL_Color){255, 255, 255, 255};
            bg_color = apply_opacity_to_color(bg_color, opacity);
            ensure_blend_mode(renderer->renderer);
            SDL_SetRenderDrawColor(renderer->renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
            SDL_RenderFillRect(renderer->renderer, &checkbox_rect);

            // Draw checkbox border
            SDL_Color border_color = apply_opacity_to_color((SDL_Color){100, 100, 100, 255}, opacity);
            SDL_SetRenderDrawColor(renderer->renderer, border_color.r, border_color.g, border_color.b, border_color.a);
            SDL_RenderRect(renderer->renderer, &checkbox_rect);

            // Check if checkbox is checked (stored in custom_data)
            bool is_checked = component->custom_data &&
                             strcmp(component->custom_data, "checked") == 0;

            // Draw checkmark if checked
            if (is_checked) {
                SDL_Color check_color = apply_opacity_to_color((SDL_Color){50, 200, 50, 255}, opacity);
                SDL_SetRenderDrawColor(renderer->renderer, check_color.r, check_color.g, check_color.b, check_color.a);
                // Draw a simple checkmark using lines
                float cx = checkbox_rect.x + checkbox_size / 2;
                float cy = checkbox_rect.y + checkbox_size / 2;
                float size = checkbox_size * 0.3f;

                SDL_RenderLine(renderer->renderer,
                              cx - size, cy,
                              cx - size/3, cy + size);
                SDL_RenderLine(renderer->renderer,
                              cx - size/3, cy + size,
                              cx + size, cy - size);
            }

            // Render label text next to checkbox
            if (component->text_content && renderer->default_font) {
                SDL_Color text_color = component->style ?
                    ir_color_to_sdl(component->style->font.color) :
                    (SDL_Color){0, 0, 0, 255};

                SDL_Surface* surface = TTF_RenderText_Blended(renderer->default_font,
                                                              component->text_content,
                                                              strlen(component->text_content),
                                                              text_color);
                if (surface) {
                    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer->renderer, surface);
                    if (texture) {
                        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);  // Crisp text
                        // Round text position to integer pixels for crisp rendering
                        SDL_FRect text_rect = {
                            .x = roundf(checkbox_rect.x + checkbox_size + 10),
                            .y = roundf(sdl_rect.y + (sdl_rect.h - surface->h) / 2.0f),
                            .w = (float)surface->w,
                            .h = (float)surface->h
                        };
                        SDL_RenderTexture(renderer->renderer, texture, NULL, &text_rect);
                        SDL_DestroyTexture(texture);
                    }
                    SDL_DestroySurface(surface);
                }
            }
            break;
        }

        case IR_COMPONENT_DROPDOWN: {
            // Get dropdown state from custom_data
            IRDropdownState* dropdown_state = ir_get_dropdown_state(component);
            if (!dropdown_state) break;

            // Get colors and styling
            SDL_Color bg_color = component->style ?
                ir_color_to_sdl(component->style->background) :
                (SDL_Color){255, 255, 255, 255};
            SDL_Color text_color = component->style ?
                ir_color_to_sdl(component->style->font.color) :
                (SDL_Color){0, 0, 0, 255};
            SDL_Color border_color = component->style ?
                ir_color_to_sdl(component->style->border.color) :
                (SDL_Color){209, 213, 219, 255};

            // Draw main dropdown box
            SDL_SetRenderDrawColor(renderer->renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
            SDL_RenderFillRect(renderer->renderer, &sdl_rect);

            // Draw border
            float border_width = component->style ? component->style->border.width : 1.0f;
            SDL_SetRenderDrawColor(renderer->renderer, border_color.r, border_color.g, border_color.b, border_color.a);
            for (int i = 0; i < (int)border_width; i++) {
                SDL_FRect border_rect = {
                    .x = sdl_rect.x + i,
                    .y = sdl_rect.y + i,
                    .w = sdl_rect.w - 2*i,
                    .h = sdl_rect.h - 2*i
                };
                SDL_RenderRect(renderer->renderer, &border_rect);
            }

            // Render selected text or placeholder
            const char* display_text = dropdown_state->placeholder;
            if (dropdown_state->selected_index >= 0 &&
                dropdown_state->selected_index < (int32_t)dropdown_state->option_count &&
                dropdown_state->options) {
                display_text = dropdown_state->options[dropdown_state->selected_index];
            }

            if (display_text && renderer->default_font) {
                SDL_Surface* surface = TTF_RenderText_Blended(renderer->default_font,
                                                              display_text,
                                                              strlen(display_text),
                                                              text_color);
                if (surface) {
                    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer->renderer, surface);
                    if (texture) {
                        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);  // Crisp text
                        SDL_FRect text_rect = {
                            .x = roundf(sdl_rect.x + 10),
                            .y = roundf(sdl_rect.y + (sdl_rect.h - surface->h) / 2.0f),
                            .w = (float)surface->w,
                            .h = (float)surface->h
                        };
                        SDL_RenderTexture(renderer->renderer, texture, NULL, &text_rect);
                        SDL_DestroyTexture(texture);
                    }
                    SDL_DestroySurface(surface);
                }
            }

            // Draw dropdown arrow on the right
            float arrow_x = sdl_rect.x + sdl_rect.w - 20;
            float arrow_y = sdl_rect.y + sdl_rect.h / 2;
            float arrow_size = 5;
            SDL_SetRenderDrawColor(renderer->renderer, text_color.r, text_color.g, text_color.b, text_color.a);
            // Draw downward arrow using lines
            SDL_RenderLine(renderer->renderer,
                          arrow_x - arrow_size, arrow_y - arrow_size/2,
                          arrow_x, arrow_y + arrow_size/2);
            SDL_RenderLine(renderer->renderer,
                          arrow_x, arrow_y + arrow_size/2,
                          arrow_x + arrow_size, arrow_y - arrow_size/2);

            // Dropdown menu rendering is deferred to second pass (after all components)
            // This ensures menus appear on top (correct z-index)
            break;
        }

        case IR_COMPONENT_CANVAS: {
            // Generic plugin dispatch - follows markdown pattern
            IRPluginBackendContext plugin_ctx = {
                .renderer = renderer->renderer,
                .font = renderer->default_font,
                .user_data = NULL
            };

            if (!ir_plugin_dispatch_component_render(&plugin_ctx, component->type,
                                                      component, sdl_rect.x, sdl_rect.y,
                                                      sdl_rect.w, sdl_rect.h)) {
                fprintf(stderr, "[kryon][desktop] No renderer registered for CANVAS component\n");
            }
            break;
        }

        case IR_COMPONENT_NATIVE_CANVAS: {
            // NativeCanvas - invoke user callback for direct backend access
            // This allows users to call backend-specific rendering functions
            // (e.g., raylib 3D functions, SDL3 rendering, etc.)

            // Invoke the callback if one is registered
            if (!ir_native_canvas_invoke_callback(component->id)) {
                // No callback registered - render a placeholder or warning
                IRNativeCanvasData* canvas_data = ir_native_canvas_get_data(component);
                if (canvas_data) {
                    // Render background color
                    SDL_FRect bg_rect = sdl_rect;
                    uint32_t bg_color = canvas_data->background_color;
                    uint8_t r = (bg_color >> 24) & 0xFF;
                    uint8_t g = (bg_color >> 16) & 0xFF;
                    uint8_t b = (bg_color >> 8) & 0xFF;
                    uint8_t a = bg_color & 0xFF;

                    SDL_SetRenderDrawColor(renderer->renderer, r, g, b, a);
                    SDL_RenderFillRect(renderer->renderer, &bg_rect);

                    // Draw a debug "X" to indicate missing callback
                    SDL_SetRenderDrawColor(renderer->renderer, 255, 0, 0, 255);
                    SDL_RenderLine(renderer->renderer, bg_rect.x, bg_rect.y,
                                 bg_rect.x + bg_rect.w, bg_rect.y + bg_rect.h);
                    SDL_RenderLine(renderer->renderer, bg_rect.x + bg_rect.w, bg_rect.y,
                                 bg_rect.x, bg_rect.y + bg_rect.h);
                }
            }
            break;
        }

        case IR_COMPONENT_MARKDOWN:
            // Generic plugin component - dispatch to registered component renderer
            // No hardcoding of plugin names - routing based on component type
            {
                IRPluginBackendContext plugin_ctx = {
                    .renderer = renderer->renderer,
                    .font = renderer->default_font,
                    .user_data = NULL
                };
                if (!ir_plugin_dispatch_component_render(&plugin_ctx, component->type, component,
                                                         sdl_rect.x, sdl_rect.y, sdl_rect.w, sdl_rect.h)) {
                    // No plugin renderer registered - silently ignore or warn
                    // This allows graceful degradation if plugin is not installed
                }
            }
            break;

        case IR_COMPONENT_IMAGE: {
            // Image component - load and render image from src (stored in custom_data)
            const char* src = component->custom_data;
            if (src && strlen(src) > 0) {
                int img_width, img_height;
                SDL_Texture* texture = load_image_texture(renderer->renderer, src, &img_width, &img_height);
                if (texture) {
                    // Apply opacity
                    float opacity = inherited_opacity;
                    if (component->style) {
                        opacity *= component->style->opacity;
                    }
                    SDL_SetTextureAlphaMod(texture, (Uint8)(opacity * 255));

                    // Determine destination rect
                    SDL_FRect dest_rect;
                    dest_rect.x = sdl_rect.x;
                    dest_rect.y = sdl_rect.y;

                    // Use component dimensions if set, otherwise use image intrinsic size
                    if (component->style && component->style->width.value > 0) {
                        dest_rect.w = sdl_rect.w;
                    } else {
                        dest_rect.w = (float)img_width;
                    }
                    if (component->style && component->style->height.value > 0) {
                        dest_rect.h = sdl_rect.h;
                    } else {
                        dest_rect.h = (float)img_height;
                    }

                    // Render the image
                    SDL_RenderTexture(renderer->renderer, texture, NULL, &dest_rect);
                }
            }
            break;
        }

        case IR_COMPONENT_TAB_GROUP:
        case IR_COMPONENT_TAB_BAR:
        case IR_COMPONENT_TAB_CONTENT:
        case IR_COMPONENT_TAB_PANEL:
            // Tab containers - render as normal containers with background
            if (component->style) {
                float opacity = component->style->opacity * inherited_opacity;
                render_background(renderer->renderer, component, &sdl_rect, opacity);
            }
            break;

        case IR_COMPONENT_TABLE: {
            // Table container - render background and outer border
            IRTableState* table_state = (IRTableState*)component->custom_data;
            float opacity = component->style ? component->style->opacity * inherited_opacity : inherited_opacity;
            render_background(renderer->renderer, component, &sdl_rect, opacity);

            // Draw outer border if show_borders is enabled
            if (table_state && table_state->style.show_borders && table_state->style.border_width > 0) {
                IRColor border_color = table_state->style.border_color;
                SDL_SetRenderDrawColor(renderer->renderer,
                    border_color.data.r, border_color.data.g, border_color.data.b,
                    (uint8_t)(border_color.data.a * opacity));
                SDL_RenderRect(renderer->renderer, &sdl_rect);
            }
            break;
        }

        case IR_COMPONENT_TABLE_HEAD:
        case IR_COMPONENT_TABLE_BODY:
        case IR_COMPONENT_TABLE_FOOT:
            // Table sections - transparent containers
            break;

        case IR_COMPONENT_TABLE_ROW: {
            // Table row - render striped background if applicable
            IRComponent* table = component->parent ? component->parent->parent : NULL;
            if (table && table->type == IR_COMPONENT_TABLE) {
                IRTableState* table_state = (IRTableState*)table->custom_data;
                if (table_state && table_state->style.striped_rows) {
                    // Find row index to determine even/odd
                    IRComponent* section = component->parent;
                    int row_index = 0;
                    for (uint32_t i = 0; i < section->child_count; i++) {
                        if (section->children[i] == component) break;
                        if (section->children[i]->type == IR_COMPONENT_TABLE_ROW) row_index++;
                    }

                    // Only apply striping to body rows
                    if (section->type == IR_COMPONENT_TABLE_BODY) {
                        IRColor bg_color = (row_index % 2 == 0) ?
                            table_state->style.even_row_background :
                            table_state->style.odd_row_background;
                        float opacity = component->style ? component->style->opacity * inherited_opacity : inherited_opacity;
                        SDL_SetRenderDrawColor(renderer->renderer,
                            bg_color.data.r, bg_color.data.g, bg_color.data.b,
                            (uint8_t)(bg_color.data.a * opacity));
                        SDL_RenderFillRect(renderer->renderer, &sdl_rect);
                    }
                }
            }
            break;
        }

        case IR_COMPONENT_TABLE_HEADER_CELL: {
            // Header cell - render with header background
            IRComponent* table = NULL;
            IRComponent* p = component->parent;
            while (p) {
                if (p->type == IR_COMPONENT_TABLE) { table = p; break; }
                p = p->parent;
            }

            IRTableState* table_state = table ? (IRTableState*)table->custom_data : NULL;
            float opacity = component->style ? component->style->opacity * inherited_opacity : inherited_opacity;

            // Draw header background
            if (table_state) {
                IRColor bg_color = table_state->style.header_background;
                SDL_SetRenderDrawColor(renderer->renderer,
                    bg_color.data.r, bg_color.data.g, bg_color.data.b,
                    (uint8_t)(bg_color.data.a * opacity));
                SDL_RenderFillRect(renderer->renderer, &sdl_rect);

                // Draw cell border
                if (table_state->style.show_borders && table_state->style.border_width > 0) {
                    IRColor border_color = table_state->style.border_color;
                    SDL_SetRenderDrawColor(renderer->renderer,
                        border_color.data.r, border_color.data.g, border_color.data.b,
                        (uint8_t)(border_color.data.a * opacity));
                    SDL_RenderRect(renderer->renderer, &sdl_rect);
                }
            } else {
                render_background(renderer->renderer, component, &sdl_rect, opacity);
            }

            // Render header cell text content (bold white text)
            if (component->text_content) {
                float font_size = component->style && component->style->font.size > 0 ? component->style->font.size : 14.0f;
                TTF_Font* font = desktop_ir_resolve_font(renderer, component, font_size);
                if (font) {
                    SDL_Color text_color = {255, 255, 255, (uint8_t)(255 * opacity)};  // White text for headers
                    float cell_padding = table_state ? table_state->style.cell_padding : 8.0f;
                    render_text_with_shadow(renderer->renderer, font, component->text_content, text_color, component,
                                           sdl_rect.x + cell_padding, sdl_rect.y + cell_padding, sdl_rect.w - 2 * cell_padding);
                }
            }
            // Header cell rendering is complete - don't fall through to child rendering
            return true;
        }

        case IR_COMPONENT_TABLE_CELL: {
            // Regular cell - render background and border
            IRComponent* table = NULL;
            IRComponent* p = component->parent;
            int depth = 0;
            while (p && depth < 100) {
                if (p->type == IR_COMPONENT_TABLE) { table = p; break; }
                p = p->parent;
                depth++;
            }

            IRTableState* table_state = table ? (IRTableState*)table->custom_data : NULL;
            float opacity = component->style ? component->style->opacity * inherited_opacity : inherited_opacity;

            // Draw cell background (if explicitly set)
            render_background(renderer->renderer, component, &sdl_rect, opacity);

            // Draw cell border
            if (table_state && table_state->style.show_borders && table_state->style.border_width > 0) {
                IRColor border_color = table_state->style.border_color;
                SDL_SetRenderDrawColor(renderer->renderer,
                    border_color.data.r, border_color.data.g, border_color.data.b,
                    (uint8_t)(border_color.data.a * opacity));
                SDL_RenderRect(renderer->renderer, &sdl_rect);
            }

            // Render cell text content
            if (component->text_content) {
                float font_size = component->style && component->style->font.size > 0 ? component->style->font.size : 14.0f;
                TTF_Font* font = desktop_ir_resolve_font(renderer, component, font_size);
                if (font) {
                    SDL_Color text_color = component->style ? ir_color_to_sdl(component->style->font.color) : (SDL_Color){255, 255, 255, 255};
                    if (text_color.a == 0) text_color = (SDL_Color){255, 255, 255, 255};  // Default to white
                    text_color.a = (uint8_t)(text_color.a * opacity);
                    float cell_padding = table_state ? table_state->style.cell_padding : 8.0f;
                    render_text_with_shadow(renderer->renderer, font, component->text_content, text_color, component,
                                           sdl_rect.x + cell_padding, sdl_rect.y + cell_padding, sdl_rect.w - 2 * cell_padding);
                }
            }
            // Cell rendering is complete - don't fall through to child rendering
            return true;
        }


        // ========================================================================
        // Markdown Components (Core Support)
        // ========================================================================

        case IR_COMPONENT_HEADING: {
            // Render heading with larger font (already styled in builder)
            IRHeadingData* data = (IRHeadingData*)component->custom_data;
            if (data && data->text) {
                // Get font (heading already has size set by builder)
                float font_size = component->style && component->style->font.size > 0 ? component->style->font.size : 24.0f;
                TTF_Font* font = desktop_ir_resolve_font(renderer, component, font_size);

                if (font) {
                    // Get text color
                    SDL_Color text_color = component->style ?
                        ir_color_to_sdl(component->style->font.color) :
                        (SDL_Color){51, 51, 51, 255};
                    if (text_color.a == 0) {
                        text_color = (SDL_Color){51, 51, 51, 255};
                    }

                    // Apply opacity
                    if (component->style) {
                        float opacity = component->style->opacity * inherited_opacity;
                        text_color.a = (uint8_t)(text_color.a * opacity);
                    }

                    // Render text
                    render_text_with_shadow(renderer->renderer, font, data->text, text_color, component, sdl_rect.x, sdl_rect.y, sdl_rect.w);
                }
            }
            break;
        }

        case IR_COMPONENT_PARAGRAPH: {
            // Render paragraph text (children contain actual text components)
            // Paragraphs are containers - render background and let children render
            if (component->style) {
                float opacity = component->style->opacity * inherited_opacity;
                render_background(renderer->renderer, component, &sdl_rect, opacity);
            }
            break;
        }

        case IR_COMPONENT_BLOCKQUOTE: {
            // Render blockquote with left border and background
            // Light background
            SDL_SetRenderDrawColor(renderer->renderer, 240, 240, 240, 255);
            SDL_RenderFillRect(renderer->renderer, &sdl_rect);

            // Left border (4px wide, blue)
            SDL_FRect border = {rect.x, sdl_rect.y, 4, sdl_rect.h};
            SDL_SetRenderDrawColor(renderer->renderer, 100, 150, 200, 255);
            SDL_RenderFillRect(renderer->renderer, &border);
            break;
        }

        case IR_COMPONENT_CODE_BLOCK: {
            // Render code block with monospace font and dark background
            IRCodeBlockData* data = (IRCodeBlockData*)component->custom_data;

            // Dark background
            SDL_SetRenderDrawColor(renderer->renderer, 40, 44, 52, 255);
            SDL_RenderFillRect(renderer->renderer, &sdl_rect);

            // Render code text with monospace font
            if (data && data->code) {
                // Get font (TODO: Use monospace font when available)
                float font_size = component->style && component->style->font.size > 0 ? component->style->font.size : 14.0f;
                TTF_Font* font = desktop_ir_resolve_font(renderer, component, font_size);

                if (font) {
                    // Light text on dark background
                    SDL_Color text_color = (SDL_Color){220, 220, 220, 255};

                    // Apply opacity
                    if (component->style) {
                        float opacity = component->style->opacity * inherited_opacity;
                        text_color.a = (uint8_t)(text_color.a * opacity);
                    }

                    // Render text with padding
                    render_text_with_shadow(renderer->renderer, font, data->code, text_color, component, sdl_rect.x + 12, sdl_rect.y + 12, sdl_rect.w - 24);
                }
            }
            break;
        }

        case IR_COMPONENT_HORIZONTAL_RULE: {
            // Render horizontal line
            SDL_FRect line = {rect.x, sdl_rect.y + sdl_rect.h / 2, sdl_rect.w, 2};
            SDL_SetRenderDrawColor(renderer->renderer, 200, 200, 200, 255);
            SDL_RenderFillRect(renderer->renderer, &line);
            break;
        }

        case IR_COMPONENT_LIST: {
            // List container - just render background if any
            if (component->style) {
                float opacity = component->style->opacity * inherited_opacity;
                render_background(renderer->renderer, component, &sdl_rect, opacity);
            }
            break;
        }

        case IR_COMPONENT_LIST_ITEM: {
            // Render list item with bullet/number
            IRListItemData* data = (IRListItemData*)component->custom_data;

            // Get font
            float font_size = component->style && component->style->font.size > 0 ? component->style->font.size : 16.0f;
            TTF_Font* font = desktop_ir_resolve_font(renderer, component, font_size);

            if (font) {
                // Get text color
                SDL_Color text_color = component->style ?
                    ir_color_to_sdl(component->style->font.color) :
                    (SDL_Color){51, 51, 51, 255};
                if (text_color.a == 0) {
                    text_color = (SDL_Color){51, 51, 51, 255};
                }

                // Apply opacity
                if (component->style) {
                    float opacity = component->style->opacity * inherited_opacity;
                    text_color.a = (uint8_t)(text_color.a * opacity);
                }

                // Render bullet or number
                if (data && data->marker) {
                    render_text_with_shadow(renderer->renderer, font, data->marker, text_color, component, sdl_rect.x, sdl_rect.y, sdl_rect.w);
                } else if (data && data->number > 0) {
                    char number_str[16];
                    snprintf(number_str, sizeof(number_str), "%d.", data->number);
                    render_text_with_shadow(renderer->renderer, font, number_str, text_color, component, sdl_rect.x, sdl_rect.y, sdl_rect.w);
                } else {
                    // Default bullet
                    render_text_with_shadow(renderer->renderer, font, "•", text_color, component, sdl_rect.x, sdl_rect.y, sdl_rect.w);
                }
            }
            break;
        }

        case IR_COMPONENT_LINK: {
            // Render link text with underline
            TTF_Font* font = desktop_ir_resolve_font(renderer, component, component->style && component->style->font.size > 0 ? component->style->font.size : 16.0f);
            if (component->text_content && font) {
                SDL_Color text_color = component->style ?
                    ir_color_to_sdl(component->style->font.color) :
                    (SDL_Color){139, 148, 158, 255};  // Default link color

                // Apply component opacity to text
                if (component->style) {
                    float opacity = component->style->opacity * inherited_opacity;
                    text_color.a = (uint8_t)(text_color.a * opacity);
                }

                // Render link text
                render_text_with_shadow(renderer->renderer, font,
                                       component->text_content, text_color, component,
                                       sdl_rect.x, sdl_rect.y, sdl_rect.w);

                // Draw underline below text
                int text_width = 0, text_height = 0;
                TTF_GetStringSize(font, component->text_content, 0, &text_width, &text_height);
                SDL_FRect underline = {rect.x, sdl_rect.y + text_height, (float)text_width, 1};
                SDL_SetRenderDrawColor(renderer->renderer, text_color.r, text_color.g, text_color.b, text_color.a);
                SDL_RenderFillRect(renderer->renderer, &underline);
            }
            break;
        }

        default:
            SDL_RenderFillRect(renderer->renderer, &sdl_rect);
            break;
    }

    // Render children (layout already computed by ir_layout_compute_tree)
    LayoutRect child_rect = rect;

    // Padding already applied in ir_layout_compute_constraints() - no need to modify child_rect
    // Layout already computed by ir_layout_compute_tree() - just render children
    // Position tracking variables (will be deleted in Step 7)

    // Layout tracing (only when enabled)

    // Shortcut: if all children are absolutely positioned, render them by z-index without flex layout
    bool all_absolute = true;
    for (uint32_t i = 0; i < component->child_count; i++) {
        IRComponent* child = component->children[i];
        if (!child || !child->style || child->style->position_mode != IR_POSITION_ABSOLUTE) {
            all_absolute = false;
            break;
        }
    }

    if (all_absolute && component->child_count > 0) {
        typedef struct {
            IRComponent* child;
            uint32_t z;
        } AbsChild;
        AbsChild* abs_children = malloc(sizeof(AbsChild) * component->child_count);
        if (!abs_children) return false;
        for (uint32_t i = 0; i < component->child_count; i++) {
            IRComponent* child = component->children[i];
            uint32_t z = child && child->style ? child->style->z_index : 0;
            abs_children[i].child = child;
            abs_children[i].z = z;
        }
        // Sort by z ascending so higher z draws last
        int cmp_abs(const void* a, const void* b) {
            const AbsChild* ca = (const AbsChild*)a;
            const AbsChild* cb = (const AbsChild*)b;
            if (ca->z < cb->z) return -1;
            if (ca->z > cb->z) return 1;
            return 0;
        }
        qsort(abs_children, component->child_count, sizeof(AbsChild), cmp_abs);

        // Calculate cascaded opacity for children
        float child_opacity = (component->style ? component->style->opacity : 1.0f) * inherited_opacity;

        for (uint32_t i = 0; i < component->child_count; i++) {
            IRComponent* child = abs_children[i].child;
            if (!child) continue;
            // Skip invisible children
            if (child->style && !child->style->visible) continue;
            // Absolute children read from layout_state->computed (already has absolute coordinates)
            LayoutRect dummy_rect = {0};
            render_component_sdl3(renderer, child, dummy_rect, child_opacity);
        }
        free(abs_children);
        return true;
    }

    // Special handling for TABLE components - use table layout algorithm
    if (component->type == IR_COMPONENT_TABLE && component->child_count > 0) {
        // Table layout is already computed in Phase 2 and cached
        // Cell positions are already absolute from Phase 3
        // No need to recompute here (cache will prevent it anyway)

        // Calculate cascaded opacity for children
        float child_opacity = (component->style ? component->style->opacity : 1.0f) * inherited_opacity;

        // Render table sections (TableHead, TableBody, TableFoot)
        for (uint32_t i = 0; i < component->child_count; i++) {
            IRComponent* section = component->children[i];
            if (!section) continue;
            if (section->style && !section->style->visible) continue;
            if (!section->layout_state) continue;

            // Use the layout_state->computed set by ir_layout_compute_table
            LayoutRect section_rect = {
                .x = child_rect.x + section->layout_state->computed.x,
                .y = child_rect.y + section->layout_state->computed.y,
                .width = section->layout_state->computed.width > 0 ? section->layout_state->computed.width : child_rect.width,
                .height = section->layout_state->computed.height
            };

            // Render the section (transparent, just for structure)
            render_component_sdl3(renderer, section, section_rect, child_opacity);
        }
        return true;
    }

    // Special handling for TABLE sections (TableHead, TableBody, TableFoot) - render rows
    if ((component->type == IR_COMPONENT_TABLE_HEAD ||
         component->type == IR_COMPONENT_TABLE_BODY ||
         component->type == IR_COMPONENT_TABLE_FOOT) && component->child_count > 0) {
        float child_opacity = (component->style ? component->style->opacity : 1.0f) * inherited_opacity;

        for (uint32_t i = 0; i < component->child_count; i++) {
            IRComponent* row = component->children[i];
            if (!row) continue;
            if (row->style && !row->style->visible) continue;
            if (!row->layout_state) continue;

            LayoutRect row_rect = {
                .x = sdl_rect.x + row->layout_state->computed.x,
                .y = sdl_rect.y + row->layout_state->computed.y,
                .width = row->layout_state->computed.width > 0 ? row->layout_state->computed.width : sdl_rect.w,
                .height = row->layout_state->computed.height
            };

            render_component_sdl3(renderer, row, row_rect, child_opacity);
        }
        return true;
    }

    // Special handling for TABLE rows - render cells
    if (component->type == IR_COMPONENT_TABLE_ROW && component->child_count > 0) {
        float child_opacity = (component->style ? component->style->opacity : 1.0f) * inherited_opacity;

        for (uint32_t i = 0; i < component->child_count; i++) {
            IRComponent* cell = component->children[i];
            if (!cell) continue;
            if (cell->style && !cell->style->visible) continue;
            if (!cell->layout_state) continue;

            LayoutRect cell_rect = {
                .x = sdl_rect.x + cell->layout_state->computed.x,
                .y = sdl_rect.y + cell->layout_state->computed.y,
                .width = cell->layout_state->computed.width,
                .height = cell->layout_state->computed.height > 0 ? cell->layout_state->computed.height : sdl_rect.h
            };

            render_component_sdl3(renderer, cell, cell_rect, child_opacity);
        }
        return true;
    }

    // Track dragged tab info so we can paint it last (above siblings)
    TabGroupState* tab_bar_state = component->custom_data ? (TabGroupState*)component->custom_data : NULL;
    IRComponent* dragged_child = NULL;
    LayoutRect dragged_rect = {0};
    bool has_dragged_rect = false;

    // Calculate cascaded opacity for children
    float child_opacity = (component->style ? component->style->opacity : 1.0f) * inherited_opacity;

    for (uint32_t i = 0; i < component->child_count; i++) {
        IRComponent* child = component->children[i];

        // Skip invisible children entirely - don't render AND don't take up space
        if (child && child->style && !child->style->visible) {
            continue;
        }

        // Layout already computed by ir_layout_compute_tree() - no positioning needed
        // Child will read from layout_state->computed
        LayoutRect rect_for_child = {0};  // Dummy rect (not used by child)

        // If this is a tab bar, draw the dragged tab after all siblings so it appears on top
        bool is_dragged_tab = false;
        if (component->custom_data && child) {
            TabGroupState* tg_state = (TabGroupState*)component->custom_data;
            if (tg_state && tg_state->dragging && tg_state->drag_index == (int)i) {
                is_dragged_tab = true;
                // Defer rendering; we'll draw it after the loop with cursor-following offset
            }
        }

        if (is_dragged_tab) {
            dragged_child = child;
            dragged_rect = rect_for_child;
            has_dragged_rect = true;
        } else {
            render_component_sdl3(renderer, child, rect_for_child, child_opacity);
        }

    }

    // If this component is a tab bar with an active drag, render the dragged tab last so it follows the cursor
    if (tab_bar_state && has_dragged_rect && dragged_child) {
        // Apply cursor-following offset and render on top
        dragged_rect.x = tab_bar_state->drag_x - dragged_rect.width * 0.5f;
        render_component_sdl3(renderer, dragged_child, dragged_rect, child_opacity);
    }


    return true;
}

// Screenshot capture function
bool desktop_save_screenshot(DesktopIRRenderer* renderer, const char* path) {
    if (!renderer || !renderer->renderer || !path) {
        return false;
    }

    // Get renderer output size
    int width, height;
    if (!SDL_GetRenderOutputSize(renderer->renderer, &width, &height)) {
        fprintf(stderr, "Failed to get render output size: %s\n", SDL_GetError());
        return false;
    }

    // Read pixels from renderer
    SDL_Surface* surface = SDL_RenderReadPixels(renderer->renderer, NULL);
    if (!surface) {
        fprintf(stderr, "Failed to read pixels: %s\n", SDL_GetError());
        return false;
    }

    // Check file extension to determine format
    const char* ext = strrchr(path, '.');
    bool is_png = (ext && strcasecmp(ext, ".png") == 0);

    bool success = false;
    if (is_png) {
        // Use stb_image_write for PNG
        // Convert surface to RGBA format for stbi_write_png
        SDL_Surface* rgba_surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        if (rgba_surface) {
            int stride = rgba_surface->w * 4;
            success = stbi_write_png(path, rgba_surface->w, rgba_surface->h, 4,
                                      rgba_surface->pixels, stride) != 0;
            SDL_DestroySurface(rgba_surface);
        }
        if (!success) {
            fprintf(stderr, "Failed to save PNG screenshot: %s\n", path);
        }
    } else {
        // Use SDL_SaveBMP for BMP files
        success = SDL_SaveBMP(surface, path);
        if (!success) {
            fprintf(stderr, "Failed to save screenshot: %s\n", SDL_GetError());
        }
    }

    SDL_DestroySurface(surface);
    return success;
}

// Debug overlay rendering function
void desktop_render_debug_overlay(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !renderer->renderer || !root) {
        return;
    }

    // Simple implementation: just draw bounding boxes for now
    // TODO: Add labels with component type and ID
    (void)root;  // Unused for now
}

#endif  // ENABLE_SDL3
