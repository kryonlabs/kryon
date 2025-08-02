/**
 * @file compile_command.c
 * @brief Kryon Compile Command Implementation
 */

#include "internal/lexer.h"
#include "internal/parser.h" 
#include "internal/codegen.h"
#include "internal/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>

// For AST validation
extern size_t kryon_ast_validate(const KryonASTNode *node, char **errors, size_t max_errors);

// Forward declarations
static KryonASTNode *ensure_app_root_container(const KryonASTNode *original_ast, KryonParser *parser, bool debug);
static KryonASTNode *create_app_element_with_metadata(const KryonASTNode *original_ast, KryonParser *parser);
static KryonASTNode *find_metadata_node(const KryonASTNode *ast);
static KryonASTNode *inject_debug_inspector_element(KryonParser *parser);
static void print_generated_kry_content(const KryonASTNode *ast, const char *title);
static void print_element_content(const KryonASTNode *element, int indent);
static void debug_print_ast(const KryonASTNode *node, int depth);

int compile_command(int argc, char *argv[]) {
    const char *input_file = NULL;
    const char *output_file = NULL;
    bool verbose = false;
    bool optimize = false;
    bool debug = false;
    
    // Parse arguments
    static struct option long_options[] = {
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"optimize", no_argument, 0, 'O'},
        {"debug", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "o:vOdh", long_options, NULL)) != -1) {
        switch (c) {
            case 'o':
                output_file = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case 'O':
                optimize = true;
                break;
            case 'd':
                debug = true;
                break;
            case 'h':
                printf("Usage: kryon compile <file.kry> [options]\n");
                printf("Options:\n");
                printf("  -o, --output <file>    Output file name\n");
                printf("  -v, --verbose          Verbose output\n");
                printf("  -O, --optimize         Enable optimizations\n");
                printf("  -d, --debug            Enable debug mode with inspector\n");
                printf("  -h, --help             Show this help\n");
                return 0;
            case '?':
                return 1;
            default:
                abort();
        }
    }
    
    if (optind >= argc) {
        fprintf(stderr, "Error: No input file specified\n");
        return 1;
    }
    
    input_file = argv[optind];
    
    // Generate output filename if not specified
    char output_buffer[256];
    if (!output_file) {
        const char *dot = strrchr(input_file, '.');
        if (dot && strcmp(dot, ".kry") == 0) {
            size_t len = dot - input_file;
            snprintf(output_buffer, sizeof(output_buffer), "%.*s.krb", (int)len, input_file);
            output_file = output_buffer;
        } else {
            snprintf(output_buffer, sizeof(output_buffer), "%s.krb", input_file);
            output_file = output_buffer;
        }
    }
    
    if (verbose) {
        printf("Compiling: %s -> %s\n", input_file, output_file);
    }
    
    // Read input file
    FILE *file = fopen(input_file, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open input file: %s\n", input_file);
        return 1;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Initialize memory manager if needed
    if (!g_kryon_memory_manager) {
        // Create default memory configuration
        KryonMemoryConfig config = {
            .initial_heap_size = 16 * 1024 * 1024,    // 16MB
            .max_heap_size = 256 * 1024 * 1024,       // 256MB  
            .enable_leak_detection = true,
            .enable_bounds_checking = false,
            .enable_statistics = false,
            .use_system_malloc = true,
            .large_alloc_threshold = 64 * 1024        // 64KB
        };
        
        g_kryon_memory_manager = kryon_memory_init(&config);
        if (!g_kryon_memory_manager) {
            fprintf(stderr, "Error: Failed to initialize memory manager\n");
            fclose(file);
            return 1;
        }
    }
    
    // Read file content
    char *source = kryon_alloc(file_size + 1);
    if (!source) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(file);
        return 1;
    }
    
    fread(source, 1, file_size, file);
    source[file_size] = '\0';
    fclose(file);
    
    if (verbose) {
        printf("Read %ld bytes from %s\n", file_size, input_file);
    }
    
    if (debug || verbose) {
        printf("Source preview (first 200 chars): %.200s%s\n", source, file_size > 200 ? "..." : "");
    }
    
    // Create lexer
    KryonLexer *lexer = kryon_lexer_create(source, file_size, input_file, NULL);
    if (!lexer) {
        fprintf(stderr, "Error: Failed to create lexer\n");
        kryon_free(source);
        return 1;
    }
    
    // Tokenize
    if (!kryon_lexer_tokenize(lexer)) {
        fprintf(stderr, "Error: Failed to tokenize: %s\n", kryon_lexer_get_error(lexer));
        kryon_lexer_destroy(lexer);
        kryon_free(source);
        return 1;
    }
    
    if (debug || verbose) {
        const char *lexer_error = kryon_lexer_get_error(lexer);
        if (lexer_error && strlen(lexer_error) > 0) {
            printf("Lexer warning: %s\n", lexer_error);
        } else {
            printf("Lexer completed successfully.\n");
        }
    }
    
    
    // Create parser
    KryonParser *parser = kryon_parser_create(lexer, NULL);
    if (!parser) {
        fprintf(stderr, "Error: Failed to create parser\n");
        kryon_lexer_destroy(lexer);
        kryon_free(source);
        return 1;
    }
    
    // Parse AST
    if (verbose) {
        printf("Parsing...\n");
    }
    
    bool parse_success = kryon_parser_parse(parser);
    
    // Always check for parser errors
    size_t error_count;
    const char **errors = kryon_parser_get_errors(parser, &error_count);
    if (errors && error_count > 0) {
        fprintf(stderr, "Parser errors detected:\n");
        for (size_t i = 0; i < error_count; i++) {
            fprintf(stderr, "  %s\n", errors[i]);
        }
        // Fail compilation if there are any parser errors
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_free(source);
        return 1;
    }
    
    if (!parse_success) {
        fprintf(stderr, "Error: Parser failed\n");
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_free(source);
        return 1;
    }
    
    if (debug || verbose) {
        printf("No parser errors detected.\n");
    }
    
    const KryonASTNode *ast = kryon_parser_get_root(parser);
    
    // Validate AST structure and properties
    if (ast) {
        char *validation_errors[100];
        size_t validation_count = kryon_ast_validate(ast, validation_errors, 100);
        if (validation_count > 0) {
            fprintf(stderr, "Semantic errors detected:\n");
            for (size_t i = 0; i < validation_count; i++) {
                fprintf(stderr, "  %s\n", validation_errors[i]);
                kryon_free(validation_errors[i]);
            }
            kryon_parser_destroy(parser);
            kryon_lexer_destroy(lexer);
            kryon_free(source);
            return 1;
        }
    }
    if (!ast) {
        fprintf(stderr, "Error: Failed to get AST root\n");
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_free(source);
        return 1;
    }
    
    if (verbose) {
        printf("Parsing completed successfully\n");
    }
    
    // Debug: Print original AST structure
    if (debug || verbose) {
        printf("\n=== ORIGINAL AST STRUCTURE ===\n");
        debug_print_ast(ast, 0);
        printf("=== END AST STRUCTURE ===\n\n");
    }
    
    // Transform AST to ensure App root container
    KryonASTNode *transformed_ast = ensure_app_root_container(ast, parser, debug);
    if (!transformed_ast) {
        fprintf(stderr, "Error: Failed to transform AST\n");
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_free(source);
        return 1;
    }
    
    if (verbose) {
        if (debug) {
            printf("AST transformed with App root container and debug inspector\n");
        } else {
            printf("AST transformed with App root container\n");
        }
    }
    
    // Create code generator
    KryonCodeGenConfig config = optimize ? kryon_codegen_speed_config() : kryon_codegen_default_config();
    KryonCodeGenerator *codegen = kryon_codegen_create(&config);
    if (!codegen) {
        fprintf(stderr, "Error: Failed to create code generator\n");
        // AST is owned by parser, don't destroy it separately
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_free(source);
        return 1;
    }
    
    // Generate code
    if (verbose) {
        printf("Generating code...\n");
    }
    
    if (!kryon_codegen_generate(codegen, transformed_ast)) {
        size_t error_count;
        const char **errors = kryon_codegen_get_errors(codegen, &error_count);
        for (size_t i = 0; i < error_count; i++) {
            fprintf(stderr, "Error: %s\n", errors[i]);
        }
        kryon_codegen_destroy(codegen);
        // AST is owned by parser, don't destroy it separately
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_free(source);
        return 1;
    }
    
    // Write output file
    if (verbose) {
        printf("Writing output file...\n");
    }
    
    if (!kryon_write_file(codegen, output_file)) {
        fprintf(stderr, "Error: Failed to write output file: %s\n", output_file);
        kryon_codegen_destroy(codegen);
        // AST is owned by parser, don't destroy it separately
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_free(source);
        return 1;
    }
    
    // Print statistics if verbose
    if (verbose) {
        const KryonCodeGenStats *stats = kryon_codegen_get_stats(codegen);
        printf("Statistics:\n");
        printf("  Output size: %zu bytes\n", stats->output_binary_size);
        printf("  Generation time: %.3f ms\n", stats->generation_time * 1000.0);
        printf("  Elements: %zu\n", stats->output_elements);
    }
    
    printf("Compilation successful: %s\n", output_file);
    
    // Cleanup
    kryon_codegen_destroy(codegen);
    kryon_parser_destroy(parser);  // This will destroy the AST
    kryon_lexer_destroy(lexer);
    kryon_free(source);
    
    return 0;
}

// =============================================================================
// AST TRANSFORMATION FUNCTIONS
// =============================================================================

static KryonASTNode *find_metadata_node(const KryonASTNode *ast) {
    if (!ast || ast->type != KRYON_AST_ROOT) {
        return NULL;
    }
    
    for (size_t i = 0; i < ast->data.element.child_count; i++) {
        KryonASTNode *child = ast->data.element.children[i];
        if (child && child->type == KRYON_AST_METADATA_DIRECTIVE) {
            return child;
        }
    }
    
    return NULL;
}

static KryonASTNode *create_app_element_with_metadata(const KryonASTNode *original_ast, KryonParser *parser) {
    // Create App element
    KryonASTNode *app_element = kryon_alloc(sizeof(KryonASTNode));
    if (!app_element) return NULL;
    
    memset(app_element, 0, sizeof(KryonASTNode));
    app_element->type = KRYON_AST_ELEMENT;
    app_element->data.element.element_type = kryon_alloc(4);
    if (!app_element->data.element.element_type) {
        kryon_free(app_element);
        return NULL;
    }
    strcpy(app_element->data.element.element_type, "App");
    
    // Find metadata in original AST
    KryonASTNode *metadata = find_metadata_node(original_ast);
    
    // Initialize properties from metadata
    app_element->data.element.property_count = 0;
    app_element->data.element.property_capacity = 4;
    app_element->data.element.properties = kryon_alloc(4 * sizeof(KryonASTNode*));
    if (!app_element->data.element.properties) {
        kryon_free(app_element->data.element.element_type);
        kryon_free(app_element);
        return NULL;
    }
    
    // Add properties from metadata if available (metadata directive uses element structure)
    if (metadata && metadata->data.element.properties) {
        for (size_t i = 0; i < metadata->data.element.property_count; i++) {
            KryonASTNode *meta_prop = metadata->data.element.properties[i];
            if (!meta_prop) continue;
            
            // Create App property from metadata property
            KryonASTNode *app_prop = kryon_alloc(sizeof(KryonASTNode));
            if (!app_prop) continue;
            
            memset(app_prop, 0, sizeof(KryonASTNode));
            app_prop->type = KRYON_AST_PROPERTY;
            app_prop->data.property.name = kryon_alloc(strlen(meta_prop->data.property.name) + 1);
            if (app_prop->data.property.name) {
                strcpy(app_prop->data.property.name, meta_prop->data.property.name);
            }
            app_prop->data.property.value = meta_prop->data.property.value; // Share the value
            
            // Add to App element properties
            if (app_element->data.element.property_count < app_element->data.element.property_capacity) {
                app_element->data.element.properties[app_element->data.element.property_count++] = app_prop;
            }
        }
    }
    
    return app_element;
}

static KryonASTNode *ensure_app_root_container(const KryonASTNode *original_ast, KryonParser *parser, bool debug) {
    if (!original_ast || original_ast->type != KRYON_AST_ROOT) {
        return (KryonASTNode*)original_ast; // Return as-is if not a root
    }
    
    // Check if there's already an App element as the first child
    bool has_app_element = false;
    for (size_t i = 0; i < original_ast->data.element.child_count; i++) {
        KryonASTNode *child = original_ast->data.element.children[i];
        if (child && child->type == KRYON_AST_ELEMENT) {
            if (child->data.element.element_type && 
                strcmp(child->data.element.element_type, "App") == 0) {
                has_app_element = true;
                break;
            }
        }
    }
    
    if (has_app_element) {
        return (KryonASTNode*)original_ast; // Already has App element
    }
    
    // Create new root AST with App container
    KryonASTNode *new_root = kryon_alloc(sizeof(KryonASTNode));
    if (!new_root) return (KryonASTNode*)original_ast;
    
    memset(new_root, 0, sizeof(KryonASTNode));
    new_root->type = KRYON_AST_ROOT;
    new_root->data.element.child_count = 0;
    new_root->data.element.child_capacity = original_ast->data.element.child_count + 2;
    new_root->data.element.children = kryon_alloc(new_root->data.element.child_capacity * sizeof(KryonASTNode*));
    if (!new_root->data.element.children) {
        kryon_free(new_root);
        return (KryonASTNode*)original_ast;
    }
    
    
    // Copy metadata and top-level definitions (styles, functions, etc.) to root
    for (size_t i = 0; i < original_ast->data.element.child_count; i++) {
        KryonASTNode *child = original_ast->data.element.children[i];
        if (child) {
            switch (child->type) {
                case KRYON_AST_METADATA_DIRECTIVE:
                case KRYON_AST_STYLE_BLOCK:
                case KRYON_AST_THEME_DEFINITION:
                case KRYON_AST_VARIABLE_DEFINITION:
                case KRYON_AST_FUNCTION_DEFINITION:
                    // These should stay at root level
                    new_root->data.element.children[new_root->data.element.child_count++] = child;
                    break;
                default:
                    // Other nodes will be moved to App element below
                    break;
            }
        }
    }
    
    // Create App element with metadata
    KryonASTNode *app_element = create_app_element_with_metadata(original_ast, parser);
    if (!app_element) {
        kryon_free(new_root->data.element.children);
        kryon_free(new_root);
        return (KryonASTNode*)original_ast;
    }
    
    // Count only UI elements that should go into App
    size_t app_child_count = 0;
    for (size_t i = 0; i < original_ast->data.element.child_count; i++) {
        KryonASTNode *child = original_ast->data.element.children[i];
        if (child) {
            switch (child->type) {
                case KRYON_AST_ELEMENT:
                    // Only UI elements go into App
                    app_child_count++;
                    break;
                case KRYON_AST_METADATA_DIRECTIVE:
                case KRYON_AST_STYLE_BLOCK:
                case KRYON_AST_THEME_DEFINITION:
                case KRYON_AST_VARIABLE_DEFINITION:
                case KRYON_AST_FUNCTION_DEFINITION:
                    // These stay at root level, don't count them
                    break;
                default:
                    // Other types might need special handling
                    app_child_count++;
                    break;
            }
        }
    }
    
    app_element->data.element.child_count = 0;
    app_element->data.element.child_capacity = app_child_count + 1;
    app_element->data.element.children = kryon_alloc(app_element->data.element.child_capacity * sizeof(KryonASTNode*));
    if (!app_element->data.element.children) {
        kryon_free(app_element->data.element.element_type);
        kryon_free(app_element->data.element.properties);
        kryon_free(app_element);
        kryon_free(new_root->data.element.children);
        kryon_free(new_root);
        return (KryonASTNode*)original_ast;
    }
    
    // Copy only UI elements to App element
    for (size_t i = 0; i < original_ast->data.element.child_count; i++) {
        KryonASTNode *child = original_ast->data.element.children[i];
        if (child) {
            switch (child->type) {
                case KRYON_AST_ELEMENT:
                    // UI elements go into App
                    app_element->data.element.children[app_element->data.element.child_count++] = child;
                    break;
                case KRYON_AST_METADATA_DIRECTIVE:
                case KRYON_AST_STYLE_BLOCK:
                case KRYON_AST_THEME_DEFINITION:
                case KRYON_AST_VARIABLE_DEFINITION:
                case KRYON_AST_FUNCTION_DEFINITION:
                    // These stay at root level, already copied above
                    break;
                default:
                    // Handle other types (may need adjustment)
                    app_element->data.element.children[app_element->data.element.child_count++] = child;
                    break;
            }
        }
    }
    
    // Inject DebugInspector if debug mode is enabled
    if (debug) {
        KryonASTNode *debug_inspector = inject_debug_inspector_element(parser);
        if (debug_inspector && app_element->data.element.child_count < app_element->data.element.child_capacity) {
            app_element->data.element.children[app_element->data.element.child_count++] = debug_inspector;
        }
    }
    
    // Add App element to new root
    new_root->data.element.children[new_root->data.element.child_count++] = app_element;
    
    // Show generated .kry content if debug mode is enabled
    if (debug) {
        print_generated_kry_content(new_root, "Generated .kry content with DebugInspector");
    }
    
    return new_root;
}

static KryonASTNode *inject_debug_inspector_element(KryonParser *parser) {
    // Create DebugInspector element as if it was parsed from:
    // DebugInspector { }
    
    KryonASTNode *debug_inspector = kryon_alloc(sizeof(KryonASTNode));
    if (!debug_inspector) return NULL;
    
    memset(debug_inspector, 0, sizeof(KryonASTNode));
    debug_inspector->type = KRYON_AST_ELEMENT;
    
    // Set element type to "DebugInspector"
    debug_inspector->data.element.element_type = kryon_alloc(15); // strlen("DebugInspector") + 1
    if (!debug_inspector->data.element.element_type) {
        kryon_free(debug_inspector);
        return NULL;
    }
    strcpy(debug_inspector->data.element.element_type, "DebugInspector");
    
    // Initialize empty properties and children arrays
    debug_inspector->data.element.property_count = 0;
    debug_inspector->data.element.property_capacity = 4;
    debug_inspector->data.element.properties = kryon_alloc(4 * sizeof(KryonASTNode*));
    
    debug_inspector->data.element.child_count = 0;  
    debug_inspector->data.element.child_capacity = 4;
    debug_inspector->data.element.children = kryon_alloc(4 * sizeof(KryonASTNode*));
    
    if (!debug_inspector->data.element.properties || !debug_inspector->data.element.children) {
        kryon_free(debug_inspector->data.element.element_type);
        kryon_free(debug_inspector->data.element.properties);
        kryon_free(debug_inspector->data.element.children);
        kryon_free(debug_inspector);
        return NULL;
    }
    
    return debug_inspector;
}


static void print_generated_kry_content(const KryonASTNode *ast, const char *title) {
    printf("\n🔍 %s:\n", title);
    printf("================================\n");
    
    if (!ast || ast->type != KRYON_AST_ROOT) {
        printf("(Invalid AST)\n");
        return;
    }
    
    // Show injected include directive in debug mode
    printf("@include \"widgets/inspector.kry\"\n\n");
    
    // Print the generated .kry content by traversing the AST
    for (size_t i = 0; i < ast->data.element.child_count; i++) {
        KryonASTNode *child = ast->data.element.children[i];
        if (!child) continue;
        
        if (child->type == KRYON_AST_INCLUDE_DIRECTIVE) {
            printf("@include \"%s\"\n\n", child->data.element.element_type ? child->data.element.element_type : "unknown");
        } else if (child->type == KRYON_AST_METADATA_DIRECTIVE) {
            printf("@metadata {\n");
            for (size_t j = 0; j < child->data.element.property_count; j++) {
                KryonASTNode *prop = child->data.element.properties[j];
                if (prop && prop->data.property.name) {
                    printf("    %s: ", prop->data.property.name);
                    if (prop->data.property.value && 
                        prop->data.property.value->type == KRYON_AST_LITERAL &&
                        prop->data.property.value->data.literal.value.type == KRYON_VALUE_STRING) {
                        printf("\"%s\"\n", prop->data.property.value->data.literal.value.data.string_value);
                    } else {
                        printf("(value)\n");
                    }
                }
            }
            printf("}\n\n");
        } else if (child->type == KRYON_AST_ELEMENT) {
            print_element_content(child, 0);
        }
    }
    
    printf("================================\n\n");
}

static void print_element_content(const KryonASTNode *element, int indent) {
    if (!element || element->type != KRYON_AST_ELEMENT) return;
    
    // Print indentation
    for (int i = 0; i < indent; i++) printf("    ");
    
    // Print element type
    printf("%s", element->data.element.element_type ? element->data.element.element_type : "Unknown");
    
    // Print properties if any
    if (element->data.element.property_count > 0) {
        printf(" {\n");
        
        // Print properties
        for (size_t i = 0; i < element->data.element.property_count; i++) {
            KryonASTNode *prop = element->data.element.properties[i];
            if (prop && prop->data.property.name) {
                for (int j = 0; j < indent + 1; j++) printf("    ");
                printf("%s: (value)\n", prop->data.property.name);
            }
        }
        
        // Print children
        for (size_t i = 0; i < element->data.element.child_count; i++) {
            if (element->data.element.children[i]) {
                printf("\n");
                print_element_content(element->data.element.children[i], indent + 1);
            }
        }
        
        // Close brace
        for (int i = 0; i < indent; i++) printf("    ");
        printf("}\n");
    } else if (element->data.element.child_count > 0) {
        printf(" {\n");
        for (size_t i = 0; i < element->data.element.child_count; i++) {
            if (element->data.element.children[i]) {
                print_element_content(element->data.element.children[i], indent + 1);
            }
        }
        for (int i = 0; i < indent; i++) printf("    ");
        printf("}\n");
    } else {
        printf(" { }\n");
    }
}

static void debug_print_ast(const KryonASTNode *node, int depth) {
    if (!node) return;
    
    // Print indentation
    for (int i = 0; i < depth; i++) printf("  ");
    
    // Print node type and basic info
    switch (node->type) {
        case KRYON_AST_ROOT:
            printf("ROOT (children: %zu)\n", node->data.element.child_count);
            for (size_t i = 0; i < node->data.element.child_count; i++) {
                debug_print_ast(node->data.element.children[i], depth + 1);
            }
            break;
            
        case KRYON_AST_ELEMENT:
            printf("ELEMENT: %s (props: %zu, children: %zu)\n", 
                   node->data.element.element_type ? node->data.element.element_type : "NULL",
                   node->data.element.property_count,
                   node->data.element.child_count);
            
            // Print properties
            for (size_t i = 0; i < node->data.element.property_count; i++) {
                if (node->data.element.properties[i]) {
                    debug_print_ast(node->data.element.properties[i], depth + 1);
                }
            }
            
            // Print children
            for (size_t i = 0; i < node->data.element.child_count; i++) {
                if (node->data.element.children[i]) {
                    debug_print_ast(node->data.element.children[i], depth + 1);
                }
            }
            break;
            
        case KRYON_AST_PROPERTY:
            printf("PROPERTY: %s = ", node->data.property.name ? node->data.property.name : "NULL");
            if (node->data.property.value) {
                debug_print_ast(node->data.property.value, 0);
            } else {
                printf("NULL\n");
            }
            break;
            
        case KRYON_AST_STYLE_BLOCK:
            printf("STYLE_BLOCK: %s (props: %zu)\n", 
                   node->data.style.name ? node->data.style.name : "NULL",
                   node->data.style.property_count);
            for (size_t i = 0; i < node->data.style.property_count; i++) {
                if (node->data.style.properties[i]) {
                    debug_print_ast(node->data.style.properties[i], depth + 1);
                }
            }
            break;
            
        case KRYON_AST_FUNCTION_DEFINITION:
            printf("FUNCTION_DEF: %s (%s)\n", 
                   node->data.function_def.name ? node->data.function_def.name : "NULL",
                   node->data.function_def.language ? node->data.function_def.language : "NULL");
            break;
            
        case KRYON_AST_THEME_DEFINITION:
            printf("THEME_DEF: %s (vars: %zu)\n", 
                   node->data.theme.group_name ? node->data.theme.group_name : "NULL",
                   node->data.theme.variable_count);
            break;
            
        case KRYON_AST_VARIABLE_DEFINITION:
            printf("VAR_DEF: %s = ", node->data.variable_def.name ? node->data.variable_def.name : "NULL");
            if (node->data.variable_def.value) {
                debug_print_ast(node->data.variable_def.value, 0);
            } else {
                printf("NULL\n");
            }
            break;
            
        case KRYON_AST_LITERAL:
            printf("LITERAL: ");
            switch (node->data.literal.value.type) {
                case KRYON_VALUE_STRING:
                    printf("STRING \"%s\"\n", node->data.literal.value.data.string_value ? node->data.literal.value.data.string_value : "NULL");
                    break;
                case KRYON_VALUE_INTEGER:
                    printf("INT %ld\n", node->data.literal.value.data.int_value);
                    break;
                case KRYON_VALUE_FLOAT:
                    printf("FLOAT %f\n", node->data.literal.value.data.float_value);
                    break;
                case KRYON_VALUE_BOOLEAN:
                    printf("BOOL %s\n", node->data.literal.value.data.bool_value ? "true" : "false");
                    break;
                case KRYON_VALUE_NULL:
                    printf("NULL\n");
                    break;
                case KRYON_VALUE_COLOR:
                    printf("COLOR #%08X\n", node->data.literal.value.data.color_value);
                    break;
                case KRYON_VALUE_UNIT:
                    printf("UNIT %f%s\n", node->data.literal.value.data.unit_value.value, node->data.literal.value.data.unit_value.unit);
                    break;
                default:
                    printf("UNKNOWN_VALUE_TYPE\n");
                    break;
            }
            break;
            
        case KRYON_AST_METADATA_DIRECTIVE:
            printf("METADATA_DIRECTIVE (props: %zu)\n", node->data.properties.property_count);
            for (size_t i = 0; i < node->data.properties.property_count; i++) {
                if (node->data.properties.properties[i]) {
                    debug_print_ast(node->data.properties.properties[i], depth + 1);
                }
            }
            break;
            
        default:
            printf("UNKNOWN_NODE_TYPE: %d\n", node->type);
            break;
    }
}