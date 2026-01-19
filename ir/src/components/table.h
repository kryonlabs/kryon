#ifndef IR_COMPONENT_TABLE_H
#define IR_COMPONENT_TABLE_H

#include "../include/ir_core.h"
#include "registry.h"

/**
 * Layout function for table components in single-pass system
 */
void layout_table_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                               float parent_x, float parent_y);

/**
 * Table component layout trait
 */
extern const IRLayoutTrait IR_TABLE_LAYOUT_TRAIT;

/**
 * Initialize table component (registers layout trait)
 * Should be called during IR system initialization
 */
void ir_table_component_init(void);

#endif // IR_COMPONENT_TABLE_H
