/**
 * DIS Module Builder for TaijiOS
 *
 * Builds .dis bytecode files matching TaijiOS format.
 * Based on TaijiOS libinterp/load.c and include/isa.h
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "internal.h"
#include "../include/dis_codegen.h"

// ============================================================================
// Constants from TaijiOS include/isa.h
// ============================================================================

#define XMAGIC  819248  // Normal magic (executable)
#define SMAGIC  923426  // Signed module

#define MUSTCOMPILE  (1<<0)
#define DONTCOMPILE  (1<<1)
#define SHAREMP      (1<<2)
#define DYNMOD       (1<<3)
#define HASLDT0      (1<<4)
#define HASEXCEPT    (1<<5)
#define HASLDT       (1<<6)

// Data encoding types
#define DEFB    1   // Byte
#define DEFW    2   // Word
#define DEFS    3   // String
#define DEFF    4   // Float
#define DEFL    8   // Long

#define DTYPE(x)    ((x) >> 4)
#define DBYTE(x, l) (((x) << 4) | (l))
#define DMAX        (1 << 4)
#define DLEN(x)     ((x) & (DMAX - 1))

// Maximum initial capacities
#define MAX_CODE_SIZE     4096
#define MAX_TYPE_SIZE     256
#define MAX_DATA_SIZE     4096
#define MAX_LINK_SIZE     128
#define MAX_IMPORT_SIZE   32

// ============================================================================
// Dynamic Array Implementation
// ============================================================================

DynamicArray* dynamic_array_create(size_t initial_capacity) {
    DynamicArray* array = (DynamicArray*)malloc(sizeof(DynamicArray));
    if (!array) return NULL;

    array->items = (void**)calloc(initial_capacity, sizeof(void*));
    if (!array->items) {
        free(array);
        return NULL;
    }

    array->count = 0;
    array->capacity = initial_capacity;
    return array;
}

bool dynamic_array_add(DynamicArray* array, void* item) {
    if (!array || !item) return false;

    if (array->count >= array->capacity) {
        size_t new_capacity = array->capacity * 2;
        void** new_items = (void**)realloc(array->items, new_capacity * sizeof(void*));
        if (!new_items) return false;
        array->items = new_items;
        array->capacity = new_capacity;
    }

    array->items[array->count++] = item;
    return true;
}

void dynamic_array_destroy(DynamicArray* array) {
    if (array) {
        free(array->items);
        free(array);
    }
}

// ============================================================================
// Hash Table Implementation
// ============================================================================

#define HASH_TABLE_SIZE 256

static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + (*str++);
    }
    return hash;
}

HashTable* hash_table_create(void) {
    return (HashTable*)calloc(1, sizeof(HashTable));
}

bool hash_table_set(HashTable* table, const char* key, void* value) {
    if (!table || !key) return false;

    uint32_t index = hash_string(key) % HASH_TABLE_SIZE;

    // Check for existing entry
    HashEntry* entry = table->entries[index];
    HashEntry* prev = NULL;
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return true;
        }
        prev = entry;
        entry = entry->next;
    }

    // Create new entry and chain it
    HashEntry* new_entry = (HashEntry*)malloc(sizeof(HashEntry));
    if (!new_entry) return false;

    new_entry->key = strdup(key);
    new_entry->value = value;
    new_entry->next = NULL;

    if (prev) {
        prev->next = new_entry;
    } else {
        table->entries[index] = new_entry;
    }
    return true;
}

void* hash_table_get(HashTable* table, const char* key) {
    if (!table || !key) return NULL;

    uint32_t index = hash_string(key) % HASH_TABLE_SIZE;
    HashEntry* entry = table->entries[index];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }

    return NULL;
}

void hash_table_destroy(HashTable* table) {
    if (table) {
        for (int i = 0; i < HASH_TABLE_SIZE; i++) {
            HashEntry* entry = table->entries[i];
            while (entry) {
                HashEntry* next = entry->next;
                free(entry->key);
                free(entry);
                entry = next;
            }
        }
        free(table);
    }
}

// ============================================================================
// Module Builder Creation and Destruction
// ============================================================================

DISModuleBuilder* module_builder_create(const char* module_name, void* opts) {
    DISModuleBuilder* builder = (DISModuleBuilder*)calloc(1, sizeof(DISModuleBuilder));
    if (!builder) {
        return NULL;
    }

    builder->module_name = strdup(module_name);
    if (!builder->module_name) {
        free(builder);
        return NULL;
    }

    // Initialize sections
    builder->code_section = dynamic_array_create(MAX_CODE_SIZE);
    builder->type_section = dynamic_array_create(MAX_TYPE_SIZE);
    builder->data_section = dynamic_array_create(MAX_DATA_SIZE);
    builder->link_section = dynamic_array_create(MAX_LINK_SIZE);
    builder->import_section = dynamic_array_create(MAX_IMPORT_SIZE);

    if (!builder->code_section || !builder->type_section || !builder->data_section ||
        !builder->link_section || !builder->import_section) {
        // Cleanup on failure
        if (builder->code_section) dynamic_array_destroy(builder->code_section);
        if (builder->type_section) dynamic_array_destroy(builder->type_section);
        if (builder->data_section) dynamic_array_destroy(builder->data_section);
        if (builder->link_section) dynamic_array_destroy(builder->link_section);
        if (builder->import_section) dynamic_array_destroy(builder->import_section);
        free(builder->module_name);
        free(builder);
        return NULL;
    }

    // Initialize hash tables
    builder->type_descriptors = hash_table_create();
    builder->type_indices = hash_table_create();
    builder->symbols = hash_table_create();
    builder->functions = hash_table_create();

    // Set options
    if (opts) {
        DISCodegenOptions* options = (DISCodegenOptions*)opts;
        builder->optimize = options->optimize;
        builder->debug_info = options->debug_info;
        builder->stack_extent = options->stack_extent;
        builder->share_mp = options->share_mp;
    } else {
        builder->optimize = false;
        builder->debug_info = false;
        builder->stack_extent = 4096;
        builder->share_mp = false;
    }

    // Initialize state
    builder->entry_pc = 0;
    builder->entry_type = 0;
    builder->current_pc = 0;
    builder->current_frame_offset = 0;
    builder->global_offset = 0;
    builder->next_type_index = 0;

    return builder;
}

void module_builder_destroy(DISModuleBuilder* builder) {
    if (!builder) return;

    free(builder->module_name);

    // Clean up sections
    if (builder->code_section) {
        for (size_t i = 0; i < builder->code_section->count; i++) {
            free(builder->code_section->items[i]);
        }
        dynamic_array_destroy(builder->code_section);
    }

    if (builder->type_section) {
        for (size_t i = 0; i < builder->type_section->count; i++) {
            DISTypeDescriptor* type = (DISTypeDescriptor*)builder->type_section->items[i];
            if (type) {
                free(type->pointer_map);
                free(type);
            }
        }
        dynamic_array_destroy(builder->type_section);
    }

    if (builder->data_section) {
        for (size_t i = 0; i < builder->data_section->count; i++) {
            DISDataItem* item = (DISDataItem*)builder->data_section->items[i];
            if (item) {
                free(item->data);
                free(item);
            }
        }
        dynamic_array_destroy(builder->data_section);
    }

    if (builder->link_section) {
        for (size_t i = 0; i < builder->link_section->count; i++) {
            DISLinkEntry* entry = (DISLinkEntry*)builder->link_section->items[i];
            if (entry) {
                free(entry->name);
                free(entry);
            }
        }
        dynamic_array_destroy(builder->link_section);
    }

    if (builder->import_section) {
        for (size_t i = 0; i < builder->import_section->count; i++) {
            DISImportEntry* entry = (DISImportEntry*)builder->import_section->items[i];
            if (entry) {
                free(entry->module);
                free(entry->name);
                free(entry);
            }
        }
        dynamic_array_destroy(builder->import_section);
    }

    // Clean up hash tables
    if (builder->type_descriptors) hash_table_destroy(builder->type_descriptors);
    if (builder->type_indices) hash_table_destroy(builder->type_indices);
    if (builder->symbols) hash_table_destroy(builder->symbols);
    if (builder->functions) hash_table_destroy(builder->functions);

    free(builder);
}

// ============================================================================
// Module Builder Accessors
// ============================================================================

void module_builder_get_sizes(DISModuleBuilder* builder,
                              uint32_t* code_size,
                              uint32_t* type_size,
                              uint32_t* data_size,
                              uint32_t* link_size) {
    if (code_size) *code_size = builder->code_section ? builder->code_section->count : 0;
    if (type_size) *type_size = builder->type_section ? builder->type_section->count : 0;
    if (data_size) *data_size = builder->data_section ? builder->data_section->count : 0;
    if (link_size) *link_size = builder->link_section ? builder->link_section->count : 0;
}

void module_builder_set_entry(DISModuleBuilder* builder, uint32_t pc, uint32_t type) {
    builder->entry_pc = pc;
    builder->entry_type = type;
}

bool module_builder_add_symbol(DISModuleBuilder* builder, const char* name,
                               uint32_t offset, uint32_t type) {
    // For simplicity, store as string "offset:type"
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%u:%u", offset, type);
    return hash_table_set(builder->symbols, name, strdup(buffer));
}

bool module_builder_add_function(DISModuleBuilder* builder, const char* name, uint32_t pc) {
    return hash_table_set(builder->functions, name, (void*)(uintptr_t)pc);
}

uint32_t module_builder_lookup_function(DISModuleBuilder* builder, const char* name) {
    void* value = hash_table_get(builder->functions, name);
    return value ? (uint32_t)(uintptr_t)value : 0xFFFFFFFF;
}

uint32_t module_builder_get_pc(DISModuleBuilder* builder) {
    if (!builder) {
        return 0;
    }
    return builder->current_pc;
}

uint32_t module_builder_allocate_global(DISModuleBuilder* builder, uint32_t size) {
    if (!builder) {
        return 0;
    }

    uint32_t offset = builder->global_offset;
    builder->global_offset += size;
    return offset;
}

FILE* module_builder_get_output_file(DISModuleBuilder* builder) {
    if (!builder) {
        return NULL;
    }
    return builder->output_file;
}

// ============================================================================
// TaijiOS File Writer
// ============================================================================

/**
 * Write the complete .dis file in TaijiOS format
 * Based on /home/wao/Projects/TaijiOS/libinterp/load.c
 */
bool module_builder_write_file(DISModuleBuilder* builder, const char* path) {
    if (!builder || !path) {
        return false;
    }

    FILE* f = fopen(path, "wb");
    if (!f) {
        return false;
    }

    builder->output_file = f;

    // Get section sizes
    uint32_t code_size = 0, type_size = 0, link_size = 0;
    uint32_t data_size = builder->global_offset;  // Data section size is global data size
    module_builder_get_sizes(builder, &code_size, &type_size, NULL, &link_size);

    // Calculate runtime flags
    uint32_t runtime_flag = 0;
    if (builder->share_mp) {
        runtime_flag |= SHAREMP;
    }

    // Note: We don't set HASLDT flag because imports are handled
    // through ILOAD/ICALL instructions, not a separate import section

    // Write magic number
    write_operand(f, XMAGIC);

    // Write runtime flags
    write_operand(f, runtime_flag);

    // Write section sizes (ss, isize, dsize, hsize, lsize)
    write_operand(f, 0);  // ss (stack size, unused in modern TaijiOS)
    write_operand(f, code_size);
    write_operand(f, data_size);
    write_operand(f, type_size);
    write_operand(f, link_size);

    // Write entry point
    write_operand(f, builder->entry_pc);
    write_operand(f, builder->entry_type);

    // Write code section (encoded instructions)
    // Each instruction is stored as: [size (4 bytes)][instruction bytes...]
    for (size_t i = 0; i < builder->code_section->count; i++) {
        uint8_t* insn_data = (uint8_t*)builder->code_section->items[i];
        if (!insn_data) continue;

        // First 4 bytes are the size
        uint32_t insn_size = *((uint32_t*)insn_data);
        // Following bytes are the instruction
        uint8_t* insn_bytes = insn_data + 4;

        // Write instruction bytes
        fwrite(insn_bytes, 1, insn_size, f);
    }

    // Write type section
    for (size_t i = 0; i < builder->type_section->count; i++) {
        DISTypeDescriptor* type = (DISTypeDescriptor*)builder->type_section->items[i];
        if (!type) continue;

        // Write type ID
        write_operand(f, (int32_t)i);

        // Write size
        write_operand(f, (int32_t)type->size);

        // Write number of pointers
        write_operand(f, (int32_t)type->nptrs);

        // Write pointer map
        if (type->map_size > 0 && type->pointer_map) {
            fwrite(type->pointer_map, 1, type->map_size, f);
        }
    }

    // Write data section
    for (size_t i = 0; i < builder->data_section->count; i++) {
        DISDataItem* item = (DISDataItem*)builder->data_section->items[i];
        if (!item) continue;

        // Write data item (type, offset, data)
        fwrite(item->data, 1, item->size, f);
    }

    // Write terminating zero for data section
    fputc(0, f);

    // Write module name (null-terminated)
    fputs(builder->module_name, f);
    fputc(0, f);

    // Write link section (exports)
    for (size_t i = 0; i < builder->link_section->count; i++) {
        DISLinkEntry* entry = (DISLinkEntry*)builder->link_section->items[i];
        if (!entry) continue;

        write_operand(f, (int32_t)entry->pc);
        // Type is written as raw 4-byte big-endian value, not variable-length encoded
        write_bigendian_32(f, entry->type);
        write_operand(f, (int32_t)entry->sig);
        fputs(entry->name, f);
        fputc(0, f);
    }

    // Write module path (source file path)
    // For generated modules, we use a synthetic path
    char module_path[256];
    snprintf(module_path, sizeof(module_path), "/dis/%s.b", builder->module_name);
    fputs(module_path, f);
    fputc(0, f);

    // Close file
    fclose(f);
    builder->output_file = NULL;

    return true;
}
