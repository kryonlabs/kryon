/**
 * @file css_class_names.c
 * @brief Component class name mapping implementation
 */

#define _GNU_SOURCE
#include "css_class_names.h"
#include "ir_types.h""
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// ============================================================================
// Component Type to Class Name Mapping
// ============================================================================

typedef struct {
    IRComponentType type;
    const char* class_name;
    const char* tag_name;
    const char* default_display;
    bool is_container;
    bool is_text;
    bool is_form;
    bool is_self_closing;
    bool uses_inline_dimensions;
} ComponentTypeMapping;

static const ComponentTypeMapping component_type_mapping[] = {
    // Container types
    { IR_COMPONENT_CONTAINER, "container", "div", "flex", true, false, false, false, false },
    { IR_COMPONENT_ROW, "row", "div", "flex", true, false, false, false, false },
    { IR_COMPONENT_COLUMN, "column", "div", "flex", true, false, false, false, false },
    { IR_COMPONENT_CENTER, "center", "div", "flex", true, false, false, false, false },
    { IR_COMPONENT_DROPDOWN, "dropdown", "div", "relative", true, false, false, false, false },
    { IR_COMPONENT_TAB_GROUP, "tab-group", "div", "flex", true, false, false, false, false },
    { IR_COMPONENT_TAB_BAR, "tab-bar", "div", "flex", true, false, false, false, false },
    { IR_COMPONENT_TAB_CONTENT, "tab-content", "div", "block", true, false, false, false, false },
    { IR_COMPONENT_TAB_PANEL, "tab-panel", "div", "block", true, false, false, false, false },
    { IR_COMPONENT_FOR_EACH, "for-each", "div", "contents", true, false, false, false, false },

    // Table components
    { IR_COMPONENT_TABLE, "table", "table", "table", true, false, false, false, false },
    { IR_COMPONENT_TABLE_HEAD, "thead", "thead", "table-header-group", true, false, false, false, false },
    { IR_COMPONENT_TABLE_BODY, "tbody", "tbody", "table-row-group", true, false, false, false, false },
    { IR_COMPONENT_TABLE_FOOT, "tfoot", "tfoot", "table-footer-group", true, false, false, false, false },
    { IR_COMPONENT_TABLE_ROW, "tr", "tr", "table-row", true, false, false, false, false },
    { IR_COMPONENT_TABLE_CELL, "td", "td", "table-cell", false, true, false, false, false },
    { IR_COMPONENT_TABLE_HEADER_CELL, "th", "th", "table-cell", false, true, false, false, false },

    // Text types
    { IR_COMPONENT_TEXT, "text", "span", "inline", false, true, false, false, false },
    { IR_COMPONENT_HEADING, "heading", "h1", "block", false, true, false, false, false },
    { IR_COMPONENT_PARAGRAPH, "paragraph", "p", "block", false, true, false, false, false },
    { IR_COMPONENT_BLOCKQUOTE, "blockquote", "blockquote", "block", false, true, false, false, false },
    { IR_COMPONENT_CODE_BLOCK, "code-block", "pre", "block", false, true, false, false, false },
    { IR_COMPONENT_HORIZONTAL_RULE, "hr", "hr", "block", false, false, false, true, false },
    { IR_COMPONENT_LIST, "list", "ul", "block", true, false, false, false, false },
    { IR_COMPONENT_LIST_ITEM, "li", "li", "list-item", false, true, false, false, false },
    { IR_COMPONENT_LINK, "link", "a", "inline", false, false, false, false, false },

    // Inline semantic components
    { IR_COMPONENT_SPAN, "span", "span", "inline", false, true, false, false, false },
    { IR_COMPONENT_STRONG, "strong", "strong", "inline", false, true, false, false, false },
    { IR_COMPONENT_EM, "em", "em", "inline", false, true, false, false, false },
    { IR_COMPONENT_CODE_INLINE, "code", "code", "inline", false, true, false, false, false },
    { IR_COMPONENT_SMALL, "small", "small", "inline", false, true, false, false, false },
    { IR_COMPONENT_MARK, "mark", "mark", "inline", false, true, false, false, false },

    // Form types
    { IR_COMPONENT_BUTTON, "button", "button", "inline-block", false, false, true, false, false },
    { IR_COMPONENT_INPUT, "input", "input", "inline-block", false, false, true, true, false },
    { IR_COMPONENT_CHECKBOX, "checkbox", "input", "inline-block", false, false, true, true, false },

    // Media types
    { IR_COMPONENT_IMAGE, "image", "img", "inline-block", false, false, false, true, true },
    { IR_COMPONENT_CANVAS, "canvas", "canvas", "inline-block", false, false, false, false, true },
    { IR_COMPONENT_NATIVE_CANVAS, "native-canvas", "canvas", "inline-block", false, false, false, false, true },

    // Modal
    { IR_COMPONENT_MODAL, "modal", "dialog", "block", true, false, false, false, false },

    // Special types
    { IR_COMPONENT_MARKDOWN, "markdown", "div", "block", true, false, false, false, false },
    { IR_COMPONENT_SPRITE, "sprite", "div", "block", false, false, false, false, false },
    { IR_COMPONENT_TAB, "tab", "button", "inline-block", false, false, true, false, false },

    // Source structure types
    { IR_COMPONENT_STATIC_BLOCK, "static-block", "div", "block", true, false, false, false, false },
    { IR_COMPONENT_FOR_LOOP, "for-loop", "div", "block", true, false, false, false, false },
    { IR_COMPONENT_VAR_DECL, "var-decl", "div", "block", true, false, false, false, false },
    { IR_COMPONENT_PLACEHOLDER, "placeholder", "div", "block", true, false, false, false, false },

    // Custom
    { IR_COMPONENT_CUSTOM, "custom", "div", "block", true, false, false, false, false },
};

static const size_t component_type_mapping_count =
    sizeof(component_type_mapping) / sizeof(component_type_mapping[0]);

// Find mapping entry for a component type
static const ComponentTypeMapping* find_mapping(IRComponentType type) {
    for (size_t i = 0; i < component_type_mapping_count; i++) {
        if (component_type_mapping[i].type == type) {
            return &component_type_mapping[i];
        }
    }
    // Return container entry as default
    return &component_type_mapping[0];
}

// ============================================================================
// Public Functions
// ============================================================================

const char* css_get_class_name_for_type(IRComponentType type) {
    return find_mapping(type)->class_name;
}

const char* css_get_tag_name_for_type(IRComponentType type) {
    return find_mapping(type)->tag_name;
}

bool css_is_container_type(IRComponentType type) {
    return find_mapping(type)->is_container;
}

bool css_is_text_type(IRComponentType type) {
    return find_mapping(type)->is_text;
}

bool css_is_form_type(IRComponentType type) {
    return find_mapping(type)->is_form;
}

bool css_is_self_closing_type(IRComponentType type) {
    return find_mapping(type)->is_self_closing;
}

const char* css_get_default_display(IRComponentType type) {
    return find_mapping(type)->default_display;
}

bool css_uses_inline_dimensions(IRComponentType type) {
    return find_mapping(type)->uses_inline_dimensions;
}

char* css_classes_to_selector(const char* class_names) {
    if (!class_names || class_names[0] == '\0') return NULL;

    // Count spaces to determine number of classes
    size_t len = strlen(class_names);
    size_t space_count = 0;
    for (size_t i = 0; i < len; i++) {
        if (class_names[i] == ' ') space_count++;
    }

    // Allocate buffer: original length + dots for each class + leading dot + null
    size_t selector_len = len + space_count + 2;
    char* selector = malloc(selector_len);
    if (!selector) return NULL;

    char* out = selector;
    *out++ = '.';  // Leading dot

    for (size_t i = 0; i < len; i++) {
        if (class_names[i] == ' ') {
            *out++ = '.';  // Replace space with dot
        } else {
            *out++ = class_names[i];
        }
    }
    *out = '\0';

    return selector;
}

char* css_generate_selector(IRComponent* component) {
    if (!component) return NULL;

    // Check for element selector
    if (component->selector_type == IR_SELECTOR_ELEMENT && component->tag) {
        return strdup(component->tag);
    }

    // Check for class name from style_analyzer
    // This uses the existing generate_natural_class_name function
    // which is declared in style_analyzer.h
    extern char* generate_natural_class_name(IRComponent* component);
    char* class_name = generate_natural_class_name(component);
    if (!class_name) return NULL;

    // Convert to compound selector if needed
    char* selector = css_classes_to_selector(class_name);
    free(class_name);

    return selector ? selector : strdup("");
}

char* css_generate_pseudo_selector(IRComponent* component, const char* pseudo_class) {
    if (!component || !pseudo_class) return NULL;

    char* base_selector = css_generate_selector(component);
    if (!base_selector) return NULL;

    char* result = NULL;
    if (asprintf(&result, "%s:%s", base_selector, pseudo_class) < 0) {
        result = NULL;
    }

    free(base_selector);
    return result;
}

bool css_is_element_selector(const char* selector) {
    if (!selector || selector[0] == '\0') return false;

    // Element selector starts with a letter, not . or #
    char first = selector[0];
    return (first != '.' && first != '#' &&
            !strchr(selector, ' ') && !strchr(selector, ':'));
}

bool css_is_class_selector(const char* selector) {
    if (!selector || selector[0] == '\0') return false;
    return selector[0] == '.';
}

bool css_is_id_selector(const char* selector) {
    if (!selector || selector[0] == '\0') return false;
    return selector[0] == '#';
}

const char* css_extract_base_class(const char* selector) {
    if (!selector || selector[0] != '.') return NULL;

    // Skip leading dot
    const char* start = selector + 1;

    // Find end of first class (before next dot, space, or pseudo-class)
    const char* end = start;
    while (*end && *end != '.' && *end != ' ' && *end != ':' && *end != '[') {
        end++;
    }

    // Copy to static buffer
    static char class_buffer[128];
    size_t len = end - start;
    if (len >= sizeof(class_buffer)) len = sizeof(class_buffer) - 1;

    memcpy(class_buffer, start, len);
    class_buffer[len] = '\0';

    return class_buffer;
}
