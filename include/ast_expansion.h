/**
 * @file ast_expansion.h
 * @brief AST Expansion Pass - Standalone phase for expanding components and directives
 *
 * This module provides the expansion phase that transforms a parsed AST into
 * a post-expansion AST ready for code generation or KIR serialization.
 *
 * Expansions performed:
 * - Component instances → Inline expansion with parameter substitution
 * - Component inheritance → Resolved parent-child merging
 * - @include directives → File content inlining
 *
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_AST_EXPANSION_H
#define KRYON_AST_EXPANSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include "parser.h"

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonExpansionContext KryonExpansionContext;

// =============================================================================
// EXPANSION CONFIGURATION
// =============================================================================

/**
 * @brief Expansion configuration options
 */
typedef struct {
    bool expand_components;         // Expand component instances
    bool expand_includes;           // Process @include directives
    bool resolve_inheritance;       // Resolve component inheritance
    bool preserve_definitions;      // Keep component definitions in output
    bool add_expansion_metadata;    // Add metadata about expansions
    bool validate_before_expand;    // Validate AST before expansion
    bool validate_after_expand;     // Validate AST after expansion
    size_t max_expansion_depth;     // Maximum recursion depth (default: 32)
} KryonExpansionConfig;

// =============================================================================
// EXPANSION STATISTICS
// =============================================================================

/**
 * @brief Expansion statistics
 */
typedef struct {
    size_t components_expanded;      // Number of component instances expanded
    size_t includes_processed;       // Number of @include directives processed
    size_t inheritance_resolved;     // Number of component inheritances resolved
    size_t nodes_created;            // Total new nodes created during expansion
    size_t nodes_removed;            // Nodes removed during expansion
    double expansion_time;           // Time spent expanding (seconds)
} KryonExpansionStats;

// =============================================================================
// EXPANSION CONTEXT
// =============================================================================

/**
 * @brief Create expansion context
 * @param config Expansion configuration (NULL for defaults)
 * @return Pointer to expansion context, or NULL on failure
 */
KryonExpansionContext *kryon_expansion_create(const KryonExpansionConfig *config);

/**
 * @brief Destroy expansion context
 * @param context Expansion context to destroy
 */
void kryon_expansion_destroy(KryonExpansionContext *context);

// =============================================================================
// EXPANSION API
// =============================================================================

/**
 * @brief Expand AST (main entry point for expansion phase)
 *
 * This function performs all configured expansions on the input AST,
 * producing a post-expansion AST suitable for code generation.
 *
 * @param context Expansion context
 * @param ast_root Root AST node (pre-expansion)
 * @param out_expanded Output pointer for expanded AST
 * @return true on success, false on error
 *
 * @note The input AST is not modified. A new AST is created.
 * @note Caller is responsible for freeing the expanded AST.
 */
bool kryon_ast_expand(KryonExpansionContext *context,
                      const KryonASTNode *ast_root,
                      KryonASTNode **out_expanded);

/**
 * @brief Expand AST in-place (modifies input AST)
 *
 * @param context Expansion context
 * @param ast_root Root AST node (will be modified)
 * @return true on success, false on error
 *
 * @warning This modifies the input AST. Use kryon_ast_expand() for non-destructive expansion.
 */
bool kryon_ast_expand_inplace(KryonExpansionContext *context,
                              KryonASTNode *ast_root);

/**
 * @brief Get expansion errors
 * @param context Expansion context
 * @param out_count Output for error count
 * @return Array of error messages
 */
const char **kryon_expansion_get_errors(const KryonExpansionContext *context,
                                        size_t *out_count);

/**
 * @brief Get expansion statistics
 * @param context Expansion context
 * @return Pointer to statistics structure
 */
const KryonExpansionStats *kryon_expansion_get_stats(const KryonExpansionContext *context);

// =============================================================================
// EXPANSION UTILITIES
// =============================================================================

/**
 * @brief Find component definition by name
 * @param ast_root Root AST node to search
 * @param component_name Component name to find
 * @return Component definition node, or NULL if not found
 */
KryonASTNode *kryon_expansion_find_component(const KryonASTNode *ast_root,
                                             const char *component_name);

/**
 * @brief Resolve component inheritance chain
 * @param component_def Component definition node
 * @param ast_root Root AST node (for parent lookup)
 * @return Resolved component with merged parent properties
 */
KryonASTNode *kryon_expansion_resolve_inheritance(const KryonASTNode *component_def,
                                                  const KryonASTNode *ast_root);

/**
 * @brief Expand single component instance
 * @param context Expansion context
 * @param component_element Component element node (e.g., "Counter {}")
 * @param ast_root Root AST node (for definition lookup)
 * @return Expanded element(s), or NULL on error
 */
KryonASTNode *kryon_expansion_expand_component(KryonExpansionContext *context,
                                               const KryonASTNode *component_element,
                                               const KryonASTNode *ast_root);

// =============================================================================
// CONFIGURATION HELPERS
// =============================================================================

/**
 * @brief Get default expansion configuration
 * @return Default configuration
 */
KryonExpansionConfig kryon_expansion_default_config(void);

/**
 * @brief Get full expansion configuration (expand everything)
 * @return Configuration with all expansions enabled
 */
KryonExpansionConfig kryon_expansion_full_config(void);

/**
 * @brief Get minimal expansion configuration (minimal expansions)
 * @return Configuration with only required expansions
 */
KryonExpansionConfig kryon_expansion_minimal_config(void);

/**
 * @brief Get KIR expansion configuration (optimized for KIR output)
 * @return Configuration suitable for KIR generation
 */
KryonExpansionConfig kryon_expansion_kir_config(void);

// =============================================================================
// VALIDATION
// =============================================================================

/**
 * @brief Validate expansion configuration
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
bool kryon_expansion_validate_config(const KryonExpansionConfig *config);

/**
 * @brief Check if AST needs expansion
 * @param ast_root Root AST node
 * @return true if AST contains expandable constructs
 */
bool kryon_expansion_needs_expansion(const KryonASTNode *ast_root);

/**
 * @brief Check if AST is already expanded
 * @param ast_root Root AST node
 * @return true if AST appears to be post-expansion
 */
bool kryon_expansion_is_expanded(const KryonASTNode *ast_root);

#ifdef __cplusplus
}
#endif

#endif // KRYON_AST_EXPANSION_H
