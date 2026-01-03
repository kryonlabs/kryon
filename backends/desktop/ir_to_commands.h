#ifndef IR_TO_COMMANDS_H
#define IR_TO_COMMANDS_H

#include "../../core/include/kryon.h"
#include "../../ir/ir_core.h"
#include "desktop_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * IR Component to Command Buffer Translation
 * ============================================================================
 *
 * This module handles the translation of IR components into command buffer
 * commands for rendering. It provides a clean separation between:
 *   - High-level IR component knowledge (backends/desktop/)
 *   - Low-level rendering primitives (renderers/)
 *
 * The translation process:
 *   1. Traverse the IR component tree
 *   2. Generate rendering commands based on component type and style
 *   3. Handle opacity cascading, transforms, z-index sorting
 *   4. Manage multi-pass rendering for overlays (dropdowns, dragged tabs)
 *
 * Usage:
 *   kryon_cmd_buf_t cmd_buf;
 *   kryon_cmd_buf_init(&cmd_buf);
 *   ir_component_to_commands(root, &cmd_buf, &root_rect, 1.0f);
 *   renderer->ops->execute_commands(renderer, &cmd_buf);
 */

/* Command generation context - tracks state during tree traversal */
typedef struct IRCommandContext {
    kryon_cmd_buf_t* cmd_buf;           // Target command buffer

    /* Opacity stack for cascading */
    float opacity_stack[16];
    int opacity_depth;
    float current_opacity;

    /* Transform stack for nested transforms */
    float transform_stack[16][6];       // 6 floats per 2D matrix
    int transform_depth;

    /* Rendering pass tracking */
    uint8_t current_pass;               // 0=main, 1=overlay

    /* Resource tracking */
    uint16_t next_font_id;
    uint16_t next_image_id;

    /* Renderer backend pointer (for font resolution, etc.) */
    void* renderer;

    /* Backend context for plugin renderers */
    void* backend_ctx;

    /* Deferred overlay components (dropdowns, dragged tabs) */
    IRComponent* overlays[16];
    int overlay_count;

    /* Root component flag - skip background for root since SDL_RenderClear handles it */
    bool is_root_component;

} IRCommandContext;

/* ============================================================================
 * Main API - IR Component to Commands
 * ============================================================================ */

/**
 * Generate rendering commands for an IR component tree.
 *
 * @param component   Root IR component to render
 * @param cmd_buf     Command buffer to populate
 * @param bounds      Bounding rectangle for the root component
 * @param opacity     Initial opacity (1.0 = fully opaque)
 * @return true on success, false on error
 */
bool ir_component_to_commands(
    IRComponent* component,
    kryon_cmd_buf_t* cmd_buf,
    LayoutRect* bounds,
    float opacity,
    void* backend_ctx
);

/**
 * Generate commands for a specific component type (internal use).
 * This is called recursively during tree traversal.
 */
bool ir_generate_component_commands(
    IRComponent* component,
    IRCommandContext* ctx,
    LayoutRect* bounds,
    float inherited_opacity
);

/* ============================================================================
 * Component-Specific Command Generators
 * ============================================================================ */

bool ir_gen_container_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds);
bool ir_gen_text_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds);
bool ir_gen_button_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds);
bool ir_gen_input_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds);
bool ir_gen_checkbox_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds);
bool ir_gen_dropdown_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds);
bool ir_gen_image_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds);
bool ir_gen_table_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds);
bool ir_gen_markdown_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds);
bool ir_gen_canvas_commands(IRComponent* comp, IRCommandContext* ctx, LayoutRect* bounds);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Apply opacity to a color value.
 */
uint32_t ir_apply_opacity_to_color(uint32_t color, float opacity);

/**
 * Push opacity onto the stack and emit SET_OPACITY command.
 */
void ir_push_opacity(IRCommandContext* ctx, float opacity);

/**
 * Pop opacity from the stack and emit POP_OPACITY command.
 */
void ir_pop_opacity(IRCommandContext* ctx);

/**
 * Check if a component should be deferred to overlay pass.
 */
bool ir_should_defer_to_overlay(IRComponent* comp, IRCommandContext* ctx);

/**
 * Defer a component to the overlay rendering pass.
 */
void ir_defer_to_overlay(IRCommandContext* ctx, IRComponent* comp);

#ifdef __cplusplus
}
#endif

#endif /* IR_TO_COMMANDS_H */
