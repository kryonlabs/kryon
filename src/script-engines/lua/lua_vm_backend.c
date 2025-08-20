/**
 * @file lua_vm_backend.c
 * @brief Lua VM Backend Implementation
 * 
 * Lua backend that implements the KryonVMInterface
 */

#include "script_vm.h"
#include "lua_engine.h"
#include "memory.h"
#include "runtime.h"
#include "events.h"
#include "../../runtime/navigation/navigation.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

// =============================================================================
// LUA PATH CONFIGURATION
// =============================================================================

/**
 * @brief Configure Lua package paths for standard library support
 */
static void configure_lua_paths(lua_State* L) {
    // Get the current package.path and package.cpath
    lua_getglobal(L, "package");
    
    if (!lua_istable(L, -1)) {
        printf("[LuaVM] Warning: package table not found\n");
        lua_pop(L, 1);
        return;
    }
    
    // Get current paths
    lua_getfield(L, -1, "path");
    const char* current_path = lua_tostring(L, -1);
    lua_pop(L, 1);
    
    lua_getfield(L, -1, "cpath");
    const char* current_cpath = lua_tostring(L, -1);
    lua_pop(L, 1);
    
    // Build new paths with common Lua library locations
    char new_path[2048];
    char new_cpath[2048];
    
    // Common Lua module paths
    snprintf(new_path, sizeof(new_path), 
        "%s;./?.lua;./?/init.lua;"
        "/usr/local/share/lua/5.4/?.lua;/usr/local/share/lua/5.4/?/init.lua;"
        "/usr/share/lua/5.4/?.lua;/usr/share/lua/5.4/?/init.lua;"
        "~/.luarocks/share/lua/5.4/?.lua;~/.luarocks/share/lua/5.4/?/init.lua",
        current_path ? current_path : "");
    
    // Common Lua C module paths
    snprintf(new_cpath, sizeof(new_cpath),
        "%s;./?.so;./?/?.so;"
        "/usr/local/lib/lua/5.4/?.so;/usr/local/lib/lua/5.4/loadall.so;"
        "/usr/lib/lua/5.4/?.so;/usr/lib/lua/5.4/loadall.so;"
        "~/.luarocks/lib/lua/5.4/?.so",
        current_cpath ? current_cpath : "");
    
    // Set the new paths
    lua_pushstring(L, new_path);
    lua_setfield(L, -2, "path");
    
    lua_pushstring(L, new_cpath);
    lua_setfield(L, -2, "cpath");
    
    lua_pop(L, 1); // Pop package table
    
    printf("[LuaVM] Configured Lua paths for external library support\n");
}

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
    // Lua log message
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
    // Setting element text
    return 0;
}

static int lua_kryon_navigation_navigate_to(lua_State* L) {
    const char* target = luaL_checkstring(L, 1);
    
    // Get runtime from current global context
    KryonRuntime* runtime = kryon_runtime_get_current();
    if (!runtime) {
        luaL_error(L, "Navigation not available: no runtime context");
        return 0;
    }
    
    // Actually call navigation manager
    printf("🔀 @onload script: Navigating to '%s'\n", target);
    
    if (runtime->navigation_manager) {
        KryonNavigationResult result = kryon_navigate_to(runtime->navigation_manager, target, false);
        lua_pushboolean(L, result == KRYON_NAV_SUCCESS);
        return 1;
    } else {
        luaL_error(L, "Navigation manager not available");
        return 0;
    }
}

static int lua_kryon_fs_readdir(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    
    printf("🗂️  @onload script: Reading directory '%s'\n", path);
    
    // Open directory
    DIR* dir = opendir(path);
    if (!dir) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to open directory");
        return 2;
    }
    
    // Create result table
    lua_newtable(L);
    int index = 1;
    
    // Read directory entries
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and .. entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Add filename to table
        lua_pushstring(L, entry->d_name);
        lua_rawseti(L, -2, index++);
    }
    
    closedir(dir);
    return 1;
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
    
    // Create navigation sub-table
    lua_newtable(L);
    lua_pushcfunction(L, lua_kryon_navigation_navigate_to);
    lua_setfield(L, -2, "navigateTo");
    lua_setfield(L, -2, "navigation");
    
    // Create fs sub-table
    lua_newtable(L);
    lua_pushcfunction(L, lua_kryon_fs_readdir);
    lua_setfield(L, -2, "readdir");
    lua_setfield(L, -2, "fs");
    
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
    
    // Configure Lua paths for external libraries
    configure_lua_paths(backend->L);
    
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

// Helper structure for lua_dump writer
typedef struct {
    char** buffer;
    size_t* size;
    size_t capacity;
} DumpData;

// Writer function for lua_dump
static int lua_dump_writer(lua_State* L, const void* p, size_t sz, void* ud) {
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
    DumpData dump_data = {&buffer, &bytecode_size, 0};
    
    result = lua_dump(backend->L, lua_dump_writer, &dump_data, 0);
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

// Helper function to find component instance for element
static KryonComponentInstance* find_component_for_element(KryonElement* element) {
    if (!element) return NULL;
    
    // Walk up the element tree to find a component instance
    KryonElement* current = element;
    while (current) {
        if (current->component_instance) {
            return current->component_instance;
        }
        current = current->parent;
    }
    
    return NULL;
}

// Helper function to get component state value by element position
static const char* get_component_state_value_for_element(KryonElement* element) {
    if (!element) {
        // Element is NULL
        return "0";
    }
    
    
    // This is a simplified approach that determines component instance based on element hierarchy
    // In the counter example, we have two Counter instances, we need to figure out which one this element belongs to
    
    // Walk up to find the root, then walk down to find position
    KryonElement* root = element;
    while (root->parent) {
        root = root->parent;
    }
    
    // Simple heuristic: find which "Row" this element belongs to
    // First Row = comp_0, Second Row = comp_1
    KryonElement* current = element;
    while (current && current->parent) {
        // Check if parent is Column
        if (current->parent->type_name && strcmp(current->parent->type_name, "Column") == 0) {
            // This element is in a Column, check if it's the first or second child
            KryonElement* column = current->parent;
            // Found Column parent
            for (size_t i = 0; i < column->child_count; i++) {
                if (column->children[i] == current) {
                    // Use runtime accessor to get component state
                    KryonRuntime* runtime = kryon_runtime_get_current();
                    if (runtime) {
                        char pattern[64];
                        snprintf(pattern, sizeof(pattern), "comp_%zu.value", i);
                        const char* value = kryon_runtime_get_variable(runtime, pattern);
                        // Component state lookup
                        return value ? value : "0";
                    }
                    break;
                }
            }
            break;
        }
        current = current->parent;
    }
    
    // Component state lookup failed - using default
    return "0"; // Default fallback
}

// Helper function to update component state value by element position  
static void update_component_state_value_for_element(KryonElement* element, const char* new_value) {
    if (!element || !new_value) {
        // NULL parameter
        return;
    }
    
    // Updating component state
    
    // Similar logic to get_component_state_value_for_element, but for updating
    KryonElement* current = element;
    while (current && current->parent) {
        if (current->parent->type_name && strcmp(current->parent->type_name, "Column") == 0) {
            KryonElement* column = current->parent;
            for (size_t i = 0; i < column->child_count; i++) {
                if (column->children[i] == current) {
                    // Update the runtime variable
                    KryonRuntime* runtime = kryon_runtime_get_current();
                    if (runtime) {
                        char pattern[64];
                        snprintf(pattern, sizeof(pattern), "comp_%zu.value", i);
                        
                        // Find and update the variable in the runtime registry 
                        for (size_t j = 0; j < runtime->variable_count; j++) {
                            if (runtime->variable_names[j] && 
                                strcmp(runtime->variable_names[j], pattern) == 0) {
                                kryon_free(runtime->variable_values[j]);
                                runtime->variable_values[j] = kryon_strdup(new_value);
                                // Updated component state
                                break;
                            }
                        }
                    }
                    break;
                }
            }
            break;
        }
        current = current->parent;
    }
}

// Find which component instance this element belongs to by examining sibling Text elements
static const char* find_component_instance_id(KryonElement* element) {
    if (!element || !element->parent) return NULL;
    
    // Look at the parent (usually a Row containing Button, Text, Button)
    KryonElement* parent = element->parent;
    
    // Search through siblings for a Text element with a bound variable
    for (size_t i = 0; i < parent->child_count; i++) {
        KryonElement* sibling = parent->children[i];
        if (sibling && sibling->type_name && strcmp(sibling->type_name, "Text") == 0) {
            // Check if this Text element has a bound variable reference
            for (size_t j = 0; j < sibling->property_count; j++) {
                KryonProperty* prop = sibling->properties[j];
                if (prop && prop->name && strcmp(prop->name, "text") == 0 && prop->is_bound) {
                    // Found a bound text property, extract the component instance ID
                    const char* var_name = prop->binding_path;
                    if (var_name && strncmp(var_name, "comp_", 5) == 0) {
                        // Extract "comp_0" from "comp_0.value"
                        const char* dot = strchr(var_name, '.');
                        if (dot) {
                            size_t prefix_len = dot - var_name;
                            char* instance_id = malloc(prefix_len + 1);
                            strncpy(instance_id, var_name, prefix_len);
                            instance_id[prefix_len] = '\0';
                            printf("🔍 COMPONENT INSTANCE: Found instance '%s' for element\n", instance_id);
                            return instance_id; // Note: caller should free this
                        }
                    }
                }
            }
        }
    }
    
    printf("❌ COMPONENT INSTANCE: No component instance found for element\n");
    return NULL;
}

// Set up component-instance-specific context in Lua (create self table)
static void setup_component_context(lua_State* L, KryonElement* element, const char* component_instance) {
    if (!L || !element) return;
    
    printf("🔧 COMPONENT CONTEXT: Setting up context for instance '%s'\n", component_instance ? component_instance : "unknown");
    
    // Create empty self table
    lua_newtable(L);              // Create new table
    lua_setglobal(L, "self");     // Set global self = table (will be populated later with instance-specific variables)
    
    printf("✅ COMPONENT CONTEXT: Created empty self table for instance '%s'\n", component_instance ? component_instance : "unknown");
}

// Update component state from Lua context after function call
static void update_component_state_after_call(lua_State* L, KryonElement* element) {
    if (!L || !element) return;
    
    // Get self.value from Lua state
    lua_getglobal(L, "self");     // Get self table
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "value"); // Get self.value
        if (lua_isnumber(L, -1)) {
            double new_value = lua_tonumber(L, -1);
            char value_str[32];
            snprintf(value_str, sizeof(value_str), "%.0f", new_value); // Convert to integer string
            
            // Update the component state in runtime
            update_component_state_value_for_element(element, value_str);
            
            // Trigger re-render so text bindings can pick up the new value
            KryonRuntime* runtime = kryon_runtime_get_current();
            if (runtime && runtime->root) {
                mark_elements_for_rerender(runtime->root);
            }
        }
        lua_pop(L, 1); // Pop value
    }
    lua_pop(L, 1); // Pop self table
}

static KryonVMResult lua_vm_call_function(KryonVM* vm, const char* function_name, KryonElement* element, const KryonEvent* event) {
    if (!vm || !vm->impl_data || !function_name) {
        printf("❌ LUA VM: Invalid parameters for function call\n");
        return KRYON_VM_ERROR_INVALID_PARAM;
    }
    
    printf("🔥 LUA VM: Calling function '%s'\n", function_name);
    
    KryonLuaBackend* backend = (KryonLuaBackend*)vm->impl_data;
    
    // Get function from global table
    lua_getglobal(backend->L, function_name);
    if (!lua_isfunction(backend->L, -1)) {
        lua_pop(backend->L, 1);
        printf("❌ LUA VM: Function '%s' not found in global table\n", function_name);
        snprintf(backend->error_buffer, sizeof(backend->error_buffer), "Lua function '%s' not found", function_name);
        return KRYON_VM_ERROR_RUNTIME;
    }
    
    printf("✅ LUA VM: Function '%s' found, setting up context\n", function_name);
    
    // Find which component instance this element belongs to
    const char* component_instance = find_component_instance_id(element);
    
    // Set up component context for the specific instance
    setup_component_context(backend->L, element, component_instance);
    
    // Sync runtime variables to Lua globals before function execution
    if (vm->runtime) {
        printf("🔄 SYNC TO LUA: Starting variable sync (%zu variables)\n", vm->runtime->variable_count);
        
        // Get the self table we just created
        lua_getglobal(backend->L, "self");
        
        for (size_t i = 0; i < vm->runtime->variable_count; i++) {
            if (vm->runtime->variable_names[i] && vm->runtime->variable_values[i]) {
                const char* name = vm->runtime->variable_names[i];
                const char* value = vm->runtime->variable_values[i];
                printf("🔄 SYNC TO LUA: Variable '%s' = '%s'\n", name, value);
                
                // Check if this is a component variable (comp_X.variableName)
                const char* dot = strrchr(name, '.');
                if (dot && strncmp(name, "comp_", 5) == 0) {
                    // This is a component variable like "comp_0.value"
                    
                    // Only sync variables that belong to the current component instance
                    if (component_instance) {
                        size_t instance_len = strlen(component_instance);
                        if (strncmp(name, component_instance, instance_len) == 0 && name[instance_len] == '.') {
                            // This variable belongs to the current component instance
                            const char* var_name = dot + 1; // Get everything after the last dot
                            printf("🔄 SYNC TO LUA: Adding component variable self.%s = %s (from %s)\n", var_name, value, name);
                            
                            // Try to convert to number first
                            char* endptr;
                            double num_value = strtod(value, &endptr);
                            if (*endptr == '\0') {
                                // Successfully converted to number
                                lua_pushnumber(backend->L, num_value);
                                printf("🔄 SYNC TO LUA: Set self.%s = %g (number)\n", var_name, num_value);
                            } else {
                                // Keep as string
                                lua_pushstring(backend->L, value);
                                printf("🔄 SYNC TO LUA: Set self.%s = '%s' (string)\n", var_name, value);
                            }
                            lua_setfield(backend->L, -2, var_name); // Set self[var_name] = value
                        } else {
                            printf("🔄 SYNC TO LUA: Skipping variable '%s' (not for instance '%s')\n", name, component_instance);
                        }
                    }
                    continue; // Skip the regular variable handling below
                }
                
                // Check if this is a JSON array format
                if (value[0] == '[' && value[strlen(value)-1] == ']') {
                    printf("🔄 SYNC TO LUA: Creating Lua table for array variable '%s'\n", name);
                    // Create Lua table from JSON array
                    lua_newtable(backend->L);
                    
                    // Parse JSON array - improved implementation
                    if (strlen(value) > 2) { // Not empty array []
                        const char* ptr = value + 1; // Skip opening [
                        int table_index = 1;
                        char item_buffer[256];
                        
                        while (*ptr && *ptr != ']') {
                            // Skip whitespace and commas
                            while (*ptr == ' ' || *ptr == ',') ptr++;
                            if (*ptr == ']') break;
                            
                            // Parse string item
                            if (*ptr == '"') {
                                ptr++; // Skip opening quote
                                size_t item_len = 0;
                                while (*ptr && *ptr != '"' && item_len < sizeof(item_buffer) - 1) {
                                    item_buffer[item_len++] = *ptr++;
                                }
                                item_buffer[item_len] = '\0';
                                if (*ptr == '"') ptr++; // Skip closing quote
                                
                                lua_pushstring(backend->L, item_buffer);
                                lua_rawseti(backend->L, -2, table_index++);
                                printf("🔄 SYNC TO LUA: Added array item [%d] = '%s'\n", table_index-1, item_buffer);
                            } else {
                                // Skip non-string items for now
                                while (*ptr && *ptr != ',' && *ptr != ']') ptr++;
                            }
                        }
                    }
                    lua_setglobal(backend->L, name);
                } else {
                    // Regular string variable
                    lua_pushstring(backend->L, value);
                    lua_setglobal(backend->L, name);
                }
            }
        }
        
        // Set the self table as a global so Lua functions can access it
        lua_setglobal(backend->L, "self");
    }
    
    int result = lua_pcall(backend->L, 0, 0, 0);
    
    // Update component state after function call
    if (result == LUA_OK) {
        update_component_state_after_call(backend->L, element);
        
        // Sync Lua self table back to component variables after function execution
        if (vm->runtime) {
            printf("🔄 SYNC FROM LUA: Starting variable sync back (%zu variables)\n", vm->runtime->variable_count);
            
            // Get the self table 
            lua_getglobal(backend->L, "self");
            if (lua_istable(backend->L, -1)) {
                printf("🔄 SYNC FROM LUA: Found self table, checking for component variable updates\n");
                
                for (size_t i = 0; i < vm->runtime->variable_count; i++) {
                    if (vm->runtime->variable_names[i]) {
                        const char* name = vm->runtime->variable_names[i];
                        
                        // Check if this is a component variable (comp_X.variableName)
                        const char* dot = strrchr(name, '.');
                        if (dot && strncmp(name, "comp_", 5) == 0) {
                            // Only sync back variables that belong to the current component instance
                            if (component_instance) {
                                size_t instance_len = strlen(component_instance);
                                if (strncmp(name, component_instance, instance_len) == 0 && name[instance_len] == '.') {
                                    // This variable belongs to the current component instance
                                    const char* var_name = dot + 1; // Get everything after the last dot
                                    
                                    // Get the value from self table
                                    lua_getfield(backend->L, -1, var_name); // Get self[var_name]
                            
                            char* new_value = NULL;
                            if (lua_isnumber(backend->L, -1)) {
                                double num_value = lua_tonumber(backend->L, -1);
                                char value_str[32];
                                snprintf(value_str, sizeof(value_str), "%.0f", num_value); // Convert to integer string
                                new_value = kryon_strdup(value_str);
                                printf("🔄 SYNC FROM LUA: Component variable '%s' self.%s = %g -> '%s'\n", name, var_name, num_value, new_value);
                            } else if (lua_isstring(backend->L, -1)) {
                                const char* str_value = lua_tostring(backend->L, -1);
                                new_value = kryon_strdup(str_value);
                                printf("🔄 SYNC FROM LUA: Component variable '%s' self.%s = '%s'\n", name, var_name, new_value);
                            } else {
                                printf("🔄 SYNC FROM LUA: Component variable '%s' self.%s is not number or string (type=%d)\n", name, var_name, lua_type(backend->L, -1));
                            }
                            
                            // Update runtime variable if it changed
                            if (new_value && (!vm->runtime->variable_values[i] || strcmp(vm->runtime->variable_values[i], new_value) != 0)) {
                                kryon_free(vm->runtime->variable_values[i]);
                                vm->runtime->variable_values[i] = new_value;
                                printf("✅ SYNC FROM LUA: Updated component variable '%s' = '%s'\n", vm->runtime->variable_names[i], new_value);
                                
                                // Trigger re-render so text bindings can pick up the new value
                                if (vm->runtime && vm->runtime->root) {
                                    printf("🔄 TRIGGERING RE-RENDER: Component variable '%s' changed, marking elements for re-render\n", name);
                                    mark_elements_for_rerender(vm->runtime->root);
                                }
                            } else if (new_value) {
                                printf("🔄 SYNC FROM LUA: Component variable '%s' unchanged, freeing new value\n", name);
                                kryon_free(new_value);
                            } else {
                                printf("🔄 SYNC FROM LUA: Component variable '%s' has no new value\n", name);
                            }
                            
                                    lua_pop(backend->L, 1); // Pop the field value
                                } else {
                                    printf("🔄 SYNC FROM LUA: Skipping variable '%s' (not for instance '%s')\n", name, component_instance);
                                }
                            }
                        } else {
                            // Handle non-component variables (arrays, etc.) from global scope
                            lua_getglobal(backend->L, name);
                            
                            char* new_value = NULL;
                            if (lua_istable(backend->L, -1)) {
                                printf("🔄 SYNC FROM LUA: Variable '%s' is a table, converting to JSON\n", name);
                                // Convert Lua table back to JSON array format
                                size_t table_len = lua_rawlen(backend->L, -1);
                                printf("🔄 SYNC FROM LUA: Table length = %zu\n", table_len);
                                size_t buffer_size = 1024;
                                char* json_buffer = kryon_alloc(buffer_size);
                                size_t json_len = 0;
                                
                                json_buffer[json_len++] = '[';
                                for (size_t j = 1; j <= table_len; j++) {
                                    if (j > 1) {
                                        json_buffer[json_len++] = ',';
                                        json_buffer[json_len++] = ' ';
                                    }
                                    lua_rawgeti(backend->L, -1, j);
                                    if (lua_isstring(backend->L, -1)) {
                                        json_buffer[json_len++] = '"';
                                        const char* str = lua_tostring(backend->L, -1);
                                        size_t str_len = strlen(str);
                                        printf("🔄 SYNC FROM LUA: Table item [%zu] = '%s'\n", j, str);
                                        if (json_len + str_len + 3 < buffer_size) {
                                            memcpy(json_buffer + json_len, str, str_len);
                                            json_len += str_len;
                                        }
                                        json_buffer[json_len++] = '"';
                                    }
                                    lua_pop(backend->L, 1);
                                }
                                json_buffer[json_len++] = ']';
                                json_buffer[json_len] = '\0';
                                new_value = json_buffer;
                                printf("🔄 SYNC FROM LUA: Generated JSON: '%s'\n", new_value);
                            } else if (lua_isstring(backend->L, -1)) {
                                const char* str_value = lua_tostring(backend->L, -1);
                                new_value = kryon_strdup(str_value);
                                printf("🔄 SYNC FROM LUA: Variable '%s' is string = '%s'\n", name, new_value);
                            } else {
                                printf("🔄 SYNC FROM LUA: Variable '%s' is not string or table (type=%d)\n", name, lua_type(backend->L, -1));
                            }
                            
                            // Update runtime variable if it changed
                            if (new_value && (!vm->runtime->variable_values[i] || strcmp(vm->runtime->variable_values[i], new_value) != 0)) {
                                kryon_free(vm->runtime->variable_values[i]);
                                vm->runtime->variable_values[i] = new_value;
                                printf("✅ SYNC FROM LUA: Updated variable '%s' = '%s'\n", vm->runtime->variable_names[i], new_value);
                                
                                // Trigger @for directive re-processing for array variables
                                if (new_value[0] == '[' && new_value[strlen(new_value)-1] == ']') {
                                    printf("🔁 SYNC FROM LUA: Array variable '%s' changed, triggering @for re-processing\n", vm->runtime->variable_names[i]);
                                    
                                    // Trigger @for re-processing by calling the runtime function
                                    if (vm->runtime->root) {
                                        printf("🔄 RE-PROCESSING: Calling @for re-processing for root element\n");
                                        process_for_directives(vm->runtime, vm->runtime->root);
                                    }
                                }
                            } else if (new_value) {
                                printf("🔄 SYNC FROM LUA: Variable '%s' unchanged, freeing new value\n", name);
                                kryon_free(new_value);
                            } else {
                                printf("🔄 SYNC FROM LUA: Variable '%s' has no new value\n", name);
                            }
                            
                            lua_pop(backend->L, 1); // Pop the global variable
                        }
                    }
                }
            } else {
                printf("🔄 SYNC FROM LUA: No self table found\n");
            }
            lua_pop(backend->L, 1); // Pop the self table or nil
        }
    }
    // Clean up allocated memory
    if (component_instance) {
        free((void*)component_instance);
    }
    
    if (result != LUA_OK) {
        const char* error = lua_tostring(backend->L, -1);
        snprintf(backend->error_buffer, sizeof(backend->error_buffer), "Lua function call error: %s", error);
        lua_pop(backend->L, 1);
        return KRYON_VM_ERROR_RUNTIME;
    }
    
    // Lua function executed successfully
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

static void lua_vm_notify_element_destroyed(KryonVM* vm, KryonElement* element) {
    if (!vm || !vm->impl_data || !element) {
        return;
    }

    KryonLuaBackend* backend = (KryonLuaBackend*)vm->impl_data;
    lua_State* L = backend->L;

    // Get the element registry from the Lua registry
    lua_getfield(L, LUA_REGISTRYINDEX, ELEMENT_REGISTRY);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1); // Pop the non-table value
        return;
    }

    // Find the userdata associated with the element pointer
    lua_pushlightuserdata(L, element);
    lua_gettable(L, -2);

    if (lua_isuserdata(L, -1)) {
        // Invalidate the userdata's pointer
        KryonElement** udata = (KryonElement**)lua_touserdata(L, -1);
        if (udata) {
            *udata = NULL;
        }
    }
    lua_pop(L, 1); // Pop the userdata or nil

    // Remove the entry from the registry
    lua_pushlightuserdata(L, element);
    lua_pushnil(L);
    lua_settable(L, -3);

    lua_pop(L, 1); // Pop the registry table
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
    .clear_error = lua_vm_clear_error,
    .notify_element_destroyed = lua_vm_notify_element_destroyed
};

// =============================================================================
// PUBLIC INTERFACE
// =============================================================================

KryonVMInterface* kryon_lua_vm_interface(void) {
    return &lua_vm_interface;
}