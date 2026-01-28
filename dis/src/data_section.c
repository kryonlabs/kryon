/**
 * DIS Data Section Encoder for TaijiOS
 *
 * Stores data items in memory for later writing to .dis file.
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
 * Create a data item and add it to the data section
 */
static DISDataItem* create_data_item(uint32_t size, uint32_t offset) {
    DISDataItem* item = (DISDataItem*)calloc(1, sizeof(DISDataItem));
    if (!item) {
        return NULL;
    }

    item->data = (uint8_t*)calloc(1, size);
    if (!item->data) {
        free(item);
        return NULL;
    }

    item->size = size;
    item->offset = offset;
    return item;
}

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
    uint32_t encoded_size = size;
    if (size > 15) {
        // Type marker (1 byte) + extended length (4 bytes) + data
        encoded_size = 1 + 4 + size;
    } else {
        // Type marker (1 byte) + data
        encoded_size = 1 + size;
    }

    // Add offset encoding (4 bytes)
    encoded_size += 4;

    DISDataItem* item = create_data_item(encoded_size, offset);
    if (!item) {
        return false;
    }

    uint8_t* ptr = item->data;

    // Write type marker
    if (size > 15) {
        *ptr++ = DBYTE(DEFB, 0);
        // Write big-endian length
        *ptr++ = (size >> 24) & 0xFF;
        *ptr++ = (size >> 16) & 0xFF;
        *ptr++ = (size >> 8) & 0xFF;
        *ptr++ = size & 0xFF;
    } else {
        *ptr++ = DBYTE(DEFB, size);
    }

    // Write offset (big-endian)
    *ptr++ = (offset >> 24) & 0xFF;
    *ptr++ = (offset >> 16) & 0xFF;
    *ptr++ = (offset >> 8) & 0xFF;
    *ptr++ = offset & 0xFF;

    // Write byte data
    memcpy(ptr, data, size);

    return dynamic_array_add(builder->data_section, item);
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

    // Type marker (1 byte) + offset (4 bytes) + value (4 bytes)
    DISDataItem* item = create_data_item(9, offset);
    if (!item) {
        return false;
    }

    uint8_t* ptr = item->data;

    // Write type marker (single word)
    *ptr++ = DBYTE(DEFW, 1);

    // Write offset (big-endian)
    *ptr++ = (offset >> 24) & 0xFF;
    *ptr++ = (offset >> 16) & 0xFF;
    *ptr++ = (offset >> 8) & 0xFF;
    *ptr++ = offset & 0xFF;

    // Write value (big-endian)
    *ptr++ = (value >> 24) & 0xFF;
    *ptr++ = (value >> 16) & 0xFF;
    *ptr++ = (value >> 8) & 0xFF;
    *ptr++ = value & 0xFF;

    return dynamic_array_add(builder->data_section, item);
}

/**
 * Emit an array of words to data section
 */
bool emit_data_words(DISModuleBuilder* builder, const int32_t* values, uint32_t offset, uint32_t count) {
    if (!builder || !values || count == 0) {
        return false;
    }

    // Calculate size: type marker + (possibly extended length) + offset + data
    uint32_t size = 1 + 4 + (count * 4);
    if (count > 15) {
        size += 4;  // Extended length
    }

    DISDataItem* item = create_data_item(size, offset);
    if (!item) {
        return false;
    }

    uint8_t* ptr = item->data;

    // Write type marker
    if (count > 15) {
        *ptr++ = DBYTE(DEFW, 0);
        // Write big-endian count
        *ptr++ = (count >> 24) & 0xFF;
        *ptr++ = (count >> 16) & 0xFF;
        *ptr++ = (count >> 8) & 0xFF;
        *ptr++ = count & 0xFF;
    } else {
        *ptr++ = DBYTE(DEFW, count);
    }

    // Write offset (big-endian)
    *ptr++ = (offset >> 24) & 0xFF;
    *ptr++ = (offset >> 16) & 0xFF;
    *ptr++ = (offset >> 8) & 0xFF;
    *ptr++ = offset & 0xFF;

    // Write words
    for (uint32_t i = 0; i < count; i++) {
        *ptr++ = (values[i] >> 24) & 0xFF;
        *ptr++ = (values[i] >> 16) & 0xFF;
        *ptr++ = (values[i] >> 8) & 0xFF;
        *ptr++ = values[i] & 0xFF;
    }

    return dynamic_array_add(builder->data_section, item);
}

/**
 * Emit a long (64-bit) to data section
 */
bool emit_data_long(DISModuleBuilder* builder, int64_t value, uint32_t offset) {
    if (!builder) {
        return false;
    }

    // Type marker (1 byte) + offset (4 bytes) + value (8 bytes)
    DISDataItem* item = create_data_item(13, offset);
    if (!item) {
        return false;
    }

    uint8_t* ptr = item->data;

    // Write type marker (single long)
    *ptr++ = DBYTE(DEFL, 1);

    // Write offset (big-endian)
    *ptr++ = (offset >> 24) & 0xFF;
    *ptr++ = (offset >> 16) & 0xFF;
    *ptr++ = (offset >> 8) & 0xFF;
    *ptr++ = offset & 0xFF;

    // Write value as two 32-bit words (hi, lo) - big-endian
    int32_t hi = (int32_t)(value >> 32);
    int32_t lo = (int32_t)(value & 0xFFFFFFFF);

    *ptr++ = (hi >> 24) & 0xFF;
    *ptr++ = (hi >> 16) & 0xFF;
    *ptr++ = (hi >> 8) & 0xFF;
    *ptr++ = hi & 0xFF;

    *ptr++ = (lo >> 24) & 0xFF;
    *ptr++ = (lo >> 16) & 0xFF;
    *ptr++ = (lo >> 8) & 0xFF;
    *ptr++ = lo & 0xFF;

    return dynamic_array_add(builder->data_section, item);
}

/**
 * Emit a float (64-bit IEEE 754) to data section
 * Note: This assumes host uses IEEE 754 (most do)
 */
bool emit_data_float(DISModuleBuilder* builder, double value, uint32_t offset) {
    if (!builder) {
        return false;
    }

    // Type marker (1 byte) + offset (4 bytes) + value (8 bytes)
    DISDataItem* item = create_data_item(13, offset);
    if (!item) {
        return false;
    }

    uint8_t* ptr = item->data;

    // Write type marker (single float)
    *ptr++ = DBYTE(DEFF, 1);

    // Write offset (big-endian)
    *ptr++ = (offset >> 24) & 0xFF;
    *ptr++ = (offset >> 16) & 0xFF;
    *ptr++ = (offset >> 8) & 0xFF;
    *ptr++ = offset & 0xFF;

    // Write float as raw bytes in big-endian format
    union {
        double d;
        uint64_t u;
    } data;
    data.d = value;

    uint64_t u = data.u;
    *ptr++ = (u >> 56) & 0xFF;
    *ptr++ = (u >> 48) & 0xFF;
    *ptr++ = (u >> 40) & 0xFF;
    *ptr++ = (u >> 32) & 0xFF;
    *ptr++ = (u >> 24) & 0xFF;
    *ptr++ = (u >> 16) & 0xFF;
    *ptr++ = (u >> 8) & 0xFF;
    *ptr++ = u & 0xFF;

    return dynamic_array_add(builder->data_section, item);
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

    // Calculate size: type marker + (possibly extended length) + offset + string data
    uint32_t size = 1 + 4 + len;
    if (len > 15) {
        size += 4;  // Extended length
    }

    DISDataItem* item = create_data_item(size, offset);
    if (!item) {
        return false;
    }

    uint8_t* ptr = item->data;

    // Write type marker
    if (len > 15) {
        *ptr++ = DBYTE(DEFS, 0);
        // Write big-endian length
        *ptr++ = (len >> 24) & 0xFF;
        *ptr++ = (len >> 16) & 0xFF;
        *ptr++ = (len >> 8) & 0xFF;
        *ptr++ = len & 0xFF;
    } else {
        *ptr++ = DBYTE(DEFS, (uint8_t)len);
    }

    // Write offset (big-endian)
    *ptr++ = (offset >> 24) & 0xFF;
    *ptr++ = (offset >> 16) & 0xFF;
    *ptr++ = (offset >> 8) & 0xFF;
    *ptr++ = offset & 0xFF;

    // Write string bytes
    memcpy(ptr, str, len);

    return dynamic_array_add(builder->data_section, item);
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

    // Type marker (1 byte) + offset (4 bytes) + type (4 bytes) + length (4 bytes)
    DISDataItem* item = create_data_item(13, offset);
    if (!item) {
        return false;
    }

    uint8_t* ptr = item->data;

    // Write type marker
    *ptr++ = DBYTE(DEFA, 0);

    // Write offset (big-endian)
    *ptr++ = (offset >> 24) & 0xFF;
    *ptr++ = (offset >> 16) & 0xFF;
    *ptr++ = (offset >> 8) & 0xFF;
    *ptr++ = offset & 0xFF;

    // Write array descriptor
    // Note: Arrays in DIS are complex structures, this is a simplified version
    *ptr++ = (type_idx >> 24) & 0xFF;
    *ptr++ = (type_idx >> 16) & 0xFF;
    *ptr++ = (type_idx >> 8) & 0xFF;
    *ptr++ = type_idx & 0xFF;

    *ptr++ = (len >> 24) & 0xFF;
    *ptr++ = (len >> 16) & 0xFF;
    *ptr++ = (len >> 8) & 0xFF;
    *ptr++ = len & 0xFF;

    return dynamic_array_add(builder->data_section, item);
}

/**
 * End the data section with a zero byte
 */
bool emit_data_end(DISModuleBuilder* builder) {
    if (!builder) {
        return false;
    }

    // Add terminating zero as a data item
    DISDataItem* item = create_data_item(1, builder->global_offset);
    if (!item) {
        return false;
    }

    item->data[0] = 0;

    return dynamic_array_add(builder->data_section, item);
}
