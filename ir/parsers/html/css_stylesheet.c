#define _POSIX_C_SOURCE 200809L
#include "css_stylesheet.h"
#include "../src/style/ir_stylesheet.h"
#include "../../include/ir_style.h"
#include "../../include/ir_layout.h"
#include "../common/parser_io_utils.h"
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

// Add a media query to the stylesheet
void ir_css_add_media_query(CSSStylesheet* stylesheet, const char* condition, const char* css_content) {
    if (!stylesheet || !condition || !css_content) return;

    // Grow array if needed
    if (stylesheet->media_query_count >= stylesheet->media_query_capacity) {
        uint32_t new_cap = stylesheet->media_query_capacity == 0 ? 8 : stylesheet->media_query_capacity * 2;
        CSSMediaQuery* new_queries = realloc(stylesheet->media_queries, new_cap * sizeof(CSSMediaQuery));
        if (!new_queries) return;
        stylesheet->media_queries = new_queries;
        stylesheet->media_query_capacity = new_cap;
    }

    // Add new media query
    CSSMediaQuery* query = &stylesheet->media_queries[stylesheet->media_query_count++];
    query->condition = strdup(condition);
    query->css_content = strdup(css_content);
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

        // Handle @rules
        if (*p == '@') {
            // Check if it's @media
            if (strncmp(p, "@media", 6) == 0) {
                // Parse @media query
                const char* start = p + 6;
                while (*start && isspace(*start)) start++;

                // Find the condition (everything in parentheses or before '{')
                const char* cond_start = start;
                while (*start && *start != '{') start++;
                if (*start == '\0') continue;

                // Extract condition
                size_t cond_len = start - cond_start;
                char* condition_raw = malloc(cond_len + 1);
                if (condition_raw) {
                    memcpy(condition_raw, cond_start, cond_len);
                    condition_raw[cond_len] = '\0';

                    // Trim parentheses and whitespace
                    char* cond = condition_raw;
                    while (*cond && (isspace(*cond) || *cond == '(')) cond++;
                    char* cond_end = cond + strlen(cond) - 1;
                    while (cond_end > cond && (isspace(*cond_end) || *cond_end == ')')) cond_end--;
                    cond_end[1] = '\0';

                    // Now find the CSS content inside the braces
                    start++; // Skip '{'
                    int brace_depth = 1;
                    const char* content_start = start;
                    while (*start && brace_depth > 0) {
                        if (*start == '{') brace_depth++;
                        else if (*start == '}') brace_depth--;
                        if (brace_depth > 0) start++;
                    }

                    // Extract content
                    size_t content_len = start - content_start;
                    char* content = malloc(content_len + 1);
                    if (content) {
                        memcpy(content, content_start, content_len);
                        content[content_len] = '\0';

                        // Add to stylesheet
                        if (stylesheet) {
                            ir_css_add_media_query(stylesheet, cond, content);
                        }
                        free(content);
                    }
                    free(condition_raw);
                }
                if (*start == '}') start++;
                p = start;
                continue;
            }

            // Skip other @rules (like @keyframes)
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

    ParserError error = {0};
    size_t size = 0;
    char* content = parser_read_file(filepath, &size, &error);
    if (!content) {
        fprintf(stderr, "Warning: Could not open CSS file: %s\n", filepath);
        return NULL;
    }

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

    // Free media queries
    for (uint32_t i = 0; i < stylesheet->media_query_count; i++) {
        free(stylesheet->media_queries[i].condition);
        free(stylesheet->media_queries[i].css_content);
    }
    free(stylesheet->media_queries);

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

#include "../../include/ir_style.h"

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

    // Colors - skip fully transparent colors (alpha=0)
    out->background = temp_style.background;
    if (temp_style.background.type == IR_COLOR_GRADIENT) {
        out->set_flags |= IR_PROP_BACKGROUND;
    } else if (temp_style.background.type == IR_COLOR_SOLID && temp_style.background.data.a > 0) {
        out->set_flags |= IR_PROP_BACKGROUND;
    }
    out->color = temp_style.font.color;
    if (temp_style.font.color.type == IR_COLOR_GRADIENT) {
        out->set_flags |= IR_PROP_COLOR;
    } else if (temp_style.font.color.type == IR_COLOR_VAR_REF || temp_style.font.color.var_name) {
        out->set_flags |= IR_PROP_COLOR;
    } else if (temp_style.font.color.type == IR_COLOR_SOLID && temp_style.font.color.data.a > 0) {
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
    out->border_radius_top_left = temp_style.border.radius_top_left;
    out->border_radius_top_right = temp_style.border.radius_top_right;
    out->border_radius_bottom_right = temp_style.border.radius_bottom_right;
    out->border_radius_bottom_left = temp_style.border.radius_bottom_left;
    out->border_radius_flags = temp_style.border.radius_flags;
    if (temp_style.border.width > 0) out->set_flags |= IR_PROP_BORDER;
    if (temp_style.border.width_top > 0) out->set_flags |= IR_PROP_BORDER_TOP;
    if (temp_style.border.width_right > 0) out->set_flags |= IR_PROP_BORDER_RIGHT;
    if (temp_style.border.width_bottom > 0) out->set_flags |= IR_PROP_BORDER_BOTTOM;
    if (temp_style.border.width_left > 0) out->set_flags |= IR_PROP_BORDER_LEFT;
    if (temp_style.border.radius > 0) out->set_flags |= IR_PROP_BORDER_RADIUS;
    // Check for border-color only (e.g., in hover states)
    if (temp_style.border.color.type == IR_COLOR_VAR_REF ||
        (temp_style.border.color.type == IR_COLOR_SOLID && temp_style.border.color.data.a > 0)) {
        out->set_flags |= IR_PROP_BORDER_COLOR;
    }

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
    // Parse font-size directly from raw properties to preserve em/rem units
    bool font_size_found = false;
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(props[i].property, "font-size") == 0) {
            const char* val = props[i].value;
            // Parse number manually to avoid sscanf eating 'e' in 'em' as exponent
            char* endptr = NULL;
            float size = strtof(val, &endptr);
            if (endptr != val) {
                out->font_size.value = size;
                // Skip whitespace after number
                while (*endptr && isspace(*endptr)) endptr++;
                // Check unit
                if (strncmp(endptr, "em", 2) == 0 && (endptr[2] == '\0' || isspace(endptr[2]))) {
                    out->font_size.type = IR_DIMENSION_EM;
                } else if (strncmp(endptr, "rem", 3) == 0) {
                    out->font_size.type = IR_DIMENSION_REM;
                } else if (*endptr == '%') {
                    out->font_size.type = IR_DIMENSION_PERCENT;
                } else if (strncmp(endptr, "vw", 2) == 0) {
                    out->font_size.type = IR_DIMENSION_VW;
                } else if (strncmp(endptr, "vh", 2) == 0) {
                    out->font_size.type = IR_DIMENSION_VH;
                } else {
                    out->font_size.type = IR_DIMENSION_PX;
                }
                out->set_flags |= IR_PROP_FONT_SIZE;
                font_size_found = true;
            }
            break;
        }
    }
    // Fall back to temp_style if not found in raw props
    if (!font_size_found && temp_style.font.size > 0) {
        out->font_size.value = temp_style.font.size;
        out->font_size.type = IR_DIMENSION_PX;
        out->set_flags |= IR_PROP_FONT_SIZE;
    }
    out->font_weight = temp_style.font.weight;
    out->font_bold = temp_style.font.bold;
    out->font_italic = temp_style.font.italic;
    out->text_align = temp_style.font.align;
    out->line_height = temp_style.font.line_height;
    out->letter_spacing = temp_style.font.letter_spacing;
    // Parse font-family directly from raw properties to preserve quotes
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(props[i].property, "font-family") == 0) {
            out->font_family = strdup(props[i].value);
            out->set_flags |= IR_PROP_FONT_FAMILY;
            break;
        }
    }
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
    // Check if flex-direction was explicitly specified (can't detect from value alone since column=0 is also default)
    for (uint32_t i = 0; i < count; i++) {
        if (props[i].property && strcmp(props[i].property, "flex-direction") == 0) {
            out->set_flags |= IR_PROP_FLEX_DIRECTION;
            break;
        }
    }
    if (temp_layout.flex.wrap) out->set_flags |= IR_PROP_FLEX_WRAP;
    if (temp_layout.flex.justify_content != IR_ALIGNMENT_START) out->set_flags |= IR_PROP_JUSTIFY_CONTENT;
    if (temp_layout.flex.cross_axis != IR_ALIGNMENT_START) out->set_flags |= IR_PROP_ALIGN_ITEMS;
    if (temp_layout.flex.gap > 0) out->set_flags |= IR_PROP_GAP;
    if (temp_layout.flex.grow > 0) out->set_flags |= IR_PROP_FLEX_GROW;
    if (temp_layout.flex.shrink != 1) out->set_flags |= IR_PROP_FLEX_SHRINK;

    // Grid template - store raw CSS string for roundtrip
    // Look through original props for raw CSS string properties
    for (uint32_t i = 0; i < count; i++) {
        if (props[i].property && props[i].value) {
            if (strcmp(props[i].property, "grid-template-columns") == 0) {
                out->grid_template_columns = strdup(props[i].value);
                out->set_flags |= IR_PROP_GRID_TEMPLATE_COLUMNS;
            } else if (strcmp(props[i].property, "grid-template-rows") == 0) {
                out->grid_template_rows = strdup(props[i].value);
                out->set_flags |= IR_PROP_GRID_TEMPLATE_ROWS;
            } else if (strcmp(props[i].property, "transform") == 0) {
                // Parse transform: "scale(1.05)" or "translateX(10px) rotate(45deg)"
                const char* val = props[i].value;

                // Initialize transform to identity
                out->transform.translate_x = 0;
                out->transform.translate_y = 0;
                out->transform.scale_x = 1.0f;
                out->transform.scale_y = 1.0f;
                out->transform.rotate = 0;

                // Parse scale()
                const char* scale_pos = strstr(val, "scale(");
                if (scale_pos) {
                    float scale = 1.0f;
                    if (sscanf(scale_pos + 6, "%f", &scale) == 1) {
                        out->transform.scale_x = scale;
                        out->transform.scale_y = scale;
                    }
                }

                // Parse scaleX()
                const char* scale_x_pos = strstr(val, "scaleX(");
                if (scale_x_pos) {
                    sscanf(scale_x_pos + 7, "%f", &out->transform.scale_x);
                }

                // Parse scaleY()
                const char* scale_y_pos = strstr(val, "scaleY(");
                if (scale_y_pos) {
                    sscanf(scale_y_pos + 7, "%f", &out->transform.scale_y);
                }

                // Parse translateX()
                const char* tx_pos = strstr(val, "translateX(");
                if (tx_pos) {
                    sscanf(tx_pos + 11, "%f", &out->transform.translate_x);
                }

                // Parse translateY()
                const char* ty_pos = strstr(val, "translateY(");
                if (ty_pos) {
                    sscanf(ty_pos + 11, "%f", &out->transform.translate_y);
                }

                // Parse translate(x, y)
                const char* translate_pos = strstr(val, "translate(");
                if (translate_pos && !strstr(val, "translateX") && !strstr(val, "translateY")) {
                    sscanf(translate_pos + 10, "%f", &out->transform.translate_x);
                    const char* comma = strchr(translate_pos + 10, ',');
                    if (comma) {
                        sscanf(comma + 1, "%f", &out->transform.translate_y);
                    }
                }

                // Parse rotate()
                const char* rotate_pos = strstr(val, "rotate(");
                if (rotate_pos) {
                    float rotate = 0;
                    sscanf(rotate_pos + 7, "%f", &rotate);
                    out->transform.rotate = rotate;
                }

                out->set_flags |= IR_PROP_TRANSFORM;
            } else if (strcmp(props[i].property, "text-decoration") == 0) {
                // Parse text-decoration: "none", "underline", "line-through", "overline"
                const char* val = props[i].value;
                out->text_decoration = IR_TEXT_DECORATION_NONE;

                if (strstr(val, "underline")) {
                    out->text_decoration |= IR_TEXT_DECORATION_UNDERLINE;
                }
                if (strstr(val, "overline")) {
                    out->text_decoration |= IR_TEXT_DECORATION_OVERLINE;
                }
                if (strstr(val, "line-through")) {
                    out->text_decoration |= IR_TEXT_DECORATION_LINE_THROUGH;
                }
                // "none" is already 0, no action needed

                out->set_flags |= IR_PROP_TEXT_DECORATION;
            } else if (strcmp(props[i].property, "box-sizing") == 0) {
                out->box_sizing = (strcmp(props[i].value, "border-box") == 0) ? 1 : 0;
                out->set_flags |= IR_PROP_BOX_SIZING;
            } else if (strcmp(props[i].property, "object-fit") == 0) {
                const char* val = props[i].value;
                if (strcmp(val, "fill") == 0) {
                    out->object_fit = IR_OBJECT_FIT_FILL;
                } else if (strcmp(val, "contain") == 0) {
                    out->object_fit = IR_OBJECT_FIT_CONTAIN;
                } else if (strcmp(val, "cover") == 0) {
                    out->object_fit = IR_OBJECT_FIT_COVER;
                } else if (strcmp(val, "none") == 0) {
                    out->object_fit = IR_OBJECT_FIT_NONE;
                } else if (strcmp(val, "scale-down") == 0) {
                    out->object_fit = IR_OBJECT_FIT_SCALE_DOWN;
                }
                out->set_flags |= IR_PROP_OBJECT_FIT;
            }
        }
    }

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

    // Background image (gradient string)
    if (temp_style.background_image) {
        out->background_image = strdup(temp_style.background_image);
        out->set_flags |= IR_PROP_BACKGROUND_IMAGE;
    }

    // Background clip
    out->background_clip = temp_style.background_clip;
    if (temp_style.background_clip != IR_BACKGROUND_CLIP_BORDER_BOX) {
        out->set_flags |= IR_PROP_BACKGROUND_CLIP;
    }

    // Text fill color - only set flag if explicitly specified
    // (type is TRANSPARENT, or type is SOLID with non-zero alpha)
    out->text_fill_color = temp_style.text_fill_color;
    if (temp_style.text_fill_color.type == IR_COLOR_TRANSPARENT) {
        out->set_flags |= IR_PROP_TEXT_FILL_COLOR;
    } else if (temp_style.text_fill_color.type == IR_COLOR_SOLID &&
               temp_style.text_fill_color.data.a > 0) {
        out->set_flags |= IR_PROP_TEXT_FILL_COLOR;
    }

    // Cleanup temporary style's dynamically allocated memory
    if (temp_style.font.family) {
        free(temp_style.font.family);
    }
    if (temp_style.background_image) {
        free(temp_style.background_image);
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

    // Copy media queries
    for (uint32_t i = 0; i < css_stylesheet->media_query_count; i++) {
        ir_stylesheet_add_media_query(ir_stylesheet,
                                      css_stylesheet->media_queries[i].condition,
                                      css_stylesheet->media_queries[i].css_content);
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
