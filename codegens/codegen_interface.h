/**
 * @file codegen_interface.h
 * @brief Unified Codegen Interface for Kryon Code Generation System
 *
 * Defines a standard interface that all codegens must implement.
 * This ensures consistent API patterns across all backends.
 *
 * Standard Codegen Pattern:
 * 1. All codegens take KIR file path as input
 * 2. All codegens produce output to a directory
 * 3. Consistent error reporting to stderr
 * 4. Boolean return (true = success, false = failure)
 */

#ifndef CODEGEN_INTERFACE_H
#define CODEGEN_INTERFACE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Codegen Interface
 * ============================================================================ */

/**
 * @brief Codegen metadata
 */
typedef struct {
    const char* name;           // Codegen name (e.g., "C", "Limbo", "Tcl/Tk")
    const char* version;        // Codegen version
    const char* description;    // Brief description
    const char** file_extensions; // Output file extensions (e.g., [".c", ".h", NULL])
} CodegenInfo;

/**
 * @brief Codegen interface vtable
 *
 * All codegens must implement these functions following the standard pattern.
 */
typedef struct {
    /**
     * Get codegen metadata
     * @return Codegen info (static, do not free)
     */
    const CodegenInfo* (*get_info)(void);

    /**
     * Generate code from KIR file
     * @param kir_path Path to KIR file
     * @param output_path Output directory or file path
     * @return true on success, false on failure
     *
     * Standard error handling:
     * - File not found: fprintf(stderr, "Error: Cannot open KIR file: %s\n", kir_path)
     * - Parse error: fprintf(stderr, "Error: Failed to parse KIR: <reason>\n")
     * - Generation error: fprintf(stderr, "Error: Failed to generate code: <reason>\n")
     */
    bool (*generate)(const char* kir_path, const char* output_path);

    /**
     * Validate KIR before generation (optional)
     * @param kir_path Path to KIR file
     * @return true if valid, false otherwise
     *
     * Provides pre-generation validation without generating code.
     * Can be NULL if codegen doesn't support validation.
     */
    bool (*validate)(const char* kir_path);

    /**
     * Cleanup codegen resources (optional)
     * Called when codegen is unloaded (for dynamic loading)
     */
    void (*cleanup)(void);

} CodegenInterface;

/* ============================================================================
 * Codegen Registry
 * ============================================================================ */

/**
 * Register a codegen
 * @param codegen Codegen interface (must remain valid for program lifetime)
 * @return 0 on success, non-zero on error
 */
int codegen_register(const CodegenInterface* codegen);

/**
 * Get codegen by target name
 * @param target Target name (e.g., "c", "limbo", "tcltk")
 * @return Codegen interface, or NULL if not found
 */
const CodegenInterface* codegen_get_interface(const char* target);

/**
 * List all registered codegens
 * @return NULL-terminated array of codegen interfaces
 */
const CodegenInterface** codegen_list_all(void);

/* ============================================================================
 * Standard Codegen Implementations
 * ============================================================================ */

/**
 * Generate code using appropriate codegen based on target name
 * @param kir_path Path to KIR file
 * @param target Target name (e.g., "c", "limbo", "tcltk")
 * @param output_path Output directory or file path
 * @return true on success, false on failure
 */
bool codegen_generate_from_registry(const char* kir_path, const char* target, const char* output_path);

/* ============================================================================
 * Helper Macros
 * ============================================================================ */

/**
 * Standard error messages for codegens
 */
#define CODEGEN_ERROR_FILE_NOT_FOUND(filepath) \
    fprintf(stderr, "Error: Cannot open KIR file: %s\n", filepath)

#define CODEGEN_ERROR_PARSE_FAILED(reason) \
    fprintf(stderr, "Error: Failed to parse KIR: %s\n", reason)

#define CODEGEN_ERROR_GENERATION_FAILED(reason) \
    fprintf(stderr, "Error: Failed to generate code: %s\n", reason)

/**
 * Declare a standard codegen implementation
 */
#define DECLARE_CODEGEN(name_str, version_str, desc_str, ext_list) \
    static const CodegenInfo name##_codegen_info = { \
        .name = name_str, \
        .version = version_str, \
        .description = desc_str, \
        .file_extensions = ext_list \
    }; \
    static const CodegenInterface name##_codegen_interface = { \
        .get_info = name##_codegen_get_info, \
        .generate = name##_codegen_generate, \
        .validate = NULL, \
        .cleanup = NULL \
    }

#ifdef __cplusplus
}
#endif

#endif // CODEGEN_INTERFACE_H
