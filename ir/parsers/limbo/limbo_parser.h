/**
 * Limbo Parser
 *
 * Parses Limbo source code into an AST
 */

#ifndef IR_LIMBO_PARSER_H
#define IR_LIMBO_PARSER_H

#include "limbo_ast.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Limbo parser
 */
typedef struct {
    LimboLexer* lexer;
    LimboToken current_token;
    bool has_error;
    char error_message[256];
} LimboParser;

/**
 * Create parser for Limbo source
 *
 * @param source Limbo source code
 * @param source_len Length of source (0 for null-terminated)
 * @return LimboParser* Parser (caller must free with limbo_parser_free)
 */
LimboParser* limbo_parser_create(const char* source, size_t source_len);

/**
 * Free parser
 *
 * @param parser Parser to free
 */
void limbo_parser_free(LimboParser* parser);

/**
 * Parse Limbo source to module AST
 *
 * @param parser Parser
 * @return LimboModule* Parsed module (caller must free), or NULL on error
 */
LimboModule* limbo_parser_parse(LimboParser* parser);

/**
 * Parse Limbo source directly (convenience function)
 *
 * @param source Limbo source code
 * @param source_len Length of source (0 for null-terminated)
 * @return LimboModule* Parsed module (caller must free), or NULL on error
 */
LimboModule* limbo_parse(const char* source, size_t source_len);

/**
 * Parse Limbo file
 *
 * @param filepath Path to .b file
 * @return LimboModule* Parsed module (caller must free), or NULL on error
 */
LimboModule* limbo_parse_file(const char* filepath);

/**
 * Get last error message from parser
 *
 * @param parser Parser
 * @return const char* Error message (or NULL if no error)
 */
const char* limbo_parser_get_error(LimboParser* parser);

#ifdef __cplusplus
}
#endif

#endif // IR_LIMBO_PARSER_H
