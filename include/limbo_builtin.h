/**
 * @file limbo_builtin.h
 * @brief Built-in functions for Limbo integration
 */

#ifndef KRYON_LIMBO_BUILTIN_H
#define KRYON_LIMBO_BUILTIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// Forward declarations
struct KryonRuntime;
struct KryonElement;

/**
 * Execute a Limbo built-in function statement
 *
 * Checks if the statement is a Limbo built-in (loadModule, callModuleFunction, etc.)
 * and executes it if so.
 *
 * @param runtime The Kryon runtime
 * @param element The element context (can be NULL)
 * @param statement The statement to execute
 * @return true if statement was a Limbo built-in (handled), false otherwise
 */
bool kryon_execute_limbo_builtin(
    struct KryonRuntime *runtime,
    struct KryonElement *element,
    const char *statement
);

#ifdef __cplusplus
}
#endif

#endif // KRYON_LIMBO_BUILTIN_H
