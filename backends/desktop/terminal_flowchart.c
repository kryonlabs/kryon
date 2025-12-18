#include "terminal_flowchart.h"
#include "../../ir/ir_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <float.h>

// =============================================================================
// Terminal Capability Detection
// =============================================================================

TerminalCapabilities detect_terminal_capabilities(void) {
    TerminalCapabilities caps = {0};

    // Check TERM environment variable
    const char* term = getenv("TERM");
    if (!term) term = "unknown";

    // Unicode support
    caps.unicode_box_drawing = (strstr(term, "xterm") != NULL ||
                                strstr(term, "256") != NULL ||
                                strstr(term, "rxvt") != NULL ||
                                strstr(term, "gnome") != NULL);
    caps.unicode_arrows = caps.unicode_box_drawing;

    // Color depth detection
    if (strstr(term, "truecolor") != NULL || strstr(term, "24bit") != NULL) {
        caps.color_depth = 16777216;  // 24-bit truecolor
    } else if (strstr(term, "256color") != NULL) {
        caps.color_depth = 256;
    } else if (strstr(term, "color") != NULL) {
        caps.color_depth = 16;
    } else {
        caps.color_depth = 0;  // No color support
    }

    // ANSI support (same as color support)
    caps.supports_ansi = (caps.color_depth > 0);

    // Get terminal size using ioctl
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        caps.max_cols = w.ws_col > 0 ? w.ws_col : 80;
        caps.max_rows = w.ws_row > 0 ? w.ws_row : 24;
    } else {
        caps.max_cols = 80;
        caps.max_rows = 24;
    }

    return caps;
}

void print_terminal_capabilities(const TerminalCapabilities* caps) {
    printf("Terminal Capabilities:\n");
    printf("  Unicode box-drawing: %s\n", caps->unicode_box_drawing ? "Yes" : "No");
    printf("  Unicode arrows: %s\n", caps->unicode_arrows ? "Yes" : "No");
    printf("  Color depth: %d%s\n", caps->color_depth,
           caps->color_depth == 16777216 ? " (truecolor)" :
           caps->color_depth == 256 ? " (256-color)" :
           caps->color_depth == 16 ? " (16-color)" : " (no color)");
    printf("  Terminal size: %dx%d\n", caps->max_cols, caps->max_rows);
    printf("  ANSI support: %s\n", caps->supports_ansi ? "Yes" : "No");
}

// =============================================================================
// Coordinate Scaling
// =============================================================================

TerminalScaling calculate_scaling(const IRFlowchartState* fc_state, int available_cols, int available_rows) {
    if (!fc_state || fc_state->node_count == 0) {
        return (TerminalScaling){1.0f, 1.0f, available_cols, available_rows, 0, 0};
    }

    // Find bounding box of all nodes
    float min_x = FLT_MAX, max_x = 0;
    float min_y = FLT_MAX, max_y = 0;

    for (uint32_t i = 0; i < fc_state->node_count; i++) {
        IRFlowchartNodeData* node = fc_state->nodes[i];
        if (!node) continue;

        min_x = fminf(min_x, node->x);
        max_x = fmaxf(max_x, node->x + node->width);
        min_y = fminf(min_y, node->y);
        max_y = fmaxf(max_y, node->y + node->height);
    }

    float width = max_x - min_x;
    float height = max_y - min_y;

    // Add padding
    int padding_cols = 2;
    int padding_rows = 2;

    return (TerminalScaling){
        .pixels_per_col = width / (available_cols - padding_cols * 2),
        .pixels_per_row = height / (available_rows - padding_rows * 2),
        .total_cols = available_cols,
        .total_rows = available_rows,
        .offset_x = min_x,
        .offset_y = min_y
    };
}

TerminalCell pixels_to_cell(float px_x, float px_y, const TerminalScaling* scale) {
    return (TerminalCell){
        .col = (int)((px_x - scale->offset_x) / scale->pixels_per_col) + 1,
        .row = (int)((px_y - scale->offset_y) / scale->pixels_per_row) + 1
    };
}

// =============================================================================
// Terminal Buffer Management
// =============================================================================

TerminalBuffer* terminal_buffer_create(int width, int height) {
    TerminalBuffer* buffer = malloc(sizeof(TerminalBuffer));
    if (!buffer) return NULL;

    buffer->width = width;
    buffer->height = height;

    // Allocate 2D character array
    buffer->chars = malloc(sizeof(char*) * height);
    buffer->colors = malloc(sizeof(uint32_t*) * height);

    for (int row = 0; row < height; row++) {
        buffer->chars[row] = malloc(sizeof(char) * width);
        buffer->colors[row] = malloc(sizeof(uint32_t) * width);
        memset(buffer->chars[row], ' ', width);
        memset(buffer->colors[row], 0, width * sizeof(uint32_t));
    }

    return buffer;
}

void terminal_buffer_destroy(TerminalBuffer* buffer) {
    if (!buffer) return;

    for (int row = 0; row < buffer->height; row++) {
        free(buffer->chars[row]);
        free(buffer->colors[row]);
    }
    free(buffer->chars);
    free(buffer->colors);
    free(buffer);
}

void terminal_buffer_clear(TerminalBuffer* buffer) {
    for (int row = 0; row < buffer->height; row++) {
        memset(buffer->chars[row], ' ', buffer->width);
        memset(buffer->colors[row], 0, buffer->width * sizeof(uint32_t));
    }
}

void terminal_buffer_set_char(TerminalBuffer* buffer, int col, int row, char ch) {
    if (col >= 0 && col < buffer->width && row >= 0 && row < buffer->height) {
        buffer->chars[row][col] = ch;
    }
}

void terminal_buffer_set_char_colored(TerminalBuffer* buffer, int col, int row, char ch, uint32_t color) {
    if (col >= 0 && col < buffer->width && row >= 0 && row < buffer->height) {
        buffer->chars[row][col] = ch;
        buffer->colors[row][col] = color;
    }
}

void terminal_buffer_render(const TerminalBuffer* buffer, const TerminalCapabilities* caps) {
    // Clear screen
    printf("\033[2J\033[H");

    for (int row = 0; row < buffer->height; row++) {
        for (int col = 0; col < buffer->width; col++) {
            uint32_t color = buffer->colors[row][col];
            if (color != 0 && caps->supports_ansi) {
                uint8_t r = (color >> 24) & 0xFF;
                uint8_t g = (color >> 16) & 0xFF;
                uint8_t b = (color >> 8) & 0xFF;
                set_terminal_color_rgb(r, g, b, true, caps);
            }

            putchar(buffer->chars[row][col]);

            if (color != 0 && caps->supports_ansi) {
                reset_terminal_color();
            }
        }
        putchar('\n');
    }

    fflush(stdout);
}

// =============================================================================
// Node Shape Rendering
// =============================================================================

void render_label_centered(TerminalBuffer* buffer, TerminalCell pos, int w, int h, const char* label) {
    if (!label || w <= 0 || h <= 0) return;

    int label_len = strlen(label);
    int label_col = pos.col + (w - label_len) / 2;
    int label_row = pos.row + h / 2;

    // Truncate label if too long
    int max_len = w - 2;
    if (max_len <= 0) return;

    for (int i = 0; i < label_len && i < max_len; i++) {
        terminal_buffer_set_char(buffer, label_col + i, label_row, label[i]);
    }
}

void render_rectangle_terminal(TerminalBuffer* buffer, TerminalCell pos, int w, int h, const TerminalCapabilities* caps) {
    if (w < 2 || h < 2) return;

    const char* tl = caps->unicode_box_drawing ? "┌" : "+";
    const char* tr = caps->unicode_box_drawing ? "┐" : "+";
    const char* bl = caps->unicode_box_drawing ? "└" : "+";
    const char* br = caps->unicode_box_drawing ? "┘" : "+";
    const char* hz = caps->unicode_box_drawing ? "─" : "-";
    const char* vt = caps->unicode_box_drawing ? "│" : "|";

    // Top edge
    terminal_buffer_set_char(buffer, pos.col, pos.row, tl[0]);
    for (int i = 1; i < w - 1; i++) {
        terminal_buffer_set_char(buffer, pos.col + i, pos.row, hz[0]);
    }
    terminal_buffer_set_char(buffer, pos.col + w - 1, pos.row, tr[0]);

    // Sides
    for (int i = 1; i < h - 1; i++) {
        terminal_buffer_set_char(buffer, pos.col, pos.row + i, vt[0]);
        terminal_buffer_set_char(buffer, pos.col + w - 1, pos.row + i, vt[0]);
    }

    // Bottom edge
    terminal_buffer_set_char(buffer, pos.col, pos.row + h - 1, bl[0]);
    for (int i = 1; i < w - 1; i++) {
        terminal_buffer_set_char(buffer, pos.col + i, pos.row + h - 1, hz[0]);
    }
    terminal_buffer_set_char(buffer, pos.col + w - 1, pos.row + h - 1, br[0]);
}

void render_rounded_terminal(TerminalBuffer* buffer, TerminalCell pos, int w, int h, const TerminalCapabilities* caps) {
    if (w < 2 || h < 2) return;

    const char* tl = caps->unicode_box_drawing ? "╭" : "/";
    const char* tr = caps->unicode_box_drawing ? "╮" : "\\";
    const char* bl = caps->unicode_box_drawing ? "╰" : "\\";
    const char* br = caps->unicode_box_drawing ? "╯" : "/";
    const char* hz = caps->unicode_box_drawing ? "─" : "-";
    const char* vt = caps->unicode_box_drawing ? "│" : "|";

    // Top edge
    terminal_buffer_set_char(buffer, pos.col, pos.row, tl[0]);
    for (int i = 1; i < w - 1; i++) {
        terminal_buffer_set_char(buffer, pos.col + i, pos.row, hz[0]);
    }
    terminal_buffer_set_char(buffer, pos.col + w - 1, pos.row, tr[0]);

    // Sides
    for (int i = 1; i < h - 1; i++) {
        terminal_buffer_set_char(buffer, pos.col, pos.row + i, vt[0]);
        terminal_buffer_set_char(buffer, pos.col + w - 1, pos.row + i, vt[0]);
    }

    // Bottom edge
    terminal_buffer_set_char(buffer, pos.col, pos.row + h - 1, bl[0]);
    for (int i = 1; i < w - 1; i++) {
        terminal_buffer_set_char(buffer, pos.col + i, pos.row + h - 1, hz[0]);
    }
    terminal_buffer_set_char(buffer, pos.col + w - 1, pos.row + h - 1, br[0]);
}

void render_diamond_terminal(TerminalBuffer* buffer, TerminalCell pos, int w, int h, const TerminalCapabilities* caps) {
    if (w < 3 || h < 3) return;

    int cx = pos.col + w / 2;
    int cy = pos.row + h / 2;
    int hw = w / 2;
    int hh = h / 2;

    // Top half
    for (int row = 0; row < hh; row++) {
        int width = (row * hw) / hh;
        terminal_buffer_set_char(buffer, cx - width, pos.row + row, '/');
        terminal_buffer_set_char(buffer, cx + width, pos.row + row, '\\');
    }

    // Bottom half
    for (int row = 0; row < hh; row++) {
        int width = hw - (row * hw) / hh;
        terminal_buffer_set_char(buffer, cx - width, cy + row, '\\');
        terminal_buffer_set_char(buffer, cx + width, cy + row, '/');
    }
}

void render_circle_terminal(TerminalBuffer* buffer, TerminalCell pos, int w, int h, const TerminalCapabilities* caps) {
    if (w < 3 || h < 3) return;

    int cx = pos.col + w / 2;
    int cy = pos.row + h / 2;
    float rx = w / 2.0f;
    float ry = h / 2.0f;

    // Draw circle using midpoint circle algorithm (adapted for ellipse)
    for (int row = 0; row < h; row++) {
        float y = row - cy;
        float x_offset = rx * sqrtf(1.0f - (y * y) / (ry * ry));

        if (!isnan(x_offset) && !isinf(x_offset)) {
            int left = cx - (int)x_offset;
            int right = cx + (int)x_offset;

            if (row == 0 || row == h - 1) {
                // Top and bottom edges
                for (int col = left; col <= right; col++) {
                    terminal_buffer_set_char(buffer, col, pos.row + row, '-');
                }
            } else {
                // Left and right edges
                terminal_buffer_set_char(buffer, left, pos.row + row, '(');
                terminal_buffer_set_char(buffer, right, pos.row + row, ')');
            }
        }
    }
}

void render_hexagon_terminal(TerminalBuffer* buffer, TerminalCell pos, int w, int h, const TerminalCapabilities* caps) {
    if (w < 5 || h < 3) return;

    int third_h = h / 3;

    // Top slant
    for (int row = 0; row < third_h; row++) {
        int offset = (third_h - row) * w / (third_h * 4);
        terminal_buffer_set_char(buffer, pos.col + offset, pos.row + row, '/');
        terminal_buffer_set_char(buffer, pos.col + w - 1 - offset, pos.row + row, '\\');
    }

    // Middle straight section
    for (int row = third_h; row < h - third_h; row++) {
        terminal_buffer_set_char(buffer, pos.col, pos.row + row, '|');
        terminal_buffer_set_char(buffer, pos.col + w - 1, pos.row + row, '|');
    }

    // Bottom slant
    for (int row = h - third_h; row < h; row++) {
        int offset = (row - (h - third_h)) * w / (third_h * 4);
        terminal_buffer_set_char(buffer, pos.col + offset, pos.row + row, '\\');
        terminal_buffer_set_char(buffer, pos.col + w - 1 - offset, pos.row + row, '/');
    }
}

void render_cylinder_terminal(TerminalBuffer* buffer, TerminalCell pos, int w, int h, const TerminalCapabilities* caps) {
    if (w < 3 || h < 3) return;

    // Top ellipse
    for (int col = 1; col < w - 1; col++) {
        terminal_buffer_set_char(buffer, pos.col + col, pos.row, '-');
    }
    terminal_buffer_set_char(buffer, pos.col, pos.row, '(');
    terminal_buffer_set_char(buffer, pos.col + w - 1, pos.row, ')');

    // Sides
    for (int row = 1; row < h - 1; row++) {
        terminal_buffer_set_char(buffer, pos.col, pos.row + row, '|');
        terminal_buffer_set_char(buffer, pos.col + w - 1, pos.row + row, '|');
    }

    // Bottom ellipse
    for (int col = 1; col < w - 1; col++) {
        terminal_buffer_set_char(buffer, pos.col + col, pos.row + h - 1, '-');
    }
    terminal_buffer_set_char(buffer, pos.col, pos.row + h - 1, '(');
    terminal_buffer_set_char(buffer, pos.col + w - 1, pos.row + h - 1, ')');
}

void render_node_terminal(TerminalBuffer* buffer, const IRFlowchartNodeData* node,
                         const TerminalScaling* scale, const TerminalCapabilities* caps) {
    if (!node || !buffer || !scale) return;

    TerminalCell top_left = pixels_to_cell(node->x, node->y, scale);
    TerminalCell bottom_right = pixels_to_cell(node->x + node->width, node->y + node->height, scale);

    int width = bottom_right.col - top_left.col;
    int height = bottom_right.row - top_left.row;

    // Minimum size: 3 cols × 3 rows
    if (width < 3) width = 3;
    if (height < 3) height = 3;

    // Render shape
    switch (node->shape) {
        case IR_FLOWCHART_SHAPE_RECTANGLE:
            render_rectangle_terminal(buffer, top_left, width, height, caps);
            break;
        case IR_FLOWCHART_SHAPE_ROUNDED:
        case IR_FLOWCHART_SHAPE_STADIUM:
            render_rounded_terminal(buffer, top_left, width, height, caps);
            break;
        case IR_FLOWCHART_SHAPE_DIAMOND:
            render_diamond_terminal(buffer, top_left, width, height, caps);
            break;
        case IR_FLOWCHART_SHAPE_CIRCLE:
            render_circle_terminal(buffer, top_left, width, height, caps);
            break;
        case IR_FLOWCHART_SHAPE_HEXAGON:
            render_hexagon_terminal(buffer, top_left, width, height, caps);
            break;
        case IR_FLOWCHART_SHAPE_CYLINDER:
            render_cylinder_terminal(buffer, top_left, width, height, caps);
            break;
        case IR_FLOWCHART_SHAPE_SUBROUTINE:
            // Render as rectangle with double sides
            render_rectangle_terminal(buffer, top_left, width, height, caps);
            if (width > 2) {
                for (int row = 0; row < height; row++) {
                    terminal_buffer_set_char(buffer, top_left.col + 1, top_left.row + row, '|');
                    terminal_buffer_set_char(buffer, top_left.col + width - 2, top_left.row + row, '|');
                }
            }
            break;
        case IR_FLOWCHART_SHAPE_ASYMMETRIC:
            // Render as trapezoid
            for (int row = 0; row < height; row++) {
                int left_offset = row / 2;
                int right_offset = row / 2;
                terminal_buffer_set_char(buffer, top_left.col + left_offset, top_left.row + row, '/');
                terminal_buffer_set_char(buffer, top_left.col + width - 1 - right_offset, top_left.row + row, '\\');
            }
            break;
        default:
            render_rectangle_terminal(buffer, top_left, width, height, caps);
            break;
    }

    // Render label
    if (node->label) {
        render_label_centered(buffer, top_left, width, height, node->label);
    }
}

// =============================================================================
// Edge Rendering
// =============================================================================

void draw_line_terminal(TerminalBuffer* buffer, TerminalCell c1, TerminalCell c2,
                       IRFlowchartEdgeType edge_type, const TerminalCapabilities* caps) {
    // Bresenham's line algorithm
    int dx = abs(c2.col - c1.col);
    int dy = abs(c2.row - c1.row);
    int sx = (c1.col < c2.col) ? 1 : -1;
    int sy = (c1.row < c2.row) ? 1 : -1;
    int err = dx - dy;

    int col = c1.col;
    int row = c1.row;

    const char ch_hz = (edge_type == IR_FLOWCHART_EDGE_DOTTED) ? '.' : '-';
    const char ch_vt = (edge_type == IR_FLOWCHART_EDGE_DOTTED) ? ':' : '|';

    while (1) {
        // Choose character based on direction
        char ch = (dx > dy) ? ch_hz : ch_vt;
        terminal_buffer_set_char(buffer, col, row, ch);

        if (col == c2.col && row == c2.row) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            col += sx;
        }
        if (e2 < dx) {
            err += dx;
            row += sy;
        }
    }
}

void render_edge_terminal(TerminalBuffer* buffer, const IRFlowchartEdgeData* edge,
                         const TerminalScaling* scale, const TerminalCapabilities* caps) {
    if (!edge || !edge->path_points || edge->path_point_count < 2) return;

    const char* arrow = caps->unicode_arrows ? "→" : ">";
    const char* bi_arrow_l = caps->unicode_arrows ? "←" : "<";

    // Draw each segment
    for (uint32_t p = 0; p < edge->path_point_count - 1; p++) {
        TerminalCell c1 = pixels_to_cell(edge->path_points[p * 2], edge->path_points[p * 2 + 1], scale);
        TerminalCell c2 = pixels_to_cell(edge->path_points[(p + 1) * 2], edge->path_points[(p + 1) * 2 + 1], scale);

        draw_line_terminal(buffer, c1, c2, edge->type, caps);
    }

    // Draw arrow heads
    if (edge->type != IR_FLOWCHART_EDGE_OPEN) {
        TerminalCell end = pixels_to_cell(
            edge->path_points[(edge->path_point_count - 1) * 2],
            edge->path_points[(edge->path_point_count - 1) * 2 + 1],
            scale);
        terminal_buffer_set_char(buffer, end.col, end.row, arrow[0]);

        if (edge->type == IR_FLOWCHART_EDGE_BIDIRECTIONAL) {
            TerminalCell start = pixels_to_cell(
                edge->path_points[0],
                edge->path_points[1],
                scale);
            terminal_buffer_set_char(buffer, start.col, start.row, bi_arrow_l[0]);
        }
    }

    // Draw edge label if present
    if (edge->label) {
        // Find midpoint of path
        uint32_t mid_idx = edge->path_point_count / 2;
        TerminalCell mid = pixels_to_cell(
            edge->path_points[mid_idx * 2],
            edge->path_points[mid_idx * 2 + 1],
            scale);

        int label_len = strlen(edge->label);
        for (int i = 0; i < label_len && i < 10; i++) {
            terminal_buffer_set_char(buffer, mid.col + i, mid.row, edge->label[i]);
        }
    }
}

// =============================================================================
// Color Support (ANSI Escape Codes)
// =============================================================================

void set_terminal_color_rgb(uint8_t r, uint8_t g, uint8_t b, bool foreground, const TerminalCapabilities* caps) {
    if (!caps->supports_ansi) return;

    if (caps->color_depth == 16777216) {
        // Truecolor (24-bit)
        printf("\033[%d;2;%d;%d;%dm", foreground ? 38 : 48, r, g, b);
    } else if (caps->color_depth == 256) {
        // 256-color palette
        int color_idx = rgb_to_256color(r, g, b);
        printf("\033[%d;5;%dm", foreground ? 38 : 48, color_idx);
    } else if (caps->color_depth == 16) {
        // 16-color ANSI
        int color_idx = rgb_to_16color(r, g, b);
        printf("\033[%dm", foreground ? (30 + color_idx) : (40 + color_idx));
    }
}

void reset_terminal_color(void) {
    printf("\033[0m");
}

int rgb_to_256color(uint8_t r, uint8_t g, uint8_t b) {
    // Convert RGB to 256-color palette (simple approximation)
    int r_idx = r * 5 / 255;
    int g_idx = g * 5 / 255;
    int b_idx = b * 5 / 255;
    return 16 + (r_idx * 36) + (g_idx * 6) + b_idx;
}

int rgb_to_16color(uint8_t r, uint8_t g, uint8_t b) {
    // Convert RGB to 16-color ANSI (simple mapping)
    int brightness = (r + g + b) / 3;
    bool bright = brightness > 128;

    if (r > g && r > b) return bright ? 9 : 1;  // Red
    if (g > r && g > b) return bright ? 10 : 2;  // Green
    if (b > r && b > g) return bright ? 12 : 4;  // Blue
    if (r > 128 && g > 128) return bright ? 11 : 3;  // Yellow
    if (r > 128 && b > 128) return bright ? 13 : 5;  // Magenta
    if (g > 128 && b > 128) return bright ? 14 : 6;  // Cyan
    return bright ? 15 : 7;  // White/Gray
}

// =============================================================================
// Main Terminal Flowchart Renderer
// =============================================================================

bool render_flowchart_terminal(IRComponent* flowchart, const TerminalCapabilities* caps) {
    if (!flowchart || flowchart->type != IR_COMPONENT_FLOWCHART) {
        fprintf(stderr, "Error: Not a flowchart component\n");
        return false;
    }

    IRFlowchartState* fc_state = ir_get_flowchart_state(flowchart);
    if (!fc_state) {
        fprintf(stderr, "Error: No flowchart state\n");
        return false;
    }

    // Compute layout if not already done
    if (!fc_state->layout_computed) {
        ir_layout_compute_flowchart(flowchart, caps->max_cols * 10.0f, caps->max_rows * 10.0f);
    }

    // Calculate scaling
    TerminalScaling scale = calculate_scaling(fc_state, caps->max_cols, caps->max_rows);

    // Create terminal buffer
    TerminalBuffer* buffer = terminal_buffer_create(caps->max_cols, caps->max_rows);
    if (!buffer) {
        fprintf(stderr, "Error: Failed to create terminal buffer\n");
        return false;
    }

    terminal_buffer_clear(buffer);

    // Render edges first (behind nodes)
    for (uint32_t i = 0; i < fc_state->edge_count; i++) {
        render_edge_terminal(buffer, fc_state->edges[i], &scale, caps);
    }

    // Render nodes
    for (uint32_t i = 0; i < fc_state->node_count; i++) {
        render_node_terminal(buffer, fc_state->nodes[i], &scale, caps);
    }

    // Render to terminal
    terminal_buffer_render(buffer, caps);

    // Cleanup
    terminal_buffer_destroy(buffer);

    return true;
}
