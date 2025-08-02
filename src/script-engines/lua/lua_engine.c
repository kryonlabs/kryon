/**
 * @file lua_engine.c
 * @brief Kryon Lua Script Engine Implementation
 */

#include "internal/lua_engine.h"
#include "internal/memory.h"
#include "internal/runtime.h"
#include "internal/events.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// LUA ENGINE STRUCTURE
// =============================================================================

struct KryonLuaEngine {
    lua_State* L;                   // Lua state
    KryonLuaConfig config;          // Engine configuration
    bool initialized;               // Initialization flag
    size_t memory_used;             // Current memory usage
    double start_time;              // Execution start time
};

// =============================================================================
// MEMORY ALLOCATOR
// =============================================================================

static void* lua_allocator(void* ud, void* ptr, size_t osize, size_t nsize) {
    KryonLuaEngine* engine = (KryonLuaEngine*)ud;
    
    if (nsize == 0) {
        // Free
        if (ptr) {
            engine->memory_used -= osize;
            kryon_free(ptr);
        }
        return NULL;
    } else {
        // Check memory limit
        if (engine->config.memory_limit > 0) {
            size_t new_usage = engine->memory_used - osize + nsize;
            if (new_usage > engine->config.memory_limit) {
                return NULL; // Memory limit exceeded
            }
        }
        
        // Allocate or reallocate
        void* new_ptr = ptr ? kryon_realloc(ptr, nsize) : kryon_alloc(nsize);
        if (new_ptr) {
            engine->memory_used = engine->memory_used - osize + nsize;
        }
        return new_ptr;
    }
}

// =============================================================================
// KRYON API BINDINGS
// =============================================================================

static int lua_kryon_log(lua_State* L) {
    const char* message = luaL_checkstring(L, 1);
    printf("🐛 Lua: %s\n", message);
    return 0;
}

static int lua_kryon_element_get_text(lua_State* L) {
    // This would get the element from the Lua registry and return its text
    // For now, return a placeholder
    lua_pushstring(L, "Click Me!");
    return 1;
}

static int lua_kryon_element_set_text(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    printf("🐛 Lua: Setting element text to '%s'\n", text);
    return 0;
}

// =============================================================================
// API FUNCTIONS
// =============================================================================

KryonLuaEngine* kryon_lua_engine_create(const KryonLuaConfig* config) {
    KryonLuaEngine* engine = kryon_alloc(sizeof(KryonLuaEngine));
    if (!engine) {
        return NULL;
    }
    
    // Set configuration
    if (config) {
        engine->config = *config;
    } else {
        engine->config = kryon_lua_default_config();
    }
    
    // Create Lua state with custom allocator
    engine->L = lua_newstate(lua_allocator, engine);
    if (!engine->L) {
        kryon_free(engine);
        return NULL;
    }
    
    // Open standard libraries
    luaL_openlibs(engine->L);
    
    // Register Kryon API
    kryon_lua_register_api(engine);
    
    engine->initialized = true;
    engine->memory_used = 0;
    engine->start_time = 0;
    
    return engine;
}

void kryon_lua_engine_destroy(KryonLuaEngine* engine) {
    if (!engine) return;
    
    if (engine->L) {
        lua_close(engine->L);
    }
    
    kryon_free(engine);
}

KryonLuaResult kryon_lua_compile(KryonLuaEngine* engine, 
                                const char* source,
                                const char* source_name,
                                KryonLuaBytecode* out_bytecode) {
    if (!engine || !source || !out_bytecode) {
        return KRYON_LUA_ERROR_INVALID_PARAM;
    }
    
    // Load and compile the source
    int result = luaL_loadbuffer(engine->L, source, strlen(source), source_name);
    if (result != LUA_OK) {
        const char* error = lua_tostring(engine->L, -1);
        printf("❌ Lua compilation error: %s\n", error);
        lua_pop(engine->L, 1);
        return KRYON_LUA_ERROR_COMPILE;
    }
    
    // Dump bytecode
    size_t bytecode_size = 0;
    char* buffer = NULL;
    
    // Use lua_dump to get bytecode
    typedef struct {
        char** buffer;
        size_t* size;
        size_t capacity;
    } DumpData;
    
    DumpData dump_data = {&buffer, &bytecode_size, 0};
    
    // Writer function for lua_dump
    int writer(lua_State* L, const void* p, size_t sz, void* ud) {
        (void)L; // Unused
        DumpData* data = (DumpData*)ud;
        
        // Resize buffer if needed
        if (*data->size + sz > data->capacity) {
            data->capacity = (*data->size + sz) * 2;
            *data->buffer = kryon_realloc(*data->buffer, data->capacity);
            if (!*data->buffer) {
                return 1; // Error
            }
        }
        
        // Copy data
        memcpy(*data->buffer + *data->size, p, sz);
        *data->size += sz;
        return 0;
    }
    
    result = lua_dump(engine->L, writer, &dump_data, 0);
    lua_pop(engine->L, 1); // Remove function from stack
    
    if (result != 0) {
        if (buffer) kryon_free(buffer);
        return KRYON_LUA_ERROR_COMPILE;
    }
    
    // Fill output structure
    out_bytecode->bytecode = (uint8_t*)buffer;
    out_bytecode->size = bytecode_size;
    out_bytecode->source_name = strdup(source_name ? source_name : "lua_script");
    
    return KRYON_LUA_SUCCESS;
}

KryonLuaResult kryon_lua_execute(KryonLuaEngine* engine,
                                const KryonLuaBytecode* bytecode) {
    if (!engine || !bytecode || !bytecode->bytecode) {
        return KRYON_LUA_ERROR_INVALID_PARAM;
    }
    
    // Load bytecode
    int result = luaL_loadbuffer(engine->L, (const char*)bytecode->bytecode, 
                                bytecode->size, bytecode->source_name);
    if (result != LUA_OK) {
        const char* error = lua_tostring(engine->L, -1);
        printf("❌ Lua load error: %s\n", error);
        lua_pop(engine->L, 1);
        return KRYON_LUA_ERROR_RUNTIME;
    }
    
    // Execute
    result = lua_pcall(engine->L, 0, 0, 0);
    if (result != LUA_OK) {
        const char* error = lua_tostring(engine->L, -1);
        printf("❌ Lua execution error: %s\n", error);
        lua_pop(engine->L, 1);
        return KRYON_LUA_ERROR_RUNTIME;
    }
    
    return KRYON_LUA_SUCCESS;
}

KryonLuaResult kryon_lua_call_function(KryonLuaEngine* engine,
                                      const char* function_name,
                                      KryonElement* element,
                                      const KryonEvent* event) {
    if (!engine || !function_name) {
        return KRYON_LUA_ERROR_INVALID_PARAM;
    }
    
    // Get function from global table
    lua_getglobal(engine->L, function_name);
    if (!lua_isfunction(engine->L, -1)) {
        lua_pop(engine->L, 1);
        printf("❌ Lua function '%s' not found\n", function_name);
        return KRYON_LUA_ERROR_RUNTIME;
    }
    
    // TODO: Push element and event as arguments
    // For now, just call with no arguments
    
    int result = lua_pcall(engine->L, 0, 0, 0);
    if (result != LUA_OK) {
        const char* error = lua_tostring(engine->L, -1);
        printf("❌ Lua function call error: %s\n", error);
        lua_pop(engine->L, 1);
        return KRYON_LUA_ERROR_RUNTIME;
    }
    
    printf("✅ Lua function '%s' executed successfully\n", function_name);
    return KRYON_LUA_SUCCESS;
}

void kryon_lua_bytecode_free(KryonLuaBytecode* bytecode) {
    if (!bytecode) return;
    
    if (bytecode->bytecode) {
        kryon_free(bytecode->bytecode);
        bytecode->bytecode = NULL;
    }
    
    if (bytecode->source_name) {
        free(bytecode->source_name);
        bytecode->source_name = NULL;
    }
    
    bytecode->size = 0;
}

KryonLuaConfig kryon_lua_default_config(void) {
    KryonLuaConfig config = {
        .enable_debug = false,
        .memory_limit = 1024 * 1024,  // 1MB limit
        .time_limit = 5.0             // 5 second limit
    };
    return config;
}

KryonLuaResult kryon_lua_register_api(KryonLuaEngine* engine) {
    if (!engine || !engine->L) {
        return KRYON_LUA_ERROR_INVALID_PARAM;
    }
    
    // Create kryon table
    lua_newtable(engine->L);
    
    // Register functions
    lua_pushcfunction(engine->L, lua_kryon_log);
    lua_setfield(engine->L, -2, "log");
    
    lua_pushcfunction(engine->L, lua_kryon_element_get_text);
    lua_setfield(engine->L, -2, "getText");
    
    lua_pushcfunction(engine->L, lua_kryon_element_set_text);
    lua_setfield(engine->L, -2, "setText");
    
    // Set as global
    lua_setglobal(engine->L, "kryon");
    
    return KRYON_LUA_SUCCESS;
}