/**
 * C Source Parser Implementation
 * Compiles and executes C code using Kryon C API to generate KIR
 */

#define _POSIX_C_SOURCE 200809L

#include "c_parser.h"
#include "../include/ir_serialization.h"
#include "../../../bindings/c/kryon.h"
#include "../../src/utils/ir_c_metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>

// External reference to metadata (defined in kryon.c)
extern CSourceMetadata g_c_metadata;

static char cached_compiler_path[512] = "";

/**
 * Find Kryon root directory using a fallback chain
 * Checks environment variable, then standard install locations
 */
static char* find_kryon_root(void) {
    static char cached_path[1024] = "";

    // Return cached result if available
    if (cached_path[0] != '\0') {
        return cached_path;
    }

    // 1. Check KRYON_ROOT environment variable
    const char* env_root = getenv("KRYON_ROOT");
    if (env_root && strlen(env_root) > 0) {
        strncpy(cached_path, env_root, sizeof(cached_path) - 1);
        cached_path[sizeof(cached_path) - 1] = '\0';
        return cached_path;
    }

    // 2. Check XDG user data directory
    const char* home = getenv("HOME");
    if (home) {
        char xdg_path[1024];
        snprintf(xdg_path, sizeof(xdg_path), "%s/.local/share/kryon", home);
        if (access(xdg_path, F_OK) == 0) {
            strncpy(cached_path, xdg_path, sizeof(cached_path) - 1);
            cached_path[sizeof(cached_path) - 1] = '\0';
            return cached_path;
        }
    }

    // 3. Check system-wide location
    if (access("/usr/local/share/kryon", F_OK) == 0) {
        strncpy(cached_path, "/usr/local/share/kryon", sizeof(cached_path) - 1);
        cached_path[sizeof(cached_path) - 1] = '\0';
        return cached_path;
    }

    // 4. Walk up parent directories from current working directory
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        char check_path[1024];
        strncpy(check_path, cwd, sizeof(check_path) - 1);
        check_path[sizeof(check_path) - 1] = '\0';

        // Walk up to 5 levels
        for (int depth = 0; depth < 5; depth++) {
            // Check for ir/ directory (characteristic of Kryon root)
            char test_path[1024];
            snprintf(test_path, sizeof(test_path), "%s/ir/ir_core.h", check_path);
            if (access(test_path, F_OK) == 0) {
                strncpy(cached_path, check_path, sizeof(cached_path) - 1);
                cached_path[sizeof(cached_path) - 1] = '\0';
                return cached_path;
            }

            // Move to parent
            char* last_slash = strrchr(check_path, '/');
            if (last_slash && last_slash != check_path) {
                *last_slash = '\0';
            } else {
                break;
            }
        }
    }

    return NULL;
}

/**
 * Find C compiler (gcc or clang) in PATH or common locations
 */
static const char* find_c_compiler(void) {
    if (cached_compiler_path[0] != '\0') {
        return cached_compiler_path;
    }

    // Try gcc first
    FILE* fp = popen("which gcc 2>/dev/null", "r");
    if (fp) {
        if (fgets(cached_compiler_path, sizeof(cached_compiler_path), fp)) {
            char* nl = strchr(cached_compiler_path, '\n');
            if (nl) *nl = '\0';

            pclose(fp);
            if (cached_compiler_path[0] != '\0' && access(cached_compiler_path, X_OK) == 0) {
                return cached_compiler_path;
            }
        }
        pclose(fp);
    }

    // Try clang
    fp = popen("which clang 2>/dev/null", "r");
    if (fp) {
        if (fgets(cached_compiler_path, sizeof(cached_compiler_path), fp)) {
            char* nl = strchr(cached_compiler_path, '\n');
            if (nl) *nl = '\0';

            pclose(fp);
            if (cached_compiler_path[0] != '\0' && access(cached_compiler_path, X_OK) == 0) {
                return cached_compiler_path;
            }
        }
        pclose(fp);
    }

    // Try common locations
    const char* paths[] = {
        "/usr/bin/gcc",
        "/usr/bin/clang",
        "/usr/local/bin/gcc",
        "/usr/local/bin/clang",
        NULL
    };

    for (int i = 0; paths[i]; i++) {
        if (access(paths[i], X_OK) == 0) {
            strncpy(cached_compiler_path, paths[i], sizeof(cached_compiler_path) - 1);
            return cached_compiler_path;
        }
    }

    return NULL;
}

bool ir_c_check_compiler_available(void) {
    return find_c_compiler() != NULL;
}

// ============================================================================
// Automatic C Source Parser for Round-Trip Conversion
// ============================================================================

/**
 * Skip whitespace and comments
 */
static const char* skip_whitespace(const char* p) {
    while (*p) {
        // Skip spaces, tabs, newlines
        if (isspace(*p)) {
            p++;
            continue;
        }

        // Skip line comments //
        if (p[0] == '/' && p[1] == '/') {
            p += 2;
            while (*p && *p != '\n') p++;
            continue;
        }

        // Skip block comments /* */
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) p++;
            if (*p) p += 2;
            continue;
        }

        break;
    }
    return p;
}

/**
 * Parse include statement
 */
static const char* parse_include(const char* line, int line_num) {
    const char* p = line;
    p = skip_whitespace(p);

    if (*p != '#') return NULL;
    p++;
    p = skip_whitespace(p);

    if (strncmp(p, "include", 7) != 0) return NULL;
    p += 7;
    p = skip_whitespace(p);

    bool is_system = (*p == '<');
    char include_str[512];

    if (is_system) {
        // System include <...>
        const char* start = p;
        const char* end = strchr(p, '>');
        if (!end) return NULL;
        size_t len = (end - start) + 1;
        if (len >= sizeof(include_str)) return NULL;
        memcpy(include_str, start, len);
        include_str[len] = '\0';
        kryon_add_include(include_str, true, line_num);
        return end + 1;
    } else if (*p == '"') {
        // Local include "..."
        const char* start = p;
        const char* end = strchr(p + 1, '"');
        if (!end) return NULL;
        size_t len = (end - start) + 1;
        if (len >= sizeof(include_str)) return NULL;
        memcpy(include_str, start, len);
        include_str[len] = '\0';
        kryon_add_include(include_str, false, line_num);
        return end + 1;
    }

    return NULL;
}

/**
 * Parse preprocessor directive
 */
static const char* parse_preprocessor(const char* line, int line_num) {
    const char* p = line;
    p = skip_whitespace(p);

    if (*p != '#') return NULL;
    p++;
    p = skip_whitespace(p);

    // Check for #include (handled separately)
    if (strncmp(p, "include", 7) == 0) {
        return parse_include(line, line_num);
    }

    // Parse directive type
    char directive[32];
    int i = 0;
    while (*p && isalpha(*p) && i < 31) {
        directive[i++] = *p++;
    }
    directive[i] = '\0';

    if (strlen(directive) == 0) return NULL;

    p = skip_whitespace(p);

    // Parse condition/name and value
    char condition[256] = "";
    char value[512] = "";

    // For #define NAME VALUE
    if (strcmp(directive, "define") == 0) {
        // Get name
        i = 0;
        while (*p && (isalnum(*p) || *p == '_') && i < 255) {
            condition[i++] = *p++;
        }
        condition[i] = '\0';

        p = skip_whitespace(p);

        // Get value (rest of line)
        i = 0;
        while (*p && *p != '\n' && i < 511) {
            value[i++] = *p++;
        }
        value[i] = '\0';

        // Trim trailing whitespace from value
        i--;
        while (i >= 0 && isspace((unsigned char)value[i])) {
            value[i--] = '\0';
        }
    } else {
        // For #ifdef, #ifndef, #if, etc. - get condition
        i = 0;
        while (*p && *p != '\n' && i < 255) {
            condition[i++] = *p++;
        }
        condition[i] = '\0';

        // Trim trailing whitespace
        i--;
        while (i >= 0 && isspace((unsigned char)condition[i])) {
            condition[i--] = '\0';
        }
    }

    kryon_add_preprocessor_directive(directive,
                                      strlen(condition) > 0 ? condition : NULL,
                                      strlen(value) > 0 ? value : NULL,
                                      line_num);

    return p;
}

/**
 * Extract function body by finding matching braces
 */
static const char* extract_function_body(const char* start, char* body, size_t body_size) {
    const char* p = start;
    p = skip_whitespace(p);

    if (*p != '{') return NULL;

    int brace_count = 0;
    const char* body_start = p + 1;  // Skip opening brace
    p++;

    while (*p) {
        if (*p == '{') {
            brace_count++;
        } else if (*p == '}') {
            if (brace_count == 0) {
                // Found closing brace
                size_t len = p - body_start;
                if (len >= body_size) len = body_size - 1;
                memcpy(body, body_start, len);
                body[len] = '\0';
                return p + 1;
            }
            brace_count--;
        } else if (*p == '"') {
            // Skip string literals
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1]) p++;
                p++;
            }
        } else if (*p == '\'') {
            // Skip character literals
            p++;
            if (*p == '\\' && p[1]) p++;
            if (*p) p++;
            if (*p == '\'') p++;
            continue;
        }
        p++;
    }

    return NULL;  // Unmatched braces
}

/**
 * Parse function declaration/definition
 */
static const char* parse_function(const char* line, int line_num, const char* source_end) {
    const char* p = line;
    p = skip_whitespace(p);

    // Parse return type
    char return_type[128];
    int i = 0;
    while (*p && (isalnum(*p) || *p == '_' || *p == '*' || isspace(*p)) && *p != '(') {
        if (!isspace(*p)) {
            return_type[i++] = *p;
        } else if (i > 0 && return_type[i-1] != ' ') {
            return_type[i++] = ' ';
        }
        p++;
    }

    if (i == 0 || *p != '(') return NULL;

    // Trim trailing space from return type
    while (i > 0 && return_type[i-1] == ' ') i--;
    return_type[i] = '\0';

    // Extract function name from return type
    char* last_space = strrchr(return_type, ' ');
    char function_name[128];
    if (last_space) {
        strcpy(function_name, last_space + 1);
        *last_space = '\0';  // Remove name from return_type
    } else {
        // No return type specified, whole thing is name
        strcpy(function_name, return_type);
        strcpy(return_type, "void");
    }

    // Parse parameters
    p++;  // Skip '('
    const char* param_start = p;
    int paren_count = 1;
    while (*p && paren_count > 0) {
        if (*p == '(') paren_count++;
        else if (*p == ')') paren_count--;
        if (paren_count > 0) p++;
    }

    if (paren_count != 0) return NULL;

    char parameters[256];
    size_t param_len = p - param_start;
    if (param_len >= sizeof(parameters)) param_len = sizeof(parameters) - 1;
    memcpy(parameters, param_start, param_len);
    parameters[param_len] = '\0';

    p++;  // Skip ')'
    p = skip_whitespace(p);

    // Check if this is a declaration (;) or definition ({)
    if (*p == ';') {
        // Just a declaration, skip it
        return p + 1;
    }

    if (*p != '{') return NULL;

    // Extract function body
    char body[4096];
    const char* after_body = extract_function_body(p, body, sizeof(body));
    if (!after_body) return NULL;

    // Determine if this is main(), event handler, or helper function
    if (strcmp(function_name, "main") == 0) {
        // Skip main function - it will be regenerated
        return after_body;
    }

    // Check if it's an event handler (starts with "on_" or contains "handler")
    bool is_event_handler = (strncmp(function_name, "on_", 3) == 0) ||
                            (strstr(function_name, "handler") != NULL) ||
                            (strstr(function_name, "callback") != NULL);

    if (is_event_handler) {
        // Register as event handler
        // Logic ID will be assigned later when we see it used in ON_CLICK, etc.
        kryon_register_event_handler("", function_name, return_type, parameters, body, line_num);
    } else {
        // Register as helper function
        kryon_register_helper_function(function_name, return_type, parameters, body, line_num);
    }

    return after_body;
}

/**
 * Parse variable declaration
 */
static const char* parse_variable_declaration(const char* line, int line_num) {
    const char* p = line;
    p = skip_whitespace(p);

    // Check for storage class
    char storage[32] = "";
    if (strncmp(p, "static", 6) == 0 && isspace(p[6])) {
        strcpy(storage, "static");
        p += 6;
        p = skip_whitespace(p);
    } else if (strncmp(p, "extern", 6) == 0 && isspace(p[6])) {
        strcpy(storage, "extern");
        p += 6;
        p = skip_whitespace(p);
    }

    // Parse type (look for IRComponent*)
    if (strncmp(p, "IRComponent", 11) != 0) return NULL;
    p += 11;
    p = skip_whitespace(p);
    if (*p != '*') return NULL;
    p++;
    p = skip_whitespace(p);

    // Parse variable name
    char var_name[128];
    int i = 0;
    while (*p && (isalnum(*p) || *p == '_') && i < 127) {
        var_name[i++] = *p++;
    }
    var_name[i] = '\0';

    if (strlen(var_name) == 0) return NULL;

    p = skip_whitespace(p);

    // Check for initialization
    char initial_value[256] = "NULL";
    if (*p == '=') {
        p++;
        p = skip_whitespace(p);

        // Extract initial value (up to semicolon)
        i = 0;
        while (*p && *p != ';' && i < 255) {
            if (!isspace(*p) || (i > 0 && initial_value[i-1] != ' ')) {
                initial_value[i++] = *p;
            }
            p++;
        }
        // Trim trailing space
        while (i > 0 && initial_value[i-1] == ' ') i--;
        initial_value[i] = '\0';
    }

    // Register variable (component_id will be linked later)
    kryon_register_variable(var_name, "IRComponent*",
                            strlen(storage) > 0 ? storage : NULL,
                            initial_value, 0, line_num);

    return p;
}

/**
 * Parse C source and extract all metadata
 */
static void parse_c_source_metadata(const char* source, const char* filepath) {
    if (!source || !filepath) return;

    // Set main source file
    const char* filename = strrchr(filepath, '/');
    if (filename) {
        kryon_set_main_source_file(filename + 1);
    } else {
        kryon_set_main_source_file(filepath);
    }

    // Add source file to metadata
    kryon_add_source_file(filename ? filename + 1 : filepath, filepath, source);

    // Parse line by line
    const char* p = source;
    int line_num = 1;

    while (*p) {
        const char* line_start = p;

        // Skip to start of meaningful content
        p = skip_whitespace(p);

        // Empty line
        if (*p == '\n' || *p == '\0') {
            if (*p == '\n') {
                line_num++;
                p++;
            }
            continue;
        }

        // Try parsing different constructs
        const char* after = NULL;

        // 1. Preprocessor directives
        if (*p == '#') {
            after = parse_preprocessor(line_start, line_num);
        }
        // 2. Variable declarations
        else if ((after = parse_variable_declaration(line_start, line_num)) != NULL) {
            // Successfully parsed variable
        }
        // 3. Function definitions
        else if ((after = parse_function(line_start, line_num, source + strlen(source))) != NULL) {
            // Successfully parsed function
        }

        // Move to next line if we couldn't parse
        if (!after) {
            while (*p && *p != '\n') p++;
        } else {
            p = after;
        }

        if (*p == '\n') {
            line_num++;
            p++;
        }
    }
}

/**
 * Parse C source to KIR JSON by compiling and executing it
 */
char* ir_c_to_kir(const char* source, size_t length) {
    if (!source) return NULL;

    if (length == 0) {
        length = strlen(source);
    }

    // Check for compiler
    const char* compiler = find_c_compiler();
    if (!compiler) {
        fprintf(stderr, "Error: C compiler not found (tried gcc, clang)\n");
        fprintf(stderr, "Install a C compiler to parse C files.\n");
        return NULL;
    }

    // Get Kryon root to find bindings
    char* kryon_root = find_kryon_root();

    if (!kryon_root) {
        fprintf(stderr, "Error: Could not locate Kryon root directory\n");
        fprintf(stderr, "Set KRYON_ROOT environment variable: export KRYON_ROOT=/path/to/kryon\n");
        fprintf(stderr, "Or run 'make install' in the kryon/cli directory\n");
        return NULL;
    }

    // Create temp directory for compilation
    const char* tmp_dir = getenv("TMPDIR");
    if (!tmp_dir) tmp_dir = "/tmp";

    char temp_base[1024];
    snprintf(temp_base, sizeof(temp_base), "%s/kryon_c_XXXXXX", tmp_dir);

    char* temp_dir = mkdtemp(temp_base);
    if (!temp_dir) {
        perror("mkdtemp");
        return NULL;
    }

    // Write source to temp file
    char src_file[1200];
    snprintf(src_file, sizeof(src_file), "%s/main.c", temp_dir);

    FILE* f = fopen(src_file, "w");
    if (!f) {
        perror("fopen");
        rmdir(temp_dir);
        return NULL;
    }

    // Write original source
    fwrite(source, 1, length, f);

    // Inject metadata registration calls at the end (before main is executed)
    // These will be called via a constructor function
    fprintf(f, "\n\n// Auto-generated metadata registration\n");
    fprintf(f, "__attribute__((constructor)) static void __kryon_register_metadata(void) {\n");

    // Register includes
    for (size_t i = 0; i < g_c_metadata.include_count; i++) {
        fprintf(f, "    kryon_add_include(\"%s\", %s, %d);\n",
                g_c_metadata.includes[i].include_string,
                g_c_metadata.includes[i].is_system ? "true" : "false",
                g_c_metadata.includes[i].line_number);
    }

    // Register variables
    for (size_t i = 0; i < g_c_metadata.variable_count; i++) {
        fprintf(f, "    kryon_register_variable(\"%s\", \"%s\", %s, \"%s\", 0, %d);\n",
                g_c_metadata.variables[i].name,
                g_c_metadata.variables[i].type,
                g_c_metadata.variables[i].storage ? "\"static\"" : "NULL",
                g_c_metadata.variables[i].initial_value,
                g_c_metadata.variables[i].line_number);
    }

    // Register event handlers
    for (size_t i = 0; i < g_c_metadata.event_handler_count; i++) {
        // Escape the body string
        fprintf(f, "    kryon_register_event_handler(\"%s\", \"%s\", \"%s\", \"%s\", ",
                g_c_metadata.event_handlers[i].logic_id,
                g_c_metadata.event_handlers[i].function_name,
                g_c_metadata.event_handlers[i].return_type,
                g_c_metadata.event_handlers[i].parameters);
        // Write body as string literal
        fprintf(f, "\"");
        const char* body = g_c_metadata.event_handlers[i].body;
        while (*body) {
            if (*body == '\"') fprintf(f, "\\\"");
            else if (*body == '\\') fprintf(f, "\\\\");
            else if (*body == '\n') fprintf(f, "\\n");
            else if (*body == '\t') fprintf(f, "\\t");
            else fputc(*body, f);
            body++;
        }
        fprintf(f, "\", %d);\n", g_c_metadata.event_handlers[i].line_number);
    }

    // Register helper functions
    for (size_t i = 0; i < g_c_metadata.helper_function_count; i++) {
        fprintf(f, "    kryon_register_helper_function(\"%s\", \"%s\", \"%s\", ",
                g_c_metadata.helper_functions[i].name,
                g_c_metadata.helper_functions[i].return_type,
                g_c_metadata.helper_functions[i].parameters);
        // Write body as string literal
        fprintf(f, "\"");
        const char* body = g_c_metadata.helper_functions[i].body;
        while (*body) {
            if (*body == '\"') fprintf(f, "\\\"");
            else if (*body == '\\') fprintf(f, "\\\\");
            else if (*body == '\n') fprintf(f, "\\n");
            else if (*body == '\t') fprintf(f, "\\t");
            else fputc(*body, f);
            body++;
        }
        fprintf(f, "\", %d);\n", g_c_metadata.helper_functions[i].line_number);
    }

    fprintf(f, "}\n");
    fclose(f);

    // Compile the C source
    char exe_file[1200];
    char kir_file[1200];
    snprintf(exe_file, sizeof(exe_file), "%s/app", temp_dir);
    snprintf(kir_file, sizeof(kir_file), "%s/output.kir", temp_dir);

    char compile_cmd[4096];
    snprintf(compile_cmd, sizeof(compile_cmd),
             "%s \"%s\" -DKRYON_KIR_ONLY -I\"%s/bindings/c\" -I\"%s/ir\" -I\"%s/runtime/desktop\" "
             "-I\"%s/third_party/cJSON\" -L\"%s/build\" -L\"%s/bindings/c\" "
             "-Wl,-rpath,%s/build -lkryon_c -lkryon_ir -lm -o \"%s\" 2>&1",
             compiler, src_file, kryon_root, kryon_root, kryon_root,
             kryon_root,
             kryon_root, kryon_root, kryon_root, exe_file);

    FILE* pipe = popen(compile_cmd, "r");
    if (!pipe) {
        fprintf(stderr, "Error: Failed to run compiler\n");
        unlink(src_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Capture compilation output
    char compile_output[4096] = "";
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        strncat(compile_output, buffer, sizeof(compile_output) - strlen(compile_output) - 1);
    }

    int compile_status = pclose(pipe);
    if (compile_status != 0) {
        fprintf(stderr, "Error: C compilation failed\n");
        if (strlen(compile_output) > 0) {
            fprintf(stderr, "%s", compile_output);
        }
        unlink(src_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Execute the compiled program
    char exec_cmd[2048];
    snprintf(exec_cmd, sizeof(exec_cmd),
             "cd \"%s\" && LD_LIBRARY_PATH=\"%s/build:%s/bindings/c:$LD_LIBRARY_PATH\" "
             "\"%s\" 2>&1",
             temp_dir, kryon_root, kryon_root, exe_file);

    pipe = popen(exec_cmd, "r");
    if (!pipe) {
        fprintf(stderr, "Error: Failed to execute compiled program\n");
        unlink(src_file);
        unlink(exe_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Capture execution output
    char exec_output[4096] = "";
    while (fgets(buffer, sizeof(buffer), pipe)) {
        strncat(exec_output, buffer, sizeof(exec_output) - strlen(exec_output) - 1);
    }

    int exec_status = pclose(pipe);
    if (exec_status != 0) {
        fprintf(stderr, "Error: Program execution failed\n");
        if (strlen(exec_output) > 0) {
            fprintf(stderr, "%s", exec_output);
        }
        unlink(src_file);
        unlink(exe_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Read the generated KIR file
    FILE* kir_f = fopen(kir_file, "r");
    if (!kir_f) {
        fprintf(stderr, "Error: KIR output file not found\n");
        fprintf(stderr, "The C program must call kryon_finalize(\"%s\") to generate output\n", kir_file);
        unlink(src_file);
        unlink(exe_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Get file size
    fseek(kir_f, 0, SEEK_END);
    long size = ftell(kir_f);
    fseek(kir_f, 0, SEEK_SET);

    // Read content
    char* result = malloc(size + 1);
    if (!result) {
        fclose(kir_f);
        unlink(src_file);
        unlink(exe_file);
        unlink(kir_file);
        rmdir(temp_dir);
        return NULL;
    }

    size_t read_bytes = fread(result, 1, size, kir_f);
    result[read_bytes] = '\0';
    fclose(kir_f);

    // Cleanup
    unlink(src_file);
    unlink(exe_file);
    unlink(kir_file);
    rmdir(temp_dir);

    return result;
}

/**
 * Parse C file to KIR JSON
 */
char* ir_c_file_to_kir(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        perror(filepath);
        return NULL;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Read file
    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        return NULL;
    }

    size_t read_size = fread(source, 1, size, f);
    source[read_size] = '\0';
    fclose(f);

    // AUTOMATIC METADATA EXTRACTION - parse source to capture all metadata
    // This happens BEFORE compilation, automatically capturing:
    // - Variable declarations
    // - Event handlers
    // - Helper functions
    // - Include statements
    // - Preprocessor directives
    parse_c_source_metadata(source, filepath);

    // Parse and compile
    char* result = ir_c_to_kir(source, read_size);
    free(source);

    return result;
}
