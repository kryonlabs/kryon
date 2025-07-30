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

int compile_command(int argc, char *argv[]) {
    const char *input_file = NULL;
    const char *output_file = NULL;
    bool verbose = false;
    bool optimize = false;
    
    // Parse arguments
    static struct option long_options[] = {
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"optimize", no_argument, 0, 'O'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "o:vOh", long_options, NULL)) != -1) {
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
            case 'h':
                printf("Usage: kryon compile <file.kry> [options]\n");
                printf("Options:\n");
                printf("  -o, --output <file>    Output file name\n");
                printf("  -v, --verbose          Verbose output\n");
                printf("  -O, --optimize         Enable optimizations\n");
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
    
    // Create lexer
    KryonLexer *lexer = kryon_lexer_create(source, input_file);
    if (!lexer) {
        fprintf(stderr, "Error: Failed to create lexer\n");
        kryon_free(source);
        return 1;
    }
    
    // Create parser
    KryonParser *parser = kryon_parser_create(lexer);
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
    
    KryonASTNode *ast = kryon_parser_parse(parser);
    if (!ast || kryon_parser_had_error(parser)) {
        fprintf(stderr, "Parse error: %s\n", kryon_parser_error_message(parser));
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_free(source);
        return 1;
    }
    
    if (verbose) {
        printf("Parsing completed successfully\n");
    }
    
    // Create code generator
    KryonCodeGenConfig config = optimize ? kryon_codegen_speed_config() : kryon_codegen_default_config();
    KryonCodeGenerator *codegen = kryon_codegen_create(&config);
    if (!codegen) {
        fprintf(stderr, "Error: Failed to create code generator\n");
        kryon_ast_destroy(ast);
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_free(source);
        return 1;
    }
    
    // Generate code
    if (verbose) {
        printf("Generating code...\n");
    }
    
    if (!kryon_codegen_generate(codegen, ast)) {
        size_t error_count;
        const char **errors = kryon_codegen_get_errors(codegen, &error_count);
        for (size_t i = 0; i < error_count; i++) {
            fprintf(stderr, "Error: %s\n", errors[i]);
        }
        kryon_codegen_destroy(codegen);
        kryon_ast_destroy(ast);
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_free(source);
        return 1;
    }
    
    // Write output file
    if (verbose) {
        printf("Writing output file...\n");
    }
    
    if (!kryon_codegen_write_file(codegen, output_file)) {
        fprintf(stderr, "Error: Failed to write output file: %s\n", output_file);
        kryon_codegen_destroy(codegen);
        kryon_ast_destroy(ast);
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
        printf("  Elements: %u\n", stats->output_elements);
    }
    
    printf("Compilation successful: %s\n", output_file);
    
    // Cleanup
    kryon_codegen_destroy(codegen);
    kryon_ast_destroy(ast);
    kryon_parser_destroy(parser);
    kryon_lexer_destroy(lexer);
    kryon_free(source);
    
    return 0;
}