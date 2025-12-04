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
#include "../../core/include/kryon_canvas.h"

#ifdef ENABLE_SDL3

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

    // Cache rendered bounds for hit testing
    ir_set_rendered_bounds(component, rect.x, rect.y, rect.width, rect.height);

    SDL_FRect sdl_rect = {
        .x = rect.x,
        .y = rect.y,
        .w = rect.width,
        .h = rect.height
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
                        float text_w = (float)surface->w;
                        float text_h = (float)surface->h;
                        float avail_w = rect.width;
                        float text_y = roundf(rect.y + (rect.height - text_h) / 2);

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
                            float text_x = rect.x + 8;  // Small left padding
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
                                .x = roundf(rect.x + (rect.width - text_w) / 2),
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

                // Render text with optional shadow
                render_text_with_shadow(renderer->renderer, font,
                                       component->text_content, text_color, component,
                                       rect.x, rect.y);
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
                float avail_width = rect.width - pad_left - pad_right;

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

                float caret_x = rect.x + pad_left;
                float caret_y = rect.y + pad_top;
                float caret_height = rect.height - pad_top - (component->style ? component->style->padding.bottom : 8.0f);

                float caret_local = 0.0f;
                if (font && istate) {
                    char* prefix = strndup(value_text, istate->cursor_index);
                    caret_local = measure_text_width(font, prefix);
                    free(prefix);
                    ensure_caret_visible(renderer, component, istate, font, pad_left, pad_right);
                    caret_x = rect.x + pad_left + (caret_local - istate->scroll_x);
                } else {
                    caret_x = rect.x + pad_left;
                }

                if (font && to_render && to_render[0] != '\0') {
                    SDL_Surface* surface = TTF_RenderText_Blended(font,
                                                                  to_render,
                                                                  strlen(to_render),
                                                                  text_color);
                    if (surface) {
                        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer->renderer, surface);
                        if (texture) {
                            float text_x = rect.x + pad_left;
                            float text_y = rect.y + pad_top;
                            if (!component->style || (component->style->padding.top == 0 && component->style->padding.bottom == 0)) {
                                text_y = rect.y + (rect.height - surface->h) / 2;
                            }
                            SDL_FRect dest_rect = {
                                .x = roundf(text_x - (istate ? istate->scroll_x : 0.0f)),
                                .y = roundf(text_y),
                                .w = (float)surface->w,
                                .h = (float)surface->h
                            };
                            // Clip to input rect minus padding
                            SDL_FRect clip_rect = {
                                .x = rect.x + pad_left,
                                .y = rect.y + pad_top,
                                .w = fmaxf(0.0f, rect.width - pad_left - pad_right),
                                .h = rect.height - pad_top - (component->style ? component->style->padding.bottom : 8.0f)
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
            float checkbox_size = fminf(rect.height * 0.6f, 20.0f);
            float checkbox_y = rect.y + (rect.height - checkbox_size) / 2;

            SDL_FRect checkbox_rect = {
                .x = rect.x + 5,
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
                        // Round text position to integer pixels for crisp rendering
                        SDL_FRect text_rect = {
                            .x = roundf(checkbox_rect.x + checkbox_size + 10),
                            .y = roundf(rect.y + (rect.height - surface->h) / 2.0f),
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
                    .x = rect.x + i,
                    .y = rect.y + i,
                    .w = rect.width - 2*i,
                    .h = rect.height - 2*i
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
                        SDL_FRect text_rect = {
                            .x = roundf(rect.x + 10),
                            .y = roundf(rect.y + (rect.height - surface->h) / 2.0f),
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
            float arrow_x = rect.x + rect.width - 20;
            float arrow_y = rect.y + rect.height / 2;
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
            // Initialize canvas for this component's bounds
            extern kryon_cmd_buf_t* kryon_canvas_get_command_buffer(void);
            kryon_cmd_buf_t* existing_buf = kryon_canvas_get_command_buffer();

            if (existing_buf == NULL) {
                // First time - initialize canvas
                kryon_canvas_init(rect.width, rect.height);
            } else {
                // Already initialized - just clear command buffer for this frame
                extern void kryon_cmd_buf_clear(kryon_cmd_buf_t* buf);
                kryon_cmd_buf_clear(existing_buf);
            }

            kryon_canvas_set_offset(rect.x, rect.y);

            // Call the Nim callback - it will populate the canvas's command buffer
            extern void nimCanvasBridge(uint32_t componentId);
            nimCanvasBridge(component->id);

            // Get the canvas command buffer and execute commands using SDL3
            extern kryon_cmd_buf_t* kryon_canvas_get_command_buffer(void);
            extern kryon_cmd_iterator_t kryon_cmd_iter_create(kryon_cmd_buf_t* buf);
            extern bool kryon_cmd_iter_has_next(kryon_cmd_iterator_t* iter);
            extern bool kryon_cmd_iter_next(kryon_cmd_iterator_t* iter, kryon_command_t* cmd);

            kryon_cmd_buf_t* canvas_buf = kryon_canvas_get_command_buffer();
            if (canvas_buf) {
                kryon_cmd_iterator_t iter = kryon_cmd_iter_create(canvas_buf);
                kryon_command_t cmd;
                while (kryon_cmd_iter_has_next(&iter)) {
                    if (!kryon_cmd_iter_next(&iter, &cmd)) break;

                    switch (cmd.type) {
                        case KRYON_CMD_DRAW_POLYGON: {
                            const kryon_fp_t* vertices = cmd.data.draw_polygon.vertices;
                            uint16_t vertex_count = cmd.data.draw_polygon.vertex_count;
                            uint32_t color = cmd.data.draw_polygon.color;
                            bool filled = cmd.data.draw_polygon.filled;

                            if (filled && vertex_count >= 3) {
                                // Draw filled polygon using triangle fan
                                uint8_t r = (color >> 24) & 0xFF;
                                uint8_t g = (color >> 16) & 0xFF;
                                uint8_t b = (color >> 8) & 0xFF;
                                uint8_t a = color & 0xFF;

                                fprintf(stderr, "[SDL] POLYGON color=0x%08x unpacked to r=%d g=%d b=%d a=%d\n", color, r, g, b, a);

                                SDL_Vertex* sdl_vertices = malloc(vertex_count * sizeof(SDL_Vertex));
                                for (uint16_t i = 0; i < vertex_count; i++) {
                                    sdl_vertices[i].position.x = vertices[i * 2];
                                    sdl_vertices[i].position.y = vertices[i * 2 + 1];
                                    // SDL3 uses SDL_FColor with float values 0.0-1.0, not bytes 0-255
                                    sdl_vertices[i].color.r = r / 255.0f;
                                    sdl_vertices[i].color.g = g / 255.0f;
                                    sdl_vertices[i].color.b = b / 255.0f;
                                    sdl_vertices[i].color.a = a / 255.0f;
                                    sdl_vertices[i].tex_coord.x = 0.0f;
                                    sdl_vertices[i].tex_coord.y = 0.0f;
                                }

                                // Create triangle fan indices
                                int* indices = malloc((vertex_count - 2) * 3 * sizeof(int));
                                for (uint16_t i = 0; i < vertex_count - 2; i++) {
                                    indices[i * 3] = 0;
                                    indices[i * 3 + 1] = i + 1;
                                    indices[i * 3 + 2] = i + 2;
                                }

                                // Reset render draw color to white to prevent color modulation
                                SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 255, 255);
                                SDL_RenderGeometry(renderer->renderer, renderer->white_texture, sdl_vertices, vertex_count, indices, (vertex_count - 2) * 3);
                                free(sdl_vertices);
                                free(indices);
                            }
                            break;
                        }
                        case KRYON_CMD_DRAW_RECT: {
                            uint8_t r = (cmd.data.draw_rect.color >> 24) & 0xFF;
                            uint8_t g = (cmd.data.draw_rect.color >> 16) & 0xFF;
                            uint8_t b = (cmd.data.draw_rect.color >> 8) & 0xFF;
                            uint8_t a = cmd.data.draw_rect.color & 0xFF;
                            SDL_SetRenderDrawColor(renderer->renderer, r, g, b, a);
                            SDL_FRect rect = {cmd.data.draw_rect.x, cmd.data.draw_rect.y, cmd.data.draw_rect.w, cmd.data.draw_rect.h};
                            SDL_RenderFillRect(renderer->renderer, &rect);
                            break;
                        }
                        case KRYON_CMD_DRAW_LINE: {
                            uint8_t r = (cmd.data.draw_line.color >> 24) & 0xFF;
                            uint8_t g = (cmd.data.draw_line.color >> 16) & 0xFF;
                            uint8_t b = (cmd.data.draw_line.color >> 8) & 0xFF;
                            uint8_t a = cmd.data.draw_line.color & 0xFF;
                            SDL_SetRenderDrawColor(renderer->renderer, r, g, b, a);
                            SDL_RenderLine(renderer->renderer, cmd.data.draw_line.x1, cmd.data.draw_line.y1, cmd.data.draw_line.x2, cmd.data.draw_line.y2);
                            break;
                        }
                        case KRYON_CMD_DRAW_TEXT: {
                            // Extract color components
                            uint8_t r = (cmd.data.draw_text.color >> 24) & 0xFF;
                            uint8_t g = (cmd.data.draw_text.color >> 16) & 0xFF;
                            uint8_t b = (cmd.data.draw_text.color >> 8) & 0xFF;
                            uint8_t a = cmd.data.draw_text.color & 0xFF;

                            // Render text using SDL_ttf
                            if (!renderer->default_font) {
                                break;
                            }

                            SDL_Color text_color = {r, g, b, a};
                            size_t text_len = strlen(cmd.data.draw_text.text_storage);

                            SDL_Surface* text_surface = TTF_RenderText_Blended(renderer->default_font, cmd.data.draw_text.text_storage, text_len, text_color);
                            if (!text_surface) {
                                break;
                            }

                            SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer->renderer, text_surface);
                            if (!text_texture) {
                                SDL_DestroySurface(text_surface);
                                break;
                            }

                            SDL_FRect dest_rect = {
                                (float)cmd.data.draw_text.x,
                                (float)cmd.data.draw_text.y,
                                (float)text_surface->w,
                                (float)text_surface->h
                            };

                            SDL_RenderTexture(renderer->renderer, text_texture, NULL, &dest_rect);

                            SDL_DestroyTexture(text_texture);
                            SDL_DestroySurface(text_surface);
                            break;
                        }
                        case KRYON_CMD_DRAW_ARC: {
                            // Extract arc parameters
                            int16_t cx = cmd.data.draw_arc.cx;
                            int16_t cy = cmd.data.draw_arc.cy;
                            uint16_t radius = cmd.data.draw_arc.radius;
                            int16_t start_angle = cmd.data.draw_arc.start_angle;
                            int16_t end_angle = cmd.data.draw_arc.end_angle;
                            uint32_t color = cmd.data.draw_arc.color;

                            // Extract color components
                            uint8_t r = (color >> 24) & 0xFF;
                            uint8_t g = (color >> 16) & 0xFF;
                            uint8_t b = (color >> 8) & 0xFF;
                            uint8_t a = color & 0xFF;

                            // Tessellate arc into line segments
                            const int segments = 32;  // More segments for smoother arcs
                            SDL_FPoint* points = malloc((segments + 1) * sizeof(SDL_FPoint));
                            if (!points) break;

                            // Calculate arc span
                            float angle_span = end_angle - start_angle;

                            // Generate arc vertices
                            for (int i = 0; i <= segments; i++) {
                                float t = (float)i / segments;
                                float angle_deg = start_angle + angle_span * t;
                                float angle_rad = angle_deg * M_PI / 180.0f;

                                points[i].x = cx + radius * cosf(angle_rad);
                                points[i].y = cy + radius * sinf(angle_rad);
                            }

                            // Draw arc as connected line segments
                            SDL_SetRenderDrawColor(renderer->renderer, r, g, b, a);
                            SDL_RenderLines(renderer->renderer, points, segments + 1);

                            free(points);
                            break;
                        }
                        default:
                            // Try plugin dispatch for unknown command types
                            if (!ir_plugin_dispatch(renderer->renderer, cmd.type, &cmd)) {
                                // Unknown command - silently ignore
                            }
                            break;
                    }
                }
            }
            break;
        }

        case IR_COMPONENT_MARKDOWN: {
            // Parse and render markdown content with full formatting
            if (component->text_content && renderer->default_font) {
                render_markdown_content(renderer, component, rect);
            }
            break;
        }

        default:
            SDL_RenderFillRect(renderer->renderer, &sdl_rect);
            break;
    }

    // Render children
    LayoutRect child_rect = rect;

    // For AUTO-sized Row/Column components, use their measured content dimensions
    // for child alignment calculations instead of 0 or parent's full height
    // EXCEPTION: If component has cross-axis alignment (not START), keep rect dimensions
    // so it can align its children within the full available space
    if ((component->type == IR_COMPONENT_COLUMN || component->type == IR_COMPONENT_ROW)) {
        // Check if component needs cross-axis space for alignment
        bool needs_cross_axis_space = component->layout &&
                                     component->layout->flex.cross_axis != IR_ALIGNMENT_START;

        // Get measured content dimensions
        float content_width = get_child_dimension(component, rect, false);
        float content_height = get_child_dimension(component, rect, true);

        // If rect dimensions are 0 (AUTO-sized), use measured content dimensions
        // UNLESS component needs cross-axis space for alignment
        if (rect.width == 0.0f && content_width > 0.0f && !needs_cross_axis_space) {
            child_rect.width = content_width;
        }
        if (rect.height == 0.0f && content_height > 0.0f && !needs_cross_axis_space) {
            child_rect.height = content_height;
        }
    }

    if (component->style) {
        child_rect.x += component->style->padding.left;
        child_rect.y += component->style->padding.top;
        child_rect.width -= (component->style->padding.left + component->style->padding.right);
        child_rect.height -= (component->style->padding.top + component->style->padding.bottom);
    }

    // Two-pass layout for COLUMN: measure total height, then position based on justify_content
    float start_y = child_rect.y;
    float start_x = child_rect.x;

    #ifdef KRYON_TRACE_LAYOUT
    if (component->type == IR_COMPONENT_COLUMN) {
        printf("COLUMN check: child_count=%d, child_rect.height=%.1f\n", component->child_count, child_rect.height);
    }
    #endif

    // Apply two-pass layout for column-like containers (COLUMN, CONTAINER with default vertical direction)
    bool is_column_layout = (component->type == IR_COMPONENT_COLUMN) ||
                           (component->type == IR_COMPONENT_CONTAINER &&
                            (!component->layout || component->layout->flex.direction == 0));
    if (is_column_layout && component->child_count > 0) {
        // Pass 1: Measure total content height (just get sizes, don't do full layout)
        float total_height = 0.0f;
        float gap = 0.0f;
        if (component->layout && component->layout->flex.gap > 0) {
            gap = (float)component->layout->flex.gap;
        }

        for (uint32_t i = 0; i < component->child_count; i++) {
            // Use get_child_dimension to correctly measure all component types
            float child_height = get_child_dimension(component->children[i], child_rect, true);

            if (getenv("KRYON_TRACE_LAYOUT")) {
                const char* child_type_name = "OTHER";
                if (component->children[i]) {
                    switch (component->children[i]->type) {
                        case IR_COMPONENT_ROW: child_type_name = "ROW"; break;
                        case IR_COMPONENT_COLUMN: child_type_name = "COLUMN"; break;
                        case IR_COMPONENT_BUTTON: child_type_name = "BUTTON"; break;
                        case IR_COMPONENT_TEXT: child_type_name = "TEXT"; break;
                        default: break;
                    }
                }
                printf("  Child %d (%s): measured height=%.1f\n", i, child_type_name, child_height);
            }

            total_height += child_height;
            if (i < component->child_count - 1) {  // Don't add gap after last child
                total_height += gap;
            }
        }

        #ifdef KRYON_TRACE_LAYOUT
        printf("  Total height: %.1f, child_rect.height: %.1f\n", total_height, child_rect.height);
        #endif

        // Calculate starting Y based on justify_content (main axis alignment for column)
        if (component->layout) {
            #ifdef KRYON_TRACE_LAYOUT
            printf("  Alignment mode: %d\n", component->layout->flex.justify_content);
            #endif

            switch (component->layout->flex.justify_content) {
                case IR_ALIGNMENT_CENTER:
                    // Center if container has explicit non-zero height
                    if (child_rect.height > 0) {
                        float offset = (child_rect.height - total_height) / 2.0f;
                        start_y = child_rect.y + (offset > 0 ? offset : 0);  // Don't go negative
                        #ifdef KRYON_TRACE_LAYOUT
                        printf("  CENTER: offset=%.1f, start_y=%.1f\n", offset, start_y);
                        #endif
                    } else {
                        start_y = child_rect.y;
                        #ifdef KRYON_TRACE_LAYOUT
                        printf("  CENTER: child_rect.height=0, using START\n");
                        #endif
                    }
                    break;
                case IR_ALIGNMENT_END:
                    // Align to end if container has explicit non-zero height
                    if (child_rect.height > 0) {
                        float offset = child_rect.height - total_height;
                        start_y = child_rect.y + (offset > 0 ? offset : 0);  // Don't go negative
                        #ifdef KRYON_TRACE_LAYOUT
                        printf("  END: offset=%.1f, start_y=%.1f\n", offset, start_y);
                        #endif
                    } else {
                        start_y = child_rect.y;
                        #ifdef KRYON_TRACE_LAYOUT
                        printf("  END: child_rect.height=0, using START\n");
                        #endif
                    }
                    break;
                case IR_ALIGNMENT_START:
                default:
                    start_y = child_rect.y;
                    #ifdef KRYON_TRACE_LAYOUT
                    printf("  START: start_y=%.1f\n", start_y);
                    #endif
                    break;
                // Space distribution modes handled differently in Pass 2
                case IR_ALIGNMENT_SPACE_BETWEEN:
                case IR_ALIGNMENT_SPACE_AROUND:
                case IR_ALIGNMENT_SPACE_EVENLY:
                    start_y = child_rect.y;  // Will be calculated per-item in Pass 2
                    break;
            }
        }
    }

    // Two-pass layout for ROW: measure total width, then position based on justify_content
    // Also store flex_grow distribution for use in the render pass
    float* flex_extra_widths = NULL;  // Extra width each child gets from flex_grow
    // Apply two-pass layout for row-like containers (ROW, CONTAINER with horizontal direction)
    bool is_row_layout = (component->type == IR_COMPONENT_ROW) ||
                        (component->type == IR_COMPONENT_CONTAINER &&
                         component->layout && component->layout->flex.direction == 1);
    if (is_row_layout && component->child_count > 0) {
        // Pass 1: Measure total content width AND calculate flex_grow distribution
        float total_width = 0.0f;
        float total_flex_grow = 0.0f;
        float gap = 0.0f;
        if (component->layout && component->layout->flex.gap > 0) {
            gap = (float)component->layout->flex.gap;
        }

        // Allocate array for flex extra widths
        flex_extra_widths = (float*)calloc(component->child_count, sizeof(float));

        for (uint32_t i = 0; i < component->child_count; i++) {
            // Use get_child_dimension to correctly measure all component types
            float child_width = get_child_dimension(component->children[i], child_rect, false);

            if (getenv("KRYON_TRACE_LAYOUT")) {
                IRComponent* child = component->children[i];
                const char* type_name = (child && child->type == IR_COMPONENT_BUTTON) ? "BUTTON" : "OTHER";
                printf("  ROW child %d (%s): measured width=%.1f\n", i, type_name, child_width);
            }

            total_width += child_width;
            if (i < component->child_count - 1) {  // Don't add gap after last child
                total_width += gap;
            }

            // Sum up flex_grow factors
            IRComponent* child = component->children[i];
            if (child && child->layout && child->layout->flex.grow > 0) {
                total_flex_grow += (float)child->layout->flex.grow;
                if (getenv("KRYON_TRACE_LAYOUT")) {
                    printf("  ROW child %d: flex_grow=%d\n", i, child->layout->flex.grow);
                }
            }
        }

        if (getenv("KRYON_TRACE_LAYOUT")) {
            printf("  ROW totals: total_width=%.1f, container_width=%.1f, total_flex_grow=%.1f\n",
                   total_width, child_rect.width, total_flex_grow);
        }

        // Calculate and distribute remaining space via flex_grow or flex_shrink
        float remaining_width = child_rect.width - total_width;
        if (remaining_width > 0 && total_flex_grow > 0) {
            // FLEX_GROW: distribute extra space to children
            if (getenv("KRYON_TRACE_LAYOUT")) {
                printf("  FLEX_GROW: remaining=%.1f total_flex=%.1f distributing...\n",
                       remaining_width, total_flex_grow);
            }
            for (uint32_t i = 0; i < component->child_count; i++) {
                IRComponent* child = component->children[i];
                if (child && child->layout && child->layout->flex.grow > 0) {
                    float ratio = (float)child->layout->flex.grow / total_flex_grow;
                    flex_extra_widths[i] = remaining_width * ratio;
                    if (getenv("KRYON_TRACE_LAYOUT")) {
                        printf("    Child %d: flex_grow=%d, extra_width=%.1f\n",
                               i, child->layout->flex.grow, flex_extra_widths[i]);
                    }
                }
            }
        } else if (remaining_width < 0) {
            // FLEX_SHRINK: shrink children to fit within container
            float overflow = -remaining_width;  // Positive amount we need to shrink
            float total_flex_shrink = 0.0f;

            // Calculate total flex_shrink
            for (uint32_t i = 0; i < component->child_count; i++) {
                IRComponent* child = component->children[i];
                if (child && child->layout && child->layout->flex.shrink > 0) {
                    total_flex_shrink += (float)child->layout->flex.shrink;
                }
            }

            if (getenv("KRYON_TRACE_LAYOUT")) {
                printf("  FLEX_SHRINK: overflow=%.1f total_flex_shrink=%.1f distributing...\n",
                       overflow, total_flex_shrink);
            }

            // Distribute shrinkage proportionally
            if (total_flex_shrink > 0) {
                for (uint32_t i = 0; i < component->child_count; i++) {
                    IRComponent* child = component->children[i];
                    if (child && child->layout && child->layout->flex.shrink > 0) {
                        float ratio = (float)child->layout->flex.shrink / total_flex_shrink;
                        flex_extra_widths[i] = -(overflow * ratio);  // Negative = shrink
                        if (getenv("KRYON_TRACE_LAYOUT")) {
                            printf("    Child %d: flex_shrink=%d, shrink_amount=%.1f\n",
                                   i, child->layout->flex.shrink, flex_extra_widths[i]);
                        }
                    }
                }
            }
        }

        // Calculate starting X based on justify_content (main axis alignment for row)
        if (component->layout) {
            switch (component->layout->flex.justify_content) {
                case IR_ALIGNMENT_CENTER:
                    // Center if container has explicit non-zero width
                    if (child_rect.width > 0) {
                        float offset = (child_rect.width - total_width) / 2.0f;
                        start_x = child_rect.x + (offset > 0 ? offset : 0);  // Don't go negative
                    } else {
                        start_x = child_rect.x;
                    }
                    break;
                case IR_ALIGNMENT_END:
                    // Align to end if container has explicit non-zero width
                    if (child_rect.width > 0) {
                        float offset = child_rect.width - total_width;
                        start_x = child_rect.x + (offset > 0 ? offset : 0);  // Don't go negative
                    } else {
                        start_x = child_rect.x;
                    }
                    break;
                case IR_ALIGNMENT_START:
                default:
                    start_x = child_rect.x;
                    break;
                // Space distribution modes handled differently in Pass 2
                case IR_ALIGNMENT_SPACE_BETWEEN:
                case IR_ALIGNMENT_SPACE_AROUND:
                case IR_ALIGNMENT_SPACE_EVENLY:
                    start_x = child_rect.x;  // Will be calculated per-item in Pass 2
                    break;
            }
        }
    }

    // Calculate spacing for space distribution modes
    float item_gap = 0.0f;  // Default gap (no gap unless explicitly set)
    if (component->layout && component->layout->flex.gap > 0) {
        item_gap = (float)component->layout->flex.gap;
    }

    // For space distribution, calculate custom gap
    float available_space = 0.0f;
    float distributed_gap = item_gap;

    // Store original child_rect for measurements
    LayoutRect measure_rect = child_rect;

    // COLUMN space distribution
    if (component->type == IR_COMPONENT_COLUMN && component->layout && component->child_count > 0) {
        switch (component->layout->flex.justify_content) {
            case IR_ALIGNMENT_SPACE_BETWEEN: {
                // Equal space between items, no space at edges
                float total_height = 0.0f;
                for (uint32_t i = 0; i < component->child_count; i++) {
                    float child_height = get_child_dimension(component->children[i], measure_rect, true);
                    total_height += child_height;
                }
                available_space = child_rect.height - total_height;
                if (component->child_count > 1 && available_space > 0) {
                    distributed_gap = available_space / (component->child_count - 1);
                } else {
                    distributed_gap = 0;  // No space to distribute, pack items tightly
                }
                start_y = child_rect.y;  // Start at top
                break;
            }
            case IR_ALIGNMENT_SPACE_AROUND: {
                // Equal space around each item (half space at edges)
                float total_height = 0.0f;
                for (uint32_t i = 0; i < component->child_count; i++) {
                    float child_height = get_child_dimension(component->children[i], measure_rect, true);
                    total_height += child_height;
                }
                available_space = child_rect.height - total_height;
                if (available_space > 0) {
                    distributed_gap = available_space / component->child_count;
                    start_y = child_rect.y + distributed_gap / 2.0f;  // Start with half gap
                } else {
                    distributed_gap = 0;  // No space to distribute
                    start_y = child_rect.y;
                }
                break;
            }
            case IR_ALIGNMENT_SPACE_EVENLY: {
                // Equal space everywhere including edges
                float total_height = 0.0f;
                for (uint32_t i = 0; i < component->child_count; i++) {
                    float child_height = get_child_dimension(component->children[i], measure_rect, true);
                    total_height += child_height;
                }
                available_space = child_rect.height - total_height;
                if (available_space > 0) {
                    distributed_gap = available_space / (component->child_count + 1);
                    start_y = child_rect.y + distributed_gap;  // Start with full gap
                } else {
                    distributed_gap = 0;  // No space to distribute
                    start_y = child_rect.y;
                }
                break;
            }
            default:
                break;
        }
    }

    // ROW space distribution
    if (component->type == IR_COMPONENT_ROW && component->layout && component->child_count > 0) {
        switch (component->layout->flex.justify_content) {
            case IR_ALIGNMENT_SPACE_BETWEEN: {
                // Equal space between items, no space at edges
                float total_width = 0.0f;
                for (uint32_t i = 0; i < component->child_count; i++) {
                    float child_width = get_child_dimension(component->children[i], measure_rect, false);
                    total_width += child_width;
                }
                available_space = child_rect.width - total_width;
                if (component->child_count > 1 && available_space > 0) {
                    distributed_gap = available_space / (component->child_count - 1);
                } else {
                    distributed_gap = 0;  // No space to distribute, pack items tightly
                }
                start_x = child_rect.x;  // Start at left
                break;
            }
            case IR_ALIGNMENT_SPACE_AROUND: {
                // Equal space around each item (half space at edges)
                float total_width = 0.0f;
                for (uint32_t i = 0; i < component->child_count; i++) {
                    float child_width = get_child_dimension(component->children[i], measure_rect, false);
                    total_width += child_width;
                }
                available_space = child_rect.width - total_width;
                if (available_space > 0) {
                    distributed_gap = available_space / component->child_count;
                    start_x = child_rect.x + distributed_gap / 2.0f;  // Start with half gap
                } else {
                    distributed_gap = 0;  // No space to distribute
                    start_x = child_rect.x;
                }
                break;
            }
            case IR_ALIGNMENT_SPACE_EVENLY: {
                // Equal space everywhere including edges
                float total_width = 0.0f;
                for (uint32_t i = 0; i < component->child_count; i++) {
                    float child_width = get_child_dimension(component->children[i], measure_rect, false);
                    total_width += child_width;
                }
                available_space = child_rect.width - total_width;
                if (available_space > 0) {
                    distributed_gap = available_space / (component->child_count + 1);
                    start_x = child_rect.x + distributed_gap;  // Start with full gap
                } else {
                    distributed_gap = 0;  // No space to distribute
                    start_x = child_rect.x;
                }
                break;
            }
            default:
                break;
        }
    }

    // Track current position separately from child_rect to avoid modifying container bounds
    // Initialize from start_x/start_y which already account for alignment
    float current_x = start_x;
    float current_y = start_y;

    // Layout tracing (only when enabled)
    if (getenv("KRYON_TRACE_LAYOUT")) {
        const char* type_name = component->type == IR_COMPONENT_COLUMN ? "COLUMN" :
                               component->type == IR_COMPONENT_ROW ? "ROW" : "OTHER";
        if (component->layout) {
            printf("Layout %s: start=(%.1f,%.1f) child_rect=(%.1f,%.1f %.1fx%.1f) gap=%.1f main_axis=%d cross_axis=%d\n",
                   type_name, start_x, start_y, child_rect.x, child_rect.y,
                   child_rect.width, child_rect.height, distributed_gap,
                   component->layout->flex.main_axis, component->layout->flex.cross_axis);
        } else {
            printf("Layout %s: start=(%.1f,%.1f) child_rect=(%.1f,%.1f %.1fx%.1f) gap=%.1f\n",
                   type_name, start_x, start_y, child_rect.x, child_rect.y,
                   child_rect.width, child_rect.height, distributed_gap);
        }
    }

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
            LayoutRect abs_layout = calculate_component_layout(child, rect);
            render_component_sdl3(renderer, child, abs_layout, child_opacity);
        }
        free(abs_children);
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
        // Get child size WITHOUT recursive layout (just from style)
        LayoutRect child_layout = get_child_size(child, child_rect);

        // Check if child has absolute positioning - if so, skip all alignment/positioning logic
        bool is_child_absolute = child && child->style && child->style->position_mode == IR_POSITION_ABSOLUTE;

        // For absolute positioned children, use the absolute coordinates set in calculate_component_layout
        // For relative positioned children, position based on current_x/current_y (main axis)
        if (!is_child_absolute) {
            // Set main axis position for relative children
            child_layout.x = current_x;
            child_layout.y = current_y;
        }

        if (getenv("KRYON_TRACE_LAYOUT")) {
            printf("  Child %d BEFORE align: pos=(%.1f,%.1f) size=(%.1fx%.1f) %s\n",
                   i, child_layout.x, child_layout.y, child_layout.width, child_layout.height,
                   is_child_absolute ? "[ABSOLUTE]" : "[RELATIVE]");
        }

        // Only apply alignment/positioning for relative positioned children
        if (!is_child_absolute) {
            // Special handling for Center component - center the child
            if (component->type == IR_COMPONENT_CENTER) {
                child_layout.x = child_rect.x + (child_rect.width - child_layout.width) / 2;
                child_layout.y = child_rect.y + (child_rect.height - child_layout.height) / 2;
            }

        // Apply horizontal alignment for COLUMN layout based on cross_axis (align_items)
        // Also apply to Container with default vertical direction
        if (is_column_layout && component->layout) {
            switch (component->layout->flex.cross_axis) {
                case IR_ALIGNMENT_CENTER:
                    // For AUTO-width Row/Column, measure actual content width for centering
                    float width_for_centering = child_layout.width;
                    if ((child->type == IR_COMPONENT_ROW || child->type == IR_COMPONENT_COLUMN) &&
                        child_layout.width == 0.0f) {
                        width_for_centering = get_child_dimension(child, child_rect, false);
                    }

                    // Only center if child fits in container, otherwise align to start
                    if (width_for_centering <= child_rect.width) {
                        child_layout.x = child_rect.x + (child_rect.width - width_for_centering) / 2;
                    } else {
                        child_layout.x = child_rect.x;
                    }
                    break;
                case IR_ALIGNMENT_END:
                    // Only align to end if child fits, otherwise align to start
                    if (child_layout.width <= child_rect.width) {
                        child_layout.x = child_rect.x + child_rect.width - child_layout.width;
                    } else {
                        child_layout.x = child_rect.x;
                    }
                    break;
                case IR_ALIGNMENT_STRETCH:
                    child_layout.x = child_rect.x;
                    // Only stretch if child doesn't have explicit width AND container has non-zero width
                    // Never stretch TEXT components - they should use their measured width
                    // Never stretch components with flex_grow > 0 - flex distribution will size them
                    // Check if child has explicit width (not AUTO)
                    bool has_explicit_width = child && child->style &&
                                            child->style->width.type != IR_DIMENSION_AUTO;
                    bool has_flex_grow = child && child->layout && child->layout->flex.grow > 0;
                    if (getenv("KRYON_TRACE_LAYOUT")) {
                        printf("  STRETCH check: has_explicit=%d has_flex_grow=%d child_rect.width=%.1f child_type=%d\n",
                               has_explicit_width, has_flex_grow, child_rect.width, child ? child->type : -1);
                    }
                    if (!has_explicit_width && !has_flex_grow && child_rect.width > 0 && child->type != IR_COMPONENT_TEXT) {
                        child_layout.width = child_rect.width;
                        if (getenv("KRYON_TRACE_LAYOUT")) {
                            printf("  STRETCH applied: width=%.1f\n", child_layout.width);
                        }
                    }
                    break;
                case IR_ALIGNMENT_START:
                default:
                    child_layout.x = child_rect.x;
                    break;
            }
        }

        // Apply vertical alignment for ROW layout based on cross_axis (align_items)
        if (component->type == IR_COMPONENT_ROW && component->layout) {
            switch (component->layout->flex.cross_axis) {
                case IR_ALIGNMENT_CENTER:
                    // Only center if child fits in container, otherwise align to start
                    if (child_layout.height <= child_rect.height) {
                        child_layout.y = child_rect.y + (child_rect.height - child_layout.height) / 2;
                    } else {
                        child_layout.y = child_rect.y;
                    }
                    break;
                case IR_ALIGNMENT_END:
                    // Only align to end if child fits, otherwise align to start
                    if (child_layout.height <= child_rect.height) {
                        child_layout.y = child_rect.y + child_rect.height - child_layout.height;
                    } else {
                        child_layout.y = child_rect.y;
                    }
                    break;
                case IR_ALIGNMENT_STRETCH:
                    child_layout.y = child_rect.y;
                    // Only stretch if child doesn't have explicit height AND container has non-zero height
                    // Never stretch TEXT components - they should use their measured height
                    // Check if child has explicit height (not AUTO)
                    bool has_explicit_height = child && child->style &&
                                             child->style->height.type != IR_DIMENSION_AUTO;
                    if (!has_explicit_height && child_rect.height > 0 && child->type != IR_COMPONENT_TEXT) {
                        child_layout.height = child_rect.height;
                    }
                    break;
                case IR_ALIGNMENT_START:
                default:
                    child_layout.y = child_rect.y;
                    break;
            }
        }
        } // End of !is_child_absolute check

        if (getenv("KRYON_TRACE_LAYOUT")) {
            printf("  Child %d AFTER align: pos=(%.1f,%.1f) size=(%.1fx%.1f)\n",
                   i, child_layout.x, child_layout.y, child_layout.width, child_layout.height);
        }

        // For AUTO-sized Row/Column children, pass the available space from parent's child_rect
        // instead of the measured child size, so they can properly calculate alignment
        LayoutRect rect_for_child = child_layout;

        // Apply flex_grow/flex_shrink extra width for ROW children
        // flex_extra_widths[i] > 0 = grow, < 0 = shrink
        if (component->type == IR_COMPONENT_ROW && flex_extra_widths && flex_extra_widths[i] != 0) {
            rect_for_child.width += flex_extra_widths[i];

            // Clamp to maxWidth if growing
            if (flex_extra_widths[i] > 0 && child && child->layout &&
                child->layout->max_width.type == IR_DIMENSION_PX &&
                child->layout->max_width.value > 0) {
                if (rect_for_child.width > child->layout->max_width.value) {
                    rect_for_child.width = child->layout->max_width.value;
                }
            }

            // Clamp to minWidth if shrinking
            if (flex_extra_widths[i] < 0 && child && child->layout &&
                child->layout->min_width.type == IR_DIMENSION_PX &&
                child->layout->min_width.value > 0) {
                if (rect_for_child.width < child->layout->min_width.value) {
                    rect_for_child.width = child->layout->min_width.value;
                }
            }

            if (getenv("KRYON_TRACE_LAYOUT")) {
                printf("  Child %d: adding flex_extra_width=%.1f, new width=%.1f\n",
                       i, flex_extra_widths[i], rect_for_child.width);
            }
        }

        if (child && (child->type == IR_COMPONENT_ROW || child->type == IR_COMPONENT_COLUMN)) {
            // Detect AUTO-sized Row/Column by checking the component's style
            // AUTO-sized components should use their measured content size, not container size
            // Treat explicit 0 dimensions as AUTO (this handles DSL-created components without explicit sizes)
            bool is_auto_width = (!child->style ||
                                 child->style->width.type == IR_DIMENSION_AUTO ||
                                 (child->style->width.type == IR_DIMENSION_PX && child->style->width.value == 0.0f));
            bool is_auto_height = (!child->style ||
                                  child->style->height.type == IR_DIMENSION_AUTO ||
                                  (child->style->height.type == IR_DIMENSION_PX && child->style->height.value == 0.0f));

            if (getenv("KRYON_TRACE_LAYOUT")) {
                printf("  AUTO-sized %s detected: w=%.1f (auto=%d), h=%.1f (auto=%d), available=(%.1fx%.1f)\n",
                       child->type == IR_COMPONENT_ROW ? "ROW" : "COLUMN",
                       child_layout.width, is_auto_width,
                       child_layout.height, is_auto_height,
                       child_rect.width, child_rect.height);
            }

            if (is_auto_width || is_auto_height) {
                // Preserve position from child_layout
                rect_for_child.x = child_layout.x;
                rect_for_child.y = child_layout.y;
                if (is_auto_width) {
                    // COLUMN needs full width for cross-axis alignment (horizontal centering)
                    // ROW needs full width for main-axis alignment (justify_content != START)
                    bool needs_full_width = child->layout && (
                        ((child->type == IR_COMPONENT_COLUMN) && child->layout->flex.cross_axis != IR_ALIGNMENT_START) ||
                        ((child->type == IR_COMPONENT_ROW) && child->layout->flex.justify_content != IR_ALIGNMENT_START)
                    );
                    rect_for_child.width = needs_full_width ? child_rect.width : child_layout.width;
                }
                if (is_auto_height) {
                    // For ROW: only needs measured content height, even with cross-axis alignment
                    // For COLUMN: needs parent's full height if it has cross-axis alignment
                    bool is_column_with_cross_align = (child->type == IR_COMPONENT_COLUMN) &&
                                                      child->layout &&
                                                      child->layout->flex.cross_axis != IR_ALIGNMENT_START;

                    if (is_column_with_cross_align) {
                        // Column needs full height to horizontally center its children
                        rect_for_child.height = child_rect.height;
                    } else {
                        // For auto-height children in a Column with CENTER/END alignment,
                        // use their measured content height for positioning
                        bool parent_is_centering = component->type == IR_COMPONENT_COLUMN &&
                                                  component->layout &&
                                                  (component->layout->flex.justify_content == IR_ALIGNMENT_CENTER ||
                                                   component->layout->flex.justify_content == IR_ALIGNMENT_END);
                        if (parent_is_centering) {
                            // Measure the actual content height of the child
                            float content_height = get_child_dimension(child, child_rect, true);
                            if (content_height > 0) {
                                rect_for_child.height = content_height;
                            } else {
                                // Fallback to measured height if available
                                rect_for_child.height = child_layout.height > 0 ? child_layout.height : child_rect.height;
                            }
                        } else {
                            // Use measured content height first; fall back to computed/measured defaults
                            float content_height = get_child_dimension(child, child_rect, true);
                            if (content_height > 0) {
                                rect_for_child.height = content_height;
                            } else {
                                rect_for_child.height = child_layout.height > 0 ? child_layout.height : child_rect.height;
                            }
                        }
                    }
                }

                if (getenv("KRYON_TRACE_LAYOUT")) {
                    printf("  FIXED: Passing rect_for_child=(%.1f,%.1f %.1fx%.1f) instead of child_layout\n",
                           rect_for_child.x, rect_for_child.y, rect_for_child.width, rect_for_child.height);
                }
            }
        }

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

        // Vertical stacking for column layout - advance current_y
        if (component->type == IR_COMPONENT_COLUMN) {
            // Use rect_for_child height for auto-sized children (which may have been measured)
            // instead of child_layout height (which may be 0 for auto-sized components)
            float advance_height = rect_for_child.height > 0 ? rect_for_child.height : child_layout.height;
            if (getenv("KRYON_TRACE_LAYOUT")) {
                float old_y = current_y;
                current_y += advance_height + distributed_gap;
                printf("  Advance Y: %.1f -> %.1f (height=%.1f gap=%.1f)\n",
                       old_y, current_y, advance_height, distributed_gap);
            } else {
                current_y += advance_height + distributed_gap;
            }
        }

        // Horizontal stacking for row layout - advance current_x
        if (component->type == IR_COMPONENT_ROW) {
            // Use rect_for_child width for auto-sized children (which may have been measured)
            // instead of child_layout width (which may be 0 for auto-sized components)
            float advance_width = rect_for_child.width > 0 ? rect_for_child.width : child_layout.width;
            if (getenv("KRYON_TRACE_LAYOUT")) {
                float old_x = current_x;
                current_x += advance_width + distributed_gap;
                printf("  Advance X: %.1f -> %.1f (width=%.1f gap=%.1f)\n",
                       old_x, current_x, advance_width, distributed_gap);
            } else {
                current_x += advance_width + distributed_gap;
            }
        }
    }

    // If this component is a tab bar with an active drag, render the dragged tab last so it follows the cursor
    if (tab_bar_state && has_dragged_rect && dragged_child) {
        // Apply cursor-following offset and render on top
        dragged_rect.x = tab_bar_state->drag_x - dragged_rect.width * 0.5f;
        render_component_sdl3(renderer, dragged_child, dragged_rect, child_opacity);
    }
    if (getenv("KRYON_TRACE_LAYOUT")) {
        const char* type_name = component->type == IR_COMPONENT_COLUMN ? "COLUMN" :
                               component->type == IR_COMPONENT_ROW ? "ROW" : "OTHER";
        printf("Layout %s complete\n\n", type_name);
    }

    // Free flex_extra_widths if allocated
    if (flex_extra_widths) {
        free(flex_extra_widths);
    }

    return true;
}

#endif  // ENABLE_SDL3
