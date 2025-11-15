#include "include/kryon.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// Event Utility Functions
// ============================================================================

kryon_event_t kryon_event_create(kryon_event_type_t type, int16_t x, int16_t y, uint32_t param) {
    kryon_event_t event = {0};
    event.type = type;
    event.x = x;
    event.y = y;
    event.param = param;
    event.data = NULL;
    event.timestamp = 0; // Would be set by platform-specific code
    return event;
}

void kryon_event_set_data(kryon_event_t* event, void* data) {
    if (event != NULL) {
        event->data = data;
    }
}

void kryon_event_set_timestamp(kryon_event_t* event, uint32_t timestamp) {
    if (event != NULL) {
        event->timestamp = timestamp;
    }
}

// ============================================================================
// Event Propagation Helper Functions
// ============================================================================

bool kryon_event_is_point_in_component(kryon_component_t* component, int16_t x, int16_t y) {
    if (component == NULL || !component->visible) {
        return false;
    }

    // Calculate absolute position by accumulating parent offsets
    kryon_fp_t abs_x = component->x;
    kryon_fp_t abs_y = component->y;
    kryon_component_t* parent = component->parent;
    while (parent != NULL) {
        abs_x += parent->x;
        abs_y += parent->y;
        parent = parent->parent;
    }

    int16_t comp_x = KRYON_FP_TO_INT(abs_x);
    int16_t comp_y = KRYON_FP_TO_INT(abs_y);
    int16_t comp_w = KRYON_FP_TO_INT(component->width);
    int16_t comp_h = KRYON_FP_TO_INT(component->height);

    // Check normal component bounds
    bool in_bounds = (x >= comp_x && x < comp_x + comp_w &&
                      y >= comp_y && y < comp_y + comp_h);

    if (in_bounds) {
        return true;
    }

    // Special case: check extended bounds for open dropdowns
    // The dropdown menu renders below the component, extending the hit area
    if (component->ops == &kryon_dropdown_ops && component->state != NULL) {
        kryon_dropdown_state_t* dropdown_state = (kryon_dropdown_state_t*)component->state;
        if (dropdown_state->is_open) {
            // Menu extends below the dropdown button
            // option_height equals component height, menu_height = option_count * option_height
            int16_t menu_height = dropdown_state->option_count * comp_h;
            int16_t total_h = comp_h + menu_height;

            return (x >= comp_x && x < comp_x + comp_w &&
                    y >= comp_y && y < comp_y + total_h);
        }
    }

    return false;
}

kryon_component_t* kryon_event_find_target_at_point(kryon_component_t* root, int16_t x, int16_t y) {
    if (root == NULL || !kryon_event_is_point_in_component(root, x, y)) {
        return NULL;
    }

    // Disabled: too noisy
    // fprintf(stderr, "[kryon][hit] checking component %p: x=%d y=%d w=%d h=%d children=%d z_index=%d\n",
    //         (void*)root,
    //         KRYON_FP_TO_INT(root->x), KRYON_FP_TO_INT(root->y),
    //         KRYON_FP_TO_INT(root->width), KRYON_FP_TO_INT(root->height),
    //         root->child_count, root->z_index);

    // Find the child with highest z-index that contains the point
    // This ensures dropdowns and other "popup" elements capture events properly
    kryon_component_t* best_target = NULL;
    int16_t best_z_index = -32768; // Minimum possible int16_t

    for (int8_t i = 0; i < root->child_count; i++) {
        kryon_component_t* child = root->children[i];
        if (child == NULL) continue;

        // Check if point is in this child's bounds (including extended bounds for dropdowns)
        if (kryon_event_is_point_in_component(child, x, y)) {
            // Recursively find target in this child's subtree
            kryon_component_t* child_target = kryon_event_find_target_at_point(child, x, y);
            if (child_target != NULL) {
                // Get the effective z-index (use child's z-index if target is the child itself)
                int16_t effective_z = (child_target == child) ? child->z_index : child_target->z_index;

                if (effective_z > best_z_index) {
                    best_z_index = effective_z;
                    best_target = child_target;
                    // Disabled: too noisy
                    // fprintf(stderr, "[kryon][hit] new best target %p with z_index=%d\n",
                    //         (void*)best_target, best_z_index);
                }
            }
        }
    }

    if (best_target != NULL) {
        // Disabled: too noisy
        // fprintf(stderr, "[kryon][hit] found target in child: %p (z_index=%d)\n",
        //         (void*)best_target, best_z_index);
        return best_target;
    }

    // No child handled the event, return this component
    // Disabled: too noisy
    // fprintf(stderr, "[kryon][hit] no child matched, returning self: %p\n", (void*)root);
    return root;
}

void kryon_event_dispatch_to_point(kryon_component_t* root, int16_t x, int16_t y, kryon_event_t* event) {
    if (root == NULL || event == NULL) {
        return;
    }

    kryon_component_t* target = kryon_event_find_target_at_point(root, x, y);
    fprintf(stderr, "[kryon][event] dispatch_to_point(%d,%d): target=%p\n", x, y, (void*)target);
    if (target != NULL) {
        fprintf(stderr, "[kryon][event] target bounds: x=%d y=%d w=%d h=%d ops=%p\n",
                KRYON_FP_TO_INT(target->x), KRYON_FP_TO_INT(target->y),
                KRYON_FP_TO_INT(target->width), KRYON_FP_TO_INT(target->height),
                (void*)target->ops);
        kryon_component_send_event(target, event);
    } else {
        fprintf(stderr, "[kryon][event] No target found for click at (%d,%d)\n", x, y);
    }
}

// ============================================================================
// Event Factory Functions
// ============================================================================

kryon_event_t kryon_event_click(int16_t x, int16_t y, uint8_t button) {
    return kryon_event_create(KRYON_EVT_CLICK, x, y, button);
}

kryon_event_t kryon_event_touch(int16_t x, int16_t y, uint8_t touch_id) {
    return kryon_event_create(KRYON_EVT_TOUCH, x, y, touch_id);
}

kryon_event_t kryon_event_key(uint32_t key_code, bool pressed) {
    return kryon_event_create(KRYON_EVT_KEY, 0, 0, key_code | (pressed ? 0x80000000 : 0));
}

kryon_event_t kryon_event_timer(uint32_t timer_id) {
    return kryon_event_create(KRYON_EVT_TIMER, 0, 0, timer_id);
}

kryon_event_t kryon_event_hover(int16_t x, int16_t y, bool entering) {
    return kryon_event_create(KRYON_EVT_HOVER, x, y, entering ? 1 : 0);
}

kryon_event_t kryon_event_scroll(int16_t x, int16_t y, int16_t delta_x, int16_t delta_y) {
    kryon_event_t event = kryon_event_create(KRYON_EVT_SCROLL, x, y, 0);
    // Pack delta values into param (16 bits each)
    event.param = ((uint16_t)delta_x << 16) | (uint16_t)delta_y;
    return event;
}

kryon_event_t kryon_event_focus(bool gained) {
    return kryon_event_create(KRYON_EVT_FOCUS, 0, 0, gained ? 1 : 0);
}

kryon_event_t kryon_event_blur(bool lost) {
    return kryon_event_create(KRYON_EVT_BLUR, 0, 0, lost ? 1 : 0);
}

// ============================================================================
// Event Data Extraction Functions
// ============================================================================

uint8_t kryon_event_get_button(kryon_event_t* event) {
    if (event == NULL || event->type != KRYON_EVT_CLICK) {
        return 0;
    }
    return (uint8_t)event->param;
}

uint8_t kryon_event_get_touch_id(kryon_event_t* event) {
    if (event == NULL || event->type != KRYON_EVT_TOUCH) {
        return 0;
    }
    return (uint8_t)event->param;
}

uint32_t kryon_event_get_key_code(kryon_event_t* event) {
    if (event == NULL || event->type != KRYON_EVT_KEY) {
        return 0;
    }
    return event->param & 0x7FFFFFFF; // Remove pressed flag
}

bool kryon_event_is_key_pressed(kryon_event_t* event) {
    if (event == NULL || event->type != KRYON_EVT_KEY) {
        return false;
    }
    return (event->param & 0x80000000) != 0;
}

uint32_t kryon_event_get_timer_id(kryon_event_t* event) {
    if (event == NULL || event->type != KRYON_EVT_TIMER) {
        return 0;
    }
    return event->param;
}

bool kryon_event_is_hover_entering(kryon_event_t* event) {
    if (event == NULL || event->type != KRYON_EVT_HOVER) {
        return false;
    }
    return event->param != 0;
}

void kryon_event_get_scroll_delta(kryon_event_t* event, int16_t* delta_x, int16_t* delta_y) {
    if (event == NULL || event->type != KRYON_EVT_SCROLL || delta_x == NULL || delta_y == NULL) {
        if (delta_x) *delta_x = 0;
        if (delta_y) *delta_y = 0;
        return;
    }

    *delta_x = (int16_t)(event->param >> 16);
    *delta_y = (int16_t)(event->param & 0xFFFF);
}

bool kryon_event_is_focus_gained(kryon_event_t* event) {
    if (event == NULL || event->type != KRYON_EVT_FOCUS) {
        return false;
    }
    return event->param != 0;
}

bool kryon_event_is_focus_lost(kryon_event_t* event) {
    if (event == NULL || event->type != KRYON_EVT_BLUR) {
        return false;
    }
    return event->param != 0;
}