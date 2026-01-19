/**
 * KRY @universal Transpiler
 *
 * Converts @universal code blocks to platform-specific Lua and JavaScript code.
 *
 * The universal syntax provides:
 * - Platform-agnostic standard library (array, math, string, object)
 * - Unified syntax that transpiles to both Lua and JavaScript
 * - Type annotations for better code generation
 */

#ifndef KRY_UNIVERSAL_H
#define KRY_UNIVERSAL_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Transpiler Options
// ============================================================================

typedef enum {
    KRY_TARGET_LUA,           // Generate Lua code
    KRY_TARGET_JAVASCRIPT,    // Generate JavaScript code
    KRY_TARGET_BOTH           // Generate both (returns two strings)
} KryTargetPlatform;

typedef struct {
    KryTargetPlatform platform;
    bool minify;              // Minify output
    bool include_comments;    // Preserve comments
    bool use_strict;          // Use strict mode (JS)
} KryTranspilerOptions;

// ============================================================================
// Transpiler API
// ============================================================================

/**
 * Transpile @universal code to target platform
 *
 * @param source Universal source code
 * @param options Transpiler options
 * @param output_length Length of generated code (output parameter)
 * @return char* Generated code (caller must free), or NULL on error
 *
 * @example
 *   const char* universal = "return array.length(arr)";
 *   size_t length;
 *   char* lua = kry_universal_transpile(universal, KRY_TARGET_LUA, &length);
 *   // Result: "return #arr"
 *   free(lua);
 */
char* kry_universal_transpile(const char* source,
                              KryTargetPlatform platform,
                              size_t* output_length);

/**
 * Transpile with full options
 *
 * @param source Universal source code
 * @param options Transpiler options
 * @param output_length Length of generated code (output parameter)
 * @return char* Generated code (caller must free), or NULL on error
 */
char* kry_universal_transpile_ex(const char* source,
                                 const KryTranspilerOptions* options,
                                 size_t* output_length);

/**
 * Get transpiler version
 *
 * @return const char* Version string
 */
const char* kry_universal_version(void);

/**
 * Get last error message
 *
 * @return const char* Error message from last transpilation, or NULL if no error
 */
const char* kry_universal_get_error(void);

/**
 * Clear last error message
 */
void kry_universal_clear_error(void);

// ============================================================================
// Standard Library Mappings
// ============================================================================

/**
 * Array function mappings
 *
 * Universal -> Lua | JavaScript
 * ----------------------------------------
 * array.length(arr) -> #arr | arr.length
 * array.push(arr, item) -> table.insert(arr, item) | arr.push(item)
 * array.pop(arr) -> table.remove(arr) | arr.pop()
 * array.map(arr, fn) -> custom_map(arr, fn) | arr.map(fn)
 * array.filter(arr, fn) -> custom_filter(arr, fn) | arr.filter(fn)
 * array.reduce(arr, fn, init) -> custom_reduce(arr, fn, init) | arr.reduce(fn, init)
 */

/**
 * Math function mappings
 *
 * Universal -> Lua | JavaScript
 * ----------------------------------------
 * math.random() -> math.random() | Math.random()
 * math.random(n) -> math.random(1, n) | Math.floor(Math.random() * n)
 * math.floor(n) -> math.floor(n) | Math.floor(n)
 * math.ceil(n) -> math.ceil(n) | Math.ceil(n)
 * math.abs(n) -> math.abs(n) | Math.abs(n)
 * math.min(a, b) -> math.min(a, b) | Math.min(a, b)
 * math.max(a, b) -> math.max(a, b) | Math.max(a, b)
 */

/**
 * String function mappings
 *
 * Universal -> Lua | JavaScript
 * ----------------------------------------
 * string.match(s, pat) -> s:match(pat) | s.match(pat)
 * string.find(s, pat) -> s:find(pat) | s.indexOf(pat) >= 0
 * string.sub(s, start, end) -> s:sub(start, end) | s.substring(start, end)
 * string.length(s) -> #s | s.length
 * string.upper(s) -> s:upper() | s.toUpperCase()
 * string.lower(s) -> s:lower() | s.toLowerCase()
 * string.format(fmt, ...) -> string.format(fmt, ...) | sprintf(fmt, ...) or template literals
 */

/**
 * Object function mappings
 *
 * Universal -> Lua | JavaScript
 * ----------------------------------------
 * object.keys(obj) -> custom_keys(obj) | Object.keys(obj)
 * object.values(obj) -> custom_values(obj) | Object.values(obj)
 * object.entries(obj) -> custom_entries(obj) | Object.entries(obj)
 */

#ifdef __cplusplus
}
#endif

#endif // KRY_UNIVERSAL_H
