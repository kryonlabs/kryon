/**
 * @file ir_stylesheet.c
 * @brief Implementation of global stylesheet support for Kryon IR
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ir_stylesheet.h"
#include "ir_core.h"

// ============================================================================
// Internal Helper Functions
// ============================================================================

static char* str_dup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

static char* str_trim(char* s) {
    if (!s) return NULL;
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    char* end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

// ============================================================================
// Style Properties Functions
// ============================================================================

void ir_style_properties_init(IRStyleProperties* props) {
    if (!props) return;
    memset(props, 0, sizeof(IRStyleProperties));
    props->opacity = 1.0f;
    props->flex_shrink = 1;  // CSS default
}

void ir_style_properties_cleanup(IRStyleProperties* props) {
    if (!props) return;
    if (props->font_family) {
        free(props->font_family);
        props->font_family = NULL;
    }
    if (props->background_image) {
        free(props->background_image);
        props->background_image = NULL;
    }
    if (props->grid_template_columns) {
        free(props->grid_template_columns);
        props->grid_template_columns = NULL;
    }
    if (props->grid_template_rows) {
        free(props->grid_template_rows);
        props->grid_template_rows = NULL;
    }
}

// ============================================================================
// Selector Chain Functions
// ============================================================================

static void ir_selector_part_free(IRSelectorPart* part) {
    if (!part) return;
    if (part->name) free(part->name);
    if (part->classes) {
        for (uint32_t i = 0; i < part->class_count; i++) {
            if (part->classes[i]) free(part->classes[i]);
        }
        free(part->classes);
    }
}

void ir_selector_chain_free(IRSelectorChain* chain) {
    if (!chain) return;
    if (chain->parts) {
        for (uint32_t i = 0; i < chain->part_count; i++) {
            ir_selector_part_free(&chain->parts[i]);
        }
        free(chain->parts);
    }
    if (chain->combinators) free(chain->combinators);
    if (chain->original_selector) free(chain->original_selector);
    free(chain);
}

/**
 * Parse a single selector part (e.g., "header", ".container", "#main")
 */
static bool parse_selector_part(const char* str, IRSelectorPart* out_part) {
    if (!str || !out_part) return false;

    memset(out_part, 0, sizeof(IRSelectorPart));

    // Skip leading whitespace
    while (isspace((unsigned char)*str)) str++;
    if (*str == '\0') return false;

    if (*str == '.') {
        // Class selector
        out_part->type = IR_SELECTOR_PART_CLASS;
        str++;  // Skip the dot

        // Handle compound classes (.btn.primary)
        const char* start = str;
        int class_count = 1;
        const char* p = str;
        while (*p) {
            if (*p == '.') class_count++;
            p++;
        }

        if (class_count == 1) {
            // Simple class
            out_part->name = str_dup(str);
        } else {
            // Compound class selector
            out_part->classes = malloc(class_count * sizeof(char*));
            out_part->class_count = 0;

            p = str;
            while (*p) {
                const char* end = p;
                while (*end && *end != '.') end++;

                size_t len = end - p;
                char* class_name = malloc(len + 1);
                memcpy(class_name, p, len);
                class_name[len] = '\0';
                out_part->classes[out_part->class_count++] = class_name;

                if (*end == '.') end++;
                p = end;
            }
            // Use first class as main name
            out_part->name = str_dup(out_part->classes[0]);
        }
    } else if (*str == '#') {
        // ID selector
        out_part->type = IR_SELECTOR_PART_ID;
        out_part->name = str_dup(str + 1);
    } else if (*str == '*') {
        // Universal selector
        out_part->type = IR_SELECTOR_PART_UNIVERSAL;
        out_part->name = str_dup("*");
    } else {
        // Element selector
        out_part->type = IR_SELECTOR_PART_ELEMENT;
        out_part->name = str_dup(str);
    }

    return out_part->name != NULL;
}

IRSelectorChain* ir_selector_parse(const char* selector) {
    if (!selector || !*selector) return NULL;

    IRSelectorChain* chain = calloc(1, sizeof(IRSelectorChain));
    if (!chain) return NULL;

    chain->original_selector = str_dup(selector);

    // Count parts by scanning for combinators
    // This is a simplified parser - assumes well-formed selectors
    int max_parts = 16;
    chain->parts = calloc(max_parts, sizeof(IRSelectorPart));
    chain->combinators = calloc(max_parts - 1, sizeof(IRCombinator));

    char* copy = str_dup(selector);
    char* p = copy;
    char* part_start = p;
    IRCombinator pending_combinator = IR_COMBINATOR_NONE;

    while (*p) {
        // Skip leading whitespace
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        // Check for combinator
        if (*p == '>') {
            pending_combinator = IR_COMBINATOR_CHILD;
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            part_start = p;
            continue;
        } else if (*p == '+') {
            pending_combinator = IR_COMBINATOR_ADJACENT;
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            part_start = p;
            continue;
        } else if (*p == '~') {
            pending_combinator = IR_COMBINATOR_GENERAL;
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            part_start = p;
            continue;
        }

        // Read selector part (until whitespace or combinator)
        part_start = p;
        while (*p && !isspace((unsigned char)*p) && *p != '>' && *p != '+' && *p != '~') {
            p++;
        }

        if (p > part_start) {
            // Extract part
            size_t len = p - part_start;
            char* part_str = malloc(len + 1);
            memcpy(part_str, part_start, len);
            part_str[len] = '\0';

            // Parse and add part
            if (chain->part_count > 0 && pending_combinator == IR_COMBINATOR_NONE) {
                // Whitespace between parts = descendant combinator
                chain->combinators[chain->part_count - 1] = IR_COMBINATOR_DESCENDANT;
            } else if (chain->part_count > 0) {
                chain->combinators[chain->part_count - 1] = pending_combinator;
            }

            if (!parse_selector_part(part_str, &chain->parts[chain->part_count])) {
                free(part_str);
                free(copy);
                ir_selector_chain_free(chain);
                return NULL;
            }
            chain->part_count++;
            pending_combinator = IR_COMBINATOR_NONE;

            free(part_str);
        }

        // Skip whitespace after part
        while (*p && isspace((unsigned char)*p)) {
            if (pending_combinator == IR_COMBINATOR_NONE && chain->part_count > 0) {
                // Mark potential descendant combinator (space)
                pending_combinator = IR_COMBINATOR_DESCENDANT;
            }
            p++;
        }
    }

    free(copy);

    // Calculate specificity
    chain->specificity = ir_selector_specificity(chain);

    return chain;
}

uint32_t ir_selector_specificity(const IRSelectorChain* chain) {
    if (!chain) return 0;

    uint32_t ids = 0;
    uint32_t classes = 0;
    uint32_t elements = 0;

    for (uint32_t i = 0; i < chain->part_count; i++) {
        switch (chain->parts[i].type) {
            case IR_SELECTOR_PART_ID:
                ids++;
                break;
            case IR_SELECTOR_PART_CLASS:
                classes++;
                // Add compound classes
                if (chain->parts[i].class_count > 1) {
                    classes += chain->parts[i].class_count - 1;
                }
                break;
            case IR_SELECTOR_PART_ELEMENT:
                elements++;
                break;
            case IR_SELECTOR_PART_UNIVERSAL:
                // Universal selector has no specificity
                break;
        }
    }

    return ids * 100 + classes * 10 + elements;
}

// ============================================================================
// Selector Matching Functions
// ============================================================================

/**
 * Check if a selector part matches a component
 */
static bool selector_part_matches(const IRSelectorPart* part, IRComponent* component) {
    if (!part || !component) return false;

    switch (part->type) {
        case IR_SELECTOR_PART_UNIVERSAL:
            return true;

        case IR_SELECTOR_PART_ELEMENT:
            // Match against tag name
            if (component->tag && part->name) {
                return strcmp(component->tag, part->name) == 0;
            }
            return false;

        case IR_SELECTOR_PART_CLASS:
            // Match against css_class
            if (component->css_class && part->name) {
                // Check if any class matches
                // Handle space-separated class list
                const char* classes = component->css_class;
                const char* p = classes;
                while (*p) {
                    // Skip whitespace
                    while (*p && isspace((unsigned char)*p)) p++;
                    if (!*p) break;

                    // Find end of class name
                    const char* end = p;
                    while (*end && !isspace((unsigned char)*end)) end++;

                    // Compare
                    size_t len = end - p;
                    if (strlen(part->name) == len && strncmp(part->name, p, len) == 0) {
                        // If compound selector, check all classes
                        if (part->class_count > 1) {
                            bool all_match = true;
                            for (uint32_t i = 0; i < part->class_count; i++) {
                                bool found = false;
                                const char* cp = classes;
                                while (*cp) {
                                    while (*cp && isspace((unsigned char)*cp)) cp++;
                                    if (!*cp) break;
                                    const char* ce = cp;
                                    while (*ce && !isspace((unsigned char)*ce)) ce++;
                                    size_t clen = ce - cp;
                                    if (strlen(part->classes[i]) == clen &&
                                        strncmp(part->classes[i], cp, clen) == 0) {
                                        found = true;
                                        break;
                                    }
                                    cp = ce;
                                }
                                if (!found) {
                                    all_match = false;
                                    break;
                                }
                            }
                            return all_match;
                        }
                        return true;
                    }
                    p = end;
                }
            }
            return false;

        case IR_SELECTOR_PART_ID:
            // TODO: Add ID support to IRComponent if needed
            return false;
    }

    return false;
}

/**
 * Get parent component (requires parent pointer in IRComponent)
 */
static IRComponent* get_parent(IRComponent* component) {
    // TODO: IRComponent needs a parent pointer for proper traversal
    // For now, we'll need to search from root
    return NULL;
}

/**
 * Get previous sibling component
 */
static IRComponent* get_previous_sibling(IRComponent* component, IRComponent* root) {
    // TODO: Implement sibling traversal
    // This requires either a parent pointer or searching from root
    return NULL;
}

/**
 * Helper: Find component in parent's children and return index
 */
static int find_child_index(IRComponent* parent, IRComponent* child) {
    if (!parent || !child) return -1;
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) return i;
    }
    return -1;
}

/**
 * Helper: Check if 'ancestor' is an ancestor of 'component' in tree rooted at 'root'
 */
static bool is_ancestor_of(IRComponent* ancestor, IRComponent* component, IRComponent* root) {
    // Simple tree search - check if component is in ancestor's subtree
    if (!ancestor || !component || ancestor == component) return false;

    for (int i = 0; i < ancestor->child_count; i++) {
        if (ancestor->children[i] == component) return true;
        if (is_ancestor_of(ancestor->children[i], component, root)) return true;
    }
    return false;
}

/**
 * Helper: Find parent of component in tree
 */
static IRComponent* find_parent_in_tree(IRComponent* component, IRComponent* root) {
    if (!component || !root || root == component) return NULL;

    for (int i = 0; i < root->child_count; i++) {
        if (root->children[i] == component) return root;
        IRComponent* parent = find_parent_in_tree(component, root->children[i]);
        if (parent) return parent;
    }
    return NULL;
}

bool ir_selector_matches(const IRSelectorChain* chain, IRComponent* component, IRComponent* root) {
    if (!chain || !component || chain->part_count == 0) return false;

    // Start from the last part of the chain (the target)
    int chain_idx = chain->part_count - 1;
    IRComponent* current = component;

    // Check if target matches last part
    if (!selector_part_matches(&chain->parts[chain_idx], current)) {
        return false;
    }

    // If single-part selector, we're done
    if (chain->part_count == 1) return true;

    // Walk backwards through selector chain
    chain_idx--;

    while (chain_idx >= 0) {
        IRCombinator combinator = chain->combinators[chain_idx];
        IRSelectorPart* part = &chain->parts[chain_idx];

        switch (combinator) {
            case IR_COMBINATOR_DESCENDANT: {
                // Find ANY ancestor that matches
                IRComponent* ancestor = find_parent_in_tree(current, root);
                bool found = false;
                while (ancestor) {
                    if (selector_part_matches(part, ancestor)) {
                        current = ancestor;
                        found = true;
                        break;
                    }
                    ancestor = find_parent_in_tree(ancestor, root);
                }
                if (!found) return false;
                break;
            }

            case IR_COMBINATOR_CHILD: {
                // Must be direct parent
                IRComponent* parent = find_parent_in_tree(current, root);
                if (!parent || !selector_part_matches(part, parent)) {
                    return false;
                }
                current = parent;
                break;
            }

            case IR_COMBINATOR_ADJACENT: {
                // Immediate previous sibling
                IRComponent* parent = find_parent_in_tree(current, root);
                if (!parent) return false;

                int idx = find_child_index(parent, current);
                if (idx <= 0) return false;  // No previous sibling

                IRComponent* prev = parent->children[idx - 1];
                if (!selector_part_matches(part, prev)) {
                    return false;
                }
                current = prev;
                break;
            }

            case IR_COMBINATOR_GENERAL: {
                // Any previous sibling
                IRComponent* parent = find_parent_in_tree(current, root);
                if (!parent) return false;

                int idx = find_child_index(parent, current);
                if (idx <= 0) return false;

                bool found = false;
                for (int i = idx - 1; i >= 0; i--) {
                    if (selector_part_matches(part, parent->children[i])) {
                        current = parent->children[i];
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
                break;
            }

            default:
                return false;
        }

        chain_idx--;
    }

    return true;
}

// ============================================================================
// Stylesheet Management Functions
// ============================================================================

IRStylesheet* ir_stylesheet_create(void) {
    IRStylesheet* stylesheet = calloc(1, sizeof(IRStylesheet));
    if (!stylesheet) return NULL;

    stylesheet->rule_capacity = 32;
    stylesheet->rules = calloc(stylesheet->rule_capacity, sizeof(IRStyleRule));

    stylesheet->variable_capacity = 32;
    stylesheet->variables = calloc(stylesheet->variable_capacity, sizeof(IRCSSVariable));

    return stylesheet;
}

void ir_stylesheet_free(IRStylesheet* stylesheet) {
    if (!stylesheet) return;

    // Free rules
    for (uint32_t i = 0; i < stylesheet->rule_count; i++) {
        ir_selector_chain_free(stylesheet->rules[i].selector);
        ir_style_properties_cleanup(&stylesheet->rules[i].properties);
    }
    free(stylesheet->rules);

    // Free variables
    for (uint32_t i = 0; i < stylesheet->variable_count; i++) {
        if (stylesheet->variables[i].name) free(stylesheet->variables[i].name);
        if (stylesheet->variables[i].value) free(stylesheet->variables[i].value);
    }
    free(stylesheet->variables);

    free(stylesheet);
}

bool ir_stylesheet_add_rule(IRStylesheet* stylesheet, const char* selector,
                            const IRStyleProperties* properties) {
    if (!stylesheet || !selector || !properties) return false;

    // Grow array if needed
    if (stylesheet->rule_count >= stylesheet->rule_capacity) {
        uint32_t new_capacity = stylesheet->rule_capacity * 2;
        IRStyleRule* new_rules = realloc(stylesheet->rules,
                                         new_capacity * sizeof(IRStyleRule));
        if (!new_rules) return false;
        stylesheet->rules = new_rules;
        stylesheet->rule_capacity = new_capacity;
    }

    // Parse selector
    IRSelectorChain* chain = ir_selector_parse(selector);
    if (!chain) return false;

    // Add rule
    IRStyleRule* rule = &stylesheet->rules[stylesheet->rule_count++];
    rule->selector = chain;
    rule->properties = *properties;  // Copy properties

    // Deep copy font_family if present
    if (properties->font_family) {
        rule->properties.font_family = str_dup(properties->font_family);
    }

    // Deep copy background_image if present
    if (properties->background_image) {
        rule->properties.background_image = str_dup(properties->background_image);
    }

    // Deep copy grid_template_columns if present
    if (properties->grid_template_columns) {
        rule->properties.grid_template_columns = str_dup(properties->grid_template_columns);
    }

    // Deep copy grid_template_rows if present
    if (properties->grid_template_rows) {
        rule->properties.grid_template_rows = str_dup(properties->grid_template_rows);
    }

    return true;
}

bool ir_stylesheet_add_variable(IRStylesheet* stylesheet, const char* name, const char* value) {
    if (!stylesheet || !name || !value) return false;

    // Check if variable already exists
    for (uint32_t i = 0; i < stylesheet->variable_count; i++) {
        if (strcmp(stylesheet->variables[i].name, name) == 0) {
            // Update existing
            free(stylesheet->variables[i].value);
            stylesheet->variables[i].value = str_dup(value);
            return true;
        }
    }

    // Grow array if needed
    if (stylesheet->variable_count >= stylesheet->variable_capacity) {
        uint32_t new_capacity = stylesheet->variable_capacity * 2;
        IRCSSVariable* new_vars = realloc(stylesheet->variables,
                                          new_capacity * sizeof(IRCSSVariable));
        if (!new_vars) return false;
        stylesheet->variables = new_vars;
        stylesheet->variable_capacity = new_capacity;
    }

    // Add new variable
    stylesheet->variables[stylesheet->variable_count].name = str_dup(name);
    stylesheet->variables[stylesheet->variable_count].value = str_dup(value);
    stylesheet->variable_count++;

    return true;
}

const char* ir_stylesheet_get_variable(IRStylesheet* stylesheet, const char* name) {
    if (!stylesheet || !name) return NULL;

    for (uint32_t i = 0; i < stylesheet->variable_count; i++) {
        if (strcmp(stylesheet->variables[i].name, name) == 0) {
            return stylesheet->variables[i].value;
        }
    }
    return NULL;
}

// ============================================================================
// Rule Matching and Application
// ============================================================================

/**
 * Compare function for sorting rules by specificity
 */
static int compare_rules_by_specificity(const void* a, const void* b) {
    const IRStyleRule* rule_a = *(const IRStyleRule**)a;
    const IRStyleRule* rule_b = *(const IRStyleRule**)b;
    return (int)rule_a->selector->specificity - (int)rule_b->selector->specificity;
}

void ir_stylesheet_get_matching_rules(IRStylesheet* stylesheet, IRComponent* component,
                                       IRComponent* root, IRStyleRule*** out_rules,
                                       uint32_t* out_count) {
    if (!stylesheet || !component || !out_rules || !out_count) return;

    *out_rules = NULL;
    *out_count = 0;

    // Collect matching rules
    IRStyleRule** matches = calloc(stylesheet->rule_count, sizeof(IRStyleRule*));
    if (!matches) return;

    uint32_t match_count = 0;
    for (uint32_t i = 0; i < stylesheet->rule_count; i++) {
        if (ir_selector_matches(stylesheet->rules[i].selector, component, root)) {
            matches[match_count++] = &stylesheet->rules[i];
        }
    }

    if (match_count == 0) {
        free(matches);
        return;
    }

    // Sort by specificity (ascending - lowest first, so higher specificity overrides)
    qsort(matches, match_count, sizeof(IRStyleRule*), compare_rules_by_specificity);

    *out_rules = matches;
    *out_count = match_count;
}

/**
 * Apply style properties to component's style and layout
 */
void ir_style_properties_apply(const IRStyleProperties* props, IRStyle* style, IRLayout* layout) {
    if (!props) return;

    // Apply to style
    if (style) {
        if (props->set_flags & IR_PROP_BACKGROUND) {
            style->background = props->background;
        }
        if (props->set_flags & IR_PROP_COLOR) {
            style->font.color = props->color;
        }
        if (props->set_flags & IR_PROP_BORDER) {
            style->border.width = props->border_width;
            style->border.color = props->border_color;
        }
        if (props->set_flags & IR_PROP_BORDER_TOP) {
            style->border.width_top = props->border_width_top;
        }
        if (props->set_flags & IR_PROP_BORDER_RIGHT) {
            style->border.width_right = props->border_width_right;
        }
        if (props->set_flags & IR_PROP_BORDER_BOTTOM) {
            style->border.width_bottom = props->border_width_bottom;
        }
        if (props->set_flags & IR_PROP_BORDER_LEFT) {
            style->border.width_left = props->border_width_left;
        }
        if (props->set_flags & IR_PROP_BORDER_RADIUS) {
            style->border.radius = props->border_radius;
        }
        if (props->set_flags & IR_PROP_PADDING) {
            style->padding = props->padding;
        }
        if (props->set_flags & IR_PROP_MARGIN) {
            style->margin = props->margin;
        }
        if (props->set_flags & IR_PROP_FONT_SIZE) {
            style->font.size = props->font_size;
        }
        if (props->set_flags & IR_PROP_FONT_WEIGHT) {
            style->font.weight = props->font_weight;
            style->font.bold = props->font_bold;
        }
        if (props->set_flags & IR_PROP_TEXT_ALIGN) {
            style->font.align = props->text_align;
        }
        if (props->set_flags & IR_PROP_OPACITY) {
            style->opacity = props->opacity;
        }
        if (props->set_flags & IR_PROP_Z_INDEX) {
            style->z_index = props->z_index;
        }
        if (props->set_flags & IR_PROP_WIDTH) {
            style->width = props->width;
        }
        if (props->set_flags & IR_PROP_HEIGHT) {
            style->height = props->height;
        }
    }

    // Apply to layout
    if (layout) {
        if (props->set_flags & IR_PROP_DISPLAY) {
            layout->mode = props->display;
            layout->display_explicit = props->display_explicit;
        }
        if (props->set_flags & IR_PROP_FLEX_DIRECTION) {
            layout->flex.direction = props->flex_direction;
        }
        if (props->set_flags & IR_PROP_FLEX_WRAP) {
            layout->flex.wrap = props->flex_wrap;
        }
        if (props->set_flags & IR_PROP_JUSTIFY_CONTENT) {
            layout->flex.justify_content = props->justify_content;
        }
        if (props->set_flags & IR_PROP_ALIGN_ITEMS) {
            layout->flex.cross_axis = props->align_items;
        }
        if (props->set_flags & IR_PROP_GAP) {
            layout->flex.gap = props->gap;
        }
        if (props->set_flags & IR_PROP_FLEX_GROW) {
            layout->flex.grow = props->flex_grow;
        }
        if (props->set_flags & IR_PROP_FLEX_SHRINK) {
            layout->flex.shrink = props->flex_shrink;
        }
        if (props->set_flags & IR_PROP_MIN_WIDTH) {
            layout->min_width = props->min_width;
        }
        if (props->set_flags & IR_PROP_MAX_WIDTH) {
            layout->max_width = props->max_width;
        }
        if (props->set_flags & IR_PROP_MIN_HEIGHT) {
            layout->min_height = props->min_height;
        }
        if (props->set_flags & IR_PROP_MAX_HEIGHT) {
            layout->max_height = props->max_height;
        }
    }
}

/**
 * Recursively apply stylesheet to component tree
 */
static void apply_stylesheet_recursive(IRStylesheet* stylesheet, IRComponent* component,
                                       IRComponent* root) {
    if (!stylesheet || !component) return;

    // Get matching rules for this component
    IRStyleRule** rules = NULL;
    uint32_t rule_count = 0;
    ir_stylesheet_get_matching_rules(stylesheet, component, root, &rules, &rule_count);

    // Apply rules in specificity order (already sorted)
    for (uint32_t i = 0; i < rule_count; i++) {
        ir_style_properties_apply(&rules[i]->properties, component->style, component->layout);
    }

    if (rules) free(rules);

    // Recurse to children
    for (int i = 0; i < component->child_count; i++) {
        apply_stylesheet_recursive(stylesheet, component->children[i], root);
    }
}

void ir_stylesheet_apply_to_tree(IRStylesheet* stylesheet, IRComponent* root) {
    if (!stylesheet || !root) return;
    apply_stylesheet_recursive(stylesheet, root, root);
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* ir_combinator_to_string(IRCombinator combinator) {
    switch (combinator) {
        case IR_COMBINATOR_NONE: return "none";
        case IR_COMBINATOR_DESCENDANT: return "descendant";
        case IR_COMBINATOR_CHILD: return "child";
        case IR_COMBINATOR_ADJACENT: return "adjacent";
        case IR_COMBINATOR_GENERAL: return "general";
        default: return "unknown";
    }
}

const char* ir_selector_part_type_to_string(IRSelectorPartType type) {
    switch (type) {
        case IR_SELECTOR_PART_ELEMENT: return "element";
        case IR_SELECTOR_PART_CLASS: return "class";
        case IR_SELECTOR_PART_ID: return "id";
        case IR_SELECTOR_PART_UNIVERSAL: return "universal";
        default: return "unknown";
    }
}
