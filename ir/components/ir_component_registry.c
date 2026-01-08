#include "ir_component_registry.h"
#include "ir_component_text.h"
#include "ir_component_flexbox.h"
#include "ir_component_button.h"
#include "ir_component_checkbox.h"
#include "ir_component_dropdown.h"
#include "ir_component_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration for dispatch
extern void ir_layout_dispatch(IRComponent* c, IRLayoutConstraints constraints,
                                float parent_x, float parent_y);

// Global trait registry - maps component types to their layout traits
// NOTE: Must NOT be static - needs to be shared across all shared library users
// NOTE: Array size must accommodate all component types up to IR_COMPONENT_PLACEHOLDER
IRLayoutTrait* g_layout_traits[IR_COMPONENT_PLACEHOLDER + 1] = {0};

// ============================================================================
// ForEach Layout Trait
// ============================================================================

/**
 * Layout function for ForEach components.
 * ForEach is "layout-transparent" - like CSS display: contents.
 * It doesn't render itself; it just recursively layouts its children.
 * The actual expansion of ForEach (duplicating template for each item)
 * happens via ir_expand_foreach() which is called after loading KIR.
 */
static void layout_foreach_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                       float parent_x, float parent_y) {
    if (!c) return;

    // ForEach is layout-transparent - just recursively layout children
    // without adding any dimensions of its own
    for (uint32_t i = 0; i < c->child_count; i++) {
        if (c->children[i]) {
            ir_layout_dispatch(c->children[i], constraints, parent_x, parent_y);
        }
    }
}

static const IRLayoutTrait IR_FOR_EACH_LAYOUT_TRAIT = {
    .layout_component = layout_foreach_single_pass,
    .name = "ForEach"
};

void ir_layout_register_trait(IRComponentType type, const IRLayoutTrait* trait) {
    if (type < 0 || type > IR_COMPONENT_PLACEHOLDER) {
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

    // Register Table component
    ir_table_component_init();

    // Register ForEach component (layout-transparent - just passes through to children)
    ir_layout_register_trait(IR_COMPONENT_FOR_EACH, &IR_FOR_EACH_LAYOUT_TRAIT);

    if (getenv("KRYON_DEBUG_REGISTRY")) {
        fprintf(stderr, "[Registry] Layout traits initialized\n");
    }
}

void ir_layout_dispatch(IRComponent* c, IRLayoutConstraints constraints,
                        float parent_x, float parent_y) {
    if (!c) return;

    // Look up trait for this component type
    IRLayoutTrait* trait = (c->type >= 0 && c->type <= IR_COMPONENT_PLACEHOLDER)
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
