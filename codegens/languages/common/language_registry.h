/**
 * Kryon Language Registry
 *
 * Registry system for language emitter profiles.
 * Each language (Tcl, Limbo, C, Kotlin, JavaScript) has its own emitter
 * that generates syntax from toolkit-specific IR.
 */

#ifndef LANGUAGE_REGISTRY_H
#define LANGUAGE_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Language Type Enumeration
// ============================================================================

/**
 * Supported programming languages
 */
typedef enum {
    LANGUAGE_TCL,          // Tcl
    LANGUAGE_LIMBO,        // Limbo (Inferno)
    LANGUAGE_C,            // C
    LANGUAGE_KOTLIN,       // Kotlin
    LANGUAGE_JAVASCRIPT,   // JavaScript
    LANGUAGE_LUA,          // Lua
    LANGUAGE_NONE          // No language
} LanguageType;

/**
 * Convert language type string to enum
 * @param type_str Language type string (e.g., "tcl", "limbo")
 * @return LanguageType enum, or LANGUAGE_NONE if invalid
 */
LanguageType language_type_from_string(const char* type_str);

/**
 * Convert language enum to string
 * @param type LanguageType enum
 * @return Static string representation, or "unknown" if invalid
 */
const char* language_type_to_string(LanguageType type);

/**
 * Check if language type is valid
 * @param type LanguageType to check
 * @return true if valid, false otherwise
 */
bool language_type_is_valid(LanguageType type);

// ============================================================================
// Language Profile
// ============================================================================

/**
 * Platform default mapping for languages
 */
typedef struct {
    const char* platform;        // Platform name
    const char* default_toolkit; // Default toolkit for this platform
} LanguagePlatformDefault;

/**
 * String escaping function type
 * @param lang_profile Language profile
 * @param input Input string to escape
 * @return Newly allocated escaped string (caller must free)
 */
typedef char* (*StringEscapeFunc)(const void* lang_profile, const char* input);

/**
 * Code formatting options
 */
typedef struct {
    int indent_size;           // Number of spaces per indent (0 for tabs)
    bool use_tabs;             // Use tabs instead of spaces
    bool trailing_newline;     // Add trailing newline
    int max_line_width;        // Maximum line width (0 for no limit)
    bool braces_on_newline;    // Put opening braces on new line (C-style)
} CodeFormatOptions;

/**
 * Language profile
 * Defines all characteristics of a programming language
 */
typedef struct {
    const char* name;              // Language name (e.g., "tcl", "limbo")
    LanguageType type;             // Language type enum

    // File extensions
    const char* source_extension;  // Source file extension (e.g., ".tcl")
    const char* header_extension;  // Header file extension (e.g., ".h", or NULL)

    // Comments
    const char* line_comment;      // Line comment prefix (e.g., "#", "//")
    const char* block_comment_start; // Block comment start (e.g., "/*", or NULL)
    const char* block_comment_end;   // Block comment end (e.g., "*/", or NULL)

    // Strings
    const char* string_quote;      // String quote character
    StringEscapeFunc escape_string; // String escape function

    // Syntax characteristics
    bool statement_terminator;     // Requires statement terminator (e.g., ";")
    const char* statement_terminator_char; // Terminator character
    bool block_delimiters;         // Uses block delimiters (e.g., "{}")
    bool requires_main;            // Requires main() function

    // Variable declaration
    const char* var_declaration;   // Variable declaration template (e.g., "set %s", "var %s")
    bool requires_type_declaration; // Requires type in declarations

    // Function declaration
    const char* func_declaration;  // Function declaration template
    bool requires_return_type;     // Requires return type declaration

    // Default formatting options
    CodeFormatOptions format;

    // Code generation
    const char* boilerplate_header;   // Header boilerplate code
    const char* boilerplate_footer;   // Footer boilerplate code

    // Platform support
    bool is_source_language;           // Has parser (kry, tcl, html, markdown, lua)
    bool is_binding_language;          // Emits code only (c, limbo, python, kotlin, rust, js/ts)
    const char** compatible_platforms; // Array of platform names
    size_t platform_count;             // Number of compatible platforms

    // Platform defaults
    LanguagePlatformDefault* defaults; // Array of platform → toolkit mappings
    size_t default_count;              // Number of platform defaults

} LanguageProfile;

// ============================================================================
// Language Registry API
// ============================================================================

/**
 * Register a language profile
 * @param profile Language profile to register (static storage, not copied)
 * @return true on success, false on failure
 */
bool language_register(const LanguageProfile* profile);

/**
 * Get language profile by name
 * @param name Language name (e.g., "tcl", "limbo")
 * @return Language profile, or NULL if not found
 */
const LanguageProfile* language_get_profile(const char* name);

/**
 * Get language profile by type
 * @param type LanguageType enum
 * @return Language profile, or NULL if not found
 */
const LanguageProfile* language_get_profile_by_type(LanguageType type);

/**
 * Check if language is registered
 * @param name Language name
 * @return true if registered, false otherwise
 */
bool language_is_registered(const char* name);

/**
 * Get all registered languages
 * @param out_profiles Output array for language profiles
 * @param max_count Maximum number of languages to return
 * @return Number of languages returned
 */
size_t language_get_all_registered(const LanguageProfile** out_profiles, size_t max_count);

/**
 * Escape a string for the target language
 * @param profile Language profile
 * @param input Input string to escape
 * @return Newly allocated escaped string (caller must free), or NULL on error
 */
char* language_escape_string(const LanguageProfile* profile, const char* input);

/**
 * Generate an identifier from a string
 * Ensures the identifier is valid for the language
 * @param profile Language profile
 * @param input Input string
 * @return Newly allocated identifier (caller must free), or NULL on error
 */
char* language_make_identifier(const LanguageProfile* profile, const char* input);

/**
 * Initialize language registry with all built-in languages
 * Called automatically during system initialization
 */
void language_registry_init(void);

/**
 * Cleanup language registry
 */
void language_registry_cleanup(void);

// ============================================================================
// Platform Support API
// ============================================================================

/**
 * Check if language supports a specific platform
 * @param profile Language profile
 * @param platform_name Platform name
 * @return true if supported, false otherwise
 */
bool language_supports_platform(const LanguageProfile* profile, const char* platform_name);

/**
 * Get default toolkit for a language on a specific platform
 * @param profile Language profile
 * @param platform_name Platform name
 * @return Default toolkit name, or NULL if not found
 */
const char* language_get_default_toolkit_for_platform(const LanguageProfile* profile, const char* platform_name);

/**
 * Check if language is a source language (has parser)
 * @param profile Language profile
 * @return true if source language, false otherwise
 */
bool language_is_source_language(const LanguageProfile* profile);

/**
 * Check if language is a binding language (emits code only)
 * @param profile Language profile
 * @return true if binding language, false otherwise
 */
bool language_is_binding_language(const LanguageProfile* profile);

#ifdef __cplusplus
}
#endif

#endif // LANGUAGE_REGISTRY_H
