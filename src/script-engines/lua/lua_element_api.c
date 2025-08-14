/**
 * @file lua_element_api.c
 * @brief Lua bindings for the element system (Userdata-based)
 */

#include "elements.h"
#include "script_vm.h"
#include "lua_engine.h"
#include "memory.h"
#include "runtime.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <string.h>

const char* ELEMENT_METATABLE = "Kryon.Element";
const char* ELEMENT_REGISTRY = "KryonElementRegistry";

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Safely gets a element pointer from a Lua userdata.
 * 
 * Checks if the element has been invalidated (pointer is NULL).
 * If invalid, throws a Lua error.
 */
static KryonElement** check_element_userdata(lua_State* L, int index) {
    KryonElement** udata = (KryonElement**)luaL_checkudata(L, index, ELEMENT_METATABLE);
    if (!udata) {
        luaL_error(L, "Expected element object");
        return NULL;
    }
    if (*udata == NULL) {
        luaL_error(L, "Attempt to use a destroyed element");
        return NULL;
    }
    return udata;
}

// =============================================================================
// ELEMENT API (self.value, etc.)
// =============================================================================

static int l_element_get_value(lua_State* L) {
    KryonElement** element_ptr = check_element_userdata(L, 1);
    if (!element_ptr) return 0; // Error already thrown

    KryonElement* element = *element_ptr;
    
    if (element->script_state) {
        const char* value = (const char*)element->script_state;
        lua_pushstring(L, value);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int l_element_set_value(lua_State* L) {
    KryonElement** element_ptr = check_element_userdata(L, 1);
    if (!element_ptr) return 0;

    KryonElement* element = *element_ptr;

    const char* value = luaL_checkstring(L, 2);

    if (element->script_state) {
        kryon_free(element->script_state);
    }
    
    element->script_state = kryon_strdup(value);

    return 0;
}

// =============================================================================
// METAMETHODS
// =============================================================================

static int l_element_gc(lua_State* L) {
    KryonElement** udata = (KryonElement**)luaL_checkudata(L, 1, ELEMENT_METATABLE);
    if (udata && *udata) {
        // C-side element system frees the element and its script_state.
        // The script bridge invalidates the pointer.
        // GC does nothing.
    }
    return 0;
}

static const luaL_Reg element_meta_methods[] = {
    {"__gc", l_element_gc},
    {NULL, NULL}
};

static const luaL_Reg element_methods[] = {
    {"get_value", l_element_get_value},
    {"set_value", l_element_set_value},
    {NULL, NULL}
};

static int l_element_index(lua_State* L) {
    if (luaL_getmetafield(L, 1, lua_tostring(L, 2))) {
        return 1;
    }
    
    lua_getglobal(L, "KryonElementMethods");
    lua_pushvalue(L, 2); // key
    lua_rawget(L, -2);
    if (!lua_isnil(L, -1)) {
        return 1;
    }

    if (strcmp("value", lua_tostring(L, 2)) == 0) {
        return l_element_get_value(L);
    }

    lua_pushnil(L);
    return 1;
}

static int l_element_newindex(lua_State* L) {
    if (strcmp("value", lua_tostring(L, 2)) == 0) {
        return l_element_set_value(L);
    }
    
    return 0;
}

// =============================================================================
// API REGISTRATION
// =============================================================================

void kryon_lua_push_element(lua_State* L, KryonElement* element) {
    if (!element) {
        lua_pushnil(L);
        return;
    }

    KryonElement** udata = (KryonElement**)lua_newuserdata(L, sizeof(KryonElement*));
    *udata = element;

    luaL_getmetatable(L, ELEMENT_METATABLE);
    lua_setmetatable(L, -2);

    lua_getfield(L, LUA_REGISTRYINDEX, ELEMENT_REGISTRY);
    lua_pushlightuserdata(L, element);
    lua_pushvalue(L, -2); 
    lua_settable(L, -3);
    lua_pop(L, 1);
}

int kryon_lua_element_api_init(lua_State* L) {
    luaL_newmetatable(L, ELEMENT_METATABLE);
    luaL_setfuncs(L, element_meta_methods, 0);
    
    lua_pushcfunction(L, l_element_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_element_newindex);
    lua_setfield(L, -2, "__newindex");

    lua_pop(L, 1);

    lua_newtable(L);
    luaL_setfuncs(L, element_methods, 0);
    lua_setglobal(L, "KryonElementMethods");

    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, ELEMENT_REGISTRY);

    return 1;
}
