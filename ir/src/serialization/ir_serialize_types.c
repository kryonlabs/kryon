/**
 * @file ir_serialize_types.c
 * @brief Type-specific binary serialization functions
 */

#define _GNU_SOURCE
#include "ir_serialize_types.h"
#include "ir_log.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Write Functions
// ============================================================================

bool ir_write_uint8(IRBuffer* buffer, uint8_t value) {
    return ir_buffer_write(buffer, &value, sizeof(uint8_t));
}

bool ir_write_uint16(IRBuffer* buffer, uint16_t value) {
    return ir_buffer_write(buffer, &value, sizeof(uint16_t));
}

bool ir_write_uint32(IRBuffer* buffer, uint32_t value) {
    return ir_buffer_write(buffer, &value, sizeof(uint32_t));
}

bool ir_write_uint64(IRBuffer* buffer, uint64_t value) {
    return ir_buffer_write(buffer, &value, sizeof(uint64_t));
}

bool ir_write_int32(IRBuffer* buffer, int32_t value) {
    return ir_buffer_write(buffer, &value, sizeof(int32_t));
}

bool ir_write_int64(IRBuffer* buffer, int64_t value) {
    return ir_buffer_write(buffer, &value, sizeof(int64_t));
}

bool ir_write_float32(IRBuffer* buffer, float value) {
    return ir_buffer_write(buffer, &value, sizeof(float));
}

bool ir_write_float64(IRBuffer* buffer, double value) {
    return ir_buffer_write(buffer, &value, sizeof(double));
}

bool ir_write_string(IRBuffer* buffer, const char* str) {
    if (!buffer) return false;

    if (!str) {
        return ir_write_uint32(buffer, 0);
    }

    uint32_t length = strlen(str) + 1;  // Include null terminator
    if (!ir_write_uint32(buffer, length)) {
        return false;
    }

    return ir_buffer_write(buffer, str, length);
}

bool ir_write_bytes(IRBuffer* buffer, const void* data, size_t size) {
    return ir_buffer_write(buffer, data, size);
}

// ============================================================================
// Read Functions
// ============================================================================

bool ir_read_uint8(IRBuffer* buffer, uint8_t* value) {
    return ir_buffer_read(buffer, value, sizeof(uint8_t));
}

bool ir_read_uint16(IRBuffer* buffer, uint16_t* value) {
    return ir_buffer_read(buffer, value, sizeof(uint16_t));
}

bool ir_read_uint32(IRBuffer* buffer, uint32_t* value) {
    return ir_buffer_read(buffer, value, sizeof(uint32_t));
}

bool ir_read_uint64(IRBuffer* buffer, uint64_t* value) {
    return ir_buffer_read(buffer, value, sizeof(uint64_t));
}

bool ir_read_int32(IRBuffer* buffer, int32_t* value) {
    return ir_buffer_read(buffer, value, sizeof(int32_t));
}

bool ir_read_int64(IRBuffer* buffer, int64_t* value) {
    return ir_buffer_read(buffer, value, sizeof(int64_t));
}

bool ir_read_float32(IRBuffer* buffer, float* value) {
    return ir_buffer_read(buffer, value, sizeof(float));
}

bool ir_read_float64(IRBuffer* buffer, double* value) {
    return ir_buffer_read(buffer, value, sizeof(double));
}

bool ir_read_string(IRBuffer* buffer, char** str) {
    if (!buffer || !str) return false;

    uint32_t length;
    if (!ir_read_uint32(buffer, &length)) {
        return false;
    }

    if (length == 0) {
        *str = NULL;
        return true;
    }

    *str = malloc(length);
    if (!*str) {
        IR_LOG_ERROR("SERIALIZE", "Failed to allocate string");
        return false;
    }

    if (!ir_buffer_read(buffer, *str, length)) {
        free(*str);
        *str = NULL;
        return false;
    }

    return true;
}

bool ir_read_bytes(IRBuffer* buffer, void* data, size_t size) {
    return ir_buffer_read(buffer, data, size);
}
