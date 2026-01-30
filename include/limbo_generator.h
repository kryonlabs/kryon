/**
 * @file limbo_generator.h
 * @brief Kryon to Limbo Code Generator API
 *
 * Generates native Limbo source code from KIR (Kryon Intermediate Representation),
 * enabling Kryon apps to compile to native Inferno/TaijiOS bytecode with full
 * module access.
 */

#ifndef KRYON_LIMBO_GENERATOR_H
#define KRYON_LIMBO_GENERATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * @brief Generate Limbo source code from KIR file
 * @param kir_file Path to input KIR file
 * @param output_file Path to output .b (Limbo source) file
 * @return true on success, false on error
 */
bool kryon_generate_limbo_from_kir(const char *kir_file, const char *output_file);

#ifdef __cplusplus
}
#endif

#endif // KRYON_LIMBO_GENERATOR_H
