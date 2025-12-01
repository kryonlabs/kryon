/*
 * Kryon IR Serialization Round-Trip Tests
 * Tests complete serialization/deserialization of all v2.0 properties
 */

#define _POSIX_C_SOURCE 200809L  // For strdup

#include "../ir_builder.h"
#include "../ir_serialization.h"
#include "../ir_validation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

// Test counters
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) do { \
    if (!(condition)) { \
        printf("  ✗ FAILED: %s\n", message); \
        tests_failed++; \
        return false; \
    } \
} while(0)

#define TEST_SUCCESS(message) do { \
    printf("  ✓ %s\n", message); \
    tests_passed++; \
    return true; \
} while(0)

// Helper: Compare floats with epsilon
static bool float_equal(float a, float b) {
    return fabs(a - b) < 0.001f;
}

// Helper: Compare dimensions
static bool dimension_equal(IRDimension a, IRDimension b) {
    return a.type == b.type && float_equal(a.value, b.value);
}

// Helper: Compare colors
static bool color_equal(IRColor a, IRColor b) {
    if (a.type != b.type) return false;
    if (a.type == IR_COLOR_SOLID) {
        return a.data.r == b.data.r && a.data.g == b.data.g &&
               a.data.b == b.data.b && a.data.a == b.data.a;
    }
    // For other color types, just check type match for now
    return true;
}

// Helper: Compare spacing
static bool spacing_equal(IRSpacing a, IRSpacing b) {
    return float_equal(a.top, b.top) &&
           float_equal(a.right, b.right) &&
           float_equal(a.bottom, b.bottom) &&
           float_equal(a.left, b.left);
}

// Test 1: Basic component properties
static bool test_basic_properties(void) {
    printf("\n[Test 1] Basic Component Properties\n");

    // Create component with basic properties
    IRComponent* comp = ir_create_component(IR_COMPONENT_CONTAINER);
    comp->tag = strdup("test-container");
    comp->text_content = strdup("Hello, World!");

    // Serialize
    IRBuffer* buffer = ir_serialize_binary(comp);
    TEST_ASSERT(buffer != NULL, "Serialization failed");
    TEST_ASSERT(buffer->size > 0, "Buffer is empty");

    // Deserialize
    ir_buffer_seek(buffer, 0);
    IRComponent* deserialized = ir_deserialize_binary(buffer);
    TEST_ASSERT(deserialized != NULL, "Deserialization failed");

    // Verify
    TEST_ASSERT(deserialized->type == IR_COMPONENT_CONTAINER, "Component type mismatch");
    TEST_ASSERT(strcmp(deserialized->tag, "test-container") == 0, "Tag mismatch");
    TEST_ASSERT(strcmp(deserialized->text_content, "Hello, World!") == 0, "Text content mismatch");

    ir_destroy_component(comp);
    ir_destroy_component(deserialized);
    ir_buffer_destroy(buffer);

    TEST_SUCCESS("Basic properties serialization");
}

// Test 2: Style properties (dimensions, colors)
static bool test_style_properties(void) {
    printf("\n[Test 2] Style Properties\n");

    IRComponent* comp = ir_create_component(IR_COMPONENT_CONTAINER);
    comp->style = ir_create_style();

    // Set various dimensions
    comp->style->width = (IRDimension){IR_DIMENSION_PX, 800.0f};
    comp->style->height = (IRDimension){IR_DIMENSION_PERCENT, 50.0f};

    // Set colors
    comp->style->background = IR_COLOR_RGBA(255, 128, 64, 255);
    comp->style->border_color = IR_COLOR_RGBA(0, 0, 0, 128);
    comp->style->font.color = IR_COLOR_RGBA(255, 255, 255, 255);

    // Set font properties
    comp->style->font.size = 16.0;
    comp->style->font.weight = 700;
    comp->style->font.line_height = 1.5f;
    comp->style->font.letter_spacing = 0.05f;

    // Serialize
    IRBuffer* buffer = ir_serialize_binary(comp);
    TEST_ASSERT(buffer != NULL, "Serialization failed");

    // Deserialize
    ir_buffer_seek(buffer, 0);
    IRComponent* deserialized = ir_deserialize_binary(buffer);
    TEST_ASSERT(deserialized != NULL, "Deserialization failed");
    TEST_ASSERT(deserialized->style != NULL, "Style not deserialized");

    // Verify dimensions
    TEST_ASSERT(dimension_equal(deserialized->style->width, comp->style->width), "Width mismatch");
    TEST_ASSERT(dimension_equal(deserialized->style->height, comp->style->height), "Height mismatch");

    // Verify colors
    TEST_ASSERT(color_equal(deserialized->style->background, comp->style->background), "Background color mismatch");
    TEST_ASSERT(color_equal(deserialized->style->border_color, comp->style->border_color), "Border color mismatch");
    TEST_ASSERT(color_equal(deserialized->style->font.color, comp->style->font.color), "Font color mismatch");

    // Verify font properties
    TEST_ASSERT(float_equal(deserialized->style->font.size, comp->style->font.size), "Font size mismatch");
    TEST_ASSERT(deserialized->style->font.weight == comp->style->font.weight, "Font weight mismatch");
    TEST_ASSERT(float_equal(deserialized->style->font.line_height, comp->style->font.line_height), "Line height mismatch");
    TEST_ASSERT(float_equal(deserialized->style->font.letter_spacing, comp->style->font.letter_spacing), "Letter spacing mismatch");

    ir_destroy_component(comp);
    ir_destroy_component(deserialized);
    ir_buffer_destroy(buffer);

    TEST_SUCCESS("Style properties serialization");
}

// Test 3: Layout properties (flexbox, grid)
static bool test_layout_properties(void) {
    printf("\n[Test 3] Layout Properties\n");

    IRComponent* comp = ir_create_component(IR_COMPONENT_CONTAINER);
    comp->layout = ir_create_layout();

    // Set flexbox properties
    comp->layout->mode = IR_LAYOUT_MODE_FLEX;
    comp->layout->flex.direction = 1; // Column
    comp->layout->flex.wrap = 1; // Wrap
    comp->layout->flex.justify_content = 2; // Center
    comp->layout->flex.main_axis = 1; // Center
    comp->layout->flex.cross_axis = 2; // Stretch
    comp->layout->flex.gap = 16;
    comp->layout->flex.grow = 1;
    comp->layout->flex.shrink = 0;

    // Set margins and padding
    comp->layout->margin = (IRSpacing){
        .top = 10.0f,
        .right = 20.0f,
        .bottom = 10.0f,
        .left = 20.0f
    };

    comp->layout->padding = (IRSpacing){
        .top = 5.0f,
        .right = 5.0f,
        .bottom = 5.0f,
        .left = 5.0f
    };

    // Set constraints
    comp->layout->min_width = (IRDimension){IR_DIMENSION_PX, 100.0f};
    comp->layout->max_width = (IRDimension){IR_DIMENSION_PX, 800.0f};
    comp->layout->aspect_ratio = 16.0f / 9.0f;

    // Serialize
    IRBuffer* buffer = ir_serialize_binary(comp);
    TEST_ASSERT(buffer != NULL, "Serialization failed");

    // Deserialize
    ir_buffer_seek(buffer, 0);
    IRComponent* deserialized = ir_deserialize_binary(buffer);
    TEST_ASSERT(deserialized != NULL, "Deserialization failed");
    TEST_ASSERT(deserialized->layout != NULL, "Layout not deserialized");

    // Verify layout mode
    TEST_ASSERT(deserialized->layout->mode == IR_LAYOUT_MODE_FLEX, "Layout mode mismatch");

    // Verify flexbox properties
    TEST_ASSERT(deserialized->layout->flex.direction == comp->layout->flex.direction, "Flex direction mismatch");
    TEST_ASSERT(deserialized->layout->flex.wrap == comp->layout->flex.wrap, "Flex wrap mismatch");
    TEST_ASSERT(deserialized->layout->flex.justify_content == comp->layout->flex.justify_content, "Justify content mismatch");
    TEST_ASSERT(deserialized->layout->flex.gap == comp->layout->flex.gap, "Gap mismatch");

    // Verify spacing
    TEST_ASSERT(spacing_equal(deserialized->layout->margin, comp->layout->margin), "Margin mismatch");
    TEST_ASSERT(spacing_equal(deserialized->layout->padding, comp->layout->padding), "Padding mismatch");

    // Verify constraints
    TEST_ASSERT(dimension_equal(deserialized->layout->min_width, comp->layout->min_width), "Min width mismatch");
    TEST_ASSERT(dimension_equal(deserialized->layout->max_width, comp->layout->max_width), "Max width mismatch");
    TEST_ASSERT(float_equal(deserialized->layout->aspect_ratio, comp->layout->aspect_ratio), "Aspect ratio mismatch");

    ir_destroy_component(comp);
    ir_destroy_component(deserialized);
    ir_buffer_destroy(buffer);

    TEST_SUCCESS("Layout properties serialization");
}

// Test 4: Component tree with children
static bool test_component_tree(void) {
    printf("\n[Test 4] Component Tree with Children\n");

    // Create root
    IRComponent* root = ir_create_component(IR_COMPONENT_CONTAINER);
    root->tag = strdup("root");

    // Add children
    for (int i = 0; i < 5; i++) {
        IRComponent* child = ir_create_component(IR_COMPONENT_TEXT);
        char tag[32];
        snprintf(tag, sizeof(tag), "child-%d", i);
        child->tag = strdup(tag);
        child->text_content = strdup("Child content");
        ir_add_child(root, child);
    }

    TEST_ASSERT(root->child_count == 5, "Root should have 5 children");

    // Serialize
    IRBuffer* buffer = ir_serialize_binary(root);
    TEST_ASSERT(buffer != NULL, "Serialization failed");

    // Deserialize
    ir_buffer_seek(buffer, 0);
    IRComponent* deserialized = ir_deserialize_binary(buffer);
    TEST_ASSERT(deserialized != NULL, "Deserialization failed");

    // Verify tree structure
    TEST_ASSERT(deserialized->child_count == 5, "Child count mismatch");

    for (size_t i = 0; i < deserialized->child_count; i++) {
        IRComponent* child = deserialized->children[i];
        TEST_ASSERT(child != NULL, "Child is NULL");
        TEST_ASSERT(child->parent == deserialized, "Parent pointer not set correctly");
        TEST_ASSERT(child->type == IR_COMPONENT_TEXT, "Child type mismatch");
        TEST_ASSERT(child->text_content != NULL, "Child text content is NULL");
    }

    ir_destroy_component(root);
    ir_destroy_component(deserialized);
    ir_buffer_destroy(buffer);

    TEST_SUCCESS("Component tree serialization");
}

// Test 5: Deep nesting
static bool test_deep_nesting(void) {
    printf("\n[Test 5] Deep Component Nesting\n");

    // Create deeply nested tree
    IRComponent* root = ir_create_component(IR_COMPONENT_CONTAINER);
    IRComponent* current = root;

    const int depth = 50;
    for (int i = 0; i < depth; i++) {
        IRComponent* child = ir_create_component(IR_COMPONENT_CONTAINER);
        char tag[32];
        snprintf(tag, sizeof(tag), "level-%d", i);
        child->tag = strdup(tag);
        ir_add_child(current, child);
        current = child;
    }

    // Serialize
    IRBuffer* buffer = ir_serialize_binary(root);
    TEST_ASSERT(buffer != NULL, "Serialization failed");

    // Deserialize
    ir_buffer_seek(buffer, 0);
    IRComponent* deserialized = ir_deserialize_binary(buffer);
    TEST_ASSERT(deserialized != NULL, "Deserialization failed");

    // Verify depth
    current = deserialized;
    int verified_depth = 0;
    while (current->child_count > 0) {
        current = current->children[0];
        verified_depth++;
    }

    TEST_ASSERT(verified_depth == depth, "Nesting depth mismatch");

    ir_destroy_component(root);
    ir_destroy_component(deserialized);
    ir_buffer_destroy(buffer);

    TEST_SUCCESS("Deep nesting serialization");
}

// Test 6: Empty component
static bool test_empty_component(void) {
    printf("\n[Test 6] Empty Component\n");

    IRComponent* comp = ir_create_component(IR_COMPONENT_CONTAINER);
    // Leave everything at defaults

    // Serialize
    IRBuffer* buffer = ir_serialize_binary(comp);
    TEST_ASSERT(buffer != NULL, "Serialization failed");

    // Deserialize
    ir_buffer_seek(buffer, 0);
    IRComponent* deserialized = ir_deserialize_binary(buffer);
    TEST_ASSERT(deserialized != NULL, "Deserialization failed");

    TEST_ASSERT(deserialized->type == IR_COMPONENT_CONTAINER, "Type mismatch");
    TEST_ASSERT(deserialized->child_count == 0, "Should have no children");
    TEST_ASSERT(deserialized->style == NULL || deserialized->style->width.type == IR_DIMENSION_AUTO, "Should have default style");

    ir_destroy_component(comp);
    ir_destroy_component(deserialized);
    ir_buffer_destroy(buffer);

    TEST_SUCCESS("Empty component serialization");
}

// Test 7: Validation integration
static bool test_validation_integration(void) {
    printf("\n[Test 7] Validation Integration\n");

    // Create valid component
    IRComponent* comp = ir_create_component(IR_COMPONENT_CONTAINER);
    comp->style = ir_create_style();
    comp->style->width = (IRDimension){IR_DIMENSION_PX, 800.0f};

    // Serialize
    IRBuffer* buffer = ir_serialize_binary(comp);
    TEST_ASSERT(buffer != NULL, "Serialization failed");

    // Validate buffer
    IRValidationOptions options = ir_default_validation_options();
    options.enable_crc_check = false; // Disable CRC for now (not written by default)

    IRValidationResult* result = ir_validate_binary_complete(buffer, &options);
    TEST_ASSERT(result != NULL, "Validation result is NULL");

    if (!result->passed) {
        printf("  Validation failed with %zu issues:\n", result->issue_count);
        ir_validation_print_result(result);
    }

    TEST_ASSERT(result->passed, "Validation should pass for valid component");

    ir_validation_result_destroy(result);
    ir_destroy_component(comp);
    ir_buffer_destroy(buffer);

    TEST_SUCCESS("Validation integration");
}

// Test 8: Large text content
static bool test_large_text(void) {
    printf("\n[Test 8] Large Text Content\n");

    IRComponent* comp = ir_create_component(IR_COMPONENT_TEXT);

    // Create large text (10KB)
    const size_t text_size = 10000;
    char* large_text = malloc(text_size + 1);
    for (size_t i = 0; i < text_size; i++) {
        large_text[i] = 'A' + (i % 26);
    }
    large_text[text_size] = '\0';

    comp->text_content = large_text;

    // Serialize
    IRBuffer* buffer = ir_serialize_binary(comp);
    TEST_ASSERT(buffer != NULL, "Serialization failed");
    TEST_ASSERT(buffer->size > text_size, "Buffer should be larger than text");

    // Deserialize
    ir_buffer_seek(buffer, 0);
    IRComponent* deserialized = ir_deserialize_binary(buffer);
    TEST_ASSERT(deserialized != NULL, "Deserialization failed");
    TEST_ASSERT(deserialized->text_content != NULL, "Text content is NULL");
    TEST_ASSERT(strlen(deserialized->text_content) == text_size, "Text size mismatch");
    TEST_ASSERT(strcmp(deserialized->text_content, large_text) == 0, "Text content mismatch");

    ir_destroy_component(comp);
    ir_destroy_component(deserialized);
    ir_buffer_destroy(buffer);

    TEST_SUCCESS("Large text content serialization");
}

// Test 9: Multiple dimension types
static bool test_dimension_types(void) {
    printf("\n[Test 9] Multiple Dimension Types\n");

    IRComponent* comp = ir_create_component(IR_COMPONENT_CONTAINER);
    comp->layout = ir_create_layout();

    // Test all dimension types
    comp->layout->min_width = (IRDimension){IR_DIMENSION_PX, 100.0f};
    comp->layout->max_width = (IRDimension){IR_DIMENSION_PERCENT, 80.0f};
    comp->layout->min_height = (IRDimension){IR_DIMENSION_VH, 50.0f};
    comp->layout->max_height = (IRDimension){IR_DIMENSION_AUTO, 0.0f};

    // Serialize
    IRBuffer* buffer = ir_serialize_binary(comp);
    TEST_ASSERT(buffer != NULL, "Serialization failed");

    // Deserialize
    ir_buffer_seek(buffer, 0);
    IRComponent* deserialized = ir_deserialize_binary(buffer);
    TEST_ASSERT(deserialized != NULL, "Deserialization failed");
    TEST_ASSERT(deserialized->layout != NULL, "Layout not deserialized");

    // Verify all dimension types
    TEST_ASSERT(dimension_equal(deserialized->layout->min_width, comp->layout->min_width), "Min width mismatch");
    TEST_ASSERT(dimension_equal(deserialized->layout->max_width, comp->layout->max_width), "Max width mismatch");
    TEST_ASSERT(dimension_equal(deserialized->layout->min_height, comp->layout->min_height), "Min height mismatch");
    TEST_ASSERT(dimension_equal(deserialized->layout->max_height, comp->layout->max_height), "Max height mismatch");

    ir_destroy_component(comp);
    ir_destroy_component(deserialized);
    ir_buffer_destroy(buffer);

    TEST_SUCCESS("Multiple dimension types serialization");
}

// Test 10: Format version check
static bool test_format_version(void) {
    printf("\n[Test 10] Format Version Check\n");

    IRComponent* comp = ir_create_component(IR_COMPONENT_CONTAINER);

    // Serialize
    IRBuffer* buffer = ir_serialize_binary(comp);
    TEST_ASSERT(buffer != NULL, "Serialization failed");
    TEST_ASSERT(buffer->size >= 8, "Buffer too small for header");

    // Check magic number (KRY\0 = 0x4B5259)
    uint32_t magic =
        ((uint32_t)buffer->data[0] << 0) |
        ((uint32_t)buffer->data[1] << 8) |
        ((uint32_t)buffer->data[2] << 16) |
        ((uint32_t)buffer->data[3] << 24);

    TEST_ASSERT(magic == 0x4B5259, "Magic number mismatch");

    // Check version (v2.0)
    uint8_t major = buffer->data[4];
    uint8_t minor = buffer->data[5];

    TEST_ASSERT(major == 2, "Major version should be 2");
    TEST_ASSERT(minor == 0, "Minor version should be 0");

    ir_destroy_component(comp);
    ir_buffer_destroy(buffer);

    TEST_SUCCESS("Format version check");
}

// Main test runner
int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Kryon IR Serialization Round-Trip Tests (v2.0)           ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    // Run all tests
    test_basic_properties();
    test_style_properties();
    test_layout_properties();
    test_component_tree();
    test_deep_nesting();
    test_empty_component();
    test_validation_integration();
    test_large_text();
    test_dimension_types();
    test_format_version();

    // Print summary
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Test Summary                                              ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  Passed: %-3d                                               ║\n", tests_passed);
    printf("║  Failed: %-3d                                               ║\n", tests_failed);
    printf("║  Total:  %-3d                                               ║\n", tests_passed + tests_failed);
    printf("╚════════════════════════════════════════════════════════════╝\n");

    if (tests_failed == 0) {
        printf("\n🎉 All tests passed!\n\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed!\n\n");
        return 1;
    }
}
