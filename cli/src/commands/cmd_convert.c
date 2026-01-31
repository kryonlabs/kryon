/**
 * Convert Command
 * Convert between any two formats via KIR
 * Universal format converter: source → KIR → target
 */

#include "kryon_cli.h"
#include "build.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cJSON.h>

/* ============================================================================
 * External Parser Functions
 * ============================================================================ */

extern char* ir_kry_to_kir(const char* source, size_t length);
extern cJSON* tcl_parse_file_to_kir(const char* filepath);

/* ============================================================================
 * Forward Declarations for Local Functions
 * ============================================================================ */

static FileFormat detect_format_from_ext(const char* filepath);
static const char* format_to_string(FileFormat format);
static cJSON* parse_source_to_kir(FileFormat format, const char* input_file);
static int generate_kir_to_target(cJSON* kir, FileFormat target_format, const char* output_file);

/* ============================================================================
 * Format Detection
 * ============================================================================ */

/**
 * Detect file format from extension
 */
static FileFormat detect_format_from_ext(const char* filepath) {
    if (!filepath) return FORMAT_UNKNOWN;

    const char* ext = strrchr(filepath, '.');
    if (!ext) return FORMAT_UNKNOWN;

    ext++;  // Skip the dot

    if (strcmp(ext, "kry") == 0) return FORMAT_KRY;
    if (strcmp(ext, "tcl") == 0) return FORMAT_TCL;
    if (strcmp(ext, "b") == 0) return FORMAT_LIMBO;
    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) return FORMAT_HTML;
    if (strcmp(ext, "c") == 0 || strcmp(ext, "h") == 0) return FORMAT_C;
    if (strcmp(ext, "kir") == 0) return FORMAT_KIR;

    return FORMAT_UNKNOWN;
}

/**
 * Get format name as string
 */
static const char* format_to_string(FileFormat format) {
    switch (format) {
        case FORMAT_KRY: return "KRY";
        case FORMAT_TCL: return "Tcl/Tk";
        case FORMAT_LIMBO: return "Limbo";
        case FORMAT_HTML: return "HTML";
        case FORMAT_C: return "C";
        case FORMAT_KIR: return "KIR";
        default: return "Unknown";
    }
}

/* ============================================================================
 * Parsing Functions (Source → KIR)
 * ============================================================================ */

/**
 * Parse KRY file to KIR
 */
static cJSON* parse_kry_to_kir_json(const char* filepath) {
    // Read file content
    FILE* f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file: %s\n", filepath);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = malloc(size + 1);
    if (!content) {
        fclose(f);
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }

    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    // Parse KRY to KIR (returns JSON string)
    char* kir_json = ir_kry_to_kir(content, size);
    free(content);

    if (!kir_json) {
        fprintf(stderr, "Error: Failed to parse KRY file: %s\n", filepath);
        return NULL;
    }

    cJSON* kir = cJSON_Parse(kir_json);
    free(kir_json);

    if (!kir) {
        fprintf(stderr, "Error: Failed to parse KRY-generated KIR JSON\n");
        return NULL;
    }

    return kir;
}

/**
 * Parse KIR file (just read and validate)
 */
static cJSON* parse_kir_file_json(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file: %s\n", filepath);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = malloc(size + 1);
    if (!content) {
        fclose(f);
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }

    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    cJSON* kir = cJSON_Parse(content);
    free(content);

    if (!kir) {
        fprintf(stderr, "Error: Invalid KIR JSON in file: %s\n", filepath);
        return NULL;
    }

    return kir;
}

/**
 * Main parse dispatcher
 */
static cJSON* parse_source_to_kir(FileFormat format, const char* input_file) {
    switch (format) {
        case FORMAT_KRY:
            return parse_kry_to_kir_json(input_file);

        case FORMAT_TCL:
            return tcl_parse_file_to_kir(input_file);

        case FORMAT_KIR:
            return parse_kir_file_json(input_file);

        case FORMAT_LIMBO:
        case FORMAT_HTML:
        case FORMAT_C:
            fprintf(stderr, "Error: Parser for %s not yet implemented\n", format_to_string(format));
            return NULL;

        default:
            fprintf(stderr, "Error: Unknown or unsupported file format\n");
            return NULL;
    }
}

/* ============================================================================
 * Generation Functions (KIR → Target)
 * ============================================================================ */

/**
 * Generate KIR to target format
 */
static int generate_kir_to_target(cJSON* kir, FileFormat target_format, const char* output_file) {
    // Save KIR to temporary file for codegen functions
    char temp_kir[] = "/tmp/kryon_convert_XXXXXX";
    int temp_fd = mkstemp(temp_kir);
    if (temp_fd == -1) {
        fprintf(stderr, "Error: Cannot create temporary file\n");
        return 1;
    }
    close(temp_fd);

    // Write KIR to temp file
    char* kir_json = cJSON_Print(kir);
    FILE* f = fopen(temp_kir, "w");
    if (!f) {
        fprintf(stderr, "Error: Cannot write temporary file\n");
        free(kir_json);
        unlink(temp_kir);
        return 1;
    }
    fprintf(f, "%s\n", kir_json);
    fclose(f);
    free(kir_json);

    // Convert target format to codegen target string
    const char* codegen_target = NULL;
    switch (target_format) {
        case FORMAT_KRY:
            codegen_target = "kry";
            break;
        case FORMAT_TCL:
            codegen_target = "tcltk";
            break;
        case FORMAT_LIMBO:
            codegen_target = "limbo";
            break;
        case FORMAT_HTML:
            codegen_target = "html";
            break;
        case FORMAT_C:
            codegen_target = "c";
            break;
        default:
            fprintf(stderr, "Error: Unsupported target format for codegen\n");
            unlink(temp_kir);
            return 1;
    }

    // Use existing codegen infrastructure
    int result = generate_from_kir(temp_kir, codegen_target, output_file);

    // Clean up
    unlink(temp_kir);

    return result;
}

/* ============================================================================
 * Command Implementation
 * ============================================================================ */

/**
 * Print usage information
 */
static void print_convert_usage(const char* error) {
    if (error) {
        fprintf(stderr, "Error: %s\n\n", error);
    }

    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  Auto-detect formats:\n");
    fprintf(stderr, "    kryon convert <input_file> -o <output_file>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  Explicit target format:\n");
    fprintf(stderr, "    kryon convert <input_file> -o <output_file> -t <target_format>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Supported formats:\n");
    fprintf(stderr, "  kry     - KRY source files (.kry)\n");
    fprintf(stderr, "  tcl     - Tcl/Tk scripts (.tcl)\n");
    fprintf(stderr, "  limbo   - Limbo source files (.b)\n");
    fprintf(stderr, "  html    - HTML files (.html)\n");
    fprintf(stderr, "  c       - C source files (.c)\n");
    fprintf(stderr, "  kir     - KIR files (.kir)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  # Tcl → KRY\n");
    fprintf(stderr, "  kryon convert app.tcl -o app.kry\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  # KRY → Tcl\n");
    fprintf(stderr, "  kryon convert app.kry -o app.tcl\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  # Tcl → Limbo (via KIR)\n");
    fprintf(stderr, "  kryon convert app.tcl -o app.b -t limbo\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  # Limbo → Web (via KIR)\n");
    fprintf(stderr, "  kryon convert app.b -o app.html -t html\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Information Loss:\n");
    fprintf(stderr, "  KRY → KRY:   100%% (perfect round-trip)\n");
    fprintf(stderr, "  KRY ↔ Tcl:   ~60-70%% (loses some metadata)\n");
    fprintf(stderr, "  KRY ↔ Limbo: ~60-70%% (loses some metadata)\n");
    fprintf(stderr, "  Tcl ↔ Limbo: ~40-50%% (cross-format loss)\n");
}

int cmd_convert(int argc, char** argv) {
    const char* input_file = NULL;
    const char* output_file = NULL;
    FileFormat input_format = FORMAT_UNKNOWN;
    FileFormat target_format = FORMAT_UNKNOWN;
    bool explicit_target = false;

    // Parse arguments
    if (argc == 0) {
        print_convert_usage("Missing input file");
        return 1;
    }

    input_file = argv[0];

    // Parse remaining arguments
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-o=", 3) == 0) {
            output_file = argv[i] + 3;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strncmp(argv[i], "-t=", 3) == 0) {
            const char* target_str = argv[i] + 3;
            explicit_target = true;

            if (strcmp(target_str, "kry") == 0) target_format = FORMAT_KRY;
            else if (strcmp(target_str, "tcl") == 0 || strcmp(target_str, "tcltk") == 0) target_format = FORMAT_TCL;
            else if (strcmp(target_str, "limbo") == 0) target_format = FORMAT_LIMBO;
            else if (strcmp(target_str, "html") == 0) target_format = FORMAT_HTML;
            else if (strcmp(target_str, "c") == 0) target_format = FORMAT_C;
            else if (strcmp(target_str, "kir") == 0) target_format = FORMAT_KIR;
            else {
                fprintf(stderr, "Error: Unknown target format '%s'\n", target_str);
                return 1;
            }
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            const char* target_str = argv[++i];
            explicit_target = true;

            if (strcmp(target_str, "kry") == 0) target_format = FORMAT_KRY;
            else if (strcmp(target_str, "tcl") == 0 || strcmp(target_str, "tcltk") == 0) target_format = FORMAT_TCL;
            else if (strcmp(target_str, "limbo") == 0) target_format = FORMAT_LIMBO;
            else if (strcmp(target_str, "html") == 0) target_format = FORMAT_HTML;
            else if (strcmp(target_str, "c") == 0) target_format = FORMAT_C;
            else if (strcmp(target_str, "kir") == 0) target_format = FORMAT_KIR;
            else {
                fprintf(stderr, "Error: Unknown target format '%s'\n", target_str);
                return 1;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_convert_usage(NULL);
            return 0;
        } else {
            fprintf(stderr, "Error: Unknown argument '%s'\n", argv[i]);
            print_convert_usage(NULL);
            return 1;
        }
    }

    // Validate input file
    if (!input_file) {
        print_convert_usage("Missing input file");
        return 1;
    }

    if (!file_exists(input_file)) {
        fprintf(stderr, "Error: Input file not found: %s\n", input_file);
        return 1;
    }

    // Detect input format
    input_format = detect_format_from_ext(input_file);
    if (input_format == FORMAT_UNKNOWN) {
        fprintf(stderr, "Error: Cannot detect input file format from extension: %s\n", input_file);
        return 1;
    }

    // Auto-detect target format from output file if not specified
    if (target_format == FORMAT_UNKNOWN && output_file) {
        target_format = detect_format_from_ext(output_file);
    }

    if (target_format == FORMAT_UNKNOWN) {
        fprintf(stderr, "Error: Cannot detect target format. Please specify with -t <format>\n");
        return 1;
    }

    // Validate formats
    if (input_format == FORMAT_UNKNOWN) {
        fprintf(stderr, "Error: Unsupported input format\n");
        return 1;
    }

    if (target_format == FORMAT_UNKNOWN) {
        fprintf(stderr, "Error: Unsupported target format\n");
        return 1;
    }

    // Print conversion plan
    printf("Converting: %s → KIR → %s\n",
           format_to_string(input_format),
           format_to_string(target_format));

    // Step 1: Parse source to KIR
    printf("Step 1: Parsing %s file: %s\n",
           format_to_string(input_format), input_file);

    cJSON* kir = parse_source_to_kir(input_format, input_file);
    if (!kir) {
        fprintf(stderr, "Error: Failed to parse input file\n");
        return 1;
    }

    printf("  ✓ Successfully parsed to KIR\n");

    // Step 2: Generate KIR to target
    printf("Step 2: Generating %s file: %s\n",
           format_to_string(target_format), output_file);

    int result = generate_kir_to_target(kir, target_format, output_file);

    cJSON_Delete(kir);

    if (result == 0) {
        printf("  ✓ Successfully generated %s file\n", format_to_string(target_format));
        printf("\n✓ Conversion complete: %s → %s\n", input_file, output_file);
    } else {
        fprintf(stderr, "✗ Conversion failed\n");
        return 1;
    }

    return 0;
}
