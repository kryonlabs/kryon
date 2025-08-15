/**
 * @file runtime_compiler.c
 * @brief Runtime compilation implementation for on-the-fly .kry to .krb compilation.
 *
 * This file implements the runtime compiler for compiling .kry files
 * during navigation, enabling live development and seamless user experiences.
 *
 * 0BSD License
 */

#include "runtime_compiler.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// Default configuration
#define KRYON_COMPILE_DEFAULT_TEMP_DIR "/tmp/kryon_compile"
#define KRYON_COMPILE_MAX_DIAGNOSTICS 100

// Global compiler state
static struct {
    bool initialized;
    KryonCompileOptions default_options;
} g_compiler_state = {0};

// =============================================================================
//  Private Helper Functions
// =============================================================================

static bool ensure_directory(const char* path);
static char* read_file_content(const char* path);
static bool write_file_content(const char* path, const char* content);
static CompileDiagnostic* create_diagnostic(int line, int column, int severity, const char* message);
static void destroy_diagnostic(CompileDiagnostic* diagnostic);
static time_t get_file_mtime(const char* path);
static char* get_file_basename(const char* path);
static char* get_file_directory(const char* path);

// Compiler integration functions
static KryonCompileResult invoke_kryon_compiler(const char* source_path, const char* output_path, const KryonCompileOptions* options);
static KryonCompileResult parse_compiler_output(const char* output, KryonCompileContext* context);

// =============================================================================
//  Runtime Compiler API Implementation
// =============================================================================

bool kryon_runtime_compiler_init(void) {
    if (g_compiler_state.initialized) {
        return true;
    }
    
    // Set up default options
    g_compiler_state.default_options.verbose = false;
    g_compiler_state.default_options.debug = false;
    g_compiler_state.default_options.optimize = false;
    g_compiler_state.default_options.cache_enabled = true;
    g_compiler_state.default_options.output_dir = NULL;
    g_compiler_state.default_options.temp_dir = KRYON_COMPILE_DEFAULT_TEMP_DIR;
    
    // Ensure temporary directory exists
    if (!ensure_directory(KRYON_COMPILE_DEFAULT_TEMP_DIR)) {
        printf("❌ Failed to create compiler temp directory: %s\n", KRYON_COMPILE_DEFAULT_TEMP_DIR);
        return false;
    }
    
    g_compiler_state.initialized = true;
    printf("⚡ Runtime compiler initialized\n");
    return true;
}

void kryon_runtime_compiler_shutdown(void) {
    if (!g_compiler_state.initialized) {
        return;
    }
    
    g_compiler_state.initialized = false;
    printf("⚡ Runtime compiler shutdown\n");
}

KryonCompileContext* kryon_compile_context_create(void) {
    KryonCompileContext* context = kryon_alloc_type(KryonCompileContext);
    if (!context) return NULL;
    
    context->options = g_compiler_state.default_options;
    context->diagnostics = NULL;
    context->diagnostic_count = 0;
    context->has_errors = false;
    context->source_path = NULL;
    context->output_path = NULL;
    context->start_time = 0;
    context->end_time = 0;
    
    return context;
}

void kryon_compile_context_destroy(KryonCompileContext* context) {
    if (!context) return;
    
    // Clear diagnostics
    kryon_compile_clear_diagnostics(context);
    
    // Free paths
    kryon_free(context->source_path);
    kryon_free(context->output_path);
    
    kryon_free(context);
}

KryonCompileResult kryon_compile_file(const char* source_path, const char* output_path, const KryonCompileOptions* options) {
    if (!g_compiler_state.initialized) {
        printf("❌ Runtime compiler not initialized\n");
        return KRYON_COMPILE_ERROR_MEMORY;
    }
    
    if (!source_path || !output_path) {
        return KRYON_COMPILE_ERROR_INVALID_SYNTAX;
    }
    
    // Use default options if none provided
    const KryonCompileOptions* compile_options = options ? options : &g_compiler_state.default_options;
    
    printf("⚡ Compiling: %s -> %s\n", source_path, output_path);
    
    // Check if source file exists
    if (access(source_path, R_OK) != 0) {
        printf("❌ Source file not found: %s\n", source_path);
        return KRYON_COMPILE_ERROR_FILE_NOT_FOUND;
    }
    
    // Ensure output directory exists
    char* output_dir = get_file_directory(output_path);
    if (output_dir) {
        ensure_directory(output_dir);
        kryon_free(output_dir);
    }
    
    // Invoke the actual Kryon compiler
    return invoke_kryon_compiler(source_path, output_path, compile_options);
}

KryonCompileResult kryon_compile_file_detailed(KryonCompileContext* context, const char* source_path, const char* output_path) {
    if (!context) return KRYON_COMPILE_ERROR_MEMORY;
    
    // Store paths in context
    kryon_free(context->source_path);
    kryon_free(context->output_path);
    context->source_path = kryon_strdup(source_path);
    context->output_path = kryon_strdup(output_path);
    
    // Clear previous diagnostics
    kryon_compile_clear_diagnostics(context);
    context->has_errors = false;
    
    // Record start time
    context->start_time = time(NULL);
    
    // Perform compilation
    KryonCompileResult result = kryon_compile_file(source_path, output_path, &context->options);
    
    // Record end time
    context->end_time = time(NULL);
    
    // Update error status
    if (result != KRYON_COMPILE_SUCCESS) {
        context->has_errors = true;
        kryon_compile_add_diagnostic(context, 0, 0, 2, kryon_compile_result_string(result));
    }
    
    return result;
}

KryonCompileResult kryon_compile_string(const char* source_code, const char* output_path, const KryonCompileOptions* options) {
    if (!source_code || !output_path) {
        return KRYON_COMPILE_ERROR_INVALID_SYNTAX;
    }
    
    // Create temporary source file
    char temp_source[256];
    snprintf(temp_source, sizeof(temp_source), "%s/temp_source_%ld.kry", 
             KRYON_COMPILE_DEFAULT_TEMP_DIR, time(NULL));
    
    if (!write_file_content(temp_source, source_code)) {
        return KRYON_COMPILE_ERROR_OUTPUT_FAILED;
    }
    
    // Compile the temporary file
    KryonCompileResult result = kryon_compile_file(temp_source, output_path, options);
    
    // Clean up temporary file
    unlink(temp_source);
    
    return result;
}

bool kryon_compile_needs_update(const char* kry_path, const char* krb_path) {
    if (!kry_path || !krb_path) return true;
    
    // Check if KRB file exists
    if (access(krb_path, R_OK) != 0) {
        return true; // KRB doesn't exist, needs compilation
    }
    
    // Compare modification times
    time_t kry_mtime = get_file_mtime(kry_path);
    time_t krb_mtime = get_file_mtime(krb_path);
    
    return kry_mtime > krb_mtime;
}

// =============================================================================
//  Diagnostic and Error Handling Implementation
// =============================================================================

void kryon_compile_add_diagnostic(KryonCompileContext* context, int line, int column, int severity, const char* message) {
    if (!context || !message || context->diagnostic_count >= KRYON_COMPILE_MAX_DIAGNOSTICS) {
        return;
    }
    
    CompileDiagnostic* diagnostic = create_diagnostic(line, column, severity, message);
    if (!diagnostic) return;
    
    // Add to front of list
    diagnostic->next = context->diagnostics;
    context->diagnostics = diagnostic;
    context->diagnostic_count++;
    
    if (severity >= 2) { // Error
        context->has_errors = true;
    }
}

CompileDiagnostic* kryon_compile_get_first_error(KryonCompileContext* context) {
    if (!context) return NULL;
    
    CompileDiagnostic* diagnostic = context->diagnostics;
    while (diagnostic) {
        if (diagnostic->severity >= 2) {
            return diagnostic;
        }
        diagnostic = diagnostic->next;
    }
    return NULL;
}

void kryon_compile_print_diagnostics(KryonCompileContext* context) {
    if (!context || !context->diagnostics) {
        printf("✅ No compilation diagnostics\n");
        return;
    }
    
    printf("📋 Compilation Diagnostics (%zu):\n", context->diagnostic_count);
    
    CompileDiagnostic* diagnostic = context->diagnostics;
    while (diagnostic) {
        const char* severity_str = "";
        const char* severity_icon = "";
        
        switch (diagnostic->severity) {
            case 0: severity_str = "INFO"; severity_icon = "ℹ️"; break;
            case 1: severity_str = "WARN"; severity_icon = "⚠️"; break;
            case 2: severity_str = "ERROR"; severity_icon = "❌"; break;
            default: severity_str = "UNKNOWN"; severity_icon = "❓"; break;
        }
        
        if (diagnostic->line > 0) {
            printf("  %s %s [%d:%d] %s\n", severity_icon, severity_str, 
                   diagnostic->line, diagnostic->column, diagnostic->message);
        } else {
            printf("  %s %s %s\n", severity_icon, severity_str, diagnostic->message);
        }
        
        diagnostic = diagnostic->next;
    }
}

void kryon_compile_clear_diagnostics(KryonCompileContext* context) {
    if (!context) return;
    
    CompileDiagnostic* diagnostic = context->diagnostics;
    while (diagnostic) {
        CompileDiagnostic* next = diagnostic->next;
        destroy_diagnostic(diagnostic);
        diagnostic = next;
    }
    
    context->diagnostics = NULL;
    context->diagnostic_count = 0;
    context->has_errors = false;
}

// =============================================================================
//  Utility Functions Implementation
// =============================================================================

const char* kryon_compile_result_string(KryonCompileResult result) {
    switch (result) {
        case KRYON_COMPILE_SUCCESS: return "Compilation successful";
        case KRYON_COMPILE_ERROR_FILE_NOT_FOUND: return "Source file not found";
        case KRYON_COMPILE_ERROR_PARSE_FAILED: return "Parse error in source file";
        case KRYON_COMPILE_ERROR_CODEGEN_FAILED: return "Code generation failed";
        case KRYON_COMPILE_ERROR_OUTPUT_FAILED: return "Failed to write output file";
        case KRYON_COMPILE_ERROR_MEMORY: return "Memory allocation failed";
        case KRYON_COMPILE_ERROR_INVALID_SYNTAX: return "Invalid syntax in source file";
        default: return "Unknown compilation error";
    }
}

KryonCompileOptions kryon_compile_get_default_options(void) {
    return g_compiler_state.default_options;
}

bool kryon_compile_validate_syntax(const char* source_path) {
    if (!source_path) return false;
    
    // Basic validation - check if file exists and has .kry extension
    if (access(source_path, R_OK) != 0) {
        return false;
    }
    
    const char* ext = strrchr(source_path, '.');
    if (!ext || strcmp(ext, ".kry") != 0) {
        return false;
    }
    
    // TODO: Add more sophisticated syntax validation
    return true;
}

double kryon_compile_get_time_ms(KryonCompileContext* context) {
    if (!context || context->start_time == 0 || context->end_time == 0) {
        return 0.0;
    }
    
    return difftime(context->end_time, context->start_time) * 1000.0;
}

char* kryon_compile_create_temp_path(const char* source_path, const char* temp_dir) {
    if (!source_path) return NULL;
    
    const char* dir = temp_dir ? temp_dir : KRYON_COMPILE_DEFAULT_TEMP_DIR;
    char* basename = get_file_basename(source_path);
    if (!basename) return NULL;
    
    // Remove .kry extension and add .krb
    char* dot = strrchr(basename, '.');
    if (dot) *dot = '\0';
    
    size_t path_len = strlen(dir) + strlen(basename) + 20;
    char* temp_path = kryon_alloc(path_len);
    if (temp_path) {
        snprintf(temp_path, path_len, "%s/%s_%ld.krb", dir, basename, time(NULL));
    }
    
    kryon_free(basename);
    return temp_path;
}

// =============================================================================
//  Private Helper Functions Implementation
// =============================================================================

static bool ensure_directory(const char* path) {
    if (!path) return false;
    
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) != 0) {
            return false;
        }
    }
    return true;
}

static char* read_file_content(const char* path) {
    if (!path) return NULL;
    
    FILE* file = fopen(path, "r");
    if (!file) return NULL;
    
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* content = kryon_alloc(length + 1);
    if (content) {
        fread(content, 1, length, file);
        content[length] = '\0';
    }
    
    fclose(file);
    return content;
}

static bool write_file_content(const char* path, const char* content) {
    if (!path || !content) return false;
    
    FILE* file = fopen(path, "w");
    if (!file) return false;
    
    size_t written = fwrite(content, 1, strlen(content), file);
    fclose(file);
    
    return written == strlen(content);
}

static CompileDiagnostic* create_diagnostic(int line, int column, int severity, const char* message) {
    CompileDiagnostic* diagnostic = kryon_alloc_type(CompileDiagnostic);
    if (!diagnostic) return NULL;
    
    diagnostic->line = line;
    diagnostic->column = column;
    diagnostic->severity = severity;
    diagnostic->message = kryon_strdup(message);
    diagnostic->source_snippet = NULL;
    diagnostic->next = NULL;
    
    return diagnostic;
}

static void destroy_diagnostic(CompileDiagnostic* diagnostic) {
    if (!diagnostic) return;
    
    kryon_free(diagnostic->message);
    kryon_free(diagnostic->source_snippet);
    kryon_free(diagnostic);
}

static time_t get_file_mtime(const char* path) {
    if (!path) return 0;
    
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

static char* get_file_basename(const char* path) {
    if (!path) return NULL;
    
    const char* basename = strrchr(path, '/');
    if (basename) {
        basename++; // Skip the '/'
    } else {
        basename = path;
    }
    
    return kryon_strdup(basename);
}

static char* get_file_directory(const char* path) {
    if (!path) return NULL;
    
    const char* last_slash = strrchr(path, '/');
    if (!last_slash) {
        return kryon_strdup("."); // Current directory
    }
    
    size_t dir_len = last_slash - path;
    char* directory = kryon_alloc(dir_len + 1);
    if (directory) {
        strncpy(directory, path, dir_len);
        directory[dir_len] = '\0';
    }
    
    return directory;
}

static KryonCompileResult invoke_kryon_compiler(const char* source_path, const char* output_path, const KryonCompileOptions* options) {
    // For now, create a simple implementation that calls the existing compiler
    // TODO: Integrate with the actual Kryon compiler functions
    
    printf("⚡ Invoking Kryon compiler: %s -> %s\n", source_path, output_path);
    
    // Build compiler command
    char command[2048];
    snprintf(command, sizeof(command), "./bin/kryon compile \"%s\" -o \"%s\"", source_path, output_path);
    
    if (options && options->verbose) {
        strcat(command, " --verbose");
    }
    
    if (options && options->debug) {
        strcat(command, " --debug");
    }
    
    // Execute compiler
    int result = system(command);
    
    if (result == 0) {
        printf("✅ Compilation successful: %s\n", output_path);
        return KRYON_COMPILE_SUCCESS;
    } else {
        printf("❌ Compilation failed with exit code: %d\n", result);
        return KRYON_COMPILE_ERROR_PARSE_FAILED;
    }
}

static KryonCompileResult parse_compiler_output(const char* output, KryonCompileContext* context) {
    // TODO: Parse compiler output for diagnostics
    // This would analyze the compiler's stdout/stderr to extract
    // error messages, warnings, and line numbers
    
    if (!output || !context) {
        return KRYON_COMPILE_ERROR_MEMORY;
    }
    
    // Simple implementation for now
    if (strstr(output, "error") || strstr(output, "Error") || strstr(output, "ERROR")) {
        kryon_compile_add_diagnostic(context, 0, 0, 2, "Compilation error detected");
        return KRYON_COMPILE_ERROR_PARSE_FAILED;
    }
    
    return KRYON_COMPILE_SUCCESS;
}