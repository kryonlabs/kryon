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
 #include <stdlib.h>
 #include <stdio.h>
 #include <time.h>
 
 //==============================================================================
// PROPERTY CATEGORIES (Hierarchical Organization)
//==============================================================================

const KryonPropertyCategory kryon_property_categories[] = {
    // Base category - properties all elements inherit
    [KRYON_CATEGORY_BASE] = {
        .name = "Base",
        .range_start = 0x0000,
        .range_end = 0x00FF,
        .inherited_ranges = NULL, // No inheritance for base
        .description = "Core properties available to all elements"
    },
    // Layout category - positioning and sizing
    [KRYON_CATEGORY_LAYOUT] = {
        .name = "Layout",
        .range_start = 0x0100,
        .range_end = 0x01FF,
        .inherited_ranges = (uint16_t[]){0x0000, 0}, // Inherits from Base
        .description = "Layout and positioning properties"
    },
    // Visual category - appearance and styling
    [KRYON_CATEGORY_VISUAL] = {
        .name = "Visual",
        .range_start = 0x0200,
        .range_end = 0x02FF,
        .inherited_ranges = (uint16_t[]){0x0000, 0}, // Inherits from Base
        .description = "Visual appearance and styling properties"
    },
    // Typography category - text and font properties
    [KRYON_CATEGORY_TYPOGRAPHY] = {
        .name = "Typography",
        .range_start = 0x0300,
        .range_end = 0x03FF,
        .inherited_ranges = (uint16_t[]){0x0000, 0}, // Inherits from Base
        .description = "Text and typography properties"
    },
    // Transform category - animations and transformations
    [KRYON_CATEGORY_TRANSFORM] = {
        .name = "Transform",
        .range_start = 0x0400,
        .range_end = 0x04FF,
        .inherited_ranges = (uint16_t[]){0x0000, 0}, // Inherits from Base
        .description = "Transform and animation properties"
    },
    // Interactive category - user interaction properties
    [KRYON_CATEGORY_INTERACTIVE] = {
        .name = "Interactive",
        .range_start = 0x0500,
        .range_end = 0x05FF,
        .inherited_ranges = (uint16_t[]){0x0000, 0}, // Inherits from Base
        .description = "User interaction and event properties"
    },
    // Element-specific category - specialized properties
    [KRYON_CATEGORY_ELEMENT_SPECIFIC] = {
        .name = "ElementSpecific",
        .range_start = 0x0600,
        .range_end = 0x06FF,
        .inherited_ranges = (uint16_t[]){0x0000, 0}, // Inherits from Base
        .description = "Element-specific properties"
    },
    // Window category - window management properties
    [KRYON_CATEGORY_WINDOW] = {
        .name = "Window",
        .range_start = 0x0700,
        .range_end = 0x07FF,
        .inherited_ranges = (uint16_t[]){0x0000, 0}, // Inherits from Base
        .description = "Window management properties"
    },
    // Checkbox category - checkbox-specific properties
    [KRYON_CATEGORY_CHECKBOX] = {
        .name = "Checkbox",
        .range_start = 0x0800,
        .range_end = 0x08FF,
        .inherited_ranges = (uint16_t[]){0x0000, 0x0500, 0}, // Inherits from Base + Interactive
        .description = "Checkbox-specific properties"
    },
    // Directive category - for @for, @if, @while properties
    [KRYON_CATEGORY_DIRECTIVE] = {
        .name = "Directive",
        .range_start = 0x8200,
        .range_end = 0x82FF,
        .inherited_ranges = (uint16_t[]){0x0000, 0}, // Inherits from Base
        .description = "Control flow directive properties"
    }
};

//==============================================================================
// ELEMENT CATEGORIES (Hierarchical Organization)
//==============================================================================

const KryonElementCategory kryon_element_categories[] = {
    // Base element category - all elements inherit from this
    [KRYON_ELEM_CATEGORY_BASE] = {
        .name = "BaseElement",
        .range_start = 0x0000,
        .range_end = 0x0000,
        .inherited_ranges = NULL,
        .valid_properties = (uint16_t[]){0x0000, 0}, // All base properties
        .description = "Base element with core properties"
    },
    // Layout elements - containers and layout structures
    [KRYON_ELEM_CATEGORY_LAYOUT] = {
        .name = "LayoutElement",
        .range_start = 0x0001,
        .range_end = 0x00FF,
        .inherited_ranges = (uint16_t[]){0x0000, 0}, // Inherits from BaseElement
        .valid_properties = (uint16_t[]){0x0000, 0x0100, 0x0200, 0}, // Base + Layout + Visual
        .description = "Layout containers and positioning elements"
    },
    // Content elements - interactive and display elements
    [KRYON_ELEM_CATEGORY_CONTENT] = {
        .name = "ContentElement",
        .range_start = 0x0400,
        .range_end = 0x04FF,
        .inherited_ranges = (uint16_t[]){0x0000, 0}, // Inherits from BaseElement
        .valid_properties = (uint16_t[]){0x0000, 0x0100, 0x0200, 0x0300, 0x0500, 0x0600, 0}, // Most properties
        .description = "Interactive content and display elements"
    },
    // Text elements - text display and input
    [KRYON_ELEM_CATEGORY_TEXT] = {
        .name = "TextElement",
        .range_start = 0x0400,
        .range_end = 0x0400,
        .inherited_ranges = (uint16_t[]){0x0000, 0x0400, 0}, // Base + Content
        .valid_properties = (uint16_t[]){0x0000, 0x0100, 0x0200, 0x0300, 0x0500, 0x0600, 0}, // Full set
        .description = "Text display elements"
    },

    // Interactive elements - buttons and inputs
    [KRYON_ELEM_CATEGORY_INTERACTIVE] = {
        .name = "InteractiveElement",
        .range_start = 0x0401,
        .range_end = 0x0403,
        .inherited_ranges = (uint16_t[]){0x0000, 0x0400, 0}, // Base + Content
        .valid_properties = (uint16_t[]){0x0000, 0x0100, 0x0200, 0x0300, 0x0500, 0x0600, 0}, // Full set
        .description = "Interactive form and input elements"
    },

    // Application elements - top-level containers
    [KRYON_ELEM_CATEGORY_APPLICATION] = {
        .name = "ApplicationElement",
        .range_start = 0x1000,
        .range_end = 0x1FFF,
        .inherited_ranges = (uint16_t[]){0x0000, 0}, // Inherits from BaseElement
        .valid_properties = (uint16_t[]){0x0000, 0x0100, 0x0200, 0x0700, 0}, // Base + Layout + Visual + Window
        .description = "Top-level application and modal elements"
    }
};

//==============================================================================
// SEPARATED PROPERTY ARRAYS BY CATEGORY
//==============================================================================

// Base properties (0x0000-0x00FF) - Available to all elements
const KryonPropertyGroup kryon_base_properties[] = {
    {
        .canonical = "id",
        .aliases = (const char*[]){NULL},
        .hex_code = 0x0001,
        .type_hint = KRYON_TYPE_HINT_STRING,
    },
    {
        .canonical = "class",
        .aliases = (const char*[]){"className", NULL},
        .hex_code = 0x0002,
        .type_hint = KRYON_TYPE_HINT_STRING,
    },
    {
        .canonical = "style",
        .aliases = (const char*[]){NULL},
        .hex_code = 0x0003,
        .type_hint = KRYON_TYPE_HINT_STRING,
    },
    {
        .canonical = "theme",
        .aliases = (const char*[]){NULL},
        .hex_code = 0x0004,
        .type_hint = KRYON_TYPE_HINT_STRING,
    },
    {
        .canonical = "extends",
        .aliases = (const char*[]){NULL},
        .hex_code = 0x0005,
        .type_hint = KRYON_TYPE_HINT_STRING,
    },
    {
        .canonical = "title",
        .aliases = (const char*[]){NULL},
        .hex_code = 0x0006,
        .type_hint = KRYON_TYPE_HINT_STRING,
    },
    {
        .canonical = "version",
        .aliases = (const char*[]){NULL},
        .hex_code = 0x0007,
        .type_hint = KRYON_TYPE_HINT_STRING,
    }
};

// Layout properties (0x0100-0x01FF) - Positioning and sizing
const KryonPropertyGroup kryon_layout_properties[] = {
    {
        .canonical = "width",
        .aliases = (const char*[]){NULL},
        .hex_code = 0x0100,
        .type_hint = KRYON_TYPE_HINT_DIMENSION,
    },
    {
        .canonical = "height",
        .aliases = (const char*[]){NULL},
        .hex_code = 0x0101,
        .type_hint = KRYON_TYPE_HINT_DIMENSION,
    },
    {
        .canonical = "minWidth",
        .aliases = (const char*[]){"minW", NULL},
        .hex_code = 0x0102,
        .type_hint = KRYON_TYPE_HINT_DIMENSION,
    },
    {
        .canonical = "maxWidth",
        .aliases = (const char*[]){"maxW", NULL},
        .hex_code = 0x0103,
        .type_hint = KRYON_TYPE_HINT_DIMENSION,
    },
    {
        .canonical = "minHeight",
        .aliases = (const char*[]){"minH", NULL},
        .hex_code = 0x0104,
        .type_hint = KRYON_TYPE_HINT_DIMENSION,
    },
    {
        .canonical = "maxHeight",
        .aliases = (const char*[]){"maxH", NULL},
        .hex_code = 0x0105,
        .type_hint = KRYON_TYPE_HINT_DIMENSION,
    },
    {
        .canonical = "padding",
        .aliases = (const char*[]){"p", NULL},
        .hex_code = 0x0106,
        .type_hint = KRYON_TYPE_HINT_SPACING,
    },
    {
        .canonical = "margin",
        .aliases = (const char*[]){"m", NULL},
        .hex_code = 0x0107,
        .type_hint = KRYON_TYPE_HINT_SPACING,
    },
    {
        .canonical = "aspectRatio",
        .aliases = (const char*[]){"aspect", NULL},
        .hex_code = 0x0109,
        .type_hint = KRYON_TYPE_HINT_FLOAT,
    },
    {
        .canonical = "flex",
        .aliases = (const char*[]){NULL},
        .hex_code = 0x010A,
        .type_hint = KRYON_TYPE_HINT_FLOAT,
    },
    {
        .canonical = "gap",
        .aliases = (const char*[]){"spacing", NULL},
        .hex_code = 0x010B,
        .type_hint = KRYON_TYPE_HINT_DIMENSION,
    },
    {
        .canonical = "columns",
        .aliases = (const char*[]){NULL},
        .hex_code = 0x010C,
        .type_hint = KRYON_TYPE_HINT_INTEGER,
    },
    {
        .canonical = "row_spacing",
        .aliases = (const char*[]){"rowSpacing", NULL},
        .hex_code = 0x011F,
        .type_hint = KRYON_TYPE_HINT_DIMENSION,
    },
    {
        .canonical = "column_spacing",
        .aliases = (const char*[]){"columnSpacing", NULL},
        .hex_code = 0x0120,
        .type_hint = KRYON_TYPE_HINT_DIMENSION,
    },
    {
        .canonical = "posX",
        .aliases = (const char*[]){"x", "positionX", NULL},
        .hex_code = 0x010D,
        .type_hint = KRYON_TYPE_HINT_FLOAT,
    },
    {
        .canonical = "posY",
        .aliases = (const char*[]){"y", "positionY", NULL},
        .hex_code = 0x010E,
        .type_hint = KRYON_TYPE_HINT_FLOAT,
    },
    {
        .canonical = "zIndex",
        .aliases = (const char*[]){"z", "layerIndex", NULL},
        .hex_code = 0x010F,
        .type_hint = KRYON_TYPE_HINT_INTEGER,
    }
};

// Visual properties (0x0200-0x02FF) - Appearance and styling
const KryonPropertyGroup kryon_visual_properties[] = {
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
    }
};

// Typography properties (0x0300-0x03FF) - Text and font properties
const KryonPropertyGroup kryon_typography_properties[] = {
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
    }
};

// Transform properties (0x0400-0x04FF) - Transform and animation properties
const KryonPropertyGroup kryon_transform_properties[] = {
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
    }
};

// Interactive properties (0x0500-0x05FF) - User interaction
const KryonPropertyGroup kryon_interactive_properties[] = {
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
    }
};

// Element-specific properties (0x0600-0x06FF) - Specialized properties
const KryonPropertyGroup kryon_element_specific_properties[] = {
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
    }
};

// Window properties (0x0700-0x07FF) - Window management
const KryonPropertyGroup kryon_window_properties[] = {
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
    }
};

// Checkbox properties (0x0800-0x08FF) - Checkbox-specific properties
const KryonPropertyGroup kryon_checkbox_properties[] = {
    {
        .canonical = "label",
        .aliases = (const char*[]){"text", NULL},
        .hex_code = 0x0800,
        .type_hint = KRYON_TYPE_HINT_STRING
    },
    {
        .canonical = "checked",
        .aliases = (const char*[]){NULL},
        .hex_code = 0x0801,
        .type_hint = KRYON_TYPE_HINT_BOOLEAN
    },
    {
        .canonical = "indeterminate",
        .aliases = (const char*[]){NULL},
        .hex_code = 0x0802,
        .type_hint = KRYON_TYPE_HINT_BOOLEAN
    },
    {
        .canonical = "checkColor",
        .aliases = (const char*[]){"checkmarkColor", NULL},
        .hex_code = 0x0803,
        .type_hint = KRYON_TYPE_HINT_COLOR
    },
    {NULL, NULL, 0, 0} // Sentinel
};

// COMBINED PROPERTY MAPPINGS (0x0000 - 0x0FFF)
 // Properties that elements can have: posX, width, color, padding, etc.
 //==============================================================================
 
 const KryonPropertyGroup kryon_property_groups[] = {
     // -- Meta & System Properties (0x00xx) --
     {
         .canonical = "id",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0001,
        .type_hint = KRYON_TYPE_HINT_STRING,
     },
     {
         .canonical = "class",
         .aliases = (const char*[]){"className", NULL},
         .hex_code = 0x0002,
        .type_hint = KRYON_TYPE_HINT_STRING,
     },
     {
         .canonical = "style",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0003,
        .type_hint = KRYON_TYPE_HINT_STRING,
     },
     {
         .canonical = "theme",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0004,
        .type_hint = KRYON_TYPE_HINT_STRING,
     },
     {
         .canonical = "extends",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0005,
        .type_hint = KRYON_TYPE_HINT_STRING,
     },
     {
         .canonical = "title",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0006,
        .type_hint = KRYON_TYPE_HINT_STRING,
     },
     {
         .canonical = "version",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0007,
        .type_hint = KRYON_TYPE_HINT_STRING,
     },
 
     // -- Layout Properties (0x01xx) --
     {
         .canonical = "width",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0100,
         .type_hint = KRYON_TYPE_HINT_DIMENSION,
     },
     {
         .canonical = "height",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0101,
         .type_hint = KRYON_TYPE_HINT_DIMENSION,
     },
     {
         .canonical = "minWidth",
         .aliases = (const char*[]){"minW", NULL},
         .hex_code = 0x0102,
         .type_hint = KRYON_TYPE_HINT_DIMENSION,
     },
     {
         .canonical = "maxWidth",
         .aliases = (const char*[]){"maxW", NULL},
         .hex_code = 0x0103,
         .type_hint = KRYON_TYPE_HINT_DIMENSION,
     },
     {
         .canonical = "minHeight",
         .aliases = (const char*[]){"minH", NULL},
         .hex_code = 0x0104,
         .type_hint = KRYON_TYPE_HINT_DIMENSION,
     },
     {
         .canonical = "maxHeight",
         .aliases = (const char*[]){"maxH", NULL},
         .hex_code = 0x0105,
         .type_hint = KRYON_TYPE_HINT_DIMENSION,
     },
     {
         .canonical = "padding",
         .aliases = (const char*[]){"p", NULL},
         .hex_code = 0x0106,
         .type_hint = KRYON_TYPE_HINT_SPACING,
     },
     {
         .canonical = "margin",
         .aliases = (const char*[]){"m", NULL},
         .hex_code = 0x0107,
         .type_hint = KRYON_TYPE_HINT_SPACING,
     },
     {
         .canonical = "aspectRatio",
         .aliases = (const char*[]){"aspect", NULL},
         .hex_code = 0x0109,
         .type_hint = KRYON_TYPE_HINT_FLOAT,
     },
     {
         .canonical = "flex",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x010A,
         .type_hint = KRYON_TYPE_HINT_FLOAT,
     },
     {
         .canonical = "gap",
         .aliases = (const char*[]){"spacing", NULL},
         .hex_code = 0x010B,
         .type_hint = KRYON_TYPE_HINT_DIMENSION,
     },
     {
         .canonical = "columns",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x010C,
         .type_hint = KRYON_TYPE_HINT_INTEGER,
     },
     {
         .canonical = "row_spacing",
         .aliases = (const char*[]){"rowSpacing", NULL},
         .hex_code = 0x011F,
         .type_hint = KRYON_TYPE_HINT_DIMENSION,
     },
     {
         .canonical = "column_spacing",
         .aliases = (const char*[]){"columnSpacing", NULL},
         .hex_code = 0x0120,
         .type_hint = KRYON_TYPE_HINT_DIMENSION,
     },
     {
         .canonical = "posX",
         .aliases = (const char*[]){"x", "positionX", NULL},
         .hex_code = 0x010D,
         .type_hint = KRYON_TYPE_HINT_FLOAT,
     },
     {
         .canonical = "posY",
         .aliases = (const char*[]){"y", "positionY", NULL},
         .hex_code = 0x010E,
         .type_hint = KRYON_TYPE_HINT_FLOAT,
     },
     {
         .canonical = "zIndex",
         .aliases = (const char*[]){"z", "layerIndex", NULL},
         .hex_code = 0x010F,
         .type_hint = KRYON_TYPE_HINT_INTEGER,
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
         .type_hint = KRYON_TYPE_HINT_COLOR,
     },
     {
         .canonical = "color",
         .aliases = (const char*[]){"textColor", NULL},
         .hex_code = 0x0201,
         .type_hint = KRYON_TYPE_HINT_COLOR,
     },
     {
         .canonical = "border",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0202,
         .type_hint = KRYON_TYPE_HINT_STRING,
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
     },

     // -- Checkbox Properties (0x08xx) --
     {
         .canonical = "label",
         .aliases = (const char*[]){"text", NULL},
         .hex_code = 0x0800,
         .type_hint = KRYON_TYPE_HINT_STRING
     },
     {
         .canonical = "checked",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0801,
         .type_hint = KRYON_TYPE_HINT_BOOLEAN
     },
     {
         .canonical = "indeterminate",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0802,
         .type_hint = KRYON_TYPE_HINT_BOOLEAN
     },
     {
         .canonical = "checkColor",
         .aliases = (const char*[]){"checkmarkColor", NULL},
         .hex_code = 0x0803,
         .type_hint = KRYON_TYPE_HINT_COLOR
     },
     {NULL, NULL, 0, 0} // NULL terminator - required for array iteration
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
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_COUNT
         }
     },
 
     // -- Layout Elements (0x0001 - 0x00FF) --
     {
         .canonical = "Column",
         .aliases = (const char*[]){"Col", NULL},
         .hex_code = 0x0001,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_ELEMENT_SPECIFIC,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "Row",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0002,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_ELEMENT_SPECIFIC,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "Center",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0003,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "Container",
         .aliases = (const char*[]){"Box", NULL},
         .hex_code = 0x0004,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "Flex",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0005,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "Spacer",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0006,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_COUNT
         }
     },
 
     // -- Content Elements (0x0400 - 0x04FF) --
     {
         .canonical = "Text",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0400,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,      // id, class, style
             KRYON_CATEGORY_LAYOUT,    // width, height, padding
             KRYON_CATEGORY_VISUAL,    // background, color, border
             KRYON_CATEGORY_TYPOGRAPHY,// fontSize, textAlign, fontFamily
             KRYON_CATEGORY_COUNT      // Sentinel (invalid category)
         }
     },
     {
         .canonical = "Button",
         .aliases = (const char*[]){"Btn", NULL},
         .hex_code = 0x0401,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,        // id, class, style
             KRYON_CATEGORY_LAYOUT,      // width, height, padding
             KRYON_CATEGORY_VISUAL,      // background, color, border
             KRYON_CATEGORY_TYPOGRAPHY,  // fontSize, textAlign, fontFamily
             KRYON_CATEGORY_INTERACTIVE, // onClick, onHover
             KRYON_CATEGORY_COUNT        // Sentinel
         }
     },
     {
         .canonical = "Image",
         .aliases = (const char*[]){"Img", NULL},
         .hex_code = 0x0402,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_ELEMENT_SPECIFIC, // src, alt, width, height properties
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "Input",
         .aliases = (const char*[]){"TextInput", NULL},
         .hex_code = 0x0403,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,             // id, class, style
             KRYON_CATEGORY_LAYOUT,           // width, height, padding
             KRYON_CATEGORY_VISUAL,           // background, color, border
             KRYON_CATEGORY_TYPOGRAPHY,       // fontSize, fontFamily
             KRYON_CATEGORY_INTERACTIVE,      // onClick, onHover
             KRYON_CATEGORY_ELEMENT_SPECIFIC, // value, placeholder, type
             KRYON_CATEGORY_COUNT             // Sentinel
         }
     },
     {
         .canonical = "Checkbox",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0404,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,      // id, class, style
             KRYON_CATEGORY_LAYOUT,    // width, height, padding
             KRYON_CATEGORY_VISUAL,    // background, color, border
             KRYON_CATEGORY_CHECKBOX,  // label, checked, indeterminate, checkColor
             KRYON_CATEGORY_COUNT      // Sentinel
         }
     },
     {
         .canonical = "Slider",
         .aliases = (const char*[]){"Range", NULL},
         .hex_code = 0x0405,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_INTERACTIVE,
             KRYON_CATEGORY_ELEMENT_SPECIFIC, // value, min, max, step
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "Dropdown",
         .aliases = (const char*[]){"Select", NULL},
         .hex_code = 0x0406,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_INTERACTIVE,
             KRYON_CATEGORY_ELEMENT_SPECIFIC, // options, value, placeholder
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "TabBar",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0407,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_INTERACTIVE,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "Tab",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0408,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_INTERACTIVE,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "TabContent",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x0409,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_COUNT
         }
     },
 
     // -- Application Elements (0x1000+) --
     {
         .canonical = "App",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1000,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_WINDOW,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "Modal",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1001,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_INTERACTIVE,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "Form",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1002,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_INTERACTIVE,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "Grid",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1003,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "List",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1004,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_INTERACTIVE,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "Dropdown",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1005,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_INTERACTIVE,
             KRYON_CATEGORY_ELEMENT_SPECIFIC,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "Menu",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1006,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_INTERACTIVE,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "Tabs",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1007,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_INTERACTIVE,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "TabBar",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1008,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_INTERACTIVE,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "Tab",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x1009,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_INTERACTIVE,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "TabContent",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x100A,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_COUNT
         }
     },
     {
         .canonical = "Link",
         .aliases = (const char*[]){NULL},
         .hex_code = 0x100B,
         .type_hint = KRYON_TYPE_HINT_ANY,
         .allowed_categories = (KryonPropertyCategoryIndex[]){
             KRYON_CATEGORY_BASE,
             KRYON_CATEGORY_LAYOUT,
             KRYON_CATEGORY_VISUAL,
             KRYON_CATEGORY_TYPOGRAPHY,
             KRYON_CATEGORY_INTERACTIVE,
             KRYON_CATEGORY_COUNT
         }
     },
     {NULL, NULL, 0, 0, NULL} // NULL terminator - required for array iteration
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
// ENHANCED PERFORMANCE SYSTEM (Phase 1 Implementation)
//==============================================================================

// Static instances for hash tables and string table
static KryonPropertyHashTable property_hash_table = {0};
static KryonStringTable string_table = {0};
static bool system_initialized = false;

//==============================================================================
// HASH FUNCTIONS AND STRING TABLE
//==============================================================================

// Forward declarations
static void check_for_conflicts(void);
static void insert_property(const char *name, uint16_t hex_code, uint16_t group_index, bool is_alias);
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

// Add string to deduplication table
static uint32_t add_string(KryonStringTable *table, const char *str) {
    // Check if string already exists
    for (uint32_t i = 0; i < table->count; i++) {
        if (strcmp(table->strings[i], str) == 0) {
            return i;
        }
    }

    // Add new string
    if (table->count >= table->capacity) {
        table->capacity = table->capacity ? table->capacity * 2 : 64;
        table->strings = realloc(table->strings, table->capacity * sizeof(char*));
        if (!table->strings) {
            return 0; // Memory allocation failed
        }
    }

    table->strings[table->count] = str;
    return table->count++;
}

// Initialize hash table entry
static void insert_property(const char *name, uint16_t hex_code, uint16_t group_index, bool is_alias) {
    if (!name || property_hash_table.count >= property_hash_table.size) {
        return;
    }

    uint32_t hash = kryon_hash_string(name);
    uint32_t index = hash & (property_hash_table.size - 1);

    // Linear probing for collision resolution
    for (uint32_t i = 0; i < property_hash_table.size; i++) {
        uint32_t current_index = (index + i) & (property_hash_table.size - 1);
        KryonPropertyHashEntry *entry = &property_hash_table.entries[current_index];

        if (entry->name == NULL) { // Empty slot found
            entry->name = name;
            entry->hex_code = hex_code;
            entry->group_index = group_index;
            entry->is_alias = is_alias;
            property_hash_table.count++;
            return;
        }
    }
}

// Initialize the enhanced mapping system
static void initialize_mapping_system(void) {
    if (system_initialized) return;

    // 1. Count total entries and build string table
    uint32_t total_entries = 0;
    for (int i = 0; kryon_property_groups[i].canonical; i++) {
        add_string(&string_table, kryon_property_groups[i].canonical);
        total_entries++; // canonical name

        if (kryon_property_groups[i].aliases) {
            for (int j = 0; kryon_property_groups[i].aliases[j]; j++) {
                add_string(&string_table, kryon_property_groups[i].aliases[j]);
                total_entries++; // each alias
            }
        }
    }

    // 2. Create hash table (size = next power of 2)
    uint32_t size = 1;
    while (size < total_entries * 2) size <<= 1;

    property_hash_table.entries = malloc(size * sizeof(KryonPropertyHashEntry));
    if (!property_hash_table.entries) {
        return; // Memory allocation failed
    }

    property_hash_table.size = size;
    property_hash_table.count = 0;
    property_hash_table.initialized = true;

    // Initialize all entries to NULL
    memset(property_hash_table.entries, 0, size * sizeof(KryonPropertyHashEntry));

    // 3. Populate hash table with linear probing
    for (int i = 0; kryon_property_groups[i].canonical; i++) {
        // Add canonical name
        insert_property(kryon_property_groups[i].canonical,
                       kryon_property_groups[i].hex_code, i, false);

        // Add aliases with alias flag
        if (kryon_property_groups[i].aliases) {
            for (int j = 0; kryon_property_groups[i].aliases[j]; j++) {
                insert_property(kryon_property_groups[i].aliases[j],
                               kryon_property_groups[i].hex_code, i, true);
            }
        }
    }

    // 4. Check for hex code conflicts
    check_for_conflicts();

    system_initialized = true;
}

// Check for hex code conflicts
static void check_for_conflicts(void) {
    // Check for duplicate hex codes in property groups
    for (int i = 0; kryon_property_groups[i].canonical; i++) {
        uint16_t hex1 = kryon_property_groups[i].hex_code;

        for (int j = i + 1; kryon_property_groups[j].canonical; j++) {
            uint16_t hex2 = kryon_property_groups[j].hex_code;

            if (hex1 == hex2) {
                fprintf(stderr, "ERROR: Hex code conflict 0x%04X between '%s' and '%s'\n",
                        hex1, kryon_property_groups[i].canonical, kryon_property_groups[j].canonical);
            }
        }
    }
}

//==============================================================================
// UTILITY FUNCTIONS - Working with Grouped Structure
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
    // Search main property groups first
    for (int i = 0; kryon_property_groups[i].canonical; i++) {
        if (kryon_name_matches_group(name, &kryon_property_groups[i])) {
            return kryon_property_groups[i].hex_code;
        }
    }

    // Search separated arrays
    for (int i = 0; kryon_checkbox_properties[i].canonical; i++) {
        if (kryon_name_matches_group(name, &kryon_checkbox_properties[i])) {
            return kryon_checkbox_properties[i].hex_code;
        }
    }

    return 0; // Not found
}
 
 const char *kryon_get_property_name(uint16_t hex_code) {
     // Search main property groups first
     for (int i = 0; kryon_property_groups[i].canonical; i++) {
         if (kryon_property_groups[i].hex_code == hex_code) {
             return kryon_property_groups[i].canonical; // Always return canonical name
         }
     }

     // Search separated arrays
     for (int i = 0; kryon_checkbox_properties[i].canonical; i++) {
         if (kryon_checkbox_properties[i].hex_code == hex_code) {
             return kryon_checkbox_properties[i].canonical;
         }
     }
     
     return NULL; // Not found
 }
 
 KryonValueTypeHint kryon_get_property_type_hint(uint16_t hex_code) {
     // Search main property groups first
     for (int i = 0; kryon_property_groups[i].canonical; i++) {
         if (kryon_property_groups[i].hex_code == hex_code) {
             return kryon_property_groups[i].type_hint;
         }
     }

     // Search separated arrays
     for (int i = 0; kryon_checkbox_properties[i].canonical; i++) {
         if (kryon_checkbox_properties[i].hex_code == hex_code) {
             return kryon_checkbox_properties[i].type_hint;
         }
     }
     
     return KRYON_TYPE_HINT_ANY; // Default for unknown properties
 }
 
 uint16_t kryon_get_element_hex(const char *name) {
    // Search main element groups first
    for (int i = 0; kryon_element_groups[i].canonical; i++) {
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
//==============================================================================
// ENHANCED VALIDATION FUNCTIONS (Phase 2 Implementation)
//==============================================================================

bool kryon_is_property_alias(const char *name) {
    if (!system_initialized) {
        initialize_mapping_system();
    }

    if (!name) return false;

    uint32_t hash = kryon_hash_string(name);
    uint32_t index = hash & (property_hash_table.size - 1);

    // Linear probing for collision resolution
    for (uint32_t i = 0; i < property_hash_table.size; i++) {
        uint32_t current_index = (index + i) & (property_hash_table.size - 1);
        KryonPropertyHashEntry *entry = &property_hash_table.entries[current_index];

        if (entry->name == NULL) {
            return false; // Not found
        }

        if (strcmp(entry->name, name) == 0) {
            return entry->is_alias;
        }
    }

    return false; // Not found
}


//==============================================================================
// TESTING AND DEBUGGING FUNCTIONS
//==============================================================================

// Test function to demonstrate the enhanced system
void kryon_mappings_test(void) {
    if (!system_initialized) {
        initialize_mapping_system();
    }

    printf("🔍 KRYON MAPPINGS TEST\n");
    printf("=====================\n");

    // Test hash lookups with various properties
    const char* test_properties[] = {
        "width", "height", "backgroundColor", "color",
        "text", "value", "fontSize", "padding"
    };

    printf("📊 Property Lookup Test:\n");
    for (size_t i = 0; i < sizeof(test_properties) / sizeof(test_properties[0]); i++) {
        uint16_t hex = kryon_get_property_hex(test_properties[i]);
        bool is_alias = kryon_is_property_alias(test_properties[i]);
        const char* name_type = is_alias ? "alias" : "canonical";

        if (hex != 0) {
            printf("  ✅ %s → 0x%04X (%s)\n", test_properties[i], hex, name_type);
        } else {
            printf("  ❌ %s → NOT FOUND\n", test_properties[i]);
        }
    }

    // Debug: Show hash table statistics
    printf("\n🔧 Hash Table Statistics:\n");
    printf("  Hash table size: %u\n", property_hash_table.size);
    printf("  Entries used: %u\n", property_hash_table.count);
    printf("  Load factor: %.2f%%\n", (float)property_hash_table.count / property_hash_table.size * 100);

    // Debug: Show first few hash table entries
    printf("  First 5 entries:\n");
    for (uint32_t i = 0; i < 5 && i < property_hash_table.size; i++) {
        if (property_hash_table.entries[i].name) {
            printf("    [%u] %s → 0x%04X (%s)\n", i,
                   property_hash_table.entries[i].name,
                   property_hash_table.entries[i].hex_code,
                   property_hash_table.entries[i].is_alias ? "alias" : "canonical");
        } else {
            printf("    [%u] (empty)\n", i);
        }
    }

    // Test validation
    printf("\n🔍 Element-Property Validation Test:\n");
    bool valid = kryon_is_valid_property_for_element(0x0403, 0x0601); // Input + value property
    printf("  Input (0x0403) + value property (0x0601): %s\n", valid ? "✅ VALID" : "❌ INVALID");

    valid = kryon_is_valid_property_for_element(0x0400, 0x060F); // Text + text property
    printf("  Text (0x0400) + text property (0x060F): %s\n", valid ? "✅ VALID" : "❌ INVALID");

    // Performance test
    printf("\n⚡ Performance Test:\n");
    const int iterations = 1000;
    clock_t start = clock();

    for (int i = 0; i < iterations; i++) {
        kryon_get_property_hex("width");
        kryon_get_property_hex("backgroundColor");
        kryon_get_property_hex("text");
        kryon_get_property_hex("value");
    }

    clock_t end = clock();
    double time_taken = (double)(end - start) / CLOCKS_PER_SEC;
    printf("  %d property lookups took %.6f seconds\n", iterations * 4, time_taken);
    printf("  Average time per lookup: %.2f microseconds\n", (time_taken / (iterations * 4)) * 1000000);

    printf("✅ Test completed successfully\n");
}

// Quick test function for debugging
void kryon_mappings_quick_test(void) {
    printf("🔍 KRYON MAPPINGS QUICK TEST\n");
    printf("===========================\n");

    // Test a few key properties
    const char* test_props[] = {"width", "backgroundColor", "text", "onClick"};
    for (size_t i = 0; i < sizeof(test_props)/sizeof(test_props[0]); i++) {
        uint16_t hex = kryon_get_property_hex(test_props[i]);
        bool is_alias = kryon_is_property_alias(test_props[i]);
        printf("  %s → 0x%04X (%s)\n", test_props[i], hex, is_alias ? "alias" : "canonical");
    }

    printf("✅ Quick test completed\n");
}

// Test the button example properties specifically
void kryon_mappings_test_button_example(void) {
    printf("🔍 KRYON MAPPINGS BUTTON EXAMPLE TEST\n");
    printf("====================================\n");

    // Properties used in button.kry
    const char* button_props[] = {
        "backgroundColor", "color", "borderColor", "borderWidth",
        "windowWidth", "windowHeight", "windowTitle", "windowResizable",
        "keepAspectRatio", "class", "width", "height", "text",
        "textAlign", "onClick"
    };

    printf("📊 Button Example Property Tests:\n");
    for (size_t i = 0; i < sizeof(button_props)/sizeof(button_props[0]); i++) {
        uint16_t hex = kryon_get_property_hex(button_props[i]);
        bool is_alias = kryon_is_property_alias(button_props[i]);
        const char* name_type = is_alias ? "alias" : "canonical";

        if (hex != 0) {
            printf("  ✅ %s → 0x%04X (%s)\n", button_props[i], hex, name_type);
        } else {
            printf("  ❌ %s → NOT FOUND\n", button_props[i]);
        }
    }

    printf("✅ Button example test completed\n");
}

//==============================================================================
// CATEGORY-BASED FUNCTIONS (Phase 3 Implementation)
//==============================================================================

// Helper function to check if a property range is in an element's valid properties
static bool kryon_range_in_valid_properties(uint16_t range_start, const uint16_t *valid_properties) {
    if (!valid_properties) return false;

    for (int i = 0; valid_properties[i] != 0; i++) {
        if (valid_properties[i] == range_start) {
            return true;
        }
    }
    return false;
}

// Helper function to check inheritance chain for element categories
static bool kryon_element_inherits_range(uint16_t element_hex, uint16_t range_start) {
    // Find element category
    const KryonElementCategory *element_category = NULL;
    for (int i = 0; kryon_element_categories[i].name; i++) {
        if (element_hex >= kryon_element_categories[i].range_start &&
            element_hex <= kryon_element_categories[i].range_end) {
            element_category = &kryon_element_categories[i];
            break;
        }
    }

    if (!element_category) return false;

    // Check direct valid properties
    if (kryon_range_in_valid_properties(range_start, element_category->valid_properties)) {
        return true;
    }

    // Check inherited ranges
    if (element_category->inherited_ranges) {
        for (int i = 0; element_category->inherited_ranges[i] != 0; i++) {
            uint16_t inherited_start = element_category->inherited_ranges[i];
            // Recursively check inherited categories
            // For simplicity, we'll check the base categories directly
            if (inherited_start == 0x0000) { // BaseElement
                if (range_start == 0x0000) return true;
            } else if (inherited_start == 0x0400) { // ContentElement
                if (range_start == 0x0000 || range_start == 0x0400) return true;
            }
        }
    }

    return false;
}

// Check if a property is valid for an element using hex code ranges
bool kryon_is_valid_property_for_element_category(uint16_t element_hex, uint16_t property_hex) {
    // Simplified validation using hex code ranges
    // Base properties (0x0000-0x00FF) - all elements
    if (property_hex >= 0x0000 && property_hex <= 0x00FF) return true;

    // Layout properties (0x0100-0x01FF) - layout and content elements
    if (property_hex >= 0x0100 && property_hex <= 0x01FF) {
        return (element_hex >= 0x0001 && element_hex <= 0x00FF) || // Layout elements
               (element_hex >= 0x0400 && element_hex <= 0x04FF) || // Content elements
               (element_hex >= 0x1000 && element_hex <= 0x1FFF);   // Application elements
    }

    // Visual properties (0x0200-0x02FF) - visual elements
    if (property_hex >= 0x0200 && property_hex <= 0x02FF) {
        return (element_hex >= 0x0001 && element_hex <= 0x00FF) || // Layout elements
               (element_hex >= 0x0400 && element_hex <= 0x04FF) || // Content elements
               (element_hex >= 0x1000 && element_hex <= 0x1FFF);   // Application elements
    }

    // Typography properties (0x0300-0x03FF) - text elements
    if (property_hex >= 0x0300 && property_hex <= 0x03FF) {
        return element_hex == 0x0400; // Text element
    }

    // Interactive properties (0x0500-0x05FF) - interactive elements
    if (property_hex >= 0x0500 && property_hex <= 0x05FF) {
        return (element_hex == 0x0401) || // Button
               (element_hex == 0x0403) || // Input
               (element_hex == 0x0404);   // Checkbox
    }

    // Element-specific properties (0x0600-0x06FF) - specific elements
    if (property_hex >= 0x0600 && property_hex <= 0x06FF) {
        if (property_hex == 0x060F) return element_hex == 0x0400; // text -> Text
        if (property_hex == 0x0601) return element_hex == 0x0403; // value -> Input
        return element_hex >= 0x0400 && element_hex <= 0x04FF; // Content elements
    }

    // Window properties (0x0700-0x07FF) - application elements
    if (property_hex >= 0x0700 && property_hex <= 0x07FF) {
        return element_hex >= 0x1000 && element_hex <= 0x1FFF; // Application elements
    }

    // Checkbox properties (0x0800-0x08FF) - checkbox element
    if (property_hex >= 0x0800 && property_hex <= 0x08FF) {
        return element_hex == 0x0404; // Checkbox
    }

    // Directive properties (0x8200-0x82FF) - directives
    if (property_hex >= 0x8200 && property_hex <= 0x82FF) {
        return true; // All elements can use directives
    }

    return false; // Unknown property range
}

// Get all valid properties for an element through category inheritance
size_t kryon_get_element_valid_properties(uint16_t element_hex, uint16_t *properties_out, size_t max_properties) {
    if (!properties_out || max_properties == 0) return 0;

    size_t count = 0;

    // Get all properties from valid categories in main array
    for (int i = 0; kryon_property_groups[i].canonical && count < max_properties; i++) {
        uint16_t property_hex = kryon_property_groups[i].hex_code;

        if (kryon_is_valid_property_for_element_category(element_hex, property_hex)) {
            properties_out[count++] = property_hex;
        }
    }

    // Add properties from separated arrays based on element type
    if (element_hex == 0x0404 && count < max_properties) { // Checkbox element
        for (int i = 0; kryon_checkbox_properties[i].canonical && count < max_properties; i++) {
            uint16_t property_hex = kryon_checkbox_properties[i].hex_code;
            properties_out[count++] = property_hex;
        }
    }

    return count;
}

// Get category information for a property (simplified)
const char *kryon_get_property_category_name(uint16_t property_hex) {
    if (property_hex >= 0x0000 && property_hex <= 0x00FF) return "Base";
    if (property_hex >= 0x0100 && property_hex <= 0x01FF) return "Layout";
    if (property_hex >= 0x0200 && property_hex <= 0x02FF) return "Visual";
    if (property_hex >= 0x0300 && property_hex <= 0x03FF) return "Typography";
    if (property_hex >= 0x0400 && property_hex <= 0x04FF) return "Transform";
    if (property_hex >= 0x0500 && property_hex <= 0x05FF) return "Interactive";
    if (property_hex >= 0x0600 && property_hex <= 0x06FF) return "ElementSpecific";
    if (property_hex >= 0x0700 && property_hex <= 0x07FF) return "Window";
    if (property_hex >= 0x0800 && property_hex <= 0x08FF) return "Checkbox";
    if (property_hex >= 0x8200 && property_hex <= 0x82FF) return "Directive";
    return "Unknown";
}



// Enhanced category-based validation that replaces the old hardcoded function
bool kryon_is_valid_property_for_element(uint16_t element_hex, uint16_t property_hex) {
    return kryon_is_valid_property_for_element_category(element_hex, property_hex);
}

// Debug function to test array iteration safety
void kryon_test_array_iteration(void) {
    printf("🧪 Testing kryon_property_groups array iteration safety...\n");

    int count = 0;
    for (int i = 0; kryon_property_groups[i].canonical; i++) {
        count++;
        if (count > 1000) { // Safety check to prevent infinite loops
            printf("❌ ERROR: Array iteration safety check failed - too many iterations\n");
            return;
        }
    }

    printf("✅ Array iteration safety test passed - found %d properties\n", count);

    // Test the specific properties that were causing issues
    bool found_0x010B = false;
    bool found_0x0606 = false;

    for (int i = 0; kryon_property_groups[i].canonical; i++) {
        if (kryon_property_groups[i].hex_code == 0x010B) {
            found_0x010B = true;
            printf("✅ Found property 0x010B (%s)\n", kryon_property_groups[i].canonical);
        }
        if (kryon_property_groups[i].hex_code == 0x0606) {
            found_0x0606 = true;
            printf("✅ Found property 0x0606 (%s)\n", kryon_property_groups[i].canonical);
        }
    }

    if (!found_0x010B) printf("⚠️  Property 0x010B not found\n");
    if (!found_0x0606) printf("⚠️  Property 0x0606 not found\n");
}

//==============================================================================
// CATEGORY SYSTEM TEST FUNCTIONS
//==============================================================================

void kryon_category_system_test(void) {
    printf("🗂️ KRYON CATEGORY SYSTEM TEST\n");
    printf("================================\n");

    // Test category inheritance
    printf("📊 Category Inheritance Test:\n");

    // Test Text element (0x0400) with various properties
    uint16_t test_cases[][2] = {
        {0x0400, 0x0001}, // Text + id (Base property)
        {0x0400, 0x0100}, // Text + width (Layout property)
        {0x0400, 0x0200}, // Text + background (Visual property)
        {0x0400, 0x0300}, // Text + fontSize (Typography property)
        {0x0400, 0x0500}, // Text + cursor (Interactive property)
        {0x0400, 0x060F}, // Text + text (Element-specific property)
        {0x0400, 0x0700}, // Text + windowWidth (Window property) - should fail
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        uint16_t element_hex = test_cases[i][0];
        uint16_t property_hex = test_cases[i][1];

        bool valid = kryon_is_valid_property_for_element_category(element_hex, property_hex);
        const char *element_name = kryon_get_element_name(element_hex);
        const char *property_name = kryon_get_property_name(property_hex);

        printf("  %s (0x%04X) + %s (0x%04X): %s\n",
               element_name ?: "Unknown", element_hex,
               property_name ?: "Unknown", property_hex,
               valid ? "✅ VALID" : "❌ INVALID");
    }

    // Test category information
    printf("\n🏷️ Category Information Test:\n");
    uint16_t test_elements[] = {0x0000, 0x0400, 0x0401, 0x1000};
    uint16_t test_properties[] = {0x0001, 0x0100, 0x0200, 0x060F};

    for (size_t i = 0; i < sizeof(test_elements) / sizeof(test_elements[0]); i++) {
        uint16_t element_hex = test_elements[i];
        const char *element_name = kryon_get_element_name(element_hex);
        const char* category_name = "Unknown";

        if (element_hex == 0x0000) category_name = "BaseElement";
        else if (element_hex == 0x0400) category_name = "TextElement";
        else if (element_hex == 0x0401) category_name = "InteractiveElement";
        else if (element_hex >= 0x1000) category_name = "ApplicationElement";

        printf("  Element %s (0x%04X) → Category: %s\n",
               element_name ?: "Unknown", element_hex, category_name);
    }

    for (size_t i = 0; i < sizeof(test_properties) / sizeof(test_properties[0]); i++) {
        uint16_t property_hex = test_properties[i];
        const char *prop_cat = kryon_get_property_category_name(property_hex);
        const char *property_name = kryon_get_property_name(property_hex);

        printf("  Property %s (0x%04X) → Category: %s\n",
               property_name ?: "Unknown", property_hex, prop_cat);
    }

    // Test inheritance checking (simplified)
    printf("\n🔗 Inheritance Test:\n");
    bool text_is_content = kryon_is_valid_property_for_element_category(0x0400, 0x0100); // Text + Layout
    printf("  Text (0x0400) can use Layout properties: %s\n", text_is_content ? "✅ YES" : "❌ NO");

    bool button_is_interactive = kryon_is_valid_property_for_element_category(0x0401, 0x0510); // Button + onClick
    printf("  Button (0x0401) can use Interactive properties: %s\n", button_is_interactive ? "✅ YES" : "❌ NO");

    printf("✅ Category system test completed\n");
}

// Test checkbox property validation specifically
void kryon_test_checkbox_validation(void) {
    printf("🧪 CHECKBOX VALIDATION TEST\n");
    printf("==========================\n");

    uint16_t checkbox_element = 0x0404; // Checkbox
    uint16_t checkbox_properties[] = {0x0800, 0x0801, 0x0802, 0x0803};

    printf("Testing Checkbox (0x0404) with checkbox properties:\n");
    for (size_t i = 0; i < sizeof(checkbox_properties) / sizeof(checkbox_properties[0]); i++) {
        uint16_t prop_hex = checkbox_properties[i];
        const char *prop_name = kryon_get_property_name(prop_hex);
        bool valid = kryon_is_valid_property_for_element_category(checkbox_element, prop_hex);

        printf("  %s (0x%04X): %s\n",
               prop_name ?: "Unknown", prop_hex,
               valid ? "✅ VALID" : "❌ INVALID");
    }

    // Test with non-checkbox element
    uint16_t text_element = 0x0400; // Text
    printf("\nTesting Text (0x0400) with checkbox properties:\n");
    for (size_t i = 0; i < sizeof(checkbox_properties) / sizeof(checkbox_properties[0]); i++) {
        uint16_t prop_hex = checkbox_properties[i];
        const char *prop_name = kryon_get_property_name(prop_hex);
        bool valid = kryon_is_valid_property_for_element_category(text_element, prop_hex);

        printf("  %s (0x%04X): %s\n",
               prop_name ?: "Unknown", prop_hex,
               valid ? "✅ VALID" : "❌ INVALID");
    }

    printf("✅ Checkbox validation test completed\n");
}

//==============================================================================
// CLEAN PROPERTY REGISTRATION SYSTEM 
// Simple validation based on property categories
//==============================================================================

/**
 * @brief Get the category of a property based on its hex code range
 * This function maps property hex codes to their logical categories
 */
static KryonPropertyCategoryIndex kryon_get_property_category(uint16_t property_hex) {
    // Base properties (0x0000-0x00FF) - Core properties like id, class, style
    if (property_hex >= 0x0000 && property_hex <= 0x00FF) {
        return KRYON_CATEGORY_BASE;
    }
    
    // Layout properties (0x0100-0x01FF) - Positioning, sizing, spacing
    if (property_hex >= 0x0100 && property_hex <= 0x01FF) {
        return KRYON_CATEGORY_LAYOUT;
    }
    
    // Visual properties (0x0200-0x02FF) - Colors, borders, visual appearance
    if (property_hex >= 0x0200 && property_hex <= 0x02FF) {
        return KRYON_CATEGORY_VISUAL;
    }
    
    // Typography properties (0x0300-0x03FF) - Text styling
    if (property_hex >= 0x0300 && property_hex <= 0x03FF) {
        return KRYON_CATEGORY_TYPOGRAPHY;
    }
    
    // Transform properties (0x0400-0x04FF) - Transformations
    if (property_hex >= 0x0400 && property_hex <= 0x04FF) {
        return KRYON_CATEGORY_TRANSFORM;
    }
    
    // Interactive properties (0x0500-0x05FF) - Event handling
    if (property_hex >= 0x0500 && property_hex <= 0x05FF) {
        return KRYON_CATEGORY_INTERACTIVE;
    }
    
    // Element-specific properties (0x0600-0x06FF) - Properties for specific elements
    if (property_hex >= 0x0600 && property_hex <= 0x06FF) {
        return KRYON_CATEGORY_ELEMENT_SPECIFIC;
    }
    
    // Window properties (0x0700-0x07FF) - Window management
    if (property_hex >= 0x0700 && property_hex <= 0x07FF) {
        return KRYON_CATEGORY_WINDOW;
    }
    
    // Checkbox properties (0x0800-0x08FF) - Checkbox-specific
    if (property_hex >= 0x0800 && property_hex <= 0x08FF) {
        return KRYON_CATEGORY_CHECKBOX;
    }
    
    // Directive properties (0x8200+) - For @for, @if, @while directives
    if (property_hex >= 0x8200 && property_hex <= 0x82FF) {
        return KRYON_CATEGORY_DIRECTIVE;
    }
    
    return KRYON_CATEGORY_COUNT; // Invalid/unknown category
}

/**
 * @brief Find element definition by hex code
 */
static const KryonElementGroup* kryon_find_element(uint16_t element_hex) {
    for (int i = 0; kryon_element_groups[i].canonical; i++) {
        if (kryon_element_groups[i].hex_code == element_hex) {
            return &kryon_element_groups[i];
        }
    }
    return NULL;
}

/**
 * @brief Simple property validation - check if element allows property's category
 */
bool kryon_element_allows_property(uint16_t element_hex, uint16_t property_hex) {
    // Find the element definition
    const KryonElementGroup* element = kryon_find_element(element_hex);
    if (!element) {
        // Element not found - allow all properties (for custom components)
        return true;
    }
    
    // Get the property's category
    KryonPropertyCategoryIndex property_category = kryon_get_property_category(property_hex);
    if (property_category == KRYON_CATEGORY_COUNT) {
        // Unknown property category - reject
        return false;
    }
    
    // Check if the element allows this category
    for (int i = 0; element->allowed_categories[i] != KRYON_CATEGORY_COUNT; i++) {
        if (element->allowed_categories[i] == property_category) {
            return true;
        }
    }
    
    return false; // Category not allowed for this element
}

/**
 * @brief Get list of allowed categories for an element (for tooling/debugging)
 */
const KryonPropertyCategoryIndex* kryon_get_element_allowed_categories(uint16_t element_hex) {
    const KryonElementGroup* element = kryon_find_element(element_hex);
    return element ? element->allowed_categories : NULL;
}

/**
 * @brief Test the new clean property validation system
 */
void kryon_test_clean_validation_system(void) {
    printf("🧪 CLEAN PROPERTY VALIDATION SYSTEM TEST\n");
    printf("========================================\n");
    
    // Test Text element (0x0400) - should allow base, layout, visual, typography
    uint16_t text_element = 0x0400;
    printf("\n🔤 Testing Text element (0x0400):\n");
    
    // Should be allowed
    bool allows_id = kryon_element_allows_property(text_element, 0x0001); // id (base)
    bool allows_width = kryon_element_allows_property(text_element, 0x0102); // width (layout) 
    bool allows_color = kryon_element_allows_property(text_element, 0x0201); // color (visual)
    bool allows_fontSize = kryon_element_allows_property(text_element, 0x0300); // fontSize (typography)
    
    // Should be rejected
    bool allows_onClick = kryon_element_allows_property(text_element, 0x0500); // onClick (interactive)
    bool allows_value = kryon_element_allows_property(text_element, 0x0601); // value (element-specific)
    
    printf("  ✅ id (base): %s\n", allows_id ? "ALLOWED" : "REJECTED");
    printf("  ✅ width (layout): %s\n", allows_width ? "ALLOWED" : "REJECTED");  
    printf("  ✅ color (visual): %s\n", allows_color ? "ALLOWED" : "REJECTED");
    printf("  ✅ fontSize (typography): %s\n", allows_fontSize ? "ALLOWED" : "REJECTED");
    printf("  ❌ onClick (interactive): %s\n", allows_onClick ? "ALLOWED" : "REJECTED");
    printf("  ❌ value (element-specific): %s\n", allows_value ? "ALLOWED" : "REJECTED");
    
    // Test Button element (0x0401) - should allow base, layout, visual, typography, interactive
    uint16_t button_element = 0x0401;
    printf("\n🔘 Testing Button element (0x0401):\n");
    
    bool btn_allows_onClick = kryon_element_allows_property(button_element, 0x0500); // onClick (interactive)
    bool btn_allows_value = kryon_element_allows_property(button_element, 0x0601); // value (element-specific)
    
    printf("  ✅ onClick (interactive): %s\n", btn_allows_onClick ? "ALLOWED" : "REJECTED");
    printf("  ❌ value (element-specific): %s\n", btn_allows_value ? "ALLOWED" : "REJECTED");
    
    // Test Checkbox element (0x0404) - should allow base, layout, visual, checkbox-specific
    uint16_t checkbox_element = 0x0404;
    printf("\n☑️ Testing Checkbox element (0x0404):\n");
    
    bool cb_allows_checked = kryon_element_allows_property(checkbox_element, 0x0801); // checked (checkbox)
    bool cb_allows_onClick = kryon_element_allows_property(checkbox_element, 0x0500); // onClick (interactive) 
    
    printf("  ✅ checked (checkbox): %s\n", cb_allows_checked ? "ALLOWED" : "REJECTED");
    printf("  ❌ onClick (interactive): %s\n", cb_allows_onClick ? "ALLOWED" : "REJECTED");
    
    printf("\n✅ Clean validation system test completed\n");
}

