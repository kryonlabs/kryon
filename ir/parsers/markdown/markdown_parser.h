/**
 * Kryon Markdown Parser - Core Implementation
 *
 * Full CommonMark specification support.
 * Parses markdown source to native Kryon IR component trees.
 *
 * Supported Features:
 *   - Headings (# through ######)
 *   - Paragraphs with inline formatting (bold, italic, code, links, images)
 *   - Lists (ordered and unordered, nested)
 *   - Tables (GFM style with | delimiters)
 *   - Code blocks (fenced and indented, with language tags)
 *   - Blockquotes (nested)
 *   - Horizontal rules (---, ***, ___)
 */

#ifndef IR_MARKDOWN_PARSER_H
#define IR_MARKDOWN_PARSER_H

#include "ir_core.h"
#include "ir_builder.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse markdown source to IR component tree
 *
 * Converts markdown text into a native Kryon IR component hierarchy.
 * The returned component is a Column container with markdown elements
 * as children (headings, paragraphs, lists, tables, etc.).
 *
 * @param source Markdown source text (UTF-8 encoded)
 * @param length Length of source in bytes (0 for null-terminated string)
 * @return IRComponent* Root component (Column with children), or NULL on error
 *
 * @note The caller is responsible for freeing the returned component tree
 *       using ir_free_component_tree().
 *
 * @example
 *   const char* md = "# Hello\n\nThis is **bold** text.";
 *   IRComponent* root = ir_markdown_parse(md, 0);
 *   if (root) {
 *       // Render or serialize the component tree
 *       ir_free_component_tree(root);
 *   }
 */
IRComponent* ir_markdown_parse(const char* source, size_t length);

/**
 * Convert markdown source to KIR JSON string
 *
 * Convenience function that combines parsing and JSON serialization.
 * Useful for CLI tools and build pipelines that need to generate
 * .kir files from .md sources.
 *
 * @param source Markdown source text (UTF-8 encoded)
 * @param length Length of source in bytes (0 for null-terminated string)
 * @return char* JSON string in KIR v3.0 format (caller must free), or NULL on error
 *
 * @example
 *   const char* md = readFile("README.md");
 *   char* kir_json = ir_markdown_to_kir(md, 0);
 *   if (kir_json) {
 *       writeFile("README.kir", kir_json);
 *       free(kir_json);
 *   }
 */
char* ir_markdown_to_kir(const char* source, size_t length);

/**
 * Parse markdown and report unsupported features
 *
 * Same as ir_markdown_parse(), but also collects warnings for any
 * markdown features that cannot be represented in Kryon IR.
 *
 * @param source Markdown source text (UTF-8 encoded)
 * @param length Length of source in bytes (0 for null-terminated string)
 * @param warnings Output parameter - array of warning strings (NULL-terminated)
 * @return IRComponent* Parsed tree, warnings in output parameter
 *
 * @note If warnings is NULL, this function behaves identically to ir_markdown_parse().
 * @note The caller must free both the component tree and the warnings array.
 *
 * @example
 *   char** warnings = NULL;
 *   IRComponent* root = ir_markdown_parse_with_warnings(md, 0, &warnings);
 *   if (warnings) {
 *       for (int i = 0; warnings[i]; i++) {
 *           fprintf(stderr, "Warning: %s\n", warnings[i]);
 *           free(warnings[i]);
 *       }
 *       free(warnings);
 *   }
 */
IRComponent* ir_markdown_parse_with_warnings(const char* source, size_t length,
                                              char*** warnings);

/**
 * Get parser version string
 *
 * @return const char* Version string (e.g., "1.0.0-commonmark-0.31.2")
 */
const char* ir_markdown_parser_version(void);

/**
 * Check if a specific markdown feature is supported
 *
 * @param feature_name Feature name (e.g., "tables", "mermaid", "footnotes")
 * @return bool True if feature is supported, false otherwise
 */
bool ir_markdown_feature_supported(const char* feature_name);

#ifdef __cplusplus
}
#endif

#endif // IR_MARKDOWN_PARSER_H
