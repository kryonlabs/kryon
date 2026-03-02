/*
 * Kryon DSL Parser
 * C89/C90 compliant
 *
 * Parses .kryon files and creates windows/widgets
 */

#ifndef KRYON_PARSER_H
#define KRYON_PARSER_H

#include <stddef.h>

/*
 * Forward declarations
 */
struct KryonWindow;
struct KryonWidget;

/*
 * AST Node types
 */
typedef enum {
    NODE_WINDOW,
    NODE_LAYOUT,
    NODE_WIDGET,
    NODE_PROPERTY,
    NODE_ROOT
} NodeType;

/*
 * AST Node structure
 */
typedef struct KryonNode {
    NodeType type;
    char *value;                 /* For properties: "button", etc. */
    char *id;                    /* Widget/window ID */
    char *text;                  /* Display text (for widgets) */
    int width;                   /* Window width */
    int height;                  /* Window height */
    char *title;                 /* Window title */

    /* Layout properties */
    char *layout_type;           /* vbox, hbox, grid, absolute, stack */
    int layout_gap;
    int layout_padding;
    char *layout_align;          /* start, center, end, stretch */
    int layout_cols;
    int layout_rows;

    /* Widget properties */
    char *widget_type;           /* button, label, etc. */
    char *prop_rect;             /* "x y w h" */
    char *prop_color;            /* "#RRGGBB" */
    char *prop_value;            /* "0", "1", etc. */

    /* Tree structure */
    struct KryonNode **children;
    int nchildren;
    int child_capacity;

    /* Link to created objects (after execution) */
    struct KryonWindow *window;
    struct KryonWidget *widget;
} KryonNode;

/*
 * Parse .kryon file into AST
 * Returns NULL on error
 */
KryonNode* kryon_parse_file(const char *filename);

/*
 * Parse .kryon from string
 * Returns NULL on error
 */
KryonNode* kryon_parse_string(const char *source);

/*
 * Execute parsed AST - create windows/widgets
 * Returns number of windows created, or -1 on error
 */
int kryon_execute_ast(KryonNode *ast);

/*
 * Free parsed AST
 */
void kryon_free_ast(KryonNode *ast);

/*
 * Convenience: load and execute .kryon file
 * Returns number of windows created, or -1 on error
 */
int kryon_load_file(const char *filename);

#endif /* KRYON_PARSER_H */
