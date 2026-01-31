#include "registry.h"
#include "../include/ir_core.h"
#include "text.h"
#include "flexbox.h"
#include "button.h"
#include "input.h"
#include "checkbox.h"
#include "dropdown.h"
#include "table.h"
#include "image.h"
#include "modal.h"
#include "canvas.h"
#include "misc.h"
#include "tabs.h"
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
 * Normally ForEach is replaced by its expanded children in the parent.
 * However, for root-level ForEach (parent is NULL), it remains in the tree
 * and acts as a vertical container for its expanded children.
 *
 * This function stacks children vertically to handle the root-level case.
 */
static void layout_foreach_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                       float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists for position tracking
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // ForEach acts as a vertical container - stack children vertically
    float current_y = parent_y;
    float max_width = 0;

    for (uint32_t i = 0; i < c->child_count; i++) {
        if (c->children[i]) {
            // Skip invisible children
            if (c->children[i]->style && !c->children[i]->style->visible) continue;

            IRLayoutConstraints child_constraints = {
                .max_width = constraints.max_width,
                .max_height = constraints.max_height - (current_y - parent_y),
                .min_width = 0,
                .min_height = 0
            };

            ir_layout_dispatch(c->children[i], child_constraints, parent_x, current_y);

            // Advance Y position based on child's computed height
            if (c->children[i]->layout_state && c->children[i]->layout_state->computed.valid) {
                current_y += c->children[i]->layout_state->computed.height;
                if (c->children[i]->layout_state->computed.width > max_width) {
                    max_width = c->children[i]->layout_state->computed.width;
                }
            }
        }
    }

    // Set ForEach's own dimensions (sum of children heights, max child width)
    c->layout_state->computed.x = parent_x;
    c->layout_state->computed.y = parent_y;
    c->layout_state->computed.width = max_width > 0 ? max_width : constraints.max_width;
    c->layout_state->computed.height = current_y - parent_y;
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;
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

    // Register Input component
    ir_input_component_init();

    // Register Checkbox component
    ir_checkbox_component_init();

    // Register Dropdown component
    ir_dropdown_component_init();

    // Register Table component
    ir_table_component_init();

    // Register Image component
    ir_image_component_init();

    // Register Modal component
    ir_modal_component_init();

    // Register Canvas components (Canvas and NativeCanvas)
    ir_canvas_components_init();

    // Register misc components (Markdown, Sprite)
    ir_misc_components_init();

    // Register Tab components (TabGroup, TabBar, Tab, TabContent, TabPanel)
    ir_tab_components_init();

    // Register For component (layout-transparent - just passes through to children)
    ir_layout_register_trait(IR_COMPONENT_FOR_LOOP, &IR_FOR_EACH_LAYOUT_TRAIT);  // Keep trait name for now

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
