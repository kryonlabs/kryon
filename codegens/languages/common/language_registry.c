/**
 * Kryon Language Registry Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "language_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ============================================================================
// Constants
// ============================================================================

#define MAX_REGISTERED_LANGUAGES 16

// ============================================================================
// Registry State
// ============================================================================

static struct {
    const LanguageProfile* profiles[MAX_REGISTERED_LANGUAGES];
    int count;
    bool initialized;
} g_registry = {0};

// ============================================================================
// String Escaping Functions
// ============================================================================

static char* escape_tcl_string(const void* profile, const char* input) {
    (void)profile;  // Unused

    if (!input) return strdup("");

    // Calculate escaped size
    size_t len = strlen(input);
    size_t escaped_len = 0;
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '\\' || input[i] == '$' || input[i] == '[' || input[i] == ']' ||
            input[i] == '"' || input[i] == '{' || input[i] == '}') {
            escaped_len += 2;
        } else {
            escaped_len++;
        }
    }

    // Allocate and escape
    char* result = malloc(escaped_len + 1);
    if (!result) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (input[i]) {
            case '\\': case '$': case '[': case ']': case '"': case '{': case '}':
                result[j++] = '\\';
                result[j++] = input[i];
                break;
            default:
                result[j++] = input[i];
                break;
        }
    }
    result[j] = '\0';

    return result;
}

static char* escape_c_string(const void* profile, const char* input) {
    (void)profile;  // Unused

    if (!input) return strdup("");

    // Calculate escaped size
    size_t len = strlen(input);
    size_t escaped_len = 0;
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '\\' || input[i] == '"' || input[i] == '\'') {
            escaped_len += 2;
        } else if (input[i] == '\n') {
            escaped_len += 2;
        } else if (input[i] == '\t') {
            escaped_len += 2;
        } else if (input[i] == '\r') {
            escaped_len += 2;
        } else {
            escaped_len++;
        }
    }

    // Allocate and escape
    char* result = malloc(escaped_len + 1);
    if (!result) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (input[i]) {
            case '\\': result[j++] = '\\'; result[j++] = '\\'; break;
            case '"': result[j++] = '\\'; result[j++] = '"'; break;
            case '\'': result[j++] = '\\'; result[j++] = '\''; break;
            case '\n': result[j++] = '\\'; result[j++] = 'n'; break;
            case '\t': result[j++] = '\\'; result[j++] = 't'; break;
            case '\r': result[j++] = '\\'; result[j++] = 'r'; break;
            default:
                result[j++] = input[i];
                break;
        }
    }
    result[j] = '\0';

    return result;
}

static char* escape_limbo_string(const void* profile, const char* input) {
    (void)profile;  // Unused

    // Limbo strings are similar to C strings
    return escape_c_string(profile, input);
}

static char* escape_javascript_string(const void* profile, const char* input) {
    (void)profile;  // Unused

    // JavaScript strings are similar to C strings
    return escape_c_string(profile, input);
}

// ============================================================================
// Built-in Language Profiles
// ============================================================================

// === Tcl Language Profile ===

static const LanguageProfile tcl_profile = {
    .name = "tcl",
    .type = LANGUAGE_TCL,
    .source_extension = ".tcl",
    .header_extension = NULL,
    .line_comment = "#",
    .block_comment_start = NULL,  // Tcl doesn't have block comments
    .block_comment_end = NULL,
    .string_quote = "\"",
    .escape_string = escape_tcl_string,
    .statement_terminator = false,  // Newline is terminator
    .statement_terminator_char = NULL,
    .block_delimiters = false,  // Uses braces for grouping, not blocks
    .requires_main = false,
    .var_declaration = "set %s",
    .requires_type_declaration = false,
    .func_declaration = "proc %s {args} {body}",
    .requires_return_type = false,
    .format = {
        .indent_size = 4,
        .use_tabs = false,
        .trailing_newline = true,
        .max_line_width = 0,
        .braces_on_newline = false
    },
    .boilerplate_header = NULL,
    .boilerplate_footer = NULL
};

// === Limbo Language Profile ===

static const LanguageProfile limbo_profile = {
    .name = "limbo",
    .type = LANGUAGE_LIMBO,
    .source_extension = ".b",
    .header_extension = NULL,
    .line_comment = "#",
    .block_comment_start = NULL,
    .block_comment_end = NULL,
    .string_quote = "\"",
    .escape_string = escape_limbo_string,
    .statement_terminator = true,
    .statement_terminator_char = ";",
    .block_delimiters = true,
    .requires_main = false,
    .var_declaration = "%s:=%s",
    .requires_type_declaration = true,
    .func_declaration = "%s%s(args) {return type}",
    .requires_return_type = true,
    .format = {
        .indent_size = 4,
        .use_tabs = true,  // Limbo convention uses tabs
        .trailing_newline = true,
        .max_line_width = 80,
        .braces_on_newline = false
    },
    .boilerplate_header = "implement Kryon;\n\n",
    .boilerplate_footer = NULL
};

// === C Language Profile ===

static const LanguageProfile c_profile = {
    .name = "c",
    .type = LANGUAGE_C,
    .source_extension = ".c",
    .header_extension = ".h",
    .line_comment = "//",
    .block_comment_start = "/*",
    .block_comment_end = "*/",
    .string_quote = "\"",
    .escape_string = escape_c_string,
    .statement_terminator = true,
    .statement_terminator_char = ";",
    .block_delimiters = true,
    .requires_main = true,
    .var_declaration = "%s %s",
    .requires_type_declaration = true,
    .func_declaration = "%s %s(%s)",
    .requires_return_type = true,
    .format = {
        .indent_size = 4,
        .use_tabs = false,
        .trailing_newline = true,
        .max_line_width = 80,
        .braces_on_newline = true  // K&R style
    },
    .boilerplate_header = "#include <stdio.h>\n#include <stdlib.h>\n\n",
    .boilerplate_footer = NULL
};

// === Kotlin Language Profile ===

static const LanguageProfile kotlin_profile = {
    .name = "kotlin",
    .type = LANGUAGE_KOTLIN,
    .source_extension = ".kt",
    .header_extension = NULL,
    .line_comment = "//",
    .block_comment_start = "/*",
    .block_comment_end = "*/",
    .string_quote = "\"",
    .escape_string = escape_c_string,  // Similar to C
    .statement_terminator = true,
    .statement_terminator_char = ";",
    .block_delimiters = true,
    .requires_main = true,
    .var_declaration = "val %s: %s = %s",
    .requires_type_declaration = false,  // Type inference
    .func_declaration = "fun %s(%s): %s",
    .requires_return_type = true,
    .format = {
        .indent_size = 4,
        .use_tabs = false,
        .trailing_newline = true,
        .max_line_width = 100,
        .braces_on_newline = false
    },
    .boilerplate_header = NULL,
    .boilerplate_footer = NULL
};

// === JavaScript Language Profile ===

static const LanguageProfile javascript_profile = {
    .name = "javascript",
    .type = LANGUAGE_JAVASCRIPT,
    .source_extension = ".js",
    .header_extension = NULL,
    .line_comment = "//",
    .block_comment_start = "/*",
    .block_comment_end = "*/",
    .string_quote = "\"",
    .escape_string = escape_javascript_string,
    .statement_terminator = true,
    .statement_terminator_char = ";",
    .block_delimiters = true,
    .requires_main = false,  // No main() required
    .var_declaration = "const %s = %s",
    .requires_type_declaration = false,
    .func_declaration = "function %s(%s)",
    .requires_return_type = false,
    .format = {
        .indent_size = 2,  // JavaScript convention
        .use_tabs = false,
        .trailing_newline = true,
        .max_line_width = 80,
        .braces_on_newline = false
    },
    .boilerplate_header = NULL,
    .boilerplate_footer = NULL
};

// === TypeScript Language Profile ===

static const LanguageProfile typescript_profile = {
    .name = "typescript",
    .type = LANGUAGE_TYPESCRIPT,
    .source_extension = ".ts",
    .header_extension = NULL,
    .line_comment = "//",
    .block_comment_start = "/*",
    .block_comment_end = "*/",
    .string_quote = "\"",
    .escape_string = escape_javascript_string,
    .statement_terminator = true,
    .statement_terminator_char = ";",
    .block_delimiters = true,
    .requires_main = false,
    .var_declaration = "const %s: %s = %s",
    .requires_type_declaration = true,
    .func_declaration = "function %s(%s): %s",
    .requires_return_type = true,
    .format = {
        .indent_size = 2,
        .use_tabs = false,
        .trailing_newline = true,
        .max_line_width = 80,
        .braces_on_newline = false
    },
    .boilerplate_header = NULL,
    .boilerplate_footer = NULL
};

// ============================================================================
// Language Type Conversion
// ============================================================================

LanguageType language_type_from_string(const char* type_str) {
    if (!type_str) return LANGUAGE_NONE;

    if (strcmp(type_str, "tcl") == 0) return LANGUAGE_TCL;
    if (strcmp(type_str, "limbo") == 0) return LANGUAGE_LIMBO;
    if (strcmp(type_str, "c") == 0) return LANGUAGE_C;
    if (strcmp(type_str, "kotlin") == 0 || strcmp(type_str, "kt") == 0) return LANGUAGE_KOTLIN;
    if (strcmp(type_str, "javascript") == 0 || strcmp(type_str, "js") == 0) return LANGUAGE_JAVASCRIPT;
    if (strcmp(type_str, "typescript") == 0 || strcmp(type_str, "ts") == 0) return LANGUAGE_TYPESCRIPT;
    if (strcmp(type_str, "python") == 0 || strcmp(type_str, "py") == 0) return LANGUAGE_PYTHON;
    if (strcmp(type_str, "rust") == 0 || strcmp(type_str, "rs") == 0) return LANGUAGE_RUST;

    return LANGUAGE_NONE;
}

const char* language_type_to_string(LanguageType type) {
    switch (type) {
        case LANGUAGE_TCL: return "tcl";
        case LANGUAGE_LIMBO: return "limbo";
        case LANGUAGE_C: return "c";
        case LANGUAGE_KOTLIN: return "kotlin";
        case LANGUAGE_JAVASCRIPT: return "javascript";
        case LANGUAGE_TYPESCRIPT: return "typescript";
        case LANGUAGE_PYTHON: return "python";
        case LANGUAGE_RUST: return "rust";
        case LANGUAGE_NONE: return "none";
        default: return "unknown";
    }
}

bool language_type_is_valid(LanguageType type) {
    return type >= LANGUAGE_TCL && type <= LANGUAGE_RUST;
}

// ============================================================================
// Registry API Implementation
// ============================================================================

bool language_register(const LanguageProfile* profile) {
    if (!profile) {
        fprintf(stderr, "Error: Cannot register NULL language profile\n");
        return false;
    }

    if (g_registry.count >= MAX_REGISTERED_LANGUAGES) {
        fprintf(stderr, "Error: Language registry full (max %d)\n", MAX_REGISTERED_LANGUAGES);
        return false;
    }

    // Check for duplicate
    for (int i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.profiles[i]->name, profile->name) == 0) {
            fprintf(stderr, "Warning: Language '%s' already registered\n", profile->name);
            return false;
        }
    }

    g_registry.profiles[g_registry.count++] = profile;
    return true;
}

const LanguageProfile* language_get_profile(const char* name) {
    if (!name) return NULL;

    for (int i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.profiles[i]->name, name) == 0) {
            return g_registry.profiles[i];
        }
    }

    return NULL;
}

const LanguageProfile* language_get_profile_by_type(LanguageType type) {
    for (int i = 0; i < g_registry.count; i++) {
        if (g_registry.profiles[i]->type == type) {
            return g_registry.profiles[i];
        }
    }

    return NULL;
}

bool language_is_registered(const char* name) {
    return language_get_profile(name) != NULL;
}

size_t language_get_all_registered(const LanguageProfile** out_profiles, size_t max_count) {
    if (!out_profiles) return 0;

    size_t count = g_registry.count;
    if (count > max_count) count = max_count;

    memcpy(out_profiles, g_registry.profiles, count * sizeof(const LanguageProfile*));
    return count;
}

char* language_escape_string(const LanguageProfile* profile, const char* input) {
    if (!profile || !profile->escape_string) {
        // Default: just duplicate the string
        return input ? strdup(input) : strdup("");
    }

    return profile->escape_string(profile, input);
}

char* language_make_identifier(const LanguageProfile* profile, const char* input) {
    if (!input) return strdup("");
    if (!profile) return strdup(input);

    // Allocate result (same size as input is enough)
    size_t len = strlen(input);
    char* result = malloc(len + 1);
    if (!result) return NULL;

    // Convert to valid identifier
    size_t j = 0;
    for (size_t i = 0; i < len && j < len; i++) {
        if (i == 0) {
            // First character: letter or underscore
            if (isalpha(input[i]) || input[i] == '_') {
                result[j++] = input[i];
            } else if (isdigit(input[i])) {
                result[j++] = '_';
                result[j++] = input[i];
            }
        } else {
            // Subsequent characters: letter, digit, or underscore
            if (isalnum(input[i]) || input[i] == '_') {
                result[j++] = input[i];
            } else if (input[i] == ' ') {
                result[j++] = '_';
            }
        }
    }

    // Handle empty result
    if (j == 0) {
        strcpy(result, "_");
        j = 1;
    } else {
        result[j] = '\0';
    }

    return result;
}

// ============================================================================
// Initialization
// ============================================================================

void language_registry_init(void) {
    if (g_registry.initialized) {
        return;  // Already initialized
    }

    // Register built-in languages
    language_register(&tcl_profile);
    language_register(&limbo_profile);
    language_register(&c_profile);
    language_register(&kotlin_profile);
    language_register(&javascript_profile);
    language_register(&typescript_profile);

    g_registry.initialized = true;
}

void language_registry_cleanup(void) {
    memset(&g_registry, 0, sizeof(g_registry));
}
