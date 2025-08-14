/**
 * @file binary_io.h
 * @brief Cross-platform binary I/O utilities with endianness handling
 * 
 * Provides consistent big-endian binary reading/writing functions
 * for the KRB file format across all platforms.
 */

#ifndef KRYON_INTERNAL_BINARY_IO_H
#define KRYON_INTERNAL_BINARY_IO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// =============================================================================
// BINARY WRITING FUNCTIONS
// =============================================================================

/**
 * Write an 8-bit unsigned integer to buffer
 */
bool kryon_write_uint8(uint8_t **buffer, size_t *offset, size_t buffer_size, uint8_t value);

/**
 * Write a 16-bit unsigned integer to buffer in big-endian format
 */
bool kryon_write_uint16(uint8_t **buffer, size_t *offset, size_t buffer_size, uint16_t value);

/**
 * Write a 32-bit unsigned integer to buffer in big-endian format
 */
bool kryon_write_uint32(uint8_t **buffer, size_t *offset, size_t buffer_size, uint32_t value);

/**
 * Write a 64-bit unsigned integer to buffer in big-endian format
 */
bool kryon_write_uint64(uint8_t **buffer, size_t *offset, size_t buffer_size, uint64_t value);

/**
 * Write raw bytes to buffer
 */
bool kryon_write_bytes(uint8_t **buffer, size_t *offset, size_t buffer_size, const void *data, size_t data_size);

// =============================================================================
// BINARY READING FUNCTIONS
// =============================================================================

/**
 * Read an 8-bit unsigned integer from buffer
 */
bool kryon_read_uint8(const uint8_t *buffer, size_t *offset, size_t buffer_size, uint8_t *value);

/**
 * Read a 16-bit unsigned integer from buffer (big-endian format)
 */
bool kryon_read_uint16(const uint8_t *buffer, size_t *offset, size_t buffer_size, uint16_t *value);

/**
 * Read a 32-bit unsigned integer from buffer (big-endian format)
 */
bool kryon_read_uint32(const uint8_t *buffer, size_t *offset, size_t buffer_size, uint32_t *value);

/**´
 * Read a 64-bit unsigned integer from buffer (big-endian format)
 */
bool kryon_read_uint64(const uint8_t *buffer, size_t *offset, size_t buffer_size, uint64_t *value);

/**
 * Read raw bytes from buffer
 */
bool kryon_read_bytes(const uint8_t *buffer, size_t *offset, size_t buffer_size, void *data, size_t data_size);

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_BINARY_IO_H