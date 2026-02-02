/**
 * C Code Generator - Main Function and Struct Generation
 *
 * This is now a thin wrapper that routes to the WIR pipeline for widget generation.
 * All widget generation logic has been moved to:
 * - codegens/wir/wir_builder.c (KIR → WIR transformation)
 * - codegens/c/c_from_wir.c (WIR → C emission)
 *
 * C-specific semantics (reactive state, structs, headers, etc.) remain here.
 */

#include "ir_c_main.h"
#include "ir_c_internal.h"
#include "ir_c_output.h"
#include "ir_c_reactive.h"
#include "ir_c_components.h"
#include "../codegen_common.h"
#include "../wir/wir.h"
#include "../wir/wir_builder.h"
#include "../wir/wir_emitter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Internal aliases for extracted modules
#define write_indent(ctx) c_write_indent(ctx)
#define writeln(ctx, str) c_writeln(ctx, str)
#define generate_reactive_signal_declarations(ctx) c_generate_reactive_signal_declarations(ctx)
#define generate_reactive_signal_initialization(ctx) c_generate_reactive_signal_initialization(ctx)
#define generate_reactive_signal_cleanup(ctx) c_generate_reactive_signal_cleanup(ctx)
#define generate_component_recursive(ctx, comp, root) c_generate_component_recursive(ctx, comp, root)
#define generate_kry_event_handlers(ctx, block) c_generate_kry_event_handlers(ctx, block)
#define generate_component_definitions(ctx, defs) c_generate_component_definitions(ctx, defs)
#define generate_struct_definitions(ctx, types) c_generate_struct_definitions(ctx, types)
#define generate_main_function(ctx) c_generate_main_function(ctx)

void c_generate_struct_definitions(CCodegenContext* ctx, cJSON* struct_types) {
    if (!struct_types || !cJSON_IsArray(struct_types)) return;

    cJSON* struct_def;
    cJSON_ArrayForEach(struct_def, struct_types) {
        cJSON* name = cJSON_GetObjectItem(struct_def, "name");
        cJSON* fields = cJSON_GetObjectItem(struct_def, "fields");

        if (!name || !cJSON_IsString(name)) continue;

        const char* struct_name = cJSON_GetStringValue(name);

        fprintf(ctx->output, "/**\n");
        fprintf(ctx->output, " * Struct: %s\n", struct_name);
        fprintf(ctx->output, " */\n");
        fprintf(ctx->output, "typedef struct {\n");

        if (fields && cJSON_IsArray(fields)) {
            cJSON* field;
            cJSON_ArrayForEach(field, fields) {
                cJSON* field_name = cJSON_GetObjectItem(field, "name");
                cJSON* field_type = cJSON_GetObjectItem(field, "type");

                if (!field_name || !cJSON_IsString(field_name)) continue;

                const char* fname = cJSON_GetStringValue(field_name);
                const char* ftype = field_type && cJSON_IsString(field_type) ?
                                    cJSON_GetStringValue(field_type) : "any";

                // Map KIR types to C types
                const char* c_type = "void*";  // default for 'any'
                if (strcmp(ftype, "string") == 0) {
                    c_type = "char*";
                } else if (strcmp(ftype, "int") == 0 || strcmp(ftype, "number") == 0) {
                    c_type = "int";
                } else if (strcmp(ftype, "float") == 0) {
                    c_type = "float";
                } else if (strcmp(ftype, "bool") == 0) {
                    c_type = "bool";
                }

                fprintf(ctx->output, "    %s %s;\n", c_type, fname);
            }
        }

        fprintf(ctx->output, "} %s;\n\n", struct_name);
    }
}

// ============================================================================
// Main Function Generation
// ============================================================================

void c_generate_main_function(CCodegenContext* ctx) {
    fprintf(ctx->output, "int main(void) {\n");
    ctx->indent_level++;

    // Extract window config from KIR app object
    cJSON* app = cJSON_GetObjectItem(ctx->root_json, "app");
    const char* title = "Kryon App";
    int width = 800;
    int height = 600;

    if (app) {
        cJSON* title_item = cJSON_GetObjectItem(app, "windowTitle");
        if (title_item && cJSON_IsString(title_item)) {
            title = title_item->valuestring;
        }
        cJSON* width_item = cJSON_GetObjectItem(app, "windowWidth");
        if (width_item && cJSON_IsNumber(width_item)) {
            width = width_item->valueint;
        }
        cJSON* height_item = cJSON_GetObjectItem(app, "windowHeight");
        if (height_item && cJSON_IsNumber(height_item)) {
            height = height_item->valueint;
        }
    }

    // kryon_init() call with values from KIR file
    fprintf(ctx->output, "    kryon_init(\"%s\", %d, %d);\n", title, width, height);
    fprintf(ctx->output, "\n");

    // Initialize arrays and function-result variables (if any)
    // Check if there are array or function_result variables in source_structures.const_declarations
    cJSON* source_structures = cJSON_GetObjectItem(ctx->root_json, "source_structures");
    cJSON* const_decls = source_structures ?
                         cJSON_GetObjectItem(source_structures, "const_declarations") : NULL;
    bool has_arrays = false;
    bool has_function_results = false;
    if (const_decls && cJSON_IsArray(const_decls)) {
        cJSON* decl;
        cJSON_ArrayForEach(decl, const_decls) {
            cJSON* value_type = cJSON_GetObjectItem(decl, "value_type");
            if (value_type && value_type->valuestring) {
                if (strcmp(value_type->valuestring, "array") == 0) {
                    has_arrays = true;
                } else if (strcmp(value_type->valuestring, "function_result") == 0) {
                    has_function_results = true;
                }
            }
        }
    }
    if (has_arrays) {
        fprintf(ctx->output, "    // Initialize arrays\n");
        fprintf(ctx->output, "    kryon_init_arrays();\n");
        fprintf(ctx->output, "\n");
    }
    if (has_function_results) {
        fprintf(ctx->output, "    // Initialize function-result variables\n");
        fprintf(ctx->output, "    kryon_init_function_arrays();\n");
        fprintf(ctx->output, "\n");
    }

    // Initialize reactive signals
    if (ctx->has_reactive_state) {
        generate_reactive_signal_initialization(ctx);
    }

    // KRYON_APP macro
    writeln(ctx, "KRYON_APP(");
    ctx->indent_level++;

    if (ctx->component_tree) {
        c_generate_component_recursive(ctx, ctx->component_tree, true);
        fprintf(ctx->output, "\n");
    }

    ctx->indent_level--;
    writeln(ctx, ");");
    fprintf(ctx->output, "\n");

    // KRYON_RUN macro
    writeln(ctx, "KRYON_RUN();");

    // Cleanup reactive signals AFTER KRYON_RUN returns
    if (ctx->has_reactive_state) {
        generate_reactive_signal_cleanup(ctx);
    }

    // Cleanup arrays (if any)
    if (has_arrays) {
        fprintf(ctx->output, "    // Cleanup arrays\n");
        fprintf(ctx->output, "    kryon_cleanup_arrays();\n");
    }

    ctx->indent_level--;
    fprintf(ctx->output, "}\n");
}

// ============================================================================
// Main Code Generation Entry Points
// ============================================================================

bool ir_generate_c_code_from_string(const char* kir_json, const char* output_path) {
    if (!kir_json || !output_path) return false;

    // Parse KIR JSON
    cJSON* root = cJSON_Parse(kir_json);
    if (!root) {
        fprintf(stderr, "Error: Failed to parse KIR JSON\n");
        return false;
    }

    // Open output file
    FILE* output = fopen(output_path, "w");
    if (!output) {
        fprintf(stderr, "Error: Failed to open output file: %s\n", output_path);
        cJSON_Delete(root);
        return false;
    }

    // Setup context
    CCodegenContext ctx = {0};
    ctx.output = output;
    ctx.indent_level = 0;
    ctx.root_json = root;
    ctx.component_tree = cJSON_GetObjectItem(root, "root");
    ctx.c_metadata = cJSON_GetObjectItem(root, "c_metadata");

    if (ctx.c_metadata) {
        ctx.variables = cJSON_GetObjectItem(ctx.c_metadata, "variables");
        ctx.event_handlers = cJSON_GetObjectItem(ctx.c_metadata, "event_handlers");
        ctx.helper_functions = cJSON_GetObjectItem(ctx.c_metadata, "helper_functions");
        ctx.includes = cJSON_GetObjectItem(ctx.c_metadata, "includes");
        ctx.preprocessor_dirs = cJSON_GetObjectItem(ctx.c_metadata, "preprocessor_directives");
    }

    // Parse reactive manifest
    ctx.reactive_manifest = cJSON_GetObjectItem(root, "reactive_manifest");
    if (ctx.reactive_manifest) {
        ctx.reactive_vars = cJSON_GetObjectItem(ctx.reactive_manifest, "variables");
        ctx.has_reactive_state = (ctx.reactive_vars &&
                                  cJSON_IsArray(ctx.reactive_vars) &&
                                  cJSON_GetArraySize(ctx.reactive_vars) > 0);
    }

    // Initialize global variable list for proper variable tracking
    c_init_global_vars(&ctx);

    // Check if this is a component module (has component_definitions but no root)
    cJSON* component_defs = cJSON_GetObjectItem(root, "component_definitions");
    bool is_component_module = (component_defs && cJSON_IsArray(component_defs) &&
                                cJSON_GetArraySize(component_defs) > 0 &&
                                !ctx.component_tree);

    // Check for struct definitions in source_structures
    cJSON* source_structures = cJSON_GetObjectItem(root, "source_structures");
    cJSON* struct_types = source_structures ?
                          cJSON_GetObjectItem(source_structures, "struct_types") : NULL;
    bool has_struct_types = (struct_types && cJSON_IsArray(struct_types) &&
                             cJSON_GetArraySize(struct_types) > 0);

    // Check for exported functions or constants (utility modules)
    cJSON* exports = source_structures ?
                     cJSON_GetObjectItem(source_structures, "exports") : NULL;
    bool has_exports = (exports && cJSON_IsArray(exports) &&
                        cJSON_GetArraySize(exports) > 0);

    cJSON* const_decls = source_structures ?
                         cJSON_GetObjectItem(source_structures, "const_declarations") : NULL;
    bool has_const_decls = (const_decls && cJSON_IsArray(const_decls) &&
                            cJSON_GetArraySize(const_decls) > 0);

    // A utility module has exports/consts but no root or component definitions
    bool is_utility_module = (has_exports || has_const_decls) &&
                             !ctx.component_tree && !is_component_module;

    // Get logic_block early - needed for multiple generators
    cJSON* logic_block = cJSON_GetObjectItem(root, "logic_block");

    // Generate code sections
    generate_includes(&ctx);
    generate_preprocessor_directives(&ctx);

    // Generate struct definitions if present
    if (has_struct_types) {
        generate_struct_definitions(&ctx, struct_types);
    }

    // Generate forward declarations for user-defined functions FIRST
    // This is needed because kryon_init_function_arrays() may call these functions
    generate_universal_function_declarations(&ctx, logic_block);

    // Generate reactive signal declarations early (so user functions can use kryon_signal_set)
    // Must come before function implementations since functions may set signal values
    if (ctx.has_reactive_state) {
        generate_reactive_signal_declarations(&ctx);
    }

    // Generate global variable declarations (so functions can reference them)
    // This includes array variables like 'habits' and their _count companions
    generate_array_declarations(&ctx);

    // Generate user-defined function implementations
    // Must come AFTER global declarations and signal declarations
    generate_universal_functions(&ctx, logic_block);

    generate_variable_declarations(&ctx);
    generate_helper_functions(&ctx);
    generate_event_handlers(&ctx);

    // Generate KRY event handlers from logic_block (transpiled from KRY expressions)
    // Must come after signal declarations so handlers can reference the signals
    generate_kry_event_handlers(&ctx, logic_block);

    if (is_component_module) {
        // Generate component definitions for module files
        generate_component_definitions(&ctx, component_defs);
    } else if (is_utility_module) {
        // Utility modules: generate exported functions and constants
        if (!generate_exported_functions(output, logic_block, exports, output_path)) {
            // Hard error occurred - cleanup and return false
            fclose(output);
            cJSON_Delete(root);
            return false;
        }
    } else if (ctx.component_tree) {
        // App files with root: generate main()
        generate_main_function(&ctx);
    }
    // Struct-only modules: struct is already generated, no main() needed

    // Cleanup
    fclose(output);
    cJSON_Delete(root);

    return true;
}

bool ir_generate_c_code(const char* kir_path, const char* output_path) {
    if (!kir_path || !output_path) return false;

    // Set error prefix for this codegen
    codegen_set_error_prefix("C");

    // Read KIR file using shared utility
    char* content = codegen_read_kir_file(kir_path, NULL);
    if (!content) {
        return false;
    }

    // Generate code
    bool result = ir_generate_c_code_from_string(content, output_path);
    free(content);

    return result;
}
