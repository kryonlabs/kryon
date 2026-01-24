/**
 * C Code Generator - Type Mapping Implementation
 */

#include "ir_c_types.h"
#include <string.h>

const char* kir_type_to_c(const char* kir_type) {
    if (!kir_type) return "void";

    if (strcmp(kir_type, "string") == 0) return "const char*";
    if (strcmp(kir_type, "int") == 0) return "int";
    if (strcmp(kir_type, "number") == 0) return "double";
    if (strcmp(kir_type, "float") == 0) return "float";
    if (strcmp(kir_type, "double") == 0) return "double";
    if (strcmp(kir_type, "bool") == 0) return "bool";
    if (strcmp(kir_type, "void") == 0) return "void";

    // Array types like "string[]" -> "const char**"
    size_t len = strlen(kir_type);
    if (len > 2 && kir_type[len-2] == '[' && kir_type[len-1] == ']') {
        // Create base type string
        char base[64];
        strncpy(base, kir_type, len - 2);
        base[len - 2] = '\0';

        if (strcmp(base, "string") == 0) return "const char**";
        if (strcmp(base, "int") == 0) return "int*";
        if (strcmp(base, "number") == 0) return "double*";
        if (strcmp(base, "float") == 0) return "float*";
        if (strcmp(base, "double") == 0) return "double*";
        if (strcmp(base, "bool") == 0) return "bool*";
    }

    return "void*";  // Unknown type
}

const char* get_component_macro(const char* type) {
    if (strcmp(type, "Container") == 0) return "CONTAINER";
    if (strcmp(type, "Column") == 0) return "COLUMN";
    if (strcmp(type, "Row") == 0) return "ROW";
    if (strcmp(type, "Center") == 0) return "CENTER";
    if (strcmp(type, "Text") == 0) return "TEXT";
    if (strcmp(type, "Button") == 0) return "BUTTON";
    if (strcmp(type, "Input") == 0) return "INPUT";
    if (strcmp(type, "Checkbox") == 0) return "CHECKBOX";
    if (strcmp(type, "Dropdown") == 0) return "DROPDOWN";
    if (strcmp(type, "Image") == 0) return "IMAGE";
    if (strcmp(type, "TabGroup") == 0) return "TAB_GROUP";
    if (strcmp(type, "TabBar") == 0) return "TAB_BAR";
    if (strcmp(type, "Tab") == 0) return "TAB";
    if (strcmp(type, "TabContent") == 0) return "TAB_CONTENT";
    if (strcmp(type, "TabPanel") == 0) return "TAB_PANEL";
    if (strcmp(type, "Table") == 0) return "TABLE";
    if (strcmp(type, "TableHead") == 0) return "TABLE_HEAD";
    if (strcmp(type, "TableBody") == 0) return "TABLE_BODY";
    if (strcmp(type, "TableRow") == 0) return "TABLE_ROW";
    if (strcmp(type, "TableCell") == 0) return "TABLE_CELL";
    if (strcmp(type, "TableHeaderCell") == 0) return "TABLE_HEADER_CELL";
    if (strcmp(type, "For") == 0) return "FOR_EACH";
    if (strcmp(type, "Custom") == 0) return "COMPONENT";
    return "CONTAINER";  // Fallback
}

bool is_numeric(const char* str) {
    if (!str) return false;
    if (*str == '-' || *str == '+') str++;
    bool has_digit = false;
    while (*str) {
        if (*str >= '0' && *str <= '9') {
            has_digit = true;
        } else if (*str == '.') {
            // Allow decimal point
        } else {
            return false;  // Non-numeric character
        }
        str++;
    }
    return has_digit;
}
