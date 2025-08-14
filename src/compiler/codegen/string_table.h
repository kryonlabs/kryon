/**
 * @file string_table.h
 * @brief String table management for KRB format
 */

#ifndef KRYON_STRING_TABLE_H
#define KRYON_STRING_TABLE_H

#include "codegen.h"

/**
 * Write the string table section to KRB format
 * @param codegen The code generator instance
 * @return true on success, false on failure
 */
bool kryon_write_string_table(KryonCodeGenerator *codegen);

/**
 * Add a string to the string table (with deduplication)
 * @param codegen The code generator instance
 * @param str The string to add
 * @return 1-based string index, or 0 on failure
 */
uint32_t add_string_to_table(KryonCodeGenerator *codegen, const char *str);

#endif // KRYON_STRING_TABLE_H