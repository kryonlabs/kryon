/**
 * @file ir_component_types.c
 * @brief Component type definitions and conversion functions
 */

#define _GNU_SOURCE
#include "ir_component_types.h"
#include <string.h>
#include <ctype.h>

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
        case IR_COMPONENT_FOR_EACH: return "ForEach";
        case IR_COMPONENT_VAR_DECL: return "VarDecl";
        case IR_COMPONENT_PLACEHOLDER: return "Placeholder";
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
