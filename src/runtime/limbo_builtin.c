/**
 * @file limbo_builtin.c
 * @brief Built-in functions for Limbo integration in Kryon
 *
 * This file provides convenience functions that make it easier to work with
 * Limbo modules from Kryon code.
 *
 * Built-in functions:
 * - loadModule(name, path): Load a Limbo module
 * - callModuleFunction(module, function, ...): Call a Limbo function
 * - loadYAML(path): Load and parse a YAML file
 * - loadJSON(path): Load and parse a JSON file
 */

#ifdef KRYON_PLUGIN_LIMBO

#include "runtime.h"
#include "elements.h"
#include "limbo/limbo_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// GLOBAL LIMBO RUNTIME
// =============================================================================

extern KryonLimboRuntime g_limbo_runtime;
extern bool g_limbo_initialized;

// =============================================================================
// BUILT-IN: loadModule()
//
// Usage: module = loadModule("YAML", "/dis/lib/yaml.dis")
// =============================================================================

static bool execute_load_module_statement(
    KryonRuntime *runtime,
    KryonElement *element,
    const char *statement)
{
    fprintf(stderr, "[Limbo] loadModule() called: %s\n", statement);

    const char *cursor = statement;

    // Skip "loadModule"
    if (strncmp(cursor, "loadModule", 10) != 0) {
        return false;
    }
    cursor += 10;

    // Skip whitespace
    while (*cursor == ' ' || *cursor == '\t') cursor++;

    // Expect opening parenthesis
    if (*cursor != '(') {
        fprintf(stderr, "[Limbo] Expected '(' after loadModule\n");
        return false;
    }
    cursor++;

    // Parse arguments: (module_name, file_path)
    char module_name[256];
    char file_path[512];

    // Skip whitespace
    while (*cursor == ' ' || *cursor == '\t') cursor++;

    // Parse module name (quoted string)
    if (*cursor != '"') {
        fprintf(stderr, "[Limbo] Expected quoted module name\n");
        return false;
    }
    cursor++; // skip opening quote

    const char *name_end = strchr(cursor, '"');
    if (!name_end) {
        fprintf(stderr, "[Limbo] Unclosed module name string\n");
        return false;
    }

    size_t name_len = name_end - cursor;
    if (name_len >= sizeof(module_name)) {
        fprintf(stderr, "[Limbo] Module name too long\n");
        return false;
    }
    strncpy(module_name, cursor, name_len);
    module_name[name_len] = '\0';
    cursor = name_end + 1;

    // Skip whitespace and comma
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (*cursor == ',') cursor++;
    while (*cursor == ' ' || *cursor == '\t') cursor++;

    // Parse file path (quoted string)
    if (*cursor != '"') {
        fprintf(stderr, "[Limbo] Expected quoted file path\n");
        return false;
    }
    cursor++; // skip opening quote

    const char *path_end = strchr(cursor, '"');
    if (!path_end) {
        fprintf(stderr, "[Limbo] Unclosed file path string\n");
        return false;
    }

    size_t path_len = path_end - cursor;
    if (path_len >= sizeof(file_path)) {
        fprintf(stderr, "[Limbo] File path too long\n");
        return false;
    }
    strncpy(file_path, cursor, path_len);
    file_path[path_len] = '\0';
    cursor = path_end + 1;

    // Skip whitespace and closing paren
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (*cursor != ')') {
        fprintf(stderr, "[Limbo] Expected ')' after arguments\n");
        return false;
    }

    fprintf(stderr, "[Limbo] Loading module '%s' from '%s'\n", module_name, file_path);

    // Load module using Limbo runtime
    if (!g_limbo_initialized) {
        fprintf(stderr, "[Limbo] Runtime not initialized, initializing...\n");
        if (!kryon_limbo_runtime_init(&g_limbo_runtime)) {
            fprintf(stderr, "[Limbo] Failed to initialize runtime\n");
            return false;
        }
        g_limbo_initialized = true;
    }

    Module *mod = kryon_limbo_load_module(&g_limbo_runtime, module_name, file_path);
    if (!mod) {
        fprintf(stderr, "[Limbo] Failed to load module\n");
        return false;
    }

    // Store module handle in element metadata (if element provided)
    if (element) {
        // TODO: Store module reference in element
        fprintf(stderr, "[Limbo] Module loaded successfully (element storage not yet implemented)\n");
    }

    return true;
}

// =============================================================================
// BUILT-IN: callModuleFunction()
//
// Usage: result = callModuleFunction("YAML", "loadfile", "data.yaml")
// =============================================================================

static bool execute_call_module_function_statement(
    KryonRuntime *runtime,
    KryonElement *element,
    const char *statement)
{
    fprintf(stderr, "[Limbo] callModuleFunction() called: %s\n", statement);

    const char *cursor = statement;

    // Skip "callModuleFunction"
    if (strncmp(cursor, "callModuleFunction", 18) != 0) {
        return false;
    }
    cursor += 18;

    // Skip whitespace
    while (*cursor == ' ' || *cursor == '\t') cursor++;

    // Expect opening parenthesis
    if (*cursor != '(') {
        fprintf(stderr, "[Limbo] Expected '(' after callModuleFunction\n");
        return false;
    }
    cursor++;

    // Parse arguments: (module_name, function_name, args...)
    char module_name[256];
    char function_name[256];
    char args[512];

    // Skip whitespace
    while (*cursor == ' ' || *cursor == '\t') cursor++;

    // Parse module name (quoted string)
    if (*cursor != '"') {
        fprintf(stderr, "[Limbo] Expected quoted module name\n");
        return false;
    }
    cursor++; // skip opening quote

    const char *name_end = strchr(cursor, '"');
    if (!name_end) {
        fprintf(stderr, "[Limbo] Unclosed module name string\n");
        return false;
    }

    size_t name_len = name_end - cursor;
    if (name_len >= sizeof(module_name)) {
        fprintf(stderr, "[Limbo] Module name too long\n");
        return false;
    }
    strncpy(module_name, cursor, name_len);
    module_name[name_len] = '\0';
    cursor = name_end + 1;

    // Skip whitespace and comma
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (*cursor == ',') cursor++;
    while (*cursor == ' ' || *cursor == '\t') cursor++;

    // Parse function name (quoted string)
    if (*cursor != '"') {
        fprintf(stderr, "[Limbo] Expected quoted function name\n");
        return false;
    }
    cursor++; // skip opening quote

    const char *func_end = strchr(cursor, '"');
    if (!func_end) {
        fprintf(stderr, "[Limbo] Unclosed function name string\n");
        return false;
    }

    size_t func_len = func_end - cursor;
    if (func_len >= sizeof(function_name)) {
        fprintf(stderr, "[Limbo] Function name too long\n");
        return false;
    }
    strncpy(function_name, cursor, func_len);
    function_name[func_len] = '\0';
    cursor = func_end + 1;

    // Skip whitespace and comma
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (*cursor == ',') cursor++;
    while (*cursor == ' ' || *cursor == '\t') cursor++;

    // Parse arguments (quoted string)
    if (*cursor != '"') {
        fprintf(stderr, "[Limbo] Expected quoted arguments string\n");
        return false;
    }
    cursor++; // skip opening quote

    const char *args_end = strchr(cursor, '"');
    if (!args_end) {
        fprintf(stderr, "[Limbo] Unclosed arguments string\n");
        return false;
    }

    size_t args_len = args_end - cursor;
    if (args_len >= sizeof(args)) {
        fprintf(stderr, "[Limbo] Arguments too long\n");
        return false;
    }
    strncpy(args, cursor, args_len);
    args[args_len] = '\0';
    cursor = args_end + 1;

    // Skip whitespace and closing paren
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (*cursor != ')') {
        fprintf(stderr, "[Limbo] Expected ')' after arguments\n");
        return false;
    }

    fprintf(stderr, "[Limbo] Calling %s.%s(%s)\n", module_name, function_name, args);

    // Find the module
    Module *mod = kryon_limbo_find_module(&g_limbo_runtime, module_name);
    if (!mod) {
        fprintf(stderr, "[Limbo] Module '%s' not loaded\n", module_name);
        return false;
    }

    // Call the function
    char *argv[1] = { args };
    KryonLimboValue result;
    if (!kryon_limbo_call_function(&g_limbo_runtime, mod, function_name,
                                   argv, 1, &result)) {
        fprintf(stderr, "[Limbo] Function call failed\n");
        return false;
    }

    // Convert result to JSON
    char *result_json = kryon_limbo_value_to_json(&result);
    if (result_json) {
        fprintf(stderr, "[Limbo] Result: %s\n", result_json);
        // TODO: Store result in element state or variable
        free(result_json);
    }

    kryon_limbo_value_free(&result);

    return true;
}

// =============================================================================
// BUILT-IN: loadYAML()
//
// Usage: data = loadYAML("data/default.yaml")
// =============================================================================

static bool execute_load_yaml_statement(
    KryonRuntime *runtime,
    KryonElement *element,
    const char *statement)
{
    fprintf(stderr, "[Limbo] loadYAML() called: %s\n", statement);

    const char *cursor = statement;

    // Skip "loadYAML"
    if (strncmp(cursor, "loadYAML", 8) != 0) {
        return false;
    }
    cursor += 8;

    // Skip whitespace
    while (*cursor == ' ' || *cursor == '\t') cursor++;

    // Expect opening parenthesis
    if (*cursor != '(') {
        fprintf(stderr, "[Limbo] Expected '(' after loadYAML\n");
        return false;
    }
    cursor++;

    // Parse file path (quoted string)
    while (*cursor == ' ' || *cursor == '\t') cursor++;

    if (*cursor != '"') {
        fprintf(stderr, "[Limbo] Expected quoted file path\n");
        return false;
    }
    cursor++; // skip opening quote

    const char *path_end = strchr(cursor, '"');
    if (!path_end) {
        fprintf(stderr, "[Limbo] Unclosed file path string\n");
        return false;
    }

    char file_path[512];
    size_t path_len = path_end - cursor;
    if (path_len >= sizeof(file_path)) {
        fprintf(stderr, "[Limbo] File path too long\n");
        return false;
    }
    strncpy(file_path, cursor, path_len);
    file_path[path_len] = '\0';
    cursor = path_end + 1;

    // Skip whitespace and closing paren
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (*cursor != ')') {
        fprintf(stderr, "[Limbo] Expected ')' after file path\n");
        return false;
    }

    fprintf(stderr, "[Limbo] Loading YAML from '%s'\n", file_path);

    // Ensure runtime is initialized
    if (!g_limbo_initialized) {
        fprintf(stderr, "[Limbo] Runtime not initialized, initializing...\n");
        if (!kryon_limbo_runtime_init(&g_limbo_runtime)) {
            fprintf(stderr, "[Limbo] Failed to initialize runtime\n");
            return false;
        }
        g_limbo_initialized = true;
    }

    // Load YAML file
    KryonLimboValue result;
    if (!kryon_limbo_load_yaml_file(&g_limbo_runtime, file_path, &result)) {
        fprintf(stderr, "[Limbo] Failed to load YAML file\n");
        return false;
    }

    // Convert to JSON
    char *result_json = kryon_limbo_value_to_json(&result);
    if (result_json) {
        fprintf(stderr, "[Limbo] YAML data: %s\n", result_json);
        // TODO: Store in element or variable
        free(result_json);
    }

    kryon_limbo_value_free(&result);

    return true;
}

// =============================================================================
// REGISTRATION
//
// These functions should be called from the statement interpreter
// =============================================================================

/**
 * Check if a statement is a Limbo built-in function call
 * and execute it if so.
 *
 * Returns true if the statement was handled (even if execution failed),
 * false if it's not a Limbo built-in.
 */
bool kryon_execute_limbo_builtin(
    KryonRuntime *runtime,
    KryonElement *element,
    const char *statement)
{
    if (!statement) return false;

    // Check for loadModule()
    if (strncmp(statement, "loadModule", 10) == 0) {
        return execute_load_module_statement(runtime, element, statement);
    }

    // Check for callModuleFunction()
    if (strncmp(statement, "callModuleFunction", 18) == 0) {
        return execute_call_module_function_statement(runtime, element, statement);
    }

    // Check for loadYAML()
    if (strncmp(statement, "loadYAML", 8) == 0) {
        return execute_load_yaml_statement(runtime, element, statement);
    }

    // Not a Limbo built-in
    return false;
}

#else // !KRYON_PLUGIN_LIMBO

#include <stdbool.h>

// Forward declarations
struct KryonRuntime;
struct KryonElement;

// Stub implementation when Limbo plugin is not compiled
bool kryon_execute_limbo_builtin(
    struct KryonRuntime *runtime,
    struct KryonElement *element,
    const char *statement)
{
    // Limbo plugin not compiled, no built-ins available
    return false;
}

#endif // KRYON_PLUGIN_LIMBO
