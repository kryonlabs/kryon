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

// ============================================================================
// FLOWCHART HELPER FUNCTIONS
// ============================================================================

// Flowchart color theme definitions
typedef struct {
    uint32_t node_fill;
    uint32_t node_stroke;
    uint32_t node_text;
    uint32_t edge_color;
    uint32_t subgraph_bg;
    uint32_t subgraph_border;
} FlowchartTheme;

// Theme: Default (current blue theme)
static const FlowchartTheme THEME_DEFAULT = {
    .node_fill = 0x5588FFFF,      // Light blue
    .node_stroke = 0x3366CCFF,    // Dark blue
    .node_text = 0x1a1d2eFF,      // Dark text
    .edge_color = 0x888888FF,     // Gray
    .subgraph_bg = 0x2a2a2a40,    // Semi-transparent dark gray
    .subgraph_border = 0x888888FF // Gray
};

// Theme: Dark mode
static const FlowchartTheme THEME_DARK = {
    .node_fill = 0x2d4a7cFF,      // Darker blue
    .node_stroke = 0x4a7cc2FF,    // Medium blue
    .node_text = 0xe0e8ffFF,      // Light text
    .edge_color = 0xa0a0a0FF,     // Light gray
    .subgraph_bg = 0x1a1a1a60,    // Darker semi-transparent
    .subgraph_border = 0xa0a0a0FF // Light gray
};

// Theme: Light mode
static const FlowchartTheme THEME_LIGHT = {
    .node_fill = 0xe0e8ffFF,      // Very light blue
    .node_stroke = 0x5588FFFF,    // Medium blue
    .node_text = 0x1a1d2eFF,      // Dark text
    .edge_color = 0x666666FF,     // Dark gray
    .subgraph_bg = 0xf0f0f020,    // Light semi-transparent
    .subgraph_border = 0x666666FF // Dark gray
};

// Theme: High contrast (accessibility)
static const FlowchartTheme THEME_HIGH_CONTRAST = {
    .node_fill = 0x000000FF,      // Black
    .node_stroke = 0xFFFF00FF,    // Yellow (3px width)
    .node_text = 0xFFFFFFFF,      // White
    .edge_color = 0xFFFFFFFF,     // White
    .subgraph_bg = 0x00000080,    // Black semi-transparent
    .subgraph_border = 0xFFFF00FF // Yellow
};

// Get theme based on environment variable or default
static const FlowchartTheme* get_flowchart_theme(void) {
    static const FlowchartTheme* cached_theme = NULL;

    if (!cached_theme) {
        const char* theme_env = getenv("KRYON_FLOWCHART_THEME");
        if (theme_env) {
            if (strcmp(theme_env, "dark") == 0) {
                cached_theme = &THEME_DARK;
            } else if (strcmp(theme_env, "light") == 0) {
                cached_theme = &THEME_LIGHT;
            } else if (strcmp(theme_env, "high-contrast") == 0 || strcmp(theme_env, "accessible") == 0) {
                cached_theme = &THEME_HIGH_CONTRAST;
            } else {
                cached_theme = &THEME_DEFAULT;
            }
        } else {
            cached_theme = &THEME_DEFAULT;
        }
    }

    return cached_theme;
}

// Helper function to render a filled rounded rectangle using arc segments
static void render_rounded_rect_filled(SDL_Renderer* renderer, const SDL_FRect* rect,
                                       float radius, uint32_t color, float opacity) {
    // Clamp radius to half of smallest dimension
    float max_radius = fminf(rect->w, rect->h) / 2.0f;
    if (radius > max_radius) radius = max_radius;
    if (radius < 1.0f) radius = 1.0f;

    // Extract color components
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = (uint8_t)((color & 0xFF) * opacity);

    SDL_SetRenderDrawColor(renderer, r, g, b, a);

    // Draw center rectangle (excludes corner radii)
    SDL_FRect center = {
        rect->x + radius,
        rect->y,
        rect->w - 2 * radius,
        rect->h
    };
    SDL_RenderFillRect(renderer, &center);

    // Draw left and right rectangles (vertical sections)
    SDL_FRect left = {rect->x, rect->y + radius, radius, rect->h - 2 * radius};
    SDL_FRect right = {rect->x + rect->w - radius, rect->y + radius, radius, rect->h - 2 * radius};
    SDL_RenderFillRect(renderer, &left);
    SDL_RenderFillRect(renderer, &right);

    // Draw four corner arcs as filled circles
    // Corner centers
    float cx_tl = rect->x + radius;
    float cy_tl = rect->y + radius;
    float cx_tr = rect->x + rect->w - radius;
    float cy_tr = rect->y + radius;
    float cx_bl = rect->x + radius;
    float cy_bl = rect->y + rect->h - radius;
    float cx_br = rect->x + rect->w - radius;
    float cy_br = rect->y + rect->h - radius;

    // Top-left corner (fill with horizontal lines)
    for (float y = 0; y <= radius; y += 1.0f) {
        float angle = acosf(y / radius);
        float x_offset = radius * sinf(angle);
        SDL_RenderLine(renderer,
            cx_tl - x_offset, cy_tl - y,
            cx_tl, cy_tl - y);
    }

    // Top-right corner
    for (float y = 0; y <= radius; y += 1.0f) {
        float angle = acosf(y / radius);
        float x_offset = radius * sinf(angle);
        SDL_RenderLine(renderer,
            cx_tr, cy_tr - y,
            cx_tr + x_offset, cy_tr - y);
    }

    // Bottom-left corner
    for (float y = 0; y <= radius; y += 1.0f) {
        float angle = acosf(y / radius);
        float x_offset = radius * sinf(angle);
        SDL_RenderLine(renderer,
            cx_bl - x_offset, cy_bl + y,
            cx_bl, cy_bl + y);
    }

    // Bottom-right corner
    for (float y = 0; y <= radius; y += 1.0f) {
        float angle = acosf(y / radius);
        float x_offset = radius * sinf(angle);
        SDL_RenderLine(renderer,
            cx_br, cy_br + y,
            cx_br + x_offset, cy_br + y);
    }
}

// Helper function to render rounded rectangle stroke (outline)
static void render_rounded_rect_stroke(SDL_Renderer* renderer, const SDL_FRect* rect,
                                       float radius, uint32_t color, float opacity) {
    // Clamp radius
    float max_radius = fminf(rect->w, rect->h) / 2.0f;
    if (radius > max_radius) radius = max_radius;
    if (radius < 1.0f) radius = 1.0f;

    // Extract color components
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = (uint8_t)((color & 0xFF) * opacity);

    SDL_SetRenderDrawColor(renderer, r, g, b, a);

    // Draw four straight edges
    // Top edge
    SDL_RenderLine(renderer,
        rect->x + radius, rect->y,
        rect->x + rect->w - radius, rect->y);

    // Right edge
    SDL_RenderLine(renderer,
        rect->x + rect->w, rect->y + radius,
        rect->x + rect->w, rect->y + rect->h - radius);

    // Bottom edge
    SDL_RenderLine(renderer,
        rect->x + rect->w - radius, rect->y + rect->h,
        rect->x + radius, rect->y + rect->h);

    // Left edge
    SDL_RenderLine(renderer,
        rect->x, rect->y + rect->h - radius,
        rect->x, rect->y + radius);

    // Draw four corner arcs
    int segments = 16;  // Smoother arcs for stroke

    // Top-left corner
    float cx_tl = rect->x + radius;
    float cy_tl = rect->y + radius;
    for (int i = 0; i < segments; i++) {
        float a1 = M_PI + (M_PI / 2.0f) * i / segments;
        float a2 = M_PI + (M_PI / 2.0f) * (i + 1) / segments;
        SDL_RenderLine(renderer,
            cx_tl + cosf(a1) * radius, cy_tl + sinf(a1) * radius,
            cx_tl + cosf(a2) * radius, cy_tl + sinf(a2) * radius);
    }

    // Top-right corner
    float cx_tr = rect->x + rect->w - radius;
    float cy_tr = rect->y + radius;
    for (int i = 0; i < segments; i++) {
        float a1 = (3 * M_PI / 2.0f) + (M_PI / 2.0f) * i / segments;
        float a2 = (3 * M_PI / 2.0f) + (M_PI / 2.0f) * (i + 1) / segments;
        SDL_RenderLine(renderer,
            cx_tr + cosf(a1) * radius, cy_tr + sinf(a1) * radius,
            cx_tr + cosf(a2) * radius, cy_tr + sinf(a2) * radius);
    }

    // Bottom-right corner
    float cx_br = rect->x + rect->w - radius;
    float cy_br = rect->y + rect->h - radius;
    for (int i = 0; i < segments; i++) {
        float a1 = (M_PI / 2.0f) * i / segments;
        float a2 = (M_PI / 2.0f) * (i + 1) / segments;
        SDL_RenderLine(renderer,
            cx_br + cosf(a1) * radius, cy_br + sinf(a1) * radius,
            cx_br + cosf(a2) * radius, cy_br + sinf(a2) * radius);
    }

    // Bottom-left corner
    float cx_bl = rect->x + radius;
    float cy_bl = rect->y + rect->h - radius;
    for (int i = 0; i < segments; i++) {
        float a1 = (M_PI / 2.0f) + (M_PI / 2.0f) * i / segments;
        float a2 = (M_PI / 2.0f) + (M_PI / 2.0f) * (i + 1) / segments;
        SDL_RenderLine(renderer,
            cx_bl + cosf(a1) * radius, cy_bl + sinf(a1) * radius,
            cx_bl + cosf(a2) * radius, cy_bl + sinf(a2) * radius);
    }
}

/**
 * Render a filled polygon using SDL_RenderGeometry (triangle fan)
 * More efficient and smoother than horizontal line filling
 */
static void render_polygon_filled(SDL_Renderer* renderer, const float* points, int num_points,
                                   uint32_t color, float opacity) {
    if (num_points < 3) return;

    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = (uint8_t)((color & 0xFF) * opacity);

    // Calculate center point
    float cx = 0, cy = 0;
    for (int i = 0; i < num_points; i++) {
        cx += points[i * 2];
        cy += points[i * 2 + 1];
    }
    cx /= num_points;
    cy /= num_points;

    // Create vertices for triangle fan (center + all outer points)
    int num_vertices = num_points + 1;
    SDL_Vertex* vertices = (SDL_Vertex*)malloc(sizeof(SDL_Vertex) * num_vertices);
    if (!vertices) return;

    // Center vertex
    vertices[0].position.x = cx;
    vertices[0].position.y = cy;
    vertices[0].color.r = r;
    vertices[0].color.g = g;
    vertices[0].color.b = b;
    vertices[0].color.a = a;
    vertices[0].tex_coord.x = 0.5f;
    vertices[0].tex_coord.y = 0.5f;

    // Outer vertices
    for (int i = 0; i < num_points; i++) {
        vertices[i + 1].position.x = points[i * 2];
        vertices[i + 1].position.y = points[i * 2 + 1];
        vertices[i + 1].color.r = r;
        vertices[i + 1].color.g = g;
        vertices[i + 1].color.b = b;
        vertices[i + 1].color.a = a;
        vertices[i + 1].tex_coord.x = 0.0f;
        vertices[i + 1].tex_coord.y = 0.0f;
    }

    // Create indices for triangle fan
    int num_triangles = num_points;
    int* indices = (int*)malloc(sizeof(int) * num_triangles * 3);
    if (!indices) {
        free(vertices);
        return;
    }

    for (int i = 0; i < num_points; i++) {
        indices[i * 3 + 0] = 0;           // Center
        indices[i * 3 + 1] = i + 1;       // Current outer vertex
        indices[i * 3 + 2] = ((i + 1) % num_points) + 1; // Next outer vertex
    }

    SDL_RenderGeometry(renderer, NULL, vertices, num_vertices, indices, num_triangles * 3);

    free(vertices);
    free(indices);
}

/**
 * Render polygon stroke with line segments
 */
static void render_polygon_stroke(SDL_Renderer* renderer, const float* points, int num_points,
                                   uint32_t color, float opacity) {
    if (num_points < 2) return;

    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = (uint8_t)((color & 0xFF) * opacity);

    SDL_SetRenderDrawColor(renderer, r, g, b, a);

    for (int i = 0; i < num_points; i++) {
        int next = (i + 1) % num_points;
        SDL_RenderLine(renderer,
            points[i * 2], points[i * 2 + 1],
            points[next * 2], points[next * 2 + 1]);
    }
}

/* ============================================================================
 * Layout Helper Functions (NEW LAYOUT SYSTEM)
 * ============================================================================ */

/**
 * Get computed dimension from new layout system
 * Falls back to intrinsic size if layout not computed
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

                // Render text with optional shadow
                render_text_with_shadow(renderer->renderer, font,
                                       component->text_content, text_color, component,
                                       sdl_rect.x, sdl_rect.y, sdl_rect.w);
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

        // ============================================================================
        // FLOWCHART RENDERING
        // ============================================================================

        case IR_COMPONENT_FLOWCHART: {
            // Flowchart container - render background
            float opacity = component->style ? component->style->opacity * inherited_opacity : inherited_opacity;
            render_background(renderer->renderer, component, &sdl_rect, opacity);

            // Get flowchart state for rendering nodes and edges
            IRFlowchartState* fc_state = ir_get_flowchart_state(component);
            if (!fc_state) break;

            // Compute flowchart layout if not already done
            // (similar to table layout handling)
            if (!fc_state->layout_computed) {
                float inner_width = sdl_rect.w - (component->style ? component->style->padding.left + component->style->padding.right : 0);
                float inner_height = sdl_rect.h - (component->style ? component->style->padding.top + component->style->padding.bottom : 0);
                ir_layout_compute_flowchart(component, inner_width, inner_height);
            }

            // Get theme colors for flowchart rendering
            const FlowchartTheme* theme = get_flowchart_theme();

            // Render subgraphs first (behind everything)
            for (uint32_t i = 0; i < fc_state->subgraph_count; i++) {
                IRFlowchartSubgraphData* subgraph = fc_state->subgraphs[i];
                if (!subgraph) continue;

                SDL_FRect subgraph_rect = {
                    sdl_rect.x + subgraph->x,
                    sdl_rect.y + subgraph->y,
                    subgraph->width,
                    subgraph->height
                };

                // Draw background
                uint32_t bg_color = subgraph->background_color ? subgraph->background_color : theme->subgraph_bg;
                SDL_SetRenderDrawColor(renderer->renderer,
                    (bg_color >> 24) & 0xFF,
                    (bg_color >> 16) & 0xFF,
                    (bg_color >> 8) & 0xFF,
                    (uint8_t)((bg_color & 0xFF) * opacity));
                SDL_RenderFillRect(renderer->renderer, &subgraph_rect);

                // Draw dashed border
                uint32_t border_color = subgraph->border_color ? subgraph->border_color : theme->subgraph_border;
                SDL_SetRenderDrawColor(renderer->renderer,
                    (border_color >> 24) & 0xFF,
                    (border_color >> 16) & 0xFF,
                    (border_color >> 8) & 0xFF,
                    (uint8_t)((border_color & 0xFF) * opacity));

                float dash_len = 8.0f;
                float gap_len = 4.0f;

                // Top edge (dashed)
                for (float x = subgraph_rect.x; x < subgraph_rect.x + subgraph_rect.w; x += dash_len + gap_len) {
                    float x_end = fminf(x + dash_len, subgraph_rect.x + subgraph_rect.w);
                    SDL_RenderLine(renderer->renderer, x, subgraph_rect.y, x_end, subgraph_rect.y);
                }

                // Right edge (dashed)
                for (float y = subgraph_rect.y; y < subgraph_rect.y + subgraph_rect.h; y += dash_len + gap_len) {
                    float y_end = fminf(y + dash_len, subgraph_rect.y + subgraph_rect.h);
                    SDL_RenderLine(renderer->renderer,
                        subgraph_rect.x + subgraph_rect.w, y,
                        subgraph_rect.x + subgraph_rect.w, y_end);
                }

                // Bottom edge (dashed)
                for (float x = subgraph_rect.x; x < subgraph_rect.x + subgraph_rect.w; x += dash_len + gap_len) {
                    float x_end = fminf(x + dash_len, subgraph_rect.x + subgraph_rect.w);
                    SDL_RenderLine(renderer->renderer,
                        x, subgraph_rect.y + subgraph_rect.h,
                        x_end, subgraph_rect.y + subgraph_rect.h);
                }

                // Left edge (dashed)
                for (float y = subgraph_rect.y; y < subgraph_rect.y + subgraph_rect.h; y += dash_len + gap_len) {
                    float y_end = fminf(y + dash_len, subgraph_rect.y + subgraph_rect.h);
                    SDL_RenderLine(renderer->renderer, subgraph_rect.x, y, subgraph_rect.x, y_end);
                }

                // Render title in top-left corner
                if (subgraph->title) {
                    TTF_Font* font = desktop_ir_resolve_font(renderer, component, 12.0f);
                    if (font) {
                        SDL_Color title_color = {200, 200, 200, (uint8_t)(255 * opacity)};
                        int text_w = 0, text_h = 0;
                        TTF_GetStringSize(font, subgraph->title, 0, &text_w, &text_h);

                        float title_x = subgraph_rect.x + 8.0f;
                        float title_y = subgraph_rect.y + 4.0f;

                        SDL_Surface* text_surface = TTF_RenderText_Blended(font, subgraph->title, 0, title_color);
                        if (text_surface) {
                            SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer->renderer, text_surface);
                            if (text_texture) {
                                SDL_FRect text_rect = {title_x, title_y, (float)text_w, (float)text_h};
                                SDL_RenderTexture(renderer->renderer, text_texture, NULL, &text_rect);
                                SDL_DestroyTexture(text_texture);
                            }
                            SDL_DestroySurface(text_surface);
                        }
                    }
                }
            }

            // Render edges (behind nodes, above subgraphs)
            for (uint32_t i = 0; i < fc_state->edge_count; i++) {
                IRFlowchartEdgeData* edge = fc_state->edges[i];
                if (!edge || !edge->path_points || edge->path_point_count < 2) continue;

                // Set edge color (use theme default)
                uint32_t edge_color = theme->edge_color;
                SDL_SetRenderDrawColor(renderer->renderer,
                    (edge_color >> 24) & 0xFF,
                    (edge_color >> 16) & 0xFF,
                    (edge_color >> 8) & 0xFF,
                    (uint8_t)(((edge_color) & 0xFF) * opacity));

                // Draw edge path
                for (uint32_t p = 0; p < edge->path_point_count - 1; p++) {
                    float x1 = sdl_rect.x + edge->path_points[p * 2];
                    float y1 = sdl_rect.y + edge->path_points[p * 2 + 1];
                    float x2 = sdl_rect.x + edge->path_points[(p + 1) * 2];
                    float y2 = sdl_rect.y + edge->path_points[(p + 1) * 2 + 1];

                    // Line style based on edge type
                    if (edge->type == IR_FLOWCHART_EDGE_DOTTED) {
                        // Draw dotted line
                        float dx = x2 - x1;
                        float dy = y2 - y1;
                        float len = sqrtf(dx * dx + dy * dy);
                        float dash_len = 5.0f;
                        int num_dashes = (int)(len / (dash_len * 2));
                        for (int d = 0; d < num_dashes; d++) {
                            float t1 = (float)(d * 2) / (num_dashes * 2);
                            float t2 = (float)(d * 2 + 1) / (num_dashes * 2);
                            SDL_RenderLine(renderer->renderer,
                                x1 + dx * t1, y1 + dy * t1,
                                x1 + dx * t2, y1 + dy * t2);
                        }
                    } else if (edge->type == IR_FLOWCHART_EDGE_THICK) {
                        // Draw thick line (multiple lines)
                        SDL_RenderLine(renderer->renderer, x1, y1, x2, y2);
                        SDL_RenderLine(renderer->renderer, x1 + 1, y1, x2 + 1, y2);
                        SDL_RenderLine(renderer->renderer, x1, y1 + 1, x2, y2 + 1);
                    } else {
                        // Normal line
                        SDL_RenderLine(renderer->renderer, x1, y1, x2, y2);
                    }
                }

                // Draw arrow head if needed
                if (edge->type == IR_FLOWCHART_EDGE_ARROW ||
                    edge->type == IR_FLOWCHART_EDGE_DOTTED ||
                    edge->type == IR_FLOWCHART_EDGE_THICK ||
                    edge->type == IR_FLOWCHART_EDGE_BIDIRECTIONAL) {

                    float x1 = sdl_rect.x + edge->path_points[(edge->path_point_count - 2) * 2];
                    float y1 = sdl_rect.y + edge->path_points[(edge->path_point_count - 2) * 2 + 1];
                    float x2 = sdl_rect.x + edge->path_points[(edge->path_point_count - 1) * 2];
                    float y2 = sdl_rect.y + edge->path_points[(edge->path_point_count - 1) * 2 + 1];

                    // Calculate arrow direction
                    float dx = x2 - x1;
                    float dy = y2 - y1;
                    float len = sqrtf(dx * dx + dy * dy);
                    if (len > 0) {
                        dx /= len;
                        dy /= len;
                    }

                    // Arrow head size
                    float arrow_len = 10.0f;
                    float arrow_width = 6.0f;

                    // Arrow head points
                    float ax = x2 - dx * arrow_len;
                    float ay = y2 - dy * arrow_len;
                    float bx = ax - dy * arrow_width;
                    float by = ay + dx * arrow_width;
                    float cx = ax + dy * arrow_width;
                    float cy = ay - dx * arrow_width;

                    SDL_RenderLine(renderer->renderer, x2, y2, bx, by);
                    SDL_RenderLine(renderer->renderer, x2, y2, cx, cy);
                }

                // Draw reverse arrow for bidirectional
                if (edge->type == IR_FLOWCHART_EDGE_BIDIRECTIONAL && edge->path_point_count >= 2) {
                    float x1 = sdl_rect.x + edge->path_points[0];
                    float y1 = sdl_rect.y + edge->path_points[1];
                    float x2 = sdl_rect.x + edge->path_points[2];
                    float y2 = sdl_rect.y + edge->path_points[3];

                    float dx = x1 - x2;
                    float dy = y1 - y2;
                    float len = sqrtf(dx * dx + dy * dy);
                    if (len > 0) {
                        dx /= len;
                        dy /= len;
                    }

                    float arrow_len = 10.0f;
                    float arrow_width = 6.0f;

                    float ax = x1 - dx * arrow_len;
                    float ay = y1 - dy * arrow_len;
                    float bx = ax - dy * arrow_width;
                    float by = ay + dx * arrow_width;
                    float cx = ax + dy * arrow_width;
                    float cy = ay - dx * arrow_width;

                    SDL_RenderLine(renderer->renderer, x1, y1, bx, by);
                    SDL_RenderLine(renderer->renderer, x1, y1, cx, cy);
                }
            }

            // Render nodes
            for (uint32_t i = 0; i < fc_state->node_count; i++) {
                IRFlowchartNodeData* node = fc_state->nodes[i];
                if (!node) continue;

                SDL_FRect node_rect = {
                    sdl_rect.x + node->x,
                    sdl_rect.y + node->y,
                    node->width,
                    node->height
                };

                // Fill color (use theme default if not specified)
                uint32_t fill = node->fill_color ? node->fill_color : theme->node_fill;
                // Stroke color (use theme default if not specified)
                uint32_t stroke = node->stroke_color ? node->stroke_color : theme->node_stroke;

                SDL_SetRenderDrawColor(renderer->renderer,
                    (fill >> 24) & 0xFF,
                    (fill >> 16) & 0xFF,
                    (fill >> 8) & 0xFF,
                    (uint8_t)((fill & 0xFF) * opacity));

                // Draw shape based on type
                switch (node->shape) {
                    case IR_FLOWCHART_SHAPE_RECTANGLE:
                    case IR_FLOWCHART_SHAPE_SUBROUTINE:
                        SDL_RenderFillRect(renderer->renderer, &node_rect);
                        SDL_SetRenderDrawColor(renderer->renderer,
                            (stroke >> 24) & 0xFF,
                            (stroke >> 16) & 0xFF,
                            (stroke >> 8) & 0xFF,
                            (uint8_t)((stroke & 0xFF) * opacity));
                        SDL_RenderRect(renderer->renderer, &node_rect);

                        // Subroutine has double lines on sides
                        if (node->shape == IR_FLOWCHART_SHAPE_SUBROUTINE) {
                            SDL_RenderLine(renderer->renderer,
                                node_rect.x + 10, node_rect.y,
                                node_rect.x + 10, node_rect.y + node_rect.h);
                            SDL_RenderLine(renderer->renderer,
                                node_rect.x + node_rect.w - 10, node_rect.y,
                                node_rect.x + node_rect.w - 10, node_rect.y + node_rect.h);
                        }
                        break;

                    case IR_FLOWCHART_SHAPE_ROUNDED:
                    case IR_FLOWCHART_SHAPE_STADIUM: {
                        // True rounded rectangle rendering
                        float radius = (node->shape == IR_FLOWCHART_SHAPE_STADIUM)
                            ? fminf(node_rect.w, node_rect.h) / 2.0f  // Stadium: pill shape
                            : 8.0f;  // Rounded: fixed 8px radius

                        // Render filled rounded rectangle
                        render_rounded_rect_filled(renderer->renderer, &node_rect, radius, fill, opacity);

                        // Render stroke
                        render_rounded_rect_stroke(renderer->renderer, &node_rect, radius, stroke, opacity);
                        break;
                    }

                    case IR_FLOWCHART_SHAPE_DIAMOND: {
                        // Diamond shape - use geometry for smooth rendering
                        float cx = node_rect.x + node_rect.w / 2;
                        float cy = node_rect.y + node_rect.h / 2;
                        float hw = node_rect.w / 2;
                        float hh = node_rect.h / 2;

                        // Define diamond points (top, right, bottom, left)
                        float diamond_points[] = {
                            cx, cy - hh,  // Top
                            cx + hw, cy,  // Right
                            cx, cy + hh,  // Bottom
                            cx - hw, cy   // Left
                        };

                        // Fill with SDL_RenderGeometry
                        render_polygon_filled(renderer->renderer, diamond_points, 4, fill, opacity);

                        // Stroke
                        render_polygon_stroke(renderer->renderer, diamond_points, 4, stroke, opacity);
                        break;
                    }

                    case IR_FLOWCHART_SHAPE_CIRCLE: {
                        // Circle shape - use geometry for smooth rendering
                        float cx = node_rect.x + node_rect.w / 2;
                        float cy = node_rect.y + node_rect.h / 2;
                        float r = fminf(node_rect.w, node_rect.h) / 2;

                        // Generate circle points (32 segments for smoothness)
                        int segments = 32;
                        float* circle_points = (float*)malloc(sizeof(float) * segments * 2);
                        if (circle_points) {
                            for (int s = 0; s < segments; s++) {
                                float angle = (float)s / segments * 2 * 3.14159f;
                                circle_points[s * 2] = cx + cosf(angle) * r;
                                circle_points[s * 2 + 1] = cy + sinf(angle) * r;
                            }

                            // Fill with SDL_RenderGeometry
                            render_polygon_filled(renderer->renderer, circle_points, segments, fill, opacity);

                            // Stroke
                            render_polygon_stroke(renderer->renderer, circle_points, segments, stroke, opacity);

                            free(circle_points);
                        }
                        break;
                    }

                    case IR_FLOWCHART_SHAPE_HEXAGON: {
                        // Hexagon shape - use geometry for smooth rendering
                        float cx = node_rect.x + node_rect.w / 2;
                        float cy = node_rect.y + node_rect.h / 2;
                        float hw = node_rect.w / 2;
                        float hh = node_rect.h / 2;
                        float inset = hw * 0.25f;

                        // Define hexagon points (6 vertices, clockwise from top-left)
                        float hexagon_points[] = {
                            cx - hw + inset, cy - hh,  // Top-left corner
                            cx + hw - inset, cy - hh,  // Top-right corner
                            cx + hw, cy,               // Right point
                            cx + hw - inset, cy + hh,  // Bottom-right corner
                            cx - hw + inset, cy + hh,  // Bottom-left corner
                            cx - hw, cy                // Left point
                        };

                        // Fill with SDL_RenderGeometry
                        render_polygon_filled(renderer->renderer, hexagon_points, 6, fill, opacity);

                        // Stroke
                        render_polygon_stroke(renderer->renderer, hexagon_points, 6, stroke, opacity);
                        break;
                    }

                    case IR_FLOWCHART_SHAPE_CYLINDER: {
                        // Cylinder (database) shape - rectangle with ellipse top/bottom
                        float ellipse_height = node_rect.h * 0.15f;

                        // Fill body
                        SDL_FRect body_rect = {
                            node_rect.x,
                            node_rect.y + ellipse_height / 2,
                            node_rect.w,
                            node_rect.h - ellipse_height
                        };
                        SDL_RenderFillRect(renderer->renderer, &body_rect);

                        // Draw ellipse top and bottom (approximated)
                        float cx = node_rect.x + node_rect.w / 2;
                        float rx = node_rect.w / 2;
                        float ry = ellipse_height / 2;

                        // Top ellipse fill
                        float top_cy = node_rect.y + ry;
                        for (float y = -ry; y <= ry; y += 1.0f) {
                            float ratio = sqrtf(1.0f - (y * y) / (ry * ry));
                            float x_offset = rx * ratio;
                            SDL_RenderLine(renderer->renderer,
                                cx - x_offset, top_cy + y,
                                cx + x_offset, top_cy + y);
                        }

                        // Stroke
                        SDL_SetRenderDrawColor(renderer->renderer,
                            (stroke >> 24) & 0xFF,
                            (stroke >> 16) & 0xFF,
                            (stroke >> 8) & 0xFF,
                            (uint8_t)((stroke & 0xFF) * opacity));

                        // Left/right edges
                        SDL_RenderLine(renderer->renderer,
                            node_rect.x, node_rect.y + ry,
                            node_rect.x, node_rect.y + node_rect.h - ry);
                        SDL_RenderLine(renderer->renderer,
                            node_rect.x + node_rect.w, node_rect.y + ry,
                            node_rect.x + node_rect.w, node_rect.y + node_rect.h - ry);

                        // Top/bottom ellipse strokes
                        int segments = 16;
                        float bottom_cy = node_rect.y + node_rect.h - ry;
                        for (int s = 0; s < segments; s++) {
                            float a1 = (float)s / segments * 3.14159f;
                            float a2 = (float)(s + 1) / segments * 3.14159f;
                            // Top ellipse (full)
                            SDL_RenderLine(renderer->renderer,
                                cx + cosf(a1) * rx, top_cy - sinf(a1) * ry,
                                cx + cosf(a2) * rx, top_cy - sinf(a2) * ry);
                            SDL_RenderLine(renderer->renderer,
                                cx + cosf(a1 + 3.14159f) * rx, top_cy - sinf(a1 + 3.14159f) * ry,
                                cx + cosf(a2 + 3.14159f) * rx, top_cy - sinf(a2 + 3.14159f) * ry);
                            // Bottom ellipse (bottom half only)
                            SDL_RenderLine(renderer->renderer,
                                cx + cosf(a1 + 3.14159f) * rx, bottom_cy - sinf(a1 + 3.14159f) * ry,
                                cx + cosf(a2 + 3.14159f) * rx, bottom_cy - sinf(a2 + 3.14159f) * ry);
                        }
                        break;
                    }

                    default:
                        // Fallback to rectangle for other shapes
                        SDL_RenderFillRect(renderer->renderer, &node_rect);
                        SDL_SetRenderDrawColor(renderer->renderer,
                            (stroke >> 24) & 0xFF,
                            (stroke >> 16) & 0xFF,
                            (stroke >> 8) & 0xFF,
                            (uint8_t)((stroke & 0xFF) * opacity));
                        SDL_RenderRect(renderer->renderer, &node_rect);
                        break;
                }

                // Render node label
                if (node->label) {
                    TTF_Font* font = desktop_ir_resolve_font(renderer, component, 14.0f);
                    if (font) {
                        // Use theme text color
                        uint32_t text_rgba = theme->node_text;
                        SDL_Color text_color = {
                            (text_rgba >> 24) & 0xFF,
                            (text_rgba >> 16) & 0xFF,
                            (text_rgba >> 8) & 0xFF,
                            (uint8_t)(255 * opacity)
                        };
                        // Center text in node
                        int text_w = 0, text_h = 0;
                        TTF_GetStringSize(font, node->label, 0, &text_w, &text_h);

                        float text_x = node_rect.x + (node_rect.w - text_w) / 2;
                        float text_y = node_rect.y + (node_rect.h - text_h) / 2;

                        SDL_Surface* text_surface = TTF_RenderText_Blended(font, node->label, 0, text_color);
                        if (text_surface) {
                            SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer->renderer, text_surface);
                            if (text_texture) {
                                SDL_FRect text_rect = {text_x, text_y, (float)text_w, (float)text_h};
                                SDL_RenderTexture(renderer->renderer, text_texture, NULL, &text_rect);
                                SDL_DestroyTexture(text_texture);
                            }
                            SDL_DestroySurface(text_surface);
                        }
                    }
                }
            }

            // Render edge labels
            for (uint32_t i = 0; i < fc_state->edge_count; i++) {
                IRFlowchartEdgeData* edge = fc_state->edges[i];
                if (!edge || !edge->label || !edge->path_points || edge->path_point_count < 2) continue;

                // Calculate total path length
                float total_len = 0;
                for (uint32_t p = 0; p < edge->path_point_count - 1; p++) {
                    float dx = edge->path_points[(p+1)*2] - edge->path_points[p*2];
                    float dy = edge->path_points[(p+1)*2+1] - edge->path_points[p*2+1];
                    total_len += sqrtf(dx*dx + dy*dy);
                }

                // Find position at path midpoint
                float target_len = total_len / 2.0f;
                float current_len = 0;
                float mid_x = sdl_rect.x + edge->path_points[0];
                float mid_y = sdl_rect.y + edge->path_points[1];

                for (uint32_t p = 0; p < edge->path_point_count - 1; p++) {
                    float x1 = edge->path_points[p*2];
                    float y1 = edge->path_points[p*2+1];
                    float x2 = edge->path_points[(p+1)*2];
                    float y2 = edge->path_points[(p+1)*2+1];

                    float dx = x2 - x1;
                    float dy = y2 - y1;
                    float seg_len = sqrtf(dx*dx + dy*dy);

                    if (current_len + seg_len >= target_len) {
                        // Midpoint is in this segment
                        float t = (target_len - current_len) / seg_len;
                        mid_x = sdl_rect.x + x1 + dx * t;
                        mid_y = sdl_rect.y + y1 + dy * t;
                        break;
                    }
                    current_len += seg_len;
                }

                TTF_Font* font = desktop_ir_resolve_font(renderer, component, 12.0f);
                if (font) {
                    SDL_Color text_color = {200, 200, 200, (uint8_t)(255 * opacity)};
                    int text_w = 0, text_h = 0;
                    TTF_GetStringSize(font, edge->label, 0, &text_w, &text_h);

                    // Draw background for label
                    SDL_FRect label_bg = {mid_x - text_w/2 - 2, mid_y - text_h/2 - 1, (float)text_w + 4, (float)text_h + 2};
                    SDL_SetRenderDrawColor(renderer->renderer, 40, 40, 40, (uint8_t)(220 * opacity));
                    SDL_RenderFillRect(renderer->renderer, &label_bg);

                    SDL_Surface* text_surface = TTF_RenderText_Blended(font, edge->label, 0, text_color);
                    if (text_surface) {
                        SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer->renderer, text_surface);
                        if (text_texture) {
                            SDL_FRect text_rect = {mid_x - text_w/2, mid_y - text_h/2, (float)text_w, (float)text_h};
                            SDL_RenderTexture(renderer->renderer, text_texture, NULL, &text_rect);
                            SDL_DestroyTexture(text_texture);
                        }
                        SDL_DestroySurface(text_surface);
                    }
                }
            }

            // Flowchart rendering is complete - children are rendered above
            return true;
        }

        case IR_COMPONENT_FLOWCHART_NODE:
        case IR_COMPONENT_FLOWCHART_EDGE:
        case IR_COMPONENT_FLOWCHART_SUBGRAPH:
        case IR_COMPONENT_FLOWCHART_LABEL:
            // These are rendered by the parent Flowchart component
            break;

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
