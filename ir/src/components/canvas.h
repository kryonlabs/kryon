#ifndef IR_COMPONENT_CANVAS_H
#define IR_COMPONENT_CANVAS_H

#include "../../include/ir_core.h"
#include "registry.h"

#ifdef __cplusplus
extern "C" {
#endif

// Layout function for Canvas component
void layout_canvas_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                float parent_x, float parent_y);

// Canvas layout trait (extern for registration)
extern const IRLayoutTrait IR_CANVAS_LAYOUT_TRAIT;

// Layout function for NativeCanvas component
void layout_native_canvas_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                       float parent_x, float parent_y);

// NativeCanvas layout trait (extern for registration)
extern const IRLayoutTrait IR_NATIVE_CANVAS_LAYOUT_TRAIT;

// Initialize Canvas component layout traits (both Canvas and NativeCanvas)
void ir_canvas_components_init(void);

#ifdef __cplusplus
}
#endif

#endif // IR_COMPONENT_CANVAS_H
