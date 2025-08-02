/**
 * @file vm_core.c
 * @brief Kryon Script VM Core Implementation
 * 
 * Main implementation of the VM abstraction layer
 */

#include "internal/script_vm.h"
#include "internal/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Backend interface declarations
#ifdef HAVE_LUA
extern KryonVMInterface* kryon_lua_vm_interface(void);
#endif

#ifdef HAVE_JAVASCRIPT
extern KryonVMInterface* kryon_js_vm_interface(void);
#endif

#ifdef HAVE_PYTHON
extern KryonVMInterface* kryon_python_vm_interface(void);
#endif

// =============================================================================
// FACTORY FUNCTIONS
// =============================================================================

KryonVM* kryon_vm_create(KryonVMType type, const KryonVMConfig* config) {
    KryonVMInterface* interface = NULL;
    
    // Get the appropriate interface
    switch (type) {
#ifdef HAVE_LUA
        case KRYON_VM_LUA:
            interface = kryon_lua_vm_interface();
            break;
#endif
#ifdef HAVE_JAVASCRIPT
        case KRYON_VM_JAVASCRIPT:
            interface = kryon_js_vm_interface();
            break;
#endif
#ifdef HAVE_PYTHON
        case KRYON_VM_PYTHON:
            interface = kryon_python_vm_interface();
            break;
#endif
        default:
            printf("❌ VM type %s not supported or not compiled in\n", kryon_vm_type_name(type));
            return NULL;
    }
    
    if (!interface) {
        printf("❌ Failed to get VM interface for %s\n", kryon_vm_type_name(type));
        return NULL;
    }
    
    // Create VM structure
    KryonVM* vm = kryon_alloc(sizeof(KryonVM));
    if (!vm) {
        return NULL;
    }
    
    vm->type = type;
    vm->vtable = interface;
    vm->impl_data = NULL;
    vm->initialized = false;
    vm->last_error = NULL;
    
    // Set configuration
    if (config) {
        vm->config = *config;
    } else {
        vm->config = kryon_vm_default_config(type);
    }
    
    // Initialize backend
    KryonVMResult result = interface->initialize(vm, &vm->config);
    if (result != KRYON_VM_SUCCESS) {
        printf("❌ Failed to initialize %s VM: %d\n", kryon_vm_type_name(type), result);
        kryon_free(vm);
        return NULL;
    }
    
    vm->initialized = true;
    printf("✅ Created %s VM successfully\n", kryon_vm_type_name(type));
    return vm;
}

void kryon_vm_destroy(KryonVM* vm) {
    if (!vm) return;
    
    if (vm->initialized && vm->vtable && vm->vtable->destroy) {
        vm->vtable->destroy(vm);
    }
    
    if (vm->last_error) {
        kryon_free(vm->last_error);
    }
    
    kryon_free(vm);
}

KryonVMConfig kryon_vm_default_config(KryonVMType type) {
    KryonVMConfig config = {
        .type = type,
        .enable_debug = false,
        .memory_limit = 1024 * 1024,  // 1MB
        .time_limit = 5.0,            // 5 seconds
        .enable_jit = false,
        .module_path = NULL
    };
    
    // Type-specific defaults
    switch (type) {
        case KRYON_VM_LUA:
            config.memory_limit = 512 * 1024;  // 512KB for Lua
            break;
        case KRYON_VM_JAVASCRIPT:
            config.memory_limit = 2 * 1024 * 1024;  // 2MB for JS
            config.enable_jit = true;
            break;
        case KRYON_VM_PYTHON:
            config.memory_limit = 4 * 1024 * 1024;  // 4MB for Python
            break;
        default:
            break;
    }
    
    return config;
}

// =============================================================================
// GENERIC VM OPERATIONS
// =============================================================================

KryonVMResult kryon_vm_compile(KryonVM* vm, const char* source, const char* source_name, KryonScript* out_script) {
    if (!vm || !vm->initialized || !vm->vtable || !vm->vtable->compile) {
        return KRYON_VM_ERROR_INVALID_PARAM;
    }
    
    return vm->vtable->compile(vm, source, source_name, out_script);
}

KryonVMResult kryon_vm_load_script(KryonVM* vm, const KryonScript* script) {
    if (!vm || !vm->initialized || !vm->vtable || !vm->vtable->load_script) {
        return KRYON_VM_ERROR_INVALID_PARAM;
    }
    
    return vm->vtable->load_script(vm, script);
}

KryonVMResult kryon_vm_call_function(KryonVM* vm, const char* function_name, KryonElement* element, const struct KryonEvent* event) {
    if (!vm || !vm->initialized || !vm->vtable || !vm->vtable->call_function) {
        return KRYON_VM_ERROR_INVALID_PARAM;
    }
    
    return vm->vtable->call_function(vm, function_name, element, event);
}

KryonVMResult kryon_vm_execute_string(KryonVM* vm, const char* code) {
    if (!vm || !vm->initialized || !vm->vtable || !vm->vtable->execute_string) {
        return KRYON_VM_ERROR_INVALID_PARAM;
    }
    
    return vm->vtable->execute_string(vm, code);
}

const char* kryon_vm_get_error(KryonVM* vm) {
    if (!vm || !vm->vtable || !vm->vtable->get_last_error) {
        return "Invalid VM or no error interface";
    }
    
    return vm->vtable->get_last_error(vm);
}

// =============================================================================
// SCRIPT MANAGEMENT
// =============================================================================

KryonScript kryon_script_create(KryonVMType vm_type) {
    KryonScript script = {
        .vm_type = vm_type,
        .format = KRYON_SCRIPT_SOURCE,
        .data = NULL,
        .size = 0,
        .source_name = NULL,
        .entry_point = NULL
    };
    return script;
}

void kryon_script_free(KryonScript* script) {
    if (!script) return;
    
    if (script->data) {
        kryon_free(script->data);
        script->data = NULL;
    }
    
    if (script->source_name) {
        kryon_free(script->source_name);
        script->source_name = NULL;
    }
    
    if (script->entry_point) {
        kryon_free(script->entry_point);
        script->entry_point = NULL;
    }
    
    script->size = 0;
}

KryonVMResult kryon_script_clone(const KryonScript* src, KryonScript* dst) {
    if (!src || !dst) {
        return KRYON_VM_ERROR_INVALID_PARAM;
    }
    
    // Initialize destination
    *dst = kryon_script_create(src->vm_type);
    dst->format = src->format;
    
    // Clone data
    if (src->data && src->size > 0) {
        dst->data = kryon_alloc(src->size);
        if (!dst->data) {
            return KRYON_VM_ERROR_MEMORY;
        }
        memcpy(dst->data, src->data, src->size);
        dst->size = src->size;
    }
    
    // Clone strings
    if (src->source_name) {
        dst->source_name = strdup(src->source_name);
        if (!dst->source_name) {
            kryon_script_free(dst);
            return KRYON_VM_ERROR_MEMORY;
        }
    }
    
    if (src->entry_point) {
        dst->entry_point = strdup(src->entry_point);
        if (!dst->entry_point) {
            kryon_script_free(dst);
            return KRYON_VM_ERROR_MEMORY;
        }
    }
    
    return KRYON_VM_SUCCESS;
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

const char* kryon_vm_type_name(KryonVMType type) {
    switch (type) {
        case KRYON_VM_NONE: return "None";
        case KRYON_VM_LUA: return "Lua";
        case KRYON_VM_JAVASCRIPT: return "JavaScript";
        case KRYON_VM_PYTHON: return "Python";
        case KRYON_VM_WASM: return "WebAssembly";
        default: return "Unknown";
    }
}

bool kryon_vm_is_available(KryonVMType type) {
    switch (type) {
#ifdef HAVE_LUA
        case KRYON_VM_LUA:
            return true;
#endif
#ifdef HAVE_JAVASCRIPT
        case KRYON_VM_JAVASCRIPT:
            return true;
#endif
#ifdef HAVE_PYTHON
        case KRYON_VM_PYTHON:
            return true;
#endif
        default:
            return false;
    }
}

size_t kryon_vm_list_available(KryonVMType* out_types, size_t max_types) {
    size_t count = 0;
    
    if (!out_types || max_types == 0) {
        return 0;
    }
    
#ifdef HAVE_LUA
    if (count < max_types) {
        out_types[count++] = KRYON_VM_LUA;
    }
#endif

#ifdef HAVE_JAVASCRIPT
    if (count < max_types) {
        out_types[count++] = KRYON_VM_JAVASCRIPT;
    }
#endif

#ifdef HAVE_PYTHON
    if (count < max_types) {
        out_types[count++] = KRYON_VM_PYTHON;
    }
#endif
    
    return count;
}