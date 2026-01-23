/**
 * KRY Arrow Function Registry
 *
 * Manages collection and code generation of arrow functions for C target.
 * C has no closures, so arrow functions are compiled to:
 *   1. Static functions (at file scope)
 *   2. Context structs (for captured variables)
 *   3. Function pointer + context pairs
 */

#ifndef KRY_ARROW_REGISTRY_H
#define KRY_ARROW_REGISTRY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration from kry_expression.h
struct KryExprNode;

/**
 * Arrow function definition stored in registry
 */
typedef struct KryArrowDef {
    uint32_t id;                    // Unique ID for this arrow function
    char* name;                     // Generated name: "kry_arrow_N"
    char** params;                  // Parameter names
    size_t param_count;             // Number of parameters
    char* body_code;                // Transpiled body expression (C code)
    char** captured_vars;           // Variables captured from outer scope
    size_t captured_count;          // Number of captured variables
    const char* context_hint;       // Optional context hint: "click", "map", etc.
    bool has_return_value;          // Whether body returns a value
    struct KryArrowDef* next;       // Next in linked list
} KryArrowDef;

/**
 * Registry for collecting arrow functions during transpilation
 */
typedef struct KryArrowRegistry {
    KryArrowDef* head;              // Head of linked list
    KryArrowDef* tail;              // Tail for efficient append
    uint32_t next_id;               // Next ID to assign
    size_t count;                   // Total number of registered arrows
} KryArrowRegistry;

/**
 * Callback struct for arrow functions with captures
 * Use this type at call sites when passing closures to event handlers
 */
typedef struct KryCallback {
    void (*func)(void* ctx);        // Function pointer
    void* ctx;                      // Context (captured variables)
} KryCallback;

// ============================================================================
// Registry API
// ============================================================================

/**
 * Create a new arrow function registry
 * @return New registry (caller must free with kry_arrow_registry_free)
 */
KryArrowRegistry* kry_arrow_registry_create(void);

/**
 * Free an arrow function registry and all definitions
 * @param reg Registry to free (can be NULL)
 */
void kry_arrow_registry_free(KryArrowRegistry* reg);

/**
 * Register an arrow function
 *
 * @param reg Registry to add to
 * @param params Parameter names (copied)
 * @param param_count Number of parameters
 * @param body_code Transpiled body C code (copied)
 * @param captured_vars Variables captured from outer scope (copied)
 * @param captured_count Number of captured variables
 * @param context_hint Optional context hint for naming
 * @param has_return_value Whether the body expression returns a value
 * @return Assigned ID
 */
uint32_t kry_arrow_register(
    KryArrowRegistry* reg,
    const char** params,
    size_t param_count,
    const char* body_code,
    const char** captured_vars,
    size_t captured_count,
    const char* context_hint,
    bool has_return_value
);

/**
 * Get an arrow function definition by ID
 *
 * @param reg Registry to search
 * @param id ID to find
 * @return Definition or NULL if not found
 */
KryArrowDef* kry_arrow_get(KryArrowRegistry* reg, uint32_t id);

/**
 * Generate C code for all registered arrow functions
 *
 * This generates:
 *   1. Context structs for arrows with captures
 *   2. Static function definitions
 *
 * @param reg Registry containing arrow functions
 * @return Generated C code (caller must free), or NULL on error
 */
char* kry_arrow_generate_stubs(KryArrowRegistry* reg);

/**
 * Generate context initialization code for a specific arrow
 *
 * Example output for arrow with captures {habit, index}:
 *   kry_ctx_0 _ctx_0 = {.habit = habit, .index = index}
 *
 * @param reg Registry
 * @param id Arrow function ID
 * @return Initialization code (caller must free), or NULL if no captures
 */
char* kry_arrow_generate_ctx_init(KryArrowRegistry* reg, uint32_t id);

// ============================================================================
// Capture Analysis API
// ============================================================================

/**
 * Collect all identifiers used in an expression
 *
 * @param node Expression AST node
 * @param ids Output array of identifier names (caller frees each + array)
 * @param count Output number of identifiers
 */
void kry_expr_collect_identifiers(
    struct KryExprNode* node,
    char*** ids,
    size_t* count
);

/**
 * Get captured variables for an arrow function
 * Captured = identifiers in body - parameters
 *
 * @param arrow Arrow function AST node
 * @param captures Output array of captured variable names
 * @param count Output number of captured variables
 */
void kry_arrow_get_captures(
    struct KryExprNode* arrow,
    char*** captures,
    size_t* count
);

#ifdef __cplusplus
}
#endif

#endif // KRY_ARROW_REGISTRY_H
