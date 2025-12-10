#ifndef KRYON_DESKTOP_TEST_EVENTS_H
#define KRYON_DESKTOP_TEST_EVENTS_H

#include "../../ir/ir_core.h"
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declaration
typedef struct DesktopIRRenderer DesktopIRRenderer;

typedef enum {
    TEST_EVENT_WAIT,
    TEST_EVENT_CLICK,
    TEST_EVENT_KEY_PRESS,
    TEST_EVENT_TEXT_INPUT,
    TEST_EVENT_MOUSE_MOVE,
    TEST_EVENT_SCREENSHOT
} TestEventType;

typedef struct {
    TestEventType type;

    // Wait event data
    int frames_to_wait;

    // Click event data
    uint32_t component_id;
    int x, y;

    // Key event data
    SDL_Keycode key;

    // Text input data
    char* text;

    // Screenshot data
    char* screenshot_path;
} TestEvent;

typedef struct {
    TestEvent* events;
    size_t count;
    size_t current_index;
    int frames_waited;
    bool enabled;
} TestEventQueue;

/**
 * Initialize test queue from JSON file
 * @param filepath Path to JSON file containing test events
 * @return Initialized test queue, or NULL on error
 */
TestEventQueue* test_queue_init_from_file(const char* filepath);

/**
 * Process test events (called every frame)
 * @param queue Test event queue
 * @param renderer Desktop renderer for component lookup and screenshot capture
 */
void test_queue_process(TestEventQueue* queue, DesktopIRRenderer* renderer);

/**
 * Free test event queue and all associated resources
 * @param queue Test event queue to free
 */
void test_queue_free(TestEventQueue* queue);

#endif // KRYON_DESKTOP_TEST_EVENTS_H
