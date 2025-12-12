#define _GNU_SOURCE
#include "ir_builder.h"
#include "ir_memory.h"
#include "ir_hashmap.h"
#include "ir_animation.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

// Global IR context
IRContext* g_ir_context = NULL;

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

// Helper function to mark component dirty when style changes
static void mark_style_dirty(IRComponent* component) {
    if (!component) return;
    ir_layout_invalidate_cache(component);
    component->dirty_flags |= IR_DIRTY_STYLE | IR_DIRTY_LAYOUT;
}

// Type system helper functions
const char* ir_component_type_to_string(IRComponentType type) {
    switch (type) {
        case IR_COMPONENT_CONTAINER: return "Container";
        case IR_COMPONENT_TEXT: return "Text";
        case IR_COMPONENT_BUTTON: return "Button";
        case IR_COMPONENT_INPUT: return "Input";
        case IR_COMPONENT_CHECKBOX: return "Checkbox";
        case IR_COMPONENT_ROW: return "Row";
        case IR_COMPONENT_COLUMN: return "Column";
        case IR_COMPONENT_CENTER: return "Center";
        case IR_COMPONENT_IMAGE: return "Image";
        case IR_COMPONENT_CANVAS: return "Canvas";
        case IR_COMPONENT_DROPDOWN: return "Dropdown";
        case IR_COMPONENT_MARKDOWN: return "Markdown";
        case IR_COMPONENT_TAB_GROUP: return "TabGroup";
        case IR_COMPONENT_TAB_BAR: return "TabBar";
        case IR_COMPONENT_TAB: return "Tab";
        case IR_COMPONENT_TAB_CONTENT: return "TabContent";
        case IR_COMPONENT_TAB_PANEL: return "TabPanel";
        case IR_COMPONENT_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

const char* ir_event_type_to_string(IREventType type) {
    switch (type) {
        case IR_EVENT_CLICK: return "Click";
        case IR_EVENT_HOVER: return "Hover";
        case IR_EVENT_FOCUS: return "Focus";
        case IR_EVENT_BLUR: return "Blur";
        case IR_EVENT_KEY: return "Key";
        case IR_EVENT_SCROLL: return "Scroll";
        case IR_EVENT_TIMER: return "Timer";
        case IR_EVENT_CUSTOM: return "Custom";
        default: return "Unknown";
    }
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
            ir_add_child(state->tab_content, state->panels[index]);
            // Notify Nim that this panel was added - resets cleanup tracking
            if (nimOnComponentAdded) {
                nimOnComponentAdded(state->panels[index]);
            }
        }

        // Invalidate layout so renderer recalculates with new child set
        state->tab_content->rendered_bounds.valid = false;
    }

    // Invalidate parent/group/root bounds to force relayout
    if (state->group) {
        state->group->rendered_bounds.valid = false;
    }
    if (g_ir_context && g_ir_context->root) {
        g_ir_context->root->rendered_bounds.valid = false;
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
    if (id == 0 && g_ir_context) {
        component->id = g_ir_context->next_component_id++;
    } else {
        component->id = id;
    }

    // Add to hash map for fast lookups
    if (g_ir_context && g_ir_context->component_map) {
        ir_map_insert(g_ir_context->component_map, component);
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

    // Free strings
    if (component->tag) free(component->tag);
    if (component->text_content) free(component->text_content);
    if (component->custom_data) free(component->custom_data);

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
    ir_layout_invalidate_cache(parent);
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
    ir_layout_invalidate_cache(parent);
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
void ir_set_width(IRStyle* style, IRDimensionType type, float value) {
    if (!style) return;
    style->width.type = type;
    style->width.value = value;
}

void ir_set_height(IRStyle* style, IRDimensionType type, float value) {
    if (!style) return;
    style->height.type = type;
    style->height.value = value;
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

void ir_set_margin(IRStyle* style, float top, float right, float bottom, float left) {
    if (!style) return;
    style->margin.top = top;
    style->margin.right = right;
    style->margin.bottom = bottom;
    style->margin.left = left;
}

void ir_set_padding(IRStyle* style, float top, float right, float bottom, float left) {
    if (!style) return;
    style->padding.top = top;
    style->padding.right = right;
    style->padding.bottom = bottom;
    style->padding.left = left;
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
    layout->flex.main_axis = IR_ALIGNMENT_START;
    layout->flex.cross_axis = IR_ALIGNMENT_START;
    layout->flex.justify_content = IR_ALIGNMENT_START;

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
    layout->flex.main_axis = main_axis;
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

    return event;
}

void ir_destroy_event(IREvent* event) {
    if (!event) return;

    if (event->logic_id) free(event->logic_id);
    if (event->handler_data) free(event->handler_data);

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

    // Mark component dirty - content changed
    ir_layout_invalidate_cache(component);
    component->dirty_flags |= IR_DIRTY_CONTENT | IR_DIRTY_LAYOUT;
}

void ir_set_custom_data(IRComponent* component, const char* data) {
    if (!component) return;

    if (component->custom_data) {
        free(component->custom_data);
    }
    component->custom_data = data ? strdup(data) : NULL;
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
    layout->flex.main_axis = IR_ALIGNMENT_START;
    layout->flex.cross_axis = IR_ALIGNMENT_CENTER;
    return component;
}

IRComponent* ir_column(void) {
    IRComponent* component = ir_create_component(IR_COMPONENT_COLUMN);
    IRLayout* layout = ir_get_layout(component);
    layout->flex.main_axis = IR_ALIGNMENT_CENTER;
    layout->flex.cross_axis = IR_ALIGNMENT_START;
    return component;
}

IRComponent* ir_center(void) {
    IRComponent* component = ir_create_component(IR_COMPONENT_CENTER);
    IRLayout* layout = ir_get_layout(component);
    layout->flex.main_axis = IR_ALIGNMENT_CENTER;
    layout->flex.cross_axis = IR_ALIGNMENT_CENTER;
    layout->flex.justify_content = IR_ALIGNMENT_CENTER;
    return component;
}

// Dimension Helpers
IRDimension ir_dimension_px(float value) {
    IRDimension dim = { IR_DIMENSION_PX, value };
    return dim;
}

IRDimension ir_dimension_percent(float value) {
    IRDimension dim = { IR_DIMENSION_PERCENT, value };
    return dim;
}

IRDimension ir_dimension_auto(void) {
    IRDimension dim = { IR_DIMENSION_AUTO, 0.0f };
    return dim;
}

IRDimension ir_dimension_flex(float value) {
    IRDimension dim = { IR_DIMENSION_FLEX, value };
    return dim;
}

// Color Helpers
IRColor ir_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return IR_COLOR_RGBA(r, g, b, 255);
}

IRColor ir_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return IR_COLOR_RGBA(r, g, b, a);
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
    if (!root || !ir_is_point_in_component(root, x, y)) {
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
    layout->flex.main_axis = align;
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

// Helper animations
IRAnimation* ir_animation_fade_in_out(float duration) {
    IRAnimation* anim = ir_animation_create_keyframe("fadeInOut", duration);
    if (!anim) return NULL;

    IRKeyframe* kf0 = ir_animation_add_keyframe(anim, 0.0f);
    ir_keyframe_set_property(kf0, IR_ANIM_PROP_OPACITY, 0.0f);

    IRKeyframe* kf1 = ir_animation_add_keyframe(anim, 0.5f);
    ir_keyframe_set_property(kf1, IR_ANIM_PROP_OPACITY, 1.0f);

    IRKeyframe* kf2 = ir_animation_add_keyframe(anim, 1.0f);
    ir_keyframe_set_property(kf2, IR_ANIM_PROP_OPACITY, 0.0f);

    return anim;
}

IRAnimation* ir_animation_pulse(float duration) {
    IRAnimation* anim = ir_animation_create_keyframe("pulse", duration);
    if (!anim) return NULL;

    IRKeyframe* kf0 = ir_animation_add_keyframe(anim, 0.0f);
    ir_keyframe_set_property(kf0, IR_ANIM_PROP_SCALE_X, 1.0f);
    ir_keyframe_set_property(kf0, IR_ANIM_PROP_SCALE_Y, 1.0f);

    IRKeyframe* kf1 = ir_animation_add_keyframe(anim, 0.5f);
    ir_keyframe_set_property(kf1, IR_ANIM_PROP_SCALE_X, 1.1f);
    ir_keyframe_set_property(kf1, IR_ANIM_PROP_SCALE_Y, 1.1f);
    ir_keyframe_set_easing(kf1, IR_EASING_EASE_IN_OUT);

    IRKeyframe* kf2 = ir_animation_add_keyframe(anim, 1.0f);
    ir_keyframe_set_property(kf2, IR_ANIM_PROP_SCALE_X, 1.0f);
    ir_keyframe_set_property(kf2, IR_ANIM_PROP_SCALE_Y, 1.0f);

    ir_animation_set_iterations(anim, -1);  // Loop forever
    return anim;
}

IRAnimation* ir_animation_slide_in_left(float duration) {
    IRAnimation* anim = ir_animation_create_keyframe("slideInLeft", duration);
    if (!anim) return NULL;

    IRKeyframe* kf0 = ir_animation_add_keyframe(anim, 0.0f);
    ir_keyframe_set_property(kf0, IR_ANIM_PROP_TRANSLATE_X, -300.0f);

    IRKeyframe* kf1 = ir_animation_add_keyframe(anim, 1.0f);
    ir_keyframe_set_property(kf1, IR_ANIM_PROP_TRANSLATE_X, 0.0f);
    ir_keyframe_set_easing(kf1, IR_EASING_EASE_OUT);

    return anim;
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
