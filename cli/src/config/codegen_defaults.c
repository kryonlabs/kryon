/**
 * Default Codegen Options
 * Provides sensible defaults for all code generation targets
 */

#include "kryon_cli.h"
#include "../../../dis/include/dis_codegen.h"

WebCodegenOptions web_codegen_default_options(void) {
    WebCodegenOptions opts = {
        .minify = true,               // Minify output by default
        .inline_css = false,          // Use external CSS file
        .include_js_runtime = true,   // Include JavaScript runtime
        .separate_files = true,       // Generate separate files
        .include_wasm = false         // WASM disabled by default
    };
    return opts;
}

DISCodegenOptions dis_codegen_default_options(void) {
    DISCodegenOptions opts = {
        .optimize = false,            // No optimizations by default
        .debug_info = true,           // Include debug info by default
        .stack_extent = 4096,         // Default stack size
        .share_mp = false,            // Don't share MP by default
        .module_name = NULL           // Use default module name
    };
    return opts;
}
