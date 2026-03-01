/*
 * Kryon Layout System
 * C89/C90 compliant
 *
 * Calculates widget positions for vbox, hbox, grid, absolute, and stack layouts
 */

#ifndef KRYON_LAYOUT_H
#define KRYON_LAYOUT_H

/*
 * Forward declarations
 */
struct KryonWindow;
struct KryonNode;

/*
 * Include graphics types (Point, Rectangle) from canonical source
 */
#include "graphics.h"

/*
 * Alignment enumeration
 */
typedef enum {
    ALIGN_START = 0,
    ALIGN_CENTER,
    ALIGN_END,
    ALIGN_STRETCH
} LayoutAlignment;

/*
 * Layout context - tracks container space and layout properties
 */
typedef struct {
    Rectangle container_rect;   /* Available space for layout */
    int gap;                    /* Gap between children */
    int padding;                /* Padding around edges */
    LayoutAlignment align;      /* Cross-axis alignment */
    const char *layout_type;    /* Type of layout */
} LayoutContext;

/*
 * Main layout dispatcher - calculates positions for all children
 * Returns 0 on success, -1 on error
 */
int layout_calculate(struct KryonNode *node, struct KryonWindow *win,
                     Rectangle parent_rect);

/*
 * Layout-specific calculation functions
 */
int layout_calculate_vbox(struct KryonNode *node, struct KryonWindow *win,
                          const LayoutContext *ctx);
int layout_calculate_hbox(struct KryonNode *node, struct KryonWindow *win,
                          const LayoutContext *ctx);
int layout_calculate_grid(struct KryonNode *node, struct KryonWindow *win,
                          const LayoutContext *ctx);
int layout_calculate_absolute(struct KryonNode *node, struct KryonWindow *win,
                              const LayoutContext *ctx);
int layout_calculate_stack(struct KryonNode *node, struct KryonWindow *win,
                           const LayoutContext *ctx);

/*
 * Helper functions
 */
LayoutAlignment layout_parse_alignment(const char *str);
void layout_get_default_widget_size(const char *widget_type, int *width, int *height);
char *layout_format_rect(int x, int y, int width, int height);
Rectangle layout_apply_padding(Rectangle rect, int padding);

#endif /* KRYON_LAYOUT_H */
