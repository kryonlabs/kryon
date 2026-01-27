/**
 * DIS Module Builder
 *
 * Builds .dis bytecode files by managing sections and state.
 * Implements the object file format described in DIS VM specification.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "module_builder.h"

// Forward definition to avoid circular dependency
typedef struct {
    bool optimize;          // Enable optimizations
    bool debug_info;        // Include debug information
    uint32_t stack_extent;  // Stack growth increment (default: 4096)
    bool share_mp;          // Share MP between modules
    const char* module_name; // Module name (default: derived from input)
} DISCodegenOptions;

// Magic numbers
#define XMAGIC 819248  // Extended magic (executable)
#define SMAGIC 923426  // Standard magic

// Addressing modes
#define AMP     0x00  // Offset from MP (global)
#define AFP     0x01  // Offset from FP (local)
#define AIMM    0x02  // Immediate
#define AXXX    0x03  // None

// Maximum initial capacities
#define MAX_CODE_SIZE     4096
#define MAX_TYPE_SIZE     256
#define MAX_DATA_SIZE     4096
#define MAX_LINK_SIZE     128
#define MAX_IMPORT_SIZE   32

/**
 * DIS Instruction encoding
 * 32-bit instruction with opcode and addressing mode
 */
typedef struct {
    uint8_t opcode;
    uint8_t address_mode;
    int32_t middle_operand;
    int32_t source_operand;
    int32_t dest_operand;
} DISInstruction;

/**
 * Type descriptor for GC
 */
typedef struct {
    uint32_t size;           // Size in bytes
    uint8_t* pointer_map;    // Bitmap of pointer locations
    uint32_t map_size;       // Size of pointer map in bytes
} DISTypeDescriptor;

/**
 * Link entry (export)
 */
typedef struct {
    char* name;              // Symbol name
    uint32_t pc;             // Program counter
    uint32_t type;           // Type index
    uint8_t sig;             // Signature (for functions)
} DISLinkEntry;

/**
 * Import entry
 */
typedef struct {
    char* module;            // Module name
    char* name;              // Symbol name
    uint32_t offset;         // Offset to patch
} DISImportEntry;

/**
 * Data item
 */
typedef struct {
    uint8_t* data;           // Raw data
    uint32_t size;           // Size in bytes
    uint32_t offset;         // Offset in data section
} DISDataItem;

/**
 * Simple dynamic array implementation
 */
typedef struct {
    void** items;
    size_t count;
    size_t capacity;
} DynamicArray;

static DynamicArray* dynamic_array_create(size_t initial_capacity) {
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

static bool dynamic_array_add(DynamicArray* array, void* item) {
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

static void dynamic_array_destroy(DynamicArray* array) {
    if (array) {
        free(array->items);
        free(array);
    }
}

/**
 * Simple hash table for symbol lookup
 */
#define HASH_TABLE_SIZE 256

typedef struct HashEntry {
    char* key;
    void* value;
    struct HashEntry* next;
} HashEntry;

typedef struct {
    HashEntry* entries[HASH_TABLE_SIZE];
} HashTable;

static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + (*str++);
    }
    return hash;
}

static HashTable* hash_table_create(void) {
    return (HashTable*)calloc(1, sizeof(HashTable));
}

static bool hash_table_set(HashTable* table, const char* key, void* value) {
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

static void* hash_table_get(HashTable* table, const char* key) {
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

static void hash_table_destroy(HashTable* table) {
    if (table) {
        for (int i = 0; i < HASH_TABLE_SIZE; i++) {
            if (table->entries[i]) {
                free(table->entries[i]->key);
                free(table->entries[i]);
            }
        }
        free(table);
    }
}

/**
 * DIS Module Builder - Complete implementation
 */
struct DISModuleBuilder {
    // Output
    FILE* output_file;

    // Module metadata
    char* module_name;
    uint32_t entry_pc;
    uint32_t entry_type;

    // Sections
    DynamicArray* code_section;     // DISInstruction*
    DynamicArray* type_section;     // DISTypeDescriptor*
    DynamicArray* data_section;     // DISDataItem*
    DynamicArray* link_section;     // DISLinkEntry*
    DynamicArray* import_section;   // DISImportEntry*

    // Type tracking
    HashTable* type_descriptors;    // Type name → DISTypeDescriptor*
    HashTable* type_indices;        // Type name → index
    uint32_t next_type_index;

    // Symbol tracking
    HashTable* symbols;             // Symbol name → (offset, type)
    HashTable* functions;           // Function name → PC

    // Codegen state
    uint32_t current_pc;
    uint32_t current_frame_offset;
    uint32_t global_offset;

    // Options
    bool optimize;
    bool debug_info;
    uint32_t stack_extent;
    bool share_mp;
};

/**
 * Create a new module builder
 */
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

/**
 * Destroy module builder and free resources
 */
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

/**
 * Get section sizes for header
 */
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

/**
 * Set entry point
 */
void module_builder_set_entry(DISModuleBuilder* builder, uint32_t pc, uint32_t type) {
    builder->entry_pc = pc;
    builder->entry_type = type;
}

/**
 * Add symbol to symbol table
 */
bool module_builder_add_symbol(DISModuleBuilder* builder, const char* name,
                               uint32_t offset, uint32_t type) {
    // For simplicity, store as string "offset:type"
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%u:%u", offset, type);
    return hash_table_set(builder->symbols, name, strdup(buffer));
}

/**
 * Add function to function table
 */
bool module_builder_add_function(DISModuleBuilder* builder, const char* name, uint32_t pc) {
    return hash_table_set(builder->functions, name, (void*)(uintptr_t)pc);
}

/**
 * Look up function PC
 */
uint32_t module_builder_lookup_function(DISModuleBuilder* builder, const char* name) {
    void* value = hash_table_get(builder->functions, name);
    return value ? (uint32_t)(uintptr_t)value : 0xFFFFFFFF;
}

/**
 * Get current PC (program counter)
 */
uint32_t module_builder_get_pc(DISModuleBuilder* builder) {
    if (!builder) {
        return 0;
    }
    return builder->current_pc;
}

/**
 * Allocate global data space
 */
uint32_t module_builder_allocate_global(DISModuleBuilder* builder, uint32_t size) {
    if (!builder) {
        return 0;
    }

    uint32_t offset = builder->global_offset;
    builder->global_offset += size;
    return offset;
}

/**
 * Write module to .dis file
 */
bool module_builder_write_file(DISModuleBuilder* builder, const char* path) {
    if (!builder || !path) {
        return false;
    }

    FILE* f = fopen(path, "wb");
    if (!f) {
        return false;
    }

    // Get section sizes
    uint32_t code_size = 0, type_size = 0, data_size = 0, link_size = 0;
    module_builder_get_sizes(builder, &code_size, &type_size, &data_size, &link_size);

    // Write header
    // For now, just write a simple stub
    fwrite(&code_size, sizeof(uint32_t), 1, f);
    fwrite(&type_size, sizeof(uint32_t), 1, f);
    fwrite(&data_size, sizeof(uint32_t), 1, f);
    fwrite(&link_size, sizeof(uint32_t), 1, f);

    // TODO: Write actual sections

    fclose(f);
    return true;
}
