#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "svg_generator.h"
#include "../../ir/ir_core.h"
#include "../../ir/ir_builder.h"

// SVG Builder - similar to HTMLGenerator
typedef struct {
    char* buffer;
    size_t length;
    size_t capacity;
} SVGBuilder;

// ============================================================================
// SVG Builder Functions
// ============================================================================

static SVGBuilder* svg_builder_create(void) {
    SVGBuilder* builder = malloc(sizeof(SVGBuilder));
    if (!builder) return NULL;

    builder->capacity = 4096;  // Start with 4KB
    builder->buffer = malloc(builder->capacity);
    if (!builder->buffer) {
        free(builder);
        return NULL;
    }

    builder->buffer[0] = '\0';
    builder->length = 0;
    return builder;
}

static void svg_builder_destroy(SVGBuilder* builder) {
    if (!builder) return;
    free(builder->buffer);
    free(builder);
}

static bool svg_builder_ensure_capacity(SVGBuilder* builder, size_t additional) {
    size_t needed = builder->length + additional + 1;
    if (needed <= builder->capacity) return true;

    size_t new_capacity = builder->capacity * 2;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }

    char* new_buffer = realloc(builder->buffer, new_capacity);
    if (!new_buffer) return false;

    builder->buffer = new_buffer;
    builder->capacity = new_capacity;
    return true;
}

static bool svg_builder_append(SVGBuilder* builder, const char* str) {
    if (!builder || !str) return false;

    size_t str_len = strlen(str);
    if (!svg_builder_ensure_capacity(builder, str_len)) return false;

    strcpy(builder->buffer + builder->length, str);
    builder->length += str_len;
    return true;
}

static bool svg_builder_append_fmt(SVGBuilder* builder, const char* fmt, ...) {
    if (!builder || !fmt) return false;

    va_list args;
    va_start(args, fmt);

    // First, calculate needed size
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        va_end(args);
        return false;
    }

    if (!svg_builder_ensure_capacity(builder, needed)) {
        va_end(args);
        return false;
    }

    vsnprintf(builder->buffer + builder->length, needed + 1, fmt, args);
    builder->length += needed;

    va_end(args);
    return true;
}

// ============================================================================
// HTML Escaping
// ============================================================================

static char* escape_html(const char* text) {
    if (!text) return strdup("");

    size_t len = strlen(text);
    size_t escaped_len = len * 6 + 1;  // Worst case: every char becomes &quot;
    char* escaped = malloc(escaped_len);
    if (!escaped) return NULL;

    size_t pos = 0;
    for (const char* p = text; *p; p++) {
        switch (*p) {
            case '&':
                strcpy(escaped + pos, "&amp;");
                pos += 5;
                break;
            case '<':
                strcpy(escaped + pos, "&lt;");
                pos += 4;
                break;
            case '>':
                strcpy(escaped + pos, "&gt;");
                pos += 4;
                break;
            case '"':
                strcpy(escaped + pos, "&quot;");
                pos += 6;
                break;
            case '\'':
                strcpy(escaped + pos, "&#x27;");
                pos += 6;
                break;
            default:
                escaped[pos++] = *p;
                break;
        }
    }
    escaped[pos] = '\0';
    return escaped;
}

// ============================================================================
// Theme Management
// ============================================================================

SVGOptions svg_options_default(void) {
    SVGOptions opts = {
        .width = 800,
        .height = 600,
        .theme = SVG_THEME_DEFAULT,
        .interactive = true,
        .accessibility = true,
        .title = "Flowchart",
        .description = "A flowchart diagram"
    };
    return opts;
}

SVGTheme svg_parse_theme(const char* theme_str) {
    if (!theme_str) return SVG_THEME_DEFAULT;

    if (strcmp(theme_str, "dark") == 0) return SVG_THEME_DARK;
    if (strcmp(theme_str, "light") == 0) return SVG_THEME_LIGHT;
    if (strcmp(theme_str, "high-contrast") == 0) return SVG_THEME_HIGH_CONTRAST;

    return SVG_THEME_DEFAULT;
}

const char* svg_theme_to_class(SVGTheme theme) {
    switch (theme) {
        case SVG_THEME_DARK:          return "dark";
        case SVG_THEME_LIGHT:         return "light";
        case SVG_THEME_HIGH_CONTRAST: return "high-contrast";
        default:                      return "default";
    }
}

// ============================================================================
// SVG Color Definitions for Themes
// ============================================================================

typedef struct {
    const char* node_fill;
    const char* node_stroke;
    const char* node_text;
    const char* edge_color;
    const char* subgraph_fill;
    const char* subgraph_stroke;
    const char* subgraph_title;
} ThemeColors;

static ThemeColors get_theme_colors(SVGTheme theme) {
    switch (theme) {
        case SVG_THEME_DARK:
            return (ThemeColors){
                .node_fill = "#2d4a7c",
                .node_stroke = "#4a7cc2",
                .node_text = "#e0e8ff",
                .edge_color = "#a0a0a0",
                .subgraph_fill = "#2a2a2a",
                .subgraph_stroke = "#555555",
                .subgraph_title = "#e0e0e0"
            };
        case SVG_THEME_LIGHT:
            return (ThemeColors){
                .node_fill = "#e0e8ff",
                .node_stroke = "#5588FF",
                .node_text = "#1a1d2e",
                .edge_color = "#666666",
                .subgraph_fill = "#f5f5f5",
                .subgraph_stroke = "#999999",
                .subgraph_title = "#333333"
            };
        case SVG_THEME_HIGH_CONTRAST:
            return (ThemeColors){
                .node_fill = "#000000",
                .node_stroke = "#FFFF00",
                .node_text = "#FFFFFF",
                .edge_color = "#FFFFFF",
                .subgraph_fill = "#000000",
                .subgraph_stroke = "#FFFF00",
                .subgraph_title = "#FFFFFF"
            };
        default: // SVG_THEME_DEFAULT
            return (ThemeColors){
                .node_fill = "#5588FF",
                .node_stroke = "#3366CC",
                .node_text = "#1a1d2e",
                .edge_color = "#888888",
                .subgraph_fill = "#f5f5f5",
                .subgraph_stroke = "#999999",
                .subgraph_title = "#333333"
            };
    }
}

// ============================================================================
// SVG Node Shape Rendering
// ============================================================================

static void svg_append_rectangle(SVGBuilder* b, const IRFlowchartNodeData* node, const ThemeColors* colors, const char* node_id) {
    char* escaped_label = escape_html(node->label);

    svg_builder_append_fmt(b,
        "  <g class=\"node\" data-node-id=\"%s\" data-shape=\"rectangle\">\n"
        "    <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" \n"
        "          fill=\"%s\" stroke=\"%s\" stroke-width=\"2\" />\n"
        "    <text x=\"%.1f\" y=\"%.1f\" fill=\"%s\" \n"
        "          text-anchor=\"middle\" dominant-baseline=\"middle\" \n"
        "          font-size=\"14px\">%s</text>\n"
        "  </g>\n",
        node_id,
        node->x, node->y, node->width, node->height,
        colors->node_fill, colors->node_stroke,
        node->x + node->width / 2, node->y + node->height / 2,
        colors->node_text,
        escaped_label
    );

    free(escaped_label);
}

static void svg_append_rounded(SVGBuilder* b, const IRFlowchartNodeData* node, const ThemeColors* colors, const char* node_id) {
    char* escaped_label = escape_html(node->label);

    svg_builder_append_fmt(b,
        "  <g class=\"node\" data-node-id=\"%s\" data-shape=\"rounded\">\n"
        "    <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" rx=\"8\" \n"
        "          fill=\"%s\" stroke=\"%s\" stroke-width=\"2\" />\n"
        "    <text x=\"%.1f\" y=\"%.1f\" fill=\"%s\" \n"
        "          text-anchor=\"middle\" dominant-baseline=\"middle\" \n"
        "          font-size=\"14px\">%s</text>\n"
        "  </g>\n",
        node_id,
        node->x, node->y, node->width, node->height,
        colors->node_fill, colors->node_stroke,
        node->x + node->width / 2, node->y + node->height / 2,
        colors->node_text,
        escaped_label
    );

    free(escaped_label);
}

static void svg_append_stadium(SVGBuilder* b, const IRFlowchartNodeData* node, const ThemeColors* colors, const char* node_id) {
    char* escaped_label = escape_html(node->label);
    float radius = fminf(node->width, node->height) / 2;

    svg_builder_append_fmt(b,
        "  <g class=\"node\" data-node-id=\"%s\" data-shape=\"stadium\">\n"
        "    <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" rx=\"%.1f\" \n"
        "          fill=\"%s\" stroke=\"%s\" stroke-width=\"2\" />\n"
        "    <text x=\"%.1f\" y=\"%.1f\" fill=\"%s\" \n"
        "          text-anchor=\"middle\" dominant-baseline=\"middle\" \n"
        "          font-size=\"14px\">%s</text>\n"
        "  </g>\n",
        node_id,
        node->x, node->y, node->width, node->height, radius,
        colors->node_fill, colors->node_stroke,
        node->x + node->width / 2, node->y + node->height / 2,
        colors->node_text,
        escaped_label
    );

    free(escaped_label);
}

static void svg_append_diamond(SVGBuilder* b, const IRFlowchartNodeData* node, const ThemeColors* colors, const char* node_id) {
    char* escaped_label = escape_html(node->label);

    float cx = node->x + node->width / 2;
    float cy = node->y + node->height / 2;
    float hw = node->width / 2;
    float hh = node->height / 2;

    svg_builder_append_fmt(b,
        "  <g class=\"node\" data-node-id=\"%s\" data-shape=\"diamond\">\n"
        "    <path d=\"M %.1f,%.1f L %.1f,%.1f L %.1f,%.1f L %.1f,%.1f Z\" \n"
        "          fill=\"%s\" stroke=\"%s\" stroke-width=\"2\" />\n"
        "    <text x=\"%.1f\" y=\"%.1f\" fill=\"%s\" \n"
        "          text-anchor=\"middle\" dominant-baseline=\"middle\" \n"
        "          font-size=\"14px\">%s</text>\n"
        "  </g>\n",
        node_id,
        cx, cy - hh,  // Top
        cx + hw, cy,  // Right
        cx, cy + hh,  // Bottom
        cx - hw, cy,  // Left
        colors->node_fill, colors->node_stroke,
        cx, cy,
        colors->node_text,
        escaped_label
    );

    free(escaped_label);
}

static void svg_append_circle(SVGBuilder* b, const IRFlowchartNodeData* node, const ThemeColors* colors, const char* node_id) {
    char* escaped_label = escape_html(node->label);

    float cx = node->x + node->width / 2;
    float cy = node->y + node->height / 2;
    float r = fminf(node->width, node->height) / 2;

    svg_builder_append_fmt(b,
        "  <g class=\"node\" data-node-id=\"%s\" data-shape=\"circle\">\n"
        "    <circle cx=\"%.1f\" cy=\"%.1f\" r=\"%.1f\" \n"
        "            fill=\"%s\" stroke=\"%s\" stroke-width=\"2\" />\n"
        "    <text x=\"%.1f\" y=\"%.1f\" fill=\"%s\" \n"
        "          text-anchor=\"middle\" dominant-baseline=\"middle\" \n"
        "          font-size=\"14px\">%s</text>\n"
        "  </g>\n",
        node_id,
        cx, cy, r,
        colors->node_fill, colors->node_stroke,
        cx, cy,
        colors->node_text,
        escaped_label
    );

    free(escaped_label);
}

static void svg_append_hexagon(SVGBuilder* b, const IRFlowchartNodeData* node, const ThemeColors* colors, const char* node_id) {
    char* escaped_label = escape_html(node->label);

    float cx = node->x + node->width / 2;
    float cy = node->y + node->height / 2;
    float hw = node->width / 2;
    float hh = node->height / 2;
    float offset = hw * 0.25f;  // Hexagon side offset

    svg_builder_append_fmt(b,
        "  <g class=\"node\" data-node-id=\"%s\" data-shape=\"hexagon\">\n"
        "    <path d=\"M %.1f,%.1f L %.1f,%.1f L %.1f,%.1f L %.1f,%.1f L %.1f,%.1f L %.1f,%.1f Z\" \n"
        "          fill=\"%s\" stroke=\"%s\" stroke-width=\"2\" />\n"
        "    <text x=\"%.1f\" y=\"%.1f\" fill=\"%s\" \n"
        "          text-anchor=\"middle\" dominant-baseline=\"middle\" \n"
        "          font-size=\"14px\">%s</text>\n"
        "  </g>\n",
        node_id,
        cx - hw + offset, cy - hh,  // Top-left
        cx + hw - offset, cy - hh,  // Top-right
        cx + hw, cy,                // Right
        cx + hw - offset, cy + hh,  // Bottom-right
        cx - hw + offset, cy + hh,  // Bottom-left
        cx - hw, cy,                // Left
        colors->node_fill, colors->node_stroke,
        cx, cy,
        colors->node_text,
        escaped_label
    );

    free(escaped_label);
}

static void svg_append_cylinder(SVGBuilder* b, const IRFlowchartNodeData* node, const ThemeColors* colors, const char* node_id) {
    char* escaped_label = escape_html(node->label);

    float x = node->x;
    float y = node->y;
    float w = node->width;
    float h = node->height;
    float ellipse_ry = h * 0.15f;  // Height of cylinder top

    svg_builder_append_fmt(b,
        "  <g class=\"node\" data-node-id=\"%s\" data-shape=\"cylinder\">\n"
        "    <!-- Cylinder body -->\n"
        "    <path d=\"M %.1f,%.1f L %.1f,%.1f A %.1f,%.1f 0 0,0 %.1f,%.1f L %.1f,%.1f A %.1f,%.1f 0 0,0 %.1f,%.1f Z\" \n"
        "          fill=\"%s\" stroke=\"%s\" stroke-width=\"2\" />\n"
        "    <!-- Cylinder top -->\n"
        "    <ellipse cx=\"%.1f\" cy=\"%.1f\" rx=\"%.1f\" ry=\"%.1f\" \n"
        "             fill=\"%s\" stroke=\"%s\" stroke-width=\"2\" />\n"
        "    <text x=\"%.1f\" y=\"%.1f\" fill=\"%s\" \n"
        "          text-anchor=\"middle\" dominant-baseline=\"middle\" \n"
        "          font-size=\"14px\">%s</text>\n"
        "  </g>\n",
        node_id,
        x, y + ellipse_ry,
        x, y + h - ellipse_ry,
        w / 2, ellipse_ry,
        x + w, y + h - ellipse_ry,
        x + w, y + ellipse_ry,
        w / 2, ellipse_ry,
        x, y + ellipse_ry,
        colors->node_fill, colors->node_stroke,
        x + w / 2, y + ellipse_ry,
        w / 2, ellipse_ry,
        colors->node_fill, colors->node_stroke,
        x + w / 2, y + h / 2,
        colors->node_text,
        escaped_label
    );

    free(escaped_label);
}

static void svg_append_subroutine(SVGBuilder* b, const IRFlowchartNodeData* node, const ThemeColors* colors, const char* node_id) {
    char* escaped_label = escape_html(node->label);

    float x = node->x;
    float y = node->y;
    float w = node->width;
    float h = node->height;
    float line_offset = w * 0.1f;  // Distance from edge for vertical lines

    svg_builder_append_fmt(b,
        "  <g class=\"node\" data-node-id=\"%s\" data-shape=\"subroutine\">\n"
        "    <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" \n"
        "          fill=\"%s\" stroke=\"%s\" stroke-width=\"2\" />\n"
        "    <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" \n"
        "          stroke=\"%s\" stroke-width=\"2\" />\n"
        "    <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" \n"
        "          stroke=\"%s\" stroke-width=\"2\" />\n"
        "    <text x=\"%.1f\" y=\"%.1f\" fill=\"%s\" \n"
        "          text-anchor=\"middle\" dominant-baseline=\"middle\" \n"
        "          font-size=\"14px\">%s</text>\n"
        "  </g>\n",
        node_id,
        x, y, w, h,
        colors->node_fill, colors->node_stroke,
        x + line_offset, y, x + line_offset, y + h,
        colors->node_stroke,
        x + w - line_offset, y, x + w - line_offset, y + h,
        colors->node_stroke,
        x + w / 2, y + h / 2,
        colors->node_text,
        escaped_label
    );

    free(escaped_label);
}

static void svg_append_asymmetric(SVGBuilder* b, const IRFlowchartNodeData* node, const ThemeColors* colors, const char* node_id) {
    char* escaped_label = escape_html(node->label);

    float x = node->x;
    float y = node->y;
    float w = node->width;
    float h = node->height;
    float slant = w * 0.15f;  // Trapezoid slant

    svg_builder_append_fmt(b,
        "  <g class=\"node\" data-node-id=\"%s\" data-shape=\"asymmetric\">\n"
        "    <path d=\"M %.1f,%.1f L %.1f,%.1f L %.1f,%.1f L %.1f,%.1f Z\" \n"
        "          fill=\"%s\" stroke=\"%s\" stroke-width=\"2\" />\n"
        "    <text x=\"%.1f\" y=\"%.1f\" fill=\"%s\" \n"
        "          text-anchor=\"middle\" dominant-baseline=\"middle\" \n"
        "          font-size=\"14px\">%s</text>\n"
        "  </g>\n",
        node_id,
        x + slant, y,        // Top-left
        x + w, y,            // Top-right
        x + w - slant, y + h, // Bottom-right
        x, y + h,            // Bottom-left
        colors->node_fill, colors->node_stroke,
        x + w / 2, y + h / 2,
        colors->node_text,
        escaped_label
    );

    free(escaped_label);
}

static void svg_append_node(SVGBuilder* b, const IRFlowchartNodeData* node, const ThemeColors* colors, const char* node_id) {
    switch (node->shape) {
        case IR_FLOWCHART_SHAPE_RECTANGLE:
            svg_append_rectangle(b, node, colors, node_id);
            break;
        case IR_FLOWCHART_SHAPE_ROUNDED:
            svg_append_rounded(b, node, colors, node_id);
            break;
        case IR_FLOWCHART_SHAPE_STADIUM:
            svg_append_stadium(b, node, colors, node_id);
            break;
        case IR_FLOWCHART_SHAPE_DIAMOND:
            svg_append_diamond(b, node, colors, node_id);
            break;
        case IR_FLOWCHART_SHAPE_CIRCLE:
            svg_append_circle(b, node, colors, node_id);
            break;
        case IR_FLOWCHART_SHAPE_HEXAGON:
            svg_append_hexagon(b, node, colors, node_id);
            break;
        case IR_FLOWCHART_SHAPE_CYLINDER:
            svg_append_cylinder(b, node, colors, node_id);
            break;
        case IR_FLOWCHART_SHAPE_SUBROUTINE:
            svg_append_subroutine(b, node, colors, node_id);
            break;
        case IR_FLOWCHART_SHAPE_ASYMMETRIC:
            svg_append_asymmetric(b, node, colors, node_id);
            break;
        default:
            // Fallback to rectangle
            svg_append_rectangle(b, node, colors, node_id);
            break;
    }
}

// ============================================================================
// SVG Subgraph Rendering
// ============================================================================

static void svg_append_subgraph(SVGBuilder* b, const IRFlowchartSubgraphData* sg, const ThemeColors* colors, const char* subgraph_id) {
    if (!sg || !b || !colors) return;

    // Escape title for HTML
    char* escaped_title = escape_html(sg->title ? sg->title : subgraph_id);
    if (!escaped_title) return;

    // Render subgraph background box with rounded corners
    svg_builder_append_fmt(b,
        "  <g class=\"subgraph\" data-subgraph-id=\"%s\">\n"
        "    <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" \n"
        "          fill=\"%s\" stroke=\"%s\" stroke-width=\"2\" \n"
        "          rx=\"8\" ry=\"8\" opacity=\"0.85\" />\n",
        subgraph_id,
        sg->x, sg->y, sg->width, sg->height,
        colors->subgraph_fill,
        colors->subgraph_stroke
    );

    // Render subgraph title at top center
    float title_x = sg->x + (sg->width / 2.0f);
    float title_y = sg->y + 20.0f;  // 20px from top edge

    svg_builder_append_fmt(b,
        "    <text x=\"%.1f\" y=\"%.1f\" \n"
        "          text-anchor=\"middle\" dominant-baseline=\"middle\" \n"
        "          font-size=\"16px\" font-weight=\"bold\" \n"
        "          fill=\"%s\">%s</text>\n"
        "  </g>\n",
        title_x, title_y,
        colors->subgraph_title,
        escaped_title
    );

    free(escaped_title);
}

// ============================================================================
// SVG Edge Rendering
// ============================================================================

static void svg_append_edge(SVGBuilder* b, const IRFlowchartEdgeData* edge, const ThemeColors* colors) {
    if (!edge || edge->path_point_count < 2) return;

    // Build path
    svg_builder_append(b, "  <path d=\"");
    svg_builder_append_fmt(b, "M %.1f,%.1f", edge->path_points[0], edge->path_points[1]);

    for (uint32_t i = 1; i < edge->path_point_count; i++) {
        svg_builder_append_fmt(b, " L %.1f,%.1f",
            edge->path_points[i * 2],
            edge->path_points[i * 2 + 1]);
    }

    svg_builder_append(b, "\" class=\"edge\" fill=\"none\" ");

    // Stroke width
    int stroke_width = (edge->type == IR_FLOWCHART_EDGE_THICK) ? 4 : 2;
    svg_builder_append_fmt(b, "stroke=\"%s\" stroke-width=\"%d\" ", colors->edge_color, stroke_width);

    // Dash array for dotted edges
    if (edge->type == IR_FLOWCHART_EDGE_DOTTED) {
        svg_builder_append(b, "stroke-dasharray=\"5,5\" ");
    }

    // Markers (arrows)
    if (edge->type == IR_FLOWCHART_EDGE_BIDIRECTIONAL) {
        svg_builder_append(b, "marker-start=\"url(#arrow-bi)\" marker-end=\"url(#arrow)\" ");
    } else if (edge->type != IR_FLOWCHART_EDGE_OPEN) {
        svg_builder_append(b, "marker-end=\"url(#arrow)\" ");
    }

    svg_builder_append(b, "/>\n");

    // Edge label (if present)
    if (edge->label && strlen(edge->label) > 0) {
        // Find midpoint of path for label placement
        float midpoint_x = edge->path_points[0];
        float midpoint_y = edge->path_points[1];

        if (edge->path_point_count > 2) {
            // Use middle segment midpoint
            uint32_t mid_idx = edge->path_point_count / 2;
            midpoint_x = (edge->path_points[mid_idx * 2] + edge->path_points[(mid_idx - 1) * 2]) / 2;
            midpoint_y = (edge->path_points[mid_idx * 2 + 1] + edge->path_points[(mid_idx - 1) * 2 + 1]) / 2;
        }

        char* escaped_label = escape_html(edge->label);
        svg_builder_append_fmt(b,
            "  <text x=\"%.1f\" y=\"%.1f\" fill=\"%s\" font-size=\"12px\" \n"
            "        text-anchor=\"middle\" dominant-baseline=\"middle\" \n"
            "        class=\"edge-label\">%s</text>\n",
            midpoint_x, midpoint_y,
            colors->node_text,
            escaped_label
        );
        free(escaped_label);
    }
}

// ============================================================================
// Main SVG Generation
// ============================================================================

char* flowchart_to_svg(const IRComponent* flowchart, const SVGOptions* opts) {
    if (!flowchart || flowchart->type != IR_COMPONENT_FLOWCHART) {
        return NULL;
    }

    // Use default options if none provided
    SVGOptions default_opts = svg_options_default();
    if (!opts) opts = &default_opts;

    // Get flowchart state
    IRFlowchartState* fc_state = ir_get_flowchart_state((IRComponent*)flowchart);
    if (!fc_state) return NULL;

    // Compute layout if not already done
    if (!fc_state->layout_computed) {
        // For intrinsic sizing, use 0 (unlimited) to allow natural dimensions
        // Otherwise use specified dimensions
        int layout_width = (opts->use_intrinsic_size && opts->width == 0) ? 0 : opts->width;
        int layout_height = (opts->use_intrinsic_size && opts->height == 0) ? 0 : opts->height;
        ir_layout_compute_flowchart((IRComponent*)flowchart, layout_width, layout_height);
    }

    // Create SVG builder
    SVGBuilder* b = svg_builder_create();
    if (!b) return NULL;

    // Get theme colors
    ThemeColors colors = get_theme_colors(opts->theme);
    const char* theme_class = svg_theme_to_class(opts->theme);

    // Determine SVG dimensions
    float svg_width, svg_height;
    if (opts->use_intrinsic_size) {
        // Responsive: use computed content bounds with padding
        const float PADDING = 20.0f;  // Padding around content

        svg_width = fc_state->content_width + (PADDING * 2);
        svg_height = fc_state->content_height + (PADDING * 2);

        // Apply max constraints if specified
        if (opts->width > 0 && svg_width > opts->width) svg_width = opts->width;
        if (opts->height > 0 && svg_height > opts->height) svg_height = opts->height;
    } else {
        // Legacy: use fixed dimensions
        svg_width = opts->width;
        svg_height = opts->height;
    }

    // SVG header
    if (opts->use_intrinsic_size) {
        // Responsive mode: width=100%, preserveAspectRatio
        svg_builder_append_fmt(b,
            "<svg xmlns=\"http://www.w3.org/2000/svg\" \n"
            "     viewBox=\"0 0 %.0f %.0f\" \n"
            "     width=\"100%%\" \n"
            "     style=\"max-width: %.0fpx;\" \n"
            "     preserveAspectRatio=\"xMidYMid meet\" \n"
            "     class=\"kryon-flowchart kryon-flowchart-theme-%s\"",
            svg_width, svg_height,
            svg_width,
            theme_class
        );
    } else {
        // Legacy mode: fixed dimensions
        svg_builder_append_fmt(b,
            "<svg xmlns=\"http://www.w3.org/2000/svg\" \n"
            "     viewBox=\"0 0 %d %d\" \n"
            "     class=\"kryon-flowchart kryon-flowchart-theme-%s\" \n"
            "     width=\"%d\" height=\"%d\"",
            opts->width, opts->height,
            theme_class,
            opts->width, opts->height
        );
    }

    // Accessibility attributes
    if (opts->accessibility) {
        svg_builder_append(b, " role=\"img\"");
        if (opts->title) {
            char* escaped_title = escape_html(opts->title);
            svg_builder_append_fmt(b, " aria-label=\"Flowchart: %s\"", escaped_title);
            free(escaped_title);
        }
    }

    svg_builder_append(b, ">\n");

    // Title and description for accessibility
    if (opts->accessibility) {
        if (opts->title) {
            char* escaped_title = escape_html(opts->title);
            svg_builder_append_fmt(b, "  <title>%s</title>\n", escaped_title);
            free(escaped_title);
        }
        if (opts->description) {
            char* escaped_desc = escape_html(opts->description);
            svg_builder_append_fmt(b, "  <desc>%s</desc>\n", escaped_desc);
            free(escaped_desc);
        }
    }

    // SVG definitions (markers for arrows)
    svg_builder_append(b, "  <defs>\n");

    // Arrow marker
    svg_builder_append_fmt(b,
        "    <marker id=\"arrow\" markerWidth=\"10\" markerHeight=\"10\" \n"
        "            refX=\"9\" refY=\"3\" orient=\"auto\">\n"
        "      <path d=\"M0,0 L0,6 L9,3 z\" fill=\"%s\" />\n"
        "    </marker>\n",
        colors.edge_color
    );

    // Bidirectional arrow marker
    svg_builder_append_fmt(b,
        "    <marker id=\"arrow-bi\" markerWidth=\"10\" markerHeight=\"10\" \n"
        "            refX=\"1\" refY=\"3\" orient=\"auto\">\n"
        "      <path d=\"M9,0 L9,6 L0,3 z\" fill=\"%s\" />\n"
        "    </marker>\n",
        colors.edge_color
    );

    svg_builder_append(b, "  </defs>\n\n");

    // CRITICAL Z-ORDER: Render back to front
    // 1. Subgraphs (background boxes) - FIRST
    // 2. Edges (connection lines) - MIDDLE
    // 3. Nodes (foreground) - LAST

    // Render subgraphs FIRST (background layer)
    svg_builder_append(b, "  <!-- Subgraphs (background boxes) -->\n");
    svg_builder_append(b, "  <g class=\"subgraphs\">\n");
    for (uint32_t i = 0; i < fc_state->subgraph_count; i++) {
        IRFlowchartSubgraphData* sg_data = fc_state->subgraphs[i];
        if (sg_data && sg_data->width > 0 && sg_data->height > 0) {
            svg_append_subgraph(b, sg_data, &colors, sg_data->subgraph_id);
        }
    }
    svg_builder_append(b, "  </g>\n\n");

    // Render edges second (middle layer)
    svg_builder_append(b, "  <!-- Edges -->\n");
    svg_builder_append(b, "  <g class=\"edges\">\n");
    for (uint32_t i = 0; i < fc_state->edge_count; i++) {
        svg_append_edge(b, fc_state->edges[i], &colors);
    }
    svg_builder_append(b, "  </g>\n\n");

    // Render nodes last (foreground layer)
    svg_builder_append(b, "  <!-- Nodes -->\n");
    svg_builder_append(b, "  <g class=\"nodes\">\n");
    for (uint32_t i = 0; i < fc_state->node_count; i++) {
        svg_append_node(b, fc_state->nodes[i], &colors, fc_state->nodes[i]->node_id);
    }
    svg_builder_append(b, "  </g>\n");

    // Close SVG
    svg_builder_append(b, "</svg>");

    // Extract result
    char* result = b->buffer;
    b->buffer = NULL;  // Prevent destruction from freeing the buffer
    svg_builder_destroy(b);

    return result;
}
