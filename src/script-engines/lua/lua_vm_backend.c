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
    
    // Get VM reference from registry
    lua_getfield(L, LUA_REGISTRYINDEX, "kryon_vm");
    KryonVM* vm = (KryonVM*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    if (!vm || !vm->runtime) {
        luaL_error(L, "Navigation not available: no VM or runtime context");
        return 0;
    }
    
    KryonRuntime* runtime = vm->runtime;
    
    // Actually call navigation manager
    printf("🔀 VM script: Navigating to '%s'\n", target);
    
    if (runtime->navigation_manager) {
        KryonNavigationResult result = kryon_navigate_to(runtime->navigation_manager, target, false);
        lua_pushboolean(L, result == KRYON_NAV_SUCCESS);
        return 1;
    } else {
        luaL_error(L, "Navigation manager not available");
        return 0;
    }
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
    
    // Store VM reference in registry for navigation functions
    lua_pushlightuserdata(backend->L, vm);
    lua_setfield(backend->L, LUA_REGISTRYINDEX, "kryon_vm");
    
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

// =============================================================================
// VARIABLE SYNC UTILITY FUNCTIONS 
// =============================================================================

/**
 * @brief Check if a variable name represents a global variable
 */
static bool is_global_variable(const char* name) {
    if (!name) return false;
    
    // Global variables don't have a component prefix (comp_X.)
    const char* dot = strrchr(name, '.');
    return !(dot && strncmp(name, "comp_", 5) == 0);
}

/**
 * @brief Check if a variable name represents a component variable
 */
static bool is_component_variable(const char* name) {
    if (!name) return false;
    
    // Component variables have format: comp_X.variableName
    const char* dot = strrchr(name, '.');
    return (dot && strncmp(name, "comp_", 5) == 0);
}

/**
 * @brief Check if a variable should be synced (only sync declared/used variables)
 */
static bool should_sync_variable(const char* name) {
    if (!name) return false;
    
    // For now, sync all variables. In the future, we can add logic to
    // check if the variable is actually declared or used in the code
    return true;
}

/**
 * @brief Convert Lua table to JSON string format
 */
static char* convert_lua_table_to_json(lua_State* L, int table_index) {
    if (!lua_istable(L, table_index)) {
        return NULL;
    }
    
    size_t table_len = lua_rawlen(L, table_index);
    size_t buffer_size = 1024;
    char* json_buffer = kryon_alloc(buffer_size);
    size_t json_len = 0;
    
    json_buffer[json_len++] = '[';
    for (size_t j = 1; j <= table_len; j++) {
        if (j > 1) {
            json_buffer[json_len++] = ',';
        }
        
        lua_rawgeti(L, table_index, (int)j);
        if (lua_istable(L, -1)) {
            // Handle nested table (like {file="x.kry", title="X"})
            json_buffer[json_len++] = '{';
            bool first_field = true;
            
            lua_pushnil(L);
            while (lua_next(L, -2) != 0) {
                if (!first_field) {
                    json_buffer[json_len++] = ',';
                }
                first_field = false;
                
                // Add key
                const char* key = lua_tostring(L, -2);
                json_len += snprintf(json_buffer + json_len, buffer_size - json_len, "\"%s\":", key);
                
                // Add value
                const char* value = lua_tostring(L, -1);
                json_len += snprintf(json_buffer + json_len, buffer_size - json_len, "\"%s\"", value);
                
                lua_pop(L, 1); // Remove value, keep key
            }
            json_buffer[json_len++] = '}';
        } else {
            const char* item = lua_tostring(L, -1);
            json_len += snprintf(json_buffer + json_len, buffer_size - json_len, "\"%s\"", item);
        }
        lua_pop(L, 1); // Pop the item
    }
    json_buffer[json_len++] = ']';
    json_buffer[json_len] = '\0';
    
    return json_buffer;
}

/**
 * @brief Convert JSON array string to Lua table
 */
static void convert_json_to_lua_table(lua_State* L, const char* json_str) {
    if (!json_str || json_str[0] != '[') {
        lua_pushnil(L);
        return;
    }
    
    lua_newtable(L);
    const char* ptr = json_str + 1; // Skip opening '['
    int table_index = 1;
    
    while (*ptr && *ptr != ']') {
        // Skip whitespace and commas
        while (*ptr && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == ',')) ptr++;
        
        if (*ptr == '"') {
            // String item
            ptr++; // Skip opening quote
            char item_buffer[256];
            size_t item_len = 0;
            
            while (*ptr && *ptr != '"' && item_len < sizeof(item_buffer)-1) {
                item_buffer[item_len++] = *ptr++;
            }
            item_buffer[item_len] = '\0';
            
            if (*ptr == '"') ptr++; // Skip closing quote
            
            lua_pushstring(L, item_buffer);
            lua_rawseti(L, -2, table_index++);
        } else {
            // Skip non-string items for now
            while (*ptr && *ptr != ',' && *ptr != ']') ptr++;
        }
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

/**
 * @brief Sync runtime variables to Lua globals (variables first approach)
 */
static void sync_variables_to_lua(KryonVM* vm, lua_State* L, const char* component_instance) {
    if (!vm || !vm->runtime || !L) return;
    
    printf("🔄 SYNC TO LUA: Starting clean variable sync (%zu variables)\n", vm->runtime->variable_count);
    
    // PHASE 1: Sync global variables first (variables first principle)
    printf("🔄 SYNC TO LUA: Phase 1 - Global variables\n");
    for (size_t i = 0; i < vm->runtime->variable_count; i++) {
        if (!vm->runtime->variable_names[i] || !vm->runtime->variable_values[i]) continue;
        if (!should_sync_variable(vm->runtime->variable_names[i])) continue;
        
        const char* name = vm->runtime->variable_names[i];
        const char* value = vm->runtime->variable_values[i];
        
        if (is_global_variable(name)) {
            printf("🔄 SYNC TO LUA: Global variable '%s' = '%s'\n", name, value);
            
            if (value[0] == '[' && value[strlen(value)-1] == ']') {
                // Array/table variable - convert JSON to Lua table
                printf("🔄 SYNC TO LUA: Converting array '%s' to Lua table\n", name);
                convert_json_to_lua_table(L, value);
                lua_setglobal(L, name);
            } else if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0) {
                // Boolean variable
                lua_pushboolean(L, strcmp(value, "true") == 0);
                lua_setglobal(L, name);
            } else {
                // Regular string/number variable
                // Check for empty string first - empty strings should remain strings, not become 0.0
                if (value[0] == '\0') {
                    lua_pushstring(L, value);  // Push empty string as-is
                } else {
                    char* endptr;
                    double num_value = strtod(value, &endptr);
                    if (*endptr == '\0') {
                        // Successfully converted to number
                        lua_pushnumber(L, num_value);
                    } else {
                        // String value
                        lua_pushstring(L, value);
                    }
                }
                lua_setglobal(L, name);
            }
        }
    }
    
    // PHASE 2: Sync component variables to self table if component_instance provided
    if (component_instance) {
        printf("🔄 SYNC TO LUA: Phase 2 - Component variables for '%s'\n", component_instance);
        
        // Get the self table (should already exist from setup_component_context)
        lua_getglobal(L, "self");
        if (lua_istable(L, -1)) {
            for (size_t i = 0; i < vm->runtime->variable_count; i++) {
                if (!vm->runtime->variable_names[i] || !vm->runtime->variable_values[i]) continue;
                if (!should_sync_variable(vm->runtime->variable_names[i])) continue;
                
                const char* name = vm->runtime->variable_names[i];
                const char* value = vm->runtime->variable_values[i];
                
                if (is_component_variable(name)) {
                    // Check if this variable belongs to the current component instance
                    size_t instance_len = strlen(component_instance);
                    if (strncmp(name, component_instance, instance_len) == 0 && name[instance_len] == '.') {
                        const char* var_name = strrchr(name, '.') + 1;
                        printf("🔄 SYNC TO LUA: Component variable self.%s = %s (from %s)\n", var_name, value, name);
                        
                        // Try to convert to number first
                        char* endptr;
                        double num_value = strtod(value, &endptr);
                        if (*endptr == '\0') {
                            lua_pushnumber(L, num_value);
                        } else {
                            lua_pushstring(L, value);
                        }
                        lua_setfield(L, -2, var_name);
                    }
                }
            }
        }
        lua_pop(L, 1); // Pop self table
    }
    
    printf("🔄 SYNC TO LUA: Variable sync complete\n");
}

/**
 * @brief Sync Lua variables back to runtime (variables first approach)
 */
static void sync_variables_from_lua(KryonVM* vm, lua_State* L, const char* component_instance) {
    if (!vm || !vm->runtime || !L) return;
    
    printf("🔄 SYNC FROM LUA: Starting clean variable sync back (%zu variables)\n", vm->runtime->variable_count);
    
    // PHASE 1: Sync global variables first (variables first principle)
    printf("🔄 SYNC FROM LUA: Phase 1 - Global variables\n");
    for (size_t i = 0; i < vm->runtime->variable_count; i++) {
        if (!vm->runtime->variable_names[i]) continue;
        if (!should_sync_variable(vm->runtime->variable_names[i])) continue;
        
        const char* name = vm->runtime->variable_names[i];
        
        if (is_global_variable(name)) {
            lua_getglobal(L, name);
            
            char* new_value = NULL;
            if (lua_istable(L, -1)) {
                printf("🔄 SYNC FROM LUA: Global variable '%s' is a table, converting to JSON\n", name);
                new_value = convert_lua_table_to_json(L, -1);
                if (new_value) {
                    printf("🔄 SYNC FROM LUA: Global variable '%s' = '%s'\n", name, new_value);
                }
            } else if (lua_isboolean(L, -1)) {
                int bool_value = lua_toboolean(L, -1);
                new_value = kryon_strdup(bool_value ? "true" : "false");
                printf("🔄 SYNC FROM LUA: Global variable '%s' = %s\n", name, new_value);
            } else if (lua_isstring(L, -1)) {
                const char* str_value = lua_tostring(L, -1);
                new_value = kryon_strdup(str_value);
                printf("🔄 SYNC FROM LUA: Global variable '%s' = '%s'\n", name, new_value);
            } else if (lua_isnumber(L, -1)) {
                double num_value = lua_tonumber(L, -1);
                char value_str[32];
                snprintf(value_str, sizeof(value_str), "%.0f", num_value);
                new_value = kryon_strdup(value_str);
                printf("🔄 SYNC FROM LUA: Global variable '%s' = %g -> '%s'\n", name, num_value, new_value);
            }
            
            // Update runtime variable if it changed
            if (new_value && (!vm->runtime->variable_values[i] || strcmp(vm->runtime->variable_values[i], new_value) != 0)) {
                kryon_free(vm->runtime->variable_values[i]);
                vm->runtime->variable_values[i] = new_value;
                printf("✅ SYNC FROM LUA: Updated global variable '%s' = '%s'\n", vm->runtime->variable_names[i], new_value);

                // Trigger directive re-processing when variables change
                if (vm->runtime->root && !vm->runtime->is_loading) {
                    // Trigger @for directive re-processing for array variables
                    if (new_value[0] == '[' && new_value[strlen(new_value)-1] == ']') {
                        printf("🔁 SYNC FROM LUA: Array variable '%s' changed, triggering @for re-processing\n", vm->runtime->variable_names[i]);
                        process_for_directives(vm->runtime, vm->runtime->root);
                    }

                    // Trigger @if directive re-processing for any variable (since @if can reference any variable)
                    printf("🔁 SYNC FROM LUA: Variable '%s' changed, triggering @if re-processing\n", vm->runtime->variable_names[i]);
                    process_if_directives(vm->runtime, vm->runtime->root);
                } else if (vm->runtime->is_loading) {
                    printf("🔄 RE-PROCESSING: Deferring directive re-processing until loading complete\n");
                }
            } else if (new_value) {
                printf("🔄 SYNC FROM LUA: Global variable '%s' unchanged, freeing new value\n", name);
                kryon_free(new_value);
            }
            
            lua_pop(L, 1); // Pop the global variable
        }
    }
    
    // PHASE 2: Sync component variables from self table if component_instance provided
    if (component_instance) {
        printf("🔄 SYNC FROM LUA: Phase 2 - Component variables for '%s'\n", component_instance);
        
        lua_getglobal(L, "self");
        if (lua_istable(L, -1)) {
            for (size_t i = 0; i < vm->runtime->variable_count; i++) {
                if (!vm->runtime->variable_names[i]) continue;
                if (!should_sync_variable(vm->runtime->variable_names[i])) continue;
                
                const char* name = vm->runtime->variable_names[i];
                
                if (is_component_variable(name)) {
                    // Check if this variable belongs to the current component instance
                    size_t instance_len = strlen(component_instance);
                    if (strncmp(name, component_instance, instance_len) == 0 && name[instance_len] == '.') {
                        const char* var_name = strrchr(name, '.') + 1;
                        
                        lua_getfield(L, -1, var_name);
                        char* new_value = NULL;
                        
                        if (lua_isnumber(L, -1)) {
                            double num_value = lua_tonumber(L, -1);
                            char value_str[32];
                            snprintf(value_str, sizeof(value_str), "%.0f", num_value);
                            new_value = kryon_strdup(value_str);
                            printf("🔄 SYNC FROM LUA: Component variable '%s' self.%s = %g -> '%s'\n", name, var_name, num_value, new_value);
                        } else if (lua_isstring(L, -1)) {
                            const char* str_value = lua_tostring(L, -1);
                            new_value = kryon_strdup(str_value);
                            printf("🔄 SYNC FROM LUA: Component variable '%s' self.%s = '%s'\n", name, var_name, new_value);
                        }
                        
                        // Update runtime variable if it changed
                        if (new_value && (!vm->runtime->variable_values[i] || strcmp(vm->runtime->variable_values[i], new_value) != 0)) {
                            kryon_free(vm->runtime->variable_values[i]);
                            vm->runtime->variable_values[i] = new_value;
                            printf("✅ SYNC FROM LUA: Updated component variable '%s' = '%s'\n", vm->runtime->variable_names[i], new_value);
                        } else if (new_value) {
                            printf("🔄 SYNC FROM LUA: Component variable '%s' unchanged, freeing new value\n", name);
                            kryon_free(new_value);
                        }
                        
                        lua_pop(L, 1); // Pop the field value
                    }
                }
            }
        }
        lua_pop(L, 1); // Pop self table
    }
    
    printf("🔄 SYNC FROM LUA: Variable sync back complete\n");
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
    
    // Sync runtime variables to Lua globals before function execution (variables first)
    sync_variables_to_lua(vm, backend->L, component_instance);
    
    int result = lua_pcall(backend->L, 0, 0, 0);
    
    // Update component state after function call
    if (result == LUA_OK) {
        update_component_state_after_call(backend->L, element);
        
        // Sync Lua variables back to runtime after function execution (variables first)
        sync_variables_from_lua(vm, backend->L, component_instance);
    }
    
    // Clean up allocated memory
    if (component_instance) {
        free((void*)component_instance);
    }
    
    if (result != LUA_OK) {
        const char* error = lua_tostring(backend->L, -1);
        printf("❌ LUA VM: Lua error: %s\n", error);
        snprintf(backend->error_buffer, sizeof(backend->error_buffer), "Lua error: %s", error);
        lua_pop(backend->L, 1);
        return KRYON_VM_ERROR_RUNTIME;
    }
    
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