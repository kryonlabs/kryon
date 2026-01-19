#ifndef IR_COMPONENT_MISC_H
#define IR_COMPONENT_MISC_H

#include "../include/ir_core.h"
#include "registry.h"

#ifdef __cplusplus
extern "C" {
#endif

// Layout function for Markdown component
void layout_markdown_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                  float parent_x, float parent_y);

// Markdown layout trait (extern for registration)
extern const IRLayoutTrait IR_MARKDOWN_LAYOUT_TRAIT;

// Layout function for Sprite component
void layout_sprite_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                float parent_x, float parent_y);

// Sprite layout trait (extern for registration)
extern const IRLayoutTrait IR_SPRITE_LAYOUT_TRAIT;

// Initialize misc component layout traits (Markdown and Sprite)
void ir_misc_components_init(void);

#ifdef __cplusplus
}
#endif

#endif // IR_COMPONENT_MISC_H
