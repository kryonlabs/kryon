/**
 * @file kryon_mappings.c
 * @brief Unified Mapping System - Properties and Elements for the KRY Language
 * @details This file serves as the single source of truth for ALL hex code assignments:
 *          - Property mappings (0x0000-0x0FFF): Organized by category (Layout, Visual, etc.).
 *          - Element mappings (0x1000+): Container, Text, Button, App, etc.
 *          This system uses a single source of truth for properties and a hash table for
 *          high-performance lookups. Validation is data-driven via allowed categories.
 *
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 */

 #include "kryon_mappings.h"
 #include <string.h>
 #include <stddef.h>
 #include <stdlib.h>
 #include <stdio.h>
 #include <time.h>
 
 //==============================================================================
 // PROPERTY CATEGORIES (Hierarchical Organization)
 //==============================================================================
 
 const KryonPropertyCategory kryon_property_categories[] = {
     [KRYON_CATEGORY_BASE] = {"Base", 0x0000, 0x00FF, "Core properties available to all elements"},
     [KRYON_CATEGORY_LAYOUT] = {"Layout", 0x0100, 0x01FF, "Layout and positioning properties"},
     [KRYON_CATEGORY_VISUAL] = {"Visual", 0x0200, 0x02FF, "Visual appearance and styling properties"},
     [KRYON_CATEGORY_TYPOGRAPHY] = {"Typography", 0x0300, 0x03FF, "Text and typography properties"},
     [KRYON_CATEGORY_TRANSFORM] = {"Transform", 0x0400, 0x04FF, "Transform and animation properties"},
     [KRYON_CATEGORY_INTERACTIVE] = {"Interactive", 0x0500, 0x05FF, "User interaction and event properties"},
     [KRYON_CATEGORY_ELEMENT_SPECIFIC] = {"ElementSpecific", 0x0600, 0x06FF, "Element-specific properties"},
     [KRYON_CATEGORY_WINDOW] = {"Window", 0x0700, 0x07FF, "Window management properties"},
     [KRYON_CATEGORY_CHECKBOX] = {"Checkbox", 0x0800, 0x08FF, "Checkbox-specific properties"},
     [KRYON_CATEGORY_DIRECTIVE] = {"Directive", 0x8200, 0x82FF, "Control flow directive properties"}
 };
 
 //==============================================================================
 // SINGLE SOURCE OF TRUTH: SEPARATED PROPERTY ARRAYS BY CATEGORY
 //==============================================================================
 
 // Base properties (0x0000-0x00FF)
 const KryonPropertyGroup kryon_base_properties[] = {
     {"id", (const char*[]){NULL}, 0x0001, KRYON_TYPE_HINT_STRING, false},
     {"class", (const char*[]){"className", NULL}, 0x0002, KRYON_TYPE_HINT_STRING, false},
     {"style", (const char*[]){NULL}, 0x0003, KRYON_TYPE_HINT_STRING, false},
     {"theme", (const char*[]){NULL}, 0x0004, KRYON_TYPE_HINT_STRING, false},
     {"extends", (const char*[]){NULL}, 0x0005, KRYON_TYPE_HINT_STRING, false},
     {"title", (const char*[]){NULL}, 0x0006, KRYON_TYPE_HINT_STRING, false},
     {"version", (const char*[]){NULL}, 0x0007, KRYON_TYPE_HINT_STRING, false},
     {"description", (const char*[]){NULL}, 0x0008, KRYON_TYPE_HINT_STRING, false},
     {NULL, NULL, 0, 0, false} // Sentinel
 };
 
 // Layout properties (0x0100-0x01FF)
 const KryonPropertyGroup kryon_layout_properties[] = {
     {"width", (const char*[]){NULL}, 0x0100, KRYON_TYPE_HINT_DIMENSION, false},
     {"height", (const char*[]){NULL}, 0x0101, KRYON_TYPE_HINT_DIMENSION, false},
     {"minWidth", (const char*[]){"minW", NULL}, 0x0102, KRYON_TYPE_HINT_DIMENSION, false},
     {"maxWidth", (const char*[]){"maxW", NULL}, 0x0103, KRYON_TYPE_HINT_DIMENSION, false},
     {"minHeight", (const char*[]){"minH", NULL}, 0x0104, KRYON_TYPE_HINT_DIMENSION, false},
     {"maxHeight", (const char*[]){"maxH", NULL}, 0x0105, KRYON_TYPE_HINT_DIMENSION, false},
     {"padding", (const char*[]){"p", NULL}, 0x0106, KRYON_TYPE_HINT_SPACING, false},
     {"margin", (const char*[]){"m", NULL}, 0x0107, KRYON_TYPE_HINT_SPACING, false},
     {"aspectRatio", (const char*[]){"aspect", NULL}, 0x0109, KRYON_TYPE_HINT_FLOAT, false},
     {"flex", (const char*[]){NULL}, 0x010A, KRYON_TYPE_HINT_FLOAT, false},
     {"gap", (const char*[]){"spacing", NULL}, 0x0108, KRYON_TYPE_HINT_DIMENSION, false},
     {"columns", (const char*[]){NULL}, 0x010C, KRYON_TYPE_HINT_INTEGER, false},
     {"posX", (const char*[]){"x", "positionX", NULL}, 0x010D, KRYON_TYPE_HINT_FLOAT, false},
     {"posY", (const char*[]){"y", "positionY", NULL}, 0x010E, KRYON_TYPE_HINT_FLOAT, false},
     {"zIndex", (const char*[]){"z", "layerIndex", NULL}, 0x010F, KRYON_TYPE_HINT_INTEGER, false},
     {"paddingTop", (const char*[]){"pt", NULL}, 0x0110, KRYON_TYPE_HINT_DIMENSION, false},
     {"paddingRight", (const char*[]){"pr", NULL}, 0x0111, KRYON_TYPE_HINT_DIMENSION, false},
     {"paddingBottom", (const char*[]){"pb", NULL}, 0x0112, KRYON_TYPE_HINT_DIMENSION, false},
     {"paddingLeft", (const char*[]){"pl", NULL}, 0x0113, KRYON_TYPE_HINT_DIMENSION, false},
     {"marginTop", (const char*[]){"mt", NULL}, 0x0114, KRYON_TYPE_HINT_DIMENSION, false},
     {"marginRight", (const char*[]){"mr", NULL}, 0x0115, KRYON_TYPE_HINT_DIMENSION, false},
     {"marginBottom", (const char*[]){"mb", NULL}, 0x0116, KRYON_TYPE_HINT_DIMENSION, false},
     {"marginLeft", (const char*[]){"ml", NULL}, 0x0117, KRYON_TYPE_HINT_DIMENSION, false},
     {"row_spacing", (const char*[]){"rowSpacing", NULL}, 0x011F, KRYON_TYPE_HINT_DIMENSION, false},
     {"column_spacing", (const char*[]){"columnSpacing", NULL}, 0x0120, KRYON_TYPE_HINT_DIMENSION, false},
     {NULL, NULL, 0, 0, false} // Sentinel
 };
 
 // Visual properties (0x0200-0x02FF)
 const KryonPropertyGroup kryon_visual_properties[] = {
     {"background", (const char*[]){"backgroundColor", "bg", "bgColor", NULL}, 0x0200, KRYON_TYPE_HINT_COLOR, false},
     {"color", (const char*[]){"textColor", NULL}, 0x0201, KRYON_TYPE_HINT_COLOR, true},  // INHERITABLE like CSS
     {"border", (const char*[]){NULL}, 0x0202, KRYON_TYPE_HINT_STRING, false},
     {"borderRadius", (const char*[]){"radius", NULL}, 0x0203, KRYON_TYPE_HINT_DIMENSION, false},
     {"boxShadow", (const char*[]){"shadow", NULL}, 0x0204, KRYON_TYPE_HINT_STRING, false},
     {"opacity", (const char*[]){NULL}, 0x0205, KRYON_TYPE_HINT_FLOAT, false},
     {"borderColor", (const char*[]){"bColor", NULL}, 0x0206, KRYON_TYPE_HINT_COLOR, false},
     {"borderWidth", (const char*[]){"bWidth", NULL}, 0x0207, KRYON_TYPE_HINT_DIMENSION, false},
     {"visible", (const char*[]){NULL}, 0x0208, KRYON_TYPE_HINT_BOOLEAN, true},  // INHERITABLE like CSS
     {NULL, NULL, 0, 0, false} // Sentinel
 };
 
 // Typography properties (0x0300-0x03FF)
 const KryonPropertyGroup kryon_typography_properties[] = {
     {"fontSize", (const char*[]){"fSize", NULL}, 0x0300, KRYON_TYPE_HINT_DIMENSION, true},  // INHERITABLE like CSS
     {"fontWeight", (const char*[]){"fWeight", NULL}, 0x0301, KRYON_TYPE_HINT_STRING, true},  // INHERITABLE like CSS
     {"fontFamily", (const char*[]){"font", NULL}, 0x0302, KRYON_TYPE_HINT_STRING, true},  // INHERITABLE like CSS
     {"lineHeight", (const char*[]){"lHeight", NULL}, 0x0303, KRYON_TYPE_HINT_DIMENSION, true},  // INHERITABLE like CSS
     {"textAlign", (const char*[]){"textAlignment", NULL}, 0x0304, KRYON_TYPE_HINT_STRING, true},  // INHERITABLE like CSS
     {"fontStyle", (const char*[]){"fStyle", NULL}, 0x0305, KRYON_TYPE_HINT_STRING, true},  // INHERITABLE like CSS
     {"to", (const char*[]){NULL}, 0x0306, KRYON_TYPE_HINT_STRING, false},  // Link-specific, not inheritable
     {"external", (const char*[]){NULL}, 0x0307, KRYON_TYPE_HINT_BOOLEAN, false},  // Link-specific, not inheritable
     {"underline", (const char*[]){NULL}, 0x0308, KRYON_TYPE_HINT_BOOLEAN, false},  // Text decoration, not typically inherited
     {NULL, NULL, 0, 0, false} // Sentinel
 };
 
 // Transform properties (0x0400-0x04FF)
 const KryonPropertyGroup kryon_transform_properties[] = {
     {"transform", (const char*[]){NULL}, 0x0400, KRYON_TYPE_HINT_STRING, false},
     {"transition", (const char*[]){NULL}, 0x0401, KRYON_TYPE_HINT_STRING, false},
     {"animation", (const char*[]){NULL}, 0x0402, KRYON_TYPE_HINT_STRING, false},
     {NULL, NULL, 0, 0, false} // Sentinel
 };
 
 // Interactive properties (0x0500-0x05FF)
 const KryonPropertyGroup kryon_interactive_properties[] = {
     {"cursor", (const char*[]){NULL}, 0x0500, KRYON_TYPE_HINT_STRING, true},  // INHERITABLE like CSS
     {"userSelect", (const char*[]){NULL}, 0x0501, KRYON_TYPE_HINT_STRING, false},
     {"pointerEvents", (const char*[]){NULL}, 0x0502, KRYON_TYPE_HINT_STRING, false},
     {"disabled", (const char*[]){NULL}, 0x0503, KRYON_TYPE_HINT_BOOLEAN, false},
     {"password", (const char*[]){NULL}, 0x0504, KRYON_TYPE_HINT_BOOLEAN, false},
     {"multiple", (const char*[]){NULL}, 0x0505, KRYON_TYPE_HINT_BOOLEAN, false},
     {"searchable", (const char*[]){NULL}, 0x0507, KRYON_TYPE_HINT_BOOLEAN, false},
     {"showIcons", (const char*[]){NULL}, 0x0508, KRYON_TYPE_HINT_BOOLEAN, false},
     {"optionIcons", (const char*[]){NULL}, 0x0509, KRYON_TYPE_HINT_ARRAY, false},
     {"optionColors", (const char*[]){NULL}, 0x050A, KRYON_TYPE_HINT_ARRAY, false},
     {"required", (const char*[]){NULL}, 0x050B, KRYON_TYPE_HINT_BOOLEAN, false},
     {"minSelections", (const char*[]){NULL}, 0x050C, KRYON_TYPE_HINT_INTEGER, false},
     {"maxSelections", (const char*[]){NULL}, 0x050D, KRYON_TYPE_HINT_INTEGER, false},
     {"onClick", (const char*[]){"onTap", NULL}, 0x0510, KRYON_TYPE_HINT_REFERENCE, false},
     {"onChange", (const char*[]){NULL}, 0x0511, KRYON_TYPE_HINT_REFERENCE, false},
     {"onFocus", (const char*[]){NULL}, 0x0512, KRYON_TYPE_HINT_REFERENCE, false},
     {"onBlur", (const char*[]){NULL}, 0x0513, KRYON_TYPE_HINT_REFERENCE, false},
     {"onMouseEnter", (const char*[]){"onHover", NULL}, 0x0514, KRYON_TYPE_HINT_REFERENCE, false},
     {"onMouseLeave", (const char*[]){"onUnhover", NULL}, 0x0515, KRYON_TYPE_HINT_REFERENCE, false},
     {NULL, NULL, 0, 0, false} // Sentinel
 };
 
 // Element-specific properties (0x0600-0x06FF)
 const KryonPropertyGroup kryon_element_specific_properties[] = {
     {"src", (const char*[]){"source", NULL}, 0x0600, KRYON_TYPE_HINT_STRING, false},
     {"value", (const char*[]){NULL}, 0x0601, KRYON_TYPE_HINT_STRING, false},
     {"child", (const char*[]){NULL}, 0x0602, KRYON_TYPE_HINT_STRING, false},
     {"children", (const char*[]){NULL}, 0x0603, KRYON_TYPE_HINT_ARRAY, false},
     {"placeholder", (const char*[]){NULL}, 0x0604, KRYON_TYPE_HINT_STRING, false},
     {"mainAxis", (const char*[]){"mainAxisAlignment", NULL}, 0x0606, KRYON_TYPE_HINT_STRING, false},
     {"crossAxis", (const char*[]){"crossAxisAlignment", NULL}, 0x0607, KRYON_TYPE_HINT_STRING, false},
     {"direction", (const char*[]){"flexDir", NULL}, 0x0608, KRYON_TYPE_HINT_STRING, false},
     {"wrap", (const char*[]){"flexWrap", NULL}, 0x0609, KRYON_TYPE_HINT_STRING, false},
     {"align", (const char*[]){"alignItems", NULL}, 0x060A, KRYON_TYPE_HINT_STRING, false},
     {"justify", (const char*[]){"justifyContent", NULL}, 0x060B, KRYON_TYPE_HINT_STRING, false},
     {"contentAlignment", (const char*[]){NULL}, 0x060E, KRYON_TYPE_HINT_STRING, false},
     {"text", (const char*[]){NULL}, 0x060F, KRYON_TYPE_HINT_STRING, false},
     {"objectFit", (const char*[]){NULL}, 0x0610, KRYON_TYPE_HINT_STRING, false},
     {"options", (const char*[]){NULL}, 0x0611, KRYON_TYPE_HINT_ARRAY, false},
     {"selectedIndex", (const char*[]){NULL}, 0x0612, KRYON_TYPE_HINT_INTEGER, false},
     {"activeIndex", (const char*[]){NULL}, 0x0613, KRYON_TYPE_HINT_INTEGER, false},
     {"position", (const char*[]){NULL}, 0x0614, KRYON_TYPE_HINT_STRING, false},
     {"tabSpacing", (const char*[]){NULL}, 0x0615, KRYON_TYPE_HINT_INTEGER, false},
     {"indicatorColor", (const char*[]){NULL}, 0x0616, KRYON_TYPE_HINT_COLOR, false},
     {"indicatorThickness", (const char*[]){NULL}, 0x0617, KRYON_TYPE_HINT_INTEGER, false},
     {"activeBackgroundColor", (const char*[]){NULL}, 0x0618, KRYON_TYPE_HINT_COLOR, false},
     {"activeTextColor", (const char*[]){NULL}, 0x0619, KRYON_TYPE_HINT_COLOR, false},
     {"disabledBackgroundColor", (const char*[]){NULL}, 0x0619, KRYON_TYPE_HINT_COLOR, false},
     {"disabledTextColor", (const char*[]){NULL}, 0x061A, KRYON_TYPE_HINT_COLOR, false},
     {"overlay", (const char*[]){NULL}, 0x061B, KRYON_TYPE_HINT_COMPONENT, false},
     {NULL, NULL, 0, 0, false} // Sentinel
 };
 
 // Window properties (0x0700-0x07FF)
 const KryonPropertyGroup kryon_window_properties[] = {
     {"windowWidth", (const char*[]){"winWidth", NULL}, 0x0700, KRYON_TYPE_HINT_DIMENSION, false},
     {"windowHeight", (const char*[]){"winHeight", NULL}, 0x0701, KRYON_TYPE_HINT_DIMENSION, false},
     {"windowTitle", (const char*[]){"winTitle", NULL}, 0x0702, KRYON_TYPE_HINT_STRING, false},
     {"windowResizable", (const char*[]){"resizable", NULL}, 0x0703, KRYON_TYPE_HINT_BOOLEAN, false},
     {"keepAspectRatio", (const char*[]){NULL}, 0x0704, KRYON_TYPE_HINT_BOOLEAN, false},
     {NULL, NULL, 0, 0, false} // Sentinel
 };
 
 // Checkbox properties (0x0800-0x08FF)
 const KryonPropertyGroup kryon_checkbox_properties[] = {
     {"label", (const char*[]){"text", NULL}, 0x0800, KRYON_TYPE_HINT_STRING, false},
     {"checked", (const char*[]){NULL}, 0x0801, KRYON_TYPE_HINT_BOOLEAN, false},
     {"indeterminate", (const char*[]){NULL}, 0x0802, KRYON_TYPE_HINT_BOOLEAN, false},
     {"checkColor", (const char*[]){"checkmarkColor", NULL}, 0x0803, KRYON_TYPE_HINT_COLOR, false},
     {NULL, NULL, 0, 0, false} // Sentinel
 };
 
 // Directive properties (0x8200+)
 const KryonPropertyGroup kryon_directive_properties[] = {
     {"variable", (const char*[]){"var", "item", NULL}, 0x8201, KRYON_TYPE_HINT_STRING, false},
     {"array", (const char*[]){"list", "items", NULL}, 0x8202, KRYON_TYPE_HINT_STRING, false},
     {NULL, NULL, 0, 0, false} // Sentinel
 };
 
 //==============================================================================
 // UNIFIED PROPERTY REGISTRY - The master list of all property groups.
 // This structure holds pointers to the single-source-of-truth arrays.
 //==============================================================================
 
 typedef struct {
     const KryonPropertyGroup* properties;
     size_t count;
 } KryonRegisteredPropertyGroup;
 
 const KryonRegisteredPropertyGroup kryon_registered_properties[] = {
     {kryon_base_properties, (sizeof(kryon_base_properties) / sizeof(KryonPropertyGroup)) - 1},
     {kryon_layout_properties, (sizeof(kryon_layout_properties) / sizeof(KryonPropertyGroup)) - 1},
     {kryon_visual_properties, (sizeof(kryon_visual_properties) / sizeof(KryonPropertyGroup)) - 1},
     {kryon_typography_properties, (sizeof(kryon_typography_properties) / sizeof(KryonPropertyGroup)) - 1},
     {kryon_transform_properties, (sizeof(kryon_transform_properties) / sizeof(KryonPropertyGroup)) - 1},
     {kryon_interactive_properties, (sizeof(kryon_interactive_properties) / sizeof(KryonPropertyGroup)) - 1},
     {kryon_element_specific_properties, (sizeof(kryon_element_specific_properties) / sizeof(KryonPropertyGroup)) - 1},
     {kryon_window_properties, (sizeof(kryon_window_properties) / sizeof(KryonPropertyGroup)) - 1},
     {kryon_checkbox_properties, (sizeof(kryon_checkbox_properties) / sizeof(KryonPropertyGroup)) - 1},
     {kryon_directive_properties, (sizeof(kryon_directive_properties) / sizeof(KryonPropertyGroup)) - 1},
     {NULL, 0} // Sentinel
 };
 
 //==============================================================================
 // ELEMENT MAPPINGS (0x1000+) & SYNTAX MAPPINGS (0x8000+)
 //==============================================================================
 

const KryonElementGroup kryon_element_groups[] = {
    {.canonical = "Element", .aliases = (const char*[]){NULL}, .hex_code = 0x0000, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_COUNT}},
    {.canonical = "Container", .aliases = (const char*[]){"Box", NULL}, .hex_code = 0x0001, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_ELEMENT_SPECIFIC, KRYON_CATEGORY_COUNT}},
    {.canonical = "Row", .aliases = (const char*[]){NULL}, .hex_code = 0x0002, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_ELEMENT_SPECIFIC, KRYON_CATEGORY_COUNT}},
    {.canonical = "Column", .aliases = (const char*[]){"Col", NULL}, .hex_code = 0x0003, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_ELEMENT_SPECIFIC, KRYON_CATEGORY_COUNT}},
    {.canonical = "Center", .aliases = (const char*[]){NULL}, .hex_code = 0x0004, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_ELEMENT_SPECIFIC, KRYON_CATEGORY_COUNT}},
    {.canonical = "Flex", .aliases = (const char*[]){NULL}, .hex_code = 0x0005, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_COUNT}},
    {.canonical = "Spacer", .aliases = (const char*[]){NULL}, .hex_code = 0x0006, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_COUNT}},
    {.canonical = "Text", .aliases = (const char*[]){NULL}, .hex_code = 0x0400, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_TYPOGRAPHY, KRYON_CATEGORY_ELEMENT_SPECIFIC, KRYON_CATEGORY_COUNT}},
    {.canonical = "Button", .aliases = (const char*[]){"Btn", NULL}, .hex_code = 0x0401, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_TYPOGRAPHY, KRYON_CATEGORY_INTERACTIVE, KRYON_CATEGORY_COUNT}},
    {.canonical = "Image", .aliases = (const char*[]){"Img", NULL}, .hex_code = 0x0402, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_ELEMENT_SPECIFIC, KRYON_CATEGORY_COUNT}},
    {.canonical = "Input", .aliases = (const char*[]){"TextInput", NULL}, .hex_code = 0x0403, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_TYPOGRAPHY, KRYON_CATEGORY_INTERACTIVE, KRYON_CATEGORY_ELEMENT_SPECIFIC, KRYON_CATEGORY_COUNT}},
    {.canonical = "Checkbox", .aliases = (const char*[]){NULL}, .hex_code = 0x0404, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_INTERACTIVE, KRYON_CATEGORY_CHECKBOX, KRYON_CATEGORY_COUNT}},
    {.canonical = "Slider", .aliases = (const char*[]){"Range", NULL}, .hex_code = 0x0405, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_INTERACTIVE, KRYON_CATEGORY_ELEMENT_SPECIFIC, KRYON_CATEGORY_COUNT}},
    {.canonical = "Dropdown", .aliases = (const char*[]){"Select", NULL}, .hex_code = 0x0406, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_INTERACTIVE, KRYON_CATEGORY_ELEMENT_SPECIFIC, KRYON_CATEGORY_COUNT}},
    {.canonical = "App", .aliases = (const char*[]){NULL}, .hex_code = 0x1000, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_WINDOW, KRYON_CATEGORY_COUNT}},
    {.canonical = "Modal", .aliases = (const char*[]){NULL}, .hex_code = 0x1001, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_INTERACTIVE, KRYON_CATEGORY_COUNT}},
    {.canonical = "Form", .aliases = (const char*[]){NULL}, .hex_code = 0x1002, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_INTERACTIVE, KRYON_CATEGORY_COUNT}},
    {.canonical = "Grid", .aliases = (const char*[]){NULL}, .hex_code = 0x1003, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_COUNT}},
    {.canonical = "List", .aliases = (const char*[]){NULL}, .hex_code = 0x1004, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_INTERACTIVE, KRYON_CATEGORY_COUNT}},
    {.canonical = "Link", .aliases = (const char*[]){NULL}, .hex_code = 0x100B, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_TYPOGRAPHY, KRYON_CATEGORY_INTERACTIVE, KRYON_CATEGORY_COUNT}},
    {.canonical = "TabBar", .aliases = (const char*[]){NULL}, .hex_code = 0x100C, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_INTERACTIVE, KRYON_CATEGORY_ELEMENT_SPECIFIC, KRYON_CATEGORY_COUNT}},
    {.canonical = "Tab", .aliases = (const char*[]){NULL}, .hex_code = 0x100D, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_TYPOGRAPHY, KRYON_CATEGORY_INTERACTIVE, KRYON_CATEGORY_ELEMENT_SPECIFIC, KRYON_CATEGORY_COUNT}},
    {.canonical = "TabContent", .aliases = (const char*[]){NULL}, .hex_code = 0x100E, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_ELEMENT_SPECIFIC, KRYON_CATEGORY_COUNT}},
    {.canonical = "TabPanel", .aliases = (const char*[]){NULL}, .hex_code = 0x100F, .type_hint = KRYON_TYPE_HINT_ANY, .allowed_categories = (const KryonPropertyCategoryIndex[]){KRYON_CATEGORY_BASE, KRYON_CATEGORY_LAYOUT, KRYON_CATEGORY_VISUAL, KRYON_CATEGORY_ELEMENT_SPECIFIC, KRYON_CATEGORY_COUNT}},
    {.canonical = NULL} // Sentinel
};

 
 const KryonSyntaxGroup kryon_syntax_groups[] = {
     {"onload", (const char*[]){"ready", "startup", NULL}, 0x8001, KRYON_TYPE_HINT_REFERENCE},
     {"component", (const char*[]){NULL}, 0x8100, KRYON_TYPE_HINT_ANY},
     {"for", (const char*[]){"foreach", "loop", NULL}, 0x8200, KRYON_TYPE_HINT_ANY},
     {"if", (const char*[]){"when", NULL}, 0x8300, KRYON_TYPE_HINT_BOOLEAN},
     {NULL, NULL, 0, 0} // Sentinel
 };
 
 //==============================================================================
 // HIGH-PERFORMANCE HASH TABLE SYSTEM
 //==============================================================================
 
 static KryonPropertyHashTable property_hash_table = {0};
 static bool system_initialized = false;
 
 // Forward declarations
 static void initialize_mapping_system(void);
 
 // Simple, fast hash function for property names
 static uint32_t kryon_hash_string(const char *str) {
     uint32_t hash = 5381;
     int c;
     while ((c = *str++)) {
         hash = ((hash << 5) + hash) + c; // hash * 33 + c
     }
     return hash;
 }
 
 // Insert a property into the hash table, handling collisions with linear probing
 static void insert_property(const KryonPropertyGroup* prop, bool is_alias, const char* name) {
     if (!name || property_hash_table.count >= property_hash_table.size) return;
 
     uint32_t hash = kryon_hash_string(name);
     uint32_t index = hash & (property_hash_table.size - 1);
 
     for (uint32_t i = 0; i < property_hash_table.size; i++) {
         uint32_t current_index = (index + i) & (property_hash_table.size - 1);
         KryonPropertyHashEntry *entry = &property_hash_table.entries[current_index];
 
         if (entry->name == NULL) { // Empty slot found
             entry->name = name;
             entry->hex_code = prop->hex_code;
             entry->is_alias = is_alias;
             // Store a pointer to the original full property definition
             entry->group_ptr = prop; 
             property_hash_table.count++;
             return;
         }
     }
 }
 
 
 // Initialize the entire mapping system, building the hash table from the registry
 static void initialize_mapping_system(void) {
     if (system_initialized) return;
 
     // 1. Count total entries (canonical + aliases)
     uint32_t total_entries = 0;
     for (int i = 0; kryon_registered_properties[i].properties != NULL; i++) {
         for (size_t j = 0; j < kryon_registered_properties[i].count; j++) {
             const KryonPropertyGroup* prop = &kryon_registered_properties[i].properties[j];
             total_entries++; // For canonical name
             if (prop->aliases) {
                 for (int k = 0; prop->aliases[k]; k++) {
                     total_entries++;
                 }
             }
         }
     }
 
     // 2. Create hash table (size = next power of 2 for efficiency)
     uint32_t size = 1;
     while (size < total_entries * 2) size <<= 1;
     property_hash_table.entries = calloc(size, sizeof(KryonPropertyHashEntry));
     if (!property_hash_table.entries) return; // Allocation failure
 
     property_hash_table.size = size;
     property_hash_table.count = 0;
 
     // 3. Populate hash table from the unified registry
     for (int i = 0; kryon_registered_properties[i].properties != NULL; i++) {
         for (size_t j = 0; j < kryon_registered_properties[i].count; j++) {
             const KryonPropertyGroup* prop = &kryon_registered_properties[i].properties[j];
             
             // Add canonical name
             insert_property(prop, false, prop->canonical);
 
             // Add aliases
             if (prop->aliases) {
                 for (int k = 0; prop->aliases[k]; k++) {
                     insert_property(prop, true, prop->aliases[k]);
                 }
             }
         }
     }
     
     system_initialized = true;
 }
 
 // Find a property entry in the hash table
 static const KryonPropertyHashEntry* find_property_entry(const char* name) {
     if (!system_initialized) initialize_mapping_system();
     if (!name) return NULL;
 
     uint32_t hash = kryon_hash_string(name);
     uint32_t index = hash & (property_hash_table.size - 1);
 
     for (uint32_t i = 0; i < property_hash_table.size; i++) {
         uint32_t current_index = (index + i) & (property_hash_table.size - 1);
         KryonPropertyHashEntry *entry = &property_hash_table.entries[current_index];
 
         if (entry->name == NULL) return NULL; // Not found
         if (strcmp(entry->name, name) == 0) return entry;
     }
     return NULL; // Not found
 }
 
 
 //==============================================================================
 // PUBLIC API FUNCTIONS
 //==============================================================================
 
 // Get property hex code from name using the hash table for O(1) average lookup
 uint16_t kryon_get_property_hex(const char *name) {
     const KryonPropertyHashEntry* entry = find_property_entry(name);
     return entry ? entry->hex_code : 0;
 }
 
 // Get property canonical name from hex code (requires iteration)
 const char *kryon_get_property_name(uint16_t hex_code) {
     if (hex_code == 0) return NULL;
 
     for (int i = 0; kryon_registered_properties[i].properties != NULL; i++) {
         for (size_t j = 0; j < kryon_registered_properties[i].count; j++) {
             if (kryon_registered_properties[i].properties[j].hex_code == hex_code) {
                 return kryon_registered_properties[i].properties[j].canonical;
             }
         }
     }
     return NULL;
 }
 
 KryonValueTypeHint kryon_get_property_type_hint(uint16_t hex_code) {
     if (hex_code == 0) return KRYON_TYPE_HINT_ANY;
 
     for (int i = 0; kryon_registered_properties[i].properties != NULL; i++) {
         for (size_t j = 0; j < kryon_registered_properties[i].count; j++) {
             if (kryon_registered_properties[i].properties[j].hex_code == hex_code) {
                 return kryon_registered_properties[i].properties[j].type_hint;
             }
         }
     }
     return KRYON_TYPE_HINT_ANY;
 }
 
 bool kryon_is_property_alias(const char *name) {
     const KryonPropertyHashEntry* entry = find_property_entry(name);
     return entry ? entry->is_alias : false;
 }
 
 uint16_t kryon_get_element_hex(const char *name) {
     if (!name) return 0;
     for (int i = 0; kryon_element_groups[i].canonical; i++) {
         if (strcmp(kryon_element_groups[i].canonical, name) == 0) return kryon_element_groups[i].hex_code;
         if (kryon_element_groups[i].aliases) {
             for (int j = 0; kryon_element_groups[i].aliases[j]; j++) {
                 if (strcmp(kryon_element_groups[i].aliases[j], name) == 0) return kryon_element_groups[i].hex_code;
             }
         }
     }
     return 0;
 }
 
 const char *kryon_get_element_name(uint16_t hex_code) {
     for (int i = 0; kryon_element_groups[i].canonical; i++) {
         if (kryon_element_groups[i].hex_code == hex_code) {
             return kryon_element_groups[i].canonical;
         }
     }
     return NULL;
 }
 
 uint16_t kryon_get_syntax_hex(const char *name) {
     if (!name) return 0;
     for (int i = 0; kryon_syntax_groups[i].canonical; i++) {
         if (strcmp(kryon_syntax_groups[i].canonical, name) == 0) return kryon_syntax_groups[i].hex_code;
         if (kryon_syntax_groups[i].aliases) {
             for (int j = 0; kryon_syntax_groups[i].aliases[j]; j++) {
                 if (strcmp(kryon_syntax_groups[i].aliases[j], name) == 0) return kryon_syntax_groups[i].hex_code;
             }
         }
     }
     return 0;
 }
 
 const char *kryon_get_syntax_name(uint16_t hex_code) {
     for (int i = 0; kryon_syntax_groups[i].canonical; i++) {
         if (kryon_syntax_groups[i].hex_code == hex_code) {
             return kryon_syntax_groups[i].canonical;
         }
     }
     return NULL;
 }
 
 
 //==============================================================================
 // DATA-DRIVEN VALIDATION SYSTEM
 //==============================================================================
 
 // Helper to get the category of a property based on its hex code range.
 static KryonPropertyCategoryIndex kryon_get_property_category(uint16_t property_hex) {
     for (int i = 0; i < KRYON_CATEGORY_COUNT; i++) {
         if (property_hex >= kryon_property_categories[i].range_start && property_hex <= kryon_property_categories[i].range_end) {
             return (KryonPropertyCategoryIndex)i;
         }
     }
     return KRYON_CATEGORY_COUNT; // Invalid/unknown category
 }
 
 // Helper to find an element's definition by its hex code.
 static const KryonElementGroup* kryon_find_element(uint16_t element_hex) {
     for (int i = 0; kryon_element_groups[i].canonical; i++) {
         if (kryon_element_groups[i].hex_code == element_hex) {
             return &kryon_element_groups[i];
         }
     }
     return NULL;
 }
 
 /**
  * @brief Validates if a property is allowed for a given element.
  * @details This function uses the data-driven `allowed_categories` list
  *          defined for each element, making it flexible and easy to maintain.
  */
 bool kryon_is_valid_property_for_element(uint16_t element_hex, uint16_t property_hex) {
     const KryonElementGroup* element = kryon_find_element(element_hex);
     if (!element) {
         // If element is not a built-in, it might be a custom component.
         // For flexibility, we allow all properties on unknown elements.
         return true;
     }
     
     KryonPropertyCategoryIndex property_category = kryon_get_property_category(property_hex);
     if (property_category == KRYON_CATEGORY_COUNT) {
         return false; // Unknown property category is never valid.
     }
     
     // Check if the element's definition allows this property's category.
     for (int i = 0; element->allowed_categories[i] != KRYON_CATEGORY_COUNT; i++) {
         if (element->allowed_categories[i] == property_category) {
             return true;
         }
     }
     
         return false; // Category not found in the allowed list.
}

/**
 * @brief Get the list of allowed property categories for an element.
 */
const KryonPropertyCategoryIndex* kryon_get_element_allowed_categories(uint16_t element_hex) {
    const KryonElementGroup* element = kryon_find_element(element_hex);
    if (!element) {
        return NULL; // Element not found
    }
    
    return element->allowed_categories;
}

/**
 * @brief Check if hex code is in a specific element range
 */
bool kryon_element_in_range(uint16_t hex_code, uint16_t range_start, uint16_t range_end) {
    return hex_code >= range_start && hex_code <= range_end;
}

/**
 * @brief Check if a property is inheritable (like CSS inheritance)
 */
bool kryon_is_property_inheritable(uint16_t property_hex) {
    if (property_hex == 0) return false;

    // Search through all property groups to find this property
    for (int i = 0; kryon_registered_properties[i].properties != NULL; i++) {
        for (size_t j = 0; j < kryon_registered_properties[i].count; j++) {
            const KryonPropertyGroup* prop = &kryon_registered_properties[i].properties[j];
            if (prop->hex_code == property_hex) {
                return prop->inheritable;
            }
        }
    }

    return false; // Property not found, default to not inheritable
}

//==============================================================================
// TESTING AND DEBUGGING FUNCTIONS
//==============================================================================
 
 void kryon_mappings_test(void) {
     printf("🔍 KRYON MAPPINGS TEST (Refactored System)\n");
     printf("===========================================\n");
 
     // Test hash lookups with various properties
     const char* test_properties[] = {"width", "height", "backgroundColor", "bg", "text", "value", "onClick", "onTap"};
 
     printf("📊 Property Hash Lookup Test:\n");
     for (size_t i = 0; i < sizeof(test_properties) / sizeof(test_properties[0]); i++) {
         const char* prop_name = test_properties[i];
         uint16_t hex = kryon_get_property_hex(prop_name);
         bool is_alias = kryon_is_property_alias(prop_name);
         const char* name_type = is_alias ? "alias" : "canonical";
 
         if (hex != 0) {
             printf("  ✅ %-15s → 0x%04X (%s)\n", prop_name, hex, name_type);
         } else {
             printf("  ❌ %-15s → NOT FOUND\n", prop_name);
         }
     }
 
     // Debug: Show hash table statistics
     printf("\n🔧 Hash Table Statistics:\n");
     printf("  Hash table size: %u\n", property_hash_table.size);
     printf("  Entries used: %u\n", property_hash_table.count);
     printf("  Load factor: %.2f%%\n", (float)property_hash_table.count / property_hash_table.size * 100);
 
     // Test data-driven validation
     printf("\n🛡️ Data-Driven Validation Test:\n");
     
     // Test Text (0x0400)
     bool text_allows_fontSize = kryon_is_valid_property_for_element(0x0400, 0x0300); // Typography
     bool text_rejects_onClick = !kryon_is_valid_property_for_element(0x0400, 0x0510); // Interactive
     printf("  Text allows 'fontSize': %s\n", text_allows_fontSize ? "✅ PASSED" : "❌ FAILED");
     printf("  Text rejects 'onClick': %s\n", text_rejects_onClick ? "✅ PASSED" : "❌ FAILED");
     
     // Test Button (0x0401)
     bool btn_allows_onClick = kryon_is_valid_property_for_element(0x0401, 0x0510); // Interactive
     bool btn_rejects_checked = !kryon_is_valid_property_for_element(0x0401, 0x0801); // Checkbox
     printf("  Button allows 'onClick': %s\n", btn_allows_onClick ? "✅ PASSED" : "❌ FAILED");
     printf("  Button rejects 'checked': %s\n", btn_rejects_checked ? "✅ PASSED" : "❌ FAILED");
     
     // Test Checkbox (0x0404)
     bool check_allows_checked = kryon_is_valid_property_for_element(0x0404, 0x0801); // Checkbox
     bool check_allows_onClick = kryon_is_valid_property_for_element(0x0404, 0x0510); // Interactive
     printf("  Checkbox allows 'checked': %s\n", check_allows_checked ? "✅ PASSED" : "❌ FAILED");
     printf("  Checkbox allows 'onClick': %s\n", check_allows_onClick ? "✅ PASSED" : "❌ FAILED");
 
     printf("\n✅ Test completed successfully.\n");
 }