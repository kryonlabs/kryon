#ifndef TERMINAL_FLOWCHART_H
#define TERMINAL_FLOWCHART_H

#include "../../ir/ir_core.h"
#include <stdbool.h>
#include <stdint.h>

// Terminal Capabilities
typedef struct {
    bool unicode_box_drawing;   // Can render Unicode box-drawing characters
    bool unicode_arrows;         // Can render Unicode arrows
    int color_depth;             // 0=none, 16, 256, or 16777216 (truecolor)
    int max_cols;                // Terminal width in columns
    int max_rows;                // Terminal height in rows
    bool supports_ansi;          // Supports ANSI escape codes
} TerminalCapabilities;

// Coordinate Scaling (pixels to terminal cells)
typedef struct {
    float pixels_per_col;
    float pixels_per_row;
    int total_cols;
    int total_rows;
    float offset_x;
    float offset_y;
} TerminalScaling;

// Terminal Cell Position
typedef struct {
    int col;
    int row;
} TerminalCell;

// Terminal Buffer (2D array of characters and colors)
typedef struct {
    char** chars;                // 2D array of characters
    uint32_t** colors;           // 2D array of colors (RGBA)
    int width;                   // Buffer width in columns
    int height;                  // Buffer height in rows
} TerminalBuffer;

// Function Prototypes

// Capability Detection
TerminalCapabilities detect_terminal_capabilities(void);
void print_terminal_capabilities(const TerminalCapabilities* caps);

// Coordinate Scaling
TerminalScaling calculate_scaling(const IRFlowchartState* fc_state, int available_cols, int available_rows);
TerminalCell pixels_to_cell(float px_x, float px_y, const TerminalScaling* scale);

// Terminal Buffer Management
TerminalBuffer* terminal_buffer_create(int width, int height);
void terminal_buffer_destroy(TerminalBuffer* buffer);
void terminal_buffer_clear(TerminalBuffer* buffer);
void terminal_buffer_set_char(TerminalBuffer* buffer, int col, int row, char ch);
void terminal_buffer_set_char_colored(TerminalBuffer* buffer, int col, int row, char ch, uint32_t color);
void terminal_buffer_render(const TerminalBuffer* buffer, const TerminalCapabilities* caps);

// Node Shape Rendering
void render_node_terminal(TerminalBuffer* buffer, const IRFlowchartNodeData* node,
                         const TerminalScaling* scale, const TerminalCapabilities* caps);
void render_rectangle_terminal(TerminalBuffer* buffer, TerminalCell pos, int w, int h,
                              const TerminalCapabilities* caps);
void render_rounded_terminal(TerminalBuffer* buffer, TerminalCell pos, int w, int h,
                            const TerminalCapabilities* caps);
void render_diamond_terminal(TerminalBuffer* buffer, TerminalCell pos, int w, int h,
                            const TerminalCapabilities* caps);
void render_circle_terminal(TerminalBuffer* buffer, TerminalCell pos, int w, int h,
                           const TerminalCapabilities* caps);
void render_hexagon_terminal(TerminalBuffer* buffer, TerminalCell pos, int w, int h,
                            const TerminalCapabilities* caps);
void render_cylinder_terminal(TerminalBuffer* buffer, TerminalCell pos, int w, int h,
                             const TerminalCapabilities* caps);

// Edge Rendering
void render_edge_terminal(TerminalBuffer* buffer, const IRFlowchartEdgeData* edge,
                         const TerminalScaling* scale, const TerminalCapabilities* caps);
void draw_line_terminal(TerminalBuffer* buffer, TerminalCell c1, TerminalCell c2,
                       IRFlowchartEdgeType edge_type, const TerminalCapabilities* caps);

// Text Rendering
void render_label_centered(TerminalBuffer* buffer, TerminalCell pos, int w, int h, const char* label);

// Color Support (ANSI Escape Codes)
void set_terminal_color_rgb(uint8_t r, uint8_t g, uint8_t b, bool foreground, const TerminalCapabilities* caps);
void reset_terminal_color(void);
int rgb_to_256color(uint8_t r, uint8_t g, uint8_t b);
int rgb_to_16color(uint8_t r, uint8_t g, uint8_t b);

// Main Terminal Flowchart Renderer
bool render_flowchart_terminal(IRComponent* flowchart, const TerminalCapabilities* caps);

#endif // TERMINAL_FLOWCHART_H
