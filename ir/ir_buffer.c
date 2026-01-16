/**
 * @file ir_buffer.c
 * @brief Dynamic buffer implementation for binary serialization
 */

#define _GNU_SOURCE
#include "ir_buffer.h"
#include "ir_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DEFAULT_CAPACITY 4096
#define GROWTH_FACTOR 2

// ============================================================================
// Buffer Creation and Destruction
// ============================================================================

IRBuffer* ir_buffer_create(size_t initial_capacity) {
    IRBuffer* buffer = calloc(1, sizeof(IRBuffer));
    if (!buffer) return NULL;

    if (initial_capacity == 0) initial_capacity = DEFAULT_CAPACITY;

    buffer->data = malloc(initial_capacity);
    if (!buffer->data) {
        free(buffer);
        return NULL;
    }

    buffer->base = buffer->data;
    buffer->capacity = initial_capacity;
    buffer->size = 0;

    return buffer;
}

IRBuffer* ir_buffer_create_from_file(const char* filename) {
    if (!filename) return NULL;

    FILE* file = fopen(filename, "rb");
    if (!file) {
        IR_LOG_ERROR("BUFFER", "Failed to open file: %s", filename);
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size == 0) {
        fclose(file);
        return ir_buffer_create(DEFAULT_CAPACITY);
    }

    // Create buffer
    IRBuffer* buffer = ir_buffer_create(file_size);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    // Read file into buffer
    size_t bytes_read = fread(buffer->data, 1, file_size, file);
    fclose(file);

    if (bytes_read != file_size) {
        IR_LOG_ERROR("BUFFER", "Failed to read complete file");
        ir_buffer_destroy(buffer);
        return NULL;
    }

    buffer->size = file_size;

    return buffer;
}

void ir_buffer_destroy(IRBuffer* buffer) {
    if (!buffer) return;

    if (buffer->base) {
        free(buffer->base);
    }

    free(buffer);
}

// ============================================================================
// Buffer Utilities
// ============================================================================

bool ir_buffer_reserve(IRBuffer* buffer, size_t capacity) {
    if (!buffer) return false;

    if (capacity <= buffer->capacity) return true;

    size_t new_capacity = buffer->capacity;
    while (new_capacity < capacity) {
        new_capacity *= GROWTH_FACTOR;
    }

    // Expand existing buffer
    uint8_t* new_data = realloc(buffer->base, new_capacity);
    if (!new_data) return false;

    // Update pointers
    size_t offset = buffer->data - buffer->base;
    buffer->base = new_data;
    buffer->data = new_data + offset;
    buffer->capacity = new_capacity;

    return true;
}

void ir_buffer_clear(IRBuffer* buffer) {
    if (!buffer) return;
    buffer->data = buffer->base;
    buffer->size = 0;
}

void* ir_buffer_data(const IRBuffer* buffer) {
    return buffer ? buffer->base : NULL;
}

// ============================================================================
// Buffer Operations
// ============================================================================

bool ir_buffer_write(IRBuffer* buffer, const void* data, size_t size) {
    if (!buffer || !data || size == 0) return false;

    // Ensure capacity
    if (buffer->size + size > buffer->capacity) {
        if (!ir_buffer_reserve(buffer, buffer->size + size)) {
            return false;
        }
    }

    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;

    return true;
}

bool ir_buffer_read(IRBuffer* buffer, void* data, size_t size) {
    if (!buffer || !data || size == 0) return false;

    // Check if we have enough data
    if (buffer->size + size > buffer->capacity) {
        return false;
    }

    memcpy(data, buffer->data, size);
    buffer->data += size;
    buffer->size += size;

    return true;
}

bool ir_buffer_seek(IRBuffer* buffer, size_t position) {
    if (!buffer) return false;

    if (position > buffer->capacity) {
        return false;
    }

    buffer->data = buffer->base + position;
    buffer->size = position;

    return true;
}

size_t ir_buffer_tell(const IRBuffer* buffer) {
    return buffer ? (buffer->data - buffer->base) : 0;
}

size_t ir_buffer_size(const IRBuffer* buffer) {
    return buffer ? buffer->size : 0;
}

size_t ir_buffer_remaining(const IRBuffer* buffer) {
    if (!buffer) return 0;
    return buffer->capacity - (buffer->data - buffer->base);
}

bool ir_buffer_at_end(const IRBuffer* buffer) {
    if (!buffer) return true;
    return (size_t)(buffer->data - buffer->base) >= buffer->capacity;
}
