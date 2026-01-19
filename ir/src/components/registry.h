#ifndef IR_COMPONENT_REGISTRY_H
#define IR_COMPONENT_REGISTRY_H

#include "../../include/ir_core.h"

// Component layout trait - defines how a component type handles layout
typedef struct IRLayoutTrait {
    // Single-pass layout function
    // Computes final dimensions and positions for this component and its children
    void (*layout_component)(IRComponent* c, IRLayoutConstraints constraints,
                            float parent_x, float parent_y);

    // Trait metadata
    const char* name;
} IRLayoutTrait;

// Register a layout trait for a specific component type
void ir_layout_register_trait(IRComponentType type, const IRLayoutTrait* trait);

// Initialize all component traits (called once at startup)
void ir_layout_init_traits(void);

// Dispatch layout to the appropriate component trait
void ir_layout_dispatch(IRComponent* c, IRLayoutConstraints constraints,
                        float parent_x, float parent_y);

#endif // IR_COMPONENT_REGISTRY_H
