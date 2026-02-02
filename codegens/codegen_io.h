/**
 * @file codegen_io.h
 * @brief Codegen Input/Output Abstraction Layer
 *
 * Eliminates repetitive file I/O patterns across 15+ codegen files.
 * Provides a unified interface for loading KIR files with consistent error handling.
 *
 * Before (15 lines of boilerplate):
 *   size_t size;
 *   char* kir_json = codegen_read_kir_file(kir_path, &size);
 *   if (!kir_json) {
 *       fprintf(stderr, "Error: Failed to read KIR file: %s\n", kir_path);
 *       return false;
 *   }
 *   cJSON* root = codegen_parse_kir_json(kir_json);
 *   free(kir_json);
 *   if (!root) {
 *       fprintf(stderr, "Error: Failed to parse KIR JSON\n");
 *       return false;
 *   }
 *
 * After (3 lines):
 *   CodegenInput* input = codegen_load_input(kir_path);
 *   if (!input) return false;
 *   // Use input->root, then codegen_free_input(input)
 */

#ifndef CODEGEN_IO_H
#define CODEGEN_IO_H

#include <stdbool.h>
#include <stddef.h>

// cJSON forward declaration
typedef struct cJSON cJSON;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Codegen Input Structure
 * ============================================================================ */

/**
 * @brief Codegen input bundle
 *
 * Encapsulates all input data needed by codegens:
 * - Parsed JSON root object
 * - Raw JSON string (for error reporting)
 * - File size
 * - Original file path
 *
 * Lifecycle:
 *   1. Create with codegen_load_input() or codegen_load_from_string()
 *   2. Use input->root to access JSON data
 *   3. Free with codegen_free_input()
 *
 * Ownership:
 * - input->root is borrowed (do not free, owned by input)
 * - All other fields are managed by the input structure
 */
typedef struct {
    cJSON* root;            /**< Parsed JSON root object (borrowed reference) */
    char* json_string;      /**< Raw JSON string (owned, will be freed) */
    size_t size;            /**< Size of JSON string in bytes */
    const char* path;       /**< Original file path (borrowed reference) */
} CodegenInput;

/* ============================================================================
 * Input Loading Functions
 * ============================================================================ */

/**
 * @brief Load KIR file and parse JSON
 *
 * Combines file reading, JSON parsing, and error handling into a single call.
 * This eliminates ~15 lines of boilerplate code from each codegen.
 *
 * Error handling:
 * - File not found: Reports error, returns NULL
 * - Parse error: Reports error, returns NULL
 * - Success: Returns allocated CodegenInput structure
 *
 * @param kir_path Path to KIR file
 * @return Allocated CodegenInput structure, or NULL on error
 *
 * Example:
 *   CodegenInput* input = codegen_load_input("app.kir");
 *   if (!input) {
 *       // Error already reported
 *       return false;
 *   }
 *
 *   cJSON* component = codegen_extract_root_component(input->root);
 *   // ... process component ...
 *
 *   codegen_free_input(input);
 */
CodegenInput* codegen_load_input(const char* kir_path);

/**
 * @brief Load input from JSON string (for testing)
 *
 * Creates a CodegenInput structure from a JSON string without reading a file.
 * Useful for unit tests and programmatic codegen invocation.
 *
 * @param json_string Null-terminated JSON string
 * @return Allocated CodegenInput structure, or NULL on parse error
 *
 * Example:
 *   const char* test_json = "{\"type\":\"Button\",\"text\":\"Click me\"}";
 *   CodegenInput* input = codegen_load_from_string(test_json);
 *   if (!input) return false;
 *   // ... use input ...
 *   codegen_free_input(input);
 */
CodegenInput* codegen_load_from_string(const char* json_string);

/**
 * @brief Free input structure
 *
 * Releases all resources associated with a CodegenInput structure:
 * - Frees the JSON string
 * - Deletes the cJSON root object
 * - Frees the CodegenInput structure itself
 *
 * After calling this function, all pointers to input->root and input->json_string
 * become invalid and must not be used.
 *
 * @param input CodegenInput structure to free (NULL-safe)
 */
void codegen_free_input(CodegenInput* input);

/* ============================================================================
 * Input Validation Helpers
 * ============================================================================ */

/**
 * @brief Validate input structure
 *
 * Checks if the input structure has valid JSON data.
 * Useful for defensive programming and early error detection.
 *
 * @param input CodegenInput structure to validate
 * @return true if input is valid, false otherwise
 */
bool codegen_input_is_valid(const CodegenInput* input);

/**
 * @brief Get root component from input
 *
 * Convenience wrapper around codegen_extract_root_component().
 * Returns the root component or NULL if not found.
 *
 * @param input CodegenInput structure
 * @return Root component cJSON object, or NULL if not found
 */
cJSON* codegen_input_get_root_component(CodegenInput* input);

/**
 * @brief Get logic block from input
 *
 * Convenience wrapper around codegen_extract_logic_block().
 * Returns the logic block or NULL if not found.
 *
 * @param input CodegenInput structure
 * @return Logic block cJSON object, or NULL if not found
 */
cJSON* codegen_input_get_logic_block(CodegenInput* input);

/* ============================================================================
 * Error Handling
 * ============================================================================ */

/**
 * @brief Set error prefix for I/O operations
 *
 * Sets a prefix for error messages to identify which codegen is reporting errors.
 * For example, "Lua", "C", "Tcl", etc.
 *
 * @param prefix Error prefix string (e.g., "Lua", "C")
 */
void codegen_io_set_error_prefix(const char* prefix);

/**
 * @brief Get last I/O error message
 *
 * Returns the last error message from an I/O operation.
 * The returned string is valid until the next I/O operation.
 *
 * @return Error message string, or NULL if no error
 */
const char* codegen_io_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // CODEGEN_IO_H
