/*
 * ir_markdown_to_ir.c
 *
 * Converts parsed markdown AST to Kryon IR components.
 * This enables:
 * - Native rendering on SDL3/terminal via existing component renderers
 * - Web rendering via the web build target's HTML generation
 * - Markdown → .kry conversion by serializing the IR to KIR format
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ir_markdown_ast.h"
#include "ir_builder.h"
#include "ir_core.h"
#include "ir_flowchart_parser.h"

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Create a text component with content
static IRComponent* create_text_component(const char* text, size_t length) {
    fprintf(stderr, "=== create_text_component: text='%.*s', length=%zu\n", (int)length, text, length);
    fflush(stderr);

    IRComponent* comp = ir_text("");
    fprintf(stderr, "=== create_text_component: ir_text returned %p\n", (void*)comp);
    fflush(stderr);

    // CRITICAL: Initialize style to prevent NULL pointer crashes during layout
    if (comp) {
        IRStyle* style = ir_get_style(comp);
        fprintf(stderr, "=== create_text_component: ir_get_style returned %p\n", (void*)style);
        fflush(stderr);
    }

    if (comp && text && length > 0) {
        char* content = (char*)malloc(length + 1);
        fprintf(stderr, "=== create_text_component: malloc returned %p\n", (void*)content);
        fflush(stderr);

        if (content) {
            memcpy(content, text, length);
            content[length] = '\0';
            fprintf(stderr, "=== create_text_component: calling ir_set_text_content...\n");
            fflush(stderr);
            ir_set_text_content(comp, content);
            // Don't free! ir_set_text_content() takes ownership of the pointer
            fprintf(stderr, "=== create_text_component: content ownership transferred\n");
            fflush(stderr);
        }
    }
    fprintf(stderr, "=== create_text_component: returning %p\n", (void*)comp);
    fflush(stderr);
    return comp;
}

// Convert markdown alignment to IR alignment
static IRAlignment md_align_to_ir(MdAlignment align) {
    switch (align) {
        case MD_ALIGN_CENTER: return IR_ALIGNMENT_CENTER;
        case MD_ALIGN_RIGHT: return IR_ALIGNMENT_END;
        case MD_ALIGN_LEFT:
        case MD_ALIGN_NONE:
        default: return IR_ALIGNMENT_START;
    }
}

// ============================================================================
// INLINE RENDERING TO IR
// ============================================================================

// Convert inline markdown elements to a single text component
// For simplicity, we flatten inlines to plain text for now
// TODO: Support nested inline formatting via multiple Text components
static void render_inlines_to_ir(MdInline* inl, IRComponent* parent) {
    while (inl) {
        IRComponent* comp = NULL;

        switch (inl->type) {
            case MD_INLINE_TEXT:
                comp = create_text_component(inl->data.text.text, inl->data.text.length);
                if (comp && comp->style) {
                    // Set default readable text color (light gray for dark theme)
                    ir_set_font(comp->style, 16.0f, NULL, 230, 237, 243, 255, false, false);
                }
                break;

            case MD_INLINE_CODE_SPAN:
                comp = create_text_component(inl->data.text.text, inl->data.text.length);
                if (comp && comp->style) {
                    // Style as inline code
                    ir_set_font(comp->style, 14.0f, "monospace", 121, 192, 255, 255, false, false);
                    ir_set_background_color(comp->style, 22, 27, 34, 255);
                    ir_set_padding(comp, 2, 6, 2, 6);
                }
                break;

            case MD_INLINE_EMPHASIS:
                comp = create_text_component(inl->data.text.text, inl->data.text.length);
                if (comp && comp->style) {
                    ir_set_font(comp->style, 16.0f, NULL, 201, 209, 217, 255, false, true);
                }
                break;

            case MD_INLINE_STRONG:
                comp = create_text_component(inl->data.text.text, inl->data.text.length);
                if (comp && comp->style) {
                    ir_set_font(comp->style, 16.0f, NULL, 230, 237, 243, 255, true, false);
                }
                break;

            case MD_INLINE_STRIKETHROUGH:
                comp = create_text_component(inl->data.text.text, inl->data.text.length);
                if (comp && comp->style) {
                    ir_set_text_decoration(comp->style, 0x02); // strikethrough flag
                }
                break;

            case MD_INLINE_LINK:
                // Links are rendered as styled text with URL in custom_data
                // (IR doesn't have a dedicated LINK component type)
                comp = ir_text("");
                if (comp) {
                    // Set link URL as custom data
                    if (inl->data.link.url && inl->data.link.url_length > 0) {
                        char* url = (char*)malloc(inl->data.link.url_length + 1);
                        if (url) {
                            memcpy(url, inl->data.link.url, inl->data.link.url_length);
                            url[inl->data.link.url_length] = '\0';
                            ir_set_custom_data(comp, url);
                            ir_set_text_content(comp, url);
                            free(url);
                        }
                    }
                    if (comp->style) {
                        // Style as link with blue color
                        ir_set_font(comp->style, 16.0f, NULL, 88, 166, 255, 255, false, false);
                    }
                    // Mark as link via tag
                    ir_set_tag(comp, "link");
                }
                break;

            case MD_INLINE_IMAGE:
                comp = ir_create_component(IR_COMPONENT_IMAGE);
                if (comp) {
                    if (inl->data.image.url && inl->data.image.url_length > 0) {
                        char* url = (char*)malloc(inl->data.image.url_length + 1);
                        if (url) {
                            memcpy(url, inl->data.image.url, inl->data.image.url_length);
                            url[inl->data.image.url_length] = '\0';
                            ir_set_custom_data(comp, url);
                            free(url);
                        }
                    }
                    if (inl->data.image.alt && inl->data.image.alt_length > 0) {
                        char* alt = (char*)malloc(inl->data.image.alt_length + 1);
                        if (alt) {
                            memcpy(alt, inl->data.image.alt, inl->data.image.alt_length);
                            alt[inl->data.image.alt_length] = '\0';
                            ir_set_text_content(comp, alt);
                            free(alt);
                        }
                    }
                }
                break;

            case MD_INLINE_LINE_BREAK:
            case MD_INLINE_SOFT_BREAK:
                // Add line break by creating a new row
                comp = ir_row();
                break;

            case MD_INLINE_RAW_HTML:
                // Pass through as text for now
                comp = create_text_component(inl->data.text.text, inl->data.text.length);
                break;

            default:
                break;
        }

        if (comp && parent) {
            ir_add_child(parent, comp);
        }

        inl = inl->next;
    }
}

// ============================================================================
// NODE TO IR CONVERSION
// ============================================================================

// Forward declaration
static IRComponent* md_node_to_ir(MdNode* node);

// Convert children nodes to IR and add to parent
static void convert_children_to_ir(MdNode* parent_node, IRComponent* parent_comp) {
    MdNode* child = parent_node->first_child;
    while (child) {
        IRComponent* child_comp = md_node_to_ir(child);
        if (child_comp && parent_comp) {
            ir_add_child(parent_comp, child_comp);
        }
        child = child->next;
    }
}

// Find the parent table node to get alignment info
static MdNode* find_parent_table_node(MdNode* node) {
    while (node) {
        if (node->type == MD_BLOCK_TABLE) return node;
        node = node->parent;
    }
    return NULL;
}

// Get the column index of a cell within its row
static int get_cell_column_idx(MdNode* cell) {
    int index = 0;
    MdNode* sibling = cell->prev;
    while (sibling) {
        if (sibling->type == MD_BLOCK_TABLE_CELL) index++;
        sibling = sibling->prev;
    }
    return index;
}

// Check if this row is a header row (first row in table)
static bool is_header_row(MdNode* row) {
    if (!row || !row->parent) return false;
    // First child of table is header row
    return row->prev == NULL && row->parent->type == MD_BLOCK_TABLE;
}

static IRComponent* md_node_to_ir(MdNode* node) {
    if (!node) return NULL;

    IRComponent* comp = NULL;

    switch (node->type) {
        case MD_BLOCK_DOCUMENT: {
            // Root container with column layout
            comp = ir_column();
            if (comp && comp->style) {
                ir_set_padding(comp, 20, 20, 20, 20);
                ir_set_font(comp->style, 16.0f, NULL, 230, 237, 243, 255, false, false);
            }
            if (comp && comp->layout) {
                ir_set_flexbox(comp->layout, false, 16, IR_ALIGNMENT_START, IR_ALIGNMENT_START);
            }
            convert_children_to_ir(node, comp);
            break;
        }

        case MD_BLOCK_HEADING_1:
        case MD_BLOCK_HEADING_2:
        case MD_BLOCK_HEADING_3:
        case MD_BLOCK_HEADING_4:
        case MD_BLOCK_HEADING_5:
        case MD_BLOCK_HEADING_6: {
            // Extract heading text
            char* text = NULL;
            if (node->data.heading.text && node->data.heading.length > 0) {
                text = (char*)malloc(node->data.heading.length + 1);
                if (text) {
                    memcpy(text, node->data.heading.text, node->data.heading.length);
                    text[node->data.heading.length] = '\0';
                }
            }

            // Calculate heading level (1-6)
            uint8_t level = (uint8_t)(node->type - MD_BLOCK_HEADING_1 + 1);

            // Use specialized heading builder
            comp = ir_heading(level, text ? text : "");

            if (text) free(text);

            // Add border for H1 and H2 (like GitHub markdown)
            if (comp && level < 3 && comp->style) {
                ir_set_border(comp->style, 1, 48, 54, 61, 255, 0);
                ir_set_padding(comp, 0, 0, 12, 0);
            }

            convert_children_to_ir(node, comp);
            break;
        }

        case MD_BLOCK_PARAGRAPH: {
            // Use specialized paragraph builder
            comp = ir_paragraph();

            // Add inline elements (includes text, bold, italic, etc.)
            // Note: Don't use node->data.paragraph.text directly as it duplicates
            // the content already present in first_inline as MD_INLINE_TEXT nodes
            if (comp && node->data.paragraph.first_inline) {
                render_inlines_to_ir(node->data.paragraph.first_inline, comp);
            }

            convert_children_to_ir(node, comp);
            break;
        }

        case MD_BLOCK_LIST_UNORDERED: {
            // Use specialized list builder
            comp = ir_list(IR_LIST_UNORDERED);

            // Add left padding for nested list appearance
            if (comp && comp->style) {
                ir_set_padding(comp, 0, 0, 0, 32);
            }

            convert_children_to_ir(node, comp);
            break;
        }

        case MD_BLOCK_LIST_ORDERED: {
            // Use specialized list builder
            comp = ir_list(IR_LIST_ORDERED);

            // Add left padding for nested list appearance
            if (comp && comp->style) {
                ir_set_padding(comp, 0, 0, 0, 32);
            }

            convert_children_to_ir(node, comp);
            break;
        }

        case MD_BLOCK_LIST_ITEM: {
            // Use specialized list item builder
            comp = ir_list_item();

            // Inline elements will be added as children
            convert_children_to_ir(node, comp);
            break;
        }

        case MD_BLOCK_CODE_BLOCK: {
            const char* lang = node->data.code_block.language;
            const char* code = node->data.code_block.code;
            uint16_t code_len = node->data.code_block.length;

            // Check if this is a mermaid diagram
            if (lang && code && code_len > 0 &&
                (strcmp(lang, "mermaid") == 0 || strcmp(lang, "mmd") == 0)) {
                // Parse mermaid → native Kryon Flowchart component
                comp = ir_flowchart_parse(code, code_len);
                if (comp) {
                    // Add margin to match other block elements
                    if (comp->style) {
                        ir_set_margin(comp, 16, 0, 16, 0);
                    }
                    break;
                }
                // If parse failed, fall through to render as code block
            }

            // Use specialized code block builder
            comp = ir_code_block(lang, code);

            // Additional styling (builder sets defaults)
            if (comp && comp->style) {
                ir_set_border(comp->style, 1, 48, 54, 61, 255, 8);
            }

            break;
        }

        case MD_BLOCK_BLOCKQUOTE: {
            // Use specialized blockquote builder
            comp = ir_blockquote();

            // Additional blockquote styling (left border accent)
            if (comp && comp->style) {
                ir_set_border(comp->style, 4, 0, 168, 255, 255, 0);
                ir_set_padding(comp, 0, 0, 0, 16);
            }

            // Add blockquote content
            if (node->data.blockquote.text && node->data.blockquote.length > 0) {
                IRComponent* text = create_text_component(node->data.blockquote.text, node->data.blockquote.length);
                if (text && text->style) {
                    ir_set_font(text->style, 16.0f, NULL, 139, 148, 158, 255, false, false);
                }
                if (text) {
                    ir_add_child(comp, text);
                }
            }
            convert_children_to_ir(node, comp);
            break;
        }

        case MD_BLOCK_THEMATIC_BREAK: {
            // Use specialized horizontal rule builder
            comp = ir_horizontal_rule();
            break;
        }

        // ================================================================
        // TABLE COMPONENTS - Native IR Table support
        // ================================================================

        case MD_BLOCK_TABLE: {
            comp = ir_table();
            if (!comp) break;

            // Create TableHead and TableBody
            IRComponent* thead = ir_table_head();
            IRComponent* tbody = ir_table_body();

            if (thead && tbody) {
                ir_add_child(comp, thead);
                ir_add_child(comp, tbody);

                // Process rows - first row goes to thead, rest to tbody
                MdNode* row = node->first_child;
                bool first_row = true;

                while (row) {
                    if (row->type == MD_BLOCK_TABLE_ROW) {
                        IRComponent* row_comp = ir_table_row();
                        if (row_comp) {
                            // Process cells in this row
                            MdNode* cell = row->first_child;
                            int col_idx = 0;

                            while (cell) {
                                if (cell->type == MD_BLOCK_TABLE_CELL) {
                                    // Get alignment for this column
                                    IRAlignment align = IR_ALIGNMENT_START;
                                    if (node->data.table.alignments && col_idx < node->data.table.columns) {
                                        align = md_align_to_ir(node->data.table.alignments[col_idx]);
                                    }

                                    // Create header or data cell
                                    IRComponent* cell_comp;
                                    if (first_row) {
                                        cell_comp = ir_table_header_cell(1, 1);
                                    } else {
                                        cell_comp = ir_table_cell(1, 1);
                                    }

                                    if (cell_comp) {
                                        // Set cell alignment
                                        ir_table_cell_set_alignment(cell_comp, align);

                                        // Add cell content from children
                                        convert_children_to_ir(cell, cell_comp);

                                        ir_add_child(row_comp, cell_comp);
                                    }
                                    col_idx++;
                                }
                                cell = cell->next;
                            }

                            // Add row to appropriate section
                            if (first_row) {
                                ir_add_child(thead, row_comp);
                            } else {
                                ir_add_child(tbody, row_comp);
                            }
                        }
                    }

                    first_row = false;
                    row = row->next;
                }
            }

            // Finalize the table
            ir_table_finalize(comp);
            break;
        }

        case MD_BLOCK_TABLE_ROW: {
            // Rows are handled by the parent TABLE case
            // This case handles orphan rows
            comp = ir_table_row();
            convert_children_to_ir(node, comp);
            break;
        }

        case MD_BLOCK_TABLE_CELL: {
            // Get alignment from parent table
            MdNode* table = find_parent_table_node(node);
            int col_idx = get_cell_column_idx(node);
            IRAlignment align = IR_ALIGNMENT_START;

            if (table && table->data.table.alignments && col_idx < table->data.table.columns) {
                align = md_align_to_ir(table->data.table.alignments[col_idx]);
            }

            // Check if this is a header cell (first row)
            bool is_header = is_header_row(node->parent);

            if (is_header) {
                comp = ir_table_header_cell(1, 1);
            } else {
                comp = ir_table_cell(1, 1);
            }

            if (comp) {
                ir_table_cell_set_alignment(comp, align);
            }

            convert_children_to_ir(node, comp);
            break;
        }

        case MD_BLOCK_HTML_BLOCK: {
            // Pass through HTML blocks as container with text
            comp = ir_container(NULL);
            convert_children_to_ir(node, comp);
            break;
        }

        default:
            // Unknown block type - create container
            comp = ir_container(NULL);
            convert_children_to_ir(node, comp);
            break;
    }

    return comp;
}

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * Convert markdown AST to IR component tree.
 * The returned component must be freed with ir_destroy_component().
 *
 * @param ast_root Root node of the markdown AST
 * @return IR component tree or NULL on error
 */
IRComponent* ir_markdown_ast_to_ir(MdNode* ast_root) {
    if (!ast_root) return NULL;

    // Convert AST to IR
    IRComponent* ir_root = md_node_to_ir(ast_root);

    return ir_root;
}
