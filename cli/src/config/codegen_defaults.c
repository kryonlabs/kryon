/**
 * Default Codegen Options
 * Provides sensible defaults for all code generation targets
 */

#include "kryon_cli.h"

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

TsxCodegenOptions tsx_codegen_default_options(void) {
    TsxCodegenOptions opts = {
        .format = true,               // Format output
        .use_prettier = false,        // Don't use Prettier by default
        .semicolons = true            // Include semicolons (TypeScript style)
    };
    return opts;
}

JsxCodegenOptions jsx_codegen_default_options(void) {
    JsxCodegenOptions opts = {
        .format = true,               // Format output
        .use_prettier = false,        // Don't use Prettier by default
        .semicolons = false           // No semicolons (JavaScript style)
    };
    return opts;
}

LuaCodegenOptions lua_codegen_default_options(void) {
    LuaCodegenOptions opts = {
        .format = true,               // Format output
        .optimize = false             // No optimization by default
    };
    return opts;
}
