#ifndef IR_WIREFRAME_H
#define IR_WIREFRAME_H

#include <stdbool.h>
#include <stdint.h>

/* Forward declarations */
typedef struct IRComponent IRComponent;
typedef struct IRCommandContext IRCommandContext;
typedef struct LayoutRect LayoutRect;

/* Wireframe configuration */
typedef struct {
    bool enabled;
    uint32_t color;           /* Default: 0xFF0000FF (red) */
    bool show_for_each;       /* Highlight ForEach containers */
    bool show_reactive;       /* Highlight reactive state containers */
    bool show_text_expr;      /* Highlight text expressions */
    bool show_conditional;    /* Highlight conditional rendering */
} ir_wireframe_config_t;

/* Get wireframe configuration from environment variables */
void ir_wireframe_load_config(ir_wireframe_config_t* config);

/* Check if wireframe mode is enabled */
bool ir_wireframe_is_enabled(void);

/* Get current wireframe color */
uint32_t ir_wireframe_get_color(void);

/* Check if a component should render as wireframe */
bool ir_wireframe_should_render(struct IRComponent* component, ir_wireframe_config_t* config);

/* Generate wireframe outline commands */
bool ir_wireframe_render_outline(
    struct IRComponent* component,
    struct IRCommandContext* ctx,
    struct LayoutRect* bounds,
    uint32_t color,
    const char* label
);

#endif /* IR_WIREFRAME_H */
