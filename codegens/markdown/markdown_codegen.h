#ifndef MARKDOWN_CODEGEN_H
#define MARKDOWN_CODEGEN_H

#include "../../ir/ir_core.h"
#include <stdbool.h>

// Markdown codegen options
typedef struct MarkdownCodegenOptions {
    bool format;                  // Run formatter if available
    bool preserve_html;           // Preserve HTML blocks
} MarkdownCodegenOptions;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate Markdown from KIR JSON file
 *
 * Reads a .kir file and generates Markdown.
 * The generated markdown includes:
 * - Component tree using Markdown syntax
 * - HTML blocks with event handlers from logic_block section
 * - Headings, paragraphs, lists, etc.
 *
 * @param kir_path Path to .kir JSON file
 * @param output_path Path where generated .md file should be written
 * @return bool true on success, false on error
 *
 * @example
 *   bool success = markdown_codegen_generate("app.kir", "app.md");
 */
bool markdown_codegen_generate(const char* kir_path, const char* output_path);

/**
 * Generate Markdown from KIR JSON string
 *
 * @param kir_json KIR JSON string
 * @return char* Generated Markdown, or NULL on error. Caller must free.
 */
char* markdown_codegen_from_json(const char* kir_json);

/**
 * Generate Markdown with codegen options
 *
 * @param kir_path Path to .kir JSON file
 * @param output_path Path where generated .md file should be written
 * @param options Codegen options (NULL for defaults)
 * @return bool true on success, false on error
 */
bool markdown_codegen_generate_with_options(const char* kir_path,
                                             const char* output_path,
                                             MarkdownCodegenOptions* options);

#ifdef __cplusplus
}
#endif

#endif // MARKDOWN_CODEGEN_H
