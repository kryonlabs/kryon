#ifndef IR_BUFFER_H
#define IR_BUFFER_H

#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// Dynamic Buffer for Binary Serialization
// ============================================================================

/**
 * IRBuffer - Dynamic buffer for binary serialization/deserialization
 *
 * Provides a simple buffer with read/write capabilities, automatic growth,
 * and ownership management for serialization operations.
 */
typedef struct IRBuffer {
    char* data;          // Current read/write position
    char* base;          // Original pointer (for free)
    size_t size;         // Current position in buffer
    size_t capacity;     // Total capacity
    bool owns_memory;    // Whether we should free the data
} IRBuffer;

// ============================================================================
// Buffer Creation and Destruction
// ============================================================================

// Create a new buffer with specified initial capacity
IRBuffer* ir_buffer_create(size_t initial_capacity);

// Create a buffer wrapping existing data (takes ownership if owns=true)
IRBuffer* ir_buffer_create_from_data(void* data, size_t size, bool owns);

// Create a buffer by reading from a file
IRBuffer* ir_buffer_create_from_file(const char* filename);

// Free a buffer and its owned memory
void ir_buffer_free(IRBuffer* buffer);

// ============================================================================
// Buffer Operations
// ============================================================================

// Write data to buffer (grows if needed)
bool ir_buffer_write(IRBuffer* buffer, const void* data, size_t size);

// Read data from buffer
bool ir_buffer_read(IRBuffer* buffer, void* data, size_t size);

// Seek to absolute position
bool ir_buffer_seek(IRBuffer* buffer, size_t position);

// Skip forward by specified bytes
bool ir_buffer_skip(IRBuffer* buffer, size_t bytes);

// Get current position
size_t ir_buffer_tell(const IRBuffer* buffer);

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

// Release ownership of data (caller must free)
char* ir_buffer_release(IRBuffer* buffer);

// Get current data pointer (base)
void* ir_buffer_data(const IRBuffer* buffer);

#endif // IR_BUFFER_H
