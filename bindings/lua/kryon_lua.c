/**
 * Kryon Lua Bindings
 *
 * Provides Lua interface to the Kryon C core UI framework.
 * This file implements the Lua module that bridges Lua applications
 * with the Kryon C API for creating cross-platform UI applications.
 */

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include Kryon C API
#include "../../core/include/kryon.h"
#include "../../core/include/kryon_markdown.h"

// ============================================================================
// Lua Helper Structures and Functions
// ============================================================================

// User data wrapper for Kryon components
typedef struct {
    kryon_component_t* component;
    bool is_owner;
} lua_kryon_component_t;

// User data wrapper for Kryon renderer
typedef struct {
    kryon_renderer_t* renderer;
    bool is_owner;
} lua_kryon_renderer_t;

// Application state
typedef struct {
    kryon_renderer_t* renderer;
    kryon_component_t* root_component;
    bool is_running;
} lua_kryon_app_t;

// ============================================================================
// Utility Functions
// ============================================================================

static int kryon_error_to_lua(kryon_error_t err) {
    switch (err) {
        case KRYON_OK: return 0;
        case KRYON_ERROR_NULL_PTR: return luaL_error(L, "Null pointer error");
        case KRYON_ERROR_INVALID_PARAM: return luaL_error(L, "Invalid parameter");
        case KRYON_ERROR_OUT_OF_MEMORY: return luaL_error(L, "Out of memory");
        case KRYON_ERROR_BUFFER_FULL: return luaL_error(L, "Buffer full");
        case KRYON_ERROR_INVALID_COMPONENT: return luaL_error(L, "Invalid component");
        case KRYON_ERROR_RENDERER_INIT: return luaL_error(L, "Renderer initialization failed");
        case KRYON_ERROR_COMMAND_OVERFLOW: return luaL_error(L, "Command overflow");
        default: return luaL_error(L, "Unknown error");
    }
}

// ============================================================================
// Component Management Functions
// ============================================================================

static int lua_kryon_container_create(lua_State *L) {
    // Create a container component
    kryon_component_t* component = kryon_component_create(&kryon_container_ops, NULL);
    if (!component) {
        return luaL_error(L, "Failed to create container component");
    }

    // Create user data wrapper
    lua_kryon_component_t* lua_comp = (lua_kryon_component_t*)lua_newuserdata(L, sizeof(lua_kryon_component_t));
    lua_comp->component = component;
    lua_comp->is_owner = true;

    // Set metatable
    luaL_getmetatable(L, "KryonComponent");
    lua_setmetatable(L, -2);

    return 1;
}

static int lua_kryon_text_create(lua_State *L) {
    const char* text = luaL_checkstring(L, 1);
    uint16_t font_id = luaL_optinteger(L, 2, 0);

    // Create text state
    kryon_text_state_t* text_state = malloc(sizeof(kryon_text_state_t));
    text_state->text = strdup(text);
    text_state->font_id = font_id;
    text_state->max_length = 256;
    text_state->word_wrap = false;

    // Create text component
    kryon_component_t* component = kryon_component_create(&kryon_text_ops, text_state);
    if (!component) {
        free(text_state->text);
        free(text_state);
        return luaL_error(L, "Failed to create text component");
    }

    // Create user data wrapper
    lua_kryon_component_t* lua_comp = (lua_kryon_component_t*)lua_newuserdata(L, sizeof(lua_kryon_component_t));
    lua_comp->component = component;
    lua_comp->is_owner = true;

    // Set metatable
    luaL_getmetatable(L, "KryonComponent");
    lua_setmetatable(L, -2);

    return 1;
}

static int lua_kryon_button_create(lua_State *L) {
    const char* text = luaL_checkstring(L, 1);
    uint16_t font_id = luaL_optinteger(L, 2, 0);

    // Create button state
    kryon_button_state_t* button_state = malloc(sizeof(kryon_button_state_t));
    button_state->text = strdup(text);
    button_state->font_id = font_id;
    button_state->pressed = false;
    button_state->on_click = NULL;  // Will be set later

    // Create button component
    kryon_component_t* component = kryon_component_create(&kryon_button_ops, button_state);
    if (!component) {
        free(button_state->text);
        free(button_state);
        return luaL_error(L, "Failed to create button component");
    }

    // Create user data wrapper
    lua_kryon_component_t* lua_comp = (lua_kryon_component_t*)lua_newuserdata(L, sizeof(lua_kryon_component_t));
    lua_comp->component = component;
    lua_comp->is_owner = true;

    // Set metatable
    luaL_getmetatable(L, "KryonComponent");
    lua_setmetatable(L, -2);

    return 1;
}

static int lua_kryon_input_create(lua_State *L) {
    const char* placeholder = luaL_checkstring(L, 1);
    const char* initial_text = luaL_optstring(L, 2, "");
    uint16_t font_id = luaL_optinteger(L, 3, 0);
    bool password_mode = lua_toboolean(L, 4);

    // Create input component using factory function
    kryon_component_t* component = kryon_input_create(placeholder, initial_text, font_id, password_mode);
    if (!component) {
        return luaL_error(L, "Failed to create input component");
    }

    // Create user data wrapper
    lua_kryon_component_t* lua_comp = (lua_kryon_component_t*)lua_newuserdata(L, sizeof(lua_kryon_component_t));
    lua_comp->component = component;
    lua_comp->is_owner = true;

    // Set metatable
    luaL_getmetatable(L, "KryonComponent");
    lua_setmetatable(L, -2);

    return 1;
}

static int lua_kryon_markdown_create(lua_State *L) {
    const char* source = luaL_checkstring(L, 1);
    uint16_t width = luaL_optinteger(L, 2, 400);
    uint16_t height = luaL_optinteger(L, 3, 300);

    // Create markdown state
    kryon_markdown_state_t* markdown_state = malloc(sizeof(kryon_markdown_state_t));
    markdown_state->source = strdup(source);
    markdown_state->theme = NULL;  // Use default theme

    // Create markdown component
    kryon_component_t* component = kryon_component_create(&kryon_markdown_ops, markdown_state);
    if (!component) {
        free(markdown_state->source);
        free(markdown_state);
        return luaL_error(L, "Failed to create markdown component");
    }

    // Create user data wrapper
    lua_kryon_component_t* lua_comp = (lua_kryon_component_t*)lua_newuserdata(L, sizeof(lua_kryon_component_t));
    lua_comp->component = component;
    lua_comp->is_owner = true;

    // Set metatable
    luaL_getmetatable(L, "KryonComponent");
    lua_setmetatable(L, -2);

    return 1;
}

// ============================================================================
// Component Methods
// ============================================================================

static int lua_kryon_component_add_child(lua_State *L) {
    lua_kryon_component_t* parent = (lua_kryon_component_t*)luaL_checkudata(L, 1, "KryonComponent");
    lua_kryon_component_t* child = (lua_kryon_component_t*)luaL_checkudata(L, 2, "KryonComponent");

    if (!kryon_component_add_child(parent->component, child->component)) {
        return luaL_error(L, "Failed to add child component");
    }

    return 0;
}

static int lua_kryon_component_set_bounds(lua_State *L) {
    lua_kryon_component_t* comp = (lua_kryon_component_t*)luaL_checkudata(L, 1, "KryonComponent");
    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);
    float width = luaL_checknumber(L, 4);
    float height = luaL_checknumber(L, 5);

    kryon_component_set_bounds(comp->component,
                              kryon_fp_from_float(x),
                              kryon_fp_from_float(y),
                              kryon_fp_from_float(width),
                              kryon_fp_from_float(height));

    return 0;
}

static int lua_kryon_component_set_background_color(lua_State *L) {
    lua_kryon_component_t* comp = (lua_kryon_component_t*)luaL_checkudata(L, 1, "KryonComponent");
    uint32_t color = luaL_checkinteger(L, 2);

    kryon_component_set_background_color(comp->component, color);

    return 0;
}

static int lua_kryon_component_set_visible(lua_State *L) {
    lua_kryon_component_t* comp = (lua_kryon_component_t*)luaL_checkudata(L, 1, "KryonComponent");
    bool visible = lua_toboolean(L, 2);

    kryon_component_set_visible(comp->component, visible);

    return 0;
}

// ============================================================================
// Utility Functions
// ============================================================================

static int lua_kryon_color_rgba(lua_State *L) {
    uint8_t r = luaL_checkinteger(L, 1);
    uint8_t g = luaL_checkinteger(L, 2);
    uint8_t b = luaL_checkinteger(L, 3);
    uint8_t a = luaL_optinteger(L, 4, 255);

    uint32_t color = kryon_color_rgba(r, g, b, a);
    lua_pushinteger(L, color);

    return 1;
}

static int lua_kryon_color_rgb(lua_State *L) {
    uint8_t r = luaL_checkinteger(L, 1);
    uint8_t g = luaL_checkinteger(L, 2);
    uint8_t b = luaL_checkinteger(L, 3);

    uint32_t color = kryon_color_rgb(r, g, b);
    lua_pushinteger(L, color);

    return 1;
}

// ============================================================================
// Application Functions
// ============================================================================

static int lua_kryon_app_create(lua_State *L) {
    // Check if we have a configuration table
    if (lua_istable(L, 1)) {
        // Get window properties from table
        lua_getfield(L, 1, "width");
        uint16_t width = luaL_optinteger(L, -1, 800);
        lua_pop(L, 1);

        lua_getfield(L, 1, "height");
        uint16_t height = luaL_optinteger(L, -1, 600);
        lua_pop(L, 1);

        lua_getfield(L, 1, "title");
        const char* title = luaL_optstring(L, -1, "Kryon App");
        lua_pop(L, 1);

        // Create application state
        lua_kryon_app_t* app = (lua_kryon_app_t*)lua_newuserdata(L, sizeof(lua_kryon_app_t));
        app->renderer = NULL;  // Will be created by backend
        app->root_component = NULL;
        app->is_running = false;

        // Set metatable
        luaL_getmetatable(L, "KryonApp");
        lua_setmetatable(L, -2);
    } else {
        return luaL_error(L, "Expected configuration table for app creation");
    }

    return 1;
}

// ============================================================================
// Module Initialization
// ============================================================================

static const struct luaL_Reg kryon_lib[] = {
    // Component creation functions
    {"container", lua_kryon_container_create},
    {"text", lua_kryon_text_create},
    {"button", lua_kryon_button_create},
    {"input", lua_kryon_input_create},
    {"markdown", lua_kryon_markdown_create},

    // Utility functions
    {"color_rgba", lua_kryon_color_rgba},
    {"color_rgb", lua_kryon_color_rgb},

    // Application functions
    {"app", lua_kryon_app_create},

    {NULL, NULL}
};

static const struct luaL_Reg kryon_component_methods[] = {
    {"add_child", lua_kryon_component_add_child},
    {"set_bounds", lua_kryon_component_set_bounds},
    {"set_background_color", lua_kryon_component_set_background_color},
    {"set_visible", lua_kryon_component_set_visible},

    // Add more methods as needed
    {NULL, NULL}
};

static const struct luaL_Reg kryon_app_methods[] = {
    // Add app methods as needed
    {NULL, NULL}
};

// Metamethods
static int lua_kryon_component_gc(lua_State *L) {
    lua_kryon_component_t* comp = (lua_kryon_component_t*)luaL_checkudata(L, 1, "KryonComponent");
    if (comp && comp->is_owner && comp->component) {
        kryon_component_destroy(comp->component);
        comp->component = NULL;
    }
    return 0;
}

static const struct luaL_Reg kryon_component_metamethods[] = {
    {"__gc", lua_kryon_component_gc},
    {NULL, NULL}
};

// ============================================================================
// Lua Module Entry Point
// ============================================================================

int luaopen_kryon(lua_State *L) {
    // Create metatable for components
    luaL_newmetatable(L, "KryonComponent");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, kryon_component_methods, 0);
    luaL_setfuncs(L, kryon_component_metamethods, 0);

    // Create metatable for app
    luaL_newmetatable(L, "KryonApp");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, kryon_app_methods, 0);

    // Create library
    luaL_newlib(L, kryon_lib);

    // Add constants
    lua_pushinteger(L, KRYON_VERSION_MAJOR);
    lua_setfield(L, -2, "VERSION_MAJOR");

    lua_pushinteger(L, KRYON_VERSION_MINOR);
    lua_setfield(L, -2, "VERSION_MINOR");

    lua_pushinteger(L, KRYON_VERSION_PATCH);
    lua_setfield(L, -2, "VERSION_PATCH");

    lua_pushstring(L, KRYON_VERSION_STRING);
    lua_setfield(L, -2, "VERSION_STRING");

    return 1;
}

// ============================================================================
// Entry Point for Standalone Execution
// ============================================================================

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <script.lua> [args...]\n", argv[0]);
        return 1;
    }

    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "Failed to create Lua state\n");
        return 1;
    }

    // Open standard libraries
    luaL_openlibs(L);

    // Load our module
    luaL_requiref(L, "kryon", luaopen_kryon, 1);
    lua_pop(L, 1);

    // Load and execute the script
    if (luaL_dofile(L, argv[1])) {
        fprintf(stderr, "Error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}