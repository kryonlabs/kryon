/**
 * @file runtime_compiler.c
 * @brief Runtime compilation implementation for on-the-fly .kry to .krb compilation.
 * (CORRECTED to use the modern compilation pipeline and error system)
 */

 #include "runtime_compiler.h"
 #include "memory.h"
 #include "error.h"
 #include "lexer.h"
 #include "parser.h"
 #include "codegen.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <sys/stat.h>
 #include <time.h>
 #include <unistd.h>
 #include <libgen.h>
 #include <stdbool.h>
 
 // Forward declare the global memory manager from memory.c
 extern KryonMemoryManager* g_kryon_memory_manager;
 
 // Default configuration
 #define KRYON_COMPILE_DEFAULT_TEMP_DIR "/tmp/kryon_compile"
 #define KRYON_COMPILE_MAX_DIAGNOSTICS 100
 
 // Global compiler state
 static struct {
     bool initialized;
     KryonCompileOptions default_options;
 } g_compiler_state = {0};
 
 // =============================================================================
 //  Private Helper Functions (Forward Declarations)
 // =============================================================================
 
 static bool ensure_directory(const char* path);
 static bool write_file_content(const char* path, const char* content);
 static CompileDiagnostic* create_diagnostic(int line, int column, int severity, const char* message);
 static void destroy_diagnostic(CompileDiagnostic* diagnostic);
 static time_t get_file_mtime(const char* path);
 static char* get_file_basename(const char* path);
 static char* get_file_directory(const char* path);
 
 // --- Core Compiler Logic and Helpers (from compile_command.c) ---
 static KryonResult invoke_kryon_compiler(const char* source_path, const char* output_path, const KryonCompileOptions* options);
 static KryonResult process_include_directives(KryonASTNode *ast, KryonParser *parser, const char *base_dir);
 static KryonASTNode *ensure_app_root_container(const KryonASTNode *original_ast, KryonParser *parser, bool debug);
 
 // =============================================================================
 //  Runtime Compiler API Implementation
 // =============================================================================
 
 bool kryon_runtime_compiler_init(void) {
     if (g_compiler_state.initialized) return true;
 
     // --- CRITICAL FIX: INITIALIZE GLOBAL SYSTEMS ---
     if (kryon_error_init() != KRYON_SUCCESS) {
         fprintf(stderr, "❌ FATAL: Failed to initialize error system for runtime compiler.\n");
         return false;
     }
 
     if (!g_kryon_memory_manager) {
         KryonMemoryConfig mem_config = {
             .initial_heap_size = 16 * 1024 * 1024, .max_heap_size = 256 * 1024 * 1024,
             .enable_leak_detection = true, .use_system_malloc = true
         };
         g_kryon_memory_manager = kryon_memory_init(&mem_config);
         if (!g_kryon_memory_manager) {
             fprintf(stderr, "❌ FATAL: Failed to initialize memory manager for runtime compiler.\n");
             kryon_error_shutdown();
             return false;
         }
     }
     // --- END CRITICAL FIX ---
 
     g_compiler_state.default_options = (KryonCompileOptions){
         .verbose = false, .debug = false, .optimize = false,
         .cache_enabled = true, .output_dir = NULL, .temp_dir = KRYON_COMPILE_DEFAULT_TEMP_DIR
     };
 
     if (!ensure_directory(KRYON_COMPILE_DEFAULT_TEMP_DIR)) {
         KRYON_LOG_ERROR("Failed to create compiler temp directory: %s", KRYON_COMPILE_DEFAULT_TEMP_DIR);
         return false;
     }
 
     g_compiler_state.initialized = true;
     KRYON_LOG_INFO("⚡ Runtime compiler initialized");
     return true;
 }
 
 void kryon_runtime_compiler_shutdown(void) {
     if (!g_compiler_state.initialized) return;
     // We don't shut down global systems here, assuming the main app will.
     g_compiler_state.initialized = false;
     KRYON_LOG_INFO("⚡ Runtime compiler shutdown");
 }
 
 KryonCompileContext* kryon_compile_context_create(void) {
     KryonCompileContext* context = kryon_alloc_type(KryonCompileContext);
     if (!context) return NULL;
     
     *context = (KryonCompileContext){
         .options = g_compiler_state.default_options,
         .diagnostics = NULL, .diagnostic_count = 0, .has_errors = false,
         .source_path = NULL, .output_path = NULL,
         .start_time = 0, .end_time = 0
     };
     return context;
 }
 
 void kryon_compile_context_destroy(KryonCompileContext* context) {
     if (!context) return;
     kryon_compile_clear_diagnostics(context);
     kryon_free(context->source_path);
     kryon_free(context->output_path);
     kryon_free(context);
 }
 
 KryonResult kryon_compile_file(const char* source_path, const char* output_path, const KryonCompileOptions* options) {
     if (!g_compiler_state.initialized) {
         KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_INVALID_STATE, KRYON_SEVERITY_ERROR, "Runtime compiler not initialized");
     }
     if (!source_path || !output_path) return KRYON_ERROR_INVALID_ARGUMENT;
 
     const KryonCompileOptions* compile_options = options ? options : &g_compiler_state.default_options;
     
     if (access(source_path, R_OK) != 0) {
         KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_FILE_NOT_FOUND, KRYON_SEVERITY_ERROR, "Source file not found");
     }
     
     char* output_dir = get_file_directory(output_path);
     if (output_dir) {
         ensure_directory(output_dir);
         kryon_free(output_dir);
     }
     
     return invoke_kryon_compiler(source_path, output_path, compile_options);
 }
 
 KryonResult kryon_compile_file_detailed(KryonCompileContext* context, const char* source_path, const char* output_path) {
     if (!context) return KRYON_ERROR_INVALID_ARGUMENT;
     
     context->source_path = kryon_strdup(source_path);
     context->output_path = kryon_strdup(output_path);
     kryon_compile_clear_diagnostics(context);
     context->start_time = time(NULL);
     
     KryonResult result = kryon_compile_file(source_path, output_path, &context->options);
     
     context->end_time = time(NULL);
     
     if (result != KRYON_SUCCESS) {
         context->has_errors = true;
         if (context->diagnostic_count == 0) {
             const KryonError *err = kryon_error_get_last();
             const char *msg = err ? err->message : kryon_error_get_message(result);
             kryon_compile_add_diagnostic(context, err ? err->line : 0, 0, 2, msg);
         }
     }
     
     return result;
 }
 
 KryonResult kryon_compile_string(const char* source_code, const char* output_path, const KryonCompileOptions* options) {
     if (!source_code || !output_path) return KRYON_ERROR_INVALID_ARGUMENT;
     
     char temp_source[256];
     snprintf(temp_source, sizeof(temp_source), "%s/temp_source_%ld.kry", KRYON_COMPILE_DEFAULT_TEMP_DIR, (long)time(NULL));
     
     if (!write_file_content(temp_source, source_code)) {
         KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_FILE_WRITE_ERROR, KRYON_SEVERITY_ERROR, "Failed to create temp source file");
     }
     
     KryonResult result = kryon_compile_file(temp_source, output_path, options);
     unlink(temp_source);
     return result;
 }
 
 bool kryon_compile_needs_update(const char* kry_path, const char* krb_path) {
     if (!kry_path || !krb_path) return true;
     if (access(krb_path, R_OK) != 0) return true;
     return get_file_mtime(kry_path) > get_file_mtime(krb_path);
 }
 
 // =============================================================================
 //  Diagnostic and Error Handling Implementation
 // =============================================================================
 
 void kryon_compile_add_diagnostic(KryonCompileContext* context, int line, int column, int severity, const char* message) {
     if (!context || !message || context->diagnostic_count >= KRYON_COMPILE_MAX_DIAGNOSTICS) return;
     CompileDiagnostic* diagnostic = create_diagnostic(line, column, severity, message);
     if (!diagnostic) return;
     diagnostic->next = context->diagnostics;
     context->diagnostics = diagnostic;
     context->diagnostic_count++;
     if (severity >= 2) context->has_errors = true;
 }
 
 void kryon_compile_clear_diagnostics(KryonCompileContext* context) {
     if (!context) return;
     CompileDiagnostic* d = context->diagnostics;
     while (d) {
         CompileDiagnostic* next = d->next;
         destroy_diagnostic(d);
         d = next;
     }
     context->diagnostics = NULL;
     context->diagnostic_count = 0;
     context->has_errors = false;
 }
 
 // =============================================================================
 //  Core Compiler Logic (Adapted from compile_command.c)
 // =============================================================================
 
 // --- All static helper functions from compile_command.c are now here ---
 static KryonResult deep_copy_ast_node(const KryonASTNode *src, KryonASTNode **out_copy);
 static KryonResult merge_include_ast(KryonASTNode *parent, size_t include_index, const KryonASTNode *include_ast, KryonParser *main_parser);
 static KryonResult process_single_include(KryonASTNode *include_node, KryonASTNode *parent, size_t index, KryonParser *parser, const char *base_dir);
 static KryonResult process_includes_recursive(KryonASTNode *node, KryonParser *parser, const char *base_dir);
 static KryonASTNode *find_metadata_node(const KryonASTNode *ast);
 static KryonResult create_app_element_with_metadata(const KryonASTNode *original_ast, KryonParser *parser, KryonASTNode **out_node);
 
 // The main compiler pipeline function
 static KryonResult invoke_kryon_compiler(const char* source_path, const char* output_path, const KryonCompileOptions* options) {
     FILE *file = NULL;
     char *source = NULL;
     KryonLexer *lexer = NULL;
     KryonParser *parser = NULL;
     KryonCodeGenerator *codegen = NULL;
     char *base_dir = NULL;
     KryonResult result = KRYON_SUCCESS;
 
     file = fopen(source_path, "r");
     if (!file) KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_FILE_NOT_FOUND, KRYON_SEVERITY_ERROR, "Cannot open source file");
 
     fseek(file, 0, SEEK_END);
     long file_size = ftell(file);
     fseek(file, 0, SEEK_SET);
 
     source = kryon_alloc(file_size + 1);
     if (!source) { result = KRYON_ERROR_OUT_OF_MEMORY; goto cleanup; }
 
     fread(source, 1, file_size, file);
     source[file_size] = '\0';
     fclose(file);
     file = NULL;
 
     lexer = kryon_lexer_create(source, file_size, source_path, NULL);
     if (!lexer || !kryon_lexer_tokenize(lexer)) {
         KRYON_ERROR_SET(KRYON_ERROR_SYNTAX_ERROR, KRYON_SEVERITY_ERROR, lexer ? kryon_lexer_get_error(lexer) : "Lexer creation failed");
         result = KRYON_ERROR_SYNTAX_ERROR;
         goto cleanup;
     }
 
     parser = kryon_parser_create(lexer, NULL);
     if (!parser) { result = KRYON_ERROR_COMPILATION_FAILED; goto cleanup; }
 
     if (!kryon_parser_parse(parser)) {
         size_t error_count;
         const char** errors = kryon_parser_get_errors(parser, &error_count);
         KRYON_ERROR_SET(KRYON_ERROR_PARSE_ERROR, KRYON_SEVERITY_ERROR, error_count > 0 ? errors[0] : "Parsing failed");
         result = KRYON_ERROR_PARSE_ERROR;
         goto cleanup;
     }
 
     const KryonASTNode *ast = kryon_parser_get_root(parser);
     if (!ast) { KRYON_ERROR_SET(KRYON_ERROR_INVALID_STATE, KRYON_SEVERITY_ERROR, "Parser returned no AST"); result = KRYON_ERROR_INVALID_STATE; goto cleanup; }
 
     base_dir = strdup(source_path);
     if (base_dir) {
         char *last_slash = strrchr(base_dir, '/');
         if (last_slash) *last_slash = '\0';
         else strcpy(base_dir, ".");
         
         if (process_include_directives((KryonASTNode*)ast, parser, base_dir) != KRYON_SUCCESS) {
             result = kryon_error_get_last() ? kryon_error_get_last()->code : KRYON_ERROR_FILE_READ_ERROR;
             goto cleanup;
         }
     }
 
     KryonASTNode *transformed_ast = ensure_app_root_container(ast, parser, options->debug);
     if (!transformed_ast) { result = KRYON_ERROR_COMPILATION_FAILED; goto cleanup; }
 
     KryonCodeGenConfig config = options->optimize ? kryon_codegen_speed_config() : kryon_codegen_default_config();
     codegen = kryon_codegen_create(&config);
     if (!codegen) { result = KRYON_ERROR_COMPILATION_FAILED; goto cleanup; }
 
     if (!kryon_codegen_generate(codegen, transformed_ast)) {
         KRYON_ERROR_SET(KRYON_ERROR_CODEGEN_FAILED, KRYON_SEVERITY_ERROR, "Code generation failed");
         result = KRYON_ERROR_CODEGEN_FAILED;
         goto cleanup;
     }
 
     if (!kryon_write_file(codegen, output_path)) {
         KRYON_ERROR_SET(KRYON_ERROR_FILE_WRITE_ERROR, KRYON_SEVERITY_ERROR, "Failed to write output file");
         result = KRYON_ERROR_FILE_WRITE_ERROR;
         goto cleanup;
     }
 
     result = KRYON_SUCCESS;
 
 cleanup:
     if (file) fclose(file);
     if (base_dir) free(base_dir);
     if (codegen) kryon_codegen_destroy(codegen);
     if (parser) kryon_parser_destroy(parser);
     if (lexer) kryon_lexer_destroy(lexer);
     if (source) kryon_free(source);
     return result;
 }
 
 // --- Implementations of all static helper functions ---
 // (These are copied directly from the working compile_command.c)
 
 static KryonResult deep_copy_ast_node(const KryonASTNode *src, KryonASTNode **out_copy) {
     if (!src) { *out_copy = NULL; return KRYON_SUCCESS; }
     KryonASTNode *copy = kryon_alloc(sizeof(KryonASTNode));
     if (!copy) KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to allocate for AST node copy");
     memcpy(copy, src, sizeof(KryonASTNode));
     copy->parent = NULL;
     switch (src->type) {
         case KRYON_AST_ROOT:
         case KRYON_AST_ELEMENT:
             if (src->data.element.element_type) copy->data.element.element_type = strdup(src->data.element.element_type);
             if (src->data.element.property_count > 0) {
                 copy->data.element.properties = kryon_alloc(src->data.element.property_capacity * sizeof(KryonASTNode*));
                 for (size_t i = 0; i < src->data.element.property_count; i++) {
                     if (deep_copy_ast_node(src->data.element.properties[i], &copy->data.element.properties[i]) != KRYON_SUCCESS) return KRYON_ERROR_OUT_OF_MEMORY;
                     if (copy->data.element.properties[i]) copy->data.element.properties[i]->parent = copy;
                 }
             }
             if (src->data.element.child_count > 0) {
                 copy->data.element.children = kryon_alloc(src->data.element.child_capacity * sizeof(KryonASTNode*));
                 for (size_t i = 0; i < src->data.element.child_count; i++) {
                      if (deep_copy_ast_node(src->data.element.children[i], &copy->data.element.children[i]) != KRYON_SUCCESS) return KRYON_ERROR_OUT_OF_MEMORY;
                     if (copy->data.element.children[i]) copy->data.element.children[i]->parent = copy;
                 }
             }
             break;
         case KRYON_AST_PROPERTY:
             if (src->data.property.name) copy->data.property.name = strdup(src->data.property.name);
             if (src->data.property.value) {
                 if (deep_copy_ast_node(src->data.property.value, &copy->data.property.value) != KRYON_SUCCESS) return KRYON_ERROR_OUT_OF_MEMORY;
                 if (copy->data.property.value) copy->data.property.value->parent = copy;
             }
             break;
         case KRYON_AST_LITERAL:
             if (src->data.literal.value.type == KRYON_VALUE_STRING && src->data.literal.value.data.string_value)
                 copy->data.literal.value.data.string_value = strdup(src->data.literal.value.data.string_value);
             break;
         default: break;
     }
     *out_copy = copy;
     return KRYON_SUCCESS;
 }
 
 static KryonResult merge_include_ast(KryonASTNode *parent, size_t include_index, const KryonASTNode *include_ast, KryonParser *main_parser) {
     if (!parent || !include_ast || !main_parser) KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_INVALID_ARGUMENT, KRYON_SEVERITY_ERROR, "NULL argument for merging AST");
     if (include_ast->type != KRYON_AST_ROOT) KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_PARSE_ERROR, KRYON_SEVERITY_ERROR, "Included file has no root node");
     size_t new_children_count = include_ast->data.element.child_count;
     if (new_children_count == 0) return KRYON_SUCCESS;
     size_t current_count = parent->data.element.child_count;
     size_t new_total = current_count + new_children_count - 1;
     if (new_total > parent->data.element.child_capacity) {
         size_t new_capacity = new_total * 2;
         KryonASTNode **new_children = kryon_realloc(parent->data.element.children, new_capacity * sizeof(KryonASTNode*));
         if (!new_children) KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to expand children array for include");
         parent->data.element.children = new_children;
         parent->data.element.child_capacity = new_capacity;
     }
     memmove(&parent->data.element.children[include_index + new_children_count], &parent->data.element.children[include_index + 1], (current_count - include_index - 1) * sizeof(KryonASTNode*));
     for (size_t i = 0; i < new_children_count; i++) {
         KryonASTNode *copied_child = NULL;
         if (deep_copy_ast_node(include_ast->data.element.children[i], &copied_child) != KRYON_SUCCESS) return KRYON_ERROR_OUT_OF_MEMORY;
         parent->data.element.children[include_index + i] = copied_child;
         if(copied_child) copied_child->parent = parent;
     }
     parent->data.element.child_count = new_total;
     return KRYON_SUCCESS;
 }
 
 static KryonResult process_single_include(KryonASTNode *include_node, KryonASTNode *parent, size_t index, KryonParser *parser, const char *base_dir) {
     const char *include_path = include_node->data.element.element_type;
     if (!include_path) KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_PARSE_ERROR, KRYON_SEVERITY_ERROR, "Include directive missing path");
     char full_path[1024];
     snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, include_path);
     if (access(full_path, R_OK) != 0) KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_FILE_NOT_FOUND, KRYON_SEVERITY_ERROR, "Cannot read included file");
     FILE *file = NULL; char *source = NULL; KryonLexer *include_lexer = NULL; KryonParser *include_parser = NULL; KryonResult result = KRYON_SUCCESS;
     file = fopen(full_path, "r");
     if (!file) KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_FILE_READ_ERROR, KRYON_SEVERITY_ERROR, "Cannot open included file");
     fseek(file, 0, SEEK_END); long file_size = ftell(file); fseek(file, 0, SEEK_SET);
     source = kryon_alloc(file_size + 1);
     if (!source) { result = KRYON_ERROR_OUT_OF_MEMORY; goto include_cleanup; }
     fread(source, 1, file_size, file); source[file_size] = '\0';
     include_lexer = kryon_lexer_create(source, file_size, full_path, NULL);
     if (!include_lexer) { result = KRYON_ERROR_COMPILATION_FAILED; goto include_cleanup; }
     if (!kryon_lexer_tokenize(include_lexer)) { KRYON_ERROR_SET(KRYON_ERROR_SYNTAX_ERROR, KRYON_SEVERITY_ERROR, kryon_lexer_get_error(include_lexer)); result = KRYON_ERROR_SYNTAX_ERROR; goto include_cleanup; }
     include_parser = kryon_parser_create(include_lexer, NULL);
     if (!include_parser) { result = KRYON_ERROR_COMPILATION_FAILED; goto include_cleanup; }
     if (!kryon_parser_parse(include_parser)) KRYON_LOG_WARNING("Included file '%s' has parse errors but continuing.", full_path);
     const KryonASTNode *include_ast = kryon_parser_get_root(include_parser);
     if (!include_ast) { KRYON_ERROR_SET(KRYON_ERROR_PARSE_ERROR, KRYON_SEVERITY_ERROR, "Failed to get AST from included file"); result = KRYON_ERROR_PARSE_ERROR; goto include_cleanup; }
     result = merge_include_ast(parent, index, include_ast, parser);
 include_cleanup:
     if (file) fclose(file); if (include_parser) kryon_parser_destroy(include_parser); if (include_lexer) kryon_lexer_destroy(include_lexer); if (source) kryon_free(source);
     return result;
 }
 
 static KryonResult process_includes_recursive(KryonASTNode *node, KryonParser *parser, const char *base_dir) {
     if (!node) return KRYON_SUCCESS;
     if (node->type == KRYON_AST_ROOT || node->type == KRYON_AST_ELEMENT) {
         size_t i = 0;
         while (i < node->data.element.child_count) {
             KryonASTNode *child = node->data.element.children[i];
             if (!child) { i++; continue; }
             if (child->type == KRYON_AST_INCLUDE_DIRECTIVE) {
                 if (process_single_include(child, node, i, parser, base_dir) != KRYON_SUCCESS) return KRYON_ERROR_FILE_READ_ERROR;
             } else {
                 if (process_includes_recursive(child, parser, base_dir) != KRYON_SUCCESS) return KRYON_ERROR_FILE_READ_ERROR;
                 i++;
             }
         }
     }
     return KRYON_SUCCESS;
 }
 
 static KryonResult process_include_directives(KryonASTNode *ast, KryonParser *parser, const char *base_dir) {
     if (!ast || !parser || !base_dir) KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_INVALID_ARGUMENT, KRYON_SEVERITY_ERROR, "NULL argument to process_include_directives");
     return process_includes_recursive(ast, parser, base_dir);
 }
 
 static KryonASTNode *find_metadata_node(const KryonASTNode *ast) {
     if (!ast || ast->type != KRYON_AST_ROOT) return NULL;
     for (size_t i = 0; i < ast->data.element.child_count; i++) {
         KryonASTNode *child = ast->data.element.children[i];
         if (child && child->type == KRYON_AST_METADATA_DIRECTIVE) return child;
     }
     return NULL;
 }
 
 static KryonResult create_app_element_with_metadata(const KryonASTNode *original_ast, KryonParser *parser, KryonASTNode **out_node) {
     *out_node = NULL;
     KryonASTNode *app_element = kryon_alloc(sizeof(KryonASTNode));
     if (!app_element) KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to allocate App element");
     memset(app_element, 0, sizeof(KryonASTNode));
     app_element->type = KRYON_AST_ELEMENT;
     app_element->data.element.element_type = strdup("App");
     if (!app_element->data.element.element_type) { kryon_free(app_element); KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to allocate App element type"); }
     KryonASTNode *metadata = find_metadata_node(original_ast);
     size_t prop_count = metadata ? metadata->data.element.property_count : 0;
     app_element->data.element.property_capacity = prop_count > 0 ? prop_count : 4;
     app_element->data.element.properties = kryon_alloc(app_element->data.element.property_capacity * sizeof(KryonASTNode*));
     if (!app_element->data.element.properties) { free(app_element->data.element.element_type); kryon_free(app_element); KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to allocate App properties"); }
     if (metadata) {
         for (size_t i = 0; i < metadata->data.element.property_count; i++) {
             KryonASTNode *app_prop = NULL;
             if (deep_copy_ast_node(metadata->data.element.properties[i], &app_prop) != KRYON_SUCCESS) continue;
             app_element->data.element.properties[app_element->data.element.property_count++] = app_prop;
         }
     }
     app_element->data.element.child_count = 0; app_element->data.element.children = NULL; app_element->data.element.child_capacity = 0;
     *out_node = app_element;
     return KRYON_SUCCESS;
 }
 
 static KryonASTNode *ensure_app_root_container(const KryonASTNode *original_ast, KryonParser *parser, bool debug) {
     if (!original_ast || original_ast->type != KRYON_AST_ROOT) return (KryonASTNode*)original_ast;
     for (size_t i = 0; i < original_ast->data.element.child_count; i++) {
         KryonASTNode *child = original_ast->data.element.children[i];
         if (child && child->type == KRYON_AST_ELEMENT && child->data.element.element_type && strcmp(child->data.element.element_type, "App") == 0) return (KryonASTNode*)original_ast;
     }
     KryonASTNode *new_root = kryon_alloc(sizeof(KryonASTNode));
     if (!new_root) { KRYON_ERROR_SET(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to allocate new root for App container"); return NULL; }
     memset(new_root, 0, sizeof(KryonASTNode));
     new_root->type = KRYON_AST_ROOT;
     new_root->data.element.child_capacity = original_ast->data.element.child_count + 2;
     new_root->data.element.children = kryon_alloc(new_root->data.element.child_capacity * sizeof(KryonASTNode*));
     if (!new_root->data.element.children) { kryon_free(new_root); KRYON_ERROR_SET(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to allocate children for new root"); return NULL; }
     KryonASTNode *app_element = NULL;
     if (create_app_element_with_metadata(original_ast, parser, &app_element) != KRYON_SUCCESS) { kryon_free(new_root->data.element.children); kryon_free(new_root); return NULL; }
     size_t app_child_count = 0;
     for (size_t i = 0; i < original_ast->data.element.child_count; i++) {
         KryonASTNode *child = original_ast->data.element.children[i];
         if (child) {
             switch (child->type) {
                 case KRYON_AST_METADATA_DIRECTIVE: case KRYON_AST_STYLE_BLOCK: case KRYON_AST_THEME_DEFINITION:
                 case KRYON_AST_VARIABLE_DEFINITION: case KRYON_AST_FUNCTION_DEFINITION: case KRYON_AST_COMPONENT:
                 case KRYON_AST_EVENT_DIRECTIVE: case KRYON_AST_CONST_DEFINITION: break;
                 default: app_child_count++; break;
             }
         }
     }
     app_element->data.element.child_capacity = app_child_count + (debug ? 1 : 0);
     app_element->data.element.children = kryon_alloc(app_element->data.element.child_capacity * sizeof(KryonASTNode*));
     if (!app_element->data.element.children) { KRYON_ERROR_SET(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to allocate children for App element"); return NULL; }
     for (size_t i = 0; i < original_ast->data.element.child_count; i++) {
         KryonASTNode *child = original_ast->data.element.children[i];
         if (child) {
             switch (child->type) {
                 case KRYON_AST_METADATA_DIRECTIVE: case KRYON_AST_STYLE_BLOCK: case KRYON_AST_THEME_DEFINITION:
                 case KRYON_AST_VARIABLE_DEFINITION: case KRYON_AST_FUNCTION_DEFINITION: case KRYON_AST_COMPONENT:
                 case KRYON_AST_EVENT_DIRECTIVE: case KRYON_AST_CONST_DEFINITION:
                     new_root->data.element.children[new_root->data.element.child_count++] = child;
                     break;
                 default:
                     app_element->data.element.children[app_element->data.element.child_count++] = child;
                     break;
             }
         }
     }
     new_root->data.element.children[new_root->data.element.child_count++] = app_element;
     return new_root;
 }
 
 // =============================================================================
 //  Utility Functions Implementation (Unchanged from Original)
 // =============================================================================
 
 static bool ensure_directory(const char* path) {
     if (!path) return false;
     struct stat st = {0};
     if (stat(path, &st) == -1) {
         if (mkdir(path, 0755) != 0) return false;
     }
     return true;
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
     if (stat(path, &st) == 0) return st.st_mtime;
     return 0;
 }
 
 static char* get_file_basename(const char* path) {
     if (!path) return NULL;
     const char* basename = strrchr(path, '/');
     return kryon_strdup(basename ? basename + 1 : path);
 }
 
 static char* get_file_directory(const char* path) {
     if (!path) return NULL;
     const char* last_slash = strrchr(path, '/');
     if (!last_slash) return kryon_strdup(".");
     size_t dir_len = last_slash - path;
     char* directory = kryon_alloc(dir_len + 1);
     if (directory) { strncpy(directory, path, dir_len); directory[dir_len] = '\0'; }
     return directory;
 }