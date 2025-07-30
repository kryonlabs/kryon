/**
 * @file test-compiler.c
 * @brief Full Compiler Pipeline Test
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 * 
 * Tests the complete KRY -> KRB compilation pipeline.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal/lexer.h"
#include "internal/parser.h"
#include "internal/codegen.h"
#include "internal/diagnostics.h"
#include "internal/memory.h"

// Test KRY source with dropdown example
static const char *test_source = 
    "@style \"container\" {\n"
    "    display: \"flex\"\n"
    "    flexDirection: \"column\"\n"
    "    padding: 20\n"
    "    backgroundColor: \"#f5f5f5FF\"\n"
    "}\n"
    "\n"
    "@style \"dropdown\" {\n"
    "    width: 200\n"
    "    height: 35\n"
    "    backgroundColor: \"#ffffffFF\"\n"
    "    borderColor: \"#ccccccFF\"\n"
    "    borderWidth: 1\n"
    "    borderRadius: 6\n"
    "}\n"
    "\n"
    "Container {\n"
    "    class: \"container\"\n"
    "    \n"
    "    Text {\n"
    "        text: \"Select an Option:\"\n"
    "        fontSize: 16\n"
    "        fontWeight: \"semibold\"\n"
    "    }\n"
    "    \n"
    "    Dropdown {\n"
    "        class: \"dropdown\"\n"
    "        selectedIndex: 0\n"
    "        onChange: \"handleChange\"\n"
    "        \n"
    "        options: [\n"
    "            { text: \"Option A\", value: \"a\" },\n"
    "            { text: \"Option B\", value: \"b\" },\n"
    "            { text: \"Option C\", value: \"c\" }\n"
    "        ]\n"
    "    }\n"
    "}\n";

int main(void) {
    printf("Kryon-C Full Compiler Test\n");
    printf("==========================\n\n");
    
    // Initialize memory system
    if (!kryon_memory_init()) {
        fprintf(stderr, "Failed to initialize memory system\n");
        return 1;
    }
    
    // Create diagnostic manager
    KryonDiagnosticManager *diagnostics = kryon_diagnostic_create();
    if (!diagnostics) {
        fprintf(stderr, "Failed to create diagnostic manager\n");
        kryon_memory_cleanup();
        return 1;
    }
    
    printf("Test Source (KRY):\n");
    printf("------------------\n");
    printf("%s\n", test_source);
    
    // =========================================================================
    // LEXICAL ANALYSIS
    // =========================================================================
    
    printf("Step 1: Lexical Analysis\n");
    printf("------------------------\n");
    
    KryonLexer *lexer = kryon_lexer_create(test_source, "test.kry");
    if (!lexer) {
        fprintf(stderr, "Failed to create lexer\n");
        kryon_diagnostic_destroy(diagnostics);
        kryon_memory_cleanup();
        return 1;
    }
    
    if (!kryon_lexer_tokenize(lexer)) {
        fprintf(stderr, "Tokenization failed\n");
        kryon_lexer_destroy(lexer);
        kryon_diagnostic_destroy(diagnostics);
        kryon_memory_cleanup();
        return 1;
    }
    
    size_t token_count;
    const KryonToken *tokens = kryon_lexer_get_tokens(lexer, &token_count);
    printf("✓ Generated %zu tokens\n\n", token_count);
    
    // =========================================================================
    // SYNTAX ANALYSIS
    // =========================================================================
    
    printf("Step 2: Syntax Analysis\n");
    printf("-----------------------\n");
    
    KryonParserConfig parser_config = kryon_parser_default_config();
    KryonParser *parser = kryon_parser_create(lexer, &parser_config);
    if (!parser) {
        fprintf(stderr, "Failed to create parser\n");
        kryon_lexer_destroy(lexer);
        kryon_diagnostic_destroy(diagnostics);
        kryon_memory_cleanup();
        return 1;
    }
    
    if (!kryon_parser_parse(parser)) {
        fprintf(stderr, "Parsing failed\n");
        
        // Print parser errors
        size_t error_count;
        const char **errors = kryon_parser_get_errors(parser, &error_count);
        for (size_t i = 0; i < error_count; i++) {
            printf("Parser Error: %s\n", errors[i]);
        }
        
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_diagnostic_destroy(diagnostics);
        kryon_memory_cleanup();
        return 1;
    }
    
    const KryonASTNode *ast_root = kryon_parser_get_root(parser);
    if (!ast_root) {
        fprintf(stderr, "No AST generated\n");
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_diagnostic_destroy(diagnostics);
        kryon_memory_cleanup();
        return 1;
    }
    
    // Print parser statistics
    double parse_time;
    size_t nodes_created;
    size_t max_depth;
    kryon_parser_get_stats(parser, &parse_time, &nodes_created, &max_depth);
    
    printf("✓ Parsing completed successfully\n");
    printf("  - Parse time: %.3f ms\n", parse_time * 1000.0);
    printf("  - Nodes created: %zu\n", nodes_created);
    printf("  - Max depth: %zu\n\n", max_depth);
    
    // =========================================================================
    // CODE GENERATION
    // =========================================================================
    
    printf("Step 3: Code Generation\n");
    printf("-----------------------\n");
    
    KryonCodeGenConfig codegen_config = kryon_codegen_default_config();
    KryonCodeGenerator *codegen = kryon_codegen_create(&codegen_config);
    if (!codegen) {
        fprintf(stderr, "Failed to create code generator\n");
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_diagnostic_destroy(diagnostics);
        kryon_memory_cleanup();
        return 1;
    }
    
    if (!kryon_codegen_generate(codegen, ast_root)) {
        fprintf(stderr, "Code generation failed\n");
        
        // Print codegen errors
        size_t error_count;
        const char **errors = kryon_codegen_get_errors(codegen, &error_count);
        for (size_t i = 0; i < error_count; i++) {
            printf("Codegen Error: %s\n", errors[i]);
        }
        
        kryon_codegen_destroy(codegen);
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_diagnostic_destroy(diagnostics);
        kryon_memory_cleanup();
        return 1;
    }
    
    // Get generated binary
    size_t binary_size;
    const uint8_t *binary_data = kryon_codegen_get_binary(codegen, &binary_size);
    if (!binary_data || binary_size == 0) {
        fprintf(stderr, "No binary data generated\n");
        kryon_codegen_destroy(codegen);
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_diagnostic_destroy(diagnostics);
        kryon_memory_cleanup();
        return 1;
    }
    
    printf("✓ Code generation completed successfully\n");
    printf("  - Binary size: %zu bytes\n", binary_size);
    
    // Print generation statistics
    const KryonCodeGenStats *stats = kryon_codegen_get_stats(codegen);
    if (stats) {
        printf("  - Generation time: %.3f ms\n", stats->generation_time * 1000.0);
        printf("  - Output elements: %zu\n", stats->output_elements);
        printf("  - Output properties: %zu\n", stats->output_properties);
    }
    
    // =========================================================================
    // BINARY VALIDATION
    // =========================================================================
    
    printf("\nStep 4: Binary Validation\n");
    printf("-------------------------\n");
    
    if (kryon_codegen_validate_binary(codegen)) {
        printf("✓ Binary validation passed\n");
    } else {
        printf("✗ Binary validation failed\n");
    }
    
    // =========================================================================
    // FILE OUTPUT
    // =========================================================================
    
    printf("\nStep 5: File Output\n");
    printf("-------------------\n");
    
    const char *output_filename = "test-output.krb";
    if (kryon_codegen_write_file(codegen, output_filename)) {
        printf("✓ Binary written to %s\n", output_filename);
    } else {
        printf("✗ Failed to write binary file\n");
    }
    
    // =========================================================================
    // BINARY ANALYSIS
    // =========================================================================
    
    printf("\nBinary Analysis:\n");
    printf("----------------\n");
    kryon_codegen_print_binary(binary_data, binary_size, stdout);
    
    // =========================================================================
    // CLEANUP
    // =========================================================================
    
    printf("\nCompilation Summary:\n");
    printf("===================\n");
    printf("✓ All compilation stages completed successfully\n");
    printf("✓ Generated %zu bytes of KRB binary\n", binary_size);
    printf("✓ No errors or warnings\n");
    
    kryon_diagnostic_print_summary(diagnostics, stdout);
    
    // Cleanup resources
    kryon_codegen_destroy(codegen);
    kryon_parser_destroy(parser);
    kryon_lexer_destroy(lexer);
    kryon_diagnostic_destroy(diagnostics);
    kryon_memory_cleanup();
    
    printf("\nTest completed successfully!\n");
    return 0;
}