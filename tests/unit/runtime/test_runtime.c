/**
 * @file test_runtime.c
 * @brief Unit tests for Kryon runtime system
 */

#include "runtime.h"
#include "../../src/shared/kryon_mappings.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// Simple null renderer for tests  
static KryonRenderResult test_null_begin_frame(KryonRenderContext** context, KryonColor clear_color) {
    (void)clear_color;
    *context = NULL;
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult test_null_end_frame(KryonRenderContext* context) {
    (void)context;
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult test_null_execute_commands(KryonRenderContext* context, const KryonRenderCommand* commands, size_t count) {
    (void)context; (void)commands; (void)count;
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult test_null_get_input_state(KryonInputState* input_state) {
    memset(input_state, 0, sizeof(KryonInputState));
    return KRYON_RENDER_SUCCESS;
}

static KryonRendererVTable test_null_vtable = {
    .begin_frame = test_null_begin_frame,
    .end_frame = test_null_end_frame,
    .execute_commands = test_null_execute_commands,
    .get_input_state = test_null_get_input_state
};

static KryonRenderer test_null_renderer = {
    .vtable = &test_null_vtable
};

static void setup_test_renderer(KryonRuntime *runtime) {
    if (runtime) {
        runtime->renderer = &test_null_renderer;
    }
}

// Simple test framework
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void test_##name(void); \
    static void run_test_##name(void) { \
        printf("Running test: " #name "... "); \
        tests_run++; \
        test_##name(); \
        tests_passed++; \
        printf("PASSED\n"); \
    } \
    static void test_##name(void)

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("FAILED\n  Assertion failed: " #condition "\n"); \
            printf("  File: %s, Line: %d\n", __FILE__, __LINE__); \
            exit(1); \
        } \
    } while(0)

#define ASSERT_STR_EQ(actual, expected) \
    do { \
        if (strcmp((actual), (expected)) != 0) { \
            printf("FAILED\n  String mismatch:\n"); \
            printf("    Expected: \"%s\"\n", (expected)); \
            printf("    Actual:   \"%s\"\n", (actual)); \
            printf("  File: %s, Line: %d\n", __FILE__, __LINE__); \
            exit(1); \
        } \
    } while(0)

// Test runtime creation and destruction
TEST(runtime_lifecycle) {
    KryonRuntime *runtime = kryon_runtime_create(NULL);
    ASSERT(runtime != NULL);
    
    // Check default configuration
    ASSERT(runtime->config.mode == KRYON_MODE_PRODUCTION);
    ASSERT(runtime->config.enable_hot_reload == false);
    ASSERT(runtime->config.max_elements == 10000);
    
    kryon_runtime_destroy(runtime);
    
    // Test with custom config
    KryonRuntimeConfig config = kryon_runtime_dev_config();
    runtime = kryon_runtime_create(&config);
    ASSERT(runtime != NULL);
    ASSERT(runtime->config.mode == KRYON_MODE_DEVELOPMENT);
    ASSERT(runtime->config.enable_hot_reload == true);
    
    kryon_runtime_destroy(runtime);
}

// Test configuration functions
TEST(runtime_configurations) {
    KryonRuntimeConfig default_config = kryon_runtime_default_config();
    ASSERT(default_config.mode == KRYON_MODE_PRODUCTION);
    ASSERT(default_config.enable_hot_reload == false);
    ASSERT(default_config.enable_profiling == false);
    
    KryonRuntimeConfig dev_config = kryon_runtime_dev_config();
    ASSERT(dev_config.mode == KRYON_MODE_DEVELOPMENT);
    ASSERT(dev_config.enable_hot_reload == true);
    ASSERT(dev_config.enable_profiling == true);
    
    KryonRuntimeConfig prod_config = kryon_runtime_prod_config();
    ASSERT(prod_config.mode == KRYON_MODE_PRODUCTION);
    ASSERT(prod_config.enable_hot_reload == false);
    ASSERT(prod_config.enable_validation == false);
}

// Test element creation and hierarchy
TEST(element_creation) {
    KryonRuntime *runtime = kryon_runtime_create(NULL);
    ASSERT(runtime != NULL);
    
    // Create root element
    KryonElement *root = kryon_element_create(runtime, kryon_get_element_hex("App"), NULL); // App
    ASSERT(root != NULL);
    ASSERT(root->id == 1);
    ASSERT(root->type == kryon_get_element_hex("App"));
    ASSERT(root->parent == NULL);
    ASSERT(root->state == KRYON_ELEMENT_STATE_CREATED);
    ASSERT(root->visible == true);
    ASSERT(root->enabled == true);
    
    // Create child element
    KryonElement *child = kryon_element_create(runtime, kryon_get_element_hex("Button"), root); // Button
    ASSERT(child != NULL);
    ASSERT(child->id == 2);
    ASSERT(child->parent == root);
    ASSERT(root->child_count == 1);
    ASSERT(root->children[0] == child);
    
    // Create another child
    KryonElement *child2 = kryon_element_create(runtime, kryon_get_element_hex("Text"), root); // Text
    ASSERT(child2 != NULL);
    ASSERT(root->child_count == 2);
    ASSERT(root->children[1] == child2);
    
    // Verify element registry
    ASSERT(runtime->element_count == 3);
    ASSERT(runtime->elements[0] == root);
    ASSERT(runtime->elements[1] == child);
    ASSERT(runtime->elements[2] == child2);
    
    kryon_runtime_destroy(runtime);
}

// Test element destruction
TEST(element_destruction) {
    KryonRuntime *runtime = kryon_runtime_create(NULL);
    ASSERT(runtime != NULL);
    
    // Create element hierarchy
    KryonElement *root = kryon_element_create(runtime, kryon_get_element_hex("App"), NULL);
    KryonElement *child1 = kryon_element_create(runtime, kryon_get_element_hex("Column"), root);
    KryonElement *child2 = kryon_element_create(runtime, kryon_get_element_hex("Button"), root);
    KryonElement *grandchild = kryon_element_create(runtime, kryon_get_element_hex("Text"), child1);
    
    ASSERT(runtime->element_count == 4);
    ASSERT(root->child_count == 2);
    ASSERT(child1->child_count == 1);
    
    // Destroy child1 (should also destroy grandchild)
    kryon_element_destroy(runtime, child1);
    
    ASSERT(runtime->element_count == 2);
    ASSERT(root->child_count == 1);
    ASSERT(root->children[0] == child2);
    
    kryon_runtime_destroy(runtime);
}

// Test runtime start/stop
TEST(runtime_execution) {
    KryonRuntime *runtime = kryon_runtime_create(NULL);
    ASSERT(runtime != NULL);
    
    // Should fail without root element
    ASSERT(!kryon_runtime_start(runtime));
    
    // Create root element
    runtime->root = kryon_element_create(runtime, kryon_get_element_hex("App"), NULL);
    ASSERT(runtime->root != NULL);
    
    // Set up test renderer
    setup_test_renderer(runtime);
    
    // Now should succeed
    ASSERT(kryon_runtime_start(runtime));
    ASSERT(runtime->is_running == true);
    
    // Should not start again while running
    ASSERT(!kryon_runtime_start(runtime));
    
    // Stop runtime
    kryon_runtime_stop(runtime);
    ASSERT(runtime->is_running == false);
    
    kryon_runtime_destroy(runtime);
}

// Test runtime update cycle
TEST(runtime_update) {
    KryonRuntime *runtime = kryon_runtime_create(NULL);
    ASSERT(runtime != NULL);
    
    runtime->root = kryon_element_create(runtime, kryon_get_element_hex("App"), NULL);
    setup_test_renderer(runtime);
    ASSERT(kryon_runtime_start(runtime));
    
    // Initial update
    ASSERT(kryon_runtime_update(runtime, 0.016)); // 16ms = ~60fps
    ASSERT(runtime->stats.frame_count == 1);
    ASSERT(runtime->stats.total_time > 0.0);
    
    // Multiple updates
    for (int i = 0; i < 10; i++) {
        ASSERT(kryon_runtime_update(runtime, 0.016));
    }
    ASSERT(runtime->stats.frame_count == 11);
    
    kryon_runtime_destroy(runtime);
}

// Test state management
TEST(state_management) {
    // Create state node
    KryonState *state = kryon_state_create("test", KRYON_STATE_OBJECT);
    ASSERT(state != NULL);
    ASSERT(strcmp(state->key, "test") == 0);
    ASSERT(state->type == KRYON_STATE_OBJECT);
    
    // Create child states
    KryonState *child1 = kryon_state_create("name", KRYON_STATE_STRING);
    ASSERT(child1 != NULL);
    
    KryonState *child2 = kryon_state_create("count", KRYON_STATE_INTEGER);
    ASSERT(child2 != NULL);
    
    // Cleanup
    kryon_state_destroy(child2);
    kryon_state_destroy(child1);
    kryon_state_destroy(state);
}

// Test event handling
TEST(event_handling) {
    KryonRuntime *runtime = kryon_runtime_create(NULL);
    ASSERT(runtime != NULL);
    
    runtime->root = kryon_element_create(runtime, kryon_get_element_hex("App"), NULL);
    setup_test_renderer(runtime);
    ASSERT(kryon_runtime_start(runtime));
    
    // Create test event
    KryonEvent event = {
        .type = KRYON_EVENT_MOUSE_BUTTON_DOWN,
        .timestamp = 0.0
    };
    event.data.mouseButton.button = 0;
    event.data.mouseButton.x = 100.0f;
    event.data.mouseButton.y = 100.0f;
    
    // Add event to queue
    ASSERT(kryon_runtime_handle_event(runtime, &event));
    ASSERT(runtime->event_count == 1);
    
    // Process events during update
    ASSERT(kryon_runtime_update(runtime, 0.016));
    ASSERT(runtime->event_count == 0);
    
    kryon_runtime_destroy(runtime);
}

// Test error handling
TEST(error_handling) {
    KryonRuntime *runtime = kryon_runtime_create(NULL);
    ASSERT(runtime != NULL);
    
    // Initially should have no errors
    size_t error_count;
    const char **errors = kryon_runtime_get_errors(runtime, &error_count);
    ASSERT(error_count == 0);
    
    // Loading non-existent file should produce error
    ASSERT(!kryon_runtime_load_file(runtime, "/non/existent/file.krb"));
    
    errors = kryon_runtime_get_errors(runtime, &error_count);
    ASSERT(error_count > 0);
    ASSERT(errors != NULL);
    
    // Clear errors
    kryon_runtime_clear_errors(runtime);
    errors = kryon_runtime_get_errors(runtime, &error_count);
    ASSERT(error_count == 0);
    
    kryon_runtime_destroy(runtime);
}

// Test element finding
TEST(element_finding) {
    KryonRuntime *runtime = kryon_runtime_create(NULL);
    ASSERT(runtime != NULL);
    
    // Create elements with IDs
    KryonElement *root = kryon_element_create(runtime, kryon_get_element_hex("App"), NULL);
    root->element_id = strdup("app");
    
    KryonElement *button = kryon_element_create(runtime, kryon_get_element_hex("Button"), root);
    button->element_id = strdup("submit-button");
    
    KryonElement *text = kryon_element_create(runtime, kryon_get_element_hex("Text"), root);
    text->element_id = strdup("status-text");
    
    // Find by ID
    KryonElement *found = kryon_element_find_by_id(runtime, "submit-button");
    ASSERT(found == button);
    
    found = kryon_element_find_by_id(runtime, "status-text");
    ASSERT(found == text);
    
    found = kryon_element_find_by_id(runtime, "non-existent");
    ASSERT(found == NULL);
    
    kryon_runtime_destroy(runtime);
}

// Test layout invalidation
TEST(layout_invalidation) {
    KryonRuntime *runtime = kryon_runtime_create(NULL);
    ASSERT(runtime != NULL);
    
    KryonElement *root = kryon_element_create(runtime, kryon_get_element_hex("App"), NULL);
    KryonElement *child = kryon_element_create(runtime, kryon_get_element_hex("Column"), root);
    KryonElement *grandchild = kryon_element_create(runtime, kryon_get_element_hex("Button"), child);
    
    // Initially all need layout
    ASSERT(root->needs_layout == true);
    ASSERT(child->needs_layout == true);
    ASSERT(grandchild->needs_layout == true);
    
    // Clear layout flags
    root->needs_layout = false;
    child->needs_layout = false;
    grandchild->needs_layout = false;
    
    // Invalidate grandchild should propagate up
    kryon_element_invalidate_layout(grandchild);
    ASSERT(grandchild->needs_layout == true);
    ASSERT(child->needs_layout == true);
    ASSERT(root->needs_layout == true);
    
    kryon_runtime_destroy(runtime);
}

// Main test runner
int main(void) {
    printf("=== Kryon Runtime Unit Tests ===\n\n");
    
    run_test_runtime_lifecycle();
    run_test_runtime_configurations();
    run_test_element_creation();
    run_test_element_destruction();
    run_test_runtime_execution();
    run_test_runtime_update();
    run_test_state_management();
    run_test_event_handling();
    run_test_error_handling();
    run_test_element_finding();
    run_test_layout_invalidation();
    
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    
    if (tests_passed == tests_run) {
        printf("All tests PASSED! ✅\n");
        return 0;
    } else {
        printf("Some tests FAILED! ❌\n");
        return 1;
    }
}