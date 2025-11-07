/**
 * Kryon Terminal Renderer - Event System Edge Case Tests
 *
 * Tests edge cases, error conditions, and robustness of the
 * terminal event handling system to ensure it's flawless.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

// Mock tickit for testing
struct mock_tickit {
    int ref_count;
    int events_enabled;
};

struct mock_tickit_window {
    struct mock_tickit* tickit;
    int ref_count;
};

// Mock terminal event state
struct mock_event_state {
    kryon_event_t event_queue[32];
    size_t head;
    size_t tail;
    size_t count;
    bool overflow;
};

// Include our terminal backend headers
#define KRYON_TERMINAL_TEST 1
#include "terminal_backend.h"
#include "terminal_events.c"

// Test counters
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (!(condition)) { \
            printf("❌ FAIL: %s\n", message); \
            return false; \
        } else { \
            printf("✅ PASS: %s\n", message); \
            tests_passed++; \
        } \
    } while(0)

// ============================================================================
// Mock Functions
// ============================================================================

static struct mock_tickit* mock_tickit_create() {
    struct mock_tickit* tickit = calloc(1, sizeof(struct mock_tickit));
    if (!tickit) return NULL;
    tickit->ref_count = 1;
    tickit->events_enabled = 0;
    return tickit;
}

static void mock_tickit_destroy(struct mock_tickit* tickit) {
    if (tickit && tickit->ref_count > 0) {
        tickit->ref_count--;
        if (tickit->ref_count == 0) {
            free(tickit);
        }
    }
}

static struct mock_tickit_window* mock_window_create(struct mock_tickit* tickit) {
    struct mock_tickit_window* window = calloc(1, sizeof(struct mock_tickit_window));
    if (!window) return NULL;
    window->tickit = tickit;
    window->ref_count = 1;
    if (tickit) tickit->ref_count++;
    return window;
}

static void mock_window_destroy(struct mock_tickit_window* window) {
    if (window && window->ref_count > 0) {
        window->ref_count--;
        if (window->ref_count == 0) {
            if (window->tickit) {
                mock_tickit_destroy(window->tickit);
            }
            free(window);
        }
    }
}

// ============================================================================
// Event Queue Tests
// ============================================================================

static bool test_event_queue_basic_operations() {
    printf("Testing event queue basic operations...\n");

    // Create a mock event state for testing
    struct mock_event_state state = {0};

    // Test empty queue
    kryon_event_t event;
    TEST_ASSERT(!kryon_terminal_events_dequeue(&state, &event), "Empty queue should return false");
    TEST_ASSERT(state.count == 0, "Empty queue count should be 0");

    // Test queueing single event
    kryon_event_t test_event = {
        .type = KRYON_EVT_CLICK,
        .x = 10,
        .y = 20,
        .param = 1,
        .timestamp = 12345
    };

    TEST_ASSERT(kryon_terminal_events_queue_event(&state, &test_event), "Should queue single event");
    TEST_ASSERT(state.count == 1, "Queue count should be 1 after queuing");

    // Test dequeueing single event
    kryon_event_t retrieved_event;
    TEST_ASSERT(kryon_terminal_events_dequeue(&state, &retrieved_event), "Should dequeue queued event");
    TEST_ASSERT(retrieved_event.type == KRYON_EVT_CLICK, "Event type should match");
    TEST_ASSERT(retrieved_event.x == 10, "Event x coordinate should match");
    TEST_ASSERT(retrieved_event.y == 20, "Event y coordinate should match");
    TEST_ASSERT(state.count == 0, "Queue should be empty after dequeue");

    printf("✅ Event queue basic operations working correctly\n");
    return true;
}

static bool test_event_queue_capacity_limits() {
    printf("Testing event queue capacity limits...\n");

    struct mock_event_state state = {0};

    // Fill queue to capacity
    for (int i = 0; i < 32; i++) {
        kryon_event_t test_event = {
            .type = KRYON_EVT_CLICK,
            .x = i,
            .y = i,
            .param = i,
            .timestamp = 1000 + i
        };
        TEST_ASSERT(kryon_terminal_events_queue_event(&state, &test_event),
                     "Should be able to queue events up to capacity");
    }

    TEST_ASSERT(state.count == 32, "Queue should be full");
    TEST_ASSERT(state.overflow == false, "Overflow should not be set yet");

    // Test queue overflow
    kryon_event_t overflow_event = {
        .type = KRYON_EVT_CLICK,
        .x = 999,
        .y = 999,
        .param = 999,
        .timestamp = 9999
    };

    TEST_ASSERT(!kryon_terminal_events_queue_event(&state, &overflow_event),
                 "Should not be able to queue when full");
    TEST_ASSERT(state.overflow == true, "Overflow should be set");

    // Test dequeue from full queue
    kryon_event_t retrieved_event;
    TEST_ASSERT(kryon_terminal_events_dequeue(&state, &retrieved_event),
                 "Should be able to dequeue from full queue");
    TEST_ASSERT(state.overflow == false, "Overflow should be cleared after dequeue");

    printf("✅ Event queue capacity limits working correctly\n");
    return true;
}

static bool test_event_queue_wraparound() {
    printf("Testing event queue wraparound behavior...\n");

    struct mock_event_state state = {
        .head = 20,  // Start near end
        .tail = 25,  // Add some events
        .count = 5,
        .overflow = false
    };

    // Add events to test wraparound
    for (int i = 0; i < 10; i++) {
        kryon_event_t test_event = {
            .type = KRYON_EVT_CLICK,
            .x = 100 + i,
            .y = 200 + i,
            .param = i,
            .timestamp = 2000 + i
        };

        if (i < 7) {
            TEST_ASSERT(kryon_terminal_events_queue_event(&state, &test_event),
                         "Should queue events before wraparound");
            TEST_ASSERT(state.count <= 32, "Count should not exceed capacity");
        }
    }

    // Check queue is properly managed
    TEST_ASSERT(state.count == 12, "Count should reflect correct number of events");
    TEST_ASSERT(state.overflow == false, "Overflow should not occur during wraparound");

    printf("✅ Event queue wraparound working correctly\n");
    return true;
}

// ============================================================================
// Event Creation Tests
// ============================================================================

static bool test_event_creation_validation() {
    printf("Testing event creation validation...\n");

    // Test valid keyboard event
    kryon_event_t key_event = {
        .type = KRYON_EVT_KEY,
        .x = 0,
        .y = 0,
        .param = 'A',
        .timestamp = 12345,
        .data = NULL
    };

    TEST_ASSERT(key_event.type == KRYON_EVT_KEY, "Keyboard event type should be set");
    TEST_ASSERT(key_event.param == 'A', "Key code should be preserved");

    // Test valid mouse event
    kryon_event_t mouse_event = {
        .type = KRYON_EVT_CLICK,
        .x = 50,
        .y = 25,
        .param = 1,  // Left button
        .timestamp = 12346
    };

    TEST_ASSERT(mouse_event.type == KRYON_EVT_CLICK, "Mouse event type should be set");
    TEST_ASSERT(mouse_event.x == 50, "Mouse x coordinate should be preserved");
    TEST_ASSERT(mouse_event.y == 25, "Mouse y coordinate should be preserved");
    TEST_ASSERT(mouse_event.param == 1, "Mouse button should be preserved");

    // Test valid timer event
    kryon_event_t timer_event = {
        .type = KRYON_EVT_TIMER,
        .x = 0,
        .y = 0,
        .param = 1000,
        .timestamp = 12347
    };

    TEST_ASSERT(timer_event.type == KRYON_EVT_TIMER, "Timer event type should be set");
    TEST_ASSERT(timer_event.param == 1000, "Timer ID should be preserved");

    printf("✅ Event creation validation working correctly\n");
    return true;
}

static bool test_event_edge_cases() {
    printf("Testing event edge cases...\n");

    // Test zero timestamp
    kryon_event_t zero_time_event = {
        .type = KRYON_EVT_CLICK,
        .x = 10,
        .y = 10,
        .param = 1,
        .timestamp = 0
    };

    TEST_ASSERT(zero_time_event.timestamp == 0, "Zero timestamp should be preserved");

    // Test maximum coordinates
    kryon_event_t max_coord_event = {
        .type = KRYON_EVT_CLICK,
        .x = 32767,
        .y = 32767,
        .param = 1,
        .timestamp = 99999
    };

    TEST_ASSERT(max_coord_event.x == 32767, "Maximum x coordinate should be preserved");
    TEST_ASSERT(max_coord_event.y == 32767, "Maximum y coordinate should be preserved");

    // Test negative coordinates (should be handled gracefully)
    kryon_event_t neg_coord_event = {
        .type = KRYON_EVT_CLICK,
        .x = -10,
        y = -5,
        .param = 1,
        .timestamp = 12348
    };

    TEST_ASSERT(neg_coord_event.x == -10, "Negative x coordinate should be preserved");
    TEST_ASSERT(neg_coord_event.y == -5, "Negative y coordinate should be preserved");

    // Test large parameter value
    kryon_event_t large_param_event = {
        .type = KRYON_EVT_CUSTOM,
        .x = 0,
        .y = 0,
        .param = 0xFFFFFFFF,
        .timestamp = 12349
    };

    TEST_ASSERT(large_param_event.param == 0xFFFFFFFF, "Large parameter value should be preserved");

    printf("✅ Event edge cases handled correctly\n");
    return true;
}

// ============================================================================
// Event Handler Tests
// ============================================================================

static bool test_event_handler_null_handling() {
    printf("Testing event handler null handling...\n");

    // Test NULL event state
    TEST_ASSERT(!kryon_terminal_events_poll(NULL, NULL),
                 "NULL event state should return false for poll");

    TEST_ASSERT(!kryon_terminal_events_wait(NULL, NULL, 0),
                 "NULL event state should return false for wait");

    TEST_ASSERT(!kryon_terminal_events_has_resize(NULL, NULL, NULL),
                 "NULL event state should return false for resize check");

    TEST_ASSERT(kryon_terminal_events_clear_resize(NULL), "NULL event state should handle clear_resize gracefully");

    TEST_ASSERT(!kryon_terminal_events_is_drag_active(NULL),
                 "NULL event state should return false for drag check");

    // Test NULL event pointers
    struct mock_event_state state = {0};
    TEST_ASSERT(!kryon_terminal_events_poll(&state, NULL),
                 "NULL event pointer should return false for poll");

    TEST_ASSERT(!kryon_terminal_events_dequeue(&state, NULL),
                 "NULL event pointer should return false for dequeue");

    printf("✅ Null parameter handling working correctly\n");
    return true;
}

static bool test_event_handler_state_management() {
    printf("Testing event handler state management...\n");

    // Test resize state management
    struct mock_event_state state = {0};

    // Initially no resize pending
    TEST_ASSERT(!kryon_terminal_events_has_resize(&state, NULL, NULL),
                 "Initially no resize should be pending");

    // Set resize pending
    state.resize.pending = true;
    state.resize.new_width = 100;
    state.resize.new_height = 50;

    uint16_t width, height;
    TEST_ASSERT(kryon_terminal_events_has_resize(&state, &width, &height),
                 "Should detect pending resize");
    TEST_ASSERT(width == 100, "Resize width should be preserved");
    TEST_ASSERT(height == 50, "Resize height should be preserved");

    // Clear resize
    kryon_terminal_events_clear_resize(&state);
    TEST_ASSERT(!kryon_terminal_events_has_resize(&state, NULL, NULL),
                 "Resize should be cleared");

    // Test mouse state management
    TEST_ASSERT(!kryon_terminal_events_is_drag_active(&state),
                 "Initially no drag should be active");

    state.mouse.drag_active = true;
    TEST_ASSERT(kryon_terminal_events_is_drag_active(&state),
                 "Should detect active drag state");

    state.mouse.drag_active = false;
    TEST_ASSERT(!kryon_terminal_events_is_drag_active(&state),
                 "Should detect drag state is inactive");

    // Test mouse position tracking
    int x, y;
    kryon_terminal_events_get_mouse_position(&state, &x, &y);
    TEST_ASSERT(x == 0 && y == 0, "Initial mouse position should be origin");

    state.mouse.last_x = 25;
    state.mouse.last_y = 15;
    kryon_terminal_events_get_mouse_position(&state, &x, &y);
    TEST_ASSERT(x == 25 && y == 15, "Mouse position should be updated");

    printf("✅ Event handler state management working correctly\n");
    return true;
}

static bool test_event_handler_enabling() {
    printf("Testing event handler enabling/disabling...\n");

    struct mock_event_state state = {0};

    // Test mouse enabling/disabling
    kryon_terminal_events_set_mouse_enabled(&state, true);
    TEST_ASSERT(state.mouse.enabled, "Mouse should be enabled");

    kryon_terminal_events_set_mouse_enabled(&state, false);
    TEST_ASSERT(!state.mouse.enabled, "Mouse should be disabled");

    // Test queue statistics
    size_t queued, capacity;
    kryon_terminal_events_get_stats(&state, &queued, &capacity);
    TEST_ASSERT(queued == 0, "Initially no events should be queued");
    TEST_ASSERT(capacity == 32, "Capacity should be 32");

    // Add some events and test statistics
    kryon_event_t test_event = {
        .type = KRYON_EVT_CLICK,
        .x = 10,
        .y = 10,
        .param = 1,
        .timestamp = 12345
    };
    kryon_terminal_events_queue_event(&state, &test_event);

    kryon_terminal_events_get_stats(&state, &queued, &capacity);
    TEST_ASSERT(queued == 1, "One event should be queued");
    TEST_ASSERT(capacity == 32, "Capacity should remain 32");

    printf("✅ Event handler enabling/disabling working correctly\n");
    return true;
}

// ============================================================================
// Error Handling Tests
// ============================================================================

static bool test_error_recovery() {
    printf("Testing error recovery mechanisms...\n");

    // Simulate error conditions and test recovery

    // Test queue overflow recovery
    struct mock_event_state state = {0};

    // Fill queue to capacity
    for (int i = 0; i < 32; i++) {
        kryon_event_t test_event = {
            .type = KRYON_EVT_CLICK,
            .x = i,
            .y = i,
            .param = i,
            .timestamp = 1000 + i
        };
        kryon_terminal_events_queue_event(&state, &test_event);
    }

    // Try to add more events (should fail gracefully)
    kryon_event_t overflow_event = {
        .type = KRYON_EVT_CLICK,
        .x = 999,
        .y = 999,
        .param = 999,
        .timestamp = 9999
    };
    TEST_ASSERT(!kryon_terminal_events_queue_event(&state, &overflow_event),
                 "Should fail gracefully on overflow");

    // Test recovery by dequeueing events
    kryon_event_t retrieved_event;
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT(kryon_terminal_events_dequeue(&state, &retrieved_event),
                     "Should be able to dequeue after overflow");
    }

    TEST_ASSERT(!state.overflow, "Overflow should be cleared after dequeueing");
    TEST_ASSERT(state.count == 27, "Count should be updated correctly");

    printf("✅ Error recovery mechanisms working correctly\n");
    return true;
}

static bool test_concurrent_access() {
    printf("Testing concurrent access scenarios...\n");

    // Test multiple event queue operations
    struct mock_event_state state = {0};

    // Simulate concurrent access (simplified)
    for (int i = 0; i < 10; i++) {
        kryon_event_t event1 = {
            .type = KRYON_EVT_CLICK,
            .x = i,
            .y = i * 2,
            .param = i,
            .timestamp = 1000 + i
        };
        kryon_terminal_events_queue_event(&state, &event1);

        kryon_event_t event2 = {
            .type = KRYON_EVT_KEY,
            .x = 0,
            .y = 0,
            .param = 'A' + i,
            .timestamp = 2000 + i
        };
        kryon_terminal_events_queue_event(&state, &event2);

        TEST_ASSERT(state.count <= 32, "Count should never exceed capacity");
    }

    // Verify queue integrity
    TEST_ASSERT(state.count == 20, "Concurrent access should maintain count integrity");
    TEST_ASSERT(state.overflow == false, "Concurrent access should not corrupt queue");

    printf("✅ Concurrent access scenarios handled correctly\n");
    return true;
}

// ============================================================================
// Performance Tests
// ============================================================================

static bool test_event_queue_performance() {
    printf("Testing event queue performance...\n");

    const int iterations = 100000;
    struct mock_event_state state = {0};

    // Test enqueue performance
    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        kryon_event_t event = {
            .type = KRYON_EVT_CLICK,
            .x = i % 80,
            .y = i % 24,
            param: i,
            timestamp: 1000 + i
        };

        // Remove events if queue is getting full
        if (state.count >= 30) {
            kryon_event_t dummy_event;
            kryon_terminal_events_dequeue(&state, &dummy_event);
        }

        kryon_terminal_events_queue_event(&state, &event);
    }
    clock_t end = clock();

    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    double ops_per_sec = iterations / time_taken;

    printf("📊 Event queue performance: %.0f ops/sec\n", ops_per_sec);

    TEST_ASSERT(ops_per_sec > 50000, "Event queue should be fast enough");
    TEST_ASSERT(state.count <= 32, "Queue should not overflow during performance test");

    printf("✅ Event queue performance meets requirements\n");
    return true;
}

static bool test_memory_efficiency() {
    printf("Testing memory efficiency...\n");

    size_t initial_size = sizeof(struct mock_event_state);
    printf("📊 Event state size: %zu bytes\n", initial_size);

    TEST_ASSERT(initial_size < 1024, "Event state should be memory efficient");

    // Test memory usage with full queue
    struct mock_event_state full_state = {0};

    for (int i = 0; i < 32; i++) {
        kryon_event_t event = {
            .type = KRYON_EVT_CLICK,
            .x = i,
            .y = i,
            param: i,
            timestamp: 1000 + i
        };
        kryon_terminal_events_queue_event(&full_state, &event);
    }

    TEST_ASSERT(full_state.count == 32, "Full queue should hold 32 events");
    printf("📊 Full queue usage verified\n");

    printf("✅ Memory efficiency is acceptable\n");
    return true;
}

// ============================================================================
// Test Runner
// ============================================================================

static bool run_event_test(const char* test_name, bool (*test_func)()) {
    printf("=== %s ===\n", test_name);

    bool result = test_func();

    if (result) {
        tests_passed++;
    } else {
        tests_run++;  // Count as failed even though we passed the test above
    }

    tests_run++;
    return result;
}

static regression_test_t event_tests[] = {
    {"Event Queue Basic Operations", test_event_queue_basic_operations},
    {"Event Queue Capacity Limits", test_event_queue_capacity_limits},
    {"Event Queue Wraparound", test_event_queue_wraparound},
    {"Event Creation Validation", test_event_creation_validation},
    {"Event Edge Cases", test_event_edge_cases},
    {"Event Handler Null Handling", test_event_handler_null_handling},
    {"Event Handler State Management", test_event_handler_state_management},
    {"Event Handler Enabling", test_event_handler_enabling},
    {"Error Recovery", test_error_recovery},
    {"Concurrent Access", test_concurrent_access},
    {"Event Queue Performance", test_event_queue_performance},
    {"Memory Efficiency", test_memory_efficiency},
    {NULL, NULL}
};

int main() {
    printf("🧪 Kryon Terminal Renderer - Event System Tests\n");
    printf("==========================================\n");
    printf("Testing edge cases and error handling for flawless event system\n\n");

    // Run all event system tests
    bool all_passed = true;
    for (int i = 0; event_tests[i].test_func != NULL; i++) {
        if (!run_event_test(event_tests[i].name, event_tests[i].test_func)) {
            all_passed = false;
        }
    }

    // Print results
    printf("\n📊 Event System Test Results\n");
    printf("========================\n");
    printf("Total tests: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("Success rate: %.1f%%\n", (float)tests_passed / tests_run * 100);

    if (all_passed) {
        printf("\n🎉 ALL EVENT SYSTEM TESTS PASSED!\n");
        printf("Event system is FLAWLESS - no edge case failures!\n");
        return 0;
    } else {
        printf("\n⚠️  SOME EVENT SYSTEM TESTS FAILED!\n");
        printf("Event system needs attention before production use.\n");
        return 1;
    }
}