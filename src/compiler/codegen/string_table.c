/**

 * @file string_table.c
 * @brief String table management for KRB format
 * 
 * Handles string deduplication, storage, and serialization for the KRB binary format.
 * Provides efficient string table operations for the code generator.
 */
#include "lib9.h"


#include "codegen.h"
#include "binary_io.h"
#include <stdint.h>

bool kryon_write_string_table(KryonCodeGenerator *codegen) {
    if (!codegen) return false;
    
    fprintf(stderr, "DEBUG: Writing string table with %zu strings (capacity=%zu):\n", codegen->string_count, codegen->string_capacity);
    for (size_t i = 0; i < codegen->string_count && i < 10; i++) {
        fprintf(stderr, "  [%zu] '%s'\n", i, codegen->string_table[i] ? codegen->string_table[i] : "(null)");
    }
    if (codegen->string_count > 10) {
        fprintf(stderr, "  ... (%zu more strings)\n", codegen->string_count - 10);
    }
    
    // Write actual string count
    if (!write_uint32(codegen, (uint32_t)codegen->string_count)) {
        return false;
    }
    
    // Write each string
    for (size_t i = 0; i < codegen->string_count; i++) {
        const char *str = codegen->string_table[i];
        if (!str) {
            codegen_error(codegen, "NULL string in string table");
            return false;
        }
        
        // Write string length
        uint16_t len = (uint16_t)strlen(str);
        if (!write_uint16(codegen, len)) {
            return false;
        }
        
        // Write string data
        if (!write_binary_data(codegen, str, len)) {
            return false;
        }
    }
    
    return true;
}

uint32_t add_string_to_table(KryonCodeGenerator *codegen, const char *str) {
    if (!str) {
        return UINT32_MAX; // Error indicator
    }
    
    // Check if string already exists
    for (size_t i = 0; i < codegen->string_count; i++) {
        if (strcmp(codegen->string_table[i], str) == 0) {
            return (uint32_t)i; // 0-based index
        }
    }
    
    // Add new string
    if (codegen->string_count >= codegen->string_capacity) {
        size_t new_capacity = codegen->string_capacity * 2;
        char **new_table = realloc(codegen->string_table, 
                                        new_capacity * sizeof(char*));
        if (!new_table) {
            return UINT32_MAX; // Error indicator
        }
        codegen->string_table = new_table;
        codegen->string_capacity = new_capacity;
    }
    
    codegen->string_table[codegen->string_count] = strdup(str);
    if (!codegen->string_table[codegen->string_count]) {
        return UINT32_MAX; // Error indicator
    }
    
    uint32_t index = (uint32_t)(codegen->string_count++); // 0-based index
    fprintf(stderr, "DEBUG: Added string [%u]: '%s'\n", index, str);
    return index; // Return 0-based index
}
