#ifndef DIS_INTERNAL_H
#define DIS_INTERNAL_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "module_builder.h"

// DynamicArray and HashTable are now defined in module_builder.h

// Forward declarations for types used in codegen
typedef struct {
    uint32_t size;           // Size in bytes
    uint32_t nptrs;          // Number of pointers
    uint8_t* pointer_map;    // Bitmap of pointer locations
    uint32_t map_size;       // Size of pointer map in bytes
} DISTypeDescriptor;

typedef struct {
    char* name;              // Symbol name
    uint32_t pc;             // Program counter
    uint32_t type;           // Type index
    uint8_t sig;             // Signature (for functions)
} DISLinkEntry;

typedef struct {
    char* module;            // Module name
    char* name;              // Symbol name
    uint32_t offset;         // Offset to patch
} DISImportEntry;

typedef struct {
    uint8_t* data;           // Raw data
    uint32_t size;           // Size in bytes
    uint32_t offset;         // Offset in data section
} DISDataItem;

// Dynamic array and hash table are defined in module_builder.h

// ============================================================================
// Operand Encoding (for TaijiOS .dis format)
// ============================================================================

/**
 * Write a variable-length encoded operand to file
 * Uses TaijiOS encoding scheme:
 * - 0x00-0x3F: single byte, positive
 * - 0x40-0x7F: single byte, negative
 * - 0x80-0xBF: two bytes
 * - 0xC0-0xFF: four bytes
 */
static inline void write_operand(FILE* f, int32_t value) {
    if (value >= 0 && value <= 0x3F) {
        fputc((char)value, f);
    } else if (value >= -64 && value < 0) {
        fputc((char)(value | 0x80), f);
    } else if (value >= -16384 && value < 16384 && value != -32768) {
        if (value < 0) {
            fputc((char)(((value >> 8) & 0x3F) | 0xA0), f);
        } else {
            fputc((char)(((value >> 8) & 0x3F) | 0x80), f);
        }
        fputc((char)(value & 0xFF), f);
    } else {
        if (value < 0) {
            fputc((char)(((value >> 24) & 0x3F) | 0xE0), f);
        } else {
            fputc((char)(((value >> 24) & 0x3F) | 0xC0), f);
        }
        fputc((char)((value >> 16) & 0xFF), f);
        fputc((char)((value >> 8) & 0xFF), f);
        fputc((char)(value & 0xFF), f);
    }
}

// ============================================================================
// Big-Endian Write Helpers
// ============================================================================

static inline void write_bigendian_32(FILE* f, uint32_t value) {
    fputc((value >> 24) & 0xFF, f);
    fputc((value >> 16) & 0xFF, f);
    fputc((value >> 8) & 0xFF, f);
    fputc(value & 0xFF, f);
}

static inline void write_bigendian_64(FILE* f, uint64_t value) {
    fputc((value >> 56) & 0xFF, f);
    fputc((value >> 48) & 0xFF, f);
    fputc((value >> 40) & 0xFF, f);
    fputc((value >> 32) & 0xFF, f);
    fputc((value >> 24) & 0xFF, f);
    fputc((value >> 16) & 0xFF, f);
    fputc((value >> 8) & 0xFF, f);
    fputc(value & 0xFF, f);
}

// ============================================================================
// Forward Declarations for Functions
// ============================================================================

// From data_section.c
bool emit_data_bytes(DISModuleBuilder* builder, const uint8_t* data, uint32_t offset, uint32_t size);
bool emit_data_byte(DISModuleBuilder* builder, uint8_t value, uint32_t offset);
bool emit_data_word(DISModuleBuilder* builder, int32_t value, uint32_t offset);
bool emit_data_words(DISModuleBuilder* builder, const int32_t* values, uint32_t offset, uint32_t count);
bool emit_data_long(DISModuleBuilder* builder, int64_t value, uint32_t offset);
bool emit_data_float(DISModuleBuilder* builder, double value, uint32_t offset);
bool emit_data_string(DISModuleBuilder* builder, const char* str, uint32_t offset);
bool emit_data_array(DISModuleBuilder* builder, uint32_t type_idx, uint32_t len, uint32_t offset);
bool emit_data_end(DISModuleBuilder* builder);

// From type_descriptor.c
DISTypeDescriptor* type_descriptor_create(uint32_t size, uint32_t nptrs);
void type_descriptor_destroy(DISTypeDescriptor* type);
bool type_descriptor_set_pointer(DISTypeDescriptor* type, uint32_t offset);
uint8_t* generate_pointer_map(const char* kir_type, size_t* map_size);
uint32_t calculate_type_size(const char* kir_type);
int32_t register_type(DISModuleBuilder* builder, const char* type_name);
bool write_type_section(DISModuleBuilder* builder);

// From link_section.c
DISLinkEntry* link_entry_create(const char* name, uint32_t pc, uint32_t type, uint8_t sig);
void link_entry_destroy(DISLinkEntry* entry);
DISImportEntry* import_entry_create(const char* module, const char* name, uint32_t offset);
void import_entry_destroy(DISImportEntry* entry);
bool add_export(DISModuleBuilder* builder, const char* name, uint32_t pc, uint32_t type, uint8_t sig);
bool add_import(DISModuleBuilder* builder, const char* module, const char* name, uint32_t offset);
bool write_link_section(DISModuleBuilder* builder);
bool write_import_section(DISModuleBuilder* builder);

#endif // DIS_INTERNAL_H
