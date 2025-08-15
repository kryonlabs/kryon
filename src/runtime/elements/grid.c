/**
 * @file grid.c
 * @brief Implementation of the Grid element.
 *
 * This file contains the layout and rendering logic for Grid elements.
 * Grid elements arrange their children in a 2D grid layout with configurable
 * columns, rows, and spacing.
 *
 * 0BSD License
 */

#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include "color_utils.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// Forward declarations for the VTable functions
static void grid_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static bool grid_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
static void grid_destroy(KryonRuntime* runtime, KryonElement* element);

// The VTable binds the generic element interface to our specific grid functions.
static const ElementVTable g_grid_vtable = {
    .render = grid_render,
    .handle_event = grid_handle_event,
    .destroy = grid_destroy
};

// =============================================================================
//  Public Registration Function
// =============================================================================

/**
 * @brief Registers the Grid element type with the element registry.
 */
bool register_grid_element(void) {
    return element_register_type("Grid", &g_grid_vtable);
}

// =============================================================================
//  Grid Layout Calculation
// =============================================================================

/**
 * @brief Calculate and apply grid layout to all children
 */
static void calculate_grid_layout(KryonRuntime* runtime, KryonElement* grid) {
    if (!grid || grid->child_count == 0) return;
    
    // Get grid properties
    int columns = get_element_property_int(grid, "columns", 3);
    float gap = get_element_property_float(grid, "gap", 10.0f);
    float column_spacing = get_element_property_float(grid, "column_spacing", gap);
    float row_spacing = get_element_property_float(grid, "row_spacing", gap);
    float padding = get_element_property_float(grid, "padding", 0.0f);
    
    // Calculate grid structure
    int child_count = (int)grid->child_count;
    int rows = (int)ceil((double)child_count / (double)columns);
        
    // Calculate available space inside padding
    float content_x = grid->x + padding;
    float content_y = grid->y + padding;
    float content_width = grid->width - (padding * 2.0f);
    float content_height = grid->height - (padding * 2.0f);
    
    // Calculate cell dimensions
    float cell_width = (content_width - (column_spacing * (columns - 1))) / columns;
    float cell_height = (content_height - (row_spacing * (rows - 1))) / rows;
        
    // Position each child in the grid
    for (size_t i = 0; i < grid->child_count; i++) {
        KryonElement* child = grid->children[i];
        if (!child) continue;
        
        // Calculate grid position
        int row = (int)(i / columns);
        int col = (int)(i % columns);
        
        // Calculate child position
        float child_x = content_x + (col * (cell_width + column_spacing));
        float child_y = content_y + (row * (cell_height + row_spacing));
        
        // Set child dimensions and position
        child->x = child_x;
        child->y = child_y;
        child->width = cell_width;
        child->height = cell_height;
    }
    
    // Update grid's own size based on content
    float total_width = (columns * cell_width) + ((columns - 1) * column_spacing) + (padding * 2.0f);
    float total_height = (rows * cell_height) + ((rows - 1) * row_spacing) + (padding * 2.0f);
    
    // Only update grid size if it wasn't explicitly set
    if (get_element_property_float(grid, "width", 0.0f) == 0.0f) {
        grid->width = total_width;
    }
    if (get_element_property_float(grid, "height", 0.0f) == 0.0f) {
        grid->height = total_height;
    }
}

// =============================================================================
//  Element VTable Implementations
// =============================================================================

/**
 * @brief Renders the Grid element by calculating layout and rendering children
 */
static void grid_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 1) return;
    
    // Calculate grid layout before rendering
    calculate_grid_layout(runtime, element);
    
    // Grid itself can have background and border styling
    uint32_t bg_color_val = get_element_property_color(element, "backgroundColor", 0x00000000); // Transparent default
    if (bg_color_val == 0x00000000) {
        bg_color_val = get_element_property_color(element, "background", 0x00000000);
    }
    uint32_t border_color_val = get_element_property_color(element, "borderColor", 0x000000FF);
    float border_width = get_element_property_float(element, "borderWidth", 0.0f);
    float border_radius = get_element_property_float(element, "borderRadius", 0.0f);
    int z_index = get_element_property_int(element, "zIndex", 0);
    
    // Render grid background if visible
    if (bg_color_val != 0x00000000 || border_width > 0.0f) {
        KryonVec2 position = { element->x, element->y };
        KryonVec2 size = { element->width, element->height };
        KryonColor bg_color = color_u32_to_f32(bg_color_val);
        KryonColor border_color = color_u32_to_f32(border_color_val);
        
        KryonRenderCommand cmd = kryon_cmd_draw_rect(
            position,
            size,
            bg_color,
            border_radius
        );
        
        if (border_width > 0.0f) {
            cmd.data.draw_rect.border_width = border_width;
            cmd.data.draw_rect.border_color = border_color;
        }
        
        cmd.z_index = z_index;
        commands[(*command_count)++] = cmd;
    }
}

/**
 * @brief Handles events for the Grid element
 */
static bool grid_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    // Use the generic script event handler for standard events
    return generic_script_event_handler(runtime, element, event);
}

/**
 * @brief Cleans up any custom data for a Grid element
 */
static void grid_destroy(KryonRuntime* runtime, KryonElement* element) {
    // No custom data to free for a basic grid element
    (void)runtime;
    (void)element;
}