#define _GNU_SOURCE
#include "ir_builder.h"
#include "ir_memory.h"
#include "ir_hashmap.h"
#include "ir_animation.h"
#include "ir_plugin.h"
#include "ir_component_handler.h"
#include "components/ir_component_registry.h"
#include "../third_party/cJSON/cJSON.h"
#include "ir_json_helpers.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

// Global IR context
IRContext* g_ir_context = NULL;

// Thread-local IRContext getter/setter (defined in ir_instance.c)
// Returns the current IRContext for this thread, or NULL if using global
extern IRContext* ir_context_get_current(void);

// Get the active IRContext (thread-local first, then global)
static IRContext* get_active_context(void) {
    IRContext* ctx = ir_context_get_current();
    return ctx ? ctx : g_ir_context;
}

// Nim callback for cleanup when components are removed
// This allows the Nim reactive system to clean up when tab panels change
extern void nimOnComponentRemoved(IRComponent* component) __attribute__((weak));

// Nim callback to clean up button/canvas/input handlers when components are removed
extern void nimCleanupHandlersForComponent(IRComponent* component) __attribute__((weak));

// Nim callback when components are added to the tree
// This resets cleanup tracking so panels can be cleaned up again when removed
extern void nimOnComponentAdded(IRComponent* component) __attribute__((weak));

// Forward declarations
void ir_tabgroup_apply_visuals(TabGroupState* state);
void ir_destroy_handler_source(IRHandlerSource* source);

// Helper function to mark component dirty when style changes
static void mark_style_dirty(IRComponent* component) {
    if (!component) return;
    ir_layout_mark_dirty(component);
    component->dirty_flags |= IR_DIRTY_STYLE | IR_DIRTY_LAYOUT;
}

const char* ir_logic_type_to_string(LogicSourceType type) {
    switch (type) {
        case IR_LOGIC_NIM: return "Nim";
        case IR_LOGIC_C: return "C";
        case IR_LOGIC_LUA: return "Lua";
        case IR_LOGIC_WASM: return "WASM";
        case IR_LOGIC_NATIVE: return "Native";
        default: return "Unknown";
    }
}

static int ir_str_ieq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = (char)tolower((unsigned char)*a);
        char cb = (char)tolower((unsigned char)*b);
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

IRColor ir_color_named(const char* name) {
    if (!name) return ir_color_rgba(255, 255, 255, 255);
    // Core named colors (CSS-like)
    struct { const char* name; uint8_t r, g, b; } colors[] = {
        {"white", 255, 255, 255}, {"black", 0, 0, 0},
        {"gray", 128, 128, 128}, {"grey", 128, 128, 128},
        {"lightgray", 211, 211, 211}, {"lightgrey", 211, 211, 211},
        {"darkgray", 169, 169, 169}, {"darkgrey", 169, 169, 169},
        {"silver", 192, 192, 192}, {"red", 255, 0, 0},
        {"green", 0, 255, 0}, {"blue", 0, 0, 255},
        {"yellow", 255, 255, 0}, {"orange", 255, 165, 0},
        {"purple", 128, 0, 128}, {"pink", 255, 192, 203},
        {"brown", 165, 42, 42}, {"cyan", 0, 255, 255},
        {"magenta", 255, 0, 255}, {"lime", 0, 255, 0},
        {"olive", 128, 128, 0}, {"navy", 0, 0, 128},
        {"teal", 0, 128, 128}, {"aqua", 0, 255, 255},
        {"maroon", 128, 0, 0}, {"fuchsia", 255, 0, 255},
        {"lightblue", 173, 216, 230}, {"lightgreen", 144, 238, 144},
        {"lightpink", 255, 182, 193}, {"lightyellow", 255, 255, 224},
        {"lightcyan", 224, 255, 255}, {"darkred", 139, 0, 0},
        {"darkgreen", 0, 100, 0}, {"darkblue", 0, 0, 139},
        {"darkorange", 255, 140, 0}, {"darkviolet", 148, 0, 211},
        {"transparent", 0, 0, 0}, {NULL, 0, 0, 0}
    };
    for (int i = 0; colors[i].name; i++) {
        if (ir_str_ieq(name, colors[i].name)) {
            if (ir_str_ieq(name, "transparent")) {
                return ir_color_rgba(0, 0, 0, 0);
            }
            return ir_color_rgba(colors[i].r, colors[i].g, colors[i].b, 255);
        }
    }
    return ir_color_rgba(255, 255, 255, 255); // default to white
}

static void ir_tabgroup_set_visible(IRComponent* component, bool visible) {
    if (!component) return;
    IRStyle* style = ir_get_style(component);
    if (!style) {
        style = ir_create_style();
        ir_set_style(component, style);
    }
    style->visible = visible;
}

// Recursively invalidate rendered bounds for a component and all its descendants
static void ir_invalidate_bounds_recursive(IRComponent* component) {
    if (!component) return;
    component->rendered_bounds.valid = false;
    for (uint32_t i = 0; i < component->child_count; i++) {
        ir_invalidate_bounds_recursive(component->children[i]);
    }
}

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
        fprintf(stderr, "[ir_builder] Tab group full: max %d tabs\n", IR_MAX_TABS_PER_GROUP);
        return;
    }
    state->tabs[state->tab_count++] = tab;
}

void ir_tabgroup_register_panel(TabGroupState* state, IRComponent* panel) {
    if (!state || !panel) return;
    if (state->panel_count >= IR_MAX_TABS_PER_GROUP) {
        fprintf(stderr, "[ir_builder] Tab group full: max %d panels\n", IR_MAX_TABS_PER_GROUP);
        return;
    }
    state->panels[state->panel_count++] = panel;
}

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

void ir_tabgroup_set_reorderable(TabGroupState* state, bool reorderable) {
    if (!state) return;
    state->reorderable = reorderable;
}

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

// Tab Group Query Functions
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

// Tab User Callback Registration
void ir_tabgroup_set_tab_callback(TabGroupState* state, uint32_t index,
                                   TabClickCallback callback, void* user_data) {
    if (!state || index >= state->tab_count) return;
    if (index >= IR_MAX_TABS_PER_GROUP) return;

    // Directly set callback in fixed-size array
    state->user_callbacks[index] = callback;
    state->user_callback_data[index] = user_data;
}

// Tab Click Handling - combines user callback + selection
void ir_tabgroup_handle_tab_click(TabGroupState* state, uint32_t tab_index) {
    if (!state || tab_index >= state->tab_count) return;

    // CRITICAL: Call user's onClick callback FIRST (as per requirement)
    if (state->user_callbacks[tab_index]) {
        state->user_callbacks[tab_index](tab_index, state->user_callback_data[tab_index]);
    }

    // Then select the tab (this applies visuals and switches panels)
    ir_tabgroup_select(state, (int)tab_index);
}

// Cleanup
void ir_tabgroup_destroy_state(TabGroupState* state) {
    if (!state) return;
    // Arrays are now stack-allocated within the struct, no need to free them
    free(state);
}

// Context Management
IRContext* ir_create_context(void) {
    IRContext* context = malloc(sizeof(IRContext));
    if (!context) return NULL;

    context->root = NULL;
    context->logic_list = NULL;
    context->next_component_id = 1;
    context->next_logic_id = 1;
    context->metadata = NULL;
    context->source_metadata = NULL;
    context->source_structures = NULL;
    context->reactive_manifest = NULL;
    context->stylesheet = NULL;  // CRITICAL: Initialize to NULL to prevent garbage pointer access

    // Create component pool
    context->component_pool = ir_pool_create();
    if (!context->component_pool) {
        free(context);
        return NULL;
    }

    // Create component hash map
    context->component_map = ir_map_create(256);
    if (!context->component_map) {
        ir_pool_destroy(context->component_pool);
        free(context);
        return NULL;
    }

    // Initialize component layout traits (once per context)
    static bool traits_initialized = false;
    if (!traits_initialized) {
        ir_layout_init_traits();
        ir_component_handler_registry_init();
        traits_initialized = true;
    }

    return context;
}

void ir_destroy_context(IRContext* context) {
    if (!context) return;

    if (context->root) {
        ir_destroy_component(context->root);
    }

    // Destroy all logic objects
    IRLogic* logic = context->logic_list;
    while (logic) {
        IRLogic* next = logic->next;
        ir_destroy_logic(logic);
        logic = next;
    }

    // Destroy component hash map
    if (context->component_map) {
        ir_map_destroy(context->component_map);
    }

    // Destroy component pool
    if (context->component_pool) {
        ir_pool_destroy(context->component_pool);
    }

    free(context);
}

void ir_set_context(IRContext* context) {
    g_ir_context = context;
}

IRContext* ir_get_global_context(void) {
    return g_ir_context;
}

IRComponent* ir_get_root(void) {
    return g_ir_context ? g_ir_context->root : NULL;
}

void ir_set_root(IRComponent* root) {
    if (g_ir_context) {
        g_ir_context->root = root;
    }
}

// Component Creation
IRComponent* ir_create_component(IRComponentType type) {
    return ir_create_component_with_id(type, 0);
}

IRComponent* ir_create_component_with_id(IRComponentType type, uint32_t id) {
    IRComponent* component = NULL;

    // Use pool allocator if context exists
    if (g_ir_context && g_ir_context->component_pool) {
        component = ir_pool_alloc_component(g_ir_context->component_pool);
        if (component) {
            // Zero-initialize to clear any stale data from previous use
            memset(component, 0, sizeof(IRComponent));
            component->layout_cache.dirty = true;
            component->dirty_flags = IR_DIRTY_LAYOUT;
        }
    } else {
        // Fallback to malloc if no context/pool
        component = malloc(sizeof(IRComponent));
        if (component) {
            memset(component, 0, sizeof(IRComponent));
            component->layout_cache.dirty = true;
            component->dirty_flags = IR_DIRTY_LAYOUT;
        }
    }

    if (!component) return NULL;

    component->type = type;

    // Get active context (thread-local first, then global)
    IRContext* active_ctx = get_active_context();

    if (id == 0 && active_ctx) {
        component->id = active_ctx->next_component_id++;
    } else {
        component->id = id;
    }

    // Add to hash map for fast lookups
    if (active_ctx && active_ctx->component_map) {
        ir_map_insert(active_ctx->component_map, component);
    }

    // Initialize layout state for two-pass layout system
    component->layout_state = calloc(1, sizeof(IRLayoutState));
    if (component->layout_state) {
        component->layout_state->dirty = true;  // Start dirty, needs initial layout
        // intrinsic_valid = false (default)
        // layout_valid = false (default)
        // cache_generation = 0 (default)
    }

    // Set flex direction based on component type
    if (type == IR_COMPONENT_ROW) {
        IRLayout* layout = ir_get_layout(component);
        if (layout) {
            layout->flex.direction = 1;  // Row = horizontal
        }
    } else if (type == IR_COMPONENT_COLUMN) {
        IRLayout* layout = ir_get_layout(component);
        if (layout) {
            layout->flex.direction = 0;  // Column = vertical
        }
    }

    return component;
}

void ir_destroy_component(IRComponent* component) {
    if (!component) return;

    // Destroy style
    if (component->style) {
        ir_destroy_style(component->style);
    }

    // Destroy events
    IREvent* event = component->events;
    while (event) {
        IREvent* next = event->next;
        ir_destroy_event(event);
        event = next;
    }

    // Destroy logic
    IRLogic* logic = component->logic;
    while (logic) {
        IRLogic* next = logic->next;
        ir_destroy_logic(logic);
        logic = next;
    }

    // Destroy children recursively
    for (uint32_t i = 0; i < component->child_count; i++) {
        ir_destroy_component(component->children[i]);
    }

    // Destroy layout
    if (component->layout) {
        ir_destroy_layout(component->layout);
    }

    // Destroy layout state
    if (component->layout_state) {
        free(component->layout_state);
        component->layout_state = NULL;
    }

    // Clean up TabGroupState if this component is a TabGroup
    // This prevents dangling pointers when UI rebuilds destroy and recreate component trees
    // IMPORTANT: custom_data might be a JSON string (e.g., '{"selectedIndex":0}') from serialization,
    // NOT a TabGroupState pointer. Only treat as TabGroupState if it doesn't start with '{'
    if (component->type == IR_COMPONENT_TAB_GROUP && component->custom_data) {
        char* data_str = (char*)component->custom_data;
        if (data_str[0] == '{') {
            // This is a JSON string, not a TabGroupState - free it as a string
            free(component->custom_data);
            component->custom_data = NULL;
        } else {
            TabGroupState* state = (TabGroupState*)component->custom_data;

            // Clear panel references to prevent dangling pointers
            for (uint32_t i = 0; i < state->panel_count; i++) {
                state->panels[i] = NULL;
            }
            state->panel_count = 0;

            // Clear tab references
            for (uint32_t i = 0; i < state->tab_count; i++) {
                state->tabs[i] = NULL;
            }
            state->tab_count = 0;

            // Clear other references
            state->tab_bar = NULL;
            state->tab_content = NULL;
            state->group = NULL;

            // Free the state itself
            free(state);
            component->custom_data = NULL;
        }
    }

    // Free strings
    if (component->tag) free(component->tag);
    if (component->text_content) free(component->text_content);
    if (component->css_class) free(component->css_class);
    if (component->text_expression) free(component->text_expression);
    if (component->component_ref) free(component->component_ref);
    if (component->component_props) free(component->component_props);
    if (component->module_ref) free(component->module_ref);
    if (component->export_name) free(component->export_name);
    if (component->scope) free(component->scope);
    if (component->source_module) free(component->source_module);
    if (component->each_source) free(component->each_source);
    if (component->each_item_name) free(component->each_item_name);
    if (component->each_index_name) free(component->each_index_name);

    // Free custom_data based on component type
    if (component->custom_data) {
        switch (component->type) {
            case IR_COMPONENT_LINK: {
                IRLinkData* link_data = (IRLinkData*)component->custom_data;
                if (link_data->url) free(link_data->url);
                if (link_data->title) free(link_data->title);
                if (link_data->target) free(link_data->target);
                if (link_data->rel) free(link_data->rel);
                free(link_data);
                break;
            }
            case IR_COMPONENT_CODE_BLOCK: {
                IRCodeBlockData* code_data = (IRCodeBlockData*)component->custom_data;
                if (code_data->language) free(code_data->language);
                if (code_data->code) free(code_data->code);
                free(code_data);
                break;
            }
            default:
                free(component->custom_data);
                break;
        }
        component->custom_data = NULL;
    }
    if (component->visible_condition) free(component->visible_condition);

    // Free tab data
    if (component->tab_data) {
        if (component->tab_data->title) free(component->tab_data->title);
        free(component->tab_data);
    }

    // Free text layout
    if (component->text_layout) {
        ir_text_layout_destroy(component->text_layout);
        component->text_layout = NULL;
    }

    // Free children array
    if (component->children) {
        free(component->children);
    }

    // Remove from hash map
    if (g_ir_context && g_ir_context->component_map) {
        ir_map_remove(g_ir_context->component_map, component->id);
    }

    // Return to pool or free
    if (g_ir_context && g_ir_context->component_pool) {
        ir_pool_free_component(g_ir_context->component_pool, component);
    } else {
        free(component);
    }
}

// Tree Structure Management
void ir_add_child(IRComponent* parent, IRComponent* child) {
    if (!parent || !child) return;

    // Exponential growth strategy (like std::vector) - 90% fewer reallocs
    if (parent->child_count >= parent->child_capacity) {
        uint32_t new_capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        IRComponent** new_children = realloc(parent->children,
                                            sizeof(IRComponent*) * new_capacity);
        if (!new_children) return;  // Failed to allocate

        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }

    parent->children[parent->child_count] = child;
    parent->child_count++;
    child->parent = parent;

    // Mark parent dirty - children changed
    ir_layout_mark_dirty(parent);
    parent->dirty_flags |= IR_DIRTY_CHILDREN | IR_DIRTY_LAYOUT;
}

void ir_remove_child(IRComponent* parent, IRComponent* child) {
    if (!parent || !child || parent->child_count == 0) return;

    // Find child
    uint32_t index = 0;
    bool found = false;
    for (uint32_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            index = i;
            found = true;
            break;
        }
    }

    if (!found) return;

    // Shift remaining children
    for (uint32_t i = index; i < parent->child_count - 1; i++) {
        parent->children[i] = parent->children[i + 1];
    }

    parent->child_count--;
    child->parent = NULL;

    // Mark parent dirty - children changed
    ir_layout_mark_dirty(parent);
    parent->dirty_flags |= IR_DIRTY_CHILDREN | IR_DIRTY_LAYOUT;
}

void ir_insert_child(IRComponent* parent, IRComponent* child, uint32_t index) {
    if (!parent || !child || index > parent->child_count) return;

    // OPTIMIZATION: Exponential growth strategy (90% fewer reallocs, same as ir_add_child)
    if (parent->child_count >= parent->child_capacity) {
        uint32_t new_capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        IRComponent** new_children = realloc(parent->children,
                                            sizeof(IRComponent*) * new_capacity);
        if (!new_children) return;  // Failed to allocate

        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }

    // Shift children to make space
    for (uint32_t i = parent->child_count; i > index; i--) {
        parent->children[i] = parent->children[i - 1];
    }

    parent->children[index] = child;
    parent->child_count++;
    child->parent = parent;
}

IRComponent* ir_get_child(IRComponent* component, uint32_t index) {
    if (!component || index >= component->child_count) return NULL;
    return component->children[index];
}

IRComponent* ir_find_component_by_id(IRComponent* root, uint32_t id) {
    // Try hash map first for O(1) lookup
    if (g_ir_context && g_ir_context->component_map) {
        IRComponent* result = ir_map_lookup(g_ir_context->component_map, id);
        if (result) {
            printf("[find] Hash map hit for #%u\n", id);
            return result;
        }
        printf("[find] Hash map miss for #%u, falling back to tree traversal\n", id);
    }

    // Tree traversal fallback
    if (!root) return NULL;

    if (root->id == id) return root;

    for (uint32_t i = 0; i < root->child_count; i++) {
        IRComponent* found = ir_find_component_by_id(root->children[i], id);
        if (found) return found;
    }

    return NULL;
}

// Style Management
IRStyle* ir_create_style(void) {
    IRStyle* style = malloc(sizeof(IRStyle));
    if (!style) return NULL;

    memset(style, 0, sizeof(IRStyle));

    // Set sensible defaults
    style->visible = true;
    style->opacity = 1.0f;
    style->position_mode = IR_POSITION_RELATIVE;
    style->absolute_x = 0.0f;
    style->absolute_y = 0.0f;

    // Initialize transforms to identity (no transformation)
    style->transform.scale_x = 1.0f;
    style->transform.scale_y = 1.0f;

    // Set default line height (1.5 is CSS standard)
    style->font.line_height = 1.5f;
    style->transform.translate_x = 0.0f;
    style->transform.translate_y = 0.0f;
    style->transform.rotate = 0.0f;

    // Default dimensions to AUTO so components can be stretched by align-items: stretch
    style->width.type = IR_DIMENSION_AUTO;
    style->width.value = 0.0f;
    style->height.type = IR_DIMENSION_AUTO;
    style->height.value = 0.0f;

    // Grid item defaults (-1 means auto placement)
    style->grid_item.row_start = -1;
    style->grid_item.row_end = -1;
    style->grid_item.column_start = -1;
    style->grid_item.column_end = -1;

    // Text effect defaults
    style->text_effect.max_width.type = IR_DIMENSION_PX;
    style->text_effect.max_width.value = 0.0f;  // 0 means no wrapping
    style->text_effect.text_direction = IR_TEXT_DIR_AUTO;  // Auto-detect direction
    style->text_effect.language = NULL;  // No language specified
    style->grid_item.justify_self = IR_ALIGNMENT_START;
    style->grid_item.align_self = IR_ALIGNMENT_START;

    return style;
}

void ir_destroy_style(IRStyle* style) {
    if (!style) return;

    // Free animations
    if (style->animations) {
        for (uint32_t i = 0; i < style->animation_count; i++) {
            ir_animation_destroy(style->animations[i]);
        }
        free(style->animations);
    }

    // Free transitions
    if (style->transitions) {
        for (uint32_t i = 0; i < style->transition_count; i++) {
            ir_transition_destroy(style->transitions[i]);
        }
        free(style->transitions);
    }

    // Free gradient objects
    if (style->background.type == IR_COLOR_GRADIENT && style->background.data.gradient) {
        ir_gradient_destroy(style->background.data.gradient);
    }
    if (style->border_color.type == IR_COLOR_GRADIENT && style->border_color.data.gradient) {
        ir_gradient_destroy(style->border_color.data.gradient);
    }

    if (style->font.family) free(style->font.family);

    free(style);
}

void ir_set_style(IRComponent* component, IRStyle* style) {
    if (!component) return;

    if (component->style) {
        ir_destroy_style(component->style);
    }
    component->style = style;
}

IRStyle* ir_get_style(IRComponent* component) {
    if (!component) return NULL;

    if (!component->style) {
        component->style = ir_create_style();
    }

    return component->style;
}

void ir_set_visible(IRStyle* style, bool visible) {
    if (!style) return;
    style->visible = visible;
}

void ir_set_z_index(IRStyle* style, uint32_t z_index) {
    if (!style) return;
    style->z_index = z_index;
}

// Text Effect Helpers
void ir_set_text_overflow(IRStyle* style, IRTextOverflowType overflow) {
    if (!style) return;
    style->text_effect.overflow = overflow;
}

void ir_set_text_fade(IRStyle* style, IRTextFadeType fade_type, float fade_length) {
    if (!style) return;
    style->text_effect.fade_type = fade_type;
    style->text_effect.fade_length = fade_length;
}

void ir_set_text_shadow(IRStyle* style, float offset_x, float offset_y, float blur_radius,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!style) return;
    style->text_effect.shadow.offset_x = offset_x;
    style->text_effect.shadow.offset_y = offset_y;
    style->text_effect.shadow.blur_radius = blur_radius;
    style->text_effect.shadow.color.type = IR_COLOR_SOLID;
    style->text_effect.shadow.color.data.r = r;
    style->text_effect.shadow.color.data.g = g;
    style->text_effect.shadow.color.data.b = b;
    style->text_effect.shadow.color.data.a = a;
    style->text_effect.shadow.enabled = true;
}

void ir_set_opacity(IRStyle* style, float opacity) {
    if (!style) return;
    style->opacity = opacity;
}

void ir_set_disabled(IRComponent* component, bool disabled) {
    if (!component) return;

    component->is_disabled = disabled;

    // Apply visual styling for disabled state
    if (disabled) {
        // Ensure style exists
        if (!component->style) {
            component->style = ir_create_style();
        }

        // Set 50% opacity for disabled appearance
        ir_set_opacity(component->style, 0.5f);
    }

    // Mark component as dirty to trigger re-render
    mark_style_dirty(component);
}

// Text Layout
void ir_set_text_max_width(IRStyle* style, IRDimensionType type, float value) {
    if (!style) return;
    style->text_effect.max_width.type = type;
    style->text_effect.max_width.value = value;
}

// Box Shadow and Filters
void ir_set_box_shadow(IRStyle* style, float offset_x, float offset_y, float blur_radius,
                       float spread_radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool inset) {
    if (!style) return;
    style->box_shadow.offset_x = offset_x;
    style->box_shadow.offset_y = offset_y;
    style->box_shadow.blur_radius = blur_radius;
    style->box_shadow.spread_radius = spread_radius;
    style->box_shadow.color.type = IR_COLOR_SOLID;
    style->box_shadow.color.data.r = r;
    style->box_shadow.color.data.g = g;
    style->box_shadow.color.data.b = b;
    style->box_shadow.color.data.a = a;
    style->box_shadow.inset = inset;
    style->box_shadow.enabled = true;
}

void ir_add_filter(IRStyle* style, IRFilterType type, float value) {
    if (!style || style->filter_count >= IR_MAX_FILTERS) return;
    style->filters[style->filter_count].type = type;
    style->filters[style->filter_count].value = value;
    style->filter_count++;
}

void ir_clear_filters(IRStyle* style) {
    if (!style) return;
    style->filter_count = 0;
}

// Style Property Helpers
void ir_set_width(IRComponent* component, IRDimensionType type, float value) {
    if (!component || !component->style) return;
    component->style->width.type = type;
    component->style->width.value = value;
    ir_layout_mark_dirty(component);
}

void ir_set_height(IRComponent* component, IRDimensionType type, float value) {
    if (!component || !component->style) return;
    component->style->height.type = type;
    component->style->height.value = value;
    ir_layout_mark_dirty(component);
}

void ir_set_background_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!style) return;
    style->background.type = IR_COLOR_SOLID;
    style->background.data.r = r;
    style->background.data.g = g;
    style->background.data.b = b;
    style->background.data.a = a;
}

void ir_set_border(IRStyle* style, float width, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t radius) {
    if (!style) return;
    style->border.width = width;
    style->border.color.type = IR_COLOR_SOLID;
    style->border.color.data.r = r;
    style->border.color.data.g = g;
    style->border.color.data.b = b;
    style->border.color.data.a = a;
    style->border.radius = radius;
}

// Individual border property setters (preserve other properties)
void ir_set_border_width(IRStyle* style, float width) {
    if (!style) return;
    style->border.width = width;
}

void ir_set_border_radius(IRStyle* style, uint8_t radius) {
    if (!style) return;
    style->border.radius = radius;
}

void ir_set_border_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!style) return;
    style->border.color.type = IR_COLOR_SOLID;
    style->border.color.data.r = r;
    style->border.color.data.g = g;
    style->border.color.data.b = b;
    style->border.color.data.a = a;
}

void ir_set_margin(IRComponent* component, float top, float right, float bottom, float left) {
    if (!component || !component->style) return;
    component->style->margin.top = top;
    component->style->margin.right = right;
    component->style->margin.bottom = bottom;
    component->style->margin.left = left;
    ir_layout_mark_dirty(component);
}

void ir_set_padding(IRComponent* component, float top, float right, float bottom, float left) {
    if (!component || !component->style) return;
    component->style->padding.top = top;
    component->style->padding.right = right;
    component->style->padding.bottom = bottom;
    component->style->padding.left = left;
    ir_layout_mark_dirty(component);
}

void ir_set_font(IRStyle* style, float size, const char* family, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool bold, bool italic) {
    if (!style) return;

    // Free old font family before replacing
    if (style->font.family) {
        free((void*)style->font.family);
        style->font.family = NULL;
    }

    style->font.size = size;
    style->font.family = family ? strdup(family) : NULL;
    style->font.color.type = IR_COLOR_SOLID;
    style->font.color.data.r = r;
    style->font.color.data.g = g;
    style->font.color.data.b = b;
    style->font.color.data.a = a;
    style->font.bold = bold;
    style->font.italic = italic;
}

// Individual font property setters (preserve other properties)
void ir_set_font_size(IRStyle* style, float size) {
    if (!style) return;
    style->font.size = size;
}

void ir_set_font_family(IRStyle* style, const char* family) {
    if (!style) return;

    // Free old font family before replacing
    if (style->font.family) {
        free((void*)style->font.family);
        style->font.family = NULL;
    }

    style->font.family = family ? strdup(family) : NULL;
}

void ir_set_font_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!style) return;
    style->font.color.type = IR_COLOR_SOLID;
    style->font.color.data.r = r;
    style->font.color.data.g = g;
    style->font.color.data.b = b;
    style->font.color.data.a = a;
}

void ir_set_font_style(IRStyle* style, bool bold, bool italic) {
    if (!style) return;
    style->font.bold = bold;
    style->font.italic = italic;
}

// Extended Typography (Phase 3)
void ir_set_font_weight(IRStyle* style, uint16_t weight) {
    if (!style) return;
    style->font.weight = weight;
}

void ir_set_line_height(IRStyle* style, float line_height) {
    if (!style) return;
    style->font.line_height = line_height;
}

void ir_set_letter_spacing(IRStyle* style, float spacing) {
    if (!style) return;
    style->font.letter_spacing = spacing;
}

void ir_set_word_spacing(IRStyle* style, float spacing) {
    if (!style) return;
    style->font.word_spacing = spacing;
}

void ir_set_text_align(IRStyle* style, IRTextAlign align) {
    if (!style) return;
    style->font.align = align;
}

void ir_set_text_decoration(IRStyle* style, uint8_t decoration) {
    if (!style) return;
    style->font.decoration = decoration;
}

// Style Variable Reference Setters (for theme support)
void ir_set_background_color_var(IRStyle* style, IRStyleVarId var_id) {
    if (!style) return;
    style->background.type = IR_COLOR_VAR_REF;
    style->background.data.var_id = var_id;
}

void ir_set_text_color_var(IRStyle* style, IRStyleVarId var_id) {
    if (!style) return;
    style->font.color.type = IR_COLOR_VAR_REF;
    style->font.color.data.var_id = var_id;
}

void ir_set_border_color_var(IRStyle* style, IRStyleVarId var_id) {
    if (!style) return;
    style->border.color.type = IR_COLOR_VAR_REF;
    style->border.color.data.var_id = var_id;
}

// Layout Management
IRLayout* ir_create_layout(void) {
    IRLayout* layout = malloc(sizeof(IRLayout));
    if (!layout) return NULL;

    memset(layout, 0, sizeof(IRLayout));

    // Set sensible defaults
    layout->flex.justify_content = IR_ALIGNMENT_START;
    layout->flex.cross_axis = IR_ALIGNMENT_START;
    layout->flex.shrink = 1;  // CSS default is 1, not 0

    return layout;
}

void ir_destroy_layout(IRLayout* layout) {
    free(layout);
}

void ir_set_layout(IRComponent* component, IRLayout* layout) {
    if (!component) return;

    if (component->layout) {
        ir_destroy_layout(component->layout);
    }
    component->layout = layout;
}

IRLayout* ir_get_layout(IRComponent* component) {
    if (!component) return NULL;

    if (!component->layout) {
        component->layout = ir_create_layout();
    }

    return component->layout;
}

void ir_set_flexbox(IRLayout* layout, bool wrap, uint32_t gap, IRAlignment main_axis, IRAlignment cross_axis) {
    if (!layout) return;
    layout->flex.wrap = wrap;
    layout->flex.gap = gap;
    layout->flex.justify_content = main_axis;
    layout->flex.cross_axis = cross_axis;
}

void ir_set_flex_properties(IRLayout* layout, uint8_t grow, uint8_t shrink, uint8_t direction) {
    if (!layout) return;
    layout->flex.grow = grow;
    layout->flex.shrink = shrink;
    layout->flex.direction = direction;
}

void ir_set_min_width(IRLayout* layout, IRDimensionType type, float value) {
    if (!layout) return;
    layout->min_width.type = type;
    layout->min_width.value = value;
}

void ir_set_min_height(IRLayout* layout, IRDimensionType type, float value) {
    if (!layout) return;
    layout->min_height.type = type;
    layout->min_height.value = value;
}

void ir_set_max_width(IRLayout* layout, IRDimensionType type, float value) {
    if (!layout) return;
    layout->max_width.type = type;
    layout->max_width.value = value;
}

void ir_set_max_height(IRLayout* layout, IRDimensionType type, float value) {
    if (!layout) return;
    layout->max_height.type = type;
    layout->max_height.value = value;
}

void ir_set_aspect_ratio(IRLayout* layout, float ratio) {
    if (!layout) return;
    layout->aspect_ratio = ratio;
}

// Event Management
IREvent* ir_create_event(IREventType type, const char* logic_id, const char* handler_data) {
    IREvent* event = malloc(sizeof(IREvent));
    if (!event) return NULL;

    memset(event, 0, sizeof(IREvent));

    event->type = type;
    event->logic_id = logic_id ? strdup(logic_id) : NULL;
    event->handler_data = handler_data ? strdup(handler_data) : NULL;

    // NEW: For plugin events, store the string name
    if (type >= IR_EVENT_PLUGIN_START && type <= IR_EVENT_PLUGIN_END) {
        const char* name = ir_plugin_get_event_type_name(type);
        event->event_name = name ? strdup(name) : NULL;
    } else {
        event->event_name = NULL;  // Core events don't need names
    }

    return event;
}

void ir_destroy_event(IREvent* event) {
    if (!event) return;

    if (event->event_name) free(event->event_name);
    if (event->logic_id) free(event->logic_id);
    if (event->handler_data) free(event->handler_data);
    if (event->handler_source) ir_destroy_handler_source(event->handler_source);

    free(event);
}

void ir_add_event(IRComponent* component, IREvent* event) {
    if (!component || !event) return;

    event->next = component->events;
    component->events = event;
}

void ir_remove_event(IRComponent* component, IREvent* event) {
    if (!component || !event || !component->events) return;

    if (component->events == event) {
        component->events = event->next;
        return;
    }

    IREvent* current = component->events;
    while (current->next) {
        if (current->next == event) {
            current->next = event->next;
            return;
        }
        current = current->next;
    }
}

IREvent* ir_find_event(IRComponent* component, IREventType type) {
    if (!component || !component->events) return NULL;

    IREvent* event = component->events;
    while (event) {
        if (event->type == type) return event;
        event = event->next;
    }

    return NULL;
}

// Event Bytecode Support (IR v2.1)
void ir_event_set_bytecode_function_id(IREvent* event, uint32_t function_id) {
    if (!event) return;
    event->bytecode_function_id = function_id;
}

uint32_t ir_event_get_bytecode_function_id(IREvent* event) {
    if (!event) return 0;
    return event->bytecode_function_id;
}

// Handler Source Management (for Lua source preservation in KIR)
IRHandlerSource* ir_create_handler_source(const char* language, const char* code, const char* file, int line) {
    IRHandlerSource* source = calloc(1, sizeof(IRHandlerSource));
    if (!source) return NULL;

    source->language = language ? strdup(language) : NULL;
    source->code = code ? strdup(code) : NULL;
    source->file = file ? strdup(file) : NULL;
    source->line = line;

    return source;
}

void ir_destroy_handler_source(IRHandlerSource* source) {
    if (!source) return;
    if (source->language) free(source->language);
    if (source->code) free(source->code);
    if (source->file) free(source->file);

    // Free closure metadata
    if (source->closure_vars) {
        for (int i = 0; i < source->closure_var_count; i++) {
            free(source->closure_vars[i]);
        }
        free(source->closure_vars);
    }

    free(source);
}

void ir_event_set_handler_source(IREvent* event, IRHandlerSource* source) {
    if (!event) return;
    // Free existing handler source if any
    if (event->handler_source) {
        ir_destroy_handler_source(event->handler_source);
    }
    event->handler_source = source;
}

int ir_handler_source_set_closures(IRHandlerSource* source, const char** vars, int count) {
    if (!source) return -1;
    if (!vars || count <= 0) {
        source->uses_closures = false;
        source->closure_vars = NULL;
        source->closure_var_count = 0;
        return 0;
    }

    source->uses_closures = true;
    source->closure_var_count = count;
    source->closure_vars = calloc(count, sizeof(char*));

    if (!source->closure_vars) {
        source->closure_var_count = 0;
        return -1;
    }

    for (int i = 0; i < count; i++) {
        if (vars[i]) {
            source->closure_vars[i] = strdup(vars[i]);
        }
    }

    return 0;
}

// Logic Management
IRLogic* ir_create_logic(const char* id, LogicSourceType type, const char* source_code) {
    IRLogic* logic = malloc(sizeof(IRLogic));
    if (!logic) return NULL;

    memset(logic, 0, sizeof(IRLogic));

    logic->id = id ? strdup(id) : NULL;
    logic->type = type;
    logic->source_code = source_code ? strdup(source_code) : NULL;

    return logic;
}

void ir_destroy_logic(IRLogic* logic) {
    if (!logic) return;

    if (logic->id) free(logic->id);
    if (logic->source_code) free(logic->source_code);

    free(logic);
}

void ir_add_logic(IRComponent* component, IRLogic* logic) {
    if (!component || !logic) return;

    logic->next = component->logic;
    component->logic = logic;
}

void ir_remove_logic(IRComponent* component, IRLogic* logic) {
    if (!component || !logic || !component->logic) return;

    if (component->logic == logic) {
        component->logic = logic->next;
        return;
    }

    IRLogic* current = component->logic;
    while (current->next) {
        if (current->next == logic) {
            current->next = logic->next;
            return;
        }
        current = current->next;
    }
}

IRLogic* ir_find_logic_by_id(IRComponent* root, const char* id) {
    if (!root || !id) return NULL;

    // Check component's logic
    IRLogic* logic = root->logic;
    while (logic) {
        if (logic->id && strcmp(logic->id, id) == 0) return logic;
        logic = logic->next;
    }

    // Search children
    for (uint32_t i = 0; i < root->child_count; i++) {
        IRLogic* found = ir_find_logic_by_id(root->children[i], id);
        if (found) return found;
    }

    return NULL;
}

// Content Management
void ir_set_text_content(IRComponent* component, const char* text) {
    if (!component) return;

    if (component->text_content) {
        free(component->text_content);
    }
    component->text_content = text ? strdup(text) : NULL;

    // Invalidate text layout when content changes
    if (component->text_layout) {
        ir_text_layout_destroy(component->text_layout);
        component->text_layout = NULL;
    }

    // Mark component dirty - content changed (two-pass layout system)
    ir_layout_mark_dirty(component);

    // Also invalidate old layout cache for backward compatibility
    ir_layout_mark_dirty(component);
    component->dirty_flags |= IR_DIRTY_CONTENT | IR_DIRTY_LAYOUT;
}

void ir_set_custom_data(IRComponent* component, const char* data) {
    if (!component) return;

    if (component->custom_data) {
        free(component->custom_data);
    }
    component->custom_data = data ? strdup(data) : NULL;
}

void ir_set_each_source(IRComponent* component, const char* source) {
    if (!component) return;

    if (component->each_source) {
        free(component->each_source);
    }
    component->each_source = source ? strdup(source) : NULL;
}

// Module Reference Management (for cross-file component references)
void ir_set_component_module_ref(IRComponent* component, const char* module_ref, const char* export_name) {
    if (!component) return;

    if (component->module_ref) {
        free(component->module_ref);
    }
    component->module_ref = module_ref ? strdup(module_ref) : NULL;

    if (component->export_name) {
        free(component->export_name);
    }
    component->export_name = export_name ? strdup(export_name) : NULL;
}

// Recursively clear module_ref for a component tree (for serialization)
// Returns a cJSON array with the old values for each component that had module_ref set
cJSON* ir_clear_tree_module_refs(IRComponent* component) {
    if (!component) return NULL;

    cJSON* refs_array = cJSON_CreateArray();

    // Process this component first
    if (component->module_ref) {
        cJSON* ref = cJSON_CreateObject();
        cJSON_AddNumberToObject(ref, "id", component->id);
        cJSON_AddStringOrNull(ref, "module_ref", component->module_ref);
        if (component->export_name) {
            cJSON_AddStringOrNull(ref, "export_name", component->export_name);
        }
        cJSON_AddItemToArray(refs_array, ref);

        free(component->module_ref);
        component->module_ref = NULL;
    }
    if (component->export_name) {
        free(component->export_name);
        component->export_name = NULL;
    }

    // Recursively process children
    for (uint32_t i = 0; i < component->child_count; i++) {
        cJSON* child_refs = ir_clear_tree_module_refs(component->children[i]);
        if (child_refs) {
            // Merge child refs into our array
            for (int j = 0; j < cJSON_GetArraySize(child_refs); j++) {
                cJSON* item = cJSON_DetachItemFromArray(child_refs, j);
                cJSON_AddItemToArray(refs_array, item);
            }
            cJSON_Delete(child_refs);
        }
    }

    return refs_array;
}

// Restore module_ref from the refs array returned by ir_clear_tree_module_refs
void ir_restore_tree_module_refs(IRComponent* component, cJSON* refs_array) {
    if (!component || !refs_array) return;

    // Create a map by id for quick lookup
    cJSON* id_map = cJSON_CreateObject();

    for (int i = 0; i < cJSON_GetArraySize(refs_array); i++) {
        cJSON* ref = cJSON_GetArrayItem(refs_array, i);
        cJSON* id_obj = cJSON_GetObjectItem(ref, "id");
        if (id_obj && cJSON_IsNumber(id_obj)) {
            cJSON_AddItemToObject(id_map, ref->string, ref);
        }
    }

    // Helper function to restore refs for a component tree
    void restore_for_component(IRComponent* comp) {
        if (!comp) return;

        // Find this component's refs by id
        char id_str[32];
        snprintf(id_str, sizeof(id_str), "%u", comp->id);

        cJSON* ref = cJSON_GetObjectItem(id_map, id_str);
        if (ref) {
            cJSON* mod_ref = cJSON_GetObjectItem(ref, "module_ref");
            cJSON* exp_name = cJSON_GetObjectItem(ref, "export_name");

            if (mod_ref && cJSON_IsString(mod_ref)) {
                comp->module_ref = strdup(mod_ref->valuestring);
            }
            if (exp_name && cJSON_IsString(exp_name)) {
                comp->export_name = strdup(exp_name->valuestring);
            }
        }

        // Recursively restore children
        for (uint32_t i = 0; i < comp->child_count; i++) {
            restore_for_component(comp->children[i]);
        }
    }

    restore_for_component(component);
    cJSON_Delete(id_map);
}

// JSON string wrapper for ir_clear_tree_module_refs (for FFI compatibility)
// Returns a JSON string that must be freed by the caller
char* ir_clear_tree_module_refs_json(IRComponent* component) {
    if (!component) return NULL;

    cJSON* refs_array = ir_clear_tree_module_refs(component);
    if (!refs_array) return NULL;

    char* result = cJSON_PrintUnformatted(refs_array);
    cJSON_Delete(refs_array);
    return result;
}

// JSON string wrapper for ir_restore_tree_module_refs (for FFI compatibility)
void ir_restore_tree_module_refs_json(IRComponent* component, const char* json_str) {
    if (!component || !json_str) return;

    cJSON* refs_array = cJSON_Parse(json_str);
    if (refs_array) {
        ir_restore_tree_module_refs(component, refs_array);
        cJSON_Delete(refs_array);
    }
}

// Temporarily clear module_ref for serialization (returns old values for restoration)
// DEPRECATED: Use ir_clear_tree_module_refs instead
char* ir_clear_component_module_ref(IRComponent* component) {
    if (!component) return NULL;
    if (!component->module_ref) return NULL;

    // Create a JSON string with the old values
    cJSON* old_vals = cJSON_CreateObject();
    if (component->module_ref) {
        cJSON_AddStringToObject(old_vals, "module_ref", component->module_ref);
        free(component->module_ref);
        component->module_ref = NULL;
    }
    if (component->export_name) {
        cJSON_AddStringToObject(old_vals, "export_name", component->export_name);
        free(component->export_name);
        component->export_name = NULL;
    }

    char* result = cJSON_PrintUnformatted(old_vals);
    cJSON_Delete(old_vals);
    return result;
}

// Restore module_ref from the JSON string returned by ir_clear_component_module_ref
// DEPRECATED: Use ir_restore_tree_module_refs instead
void ir_restore_component_module_ref(IRComponent* component, const char* json_str) {
    if (!component || !json_str) return;

    cJSON* old_vals = cJSON_Parse(json_str);
    if (old_vals) {
        cJSON* mod_ref = cJSON_GetObjectItem(old_vals, "module_ref");
        cJSON* exp_name = cJSON_GetObjectItem(old_vals, "export_name");

        if (mod_ref && cJSON_IsString(mod_ref)) {
            component->module_ref = strdup(mod_ref->valuestring);
        }
        if (exp_name && cJSON_IsString(exp_name)) {
            component->export_name = strdup(exp_name->valuestring);
        }

        cJSON_Delete(old_vals);
    }
}

// Checkbox state helpers
bool ir_get_checkbox_state(IRComponent* component) {
    if (!component || !component->custom_data) return false;
    return strcmp(component->custom_data, "checked") == 0;
}

void ir_set_checkbox_state(IRComponent* component, bool checked) {
    if (!component) return;
    ir_set_custom_data(component, checked ? "checked" : "unchecked");
}

void ir_toggle_checkbox_state(IRComponent* component) {
    if (!component) return;
    bool current = ir_get_checkbox_state(component);
    printf("[CHECKBOX_TOGGLE] Component ID %u: %s -> %s\n",
           component->id,
           current ? "checked" : "unchecked",
           !current ? "checked" : "unchecked");
    ir_set_checkbox_state(component, !current);
}

// Dropdown state helpers
IRDropdownState* ir_get_dropdown_state(IRComponent* component) {
    if (!component || component->type != IR_COMPONENT_DROPDOWN || !component->custom_data) {
        return NULL;
    }
    return (IRDropdownState*)component->custom_data;
}

int32_t ir_get_dropdown_selected_index(IRComponent* component) {
    IRDropdownState* state = ir_get_dropdown_state(component);
    return state ? state->selected_index : -1;
}

void ir_set_dropdown_selected_index(IRComponent* component, int32_t index) {
    IRDropdownState* state = ir_get_dropdown_state(component);
    if (!state) return;

    // Validate index
    if (index >= -1 && (uint32_t)index < state->option_count) {
        state->selected_index = index;
    }
}

void ir_set_dropdown_options(IRComponent* component, char** options, uint32_t count) {
    IRDropdownState* state = ir_get_dropdown_state(component);
    if (!state) return;

    // Free existing options
    if (state->options) {
        for (uint32_t i = 0; i < state->option_count; i++) {
            free(state->options[i]);
        }
        free(state->options);
    }

    // Copy new options
    state->option_count = count;
    if (count > 0 && options) {
        state->options = (char**)malloc(sizeof(char*) * count);
        for (uint32_t i = 0; i < count; i++) {
            state->options[i] = options[i] ? strdup(options[i]) : NULL;
        }
    } else {
        state->options = NULL;
    }

    // Reset selection if out of range
    if (state->selected_index >= (int32_t)count) {
        state->selected_index = -1;
    }
}

bool ir_get_dropdown_open_state(IRComponent* component) {
    IRDropdownState* state = ir_get_dropdown_state(component);
    return state ? state->is_open : false;
}

void ir_set_dropdown_open_state(IRComponent* component, bool is_open) {
    IRDropdownState* state = ir_get_dropdown_state(component);
    if (state) {
        state->is_open = is_open;
        if (!is_open) {
            state->hovered_index = -1;  // Reset hover when closing
        }
    }
}

void ir_toggle_dropdown_open_state(IRComponent* component) {
    IRDropdownState* state = ir_get_dropdown_state(component);
    if (state) {
        state->is_open = !state->is_open;
        if (!state->is_open) {
            state->hovered_index = -1;  // Reset hover when closing
        }
    }
}

int32_t ir_get_dropdown_hovered_index(IRComponent* component) {
    IRDropdownState* state = ir_get_dropdown_state(component);
    return state ? state->hovered_index : -1;
}

void ir_set_dropdown_hovered_index(IRComponent* component, int32_t index) {
    IRDropdownState* state = ir_get_dropdown_state(component);
    if (!state) return;

    // Validate index (-1 is valid for no hover)
    if (index >= -1 && (uint32_t)index < state->option_count) {
        state->hovered_index = index;
    }
}

void ir_set_tag(IRComponent* component, const char* tag) {
    if (!component) return;

    if (component->tag) {
        free(component->tag);
    }
    component->tag = tag ? strdup(tag) : NULL;
}

// Convenience Functions
IRComponent* ir_container(const char* tag) {
    IRComponent* component = ir_create_component(IR_COMPONENT_CONTAINER);
    // Container is a generic block-level element like HTML <div>
    // It stacks children vertically (block flow) without flex layout
    IRLayout* layout = ir_get_layout(component);
    layout->flex.direction = 0xFF;  // Not a flex container - uses block layout
    layout->flex.justify_content = IR_ALIGNMENT_START;
    layout->flex.cross_axis = IR_ALIGNMENT_START;
    if (tag) ir_set_tag(component, tag);
    return component;
}

IRComponent* ir_text(const char* content) {
    IRComponent* component = ir_create_component(IR_COMPONENT_TEXT);
    if (content) ir_set_text_content(component, content);
    return component;
}

IRComponent* ir_button(const char* text) {
    IRComponent* component = ir_create_component(IR_COMPONENT_BUTTON);
    if (text) ir_set_text_content(component, text);
    return component;
}

IRComponent* ir_input(const char* placeholder) {
    IRComponent* component = ir_create_component(IR_COMPONENT_INPUT);
    if (placeholder) ir_set_custom_data(component, placeholder);
    return component;
}

IRComponent* ir_checkbox(const char* label) {
    IRComponent* component = ir_create_component(IR_COMPONENT_CHECKBOX);
    if (label) ir_set_text_content(component, label);
    return component;
}

IRComponent* ir_dropdown(const char* placeholder, char** options, uint32_t option_count) {
    IRComponent* component = ir_create_component(IR_COMPONENT_DROPDOWN);

    // Create and initialize dropdown state
    IRDropdownState* state = (IRDropdownState*)malloc(sizeof(IRDropdownState));
    if (!state) return component;

    state->placeholder = placeholder ? strdup(placeholder) : NULL;
    state->option_count = option_count;
    state->selected_index = -1;  // No selection initially
    state->is_open = false;
    state->hovered_index = -1;

    // Copy options array
    if (option_count > 0 && options) {
        state->options = (char**)malloc(sizeof(char*) * option_count);
        for (uint32_t i = 0; i < option_count; i++) {
            state->options[i] = options[i] ? strdup(options[i]) : NULL;
        }
    } else {
        state->options = NULL;
    }

    // Store state pointer in custom_data
    component->custom_data = (char*)state;

    return component;
}

IRComponent* ir_row(void) {
    IRComponent* component = ir_create_component(IR_COMPONENT_ROW);
    IRLayout* layout = ir_get_layout(component);
    layout->flex.direction = 1;  // Row = horizontal (1)
    layout->flex.justify_content = IR_ALIGNMENT_START;
    layout->flex.cross_axis = IR_ALIGNMENT_CENTER;
    return component;
}

IRComponent* ir_column(void) {
    IRComponent* component = ir_create_component(IR_COMPONENT_COLUMN);
    IRLayout* layout = ir_get_layout(component);
    layout->flex.direction = 0;  // Column = vertical (0)
    layout->flex.justify_content = IR_ALIGNMENT_CENTER;
    layout->flex.cross_axis = IR_ALIGNMENT_START;
    return component;
}

IRComponent* ir_center(void) {
    IRComponent* component = ir_create_component(IR_COMPONENT_CENTER);
    IRLayout* layout = ir_get_layout(component);
    layout->flex.justify_content = IR_ALIGNMENT_CENTER;
    layout->flex.cross_axis = IR_ALIGNMENT_CENTER;
    layout->flex.justify_content = IR_ALIGNMENT_CENTER;
    return component;
}

// Inline Semantic Components (for rich text)
IRComponent* ir_span(void) {
    return ir_create_component(IR_COMPONENT_SPAN);
}

IRComponent* ir_strong(const char* text) {
    IRComponent* component = ir_create_component(IR_COMPONENT_STRONG);
    if (text) {
        component->text_content = strdup(text);
    }
    // Apply bold font weight
    IRStyle* style = ir_get_style(component);
    if (style) {
        style->font.weight = 700;
    }
    return component;
}

IRComponent* ir_em(const char* text) {
    IRComponent* component = ir_create_component(IR_COMPONENT_EM);
    if (text) {
        component->text_content = strdup(text);
    }
    // Apply italic style
    IRStyle* style = ir_get_style(component);
    if (style) {
        style->font.italic = true;
    }
    return component;
}

IRComponent* ir_code_inline(const char* text) {
    IRComponent* component = ir_create_component(IR_COMPONENT_CODE_INLINE);
    if (text) {
        component->text_content = strdup(text);
    }
    // Apply monospace font
    IRStyle* style = ir_get_style(component);
    if (style) {
        if (style->font.family) free(style->font.family);
        style->font.family = strdup("monospace");
    }
    return component;
}

IRComponent* ir_small(const char* text) {
    IRComponent* component = ir_create_component(IR_COMPONENT_SMALL);
    if (text) {
        component->text_content = strdup(text);
    }
    // Apply smaller font size
    IRStyle* style = ir_get_style(component);
    if (style) {
        style->font.size = 0.8f * 16.0f;  // 80% of default 16px
    }
    return component;
}

IRComponent* ir_mark(const char* text) {
    IRComponent* component = ir_create_component(IR_COMPONENT_MARK);
    if (text) {
        component->text_content = strdup(text);
    }
    // Apply yellow highlight background
    IRStyle* style = ir_get_style(component);
    if (style) {
        style->background = IR_COLOR_RGBA(255, 255, 0, 255);
    }
    return component;
}

// Dimension Helpers
IRDimension ir_dimension_flex(float value) {
    IRDimension dim = { IR_DIMENSION_FLEX, value };
    return dim;
}

// Color Helpers
IRColor ir_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return IR_COLOR_RGBA(r, g, b, 255);
}

IRColor ir_color_transparent(void) {
    return IR_COLOR_RGBA(0, 0, 0, 0);
}

// Validation and Optimization
bool ir_validate_component(IRComponent* component) {
    if (!component) return false;

    // Basic validation
    if (component->type == IR_COMPONENT_TEXT && !component->text_content) {
        return false;  // Text components need content
    }

    // Validate children recursively
    for (uint32_t i = 0; i < component->child_count; i++) {
        if (!ir_validate_component(component->children[i])) {
            return false;
        }
    }

    return true;
}

void ir_optimize_component(IRComponent* component) {
    if (!component) return;

    // Remove empty containers
    if (component->type == IR_COMPONENT_CONTAINER && component->child_count == 0) {
        // Mark for removal if parent exists
        return;
    }

    // Optimize children
    for (uint32_t i = 0; i < component->child_count; i++) {
        ir_optimize_component(component->children[i]);
    }
}

// ============================================================================
// Hit Testing - Find component at a given point
// ============================================================================

bool ir_is_point_in_component(IRComponent* component, float x, float y) {
    if (!component || !component->rendered_bounds.valid) {
        return false;
    }

    float comp_x = component->rendered_bounds.x;
    float comp_y = component->rendered_bounds.y;
    float comp_w = component->rendered_bounds.width;
    float comp_h = component->rendered_bounds.height;

    return (x >= comp_x && x < comp_x + comp_w &&
            y >= comp_y && y < comp_y + comp_h);
}

IRComponent* ir_find_component_at_point(IRComponent* root, float x, float y) {
    if (!root) {
        return NULL;
    }

    // Skip invisible components - they shouldn't be interactive
    if (root->style && !root->style->visible) {
        return NULL;
    }

    if (!ir_is_point_in_component(root, x, y)) {
        return NULL;
    }

    // Find the child with highest z-index that contains the point
    IRComponent* best_target = NULL;
    uint32_t best_z_index = 0;

    for (uint32_t i = 0; i < root->child_count; i++) {
        IRComponent* child = root->children[i];
        if (!child) continue;

        // Check if point is in this child's bounds
        if (ir_is_point_in_component(child, x, y)) {
            // Recursively find target in this child's subtree
            IRComponent* child_target = ir_find_component_at_point(child, x, y);
            if (child_target != NULL) {
                // Get the effective z-index
                uint32_t effective_z = child_target->style ? child_target->style->z_index : 0;

                if (best_target == NULL || effective_z > best_z_index) {
                    best_z_index = effective_z;
                    best_target = child_target;
                }
            }
        }
    }

    if (best_target != NULL) {
        return best_target;
    }

    // No child handled the event, return this component
    return root;
}

void ir_set_rendered_bounds(IRComponent* component, float x, float y, float width, float height) {
    if (!component) return;

    component->rendered_bounds.x = x;
    component->rendered_bounds.y = y;
    component->rendered_bounds.width = width;
    component->rendered_bounds.height = height;
    component->rendered_bounds.valid = true;
}

// Layout alignment functions - proper implementation using flexbox
void ir_set_justify_content(IRLayout* layout, IRAlignment justify) {
    if (!layout) return;
    // justify-content controls main axis alignment (horizontal for row, vertical for column)
    layout->flex.justify_content = justify;
}

void ir_set_align_items(IRLayout* layout, IRAlignment align) {
    if (!layout) return;
    // align-items controls cross axis alignment (vertical for row, horizontal for column)
    layout->flex.cross_axis = align;
}

void ir_set_align_content(IRLayout* layout, IRAlignment align) {
    if (!layout) return;
    // align-content controls how multiple rows/columns are aligned (when wrapping)
    // For now, use main_axis to control overall alignment
    layout->flex.justify_content = align;
}

// ============================================================================
// BiDi Direction Property Helpers
// ============================================================================

void ir_set_base_direction(IRComponent* component, IRDirection dir) {
    if (!component || !component->layout) return;
    component->layout->flex.base_direction = (uint8_t)dir;
    mark_style_dirty(component);
}

void ir_set_unicode_bidi(IRComponent* component, IRUnicodeBidi bidi) {
    if (!component || !component->layout) return;
    component->layout->flex.unicode_bidi = (uint8_t)bidi;
    mark_style_dirty(component);
}

IRDirection ir_parse_direction(const char* str) {
    if (!str) return IR_DIRECTION_LTR;
    if (ir_str_ieq(str, "rtl")) return IR_DIRECTION_RTL;
    if (ir_str_ieq(str, "ltr")) return IR_DIRECTION_LTR;
    if (ir_str_ieq(str, "auto")) return IR_DIRECTION_AUTO;
    if (ir_str_ieq(str, "inherit")) return IR_DIRECTION_INHERIT;
    return IR_DIRECTION_LTR;  // Default to LTR
}

IRUnicodeBidi ir_parse_unicode_bidi(const char* str) {
    if (!str) return IR_UNICODE_BIDI_NORMAL;
    if (ir_str_ieq(str, "normal")) return IR_UNICODE_BIDI_NORMAL;
    if (ir_str_ieq(str, "embed")) return IR_UNICODE_BIDI_EMBED;
    if (ir_str_ieq(str, "isolate") || ir_str_ieq(str, "bidi-override")) return IR_UNICODE_BIDI_ISOLATE;
    if (ir_str_ieq(str, "plaintext")) return IR_UNICODE_BIDI_PLAINTEXT;
    return IR_UNICODE_BIDI_NORMAL;  // Default to normal
}

// ============================================================================
// Gradient Helpers
// ============================================================================

IRGradient* ir_gradient_create_linear(float angle) {
    IRGradient* gradient = (IRGradient*)calloc(1, sizeof(IRGradient));
    if (!gradient) return NULL;

    gradient->type = IR_GRADIENT_LINEAR;
    gradient->angle = angle;
    gradient->stop_count = 0;
    gradient->center_x = 0.5f;
    gradient->center_y = 0.5f;

    return gradient;
}

IRGradient* ir_gradient_create_radial(float center_x, float center_y) {
    IRGradient* gradient = (IRGradient*)calloc(1, sizeof(IRGradient));
    if (!gradient) return NULL;

    gradient->type = IR_GRADIENT_RADIAL;
    gradient->center_x = center_x;
    gradient->center_y = center_y;
    gradient->stop_count = 0;
    gradient->angle = 0.0f;

    return gradient;
}

IRGradient* ir_gradient_create_conic(float center_x, float center_y) {
    IRGradient* gradient = (IRGradient*)calloc(1, sizeof(IRGradient));
    if (!gradient) return NULL;

    gradient->type = IR_GRADIENT_CONIC;
    gradient->center_x = center_x;
    gradient->center_y = center_y;
    gradient->stop_count = 0;
    gradient->angle = 0.0f;

    return gradient;
}

void ir_gradient_add_stop(IRGradient* gradient, float position, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!gradient) return;

    if (gradient->stop_count >= 8) {
        fprintf(stderr, "Warning: Gradient stop limit (8) exceeded, ignoring additional stop\n");
        return;
    }

    IRGradientStop* stop = &gradient->stops[gradient->stop_count];
    stop->position = position;
    stop->r = r;
    stop->g = g;
    stop->b = b;
    stop->a = a;

    gradient->stop_count++;
}

void ir_gradient_destroy(IRGradient* gradient) {
    if (gradient) {
        free(gradient);
    }
}

// Unified gradient creation (for Nim bindings)
IRGradient* ir_gradient_create(IRGradientType type) {
    IRGradient* gradient = (IRGradient*)calloc(1, sizeof(IRGradient));
    if (!gradient) return NULL;

    gradient->type = type;
    gradient->stop_count = 0;
    gradient->center_x = 0.5f;
    gradient->center_y = 0.5f;
    gradient->angle = 0.0f;

    return gradient;
}

void ir_gradient_set_angle(IRGradient* gradient, float angle) {
    if (!gradient) return;
    gradient->angle = angle;
}

void ir_gradient_set_center(IRGradient* gradient, float x, float y) {
    if (!gradient) return;
    gradient->center_x = x;
    gradient->center_y = y;
}

IRColor ir_color_from_gradient(IRGradient* gradient) {
    IRColor color;
    color.type = IR_COLOR_GRADIENT;
    color.data.gradient = gradient;
    return color;
}

void ir_set_background_gradient(IRStyle* style, IRGradient* gradient) {
    if (!style || !gradient) return;
    style->background = ir_color_from_gradient(gradient);
}

// ============================================================================
// Animation Builder Functions
// ============================================================================

IRAnimation* ir_animation_create_keyframe(const char* name, float duration) {
    IRAnimation* anim = (IRAnimation*)calloc(1, sizeof(IRAnimation));
    if (!anim) return NULL;

    if (name) {
        anim->name = strdup(name);
    }
    anim->duration = duration;
    anim->delay = 0.0f;
    anim->iteration_count = 1;
    anim->alternate = false;
    anim->default_easing = IR_EASING_LINEAR;
    anim->keyframe_count = 0;
    anim->current_time = 0.0f;
    anim->current_iteration = 0;
    anim->is_paused = false;

    return anim;
}

void ir_animation_destroy(IRAnimation* anim) {
    if (!anim) return;
    if (anim->name) {
        free(anim->name);
    }
    free(anim);
}

void ir_animation_set_iterations(IRAnimation* anim, int32_t count) {
    if (anim) {
        anim->iteration_count = count;
    }
}

void ir_animation_set_alternate(IRAnimation* anim, bool alternate) {
    if (anim) {
        anim->alternate = alternate;
    }
}

void ir_animation_set_delay(IRAnimation* anim, float delay) {
    if (anim) {
        anim->delay = delay;
    }
}

void ir_animation_set_default_easing(IRAnimation* anim, IREasingType easing) {
    if (anim) {
        anim->default_easing = easing;
    }
}

IRKeyframe* ir_animation_add_keyframe(IRAnimation* anim, float offset) {
    if (!anim || anim->keyframe_count >= IR_MAX_KEYFRAMES) return NULL;

    IRKeyframe* kf = &anim->keyframes[anim->keyframe_count];
    kf->offset = offset;
    kf->easing = anim->default_easing;
    kf->property_count = 0;

    // Initialize all properties as not set
    for (int i = 0; i < IR_MAX_KEYFRAME_PROPERTIES; i++) {
        kf->properties[i].is_set = false;
    }

    anim->keyframe_count++;
    return kf;
}

void ir_keyframe_set_property(IRKeyframe* kf, IRAnimationProperty prop, float value) {
    if (!kf || kf->property_count >= IR_MAX_KEYFRAME_PROPERTIES) return;

    // Check if property already exists, update it
    for (uint8_t i = 0; i < kf->property_count; i++) {
        if (kf->properties[i].property == prop) {
            kf->properties[i].value = value;
            kf->properties[i].is_set = true;
            return;
        }
    }

    // Add new property
    kf->properties[kf->property_count].property = prop;
    kf->properties[kf->property_count].value = value;
    kf->properties[kf->property_count].is_set = true;
    kf->property_count++;
}

void ir_keyframe_set_color_property(IRKeyframe* kf, IRAnimationProperty prop, IRColor color) {
    if (!kf || kf->property_count >= IR_MAX_KEYFRAME_PROPERTIES) return;

    // Check if property already exists, update it
    for (uint8_t i = 0; i < kf->property_count; i++) {
        if (kf->properties[i].property == prop) {
            kf->properties[i].color_value = color;
            kf->properties[i].is_set = true;
            return;
        }
    }

    // Add new property
    kf->properties[kf->property_count].property = prop;
    kf->properties[kf->property_count].color_value = color;
    kf->properties[kf->property_count].is_set = true;
    kf->property_count++;
}

void ir_keyframe_set_easing(IRKeyframe* kf, IREasingType easing) {
    if (kf) {
        kf->easing = easing;
    }
}

void ir_component_add_animation(IRComponent* component, IRAnimation* anim) {
    if (!component || !anim) return;

    IRStyle* style = ir_get_style(component);
    if (!style) {
        style = ir_create_style();
        ir_set_style(component, style);
    }

    // Reallocate animations array (array of pointers)
    IRAnimation** new_anims = (IRAnimation**)realloc(style->animations,
                                                     (style->animation_count + 1) * sizeof(IRAnimation*));
    if (!new_anims) return;

    style->animations = new_anims;
    style->animations[style->animation_count] = anim;  // Store pointer
    style->animation_count++;

    // OPTIMIZATION: Set animation flag and propagate to ancestors
    component->has_active_animations = true;
    IRComponent* ancestor = component->parent;
    while (ancestor) {
        ancestor->has_active_animations = true;
        ancestor = ancestor->parent;
    }
}

// Re-propagate has_active_animations flags from children to ancestors
// Call this after the component tree is fully constructed
// FIX: Animations were attached before components were added to parents,
//      so the flag propagation in ir_component_add_animation didn't work
void ir_animation_propagate_flags(IRComponent* root) {
    if (!root) return;

    // Reset flag for this component
    root->has_active_animations = false;

    // Check if this component has animations (stored in style)
    IRStyle* style = ir_get_style(root);
    if (style && style->animation_count > 0) {
        root->has_active_animations = true;
    }

    // Recursively check children and propagate upward
    for (uint32_t i = 0; i < root->child_count; i++) {
        ir_animation_propagate_flags(root->children[i]);

        // If any child has animations, propagate to parent
        if (root->children[i]->has_active_animations) {
            root->has_active_animations = true;
        }
    }
}

// General component subtree finalization
// Call this after components are added (especially from static loops) to ensure
// all post-construction propagation steps are performed
void ir_component_finalize_subtree(IRComponent* component) {
    if (!component) return;

    // Propagate animation flags for this subtree
    ir_animation_propagate_flags(component);

    // Future finalization steps can be added here:
    // - Validate event handlers are properly registered
    // - Propagate style inheritance flags
    // - Initialize layout constraint caches
    // - etc.
}

// Transition functions
IRTransition* ir_transition_create(IRAnimationProperty property, float duration) {
    IRTransition* trans = (IRTransition*)calloc(1, sizeof(IRTransition));
    if (!trans) return NULL;

    trans->property = property;
    trans->duration = duration;
    trans->delay = 0.0f;
    trans->easing = IR_EASING_EASE_IN_OUT;
    trans->trigger_state = 0;  // All states by default

    return trans;
}

void ir_transition_destroy(IRTransition* transition) {
    free(transition);
}

void ir_transition_set_easing(IRTransition* transition, IREasingType easing) {
    if (transition) {
        transition->easing = easing;
    }
}

void ir_transition_set_delay(IRTransition* transition, float delay) {
    if (transition) {
        transition->delay = delay;
    }
}

void ir_transition_set_trigger(IRTransition* transition, uint32_t state_mask) {
    if (transition) {
        transition->trigger_state = state_mask;
    }
}

void ir_component_add_transition(IRComponent* component, IRTransition* transition) {
    if (!component || !transition) return;

    IRStyle* style = ir_get_style(component);
    if (!style) {
        style = ir_create_style();
        ir_set_style(component, style);
    }

    // Reallocate transitions array
    IRTransition** new_trans = (IRTransition**)realloc(style->transitions,
                                                      (style->transition_count + 1) * sizeof(IRTransition*));
    if (!new_trans) return;

    style->transitions = new_trans;
    style->transitions[style->transition_count] = transition;
    style->transition_count++;
}

// Tree-wide animation update
void ir_animation_tree_update(IRComponent* root, float current_time) {
    if (!root) return;

    // OPTIMIZATION: Skip entire subtree if no animations (80% reduction - visits only ~5% of nodes)
    if (!root->has_active_animations) return;

    // Apply all animations on this component
    if (root->style && root->style->animations) {
        for (uint32_t i = 0; i < root->style->animation_count; i++) {
            ir_animation_apply_keyframes(root, root->style->animations[i], current_time);
        }
    }

    // Recursively update children
    for (uint32_t i = 0; i < root->child_count; i++) {
        ir_animation_tree_update(root->children[i], current_time);
    }
}

// ============================================================================
// Grid Layout (Phase 5)
// ============================================================================

void ir_set_grid_template_rows(IRLayout* layout, IRGridTrack* tracks, uint8_t count) {
    if (!layout || !tracks) return;
    if (count > IR_MAX_GRID_TRACKS) count = IR_MAX_GRID_TRACKS;

    layout->mode = IR_LAYOUT_MODE_GRID;
    for (uint8_t i = 0; i < count; i++) {
        layout->grid.rows[i] = tracks[i];
    }
    layout->grid.row_count = count;
}

void ir_set_grid_template_columns(IRLayout* layout, IRGridTrack* tracks, uint8_t count) {
    if (!layout || !tracks) return;
    if (count > IR_MAX_GRID_TRACKS) count = IR_MAX_GRID_TRACKS;

    layout->mode = IR_LAYOUT_MODE_GRID;
    for (uint8_t i = 0; i < count; i++) {
        layout->grid.columns[i] = tracks[i];
    }
    layout->grid.column_count = count;
}

void ir_set_grid_gap(IRLayout* layout, float row_gap, float column_gap) {
    if (!layout) return;
    layout->grid.row_gap = row_gap;
    layout->grid.column_gap = column_gap;
}

void ir_set_grid_auto_flow(IRLayout* layout, bool row_direction, bool dense) {
    if (!layout) return;
    layout->grid.auto_flow_row = row_direction;
    layout->grid.auto_flow_dense = dense;
}

void ir_set_grid_alignment(IRLayout* layout, IRAlignment justify_items, IRAlignment align_items,
                            IRAlignment justify_content, IRAlignment align_content) {
    if (!layout) return;
    layout->grid.justify_items = justify_items;
    layout->grid.align_items = align_items;
    layout->grid.justify_content = justify_content;
    layout->grid.align_content = align_content;
}

// Grid Item Placement
void ir_set_grid_item_placement(IRStyle* style, int16_t row_start, int16_t row_end,
                                  int16_t column_start, int16_t column_end) {
    if (!style) return;
    style->grid_item.row_start = row_start;
    style->grid_item.row_end = row_end;
    style->grid_item.column_start = column_start;
    style->grid_item.column_end = column_end;
}

void ir_set_grid_item_alignment(IRStyle* style, IRAlignment justify_self, IRAlignment align_self) {
    if (!style) return;
    style->grid_item.justify_self = justify_self;
    style->grid_item.align_self = align_self;
}

// Grid Track Helpers
IRGridTrack ir_grid_track_px(float value) {
    IRGridTrack track;
    track.type = IR_GRID_TRACK_PX;
    track.value = value;
    return track;
}

IRGridTrack ir_grid_track_percent(float value) {
    IRGridTrack track;
    track.type = IR_GRID_TRACK_PERCENT;
    track.value = value;
    return track;
}

IRGridTrack ir_grid_track_fr(float value) {
    IRGridTrack track;
    track.type = IR_GRID_TRACK_FR;
    track.value = value;
    return track;
}

IRGridTrack ir_grid_track_auto(void) {
    IRGridTrack track;
    track.type = IR_GRID_TRACK_AUTO;
    track.value = 0;
    return track;
}

IRGridTrack ir_grid_track_min_content(void) {
    IRGridTrack track;
    track.type = IR_GRID_TRACK_MIN_CONTENT;
    track.value = 0;
    return track;
}

IRGridTrack ir_grid_track_max_content(void) {
    IRGridTrack track;
    track.type = IR_GRID_TRACK_MAX_CONTENT;
    track.value = 0;
    return track;
}

// ============================================================================
// Container Queries (Phase 6)
// ============================================================================

void ir_set_container_type(IRStyle* style, IRContainerType type) {
    if (!style) return;
    style->container_type = type;
}

void ir_set_container_name(IRStyle* style, const char* name) {
    if (!style) return;
    if (style->container_name) {
        free(style->container_name);
    }
    style->container_name = name ? strdup(name) : NULL;
}

void ir_add_breakpoint(IRStyle* style, IRQueryCondition* conditions, uint8_t condition_count) {
    if (!style || !conditions) return;
    if (style->breakpoint_count >= IR_MAX_BREAKPOINTS) return;
    if (condition_count > IR_MAX_QUERY_CONDITIONS) condition_count = IR_MAX_QUERY_CONDITIONS;

    IRBreakpoint* bp = &style->breakpoints[style->breakpoint_count];

    // Copy conditions
    for (uint8_t i = 0; i < condition_count; i++) {
        bp->conditions[i] = conditions[i];
    }
    bp->condition_count = condition_count;

    // Initialize with defaults
    bp->width.type = IR_DIMENSION_AUTO;
    bp->height.type = IR_DIMENSION_AUTO;
    bp->visible = true;
    bp->opacity = -1.0f;  // -1 means don't override
    bp->has_layout_mode = false;

    style->breakpoint_count++;
}

void ir_breakpoint_set_width(IRStyle* style, uint8_t breakpoint_index, IRDimensionType type, float value) {
    if (!style || breakpoint_index >= style->breakpoint_count) return;
    IRBreakpoint* bp = &style->breakpoints[breakpoint_index];
    bp->width.type = type;
    bp->width.value = value;
}

void ir_breakpoint_set_height(IRStyle* style, uint8_t breakpoint_index, IRDimensionType type, float value) {
    if (!style || breakpoint_index >= style->breakpoint_count) return;
    IRBreakpoint* bp = &style->breakpoints[breakpoint_index];
    bp->height.type = type;
    bp->height.value = value;
}

void ir_breakpoint_set_visible(IRStyle* style, uint8_t breakpoint_index, bool visible) {
    if (!style || breakpoint_index >= style->breakpoint_count) return;
    style->breakpoints[breakpoint_index].visible = visible;
}

void ir_breakpoint_set_opacity(IRStyle* style, uint8_t breakpoint_index, float opacity) {
    if (!style || breakpoint_index >= style->breakpoint_count) return;
    style->breakpoints[breakpoint_index].opacity = opacity;
}

void ir_breakpoint_set_layout_mode(IRStyle* style, uint8_t breakpoint_index, IRLayoutMode mode) {
    if (!style || breakpoint_index >= style->breakpoint_count) return;
    IRBreakpoint* bp = &style->breakpoints[breakpoint_index];
    bp->layout_mode = mode;
    bp->has_layout_mode = true;
}

// Query Condition Helpers
IRQueryCondition ir_query_min_width(float value) {
    IRQueryCondition cond;
    cond.type = IR_QUERY_MIN_WIDTH;
    cond.value = value;
    return cond;
}

IRQueryCondition ir_query_max_width(float value) {
    IRQueryCondition cond;
    cond.type = IR_QUERY_MAX_WIDTH;
    cond.value = value;
    return cond;
}

IRQueryCondition ir_query_min_height(float value) {
    IRQueryCondition cond;
    cond.type = IR_QUERY_MIN_HEIGHT;
    cond.value = value;
    return cond;
}

IRQueryCondition ir_query_max_height(float value) {
    IRQueryCondition cond;
    cond.type = IR_QUERY_MAX_HEIGHT;
    cond.value = value;
    return cond;
}

// ============================================================================
// Table Components
// ============================================================================

// Create table state with default styling
IRTableState* ir_table_create_state(void) {
    IRTableState* state = (IRTableState*)calloc(1, sizeof(IRTableState));
    if (!state) return NULL;

    // Initialize with default styling
    state->style.border_color = ir_color_rgba(200, 200, 200, 255);  // Light gray
    state->style.header_background = ir_color_rgba(240, 240, 240, 255);  // Very light gray
    state->style.even_row_background = ir_color_rgba(255, 255, 255, 255);  // White
    state->style.odd_row_background = ir_color_rgba(249, 249, 249, 255);  // Off-white
    state->style.border_width = 1.0f;
    state->style.cell_padding = 8.0f;
    state->style.show_borders = true;
    state->style.striped_rows = false;
    state->style.header_sticky = false;
    state->style.collapse_borders = true;

    state->columns = NULL;
    state->column_count = 0;
    state->calculated_widths = NULL;
    state->calculated_heights = NULL;
    state->row_count = 0;
    state->header_row_count = 0;
    state->footer_row_count = 0;
    state->span_map = NULL;
    state->span_map_rows = 0;
    state->span_map_cols = 0;
    state->layout_valid = false;

    return state;
}

// Destroy table state
void ir_table_destroy_state(IRTableState* state) {
    if (!state) return;

    if (state->columns) free(state->columns);
    if (state->calculated_widths) free(state->calculated_widths);
    if (state->calculated_heights) free(state->calculated_heights);
    if (state->span_map) free(state->span_map);

    free(state);
}

// Get table state from component
IRTableState* ir_get_table_state(IRComponent* component) {
    if (!component || component->type != IR_COMPONENT_TABLE || !component->custom_data) {
        return NULL;
    }
    return (IRTableState*)component->custom_data;
}

// Add a column definition to the table
void ir_table_add_column(IRTableState* state, IRTableColumnDef column) {
    if (!state) return;
    if (state->column_count >= IR_MAX_TABLE_COLUMNS) {
        fprintf(stderr, "[ir_builder] Table column limit (%d) exceeded\n", IR_MAX_TABLE_COLUMNS);
        return;
    }

    // Reallocate columns array
    IRTableColumnDef* new_columns = (IRTableColumnDef*)realloc(
        state->columns,
        (state->column_count + 1) * sizeof(IRTableColumnDef)
    );
    if (!new_columns) return;

    state->columns = new_columns;
    state->columns[state->column_count] = column;
    state->column_count++;
    state->layout_valid = false;
}

// Set table styling
void ir_table_set_border_color(IRTableState* state, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!state) return;
    state->style.border_color = ir_color_rgba(r, g, b, a);
}

void ir_table_set_header_background(IRTableState* state, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!state) return;
    state->style.header_background = ir_color_rgba(r, g, b, a);
}

void ir_table_set_row_backgrounds(IRTableState* state,
                                   uint8_t even_r, uint8_t even_g, uint8_t even_b, uint8_t even_a,
                                   uint8_t odd_r, uint8_t odd_g, uint8_t odd_b, uint8_t odd_a) {
    if (!state) return;
    state->style.even_row_background = ir_color_rgba(even_r, even_g, even_b, even_a);
    state->style.odd_row_background = ir_color_rgba(odd_r, odd_g, odd_b, odd_a);
}

void ir_table_set_border_width(IRTableState* state, float width) {
    if (!state) return;
    state->style.border_width = width;
}

void ir_table_set_cell_padding(IRTableState* state, float padding) {
    if (!state) return;
    state->style.cell_padding = padding;
}

void ir_table_set_show_borders(IRTableState* state, bool show) {
    if (!state) return;
    state->style.show_borders = show;
}

void ir_table_set_striped(IRTableState* state, bool striped) {
    if (!state) return;
    state->style.striped_rows = striped;
}

// Create table cell data with colspan/rowspan
IRTableCellData* ir_table_cell_data_create(uint16_t colspan, uint16_t rowspan, IRAlignment alignment) {
    IRTableCellData* data = (IRTableCellData*)calloc(1, sizeof(IRTableCellData));
    if (!data) return NULL;

    data->colspan = colspan > 0 ? colspan : 1;
    data->rowspan = rowspan > 0 ? rowspan : 1;
    data->alignment = (uint8_t)alignment;
    data->vertical_alignment = (uint8_t)IR_ALIGNMENT_CENTER;
    data->is_spanned = false;
    data->spanned_by_id = 0;

    return data;
}

// Get cell data from a table cell component
IRTableCellData* ir_get_table_cell_data(IRComponent* component) {
    if (!component) return NULL;
    if (component->type != IR_COMPONENT_TABLE_CELL &&
        component->type != IR_COMPONENT_TABLE_HEADER_CELL) {
        return NULL;
    }
    if (!component->custom_data) return NULL;
    return (IRTableCellData*)component->custom_data;
}

// Set cell colspan
void ir_table_cell_set_colspan(IRComponent* cell, uint16_t colspan) {
    IRTableCellData* data = ir_get_table_cell_data(cell);
    if (data) {
        data->colspan = colspan > 0 ? colspan : 1;
    }
}

// Set cell rowspan
void ir_table_cell_set_rowspan(IRComponent* cell, uint16_t rowspan) {
    IRTableCellData* data = ir_get_table_cell_data(cell);
    if (data) {
        data->rowspan = rowspan > 0 ? rowspan : 1;
    }
}

// Set cell alignment
void ir_table_cell_set_alignment(IRComponent* cell, IRAlignment alignment) {
    IRTableCellData* data = ir_get_table_cell_data(cell);
    if (data) {
        data->alignment = (uint8_t)alignment;
    }
}

// Convenience component creation functions

IRComponent* ir_table(void) {
    IRComponent* component = ir_create_component(IR_COMPONENT_TABLE);
    if (!component) return NULL;

    // Create and attach table state
    IRTableState* state = ir_table_create_state();
    if (state) {
        component->custom_data = (char*)state;
    }

    return component;
}

IRComponent* ir_table_head(void) {
    return ir_create_component(IR_COMPONENT_TABLE_HEAD);
}

IRComponent* ir_table_body(void) {
    return ir_create_component(IR_COMPONENT_TABLE_BODY);
}

IRComponent* ir_table_foot(void) {
    return ir_create_component(IR_COMPONENT_TABLE_FOOT);
}

IRComponent* ir_table_row(void) {
    return ir_create_component(IR_COMPONENT_TABLE_ROW);
}

IRComponent* ir_table_cell(uint16_t colspan, uint16_t rowspan) {
    IRComponent* component = ir_create_component(IR_COMPONENT_TABLE_CELL);
    if (!component) return NULL;

    // Create and attach cell data
    IRTableCellData* data = ir_table_cell_data_create(colspan, rowspan, IR_ALIGNMENT_START);
    if (data) {
        component->custom_data = (char*)data;
    }

    return component;
}

IRComponent* ir_table_header_cell(uint16_t colspan, uint16_t rowspan) {
    IRComponent* component = ir_create_component(IR_COMPONENT_TABLE_HEADER_CELL);
    if (!component) return NULL;

    // Create and attach cell data with center alignment for headers
    IRTableCellData* data = ir_table_cell_data_create(colspan, rowspan, IR_ALIGNMENT_CENTER);
    if (data) {
        component->custom_data = (char*)data;
    }

    return component;
}

// Build span map for efficient layout
static void ir_table_build_span_map(IRTableState* state, IRComponent* table) {
    if (!state || !table) return;

    // Count rows and determine column count
    uint32_t total_rows = 0;
    uint32_t max_cols = state->column_count;

    // Traverse sections to count rows
    for (uint32_t i = 0; i < table->child_count; i++) {
        IRComponent* section = table->children[i];
        if (!section) continue;

        if (section->type == IR_COMPONENT_TABLE_HEAD) {
            state->head_section = section;
            state->header_row_count = section->child_count;
        } else if (section->type == IR_COMPONENT_TABLE_BODY) {
            state->body_section = section;
        } else if (section->type == IR_COMPONENT_TABLE_FOOT) {
            state->foot_section = section;
            state->footer_row_count = section->child_count;
        }

        total_rows += section->child_count;

        // Count columns from first row if not explicitly set
        if (max_cols == 0 && section->child_count > 0) {
            IRComponent* first_row = section->children[0];
            if (first_row) {
                max_cols = first_row->child_count;
            }
        }
    }

    state->row_count = total_rows;
    if (max_cols == 0) max_cols = 1;

    // Allocate span map
    if (state->span_map) free(state->span_map);
    state->span_map = (IRTableCellData*)calloc(total_rows * max_cols, sizeof(IRTableCellData));
    state->span_map_rows = total_rows;
    state->span_map_cols = max_cols;

    // Initialize all cells with default span (1x1)
    for (uint32_t i = 0; i < total_rows * max_cols; i++) {
        state->span_map[i].colspan = 1;
        state->span_map[i].rowspan = 1;
        state->span_map[i].is_spanned = false;
    }

    // Populate span map from actual cells
    uint32_t row_offset = 0;
    for (uint32_t s = 0; s < table->child_count; s++) {
        IRComponent* section = table->children[s];
        if (!section) continue;

        for (uint32_t r = 0; r < section->child_count; r++) {
            IRComponent* row = section->children[r];
            if (!row || row->type != IR_COMPONENT_TABLE_ROW) continue;

            uint32_t col = 0;
            for (uint32_t c = 0; c < row->child_count && col < max_cols; c++) {
                IRComponent* cell = row->children[c];
                if (!cell) continue;

                // Skip spanned positions
                while (col < max_cols && state->span_map[(row_offset + r) * max_cols + col].is_spanned) {
                    col++;
                }
                if (col >= max_cols) break;

                IRTableCellData* cell_data = ir_get_table_cell_data(cell);
                uint16_t colspan = cell_data ? cell_data->colspan : 1;
                uint16_t rowspan = cell_data ? cell_data->rowspan : 1;

                // Mark this cell's position
                IRTableCellData* map_entry = &state->span_map[(row_offset + r) * max_cols + col];
                map_entry->colspan = colspan;
                map_entry->rowspan = rowspan;
                map_entry->alignment = cell_data ? cell_data->alignment : (uint8_t)IR_ALIGNMENT_START;

                // Mark spanned positions
                for (uint16_t rs = 0; rs < rowspan && (row_offset + r + rs) < total_rows; rs++) {
                    for (uint16_t cs = 0; cs < colspan && (col + cs) < max_cols; cs++) {
                        if (rs == 0 && cs == 0) continue;  // Skip the cell itself
                        IRTableCellData* spanned = &state->span_map[(row_offset + r + rs) * max_cols + (col + cs)];
                        spanned->is_spanned = true;
                        spanned->spanned_by_id = cell->id;
                    }
                }

                col += colspan;
            }
        }
        row_offset += section->child_count;
    }
}

// Finalize table structure (call after all children are added)
void ir_table_finalize(IRComponent* table) {
    if (!table || table->type != IR_COMPONENT_TABLE) return;

    IRTableState* state = ir_get_table_state(table);
    if (!state) return;

    // Build span map
    ir_table_build_span_map(state, table);

    // Allocate calculated widths array if needed
    if (state->column_count > 0 || state->span_map_cols > 0) {
        uint32_t cols = state->column_count > 0 ? state->column_count : state->span_map_cols;
        if (state->calculated_widths) free(state->calculated_widths);
        state->calculated_widths = (float*)calloc(cols, sizeof(float));
    }

    // Allocate calculated heights array
    if (state->row_count > 0) {
        if (state->calculated_heights) free(state->calculated_heights);
        state->calculated_heights = (float*)calloc(state->row_count, sizeof(float));
    }

    state->layout_valid = false;
}

// Column definition helpers
IRTableColumnDef ir_table_column_auto(void) {
    IRTableColumnDef col = {0};
    col.auto_size = true;
    col.alignment = IR_ALIGNMENT_START;
    col.width.type = IR_DIMENSION_AUTO;
    col.min_width.type = IR_DIMENSION_PX;
    col.min_width.value = 50.0f;  // Reasonable minimum
    col.max_width.type = IR_DIMENSION_AUTO;
    return col;
}

IRTableColumnDef ir_table_column_px(float width) {
    IRTableColumnDef col = {0};
    col.auto_size = false;
    col.alignment = IR_ALIGNMENT_START;
    col.width.type = IR_DIMENSION_PX;
    col.width.value = width;
    col.min_width.type = IR_DIMENSION_PX;
    col.min_width.value = width;
    col.max_width.type = IR_DIMENSION_PX;
    col.max_width.value = width;
    return col;
}

IRTableColumnDef ir_table_column_percent(float percent) {
    IRTableColumnDef col = {0};
    col.auto_size = false;
    col.alignment = IR_ALIGNMENT_START;
    col.width.type = IR_DIMENSION_PERCENT;
    col.width.value = percent;
    col.min_width.type = IR_DIMENSION_AUTO;
    col.max_width.type = IR_DIMENSION_AUTO;
    return col;
}

IRTableColumnDef ir_table_column_with_alignment(IRTableColumnDef col, IRAlignment alignment) {
    col.alignment = alignment;
    return col;
}

// ============================================================================
// Markdown Components Implementation
// ============================================================================

IRComponent* ir_heading(uint8_t level, const char* text) {
    if (level < 1) level = 1;
    if (level > 6) level = 6;

    IRComponent* comp = ir_create_component(IR_COMPONENT_HEADING);
    if (!comp) return NULL;

    IRHeadingData* data = (IRHeadingData*)calloc(1, sizeof(IRHeadingData));
    data->level = level;
    data->text = text ? strdup(text) : NULL;
    data->id = NULL;
    comp->custom_data = (char*)data;

    // Set default styling based on level
    IRStyle* style = ir_get_style(comp);
    float font_sizes[] = {32.0f, 28.0f, 24.0f, 20.0f, 18.0f, 16.0f};
    ir_set_font(style, font_sizes[level - 1], NULL, 255, 255, 255, 255, true, false);
    ir_set_margin(comp, 24.0f, 0.0f, 12.0f, 0.0f);

    return comp;
}

IRComponent* ir_paragraph(void) {
    IRComponent* comp = ir_create_component(IR_COMPONENT_PARAGRAPH);
    if (!comp) return NULL;

    // Set default paragraph styling
    // Text wrapping is handled automatically by the backend based on component type
    IRStyle* style = ir_get_style(comp);
    ir_set_font(style, 16.0f, NULL, 230, 237, 243, 255, false, false);  // Light gray text for readability
    ir_set_margin(comp, 0.0f, 0.0f, 16.0f, 0.0f);

    return comp;
}

IRComponent* ir_blockquote(void) {
    IRComponent* comp = ir_create_component(IR_COMPONENT_BLOCKQUOTE);
    if (!comp) return NULL;

    IRBlockquoteData* data = (IRBlockquoteData*)calloc(1, sizeof(IRBlockquoteData));
    data->depth = 1;
    comp->custom_data = (char*)data;

    // Set default blockquote styling
    IRStyle* style = ir_get_style(comp);
    ir_set_padding(comp, 12.0f, 16.0f, 12.0f, 16.0f);
    ir_set_margin(comp, 0.0f, 0.0f, 16.0f, 0.0f);
    ir_set_background_color(style, 40, 44, 52, 255);  // Dark background
    ir_set_border(style, 4.0f, 80, 120, 200, 255, 0);  // Left border (blue-ish)

    return comp;
}

IRComponent* ir_code_block(const char* language, const char* code) {
    IRComponent* comp = ir_create_component(IR_COMPONENT_CODE_BLOCK);
    if (!comp) return NULL;

    IRCodeBlockData* data = (IRCodeBlockData*)calloc(1, sizeof(IRCodeBlockData));
    data->language = language ? strdup(language) : NULL;
    data->code = code ? strdup(code) : NULL;
    data->length = code ? strlen(code) : 0;
    data->show_line_numbers = false;
    data->start_line = 1;
    comp->custom_data = (char*)data;

    // Set default code block styling
    IRStyle* style = ir_get_style(comp);
    ir_set_font(style, 14.0f, "monospace", 220, 220, 220, 255, false, false);
    ir_set_background_color(style, 22, 27, 34, 255);  // Dark background
    ir_set_padding(comp, 16.0f, 16.0f, 16.0f, 16.0f);
    ir_set_margin(comp, 0.0f, 0.0f, 16.0f, 0.0f);

    return comp;
}

IRComponent* ir_horizontal_rule(void) {
    IRComponent* comp = ir_create_component(IR_COMPONENT_HORIZONTAL_RULE);
    if (!comp) return NULL;

    // Set default HR styling
    IRStyle* style = ir_get_style(comp);
    ir_set_height(comp, IR_DIMENSION_PX, 2.0f);
    ir_set_width(comp, IR_DIMENSION_PERCENT, 100.0f);
    ir_set_background_color(style, 80, 80, 80, 255);
    ir_set_margin(comp, 24.0f, 0.0f, 24.0f, 0.0f);

    return comp;
}

IRComponent* ir_list(IRListType type) {
    IRComponent* comp = ir_create_component(IR_COMPONENT_LIST);
    if (!comp) return NULL;

    IRListData* data = (IRListData*)calloc(1, sizeof(IRListData));
    data->type = type;
    data->start = 1;
    data->tight = true;
    comp->custom_data = (char*)data;

    // Set default list styling
    IRStyle* style = ir_get_style(comp);
    ir_set_margin(comp, 0.0f, 0.0f, 16.0f, 0.0f);
    ir_set_padding(comp, 0.0f, 0.0f, 0.0f, 24.0f);  // Left padding for indentation

    return comp;
}

IRComponent* ir_list_item(void) {
    IRComponent* comp = ir_create_component(IR_COMPONENT_LIST_ITEM);
    if (!comp) return NULL;

    IRListItemData* data = (IRListItemData*)calloc(1, sizeof(IRListItemData));
    data->number = 0;
    data->marker = NULL;
    data->is_task_item = false;
    data->is_checked = false;
    comp->custom_data = (char*)data;

    // Set default list item styling
    IRStyle* style = ir_get_style(comp);
    ir_set_margin(comp, 0.0f, 0.0f, 8.0f, 0.0f);

    return comp;
}

IRComponent* ir_link(const char* url, const char* text) {
    IRComponent* comp = ir_create_component(IR_COMPONENT_LINK);
    if (!comp) return NULL;

    IRLinkData* data = (IRLinkData*)calloc(1, sizeof(IRLinkData));
    data->url = url ? strdup(url) : NULL;
    data->title = NULL;
    comp->custom_data = (char*)data;

    // Set default link styling (underlined blue text)
    IRStyle* style = ir_get_style(comp);
    ir_set_font(style, 16.0f, NULL, 100, 150, 255, 255, false, false);
    // Note: Text decoration flags are set via style->text_decorations (not text_decoration)

    // If text is provided, create a Text child component
    if (text) {
        IRComponent* text_comp = ir_text(text);
        if (text_comp) {
            ir_add_child(comp, text_comp);
        }
    }

    return comp;
}

// Markdown Component Setters

void ir_set_heading_level(IRComponent* comp, uint8_t level) {
    if (!comp || comp->type != IR_COMPONENT_HEADING) return;
    IRHeadingData* data = (IRHeadingData*)comp->custom_data;
    if (!data) return;

    if (level < 1) level = 1;
    if (level > 6) level = 6;
    data->level = level;

    // Update font size based on level
    float font_sizes[] = {32.0f, 28.0f, 24.0f, 20.0f, 18.0f, 16.0f};
    IRStyle* style = ir_get_style(comp);
    style->font.size = font_sizes[level - 1];
}

void ir_set_heading_id(IRComponent* comp, const char* id) {
    if (!comp || comp->type != IR_COMPONENT_HEADING) return;
    IRHeadingData* data = (IRHeadingData*)comp->custom_data;
    if (!data) return;

    if (data->id) free(data->id);
    data->id = id ? strdup(id) : NULL;
}

void ir_set_code_language(IRComponent* comp, const char* language) {
    if (!comp || comp->type != IR_COMPONENT_CODE_BLOCK) return;
    IRCodeBlockData* data = (IRCodeBlockData*)comp->custom_data;
    if (!data) return;

    if (data->language) free(data->language);
    data->language = language ? strdup(language) : NULL;
}

void ir_set_code_content(IRComponent* comp, const char* code, size_t length) {
    if (!comp || comp->type != IR_COMPONENT_CODE_BLOCK) return;
    IRCodeBlockData* data = (IRCodeBlockData*)comp->custom_data;
    if (!data) return;

    if (data->code) free(data->code);
    if (code && length > 0) {
        data->code = (char*)malloc(length + 1);
        memcpy(data->code, code, length);
        data->code[length] = '\0';
        data->length = length;
    } else {
        data->code = NULL;
        data->length = 0;
    }
}

void ir_set_code_show_line_numbers(IRComponent* comp, bool show) {
    if (!comp || comp->type != IR_COMPONENT_CODE_BLOCK) return;
    IRCodeBlockData* data = (IRCodeBlockData*)comp->custom_data;
    if (!data) return;
    data->show_line_numbers = show;
}

void ir_set_list_type(IRComponent* comp, IRListType type) {
    if (!comp || comp->type != IR_COMPONENT_LIST) return;
    IRListData* data = (IRListData*)comp->custom_data;
    if (!data) return;
    data->type = type;
}

void ir_set_list_start(IRComponent* comp, uint32_t start) {
    if (!comp || comp->type != IR_COMPONENT_LIST) return;
    IRListData* data = (IRListData*)comp->custom_data;
    if (!data) return;
    data->start = start;
}

void ir_set_list_tight(IRComponent* comp, bool tight) {
    if (!comp || comp->type != IR_COMPONENT_LIST) return;
    IRListData* data = (IRListData*)comp->custom_data;
    if (!data) return;
    data->tight = tight;
}

void ir_set_list_item_number(IRComponent* comp, uint32_t number) {
    if (!comp || comp->type != IR_COMPONENT_LIST_ITEM) return;
    IRListItemData* data = (IRListItemData*)comp->custom_data;
    if (!data) return;
    data->number = number;
}

void ir_set_list_item_marker(IRComponent* comp, const char* marker) {
    if (!comp || comp->type != IR_COMPONENT_LIST_ITEM) return;
    IRListItemData* data = (IRListItemData*)comp->custom_data;
    if (!data) return;

    if (data->marker) free(data->marker);
    data->marker = marker ? strdup(marker) : NULL;
}

void ir_set_list_item_task(IRComponent* comp, bool is_task, bool checked) {
    if (!comp || comp->type != IR_COMPONENT_LIST_ITEM) return;
    IRListItemData* data = (IRListItemData*)comp->custom_data;
    if (!data) return;
    data->is_task_item = is_task;
    data->is_checked = checked;
}

void ir_set_link_url(IRComponent* comp, const char* url) {
    if (!comp || comp->type != IR_COMPONENT_LINK) return;
    IRLinkData* data = (IRLinkData*)comp->custom_data;
    if (!data) return;

    if (data->url) free(data->url);
    data->url = url ? strdup(url) : NULL;
}

void ir_set_link_title(IRComponent* comp, const char* title) {
    if (!comp || comp->type != IR_COMPONENT_LINK) return;
    IRLinkData* data = (IRLinkData*)comp->custom_data;
    if (!data) return;

    if (data->title) free(data->title);
    data->title = title ? strdup(title) : NULL;
}

void ir_set_link_target(IRComponent* comp, const char* target) {
    if (!comp || comp->type != IR_COMPONENT_LINK) return;
    IRLinkData* data = (IRLinkData*)comp->custom_data;
    if (!data) return;

    if (data->target) free(data->target);
    data->target = target ? strdup(target) : NULL;
}

void ir_set_link_rel(IRComponent* comp, const char* rel) {
    if (!comp || comp->type != IR_COMPONENT_LINK) return;
    IRLinkData* data = (IRLinkData*)comp->custom_data;
    if (!data) return;

    if (data->rel) free(data->rel);
    data->rel = rel ? strdup(rel) : NULL;
}

void ir_set_blockquote_depth(IRComponent* comp, uint8_t depth) {
    if (!comp || comp->type != IR_COMPONENT_BLOCKQUOTE) return;
    IRBlockquoteData* data = (IRBlockquoteData*)comp->custom_data;
    if (!data) return;
    data->depth = depth;
}

// Get component type (helper for Lua FFI)
IRComponentType ir_get_component_type(IRComponent* component) {
    if (!component) return IR_COMPONENT_CONTAINER;  // Default fallback
    return component->type;
}

// Get component ID (helper for Lua FFI)
uint32_t ir_get_component_id(IRComponent* component) {
    if (!component) return 0;
    return component->id;
}

// Get child count (helper for Lua FFI)
uint32_t ir_get_child_count(IRComponent* component) {
    if (!component) return 0;
    return component->child_count;
}

// Get child at index (helper for Lua FFI)
IRComponent* ir_get_child_at(IRComponent* component, uint32_t index) {
    if (!component || index >= component->child_count) return NULL;
    return component->children[index];
}

// Set window metadata on IR context
void ir_set_window_metadata(int width, int height, const char* title) {
    if (!g_ir_context) {
        g_ir_context = ir_create_context();
        if (!g_ir_context) return;
    }

    // Create metadata if it doesn't exist
    if (!g_ir_context->metadata) {
        g_ir_context->metadata = (IRMetadata*)calloc(1, sizeof(IRMetadata));
        if (!g_ir_context->metadata) return;
    }

    // Set window dimensions
    g_ir_context->metadata->window_width = width;
    g_ir_context->metadata->window_height = height;

    // Set window title
    if (title) {
        if (g_ir_context->metadata->window_title) {
            free(g_ir_context->metadata->window_title);
        }
        g_ir_context->metadata->window_title = strdup(title);
    }
}
