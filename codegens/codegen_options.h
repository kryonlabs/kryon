/**
 * @file codegen_options.h
 * @brief Unified Codegen Options Structures
 *
 * Standardizes options handling across all codegens to eliminate inconsistency.
 * Provides a base structure with common options and language-specific extensions.
 *
 * Before: Each codegen defines its own options with different fields
 *   C:           include_comments, generate_types, include_headers
 *   Limbo:       include_comments, generate_types, module_name, modules
 *   Lua:         format, optimize
 *
 * After: All codegens use unified base + language-specific extensions
 *   All:         CodegenOptionsBase (include_comments, verbose, indent_size)
 *   C:           CCodegenOptions (base + generate_types, include_headers)
 *   Limbo:       LimboCodegenOptions (base + generate_types, module_name)
 *   Lua:         LuaCodegenOptions (base + format, optimize)
 */

#ifndef CODEGEN_OPTIONS_H
#define CODEGEN_OPTIONS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Base Options Structure
 * ============================================================================ */

/**
 * @brief Base options shared by all codegens
 *
 * These options are available across all code generation backends.
 */
typedef struct {
    bool include_comments;    /**< Include comments in generated code */
    bool verbose;             /**< Enable verbose output */
    int indent_size;          /**< Indentation size (default: 4) */
    char* indent_string;      /**< Custom indent string (NULL for default) */
} CodegenOptionsBase;

/* ============================================================================
 * Language-Specific Options
 * ============================================================================ */

/**
 * @brief C codegen options
 *
 * Extends base options with C-specific settings.
 */
typedef struct {
    CodegenOptionsBase base;
    bool generate_types;      /**< Generate type definitions */
    bool include_headers;     /**< Include standard headers */
    bool generate_main;       /**< Generate main() function */
} CCodegenOptions;

/**
 * @brief Limbo codegen options
 *
 * Extends base options with Limbo-specific settings.
 */
typedef struct {
    CodegenOptionsBase base;
    bool generate_types;      /**< Generate type definitions */
    char* module_name;        /**< Module/namespace name */
} LimboCodegenOptions;

/**
 * @brief Lua codegen options
 *
 * Extends base options with Lua-specific settings.
 */
typedef struct {
    CodegenOptionsBase base;
    bool format;              /**< Format generated code */
    bool optimize;            /**< Enable optimizations */
} LuaCodegenOptions;

/**
 * @brief Markdown codegen options
 *
 * Extends base options with Markdown-specific settings.
 */
typedef struct {
    CodegenOptionsBase base;
    bool include_toc;         /**< Include table of contents */
    bool include_metadata;    /**< Include frontmatter metadata */
} MarkdownCodegenOptions;

/**
 * @brief Tcl/Tk codegen options
 *
 * Extends base options with Tcl/Tk-specific settings.
 */
typedef struct {
    CodegenOptionsBase base;
    bool use_tk_namespace;    /**< Use ::tk namespace */
    bool pack_geometry;       /**< Use pack geometry manager */
} TclCodegenOptions;

/* ============================================================================
 * Options Parsing
 * ============================================================================ */

/**
 * @brief Initialize base options to defaults
 *
 * Sets all options to sensible defaults:
 * - include_comments: true
 * - verbose: false
 * - indent_size: 4
 * - indent_string: NULL (use spaces based on indent_size)
 *
 * @param opts Base options structure to initialize
 */
void codegen_options_init_base(CodegenOptionsBase* opts);

/**
 * @brief Parse options from command-line style arguments
 *
 * Supports common options:
 *   --comments         Enable comments (default: true)
 *   --no-comments      Disable comments
 *   --verbose          Enable verbose output
 *   --indent=N         Set indent size (default: 4)
 *   --indent=spaces    Use spaces for indentation
 *   --indent=tabs      Use tabs for indentation
 *
 * @param opts Base options structure to populate
 * @param argc Argument count
 * @param argv Argument values
 * @return true on success, false on error (unknown option)
 */
bool codegen_options_parse_base(CodegenOptionsBase* opts, int argc, char** argv);

/**
 * @brief Free options resources
 *
 * Frees any dynamically allocated memory in the options structure.
 * Safe to call on stack-allocated structures.
 *
 * @param opts Base options structure to free
 */
void codegen_options_free_base(CodegenOptionsBase* opts);

/* ============================================================================
 * Language-Specific Initialization
 * ============================================================================ */

/**
 * @brief Initialize C codegen options to defaults
 */
void codegen_options_init_c(CCodegenOptions* opts);

/**
 * @brief Initialize Limbo codegen options to defaults
 */
void codegen_options_init_limbo(LimboCodegenOptions* opts);

/**
 * @brief Initialize Lua codegen options to defaults
 */
void codegen_options_init_lua(LuaCodegenOptions* opts);

/**
 * @brief Initialize Markdown codegen options to defaults
 */
void codegen_options_init_markdown(MarkdownCodegenOptions* opts);

/**
 * @brief Initialize Tcl codegen options to defaults
 */
void codegen_options_init_tcl(TclCodegenOptions* opts);

/* ============================================================================
 * Language-Specific Cleanup
 * ============================================================================ */

/**
 * @brief Free C codegen options
 */
void codegen_options_free_c(CCodegenOptions* opts);

/**
 * @brief Free Limbo codegen options
 */
void codegen_options_free_limbo(LimboCodegenOptions* opts);

/**
 * @brief Free Lua codegen options
 */
void codegen_options_free_lua(LuaCodegenOptions* opts);

/**
 * @brief Free Markdown codegen options
 */
void codegen_options_free_markdown(MarkdownCodegenOptions* opts);

/**
 * @brief Free Tcl codegen options
 */
void codegen_options_free_tcl(TclCodegenOptions* opts);

#ifdef __cplusplus
}
#endif

#endif // CODEGEN_OPTIONS_H
