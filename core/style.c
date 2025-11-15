#include "include/kryon.h"
#include <string.h>

// ============================================================================
// Style System Implementation
// ============================================================================

// Style property types
typedef enum {
    KRYON_STYLE_PROP_COLOR = 0,
    KRYON_STYLE_PROP_BACKGROUND_COLOR,
    KRYON_STYLE_PROP_WIDTH,
    KRYON_STYLE_PROP_HEIGHT,
    KRYON_STYLE_PROP_MARGIN,
    KRYON_STYLE_PROP_PADDING,
    KRYON_STYLE_PROP_FONT_SIZE,
    KRYON_STYLE_PROP_TEXT_COLOR,
    KRYON_STYLE_PROP_BORDER_WIDTH,
    KRYON_STYLE_PROP_BORDER_COLOR,
    KRYON_STYLE_PROP_VISIBLE,
    KRYON_STYLE_PROP_FLEX_GROW,
    KRYON_STYLE_PROP_FLEX_SHRINK,
    KRYON_STYLE_PROP_ALIGN_SELF,
    KRYON_STYLE_PROP_Z_INDEX,
    KRYON_STYLE_PROP_COUNT
} kryon_style_prop_type_t;

// Style property value - implementation
struct kryon_style_prop {
    kryon_style_prop_type_t type;
    union {
        uint32_t color_val;
        kryon_fp_t fp_val;
        uint8_t uint8_val;
        uint16_t uint16_val;
        bool bool_val;
    } value;
};

// Style rule - implementation
struct kryon_style_rule {
    kryon_style_prop_t properties[KRYON_STYLE_PROP_COUNT];
    uint8_t prop_count;
    uint8_t specificity;  // CSS specificity-inspired value
    bool important;       // !important flag
};

// Style selector
typedef enum {
    KRYON_SELECTOR_TYPE = 0,     // element type (e.g., button)
    KRYON_SELECTOR_ID,           // element id (e.g., #submit-btn)
    KRYON_SELECTOR_CLASS,        // element class (e.g., .primary)
    KRYON_SELECTOR_PSEUDO,       // pseudo-class (e.g., :hover)
    KRYON_SELECTOR_ATTRIBUTE     // attribute (e.g., [disabled])
} kryon_selector_type_t;

typedef struct {
    kryon_selector_type_t type;
    const char* value;
    uint8_t specificity;
} kryon_selector_t;

// Style selector group (for complex selectors) - implementation
struct kryon_selector_group {
    kryon_selector_t selectors[8];  // Support up to 8 selectors per rule
    uint8_t selector_count;
    uint8_t combined_specificity;
};

// Complete style rule with selector - implementation
typedef struct {
    struct kryon_selector_group selector;
    struct kryon_style_rule rule;
} kryon_style_definition_t;

// StyleSheet
#define KRYON_MAX_STYLES 128
#define KRYON_MAX_STYLE_DEFS 512

typedef struct {
    kryon_style_definition_t definitions[KRYON_MAX_STYLE_DEFS];
    uint16_t definition_count;
    bool dirty;  // Cache invalidation flag
} kryon_stylesheet_t;

// Global stylesheet instance
static kryon_stylesheet_t global_stylesheet = {0};

// ============================================================================
// Style Property Management
// ============================================================================

static kryon_style_prop_t create_style_prop_color(kryon_style_prop_type_t type, uint32_t color) {
    kryon_style_prop_t prop = {0};
    prop.type = type;
    prop.value.color_val = color;
    return prop;
}

static kryon_style_prop_t create_style_prop_fp(kryon_style_prop_type_t type, kryon_fp_t value) {
    kryon_style_prop_t prop = {0};
    prop.type = type;
    prop.value.fp_val = value;
    return prop;
}

static kryon_style_prop_t create_style_prop_uint8(kryon_style_prop_type_t type, uint8_t value) {
    kryon_style_prop_t prop = {0};
    prop.type = type;
    prop.value.uint8_val = value;
    return prop;
}

static kryon_style_prop_t create_style_prop_uint16(kryon_style_prop_type_t type, uint16_t value) {
    kryon_style_prop_t prop = {0};
    prop.type = type;
    prop.value.uint16_val = value;
    return prop;
}

static kryon_style_prop_t create_style_prop_bool(kryon_style_prop_type_t type, bool value) {
    kryon_style_prop_t prop = {0};
    prop.type = type;
    prop.value.bool_val = value;
    return prop;
}

// ============================================================================
// Selector Matching
// ============================================================================

static bool selector_matches_component(const kryon_selector_t* selector, kryon_component_t* component) {
    if (selector == NULL || component == NULL) return false;

    switch (selector->type) {
        case KRYON_SELECTOR_TYPE:
            // This would require component type information
            // For now, return true as a placeholder
            return true;

        case KRYON_SELECTOR_ID:
            // This would require component ID information
            // For now, return false as placeholder
            return false;

        case KRYON_SELECTOR_CLASS:
            // This would require component class information
            // For now, return false as placeholder
            return false;

        case KRYON_SELECTOR_PSEUDO:
            // Handle pseudo-classes like :hover, :active
            if (strcmp(selector->value, "hover") == 0) {
                // Check if component is in hover state
                // This would need state tracking
                return false;
            }
            return false;

        case KRYON_SELECTOR_ATTRIBUTE:
            // Handle attribute selectors like [disabled]
            if (strcmp(selector->value, "disabled") == 0) {
                // Check if component is disabled
                // This would need component state
                return false;
            }
            return false;

        default:
            return false;
    }
}

static bool selector_group_matches(const kryon_selector_group_t* group, kryon_component_t* component) {
    if (group == NULL || component == NULL) return false;

    // All selectors in the group must match
    for (uint8_t i = 0; i < group->selector_count; i++) {
        if (!selector_matches_component(&group->selectors[i], component)) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Style Application
// ============================================================================

static void apply_style_property(kryon_component_t* component, const kryon_style_prop_t* prop) {
    if (component == NULL || prop == NULL) return;

    switch (prop->type) {
        case KRYON_STYLE_PROP_COLOR:
            // Set text color (would need component-specific handling)
            break;

        case KRYON_STYLE_PROP_BACKGROUND_COLOR:
            kryon_component_set_background_color(component, prop->value.color_val);
            break;

        case KRYON_STYLE_PROP_WIDTH:
            kryon_component_set_bounds(component, component->x, component->y, prop->value.fp_val, component->height);
            break;

        case KRYON_STYLE_PROP_HEIGHT:
            kryon_component_set_bounds(component, component->x, component->y, component->width, prop->value.fp_val);
            break;

        case KRYON_STYLE_PROP_MARGIN:
            // This would need to be split into top, right, bottom, left
            kryon_component_set_margin(component, prop->value.uint8_val, prop->value.uint8_val,
                                     prop->value.uint8_val, prop->value.uint8_val);
            break;

        case KRYON_STYLE_PROP_PADDING:
            // This would need to be split into top, right, bottom, left
            kryon_component_set_padding(component, prop->value.uint8_val, prop->value.uint8_val,
                                      prop->value.uint8_val, prop->value.uint8_val);
            break;

        case KRYON_STYLE_PROP_VISIBLE:
            kryon_component_set_visible(component, prop->value.bool_val);
            break;

        case KRYON_STYLE_PROP_Z_INDEX:
            component->z_index = prop->value.uint8_val;
            break;

        default:
            // Handle other property types
            break;
    }
}

// ============================================================================
// Style Resolution Algorithm
// ============================================================================

static void resolve_component_styles(kryon_component_t* component) {
    if (component == NULL) return;

    // Find all matching style rules
    kryon_style_definition_t* matching_rules[16];
    uint8_t matching_count = 0;

    for (uint16_t i = 0; i < global_stylesheet.definition_count && matching_count < 16; i++) {
        if (selector_group_matches(&global_stylesheet.definitions[i].selector, component)) {
            matching_rules[matching_count++] = &global_stylesheet.definitions[i];
        }
    }

    // Sort rules by specificity (higher specificity last, so they override)
    for (uint8_t i = 0; i < matching_count - 1; i++) {
        for (uint8_t j = i + 1; j < matching_count; j++) {
            if (matching_rules[i]->selector.combined_specificity > matching_rules[j]->selector.combined_specificity) {
                kryon_style_definition_t* temp = matching_rules[i];
                matching_rules[i] = matching_rules[j];
                matching_rules[j] = temp;
            }
        }
    }

    // Apply styles in order (less specific first, more specific overrides)
    for (uint8_t i = 0; i < matching_count; i++) {
        const kryon_style_rule_t* rule = &matching_rules[i]->rule;
        for (uint8_t j = 0; j < rule->prop_count; j++) {
            apply_style_property(component, &rule->properties[j]);
        }
    }
}

// ============================================================================
// Public API Functions
// ============================================================================

void kryon_style_init(void) {
    memset(&global_stylesheet, 0, sizeof(kryon_stylesheet_t));
}

void kryon_style_clear(void) {
    memset(&global_stylesheet, 0, sizeof(kryon_stylesheet_t));
}

uint16_t kryon_style_add_rule(const kryon_selector_group_t* selector, const kryon_style_rule_t* rule) {
    if (selector == NULL || rule == NULL ||
        global_stylesheet.definition_count >= KRYON_MAX_STYLE_DEFS) {
        return 0; // Invalid index
    }

    uint16_t index = global_stylesheet.definition_count++;

    // Copy selector and rule
    global_stylesheet.definitions[index].selector = *selector;
    global_stylesheet.definitions[index].rule = *rule;

    // Mark stylesheet as dirty
    global_stylesheet.dirty = true;

    return index + 1; // Return 1-based index
}

void kryon_style_remove_rule(uint16_t rule_index) {
    if (rule_index == 0 || rule_index > global_stylesheet.definition_count) {
        return; // Invalid index
    }

    // Remove rule by shifting remaining rules
    for (uint16_t i = rule_index - 1; i < global_stylesheet.definition_count - 1; i++) {
        global_stylesheet.definitions[i] = global_stylesheet.definitions[i + 1];
    }

    global_stylesheet.definition_count--;
    global_stylesheet.dirty = true;
}

void kryon_style_resolve_tree(kryon_component_t* root) {
    if (root == NULL) return;

    // Resolve styles for this component
    resolve_component_styles(root);

    // Recursively resolve styles for children
    for (uint8_t i = 0; i < root->child_count; i++) {
        kryon_style_resolve_tree(root->children[i]);
    }
}

void kryon_style_resolve_component(kryon_component_t* component) {
    resolve_component_styles(component);
}

bool kryon_style_is_dirty(void) {
    return global_stylesheet.dirty;
}

void kryon_style_mark_clean(void) {
    global_stylesheet.dirty = false;
}

// ============================================================================
// Style Rule Creation Helpers
// ============================================================================

kryon_style_rule_t kryon_style_create_rule(void) {
    kryon_style_rule_t rule = {0};
    return rule;
}

void kryon_style_add_property(kryon_style_rule_t* rule, const kryon_style_prop_t* prop) {
    if (rule == NULL || prop == NULL || rule->prop_count >= KRYON_STYLE_PROP_COUNT) {
        return;
    }

    rule->properties[rule->prop_count++] = *prop;
}

// Property creation functions
kryon_style_prop_t kryon_style_color(uint32_t color) {
    kryon_style_prop_t prop = create_style_prop_color(KRYON_STYLE_PROP_COLOR, color);
    return prop;
}

kryon_style_prop_t kryon_style_background_color(uint32_t color) {
    kryon_style_prop_t prop = create_style_prop_color(KRYON_STYLE_PROP_BACKGROUND_COLOR, color);
    return prop;
}

kryon_style_prop_t kryon_style_width(kryon_fp_t width) {
    kryon_style_prop_t prop = create_style_prop_fp(KRYON_STYLE_PROP_WIDTH, width);
    return prop;
}

kryon_style_prop_t kryon_style_height(kryon_fp_t height) {
    kryon_style_prop_t prop = create_style_prop_fp(KRYON_STYLE_PROP_HEIGHT, height);
    return prop;
}

kryon_style_prop_t kryon_style_margin(uint8_t margin) {
    kryon_style_prop_t prop = create_style_prop_uint8(KRYON_STYLE_PROP_MARGIN, margin);
    return prop;
}

kryon_style_prop_t kryon_style_padding(uint8_t padding) {
    kryon_style_prop_t prop = create_style_prop_uint8(KRYON_STYLE_PROP_PADDING, padding);
    return prop;
}

kryon_style_prop_t kryon_style_visible(bool visible) {
    kryon_style_prop_t prop = create_style_prop_bool(KRYON_STYLE_PROP_VISIBLE, visible);
    return prop;
}

kryon_style_prop_t kryon_style_z_index(uint16_t z_index) {
    kryon_style_prop_t prop = create_style_prop_uint16(KRYON_STYLE_PROP_Z_INDEX, z_index);
    return prop;
}

// ============================================================================
// Selector Creation Helpers
// ============================================================================

kryon_selector_group_t kryon_style_selector_type(const char* type) {
    kryon_selector_group_t group = {0};

    group.selectors[0].type = KRYON_SELECTOR_TYPE;
    group.selectors[0].value = type;
    group.selectors[0].specificity = 1;
    group.selector_count = 1;
    group.combined_specificity = 1;

    return group;
}

kryon_selector_group_t kryon_style_selector_class(const char* class_name) {
    kryon_selector_group_t group = {0};

    group.selectors[0].type = KRYON_SELECTOR_CLASS;
    group.selectors[0].value = class_name;
    group.selectors[0].specificity = 10;
    group.selector_count = 1;
    group.combined_specificity = 10;

    return group;
}

kryon_selector_group_t kryon_style_selector_id(const char* id) {
    kryon_selector_group_t group = {0};

    group.selectors[0].type = KRYON_SELECTOR_ID;
    group.selectors[0].value = id;
    group.selectors[0].specificity = 100;
    group.selector_count = 1;
    group.combined_specificity = 100;

    return group;
}

kryon_selector_group_t kryon_style_selector_pseudo(const char* pseudo) {
    kryon_selector_group_t group = {0};

    group.selectors[0].type = KRYON_SELECTOR_PSEUDO;
    group.selectors[0].value = pseudo;
    group.selectors[0].specificity = 10;
    group.selector_count = 1;
    group.combined_specificity = 10;

    return group;
}

// ============================================================================
// Convenience Functions for Common Styles
// ============================================================================

uint16_t kryon_style_add_button_style(const char* class_name, uint32_t bg_color, uint32_t text_color,
                                     uint8_t padding, uint16_t width, uint16_t height) {
    // Create selector
    kryon_selector_group_t selector = kryon_style_selector_class(class_name);

    // Create rule
    kryon_style_rule_t rule = kryon_style_create_rule();

    // Add properties
    kryon_style_prop_t prop_bg = kryon_style_background_color(bg_color);
    kryon_style_add_property(&rule, &prop_bg);

    kryon_style_prop_t prop_text = kryon_style_color(text_color);
    kryon_style_add_property(&rule, &prop_text);

    kryon_style_prop_t prop_padding = kryon_style_padding(padding);
    kryon_style_add_property(&rule, &prop_padding);

    kryon_style_prop_t prop_width = kryon_style_width(KRYON_FP_FROM_INT(width));
    kryon_style_add_property(&rule, &prop_width);

    kryon_style_prop_t prop_height = kryon_style_height(KRYON_FP_FROM_INT(height));
    kryon_style_add_property(&rule, &prop_height);

    kryon_style_prop_t prop_visible = kryon_style_visible(true);
    kryon_style_add_property(&rule, &prop_visible);

    return kryon_style_add_rule(&selector, &rule);
}

uint16_t kryon_style_add_container_style(const char* class_name, uint32_t bg_color,
                                        uint8_t margin, uint8_t padding) {
    // Create selector
    kryon_selector_group_t selector = kryon_style_selector_class(class_name);

    // Create rule
    kryon_style_rule_t rule = kryon_style_create_rule();

    // Add properties
    kryon_style_prop_t prop_bg = kryon_style_background_color(bg_color);
    kryon_style_add_property(&rule, &prop_bg);

    kryon_style_prop_t prop_margin = kryon_style_margin(margin);
    kryon_style_add_property(&rule, &prop_margin);

    kryon_style_prop_t prop_padding = kryon_style_padding(padding);
    kryon_style_add_property(&rule, &prop_padding);

    kryon_style_prop_t prop_visible = kryon_style_visible(true);
    kryon_style_add_property(&rule, &prop_visible);

    return kryon_style_add_rule(&selector, &rule);
}

uint16_t kryon_style_add_text_style(const char* class_name, uint32_t text_color,
                                   uint16_t font_size) {
    // Create selector
    kryon_selector_group_t selector = kryon_style_selector_class(class_name);

    // Create rule
    kryon_style_rule_t rule = kryon_style_create_rule();

    // Add properties
    kryon_style_prop_t prop_color = kryon_style_color(text_color);
    kryon_style_add_property(&rule, &prop_color);

    kryon_style_prop_t prop_visible = kryon_style_visible(true);
    kryon_style_add_property(&rule, &prop_visible);

    return kryon_style_add_rule(&selector, &rule);
}