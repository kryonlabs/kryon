/**
 * DIS Instruction Emitter for TaijiOS
 *
 * Emits DIS VM instructions matching TaijiOS isa.h format.
 * Based on /home/wao/Projects/TaijiOS/include/isa.h
 */

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include "internal.h"

// ============================================================================
// Opcode Definitions (from TaijiOS include/isa.h)
// ============================================================================

// Control flow
#define INOP    0
#define IALT    1
#define INBALT  2
#define IGOTO   3
#define ICALL   4
#define IFRAME  5
#define ISPAWN  6
#define IRUNT   7
#define ILOAD   8
#define IMCALL  9
#define IMSPAWN 10
#define IMFRAME 11
#define IRET    12
#define IJMP    13
#define ICASE   14
#define IEXIT   15

// Object operations
#define INEW    16
#define INEWA   17
#define INEWCB  18
#define INEWCW  19
#define INEWCF  20
#define INEWCP  21
#define INEWCM  22
#define INEWCMP 23
#define ISEND   24
#define IRECV   25

// Array constants
#define ICONSB  26
#define ICONSW  27
#define ICONSP  28
#define ICONSF  29
#define ICONSM  30
#define ICONSMP 31

// Array operations
#define IHEADB  32
#define IHEADW  33
#define IHEADP  34
#define IHEADF  35
#define IHEADM  36
#define IHEADMP 37
#define ITAIL   38
#define ILEA    39
#define IINDX   40

// Move operations
#define IMOVP   41
#define IMOVM   42
#define IMOVMP  43
#define IMOVB   44
#define IMOVW   45
#define IMOVF   46

// Type conversions
#define ICVTBW  47
#define ICVTWB  48
#define ICVTFW  49
#define ICVTWF  50
#define ICVTCA  51
#define ICVTAC  52
#define ICVTWC  53
#define ICVTCW  54
#define ICVTFC  55
#define ICVTCF  56

// Arithmetic (byte)
#define IADDB   57
#define ISUBB   58
#define IMULB   59
#define IDIVB   60
#define IMODB   61
#define IANDB   62
#define IORB    63
#define IXORB   64
#define ISHLB   65
#define ISHRB   66

// Arithmetic (word)
#define IADDW   67
#define ISUBW   68
#define IMULW   69
#define IDIVW   70
#define IMODW   71
#define IANDW   72
#define IORW    73
#define IXORW   74
#define ISHLW   75
#define ISHRW   76

// Arithmetic (float)
#define IADDF   77
#define ISUBF   78
#define IMULF   79
#define IDIVF   80

// Arithmetic (big)
#define IMOVL   81
#define IADDL   82
#define ISUBL   83
#define IDIVL   84
#define IMODL   85
#define IMULL   86
#define IANDL   87
#define IORL    88
#define IXORL   89
#define ISHLL   90
#define ISHRL   91

// Comparison
#define IBEQB   92
#define IBNEB   93
#define IBLTB   94
#define IBLEB   95
#define IBGTB   96
#define IBGEB   97
#define IBEQW   98
#define IBNEW   99
#define IBLTW   100
#define IBLEW   101
#define IBGTW   102
#define IBGEW   103
#define IBEQF   104
#define IBNEF   105
#define IBLTF   106
#define IBLEF   107
#define IBGTF   108
#define IBGEF   109
#define IBEQC   110
#define IBNEC   111
#define IBLTC   112
#define IBLEC   113
#define IBGTC   114
#define IBGEC   115
#define IBNEL   116
#define IBLTL   117
#define IBLEL   118
#define IBGTL   119
#define IBGEL   120
#define IBEQL   121

// Addressing modes (from TaijiOS isa.h)
#define AMP     0x00  // Offset from MP (module pointer / global)
#define AFP     0x01  // Offset from FP (frame pointer / local)
#define AIMM    0x02  // Immediate value
#define AXXX    0x03  // No operand
#define AIND    0x04  // Indirect
#define AMASK   0x07  // Mask for addressing mode
#define AOFF    0x08  // Offset flag
#define AVAL    0x10  // Value flag

#define ARM     0xC0  // Mask for middle operand addressing
#define AXNON   0x00  // No middle operand
#define AXIMM   0x40  // Immediate middle operand
#define AXINF   0x80  // Frame offset middle operand
#define AXINM   0xC0  // Module offset middle operand

// Macros for encoding addressing modes
#define SRC(x)  ((x) << 3)
#define DST(x)  ((x) << 0)

// ============================================================================
// Instruction Encoding (TaijiOS format)
// ============================================================================
/**
 * TaijiOS DIS Instruction format:
 * Based on /home/wao/Projects/TaijiOS/libinterp/load.c
 *
 * Each instruction is variable-length encoded:
 * - opcode (1 byte)
 * - address mode (1 byte)
 * - operands (variable-length encoded)
 *
 * Operands use variable-length encoding:
 * - 0x00-0x3F: single byte, positive 0-63
 * - 0x40-0x7F: single byte, negative -64 to -1
 * - 0x80-0xBF: two bytes
 * - 0xC0-0xFF: four bytes
 */

/**
 * Emit a complete instruction
 * This is the main instruction emission function
 *
 * @param builder Module builder
 * @param opcode Opcode (from TaijiOS isa.h)
 * @param addr_mode Addressing mode byte (combination of SRC|DST|middle)
 * @param middle Middle operand (if present in addressing mode)
 * @param src Source operand
 * @param dst Destination operand
 * @return PC of emitted instruction
 */
uint32_t emit_insn(DISModuleBuilder* builder,
                   uint8_t opcode,
                   uint8_t addr_mode,
                   int32_t middle,
                   int32_t src,
                   int32_t dst) {
    if (!builder) {
        return -1;
    }

    // Calculate instruction size first
    uint32_t insn_size = 2;  // opcode + addr_mode

    // Middle operand (if present)
    if ((addr_mode & ARM) == AXIMM || (addr_mode & ARM) == AXINF || (addr_mode & ARM) == AXINM) {
        insn_size += 4;  // Middle operand is always 4 bytes
    }

    // Source operand (variable-length encoded)
    int src_size = 1;
    if ((addr_mode & SRC(AMASK)) != 0x00 &&  // Not AXXX
        ((addr_mode & SRC(AMASK)) == SRC(AFP) ||
         (addr_mode & SRC(AMASK)) == SRC(AMP) ||
         (addr_mode & SRC(AMASK)) == SRC(AIMM) ||
         (addr_mode & SRC(AMASK)) == SRC(AIND|AFP) ||
         (addr_mode & SRC(AMASK)) == SRC(AIND|AMP))) {
        // For now, use 4-byte encoding for all non-XXX operands
        src_size = 4;
        insn_size += src_size;
    }

    // Destination operand (variable-length encoded)
    int dst_size = 1;
    if ((addr_mode & DST(AMASK)) != 0x00 &&  // Not AXXX
        ((addr_mode & DST(AMASK)) == DST(AFP) ||
         (addr_mode & DST(AMASK)) == DST(AMP) ||
         (addr_mode & DST(AMASK)) == DST(AIMM) ||
         (addr_mode & DST(AMASK)) == DST(AIND|AFP) ||
         (addr_mode & DST(AMASK)) == DST(AIND|AMP))) {
        dst_size = 4;
        insn_size += dst_size;
    }

    // Allocate instruction data
    uint8_t* insn_data = (uint8_t*)malloc(4 + insn_size);
    if (!insn_data) {
        return -1;
    }

    // Write size prefix (4 bytes, BIG-ENDIAN)
    insn_data[0] = (insn_size >> 24) & 0xFF;
    insn_data[1] = (insn_size >> 16) & 0xFF;
    insn_data[2] = (insn_size >> 8) & 0xFF;
    insn_data[3] = insn_size & 0xFF;

    // Write opcode
    insn_data[4] = opcode;

    // Write address mode
    insn_data[5] = addr_mode;

    // Write operands
    uint32_t offset = 6;

    // Middle operand
    if ((addr_mode & ARM) == AXIMM || (addr_mode & ARM) == AXINF || (addr_mode & ARM) == AXINM) {
        // Write 32-bit big-endian
        insn_data[offset++] = (middle >> 24) & 0xFF;
        insn_data[offset++] = (middle >> 16) & 0xFF;
        insn_data[offset++] = (middle >> 8) & 0xFF;
        insn_data[offset++] = middle & 0xFF;
    }

    // Source operand
    // Only encode if source mode is not 0x00 (AXXX)
    if ((addr_mode & SRC(AMASK)) != 0x00) {
        if ((addr_mode & SRC(AMASK)) == SRC(AIMM)) {
            // Immediate - big-endian
            insn_data[offset++] = (src >> 24) & 0xFF;
            insn_data[offset++] = (src >> 16) & 0xFF;
            insn_data[offset++] = (src >> 8) & 0xFF;
            insn_data[offset++] = src & 0xFF;
        } else {
            // Register offset - big-endian
            insn_data[offset++] = (src >> 24) & 0xFF;
            insn_data[offset++] = (src >> 16) & 0xFF;
            insn_data[offset++] = (src >> 8) & 0xFF;
            insn_data[offset++] = src & 0xFF;
        }
    }

    // Destination operand
    // Only encode if destination mode is not 0x00 (AXXX)
    if ((addr_mode & DST(AMASK)) != 0x00) {
        // Register offset or immediate - big-endian
        insn_data[offset++] = (dst >> 24) & 0xFF;
        insn_data[offset++] = (dst >> 16) & 0xFF;
        insn_data[offset++] = (dst >> 8) & 0xFF;
        insn_data[offset++] = dst & 0xFF;
    }

    // Add to code section
    if (!dynamic_array_add(builder->code_section, insn_data)) {
        free(insn_data);
        return -1;
    }

    // Update PC
    uint32_t pc = builder->current_pc;
    builder->current_pc += insn_size;

    return pc;
}

// ============================================================================
// Move Operations
// ============================================================================

/**
 * Emit move word: dst = src
 */
uint32_t emit_mov_w(DISModuleBuilder* builder, int32_t src, int32_t dst) {
    return emit_insn(builder, IMOVW, SRC(AMP) | DST(AMP), 0, src, dst);
}

/**
 * Emit move pointer: dst = src
 */
uint32_t emit_mov_p(DISModuleBuilder* builder, int32_t src, int32_t dst) {
    return emit_insn(builder, IMOVP, SRC(AMP) | DST(AMP), 0, src, dst);
}

/**
 * Emit move immediate word: dst = immediate
 */
uint32_t emit_mov_imm_w(DISModuleBuilder* builder, int32_t imm, int32_t dst) {
    return emit_insn(builder, IMOVW, SRC(AIMM) | DST(AMP), 0, imm, dst);
}

// ============================================================================
// Arithmetic Operations
// ============================================================================

/**
 * Emit add word: dst = src1 + src2
 */
uint32_t emit_add_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst) {
    return emit_insn(builder, IADDW, SRC(AMP) | DST(AMP), AXINM, src1, dst);
}

/**
 * Emit subtract word: dst = src1 - src2
 */
uint32_t emit_sub_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst) {
    return emit_insn(builder, ISUBW, SRC(AMP) | DST(AMP), AXINM, src1, dst);
}

/**
 * Emit multiply word: dst = src1 * src2
 */
uint32_t emit_mul_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst) {
    return emit_insn(builder, IMULW, SRC(AMP) | DST(AMP), AXINM, src1, dst);
}

/**
 * Emit divide word: dst = src1 / src2
 */
uint32_t emit_div_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst) {
    return emit_insn(builder, IDIVW, SRC(AMP) | DST(AMP), AXINM, src1, dst);
}

// ============================================================================
// Control Flow
// ============================================================================

/**
 * Emit call instruction
 * @param builder Module builder
 * @param target_pc Target function PC
 * @return PC of call instruction
 */
uint32_t emit_call(DISModuleBuilder* builder, uint32_t target_pc) {
    return emit_insn(builder, ICALL, SRC(AIMM), 0, 0, target_pc);
}

/**
 * Emit return instruction
 */
uint32_t emit_ret(DISModuleBuilder* builder) {
    return emit_insn(builder, IRET, AXXX, 0, 0, 0);
}

/**
 * Emit unconditional jump
 * @param builder Module builder
 * @param target_pc Target PC
 * @return PC of jump instruction
 */
uint32_t emit_jmp(DISModuleBuilder* builder, uint32_t target_pc) {
    return emit_insn(builder, IGOTO, SRC(AIMM), 0, 0, target_pc);
}

/**
 * Emit branch if equal (word)
 */
uint32_t emit_branch_eq_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, uint32_t target_pc) {
    return emit_insn(builder, IBEQW, SRC(AMP) | DST(AIMM), AXINM, src1, target_pc);
}

/**
 * Emit branch if not equal (word)
 */
uint32_t emit_branch_ne_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, uint32_t target_pc) {
    return emit_insn(builder, IBNEW, SRC(AMP) | DST(AIMM), AXINM, src1, target_pc);
}

/**
 * Emit branch if less than (word)
 */
uint32_t emit_branch_lt_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, uint32_t target_pc) {
    return emit_insn(builder, IBLTW, SRC(AMP) | DST(AIMM), AXINM, src1, target_pc);
}

/**
 * Emit branch if greater than (word)
 */
uint32_t emit_branch_gt_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, uint32_t target_pc) {
    return emit_insn(builder, IBGTW, SRC(AMP) | DST(AIMM), AXINM, src1, target_pc);
}

// ============================================================================
// Object Allocation
// ============================================================================

/**
 * Emit new object allocation
 * @param builder Module builder
 * @param type_idx Type index to allocate
 * @param dst Destination register for object pointer
 * @return PC of instruction
 */
uint32_t emit_new(DISModuleBuilder* builder, uint32_t type_idx, int32_t dst) {
    return emit_insn(builder, INEW, SRC(AIMM) | DST(AMP), 0, type_idx, dst);
}

/**
 * Emit new array allocation
 * @param builder Module builder
 * @param count Number of elements
 * @param type_idx Type index for array elements
 * @param dst Destination register for array pointer
 * @return PC of instruction
 */
uint32_t emit_newa(DISModuleBuilder* builder, int32_t count, uint32_t type_idx, int32_t dst) {
    return emit_insn(builder, INEWA, SRC(AIMM) | DST(AMP), type_idx, count, dst);
}

/**
 * Emit frame allocation (stack frame with type)
 * @param builder Module builder
 * @param type_idx Type index for frame
 * @param dst Destination register for frame pointer
 * @return PC of instruction
 */
uint32_t emit_frame(DISModuleBuilder* builder, uint32_t type_idx, int32_t dst) {
    return emit_insn(builder, IFRAME, SRC(AIMM) | DST(AMP), 0, type_idx, dst);
}

// ============================================================================
// Exit Instruction
// ============================================================================

/**
 * Emit exit with status code
 */
uint32_t emit_exit(DISModuleBuilder* builder, int32_t code) {
    return emit_insn(builder, IEXIT, SRC(AIMM), 0, 0, code);
}

// ============================================================================
// Module Loading and Import Instructions
// ============================================================================

/**
 * Emit load module instruction
 * @param builder Module builder
 * @param module_name Name of module to load
 * @return PC of load instruction
 */
uint32_t emit_load(DISModuleBuilder* builder, const char* module_name) {
    if (!builder || !module_name) {
        return -1;
    }

    // Add to import section
    uint32_t offset = builder->current_pc;
    add_import(builder, module_name, "$load", offset);

    // Emit ILOAD instruction
    return emit_insn(builder, ILOAD, AXXX, 0, 0, 0);
}

/**
 * Emit call to imported function
 * @param builder Module builder
 * @param module Module name
 * @param func Function name
 * @return PC of call instruction
 */
uint32_t emit_call_import(DISModuleBuilder* builder, const char* module, const char* func) {
    if (!builder || !module || !func) {
        return -1;
    }

    // Add to import section
    uint32_t offset = builder->current_pc;
    add_import(builder, module, func, offset);

    // Emit ICALL instruction (will be patched by linker)
    return emit_insn(builder, ICALL, SRC(AIMM), 0, 0, 0);
}

// ============================================================================
// Channel Operations (for Event Loop)
// ============================================================================

/**
 * Emit alt instruction (start of alt statement)
 */
uint32_t emit_alt(DISModuleBuilder* builder) {
    if (!builder) {
        return -1;
    }

    return emit_insn(builder, IALT, AXXX, 0, 0, 0);
}

/**
 * Emit channel receive instruction
 */
uint32_t emit_chan_recv(DISModuleBuilder* builder, int32_t channel_reg, int32_t dst) {
    if (!builder) {
        return -1;
    }

    return emit_insn(builder, IRECV, SRC(AMP) | DST(AMP), 0, channel_reg, dst);
}

// ============================================================================
// Method Calls (for libdraw API)
// ============================================================================

/**
 * Emit method call instruction
 */
uint32_t emit_method_call(DISModuleBuilder* builder, uint32_t method_offset, int32_t argc) {
    if (!builder) {
        return -1;
    }

    return emit_insn(builder, IMCALL, AXIMM | SRC(AMP), method_offset, 0, argc);
}

// ============================================================================
// Frame Operations
// ============================================================================

/**
 * Emit allocate stack frame
 */
uint32_t emit_enter_frame(DISModuleBuilder* builder, uint32_t size) {
    if (!builder) {
        return -1;
    }

    return emit_insn(builder, IFRAME, SRC(AIMM) | DST(AMP), 0, size, 0);
}

/**
 * Emit leave frame (return)
 */
uint32_t emit_leave_frame(DISModuleBuilder* builder) {
    return emit_ret(builder);
}

// ============================================================================
// Address Loading
// ============================================================================

/**
 * Emit move to address register
 * Loads an address into a pointer register
 */
uint32_t emit_mov_address(DISModuleBuilder* builder, uint32_t addr, int32_t dst) {
    if (!builder) {
        return -1;
    }

    return emit_insn(builder, IMOVP, SRC(AIMM) | DST(AMP), 0, addr, dst);
}

// ============================================================================
// String Loading
// ============================================================================

/**
 * Load string address into register
 * Adds string to data section and loads its address
 */
uint32_t emit_load_string_address(DISModuleBuilder* builder, const char* str, int32_t dst) {
    if (!builder || !str) {
        return -1;
    }

    // Add string to data section
    uint32_t offset = builder->global_offset;
    if (!emit_data_string(builder, str, offset)) {
        return -1;
    }

    // Load address into register using move immediate with address
    return emit_mov_address(builder, offset, dst);
}

/**
 * Add string to data section and return its offset
 */
bool emit_string_to_data(DISModuleBuilder* builder, const char* str, uint32_t* offset) {
    if (!builder || !str || !offset) {
        return false;
    }

    *offset = builder->global_offset;
    return emit_data_string(builder, str, *offset);
}
