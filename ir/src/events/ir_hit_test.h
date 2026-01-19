#ifndef IR_HIT_TEST_H
#define IR_HIT_TEST_H

#include <stdbool.h>
#include "../include/ir_core.h"

// ============================================================================
// Hit Testing - Find component at a given point
// ============================================================================

// Check if a point is within a component's rendered bounds
bool ir_is_point_in_component(IRComponent* component, float x, float y);

// Find the component at a given point (considers z-index and visibility)
IRComponent* ir_find_component_at_point(IRComponent* root, float x, float y);

// Set the rendered bounds of a component (for hit testing)
void ir_set_rendered_bounds(IRComponent* component, float x, float y, float width, float height);

#endif // IR_HIT_TEST_H
