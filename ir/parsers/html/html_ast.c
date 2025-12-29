#define _POSIX_C_SOURCE 200809L
#include "html_ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define INITIAL_CHILDREN_CAPACITY 4

// ============================================================================
// HTML Node Creation
// ============================================================================

HtmlNode* html_node_create(HtmlNodeType type) {
    HtmlNode* node = (HtmlNode*)calloc(1, sizeof(HtmlNode));
    if (!node) return NULL;

    node->type = type;
    node->child_capacity = INITIAL_CHILDREN_CAPACITY;
    node->children = (HtmlNode**)calloc(node->child_capacity, sizeof(HtmlNode*));

    if (!node->children) {
        free(node);
        return NULL;
    }

    return node;
}

HtmlNode* html_node_create_element(const char* tag_name) {
    HtmlNode* node = html_node_create(HTML_NODE_ELEMENT);
    if (!node) return NULL;

    if (tag_name) {
        node->tag_name = strdup(tag_name);
        if (!node->tag_name) {
            html_node_free(node);
            return NULL;
        }
    }

    return node;
}

HtmlNode* html_node_create_text(const char* text) {
    HtmlNode* node = html_node_create(HTML_NODE_TEXT);
    if (!node) return NULL;

    if (text) {
        node->text_content = strdup(text);
        if (!node->text_content) {
            html_node_free(node);
            return NULL;
        }
    }

    return node;
}

// ============================================================================
// Tree Manipulation
// ============================================================================

void html_node_add_child(HtmlNode* parent, HtmlNode* child) {
    if (!parent || !child) return;

    // Grow children array if needed
    if (parent->child_count >= parent->child_capacity) {
        uint32_t new_capacity = parent->child_capacity * 2;
        HtmlNode** new_children = (HtmlNode**)realloc(
            parent->children,
            new_capacity * sizeof(HtmlNode*)
        );

        if (!new_children) return;  // Allocation failed, skip adding child

        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }

    parent->children[parent->child_count++] = child;
    child->parent = parent;
}

// ============================================================================
// Attribute Handling
// ============================================================================

void html_node_set_attribute(HtmlNode* node, const char* name, const char* value) {
    if (!node || !name) return;

    // Free existing attribute if present
    char** attr_ptr = NULL;

    if (strcmp(name, "id") == 0) {
        attr_ptr = &node->id;
    } else if (strcmp(name, "class") == 0) {
        attr_ptr = &node->class_name;
    } else if (strcmp(name, "style") == 0) {
        attr_ptr = &node->style;
    } else if (strcmp(name, "src") == 0) {
        attr_ptr = &node->src;
    } else if (strcmp(name, "href") == 0) {
        attr_ptr = &node->href;
    } else if (strcmp(name, "target") == 0) {
        attr_ptr = &node->target;
    } else if (strcmp(name, "rel") == 0) {
        attr_ptr = &node->rel;
    } else if (strcmp(name, "alt") == 0) {
        attr_ptr = &node->alt;
    } else if (strcmp(name, "type") == 0) {
        attr_ptr = &node->input_type;
    } else if (strcmp(name, "value") == 0) {
        attr_ptr = &node->value;
    } else if (strcmp(name, "checked") == 0) {
        node->checked = (value && strcmp(value, "true") == 0);
        return;
    } else if (strcmp(name, "start") == 0) {
        if (value) node->start = (uint32_t)atoi(value);
        return;
    }

    if (attr_ptr) {
        if (*attr_ptr) free(*attr_ptr);
        *attr_ptr = value ? strdup(value) : NULL;
    }
}

const char* html_node_get_attribute(HtmlNode* node, const char* name) {
    if (!node || !name) return NULL;

    if (strcmp(name, "id") == 0) return node->id;
    if (strcmp(name, "class") == 0) return node->class_name;
    if (strcmp(name, "style") == 0) return node->style;
    if (strcmp(name, "src") == 0) return node->src;
    if (strcmp(name, "href") == 0) return node->href;
    if (strcmp(name, "target") == 0) return node->target;
    if (strcmp(name, "rel") == 0) return node->rel;
    if (strcmp(name, "alt") == 0) return node->alt;
    if (strcmp(name, "type") == 0) return node->input_type;
    if (strcmp(name, "value") == 0) return node->value;

    return NULL;
}

void html_node_set_data_attribute(HtmlNode* node, const char* name, const char* value) {
    if (!node || !name) return;

    if (strcmp(name, "data-ir-type") == 0) {
        if (node->data_ir_type) free(node->data_ir_type);
        node->data_ir_type = value ? strdup(value) : NULL;
    } else if (strcmp(name, "data-ir-id") == 0) {
        if (value) node->data_ir_id = (uint32_t)atoi(value);
    } else if (strcmp(name, "data-level") == 0) {
        if (value) node->data_level = (uint32_t)atoi(value);
    } else if (strcmp(name, "data-list-type") == 0) {
        if (node->data_list_type) free(node->data_list_type);
        node->data_list_type = value ? strdup(value) : NULL;
    } else if (strcmp(name, "data-checked") == 0) {
        node->data_checked = (value && strcmp(value, "true") == 0);
    } else if (strcmp(name, "data-value") == 0) {
        if (node->data_value) free(node->data_value);
        node->data_value = value ? strdup(value) : NULL;
    }
}

// ============================================================================
// Memory Management
// ============================================================================

void html_node_free(HtmlNode* node) {
    if (!node) return;

    // Free string attributes
    if (node->tag_name) free(node->tag_name);
    if (node->text_content) free(node->text_content);
    if (node->id) free(node->id);
    if (node->class_name) free(node->class_name);
    if (node->style) free(node->style);
    if (node->src) free(node->src);
    if (node->href) free(node->href);
    if (node->target) free(node->target);
    if (node->rel) free(node->rel);
    if (node->alt) free(node->alt);
    if (node->input_type) free(node->input_type);
    if (node->value) free(node->value);
    if (node->data_ir_type) free(node->data_ir_type);
    if (node->data_list_type) free(node->data_list_type);
    if (node->data_value) free(node->data_value);
    if (node->link_url) free(node->link_url);

    // Free children recursively
    for (uint32_t i = 0; i < node->child_count; i++) {
        html_node_free(node->children[i]);
    }

    if (node->children) free(node->children);

    free(node);
}

HtmlNode* html_node_clone(const HtmlNode* node) {
    if (!node) return NULL;

    HtmlNode* clone = html_node_create(node->type);
    if (!clone) return NULL;

    // Clone string attributes
    #define CLONE_STR(field) \
        if (node->field) { \
            clone->field = strdup(node->field); \
            if (!clone->field) { \
                html_node_free(clone); \
                return NULL; \
            } \
        }

    CLONE_STR(tag_name);
    CLONE_STR(text_content);
    CLONE_STR(id);
    CLONE_STR(class_name);
    CLONE_STR(style);
    CLONE_STR(src);
    CLONE_STR(href);
    CLONE_STR(target);
    CLONE_STR(rel);
    CLONE_STR(alt);
    CLONE_STR(input_type);
    CLONE_STR(value);
    CLONE_STR(data_ir_type);
    CLONE_STR(data_list_type);
    CLONE_STR(data_value);
    CLONE_STR(link_url);

    #undef CLONE_STR

    // Clone primitive fields
    clone->checked = node->checked;
    clone->start = node->start;
    clone->data_ir_id = node->data_ir_id;
    clone->data_level = node->data_level;
    clone->data_checked = node->data_checked;
    clone->is_bold = node->is_bold;
    clone->is_italic = node->is_italic;
    clone->is_code = node->is_code;
    clone->is_link = node->is_link;

    // Note: Children are NOT cloned (shallow copy)

    return clone;
}

// ============================================================================
// Debugging
// ============================================================================

void html_node_print(const HtmlNode* node, int indent) {
    if (!node) return;

    for (int i = 0; i < indent; i++) printf("  ");

    if (node->type == HTML_NODE_TEXT) {
        printf("TEXT: \"%s\"\n", node->text_content ? node->text_content : "");
    } else if (node->type == HTML_NODE_ELEMENT) {
        printf("<%s>", node->tag_name ? node->tag_name : "?");

        if (node->id) printf(" id=\"%s\"", node->id);
        if (node->class_name) printf(" class=\"%s\"", node->class_name);
        if (node->data_ir_type) printf(" data-ir-type=\"%s\"", node->data_ir_type);
        if (node->data_level > 0) printf(" data-level=\"%u\"", node->data_level);

        printf(" (%u children)\n", node->child_count);

        for (uint32_t i = 0; i < node->child_count; i++) {
            html_node_print(node->children[i], indent + 1);
        }
    } else if (node->type == HTML_NODE_COMMENT) {
        printf("<!-- comment -->\n");
    }
}
