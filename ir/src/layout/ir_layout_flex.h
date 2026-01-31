// ir_layout_flex.h
//
// Consolidated flex layout interface
// Replaces duplicated row/column layout functions

#ifndef IR_LAYOUT_FLEX_H
#define IR_LAYOUT_FLEX_H

#include "../include/ir_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Layout direction for flex layout computation
 */
typedef enum {
    LAYOUT_AXIS_ROW,      // Horizontal (left to right)
    LAYOUT_AXIS_COLUMN    // Vertical (top to bottom)
} LayoutAxis;

/**
 * Consolidated flex layout computation
 *
 * This single function handles both row and column layouts by
 * parameterizing the layout axis, eliminating ~150 lines of
 * duplicated code.
 *
 * @param container      Container component to layout
 * @param available_width Available width for layout
 * @param available_height Available height for layout
 * @param axis           Layout axis (ROW for horizontal, COLUMN for vertical)
 */
void ir_layout_compute_flex(IRComponent* container,
                           float available_width,
                           float available_height,
                           LayoutAxis axis);

/**
 * Convenience wrapper for row layout
 * Equivalent to ir_layout_compute_flex(container, w, h, LAYOUT_AXIS_ROW)
 */
void ir_layout_compute_row_public(IRComponent* container,
                                 float available_width,
                                 float available_height);

/**
 * Convenience wrapper for column layout
 * Equivalent to ir_layout_compute_flex(container, w, h, LAYOUT_AXIS_COLUMN)
 */
void ir_layout_compute_column_public(IRComponent* container,
                                   float available_width,
                                   float available_height);

#ifdef __cplusplus
}
#endif

#endif // IR_LAYOUT_FLEX_H
