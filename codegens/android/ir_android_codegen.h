/**
 * Android/Kotlin Code Generator - KIR → Kotlin MainActivity
 *
 * Generates idiomatic Kotlin code with Kryon DSL from KIR JSON files.
 * Pipeline: kry -> kir -> kotlin -> android
 * Everything goes through KIR (intermediate representation).
 */

#ifndef IR_ANDROID_CODEGEN_H
#define IR_ANDROID_CODEGEN_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate Kotlin MainActivity code from a KIR file
 *
 * @param kir_path Path to input .kir file
 * @param output_path Path to output MainActivity.kt file
 * @return true on success, false on error
 */
bool ir_generate_kotlin_code(const char* kir_path, const char* output_path);

/**
 * Generate Kotlin MainActivity code from KIR JSON string
 *
 * @param kir_json KIR JSON string
 * @param output_path Path to output MainActivity.kt file
 * @return true on success, false on error
 */
bool ir_generate_kotlin_code_from_string(const char* kir_json, const char* output_path);

/**
 * Generate AndroidManifest.xml from KIR file
 *
 * @param kir_path Path to input .kir file
 * @param output_path Path to output AndroidManifest.xml file
 * @param app_name Application name (e.g., "MyApp")
 * @return true on success, false on error
 */
bool ir_generate_android_manifest(const char* kir_path, const char* output_path,
                                   const char* app_name);

/**
 * Generate build.gradle.kts (app level) from KIR file
 *
 * @param kir_path Path to input .kir file
 * @param output_path Path to output build.gradle.kts file
 * @param namespace Application namespace (e.g., "com.example.myapp")
 * @return true on success, false on error
 */
bool ir_generate_gradle_build(const char* kir_path, const char* output_path,
                               const char* namespace);

/**
 * Generate complete Android project structure from KIR
 *
 * Creates:
 * - MainActivity.kt (Kotlin DSL code)
 * - AndroidManifest.xml
 * - build.gradle.kts (app level)
 * - res/layout/ directory if needed
 *
 * @param kir_path Path to input .kir file
 * @param output_dir Directory where Android project will be created
 * @param package_name Package name (e.g., "com.example.myapp")
 * @param app_name Application display name (e.g., "My App")
 * @return true on success, false on error
 */
bool ir_generate_android_project(const char* kir_path, const char* output_dir,
                                  const char* package_name, const char* app_name);

#ifdef __cplusplus
}
#endif

#endif // IR_ANDROID_CODEGEN_H
