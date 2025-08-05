/**
 * @file element_system_test.c
 * @brief Test suite for the Flutter-inspired element system
 */

#include "internal/elements.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// =============================================================================
// TEST UTILITIES
// =============================================================================

#define ASSERT(condition, message) \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        return 0; \
    }

#define ASSERT_EQ(actual, expected, message) \
    if ((actual) != (expected)) { \
        printf("FAIL: %s (expected %d, got %d)\n", message, expected, actual); \
        return 0; \
    }

#define ASSERT_STR_EQ(actual, expected, message) \
    if (strcmp((actual), (expected)) != 0) { \
        printf("FAIL: %s (expected '%s', got '%s')\n", message, expected, actual); \
        return 0; \
    }

// =============================================================================
// TEST FUNCTIONS
// =============================================================================

/// Test element registry creation and destruction
int test_element_registry() {
    printf("Testing element registry...\n");
    
    KryonElementRegistry* registry = kryon_element_registry_create(10);
    ASSERT(registry != NULL, "Registry creation failed");
    
    kryon_element_registry_destroy(registry);
    printf("✓ Element registry test passed\n");
    return 1;
}

/// Test element creation and basic properties
int test_element_creation() {
    printf("Testing element creation...\n");
    
    KryonElementRegistry* registry = kryon_element_registry_create(10);
    ASSERT(registry != NULL, "Registry creation failed");
    
    // Create a column element
    KryonElement* column = kryon_element_create(registry, KRYON_ELEMENT_COLUMN, "test_column");
    ASSERT(column != NULL, "Element creation failed");
    ASSERT_STR_EQ(column->id, "test_column", "Element ID mismatch");
    ASSERT_EQ(column->type, KRYON_ELEMENT_COLUMN, "Element type mismatch");
    ASSERT_EQ(column->ref_count, 1, "Initial ref count should be 1");
    
    // Test element lookup
    KryonElement* found = kryon_element_find_by_id(registry, "test_column");
    ASSERT(found == column, "Element lookup failed");
    
    // Create a text element
    KryonElement* text = kryon_element_create(registry, KRYON_ELEMENT_TEXT, "test_text");
    ASSERT(text != NULL, "Text element creation failed");
    ASSERT_EQ(text->type, KRYON_ELEMENT_TEXT, "Text element type mismatch");
    
    kryon_element_registry_destroy(registry);
    printf("✓ Element creation test passed\n");
    return 1;
}


/// Test element hierarchy management
int test_element_hierarchy() {
    printf("Testing element hierarchy...\n");
    
    KryonElementRegistry* registry = kryon_element_registry_create(10);
    
    // Create parent and children
    KryonElement* column = kryon_element_create(registry, KRYON_ELEMENT_COLUMN, "parent");
    KryonElement* text1 = kryon_element_create(registry, KRYON_ELEMENT_TEXT, "child1");
    KryonElement* text2 = kryon_element_create(registry, KRYON_ELEMENT_TEXT, "child2");
    
    ASSERT(column != NULL && text1 != NULL && text2 != NULL, "Element creation failed");
    
    // Add children
    ASSERT(kryon_element_add_child(column, text1), "Failed to add first child");
    ASSERT(kryon_element_add_child(column, text2), "Failed to add second child");
    
    ASSERT_EQ(column->child_count, 2, "Child count mismatch");
    ASSERT(column->children[0] == text1, "First child mismatch");
    ASSERT(column->children[1] == text2, "Second child mismatch");
    
    // Test child lookup
    KryonElement* child = kryon_element_get_child(column, 0);
    ASSERT(child == text1, "Child lookup failed");
    
    // Remove child
    ASSERT(kryon_element_remove_child(column, text1), "Failed to remove child");
    ASSERT_EQ(column->child_count, 1, "Child count after removal");
    ASSERT(column->children[0] == text2, "Remaining child mismatch");
    
    kryon_element_registry_destroy(registry);
    printf("✓ Element hierarchy test passed\n");
    return 1;
}


/// Test element transformation
int test_element_transformation() {
    printf("Testing element transformation...\n");
    
    KryonElementRegistry* registry = kryon_element_registry_create(10);
    
    // Create a button element
    KryonElement* element = kryon_element_create(registry, KRYON_ELEMENT_BUTTON, "transform_test");
    ASSERT(element != NULL, "Element creation failed");
    ASSERT_EQ(element->type, KRYON_ELEMENT_BUTTON, "Initial type mismatch");
    
    // Set button properties
    ASSERT(kryon_element_set_property(element, "text", "Test Button"), "Failed to set button text");
    char* text = kryon_element_get_property(element, "text");
    ASSERT_STR_EQ(text, "Test Button", "Button text property mismatch");
    free(text);
    
    // Transform to column
    ASSERT(kryon_element_transform_to(element, KRYON_ELEMENT_COLUMN), "Transformation failed");
    ASSERT_EQ(element->type, KRYON_ELEMENT_COLUMN, "Type after transformation");
    ASSERT_EQ(element->previous_type, KRYON_ELEMENT_BUTTON, "Previous type not saved");
    
    // Should now have children array
    ASSERT(element->children != NULL, "Children array not created");
    ASSERT_EQ(element->child_count, 0, "Initial child count should be 0");
    
    // Transform back to button
    ASSERT(kryon_element_transform_to(element, KRYON_ELEMENT_BUTTON), "Reverse transformation failed");
    ASSERT_EQ(element->type, KRYON_ELEMENT_BUTTON, "Type after reverse transformation");
    
    kryon_element_registry_destroy(registry);
    printf("✓ Element transformation test passed\n");
    return 1;
}


/// Test layout property management
int test_layout_properties() {
    printf("Testing layout properties...\n");
    
    KryonElementRegistry* registry = kryon_element_registry_create(10);
    
    KryonElement* column = kryon_element_create(registry, KRYON_ELEMENT_COLUMN, "layout_test");
    ASSERT(column != NULL, "Element creation failed");
    
    // Test main axis alignment
    ASSERT(kryon_element_set_property(column, "main_axis", "center"), "Failed to set main_axis");
    ASSERT_EQ(column->main_axis, KRYON_MAIN_CENTER, "Main axis property not set");
    
    char* main_axis = kryon_element_get_property(column, "main_axis");
    ASSERT_STR_EQ(main_axis, "center", "Main axis getter failed");
    free(main_axis);
    
    // Test cross axis alignment
    ASSERT(kryon_element_set_property(column, "cross_axis", "stretch"), "Failed to set cross_axis");
    ASSERT_EQ(column->cross_axis, KRYON_CROSS_STRETCH, "Cross axis property not set");
    
    // Test spacing
    ASSERT(kryon_element_set_property(column, "spacing", "16.5"), "Failed to set spacing");
    ASSERT(column->spacing == 16.5f, "Spacing property not set");
    
    // Test flex
    ASSERT(kryon_element_set_property(column, "flex", "2"), "Failed to set flex");
    ASSERT_EQ(column->flex, 2, "Flex property not set");
    
    kryon_element_registry_destroy(registry);
    printf("✓ Layout properties test passed\n");
    return 1;
}


/// Test basic layout calculation
int test_layout_calculation() {
    printf("Testing layout calculation...\n");
    
    KryonElementRegistry* registry = kryon_element_registry_create(10);
    
    // Create a simple column with text children
    KryonElement* column = kryon_element_create(registry, KRYON_ELEMENT_COLUMN, "layout_column");
    KryonElement* text1 = kryon_element_create(registry, KRYON_ELEMENT_TEXT, "text1");
    KryonElement* text2 = kryon_element_create(registry, KRYON_ELEMENT_TEXT, "text2");
    
    // Set text content
    kryon_element_set_property(text1, "text", "Hello");
    kryon_element_set_property(text2, "text", "World");
    
    // Add children
    kryon_element_add_child(column, text1);
    kryon_element_add_child(column, text2);
    
    // Set column properties
    kryon_element_set_property(column, "spacing", "10");
    kryon_element_set_property(column, "main_axis", "start");
    kryon_element_set_property(column, "cross_axis", "stretch");
    
    // Calculate layout
    KryonSize available = {400, 300};
    kryon_element_calculate_layout(column, available);
    
    // Check that layout was calculated (positions should be set)
    ASSERT(!column->layout_dirty, "Layout should not be dirty after calculation");
    ASSERT(column->computed_rect.size.width > 0, "Column width should be set");
    ASSERT(column->computed_rect.size.height > 0, "Column height should be set");
    
    kryon_element_registry_destroy(registry);
    printf("✓ Layout calculation test passed\n");
    return 1;
}


/// Test utility functions
int test_utility_functions() {
    printf("Testing utility functions...\n");
    
    // Test element type checking
    ASSERT(kryon_element_is_layout_type(KRYON_ELEMENT_COLUMN), "Column should be layout type");
    ASSERT(kryon_element_is_layout_type(KRYON_ELEMENT_ROW), "Row should be layout type");
    ASSERT(!kryon_element_is_layout_type(KRYON_ELEMENT_TEXT), "Text should not be layout type");
    ASSERT(!kryon_element_is_layout_type(KRYON_ELEMENT_BUTTON), "Button should not be layout type");
    
    // Test type string conversion
    ASSERT_STR_EQ(kryon_element_type_to_string(KRYON_ELEMENT_COLUMN), "column", "Column type string");
    ASSERT_STR_EQ(kryon_element_type_to_string(KRYON_ELEMENT_TEXT), "text", "Text type string");
    
    ASSERT_EQ(kryon_element_type_from_string("column"), KRYON_ELEMENT_COLUMN, "String to column type");
    ASSERT_EQ(kryon_element_type_from_string("text"), KRYON_ELEMENT_TEXT, "String to text type");
    
    // Test edge insets
    KryonEdgeInsets insets = kryon_edge_insets_all(10.0f);
    ASSERT(insets.top == 10.0f && insets.right == 10.0f && 
           insets.bottom == 10.0f && insets.left == 10.0f, "Edge insets all");
    
    printf("✓ Utility functions test passed\n");
    return 1;
}


// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

int main() {
    printf("Running Flutter-inspired element system tests...\n\n");
    
    int tests_passed = 0;
    int total_tests = 0;
    
    #define RUN_TEST(test_func) \
        total_tests++; \
        if (test_func()) tests_passed++; \
        printf("\n");
    
    RUN_TEST(test_element_registry);
    RUN_TEST(test_element_creation);
    RUN_TEST(test_element_hierarchy);
    RUN_TEST(test_element_transformation);
    RUN_TEST(test_layout_properties);
    RUN_TEST(test_layout_calculation);
    RUN_TEST(test_utility_functions);
    
    printf("=== Test Results ===\n");
    printf("Passed: %d/%d\n", tests_passed, total_tests);
    
    if (tests_passed == total_tests) {
        printf("✓ All tests passed!\n");
        return 0;
    } else {
        printf("✗ %d tests failed\n", total_tests - tests_passed);
        return 1;
    }
}