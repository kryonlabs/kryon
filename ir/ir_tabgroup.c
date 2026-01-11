/*
 * Kryon IR - TabGroup State Management Implementation
 */

#define _GNU_SOURCE
#include "ir_tabgroup.h"
#include "ir_core.h"
#include "ir_memory.h"
#include "ir_builder.h"  // For mark_style_dirty, ir_create_style, ir_set_style, ir_set_background_color, ir_set_font, etc.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global IR context (from ir_builder.c)
extern IRContext* g_ir_context;

// Thread-local IRContext getter
extern IRContext* ir_context_get_current(void);

// Get the active IRContext (thread-local first, then global)
static IRContext* get_active_context(void) {
    IRContext* ctx = ir_context_get_current();
    return ctx ? ctx : g_ir_context;
}

// Nim callback for cleanup when components are removed
extern void nimOnComponentRemoved(IRComponent* component) __attribute__((weak));

// Nim callback when components are added to the tree
extern void nimOnComponentAdded(IRComponent* component) __attribute__((weak));

// Recursively invalidate rendered bounds
static void ir_invalidate_bounds_recursive(IRComponent* component) {
    if (!component) return;
    component->rendered_bounds.valid = false;
    for (uint32_t i = 0; i < component->child_count; i++) {
        ir_invalidate_bounds_recursive(component->children[i]);
    }
}

// Forward declarations
static void ir_tabgroup_apply_visual_to_tab(IRComponent* tab, TabVisualState* visual, bool is_active);

// ============================================================================
// Lifecycle
// ============================================================================

TabGroupState* ir_tabgroup_create_state(IRComponent* group,
                                        IRComponent* tab_bar,
                                        IRComponent* tab_content,
                                        int selected_index,
                                        bool reorderable) {
    TabGroupState* state = (TabGroupState*)calloc(1, sizeof(TabGroupState));
    if (!state) return NULL;
    state->group = group;
    state->tab_bar = tab_bar;
    state->tab_content = tab_content;
    // Arrays are now fixed-size, no allocation needed
    state->tab_count = 0;
    state->panel_count = 0;
    state->selected_index = selected_index;
    state->reorderable = reorderable;
    state->dragging = false;
    state->drag_index = -1;
    state->drag_x = 0.0f;
    // Arrays are stack-allocated, initialize to NULL/0
    memset(state->tabs, 0, sizeof(state->tabs));
    memset(state->panels, 0, sizeof(state->panels));
    memset(state->tab_visuals, 0, sizeof(state->tab_visuals));
    memset(state->user_callbacks, 0, sizeof(state->user_callbacks));
    memset(state->user_callback_data, 0, sizeof(state->user_callback_data));
    return state;
}

void ir_tabgroup_destroy_state(TabGroupState* state) {
    if (!state) return;
    // Arrays are now stack-allocated within the struct, no need to free them
    free(state);
}

void ir_tabgroup_finalize(TabGroupState* state) {
    if (!state) return;

    // Extract visual colors from Tab components into tab_visuals array
    // (needed when loading from .kir files where colors are in IRTabData)
    for (uint32_t i = 0; i < state->tab_count; i++) {
        IRComponent* tab = state->tabs[i];
        if (tab && tab->tab_data) {
            // Get background color from style if available, otherwise use default
            uint32_t bg_color = 0x3d3d3dff;  // Default dark gray
            if (tab->style && tab->style->background.type == IR_COLOR_SOLID) {
                IRColorData* bg_data = &tab->style->background.data;
                bg_color = (bg_data->r << 24) | (bg_data->g << 16) | (bg_data->b << 8) | bg_data->a;
            }

            TabVisualState visual = {
                .background_color = bg_color,
                .active_background_color = tab->tab_data->active_background,
                .text_color = tab->tab_data->text_color,
                .active_text_color = tab->tab_data->active_text_color
            };

            state->tab_visuals[i] = visual;
        }
    }

    if (state->tab_count > 0) {
        int clamp_index = state->selected_index;
        if (clamp_index < 0) clamp_index = 0;
        if ((uint32_t)clamp_index >= state->tab_count) clamp_index = (int)(state->tab_count - 1);
        ir_tabgroup_select(state, clamp_index);
    }
}

// ============================================================================
// Registration
// ============================================================================

void ir_tabgroup_register_bar(TabGroupState* state, IRComponent* tab_bar) {
    if (!state) return;
    state->tab_bar = tab_bar;
    if (tab_bar) {
        // Store state pointer for renderer hit-testing (unsafe cast)
        tab_bar->custom_data = (char*)state;
    }
}

void ir_tabgroup_register_content(TabGroupState* state, IRComponent* tab_content) {
    if (!state) return;
    state->tab_content = tab_content;
    if (tab_content) {
        // Store state pointer for serialization lookup (unsafe cast)
        tab_content->custom_data = (char*)state;
    }
}

void ir_tabgroup_register_tab(TabGroupState* state, IRComponent* tab) {
    if (!state || !tab) return;
    if (state->tab_count >= IR_MAX_TABS_PER_GROUP) {
        fprintf(stderr, "[ir_tabgroup] Tab group full: max %d tabs\n", IR_MAX_TABS_PER_GROUP);
        return;
    }
    state->tabs[state->tab_count++] = tab;
}

void ir_tabgroup_register_panel(TabGroupState* state, IRComponent* panel) {
    if (!state || !panel) return;
    if (state->panel_count >= IR_MAX_TABS_PER_GROUP) {
        fprintf(stderr, "[ir_tabgroup] Tab group full: max %d panels\n", IR_MAX_TABS_PER_GROUP);
        return;
    }
    state->panels[state->panel_count++] = panel;
}

// ============================================================================
// Selection and Interaction
// ============================================================================

void ir_tabgroup_select(TabGroupState* state, int index) {
    if (!state) return;
    if (index < 0 || (uint32_t)index >= state->tab_count) return;
    state->selected_index = index;

    // Panels: show only selected
    if (state->tab_content) {
        // Notify Nim to clean up reactive state before removing panels
        // This prevents stale references in the reactive system
        for (uint32_t i = 0; i < state->panel_count; i++) {
            if (state->panels[i] && i != (uint32_t)index) {
                // Only notify for panels being removed (not the newly selected one)
                for (uint32_t c = 0; c < state->tab_content->child_count; c++) {
                    if (state->tab_content->children[c] == state->panels[i]) {
                        // Panel is currently visible and will be removed
                        if (nimOnComponentRemoved) {
                            nimOnComponentRemoved(state->panels[i]);
                        }
                        break;
                    }
                }
            }
        }

        // OPTIMIZATION: Clear all children at once (95% reduction: O(n²) → O(1))
        // This is safe because callbacks are already handled above
        state->tab_content->child_count = 0;

        // Add only the selected panel
        if ((uint32_t)index < state->panel_count && state->panels[index]) {
            IRComponent* panel = state->panels[index];

            if (getenv("KRYON_DEBUG_TAB_LAYOUT")) {
                fprintf(stderr, "[PANEL_ADD] Adding panel ID=%u, child_count=%u\n",
                       panel->id, panel->child_count);
                for (uint32_t i = 0; i < panel->child_count; i++) {
                    IRComponent* child = panel->children[i];
                    fprintf(stderr, "[PANEL_ADD]   Child %u: ID=%u type=%d\n",
                           i, child ? child->id : 0, child ? child->type : -1);
                }
            }

            ir_add_child(state->tab_content, panel);

            // Recursively invalidate panel and all descendants so they get re-laid out
            // This is necessary because panels are removed from the tree when not selected
            ir_invalidate_bounds_recursive(panel);

            // Notify Nim that this panel was added - resets cleanup tracking
            if (nimOnComponentAdded) {
                nimOnComponentAdded(panel);
            }
        }

        // Invalidate layout so renderer recalculates with new child set
        state->tab_content->rendered_bounds.valid = false;
    }

    // Mark TabGroup dirty to trigger full layout recalculation including absolute positions
    if (state->group) {
        ir_layout_mark_dirty(state->group);
    }
    if (g_ir_context && g_ir_context->root) {
        ir_layout_mark_dirty(g_ir_context->root);
    }

    // Debug: Print layout state before and after next frame
    if (getenv("KRYON_DEBUG_TAB_LAYOUT")) {
        fprintf(stderr, "\n[TAB_SWITCH] Selected tab %d\n", index);
        if ((uint32_t)index < state->panel_count && state->panels[index]) {
            IRComponent* panel = state->panels[index];
            fprintf(stderr, "[TAB_SWITCH]   Panel ID=%u rendered_bounds: valid=%d [%.1f, %.1f, %.1f, %.1f]\n",
                   panel->id, panel->rendered_bounds.valid,
                   panel->rendered_bounds.x, panel->rendered_bounds.y,
                   panel->rendered_bounds.width, panel->rendered_bounds.height);
        }
        if (state->tab_content) {
            fprintf(stderr, "[TAB_SWITCH]   TabContent ID=%u rendered_bounds: valid=%d [%.1f, %.1f, %.1f, %.1f]\n",
                   state->tab_content->id, state->tab_content->rendered_bounds.valid,
                   state->tab_content->rendered_bounds.x, state->tab_content->rendered_bounds.y,
                   state->tab_content->rendered_bounds.width, state->tab_content->rendered_bounds.height);
        }
        if (state->group) {
            fprintf(stderr, "[TAB_SWITCH]   TabGroup ID=%u rendered_bounds: valid=%d [%.1f, %.1f, %.1f, %.1f]\n",
                   state->group->id, state->group->rendered_bounds.valid,
                   state->group->rendered_bounds.x, state->group->rendered_bounds.y,
                   state->group->rendered_bounds.width, state->group->rendered_bounds.height);
        }
    }

    // Apply tab visuals (active/inactive colors)
    ir_tabgroup_apply_visuals(state);
}

void ir_tabgroup_handle_tab_click(TabGroupState* state, uint32_t tab_index) {
    if (!state || tab_index >= state->tab_count) return;

    // CRITICAL: Call user's onClick callback FIRST (as per requirement)
    if (state->user_callbacks[tab_index]) {
        state->user_callbacks[tab_index](tab_index, state->user_callback_data[tab_index]);
    }

    // Then select the tab (this applies visuals and switches panels)
    ir_tabgroup_select(state, (int)tab_index);
}

void ir_tabgroup_set_tab_callback(TabGroupState* state, uint32_t index,
                                   TabClickCallback callback, void* user_data) {
    if (!state || index >= state->tab_count) return;
    if (index >= IR_MAX_TABS_PER_GROUP) return;

    // Directly set callback in fixed-size array
    state->user_callbacks[index] = callback;
    state->user_callback_data[index] = user_data;
}

// ============================================================================
// Reordering
// ============================================================================

void ir_tabgroup_reorder(TabGroupState* state, int from_index, int to_index) {
    if (!state) return;
    if (from_index < 0 || to_index < 0) return;
    if ((uint32_t)from_index >= state->tab_count || (uint32_t)to_index >= state->tab_count) return;
    if (from_index == to_index) return;

    // Reorder tabs array
    IRComponent* moved_tab = state->tabs[from_index];
    if (from_index < to_index) {
        memmove(&state->tabs[from_index], &state->tabs[from_index + 1], (size_t)(to_index - from_index) * sizeof(IRComponent*));
    } else {
        memmove(&state->tabs[to_index + 1], &state->tabs[to_index], (size_t)(from_index - to_index) * sizeof(IRComponent*));
    }
    state->tabs[to_index] = moved_tab;

    // Reorder panels to match tabs if counts align
    if (state->panel_count == state->tab_count) {
        IRComponent* moved_panel = state->panels[from_index];
        if (from_index < to_index) {
            memmove(&state->panels[from_index], &state->panels[from_index + 1], (size_t)(to_index - from_index) * sizeof(IRComponent*));
        } else {
            memmove(&state->panels[to_index + 1], &state->panels[to_index], (size_t)(from_index - to_index) * sizeof(IRComponent*));
        }
        state->panels[to_index] = moved_panel;
    }

    // Reorder children inside tab bar for visual order
    if (state->tab_bar && state->tab_bar->children && state->tab_bar->child_count == state->tab_count) {
        IRComponent* moved_child = state->tab_bar->children[from_index];
        if (from_index < to_index) {
            memmove(&state->tab_bar->children[from_index], &state->tab_bar->children[from_index + 1], (size_t)(to_index - from_index) * sizeof(IRComponent*));
        } else {
            memmove(&state->tab_bar->children[to_index + 1], &state->tab_bar->children[to_index], (size_t)(from_index - to_index) * sizeof(IRComponent*));
        }
        state->tab_bar->children[to_index] = moved_child;
    }

    // Maintain selection if needed
    int sel = state->selected_index;
    if (from_index == sel) sel = to_index;
    else if (from_index < sel && to_index >= sel) sel -= 1;
    else if (from_index > sel && to_index <= sel) sel += 1;
    state->selected_index = sel;
    ir_tabgroup_select(state, state->selected_index);
}

void ir_tabgroup_handle_drag(TabGroupState* state, float x, float y, bool is_down, bool is_up) {
    if (!state || !state->tab_bar) return;
    if (!state->reorderable) return;

    if (is_down) {
        // Start drag: find tab under x,y in tab_bar
        if (!state->tab_bar->children) return;
        for (uint32_t i = 0; i < state->tab_bar->child_count; i++) {
            IRComponent* tab = state->tab_bar->children[i];
            if (!tab) continue;
            IRRenderedBounds b = tab->rendered_bounds;
            if (b.valid && x >= b.x && x <= b.x + b.width && y >= b.y && y <= b.y + b.height) {
                state->dragging = true;
                state->drag_index = (int)i;
                state->drag_x = x;
                // Switch to the tab we're dragging so content stays in sync during drag
                ir_tabgroup_select(state, (int)i);
                break;
            }
        }
        return;
    }

    if (is_up) {
        state->dragging = false;
        state->drag_index = -1;
        return;
    }

    if (!state->dragging || state->drag_index < 0) return;

    state->drag_x = x;

    // Track drag movement; if we cross midpoint of adjacent tab, reorder
    if (!state->tab_bar->children || state->tab_bar->child_count == 0) return;
    int current_index = state->drag_index;
    IRComponent* current_tab = state->tab_bar->children[current_index];
    if (!current_tab) return;

    // Check neighbor midpoints
    if (current_index > 0) {
        IRComponent* left = state->tab_bar->children[current_index - 1];
        if (left && left->rendered_bounds.valid) {
            float midpoint = left->rendered_bounds.x + left->rendered_bounds.width * 0.5f;
            if (x < midpoint) {
                ir_tabgroup_reorder(state, current_index, current_index - 1);
                state->drag_index = current_index - 1;
                return;
            }
        }
    }
    if ((uint32_t)current_index + 1 < state->tab_bar->child_count) {
        IRComponent* right = state->tab_bar->children[current_index + 1];
        if (right && right->rendered_bounds.valid) {
            float midpoint = right->rendered_bounds.x + right->rendered_bounds.width * 0.5f;
            if (x > midpoint) {
                ir_tabgroup_reorder(state, current_index, current_index + 1);
                state->drag_index = current_index + 1;
                return;
            }
        }
    }
}

void ir_tabgroup_set_reorderable(TabGroupState* state, bool reorderable) {
    if (!state) return;
    state->reorderable = reorderable;
}

// ============================================================================
// Visual State
// ============================================================================

void ir_tabgroup_set_tab_visual(TabGroupState* state, int index, TabVisualState visual) {
    if (!state) return;
    if (index < 0 || (uint32_t)index >= state->tab_count) return;
    if (index >= IR_MAX_TABS_PER_GROUP) return;

    // Directly set visual in fixed-size array
    state->tab_visuals[index] = visual;
}

static void ir_tabgroup_apply_visual_to_tab(IRComponent* tab, TabVisualState* visual, bool is_active) {
    if (!tab || !visual) return;

    IRStyle* style = ir_get_style(tab);
    if (!style) {
        style = ir_create_style();
        ir_set_style(tab, style);
    }

    // Apply background color
    uint32_t bg_color = is_active ? visual->active_background_color : visual->background_color;
    uint8_t bg_r = (bg_color >> 24) & 0xFF;
    uint8_t bg_g = (bg_color >> 16) & 0xFF;
    uint8_t bg_b = (bg_color >> 8) & 0xFF;
    uint8_t bg_a = bg_color & 0xFF;

    // DEBUG: Print colors being applied
    const char* trace = getenv("KRYON_TRACE_TAB_COLORS");
    if (trace && strcmp(trace, "1") == 0) {
        printf("[tab_colors] Tab #%d %s: bg=#%02x%02x%02x%02x (raw=0x%08x)\n",
               tab->id, is_active ? "ACTIVE" : "inactive",
               bg_r, bg_g, bg_b, bg_a, bg_color);
    }

    ir_set_background_color(style, bg_r, bg_g, bg_b, bg_a);

    // Apply text color
    uint32_t text_color = is_active ? visual->active_text_color : visual->text_color;
    uint8_t text_r = (text_color >> 24) & 0xFF;
    uint8_t text_g = (text_color >> 16) & 0xFF;
    uint8_t text_b = (text_color >> 8) & 0xFF;
    uint8_t text_a = text_color & 0xFF;
    // Keep existing font properties, just update color
    float font_size = style->font.size > 0 ? style->font.size : 16.0f;
    ir_set_font(style, font_size, style->font.family, text_r, text_g, text_b, text_a, style->font.bold, style->font.italic);
}

void ir_tabgroup_apply_visuals(TabGroupState* state) {
    if (!state) return;

    for (uint32_t i = 0; i < state->tab_count; i++) {
        if (state->tabs[i] && i < state->tab_count) {
            bool is_active = ((int)i == state->selected_index);
            ir_tabgroup_apply_visual_to_tab(state->tabs[i], &state->tab_visuals[i], is_active);
        }
    }
}

// ============================================================================
// Query Functions
// ============================================================================

uint32_t ir_tabgroup_get_tab_count(TabGroupState* state) {
    return state ? state->tab_count : 0;
}

uint32_t ir_tabgroup_get_panel_count(TabGroupState* state) {
    return state ? state->panel_count : 0;
}

int ir_tabgroup_get_selected(TabGroupState* state) {
    return state ? state->selected_index : -1;
}

IRComponent* ir_tabgroup_get_tab(TabGroupState* state, uint32_t index) {
    if (!state || index >= state->tab_count) return NULL;
    return state->tabs[index];
}

IRComponent* ir_tabgroup_get_panel(TabGroupState* state, uint32_t index) {
    if (!state || index >= state->panel_count) return NULL;
    return state->panels[index];
}
