/**
 * @file compile_command.c
 * @brief Kryon Compile Command Implementation
 */

#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "memory.h"
#include "error.h"
#include "kir_format.h"
#include "krl_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <libgen.h>
#include <unistd.h>

// For AST validation
extern size_t kryon_ast_validate(const KryonASTNode *node, char **errors, size_t max_errors);


/**
 * @brief Finds the first metadata directive node in the root of the AST.
 */
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


/**
 * @brief Creates a deep copy of an AST node.
 * @param[in] src The source node to copy.
 * @param[out] out_copy A pointer to store the newly allocated copy.
 * @return KRYON_SUCCESS on success, or an error code on failure.
 */
 static KryonResult deep_copy_ast_node(const KryonASTNode *src, KryonASTNode **out_copy) {
    if (!src) {
        *out_copy = NULL;
        return KRYON_SUCCESS;
    }

    KryonASTNode *copy = kryon_alloc(sizeof(KryonASTNode));
    if (!copy) {
        KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to allocate for AST node copy");
    }
    memcpy(copy, src, sizeof(KryonASTNode));
    copy->parent = NULL; // Parent will be set by the caller

    switch (src->type) {
        case KRYON_AST_ROOT:
        case KRYON_AST_ELEMENT:
            if (src->data.element.element_type) {
                copy->data.element.element_type = strdup(src->data.element.element_type);
            }
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
            if (src->data.literal.value.type == KRYON_VALUE_STRING && src->data.literal.value.data.string_value) {
                copy->data.literal.value.data.string_value = strdup(src->data.literal.value.data.string_value);
            }
            break;
        // Add other cases for deep copying as needed, e.g., KRYON_AST_FUNCTION_DEFINITION
        default:
            // For now, other types use the shallow copy from memcpy
            break;
    }

    *out_copy = copy;
    return KRYON_SUCCESS;
}



/**
 * @brief Creates an 'App' element, populating its properties from any @metadata directive found.
 * @note This version correctly allocates memory for the element and its properties.
 * @param[in] original_ast The source AST to scan for metadata.
 * @param[in] parser The parser instance (for context, not used here).
 * @param[out] out_node Pointer to store the newly created AST node.
 * @return KRYON_SUCCESS or an error code.
 */
 static KryonResult create_app_element_with_metadata(const KryonASTNode *original_ast, KryonParser *parser, KryonASTNode **out_node) {
    *out_node = NULL;
    KryonASTNode *app_element = kryon_alloc(sizeof(KryonASTNode));
    if (!app_element) {
        KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to allocate App element");
    }
    memset(app_element, 0, sizeof(KryonASTNode));
    
    app_element->type = KRYON_AST_ELEMENT;
    app_element->data.element.element_type = strdup("App");
    if (!app_element->data.element.element_type) {
        kryon_free(app_element);
        KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to allocate App element type string");
    }

    // Find metadata in the original AST
    KryonASTNode *metadata = find_metadata_node(original_ast);
    
    // Initialize properties from metadata
    size_t prop_count = metadata ? metadata->data.element.property_count : 0;
    app_element->data.element.property_count = 0;
    app_element->data.element.property_capacity = prop_count > 0 ? prop_count : 4; // Default capacity
    app_element->data.element.properties = kryon_alloc(app_element->data.element.property_capacity * sizeof(KryonASTNode*));
    if (!app_element->data.element.properties) {
        free(app_element->data.element.element_type);
        kryon_free(app_element);
        KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to allocate properties for App element");
    }

    // If metadata exists, deep copy its properties into the new App element
    if (metadata) {
        for (size_t i = 0; i < metadata->data.element.property_count; i++) {
            KryonASTNode *meta_prop = metadata->data.element.properties[i];
            if (!meta_prop) continue;

            KryonASTNode *app_prop = NULL;
            // A deep copy is safer to avoid shared ownership issues.
            if (deep_copy_ast_node(meta_prop, &app_prop) != KRYON_SUCCESS) {
                // In a real scenario, you'd clean up partially created properties here.
                KRYON_LOG_WARNING("Failed to copy metadata property: %s", meta_prop->data.property.name);
                continue;
            }
            app_element->data.element.properties[app_element->data.element.property_count++] = app_prop;
        }
    }
    
    // Children will be allocated and populated by the calling function
    app_element->data.element.child_count = 0;
    app_element->data.element.children = NULL;
    app_element->data.element.child_capacity = 0;

    *out_node = app_element;
    return KRYON_SUCCESS;
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


static void print_generated_kry_content(const KryonASTNode *ast, const char *title) {
    printf("\n🔍 %s:\n", title);
    printf("================================\n");
    
    if (!ast || ast->type != KRYON_AST_ROOT) {
        printf("(Invalid AST)\n");
        return;
    }
    
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


/**
 * @brief Ensures the final AST has a single root 'App' element containing all UI nodes.
 * @note This version restores the correct logic for separating root-level vs. UI-level nodes.
 * @param[in] original_ast The parsed AST.
 * @param[in] parser The parser instance.
 * @param[in] debug Flag to enable debug features like the inspector.
 * @return A pointer to the transformed AST, or NULL on failure.
 */
 static KryonASTNode *ensure_app_root_container(const KryonASTNode *original_ast, KryonParser *parser, bool debug) {
    if (!original_ast || original_ast->type != KRYON_AST_ROOT) {
        return (KryonASTNode*)original_ast;
    }

    // Check if an 'App' element already exists at the top level
    bool has_app_element = false;
    for (size_t i = 0; i < original_ast->data.element.child_count; i++) {
        KryonASTNode *child = original_ast->data.element.children[i];
        if (child && child->type == KRYON_AST_ELEMENT && child->data.element.element_type && 
            strcmp(child->data.element.element_type, "App") == 0) {
            has_app_element = true;
            break;
        }
    }

    if (has_app_element) {
        return (KryonASTNode*)original_ast; // AST is already structured correctly
    }

    // --- Create a new root to hold the App container and other top-level definitions ---
    KryonASTNode *new_root = kryon_alloc(sizeof(KryonASTNode));
    if (!new_root) {
        KRYON_ERROR_SET(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to allocate new root for App container");
        return NULL;
    }
    memset(new_root, 0, sizeof(KryonASTNode));
    new_root->type = KRYON_AST_ROOT;
    new_root->data.element.child_capacity = original_ast->data.element.child_count + 2;
    new_root->data.element.children = kryon_alloc(new_root->data.element.child_capacity * sizeof(KryonASTNode*));
    if (!new_root->data.element.children) {
        kryon_free(new_root);
        KRYON_ERROR_SET(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to allocate children for new root");
        return NULL;
    }
    
    // --- Create the 'App' element itself ---
    KryonASTNode *app_element = NULL;
    if (create_app_element_with_metadata(original_ast, parser, &app_element) != KRYON_SUCCESS) {
        kryon_free(new_root->data.element.children);
        kryon_free(new_root);
        return NULL; // Error was already set
    }

    // --- Count how many nodes need to be moved inside the 'App' element ---
    size_t app_child_count = 0;
    for (size_t i = 0; i < original_ast->data.element.child_count; i++) {
        KryonASTNode *child = original_ast->data.element.children[i];
        if (child) {
            switch (child->type) {
                case KRYON_AST_METADATA_DIRECTIVE:
                case KRYON_AST_STYLE_BLOCK:
                case KRYON_AST_THEME_DEFINITION:
                case KRYON_AST_VARIABLE_DEFINITION:
                case KRYON_AST_FUNCTION_DEFINITION:
                case KRYON_AST_COMPONENT:
                case KRYON_AST_EVENT_DIRECTIVE:
                case KRYON_AST_CONST_DEFINITION:
                    // These stay at the root level, so don't count them for the App element
                    break;
                default:
                    app_child_count++; // All others go inside App
                    break;
            }
        }
    }

    // --- Allocate children array for the 'App' element ---
    app_element->data.element.child_capacity = app_child_count + (debug ? 1 : 0); // +1 for debug inspector
    app_element->data.element.children = kryon_alloc(app_element->data.element.child_capacity * sizeof(KryonASTNode*));
    if (!app_element->data.element.children) {
        // Proper cleanup needed here for app_element and new_root
        KRYON_ERROR_SET(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to allocate children for App element");
        return NULL;
    }

    // --- Populate the new_root and the app_element ---
    for (size_t i = 0; i < original_ast->data.element.child_count; i++) {
        KryonASTNode *child = original_ast->data.element.children[i];
        if (child) {
            switch (child->type) {
                case KRYON_AST_METADATA_DIRECTIVE:
                case KRYON_AST_STYLE_BLOCK:
                case KRYON_AST_THEME_DEFINITION:
                case KRYON_AST_VARIABLE_DEFINITION:
                case KRYON_AST_FUNCTION_DEFINITION:
                case KRYON_AST_COMPONENT:
                case KRYON_AST_EVENT_DIRECTIVE:
                case KRYON_AST_CONST_DEFINITION:
                    // Add root-level nodes to the new_root
                    new_root->data.element.children[new_root->data.element.child_count++] = child;
                    break;
                default:
                    // Add UI-level nodes to the app_element
                    app_element->data.element.children[app_element->data.element.child_count++] = child;
                    break;
            }
        }
    }
    
    // Inject Debug Inspector if needed
    if (debug) {
        KryonASTNode *debug_inspector = inject_debug_inspector_element(parser);
        if (debug_inspector && app_element->data.element.child_count < app_element->data.element.child_capacity) {
            app_element->data.element.children[app_element->data.element.child_count++] = debug_inspector;
        }
    }
    
    // Add the fully populated 'App' element to the new root
    new_root->data.element.children[new_root->data.element.child_count++] = app_element;
    
    if (debug) {
        print_generated_kry_content(new_root, "Generated .kry content with DebugInspector");
    }
    
    return new_root;
}



/**
 * @brief Merges the AST from an included file into a parent node.
 * @return KRYON_SUCCESS or an error code.
 */
static KryonResult merge_include_ast(KryonASTNode *parent, size_t include_index, 
                                     const KryonASTNode *include_ast, KryonParser *main_parser) {
    if (!parent || !include_ast || !main_parser) {
        KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_INVALID_ARGUMENT, KRYON_SEVERITY_ERROR, "NULL argument provided for merging AST");
    }

    if (include_ast->type != KRYON_AST_ROOT) {
        KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_PARSE_ERROR, KRYON_SEVERITY_ERROR, "Included file does not have a valid root node");
    }
    
    size_t new_children_count = include_ast->data.element.child_count;
    if (new_children_count == 0) return KRYON_SUCCESS; // Nothing to merge

    size_t current_count = parent->data.element.child_count;
    // Replace the @include directive, so net gain is new_children_count - 1
    size_t new_total = current_count + new_children_count - 1;

    if (new_total > parent->data.element.child_capacity) {
        size_t new_capacity = new_total * 2;
        KryonASTNode **new_children = kryon_realloc(parent->data.element.children, new_capacity * sizeof(KryonASTNode*));
        if (!new_children) {
            KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to expand children array for include merge");
        }
        parent->data.element.children = new_children;
        parent->data.element.child_capacity = new_capacity;
    }

    // Shift elements after the include directive to make space
    memmove(&parent->data.element.children[include_index + new_children_count],
            &parent->data.element.children[include_index + 1],
            (current_count - include_index - 1) * sizeof(KryonASTNode*));

    // Copy new children from the included AST into the created space
    for (size_t i = 0; i < new_children_count; i++) {
        KryonASTNode *copied_child = NULL;
        if (deep_copy_ast_node(include_ast->data.element.children[i], &copied_child) != KRYON_SUCCESS) {
            // This is a complex failure case; would need to clean up partially copied nodes.
            return KRYON_ERROR_OUT_OF_MEMORY;
        }
        parent->data.element.children[include_index + i] = copied_child;
        if(copied_child) copied_child->parent = parent;
    }
    
    parent->data.element.child_count = new_total;
    return KRYON_SUCCESS;
}


/**
 * @brief Processes a single @include directive.
 * @return KRYON_SUCCESS or an error code.
 */
static KryonResult process_single_include(KryonASTNode *include_node, KryonASTNode *parent, size_t index, 
                                          KryonParser *parser, const char *base_dir) {
    if (!include_node || include_node->type != KRYON_AST_INCLUDE_DIRECTIVE) {
        KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_INVALID_ARGUMENT, KRYON_SEVERITY_ERROR, "Invalid node passed to process_single_include");
    }

    const char *include_path = include_node->data.element.element_type;
    if (!include_path) {
        KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_PARSE_ERROR, KRYON_SEVERITY_ERROR, "Include directive is missing a file path");
    }

    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, include_path);
    KRYON_LOG_INFO("Processing include: %s", full_path);

    if (access(full_path, R_OK) != 0) {
        KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_FILE_NOT_FOUND, KRYON_SEVERITY_ERROR, "Cannot read included file");
    }

    // --- Resources for this function ---
    FILE *file = NULL;
    char *source = NULL;
    KryonLexer *include_lexer = NULL;
    KryonParser *include_parser = NULL;
    KryonResult result = KRYON_SUCCESS;

    file = fopen(full_path, "r");
    if (!file) {
        KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_FILE_READ_ERROR, KRYON_SEVERITY_ERROR, "Cannot open included file");
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    source = kryon_alloc(file_size + 1);
    if (!source) {
        result = KRYON_ERROR_OUT_OF_MEMORY;
        goto include_cleanup;
    }
    
    fread(source, 1, file_size, file);
    source[file_size] = '\0';
    
    include_lexer = kryon_lexer_create(source, file_size, full_path, NULL);
    if (!include_lexer) {
        result = KRYON_ERROR_COMPILATION_FAILED;
        goto include_cleanup;
    }

    if (!kryon_lexer_tokenize(include_lexer)) {
        KRYON_ERROR_SET(KRYON_ERROR_SYNTAX_ERROR, KRYON_SEVERITY_ERROR, kryon_lexer_get_error(include_lexer));
        result = KRYON_ERROR_SYNTAX_ERROR;
        goto include_cleanup;
    }

    include_parser = kryon_parser_create(include_lexer, NULL);
    if (!include_parser) {
        result = KRYON_ERROR_COMPILATION_FAILED;
        goto include_cleanup;
    }

    if (!kryon_parser_parse(include_parser)) {
        // Log as a warning but continue, as the original code did
        KRYON_LOG_WARNING("Included file '%s' has parse errors but continuing.", full_path);
    }

    const KryonASTNode *include_ast = kryon_parser_get_root(include_parser);
    if (!include_ast) {
        KRYON_ERROR_SET(KRYON_ERROR_PARSE_ERROR, KRYON_SEVERITY_ERROR, "Failed to get AST from included file");
        result = KRYON_ERROR_PARSE_ERROR;
        goto include_cleanup;
    }

    result = merge_include_ast(parent, index, include_ast, parser);

include_cleanup:
    if (file) fclose(file);
    if (include_parser) kryon_parser_destroy(include_parser);
    if (include_lexer) kryon_lexer_destroy(include_lexer);
    if (source) kryon_free(source);
    
    return result;
}


/**
 * @brief Recursively processes include directives in an AST.
 * @return KRYON_SUCCESS or an error code.
 */
 
static KryonResult process_includes_recursive(KryonASTNode *node, KryonParser *parser, const char *base_dir) {
    if (!node) {
        return KRYON_SUCCESS;
    }

    if (node->type == KRYON_AST_ROOT || node->type == KRYON_AST_ELEMENT) {
        // Use a 'while' loop for safety, as the child count can change during iteration.
        size_t i = 0;
        while (i < node->data.element.child_count) {
            KryonASTNode *child = node->data.element.children[i];
            
            if (!child) {
                i++;
                continue;
            }

            if (child->type == KRYON_AST_INCLUDE_DIRECTIVE) {
                KryonResult res = process_single_include(child, node, i, parser, base_dir);
                if (res != KRYON_SUCCESS) {
                    return res; // Propagate the error
                }
                // Do NOT increment 'i'. After a merge, the new nodes are now at index 'i',
                // and the loop will re-process from this index to handle nested includes.
                // If the include was empty, a node was removed, so we also need to re-evaluate from 'i'.
            } else {
                // If it's not an include, process its children recursively.
                KryonResult res = process_includes_recursive(child, parser, base_dir);
                if (res != KRYON_SUCCESS) {
                    return res; // Propagate the error
                }
                // Only increment the index if we did NOT process an include at this position.
                i++;
            }
        }
    }
    return KRYON_SUCCESS;
}


/**
 * @brief Kicks off the recursive include processing.
 * @return KRYON_SUCCESS or an error code.
 */
 static KryonResult process_include_directives(KryonASTNode *ast, KryonParser *parser, const char *base_dir) {
    if (!ast || !parser || !base_dir) {
        KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_INVALID_ARGUMENT, KRYON_SEVERITY_ERROR, "NULL argument to process_include_directives");
    }
    return process_includes_recursive(ast, parser, base_dir);
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



static KryonASTNode *inject_debug_inspector_include(KryonParser *parser) {
    // Create include directive as if it was parsed from:
    // @include "debug/inspector.kry"
    
    KryonASTNode *include_directive = kryon_alloc(sizeof(KryonASTNode));
    if (!include_directive) return NULL;
    
    memset(include_directive, 0, sizeof(KryonASTNode));
    include_directive->type = KRYON_AST_INCLUDE_DIRECTIVE;
    
    // Set include path to "debug/inspector.kry"
    include_directive->data.element.element_type = kryon_alloc(20); // strlen("debug/inspector.kry") + 1
    if (!include_directive->data.element.element_type) {
        kryon_free(include_directive);
        return NULL;
    }
    strcpy(include_directive->data.element.element_type, "debug/inspector.kry");
    
    // Initialize empty properties and children arrays
    include_directive->data.element.property_count = 0;
    include_directive->data.element.properties = NULL;
    include_directive->data.element.child_count = 0;
    include_directive->data.element.children = NULL;
    include_directive->data.element.child_capacity = 0;
    
    return include_directive;
}


/**
 * @brief Compiles a .kry file into a .krb binary using the structured error system.
 * @param argc Argument count.
 * @param argv Argument values.
 * @return KRYON_SUCCESS on success, or a KryonResult error code on failure.
 */
 KryonResult compile_command(int argc, char *argv[]) {
    const char *input_file = NULL;
    const char *output_file = NULL;
    const char *kir_output_file = NULL;  // KIR output path
    bool verbose = false;
    bool optimize = false;
    bool debug = false;
    bool no_krb = false;                   // Skip KRB generation
    bool no_cache = false;                 // Output to same directory instead of .kryon_cache
    bool is_kir_input = false;             // Input is .kir file
    bool is_krl_input = false;             // Input is .krl file

    // --- Resource Handles for Centralized Cleanup ---
    FILE *file = NULL;
    char *source = NULL;
    KryonLexer *lexer = NULL;
    KryonParser *parser = NULL;
    KryonCodeGenerator *codegen = NULL;
    char *base_dir = NULL;
    char *auto_kir_path = NULL;
    size_t error_count = 0;
    KryonResult result = KRYON_SUCCESS;

    // Initialize the error and logging system
    if (kryon_error_init() != KRYON_SUCCESS) {
        fprintf(stderr, "Fatal: Could not initialize the error system.\n");
        return KRYON_ERROR_PLATFORM_ERROR;
    }

    // --- Argument Parsing ---
    static struct option long_options[] = {
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"optimize", no_argument, 0, 'O'},
        {"debug", no_argument, 0, 'd'},
        {"kir-output", required_argument, 0, 'k'},  // KIR output
        {"no-krb", no_argument, 0, 'n'},            // KIR only
        {"no-cache", no_argument, 0, 'C'},          // Disable .kryon_cache
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "o:k:nvOdCh", long_options, NULL)) != -1) {
        switch (c) {
            case 'o': output_file = optarg; break;
            case 'k': kir_output_file = optarg; break;
            case 'n': no_krb = true; break;
            case 'C': no_cache = true; break;
            case 'v': verbose = true; break;
            case 'O': optimize = true; break;
            case 'd': debug = true; break;
            case 'h':
                printf("Usage: kryon compile <file.kry|file.kir|file.krl> [options]\n");
                printf("Options:\n");
                printf("  -o, --output <file>      Output .krb file name\n");
                printf("  -k, --kir-output <file>  Output .kir file name\n");
                printf("  -n, --no-krb             Generate KIR only (skip KRB)\n");
                printf("  -C, --no-cache           Output to same directory instead of .kryon_cache/\n");
                printf("  -v, --verbose            Verbose output\n");
                printf("  -O, --optimize           Enable optimizations\n");
                printf("  -d, --debug              Enable debug mode with inspector\n");
                printf("  -h, --help               Show this help\n");
                result = KRYON_SUCCESS;
                goto cleanup;
            case '?':
                KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_INVALID_ARGUMENT, KRYON_SEVERITY_ERROR, "Invalid command-line argument.");
            default:
                abort();
        }
    }

    if (debug) kryon_log_set_level(KRYON_SEVERITY_DEBUG);
    else if (verbose) kryon_log_set_level(KRYON_SEVERITY_INFO);
    else kryon_log_set_level(KRYON_SEVERITY_WARNING);
    
    if (optind >= argc) {
        KRYON_SET_ERROR_AND_RETURN(KRYON_ERROR_INVALID_ARGUMENT, KRYON_SEVERITY_ERROR, "No input file specified");
    }
    input_file = argv[optind];

    // --- Detect KIR and KRL input files ---
    size_t input_len = strlen(input_file);
    if (input_len > 4 && strcmp(input_file + input_len - 4, ".kir") == 0) {
        is_kir_input = true;
        KRYON_LOG_INFO("Detected KIR input file");
    } else if (input_len > 4 && strcmp(input_file + input_len - 4, ".krl") == 0) {
        is_krl_input = true;
        KRYON_LOG_INFO("Detected KRL (KRyon Lisp) input file");
    }

    // --- Output Filename Generation ---
    char output_buffer[256];
    char *auto_krb_cache_path = NULL;  // For .kryon_cache output
    if (!output_file && !no_krb) {
        if (no_cache) {
            // Old behavior: same directory
            const char *dot = strrchr(input_file, '.');
            if (dot && (strcmp(dot, ".kry") == 0 || strcmp(dot, ".kir") == 0 || strcmp(dot, ".krl") == 0)) {
                size_t len = dot - input_file;
                snprintf(output_buffer, sizeof(output_buffer), "%.*s.krb", (int)len, input_file);
            } else {
                snprintf(output_buffer, sizeof(output_buffer), "%s.krb", input_file);
            }
            output_file = output_buffer;
        } else {
            // New behavior: .kryon_cache directory
            auto_krb_cache_path = kryon_cache_get_output_path(input_file, ".krb", true);
            if (auto_krb_cache_path) {
                output_file = auto_krb_cache_path;
            } else {
                // Fallback to old behavior on error
                const char *dot = strrchr(input_file, '.');
                if (dot && (strcmp(dot, ".kry") == 0 || strcmp(dot, ".kir") == 0 || strcmp(dot, ".krl") == 0)) {
                    size_t len = dot - input_file;
                    snprintf(output_buffer, sizeof(output_buffer), "%.*s.krb", (int)len, input_file);
                } else {
                    snprintf(output_buffer, sizeof(output_buffer), "%s.krb", input_file);
                }
                output_file = output_buffer;
            }
        }
    }

    if (no_krb) {
        KRYON_LOG_INFO("Compiling (KIR only): %s", input_file);
    } else {
        KRYON_LOG_INFO("Compiling: %s -> %s", input_file, output_file);
    }

    // --- File Reading ---
    file = fopen(input_file, "r");
    if (!file) {
        KRYON_ERROR_SET(KRYON_ERROR_FILE_NOT_FOUND, KRYON_SEVERITY_ERROR, "Cannot open input file");
        result = KRYON_ERROR_FILE_NOT_FOUND;
        goto cleanup;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // --- Memory Manager Init ---
    if (!g_kryon_memory_manager) {
        KryonMemoryConfig mem_config = {
            .initial_heap_size = 16 * 1024 * 1024, .max_heap_size = 256 * 1024 * 1024, 
            .enable_leak_detection = true, .use_system_malloc = true
        };
        g_kryon_memory_manager = kryon_memory_init(&mem_config);
        if (!g_kryon_memory_manager) {
            KRYON_ERROR_SET(KRYON_ERROR_MEMORY_CORRUPTION, KRYON_SEVERITY_FATAL, "Failed to initialize memory manager");
            result = KRYON_ERROR_MEMORY_CORRUPTION;
            goto cleanup;
        }
    }
    
    source = kryon_alloc(file_size + 1);
    if (!source) {
        KRYON_ERROR_SET(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Memory allocation failed for source file");
        result = KRYON_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    
    fread(source, 1, file_size, file);
    source[file_size] = '\0';
    fclose(file);
    file = NULL;

    KRYON_LOG_INFO("Read %ld bytes from %s", file_size, input_file);
    KRYON_LOG_DEBUG("Source preview (first 200 chars): %.200s%s", source, file_size > 200 ? "..." : "");

    // --- Check if input is KRL file - convert to KIR first ---
    char *temp_kir_file = NULL;
    if (is_krl_input) {
        KRYON_LOG_INFO("Transpiling KRL to KIR: %s", input_file);

        // Generate temp KIR file path
        if (no_cache) {
            const char *dot = strrchr(input_file, '.');
            if (dot) {
                size_t len = dot - input_file;
                char temp_buffer[256];
                snprintf(temp_buffer, sizeof(temp_buffer), "%.*s.kir", (int)len, input_file);
                temp_kir_file = strdup(temp_buffer);
            }
        } else {
            temp_kir_file = kryon_cache_get_output_path(input_file, ".kir", true);
        }

        if (!temp_kir_file) {
            KRYON_ERROR_SET(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_ERROR, "Failed to generate KIR output path");
            result = KRYON_ERROR_OUT_OF_MEMORY;
            goto cleanup;
        }

        // Convert KRL to KIR
        if (!krl_compile_to_kir(input_file, temp_kir_file)) {
            KRYON_ERROR_SET(KRYON_ERROR_COMPILATION_FAILED, KRYON_SEVERITY_ERROR, "Failed to transpile KRL to KIR");
            result = KRYON_ERROR_COMPILATION_FAILED;
            free(temp_kir_file);
            goto cleanup;
        }

        KRYON_LOG_INFO("Generated KIR file: %s", temp_kir_file);

        // Now treat it as KIR input
        is_kir_input = true;
        input_file = temp_kir_file;

        // Re-read the generated KIR file
        kryon_free(source);
        file = fopen(input_file, "r");
        if (!file) {
            KRYON_ERROR_SET(KRYON_ERROR_FILE_NOT_FOUND, KRYON_SEVERITY_ERROR, "Cannot open generated KIR file");
            result = KRYON_ERROR_FILE_NOT_FOUND;
            free(temp_kir_file);
            goto cleanup;
        }

        fseek(file, 0, SEEK_END);
        file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        source = kryon_alloc(file_size + 1);
        if (!source) {
            KRYON_ERROR_SET(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Memory allocation failed for KIR file");
            result = KRYON_ERROR_OUT_OF_MEMORY;
            fclose(file);
            file = NULL;
            free(temp_kir_file);
            goto cleanup;
        }

        fread(source, 1, file_size, file);
        source[file_size] = '\0';
        fclose(file);
        file = NULL;
    }

    // --- Check if input is KIR file ---
    KryonASTNode *transformed_ast = NULL;

    if (is_kir_input) {
        // --- Read KIR file ---
        KRYON_LOG_INFO("Reading KIR file: %s", input_file);

        KryonKIRReader *kir_reader = kryon_kir_reader_create(NULL);
        if (!kir_reader) {
            KRYON_ERROR_SET(KRYON_ERROR_COMPILATION_FAILED, KRYON_SEVERITY_ERROR, "Failed to create KIR reader");
            result = KRYON_ERROR_COMPILATION_FAILED;
            goto cleanup;
        }

        KryonASTNode *kir_ast = NULL;
        if (!kryon_kir_read_file(kir_reader, input_file, &kir_ast)) {
            size_t kir_error_count;
            const char **kir_errors = kryon_kir_reader_get_errors(kir_reader, &kir_error_count);
            KRYON_LOG_ERROR("Failed to read KIR file:");
            for (size_t i = 0; i < kir_error_count; i++) {
                KRYON_LOG_ERROR("  %s", kir_errors[i]);
            }
            kryon_kir_reader_destroy(kir_reader);
            KRYON_ERROR_SET(KRYON_ERROR_COMPILATION_FAILED, KRYON_SEVERITY_ERROR, "KIR parsing failed");
            result = KRYON_ERROR_COMPILATION_FAILED;
            goto cleanup;
        }

        kryon_kir_reader_destroy(kir_reader);
        KRYON_LOG_INFO("KIR file read successfully");

        // KIR contains post-expansion AST, skip transformation
        transformed_ast = kir_ast;
    } else {
        // --- Standard compilation path: Lexing, Parsing, Transformation ---

        // --- Lexing ---
        lexer = kryon_lexer_create(source, file_size, input_file, NULL);
    if (!lexer) {
        KRYON_ERROR_SET(KRYON_ERROR_COMPILATION_FAILED, KRYON_SEVERITY_ERROR, "Failed to create lexer");
        result = KRYON_ERROR_COMPILATION_FAILED;
        goto cleanup;
    }
    
    if (!kryon_lexer_tokenize(lexer)) {
        KRYON_ERROR_SET(KRYON_ERROR_SYNTAX_ERROR, KRYON_SEVERITY_ERROR, kryon_lexer_get_error(lexer));
        result = KRYON_ERROR_SYNTAX_ERROR;
        goto cleanup;
    }
    KRYON_LOG_DEBUG("Lexer completed successfully.");

    // --- Parsing ---
    parser = kryon_parser_create(lexer, NULL);
    if (!parser) {
        KRYON_ERROR_SET(KRYON_ERROR_COMPILATION_FAILED, KRYON_SEVERITY_ERROR, "Failed to create parser");
        result = KRYON_ERROR_COMPILATION_FAILED;
        goto cleanup;
    }
    
    KRYON_LOG_INFO("Parsing...");
    kryon_parser_parse(parser);
    
    size_t error_count;
    const char **errors = kryon_parser_get_errors(parser, &error_count);
    if (error_count > 0) {
        KRYON_LOG_ERROR("Parser errors detected:");
        for (size_t i = 0; i < error_count; i++) {
            KRYON_LOG_ERROR("  %s", errors[i]);
        }
        KRYON_ERROR_SET(KRYON_ERROR_PARSE_ERROR, KRYON_SEVERITY_ERROR, "Parsing failed with one or more errors");
        result = KRYON_ERROR_PARSE_ERROR;
        goto cleanup;
    }
    KRYON_LOG_DEBUG("No parser errors detected.");
    
    const KryonASTNode *ast = kryon_parser_get_root(parser);
    if (!ast) {
        KRYON_ERROR_SET(KRYON_ERROR_INVALID_STATE, KRYON_SEVERITY_ERROR, "Failed to get AST root from parser");
        result = KRYON_ERROR_INVALID_STATE;
        goto cleanup;
    }
    
    // --- AST Validation ---
    char *validation_errors[100];
    size_t validation_count = kryon_ast_validate(ast, validation_errors, 100);
    if (validation_count > 0) {
        KRYON_LOG_ERROR("Semantic errors detected in AST:");
        for (size_t i = 0; i < validation_count; i++) {
            KRYON_LOG_ERROR("  %s", validation_errors[i]);
            kryon_free(validation_errors[i]);
        }
        KRYON_ERROR_SET(KRYON_ERROR_SEMANTIC_ERROR, KRYON_SEVERITY_ERROR, "AST validation failed");
        result = KRYON_ERROR_SEMANTIC_ERROR;
        goto cleanup;
    }
    KRYON_LOG_INFO("Parsing completed successfully.");

    // --- IMPORTANT: Original AST Debug Print ---
    if (debug || verbose) {
        KRYON_LOG_DEBUG("\n=== ORIGINAL AST STRUCTURE ===\n");
        debug_print_ast(ast, 0);
        KRYON_LOG_DEBUG("=== END AST STRUCTURE ===\n\n");
    }
    
    // --- IMPORTANT: Debug Inspector Injection ---
    if (debug) {
        KryonASTNode *include_directive = inject_debug_inspector_include(parser);
        if (include_directive) {
            KryonASTNode *mutable_ast = (KryonASTNode*)ast;
            if (mutable_ast->data.element.child_count >= mutable_ast->data.element.child_capacity) {
                // Expand if necessary
            }
            if (mutable_ast->data.element.child_count < mutable_ast->data.element.child_capacity) {
                for (size_t i = mutable_ast->data.element.child_count; i > 0; i--) {
                    mutable_ast->data.element.children[i] = mutable_ast->data.element.children[i-1];
                }
                mutable_ast->data.element.children[0] = include_directive;
                mutable_ast->data.element.child_count++;
                KRYON_LOG_INFO("Injected @include directive for DebugInspector");
            }
        }
    }
    
    // --- IMPORTANT: Include Directive Processing ---
    base_dir = strdup(input_file);
    if (!base_dir) {
        KRYON_ERROR_SET(KRYON_ERROR_OUT_OF_MEMORY, KRYON_SEVERITY_FATAL, "Failed to duplicate input file path");
        result = KRYON_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    char *last_slash = strrchr(base_dir, '/');
    if (last_slash) *last_slash = '\0';
    else strcpy(base_dir, ".");
    
    KRYON_LOG_DEBUG("Processing include directives from base_dir: %s", base_dir);
    if (process_include_directives((KryonASTNode*)ast, parser, base_dir) != KRYON_SUCCESS) {
        KRYON_ERROR_SET(KRYON_ERROR_FILE_READ_ERROR, KRYON_SEVERITY_ERROR, "Failed to process include directives");
        result = KRYON_ERROR_FILE_READ_ERROR;
        goto cleanup;
    }
    
        // --- AST Transformation ---
        transformed_ast = ensure_app_root_container(ast, parser, debug);
        if (!transformed_ast) {
            KRYON_ERROR_SET(KRYON_ERROR_COMPILATION_FAILED, KRYON_SEVERITY_ERROR, "Failed to transform AST");
            result = KRYON_ERROR_COMPILATION_FAILED;
            goto cleanup;
        }
        KRYON_LOG_INFO("AST transformed successfully.");
    } // End of standard compilation path

    // --- KIR Generation (if requested) ---
    if (kir_output_file || no_krb) {
        const char *kir_path = kir_output_file;

        // Auto-generate KIR path if not specified
        if (!kir_path && no_krb) {
            if (no_cache) {
                auto_kir_path = kryon_kir_get_output_path(input_file);
            } else {
                auto_kir_path = kryon_cache_get_output_path(input_file, ".kir", true);
                if (!auto_kir_path) {
                    // Fallback to old behavior on error
                    auto_kir_path = kryon_kir_get_output_path(input_file);
                }
            }
            kir_path = auto_kir_path;
        }

        if (kir_path) {
            KRYON_LOG_INFO("Writing KIR to: %s", kir_path);

            // For .krl files, the KIR was already generated - just copy if needed
            if (is_krl_input && temp_kir_file && strcmp(temp_kir_file, kir_path) != 0) {
                // Copy temp KIR file to output location
                FILE *src = fopen(temp_kir_file, "rb");
                FILE *dst = fopen(kir_path, "wb");
                if (src && dst) {
                    char buffer[4096];
                    size_t n;
                    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
                        fwrite(buffer, 1, n, dst);
                    }
                    fclose(src);
                    fclose(dst);
                    printf("KIR written: %s\n", kir_path);
                } else {
                    if (src) fclose(src);
                    if (dst) fclose(dst);
                    KRYON_ERROR_SET(KRYON_ERROR_FILE_WRITE_ERROR, KRYON_SEVERITY_ERROR, "Failed to copy KIR file");
                    result = KRYON_ERROR_FILE_WRITE_ERROR;
                    goto cleanup;
                }
            } else if (!is_krl_input) {
                // For .kry files, generate KIR from AST
                KryonKIRConfig kir_config = kryon_kir_default_config();
                kir_config.source_file = input_file;

                KryonKIRWriter *kir_writer = kryon_kir_writer_create(&kir_config);
                if (!kir_writer) {
                    KRYON_ERROR_SET(KRYON_ERROR_COMPILATION_FAILED, KRYON_SEVERITY_ERROR, "Failed to create KIR writer");
                    result = KRYON_ERROR_COMPILATION_FAILED;
                    goto cleanup;
                }

                if (!kryon_kir_write_file(kir_writer, transformed_ast, kir_path)) {
                    size_t kir_error_count;
                    const char **kir_errors = kryon_kir_writer_get_errors(kir_writer, &kir_error_count);
                    KRYON_LOG_ERROR("Failed to write KIR file:");
                    for (size_t i = 0; i < kir_error_count; i++) {
                        KRYON_LOG_ERROR("  %s", kir_errors[i]);
                    }
                    kryon_kir_writer_destroy(kir_writer);
                    KRYON_ERROR_SET(KRYON_ERROR_FILE_WRITE_ERROR, KRYON_SEVERITY_ERROR, "Failed to write KIR file");
                    result = KRYON_ERROR_FILE_WRITE_ERROR;
                    goto cleanup;
                }

                kryon_kir_writer_destroy(kir_writer);
                printf("KIR written: %s\n", kir_path);
            } else {
                // KIR already at correct location (temp_kir_file == kir_path)
                printf("KIR written: %s\n", kir_path);
            }
        }
    }

    // If --no-krb is set, skip code generation
    if (no_krb) {
        printf("Compilation successful (KIR only)\n");
        result = KRYON_SUCCESS;
        goto cleanup;
    }

    // --- Code Generation ---
    KryonCodeGenConfig config = optimize ? kryon_codegen_speed_config() : kryon_codegen_default_config();
    codegen = kryon_codegen_create(&config);
    if (!codegen) {
        KRYON_ERROR_SET(KRYON_ERROR_COMPILATION_FAILED, KRYON_SEVERITY_ERROR, "Failed to create code generator");
        result = KRYON_ERROR_COMPILATION_FAILED;
        goto cleanup;
    }
    
    KRYON_LOG_INFO("Generating code...");
    if (!kryon_codegen_generate(codegen, transformed_ast)) {
        const char **codegen_errors = kryon_codegen_get_errors(codegen, &error_count);
        KRYON_LOG_ERROR("Code generation failed with errors:");
        for (size_t i = 0; i < error_count; i++) {
            KRYON_LOG_ERROR("  %s", codegen_errors[i]);
        }
        KRYON_ERROR_SET(KRYON_ERROR_COMPILATION_FAILED, KRYON_SEVERITY_ERROR, "Code generation failed");
        result = KRYON_ERROR_COMPILATION_FAILED;
        goto cleanup;
    }
    
    // --- Writing Output File ---
    KRYON_LOG_INFO("Writing output file...");
    if (!kryon_write_file(codegen, output_file)) {
        KRYON_ERROR_SET(KRYON_ERROR_FILE_WRITE_ERROR, KRYON_SEVERITY_ERROR, "Failed to write output file");
        result = KRYON_ERROR_FILE_WRITE_ERROR;
        goto cleanup;
    }
    
    // --- Final Report ---
    printf("Compilation successful: %s\n", output_file);
    if (verbose) {
        const KryonCodeGenStats *stats = kryon_codegen_get_stats(codegen);
        KRYON_LOG_INFO("Statistics:");
        KRYON_LOG_INFO("  Output size: %zu bytes", stats->output_binary_size);
        KRYON_LOG_INFO("  Generation time: %.3f ms", stats->generation_time * 1000.0);
        KRYON_LOG_INFO("  Elements: %zu", stats->output_elements);
    }
    
    result = KRYON_SUCCESS;

// --- Centralized Cleanup ---
cleanup:
    KRYON_LOG_DEBUG("Cleaning up compilation resources...");
    if (file) fclose(file);
    if (base_dir) free(base_dir);
    if (auto_kir_path) free(auto_kir_path);
    if (auto_krb_cache_path) free(auto_krb_cache_path);
    if (codegen) kryon_codegen_destroy(codegen);
    if (parser) kryon_parser_destroy(parser);
    if (lexer) kryon_lexer_destroy(lexer);
    if (source) kryon_free(source);
    
    if (result != KRYON_SUCCESS) {
        const KryonError *last_error = kryon_error_get_last();
        if (last_error) {
            kryon_error_print(last_error, stderr);
        }
    }

    kryon_error_shutdown();
    return result;
}
