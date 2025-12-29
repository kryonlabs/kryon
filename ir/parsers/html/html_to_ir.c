#define _POSIX_C_SOURCE 200809L
#include "html_to_ir.h"
#include "../../ir_builder.h"
#include "css_parser.h"
#include "../../ir_serialization.h"  // For ir_string_to_component_type
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================================
// HTML Tag to IR Component Type Mapping
// ============================================================================

IRComponentType ir_html_tag_to_component_type(const char* tag_name) {
    if (!tag_name) return IR_COMPONENT_CONTAINER;

    // Semantic HTML tags
    if (strcmp(tag_name, "h1") == 0 || strcmp(tag_name, "h2") == 0 ||
        strcmp(tag_name, "h3") == 0 || strcmp(tag_name, "h4") == 0 ||
        strcmp(tag_name, "h5") == 0 || strcmp(tag_name, "h6") == 0) {
        return IR_COMPONENT_HEADING;
    }

    if (strcmp(tag_name, "p") == 0) return IR_COMPONENT_PARAGRAPH;
    if (strcmp(tag_name, "blockquote") == 0) return IR_COMPONENT_BLOCKQUOTE;
    if (strcmp(tag_name, "pre") == 0) return IR_COMPONENT_CODE_BLOCK;
    if (strcmp(tag_name, "hr") == 0) return IR_COMPONENT_HORIZONTAL_RULE;

    if (strcmp(tag_name, "ul") == 0) return IR_COMPONENT_LIST;
    if (strcmp(tag_name, "ol") == 0) return IR_COMPONENT_LIST;
    if (strcmp(tag_name, "li") == 0) return IR_COMPONENT_LIST_ITEM;

    if (strcmp(tag_name, "table") == 0) return IR_COMPONENT_TABLE;
    if (strcmp(tag_name, "thead") == 0) return IR_COMPONENT_TABLE_HEAD;
    if (strcmp(tag_name, "tbody") == 0) return IR_COMPONENT_TABLE_BODY;
    if (strcmp(tag_name, "tfoot") == 0) return IR_COMPONENT_TABLE_FOOT;
    if (strcmp(tag_name, "tr") == 0) return IR_COMPONENT_TABLE_ROW;
    if (strcmp(tag_name, "td") == 0) return IR_COMPONENT_TABLE_CELL;
    if (strcmp(tag_name, "th") == 0) return IR_COMPONENT_TABLE_HEADER_CELL;

    // UI components
    if (strcmp(tag_name, "button") == 0) return IR_COMPONENT_BUTTON;
    if (strcmp(tag_name, "input") == 0) return IR_COMPONENT_INPUT;
    if (strcmp(tag_name, "select") == 0) return IR_COMPONENT_DROPDOWN;
    if (strcmp(tag_name, "img") == 0) return IR_COMPONENT_IMAGE;
    if (strcmp(tag_name, "a") == 0) return IR_COMPONENT_LINK;

    // Layout containers
    if (strcmp(tag_name, "div") == 0) return IR_COMPONENT_CONTAINER;

    // HTML5 semantic elements (all map to CONTAINER)
    if (strcmp(tag_name, "main") == 0) return IR_COMPONENT_CONTAINER;
    if (strcmp(tag_name, "section") == 0) return IR_COMPONENT_CONTAINER;
    if (strcmp(tag_name, "article") == 0) return IR_COMPONENT_CONTAINER;
    if (strcmp(tag_name, "nav") == 0) return IR_COMPONENT_CONTAINER;
    if (strcmp(tag_name, "header") == 0) return IR_COMPONENT_CONTAINER;
    if (strcmp(tag_name, "footer") == 0) return IR_COMPONENT_CONTAINER;
    if (strcmp(tag_name, "aside") == 0) return IR_COMPONENT_CONTAINER;
    if (strcmp(tag_name, "figure") == 0) return IR_COMPONENT_CONTAINER;
    if (strcmp(tag_name, "figcaption") == 0) return IR_COMPONENT_CONTAINER;

    // Default
    return IR_COMPONENT_CONTAINER;
}

// ============================================================================
// Natural Class Name Inference
// ============================================================================

/**
 * Infer IR component type from CSS class name
 *
 * Allows natural HTML like:
 *   <div class="row"> → IR_COMPONENT_ROW
 *   <div class="column"> → IR_COMPONENT_COLUMN
 *   <div class="center"> → IR_COMPONENT_CENTER
 *   <div class="button primary"> → IR_COMPONENT_BUTTON
 */
IRComponentType ir_infer_type_from_classes(const char* class_string) {
    if (!class_string || class_string[0] == '\0') {
        return IR_COMPONENT_CONTAINER;
    }

    // Check for exact matches first (highest priority)
    if (strcmp(class_string, "row") == 0) return IR_COMPONENT_ROW;
    if (strcmp(class_string, "column") == 0) return IR_COMPONENT_COLUMN;
    if (strcmp(class_string, "col") == 0) return IR_COMPONENT_COLUMN;
    if (strcmp(class_string, "center") == 0) return IR_COMPONENT_CENTER;
    if (strcmp(class_string, "button") == 0) return IR_COMPONENT_BUTTON;

    // Check for partial matches (class contains keyword)
    // Use case-insensitive matching for flexibility
    char* class_lower = strdup(class_string);
    if (!class_lower) return IR_COMPONENT_CONTAINER;

    for (char* p = class_lower; *p; p++) {
        *p = tolower(*p);
    }

    IRComponentType result = IR_COMPONENT_CONTAINER;

    // Check for layout hints in class name
    if (strstr(class_lower, "row") != NULL && strstr(class_lower, "column") == NULL) {
        result = IR_COMPONENT_ROW;
    } else if (strstr(class_lower, "column") != NULL || strstr(class_lower, "col") != NULL) {
        result = IR_COMPONENT_COLUMN;
    } else if (strstr(class_lower, "center") != NULL) {
        result = IR_COMPONENT_CENTER;
    } else if (strstr(class_lower, "button") != NULL || strstr(class_lower, "btn") != NULL) {
        result = IR_COMPONENT_BUTTON;
    }

    free(class_lower);
    return result;
}

// ============================================================================
// CSS Parser Helper
// ============================================================================

void ir_html_parse_inline_css(const char* style_str, IRStyle* ir_style) {
    if (!style_str || !ir_style) return;

    // Use the dedicated CSS parser from ir_css_parser.c
    uint32_t prop_count = 0;
    CSSProperty* props = ir_css_parse_inline(style_str, &prop_count);

    if (props && prop_count > 0) {
        ir_css_apply_to_style(ir_style, props, prop_count);
        ir_css_free_properties(props, prop_count);
    }
}

// ============================================================================
// HTML to IR Conversion
// ============================================================================

IRComponent* ir_html_node_to_component(HtmlNode* node) {
    if (!node) return NULL;

    IRComponent* component = NULL;

    switch (node->type) {
        case HTML_NODE_TEXT: {
            // Create text component
            component = ir_text(node->text_content);
            break;
        }

        case HTML_NODE_ELEMENT: {
            // 3-tier type detection system:
            // Priority 1: data-ir-type attribute (for roundtrip preservation)
            // Priority 2: HTML tag name (<button>, <h1>, etc.)
            // Priority 3: CSS class name inference (class="row", class="column")
            IRComponentType type;

            if (node->data_ir_type) {
                // Tier 1: Restore original component type from metadata
                type = ir_string_to_component_type(node->data_ir_type);
                printf("  [html→ir] Restored type from data-ir-type: %s → %d\n", node->data_ir_type, type);
            } else {
                // Tier 2: Map HTML tag to component type
                type = ir_html_tag_to_component_type(node->tag_name);

                // Tier 3: If tag is ambiguous (div), check class for hints
                if (type == IR_COMPONENT_CONTAINER && node->class_name) {
                    IRComponentType inferred = ir_infer_type_from_classes(node->class_name);
                    if (inferred != IR_COMPONENT_CONTAINER) {
                        type = inferred;
                        printf("  [html→ir] Inferred type from class '%s' → %d\n", node->class_name, type);
                    }
                }
            }

            switch (type) {
                case IR_COMPONENT_HEADING: {
                    // Extract heading level from tag name (h1-h6) or data-level
                    uint8_t level = 1;
                    if (node->data_level > 0) {
                        level = (uint8_t)node->data_level;
                    } else if (node->tag_name && strlen(node->tag_name) == 2 &&
                               node->tag_name[0] == 'h' && isdigit(node->tag_name[1])) {
                        level = node->tag_name[1] - '0';
                    }

                    // Extract text content from children
                    const char* text = "";
                    if (node->child_count > 0 && node->children[0]->type == HTML_NODE_TEXT) {
                        text = node->children[0]->text_content;
                    }

                    component = ir_heading(level, text);
                    break;
                }

                case IR_COMPONENT_PARAGRAPH:
                    component = ir_paragraph();
                    break;

                case IR_COMPONENT_BLOCKQUOTE:
                    component = ir_blockquote();
                    break;

                case IR_COMPONENT_CODE_BLOCK: {
                    // Extract language and code
                    const char* language = "";
                    const char* code = "";
                    if (node->child_count > 0) {
                        HtmlNode* code_node = node->children[0];
                        if (code_node->tag_name && strcmp(code_node->tag_name, "code") == 0) {
                            if (code_node->class_name) {
                                // Extract language from class (e.g., "language-python")
                                if (strncmp(code_node->class_name, "language-", 9) == 0) {
                                    language = code_node->class_name + 9;
                                }
                            }
                            if (code_node->child_count > 0 && code_node->children[0]->type == HTML_NODE_TEXT) {
                                code = code_node->children[0]->text_content;
                            }
                        }
                    }
                    component = ir_code_block(language, code);
                    break;
                }

                case IR_COMPONENT_HORIZONTAL_RULE:
                    component = ir_horizontal_rule();
                    break;

                case IR_COMPONENT_LIST: {
                    // Determine list type from tag (ul/ol)
                    IRListType list_type = IR_LIST_UNORDERED;
                    if (node->tag_name && strcmp(node->tag_name, "ol") == 0) {
                        list_type = IR_LIST_ORDERED;
                    }
                    component = ir_list(list_type);
                    break;
                }

                case IR_COMPONENT_LIST_ITEM:
                    component = ir_list_item();
                    break;

                case IR_COMPONENT_TABLE:
                    component = ir_table();
                    break;

                case IR_COMPONENT_TABLE_HEAD:
                    component = ir_table_head();
                    break;

                case IR_COMPONENT_TABLE_BODY:
                    component = ir_table_body();
                    break;

                case IR_COMPONENT_TABLE_FOOT:
                    component = ir_table_foot();
                    break;

                case IR_COMPONENT_TABLE_ROW:
                    component = ir_table_row();
                    break;

                case IR_COMPONENT_TABLE_CELL:
                    component = ir_table_cell(1, 1);  // TODO: Parse colspan/rowspan
                    break;

                case IR_COMPONENT_TABLE_HEADER_CELL:
                    component = ir_table_header_cell(1, 1);
                    break;

                case IR_COMPONENT_BUTTON: {
                    // Extract button text from children
                    const char* text = "";
                    if (node->child_count > 0 && node->children[0]->type == HTML_NODE_TEXT) {
                        text = node->children[0]->text_content;
                    }
                    component = ir_button(text);
                    break;
                }

                case IR_COMPONENT_INPUT: {
                    const char* placeholder = node->value ? node->value : "";
                    component = ir_input(placeholder);
                    break;
                }

                case IR_COMPONENT_CHECKBOX: {
                    const char* label = "";
                    component = ir_checkbox(label);
                    if (node->checked || node->data_checked) {
                        // TODO: Set checkbox state
                    }
                    break;
                }

                case IR_COMPONENT_IMAGE: {
                    component = ir_create_component(IR_COMPONENT_IMAGE);
                    // TODO: Set image src, alt
                    break;
                }

                case IR_COMPONENT_LINK: {
                    const char* url = node->href ? node->href : "";
                    const char* text = "";
                    if (node->child_count > 0 && node->children[0]->type == HTML_NODE_TEXT) {
                        text = node->children[0]->text_content;
                    }
                    component = ir_link(url, text);
                    break;
                }

                default:
                    // Use the type from data-ir-type (or tag mapping) to create component
                    // This handles Column, Row, Button, Input, and other common types
                    component = ir_create_component(type);
                    break;
            }

            // Preserve custom class name in component->tag (for CSS generation)
            // Priority: class name > tag name
            if (component) {
                if (node->class_name && node->class_name[0] != '\0') {
                    // Use class name for custom styling
                    component->tag = strdup(node->class_name);
                } else if (node->tag_name && strcmp(node->tag_name, "div") != 0) {
                    // Fall back to tag name (but skip generic "div")
                    component->tag = strdup(node->tag_name);
                }
            }

            // Parse inline CSS styles
            if (node->style && component) {
                IRStyle* style = ir_get_style(component);  // Ensures style is created
                ir_html_parse_inline_css(node->style, style);
            }

            // Set component ID (if data-ir-id is present)
            if (node->data_ir_id > 0 && component) {
                component->id = node->data_ir_id;
            }
            // Also extract ID from id="kryon-N" attribute
            else if (component && node->id && strncmp(node->id, "kryon-", 6) == 0) {
                uint32_t id = (uint32_t)atoi(node->id + 6);
                if (id > 0) {
                    component->id = id;
                }
            }

            // Recursively convert children (except for components that already consumed their children)
            if (component && type != IR_COMPONENT_HEADING &&
                type != IR_COMPONENT_BUTTON && type != IR_COMPONENT_CODE_BLOCK) {
                for (uint32_t i = 0; i < node->child_count; i++) {
                    IRComponent* child = ir_html_node_to_component(node->children[i]);
                    if (child) {
                        ir_add_child(component, child);
                    }
                }
            }

            break;
        }

        case HTML_NODE_COMMENT:
            // Ignore comments
            break;
    }

    return component;
}

IRComponent* ir_html_to_ir_convert(HtmlNode* html_root) {
    if (!html_root) return NULL;

    // Handle wrapper root node (skip if tag is "root")
    if (html_root->tag_name && strcmp(html_root->tag_name, "root") == 0 &&
        html_root->child_count == 1) {
        html_root = html_root->children[0];
    }

    // Skip <html> wrapper and find <body>
    if (html_root->tag_name && strcmp(html_root->tag_name, "html") == 0) {
        // Look for <body> tag in children
        for (uint32_t i = 0; i < html_root->child_count; i++) {
            HtmlNode* child = html_root->children[i];
            if (child && child->tag_name && strcmp(child->tag_name, "body") == 0) {
                // Convert body's children directly (skip body wrapper)
                if (child->child_count == 1) {
                    return ir_html_node_to_component(child->children[0]);
                } else if (child->child_count > 1) {
                    // Multiple children in body - wrap in a column
                    IRComponent* column = ir_column();
                    for (uint32_t j = 0; j < child->child_count; j++) {
                        IRComponent* body_child = ir_html_node_to_component(child->children[j]);
                        if (body_child) {
                            ir_add_child(column, body_child);
                        }
                    }
                    return column;
                }
            }
        }
    }

    // Skip <body> wrapper if this node is <body>
    if (html_root->tag_name && strcmp(html_root->tag_name, "body") == 0) {
        if (html_root->child_count == 1) {
            return ir_html_node_to_component(html_root->children[0]);
        } else if (html_root->child_count > 1) {
            // Multiple children - wrap in column
            IRComponent* column = ir_column();
            for (uint32_t i = 0; i < html_root->child_count; i++) {
                IRComponent* child = ir_html_node_to_component(html_root->children[i]);
                if (child) {
                    ir_add_child(column, child);
                }
            }
            return column;
        }
    }

    return ir_html_node_to_component(html_root);
}
