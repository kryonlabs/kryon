/**
 * @file codegen_adapter_macros.h
 * @brief Macro-Based Codegen Adapter Generator
 *
 * Eliminates ~350 lines of duplicated boilerplate across 7 adapter files.
 * Each adapter can now be implemented in ~10 lines using these macros.
 *
 * Usage Example:
 *   #include "codegen_adapter_macros.h"
 *   #include "my_codegen.h"
 *
 *   // Define generate function with custom logic
 *   static bool my_codegen_generate_impl(const char* kir_path, const char* output_path) {
 *       if (!kir_path || !output_path) {
 *           CODEGEN_ERROR("Invalid arguments to MyLang codegen");
 *           return false;
 *       }
 *       return my_codegen_generate(kir_path, output_path);
 *   }
 *
 *   // Use macro to generate the adapter
 *   IMPLEMENT_CODEGEN_ADAPTER(
 *       MyLang,
 *       "MyLang code generator",
 *       my_codegen_generate_impl,
 *       ".ml", ".mlh", NULL
 *   )
 */

#ifndef CODEGEN_ADAPTER_MACROS_H
#define CODEGEN_ADAPTER_MACROS_H

#include "codegen_interface.h"
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Adapter Generation Macros
 * ============================================================================ */

/**
 * @brief Implement a complete codegen adapter with metadata
 *
 * This macro generates all boilerplate code for a codegen adapter:
 * - Extensions array
 * - CodegenInfo structure
 * - get_info() function
 * - CodegenInterface structure
 *
 * @param NAME       Name prefix for symbols (e.g., MyLang)
 * @param DESC       Description string
 * @param GENERATE_FUNC Function to call for generation (bool (*)(const char*, const char*))
 * @param ...        File extensions (variable args, e.g., ".c", ".h", NULL)
 *
 * Example:
 *   IMPLEMENT_CODEGEN_ADAPTER(
 *       C,
 *       "C code generator",
 *       ir_generate_c_code_multi,
 *       ".c", ".h", NULL
 *   )
 *
 * Generates:
 *   - static const char* c_extensions[]
 *   - static const CodegenInfo c_codegen_info_impl
 *   - static const CodegenInfo* c_codegen_get_info(void)
 *   - const CodegenInterface c_codegen_interface
 */
#define IMPLEMENT_CODEGEN_ADAPTER(NAME, DESC, GENERATE_FUNC, ...) \
    \
    static const char* NAME##_extensions[] = { __VA_ARGS__ }; \
    \
    static const CodegenInfo NAME##_codegen_info_impl = { \
        .name = #NAME, \
        .version = "alpha", \
        .description = DESC, \
        .file_extensions = NAME##_extensions \
    }; \
    \
    static const CodegenInfo* NAME##_codegen_get_info(void) { \
        return &NAME##_codegen_info_impl; \
    } \
    \
    static bool NAME##_codegen_generate_impl(const char* kir_path, const char* output_path) { \
        if (!kir_path || !output_path) { \
            fprintf(stderr, "Error: Invalid arguments to " #NAME " codegen\n"); \
            return false; \
        } \
        return GENERATE_FUNC(kir_path, output_path); \
    } \
    \
    const CodegenInterface NAME##_codegen_interface = { \
        .get_info = NAME##_codegen_get_info, \
        .generate = NAME##_codegen_generate_impl, \
        .validate = NULL, \
        .cleanup = NULL \
    };

/**
 * @brief Implement a codegen adapter with custom validation
 *
 * Same as IMPLEMENT_CODEGEN_ADAPTER but includes a validation function.
 *
 * @param NAME           Name prefix for symbols
 * @param DESC           Description string
 * @param GENERATE_FUNC  Function to call for generation
 * @param VALIDATE_FUNC  Function to call for validation (can be NULL)
 * @param ...            File extensions (variable args)
 */
#define IMPLEMENT_CODEGEN_ADAPTER_WITH_VALIDATION(NAME, DESC, GENERATE_FUNC, VALIDATE_FUNC, ...) \
    \
    static const char* NAME##_extensions[] = { __VA_ARGS__ }; \
    \
    static const CodegenInfo NAME##_codegen_info_impl = { \
        .name = #NAME, \
        .version = "alpha", \
        .description = DESC, \
        .file_extensions = NAME##_extensions \
    }; \
    \
    static const CodegenInfo* NAME##_codegen_get_info(void) { \
        return &NAME##_codegen_info_impl; \
    } \
    \
    static bool NAME##_codegen_generate_impl(const char* kir_path, const char* output_path) { \
        if (!kir_path || !output_path) { \
            fprintf(stderr, "Error: Invalid arguments to " #NAME " codegen\n"); \
            return false; \
        } \
        return GENERATE_FUNC(kir_path, output_path); \
    } \
    \
    const CodegenInterface NAME##_codegen_interface = { \
        .get_info = NAME##_codegen_get_info, \
        .generate = NAME##_codegen_generate_impl, \
        .validate = VALIDATE_FUNC, \
        .cleanup = NULL \
    };

/**
 * @brief Implement a codegen adapter with custom validation and cleanup
 *
 * Same as IMPLEMENT_CODEGEN_ADAPTER but includes both validation and cleanup functions.
 *
 * @param NAME           Name prefix for symbols
 * @param DESC           Description string
 * @param GENERATE_FUNC  Function to call for generation
 * @param VALIDATE_FUNC  Function to call for validation (can be NULL)
 * @param CLEANUP_FUNC   Function to call for cleanup (can be NULL)
 * @param ...            File extensions (variable args)
 */
#define IMPLEMENT_CODEGEN_ADAPTER_FULL(NAME, DESC, GENERATE_FUNC, VALIDATE_FUNC, CLEANUP_FUNC, ...) \
    \
    static const char* NAME##_extensions[] = { __VA_ARGS__ }; \
    \
    static const CodegenInfo NAME##_codegen_info_impl = { \
        .name = #NAME, \
        .version = "alpha", \
        .description = DESC, \
        .file_extensions = NAME##_extensions \
    }; \
    \
    static const CodegenInfo* NAME##_codegen_get_info(void) { \
        return &NAME##_codegen_info_impl; \
    } \
    \
    static bool NAME##_codegen_generate_impl(const char* kir_path, const char* output_path) { \
        if (!kir_path || !output_path) { \
            fprintf(stderr, "Error: Invalid arguments to " #NAME " codegen\n"); \
            return false; \
        } \
        return GENERATE_FUNC(kir_path, output_path); \
    } \
    \
    const CodegenInterface NAME##_codegen_interface = { \
        .get_info = NAME##_codegen_get_info, \
        .generate = NAME##_codegen_generate_impl, \
        .validate = VALIDATE_FUNC, \
        .cleanup = CLEANUP_FUNC \
    };

#ifdef __cplusplus
}
#endif

#endif // CODEGEN_ADAPTER_MACROS_H
