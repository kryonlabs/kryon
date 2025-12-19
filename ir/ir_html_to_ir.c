#define _POSIX_C_SOURCE 200809L
#include "ir_html_to_ir.h"
#include "ir_builder.h"
#include "ir_css_parser.h"
#include "ir_serialization.h"  // For ir_string_to_component_type
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

    // Default
    return IR_COMPONENT_CONTAINER;
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
            // Use data-ir-type if available (preserves original type)
            IRComponentType type;
            if (node->data_ir_type) {
                // Restore original component type from metadata
                type = ir_string_to_component_type(node->data_ir_type);
                printf("  [html→ir] Restored type from data-ir-type: %s → %d\n", node->data_ir_type, type);
            } else {
                // Fall back to HTML tag mapping
                type = ir_html_tag_to_component_type(node->tag_name);
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
                    if (component && node->tag_name) {
                        component->tag = strdup(node->tag_name);
                    }
                    break;
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
        return ir_html_node_to_component(html_root->children[0]);
    }

    return ir_html_node_to_component(html_root);
}
