/**
 * Kryon Toolkit Registry Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "toolkit_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ============================================================================
// Constants
// ============================================================================

#define MAX_REGISTERED_TOOLKITS 16
#define MAX_EVENTS 64

// ============================================================================
// Registry State
// ============================================================================

static struct {
    const ToolkitProfile* profiles[MAX_REGISTERED_TOOLKITS];
    int count;
    bool initialized;
} g_registry = {0};

// ============================================================================
// Built-in Toolkit Profiles
// ============================================================================

// === Tk Toolkit Profile ===

static const char* tk_supported_events[] = {
    "click", "doubleclick", "keypress", "keyrelease",
    "focus", "focusout", "enter", "leave",
    "buttonpress", "buttonrelease", "motion",
    NULL
};

static const ToolkitWidgetMapping tk_widgets[] = {
    // Layout containers
    {"Container", "frame", NULL},
    {"Column", "frame", NULL},
    {"Row", "frame", NULL},
    {"Scroll", "frame", NULL},

    // Basic widgets
    {"Button", "button", NULL},
    {"Text", "label", NULL},
    {"Input", "entry", NULL},
    {"TextArea", "text", NULL},

    // Selection widgets
    {"Checkbox", "checkbutton", NULL},
    {"Radio", "radiobutton", NULL},
    {"Select", "ttk::combobox", NULL},
    {"List", "listbox", NULL},

    // Display widgets
    {"Image", "label", NULL},
    {"Progress", "ttk::progressbar", NULL},
    {"Slider", "scale", NULL},

    // Menu widgets
    {"MenuBar", "menu", NULL},
    {"Menu", "menu", NULL},
    {"MenuItem", "command", NULL},

    // Advanced widgets
    {"Canvas", "canvas", NULL},
    {"PanedWindow", "panedwindow", NULL},
    {"Notebook", "ttk::notebook", NULL},
    {"Tree", "ttk::treeview", NULL},

    {NULL, NULL, NULL}
};

static const ToolkitPropertyMapping tk_properties[] = {
    {"text", "text", NULL},
    {"placeholder", "placeholder", NULL},
    {"width", "width", NULL},
    {"height", "height", NULL},
    {"background", "background", NULL},
    {"foreground", "foreground", NULL},
    {"font", "font", NULL},
    {"padding", "padding", NULL},
    {"margin", "padx", NULL},  // Tk uses padx/pady
    {"x", "x", NULL},
    {"y", "y", NULL},
    {"enabled", "state", NULL},  // Needs transformation: disabled -> "disabled"
    {"visible", "visible", NULL},
    {"value", "value", NULL},
    {"min", "from", NULL},  // Tk scale uses "from"
    {"max", "to", NULL},    // Tk scale uses "to"
    {NULL, NULL, NULL}
};

static const ToolkitProfile tk_profile = {
    .name = "tk",
    .type = TOOLKIT_TK,
    .widgets = tk_widgets,
    .widget_count = sizeof(tk_widgets) / sizeof(tk_widgets[0]) - 1,
    .properties = tk_properties,
    .property_count = sizeof(tk_properties) / sizeof(tk_properties[0]) - 1,
    .layout_system = LAYOUT_PACK,
    .supports_percentages = false,
    .supports_absolute_positioning = true,
    .supports_events = true,
    .supported_events = tk_supported_events,
    .event_count = sizeof(tk_supported_events) / sizeof(tk_supported_events[0]) - 1,
    .file_extension = ".tcl",
    .comment_prefix = "#",
    .string_quote = "\""
};

// === Draw Toolkit Profile ===

static const char* draw_supported_events[] = {
    "click", "mousedown", "mouseup", "mousemove",
    "keypress", "keyrelease",
    "focus", "focusout",
    NULL
};

static const ToolkitWidgetMapping draw_widgets[] = {
    // Layout containers
    {"Container", "Frame", NULL},
    {"Column", "Column", NULL},
    {"Row", "Row", NULL},
    {"Scroll", "Scroll", NULL},

    // Basic widgets (Draw uses proper capitalization!)
    {"Button", "Button", NULL},
    {"Text", "Text", NULL},
    {"Input", "Textfield", NULL},  // NOT "entry"!
    {"TextArea", "Textarea", NULL},

    // Selection widgets
    {"Checkbox", "Checkbox", NULL},
    {"Radio", "Radio", NULL},

    // Display widgets
    {"Image", "Image", NULL},
    {"Progress", "Progressbar", NULL},
    {"Slider", "Slider", NULL},

    // Advanced widgets
    {"Canvas", "Canvas", NULL},
    {"List", "List", NULL},

    {NULL, NULL, NULL}
};

static const ToolkitPropertyMapping draw_properties[] = {
    {"text", "text", NULL},
    {"placeholder", "placeholder", NULL},
    {"width", "size", NULL},  // Draw uses "size" for width
    {"height", "size", NULL}, // Draw uses "size" for height
    {"background", "bg", NULL},
    {"foreground", "fg", NULL},
    {"font", "font", NULL},
    {"padding", "padding", NULL},
    {"margin", "padding", NULL},
    {"x", "x", NULL},
    {"y", "y", NULL},
    {"enabled", "enabled", NULL},
    {"visible", "visible", NULL},
    {"value", "value", NULL},
    {"min", "min", NULL},
    {"max", "max", NULL},
    {NULL, NULL, NULL}
};

static const ToolkitProfile draw_profile = {
    .name = "draw",
    .type = TOOLKIT_DRAW,
    .widgets = draw_widgets,
    .widget_count = sizeof(draw_widgets) / sizeof(draw_widgets[0]) - 1,
    .properties = draw_properties,
    .property_count = sizeof(draw_properties) / sizeof(draw_properties[0]) - 1,
    .layout_system = LAYOUT_ABSOLUTE,
    .supports_percentages = false,
    .supports_absolute_positioning = true,
    .supports_events = true,
    .supported_events = draw_supported_events,
    .event_count = sizeof(draw_supported_events) / sizeof(draw_supported_events[0]) - 1,
    .file_extension = ".b",
    .comment_prefix = "#",
    .string_quote = "\""
};

// === DOM Toolkit Profile ===

static const char* dom_supported_events[] = {
    "click", "dblclick", "mousedown", "mouseup", "mousemove",
    "keypress", "keydown", "keyup",
    "focus", "blur",
    "submit", "change", "input",
    NULL
};

static const ToolkitWidgetMapping dom_widgets[] = {
    // Layout containers
    {"Container", "div", NULL},
    {"Column", "div", NULL},  // Uses flexbox in CSS
    {"Row", "div", NULL},     // Uses flexbox in CSS
    {"Scroll", "div", NULL},  // Uses overflow in CSS

    // Basic widgets
    {"Button", "button", NULL},
    {"Text", "span", NULL},
    {"Input", "input", NULL},
    {"TextArea", "textarea", NULL},

    // Selection widgets
    {"Checkbox", "input", NULL},  // type="checkbox"
    {"Radio", "input", NULL},     // type="radio"
    {"Select", "select", NULL},
    {"List", "ul", NULL},

    // Display widgets
    {"Image", "img", NULL},
    {"Progress", "progress", NULL},
    {"Slider", "input", NULL},    // type="range"

    // Semantic elements
    {"Heading", "h1", NULL},
    {"Paragraph", "p", NULL},
    {"Link", "a", NULL},

    {NULL, NULL, NULL}
};

static const ToolkitPropertyMapping dom_properties[] = {
    {"text", "textContent", NULL},
    {"placeholder", "placeholder", NULL},
    {"width", "style.width", NULL},
    {"height", "style.height", NULL},
    {"background", "style.backgroundColor", NULL},
    {"foreground", "style.color", NULL},
    {"font", "style.font", NULL},
    {"padding", "style.padding", NULL},
    {"margin", "style.margin", NULL},
    {"x", "style.left", NULL},
    {"y", "style.top", NULL},
    {"enabled", "disabled", NULL},  // Inverted: disabled attribute
    {"visible", "style.display", NULL},
    {"value", "value", NULL},
    {"min", "min", NULL},
    {"max", "max", NULL},
    {NULL, NULL, NULL}
};

static const ToolkitProfile dom_profile = {
    .name = "dom",
    .type = TOOLKIT_DOM,
    .widgets = dom_widgets,
    .widget_count = sizeof(dom_widgets) / sizeof(dom_widgets[0]) - 1,
    .properties = dom_properties,
    .property_count = sizeof(dom_properties) / sizeof(dom_properties[0]) - 1,
    .layout_system = LAYOUT_FLEX,
    .supports_percentages = true,
    .supports_absolute_positioning = true,
    .supports_events = true,
    .supported_events = dom_supported_events,
    .event_count = sizeof(dom_supported_events) / sizeof(dom_supported_events[0]) - 1,
    .file_extension = ".html",
    .comment_prefix = "<!--",
    .string_quote = "\""
};

// === Terminal Toolkit Profile ===

static const ToolkitWidgetMapping terminal_widgets[] = {
    {"Text", "text", NULL},
    {"Input", "prompt", NULL},
    {"Button", "menu", NULL},
    {"Select", "menu", NULL},
    {NULL, NULL, NULL}
};

static const ToolkitProfile terminal_profile = {
    .name = "terminal",
    .type = TOOLKIT_TERMINAL,
    .widgets = terminal_widgets,
    .widget_count = sizeof(terminal_widgets) / sizeof(terminal_widgets[0]) - 1,
    .properties = NULL,
    .property_count = 0,
    .layout_system = LAYOUT_NONE,
    .supports_percentages = false,
    .supports_absolute_positioning = false,
    .supports_events = false,
    .supported_events = NULL,
    .event_count = 0,
    .file_extension = ".c",
    .comment_prefix = "//",
    .string_quote = "\""
};

// ============================================================================
// Toolkit Type Conversion
// ============================================================================

ToolkitType toolkit_type_from_string(const char* type_str) {
    if (!type_str) return TOOLKIT_NONE;

    if (strcmp(type_str, "tk") == 0) return TOOLKIT_TK;
    if (strcmp(type_str, "draw") == 0) return TOOLKIT_DRAW;
    if (strcmp(type_str, "dom") == 0) return TOOLKIT_DOM;
    if (strcmp(type_str, "android_views") == 0 ||
        strcmp(type_str, "android") == 0) return TOOLKIT_ANDROID_VIEWS;
    if (strcmp(type_str, "terminal") == 0) return TOOLKIT_TERMINAL;
    if (strcmp(type_str, "sdl3") == 0) return TOOLKIT_SDL3;
    if (strcmp(type_str, "raylib") == 0) return TOOLKIT_RAYLIB;

    return TOOLKIT_NONE;
}

const char* toolkit_type_to_string(ToolkitType type) {
    switch (type) {
        case TOOLKIT_TK: return "tk";
        case TOOLKIT_DRAW: return "draw";
        case TOOLKIT_DOM: return "dom";
        case TOOLKIT_ANDROID_VIEWS: return "android_views";
        case TOOLKIT_TERMINAL: return "terminal";
        case TOOLKIT_SDL3: return "sdl3";
        case TOOLKIT_RAYLIB: return "raylib";
        case TOOLKIT_NONE: return "none";
        default: return "unknown";
    }
}

bool toolkit_type_is_valid(ToolkitType type) {
    return type >= TOOLKIT_TK && type <= TOOLKIT_RAYLIB;
}

// ============================================================================
// Registry API Implementation
// ============================================================================

bool toolkit_register(const ToolkitProfile* profile) {
    if (!profile) {
        fprintf(stderr, "Error: Cannot register NULL profile\n");
        return false;
    }

    if (g_registry.count >= MAX_REGISTERED_TOOLKITS) {
        fprintf(stderr, "Error: Toolkit registry full (max %d)\n", MAX_REGISTERED_TOOLKITS);
        return false;
    }

    // Check for duplicate
    for (int i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.profiles[i]->name, profile->name) == 0) {
            fprintf(stderr, "Warning: Toolkit '%s' already registered\n", profile->name);
            return false;
        }
    }

    g_registry.profiles[g_registry.count++] = profile;
    return true;
}

const ToolkitProfile* toolkit_get_profile(const char* name) {
    if (!name) return NULL;

    for (int i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.profiles[i]->name, name) == 0) {
            return g_registry.profiles[i];
        }
    }

    return NULL;
}

const ToolkitProfile* toolkit_get_profile_by_type(ToolkitType type) {
    for (int i = 0; i < g_registry.count; i++) {
        if (g_registry.profiles[i]->type == type) {
            return g_registry.profiles[i];
        }
    }

    return NULL;
}

bool toolkit_is_registered(const char* name) {
    return toolkit_get_profile(name) != NULL;
}

size_t toolkit_get_all_registered(const ToolkitProfile** out_profiles, size_t max_count) {
    if (!out_profiles) return 0;

    size_t count = g_registry.count;
    if (count > max_count) count = max_count;

    memcpy(out_profiles, g_registry.profiles, count * sizeof(const ToolkitProfile*));
    return count;
}

const char* toolkit_map_widget_type(const ToolkitProfile* profile, const char* kir_type) {
    if (!profile || !kir_type) return NULL;

    for (size_t i = 0; i < profile->widget_count; i++) {
        if (strcmp(profile->widgets[i].kir_type, kir_type) == 0) {
            return profile->widgets[i].toolkit_type;
        }
    }

    return NULL;
}

const char* toolkit_map_property(const ToolkitProfile* profile, const char* kir_property) {
    if (!profile || !kir_property) return NULL;

    for (size_t i = 0; i < profile->property_count; i++) {
        if (strcmp(profile->properties[i].kir_property, kir_property) == 0) {
            return profile->properties[i].toolkit_property;
        }
    }

    return NULL;
}

// ============================================================================
// Initialization
// ============================================================================

void toolkit_registry_init(void) {
    if (g_registry.initialized) {
        return;  // Already initialized
    }

    // Register built-in toolkits
    toolkit_register(&tk_profile);
    toolkit_register(&draw_profile);
    toolkit_register(&dom_profile);
    toolkit_register(&terminal_profile);

    g_registry.initialized = true;
}

void toolkit_registry_cleanup(void) {
    memset(&g_registry, 0, sizeof(g_registry));
}
