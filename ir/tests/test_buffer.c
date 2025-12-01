/*
 * Unit tests for IRBuffer (ir_serialization.c)
 * Tests buffer creation, writing, reading, seeking, and memory management
 */

#include "test_framework.h"
#include "../ir_core.h"
#include "../ir_serialization.h"
#include <stdint.h>
#include <string.h>

// Test: Create and destroy buffer
TEST(test_buffer_create_destroy) {
    IRBuffer* buffer = ir_buffer_create(1024);
    ASSERT_NONNULL(buffer);
    ASSERT_NONNULL(buffer->data);
    ASSERT_NONNULL(buffer->base);
    ASSERT_EQ(buffer->size, 0);
    ASSERT_EQ(buffer->capacity, 1024);
    ASSERT_EQ(buffer->data, buffer->base);  // Initially should point to same address

    ir_buffer_destroy(buffer);
}

// Test: Create buffer with zero capacity
TEST(test_buffer_create_zero_capacity) {
    IRBuffer* buffer = ir_buffer_create(0);
    ASSERT_NONNULL(buffer);
    ir_buffer_destroy(buffer);
}

// Test: Destroy NULL buffer (should not crash)
TEST(test_buffer_destroy_null) {
    ir_buffer_destroy(NULL);  // Should not crash
}

// Test: Write single byte
TEST(test_buffer_write_single_byte) {
    IRBuffer* buffer = ir_buffer_create(1024);
    ASSERT_NONNULL(buffer);

    uint8_t byte = 0x42;
    bool success = ir_buffer_write(buffer, &byte, 1);
    ASSERT_TRUE(success);
    ASSERT_EQ(buffer->size, 1);
    ASSERT_EQ(buffer->base[0], 0x42);

    ir_buffer_destroy(buffer);
}

// Test: Write multiple bytes
TEST(test_buffer_write_multiple_bytes) {
    IRBuffer* buffer = ir_buffer_create(1024);
    ASSERT_NONNULL(buffer);

    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    bool success = ir_buffer_write(buffer, data, sizeof(data));
    ASSERT_TRUE(success);
    ASSERT_EQ(buffer->size, 5);
    ASSERT_MEM_EQ(buffer->base, data, sizeof(data));

    ir_buffer_destroy(buffer);
}

// Test: Write causes buffer expansion
TEST(test_buffer_write_expansion) {
    IRBuffer* buffer = ir_buffer_create(4);  // Small initial capacity
    ASSERT_NONNULL(buffer);
    ASSERT_EQ(buffer->capacity, 4);

    uint8_t data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    bool success = ir_buffer_write(buffer, data, sizeof(data));
    ASSERT_TRUE(success);
    ASSERT_EQ(buffer->size, 10);
    ASSERT_GTE(buffer->capacity, 10);  // Capacity should have grown
    ASSERT_MEM_EQ(buffer->base, data, sizeof(data));

    ir_buffer_destroy(buffer);
}

// Test: Multiple writes with expansion
TEST(test_buffer_multiple_writes_expansion) {
    IRBuffer* buffer = ir_buffer_create(4);
    ASSERT_NONNULL(buffer);

    // Write 1st chunk
    uint8_t data1[] = {1, 2, 3};
    bool success = ir_buffer_write(buffer, data1, sizeof(data1));
    ASSERT_TRUE(success);
    ASSERT_EQ(buffer->size, 3);

    // Write 2nd chunk (should trigger expansion)
    uint8_t data2[] = {4, 5, 6, 7, 8};
    success = ir_buffer_write(buffer, data2, sizeof(data2));
    ASSERT_TRUE(success);
    ASSERT_EQ(buffer->size, 8);

    // Verify all data
    uint8_t expected[] = {1, 2, 3, 4, 5, 6, 7, 8};
    ASSERT_MEM_EQ(buffer->base, expected, sizeof(expected));

    ir_buffer_destroy(buffer);
}

// Test: Read single byte
TEST(test_buffer_read_single_byte) {
    IRBuffer* buffer = ir_buffer_create(1024);
    ASSERT_NONNULL(buffer);

    // Write a byte
    uint8_t write_byte = 0x42;
    ir_buffer_write(buffer, &write_byte, 1);

    // Reset position for reading
    buffer->data = buffer->base;
    buffer->size = 1;

    // Read it back
    uint8_t read_byte = 0;
    bool success = ir_buffer_read(buffer, &read_byte, 1);
    ASSERT_TRUE(success);
    ASSERT_EQ(read_byte, 0x42);

    ir_buffer_destroy(buffer);
}

// Test: Read multiple bytes
TEST(test_buffer_read_multiple_bytes) {
    IRBuffer* buffer = ir_buffer_create(1024);
    ASSERT_NONNULL(buffer);

    // Write data
    uint8_t write_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    ir_buffer_write(buffer, write_data, sizeof(write_data));

    // Reset position for reading
    buffer->data = buffer->base;
    buffer->size = sizeof(write_data);

    // Read it back
    uint8_t read_data[5] = {0};
    bool success = ir_buffer_read(buffer, read_data, sizeof(read_data));
    ASSERT_TRUE(success);
    ASSERT_MEM_EQ(read_data, write_data, sizeof(write_data));

    ir_buffer_destroy(buffer);
}

// Test: Sequential reads (this tests the base pointer bug fix!)
TEST(test_buffer_sequential_reads) {
    IRBuffer* buffer = ir_buffer_create(1024);
    ASSERT_NONNULL(buffer);

    // Write data in chunks
    uint32_t chunk1 = 0x12345678;
    uint32_t chunk2 = 0xABCDEF00;
    uint32_t chunk3 = 0xDEADBEEF;
    ir_buffer_write(buffer, &chunk1, sizeof(chunk1));
    ir_buffer_write(buffer, &chunk2, sizeof(chunk2));
    ir_buffer_write(buffer, &chunk3, sizeof(chunk3));

    // Reset for reading
    buffer->data = buffer->base;
    buffer->size = sizeof(chunk1) + sizeof(chunk2) + sizeof(chunk3);

    // Read chunks sequentially (this advances buffer->data pointer)
    uint32_t read1 = 0, read2 = 0, read3 = 0;
    ASSERT_TRUE(ir_buffer_read(buffer, &read1, sizeof(read1)));
    ASSERT_TRUE(ir_buffer_read(buffer, &read2, sizeof(read2)));
    ASSERT_TRUE(ir_buffer_read(buffer, &read3, sizeof(read3)));

    ASSERT_EQ(read1, chunk1);
    ASSERT_EQ(read2, chunk2);
    ASSERT_EQ(read3, chunk3);

    // CRITICAL: buffer->data should be offset, but buffer->base should still point to original
    ASSERT_NEQ((uintptr_t)buffer->data, (uintptr_t)buffer->base);
    ASSERT_EQ((uintptr_t)buffer->data, (uintptr_t)buffer->base + 12);

    // This should NOT crash (this is the bug we fixed!)
    ir_buffer_destroy(buffer);
}

// Test: Read beyond buffer size (should fail)
TEST(test_buffer_read_overflow) {
    IRBuffer* buffer = ir_buffer_create(1024);
    ASSERT_NONNULL(buffer);

    // Write 5 bytes
    uint8_t data[] = {1, 2, 3, 4, 5};
    ir_buffer_write(buffer, data, sizeof(data));

    // Reset for reading
    buffer->data = buffer->base;
    buffer->size = sizeof(data);

    // Try to read 10 bytes (should fail)
    uint8_t read_data[10] = {0};
    bool success = ir_buffer_read(buffer, read_data, sizeof(read_data));
    ASSERT_FALSE(success);

    ir_buffer_destroy(buffer);
}

// Note: ir_buffer_seek() is not implemented, so seek tests are skipped

// Test: Write-Read-Destroy cycle (integration test)
TEST(test_buffer_write_read_destroy_cycle) {
    IRBuffer* buffer = ir_buffer_create(256);
    ASSERT_NONNULL(buffer);

    // Write various data types
    uint8_t byte = 0x42;
    uint16_t word = 0x1234;
    uint32_t dword = 0xDEADBEEF;
    uint64_t qword = 0x0123456789ABCDEFULL;

    ir_buffer_write(buffer, &byte, sizeof(byte));
    ir_buffer_write(buffer, &word, sizeof(word));
    ir_buffer_write(buffer, &dword, sizeof(dword));
    ir_buffer_write(buffer, &qword, sizeof(qword));

    // Reset for reading
    size_t total_size = sizeof(byte) + sizeof(word) + sizeof(dword) + sizeof(qword);
    buffer->data = buffer->base;
    buffer->size = total_size;

    // Read back
    uint8_t r_byte = 0;
    uint16_t r_word = 0;
    uint32_t r_dword = 0;
    uint64_t r_qword = 0;

    ASSERT_TRUE(ir_buffer_read(buffer, &r_byte, sizeof(r_byte)));
    ASSERT_TRUE(ir_buffer_read(buffer, &r_word, sizeof(r_word)));
    ASSERT_TRUE(ir_buffer_read(buffer, &r_dword, sizeof(r_dword)));
    ASSERT_TRUE(ir_buffer_read(buffer, &r_qword, sizeof(r_qword)));

    ASSERT_EQ(r_byte, byte);
    ASSERT_EQ(r_word, word);
    ASSERT_EQ(r_dword, dword);
    ASSERT_EQ(r_qword, qword);

    // Destroy (should not crash even though data pointer was advanced)
    ir_buffer_destroy(buffer);
}

// Test suite runner
void run_buffer_tests(void) {
    BEGIN_SUITE("IRBuffer Tests");

    RUN_TEST(test_buffer_create_destroy);
    RUN_TEST(test_buffer_create_zero_capacity);
    RUN_TEST(test_buffer_destroy_null);
    RUN_TEST(test_buffer_write_single_byte);
    RUN_TEST(test_buffer_write_multiple_bytes);
    RUN_TEST(test_buffer_write_expansion);
    RUN_TEST(test_buffer_multiple_writes_expansion);
    RUN_TEST(test_buffer_read_single_byte);
    RUN_TEST(test_buffer_read_multiple_bytes);
    RUN_TEST(test_buffer_sequential_reads);
    RUN_TEST(test_buffer_read_overflow);
    RUN_TEST(test_buffer_write_read_destroy_cycle);

    END_SUITE();
}

RUN_TEST_SUITE(run_buffer_tests)
