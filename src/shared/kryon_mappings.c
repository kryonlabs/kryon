/**
 * @file kryon_mappings.c
 * @brief Unified Mapping System - Properties and Elements for the KRY Language
 * @details This file serves as the single source of truth for ALL hex code assignments:
 *          - Property mappings (0x0000-0x0FFF): posX, width, color, padding, etc.
 *          - Element mappings (0x1000+): Container, Text, Button, App, etc.
 *          Includes canonical names and aliases to improve developer experience.
 *
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 */

 #include "kryon_mappings.h"
 #include <string.h>
 #include <stddef.h>
 
 //==============================================================================
 // PROPERTY MAPPINGS (0x0000 - 0x0FFF)
 // Properties that elements can have: posX, width, color, padding, etc.
 //==============================================================================
 
 const KryonPropertyGroup kryon_property_groups[] = {
     // -- Meta & System Properties (0x00xx) --
     {
         .canonical = "id",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0001,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "class",
         .aliases = (const char*[]){"className", NULL},
         .hex_code = 0x0002,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "style",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0003,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "theme",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0004,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "extends",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0005,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "title",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0006,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "version",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0007,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
 
     // -- Layout Properties (0x01xx) --
     {
         .canonical = "width",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0100,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "height",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0101,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "minWidth",
         .aliases = (const char*[]){"minW", NULL},
         .hex_code = 0x0102,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "maxWidth",
         .aliases = (const char*[]){"maxW", NULL},
         .hex_code = 0x0103,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "minHeight",
         .aliases = (const char*[]){"minH", NULL},
         .hex_code = 0x0104,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "maxHeight",
         .aliases = (const char*[]){"maxH", NULL},
         .hex_code = 0x0105,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "padding",
         .aliases = (const char*[]){"p", NULL},
         .hex_code = 0x0106,
         .type_hint = KRYON_TYPE_HINT_SPACING
     },
     {
         .canonical = "margin",
         .aliases = (const char*[]){"m", NULL},
         .hex_code = 0x0107,
         .type_hint = KRYON_TYPE_HINT_SPACING
     },
     {
         .canonical = "aspectRatio",
         .aliases = (const char*[]){"aspect", NULL},
         .hex_code = 0x0109,
         .type_hint = KRYON_TYPE_HINT_FLOAT
     },
     {
         .canonical = "flex",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x010A,
         .type_hint = KRYON_TYPE_HINT_FLOAT
     },
     {
         .canonical = "gap",
         .aliases = (const char*[]){"spacing", NULL},
         .hex_code = 0x010B,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "columns",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x010C,
         .type_hint = KRYON_TYPE_HINT_INTEGER
     },
     {
         .canonical = "row_spacing",
         .aliases = (const char*[]){"rowSpacing", NULL},
         .hex_code = 0x011F,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "column_spacing",
         .aliases = (const char*[]){"columnSpacing", NULL},
         .hex_code = 0x0120,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "posX",
         .aliases = (const char*[]){"x", "positionX", NULL},
         .hex_code = 0x010D,
         .type_hint = KRYON_TYPE_HINT_FLOAT
     },
     {
         .canonical = "posY",
         .aliases = (const char*[]){"y", "positionY", NULL},
         .hex_code = 0x010E,
         .type_hint = KRYON_TYPE_HINT_FLOAT
     },
     {
         .canonical = "zIndex",
         .aliases = (const char*[]){"z", "layerIndex", NULL},
         .hex_code = 0x010F,
         .type_hint = KRYON_TYPE_HINT_INTEGER
     },
 
     // -- Box Model Shorthands (0x011x) --
     {
         .canonical = "paddingTop",
         .aliases = (const char*[]){"pt", NULL},
         .hex_code = 0x0110,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "paddingRight",
         .aliases = (const char*[]){"pr", NULL},
         .hex_code = 0x0111,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "paddingBottom",
         .aliases = (const char*[]){"pb", NULL},
         .hex_code = 0x0112,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "paddingLeft",
         .aliases = (const char*[]){"pl", NULL},
         .hex_code = 0x0113,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "marginTop",
         .aliases = (const char*[]){"mt", NULL},
         .hex_code = 0x0114,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "marginRight",
         .aliases = (const char*[]){"mr", NULL},
         .hex_code = 0x0115,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "marginBottom",
         .aliases = (const char*[]){"mb", NULL},
         .hex_code = 0x0116,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "marginLeft",
         .aliases = (const char*[]){"ml", NULL},
         .hex_code = 0x0117,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
 
     // -- Visual Properties (0x02xx) --
     {
         .canonical = "background",
         .aliases = (const char*[]){"backgroundColor", "bg", "bgColor", NULL},
         .hex_code = 0x0200,
         .type_hint = KRYON_TYPE_HINT_COLOR
     },
     {
         .canonical = "color",
         .aliases = (const char*[]){"textColor", NULL},
         .hex_code = 0x0201,
         .type_hint = KRYON_TYPE_HINT_COLOR
     },
     {
         .canonical = "border",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0202,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "borderRadius",
         .aliases = (const char*[]){"radius", NULL},
         .hex_code = 0x0203,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "boxShadow",
         .aliases = (const char*[]){"shadow", NULL},
         .hex_code = 0x0204,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "opacity",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0205,
         .type_hint = KRYON_TYPE_HINT_FLOAT
     },
     {
         .canonical = "borderColor",
         .aliases = (const char*[]){"bColor", NULL},
         .hex_code = 0x0206,
         .type_hint = KRYON_TYPE_HINT_COLOR
     },
     {
         .canonical = "borderWidth",
         .aliases = (const char*[]){"bWidth", NULL},
         .hex_code = 0x0207,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "visible",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0208,
         .type_hint = KRYON_TYPE_HINT_BOOLEAN
     },
 
     // -- Typography Properties (0x03xx) --
     {
         .canonical = "fontSize",
         .aliases = (const char*[]){"fSize", NULL},
         .hex_code = 0x0300,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "fontWeight",
         .aliases = (const char*[]){"fWeight", NULL},
         .hex_code = 0x0301,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "fontFamily",
         .aliases = (const char*[]){"font", NULL},
         .hex_code = 0x0302,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "lineHeight",
         .aliases = (const char*[]){"lHeight", NULL},
         .hex_code = 0x0303,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "textAlign",
         .aliases = (const char*[]){"textAlignment", NULL},
         .hex_code = 0x0304,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "fontStyle",
         .aliases = (const char*[]){"fStyle", NULL},
         .hex_code = 0x0305,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "to",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0306,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "external",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0307,
         .type_hint = KRYON_TYPE_HINT_BOOLEAN
     },
     {
         .canonical = "underline",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0308,
         .type_hint = KRYON_TYPE_HINT_BOOLEAN
     },
 
     // -- Transform & Animation Properties (0x04xx) --
     {
         .canonical = "transform",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0400,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "transition",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0401,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "animation",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0402,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
 
     // -- Interaction & Event Properties (0x05xx) --
     {
         .canonical = "cursor",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0500,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "userSelect",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0501,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "pointerEvents",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0502,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "disabled",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0503,
         .type_hint = KRYON_TYPE_HINT_BOOLEAN
     },
     {
         .canonical = "password",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0504,
         .type_hint = KRYON_TYPE_HINT_BOOLEAN
     },
     {
         .canonical = "multiple",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0505,
         .type_hint = KRYON_TYPE_HINT_BOOLEAN
     },
     {
         .canonical = "maxHeight",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0506,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "searchable",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0507,
         .type_hint = KRYON_TYPE_HINT_BOOLEAN
     },
     {
         .canonical = "showIcons",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0508,
         .type_hint = KRYON_TYPE_HINT_BOOLEAN
     },
     {
         .canonical = "optionIcons",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0509,
         .type_hint = KRYON_TYPE_HINT_ARRAY
     },
     {
         .canonical = "optionColors",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x050A,
         .type_hint = KRYON_TYPE_HINT_ARRAY
     },
     {
         .canonical = "required",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x050B,
         .type_hint = KRYON_TYPE_HINT_BOOLEAN
     },
     {
         .canonical = "minSelections",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x050C,
         .type_hint = KRYON_TYPE_HINT_INTEGER
     },
     {
         .canonical = "maxSelections",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x050D,
         .type_hint = KRYON_TYPE_HINT_INTEGER
     },
     {
         .canonical = "onClick",
         .aliases = (const char*[]){"onTap", NULL},
         .hex_code = 0x0510,
         .type_hint = KRYON_TYPE_HINT_REFERENCE
     },
     {
         .canonical = "onChange",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0511,
         .type_hint = KRYON_TYPE_HINT_REFERENCE
     },
     {
         .canonical = "onFocus",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0512,
         .type_hint = KRYON_TYPE_HINT_REFERENCE
     },
     {
         .canonical = "onBlur",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0513,
         .type_hint = KRYON_TYPE_HINT_REFERENCE
     },
     {
         .canonical = "onMouseEnter",
         .aliases = (const char*[]){"onHover", NULL},
         .hex_code = 0x0514,
         .type_hint = KRYON_TYPE_HINT_REFERENCE
     },
     {
         .canonical = "onMouseLeave",
         .aliases = (const char*[]){"onUnhover", NULL},
         .hex_code = 0x0515,
         .type_hint = KRYON_TYPE_HINT_REFERENCE
     },
 
     // -- Element-Specific Properties (0x06xx) --
     {
         .canonical = "src",
         .aliases = (const char*[]){"source", NULL},
         .hex_code = 0x0600,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "value",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0601,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "child",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0602,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "children",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0603,
         .type_hint = KRYON_TYPE_HINT_ARRAY
     },
     {
         .canonical = "placeholder",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0604,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "value",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0605,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "objectFit",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0610,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "mainAxis",
         .aliases = (const char*[]){"mainAxisAlignment", NULL},
         .hex_code = 0x0606,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "crossAxis",
         .aliases = (const char*[]){"crossAxisAlignment", NULL},
         .hex_code = 0x0607,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "direction",
         .aliases = (const char*[]){"flexDir", NULL},
         .hex_code = 0x0608,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "wrap",
         .aliases = (const char*[]){"flexWrap", NULL},
         .hex_code = 0x0609,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "align",
         .aliases = (const char*[]){"alignItems", NULL},
         .hex_code = 0x060A,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "justify",
         .aliases = (const char*[]){"justifyContent", NULL},
         .hex_code = 0x060B,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "contentAlignment",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x060E,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "text",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x060F,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "options",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0611,
         .type_hint = KRYON_TYPE_HINT_ARRAY
     },
     {
         .canonical = "selectedIndex",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0612,
         .type_hint = KRYON_TYPE_HINT_INTEGER
     },
     {
         .canonical = "position",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0613,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "tabSpacing",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0614,
         .type_hint = KRYON_TYPE_HINT_INTEGER
     },
     {
         .canonical = "indicatorColor",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0615,
         .type_hint = KRYON_TYPE_HINT_COLOR
     },
     {
         .canonical = "indicatorThickness",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0616,
         .type_hint = KRYON_TYPE_HINT_INTEGER
     },
     {
         .canonical = "activeBackgroundColor",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0617,
         .type_hint = KRYON_TYPE_HINT_COLOR
     },
     {
         .canonical = "activeTextColor",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0618,
         .type_hint = KRYON_TYPE_HINT_COLOR
     },
     {
         .canonical = "disabledBackgroundColor",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0619,
         .type_hint = KRYON_TYPE_HINT_COLOR
     },
     {
         .canonical = "disabledTextColor",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x061A,
         .type_hint = KRYON_TYPE_HINT_COLOR
     },
     {
         .canonical = "overlay",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x061B,
         .type_hint = KRYON_TYPE_HINT_COMPONENT
     },
 
     // -- Window Properties (0x07xx) --
     {
         .canonical = "windowWidth",
         .aliases = (const char*[]){"winWidth", NULL},
         .hex_code = 0x0700,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "windowHeight",
         .aliases = (const char*[]){"winHeight", NULL},
         .hex_code = 0x0701,
         .type_hint = KRYON_TYPE_HINT_DIMENSION
     },
     {
         .canonical = "windowTitle",
         .aliases = (const char*[]){"winTitle", NULL},
         .hex_code = 0x0702,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "windowResizable",
         .aliases = (const char*[]){"resizable", NULL},
         .hex_code = 0x0703,
         .type_hint = KRYON_TYPE_HINT_BOOLEAN
     },
     {
         .canonical = "keepAspectRatio",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0704,
         .type_hint = KRYON_TYPE_HINT_BOOLEAN
     },

     // -- Directive Properties (0x8200+) --
     // Properties specific to @for, @if, @while and other directives
     {
         .canonical = "variable",
         .aliases = (const char*[]){"var", "item", NULL},
         .hex_code = 0x8201,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "array",
         .aliases = (const char*[]){"list", "items", NULL},
         .hex_code = 0x8202,
         .type_hint = KRYON_TYPE_HINT_STRING
     }
 };
 
 //==============================================================================
 // WIDGET MAPPINGS (0x1000+)  
 // Element types: Container, Text, Button, App, etc.
 //==============================================================================
 
 const KryonElementGroup kryon_element_groups[] = {
     // -- Base Element (0x0000) --
     {
         .canonical = "Element",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0000,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
 
     // -- Layout Elements (0x0001 - 0x00FF) --
     {
         .canonical = "Column",
         .aliases = (const char*[]){"Col", NULL},
         .hex_code = 0x0001,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "Row",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0002,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "Center",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0003,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "Container",
         .aliases = (const char*[]){"Box", NULL},
         .hex_code = 0x0004,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "Flex",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0005,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "Spacer",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0006,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
 
     // -- Content Elements (0x0400 - 0x04FF) --
     {
         .canonical = "Text",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0400,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "Button",
         .aliases = (const char*[]){"Btn", NULL},
         .hex_code = 0x0401,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "Image",
         .aliases = (const char*[]){"Img", NULL},
         .hex_code = 0x0402,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "Input",
         .aliases = (const char*[]){"TextInput", NULL},
         .hex_code = 0x0403,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
 
     // -- Application Elements (0x1000+) --
     {
         .canonical = "App",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1000,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "Modal",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1001,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "Form",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1002,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "Grid",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1003,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "List",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1004,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "Dropdown",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1005,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "Menu",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1006,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "Tabs",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1007,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "TabBar",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1008,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "Tab",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1009,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "TabContent",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x100A,
         .type_hint = KRYON_TYPE_HINT_ANY
     },
     {
         .canonical = "Link",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x100B,
         .type_hint = KRYON_TYPE_HINT_ANY
     }
 };
 
 //==============================================================================
 // Utility Functions - Working with Grouped Structure
 //==============================================================================
 
 //==============================================================================
// SYNTAX KEYWORD MAPPINGS (0x8000+)
// Special syntax keywords and directives: @onload, @component, @variable, @for, etc.
//==============================================================================

const KryonSyntaxGroup kryon_syntax_groups[] = {
    // -- Lifecycle Directives (0x80xx) --
    {
        .canonical = "onload",
        .aliases = (const char*[]){"ready", "startup", "initialize", NULL},
        .hex_code = 0x8001,
        .type_hint = KRYON_TYPE_HINT_REFERENCE
    },
    {
        .canonical = "onmount",
        .aliases = (const char*[]){"mount", NULL},
        .hex_code = 0x8002,
        .type_hint = KRYON_TYPE_HINT_REFERENCE
    },
    {
        .canonical = "onunmount",
        .aliases = (const char*[]){"unmount", "cleanup", NULL},
        .hex_code = 0x8003,
        .type_hint = KRYON_TYPE_HINT_REFERENCE
    },

    // -- Structure Directives (0x81xx) --
    {
        .canonical = "component",
        .aliases = (const char*[]){NULL},
        .hex_code = 0x8100,
        .type_hint = KRYON_TYPE_HINT_ANY
    },
    {
        .canonical = "variable",
        .aliases = (const char*[]){"var", NULL},
        .hex_code = 0x8101,
        .type_hint = KRYON_TYPE_HINT_ANY
    },
    {
        .canonical = "function",
        .aliases = (const char*[]){"func", NULL},
        .hex_code = 0x8102,
        .type_hint = KRYON_TYPE_HINT_REFERENCE
    },

    // -- Control Flow Directives (0x82xx) --
    {
        .canonical = "for",
        .aliases = (const char*[]){"foreach", "loop", NULL},
        .hex_code = 0x8200,
        .type_hint = KRYON_TYPE_HINT_ANY
    },
    {
        .canonical = "if",
        .aliases = (const char*[]){"when", NULL},
        .hex_code = 0x8300,
        .type_hint = KRYON_TYPE_HINT_BOOLEAN
    },
    {
        .canonical = "else",
        .aliases = (const char*[]){"otherwise", NULL},
        .hex_code = 0x8301,
        .type_hint = KRYON_TYPE_HINT_ANY
    },
    
    // -- Event Directives (0x85xx) --
    {
        .canonical = "event",
        .aliases = (const char*[]){NULL},
        .hex_code = 0x8500,
        .type_hint = KRYON_TYPE_HINT_ANY
    }
};

//==============================================================================
// Utility Functions - Working with Grouped Structure
//==============================================================================

// Helper function to check if a name matches any alias in a group
 static bool kryon_name_matches_group(const char *name, const KryonPropertyGroup *group) {
     // Check canonical name
     if (strcmp(group->canonical, name) == 0) {
         return true;
     }
     
     // Check aliases
     if (group->aliases) {
         for (int i = 0; group->aliases[i] != NULL; i++) {
             if (strcmp(group->aliases[i], name) == 0) {
                 return true;
             }
         }
     }
     
     return false;
 }
 
 static bool kryon_element_name_matches_group(const char *name, const KryonElementGroup *group) {
     // Check canonical name
     if (strcmp(group->canonical, name) == 0) {
         return true;
     }
     
     // Check aliases
     if (group->aliases) {
         for (int i = 0; group->aliases[i] != NULL; i++) {
             if (strcmp(group->aliases[i], name) == 0) {
                 return true;
             }
         }
     }
     
     return false;
 }
 
 static bool kryon_syntax_name_matches_group(const char *name, const KryonSyntaxGroup *group) {
    // Check canonical name
    if (strcmp(group->canonical, name) == 0) {
        return true;
    }
    
    // Check aliases
    if (group->aliases) {
        for (int i = 0; group->aliases[i] != NULL; i++) {
            if (strcmp(group->aliases[i], name) == 0) {
                return true;
            }
        }
    }
    
    return false;
}

uint16_t kryon_get_property_hex(const char *name) {
     const int group_count = sizeof(kryon_property_groups) / sizeof(kryon_property_groups[0]);
     
     for (int i = 0; i < group_count; i++) {
         if (kryon_name_matches_group(name, &kryon_property_groups[i])) {
             return kryon_property_groups[i].hex_code;
         }
     }
     
     return 0; // Not found
 }
 
 const char *kryon_get_property_name(uint16_t hex_code) {
     const int group_count = sizeof(kryon_property_groups) / sizeof(kryon_property_groups[0]);
     
     for (int i = 0; i < group_count; i++) {
         if (kryon_property_groups[i].hex_code == hex_code) {
             return kryon_property_groups[i].canonical; // Always return canonical name
         }
     }
     
     return NULL; // Not found
 }
 
 KryonValueTypeHint kryon_get_property_type_hint(uint16_t hex_code) {
     const int group_count = sizeof(kryon_property_groups) / sizeof(kryon_property_groups[0]);
     
     for (int i = 0; i < group_count; i++) {
         if (kryon_property_groups[i].hex_code == hex_code) {
             return kryon_property_groups[i].type_hint;
         }
     }
     
     return KRYON_TYPE_HINT_ANY; // Default for unknown properties
 }
 
 uint16_t kryon_get_element_hex(const char *name) {
     const int group_count = sizeof(kryon_element_groups) / sizeof(kryon_element_groups[0]);
     
     for (int i = 0; i < group_count; i++) {
         if (kryon_element_name_matches_group(name, &kryon_element_groups[i])) {
             return kryon_element_groups[i].hex_code;
         }
     }
     
     return 0; // Not found
 }
 
 const char *kryon_get_element_name(uint16_t hex_code) {
     const int group_count = sizeof(kryon_element_groups) / sizeof(kryon_element_groups[0]);
     
     for (int i = 0; i < group_count; i++) {
         if (kryon_element_groups[i].hex_code == hex_code) {
             return kryon_element_groups[i].canonical; // Always return canonical name
         }
     }
     
     return NULL; // Not found
 }
 
 // Get all aliases for a property (useful for IDE/tooling)
 const char **kryon_get_property_aliases(const char *name) {
     const int group_count = sizeof(kryon_property_groups) / sizeof(kryon_property_groups[0]);
     
     for (int i = 0; i < group_count; i++) {
         if (kryon_name_matches_group(name, &kryon_property_groups[i])) {
             return kryon_property_groups[i].aliases;
         }
     }
     
     return NULL; // Not found
 }
 
 // Get all aliases for a element (useful for IDE/tooling)
 const char **kryon_get_element_aliases(const char *name) {
     const int group_count = sizeof(kryon_element_groups) / sizeof(kryon_element_groups[0]);
     
     for (int i = 0; i < group_count; i++) {
         if (kryon_element_name_matches_group(name, &kryon_element_groups[i])) {
             return kryon_element_groups[i].aliases;
         }
     }
     
     return NULL; // Not found
 }

/**
 * @brief Get element type name from hex code (unified API alias)
 */
const char *kryon_get_element_type_name(uint16_t hex_code) {
    return kryon_get_element_name(hex_code);
}

uint16_t kryon_get_syntax_hex(const char *name) {
    const int group_count = sizeof(kryon_syntax_groups) / sizeof(kryon_syntax_groups[0]);
    
    for (int i = 0; i < group_count; i++) {
        if (kryon_syntax_name_matches_group(name, &kryon_syntax_groups[i])) {
            return kryon_syntax_groups[i].hex_code;
        }
    }
    
    return 0; // Not found
}

const char *kryon_get_syntax_name(uint16_t hex_code) {
    const int group_count = sizeof(kryon_syntax_groups) / sizeof(kryon_syntax_groups[0]);
    
    for (int i = 0; i < group_count; i++) {
        if (kryon_syntax_groups[i].hex_code == hex_code) {
            return kryon_syntax_groups[i].canonical; // Always return canonical name
        }
    }
    
    return NULL; // Not found
}

// Get all aliases for a syntax keyword (useful for IDE/tooling)
const char **kryon_get_syntax_aliases(const char *name) {
    const int group_count = sizeof(kryon_syntax_groups) / sizeof(kryon_syntax_groups[0]);
    
    for (int i = 0; i < group_count; i++) {
        if (kryon_syntax_name_matches_group(name, &kryon_syntax_groups[i])) {
            return kryon_syntax_groups[i].aliases;
        }
    }
    
    return NULL; // Not found
}