/**
 * @file ir_component_types.c
 * @brief Component type definitions and conversion functions
 */

#define _GNU_SOURCE
#include "../include/ir_types.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

const char* ir_component_type_to_string(IRComponentType type) {
    switch (type) {
        case IR_COMPONENT_CONTAINER: return "Container";
        case IR_COMPONENT_TEXT: return "Text";
        case IR_COMPONENT_BUTTON: return "Button";
        case IR_COMPONENT_INPUT: return "Input";
        case IR_COMPONENT_CHECKBOX: return "Checkbox";
        case IR_COMPONENT_DROPDOWN: return "Dropdown";
        case IR_COMPONENT_TEXTAREA: return "Textarea";
        case IR_COMPONENT_ROW: return "Row";
        case IR_COMPONENT_COLUMN: return "Column";
        case IR_COMPONENT_CENTER: return "Center";
        case IR_COMPONENT_IMAGE: return "Image";
        case IR_COMPONENT_CANVAS: return "Canvas";
        case IR_COMPONENT_NATIVE_CANVAS: return "NativeCanvas";
        case IR_COMPONENT_MARKDOWN: return "Markdown";
        case IR_COMPONENT_SPRITE: return "Sprite";
        case IR_COMPONENT_TAB_GROUP: return "TabGroup";
        case IR_COMPONENT_TAB_BAR: return "TabBar";
        case IR_COMPONENT_TAB: return "Tab";
        case IR_COMPONENT_TAB_CONTENT: return "TabContent";
        case IR_COMPONENT_TAB_PANEL: return "TabPanel";
        case IR_COMPONENT_MODAL: return "Modal";
        case IR_COMPONENT_TABLE: return "Table";
        case IR_COMPONENT_TABLE_HEAD: return "TableHead";
        case IR_COMPONENT_TABLE_BODY: return "TableBody";
        case IR_COMPONENT_TABLE_FOOT: return "TableFoot";
        case IR_COMPONENT_TABLE_ROW: return "TableRow";
        case IR_COMPONENT_TABLE_CELL: return "TableCell";
        case IR_COMPONENT_TABLE_HEADER_CELL: return "TableHeaderCell";
        case IR_COMPONENT_HEADING: return "Heading";
        case IR_COMPONENT_PARAGRAPH: return "Paragraph";
        case IR_COMPONENT_BLOCKQUOTE: return "Blockquote";
        case IR_COMPONENT_CODE_BLOCK: return "CodeBlock";
        case IR_COMPONENT_HORIZONTAL_RULE: return "HorizontalRule";
        case IR_COMPONENT_LIST: return "List";
        case IR_COMPONENT_LIST_ITEM: return "ListItem";
        case IR_COMPONENT_LINK: return "Link";
        case IR_COMPONENT_SPAN: return "Span";
        case IR_COMPONENT_STRONG: return "Strong";
        case IR_COMPONENT_EM: return "Em";
        case IR_COMPONENT_CODE_INLINE: return "CodeInline";
        case IR_COMPONENT_SMALL: return "Small";
        case IR_COMPONENT_MARK: return "Mark";
        case IR_COMPONENT_CUSTOM: return "Custom";
        case IR_COMPONENT_STATIC_BLOCK: return "StaticBlock";
        case IR_COMPONENT_FOR_LOOP: return "ForLoop";
        case IR_COMPONENT_FOR_EACH: return "For";
        case IR_COMPONENT_VAR_DECL: return "VarDecl";
        case IR_COMPONENT_PLACEHOLDER: return "Placeholder";
        case IR_COMPONENT_FLOWCHART: return "Flowchart";
        case IR_COMPONENT_FLOWCHART_NODE: return "FlowchartNode";
        case IR_COMPONENT_FLOWCHART_EDGE: return "FlowchartEdge";
        case IR_COMPONENT_FLOWCHART_SUBGRAPH: return "FlowchartSubgraph";
        case IR_COMPONENT_FLOWCHART_LABEL: return "FlowchartLabel";
        default: return "Unknown";
    }
}

static int str_ieq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = (char)tolower((unsigned char)*a);
        char cb = (char)tolower((unsigned char)*b);
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

IRComponentType ir_component_type_from_string(const char* str) {
    if (!str) return IR_COMPONENT_CONTAINER;

    for (int i = 0; i <= IR_COMPONENT_MAX; i++) {
        if (strcmp(ir_component_type_to_string((IRComponentType)i), str) == 0) {
            return (IRComponentType)i;
        }
    }
    return IR_COMPONENT_CONTAINER;
}

IRComponentType ir_component_type_from_string_insensitive(const char* str) {
    if (!str) return IR_COMPONENT_CONTAINER;

    for (int i = 0; i <= IR_COMPONENT_MAX; i++) {
        if (str_ieq(ir_component_type_to_string((IRComponentType)i), str)) {
            return (IRComponentType)i;
        }
    }
    return IR_COMPONENT_CONTAINER;
}

/**
 * Map snake_case component names to IRComponentType.
 * Used by the plugin capability API for string-based registration.
 *
 * Supports both snake_case ("code_block") and PascalCase ("CodeBlock").
 * Returns IR_COMPONENT_CONTAINER for unknown names (use the _valid variant
 * to check if a name is valid).
 */
IRComponentType ir_component_type_from_snake_case(const char* name) {
    if (!name) return IR_COMPONENT_CONTAINER;

    /* Try snake_case lookup first (more common in plugin API) */
    if (strcmp(name, "container") == 0) return IR_COMPONENT_CONTAINER;
    if (strcmp(name, "text") == 0) return IR_COMPONENT_TEXT;
    if (strcmp(name, "button") == 0) return IR_COMPONENT_BUTTON;
    if (strcmp(name, "input") == 0) return IR_COMPONENT_INPUT;
    if (strcmp(name, "checkbox") == 0) return IR_COMPONENT_CHECKBOX;
    if (strcmp(name, "dropdown") == 0) return IR_COMPONENT_DROPDOWN;
    if (strcmp(name, "textarea") == 0) return IR_COMPONENT_TEXTAREA;
    if (strcmp(name, "row") == 0) return IR_COMPONENT_ROW;
    if (strcmp(name, "column") == 0) return IR_COMPONENT_COLUMN;
    if (strcmp(name, "center") == 0) return IR_COMPONENT_CENTER;
    if (strcmp(name, "image") == 0) return IR_COMPONENT_IMAGE;
    if (strcmp(name, "canvas") == 0) return IR_COMPONENT_CANVAS;
    if (strcmp(name, "native_canvas") == 0) return IR_COMPONENT_NATIVE_CANVAS;
    if (strcmp(name, "markdown") == 0) return IR_COMPONENT_MARKDOWN;
    if (strcmp(name, "sprite") == 0) return IR_COMPONENT_SPRITE;
    if (strcmp(name, "tab_group") == 0) return IR_COMPONENT_TAB_GROUP;
    if (strcmp(name, "tab_bar") == 0) return IR_COMPONENT_TAB_BAR;
    if (strcmp(name, "tab") == 0) return IR_COMPONENT_TAB;
    if (strcmp(name, "tab_content") == 0) return IR_COMPONENT_TAB_CONTENT;
    if (strcmp(name, "tab_panel") == 0) return IR_COMPONENT_TAB_PANEL;
    if (strcmp(name, "modal") == 0) return IR_COMPONENT_MODAL;
    if (strcmp(name, "table") == 0) return IR_COMPONENT_TABLE;
    if (strcmp(name, "table_head") == 0) return IR_COMPONENT_TABLE_HEAD;
    if (strcmp(name, "table_body") == 0) return IR_COMPONENT_TABLE_BODY;
    if (strcmp(name, "table_foot") == 0) return IR_COMPONENT_TABLE_FOOT;
    if (strcmp(name, "table_row") == 0) return IR_COMPONENT_TABLE_ROW;
    if (strcmp(name, "table_cell") == 0) return IR_COMPONENT_TABLE_CELL;
    if (strcmp(name, "table_header_cell") == 0) return IR_COMPONENT_TABLE_HEADER_CELL;
    if (strcmp(name, "heading") == 0) return IR_COMPONENT_HEADING;
    if (strcmp(name, "paragraph") == 0) return IR_COMPONENT_PARAGRAPH;
    if (strcmp(name, "blockquote") == 0) return IR_COMPONENT_BLOCKQUOTE;
    if (strcmp(name, "code_block") == 0) return IR_COMPONENT_CODE_BLOCK;
    if (strcmp(name, "horizontal_rule") == 0) return IR_COMPONENT_HORIZONTAL_RULE;
    if (strcmp(name, "list") == 0) return IR_COMPONENT_LIST;
    if (strcmp(name, "list_item") == 0) return IR_COMPONENT_LIST_ITEM;
    if (strcmp(name, "link") == 0) return IR_COMPONENT_LINK;
    if (strcmp(name, "span") == 0) return IR_COMPONENT_SPAN;
    if (strcmp(name, "strong") == 0) return IR_COMPONENT_STRONG;
    if (strcmp(name, "em") == 0) return IR_COMPONENT_EM;
    if (strcmp(name, "code_inline") == 0) return IR_COMPONENT_CODE_INLINE;
    if (strcmp(name, "small") == 0) return IR_COMPONENT_SMALL;
    if (strcmp(name, "mark") == 0) return IR_COMPONENT_MARK;
    if (strcmp(name, "custom") == 0) return IR_COMPONENT_CUSTOM;
    if (strcmp(name, "static_block") == 0) return IR_COMPONENT_STATIC_BLOCK;
    if (strcmp(name, "for_loop") == 0) return IR_COMPONENT_FOR_LOOP;
    if (strcmp(name, "for") == 0) return IR_COMPONENT_FOR_EACH;
    if (strcmp(name, "var_decl") == 0) return IR_COMPONENT_VAR_DECL;
    if (strcmp(name, "placeholder") == 0) return IR_COMPONENT_PLACEHOLDER;
    if (strcmp(name, "flowchart") == 0) return IR_COMPONENT_FLOWCHART;
    if (strcmp(name, "flowchart_node") == 0) return IR_COMPONENT_FLOWCHART_NODE;
    if (strcmp(name, "flowchart_edge") == 0) return IR_COMPONENT_FLOWCHART_EDGE;
    if (strcmp(name, "flowchart_subgraph") == 0) return IR_COMPONENT_FLOWCHART_SUBGRAPH;
    if (strcmp(name, "flowchart_label") == 0) return IR_COMPONENT_FLOWCHART_LABEL;

    /* Fall back to PascalCase lookup (for compatibility) */
    return ir_component_type_from_string_insensitive(name);
}

/**
 * Check if a component name is valid.
 * Returns true if the name maps to a known component type.
 */
bool ir_component_type_name_valid(const char* name) {
    if (!name) return false;

    /* First try snake_case lookup */
    IRComponentType type = ir_component_type_from_snake_case(name);

    /* If we got CONTAINER, check if the name was actually "container" */
    if (type == IR_COMPONENT_CONTAINER) {
        return strcmp(name, "container") == 0 ||
               str_ieq(name, "Container");
    }

    return true;
}
