/**
 * DIS Instruction Emitter
 *
 * Emits DIS VM instructions with proper encoding and addressing modes.
 * Based on DIS VM opcode specification.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include "module_builder.h"

// ============================================================================
// Opcode Definitions (from DIS VM spec)
// ============================================================================

// Load/Store operations
#define OP_MOVB   0   // Move byte
#define OP_MOVW   1   // Move word
#define OP_MOVF   2   // Move float
#define OP_MOVP   3   // Move pointer

// Arithmetic operations (word)
#define OP_ADDW   10
#define OP_SUBW   11
#define OP_MULW   12
#define OP_DIVW   13
#define OP_MODW   14
#define OP_ANDW   15
#define OP_ORW    16
#define OP_XORW   17
#define OP_SHLW   18
#define OP_SHRW   19

// Arithmetic operations (float)
#define OP_ADDF   20
#define OP_SUBF   21
#define OP_MULF   22
#define OP_DIVF   23

// Comparison operations
#define OP_CMPW   30
#define OP_CMPF   31

// Control flow
#define OP_CALL   40
#define OP_RET    41
#define OP_JMP    42
#define OP_BEQW   43  // Branch if equal (word)
#define OP_BNEW   44  // Branch if not equal (word)
#define OP_BLTW   45  // Branch if less than (word)
#define OP_BGTW   46  // Branch if greater than (word)
#define OP_BLEW   47  // Branch if less or equal (word)
#define OP_BGEW   48  // Branch if greater or equal (word)

// Memory operations
#define OP_NEW    50   // Allocate new object
#define OP_NEWA   51   // Allocate new array
#define OP_FRAME  52   // Allocate stack frame

// Stack operations
#define OP_PUSH   60
#define OP_POP    61

// Type conversions
#define OP_CVTFW  70  // Convert float to word
#define OP_CVTWF  71  // Convert word to float

// Addressing modes
#define AMP     0x00  // Offset from MP (global)
#define AFP     0x01  // Offset from FP (local)
#define AIMM    0x02  // Immediate
#define AXXX    0x03  // None

// ============================================================================
// Instruction Encoding
// ============================================================================

/**
 * DIS Instruction encoding (32-bit)
 *
 * Format: [opcode:8 | address_mode:8 | unused:16]
 *
 * Operands are encoded as separate 32-bit values following the instruction
 */

typedef struct {
    uint8_t opcode;
    uint8_t address_mode;
    int32_t middle_operand;
    int32_t source_operand;
    int32_t dest_operand;
} DISInstruction;

/**
 * Encode operand with addressing mode
 *
 * @param value Operand value
 * @param mode Addressing mode (AMP, AFP, AIMM, AXXX)
 * @return Encoded operand
 */
static int32_t encode_operand(int32_t value, uint8_t mode) {
    // For DIS, the addressing mode is encoded in the instruction
    // The operand value is stored as-is (offset or immediate)
    return value;
}

/**
 * Emit a complete instruction with operands
 *
 * @param builder Module builder
 * @param insn Instruction structure
 * @return PC of emitted instruction, or -1 on error
 */
uint32_t emit_insn_full(DISModuleBuilder* builder, DISInstruction* insn) {
    if (!builder || !insn) {
        return -1;
    }

    // Allocate instruction
    DISInstruction* new_insn = (DISInstruction*)malloc(sizeof(DISInstruction));
    if (!new_insn) {
        return -1;
    }

    *new_insn = *insn;

    // Add to code section and get PC
    uint32_t pc = module_builder_get_pc(builder);

    // TODO: Add to code section via dynamic_array_add
    // dynamic_array_add(builder->code_section, new_insn);

    return pc;
}

/**
 * Emit instruction with simplified interface
 *
 * @param builder Module builder
 * @param opcode Opcode
 * @param addr_mode Addressing mode
 * @param middle Middle operand (or AIMM value)
 * @param src Source operand
 * @param dst Destination operand
 * @return PC of emitted instruction, or -1 on error
 */
uint32_t emit_insn(DISModuleBuilder* builder,
                   uint8_t opcode,
                   uint8_t addr_mode,
                   int32_t middle,
                   int32_t src,
                   int32_t dst) {
    DISInstruction insn = {
        .opcode = opcode,
        .address_mode = addr_mode,
        .middle_operand = middle,
        .source_operand = src,
        .dest_operand = dst
    };

    return emit_insn_full(builder, &insn);
}

// ============================================================================
// Arithmetic Operations (Word)
// ============================================================================

/**
 * Emit add word: dst = src1 + src2
 */
uint32_t emit_add_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst) {
    return emit_insn(builder, OP_ADDW, AMP, src2, src1, dst);
}

/**
 * Emit subtract word: dst = src1 - src2
 */
uint32_t emit_sub_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst) {
    return emit_insn(builder, OP_SUBW, AMP, src2, src1, dst);
}

/**
 * Emit multiply word: dst = src1 * src2
 */
uint32_t emit_mul_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst) {
    return emit_insn(builder, OP_MULW, AMP, src2, src1, dst);
}

/**
 * Emit divide word: dst = src1 / src2
 */
uint32_t emit_div_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst) {
    return emit_insn(builder, OP_DIVW, AMP, src2, src1, dst);
}

// ============================================================================
// Arithmetic Operations (Float)
// ============================================================================

/**
 * Emit add float: dst = src1 + src2
 */
uint32_t emit_add_f(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst) {
    return emit_insn(builder, OP_ADDF, AMP, src2, src1, dst);
}

/**
 * Emit subtract float: dst = src1 - src2
 */
uint32_t emit_sub_f(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst) {
    return emit_insn(builder, OP_SUBF, AMP, src2, src1, dst);
}

// ============================================================================
// Move Operations
// ============================================================================

/**
 * Emit move word: dst = src
 */
uint32_t emit_mov_w(DISModuleBuilder* builder, int32_t src, int32_t dst) {
    return emit_insn(builder, OP_MOVW, AMP, 0, src, dst);
}

/**
 * Emit move pointer: dst = src
 */
uint32_t emit_mov_p(DISModuleBuilder* builder, int32_t src, int32_t dst) {
    return emit_insn(builder, OP_MOVP, AMP, 0, src, dst);
}

/**
 * Emit move immediate: dst = immediate
 */
uint32_t emit_mov_imm_w(DISModuleBuilder* builder, int32_t imm, int32_t dst) {
    return emit_insn(builder, OP_MOVW, AIMM, 0, imm, dst);
}

// ============================================================================
// Control Flow
// ============================================================================

/**
 * Emit call instruction
 * @param builder Module builder
 * @param frame_size Stack frame size
 * @param target_pc Target function PC
 * @return PC of call instruction
 */
uint32_t emit_call(DISModuleBuilder* builder, uint32_t frame_size, uint32_t target_pc) {
    return emit_insn(builder, OP_CALL, AIMM, frame_size, target_pc, 0);
}

/**
 * Emit return instruction
 */
uint32_t emit_ret(DISModuleBuilder* builder) {
    return emit_insn(builder, OP_RET, AXXX, 0, 0, 0);
}

/**
 * Emit unconditional jump
 * @param builder Module builder
 * @param target_pc Target PC
 * @return PC of jump instruction
 */
uint32_t emit_jmp(DISModuleBuilder* builder, uint32_t target_pc) {
    return emit_insn(builder, OP_JMP, AIMM, 0, target_pc, 0);
}

/**
 * Emit branch if equal (word)
 * @param builder Module builder
 * @param src1 First operand
 * @param src2 Second operand
 * @param target_pc Target PC if condition true
 * @return PC of branch instruction
 */
uint32_t emit_branch_eq_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, uint32_t target_pc) {
    return emit_insn(builder, OP_BEQW, AMP, src2, src1, target_pc);
}

/**
 * Emit branch if not equal (word)
 */
uint32_t emit_branch_ne_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, uint32_t target_pc) {
    return emit_insn(builder, OP_BNEW, AMP, src2, src1, target_pc);
}

/**
 * Emit branch if less than (word)
 */
uint32_t emit_branch_lt_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, uint32_t target_pc) {
    return emit_insn(builder, OP_BLTW, AMP, src2, src1, target_pc);
}

/**
 * Emit branch if greater than (word)
 */
uint32_t emit_branch_gt_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, uint32_t target_pc) {
    return emit_insn(builder, OP_BGTW, AMP, src2, src1, target_pc);
}

// ============================================================================
// Memory Operations
// ============================================================================

/**
 * Emit allocate new object
 * @param builder Module builder
 * @param type_idx Type descriptor index
 * @param dst Destination for object pointer
 * @return PC of new instruction
 */
uint32_t emit_new(DISModuleBuilder* builder, uint32_t type_idx, int32_t dst) {
    return emit_insn(builder, OP_NEW, AIMM, type_idx, 0, dst);
}

/**
 * Emit allocate new array
 * @param builder Module builder
 * @param count Array element count
 * @param type_idx Element type index
 * @param dst Destination for array pointer
 * @return PC of newa instruction
 */
uint32_t emit_newa(DISModuleBuilder* builder, int32_t count, uint32_t type_idx, int32_t dst) {
    return emit_insn(builder, OP_NEWA, AIMM, type_idx, count, dst);
}

/**
 * Emit allocate stack frame
 * @param builder Module builder
 * @param type_idx Frame type index
 * @param dst Destination for frame pointer
 * @return PC of frame instruction
 */
uint32_t emit_frame(DISModuleBuilder* builder, uint32_t type_idx, int32_t dst) {
    return emit_insn(builder, OP_FRAME, AIMM, type_idx, 0, dst);
}

// ============================================================================
// Comparison Operations
// ============================================================================

/**
 * Emit compare words (sets flags)
 * @param builder Module builder
 * @param src1 First operand
 * @param src2 Second operand
 * @return PC of cmp instruction
 */
uint32_t emit_cmp_w(DISModuleBuilder* builder, int32_t src1, int32_t src2) {
    return emit_insn(builder, OP_CMPW, AMP, src2, src1, 0);
}

/**
 * Emit compare floats (sets flags)
 */
uint32_t emit_cmp_f(DISModuleBuilder* builder, int32_t src1, int32_t src2) {
    return emit_insn(builder, OP_CMPF, AMP, src2, src1, 0);
}

// ============================================================================
// Type Conversions
// ============================================================================

/**
 * Emit convert float to word
 */
uint32_t emit_cvt_f_w(DISModuleBuilder* builder, int32_t src, int32_t dst) {
    return emit_insn(builder, OP_CVTFW, AMP, 0, src, dst);
}

/**
 * Emit convert word to float
 */
uint32_t emit_cvt_w_f(DISModuleBuilder* builder, int32_t src, int32_t dst) {
    return emit_insn(builder, OP_CVTWF, AMP, 0, src, dst);
}

// ============================================================================
// Stack Operations
// ============================================================================

/**
 * Emit push word onto stack
 */
uint32_t emit_push_w(DISModuleBuilder* builder, int32_t src) {
    return emit_insn(builder, OP_PUSH, AMP, 0, src, 0);
}

/**
 * Emit pop word from stack
 */
uint32_t emit_pop_w(DISModuleBuilder* builder, int32_t dst) {
    return emit_insn(builder, OP_POP, AMP, 0, 0, dst);
}
