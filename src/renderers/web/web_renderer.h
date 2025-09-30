/**
 * @file web_renderer.h
 * @brief Web Renderer Interface - HTML/CSS/JS output for Kryon
 *
 * Implements the Kryon renderer interface to generate static web pages
 * from KRB files. Outputs HTML structure, CSS styles, and JavaScript
 * for interactivity with optional Lua support via fengari.
 */

#ifndef KRYON_WEB_RENDERER_H
#define KRYON_WEB_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "renderer_interface.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

// Forward declarations
typedef struct KryonRuntime KryonRuntime;

// =============================================================================
// WEB RENDERER CONFIGURATION
// =============================================================================

typedef struct {
    char* output_dir;           ///< Output directory for HTML/CSS/JS files
    char* title;                ///< Page title
    bool inline_styles;         ///< Inline CSS vs external stylesheet
    bool inline_scripts;        ///< Inline JS vs external script
    bool include_fengari;       ///< Include fengari Lua VM
    bool minify;                ///< Minify output
    bool standalone;            ///< Single-file HTML output
} WebRendererConfig;

// =============================================================================
// WEB RENDERER STRUCTURE
// =============================================================================

typedef struct {
    WebRendererConfig config;
    FILE* html_file;
    FILE* css_file;
    FILE* js_file;
    char* html_buffer;          ///< HTML content buffer
    char* css_buffer;           ///< CSS content buffer
    char* js_buffer;            ///< JavaScript content buffer
    size_t html_size;
    size_t html_capacity;
    size_t css_size;
    size_t css_capacity;
    size_t js_size;
    size_t js_capacity;
    int element_id_counter;     ///< Counter for unique element IDs
    bool initialized;
} WebRendererImpl;

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * Create web renderer with configuration
 * @param config Renderer configuration with event callback
 * @return Renderer instance or NULL on failure
 */
KryonRenderer* kryon_web_renderer_create(const KryonRendererConfig* config);

/**
 * Create web renderer with web-specific config
 * @param web_config Web-specific configuration
 * @return Renderer instance or NULL on failure
 */
KryonRenderer* kryon_web_renderer_create_with_config(const WebRendererConfig* web_config);

/**
 * Get default web renderer configuration
 * @return Default configuration
 */
WebRendererConfig kryon_web_renderer_default_config(void);

/**
 * Finalize and write all output files
 * @param renderer The web renderer instance
 * @return true on success, false on failure
 */
bool kryon_web_renderer_finalize(KryonRenderer* renderer);

/**
 * Finalize and write all output files with runtime context
 * @param renderer The web renderer instance
 * @param runtime Runtime for accessing element tree
 * @return true on success, false on failure
 */
bool kryon_web_renderer_finalize_with_runtime(KryonRenderer* renderer, KryonRuntime* runtime);

#ifdef __cplusplus
}
#endif

#endif // KRYON_WEB_RENDERER_H