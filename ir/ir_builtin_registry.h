#ifndef IR_BUILTIN_REGISTRY_H
#define IR_BUILTIN_REGISTRY_H

#include "ir_expression_compiler.h"
#include <stdint.h>
#include <stdbool.h>

// Forward declaration
typedef struct IREvalContext IREvalContext;

// ============================================================================
// BUILTIN FUNCTION TYPES
// ============================================================================

// Builtin function signature
typedef IRValue (*IRBuiltinFunc)(IREvalContext* ctx, IRValue* args, uint32_t arg_count);

// Builtin function metadata
typedef struct IRBuiltinDef {
    const char* name;           // Function name
    const char* signature;      // Signature string (e.g., "(str: string) -> string")
    const char* description;    // Human-readable description
    IRBuiltinFunc func;         // Function pointer

    uint32_t min_args;          // Minimum argument count
    uint32_t max_args;          // Maximum argument count (0 = unlimited)

    VarType return_type;        // Expected return type
    bool is_pure;               // Pure function (no side effects)

    // Hash table chain (for internal registry use)
    struct IRBuiltinDef* next_in_bucket;
} IRBuiltinDef;

// Builtin registry
typedef struct {
    IRBuiltinDef* builtins;      // Array of builtin definitions
    uint32_t count;             // Number of builtins
    uint32_t capacity;          // Allocated capacity

    // Hash table for fast lookup
    IRBuiltinDef** buckets;     // Hash table buckets
    uint32_t bucket_count;      // Number of buckets (power of 2)
} IRBuiltinRegistry;

// ============================================================================
// REGISTRY LIFECYCLE
// ============================================================================

// Create builtin registry with all standard functions
IRBuiltinRegistry* ir_builtin_registry_create(void);

// Destroy registry and free all builtins
void ir_builtin_registry_destroy(IRBuiltinRegistry* registry);

// ============================================================================
// BUILTIN REGISTRATION
// ============================================================================

// Register a builtin function
void ir_builtin_register(IRBuiltinRegistry* registry, const char* name,
                        IRBuiltinFunc func, const char* signature,
                        const char* description, uint32_t min_args, uint32_t max_args,
                        VarType return_type, bool is_pure);

// Lookup builtin by name
IRBuiltinDef* ir_builtin_lookup(IRBuiltinRegistry* registry, const char* name);

// Call builtin function by name
IRValue ir_builtin_call(IRBuiltinRegistry* registry, const char* name,
                       IREvalContext* ctx, IRValue* args, uint32_t arg_count);

// ============================================================================
// GLOBAL REGISTRY
// ============================================================================

// Initialize global builtin registry
void ir_builtin_global_init(void);

// Get global builtin registry
IRBuiltinRegistry* ir_builtin_global_get(void);

// Cleanup global builtin registry
void ir_builtin_global_cleanup(void);

// Check if global registry is initialized
bool ir_builtin_global_is_initialized(void);

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Convert value to string representation (for builtin implementations)
char* ir_value_to_string_alloc(const IRValue* value);

// Compare two values for equality (for builtin implementations)
bool ir_value_equals_builtin(const IRValue* a, const IRValue* b);

#endif // IR_BUILTIN_REGISTRY_H
