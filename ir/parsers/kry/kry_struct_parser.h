/**
 * KRY Struct Parser
 *
 * Handles parsing of struct declarations and instantiations in .kry files.
 * This is a separate module to keep the main parser clean and focused.
 */

#ifndef IR_KRY_STRUCT_PARSER_H
#define IR_KRY_STRUCT_PARSER_H

#include "kry_ast.h"

#ifdef __cplusplus
extern "C" {
#endif

// Parse struct declaration: struct Habit { name: string = ""; ... }
KryNode* kry_parse_struct_declaration(KryParser* parser);

// Parse struct instantiation: Habit { name = "Exercise" }
KryNode* kry_parse_struct_instantiation(KryParser* parser, char* type_name);

// Check if identifier matches struct keyword
bool kry_is_struct_keyword(const char* identifier);

#ifdef __cplusplus
}
#endif

#endif // IR_KRY_STRUCT_PARSER_H
