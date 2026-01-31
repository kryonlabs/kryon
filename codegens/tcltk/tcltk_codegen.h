#ifndef TCLTK_CODEGEN_H
#define TCLTK_CODEGEN_H

#include <stdbool.h>

/**
 * @brief Generate Tcl/Tk script from KIR file
 * @param kir_path Path to input .kir file (JSON)
 * @param output_path Path to output .tcl file (Tcl/Tk script)
 * @return true on success, false on error
 */
bool tcltk_codegen_generate(const char* kir_path, const char* output_path);

/**
 * @brief Generate Tcl/Tk script from KIR JSON string
 * @param kir_json KIR JSON string
 * @return Allocated Tcl/Tk script string (caller must free), or NULL on error
 */
char* tcltk_codegen_from_json(const char* kir_json);

/**
 * @brief Generate Tcl/Tk module files from KIR
 * @param kir_path Path to input .kir file
 * @param output_dir Directory for output .tcl files
 * @return true on success, false on error
 */
bool tcltk_codegen_generate_multi(const char* kir_path, const char* output_dir);

/**
 * @brief Tcl/Tk codegen options
 */
typedef struct {
    bool include_comments;      // Add comments to generated code
    bool use_ttk_widgets;       // Use themed ttk widgets (modern look)
    bool generate_main;         // Generate main event loop code
    const char* window_title;   // Window title (default: from KIR or "Kryon App")
    int window_width;           // Window width (default: 800)
    int window_height;          // Window height (default: 600)
} TclTkCodegenOptions;

/**
 * @brief Generate Tcl/Tk script with options
 * @param kir_path Path to input .kir file
 * @param output_path Path to output .tcl file
 * @param options Codegen options (NULL for defaults)
 * @return true on success, false on error
 */
bool tcltk_codegen_generate_with_options(const char* kir_path,
                                         const char* output_path,
                                         TclTkCodegenOptions* options);

#endif // TCLTK_CODEGEN_H
