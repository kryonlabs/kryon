#ifndef DIS_MODULE_BUILDER_H
#define DIS_MODULE_BUILDER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
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

#ifdef __cplusplus
}
#endif

#endif // DIS_MODULE_BUILDER_H
