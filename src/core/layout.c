/*
 * Kryon Layout System Implementation
 * C89/C90 compliant
 *
 * Calculates widget positions for various layout types
 */

#include "layout.h"
#include "../kryon/parser.h"
#include "widget.h"
#include "window.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * snprintf prototype for C89 compatibility
 */
extern int snprintf(char *str, size_t size, const char *format, ...);

/*
 * Helper: Parse alignment string to enum
 */
LayoutAlignment layout_parse_alignment(const char *str)
{
    if (str == NULL) {
        return ALIGN_START;  /* Default alignment */
    }

    if (strcmp(str, "start") == 0) return ALIGN_START;
    if (strcmp(str, "center") == 0) return ALIGN_CENTER;
    if (strcmp(str, "end") == 0) return ALIGN_END;
    if (strcmp(str, "stretch") == 0) return ALIGN_STRETCH;

    return ALIGN_START;  /* Default */
}

/*
 * Helper: Get default widget size based on type
 */
void layout_get_default_widget_size(const char *widget_type, int *width, int *height)
{
    if (widget_type == NULL) {
        *width = 100;
        *height = 30;
        return;
    }

    /* Default sizes for different widget types */
    if (strcmp(widget_type, "button") == 0) {
        *width = 120;
        *height = 26;
    } else if (strcmp(widget_type, "label") == 0) {
        *width = 100;
        *height = 30;
    } else if (strcmp(widget_type, "switch") == 0) {
        *width = 60;
        *height = 30;
    } else if (strcmp(widget_type, "slider") == 0) {
        *width = 150;
        *height = 30;
    } else if (strcmp(widget_type, "textinput") == 0) {
        *width = 150;
        *height = 35;
    } else if (strcmp(widget_type, "checkbox") == 0) {
        *width = 100;
        *height = 30;
    } else if (strcmp(widget_type, "radiobutton") == 0) {
        *width = 100;
        *height = 30;
    } else if (strcmp(widget_type, "dropdown") == 0) {
        *width = 120;
        *height = 35;
    } else if (strcmp(widget_type, "heading") == 0) {
        *width = 200;
        *height = 40;
    } else if (strcmp(widget_type, "paragraph") == 0) {
        *width = 250;
        *height = 60;
    } else if (strcmp(widget_type, "container") == 0) {
        *width = 200;
        *height = 150;
    } else if (strcmp(widget_type, "progressbar") == 0) {
        *width = 200;
        *height = 25;
    } else {
        /* Default size for unknown widgets */
        *width = 100;
        *height = 30;
    }
}

/*
 * Helper: Format rectangle as "x y w h" string
 * Caller must free the returned string
 */
char *layout_format_rect(int x, int y, int width, int height)
{
    char *rect_str;
    rect_str = (char *)malloc(64);
    if (rect_str == NULL) {
        return NULL;
    }
    snprintf(rect_str, 64, "%d %d %d %d", x, y, width, height);
    return rect_str;
}

/*
 * Helper: Apply padding to rectangle (inset by padding amount)
 */
Rectangle layout_apply_padding(Rectangle rect, int padding)
{
    Rectangle result;
    result.min.x = rect.min.x + padding;
    result.min.y = rect.min.y + padding;
    result.max.x = rect.max.x - padding;
    result.max.y = rect.max.y - padding;
    return result;
}

/*
 * VBox Layout Algorithm
 * Stacks children vertically with configurable gap and cross-axis alignment
 */
int layout_calculate_vbox(struct KryonNode *node, struct KryonWindow *win,
                          const LayoutContext *ctx)
{
    int i;
    Rectangle content_rect;
    int current_y;
    int container_width;
    int container_height;

    if (node == NULL || ctx == NULL) {
        return -1;
    }

    /* Apply padding to get content area */
    content_rect = layout_apply_padding(ctx->container_rect, ctx->padding);
    container_width = content_rect.max.x - content_rect.min.x;
    container_height = content_rect.max.y - content_rect.min.y;

    fprintf(stderr, "layout_calculate_vbox: container=%dx%d, gap=%d, padding=%d\n",
            container_width, container_height, ctx->gap, ctx->padding);

    current_y = content_rect.min.y;

    for (i = 0; i < node->nchildren; i++) {
        struct KryonNode *child = node->children[i];
        int widget_width, widget_height;
        int x, y;
        char *rect_str;

        /* Get default size for this widget type */
        layout_get_default_widget_size(child->widget_type, &widget_width, &widget_height);

        /* Calculate X based on alignment */
        switch (ctx->align) {
            case ALIGN_CENTER:
                x = content_rect.min.x + (container_width - widget_width) / 2;
                break;
            case ALIGN_END:
                x = content_rect.min.x + container_width - widget_width;
                break;
            case ALIGN_STRETCH:
                x = content_rect.min.x;
                widget_width = container_width;
                break;
            case ALIGN_START:
            default:
                x = content_rect.min.x;
                break;
        }

        /* Calculate Y position */
        y = current_y;

        /* Format and set rect */
        rect_str = layout_format_rect(x, y, widget_width, widget_height);
        if (rect_str == NULL) {
            return -1;
        }

        /* Free existing rect if present */
        if (child->prop_rect != NULL) {
            free(child->prop_rect);
        }
        child->prop_rect = rect_str;

        fprintf(stderr, "  vbox child[%d]: %s -> rect='%s'\n",
                i, child->widget_type ? child->widget_type : "(null)", rect_str);

        /* Handle nested layouts */
        if (child->type == NODE_LAYOUT) {
            Rectangle child_rect;
            child_rect.min.x = x;
            child_rect.min.y = y;
            child_rect.max.x = x + widget_width;
            child_rect.max.y = y + widget_height;

            if (layout_calculate(child, win, child_rect) != 0) {
                return -1;
            }
        }

        /* Advance Y position */
        current_y += widget_height + ctx->gap;
    }

    return 0;
}

/*
 * HBox Layout Algorithm
 * Arranges children horizontally with configurable gap and cross-axis alignment
 */
int layout_calculate_hbox(struct KryonNode *node, struct KryonWindow *win,
                          const LayoutContext *ctx)
{
    int i;
    Rectangle content_rect;
    int current_x;
    int container_width;
    int container_height;

    if (node == NULL || ctx == NULL) {
        return -1;
    }

    /* Apply padding to get content area */
    content_rect = layout_apply_padding(ctx->container_rect, ctx->padding);
    container_width = content_rect.max.x - content_rect.min.x;
    container_height = content_rect.max.y - content_rect.min.y;

    fprintf(stderr, "layout_calculate_hbox: container=%dx%d, gap=%d, padding=%d\n",
            container_width, container_height, ctx->gap, ctx->padding);

    current_x = content_rect.min.x;

    for (i = 0; i < node->nchildren; i++) {
        struct KryonNode *child = node->children[i];
        int widget_width, widget_height;
        int x, y;
        char *rect_str;

        /* Get default size for this widget type */
        layout_get_default_widget_size(child->widget_type, &widget_width, &widget_height);

        /* Calculate X position */
        x = current_x;

        /* Calculate Y based on alignment */
        switch (ctx->align) {
            case ALIGN_CENTER:
                y = content_rect.min.y + (container_height - widget_height) / 2;
                break;
            case ALIGN_END:
                y = content_rect.min.y + container_height - widget_height;
                break;
            case ALIGN_STRETCH:
                y = content_rect.min.y;
                widget_height = container_height;
                break;
            case ALIGN_START:
            default:
                y = content_rect.min.y;
                break;
        }

        /* Format and set rect */
        rect_str = layout_format_rect(x, y, widget_width, widget_height);
        if (rect_str == NULL) {
            return -1;
        }

        /* Free existing rect if present */
        if (child->prop_rect != NULL) {
            free(child->prop_rect);
        }
        child->prop_rect = rect_str;

        fprintf(stderr, "  hbox child[%d]: %s -> rect='%s'\n",
                i, child->widget_type ? child->widget_type : "(null)", rect_str);

        /* Handle nested layouts */
        if (child->type == NODE_LAYOUT) {
            Rectangle child_rect;
            child_rect.min.x = x;
            child_rect.min.y = y;
            child_rect.max.x = x + widget_width;
            child_rect.max.y = y + widget_height;

            if (layout_calculate(child, win, child_rect) != 0) {
                return -1;
            }
        }

        /* Advance X position */
        current_x += widget_width + ctx->gap;
    }

    return 0;
}

/*
 * Grid Layout Algorithm
 * Arranges children in a grid with configurable columns
 */
int layout_calculate_grid(struct KryonNode *node, struct KryonWindow *win,
                          const LayoutContext *ctx)
{
    int i;
    Rectangle content_rect;
    int cols, rows;
    int cell_width, cell_height;

    if (node == NULL || ctx == NULL) {
        return -1;
    }

    /* Apply padding to get content area */
    content_rect = layout_apply_padding(ctx->container_rect, ctx->padding);

    /* Determine grid dimensions */
    cols = node->layout_cols;
    if (cols <= 0) {
        cols = 2;  /* Default to 2 columns */
    }

    rows = (node->nchildren + cols - 1) / cols;  /* Round up */
    if (rows <= 0) {
        rows = 1;
    }

    cell_width = (content_rect.max.x - content_rect.min.x) / cols;
    cell_height = (content_rect.max.y - content_rect.min.y) / rows;

    fprintf(stderr, "layout_calculate_grid: %dx%d grid, cell=%dx%d, gap=%d, padding=%d\n",
            cols, rows, cell_width, cell_height, ctx->gap, ctx->padding);

    for (i = 0; i < node->nchildren; i++) {
        struct KryonNode *child = node->children[i];
        int row, col;
        int x, y;
        int widget_width, widget_height;
        char *rect_str;

        /* Calculate grid position */
        row = i / cols;
        col = i % cols;

        /* Base position */
        x = content_rect.min.x + col * cell_width;
        y = content_rect.min.y + row * cell_height;

        /* Apply gap to inset within cell */
        if (ctx->gap > 0) {
            x += ctx->gap;
            y += ctx->gap;
            widget_width = cell_width - (2 * ctx->gap);
            widget_height = cell_height - (2 * ctx->gap);
        } else {
            widget_width = cell_width;
            widget_height = cell_height;
        }

        /* Ensure minimum size */
        if (widget_width < 10) widget_width = 10;
        if (widget_height < 10) widget_height = 10;

        /* Format and set rect */
        rect_str = layout_format_rect(x, y, widget_width, widget_height);
        if (rect_str == NULL) {
            return -1;
        }

        /* Free existing rect if present */
        if (child->prop_rect != NULL) {
            free(child->prop_rect);
        }
        child->prop_rect = rect_str;

        fprintf(stderr, "  grid child[%d] (row=%d,col=%d): %s -> rect='%s'\n",
                i, row, col, child->widget_type ? child->widget_type : "(null)", rect_str);

        /* Handle nested layouts */
        if (child->type == NODE_LAYOUT) {
            Rectangle child_rect;
            child_rect.min.x = x;
            child_rect.min.y = y;
            child_rect.max.x = x + widget_width;
            child_rect.max.y = y + widget_height;

            if (layout_calculate(child, win, child_rect) != 0) {
                return -1;
            }
        }
    }

    return 0;
}

/*
 * Absolute Layout Algorithm
 * Children have manually specified positions
 */
int layout_calculate_absolute(struct KryonNode *node, struct KryonWindow *win,
                              const LayoutContext *ctx)
{
    int i;

    if (node == NULL || ctx == NULL) {
        return -1;
    }

    fprintf(stderr, "layout_calculate_absolute: padding=%d\n", ctx->padding);

    for (i = 0; i < node->nchildren; i++) {
        struct KryonNode *child = node->children[i];

        /* Children already have rects from parsing, just handle nested layouts */
        if (child->type == NODE_LAYOUT && child->prop_rect != NULL) {
            Rectangle child_rect;
            int x, y, w, h;

            /* Parse the existing rect to get bounds for nested layouts */
            if (sscanf(child->prop_rect, "%d %d %d %d", &x, &y, &w, &h) == 4) {
                child_rect.min.x = x;
                child_rect.min.y = y;
                child_rect.max.x = x + w;
                child_rect.max.y = y + h;

                if (layout_calculate(child, win, child_rect) != 0) {
                    return -1;
                }
            }
        }
    }

    return 0;
}

/*
 * Stack Layout Algorithm
 * All children overlap, filling the container
 */
int layout_calculate_stack(struct KryonNode *node, struct KryonWindow *win,
                           const LayoutContext *ctx)
{
    int i;
    Rectangle content_rect;

    if (node == NULL || ctx == NULL) {
        return -1;
    }

    /* Apply padding to get content area */
    content_rect = layout_apply_padding(ctx->container_rect, ctx->padding);

    fprintf(stderr, "layout_calculate_stack: container=%dx%d, padding=%d\n",
            content_rect.max.x - content_rect.min.x,
            content_rect.max.y - content_rect.min.y,
            ctx->padding);

    for (i = 0; i < node->nchildren; i++) {
        struct KryonNode *child = node->children[i];
        char *rect_str;

        /* All children get the same rect */
        rect_str = layout_format_rect(content_rect.min.x, content_rect.min.y,
                                      content_rect.max.x - content_rect.min.x,
                                      content_rect.max.y - content_rect.min.y);
        if (rect_str == NULL) {
            return -1;
        }

        /* Free existing rect if present */
        if (child->prop_rect != NULL) {
            free(child->prop_rect);
        }
        child->prop_rect = rect_str;

        fprintf(stderr, "  stack child[%d]: %s -> rect='%s'\n",
                i, child->widget_type ? child->widget_type : "(null)", rect_str);

        /* Handle nested layouts */
        if (child->type == NODE_LAYOUT) {
            if (layout_calculate(child, win, content_rect) != 0) {
                return -1;
            }
        }
    }

    return 0;
}

/*
 * Main Layout Dispatcher
 * Calculates positions for all children based on layout type
 */
int layout_calculate(struct KryonNode *node, struct KryonWindow *win,
                     Rectangle parent_rect)
{
    LayoutContext ctx;

    if (node == NULL || win == NULL) {
        return -1;
    }

    /* Initialize layout context from node properties */
    ctx.container_rect = parent_rect;
    ctx.gap = node->layout_gap;
    ctx.padding = node->layout_padding;
    ctx.align = layout_parse_alignment(node->layout_align);
    ctx.layout_type = node->layout_type;

    fprintf(stderr, "layout_calculate: type='%s', gap=%d, padding=%d, align=%d\n",
            node->layout_type ? node->layout_type : "(null)",
            ctx.gap, ctx.padding, ctx.align);

    /* Dispatch to specific layout algorithm */
    if (node->layout_type == NULL) {
        fprintf(stderr, "layout_calculate: warning - no layout_type specified\n");
        return -1;
    }

    if (strcmp(node->layout_type, "vbox") == 0) {
        return layout_calculate_vbox(node, win, &ctx);
    } else if (strcmp(node->layout_type, "hbox") == 0) {
        return layout_calculate_hbox(node, win, &ctx);
    } else if (strcmp(node->layout_type, "grid") == 0) {
        return layout_calculate_grid(node, win, &ctx);
    } else if (strcmp(node->layout_type, "absolute") == 0) {
        return layout_calculate_absolute(node, win, &ctx);
    } else if (strcmp(node->layout_type, "stack") == 0) {
        return layout_calculate_stack(node, win, &ctx);
    } else {
        fprintf(stderr, "layout_calculate: unknown layout type '%s'\n",
                node->layout_type);
        return -1;
    }

    return 0;
}
