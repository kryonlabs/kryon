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
