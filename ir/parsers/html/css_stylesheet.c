#define _POSIX_C_SOURCE 200809L
#include "css_stylesheet.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================================
// Helper Functions
// ============================================================================

static char* trim_whitespace_alloc(const char* str) {
    if (!str) return NULL;

    // Skip leading whitespace
    while (*str && isspace(*str)) str++;

    if (*str == '\0') return strdup("");

    // Find end
    const char* end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) end--;

    // Allocate and copy
    size_t len = end - str + 1;
    char* result = malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, str, len);
    result[len] = '\0';
    return result;
}

static bool starts_with(const char* str, const char* prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

// ============================================================================
// Stylesheet Parsing
// ============================================================================

// Parse a single CSS rule block (the properties between { and })
static CSSProperty* parse_rule_block(const char* block, uint32_t* count) {
    // Reuse existing inline CSS parser
    return ir_css_parse_inline(block, count);
}

// ============================================================================
// CSS Variable Functions
// ============================================================================

// Add a CSS variable to the stylesheet
void ir_css_add_variable(CSSStylesheet* stylesheet, const char* name, const char* value) {
    if (!stylesheet || !name || !value) return;

    // Grow array if needed
    if (stylesheet->variable_count >= stylesheet->variable_capacity) {
        uint32_t new_cap = stylesheet->variable_capacity == 0 ? 16 : stylesheet->variable_capacity * 2;
        CSSVariable* new_vars = realloc(stylesheet->variables, new_cap * sizeof(CSSVariable));
        if (!new_vars) return;
        stylesheet->variables = new_vars;
        stylesheet->variable_capacity = new_cap;
    }

    // Check if variable already exists (update it)
    for (uint32_t i = 0; i < stylesheet->variable_count; i++) {
        if (strcmp(stylesheet->variables[i].name, name) == 0) {
            free(stylesheet->variables[i].value);
            stylesheet->variables[i].value = strdup(value);
            return;
        }
    }

    // Add new variable
    CSSVariable* var = &stylesheet->variables[stylesheet->variable_count++];
    var->name = strdup(name);
    var->value = strdup(value);
}

// Get a CSS variable value by name (without -- prefix)
const char* ir_css_get_variable(CSSStylesheet* stylesheet, const char* name) {
    if (!stylesheet || !name) return NULL;

    for (uint32_t i = 0; i < stylesheet->variable_count; i++) {
        if (strcmp(stylesheet->variables[i].name, name) == 0) {
            return stylesheet->variables[i].value;
        }
    }
    return NULL;
}

// Resolve var(--name) references in a value string
// Returns newly allocated string with variables resolved, or NULL if no var() found
char* ir_css_resolve_variables(CSSStylesheet* stylesheet, const char* value) {
    if (!stylesheet || !value) return NULL;

    // Check if value contains var(
    const char* var_start = strstr(value, "var(");
    if (!var_start) return NULL;

    // Build resolved string
    size_t result_cap = strlen(value) * 2;
    char* result = malloc(result_cap);
    if (!result) return NULL;
    size_t result_len = 0;
    result[0] = '\0';

    const char* p = value;
    while (*p) {
        var_start = strstr(p, "var(");
        if (!var_start) {
            // Copy rest of string
            size_t rest_len = strlen(p);
            if (result_len + rest_len + 1 > result_cap) {
                result_cap = result_len + rest_len + 64;
                result = realloc(result, result_cap);
            }
            strcpy(result + result_len, p);
            result_len += rest_len;
            break;
        }

        // Copy text before var(
        size_t prefix_len = var_start - p;
        if (result_len + prefix_len + 1 > result_cap) {
            result_cap = result_len + prefix_len + 256;
            result = realloc(result, result_cap);
        }
        memcpy(result + result_len, p, prefix_len);
        result_len += prefix_len;
        result[result_len] = '\0';

        // Parse var(--name) or var(--name, fallback)
        const char* name_start = var_start + 4; // Skip "var("

        // Skip whitespace
        while (*name_start && isspace(*name_start)) name_start++;

        // Find variable name (starts with --)
        if (name_start[0] == '-' && name_start[1] == '-') {
            name_start += 2; // Skip --

            // Find end of name (comma or closing paren)
            const char* name_end = name_start;
            while (*name_end && *name_end != ',' && *name_end != ')') name_end++;

            // Extract name
            size_t name_len = name_end - name_start;
            char* var_name = malloc(name_len + 1);
            memcpy(var_name, name_start, name_len);
            var_name[name_len] = '\0';

            // Trim whitespace from name
            char* trimmed = trim_whitespace_alloc(var_name);
            free(var_name);
            var_name = trimmed;

            // Look up variable
            const char* resolved = ir_css_get_variable(stylesheet, var_name);
            free(var_name);

            // Find closing paren
            const char* close_paren = strchr(name_end, ')');
            if (!close_paren) close_paren = name_end;

            if (resolved) {
                // Append resolved value
                size_t val_len = strlen(resolved);
                if (result_len + val_len + 1 > result_cap) {
                    result_cap = result_len + val_len + 256;
                    result = realloc(result, result_cap);
                }
                strcpy(result + result_len, resolved);
                result_len += val_len;
            } else if (*name_end == ',') {
                // Use fallback value
                const char* fallback_start = name_end + 1;
                while (*fallback_start && isspace(*fallback_start)) fallback_start++;
                size_t fallback_len = close_paren - fallback_start;
                if (result_len + fallback_len + 1 > result_cap) {
                    result_cap = result_len + fallback_len + 256;
                    result = realloc(result, result_cap);
                }
                memcpy(result + result_len, fallback_start, fallback_len);
                result_len += fallback_len;
                result[result_len] = '\0';
            }
            // else: no value, no fallback - leave empty

            p = close_paren + 1;
        } else {
            // Invalid var() syntax, copy as-is
            if (result_len + 4 + 1 > result_cap) {
                result_cap += 256;
                result = realloc(result, result_cap);
            }
            memcpy(result + result_len, "var(", 4);
            result_len += 4;
            result[result_len] = '\0';
            p = name_start;
        }
    }

    return result;
}

// Parse CSS content into a stylesheet
CSSStylesheet* ir_css_parse_stylesheet(const char* css_content) {
    if (!css_content) return NULL;

    CSSStylesheet* stylesheet = calloc(1, sizeof(CSSStylesheet));
    if (!stylesheet) return NULL;

    stylesheet->rule_capacity = 32;
    stylesheet->rules = calloc(stylesheet->rule_capacity, sizeof(CSSRule));
    if (!stylesheet->rules) {
        free(stylesheet);
        return NULL;
    }

    // Initialize variables
    stylesheet->variable_capacity = 16;
    stylesheet->variables = calloc(stylesheet->variable_capacity, sizeof(CSSVariable));

    const char* p = css_content;

    while (*p) {
        // Skip whitespace and comments
        while (*p && isspace(*p)) p++;
        if (*p == '\0') break;

        // Skip CSS comments
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) p++;
            if (*p) p += 2;
            continue;
        }

        // Skip @rules (like @keyframes, @media, etc.)
        if (*p == '@') {
            // Skip to end of rule
            int brace_depth = 0;
            while (*p) {
                if (*p == '{') brace_depth++;
                else if (*p == '}') {
                    brace_depth--;
                    if (brace_depth <= 0) {
                        p++;
                        break;
                    }
                }
                p++;
            }
            continue;
        }

        // Find selector (everything before '{')
        const char* selector_start = p;
        while (*p && *p != '{') p++;
        if (*p == '\0') break;

        // Extract and trim selector
        size_t selector_len = p - selector_start;
        char* selector_raw = malloc(selector_len + 1);
        if (!selector_raw) break;
        memcpy(selector_raw, selector_start, selector_len);
        selector_raw[selector_len] = '\0';

        char* selector = trim_whitespace_alloc(selector_raw);
        free(selector_raw);

        if (!selector || strlen(selector) == 0) {
            free(selector);
            p++; // Skip '{'
            // Skip to matching '}'
            while (*p && *p != '}') p++;
            if (*p) p++;
            continue;
        }

        p++; // Skip '{'

        // Find rule block (everything before '}')
        const char* block_start = p;
        int brace_depth = 1;
        while (*p && brace_depth > 0) {
            if (*p == '{') brace_depth++;
            else if (*p == '}') brace_depth--;
            if (brace_depth > 0) p++;
        }

        // Extract block
        size_t block_len = p - block_start;
        char* block = malloc(block_len + 1);
        if (!block) {
            free(selector);
            break;
        }
        memcpy(block, block_start, block_len);
        block[block_len] = '\0';

        if (*p == '}') p++;

        // Parse properties
        uint32_t prop_count = 0;
        CSSProperty* properties = parse_rule_block(block, &prop_count);
        free(block);

        if (properties && prop_count > 0) {
            // Special handling for :root - extract CSS variables
            if (strcmp(selector, ":root") == 0) {
                for (uint32_t i = 0; i < prop_count; i++) {
                    const char* prop = properties[i].property;
                    const char* val = properties[i].value;

                    // Check if property is a CSS variable (starts with --)
                    if (prop && prop[0] == '-' && prop[1] == '-') {
                        // Store without the -- prefix
                        ir_css_add_variable(stylesheet, prop + 2, val);
                    }
                }

                // Don't add :root as a regular rule
                ir_css_free_properties(properties, prop_count);
                free(selector);
            } else {
                // Grow rules array if needed
                if (stylesheet->rule_count >= stylesheet->rule_capacity) {
                    stylesheet->rule_capacity *= 2;
                    stylesheet->rules = realloc(stylesheet->rules,
                        stylesheet->rule_capacity * sizeof(CSSRule));
                }

                // Add rule
                CSSRule* rule = &stylesheet->rules[stylesheet->rule_count++];
                rule->selector = selector;
                rule->properties = properties;
                rule->property_count = prop_count;
            }
        } else {
            free(selector);
        }
    }

    // Resolve all var() references in property values
    for (uint32_t i = 0; i < stylesheet->rule_count; i++) {
        CSSRule* rule = &stylesheet->rules[i];
        for (uint32_t j = 0; j < rule->property_count; j++) {
            char* resolved = ir_css_resolve_variables(stylesheet, rule->properties[j].value);
            if (resolved) {
                free(rule->properties[j].value);
                rule->properties[j].value = resolved;
            }
        }
    }

    return stylesheet;
}

// Parse CSS file into a stylesheet
CSSStylesheet* ir_css_parse_stylesheet_file(const char* filepath) {
    if (!filepath) return NULL;

    FILE* f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "Warning: Could not open CSS file: %s\n", filepath);
        return NULL;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return NULL;
    }

    // Read content
    char* content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    size_t bytes_read = fread(content, 1, size, f);
    fclose(f);

    content[bytes_read] = '\0';

    // Parse
    CSSStylesheet* stylesheet = ir_css_parse_stylesheet(content);
    free(content);

    return stylesheet;
}

// Free stylesheet
void ir_css_stylesheet_free(CSSStylesheet* stylesheet) {
    if (!stylesheet) return;

    for (uint32_t i = 0; i < stylesheet->rule_count; i++) {
        CSSRule* rule = &stylesheet->rules[i];
        free(rule->selector);
        ir_css_free_properties(rule->properties, rule->property_count);
    }
    free(stylesheet->rules);

    // Free CSS variables
    for (uint32_t i = 0; i < stylesheet->variable_count; i++) {
        free(stylesheet->variables[i].name);
        free(stylesheet->variables[i].value);
    }
    free(stylesheet->variables);

    free(stylesheet);
}

// Merge another stylesheet into this one (rules from 'other' are appended)
void ir_css_stylesheet_merge(CSSStylesheet* target, CSSStylesheet* other) {
    if (!target || !other) return;

    // Merge CSS variables (later variables override earlier)
    for (uint32_t i = 0; i < other->variable_count; i++) {
        ir_css_add_variable(target, other->variables[i].name, other->variables[i].value);
    }

    if (other->rule_count == 0) return;

    // Grow target array if needed
    uint32_t new_count = target->rule_count + other->rule_count;
    if (new_count > target->rule_capacity) {
        while (target->rule_capacity < new_count) {
            target->rule_capacity *= 2;
        }
        target->rules = realloc(target->rules, target->rule_capacity * sizeof(CSSRule));
    }

    // Copy rules from other to target
    for (uint32_t i = 0; i < other->rule_count; i++) {
        CSSRule* src = &other->rules[i];
        CSSRule* dst = &target->rules[target->rule_count++];

        // Deep copy selector
        dst->selector = strdup(src->selector);

        // Deep copy properties
        dst->property_count = src->property_count;
        dst->properties = calloc(src->property_count, sizeof(CSSProperty));
        for (uint32_t j = 0; j < src->property_count; j++) {
            dst->properties[j].property = strdup(src->properties[j].property);
            dst->properties[j].value = strdup(src->properties[j].value);
        }
    }
}

// ============================================================================
// Selector Matching
// ============================================================================

// Check if selector matches a class name
// selector: ".button" -> matches element with class containing "button"
bool ir_css_selector_matches_class(const char* selector, const char* class_name) {
    if (!selector || !class_name) return false;

    // Only handle class selectors for now
    if (selector[0] != '.') return false;

    const char* sel_class = selector + 1; // Skip '.'

    // Check if class_name contains sel_class as a whole word
    // class_name might be "button primary" (space-separated)
    const char* p = class_name;
    size_t sel_len = strlen(sel_class);

    while (*p) {
        // Skip whitespace
        while (*p && isspace(*p)) p++;
        if (*p == '\0') break;

        // Find end of this class token
        const char* token_start = p;
        while (*p && !isspace(*p)) p++;
        size_t token_len = p - token_start;

        // Compare
        if (token_len == sel_len && strncmp(token_start, sel_class, sel_len) == 0) {
            return true;
        }
    }

    return false;
}

// Check if selector matches an ID
// selector: "#header" -> matches element with id="header"
bool ir_css_selector_matches_id(const char* selector, const char* id) {
    if (!selector || !id) return false;

    // Only handle ID selectors
    if (selector[0] != '#') return false;

    const char* sel_id = selector + 1; // Skip '#'
    return strcmp(sel_id, id) == 0;
}

// Check if selector is an element selector (no . or # prefix)
static bool is_element_selector(const char* selector) {
    if (!selector || selector[0] == '\0') return false;
    return selector[0] != '.' && selector[0] != '#' && isalpha(selector[0]);
}

// Check if selector matches an element's tag name
bool ir_css_selector_matches_element(const char* selector, const char* tag_name) {
    if (!selector || !tag_name) return false;
    if (!is_element_selector(selector)) return false;
    return strcmp(selector, tag_name) == 0;
}

// Apply all matching rules from stylesheet
void ir_css_apply_stylesheet_rules(
    CSSStylesheet* stylesheet,
    const char* tag_name,
    const char* class_name,
    const char* id,
    IRStyle* style,
    IRLayout* layout
) {
    if (!stylesheet) return;

    // Apply rules in order (later rules override earlier)
    for (uint32_t i = 0; i < stylesheet->rule_count; i++) {
        CSSRule* rule = &stylesheet->rules[i];

        bool matches = false;

        // Check element selector match (e.g., "div", "span", "button")
        if (tag_name && ir_css_selector_matches_element(rule->selector, tag_name)) {
            matches = true;
        }

        // Check class match (higher specificity than element)
        if (class_name && ir_css_selector_matches_class(rule->selector, class_name)) {
            matches = true;
        }

        // Check ID match (highest specificity)
        if (id && ir_css_selector_matches_id(rule->selector, id)) {
            matches = true;
        }

        if (matches) {
            // Apply style properties
            if (style) {
                ir_css_apply_to_style(style, rule->properties, rule->property_count);
            }

            // Apply layout properties
            if (layout) {
                ir_css_apply_to_layout(layout, rule->properties, rule->property_count);
            }
        }
    }
}

// ============================================================================
// IRStylesheet Conversion (Bridge to new stylesheet system)
// ============================================================================

#include "../../ir_stylesheet.h"

/**
 * Convert CSSProperty array to IRStyleProperties
 * This bridges the existing CSS parsing with the new IRStyleProperties structure
 */
void ir_css_to_style_properties(const CSSProperty* props, uint32_t count, IRStyleProperties* out) {
    if (!props || !out) return;

    ir_style_properties_init(out);

    // Create temporary style and layout for parsing
    IRStyle temp_style = {0};
    IRLayout temp_layout = {0};

    // Use existing CSS parsers
    ir_css_apply_to_style(&temp_style, props, count);
    ir_css_apply_to_layout(&temp_layout, props, count);

    // Transfer parsed values to IRStyleProperties
    // Dimensions
    out->width = temp_style.width;
    out->height = temp_style.height;
    if (temp_style.width.type != IR_DIMENSION_AUTO || temp_style.width.value != 0) {
        out->set_flags |= IR_PROP_WIDTH;
    }
    if (temp_style.height.type != IR_DIMENSION_AUTO || temp_style.height.value != 0) {
        out->set_flags |= IR_PROP_HEIGHT;
    }

    // Colors
    out->background = temp_style.background;
    if (temp_style.background.type == IR_COLOR_SOLID || temp_style.background.type == IR_COLOR_GRADIENT) {
        out->set_flags |= IR_PROP_BACKGROUND;
    }
    out->color = temp_style.font.color;
    if (temp_style.font.color.type == IR_COLOR_SOLID || temp_style.font.color.type == IR_COLOR_GRADIENT) {
        out->set_flags |= IR_PROP_COLOR;
    }

    // Border
    out->border_color = temp_style.border.color;
    out->border_width = temp_style.border.width;
    out->border_width_top = temp_style.border.width_top;
    out->border_width_right = temp_style.border.width_right;
    out->border_width_bottom = temp_style.border.width_bottom;
    out->border_width_left = temp_style.border.width_left;
    out->border_radius = temp_style.border.radius;
    if (temp_style.border.width > 0) out->set_flags |= IR_PROP_BORDER;
    if (temp_style.border.width_top > 0) out->set_flags |= IR_PROP_BORDER_TOP;
    if (temp_style.border.width_right > 0) out->set_flags |= IR_PROP_BORDER_RIGHT;
    if (temp_style.border.width_bottom > 0) out->set_flags |= IR_PROP_BORDER_BOTTOM;
    if (temp_style.border.width_left > 0) out->set_flags |= IR_PROP_BORDER_LEFT;
    if (temp_style.border.radius > 0) out->set_flags |= IR_PROP_BORDER_RADIUS;

    // Spacing
    out->padding = temp_style.padding;
    out->margin = temp_style.margin;
    if (temp_style.padding.top > 0 || temp_style.padding.bottom > 0 ||
        temp_style.padding.left > 0 || temp_style.padding.right > 0) {
        out->set_flags |= IR_PROP_PADDING;
    }
    if (temp_style.margin.top != 0 || temp_style.margin.bottom != 0 ||
        temp_style.margin.left != 0 || temp_style.margin.right != 0) {
        out->set_flags |= IR_PROP_MARGIN;
    }

    // Typography
    out->font_size = temp_style.font.size;
    out->font_weight = temp_style.font.weight;
    out->font_bold = temp_style.font.bold;
    out->font_italic = temp_style.font.italic;
    out->text_align = temp_style.font.align;
    out->line_height = temp_style.font.line_height;
    out->letter_spacing = temp_style.font.letter_spacing;
    if (temp_style.font.family) {
        out->font_family = strdup(temp_style.font.family);
        out->set_flags |= IR_PROP_FONT_FAMILY;
    }
    if (temp_style.font.size > 0) out->set_flags |= IR_PROP_FONT_SIZE;
    if (temp_style.font.weight > 0) out->set_flags |= IR_PROP_FONT_WEIGHT;
    if (temp_style.font.align != IR_TEXT_ALIGN_LEFT) out->set_flags |= IR_PROP_TEXT_ALIGN;
    if (temp_style.font.line_height > 0) out->set_flags |= IR_PROP_LINE_HEIGHT;
    if (temp_style.font.letter_spacing != 0) out->set_flags |= IR_PROP_LETTER_SPACING;

    // Layout
    out->display = temp_layout.mode;
    out->display_explicit = temp_layout.display_explicit;
    out->flex_direction = temp_layout.flex.direction;
    out->flex_wrap = temp_layout.flex.wrap;
    out->justify_content = temp_layout.flex.justify_content;
    out->align_items = temp_layout.flex.cross_axis;
    out->gap = temp_layout.flex.gap;
    out->flex_grow = temp_layout.flex.grow;
    out->flex_shrink = temp_layout.flex.shrink;
    if (temp_layout.display_explicit) out->set_flags |= IR_PROP_DISPLAY;
    if (temp_layout.flex.direction != 0) out->set_flags |= IR_PROP_FLEX_DIRECTION;
    if (temp_layout.flex.wrap) out->set_flags |= IR_PROP_FLEX_WRAP;
    if (temp_layout.flex.justify_content != IR_ALIGNMENT_START) out->set_flags |= IR_PROP_JUSTIFY_CONTENT;
    if (temp_layout.flex.cross_axis != IR_ALIGNMENT_START) out->set_flags |= IR_PROP_ALIGN_ITEMS;
    if (temp_layout.flex.gap > 0) out->set_flags |= IR_PROP_GAP;
    if (temp_layout.flex.grow > 0) out->set_flags |= IR_PROP_FLEX_GROW;
    if (temp_layout.flex.shrink != 1) out->set_flags |= IR_PROP_FLEX_SHRINK;

    // Size constraints
    out->min_width = temp_layout.min_width;
    out->max_width = temp_layout.max_width;
    out->min_height = temp_layout.min_height;
    out->max_height = temp_layout.max_height;
    if (temp_layout.min_width.type != IR_DIMENSION_AUTO) out->set_flags |= IR_PROP_MIN_WIDTH;
    if (temp_layout.max_width.type != IR_DIMENSION_AUTO) out->set_flags |= IR_PROP_MAX_WIDTH;
    if (temp_layout.min_height.type != IR_DIMENSION_AUTO) out->set_flags |= IR_PROP_MIN_HEIGHT;
    if (temp_layout.max_height.type != IR_DIMENSION_AUTO) out->set_flags |= IR_PROP_MAX_HEIGHT;

    // Other
    out->opacity = temp_style.opacity > 0 ? temp_style.opacity : 1.0f;
    out->z_index = temp_style.z_index;
    if (temp_style.opacity != 1.0f && temp_style.opacity > 0) out->set_flags |= IR_PROP_OPACITY;
    if (temp_style.z_index > 0) out->set_flags |= IR_PROP_Z_INDEX;

    // Cleanup temporary style's dynamically allocated font.family
    if (temp_style.font.family) {
        free(temp_style.font.family);
    }
}

/**
 * Convert a CSSStylesheet to IRStylesheet
 * This creates a new IRStylesheet with parsed selectors and style properties
 */
IRStylesheet* ir_css_stylesheet_to_ir_stylesheet(CSSStylesheet* css_stylesheet) {
    if (!css_stylesheet) return NULL;

    IRStylesheet* ir_stylesheet = ir_stylesheet_create();
    if (!ir_stylesheet) return NULL;

    // Copy CSS variables
    for (uint32_t i = 0; i < css_stylesheet->variable_count; i++) {
        ir_stylesheet_add_variable(ir_stylesheet,
                                    css_stylesheet->variables[i].name,
                                    css_stylesheet->variables[i].value);
    }

    // Convert rules
    for (uint32_t i = 0; i < css_stylesheet->rule_count; i++) {
        CSSRule* css_rule = &css_stylesheet->rules[i];

        // Convert properties
        IRStyleProperties props;
        ir_css_to_style_properties(css_rule->properties, css_rule->property_count, &props);

        // Add rule with parsed selector
        ir_stylesheet_add_rule(ir_stylesheet, css_rule->selector, &props);

        // Cleanup props (font_family is copied in add_rule)
        ir_style_properties_cleanup(&props);
    }

    return ir_stylesheet;
}

// Debug print
void ir_css_stylesheet_print(CSSStylesheet* stylesheet) {
    if (!stylesheet) {
        printf("(null stylesheet)\n");
        return;
    }

    printf("CSSStylesheet with %u rules:\n", stylesheet->rule_count);
    for (uint32_t i = 0; i < stylesheet->rule_count; i++) {
        CSSRule* rule = &stylesheet->rules[i];
        printf("  Rule %u: '%s' (%u properties)\n",
               i, rule->selector, rule->property_count);

        for (uint32_t j = 0; j < rule->property_count; j++) {
            printf("    %s: %s\n",
                   rule->properties[j].property,
                   rule->properties[j].value);
        }
    }
}
