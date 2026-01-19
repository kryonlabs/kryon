#ifndef IR_COMPONENT_IMAGE_H
#define IR_COMPONENT_IMAGE_H

#include "../include/ir_core.h"
#include "registry.h"

#ifdef __cplusplus
extern "C" {
#endif

// Layout function for Image component
void layout_image_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                               float parent_x, float parent_y);

// Image layout trait (extern for registration)
extern const IRLayoutTrait IR_IMAGE_LAYOUT_TRAIT;

// Initialize Image component layout trait
void ir_image_component_init(void);

#ifdef __cplusplus
}
#endif

#endif // IR_COMPONENT_IMAGE_H
