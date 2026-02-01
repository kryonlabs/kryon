/**
 * Parse Command
 * Parse source files and convert to KIR format
 * Supports: KRY, Tcl/Tk, Limbo, HTML, Markdown, C
 *
 * Uses the unified parser interface registry for all parsers.
 * Pipeline: Source → Parser Registry → KIR
 */

#include "kryon_cli.h"
#include "build.h"
#include "../../../ir/include/parser_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <cJSON.h>

/* ============================================================================
 * Parser Interface
 * ============================================================================ */

// External function to register all parsers
extern int parser_registry_register_all(void);

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

// Wrapper parser functions (defined later in this file)
static cJSON* parse_kir_file(const char* filepath);

/* ============================================================================
 * Format Detection
 * ============================================================================ */

/**
 * Detect file format from extension
 */
static FileFormat detect_format(const char* filepath) {
    if (!filepath) return FORMAT_UNKNOWN;

    const char* ext = strrchr(filepath, '.');
    if (!ext) return FORMAT_UNKNOWN;

    ext++;  // Skip the dot

    if (strcmp(ext, "kry") == 0) return FORMAT_KRY;
    if (strcmp(ext, "tcl") == 0) return FORMAT_TCL;
    if (strcmp(ext, "b") == 0) return FORMAT_LIMBO;
    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) return FORMAT_HTML;
    if (strcmp(ext, "md") == 0 || strcmp(ext, "markdown") == 0) return FORMAT_MARKDOWN;
    if (strcmp(ext, "c") == 0 || strcmp(ext, "h") == 0) return FORMAT_C;
    if (strcmp(ext, "kir") == 0) return FORMAT_KIR;

    return FORMAT_UNKNOWN;
}

/**
 * Get format name as string
 */
static const char* format_name(FileFormat format) {
    switch (format) {
        case FORMAT_KRY: return "KRY";
        case FORMAT_TCL: return "Tcl/Tk";
        case FORMAT_LIMBO: return "Limbo";
        case FORMAT_HTML: return "HTML";
        case FORMAT_MARKDOWN: return "Markdown";
        case FORMAT_C: return "C";
        case FORMAT_KIR: return "KIR";
        default: return "Unknown";
    }
}

/* ============================================================================
 * Parser Registry (Legacy - for reference only)
 * ============================================================================ */

/**
 * Check if a parser is implemented for a given format
 * Now delegates to the unified parser interface
 */
static bool is_parser_implemented(FileFormat format) {
    // Map FileFormat to format name
    const char* format_name = NULL;
    switch (format) {
        case FORMAT_KRY: format_name = "kry"; break;
        case FORMAT_TCL: format_name = "tcl"; break;
        case FORMAT_LIMBO: format_name = "limbo"; break;
        case FORMAT_HTML: format_name = "html"; break;
        case FORMAT_MARKDOWN: format_name = "markdown"; break;
        case FORMAT_C: format_name = "c"; break;
        case FORMAT_KIR: format_name = "kir"; break;
        default: return false;
    }

    return parser_get_interface(format_name) != NULL;
}

/* ============================================================================
 * Parsing Functions
 * ============================================================================ */

/**
 * Parse KIR file (just read and validate)
 */
static cJSON* parse_kir_file(const char* filepath) {
    // Read file
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

    // Parse JSON
    cJSON* kir = cJSON_Parse(content);
    free(content);

    if (!kir) {
        fprintf(stderr, "Error: Invalid KIR JSON in file: %s\n", filepath);
        return NULL;
    }

    return kir;
}

/**
 * Main parse dispatcher - now uses unified parser interface
 */
static cJSON* parse_file(FileFormat format, const char* filepath) {
    // Special case: KIR files just need validation
    if (format == FORMAT_KIR) {
        return parse_kir_file(filepath);
    }

    // Map FileFormat to parser format name
    const char* format_name = NULL;
    switch (format) {
        case FORMAT_KRY: format_name = "kry"; break;
        case FORMAT_TCL: format_name = "tcl"; break;
        case FORMAT_LIMBO: format_name = "limbo"; break;
        case FORMAT_HTML: format_name = "html"; break;
        case FORMAT_MARKDOWN: format_name = "markdown"; break;
        case FORMAT_C: format_name = "c"; break;
        default:
            fprintf(stderr, "Error: Unknown or unsupported file format\n");
            return NULL;
    }

    // Use unified parser interface
    cJSON* kir = parser_parse_file_as(filepath, format_name);
    if (!kir) {
        // Error message already printed by parser
        return NULL;
    }

    return kir;
}

/* ============================================================================
 * Output Functions
 * ============================================================================ */

/**
 * Write KIR JSON to file
 */
static bool write_kir_file(cJSON* kir, const char* output_path) {
    if (!kir || !output_path) {
        fprintf(stderr, "Error: Invalid arguments to write_kir_file\n");
        return false;
    }

    // Generate formatted JSON
    char* json_string = cJSON_Print(kir);
    if (!json_string) {
        fprintf(stderr, "Error: Failed to serialize KIR to JSON\n");
        return false;
    }

    // Write to file
    FILE* f = fopen(output_path, "w");
    if (!f) {
        fprintf(stderr, "Error: Cannot open output file: %s\n", output_path);
        free(json_string);
        return false;
    }

    fprintf(f, "%s\n", json_string);
    fclose(f);
    free(json_string);

    return true;
}

/* ============================================================================
 * Command Implementation
 * ============================================================================ */

/**
 * Print usage information
 */
static void print_parse_usage(const char* error) {
    if (error) {
        fprintf(stderr, "Error: %s\n\n", error);
    }

    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  Auto-detect format:\n");
    fprintf(stderr, "    kryon parse <input_file> -o <output.kir>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  Explicit format:\n");
    fprintf(stderr, "    kryon parse <format> <input_file> -o <output.kir>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Supported formats:\n");
    fprintf(stderr, "  kry       - KRY source files (.kry)\n");
    fprintf(stderr, "  tcl       - Tcl/Tk scripts (.tcl)\n");
    fprintf(stderr, "  limbo     - Limbo source files (.b)\n");
    fprintf(stderr, "  html      - HTML files (.html)\n");
    fprintf(stderr, "  markdown  - Markdown files (.md)\n");
    fprintf(stderr, "  c         - C source files (.c, .h)\n");
    fprintf(stderr, "  kir       - KIR files (validation only)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  kryon parse app.tcl -o app.kir\n");
    fprintf(stderr, "  kryon parse tcl app.tcl -o app.kir\n");
    fprintf(stderr, "  kryon parse app.kry -o app.kir\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Round-trip conversion:\n");
    fprintf(stderr, "  kryon parse app.tcl -o temp.kir\n");
    fprintf(stderr, "  kryon codegen kry temp.kir -o app_roundtrip.kry\n");
}

int cmd_parse(int argc, char** argv) {
    const char* input_file = NULL;
    const char* output_file = NULL;
    FileFormat format = FORMAT_UNKNOWN;
    bool explicit_format = false;

    // Parse arguments
    if (argc == 0) {
        print_parse_usage("Missing input file");
        return 1;
    }

    // Check if first argument is a format name or a file
    const char* first = argv[0];

    // Try to interpret first arg as a format
    if (strcmp(first, "kry") == 0) {
        format = FORMAT_KRY;
        explicit_format = true;
    } else if (strcmp(first, "tcl") == 0 || strcmp(first, "tcltk") == 0) {
        format = FORMAT_TCL;
        explicit_format = true;
    } else if (strcmp(first, "limbo") == 0) {
        format = FORMAT_LIMBO;
        explicit_format = true;
    } else if (strcmp(first, "html") == 0) {
        format = FORMAT_HTML;
        explicit_format = true;
    } else if (strcmp(first, "markdown") == 0 || strcmp(first, "md") == 0) {
        format = FORMAT_MARKDOWN;
        explicit_format = true;
    } else if (strcmp(first, "c") == 0) {
        format = FORMAT_C;
        explicit_format = true;
    } else if (strcmp(first, "kir") == 0) {
        format = FORMAT_KIR;
        explicit_format = true;
    } else {
        // First arg is a file, auto-detect format
        input_file = first;
        format = detect_format(input_file);
    }

    // Parse remaining arguments
    // Start loop at 1 if first arg was a file (already consumed above)
    // Start loop at 1 if first arg was a format (explicit format case)
    int start_idx = (input_file || explicit_format) ? 1 : 0;
    for (int i = start_idx; i < argc; i++) {
        if (strncmp(argv[i], "-o=", 3) == 0) {
            output_file = argv[i] + 3;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_parse_usage(NULL);
            return 0;
        } else if (!input_file) {
            input_file = argv[i];
        } else {
            fprintf(stderr, "Error: Unknown argument '%s'\n", argv[i]);
            print_parse_usage(NULL);
            return 1;
        }
    }

    // Validate input file
    if (!input_file) {
        print_parse_usage("Missing input file");
        return 1;
    }

    if (format == FORMAT_UNKNOWN && !explicit_format) {
        format = detect_format(input_file);
    }

    if (format == FORMAT_UNKNOWN) {
        fprintf(stderr, "Error: Cannot detect file format from extension: %s\n", input_file);
        fprintf(stderr, "Please specify format explicitly: kryon parse <format> %s\n", input_file);
        return 1;
    }

    // Check if file exists
    if (!file_exists(input_file)) {
        fprintf(stderr, "Error: Input file not found: %s\n", input_file);
        return 1;
    }

    // Generate output filename if not specified
    char output_buffer[1024];
    if (!output_file) {
        const char* basename = strrchr(input_file, '/');
        basename = basename ? basename + 1 : input_file;

        char* name_copy = str_copy(basename);
        char* dot = strrchr(name_copy, '.');
        if (dot) *dot = '\0';

        // Always use .kir extension (TKIR is internal-only)
        snprintf(output_buffer, sizeof(output_buffer), "%s.kir", name_copy);
        free(name_copy);

        output_file = output_buffer;
    }

    // Check if parser is implemented
    if (!is_parser_implemented(format)) {
        fprintf(stderr, "Error: Parser for %s is not yet implemented\n", format_name(format));
        fprintf(stderr, "Supported formats: KRY, Tcl/Tk, Limbo, HTML, Markdown, C, KIR\n");
        return 1;
    }

    // Parse the file
    printf("Parsing %s file: %s\n", format_name(format), input_file);

    cJSON* kir = parse_file(format, input_file);
    if (!kir) {
        fprintf(stderr, "Error: Failed to parse file\n");
        return 1;
    }

    // Write KIR output (TKIR conversion is internal-only in build pipeline)
    printf("Writing KIR to: %s\n", output_file);

    if (!write_kir_file(kir, output_file)) {
        cJSON_Delete(kir);
        return 1;
    }

    printf("✓ Successfully parsed KIR file\n");

    // Print statistics
    cJSON* components = cJSON_GetObjectItem(kir, "components");
    if (components && cJSON_IsArray(components)) {
        int count = cJSON_GetArraySize(components);
        printf("  Components: %d\n", count);
    }

    cJSON* handlers = cJSON_GetObjectItem(kir, "logic_block");
    if (handlers && cJSON_IsObject(handlers)) {
        cJSON* events = cJSON_GetObjectItem(handlers, "events");
        if (events && cJSON_IsArray(events)) {
            int count = cJSON_GetArraySize(events);
            printf("  Event handlers: %d\n", count);
        }
    }

    // Cleanup
    cJSON_Delete(kir);

    return 0;
}
