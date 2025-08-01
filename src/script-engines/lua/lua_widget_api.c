/**
 * @file lua_widget_api.c
 * @brief Lua bindings for the widget transformation system
 */

#include "kryon/widget_system.h"
#include "internal/memory.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <string.h>

// =============================================================================
// GLOBAL STATE
// =============================================================================

/// Global widget registry (would be passed in via Lua state in real implementation)
static KryonWidgetRegistry* g_widget_registry = NULL;

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/// Get widget registry from Lua state
static KryonWidgetRegistry* get_widget_registry(lua_State* L) {
    // In a real implementation, this would be stored in the Lua state
    return g_widget_registry;
}

/// Check if a value at stack index is a widget ID string
static const char* check_widget_id(lua_State* L, int index) {
    return luaL_checkstring(L, index);
}

/// Get widget from Lua stack (by ID)
static KryonWidget* get_widget_from_lua(lua_State* L, int index) {
    const char* id = check_widget_id(L, index);
    KryonWidgetRegistry* registry = get_widget_registry(L);
    
    if (!registry) {
        luaL_error(L, "Widget registry not initialized");
        return NULL;
    }
    
    KryonWidget* widget = kryon_widget_find_by_id(registry, id);
    if (!widget) {
        luaL_error(L, "Widget with ID '%s' not found", id);
        return NULL;
    }
    
    return widget;
}

// =============================================================================
// LUA API FUNCTIONS
// =============================================================================

/// kryon.get_widget(id) -> widget_id
static int lua_get_widget(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    KryonWidgetRegistry* registry = get_widget_registry(L);
    
    if (!registry) {
        lua_pushnil(L);
        lua_pushstring(L, "Widget registry not initialized");
        return 2;
    }
    
    KryonWidget* widget = kryon_widget_find_by_id(registry, id);
    if (widget) {
        lua_pushstring(L, widget->id);
        return 1;
    }
    
    lua_pushnil(L);
    return 1;
}

/// kryon.create_widget(type, properties) -> widget_id
static int lua_create_widget(lua_State* L) {
    const char* type_str = luaL_checkstring(L, 1);
    KryonWidgetRegistry* registry = get_widget_registry(L);
    
    if (!registry) {
        lua_pushnil(L);
        lua_pushstring(L, "Widget registry not initialized");
        return 2;
    }
    
    KryonWidgetType type = kryon_widget_type_from_string(type_str);
    
    // Get optional properties table
    const char* id = NULL;
    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "id");
        if (lua_isstring(L, -1)) {
            id = lua_tostring(L, -1);
        }
        lua_pop(L, 1);
    }
    
    KryonWidget* widget = kryon_widget_create(registry, type, id);
    if (!widget) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to create widget");
        return 2;
    }
    
    // Apply properties if provided
    if (lua_istable(L, 2)) {
        lua_pushnil(L);
        while (lua_next(L, 2) != 0) {
            if (lua_isstring(L, -2) && lua_isstring(L, -1)) {
                const char* key = lua_tostring(L, -2);
                const char* value = lua_tostring(L, -1);
                
                if (strcmp(key, "id") != 0) { // Skip ID, already handled
                    kryon_widget_set_property(widget, key, value);
                }
            }
            lua_pop(L, 1);
        }
    }
    
    lua_pushstring(L, widget->id);
    return 1;
}

/// kryon.transform_widget(widget_id, new_type) -> success
static int lua_transform_widget(lua_State* L) {
    KryonWidget* widget = get_widget_from_lua(L, 1);
    const char* new_type_str = luaL_checkstring(L, 2);
    
    if (!widget) {
        lua_pushboolean(L, 0);
        return 1;
    }
    
    KryonWidgetType new_type = kryon_widget_type_from_string(new_type_str);
    bool success = kryon_widget_transform_to(widget, new_type);
    
    lua_pushboolean(L, success);
    return 1;
}

/// kryon.set_property(widget_id, property, value) -> success
static int lua_set_property(lua_State* L) {
    KryonWidget* widget = get_widget_from_lua(L, 1);
    const char* property = luaL_checkstring(L, 2);
    const char* value = luaL_checkstring(L, 3);
    
    if (!widget) {
        lua_pushboolean(L, 0);
        return 1;
    }
    
    bool success = kryon_widget_set_property(widget, property, value);
    lua_pushboolean(L, success);
    return 1;
}

/// kryon.get_property(widget_id, property) -> value
static int lua_get_property(lua_State* L) {
    KryonWidget* widget = get_widget_from_lua(L, 1);
    const char* property = luaL_checkstring(L, 2);
    
    if (!widget) {
        lua_pushnil(L);
        return 1;
    }
    
    char* value = kryon_widget_get_property(widget, property);
    if (value) {
        lua_pushstring(L, value);
        kryon_free(value);
        return 1;
    }
    
    lua_pushnil(L);
    return 1;
}

/// kryon.add_child(parent_id, child_id) -> success
static int lua_add_child(lua_State* L) {
    KryonWidget* parent = get_widget_from_lua(L, 1);
    KryonWidget* child = get_widget_from_lua(L, 2);
    
    if (!parent || !child) {
        lua_pushboolean(L, 0);
        return 1;
    }
    
    bool success = kryon_widget_add_child(parent, child);
    lua_pushboolean(L, success);
    return 1;
}

/// kryon.remove_child(parent_id, child_id) -> success
static int lua_remove_child(lua_State* L) {
    KryonWidget* parent = get_widget_from_lua(L, 1);
    KryonWidget* child = get_widget_from_lua(L, 2);
    
    if (!parent || !child) {
        lua_pushboolean(L, 0);
        return 1;
    }
    
    bool success = kryon_widget_remove_child(parent, child);
    lua_pushboolean(L, success);
    return 1;
}

/// kryon.clear_children(widget_id) -> success
static int lua_clear_children(lua_State* L) {
    KryonWidget* widget = get_widget_from_lua(L, 1);
    
    if (!widget) {
        lua_pushboolean(L, 0);
        return 1;
    }
    
    kryon_widget_clear_children(widget);
    lua_pushboolean(L, 1);
    return 1;
}

/// kryon.get_child_count(widget_id) -> count
static int lua_get_child_count(lua_State* L) {
    KryonWidget* widget = get_widget_from_lua(L, 1);
    
    if (!widget) {
        lua_pushinteger(L, 0);
        return 1;
    }
    
    lua_pushinteger(L, (lua_Integer)widget->child_count);
    return 1;
}

/// kryon.get_child(widget_id, index) -> child_id
static int lua_get_child(lua_State* L) {
    KryonWidget* widget = get_widget_from_lua(L, 1);
    int index = (int)luaL_checkinteger(L, 2);
    
    if (!widget) {
        lua_pushnil(L);
        return 1;
    }
    
    // Convert from 1-based Lua indexing to 0-based C indexing
    index -= 1;
    
    KryonWidget* child = kryon_widget_get_child(widget, (size_t)index);
    if (child) {
        lua_pushstring(L, child->id);
        return 1;
    }
    
    lua_pushnil(L);
    return 1;
}

/// kryon.calculate_layout(root_id, width, height)
static int lua_calculate_layout(lua_State* L) {
    KryonWidget* root = get_widget_from_lua(L, 1);
    float width = (float)luaL_checknumber(L, 2);
    float height = (float)luaL_checknumber(L, 3);
    
    if (!root) {
        return 0;
    }
    
    KryonSize available_space = {width, height};
    kryon_widget_calculate_layout(root, available_space);
    
    return 0;
}

/// kryon.get_computed_rect(widget_id) -> {x, y, width, height}
static int lua_get_computed_rect(lua_State* L) {
    KryonWidget* widget = get_widget_from_lua(L, 1);
    
    if (!widget) {
        lua_pushnil(L);
        return 1;
    }
    
    lua_createtable(L, 0, 4);
    
    lua_pushnumber(L, widget->computed_rect.position.x);
    lua_setfield(L, -2, "x");
    
    lua_pushnumber(L, widget->computed_rect.position.y);
    lua_setfield(L, -2, "y");
    
    lua_pushnumber(L, widget->computed_rect.size.width);
    lua_setfield(L, -2, "width");
    
    lua_pushnumber(L, widget->computed_rect.size.height);
    lua_setfield(L, -2, "height");
    
    return 1;
}

// =============================================================================
// CONVENIENCE FUNCTIONS FOR COMMON PATTERNS
// =============================================================================

/// Helper for creating aligned layouts
static int lua_create_aligned_layout(lua_State* L) {
    const char* type = luaL_checkstring(L, 1);  // "column" or "row"
    const char* main_axis = luaL_optstring(L, 2, "start");
    const char* cross_axis = luaL_optstring(L, 3, "start");
    
    KryonWidgetRegistry* registry = get_widget_registry(L);
    if (!registry) {
        lua_pushnil(L);
        return 1;
    }
    
    KryonWidgetType widget_type = kryon_widget_type_from_string(type);
    KryonWidget* widget = kryon_widget_create(registry, widget_type, NULL);
    
    if (widget) {
        kryon_widget_set_property(widget, "main_axis", main_axis);
        kryon_widget_set_property(widget, "cross_axis", cross_axis);
        lua_pushstring(L, widget->id);
    } else {
        lua_pushnil(L);
    }
    
    return 1;
}

// =============================================================================
// LUA MODULE REGISTRATION
// =============================================================================

static const luaL_Reg kryon_widget_lib[] = {
    {"get_widget", lua_get_widget},
    {"create_widget", lua_create_widget},
    {"transform_widget", lua_transform_widget},
    {"set_property", lua_set_property},
    {"get_property", lua_get_property},
    {"add_child", lua_add_child},
    {"remove_child", lua_remove_child},
    {"clear_children", lua_clear_children},
    {"get_child_count", lua_get_child_count},
    {"get_child", lua_get_child},
    {"calculate_layout", lua_calculate_layout},
    {"get_computed_rect", lua_get_computed_rect},
    {"create_aligned_layout", lua_create_aligned_layout},
    {NULL, NULL}
};

/// Initialize the Kryon widget API in Lua state
int kryon_lua_widget_api_init(lua_State* L, KryonWidgetRegistry* registry) {
    // Store registry reference
    g_widget_registry = registry;
    
    // Create kryon table if it doesn't exist
    lua_getglobal(L, "kryon");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_setglobal(L, "kryon");
        lua_getglobal(L, "kryon");
    }
    
    // Register widget functions
    luaL_setfuncs(L, kryon_widget_lib, 0);
    
    // Add widget type constants
    lua_createtable(L, 0, 12);
    
    lua_pushnumber(L, KRYON_WIDGET_COLUMN);
    lua_setfield(L, -2, "COLUMN");
    
    lua_pushnumber(L, KRYON_WIDGET_ROW);
    lua_setfield(L, -2, "ROW");
    
    lua_pushnumber(L, KRYON_WIDGET_STACK);
    lua_setfield(L, -2, "STACK");
    
    lua_pushnumber(L, KRYON_WIDGET_CONTAINER);
    lua_setfield(L, -2, "CONTAINER");
    
    lua_pushnumber(L, KRYON_WIDGET_TEXT);
    lua_setfield(L, -2, "TEXT");
    
    lua_pushnumber(L, KRYON_WIDGET_BUTTON);
    lua_setfield(L, -2, "BUTTON");
    
    lua_pushnumber(L, KRYON_WIDGET_INPUT);
    lua_setfield(L, -2, "INPUT");
    
    lua_setfield(L, -2, "WidgetType");
    
    lua_pop(L, 1); // Pop kryon table
    
    return 1;
}

/// Example Lua usage script
const char* kryon_lua_widget_example = R"(
-- Example usage of the Kryon widget API

-- Create a responsive layout
local main_content = kryon.create_widget("container", {id = "main_content"})
local content_area = kryon.create_widget("column", {
    id = "content_area",
    main_axis = "start",
    cross_axis = "stretch",
    spacing = "16"
})

kryon.add_child(main_content, content_area)

-- Add some widgets
local header = kryon.create_widget("text", {text = "Responsive Content"})
local button1 = kryon.create_widget("button", {text = "Action 1"})
local button2 = kryon.create_widget("button", {text = "Action 2"})

kryon.add_child(content_area, header)
kryon.add_child(content_area, button1)
kryon.add_child(content_area, button2)

-- Responsive layout function
function on_resize(width, height)
    if width < 600 then
        -- Mobile: vertical layout
        kryon.transform_widget(content_area, "column")
        kryon.set_property(content_area, "main_axis", "start")
        kryon.set_property(content_area, "cross_axis", "stretch")
    else
        -- Desktop: horizontal layout
        kryon.transform_widget(content_area, "row")
        kryon.set_property(content_area, "main_axis", "space_around")
        kryon.set_property(content_area, "cross_axis", "center")
    end
    
    -- Recalculate layout
    kryon.calculate_layout(main_content, width, height)
end

-- Interactive menu example
local menu_expanded = false
local menu_options = {
    {text = "Home", action = "go_home"},
    {text = "About", action = "show_about"},
    {text = "Contact", action = "show_contact"}
}

function toggle_menu()
    local menu_button = kryon.get_widget("menu_button")
    menu_expanded = not menu_expanded
    
    if menu_expanded then
        -- Transform to column menu
        kryon.transform_widget(menu_button, "column")
        kryon.set_property(menu_button, "main_axis", "start")
        kryon.set_property(menu_button, "cross_axis", "stretch")
        kryon.set_property(menu_button, "spacing", "2")
        
        -- Add menu options
        for i, option in ipairs(menu_options) do
            local option_button = kryon.create_widget("button", {
                text = option.text,
                background_color = "#f8f9fa",
                text_color = "#333"
            })
            kryon.add_child(menu_button, option_button)
        end
    else
        -- Transform back to single button
        kryon.clear_children(menu_button)
        kryon.transform_widget(menu_button, "button")
        kryon.set_property(menu_button, "text", "☰ Menu")
    end
end
)";