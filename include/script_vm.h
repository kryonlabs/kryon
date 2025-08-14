/**
 * @file script_vm.h
 * @brief Kryon Script VM Abstraction Layer
 * 
 * Generic interface for multiple virtual machine backends (Lua, JavaScript, Python, etc.)
 */

#ifndef KRYON_INTERNAL_SCRIPT_VM_H
#define KRYON_INTERNAL_SCRIPT_VM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "elements.h"

// Forward declarations
typedef struct KryonVM KryonVM;
typedef struct KryonVMConfig KryonVMConfig;
typedef struct KryonScript KryonScript;

// Include events.h to get KryonEvent definition
#include "events.h"

// =============================================================================
// VM TYPES AND ENUMS
// =============================================================================

/**
 * @brief Supported VM backends
 */
typedef enum {
    KRYON_VM_NONE = 0,
    KRYON_VM_LUA,
    KRYON_VM_JAVASCRIPT,
    KRYON_VM_PYTHON,
    KRYON_VM_WASM
} KryonVMType;

/**
 * @brief VM operation results
 */
typedef enum {
    KRYON_VM_SUCCESS = 0,
    KRYON_VM_ERROR_COMPILE,
    KRYON_VM_ERROR_RUNTIME,
    KRYON_VM_ERROR_MEMORY,
    KRYON_VM_ERROR_TIMEOUT,
    KRYON_VM_ERROR_INVALID_PARAM,
    KRYON_VM_ERROR_NOT_SUPPORTED,
    KRYON_VM_ERROR_BACKEND_INIT_FAILED
} KryonVMResult;

/**
 * @brief Script compilation target format
 */
typedef enum {
    KRYON_SCRIPT_SOURCE = 0,    // Keep as source code
    KRYON_SCRIPT_BYTECODE,      // Compile to bytecode
    KRYON_SCRIPT_JIT            // Just-in-time compilation
} KryonScriptFormat;

// =============================================================================
// CONFIGURATION STRUCTURES
// =============================================================================

/**
 * @brief VM configuration
 */
struct KryonVMConfig {
    KryonVMType type;           // VM backend type
    bool enable_debug;          // Enable debug features
    size_t memory_limit;        // Memory limit in bytes (0 = unlimited)
    double time_limit;          // Execution time limit in seconds (0 = unlimited)
    bool enable_jit;            // Enable JIT compilation (if supported)
    const char* module_path;    // Path to additional modules/libraries
};

/**
 * @brief Compiled script structure
 */
struct KryonScript {
    KryonVMType vm_type;        // VM type this script is compiled for
    KryonScriptFormat format;   // Script format (source/bytecode/jit)
    uint8_t* data;              // Script data
    size_t size;                // Size of script data
    char* source_name;          // Source identifier
    char* entry_point;          // Main function name (optional)
};

// =============================================================================
// VM INTERFACE (VTABLE PATTERN)
// =============================================================================

/**
 * @brief VM interface function table
 */
typedef struct KryonVMInterface {
    // VM Lifecycle
    KryonVMResult (*initialize)(KryonVM* vm, const KryonVMConfig* config);
    void (*destroy)(KryonVM* vm);
    
    // Script Compilation
    KryonVMResult (*compile)(KryonVM* vm, const char* source, const char* source_name, KryonScript* out_script);
    KryonVMResult (*load_script)(KryonVM* vm, const KryonScript* script);
    
    // Script Execution
    KryonVMResult (*call_function)(KryonVM* vm, const char* function_name, KryonElement* element, const KryonEvent* event);
    KryonVMResult (*execute_string)(KryonVM* vm, const char* code);
    
    // State Management
    KryonVMResult (*get_global)(KryonVM* vm, const char* name, void* out_value, size_t* out_size);
    KryonVMResult (*set_global)(KryonVM* vm, const char* name, const void* value, size_t size);
    
    // Introspection
    const char* (*get_version)(KryonVM* vm);
    bool (*supports_feature)(KryonVM* vm, const char* feature);
    
    // Error Handling
    const char* (*get_last_error)(KryonVM* vm);
    void (*clear_error)(KryonVM* vm);

    // Element Lifecycle
    void (*notify_element_destroyed)(KryonVM* vm, KryonElement* element);
    
} KryonVMInterface;

/**
 * @brief Generic VM structure
 */
struct KryonVM {
    KryonVMType type;           // VM backend type
    KryonVMInterface* vtable;   // Function table
    void* impl_data;            // Backend-specific data
    KryonVMConfig config;       // VM configuration
    bool initialized;           // Initialization flag
    char* last_error;           // Last error message
};

// =============================================================================
// FACTORY FUNCTIONS
// =============================================================================

/**
 * @brief Create VM instance
 * @param type VM backend type
 * @param config VM configuration (NULL for defaults)
 * @return VM instance or NULL on failure
 */
KryonVM* kryon_vm_create(KryonVMType type, const KryonVMConfig* config);

/**
 * @brief Destroy VM instance
 * @param vm VM instance
 */
void kryon_vm_destroy(KryonVM* vm);

/**
 * @brief Get default configuration for VM type
 * @param type VM backend type
 * @return Default configuration
 */
KryonVMConfig kryon_vm_default_config(KryonVMType type);

// =============================================================================
// GENERIC VM OPERATIONS
// =============================================================================

/**
 * @brief Compile source code to script
 * @param vm VM instance
 * @param source Source code
 * @param source_name Source identifier
 * @param out_script Output script structure
 * @return KRYON_VM_SUCCESS on success
 */
KryonVMResult kryon_vm_compile(KryonVM* vm, const char* source, const char* source_name, KryonScript* out_script);

/**
 * @brief Load compiled script
 * @param vm VM instance
 * @param script Compiled script
 * @return KRYON_VM_SUCCESS on success
 */
KryonVMResult kryon_vm_load_script(KryonVM* vm, const KryonScript* script);

/**
 * @brief Call function with element and event context
 * @param vm VM instance
 * @param function_name Function name
 * @param element Element context (optional)
 * @param event Event context (optional)
 * @return KRYON_VM_SUCCESS on success
 */
KryonVMResult kryon_vm_call_function(KryonVM* vm, const char* function_name, KryonElement* element, const KryonEvent* event);

/**
 * @brief Execute code string directly
 * @param vm VM instance
 * @param code Code to execute
 * @return KRYON_VM_SUCCESS on success
 */
KryonVMResult kryon_vm_execute_string(KryonVM* vm, const char* code);

/**
 * @brief Get last error message
 * @param vm VM instance
 * @return Error message or NULL
 */
const char* kryon_vm_get_error(KryonVM* vm);

/**
 * @brief Notify the VM that a C-side element has been destroyed.
 * 
 * This allows the VM to invalidate any script-side references (e.g., userdata)
 * to prevent use-after-free errors.
 * 
 * @param vm The VM instance.
 * @param element The element that is being destroyed.
 */
void kryon_vm_notify_element_destroyed(KryonVM* vm, KryonElement* element);

// =============================================================================
// SCRIPT MANAGEMENT
// =============================================================================

/**
 * @brief Create empty script structure
 * @param vm_type Target VM type
 * @return Script structure
 */
KryonScript kryon_script_create(KryonVMType vm_type);

/**
 * @brief Free script resources
 * @param script Script to free
 */
void kryon_script_free(KryonScript* script);

/**
 * @brief Clone script
 * @param src Source script
 * @param dst Destination script
 * @return KRYON_VM_SUCCESS on success
 */
KryonVMResult kryon_script_clone(const KryonScript* src, KryonScript* dst);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Get VM type name
 * @param type VM type
 * @return Type name string
 */
const char* kryon_vm_type_name(KryonVMType type);

/**
 * @brief Check if VM type is available
 * @param type VM type
 * @return true if available
 */
bool kryon_vm_is_available(KryonVMType type);

/**
 * @brief List available VM types
 * @param out_types Output array of available types
 * @param max_types Maximum number of types to return
 * @return Number of available types
 */
size_t kryon_vm_list_available(KryonVMType* out_types, size_t max_types);

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_SCRIPT_VM_H