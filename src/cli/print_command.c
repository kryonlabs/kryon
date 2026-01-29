/**

 * @file print_command.c
 * @brief KIR Printer CLI Command - Convert KIR to readable .kry source
 */
#include "lib9.h"


#include "kir_printer.h"
#include "error.h"
#include "memory.h"
#include <stdbool.h>
#include <getopt.h>

/**
 * @brief Print command - Convert KIR to .kry source code
 * @param argc Argument count
 * @param argv Argument vector
 * @return Result code
 */
KryonResult print_command(int argc, char *argv[]) {
    const char *input_file = NULL;
    const char *output_file = NULL;
    bool verbose = false;
    bool compact = false;
    bool readable = false;

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
        {"compact", no_argument, 0, 'c'},
        {"readable", no_argument, 0, 'r'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "o:vcrh", long_options, NULL)) != -1) {
        switch (c) {
            case 'o':
                output_file = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case 'c':
                compact = true;
                break;
            case 'r':
                readable = true;
                break;
            case 'h':
                print("Usage: kryon print <input.kir> [options]\n");
                print("Options:\n");
                print("  -o, --output <file>  Output .kry file path (default: input.kry)\n");
                print("  -c, --compact        Use compact formatting (minimal whitespace)\n");
                print("  -r, --readable       Use readable formatting (generous whitespace)\n");
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
        fprint(2, "Usage: kryon print <input.kir> [-o output.kry]\n");
        return KRYON_ERROR_INVALID_ARGUMENT;
    }

    input_file = argv[optind];

    // Generate output file name if not specified
    char *auto_output_file = NULL;
    if (!output_file) {
        // Replace .kir with .kry
        size_t len = strlen(input_file);
        if (len > 4 && strcmp(input_file + len - 4, ".kir") == 0) {
            auto_output_file = malloc(len + 1);
            if (!auto_output_file) {
                fprint(2, "Error: Memory allocation failed\n");
                return KRYON_ERROR_OUT_OF_MEMORY;
            }
            strcpy(auto_output_file, input_file);
            strcpy(auto_output_file + len - 4, ".kry");
            output_file = auto_output_file;
        } else {
            // Just append .kry
            auto_output_file = malloc(len + 5);
            if (!auto_output_file) {
                fprint(2, "Error: Memory allocation failed\n");
                return KRYON_ERROR_OUT_OF_MEMORY;
            }
            sprintf(auto_output_file, "%s.kry", input_file);
            output_file = auto_output_file;
        }
    }

    if (verbose) {
        KRYON_LOG_INFO("Printing: %s -> %s", input_file, output_file);
    }

    // --- Printing ---

    // Create printer with configuration
    KryonPrinterConfig config;
    if (compact) {
        config = kryon_printer_compact_config();
    } else if (readable) {
        config = kryon_printer_readable_config();
    } else {
        config = kryon_printer_default_config();
    }

    KryonKIRPrinter *printer = kryon_printer_create(&config);
    if (!printer) {
        fprint(2, "Error: Failed to create printer\n");
        result = KRYON_ERROR_COMPILATION_FAILED;
        goto cleanup;
    }

    // Print KIR to source file
    if (!kryon_printer_kir_to_source(printer, input_file, output_file)) {
        size_t error_count;
        const char **errors = kryon_printer_get_errors(printer, &error_count);
        fprint(2, "Error: Printing failed:\n");
        for (size_t i = 0; i < error_count; i++) {
            fprint(2, "  %s\n", errors[i]);
        }
        result = KRYON_ERROR_COMPILATION_FAILED;
        goto cleanup_printer;
    }

    if (verbose) {
        const KryonPrinterStats *stats = kryon_printer_get_stats(printer);
        KRYON_LOG_INFO("Printing statistics:");
        KRYON_LOG_INFO("  Elements: %zu", stats->elements_printed);
        KRYON_LOG_INFO("  Properties: %zu", stats->properties_printed);
        KRYON_LOG_INFO("  Lines: %zu", stats->lines_generated);
        KRYON_LOG_INFO("  Bytes: %zu", stats->total_bytes);
    }

    print("Source code generated: %s\n", output_file);

cleanup_printer:
    kryon_printer_destroy(printer);

cleanup:
    if (auto_output_file) {
        free(auto_output_file);
    }

    return result;
}
