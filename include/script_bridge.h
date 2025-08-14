/**
 * @file script_bridge.h
 * @brief Kryon Script Bridge API
 * 
 * Provides the bridge between C runtime and script engines (Lua, JavaScript, Python, etc.)
 * for the kryon.* API exposed to scripts.
 */

#ifndef KRYON_INTERNAL_SCRIPT_BRIDGE_H
#define KRYON_INTERNAL_SCRIPT_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include "events.h"
#include "runtime.h"

// =============================================================================
// SCRIPT ENGINE INTEGRATION
// =============================================================================

typedef enum {
    KRYON_SCRIPT_LUA,
    KRYON_SCRIPT_JAVASCRIPT,
    KRYON_SCRIPT_PYTHON,
    KRYON_SCRIPT_WREN
} KryonScriptEngineType;

typedef struct KryonScriptEngine KryonScriptEngine;
typedef struct KryonScriptContext KryonScriptContext;

// Script function callback type
typedef bool (*KryonScriptEventHandler)(const KryonEvent* event, void* scriptFunction);

// =============================================================================
// SCRIPT API REGISTRATION
// =============================================================================

/**
 * Initialize script bridge for a runtime
 */
bool kryon_script_bridge_init(KryonRuntime* runtime);

/**
 * Cleanup script bridge for a runtime
 */
void kryon_script_bridge_cleanup(KryonRuntime* runtime);

/**
 * Register a script function as an event handler
 * This is called from script contexts via kryon.register_event()
 */
bool kryon_script_register_event_handler(KryonRuntime* runtime,
                                         const char* eventType,
                                         const char* eventPattern, 
                                         void* scriptFunction,
                                         KryonScriptEngineType engineType);

/**
 * Unregister a script event handler
 * This is called from script contexts via kryon.unregister_event()
 */
bool kryon_script_unregister_event_handler(KryonRuntime* runtime,
                                           const char* eventType,
                                           const char* eventPattern,
                                           void* scriptFunction);

/**
 * Look up a script function by name
 * Used when loading event handlers from KRB files
 */
void* kryon_script_lookup_function(KryonRuntime* runtime, const char* functionName);

// =============================================================================
// LUA API FUNCTIONS  
// =============================================================================

/**
 * Register Lua API functions (kryon.register_event, etc.)
 * Called when initializing a Lua script context
 */
bool kryon_script_register_lua_api(void* luaState, KryonRuntime* runtime);

// =============================================================================
// JAVASCRIPT API FUNCTIONS
// =============================================================================

/**
 * Register JavaScript API functions
 * Called when initializing a JavaScript script context
 */
bool kryon_script_register_js_api(void* jsContext, KryonRuntime* runtime);

// =============================================================================
// PYTHON API FUNCTIONS
// =============================================================================

/**
 * Register Python API functions
 * Called when initializing a Python script context
 */
bool kryon_script_register_python_api(void* pythonContext, KryonRuntime* runtime);

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_SCRIPT_BRIDGE_H