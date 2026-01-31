/**
 * Tcl Widget Registry
 * Maps between Tcl/Tk widget types and KIR widget types
 */

#define _POSIX_C_SOURCE 200809L

#include "tcl_widget_registry.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* ============================================================================
 * Widget Type Mappings
 * ============================================================================ */

/**
 * Mapping table: Tcl widget type → KIR widget type
 */
static const TclWidgetMapping widget_type_map[] = {
    // Basic widgets
    {"button", "Button"},
    {"label", "Text"},
    {"entry", "Input"},
    {"text", "TextArea"},
    {"message", "Text"},  // Multi-line text

    // Containers
    {"frame", "Container"},
    {"labelframe", "Container"},
    {"toplevel", "Container"},
    {"panedwindow", "Container"},

    // Selection widgets
    {"checkbutton", "Checkbox"},
    {"radiobutton", "Radio"},
    {"listbox", "List"},
    {"combobox", "Dropdown"},  // Tk 8.5+
    {"spinbox", "NumberInput"},

    // Canvas and drawing
    {"canvas", "Canvas"},

    // Menus
    {"menubutton", "MenuButton"},
    {"menu", "Menu"},

    // Progress and scale
    {"scale", "Slider"},
    {"progress", "ProgressBar"},  // Usually custom widget

    // Scrolling
    {"scrollbar", "Scrollbar"},

    // Notebook (tabs)
    {"notebook", "TabView"},  // ttk::notebook

    // Tree view
    {"treeview", "TreeView"},  // ttk::treeview

    // Separator
    {"separator", "Separator"},  // ttk::separator

    // Size grip
    {"sizegrip", "SizeGrip"},  // ttk::sizegrip

    // Label frames (often treated as containers)
    {"labelframe", "Container"},

    NULL_TERMINATOR
};

/**
 * Mapping table: KIR widget type → Tcl widget type (reverse mapping)
 */
static const TclWidgetMapping kir_widget_type_map[] = {
    // Basic widgets
    {"Button", "button"},
    {"Text", "label"},
    {"Input", "entry"},
    {"TextArea", "text"},
    {"Checkbox", "checkbutton"},
    {"Radio", "radiobutton"},
    {"List", "listbox"},
    {"Dropdown", "ttk::combobox"},
    {"NumberInput", "spinbox"},

    // Containers
    {"Container", "frame"},
    {"Row", "frame"},
    {"Column", "frame"},
    {"Center", "frame"},
    {"Scroll", "frame"},

    // Canvas
    {"Canvas", "canvas"},

    // Menus
    {"Menu", "menu"},
    {"MenuBar", "menubutton"},

    // Sliders and progress
    {"Slider", "scale"},
    {"ProgressBar", "ttk::progress"},

    // Tabs
    {"TabView", "ttk::notebook"},

    // Trees
    {"TreeView", "ttk::treeview"},

    // Separator
    {"Separator", "ttk::separator"},

    NULL_TERMINATOR
};

/* ============================================================================
 * Property Mappings
 * ============================================================================ */

/**
 * Mapping table: Tk widget options → KIR properties
 */
static const TclPropertyMapping property_map[] = {
    // Text content
    {"-text", "text"},
    {"-label", "text"},  // For some widgets

    // Colors
    {"-background", "background"},
    {"-bg", "background"},
    {"-foreground", "color"},
    {"-fg", "color"},
    {"-activebackground", "activeBackground"},
    {"-activeforeground", "activeColor"},

    // Fonts
    {"-font", "font"},

    // Dimensions
    {"-width", "width"},
    {"-height", "height"},
    {"-padx", "paddingX"},
    {"-pady", "paddingY"},

    // Borders
    {"-borderwidth", "borderWidth"},
    {"-bd", "borderWidth"},
    {"-relief", "borderStyle"},

    // State
    {"-state", "state"},
    {"-disabledforeground", "disabledColor"},

    // Alignment
    {"-anchor", "alignment"},

    // Images
    {"-image", "image"},

    // Special widget properties
    {"-variable", "variable"},  // For checkbutton, radiobutton
    {"-value", "value"},
    {"-from", "min"},  // For scale
    {"-to", "max"},    // For scale
    {"-orient", "orientation"},  // For scale (horizontal/vertical)
    {"-show", "show"},  // For entry (e.g., '*' for password)

    NULL_TERMINATOR
};

/**
 * Mapping table: KIR properties → Tk widget options (reverse mapping)
 */
static const TclPropertyMapping kir_property_map[] = {
    // Text content
    {"text", "-text"},

    // Colors
    {"background", "-background"},
    {"backgroundColor", "-background"},
    {"color", "-foreground"},
    {"textColor", "-foreground"},
    {"foregroundColor", "-foreground"},
    {"activeBackground", "-activebackground"},
    {"activeColor", "-activeforeground"},

    // Fonts
    {"font", "-font"},

    // Dimensions
    {"width", "-width"},
    {"height", "-height"},
    {"paddingX", "-padx"},
    {"paddingY", "-pady"},

    // Borders
    {"borderWidth", "-borderwidth"},
    {"borderStyle", "-relief"},

    // State
    {"state", "-state"},
    {"disabledColor", "-disabledforeground"},

    // Alignment
    {"alignment", "-anchor"},

    // Images
    {"image", "-image"},

    NULL_TERMINATOR
};

/* ============================================================================
 * Public API: Widget Type Mapping
 * ============================================================================ */

const char* tcl_widget_to_kir_type(const char* tcl_widget_type) {
    if (!tcl_widget_type) return "Container";  // Default fallback

    // Handle ttk:: prefix
    const char* widget_type = tcl_widget_type;
    if (strncmp(widget_type, "ttk::", 5) == 0) {
        widget_type += 5;
    }

    for (int i = 0; widget_type_map[i].tcl_widget != NULL; i++) {
        if (strcmp(widget_type, widget_type_map[i].tcl_widget) == 0) {
            return widget_type_map[i].kir_type;
        }
    }

    // Unknown widget type - treat as container
    return "Container";
}

const char* kir_widget_to_tcl_type(const char* kir_widget_type) {
    if (!kir_widget_type) return "frame";  // Default fallback

    for (int i = 0; kir_widget_type_map[i].tcl_widget != NULL; i++) {
        if (strcmp(kir_widget_type, kir_widget_type_map[i].kir_type) == 0) {
            return kir_widget_type_map[i].tcl_widget;
        }
    }

    // Unknown KIR type - default to frame
    return "frame";
}

bool tcl_is_valid_widget_type(const char* tcl_widget_type) {
    if (!tcl_widget_type) return false;

    // Handle ttk:: prefix
    const char* widget_type = tcl_widget_type;
    if (strncmp(widget_type, "ttk::", 5) == 0) {
        widget_type += 5;
    }

    for (int i = 0; widget_type_map[i].tcl_widget != NULL; i++) {
        if (strcmp(widget_type, widget_type_map[i].tcl_widget) == 0) {
            return true;
        }
    }

    return false;
}

/* ============================================================================
 * Public API: Property Mapping
 * ============================================================================ */

const char* tcl_option_to_kir_property(const char* tcl_option) {
    if (!tcl_option) return NULL;

    for (int i = 0; property_map[i].tcl_option != NULL; i++) {
        if (strcmp(tcl_option, property_map[i].tcl_option) == 0) {
            return property_map[i].kir_property;
        }
    }

    return NULL;  // Unknown option
}

const char* kir_property_to_tcl_option(const char* kir_property) {
    if (!kir_property) return NULL;

    for (int i = 0; kir_property_map[i].tcl_option != NULL; i++) {
        if (strcmp(kir_property, kir_property_map[i].kir_property) == 0) {
            return kir_property_map[i].tcl_option;
        }
    }

    return NULL;  // Unknown property
}

bool tcl_is_valid_option(const char* tcl_option) {
    if (!tcl_option) return false;

    for (int i = 0; property_map[i].tcl_option != NULL; i++) {
        if (strcmp(tcl_option, property_map[i].tcl_option) == 0) {
            return true;
        }
    }

    return false;
}

/* ============================================================================
 * Public API: Layout Managers
 * ============================================================================ */

bool tcl_is_layout_command(const char* command) {
    if (!command) return false;

    return (strcmp(command, "pack") == 0 ||
            strcmp(command, "grid") == 0 ||
            strcmp(command, "place") == 0);
}

const char* tcl_layout_to_kir_layout(const char* tcl_layout) {
    if (!tcl_layout) return NULL;

    if (strcmp(tcl_layout, "pack") == 0) {
        return "flex";  // pack is similar to flexbox
    } else if (strcmp(tcl_layout, "grid") == 0) {
        return "grid";  // direct mapping
    } else if (strcmp(tcl_layout, "place") == 0) {
        return "absolute";  // place is absolute positioning
    }

    return NULL;
}

/* ============================================================================
 * Public API: Widget Capabilities
 * ============================================================================ */

bool tcl_widget_supports_option(const char* widget_type, const char* option) {
    if (!widget_type || !option) return false;

    // Handle ttk:: prefix
    if (strncmp(widget_type, "ttk::", 5) == 0) {
        widget_type += 5;
    }

    // Frame and canvas don't support -foreground
    if (strcmp(option, "-foreground") == 0 || strcmp(option, "-fg") == 0) {
        return (strcmp(widget_type, "frame") != 0 &&
                strcmp(widget_type, "canvas") != 0 &&
                strcmp(widget_type, "toplevel") != 0);
    }

    // Most widgets support most options
    return true;
}

/* ============================================================================
 * Debug/Utilities
 * ============================================================================ */

void tcl_widget_registry_dump_mappings(void) {
    printf("Tcl → KIR Widget Type Mappings:\n");
    for (int i = 0; widget_type_map[i].tcl_widget != NULL; i++) {
        printf("  %-20s → %s\n", widget_type_map[i].tcl_widget, widget_type_map[i].kir_type);
    }

    printf("\nTcl Options → KIR Properties:\n");
    for (int i = 0; property_map[i].tcl_option != NULL; i++) {
        printf("  %-20s → %s\n", property_map[i].tcl_option, property_map[i].kir_property);
    }
}
