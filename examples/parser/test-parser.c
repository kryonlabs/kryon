/**
 * @file test-parser.c
 * @brief Parser Test Program
 * 
 * Tests the KRY parser implementation with various syntax examples.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal/parser.h"
#include "internal/lexer.h"
#include "internal/memory.h"

// Simple test KRY source
static const char *test_source = 
    "@style \"button\" {\n"
    "    width: 100\n"
    "    height: 40\n"
    "    backgroundColor: \"#4285f4FF\"\n"
    "}\n"
    "\n"
    "Container {\n"
    "    class: \"main\"\n"
    "    \n"
    "    Text {\n"
    "        text: \"Hello World\"\n"
    "        fontSize: 18\n"
    "    }\n"
    "    \n"
    "    Button {\n"
    "        class: \"button\"\n"
    "        text: \"Click Me\"\n"
    "        onClick: \"handleClick\"\n"
    "    }\n"
    "}\n";

static void print_parser_errors(KryonParser *parser) {
    size_t error_count;
    const char **errors = kryon_parser_get_errors(parser, &error_count);
    
    if (error_count > 0) {
        printf("Parser Errors (%zu):\n", error_count);
        for (size_t i = 0; i < error_count; i++) {
            printf("  %s\n", errors[i]);
        }
    } else {
        printf("No parser errors.\n");
    }
}

static void print_parser_stats(KryonParser *parser) {
    double parse_time;
    size_t nodes_created;
    size_t max_depth;
    
    kryon_parser_get_stats(parser, &parse_time, &nodes_created, &max_depth);
    
    printf("Parser Statistics:\n");
    printf("  Parse time: %.3f ms\n", parse_time * 1000.0);
    printf("  Nodes created: %zu\n", nodes_created);
    printf("  Max depth: %zu\n", max_depth);
}

int main(void) {
    printf("Kryon Parser Test\n");
    printf("=================\n\n");
    
    // Initialize memory system
    if (!kryon_memory_init()) {
        fprintf(stderr, "Failed to initialize memory system\n");
        return 1;
    }
    
    printf("Test Source:\n");
    printf("------------\n");
    printf("%s\n", test_source);
    
    // Create lexer
    KryonLexer *lexer = kryon_lexer_create(test_source, "test.kry");
    if (!lexer) {
        fprintf(stderr, "Failed to create lexer\n");
        kryon_memory_cleanup();
        return 1;
    }
    
    // Tokenize
    printf("Tokenizing...\n");
    if (!kryon_lexer_tokenize(lexer)) {
        fprintf(stderr, "Tokenization failed\n");
        kryon_lexer_destroy(lexer);
        kryon_memory_cleanup();
        return 1;
    }
    
    size_t token_count;
    const KryonToken *tokens = kryon_lexer_get_tokens(lexer, &token_count);
    printf("Generated %zu tokens\n\n", token_count);
    
    // Create parser
    printf("Creating parser...\n");
    KryonParserConfig config = kryon_parser_default_config();
    KryonParser *parser = kryon_parser_create(lexer, &config);
    if (!parser) {
        fprintf(stderr, "Failed to create parser\n");
        kryon_lexer_destroy(lexer);
        kryon_memory_cleanup();
        return 1;
    }
    
    // Parse
    printf("Parsing...\n");
    bool success = kryon_parser_parse(parser);
    
    printf("Parse result: %s\n\n", success ? "SUCCESS" : "FAILED");
    
    // Print errors
    print_parser_errors(parser);
    printf("\n");
    
    // Print statistics
    print_parser_stats(parser);
    printf("\n");
    
    // Print AST if successful
    if (success) {
        const KryonASTNode *root = kryon_parser_get_root(parser);
        if (root) {
            printf("Generated AST:\n");
            printf("--------------\n");
            kryon_ast_print(root, stdout, 0);
            printf("\n");
            
            // Test AST traversal
            printf("AST Validation:\n");
            printf("---------------\n");
            char *errors[10];
            size_t validation_errors = kryon_ast_validate(root, errors, 10);
            
            if (validation_errors > 0) {
                printf("Validation errors (%zu):\n", validation_errors);
                for (size_t i = 0; i < validation_errors; i++) {
                    printf("  %s\n", errors[i]);
                    kryon_free(errors[i]);
                }
            } else {
                printf("AST validation passed.\n");
            }
            printf("\n");
            
            // Test search functionality
            printf("AST Search Test:\n");
            printf("----------------\n");
            const KryonASTNode *elements[10];
            size_t element_count = kryon_ast_find_by_type(root, KRYON_AST_ELEMENT, elements, 10);
            printf("Found %zu element nodes\n", element_count);
            
            const KryonASTNode *text_elements[5];
            size_t text_count = kryon_ast_find_elements(root, "Text", text_elements, 5);
            printf("Found %zu Text elements\n", text_count);
            
            const KryonASTNode *button_elements[5];
            size_t button_count = kryon_ast_find_elements(root, "Button", button_elements, 5);
            printf("Found %zu Button elements\n", button_count);
        }
    }
    
    // Cleanup
    kryon_parser_destroy(parser);
    kryon_lexer_destroy(lexer);
    kryon_memory_cleanup();
    
    printf("\nTest completed successfully!\n");
    return 0;
}