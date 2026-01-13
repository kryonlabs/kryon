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
    if (strcmp(tag_name, "textarea") == 0) return IR_COMPONENT_TEXTAREA;
    if (strcmp(tag_name, "select") == 0) return IR_COMPONENT_DROPDOWN;
    if (strcmp(tag_name, "img") == 0) return IR_COMPONENT_IMAGE;
    if (strcmp(tag_name, "a") == 0) return IR_COMPONENT_LINK;
    if (strcmp(tag_name, "label") == 0) return IR_COMPONENT_SPAN;  // Label maps to span (inline container)

    // Inline semantic elements (for rich text)
    if (strcmp(tag_name, "span") == 0) return IR_COMPONENT_SPAN;
    if (strcmp(tag_name, "strong") == 0 || strcmp(tag_name, "b") == 0) return IR_COMPONENT_STRONG;
    if (strcmp(tag_name, "em") == 0 || strcmp(tag_name, "i") == 0) return IR_COMPONENT_EM;
    if (strcmp(tag_name, "code") == 0) return IR_COMPONENT_CODE_INLINE;
    if (strcmp(tag_name, "small") == 0) return IR_COMPONENT_SMALL;
    if (strcmp(tag_name, "mark") == 0) return IR_COMPONENT_MARK;

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

// Parse inline CSS and apply to both style and layout
void ir_html_parse_inline_css_full(const char* style_str, IRStyle* ir_style, IRLayout* ir_layout) {
    if (!style_str) return;

    // Use the dedicated CSS parser from ir_css_parser.c
    uint32_t prop_count = 0;
    CSSProperty* props = ir_css_parse_inline(style_str, &prop_count);

    if (props && prop_count > 0) {
        if (ir_style) {
            ir_css_apply_to_style(ir_style, props, prop_count);
        }
        if (ir_layout) {
            ir_css_apply_to_layout(ir_layout, props, prop_count);
        }
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
            // Skip non-visible HTML elements
            if (node->tag_name) {
                if (strcmp(node->tag_name, "head") == 0 ||
                    strcmp(node->tag_name, "meta") == 0 ||
                    strcmp(node->tag_name, "title") == 0 ||
                    strcmp(node->tag_name, "script") == 0 ||
                    strcmp(node->tag_name, "link") == 0 ||
                    strcmp(node->tag_name, "style") == 0 ||
                    strcmp(node->tag_name, "noscript") == 0) {
                    return NULL;  // Skip these elements entirely
                }
            }

            // Handle <span class="text"> as Text component (unwrap to text content)
            if (node->tag_name && strcmp(node->tag_name, "span") == 0 &&
                node->class_name && strstr(node->class_name, "text") != NULL) {
                // Extract text content from children
                for (uint32_t i = 0; i < node->child_count; i++) {
                    if (node->children[i]->type == HTML_NODE_TEXT) {
                        return ir_text(node->children[i]->text_content);
                    }
                }
            }

            // Type detection - ONLY from HTML tag or explicit data-ir-type attribute
            // CSS class names are for STYLING, never for type inference
            IRComponentType type;

            if (node->data_ir_type) {
                // Priority 1: Restore original component type from data-ir-type metadata
                type = ir_string_to_component_type(node->data_ir_type);
            } else {
                // Priority 2: Map HTML tag to component type
                // This is the ONLY source of truth for element type
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

                case IR_COMPONENT_TABLE_CELL: {
                    // Parse colspan and rowspan attributes
                    const char* colspan_str = html_node_get_attribute(node, "colspan");
                    const char* rowspan_str = html_node_get_attribute(node, "rowspan");
                    int colspan = colspan_str ? atoi(colspan_str) : 1;
                    int rowspan = rowspan_str ? atoi(rowspan_str) : 1;
                    // Clamp to reasonable values
                    if (colspan < 1) colspan = 1;
                    if (colspan > 100) colspan = 100;
                    if (rowspan < 1) rowspan = 1;
                    if (rowspan > 100) rowspan = 100;
                    component = ir_table_cell((uint8_t)colspan, (uint8_t)rowspan);
                    break;
                }

                case IR_COMPONENT_TABLE_HEADER_CELL: {
                    // Parse colspan and rowspan attributes
                    const char* colspan_str = html_node_get_attribute(node, "colspan");
                    const char* rowspan_str = html_node_get_attribute(node, "rowspan");
                    int colspan = colspan_str ? atoi(colspan_str) : 1;
                    int rowspan = rowspan_str ? atoi(rowspan_str) : 1;
                    // Clamp to reasonable values
                    if (colspan < 1) colspan = 1;
                    if (colspan > 100) colspan = 100;
                    if (rowspan < 1) rowspan = 1;
                    if (rowspan > 100) rowspan = 100;
                    component = ir_table_header_cell((uint8_t)colspan, (uint8_t)rowspan);
                    break;
                }

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

                case IR_COMPONENT_TEXTAREA: {
                    const char* placeholder = node->placeholder ? node->placeholder : "";
                    uint32_t rows = node->rows > 0 ? node->rows : 3;  // Default 3 rows
                    uint32_t cols = node->cols > 0 ? node->cols : 40;  // Default 40 cols
                    component = ir_textarea(placeholder, rows, cols);
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
                    // Set image src (stored in custom_data)
                    if (node->src) {
                        component->custom_data = strdup(node->src);
                    }
                    // Set image alt text (stored in text_content)
                    if (node->alt) {
                        component->text_content = strdup(node->alt);
                    }
                    break;
                }

                case IR_COMPONENT_LINK: {
                    const char* url = node->href ? node->href : "";
                    const char* text = "";

                    // Extract text from children - handle both direct text and wrapped text
                    // But only use simple text if there's just one text child
                    bool has_complex_children = false;
                    if (node->child_count > 0) {
                        HtmlNode* child = node->children[0];

                        // Case 1: Direct text child (only child)
                        if (node->child_count == 1 && child->type == HTML_NODE_TEXT && child->text_content) {
                            text = child->text_content;
                        }
                        // Case 2: Element child (like <span class="text">) - look for text inside
                        else if (node->child_count == 1 && child->type == HTML_NODE_ELEMENT && child->child_count > 0) {
                            for (uint32_t i = 0; i < child->child_count; i++) {
                                if (child->children[i]->type == HTML_NODE_TEXT &&
                                    child->children[i]->text_content) {
                                    text = child->children[i]->text_content;
                                    break;
                                }
                            }
                        }
                        // Case 3: Multiple children or complex content (images, etc.)
                        else {
                            has_complex_children = true;
                        }
                    }

                    component = ir_link(url, text);

                    // Set target and rel attributes if present
                    if (component) {
                        if (node->target) {
                            ir_set_link_target(component, node->target);
                        }
                        if (node->rel) {
                            ir_set_link_rel(component, node->rel);
                        }
                    }

                    // If there are complex children (images, etc.), process them
                    if (has_complex_children && component) {
                        for (uint32_t i = 0; i < node->child_count; i++) {
                            IRComponent* child_comp = ir_html_node_to_component(node->children[i]);
                            if (child_comp) {
                                ir_add_child(component, child_comp);
                            }
                        }
                    }
                    break;
                }

                default:
                    // Use the type from data-ir-type (or tag mapping) to create component
                    // This handles Column, Row, Button, Input, and other common types
                    component = ir_create_component(type);
                    break;
            }

            // Preserve semantic HTML tag AND CSS class for roundtrip generation
            if (component && node->tag_name) {
                // Check if this is a semantic HTML tag that should be preserved
                bool is_semantic = (strcmp(node->tag_name, "header") == 0 ||
                                   strcmp(node->tag_name, "footer") == 0 ||
                                   strcmp(node->tag_name, "section") == 0 ||
                                   strcmp(node->tag_name, "nav") == 0 ||
                                   strcmp(node->tag_name, "main") == 0 ||
                                   strcmp(node->tag_name, "article") == 0 ||
                                   strcmp(node->tag_name, "aside") == 0);

                if (is_semantic) {
                    // For semantic tags, store the tag name
                    component->tag = strdup(node->tag_name);
                    // Also store the class name separately (for CSS styling)
                    if (node->class_name && node->class_name[0] != '\0') {
                        component->css_class = strdup(node->class_name);
                        // If has class, CSS should use class selector (.hero)
                        component->selector_type = IR_SELECTOR_CLASS;
                    } else {
                        // No class, CSS should use element selector (header)
                        component->selector_type = IR_SELECTOR_ELEMENT;
                    }
                } else if (node->class_name && node->class_name[0] != '\0') {
                    // For non-semantic tags with class, don't set tag (it's not a semantic tag)
                    // Only set css_class for styling
                    component->tag = NULL;  // Don't store class name as tag
                    component->css_class = strdup(node->class_name);
                    component->selector_type = IR_SELECTOR_CLASS;
                } else if (strcmp(node->tag_name, "div") != 0) {
                    // Fall back to tag name (but skip generic "div")
                    component->tag = strdup(node->tag_name);
                    // Generic elements like span, a, etc use element selector
                    component->selector_type = IR_SELECTOR_ELEMENT;
                }
            }

            // Parse inline CSS styles and layout
            if (node->style && component) {
                IRStyle* style = ir_get_style(component);  // Ensures style is created
                IRLayout* layout = ir_get_layout(component);  // Ensures layout is created
                ir_html_parse_inline_css_full(node->style, style, layout);
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
                type != IR_COMPONENT_BUTTON && type != IR_COMPONENT_CODE_BLOCK &&
                type != IR_COMPONENT_LINK) {
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
