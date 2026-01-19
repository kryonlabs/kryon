#ifndef KRYON_BUILD_H
#define KRYON_BUILD_H

#include <stdbool.h>
#include "kryon_cli.h"

// Forward declarations
struct IRComponent;
struct DocsTemplateContext;

/* ============================================================================
 * Frontend Detection
 * ============================================================================ */

/**
 * Detect the frontend type based on file extension.
 *
 * Supported extensions:
 * - .tsx, .jsx  -> "tsx"
 * - .kir        -> "kir"
 * - .md         -> "markdown"
 * - .html       -> "html"
 * - .kry        -> "kry"
 * - .lua        -> "lua"
 * - .c, .h      -> "c"
 *
 * @param source_file Path to the source file
 * @return Frontend type string, or NULL if unknown
 */
const char* detect_frontend_type(const char* source_file);

/* ============================================================================
 * KIR Compilation
 * ============================================================================ */

/**
 * Compile a source file to KIR format.
 *
 * Handles all frontend types by calling the appropriate parser.
 * Creates .kryon_cache directory if needed.
 *
 * @param source_file Path to the source file
 * @param output_kir  Path where KIR output should be written
 * @return 0 on success, non-zero on failure
 */
int compile_source_to_kir(const char* source_file, const char* output_kir);

/* ============================================================================
 * Web Codegen (HTML Generation)
 * ============================================================================ */

/**
 * Generate HTML files from a KIR file using kir_to_html.
 *
 * @param kir_file      Path to the input KIR file
 * @param output_dir    Directory where HTML files should be written
 * @param project_name  Optional project name for the generated HTML
 * @param source_dir    Source directory (where assets are located)
 * @return 0 on success, non-zero on failure
 */
int generate_html_from_kir(const char* kir_file, const char* output_dir,
                           const char* project_name, const char* source_dir);

/* ============================================================================
 * Lua Runtime Detection
 * ============================================================================ */

/**
 * Check if a KIR file requires the Lua runtime for execution.
 *
 * Loads the KIR file and checks the source metadata.
 *
 * @param kir_file Path to the KIR file
 * @return true if Lua runtime is needed, false otherwise
 */
bool kir_needs_lua_runtime(const char* kir_file);

/* ============================================================================
 * Docs Template
 * ============================================================================ */

/**
 * Build a markdown file with optional docs template applied.
 *
 * If a template is provided, it wraps the content KIR in a layout.
 * Otherwise, directly generates HTML from the content KIR.
 *
 * @param content_kir_file    Path to the content KIR file
 * @param source_path         Original source file path
 * @param route               Route path for the page
 * @param output_dir          Directory for generated output
 * @param docs_template_path  Path to docs template (NULL to skip)
 * @param config              Kryon configuration
 * @return 0 on success, non-zero on failure
 */
int build_with_docs_template(const char* content_kir_file,
                             const char* source_path,
                             const char* route,
                             const char* output_dir,
                             const char* docs_template_path,
                             KryonConfig* config);

/* ============================================================================
 * Desktop Execution
 * ============================================================================ */

/**
 * Execute a KIR file on the desktop backend.
 *
 * Handles both standard C execution and Lua runtime delegation.
 *
 * @param kir_file      Path to the KIR file
 * @param desktop_lib   Path to desktop renderer library (NULL to detect)
 * @param renderer      Override renderer: "sdl3", "raylib", or NULL (use config)
 * @return 0 on success, non-zero on failure
 */
#ifndef KRYON_MINIMAL_BUILD
int run_kir_on_desktop(const char* kir_file, const char* desktop_lib, const char* renderer);

/**
 * Execute a KIR file on desktop with hot reload support.
 *
 * Uses per-instance API for state preservation during reloads.
 * Watches for file changes and automatically reloads the KIR file.
 *
 * @param kir_file  Path to the KIR file
 * @param desktop_lib   Path to desktop renderer library (NULL to detect)
 * @param renderer  Override renderer: "sdl3", "raylib", or NULL (use config)
 * @param watch_path  Directory to watch for file changes (NULL for KIR file directory)
 * @return 0 on success, non-zero on failure
 */
int run_kir_on_desktop_with_hot_reload(const char* kir_file, const char* desktop_lib,
                                        const char* renderer, const char* watch_path);
#endif

/**
 * Execute a KIR file using the Lua runtime.
 *
 * Delegates to LuaJIT with Runtime.loadKIR/Runtime.runDesktop.
 *
 * @param kir_file  Path to the KIR file
 * @return 0 on success, non-zero on failure
 */
int run_kir_with_lua_runtime(const char* kir_file);

/* ============================================================================
 * Build Pipeline
 * ============================================================================ */

/**
 * Options for building a source file.
 */
typedef struct {
    const char* target;        /* "web", "desktop", etc. */
    const char* output_dir;
    const char* project_name;
    const char* docs_template; /* Optional template path for markdown */
} BuildOptions;

/**
 * Build a source file through the full pipeline.
 *
 * Compiles source to KIR, then applies codegen based on target.
 *
 * @param source_file Path to the source file
 * @param opts        Build options
 * @param config      Kryon configuration
 * @return 0 on success, non-zero on failure
 */
int build_source_file(const char* source_file, BuildOptions* opts, KryonConfig* config);

/* ============================================================================
 * Codegen Pipeline
 * ============================================================================ */

/**
 * Get default output directory for a specific codegen target.
 * Returns a newly allocated string that caller must free.
 *
 * Uses the following priority:
 * 1. config->codegen_output_dir
 * 2. config->build_output_dir
 * 3. "build" (default)
 *
 * The result is always "<base>/<target>" for organization.
 *
 * @param target    Codegen target (kry, lua, c, tsx)
 * @param config    Kryon config (optional, can be NULL)
 * @return Default output directory path (caller must free)
 */
char* get_codegen_output_dir(const char* target, KryonConfig* config);

/**
 * Generate code from a KIR file.
 *
 * Calls the appropriate codegen function based on target.
 *
 * @param kir_file      Path to the KIR file
 * @param target        Codegen target (kry, lua, c, tsx)
 * @param output_path   Output file/directory path
 * @return 0 on success, non-zero on failure
 */
int generate_from_kir(const char* kir_file, const char* target,
                      const char* output_path);

/**
 * Perform full codegen pipeline: source → KIR → target.
 *
 * If source_file is not provided, uses build.entry from config.
 * If output_dir is not provided, uses get_codegen_output_dir().
 *
 * @param source_file   Source file path (uses build.entry if NULL)
 * @param target        Codegen target (kry, lua, c, tsx, web)
 * @param output_dir    Output directory (uses default if NULL)
 * @param config        Kryon config (loaded if NULL)
 * @return 0 on success, non-zero on failure
 */
int codegen_pipeline(const char* source_file, const char* target,
                     const char* output_dir, KryonConfig* config);

#endif /* KRYON_BUILD_H */
