#ifndef DIS_MODULE_BUILDER_H
#define DIS_MODULE_BUILDER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for types used in module builder
typedef struct HashEntry HashEntry;

// Dynamic Array
typedef struct {
    void** items;
    size_t count;
    size_t capacity;
} DynamicArray;

// Hash Entry
struct HashEntry {
    char* key;
    void* value;
    struct HashEntry* next;
};

// Hash Table
typedef struct {
    HashEntry* entries[256];
} HashTable;

// ============================================================================
// Dynamic Array and Hash Table Functions
// ============================================================================

DynamicArray* dynamic_array_create(size_t initial_capacity);
bool dynamic_array_add(DynamicArray* array, void* item);
void dynamic_array_destroy(DynamicArray* array);

HashTable* hash_table_create(void);
bool hash_table_set(HashTable* table, const char* key, void* value);
void* hash_table_get(HashTable* table, const char* key);
void hash_table_destroy(HashTable* table);

// ============================================================================
// Complete DISModuleBuilder definition
// ============================================================================
struct DISModuleBuilder {
    // Output
    FILE* output_file;

    // Module metadata
    char* module_name;
    uint32_t entry_pc;
    uint32_t entry_type;

    // Sections
    DynamicArray* code_section;
    DynamicArray* type_section;
    DynamicArray* data_section;
    DynamicArray* link_section;
    DynamicArray* import_section;

    // Type tracking
    HashTable* type_descriptors;
    HashTable* type_indices;
    uint32_t next_type_index;

    // Symbol tracking
    HashTable* symbols;
    HashTable* functions;

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

typedef struct DISModuleBuilder DISModuleBuilder;

/**
 * Create a new module builder
 * @param module_name Name of the module
 * @param opts Codegen options (NULL for defaults)
 * @return New module builder, or NULL on error
 */
DISModuleBuilder* module_builder_create(const char* module_name, void* opts);

/**
 * Destroy module builder and free all resources
 * @param builder Module builder to destroy
 */
void module_builder_destroy(DISModuleBuilder* builder);

/**
 * Get section sizes for header generation
 * @param builder Module builder
 * @param code_size Output code section size
 * @param type_size Output type section size
 * @param data_size Output data section size
 * @param link_size Output link section size
 */
void module_builder_get_sizes(DISModuleBuilder* builder,
                              uint32_t* code_size,
                              uint32_t* type_size,
                              uint32_t* data_size,
                              uint32_t* link_size);

/**
 * Set module entry point
 * @param builder Module builder
 * @param pc Entry point program counter
 * @param type Entry point type index
 */
void module_builder_set_entry(DISModuleBuilder* builder, uint32_t pc, uint32_t type);

/**
 * Add symbol to symbol table
 * @param builder Module builder
 * @param name Symbol name
 * @param offset Offset in module
 * @param type Type index
 * @return true on success, false on error
 */
bool module_builder_add_symbol(DISModuleBuilder* builder, const char* name,
                               uint32_t offset, uint32_t type);

/**
 * Add function to function table
 * @param builder Module builder
 * @param name Function name
 * @param pc Function entry point PC
 * @return true on success, false on error
 */
bool module_builder_add_function(DISModuleBuilder* builder, const char* name, uint32_t pc);

/**
 * Look up function PC by name
 * @param builder Module builder
 * @param name Function name
 * @return Function PC, or 0xFFFFFFFF if not found
 */
uint32_t module_builder_lookup_function(DISModuleBuilder* builder, const char* name);

/**
 * Get current PC (program counter)
 * @param builder Module builder
 * @return Current PC
 */
uint32_t module_builder_get_pc(DISModuleBuilder* builder);

/**
 * Allocate global data space
 * @param builder Module builder
 * @param size Size in bytes
 * @return Offset in global data, or 0 on error
 */
uint32_t module_builder_allocate_global(DISModuleBuilder* builder, uint32_t size);

/**
 * Write module to .dis file
 * @param builder Module builder
 * @param path Output file path
 * @return true on success, false on error
 */
bool module_builder_write_file(DISModuleBuilder* builder, const char* path);

/**
 * Get output file handle (internal use)
 * @param builder Module builder
 * @return FILE handle, or NULL if not open
 */
FILE* module_builder_get_output_file(DISModuleBuilder* builder);

/**
 * Emit an instruction to the code section
 * @param builder Module builder
 * @param opcode Instruction opcode
 * @param addr_mode Addressing mode
 * @param middle Middle operand
 * @param src Source operand
 * @param dst Destination operand
 * @return PC of emitted instruction
 */
uint32_t emit_insn(DISModuleBuilder* builder,
                   uint8_t opcode,
                   uint8_t addr_mode,
                   int32_t middle,
                   int32_t src,
                   int32_t dst);

// ============================================================================
// Instruction Emitter Functions (from instruction_emitter.c)
// ============================================================================

uint32_t emit_add_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst);
uint32_t emit_sub_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst);
uint32_t emit_mul_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst);
uint32_t emit_div_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst);
uint32_t emit_mov_w(DISModuleBuilder* builder, int32_t src, int32_t dst);
uint32_t emit_mov_p(DISModuleBuilder* builder, int32_t src, int32_t dst);
uint32_t emit_mov_imm_w(DISModuleBuilder* builder, int32_t imm, int32_t dst);
uint32_t emit_call(DISModuleBuilder* builder, uint32_t target_pc);
uint32_t emit_ret(DISModuleBuilder* builder);
uint32_t emit_jmp(DISModuleBuilder* builder, uint32_t target_pc);
uint32_t emit_branch_eq_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, uint32_t target_pc);
uint32_t emit_branch_ne_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, uint32_t target_pc);
uint32_t emit_branch_lt_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, uint32_t target_pc);
uint32_t emit_branch_gt_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, uint32_t target_pc);
uint32_t emit_new(DISModuleBuilder* builder, uint32_t type_idx, int32_t dst);
uint32_t emit_newa(DISModuleBuilder* builder, int32_t count, uint32_t type_idx, int32_t dst);
uint32_t emit_frame(DISModuleBuilder* builder, uint32_t type_idx, int32_t dst);
uint32_t emit_exit(DISModuleBuilder* builder, int32_t code);

// ============================================================================
// Module Loading and Import Instructions
// ============================================================================

uint32_t emit_load(DISModuleBuilder* builder, const char* module_name);
uint32_t emit_call_import(DISModuleBuilder* builder, const char* module, const char* func);

// ============================================================================
// String Operations
// ============================================================================

uint32_t emit_load_string_address(DISModuleBuilder* builder, const char* str, int32_t dst);
bool emit_string_to_data(DISModuleBuilder* builder, const char* str, uint32_t* offset);

// ============================================================================
// Channel Operations (for Event Loop)
// ============================================================================

uint32_t emit_alt(DISModuleBuilder* builder);
uint32_t emit_chan_recv(DISModuleBuilder* builder, int32_t channel_reg, int32_t dst);

// ============================================================================
// Method Calls (for libdraw API)
// ============================================================================

uint32_t emit_method_call(DISModuleBuilder* builder, uint32_t method_offset, int32_t argc);

// ============================================================================
// Frame Operations
// ============================================================================

uint32_t emit_enter_frame(DISModuleBuilder* builder, uint32_t size);
uint32_t emit_leave_frame(DISModuleBuilder* builder);

// ============================================================================
// Additional Move Operations
// ============================================================================

uint32_t emit_mov_address(DISModuleBuilder* builder, uint32_t addr, int32_t dst);

#ifdef __cplusplus
}
#endif

#endif // DIS_MODULE_BUILDER_H
