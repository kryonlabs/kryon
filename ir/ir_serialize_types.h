#ifndef IR_SERIALIZE_TYPES_H
#define IR_SERIALIZE_TYPES_H

#include "ir_buffer.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// Write Functions
// ============================================================================

// Write primitive types to buffer
bool ir_write_uint8(IRBuffer* buffer, uint8_t value);
bool ir_write_uint16(IRBuffer* buffer, uint16_t value);
bool ir_write_uint32(IRBuffer* buffer, uint32_t value);
bool ir_write_uint64(IRBuffer* buffer, uint64_t value);
bool ir_write_int32(IRBuffer* buffer, int32_t value);
bool ir_write_int64(IRBuffer* buffer, int64_t value);
bool ir_write_float32(IRBuffer* buffer, float value);
bool ir_write_float64(IRBuffer* buffer, double value);

// Write string (length-prefixed, null-terminated included)
bool ir_write_string(IRBuffer* buffer, const char* str);

// Write raw bytes
bool ir_write_bytes(IRBuffer* buffer, const void* data, size_t size);

// ============================================================================
// Read Functions
// ============================================================================

// Read primitive types from buffer
bool ir_read_uint8(IRBuffer* buffer, uint8_t* value);
bool ir_read_uint16(IRBuffer* buffer, uint16_t* value);
bool ir_read_uint32(IRBuffer* buffer, uint32_t* value);
bool ir_read_uint64(IRBuffer* buffer, uint64_t* value);
bool ir_read_int32(IRBuffer* buffer, int32_t* value);
bool ir_read_int64(IRBuffer* buffer, int64_t* value);
bool ir_read_float32(IRBuffer* buffer, float* value);
bool ir_read_float64(IRBuffer* buffer, double* value);

// Read string (allocates memory, caller must free)
bool ir_read_string(IRBuffer* buffer, char** str);

// Read raw bytes
bool ir_read_bytes(IRBuffer* buffer, void* data, size_t size);

#endif // IR_SERIALIZE_TYPES_H
