/**

 * @file binary_io.c
 * @brief Cross-platform binary I/O utilities implementation
 * 
 * Implements consistent big-endian binary reading/writing functions
 * for the KRB file format across all platforms.
 */
#include "lib9.h"


#include "binary_io.h"
#include <endian.h>

// =============================================================================
// BINARY WRITING FUNCTIONS
// =============================================================================

bool kryon_write_uint8(uint8_t **buffer, size_t *offset, size_t buffer_size, uint8_t value) {
    if (!buffer || !*buffer || !offset || *offset >= buffer_size) {
        return false;
    }
    
    (*buffer)[*offset] = value;
    (*offset)++;
    return true;
}

bool kryon_write_uint16(uint8_t **buffer, size_t *offset, size_t buffer_size, uint16_t value) {
    if (!buffer || !*buffer || !offset || *offset + sizeof(uint16_t) > buffer_size) {
        return false;
    }

    // Write in native byte order (little-endian on x86)
    memcpy(*buffer + *offset, &value, sizeof(uint16_t));
    *offset += sizeof(uint16_t);
    return true;
}

bool kryon_write_uint32(uint8_t **buffer, size_t *offset, size_t buffer_size, uint32_t value) {
    if (!buffer || !*buffer || !offset || *offset + sizeof(uint32_t) > buffer_size) {
        return false;
    }

    // Convert to big-endian
    uint32_t big_endian_value = htobe32(value);
    memcpy(*buffer + *offset, &big_endian_value, sizeof(uint32_t));
    *offset += sizeof(uint32_t);
    return true;
}

bool kryon_write_uint64(uint8_t **buffer, size_t *offset, size_t buffer_size, uint64_t value) {
    if (!buffer || !*buffer || !offset || *offset + sizeof(uint64_t) > buffer_size) {
        return false;
    }
    
    // Convert to big-endian
    uint64_t big_endian_value = htobe64(value);
    memcpy(*buffer + *offset, &big_endian_value, sizeof(uint64_t));
    *offset += sizeof(uint64_t);
    return true;
}

bool kryon_write_bytes(uint8_t **buffer, size_t *offset, size_t buffer_size, const void *data, size_t data_size) {
    if (!buffer || !*buffer || !offset || !data || *offset + data_size > buffer_size) {
        return false;
    }
    
    memcpy(*buffer + *offset, data, data_size);
    *offset += data_size;
    return true;
}

// =============================================================================
// BINARY READING FUNCTIONS
// =============================================================================

bool kryon_read_uint8(const uint8_t *buffer, size_t *offset, size_t buffer_size, uint8_t *value) {
    if (!buffer || !offset || !value || *offset >= buffer_size) {
        return false;
    }
    
    *value = buffer[*offset];
    (*offset)++;
    return true;
}

bool kryon_read_uint16(const uint8_t *buffer, size_t *offset, size_t buffer_size, uint16_t *value) {
    if (!buffer || !offset || !value || *offset + sizeof(uint16_t) > buffer_size) {
        return false;
    }

    // Read in native byte order (little-endian on x86)
    memcpy(value, buffer + *offset, sizeof(uint16_t));
    *offset += sizeof(uint16_t);
    return true;
}

bool kryon_read_uint32(const uint8_t *buffer, size_t *offset, size_t buffer_size, uint32_t *value) {
    if (!buffer || !offset || !value || *offset + sizeof(uint32_t) > buffer_size) {
        return false;
    }

    uint32_t big_endian_value;
    memcpy(&big_endian_value, buffer + *offset, sizeof(uint32_t));

    // Convert from big-endian to host byte order
    *value = be32toh(big_endian_value);
    *offset += sizeof(uint32_t);
    return true;
}

bool kryon_read_uint64(const uint8_t *buffer, size_t *offset, size_t buffer_size, uint64_t *value) {
    if (!buffer || !offset || !value || *offset + sizeof(uint64_t) > buffer_size) {
        return false;
    }
    
    uint64_t big_endian_value;
    memcpy(&big_endian_value, buffer + *offset, sizeof(uint64_t));
    
    // Convert from big-endian to host byte order
    *value = be64toh(big_endian_value);
    *offset += sizeof(uint64_t);
    return true;
}

bool kryon_read_bytes(const uint8_t *buffer, size_t *offset, size_t buffer_size, void *data, size_t data_size) {
    if (!buffer || !offset || !data || *offset + data_size > buffer_size) {
        return false;
    }
    
    memcpy(data, buffer + *offset, data_size);
    *offset += data_size;
    return true;
}
