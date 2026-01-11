#ifndef IR_BUFFER_H
#define IR_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ============================================================================
// Dynamic Buffer for Binary Serialization
// ============================================================================

/**
 * IRBuffer - Dynamic buffer for binary serialization/deserialization
 *
 * Note: This struct is defined in ir_core.h for backward compatibility.
 * This header provides the buffer management functions.
 */
typedef struct IRBuffer {
    uint8_t* data;       // Current read/write position
    uint8_t* base;        // Original pointer (for free)
    size_t size;          // Remaining/used bytes
    size_t capacity;      // Total capacity
} IRBuffer;

// ============================================================================
// Buffer Creation and Destruction
// ============================================================================

// Create a new buffer with specified initial capacity
IRBuffer* ir_buffer_create(size_t initial_capacity);

// Create a buffer by reading from a file
IRBuffer* ir_buffer_create_from_file(const char* filename);

// Free a buffer and its memory
void ir_buffer_destroy(IRBuffer* buffer);

// ============================================================================
// Buffer Operations
// ============================================================================

// Write data to buffer (grows if needed)
bool ir_buffer_write(IRBuffer* buffer, const void* data, size_t size);

// Read data from buffer
bool ir_buffer_read(IRBuffer* buffer, void* data, size_t size);

// Seek to absolute position (relative to base)
bool ir_buffer_seek(IRBuffer* buffer, size_t position);

// Get current position (bytes from base)
size_t ir_buffer_tell(const IRBuffer* buffer);

// Get current size/position
size_t ir_buffer_size(const IRBuffer* buffer);

// Get remaining bytes
size_t ir_buffer_remaining(const IRBuffer* buffer);

// Check if at end of buffer
bool ir_buffer_at_end(const IRBuffer* buffer);

// ============================================================================
// Buffer Utilities
// ============================================================================

// Reset position to beginning
void ir_buffer_clear(IRBuffer* buffer);

// Ensure capacity (grows if needed)
bool ir_buffer_reserve(IRBuffer* buffer, size_t capacity);

// Get current data pointer (base)
void* ir_buffer_data(const IRBuffer* buffer);

#endif // IR_BUFFER_H
