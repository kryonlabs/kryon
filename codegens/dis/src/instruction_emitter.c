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
 *
 * Byte 0: Opcode
 * Byte 1: Addressing mode (src|dst with ARM middle bits)
 * Bytes 2+: Variable-length encoded operands
 *
 * Operands are encoded with variable-length encoding:
 * - 0x00-0x3F: Single byte, value 0-63
 * - 0x40-0x7F: Single byte, value -64 to -1 (sign-extended)
 * - 0x80-0xBF: Two bytes, value 0-16383 or signed
 * - 0xC0-0xFF: Four bytes, full 32-bit value
 */

typedef struct {
    uint8_t opcode;
    uint8_t address_mode;
    int32_t middle_operand;
    int32_t source_operand;
    int32_t dest_operand;
} DISInstruction;

/**
 * Encode a 32-bit operand using TaijiOS variable-length encoding
 * Returns the number of bytes written (1, 2, or 4)
 */
static int encode_operand(char* buf, int32_t value) {
    // Try to fit in 1 byte (0x00-0x3F for positive, 0x40-0x7F for negative)
    if (value >= 0 && value <= 0x3F) {
        buf[0] = (char)value;
        return 1;
    }
    if (value >= -64 && value < 0) {
        buf[0] = (char)(value | 0x80);
        return 1;
    }

    // Try to fit in 2 bytes (0x80-0xBF range)
    if ((value >= -16384 && value < 16384) && value != -32768) {
        if (value < 0) {
            buf[0] = (char)(((value >> 8) & 0x3F) | 0xA0);
        } else {
            buf[0] = (char)(((value >> 8) & 0x3F) | 0x80);
        }
        buf[1] = (char)(value & 0xFF);
        return 2;
    }

    // Use 4 bytes (0xC0-0xFF range)
    if (value < 0) {
        buf[0] = (char)(((value >> 24) & 0x3F) | 0xE0);
    } else {
        buf[0] = (char)(((value >> 24) & 0x3F) | 0xC0);
    }
    buf[1] = (char)((value >> 16) & 0xFF);
    buf[2] = (char)((value >> 8) & 0xFF);
    buf[3] = (char)(value & 0xFF);
    return 4;
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

    // Allocate buffer for encoded instruction
    char buffer[256];  // Maximum instruction size
    int offset = 0;

    // Write opcode and addressing mode
    buffer[offset++] = (char)insn->opcode;
    buffer[offset++] = (char)insn->address_mode;

    // Write middle operand (if present)
    switch (insn->address_mode & ARM) {
    case AXIMM:
    case AXINF:
    case AXINM:
        offset += encode_operand(buffer + offset, insn->middle_operand);
        break;
    }

    // Write source operand (if present)
    switch (SRC(AMASK) & insn->address_mode) {
    case SRC(AFP):
    case SRC(AMP):
    case SRC(AIMM):
        offset += encode_operand(buffer + offset, insn->source_operand);
        break;
    case SRC(AIND|AFP):
    case SRC(AIND|AMP):
        // Indirect: write frame and offset
        offset += encode_operand(buffer + offset, insn->source_operand);
        offset += encode_operand(buffer + offset, insn->middle_operand);  // reuse middle for offset
        break;
    }

    // Write destination operand (if present)
    switch (DST(AMASK) & insn->address_mode) {
    case DST(AFP):
    case DST(AMP):
        offset += encode_operand(buffer + offset, insn->dest_operand);
        break;
    case DST(AIMM):
        // Immediate destination (for branches)
        offset += encode_operand(buffer + offset, insn->dest_operand);
        break;
    case DST(AIND|AFP):
    case DST(AIND|AMP):
        // Indirect: write frame and offset
        offset += encode_operand(buffer + offset, insn->dest_operand);
        offset += encode_operand(buffer + offset, insn->middle_operand);
        break;
    }

    // TODO: Add to code section
    // For now, just increment PC
    uint32_t pc = builder->current_pc;
    builder->current_pc++;

    return pc;
}

/**
 * Emit instruction with simplified interface
 *
 * @param builder Module builder
 * @param opcode Opcode (from TaijiOS isa.h)
 * @param addr_mode Addressing mode (AMP, AFP, AIMM, AXXX, or combinations)
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
    return emit_insn(builder, IADDW, SRC(AMP) | DST(AMP), 0, src1, dst);
}

/**
 * Emit subtract word: dst = src1 - src2
 */
uint32_t emit_sub_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst) {
    return emit_insn(builder, ISUBW, SRC(AMP) | DST(AMP), 0, src1, dst);
}

/**
 * Emit multiply word: dst = src1 * src2
 */
uint32_t emit_mul_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst) {
    return emit_insn(builder, IMULW, SRC(AMP) | DST(AMP), 0, src1, dst);
}

/**
 * Emit divide word: dst = src1 / src2
 */
uint32_t emit_div_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst) {
    return emit_insn(builder, IDIVW, SRC(AMP) | DST(AMP), 0, src1, dst);
}

// ============================================================================
// Arithmetic Operations (Float)
// ============================================================================

/**
 * Emit add float: dst = src1 + src2
 */
uint32_t emit_add_f(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst) {
    return emit_insn(builder, IADDF, SRC(AMP) | DST(AMP), 0, src1, dst);
}

/**
 * Emit subtract float: dst = src1 - src2
 */
uint32_t emit_sub_f(DISModuleBuilder* builder, int32_t src1, int32_t src2, int32_t dst) {
    return emit_insn(builder, ISUBF, SRC(AMP) | DST(AMP), 0, src1, dst);
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
 * @param builder Module builder
 * @param src1 First operand
 * @param src2 Second operand
 * @param target_pc Target PC if condition true
 * @return PC of branch instruction
 */
uint32_t emit_branch_eq_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, uint32_t target_pc) {
    return emit_insn(builder, IBEQW, SRC(AMP) | DST(AIMM), 0, src1, target_pc);
}

/**
 * Emit branch if not equal (word)
 */
uint32_t emit_branch_ne_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, uint32_t target_pc) {
    return emit_insn(builder, IBNEW, SRC(AMP) | DST(AIMM), 0, src1, target_pc);
}

/**
 * Emit branch if less than (word)
 */
uint32_t emit_branch_lt_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, uint32_t target_pc) {
    return emit_insn(builder, IBLTW, SRC(AMP) | DST(AIMM), 0, src1, target_pc);
}

/**
 * Emit branch if greater than (word)
 */
uint32_t emit_branch_gt_w(DISModuleBuilder* builder, int32_t src1, int32_t src2, uint32_t target_pc) {
    return emit_insn(builder, IBGTW, SRC(AMP) | DST(AIMM), 0, src1, target_pc);
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
    return emit_insn(builder, INEW, SRC(AIMM) | DST(AMP), 0, type_idx, dst);
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
    return emit_insn(builder, INEWA, AXIMM | SRC(AMP) | DST(AMP), type_idx, count, dst);
}

/**
 * Emit allocate stack frame
 * @param builder Module builder
 * @param type_idx Frame type index
 * @param dst Destination for frame pointer
 * @return PC of frame instruction
 */
uint32_t emit_frame(DISModuleBuilder* builder, uint32_t type_idx, int32_t dst) {
    return emit_insn(builder, IFRAME, SRC(AIMM) | DST(AMP), 0, type_idx, dst);
}

// ============================================================================
// Comparison Operations
// ============================================================================

/**
 * Emit compare words (sets flags for subsequent branches)
 * @param builder Module builder
 * @param src1 First operand
 * @param src2 Second operand
 * @return PC of cmp instruction
 */
uint32_t emit_cmp_w(DISModuleBuilder* builder, int32_t src1, int32_t src2) {
    // DIS doesn't have a separate compare - use branches directly
    // This is a placeholder for compatibility
    return emit_insn(builder, INOP, AXXX, 0, 0, 0);
}

/**
 * Emit compare floats (sets flags)
 */
uint32_t emit_cmp_f(DISModuleBuilder* builder, int32_t src1, int32_t src2) {
    // DIS doesn't have a separate compare - use branches directly
    return emit_insn(builder, INOP, AXXX, 0, 0, 0);
}

// ============================================================================
// Type Conversions
// ============================================================================

/**
 * Emit convert float to word
 */
uint32_t emit_cvt_f_w(DISModuleBuilder* builder, int32_t src, int32_t dst) {
    return emit_insn(builder, ICVTFW, SRC(AMP) | DST(AMP), 0, src, dst);
}

/**
 * Emit convert word to float
 */
uint32_t emit_cvt_w_f(DISModuleBuilder* builder, int32_t src, int32_t dst) {
    return emit_insn(builder, ICVTWF, SRC(AMP) | DST(AMP), 0, src, dst);
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
