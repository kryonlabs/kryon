/**
 * Parse Command
 * Parse source files and convert to KIR format
 * Supports: KRY, Tcl/Tk, Limbo, HTML, C
 */

#include "kryon_cli.h"
#include "build.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <cJSON.h>

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

// Parser functions (from different frontends)
extern char* ir_kry_to_kir(const char* source, size_t length);  // KRY parser
extern cJSON* tcl_parse_file_to_kir(const char* filepath);  // Tcl parser (cJSON version)

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
        case FORMAT_C: return "C";
        case FORMAT_KIR: return "KIR";
        default: return "Unknown";
    }
}

/* ============================================================================
 * Parser Registry
 * ============================================================================ */

/**
 * Parse function signature
 */
typedef cJSON* (*ParseFunc)(const char* filepath);

/**
 * Parser registry entry
 */
typedef struct {
    FileFormat format;
    const char* name;
    ParseFunc parse_func;
    bool implemented;
} ParserEntry;

/**
 * Parser registry
 * Maps file formats to their parser functions
 */
static ParserEntry parser_registry[] = {
    {FORMAT_KRY, "kry", NULL, true},       // Uses ir_kry_to_kir_file (returns string)
    {FORMAT_TCL, "tcl", (ParseFunc)tcl_parse_file_to_kir, true},
    {FORMAT_LIMBO, "limbo", NULL, false},  // TODO: Not yet implemented
    {FORMAT_HTML, "html", NULL, false},    // TODO: Not yet implemented
    {FORMAT_C, "c", NULL, false},          // TODO: Not yet implemented
    {FORMAT_KIR, "kir", NULL, true},       // Already KIR, just copy
};

static int parser_registry_count = sizeof(parser_registry) / sizeof(ParserEntry);

/**
 * Find parser entry for format
 */
static ParserEntry* find_parser(FileFormat format) {
    for (int i = 0; i < parser_registry_count; i++) {
        if (parser_registry[i].format == format) {
            return &parser_registry[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Parsing Functions
 * ============================================================================ */

/**
 * Parse KRY file to KIR
 */
static cJSON* parse_kry_to_kir(const char* filepath) {
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

    // Parse the JSON string into cJSON
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
 * Main parse dispatcher
 */
static cJSON* parse_file(FileFormat format, const char* filepath) {
    switch (format) {
        case FORMAT_KRY:
            return parse_kry_to_kir(filepath);

        case FORMAT_TCL:
            return tcl_parse_file_to_kir(filepath);

        case FORMAT_KIR:
            return parse_kir_file(filepath);

        case FORMAT_LIMBO:
        case FORMAT_HTML:
        case FORMAT_C:
            fprintf(stderr, "Error: Parser for %s not yet implemented\n", format_name(format));
            return NULL;

        default:
            fprintf(stderr, "Error: Unknown or unsupported file format\n");
            return NULL;
    }
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
    fprintf(stderr, "  kry     - KRY source files (.kry)\n");
    fprintf(stderr, "  tcl     - Tcl/Tk scripts (.tcl)\n");
    fprintf(stderr, "  limbo   - Limbo source files (.b) [TODO]\n");
    fprintf(stderr, "  html    - HTML files (.html) [TODO]\n");
    fprintf(stderr, "  c       - C source files (.c, .h) [TODO]\n");
    fprintf(stderr, "  kir     - KIR files (validation only)\n");
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
    for (int i = (explicit_format ? 1 : 0); i < argc; i++) {
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

        snprintf(output_buffer, sizeof(output_buffer), "%s.kir", name_copy);
        free(name_copy);

        output_file = output_buffer;
    }

    // Check if parser is implemented
    ParserEntry* parser = find_parser(format);
    if (!parser || !parser->implemented) {
        fprintf(stderr, "Error: Parser for %s is not yet implemented\n", format_name(format));
        fprintf(stderr, "Supported formats: KRY, Tcl/Tk, KIR\n");
        fprintf(stderr, "TODO: Limbo, HTML, C\n");
        return 1;
    }

    // Parse the file
    printf("Parsing %s file: %s\n", format_name(format), input_file);

    cJSON* kir = parse_file(format, input_file);
    if (!kir) {
        fprintf(stderr, "Error: Failed to parse file\n");
        return 1;
    }

    // Write output
    printf("Writing KIR to: %s\n", output_file);

    if (!write_kir_file(kir, output_file)) {
        cJSON_Delete(kir);
        return 1;
    }

    printf("✓ Successfully parsed and wrote KIR file\n");

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

    cJSON_Delete(kir);

    return 0;
}
