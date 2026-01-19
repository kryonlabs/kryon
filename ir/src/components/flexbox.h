#ifndef IR_COMPONENT_FLEXBOX_H
#define IR_COMPONENT_FLEXBOX_H

#include "../include/ir_core.h"
#include "../include/ir_component_registry.h"

/**
 * Unified Flexbox Component - Axis-Parameterized Layout
 *
 * Eliminates code duplication between Row/Column/Container by using
 * a unified implementation with axis abstraction.
 *
 * Row = Flexbox with HORIZONTAL axis
 * Column = Flexbox with VERTICAL axis
 * Container = Flexbox with configurable direction
 */

// Layout axis (determines whether we're laying out horizontally or vertically)
typedef enum {
    LAYOUT_AXIS_HORIZONTAL = 0,  // Row: layout along X-axis (width is main, height is cross)
    LAYOUT_AXIS_VERTICAL = 1     // Column: layout along Y-axis (height is main, width is cross)
} LayoutAxis;

// Single-pass layout functions for flexbox components
void layout_flexbox_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                float parent_x, float parent_y, LayoutAxis axis);

void layout_row_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                            float parent_x, float parent_y);

void layout_column_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                               float parent_x, float parent_y);

void layout_container_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                  float parent_x, float parent_y);

void layout_center_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                float parent_x, float parent_y);

// Component traits
extern const IRLayoutTrait IR_ROW_LAYOUT_TRAIT;
extern const IRLayoutTrait IR_COLUMN_LAYOUT_TRAIT;
extern const IRLayoutTrait IR_CONTAINER_LAYOUT_TRAIT;
extern const IRLayoutTrait IR_CENTER_LAYOUT_TRAIT;

// Initialize and register flexbox component traits
void ir_flexbox_components_init(void);

#endif // IR_COMPONENT_FLEXBOX_H
