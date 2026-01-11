#define _GNU_SOURCE
#include "ir_builder.h"
#include "ir_color_utils.h"
#include "ir_component_factory.h"
#include "ir_memory.h"
#include "ir_hashmap.h"
#include "ir_animation.h"
#include "ir_plugin.h"
#include "ir_component_handler.h"
#include "components/ir_component_registry.h"
#include "cJSON.h"
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
void ir_destroy_handler_source(IRHandlerSource* source);

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

// Case-insensitive string comparison (used by ir_parse_direction and ir_parse_unicode_bidi)
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

// ir_color_named moved to ir_color_utils.c
// TabGroup functions moved to ir_tabgroup.c

// Helper function to mark component dirty when style changes
static void mark_style_dirty(IRComponent* component) {
    if (!component) return;
    ir_layout_mark_dirty(component);
    component->dirty_flags |= IR_DIRTY_STYLE | IR_DIRTY_LAYOUT;
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

// Component Creation functions moved to ir_component_factory.c
// Tree Structure Management functions moved to ir_component_factory.c
// Style Management functions moved to ir_style_builder.c

// Layout Management functions moved to ir_layout_builder.c
// Event Management functions moved to ir_event_builder.c

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

// Content Management functions moved to ir_component_factory.c
// ir_set_text_content
// ir_set_custom_data
// ir_set_tag
// ir_set_each_source

// Module Reference Management functions moved to ir_module_refs.c

// Checkbox state helpers moved to ir_component_factory.c
// Dropdown state helpers moved to ir_component_factory.c
// ir_set_tag moved to ir_component_factory.c

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

// Color Helpers moved to ir_color_utils.c
// ir_color_rgb, ir_color_transparent, ir_color_named

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

// Layout alignment functions moved to ir_layout_builder.c
// BiDi Direction Property Helpers moved to ir_layout_builder.c

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

// Grid Layout functions moved to ir_layout_builder.c
// Container Queries functions moved to ir_layout_builder.c

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
