#include "ir_format_detect.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

// Magic numbers for format detection
#define IR_MAGIC_KIRB   0x4B495242      // "KIRB" - new binary format
#define IR_MAGIC_LEGACY 0x4B5259        // "KRY" - legacy binary format

/**
 * Get file extension (including the dot)
 */
static const char* get_extension(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot;
}

/**
 * Check if string ends with suffix (case-insensitive)
 */
__attribute__((unused)) static bool ends_with_ci(const char* str, const char* suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len) return false;

    const char* str_end = str + str_len - suffix_len;
    for (size_t i = 0; i < suffix_len; i++) {
        if (tolower(str_end[i]) != tolower(suffix[i])) {
            return false;
        }
    }
    return true;
}

/**
 * Detect format from file extension
 */
IRFormat ir_detect_format_by_extension(const char* filename) {
    if (!filename) return IR_FORMAT_UNKNOWN;

    const char* ext = get_extension(filename);

    // .kirb = binary format
    if (strcmp(ext, ".kirb") == 0) {
        return IR_FORMAT_BINARY;
    }

    // .kir = JSON format (new convention)
    if (strcmp(ext, ".kir") == 0) {
        return IR_FORMAT_JSON;
    }

    // .json = also JSON format
    if (strcmp(ext, ".json") == 0) {
        return IR_FORMAT_JSON;
    }

    return IR_FORMAT_UNKNOWN;
}

/**
 * Detect format from file content
 */
IRFormat ir_detect_format_by_content(const char* filename) {
    if (!filename) return IR_FORMAT_UNKNOWN;

    FILE* file = fopen(filename, "rb");
    if (!file) return IR_FORMAT_UNKNOWN;

    // Read first 4 bytes to check magic number
    uint32_t magic;
    size_t bytes_read = fread(&magic, 1, sizeof(uint32_t), file);

    if (bytes_read == sizeof(uint32_t)) {
        // Check for binary magic numbers
        if (magic == IR_MAGIC_KIRB) {
            fclose(file);
            return IR_FORMAT_BINARY;
        }
        if (magic == IR_MAGIC_LEGACY) {
            fclose(file);
            return IR_FORMAT_LEGACY;
        }
    }

    // Rewind and check for JSON signature
    rewind(file);
    char first_char;
    if (fread(&first_char, 1, 1, file) == 1) {
        // Skip whitespace
        while (isspace(first_char)) {
            if (fread(&first_char, 1, 1, file) != 1) {
                fclose(file);
                return IR_FORMAT_UNKNOWN;
            }
        }

        // JSON files start with '{' or '['
        if (first_char == '{' || first_char == '[') {
            fclose(file);
            return IR_FORMAT_JSON;
        }
    }

    fclose(file);
    return IR_FORMAT_UNKNOWN;
}

/**
 * Detect IR format (tries multiple strategies)
 */
IRFormat ir_detect_format(const char* filename) {
    if (!filename) return IR_FORMAT_UNKNOWN;

    // Strategy 1: Check file extension first (fast)
    IRFormat format = ir_detect_format_by_extension(filename);
    if (format != IR_FORMAT_UNKNOWN) {
        return format;
    }

    // Strategy 2: Check file content (magic number or JSON signature)
    format = ir_detect_format_by_content(filename);
    if (format != IR_FORMAT_UNKNOWN) {
        return format;
    }

    // Unknown format
    return IR_FORMAT_UNKNOWN;
}

/**
 * Check if file is binary format
 */
bool ir_is_binary_format(const char* filename) {
    IRFormat format = ir_detect_format(filename);
    return (format == IR_FORMAT_BINARY || format == IR_FORMAT_LEGACY);
}

/**
 * Check if file is JSON format
 */
bool ir_is_json_format(const char* filename) {
    IRFormat format = ir_detect_format(filename);
    return (format == IR_FORMAT_JSON);
}

/**
 * Get format name as string
 */
const char* ir_format_name(IRFormat format) {
    switch (format) {
        case IR_FORMAT_BINARY:
            return "KIRB (Binary)";
        case IR_FORMAT_JSON:
            return "KIR (JSON)";
        case IR_FORMAT_LEGACY:
            return "Legacy KIR (Binary)";
        case IR_FORMAT_UNKNOWN:
        default:
            return "Unknown";
    }
}
