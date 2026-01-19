#ifndef IR_COMPONENT_MODAL_H
#define IR_COMPONENT_MODAL_H

#include "../../include/ir_core.h"
#include "registry.h"

#ifdef __cplusplus
extern "C" {
#endif

// Layout function for Modal component
void layout_modal_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                               float parent_x, float parent_y);

// Modal layout trait (extern for registration)
extern const IRLayoutTrait IR_MODAL_LAYOUT_TRAIT;

// Initialize Modal component layout trait
void ir_modal_component_init(void);

#ifdef __cplusplus
}
#endif

#endif // IR_COMPONENT_MODAL_H
