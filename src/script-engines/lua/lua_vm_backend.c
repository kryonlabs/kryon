/**
 * @file lua_vm_backend.c
 * @brief Lua VM Backend Implementation
 * 
 * Lua backend that implements the KryonVMInterface
 */

#include "internal/script_vm.h"
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
// LUA BACKEND STRUCTURE
// =============================================================================

typedef struct {
    lua_State* L;               // Lua state
    size_t memory_used;         // Current memory usage
    double start_time;          // Execution start time
    char error_buffer[1024];    // Error message buffer
} KryonLuaBackend;

// =============================================================================
// MEMORY ALLOCATOR
// =============================================================================

static void* lua_allocator(void* ud, void* ptr, size_t osize, size_t nsize) {
    KryonVM* vm = (KryonVM*)ud;
    KryonLuaBackend* backend = (KryonLuaBackend*)vm->impl_data;
    
    if (nsize == 0) {
        // Free
        if (ptr) {
            backend->memory_used -= osize;
            kryon_free(ptr);
        }
        return NULL;
    } else {
        // Check memory limit
        if (vm->config.memory_limit > 0) {
            size_t new_usage = backend->memory_used - osize + nsize;
            if (new_usage > vm->config.memory_limit) {
                return NULL; // Memory limit exceeded
            }
        }
        
        // Allocate or reallocate
        void* new_ptr = ptr ? kryon_realloc(ptr, nsize) : kryon_alloc(nsize);
        if (new_ptr) {
            backend->memory_used = backend->memory_used - osize + nsize;
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

static void register_kryon_api(lua_State* L) {
    // Create kryon table
    lua_newtable(L);
    
    // Register functions
    lua_pushcfunction(L, lua_kryon_log);
    lua_setfield(L, -2, "log");
    
    lua_pushcfunction(L, lua_kryon_element_get_text);
    lua_setfield(L, -2, "getText");
    
    lua_pushcfunction(L, lua_kryon_element_set_text);
    lua_setfield(L, -2, "setText");
    
    // Set as global
    lua_setglobal(L, "kryon");
}

// =============================================================================
// VM INTERFACE IMPLEMENTATION
// =============================================================================

static KryonVMResult lua_vm_initialize(KryonVM* vm, const KryonVMConfig* config) {
    KryonLuaBackend* backend = kryon_alloc(sizeof(KryonLuaBackend));
    if (!backend) {
        return KRYON_VM_ERROR_MEMORY;
    }
    
    // Initialize backend
    backend->memory_used = 0;
    backend->start_time = 0;
    backend->error_buffer[0] = '\0';
    
    // Set backend in VM first so allocator can access it
    vm->impl_data = backend;
    
    // Create Lua state with custom allocator
    backend->L = lua_newstate(lua_allocator, vm);
    if (!backend->L) {
        vm->impl_data = NULL;
        kryon_free(backend);
        return KRYON_VM_ERROR_BACKEND_INIT_FAILED;
    }
    
    // Open standard libraries
    luaL_openlibs(backend->L);
    
    // Register Kryon API
    register_kryon_api(backend->L);
    
    return KRYON_VM_SUCCESS;
}

static void lua_vm_destroy(KryonVM* vm) {
    if (!vm || !vm->impl_data) return;
    
    KryonLuaBackend* backend = (KryonLuaBackend*)vm->impl_data;
    
    if (backend->L) {
        lua_close(backend->L);
    }
    
    kryon_free(backend);
    vm->impl_data = NULL;
}

static KryonVMResult lua_vm_compile(KryonVM* vm, const char* source, const char* source_name, KryonScript* out_script) {
    if (!vm || !vm->impl_data || !source || !out_script) {
        return KRYON_VM_ERROR_INVALID_PARAM;
    }
    
    KryonLuaBackend* backend = (KryonLuaBackend*)vm->impl_data;
    
    // Load and compile the source
    int result = luaL_loadbuffer(backend->L, source, strlen(source), source_name);
    if (result != LUA_OK) {
        const char* error = lua_tostring(backend->L, -1);
        snprintf(backend->error_buffer, sizeof(backend->error_buffer), "Lua compilation error: %s", error);
        lua_pop(backend->L, 1);
        return KRYON_VM_ERROR_COMPILE;
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
    
    result = lua_dump(backend->L, writer, &dump_data, 0);
    lua_pop(backend->L, 1); // Remove function from stack
    
    if (result != 0) {
        if (buffer) kryon_free(buffer);
        snprintf(backend->error_buffer, sizeof(backend->error_buffer), "Lua bytecode dump failed");
        return KRYON_VM_ERROR_COMPILE;
    }
    
    // Fill output structure
    out_script->vm_type = KRYON_VM_LUA;
    out_script->format = KRYON_SCRIPT_BYTECODE;
    out_script->data = (uint8_t*)buffer;
    out_script->size = bytecode_size;
    out_script->source_name = source_name ? strdup(source_name) : strdup("lua_script");
    out_script->entry_point = NULL;
    
    return KRYON_VM_SUCCESS;
}

static KryonVMResult lua_vm_load_script(KryonVM* vm, const KryonScript* script) {
    if (!vm || !vm->impl_data || !script || !script->data) {
        return KRYON_VM_ERROR_INVALID_PARAM;
    }
    
    KryonLuaBackend* backend = (KryonLuaBackend*)vm->impl_data;
    
    // Load bytecode
    int result = luaL_loadbuffer(backend->L, (const char*)script->data, 
                                script->size, script->source_name);
    if (result != LUA_OK) {
        const char* error = lua_tostring(backend->L, -1);
        snprintf(backend->error_buffer, sizeof(backend->error_buffer), "Lua load error: %s", error);
        lua_pop(backend->L, 1);
        return KRYON_VM_ERROR_RUNTIME;
    }
    
    // Execute to load functions into global scope
    result = lua_pcall(backend->L, 0, 0, 0);
    if (result != LUA_OK) {
        const char* error = lua_tostring(backend->L, -1);
        snprintf(backend->error_buffer, sizeof(backend->error_buffer), "Lua execution error: %s", error);
        lua_pop(backend->L, 1);
        return KRYON_VM_ERROR_RUNTIME;
    }
    
    return KRYON_VM_SUCCESS;
}

static KryonVMResult lua_vm_call_function(KryonVM* vm, const char* function_name, KryonElement* element, const struct KryonEvent* event) {
    if (!vm || !vm->impl_data || !function_name) {
        return KRYON_VM_ERROR_INVALID_PARAM;
    }
    
    KryonLuaBackend* backend = (KryonLuaBackend*)vm->impl_data;
    
    // Get function from global table
    lua_getglobal(backend->L, function_name);
    if (!lua_isfunction(backend->L, -1)) {
        lua_pop(backend->L, 1);
        snprintf(backend->error_buffer, sizeof(backend->error_buffer), "Lua function '%s' not found", function_name);
        return KRYON_VM_ERROR_RUNTIME;
    }
    
    // TODO: Push element and event as arguments
    // For now, just call with no arguments
    
    int result = lua_pcall(backend->L, 0, 0, 0);
    if (result != LUA_OK) {
        const char* error = lua_tostring(backend->L, -1);
        snprintf(backend->error_buffer, sizeof(backend->error_buffer), "Lua function call error: %s", error);
        lua_pop(backend->L, 1);
        return KRYON_VM_ERROR_RUNTIME;
    }
    
    printf("✅ Lua function '%s' executed successfully\n", function_name);
    return KRYON_VM_SUCCESS;
}

static KryonVMResult lua_vm_execute_string(KryonVM* vm, const char* code) {
    if (!vm || !vm->impl_data || !code) {
        return KRYON_VM_ERROR_INVALID_PARAM;
    }
    
    KryonLuaBackend* backend = (KryonLuaBackend*)vm->impl_data;
    
    int result = luaL_dostring(backend->L, code);
    if (result != LUA_OK) {
        const char* error = lua_tostring(backend->L, -1);
        snprintf(backend->error_buffer, sizeof(backend->error_buffer), "Lua execution error: %s", error);
        lua_pop(backend->L, 1);
        return KRYON_VM_ERROR_RUNTIME;
    }
    
    return KRYON_VM_SUCCESS;
}

static KryonVMResult lua_vm_get_global(KryonVM* vm, const char* name, void* out_value, size_t* out_size) {
    // TODO: Implement global variable retrieval
    (void)vm; (void)name; (void)out_value; (void)out_size;
    return KRYON_VM_ERROR_NOT_SUPPORTED;
}

static KryonVMResult lua_vm_set_global(KryonVM* vm, const char* name, const void* value, size_t size) {
    // TODO: Implement global variable setting
    (void)vm; (void)name; (void)value; (void)size;
    return KRYON_VM_ERROR_NOT_SUPPORTED;
}

static const char* lua_vm_get_version(KryonVM* vm) {
    (void)vm;
    return LUA_VERSION;
}

static bool lua_vm_supports_feature(KryonVM* vm, const char* feature) {
    (void)vm;
    if (strcmp(feature, "bytecode") == 0) return true;
    if (strcmp(feature, "functions") == 0) return true;
    if (strcmp(feature, "coroutines") == 0) return true;
    return false;
}

static const char* lua_vm_get_last_error(KryonVM* vm) {
    if (!vm || !vm->impl_data) {
        return "Invalid VM";
    }
    
    KryonLuaBackend* backend = (KryonLuaBackend*)vm->impl_data;
    return backend->error_buffer[0] ? backend->error_buffer : NULL;
}

static void lua_vm_clear_error(KryonVM* vm) {
    if (!vm || !vm->impl_data) return;
    
    KryonLuaBackend* backend = (KryonLuaBackend*)vm->impl_data;
    backend->error_buffer[0] = '\0';
}

// =============================================================================
// INTERFACE DECLARATION
// =============================================================================

static KryonVMInterface lua_vm_interface = {
    .initialize = lua_vm_initialize,
    .destroy = lua_vm_destroy,
    .compile = lua_vm_compile,
    .load_script = lua_vm_load_script,
    .call_function = lua_vm_call_function,
    .execute_string = lua_vm_execute_string,
    .get_global = lua_vm_get_global,
    .set_global = lua_vm_set_global,
    .get_version = lua_vm_get_version,
    .supports_feature = lua_vm_supports_feature,
    .get_last_error = lua_vm_get_last_error,
    .clear_error = lua_vm_clear_error
};

// =============================================================================
// PUBLIC INTERFACE
// =============================================================================

KryonVMInterface* kryon_lua_vm_interface(void) {
    return &lua_vm_interface;
}