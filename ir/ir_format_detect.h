#ifndef IR_FORMAT_DETECT_H
#define IR_FORMAT_DETECT_H

#include <stdbool.h>
#include <stddef.h>

// IR Format Types
typedef enum {
    IR_FORMAT_UNKNOWN = 0,
    IR_FORMAT_BINARY,   // .kirb format (new binary format)
    IR_FORMAT_JSON,     // .kir format (human-readable JSON)
    IR_FORMAT_LEGACY    // Legacy .kir binary format (old magic number)
} IRFormat;

// Format Detection Functions

/**
 * Detect IR format from filename (extension, magic number, or content)
 * @param filename Path to IR file
 * @return IRFormat type (BINARY, JSON, LEGACY, or UNKNOWN)
 */
IRFormat ir_detect_format(const char* filename);

/**
 * Detect format from file extension only
 * @param filename Path to IR file
 * @return IRFormat type based on extension
 */
IRFormat ir_detect_format_by_extension(const char* filename);

/**
 * Detect format from file content (reads magic number or JSON signature)
 * @param filename Path to IR file
 * @return IRFormat type based on content
 */
IRFormat ir_detect_format_by_content(const char* filename);

/**
 * Check if file is binary format (KIRB or legacy)
 * @param filename Path to IR file
 * @return true if binary format detected
 */
bool ir_is_binary_format(const char* filename);

/**
 * Check if file is JSON format
 * @param filename Path to IR file
 * @return true if JSON format detected
 */
bool ir_is_json_format(const char* filename);

/**
 * Get format name as string for display
 * @param format IRFormat enum value
 * @return String representation of format
 */
const char* ir_format_name(IRFormat format);

#endif // IR_FORMAT_DETECT_H
