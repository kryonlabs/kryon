// JSON Serialization - Main Entry Points
// This file provides the public API and delegates to specialized modules

#define _GNU_SOURCE
#include "../include/ir_serialization.h"
#include "../include/ir_builder.h"
#include "../include/ir_logic.h"
#include "../utils/ir_c_metadata.h"
#include "../style/ir_stylesheet.h"
#include "../serialization/ir_json_serialize.h"
#include "../serialization/ir_json_deserialize.h"
#include "../serialization/ir_json_context.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Android logging
#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "KryonIR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) fprintf(stderr, __VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

// Weak symbol fallback if C bindings aren't linked
__attribute__((weak)) CSourceMetadata g_c_metadata = {0};

// ============================================================================
// C Metadata Cleanup (kept here as it's specific to ir_json.c)
// ============================================================================

/**
 * Free a single string field safely
 */
static void free_string(char** str) {
    if (*str) {
        free(*str);
        *str = NULL;
    }
}

/**
 * Free C variable metadata array
 */
static void free_c_variables(CVariableDecl* variables, size_t count) {
    if (!variables) return;
    for (size_t i = 0; i < count; i++) {
        free_string(&variables[i].name);
        free_string(&variables[i].type);
        free_string(&variables[i].storage);
        free_string(&variables[i].initial_value);
    }
    free(variables);
}

/**
 * Free C event handler metadata array
 */
static void free_c_event_handlers(CEventHandlerDecl* handlers, size_t count) {
    if (!handlers) return;
    for (size_t i = 0; i < count; i++) {
        free_string(&handlers[i].logic_id);
        free_string(&handlers[i].function_name);
        free_string(&handlers[i].return_type);
        free_string(&handlers[i].parameters);
        free_string(&handlers[i].body);
    }
    free(handlers);
}

/**
 * Free C helper function metadata array
 */
static void free_c_helper_functions(CHelperFunction* helpers, size_t count) {
    if (!helpers) return;
    for (size_t i = 0; i < count; i++) {
        free_string(&helpers[i].name);
        free_string(&helpers[i].return_type);
        free_string(&helpers[i].parameters);
        free_string(&helpers[i].body);
    }
    free(helpers);
}

/**
 * Free C include metadata array
 */
static void free_c_includes(CInclude* includes, size_t count) {
    if (!includes) return;
    for (size_t i = 0; i < count; i++) {
        free_string(&includes[i].include_string);
    }
    free(includes);
}

/**
 * Free C preprocessor directive metadata array
 */
static void free_c_preprocessor_directives(CPreprocessorDirective* directives, size_t count) {
    if (!directives) return;
    for (size_t i = 0; i < count; i++) {
        free_string(&directives[i].directive_type);
        free_string(&directives[i].condition);
        free_string(&directives[i].value);
    }
    free(directives);
}

/**
 * Free C source file metadata array
 */
static void free_c_source_files(CSourceFile* source_files, size_t count) {
    if (!source_files) return;
    for (size_t i = 0; i < count; i++) {
        free_string(&source_files[i].filename);
        free_string(&source_files[i].full_path);
        free_string(&source_files[i].content);
    }
    free(source_files);
}

/**
 * Clean up all C metadata, freeing allocated memory
 * Call this before loading new metadata to avoid memory leaks
 */
void ir_c_metadata_cleanup(CSourceMetadata* metadata) {
    if (!metadata) return;

    free_c_variables(metadata->variables, metadata->variable_count);
    metadata->variables = NULL;
    metadata->variable_count = 0;
    metadata->variable_capacity = 0;

    free_c_event_handlers(metadata->event_handlers, metadata->event_handler_count);
    metadata->event_handlers = NULL;
    metadata->event_handler_count = 0;
    metadata->event_handler_capacity = 0;

    free_c_helper_functions(metadata->helper_functions, metadata->helper_function_count);
    metadata->helper_functions = NULL;
    metadata->helper_function_count = 0;
    metadata->helper_function_capacity = 0;

    free_c_includes(metadata->includes, metadata->include_count);
    metadata->includes = NULL;
    metadata->include_count = 0;
    metadata->include_capacity = 0;

    free_c_preprocessor_directives(metadata->preprocessor_directives, metadata->preprocessor_directive_count);
    metadata->preprocessor_directives = NULL;
    metadata->preprocessor_directive_count = 0;
    metadata->preprocessor_directive_capacity = 0;

    free_c_source_files(metadata->source_files, metadata->source_file_count);
    metadata->source_files = NULL;
    metadata->source_file_count = 0;
    metadata->source_file_capacity = 0;

    free_string(&metadata->main_source_file);
}

// ============================================================================
// JSON Serialization API
// ============================================================================

/**
 * Serialize to KIR JSON (complete format with manifest, logic, and metadata)
 * This function now delegates to ir_json_serialize.c
 */
char* ir_serialize_json_complete(
    IRComponent* root,
    IRReactiveManifest* manifest,
    IRLogicBlock* logic_block,
    IRSourceMetadata* source_metadata,
    IRSourceStructures* source_structures
) {
    if (!root) return NULL;

    // Create wrapper object
    cJSON* wrapper = cJSON_CreateObject();
    if (!wrapper) {
        return NULL;
    }

    // Add format identifier
    cJSON_AddStringToObject(wrapper, "format", "kir");

    // Add source metadata
    if (source_metadata) {
        cJSON* metaJson = ir_json_serialize_metadata(source_metadata);
        if (metaJson) {
            cJSON_AddItemToObject(wrapper, "metadata", metaJson);
        }
    }

    // Add window properties from global IR context
    IRContext* ctx = g_ir_context;
    if (ctx && ctx->metadata) {
        cJSON* appJson = cJSON_CreateObject();
        if (ctx->metadata->window_title) {
            cJSON_AddStringToObject(appJson, "windowTitle", ctx->metadata->window_title);
        }
        if (ctx->metadata->window_width > 0) {
            cJSON_AddNumberToObject(appJson, "windowWidth", ctx->metadata->window_width);
        }
        if (ctx->metadata->window_height > 0) {
            cJSON_AddNumberToObject(appJson, "windowHeight", ctx->metadata->window_height);
        }
        // Only add app object if it has any properties
        int app_size = cJSON_GetArraySize(appJson);
        if (app_size > 0) {
            cJSON_AddItemToObject(wrapper, "app", appJson);
        } else {
            cJSON_Delete(appJson);
        }
    }

    // Add component definitions FIRST (at the top)
    if (manifest && manifest->component_def_count > 0) {
        cJSON* componentDefsJson = ir_json_serialize_component_definitions(manifest);
        if (componentDefsJson) {
            cJSON_AddItemToObject(wrapper, "component_definitions", componentDefsJson);
        }
    }

    // Add reactive manifest if present (variables, bindings, conditionals, for-loops)
    if (manifest && (manifest->variable_count > 0 || manifest->binding_count > 0 ||
                     manifest->conditional_count > 0 || manifest->for_loop_count > 0)) {
        cJSON* manifestJson = ir_json_serialize_reactive_manifest(manifest);
        if (manifestJson) {
            cJSON_AddItemToObject(wrapper, "reactive_manifest", manifestJson);
        }
    }

    // Add global stylesheet if present (for CSS descendant selectors round-trip)
    if (ctx && ctx->stylesheet && (ctx->stylesheet->rule_count > 0 ||
                                    ctx->stylesheet->variable_count > 0 ||
                                    ctx->stylesheet->media_query_count > 0)) {
        cJSON* stylesheetJson = ir_json_serialize_stylesheet(ctx->stylesheet);
        if (stylesheetJson) {
            cJSON_AddItemToObject(wrapper, "stylesheet", stylesheetJson);
        }
    }

    // Add source structures if present (for Kry->KIR->Kry round-trip)
    if (source_structures && (source_structures->static_block_count > 0 ||
                              source_structures->var_decl_count > 0 ||
                              source_structures->for_loop_count > 0 ||
                              source_structures->import_count > 0 ||
                              source_structures->export_count > 0)) {
        cJSON* sourceStructsJson = ir_json_serialize_source_structures(source_structures);
        if (sourceStructsJson) {
            cJSON_AddItemToObject(wrapper, "source_structures", sourceStructsJson);
        }
    }

    // Add C metadata (for C->KIR->C round-trip)
    cJSON* c_metadata = ir_json_serialize_c_metadata();
    if (c_metadata) {
        cJSON_AddItemToObject(wrapper, "c_metadata", c_metadata);
    }

    // Add logic block (new!)
    if (logic_block) {
        cJSON* logicJson = ir_json_serialize_logic_block(logic_block);
        if (logicJson) {
            cJSON_AddItemToObject(wrapper, "logic_block", logicJson);
        }
    }

    // Add component tree
    cJSON* componentJson = ir_json_serialize_component_recursive(root);
    if (!componentJson) {
        cJSON_Delete(wrapper);
        return NULL;
    }
    cJSON_AddItemToObject(wrapper, "root", componentJson);

    // Plugin requirements are now handled by the capability system
    // and do not need to be serialized in the KIR file

    // Add source code entries from manifest
    if (manifest && manifest->source_count > 0) {
        cJSON* sourcesArray = cJSON_CreateArray();
        for (uint32_t i = 0; i < manifest->source_count; i++) {
            cJSON* sourceObj = cJSON_CreateObject();
            cJSON_AddStringToObject(sourceObj, "lang", manifest->sources[i].lang);
            cJSON_AddStringToObject(sourceObj, "code", manifest->sources[i].code);
            cJSON_AddItemToArray(sourcesArray, sourceObj);
        }
        cJSON_AddItemToObject(wrapper, "sources", sourcesArray);
    }

    // Convert to string
    char* jsonString = cJSON_Print(wrapper);
    cJSON_Delete(wrapper);

    return jsonString;
}

/**
 * Legacy serialize function - calls complete version with NULL logic/metadata
 */
char* ir_serialize_json(IRComponent* root, IRReactiveManifest* manifest) {
    // Legacy function - just call the complete version with NULL logic/metadata
    return ir_serialize_json_complete(root, manifest, NULL, NULL, NULL);
}

// ============================================================================
// JSON Deserialization API
// ============================================================================

/**
 * Deserialize from KIR JSON
 * This function now delegates to ir_json_deserialize.c
 */
IRComponent* ir_deserialize_json(const char* json_string) {
    if (!json_string) {
        return NULL;
    }

    cJSON* root = cJSON_Parse(json_string);
    if (!root) {
        return NULL;
    }

    // Initialize global module cache
    ir_json_context_cache_init();

    // Parse component_definitions for expansion
    ComponentDefContext* ctx = ir_json_context_create();
    cJSON* componentDefs = cJSON_GetObjectItem(root, "component_definitions");
    if (componentDefs && cJSON_IsArray(componentDefs)) {
        int defCount = cJSON_GetArraySize(componentDefs);
        for (int i = 0; i < defCount; i++) {
            cJSON* def = cJSON_GetArrayItem(componentDefs, i);
            if (!def || !cJSON_IsObject(def)) continue;

            cJSON* nameItem = cJSON_GetObjectItem(def, "name");
            cJSON* templateItem = cJSON_GetObjectItem(def, "template");

            if (nameItem && cJSON_IsString(nameItem) && templateItem && cJSON_IsObject(templateItem)) {
                // Add the full definition (not just template) so we can access props/state
                ir_json_context_add(ctx, nameItem->valuestring, def);
            }
        }
    }

    IRComponent* component = NULL;

    // Check for "root" key first (new format), then "component" (legacy)
    cJSON* componentJson = cJSON_GetObjectItem(root, "root");
    if (!componentJson) {
        componentJson = cJSON_GetObjectItem(root, "component");
    }

    if (componentJson && cJSON_IsObject(componentJson)) {
        // Wrapped format: { "root": {...} } or { "component": {...} }
        component = ir_json_deserialize_component_with_context(componentJson, ctx);
    } else {
        // Unwrapped format: just component tree at root
        component = ir_json_deserialize_component_with_context(root, ctx);
    }

    // Plugin requirements are now handled by the capability system
    // The capability system will automatically load required plugins

    // Parse window properties from "app" object (canonical format)
    cJSON* appJson = cJSON_GetObjectItem(root, "app");

    if (appJson) {
        IRContext* ir_ctx = g_ir_context ?: (ir_set_context(ir_create_context()), g_ir_context);
        if (!ir_ctx->metadata) {
            ir_ctx->metadata = (IRMetadata*)calloc(1, sizeof(IRMetadata));
        }

        // Window title: app.windowTitle
        cJSON* titleItem = cJSON_GetObjectItem(appJson, "windowTitle");
        if (titleItem && cJSON_IsString(titleItem)) {
            free(ir_ctx->metadata->window_title);
            ir_ctx->metadata->window_title = strdup(titleItem->valuestring);
        }

        // Window dimensions: app.windowWidth, app.windowHeight
        cJSON* widthItem = cJSON_GetObjectItem(appJson, "windowWidth");
        if (widthItem && cJSON_IsNumber(widthItem)) {
            ir_ctx->metadata->window_width = (int)widthItem->valuedouble;
        }

        cJSON* heightItem = cJSON_GetObjectItem(appJson, "windowHeight");
        if (heightItem && cJSON_IsNumber(heightItem)) {
            ir_ctx->metadata->window_height = (int)heightItem->valuedouble;
        }
    }

    // Parse C metadata for C->KIR->C round-trip
    cJSON* c_metadata = cJSON_GetObjectItem(root, "c_metadata");
    if (c_metadata) {
        ir_json_deserialize_c_metadata(c_metadata);
    }

    // Parse stylesheet for CSS rules with complex selectors (e.g., ".logo span")
    cJSON* stylesheetJson = cJSON_GetObjectItem(root, "stylesheet");
    if (stylesheetJson && cJSON_IsObject(stylesheetJson)) {
        IRStylesheet* stylesheet = ir_json_deserialize_stylesheet(stylesheetJson);
        if (stylesheet) {
            IRContext* ir_ctx = g_ir_context ?: (ir_set_context(ir_create_context()), g_ir_context);
            if (ir_ctx->stylesheet) {
                ir_stylesheet_free(ir_ctx->stylesheet);
            }
            ir_ctx->stylesheet = stylesheet;
        }
    }

    // Clean up context before deleting JSON (context references JSON nodes)
    ir_json_context_free(ctx);
    cJSON_Delete(root);

    return component;
}
