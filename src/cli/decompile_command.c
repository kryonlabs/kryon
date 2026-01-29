/**

 * @file decompile_command.c
 * @brief KRB Decompiler CLI Command
 */
#include "lib9.h"


#include "krb_decompiler.h"
#include "kir_format.h"
#include "error.h"
#include "memory.h"
#include <stdbool.h>
#include <getopt.h>

/**
 * @brief Decompile command - Convert KRB to KIR
 * @param argc Argument count
 * @param argv Argument vector
 * @return Result code
 */
KryonResult decompile_command(int argc, char *argv[]) {
    const char *input_file = NULL;
    const char *output_file = NULL;
    bool verbose = false;

    KryonResult result = KRYON_SUCCESS;

    // Initialize error system
    if (kryon_error_init() != KRYON_SUCCESS) {
        fprint(2, "Fatal: Could not initialize error system.\n");
        return KRYON_ERROR_PLATFORM_ERROR;
    }

    // --- Argument Parsing ---
    static struct option long_options[] = {
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "o:vh", long_options, NULL)) != -1) {
        switch (c) {
            case 'o':
                output_file = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                print("Usage: kryon decompile <input.krb> [options]\n");
                print("Options:\n");
                print("  -o, --output <file>  Output .kir file path (default: input.kir)\n");
                print("  -v, --verbose        Enable verbose output\n");
                print("  -h, --help           Show this help message\n");
                return KRYON_SUCCESS;
            default:
                return KRYON_ERROR_INVALID_ARGUMENT;
        }
    }

    // Get input file
    if (optind >= argc) {
        fprint(2, "Error: No input file specified\n");
        fprint(2, "Usage: kryon decompile <input.krb> [-o output.kir]\n");
        return KRYON_ERROR_INVALID_ARGUMENT;
    }

    input_file = argv[optind];

    // Generate output file name if not specified
    char *auto_output_file = NULL;
    if (!output_file) {
        // Replace .krb with .kir
        size_t len = strlen(input_file);
        if (len > 4 && strcmp(input_file + len - 4, ".krb") == 0) {
            auto_output_file = malloc(len + 1);
            if (!auto_output_file) {
                fprint(2, "Error: Memory allocation failed\n");
                return KRYON_ERROR_OUT_OF_MEMORY;
            }
            strcpy(auto_output_file, input_file);
            strcpy(auto_output_file + len - 4, ".kir");
            output_file = auto_output_file;
        } else {
            // Just append .kir
            auto_output_file = malloc(len + 5);
            if (!auto_output_file) {
                fprint(2, "Error: Memory allocation failed\n");
                return KRYON_ERROR_OUT_OF_MEMORY;
            }
            sprintf(auto_output_file, "%s.kir", input_file);
            output_file = auto_output_file;
        }
    }

    if (verbose) {
        KRYON_LOG_INFO("Decompiling: %s -> %s", input_file, output_file);
    }

    // --- Decompilation ---

    // Create decompiler
    KryonDecompilerConfig config = kryon_decompiler_default_config();
    config.source_hint = input_file;

    KryonKrbDecompiler *decompiler = kryon_decompiler_create(&config);
    if (!decompiler) {
        fprint(2, "Error: Failed to create decompiler\n");
        result = KRYON_ERROR_COMPILATION_FAILED;
        goto cleanup;
    }

    // Decompile KRB to AST
    KryonASTNode *ast = NULL;
    if (!kryon_decompile_file(decompiler, input_file, &ast)) {
        size_t error_count;
        const char **errors = kryon_decompiler_get_errors(decompiler, &error_count);
        fprint(2, "Error: Decompilation failed:\n");
        for (size_t i = 0; i < error_count; i++) {
            fprint(2, "  %s\n", errors[i]);
        }
        result = KRYON_ERROR_COMPILATION_FAILED;
        goto cleanup_decompiler;
    }

    if (verbose) {
        const KryonDecompilerStats *stats = kryon_decompiler_get_stats(decompiler);
        KRYON_LOG_INFO("Decompilation statistics:");
        KRYON_LOG_INFO("  Elements: %zu", stats->elements_decompiled);
        KRYON_LOG_INFO("  Properties: %zu", stats->properties_decompiled);
        KRYON_LOG_INFO("  Strings recovered: %zu", stats->strings_recovered);
        KRYON_LOG_INFO("  Total AST nodes: %zu", stats->total_ast_nodes);
    }

    // --- Write KIR ---

    // Create KIR writer
    KryonKIRConfig kir_config = kryon_kir_default_config();
    kir_config.source_file = input_file;
    kir_config.format_style = KRYON_KIR_FORMAT_READABLE;

    KryonKIRWriter *kir_writer = kryon_kir_writer_create(&kir_config);
    if (!kir_writer) {
        fprint(2, "Error: Failed to create KIR writer\n");
        result = KRYON_ERROR_COMPILATION_FAILED;
        goto cleanup_ast;
    }

    // Write KIR file
    if (!kryon_kir_write_file(kir_writer, ast, output_file)) {
        size_t kir_error_count;
        const char **kir_errors = kryon_kir_writer_get_errors(kir_writer, &kir_error_count);
        fprint(2, "Error: Failed to write KIR file:\n");
        for (size_t i = 0; i < kir_error_count; i++) {
            fprint(2, "  %s\n", kir_errors[i]);
        }
        result = KRYON_ERROR_FILE_WRITE_ERROR;
        kryon_kir_writer_destroy(kir_writer);
        goto cleanup_ast;
    }

    kryon_kir_writer_destroy(kir_writer);

    print("Decompilation successful: %s\n", output_file);

cleanup_ast:
    // Note: AST cleanup would go here if we had a destroy function
    // For now, we rely on process cleanup

cleanup_decompiler:
    kryon_decompiler_destroy(decompiler);

cleanup:
    if (auto_output_file) {
        free(auto_output_file);
    }

    return result;
}
