/**
 * DIS Type Descriptor Generator for TaijiOS
 *
 * Generates GC type descriptors for TaijiOS DIS format.
 * Based on /home/wao/Projects/TaijiOS/libinterp/load.c
 */

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "internal.h"

/**
 * Create a new type descriptor
 */
DISTypeDescriptor* type_descriptor_create(uint32_t size, uint32_t nptrs) {
    DISTypeDescriptor* type = (DISTypeDescriptor*)calloc(1, sizeof(DISTypeDescriptor));
    if (!type) {
        return NULL;
    }

    type->size = size;
    type->nptrs = nptrs;

    // Calculate bitmap size (1 bit per word, rounded up to bytes)
    uint32_t nwords = (size + sizeof(intptr_t) - 1) / sizeof(intptr_t);
    type->map_size = (nwords + 7) / 8;

    if (type->map_size > 0) {
        type->pointer_map = (uint8_t*)calloc(type->map_size, 1);
        if (!type->pointer_map) {
            free(type);
            return NULL;
        }
    }

    return type;
}

/**
 * Destroy a type descriptor
 */
void type_descriptor_destroy(DISTypeDescriptor* type) {
    if (type) {
        free(type->pointer_map);
        free(type);
    }
}

/**
 * Set a pointer at a specific offset
 * @param type Type descriptor
 * @param offset Byte offset (must be word-aligned)
 */
bool type_descriptor_set_pointer(DISTypeDescriptor* type, uint32_t offset) {
    if (!type || !type->pointer_map) {
        return false;
    }

    // Verify word alignment
    if (offset % sizeof(intptr_t) != 0) {
        return false;
    }

    uint32_t word_index = offset / sizeof(intptr_t);
    uint32_t byte_index = word_index / 8;
    uint32_t bit_index = word_index % 8;

    if (byte_index >= type->map_size) {
        return false;
    }

    type->pointer_map[byte_index] |= (1 << bit_index);
    return true;
}

/**
 * Generate pointer map for a KIR type
 * This is a simplified version - real implementation would parse KIR types
 */
uint8_t* generate_pointer_map(const char* kir_type, size_t* map_size) {
    if (!kir_type || !map_size) {
        return NULL;
    }

    // For now, use simple heuristics
    // In a real implementation, this would parse KIR type definitions

    // String type: has 1 pointer (the string data)
    if (strcmp(kir_type, "string") == 0 || strcmp(kir_type, "text") == 0) {
        *map_size = 1;
        uint8_t* map = (uint8_t*)calloc(1, 1);
        if (map) {
            map[0] = 0x01;  // First word is a pointer
        }
        return map;
    }

    // Array type: has 1 pointer (the array data)
    if (strcmp(kir_type, "array") == 0) {
        *map_size = 1;
        uint8_t* map = (uint8_t*)calloc(1, 1);
        if (map) {
            map[0] = 0x01;  // First word is a pointer
        }
        return map;
    }

    // Number/boolean: no pointers
    if (strcmp(kir_type, "number") == 0 || strcmp(kir_type, "integer") == 0 ||
        strcmp(kir_type, "float") == 0 || strcmp(kir_type, "boolean") == 0) {
        *map_size = 1;
        uint8_t* map = (uint8_t*)calloc(1, 1);
        return map;
    }

    // Component/record: assume all fields are pointers (conservative)
    // Real implementation would analyze the structure
    if (strncmp(kir_type, "component:", 10) == 0 ||
        strncmp(kir_type, "record:", 7) == 0) {
        *map_size = 8;  // Up to 64 pointers
        uint8_t* map = (uint8_t*)calloc(8, 1);
        if (map) {
            memset(map, 0xFF, 8);  // All pointers
        }
        return map;
    }

    // Unknown type: no pointers (conservative)
    *map_size = 1;
    return (uint8_t*)calloc(1, 1);
}

/**
 * Calculate size of a KIR type in bytes
 */
uint32_t calculate_type_size(const char* kir_type) {
    if (!kir_type) {
        return 0;
    }

    // Primitive types
    if (strcmp(kir_type, "number") == 0 || strcmp(kir_type, "integer") == 0) {
        return sizeof(int32_t);  // 4 bytes
    }
    if (strcmp(kir_type, "float") == 0) {
        return sizeof(double);   // 8 bytes
    }
    if (strcmp(kir_type, "boolean") == 0) {
        return sizeof(int32_t);  // 4 bytes (DIS uses word for bool)
    }
    if (strcmp(kir_type, "string") == 0 || strcmp(kir_type, "text") == 0) {
        return sizeof(intptr_t); // Pointer to string
    }
    if (strcmp(kir_type, "array") == 0) {
        return sizeof(intptr_t); // Pointer to array
    }

    // Component/record: placeholder
    // Real implementation would sum field sizes
    if (strncmp(kir_type, "component:", 10) == 0 ||
        strncmp(kir_type, "record:", 7) == 0) {
        return 32;  // Arbitrary size for now
    }

    return 0;
}

/**
 * Register a type and get its index
 * @param builder Module builder
 * @param type_name KIR type name
 * @return Type index, or -1 on error
 */
int32_t register_type(DISModuleBuilder* builder, const char* type_name) {
    if (!builder || !type_name) {
        return -1;
    }

    // Check if type already registered
    void* existing = hash_table_get(builder->type_indices, type_name);
    if (existing) {
        return (int32_t)(intptr_t)existing;
    }

    // Create new type descriptor
    uint32_t size = calculate_type_size(type_name);
    size_t map_size = 0;
    uint8_t* pointer_map = generate_pointer_map(type_name, &map_size);

    DISTypeDescriptor* type = type_descriptor_create(size, 0);
    if (!type) {
        free(pointer_map);
        return -1;
    }

    // Copy pointer map
    if (pointer_map && map_size > 0) {
        if (map_size <= type->map_size) {
            memcpy(type->pointer_map, pointer_map, map_size);
        }
        free(pointer_map);
    }

    // Count pointers from bitmap
    for (uint32_t i = 0; i < type->map_size; i++) {
        for (int bit = 0; bit < 8; bit++) {
            if (type->pointer_map[i] & (1 << bit)) {
                type->nptrs++;
            }
        }
    }

    // Add to type section
    if (!dynamic_array_add(builder->type_section, type)) {
        type_descriptor_destroy(type);
        return -1;
    }

    // Store type descriptor
    hash_table_set(builder->type_descriptors, type_name, type);

    // Get and store index
    uint32_t index = builder->next_type_index++;
    hash_table_set(builder->type_indices, type_name, (void*)(intptr_t)index);

    return (int32_t)index;
}

/**
 * Write type descriptors to file
 * Based on TaijiOS libinterp/load.c format
 */
bool write_type_section(DISModuleBuilder* builder) {
    if (!builder || !module_builder_get_output_file(builder)) {
        return false;
    }

    for (size_t i = 0; i < builder->type_section->count; i++) {
        DISTypeDescriptor* type = (DISTypeDescriptor*)builder->type_section->items[i];
        if (!type) {
            continue;
        }

        // Write type ID (index)
        write_operand(module_builder_get_output_file(builder), (int32_t)i);

        // Write size
        write_operand(module_builder_get_output_file(builder), (int32_t)type->size);

        // Write number of pointers
        write_operand(module_builder_get_output_file(builder), (int32_t)type->nptrs);

        // Write pointer map
        if (type->map_size > 0 && type->pointer_map) {
            fwrite(type->pointer_map, 1, type->map_size, module_builder_get_output_file(builder));
        }
    }

    return true;
}
