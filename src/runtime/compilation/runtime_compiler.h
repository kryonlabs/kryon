/**
 * @file runtime_compiler.h
 * @brief Runtime compilation interface for on-the-fly .kry to .krb compilation.
 * (Refactored to use the unified KryonResult error system)
 */

 #ifndef KRYON_RUNTIME_COMPILER_H
 #define KRYON_RUNTIME_COMPILER_H
 
 #include <stddef.h>
 #include <stdbool.h>
 #include <time.h>
 #include <sys/stat.h>
 
 // CRITICAL FIX: Include error.h to get the definition of KryonResult
 #include "error.h" 
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 // =============================================================================
 //  Types and Structures
 // =============================================================================
 
 // THIS ENUM IS NOW REMOVED. WE USE KryonResult for everything.
 // typedef enum { ... } KryonCompileResult;
 
 /**
  * @brief Compilation options and settings.
  */
 typedef struct {
     bool verbose;                    // Enable verbose compilation output
     bool debug;                      // Include debug information
     bool optimize;                   // Enable optimizations
     bool cache_enabled;              // Enable compilation caching
     const char* output_dir;          // Output directory for compiled files
     const char* temp_dir;            // Temporary directory for compilation
 } KryonCompileOptions;
 
 /**
  * @brief Compilation diagnostic information.
  */
 typedef struct CompileDiagnostic {
     int line;                        // Line number
     int column;                      // Column number
     int severity;                    // 0=info, 1=warning, 2=error
     char* message;                   // Diagnostic message
     char* source_snippet;            // Source code snippet
     struct CompileDiagnostic* next;  // Next diagnostic in list
 } CompileDiagnostic;
 
 /**
  * @brief Compilation context and state.
  */
 typedef struct {
     KryonCompileOptions options;     // Compilation options
     CompileDiagnostic* diagnostics;  // List of compilation diagnostics
     size_t diagnostic_count;         // Number of diagnostics
     bool has_errors;                 // Whether compilation has errors
     char* source_path;               // Source file path
     char* output_path;               // Output file path
     time_t start_time;               // Compilation start time
     time_t end_time;                 // Compilation end time
 } KryonCompileContext;
 
 // =============================================================================
 //  Runtime Compiler API (UPDATED TO USE KryonResult)
 // =============================================================================
 
 /**
  * @brief Initializes the runtime compiler system.
  * @return true on success, false on failure
  */
 bool kryon_runtime_compiler_init(void);
 
 /**
  * @brief Shuts down the runtime compiler system.
  */
 void kryon_runtime_compiler_shutdown(void);
 
 /**
  * @brief Creates a new compilation context with default options.
  * @return Pointer to compilation context or NULL on failure
  */
 KryonCompileContext* kryon_compile_context_create(void);
 
 /**
  * @brief Destroys a compilation context and frees all resources.
  * @param context Compilation context to destroy
  */
 void kryon_compile_context_destroy(KryonCompileContext* context);
 
 /**
  * @brief Compiles a .kry file to a .krb file.
  * @param source_path Path to source .kry file
  * @param output_path Path to output .krb file
  * @param options Compilation options (NULL for defaults)
  * @return KryonResult status code (KRYON_SUCCESS on success)
  */
 KryonResult kryon_compile_file(const char* source_path, const char* output_path, const KryonCompileOptions* options);
 
 /**
  * @brief Compiles a .kry file with detailed context and diagnostics.
  * @param context Compilation context
  * @param source_path Path to source .kry file
  * @param output_path Path to output .krb file
  * @return KryonResult status code (KRYON_SUCCESS on success)
  */
 KryonResult kryon_compile_file_detailed(KryonCompileContext* context, const char* source_path, const char* output_path);
 
 /**
  * @brief Compiles a .kry string to a .krb file.
  * @param source_code Source code string
  * @param output_path Path to output .krb file
  * @param options Compilation options (NULL for defaults)
  * @return KryonResult status code (KRYON_SUCCESS on success)
  */
 KryonResult kryon_compile_string(const char* source_code, const char* output_path, const KryonCompileOptions* options);
 
 /**
  * @brief Checks if a .kry file needs recompilation.
  * @param kry_path Path to .kry file
  * @param krb_path Path to .krb file
  * @return true if recompilation is needed
  */
 bool kryon_compile_needs_update(const char* kry_path, const char* krb_path);
 
 // =============================================================================
 //  Diagnostic and Error Handling
 // =============================================================================
 
 /**
  * @brief Adds a diagnostic message to the compilation context.
  * @param context Compilation context
  * @param line Line number
  * @param column Column number
  * @param severity Severity level (0=info, 1=warning, 2=error)
  * @param message Diagnostic message
  */
 void kryon_compile_add_diagnostic(KryonCompileContext* context, int line, int column, int severity, const char* message);
 
 /**
  * @brief Gets the first error diagnostic from the context.
  * @param context Compilation context
  * @return First error diagnostic or NULL if no errors
  */
 CompileDiagnostic* kryon_compile_get_first_error(KryonCompileContext* context);
 
 /**
  * @brief Prints all diagnostics to stdout.
  * @param context Compilation context
  */
 void kryon_compile_print_diagnostics(KryonCompileContext* context);
 
 /**
  * @brief Clears all diagnostics from the context.
  * @param context Compilation context
  */
 void kryon_compile_clear_diagnostics(KryonCompileContext* context);
 
 // =============================================================================
 //  Utility Functions
 // =============================================================================
 
 /**
  * @brief Converts compilation result to human-readable string.
  * @param result KryonResult status code
  * @return Human-readable string
  */
 const char* kryon_compile_result_string(KryonResult result);
 
 /**
  * @brief Gets default compilation options.
  * @return Default compilation options
  */
 KryonCompileOptions kryon_compile_get_default_options(void);
 
 /**
  * @brief Validates a .kry file for basic syntax.
  * @param source_path Path to .kry file
  * @return true if file has valid basic syntax
  */
 bool kryon_compile_validate_syntax(const char* source_path);
 
 /**
  * @brief Gets compilation time in milliseconds.
  * @param context Compilation context
  * @return Compilation time in milliseconds
  */
 double kryon_compile_get_time_ms(KryonCompileContext* context);
 
 /**
  * @brief Creates a temporary file path for compilation output.
  * @param source_path Source file path
  * @param temp_dir Temporary directory (NULL for default)
  * @return Allocated temporary file path (must be freed)
  */
 char* kryon_compile_create_temp_path(const char* source_path, const char* temp_dir);
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif // KRYON_RUNTIME_COMPILER_H