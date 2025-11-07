#include "../core/include/kryon.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// Test Framework
// ============================================================================

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("✓ %s\n", message); \
        } else { \
            tests_failed++; \
            printf("✗ %s\n", message); \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL(expected, actual, message) \
    TEST_ASSERT((expected) == (actual), message)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT((ptr) != NULL, message)

#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT((ptr) == NULL, message)

// ============================================================================
// Mock Renderer for Testing
// ============================================================================

typedef struct {
    uint32_t draw_rect_count;
    uint32_t draw_text_count;
    uint32_t draw_line_count;
    kryon_cmd_buf_t last_commands[10];
    uint32_t last_command_count;
} mock_renderer_state_t;

static bool mock_init(kryon_renderer_t* renderer, void* native_window) {
    (void)native_window;
    mock_renderer_state_t* state = (mock_renderer_state_t*)malloc(sizeof(mock_renderer_state_t));
    if (state == NULL) return false;

    memset(state, 0, sizeof(mock_renderer_state_t));
    renderer->backend_data = state;
    renderer->width = 800;
    renderer->height = 600;
    return true;
}

static void mock_shutdown(kryon_renderer_t* renderer) {
    if (renderer && renderer->backend_data) {
        free(renderer->backend_data);
        renderer->backend_data = NULL;
    }
}

static void mock_execute_commands(kryon_renderer_t* renderer, kryon_cmd_buf_t* buf) {
    if (renderer == NULL || renderer->backend_data == NULL || buf == NULL) return;

    mock_renderer_state_t* state = (mock_renderer_state_t*)renderer->backend_data;

    // Count commands by type
    kryon_cmd_iterator_t iter = kryon_cmd_iter_create(buf);
    kryon_command_t cmd;

    state->last_command_count = 0;
    while (kryon_cmd_iter_has_next(&iter) && state->last_command_count < 10) {
        if (kryon_cmd_iter_next(&iter, &cmd)) {
            state->last_commands[state->last_command_count++] = cmd;

            switch (cmd.type) {
                case KRYON_CMD_DRAW_RECT:
                    state->draw_rect_count++;
                    break;
                case KRYON_CMD_DRAW_TEXT:
                    state->draw_text_count++;
                    break;
                case KRYON_CMD_DRAW_LINE:
                    state->draw_line_count++;
                    break;
                default:
                    break;
            }
        }
    }
}

static const kryon_renderer_ops_t mock_renderer_ops = {
    .init = mock_init,
    .shutdown = mock_shutdown,
    .begin_frame = NULL,
    .end_frame = NULL,
    .execute_commands = mock_execute_commands,
    .swap_buffers = NULL,
    .get_dimensions = NULL,
    .set_clear_color = NULL
};

// ============================================================================
// Component Tests
// ============================================================================

static void test_component_creation() {
    printf("\n=== Component Creation Tests ===\n");

    kryon_component_t* container = kryon_component_create(&kryon_container_ops, NULL);
    TEST_ASSERT_NOT_NULL(container, "Container component created");

    TEST_ASSERT_EQUAL(0, kryon_component_get_child_count(container), "Container has no children initially");
    TEST_ASSERT_EQUAL(true, kryon_component_is_dirty(container), "New component is dirty");
    TEST_ASSERT_EQUAL(true, container->visible, "New component is visible");

    kryon_component_destroy(container);
}

static void test_component_tree() {
    printf("\n=== Component Tree Tests ===\n");

    // Create parent container
    kryon_component_t* parent = kryon_component_create(&kryon_container_ops, NULL);
    TEST_ASSERT_NOT_NULL(parent, "Parent container created");

    // Create child components
    kryon_text_state_t text_state = {
        .text = "Hello World",
        .font_id = 0,
        .max_length = 32,
        .word_wrap = false
    };

    kryon_component_t* text = kryon_component_create(&kryon_text_ops, &text_state);
    TEST_ASSERT_NOT_NULL(text, "Text component created");

    kryon_button_state_t button_state = {
        .text = "Click Me",
        .font_id = 0,
        .pressed = false,
        .on_click = NULL
    };

    kryon_component_t* button = kryon_component_create(&kryon_button_ops, &button_state);
    TEST_ASSERT_NOT_NULL(button, "Button component created");

    // Add children to parent
    TEST_ASSERT_EQUAL(true, kryon_component_add_child(parent, text), "Added text child to parent");
    TEST_ASSERT_EQUAL(true, kryon_component_add_child(parent, button), "Added button child to parent");

    TEST_ASSERT_EQUAL(2, kryon_component_get_child_count(parent), "Parent has 2 children");
    TEST_ASSERT_EQUAL(parent, kryon_component_get_parent(text), "Text has correct parent");
    TEST_ASSERT_EQUAL(parent, kryon_component_get_parent(button), "Button has correct parent");

    // Test child retrieval
    kryon_component_t* child1 = kryon_component_get_child(parent, 0);
    kryon_component_t* child2 = kryon_component_get_child(parent, 1);
    TEST_ASSERT_EQUAL(text, child1, "First child is text component");
    TEST_ASSERT_EQUAL(button, child2, "Second child is button component");

    // Test removing child
    kryon_component_remove_child(parent, text);
    TEST_ASSERT_EQUAL(1, kryon_component_get_child_count(parent), "Parent has 1 child after removal");
    TEST_ASSERT_NULL(kryon_component_get_parent(text), "Removed child has no parent");

    // Cleanup
    kryon_component_destroy(parent); // Should destroy button too
    kryon_component_destroy(text);  // Destroy separately since it was removed
}

// ============================================================================
// Event System Tests
// ============================================================================

static bool click_handler_called = false;
static kryon_component_t* last_event_component = NULL;

static void test_click_handler(kryon_component_t* component, kryon_event_t* event) {
    click_handler_called = true;
    last_event_component = component;
    (void)event;
}

static void test_event_system() {
    printf("\n=== Event System Tests ===\n");

    click_handler_called = false;
    last_event_component = NULL;

    // Create button with click handler
    kryon_button_state_t button_state = {
        .text = "Test Button",
        .font_id = 0,
        .pressed = false,
        .on_click = test_click_handler
    };

    kryon_component_t* button = kryon_component_create(&kryon_button_ops, &button_state);
    TEST_ASSERT_NOT_NULL(button, "Button component created");

    // Add custom event handler
    TEST_ASSERT_EQUAL(true, kryon_component_add_event_handler(button, test_click_handler),
                     "Added event handler to button");

    // Create and send click event
    kryon_event_t click_event = kryon_event_click(50, 25, 1);
    kryon_component_send_event(button, &click_event);

    TEST_ASSERT_EQUAL(true, click_handler_called, "Click handler was called");
    TEST_ASSERT_EQUAL(button, last_event_component, "Correct component received event");

    kryon_component_destroy(button);
}

static void test_event_targeting() {
    printf("\n=== Event Targeting Tests ===\n");

    // Create component hierarchy
    kryon_component_t* root = kryon_component_create(&kryon_container_ops, NULL);
    kryon_component_t* child = kryon_component_create(&kryon_container_ops, NULL);
    kryon_button_state_t button_state = {
        .text = "Target Button",
        .font_id = 0,
        .pressed = false,
        .on_click = NULL
    };
    kryon_component_t* button = kryon_component_create(&kryon_button_ops, &button_state);

    kryon_component_add_child(root, child);
    kryon_component_add_child(child, button);

    // Set component positions
    kryon_component_set_bounds(root, 0, 0, 200, 200);
    kryon_component_set_bounds(child, 50, 50, 100, 100);
    kryon_component_set_bounds(button, 25, 25, 50, 30);

    // Test point-in-component detection
    TEST_ASSERT_EQUAL(true, kryon_event_is_point_in_component(root, 100, 100),
                     "Point (100,100) is in root component");
    TEST_ASSERT_EQUAL(true, kryon_event_is_point_in_component(child, 100, 100),
                     "Point (100,100) is in child component");
    TEST_ASSERT_EQUAL(false, kryon_event_is_point_in_component(button, 100, 100),
                     "Point (100,100) is not in button component");

    // Test finding component at point
    kryon_component_t* target = kryon_event_find_target_at_point(root, 100, 100);
    TEST_ASSERT_EQUAL(child, target, "Found correct component at point (100,100)");

    kryon_component_destroy(root);
}

// ============================================================================
// Command Buffer Tests
// ============================================================================

static void test_command_buffer() {
    printf("\n=== Command Buffer Tests ===\n");

    kryon_cmd_buf_t buf;
    kryon_cmd_buf_init(&buf);

    TEST_ASSERT_EQUAL(0, kryon_cmd_buf_count(&buf), "Empty buffer has 0 commands");
    TEST_ASSERT_EQUAL(true, kryon_cmd_buf_is_empty(&buf), "Empty buffer is empty");
    TEST_ASSERT_EQUAL(false, kryon_cmd_buf_is_full(&buf), "Empty buffer is not full");

    // Add drawing commands
    TEST_ASSERT_EQUAL(true, kryon_draw_rect(&buf, 10, 20, 100, 50, 0xFF0000FF),
                     "Added draw rect command");
    TEST_ASSERT_EQUAL(true, kryon_draw_text(&buf, "Hello", 50, 75, 0, 0x00FF00FF),
                     "Added draw text command");
    TEST_ASSERT_EQUAL(true, kryon_draw_line(&buf, 0, 0, 100, 100, 0x0000FFFF),
                     "Added draw line command");

    TEST_ASSERT_EQUAL(false, kryon_cmd_buf_is_empty(&buf), "Buffer with commands is not empty");

    // Read commands back
    kryon_command_t cmd;
    TEST_ASSERT_EQUAL(true, kryon_cmd_buf_pop(&buf, &cmd), "Popped first command");
    TEST_ASSERT_EQUAL(KRYON_CMD_DRAW_RECT, cmd.type, "First command is draw rect");
    TEST_ASSERT_EQUAL(10, cmd.data.draw_rect.x, "Draw rect has correct x");
    TEST_ASSERT_EQUAL(20, cmd.data.draw_rect.y, "Draw rect has correct y");
    TEST_ASSERT_EQUAL(100, cmd.data.draw_rect.w, "Draw rect has correct width");
    TEST_ASSERT_EQUAL(50, cmd.data.draw_rect.h, "Draw rect has correct height");
    TEST_ASSERT_EQUAL(0xFF0000FF, cmd.data.draw_rect.color, "Draw rect has correct color");

    TEST_ASSERT_EQUAL(true, kryon_cmd_buf_pop(&buf, &cmd), "Popped second command");
    TEST_ASSERT_EQUAL(KRYON_CMD_DRAW_TEXT, cmd.type, "Second command is draw text");

    TEST_ASSERT_EQUAL(true, kryon_cmd_buf_pop(&buf, &cmd), "Popped third command");
    TEST_ASSERT_EQUAL(KRYON_CMD_DRAW_LINE, cmd.type, "Third command is draw line");

    TEST_ASSERT_EQUAL(true, kryon_cmd_buf_is_empty(&buf), "Buffer is empty after reading all commands");
    TEST_ASSERT_EQUAL(false, kryon_cmd_buf_pop(&buf, &cmd), "Cannot pop from empty buffer");
}

// ============================================================================
// Layout Tests
// ============================================================================

static void test_layout_system() {
    printf("\n=== Layout System Tests ===\n");

    // Create container with children
    kryon_component_t* container = kryon_component_create(&kryon_container_ops, NULL);
    kryon_component_set_bounds(container, 0, 0, 300, 200);

    // Add some children
    for (int i = 0; i < 3; i++) {
        kryon_text_state_t text_state = {
            .text = "Child",
            .font_id = 0,
            .max_length = 32,
            .word_wrap = false
        };
        kryon_component_t* child = kryon_component_create(&kryon_text_ops, &text_state);
        kryon_component_add_child(container, child);
    }

    // Perform layout
    kryon_layout_tree(container, 300, 200);

    TEST_ASSERT_EQUAL(false, kryon_component_is_dirty(container), "Container is clean after layout");

    // Check that children have been positioned
    for (int i = 0; i < 3; i++) {
        kryon_component_t* child = kryon_component_get_child(container, i);
        TEST_ASSERT(child->x > 0, "Child has x position set");
        TEST_ASSERT(child->y > 0, "Child has y position set");
        TEST_ASSERT(child->width > 0, "Child has width set");
        TEST_ASSERT(child->height > 0, "Child has height set");
    }

    kryon_component_destroy(container);
}

// ============================================================================
// Renderer Tests
// ============================================================================

static void test_renderer_system() {
    printf("\n=== Renderer System Tests ===\n");

    // Create mock renderer
    kryon_renderer_t* renderer = kryon_renderer_create(&mock_renderer_ops);
    TEST_ASSERT_NOT_NULL(renderer, "Renderer created");

    // Initialize renderer
    TEST_ASSERT_EQUAL(true, kryon_renderer_init(renderer, NULL), "Renderer initialized");

    // Create test component
    kryon_component_t* container = kryon_component_create(&kryon_container_ops, NULL);
    kryon_component_set_bounds(container, 0, 0, 100, 100);

    // Set background color
    kryon_component_set_background_color(container, 0xFF0000FF);

    // Render frame
    kryon_render_frame(renderer, container);

    // Check that commands were executed
    mock_renderer_state_t* state = (mock_renderer_state_t*)renderer->backend_data;
    TEST_ASSERT(state->draw_rect_count > 0, "Draw rect commands were executed");

    // Cleanup
    kryon_component_destroy(container);
    kryon_renderer_destroy(renderer);
}

// ============================================================================
// Utility Tests
// ============================================================================

static void test_color_utilities() {
    printf("\n=== Color Utility Tests ===\n");

    uint32_t red = kryon_color_rgb(255, 0, 0);
    TEST_ASSERT_EQUAL(0xFF0000FF, red, "RGB red color created correctly");

    uint32_t blue = kryon_color_rgba(0, 0, 255, 128);
    TEST_ASSERT_EQUAL(0x0000FF80, blue, "RGBA blue color created correctly");

    uint8_t r, g, b, a;
    kryon_color_get_components(blue, &r, &g, &b, &a);
    TEST_ASSERT_EQUAL(0, r, "Red component extracted correctly");
    TEST_ASSERT_EQUAL(0, g, "Green component extracted correctly");
    TEST_ASSERT_EQUAL(255, b, "Blue component extracted correctly");
    TEST_ASSERT_EQUAL(128, a, "Alpha component extracted correctly");
}

static void test_fixed_point_arithmetic() {
    printf("\n=== Fixed-Point Arithmetic Tests ===\n");

    // Test integer conversion
    kryon_fp_t fp_int = kryon_fp_from_int(42);
    TEST_ASSERT_EQUAL(42, KRYON_FP_TO_INT(fp_int), "Integer conversion works");

    // Test addition
    kryon_fp_t a = kryon_fp_from_int(10);
    kryon_fp_t b = kryon_fp_from_int(5);
    kryon_fp_t sum = kryon_fp_add(a, b);
    TEST_ASSERT_EQUAL(15, KRYON_FP_TO_INT(sum), "Addition works");

    // Test multiplication
    kryon_fp_t product = kryon_fp_mul(kryon_fp_from_int(6), kryon_fp_from_int(7));
    TEST_ASSERT_EQUAL(42, KRYON_FP_TO_INT(product), "Multiplication works");

    // Test division
    kryon_fp_t quotient = kryon_fp_div(kryon_fp_from_int(20), kryon_fp_from_int(4));
    TEST_ASSERT_EQUAL(5, KRYON_FP_TO_INT(quotient), "Division works");

    // Test clamp
    kryon_fp_t clamped = kryon_fp_clamp(kryon_fp_from_int(15), kryon_fp_from_int(10), kryon_fp_from_int(20));
    TEST_ASSERT_EQUAL(15, KRYON_FP_TO_INT(clamped), "Clamp works (within range)");

    clamped = kryon_fp_clamp(kryon_fp_from_int(5), kryon_fp_from_int(10), kryon_fp_from_int(20));
    TEST_ASSERT_EQUAL(10, KRYON_FP_TO_INT(clamped), "Clamp works (below range)");

    clamped = kryon_fp_clamp(kryon_fp_from_int(25), kryon_fp_from_int(10), kryon_fp_from_int(20));
    TEST_ASSERT_EQUAL(20, KRYON_FP_TO_INT(clamped), "Clamp works (above range)");
}

// ============================================================================
// Memory Tests (MCU-specific)
// ============================================================================

static void test_memory_constraints() {
    printf("\n=== Memory Constraint Tests ===\n");

    // Test that we can create the expected number of components
    kryon_component_t* components[KRYON_MAX_COMPONENTS];
    int created = 0;

    for (int i = 0; i < KRYON_MAX_COMPONENTS; i++) {
        components[i] = kryon_component_create(&kryon_container_ops, NULL);
        if (components[i] != NULL) {
            created++;
        } else {
            break;
        }
    }

    TEST_ASSERT(created > 0, "At least one component created");

#if KRYON_NO_HEAP
    // On MCU, we should be able to create the maximum number
    TEST_ASSERT_EQUAL(KRYON_MAX_COMPONENTS, created, "Created maximum number of components");
#endif

    // Cleanup
    for (int i = 0; i < created; i++) {
        kryon_component_destroy(components[i]);
    }
}

// ============================================================================
// Integration Test
// ============================================================================

static void test_simple_ui() {
    printf("\n=== Integration Test: Simple UI ===\n");

    // Create a simple UI hierarchy
    kryon_component_t* root = kryon_component_create(&kryon_container_ops, NULL);
    kryon_component_set_bounds(root, 0, 0, 320, 240);
    kryon_component_set_background_color(root, 0x2D2D2DFF);

    // Add a text label
    kryon_text_state_t label_state = {
        .text = "Hello Kryon!",
        .font_id = 0,
        .max_length = 32,
        .word_wrap = false
    };
    kryon_component_t* label = kryon_component_create(&kryon_text_ops, &label_state);
    kryon_component_add_child(root, label);

    // Add a button
    kryon_button_state_t button_state = {
        .text = "Click Me",
        .font_id = 0,
        .pressed = false,
        .on_click = NULL
    };
    kryon_component_t* button = kryon_component_create(&kryon_button_ops, &button_state);
    kryon_component_add_child(root, button);

    // Perform layout
    kryon_layout_tree(root, 320, 240);
    TEST_ASSERT_EQUAL(false, kryon_component_is_dirty(root), "Root is clean after layout");

    // Render with mock renderer
    kryon_renderer_t* renderer = kryon_renderer_create(&mock_renderer_ops);
    kryon_renderer_init(renderer, NULL);

    kryon_render_frame(renderer, root);

    mock_renderer_state_t* state = (mock_renderer_state_t*)renderer->backend_data;
    TEST_ASSERT(state->draw_rect_count > 0, "Background was drawn");
    TEST_ASSERT(state->draw_text_count > 0, "Text was drawn");

    // Test event handling
    kryon_event_t click_event = kryon_event_click(160, 120, 1);
    kryon_component_send_event(root, &click_event);

    // Cleanup
    kryon_component_destroy(root);
    kryon_renderer_destroy(renderer);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char* argv[]) {
    printf("Kryon C Core Test Suite\n");
    printf("=======================\n");

    // Print configuration
    printf("Configuration:\n");
    printf("  Max Components: %d\n", KRYON_MAX_COMPONENTS);
    printf("  Command Buffer Size: %d\n", KRYON_CMD_BUF_SIZE);
    printf("  No Heap: %s\n", KRYON_NO_HEAP ? "Yes" : "No");
    printf("  No Float: %s\n", KRYON_NO_FLOAT ? "Yes" : "No");
    printf("  Target Platform: %d\n", KRYON_TARGET_PLATFORM);

    // Run all tests
    test_component_creation();
    test_component_tree();
    test_event_system();
    test_event_targeting();
    test_command_buffer();
    test_layout_system();
    test_renderer_system();
    test_color_utilities();
    test_fixed_point_arithmetic();
    test_memory_constraints();
    test_simple_ui();

    // Print results
    printf("\n=== Test Results ===\n");
    printf("Tests Run: %d\n", tests_run);
    printf("Tests Passed: %d\n", tests_passed);
    printf("Tests Failed: %d\n", tests_failed);

    if (tests_failed == 0) {
        printf("✓ All tests passed!\n");
        return 0;
    } else {
        printf("✗ Some tests failed!\n");
        return 1;
    }
}