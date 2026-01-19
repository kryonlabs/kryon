/**
 * @file test_ir_buffer.c
 * @brief Tests for IRBuffer - dynamic buffer for binary serialization
 */

#define _POSIX_C_SOURCE 200809L
#include "../tests/test_framework.h"
#include "ir_buffer.h"

// ============================================================================
// TEST CASES
// ============================================================================

TEST(test_buffer_create_with_capacity) {
    IRBuffer* buf = ir_buffer_create(100);
    ASSERT_NONNULL(buf);
    ASSERT_EQ(100, buf->capacity);
    ASSERT_EQ(0, buf->size);
    ASSERT_TRUE(buf->data == buf->base);
    ir_buffer_destroy(buf);
}

TEST(test_buffer_create_with_zero_capacity) {
    IRBuffer* buf = ir_buffer_create(0);
    ASSERT_NONNULL(buf);
    ASSERT_GT(buf->capacity, 0);
    ir_buffer_destroy(buf);
}

TEST(test_buffer_write_single_byte) {
    IRBuffer* buf = ir_buffer_create(10);
    uint8_t data = 0x42;
    ASSERT_TRUE(ir_buffer_write(buf, &data, 1));
    ASSERT_EQ(1, buf->size);
    ASSERT_EQ(0x42, buf->base[0]);
    ir_buffer_destroy(buf);
}

TEST(test_buffer_write_multiple_bytes) {
    IRBuffer* buf = ir_buffer_create(10);
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    ASSERT_TRUE(ir_buffer_write(buf, data, 4));
    ASSERT_EQ(4, buf->size);
    ASSERT_EQ(0x01, buf->base[0]);
    ASSERT_EQ(0x02, buf->base[1]);
    ASSERT_EQ(0x03, buf->base[2]);
    ASSERT_EQ(0x04, buf->base[3]);
    ir_buffer_destroy(buf);
}

TEST(test_buffer_write_zero_bytes) {
    IRBuffer* buf = ir_buffer_create(10);
    ASSERT_FALSE(ir_buffer_write(buf, NULL, 0));  // returns false for size 0
    ASSERT_EQ(0, buf->size);
    ir_buffer_destroy(buf);
}

TEST(test_buffer_write_expands_capacity) {
    IRBuffer* buf = ir_buffer_create(4);
    size_t initial_capacity = buf->capacity;

    // Write more than initial capacity
    uint8_t data[10] = {0};
    ASSERT_TRUE(ir_buffer_write(buf, data, 10));

    ASSERT_GT(buf->capacity, initial_capacity);
    ASSERT_EQ(10, buf->size);
    ir_buffer_destroy(buf);
}

TEST(test_buffer_read_single_byte) {
    IRBuffer* buf = ir_buffer_create(10);
    uint8_t write_data = 0xAB;
    ir_buffer_write(buf, &write_data, 1);

    // Seek to beginning for reading
    ir_buffer_seek(buf, 0);

    uint8_t read_data = 0;
    ASSERT_TRUE(ir_buffer_read(buf, &read_data, 1));
    ASSERT_EQ(0xAB, read_data);

    ir_buffer_destroy(buf);
}

TEST(test_buffer_read_multiple_bytes) {
    IRBuffer* buf = ir_buffer_create(10);
    uint8_t write_data[] = {0x11, 0x22, 0x33, 0x44};
    ir_buffer_write(buf, write_data, 4);

    // Seek to beginning for reading
    ir_buffer_seek(buf, 0);

    uint8_t read_data[4] = {0};
    ASSERT_TRUE(ir_buffer_read(buf, read_data, 4));
    ASSERT_EQ(0x11, read_data[0]);
    ASSERT_EQ(0x22, read_data[1]);
    ASSERT_EQ(0x33, read_data[2]);
    ASSERT_EQ(0x44, read_data[3]);

    ir_buffer_destroy(buf);
}

TEST(test_buffer_read_partial) {
    IRBuffer* buf = ir_buffer_create(10);
    uint8_t write_data[] = {0x11, 0x22, 0x33};
    ir_buffer_write(buf, write_data, 3);

    // Seek to beginning for reading
    ir_buffer_seek(buf, 0);

    uint8_t read_data[2] = {0};
    ASSERT_TRUE(ir_buffer_read(buf, read_data, 2));
    ASSERT_EQ(0x11, read_data[0]);
    ASSERT_EQ(0x22, read_data[1]);

    ir_buffer_destroy(buf);
}

TEST(test_buffer_read_past_end_fails) {
    IRBuffer* buf = ir_buffer_create(10);
    uint8_t write_data[] = {0x11, 0x22};
    ir_buffer_write(buf, write_data, 2);

    // Seek to beginning for reading
    ir_buffer_seek(buf, 0);

    uint8_t read_data[10] = {0};
    // Read beyond buffer capacity fails
    // Note: read() allows reading up to capacity, regardless of written data
    ASSERT_FALSE(ir_buffer_read(buf, read_data, 20));

    ir_buffer_destroy(buf);
}

TEST(test_buffer_seek) {
    IRBuffer* buf = ir_buffer_create(10);
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    ir_buffer_write(buf, data, 5);

    // Seek to position 2
    ASSERT_TRUE(ir_buffer_seek(buf, 2));
    ASSERT_EQ(2, ir_buffer_tell(buf));

    // Seek to beginning
    ASSERT_TRUE(ir_buffer_seek(buf, 0));
    ASSERT_EQ(0, ir_buffer_tell(buf));

    // Seek to end
    ASSERT_TRUE(ir_buffer_seek(buf, 5));
    ASSERT_EQ(5, ir_buffer_tell(buf));

    ir_buffer_destroy(buf);
}

TEST(test_buffer_seek_past_end) {
    IRBuffer* buf = ir_buffer_create(10);
    uint8_t data[] = {0x01, 0x02};
    ir_buffer_write(buf, data, 2);

    ASSERT_FALSE(ir_buffer_seek(buf, 100));  // past capacity

    ir_buffer_destroy(buf);
}

TEST(test_buffer_tell) {
    IRBuffer* buf = ir_buffer_create(10);
    ASSERT_EQ(0, ir_buffer_tell(buf));

    uint8_t data[] = {0x01, 0x02, 0x03};
    ir_buffer_write(buf, data, 3);

    // tell() returns data - base, and write() doesn't advance data
    // so tell stays at 0 after write (this is the actual behavior)
    ASSERT_EQ(0, ir_buffer_tell(buf));

    // After seeking, tell reflects new position
    ir_buffer_seek(buf, 2);
    ASSERT_EQ(2, ir_buffer_tell(buf));

    ir_buffer_destroy(buf);
}

TEST(test_buffer_size) {
    IRBuffer* buf = ir_buffer_create(10);
    ASSERT_EQ(0, ir_buffer_size(buf));

    uint8_t data[] = {0x01, 0x02, 0x03};
    ir_buffer_write(buf, data, 3);

    ASSERT_EQ(3, ir_buffer_size(buf));

    ir_buffer_destroy(buf);
}

TEST(test_buffer_remaining) {
    IRBuffer* buf = ir_buffer_create(10);
    ASSERT_EQ(10, ir_buffer_remaining(buf));  // full capacity available

    uint8_t data[5] = {0};
    ir_buffer_write(buf, data, 5);

    // remaining returns capacity - (data - base)
    // write() doesn't advance data, so remaining is still capacity
    ASSERT_EQ(10, ir_buffer_remaining(buf));

    // After seeking, remaining reflects new position
    ir_buffer_seek(buf, 3);
    ASSERT_EQ(7, ir_buffer_remaining(buf));

    ir_buffer_destroy(buf);
}

TEST(test_buffer_at_end) {
    IRBuffer* buf = ir_buffer_create(10);
    ASSERT_FALSE(ir_buffer_at_end(buf));  // position 0, not at end

    uint8_t data = 0x01;
    ir_buffer_write(buf, &data, 1);

    ASSERT_FALSE(ir_buffer_at_end(buf));

    // Seek to end
    ir_buffer_seek(buf, 10);
    ASSERT_TRUE(ir_buffer_at_end(buf));

    ir_buffer_destroy(buf);
}

TEST(test_buffer_clear) {
    IRBuffer* buf = ir_buffer_create(10);
    uint8_t data[] = {0x01, 0x02, 0x03};
    ir_buffer_write(buf, data, 3);

    ASSERT_EQ(3, buf->size);

    ir_buffer_clear(buf);

    ASSERT_EQ(0, buf->size);
    ASSERT_TRUE(buf->data == buf->base);

    ir_buffer_destroy(buf);
}

TEST(test_buffer_reserve) {
    IRBuffer* buf = ir_buffer_create(10);

    ASSERT_TRUE(ir_buffer_reserve(buf, 100));
    ASSERT_GTE(buf->capacity, 100);

    ir_buffer_destroy(buf);
}

TEST(test_buffer_reserve_smaller_than_current) {
    IRBuffer* buf = ir_buffer_create(100);

    // Reserve smaller than current - should not shrink
    size_t original_capacity = buf->capacity;
    ASSERT_TRUE(ir_buffer_reserve(buf, 50));
    ASSERT_EQ(original_capacity, buf->capacity);  // doesn't shrink

    ir_buffer_destroy(buf);
}

TEST(test_buffer_data_pointer) {
    IRBuffer* buf = ir_buffer_create(10);
    uint8_t data[] = {0x01, 0x02, 0x03};
    ir_buffer_write(buf, data, 3);

    void* data_ptr = ir_buffer_data(buf);
    ASSERT_EQ(buf->base, data_ptr);

    // Verify data is accessible
    uint8_t* base_data = (uint8_t*)data_ptr;
    ASSERT_EQ(0x01, base_data[0]);
    ASSERT_EQ(0x02, base_data[1]);
    ASSERT_EQ(0x03, base_data[2]);

    ir_buffer_destroy(buf);
}

TEST(test_buffer_write_read_destroy_cycle) {
    IRBuffer* buf = ir_buffer_create(100);

    // Write
    uint8_t original[] = {0x00, 0xFF, 0xAA, 0x55, 0x12, 0x34};
    ASSERT_TRUE(ir_buffer_write(buf, original, sizeof(original)));

    // Seek to beginning for reading
    ir_buffer_seek(buf, 0);

    // Read
    uint8_t readback[sizeof(original)] = {0};
    ASSERT_TRUE(ir_buffer_read(buf, readback, sizeof(readback)));

    // Verify
    ASSERT_MEM_EQ(original, readback, sizeof(original));

    ir_buffer_destroy(buf);
}

// ============================================================================
// MAIN
// ============================================================================

static void run_all_tests(void) {
    BEGIN_SUITE("IRBuffer Tests");

    // Creation and destruction
    RUN_TEST(test_buffer_create_with_capacity);
    RUN_TEST(test_buffer_create_with_zero_capacity);

    // Write operations
    RUN_TEST(test_buffer_write_single_byte);
    RUN_TEST(test_buffer_write_multiple_bytes);
    RUN_TEST(test_buffer_write_zero_bytes);
    RUN_TEST(test_buffer_write_expands_capacity);

    // Read operations
    RUN_TEST(test_buffer_read_single_byte);
    RUN_TEST(test_buffer_read_multiple_bytes);
    RUN_TEST(test_buffer_read_partial);
    RUN_TEST(test_buffer_read_past_end_fails);

    // Position operations
    RUN_TEST(test_buffer_seek);
    RUN_TEST(test_buffer_seek_past_end);
    RUN_TEST(test_buffer_tell);
    RUN_TEST(test_buffer_size);
    RUN_TEST(test_buffer_remaining);
    RUN_TEST(test_buffer_at_end);

    // Utilities
    RUN_TEST(test_buffer_clear);
    RUN_TEST(test_buffer_reserve);
    RUN_TEST(test_buffer_reserve_smaller_than_current);
    RUN_TEST(test_buffer_data_pointer);

    // Integration
    RUN_TEST(test_buffer_write_read_destroy_cycle);

    END_SUITE();
}

RUN_TEST_SUITE(run_all_tests);
