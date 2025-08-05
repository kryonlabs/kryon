/**
 * @file lua_engine.h
 * @brief Kryon Lua Script Engine
 * 
 * Lua scripting integration for event handlers and dynamic behavior
 */

#ifndef KRYON_INTERNAL_LUA_ENGINE_H
#define KRYON_INTERNAL_LUA_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "internal/elements.h"

// =============================================================================
// LUA CONSTANTS
// =============================================================================

extern const char* ELEMENT_METATABLE;
extern const char* ELEMENT_REGISTRY;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonLuaVM KryonLuaVM;
typedef struct lua_State lua_State;
typedef struct KryonLuaEngine KryonLuaEngine;
typedef struct KryonLuaConfig KryonLuaConfig;

// =============================================================================
// LUA ENGINE TYPES
// =============================================================================

/**
 * @brief Lua script compilation result
 */
typedef enum {
    KRYON_LUA_SUCCESS = 0,
    KRYON_LUA_ERROR_COMPILE,
    KRYON_LUA_ERROR_RUNTIME,
    KRYON_LUA_ERROR_MEMORY,
    KRYON_LUA_ERROR_INVALID_PARAM
} KryonLuaResult;

/**
 * @brief Compiled Lua bytecode
 */
typedef struct {
    uint8_t* bytecode;      // Lua bytecode
    size_t size;            // Size of bytecode
    char* source_name;      // Source filename/identifier
} KryonLuaBytecode;



// =============================================================================
// LUA ENGINE API
// =============================================================================

/**
 * @brief Create Lua engine
 * @param config Engine configuration (NULL for defaults)
 * @return Lua engine instance or NULL on failure
 */
KryonLuaEngine* kryon_lua_engine_create(const KryonLuaConfig* config);

/**
 * @brief Destroy Lua engine
 * @param engine Lua engine instance
 */
void kryon_lua_engine_destroy(KryonLuaEngine* engine);

/**
 * @brief Compile Lua source to bytecode
 * @param engine Lua engine instance
 * @param source Lua source code
 * @param source_name Source identifier (for error messages)
 * @param out_bytecode Output bytecode structure
 * @return KRYON_LUA_SUCCESS on success
 */
KryonLuaResult kryon_lua_compile(KryonLuaEngine* engine, 
                                const char* source,
                                const char* source_name,
                                KryonLuaBytecode* out_bytecode);

/**
 * @brief Execute Lua bytecode
 * @param engine Lua engine instance
 * @param bytecode Compiled bytecode
 * @return KRYON_LUA_SUCCESS on success
 */
KryonLuaResult kryon_lua_execute(KryonLuaEngine* engine,
                                const KryonLuaBytecode* bytecode);

/**
 * @brief Call Lua function
 * @param engine Lua engine instance
 * @param function_name Function name to call
 * @param element Element context (optional)
 * @param event Event context (optional)
 * @return KRYON_LUA_SUCCESS on success
 */
KryonLuaResult kryon_lua_call_function(KryonLuaEngine* engine,
                                      const char* function_name,
                                      KryonElement* element,
                                      const KryonEvent* event);

/**
 * @brief Free bytecode structure
 * @param bytecode Bytecode to free
 */
void kryon_lua_bytecode_free(KryonLuaBytecode* bytecode);

/**
 * @brief Get default Lua configuration
 * @return Default configuration
 */
KryonLuaConfig kryon_lua_default_config(void);

/**
 * @brief Register Kryon API functions in Lua
 * @param engine Lua engine instance
 * @return KRYON_LUA_SUCCESS on success
 */
KryonLuaResult kryon_lua_register_api(KryonLuaEngine* engine);

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_LUA_ENGINE_H