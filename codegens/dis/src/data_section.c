/**
 * DIS Data Section Encoder for TaijiOS
 *
 * Encodes data items in TaijiOS .dis format.
 * Based on /home/wao/Projects/TaijiOS/libinterp/load.c
 */

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "internal.h"

// Data encoding types (from TaijiOS isa.h)
#define DEFZ    0   // Zero/empty
#define DEFB    1   // Byte
#define DEFW    2   // Word (32-bit)
#define DEFS    3   // UTF-8 string
#define DEFF    4   // Float (64-bit)
#define DEFA    5   // Array
#define DIND    6   // Set index
#define DAPOP   7   // Restore address register
#define DEFL    8   // Long (64-bit)
#define DEFSS   9   // String share (not used)

#define DTYPE(x)    ((x) >> 4)
#define DBYTE(x, l) (((x) << 4) | (l))
#define DMAX        (1 << 4)
#define DLEN(x)     ((x) & (DMAX - 1))

/**
 * Emit a byte array to data section
 * @param builder Module builder
 * @param data Byte data
 * @param offset Offset in module data (MP)
 * @param size Number of bytes
 * @return true on success
 */
bool emit_data_bytes(DISModuleBuilder* builder, const uint8_t* data, uint32_t offset, uint32_t size) {
    if (!builder || !data || size == 0) {
        return false;
    }

    // For large arrays, use length encoding
    if (size > 15) {
        // Write type marker with extended length
        fputc(DBYTE(DEFB, 0), module_builder_get_output_file(builder));
        write_bigendian_32(module_builder_get_output_file(builder), size);
    } else {
        // Write type marker with inline length
        fputc(DBYTE(DEFB, size), module_builder_get_output_file(builder));
    }

    // Write offset
    write_bigendian_32(module_builder_get_output_file(builder), offset);

    // Write byte data
    fwrite(data, 1, size, module_builder_get_output_file(builder));

    return true;
}

/**
 * Emit a single byte to data section
 */
bool emit_data_byte(DISModuleBuilder* builder, uint8_t value, uint32_t offset) {
    return emit_data_bytes(builder, &value, offset, 1);
}

/**
 * Emit a word (32-bit) to data section
 * @param builder Module builder
 * @param value Word value (little-endian in host, big-endian in file)
 * @param offset Offset in module data
 */
bool emit_data_word(DISModuleBuilder* builder, int32_t value, uint32_t offset) {
    if (!builder) {
        return false;
    }

    // Write type marker (single word)
    fputc(DBYTE(DEFW, 1), module_builder_get_output_file(builder));

    // Write offset
    write_bigendian_32(module_builder_get_output_file(builder), offset);

    // Write value (big-endian)
    write_bigendian_32(module_builder_get_output_file(builder), (uint32_t)value);

    return true;
}

/**
 * Emit an array of words to data section
 */
bool emit_data_words(DISModuleBuilder* builder, const int32_t* values, uint32_t offset, uint32_t count) {
    if (!builder || !values || count == 0) {
        return false;
    }

    // For large arrays, use extended length
    if (count > 15) {
        fputc(DBYTE(DEFW, 0), module_builder_get_output_file(builder));
        write_bigendian_32(module_builder_get_output_file(builder), count);
    } else {
        fputc(DBYTE(DEFW, count), module_builder_get_output_file(builder));
    }

    // Write offset
    write_bigendian_32(module_builder_get_output_file(builder), offset);

    // Write words
    for (uint32_t i = 0; i < count; i++) {
        write_bigendian_32(module_builder_get_output_file(builder), (uint32_t)values[i]);
    }

    return true;
}

/**
 * Emit a long (64-bit) to data section
 */
bool emit_data_long(DISModuleBuilder* builder, int64_t value, uint32_t offset) {
    if (!builder) {
        return false;
    }

    // Write type marker (single long)
    fputc(DBYTE(DEFL, 1), module_builder_get_output_file(builder));

    // Write offset
    write_bigendian_32(module_builder_get_output_file(builder), offset);

    // Write value as two 32-bit words (hi, lo)
    int32_t hi = (int32_t)(value >> 32);
    int32_t lo = (int32_t)(value & 0xFFFFFFFF);
    write_bigendian_32(module_builder_get_output_file(builder), (uint32_t)hi);
    write_bigendian_32(module_builder_get_output_file(builder), (uint32_t)lo);

    return true;
}

/**
 * Emit a float (64-bit IEEE 754) to data section
 * Note: This assumes host uses IEEE 754 (most do)
 */
bool emit_data_float(DISModuleBuilder* builder, double value, uint32_t offset) {
    if (!builder) {
        return false;
    }

    // Write type marker (single float)
    fputc(DBYTE(DEFF, 1), module_builder_get_output_file(builder));

    // Write offset
    write_bigendian_32(module_builder_get_output_file(builder), offset);

    // Write float as raw bytes in big-endian format
    union {
        double d;
        uint64_t u;
    } data;
    data.d = value;
    write_bigendian_64(module_builder_get_output_file(builder), data.u);

    return true;
}

/**
 * Emit a string to data section
 * @param builder Module builder
 * @param str Null-terminated string
 * @param offset Offset in module data
 */
bool emit_data_string(DISModuleBuilder* builder, const char* str, uint32_t offset) {
    if (!builder || !str) {
        return false;
    }

    size_t len = strlen(str);

    // For long strings, use extended length
    if (len > 15) {
        fputc(DBYTE(DEFS, 0), module_builder_get_output_file(builder));
        write_bigendian_32(module_builder_get_output_file(builder), (uint32_t)len);
    } else {
        fputc(DBYTE(DEFS, (uint8_t)len), module_builder_get_output_file(builder));
    }

    // Write offset
    write_bigendian_32(module_builder_get_output_file(builder), offset);

    // Write string bytes
    fwrite(str, 1, len, module_builder_get_output_file(builder));

    return true;
}

/**
 * Emit an array to data section
 * @param builder Module builder
 * @param type_idx Type descriptor index for array elements
 * @param len Number of elements
 * @param offset Offset in module data
 */
bool emit_data_array(DISModuleBuilder* builder, uint32_t type_idx, uint32_t len, uint32_t offset) {
    if (!builder) {
        return false;
    }

    // Write type marker
    fputc(DBYTE(DEFA, 0), module_builder_get_output_file(builder));

    // Write offset
    write_bigendian_32(module_builder_get_output_file(builder), offset);

    // Write array descriptor
    // Note: Arrays in DIS are complex structures, this is a simplified version
    write_bigendian_32(module_builder_get_output_file(builder), type_idx);
    write_bigendian_32(module_builder_get_output_file(builder), len);

    return true;
}

/**
 * End the data section with a zero byte
 */
bool emit_data_end(DISModuleBuilder* builder) {
    if (!builder) {
        return false;
    }

    // Write terminating zero
    fputc(0, module_builder_get_output_file(builder));

    return true;
}
