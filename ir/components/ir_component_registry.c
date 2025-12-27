#include "ir_component_registry.h"
#include "ir_component_text.h"
#include "ir_component_flexbox.h"
#include "ir_component_button.h"
#include "ir_component_checkbox.h"
#include "ir_component_dropdown.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global trait registry - maps component types to their layout traits
// NOTE: Must NOT be static - needs to be shared across all shared library users
IRLayoutTrait* g_layout_traits[IR_COMPONENT_CUSTOM + 1] = {0};

void ir_layout_register_trait(IRComponentType type, const IRLayoutTrait* trait) {
    if (type < 0 || type > IR_COMPONENT_CUSTOM) {
        fprintf(stderr, "[Registry] Invalid component type: %d\n", type);
        return;
    }

    g_layout_traits[type] = (IRLayoutTrait*)trait;

    if (getenv("KRYON_DEBUG_REGISTRY")) {
        fprintf(stderr, "[Registry] Registered trait for type %d: %s (array=%p, stored=%p)\n",
                type, trait ? trait->name : "NULL", (void*)g_layout_traits, (void*)g_layout_traits[type]);
    }
}

void ir_layout_init_traits(void) {
    // Initialize all component traits

    // Register Text component
    ir_text_component_init();

    // Register Flexbox components (Row, Column, Container)
    ir_flexbox_components_init();

    // Register Button component
    ir_button_component_init();

    // Register Checkbox component
    ir_checkbox_component_init();

    // Register Dropdown component
    ir_dropdown_component_init();

    if (getenv("KRYON_DEBUG_REGISTRY")) {
        fprintf(stderr, "[Registry] Layout traits initialized\n");
    }
}

void ir_layout_dispatch(IRComponent* c, IRLayoutConstraints constraints,
                        float parent_x, float parent_y) {
    if (!c) return;

    // Look up trait for this component type
    IRLayoutTrait* trait = (c->type >= 0 && c->type <= IR_COMPONENT_CUSTOM)
                          ? g_layout_traits[c->type]
                          : NULL;

    if (getenv("KRYON_DEBUG_REGISTRY")) {
        fprintf(stderr, "[Registry] Dispatch for type %d: trait=%p, has_func=%d (array=%p, stored=%p)\n",
                c->type, (void*)trait, trait && trait->layout_component ? 1 : 0,
                (void*)g_layout_traits, (void*)g_layout_traits[c->type]);
    }

    if (trait && trait->layout_component) {
        // Dispatch to component-specific layout
        trait->layout_component(c, constraints, parent_x, parent_y);
    } else {
        // Fallback: use old system for now
        // This will be replaced as we migrate components
        if (getenv("KRYON_DEBUG_REGISTRY")) {
            fprintf(stderr, "[Registry] No trait for component type %d, using fallback\n",
                    c->type);
        }
    }
}
