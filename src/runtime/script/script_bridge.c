/**
 * @file script_bridge.c
 * @brief Kryon Script Bridge Implementation
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 */

#include "script_bridge.h"
#include "memory.h"
#include <stdlib.h>
#include <string.h>

// =============================================================================
// SCRIPT FUNCTION REGISTRY
// =============================================================================

typedef struct ScriptEventHandler {
    char* eventType;                  // "keyboard", "mouse", etc.
    char* eventPattern;               // "Ctrl+I", "LeftClick", etc.
    void* scriptFunction;             // Script-engine specific function reference
    KryonScriptEngineType engineType; // Which script engine
    struct ScriptEventHandler* next;
} ScriptEventHandler;

typedef struct {
    ScriptEventHandler* handlers;
    // TODO: Add script engine instances when implemented
} KryonScriptBridge;

// =============================================================================
// EVENT HANDLER CALLBACK
// =============================================================================

/**
 * Generic event handler that bridges C events to script functions
 */
static bool script_event_handler_callback(const KryonEvent* event, void* userData) {
    ScriptEventHandler* handler = (ScriptEventHandler*)userData;
    if (!handler || !event) {
        return false;
    }
    
    // For keyboard events, check if the event matches the pattern
    if (event->type == KRYON_EVENT_KEY_DOWN && 
        strcmp(handler->eventType, "keyboard") == 0) {
        
        if (kryon_event_matches_shortcut(&event->data.key, handler->eventPattern)) {
            printf("DEBUG: Script event handler matched: %s -> %s\n", 
                   handler->eventType, handler->eventPattern);
            
            // TODO: Call the actual script function based on engine type
            // For now, just return true to indicate the event was handled
            return true;
        }
    }
    
    // For mouse events
    if ((event->type == KRYON_EVENT_MOUSE_BUTTON_DOWN || 
         event->type == KRYON_EVENT_MOUSE_BUTTON_UP) &&
        strcmp(handler->eventType, "mouse") == 0) {
        
        // Simple pattern matching for mouse events
        bool matches = false;
        if (strcmp(handler->eventPattern, "LeftClick") == 0 && event->data.mouseButton.button == 0) {
            matches = true;
        } else if (strcmp(handler->eventPattern, "RightClick") == 0 && event->data.mouseButton.button == 1) {
            matches = true;
        } else if (strcmp(handler->eventPattern, "MiddleClick") == 0 && event->data.mouseButton.button == 2) {
            matches = true;
        }
        
        if (matches) {
            printf("DEBUG: Script mouse event handler matched: %s -> %s\n", 
                   handler->eventType, handler->eventPattern);
            // TODO: Call the actual script function
            return true;
        }
    }
    
    return false;
}

// =============================================================================
// SCRIPT BRIDGE API
// =============================================================================

bool kryon_script_bridge_init(KryonRuntime* runtime) {
    if (!runtime) {
        return false;
    }
    
    KryonScriptBridge* bridge = kryon_calloc(1, sizeof(KryonScriptBridge));
    if (!bridge) {
        return false;
    }
    
    // Store the bridge in the runtime
    // TODO: Add script_bridge field to KryonRuntime structure
    // runtime->script_bridge = bridge;
    
    printf("DEBUG: Script bridge initialized\n");
    return true;
}

void kryon_script_bridge_cleanup(KryonRuntime* runtime) {
    if (!runtime) {
        return;
    }
    
    // TODO: Get bridge from runtime and cleanup
    // KryonScriptBridge* bridge = runtime->script_bridge;
    // if (!bridge) return;
    
    // Free all script handlers
    // ScriptEventHandler* handler = bridge->handlers;
    // while (handler) {
    //     ScriptEventHandler* next = handler->next;
    //     kryon_free(handler->eventType);
    //     kryon_free(handler->eventPattern);
    //     kryon_free(handler);
    //     handler = next;
    // }
    
    // kryon_free(bridge);
    // runtime->script_bridge = NULL;
    
    printf("DEBUG: Script bridge cleaned up\n");
}

bool kryon_script_register_event_handler(KryonRuntime* runtime,
                                         const char* eventType,
                                         const char* eventPattern, 
                                         void* scriptFunction,
                                         KryonScriptEngineType engineType) {
    if (!runtime || !eventType || !eventPattern || !scriptFunction) {
        return false;
    }
    
    printf("DEBUG: Registering script event handler: %s %s\n", eventType, eventPattern);
    
    // Create handler structure
    ScriptEventHandler* handler = kryon_calloc(1, sizeof(ScriptEventHandler));
    if (!handler) {
        return false;
    }
    
    handler->eventType = kryon_strdup(eventType);
    handler->eventPattern = kryon_strdup(eventPattern);
    handler->scriptFunction = scriptFunction;
    handler->engineType = engineType;
    
    if (!handler->eventType || !handler->eventPattern) {
        kryon_free(handler->eventType);
        kryon_free(handler->eventPattern);
        kryon_free(handler);
        return false;
    }
    
    // Register with the event system
    if (strcmp(eventType, "keyboard") == 0) {
        kryon_event_register_keyboard_handler(runtime->event_system, 
                                             eventPattern,
                                             script_event_handler_callback,
                                             handler);
    } else if (strcmp(eventType, "mouse") == 0) {
        kryon_event_register_mouse_handler(runtime->event_system,
                                          eventPattern,
                                          script_event_handler_callback,
                                          handler);
    }
    
    // TODO: Add to bridge's handler list
    // KryonScriptBridge* bridge = runtime->script_bridge;
    // if (bridge) {
    //     handler->next = bridge->handlers;
    //     bridge->handlers = handler;
    // }
    
    return true;
}

bool kryon_script_unregister_event_handler(KryonRuntime* runtime,
                                           const char* eventType,
                                           const char* eventPattern,
                                           void* scriptFunction) {
    if (!runtime || !eventType || !eventPattern || !scriptFunction) {
        return false;
    }
    
    printf("DEBUG: Unregistering script event handler: %s %s\n", eventType, eventPattern);
    
    // TODO: Find and remove the handler from the bridge's list
    // and unregister from the event system
    
    return true;
}

void* kryon_script_lookup_function(KryonRuntime* runtime, const char* functionName) {
    if (!runtime || !functionName) {
        return NULL;
    }
    
    printf("DEBUG: Looking up script function: %s\n", functionName);
    
    // TODO: Look up function in script engines
    // For now, return a placeholder
    return (void*)functionName; // Just return the name as a placeholder
}

// =============================================================================
// PLACEHOLDER API REGISTRATIONS
// =============================================================================

bool kryon_script_register_lua_api(void* luaState, KryonRuntime* runtime) {
    if (!luaState || !runtime) {
        return false;
    }
    
    printf("DEBUG: Registering Lua API functions\n");
    
    // TODO: Register kryon.register_event, kryon.unregister_event, etc.
    // in the Lua state
    
    return true;
}

bool kryon_script_register_js_api(void* jsContext, KryonRuntime* runtime) {
    if (!jsContext || !runtime) {
        return false;
    }
    
    printf("DEBUG: Registering JavaScript API functions\n");
    
    // TODO: Register kryon.register_event, kryon.unregister_event, etc.
    // in the JavaScript context
    
    return true;
}

bool kryon_script_register_python_api(void* pythonContext, KryonRuntime* runtime) {
    if (!pythonContext || !runtime) {
        return false;
    }
    
    printf("DEBUG: Registering Python API functions\n");
    
    // TODO: Register kryon.register_event, kryon.unregister_event, etc.
    // in the Python context
    
    return true;
}