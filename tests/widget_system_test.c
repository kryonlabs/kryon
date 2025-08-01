/**
 * @file widget_system_test.c
 * @brief Test suite for the Flutter-inspired widget system
 */

#include "kryon/widget_system.h"
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

/// Test widget registry creation and destruction
int test_widget_registry() {
    printf("Testing widget registry...\n");
    
    KryonWidgetRegistry* registry = kryon_widget_registry_create(10);
    ASSERT(registry != NULL, "Registry creation failed");
    
    kryon_widget_registry_destroy(registry);
    printf("✓ Widget registry test passed\n");
    return 1;
}

/// Test widget creation and basic properties
int test_widget_creation() {
    printf("Testing widget creation...\n");
    
    KryonWidgetRegistry* registry = kryon_widget_registry_create(10);
    ASSERT(registry != NULL, "Registry creation failed");
    
    // Create a column widget
    KryonWidget* column = kryon_widget_create(registry, KRYON_WIDGET_COLUMN, "test_column");
    ASSERT(column != NULL, "Widget creation failed");
    ASSERT_STR_EQ(column->id, "test_column", "Widget ID mismatch");
    ASSERT_EQ(column->type, KRYON_WIDGET_COLUMN, "Widget type mismatch");
    ASSERT_EQ(column->ref_count, 1, "Initial ref count should be 1");
    
    // Test widget lookup
    KryonWidget* found = kryon_widget_find_by_id(registry, "test_column");
    ASSERT(found == column, "Widget lookup failed");
    
    // Create a text widget
    KryonWidget* text = kryon_widget_create(registry, KRYON_WIDGET_TEXT, "test_text");
    ASSERT(text != NULL, "Text widget creation failed");
    ASSERT_EQ(text->type, KRYON_WIDGET_TEXT, "Text widget type mismatch");
    
    kryon_widget_registry_destroy(registry);
    printf("✓ Widget creation test passed\n");
    return 1;
}

/// Test widget hierarchy management
int test_widget_hierarchy() {
    printf("Testing widget hierarchy...\n");
    
    KryonWidgetRegistry* registry = kryon_widget_registry_create(10);
    
    // Create parent and children
    KryonWidget* column = kryon_widget_create(registry, KRYON_WIDGET_COLUMN, "parent");
    KryonWidget* text1 = kryon_widget_create(registry, KRYON_WIDGET_TEXT, "child1");
    KryonWidget* text2 = kryon_widget_create(registry, KRYON_WIDGET_TEXT, "child2");
    
    ASSERT(column != NULL && text1 != NULL && text2 != NULL, "Widget creation failed");
    
    // Add children
    ASSERT(kryon_widget_add_child(column, text1), "Failed to add first child");
    ASSERT(kryon_widget_add_child(column, text2), "Failed to add second child");
    
    ASSERT_EQ(column->child_count, 2, "Child count mismatch");
    ASSERT(column->children[0] == text1, "First child mismatch");
    ASSERT(column->children[1] == text2, "Second child mismatch");
    
    // Test child lookup
    KryonWidget* child = kryon_widget_get_child(column, 0);
    ASSERT(child == text1, "Child lookup failed");
    
    // Remove child
    ASSERT(kryon_widget_remove_child(column, text1), "Failed to remove child");
    ASSERT_EQ(column->child_count, 1, "Child count after removal");
    ASSERT(column->children[0] == text2, "Remaining child mismatch");
    
    kryon_widget_registry_destroy(registry);
    printf("✓ Widget hierarchy test passed\n");
    return 1;
}

/// Test widget transformation
int test_widget_transformation() {
    printf("Testing widget transformation...\n");
    
    KryonWidgetRegistry* registry = kryon_widget_registry_create(10);
    
    // Create a button widget
    KryonWidget* widget = kryon_widget_create(registry, KRYON_WIDGET_BUTTON, "transform_test");
    ASSERT(widget != NULL, "Widget creation failed");
    ASSERT_EQ(widget->type, KRYON_WIDGET_BUTTON, "Initial type mismatch");
    
    // Set button properties
    ASSERT(kryon_widget_set_property(widget, "text", "Test Button"), "Failed to set button text");
    char* text = kryon_widget_get_property(widget, "text");
    ASSERT_STR_EQ(text, "Test Button", "Button text property mismatch");
    free(text);
    
    // Transform to column
    ASSERT(kryon_widget_transform_to(widget, KRYON_WIDGET_COLUMN), "Transformation failed");
    ASSERT_EQ(widget->type, KRYON_WIDGET_COLUMN, "Type after transformation");
    ASSERT_EQ(widget->previous_type, KRYON_WIDGET_BUTTON, "Previous type not saved");
    
    // Should now have children array
    ASSERT(widget->children != NULL, "Children array not created");
    ASSERT_EQ(widget->child_count, 0, "Initial child count should be 0");
    
    // Transform back to button
    ASSERT(kryon_widget_transform_to(widget, KRYON_WIDGET_BUTTON), "Reverse transformation failed");
    ASSERT_EQ(widget->type, KRYON_WIDGET_BUTTON, "Type after reverse transformation");
    
    kryon_widget_registry_destroy(registry);
    printf("✓ Widget transformation test passed\n");
    return 1;
}

/// Test layout property management
int test_layout_properties() {
    printf("Testing layout properties...\n");
    
    KryonWidgetRegistry* registry = kryon_widget_registry_create(10);
    
    KryonWidget* column = kryon_widget_create(registry, KRYON_WIDGET_COLUMN, "layout_test");
    ASSERT(column != NULL, "Widget creation failed");
    
    // Test main axis alignment
    ASSERT(kryon_widget_set_property(column, "main_axis", "center"), "Failed to set main_axis");
    ASSERT_EQ(column->main_axis, KRYON_MAIN_CENTER, "Main axis property not set");
    
    char* main_axis = kryon_widget_get_property(column, "main_axis");
    ASSERT_STR_EQ(main_axis, "center", "Main axis getter failed");
    free(main_axis);
    
    // Test cross axis alignment
    ASSERT(kryon_widget_set_property(column, "cross_axis", "stretch"), "Failed to set cross_axis");
    ASSERT_EQ(column->cross_axis, KRYON_CROSS_STRETCH, "Cross axis property not set");
    
    // Test spacing
    ASSERT(kryon_widget_set_property(column, "spacing", "16.5"), "Failed to set spacing");
    ASSERT(column->spacing == 16.5f, "Spacing property not set");
    
    // Test flex
    ASSERT(kryon_widget_set_property(column, "flex", "2"), "Failed to set flex");
    ASSERT_EQ(column->flex, 2, "Flex property not set");
    
    kryon_widget_registry_destroy(registry);
    printf("✓ Layout properties test passed\n");
    return 1;
}

/// Test basic layout calculation
int test_layout_calculation() {
    printf("Testing layout calculation...\n");
    
    KryonWidgetRegistry* registry = kryon_widget_registry_create(10);
    
    // Create a simple column with text children
    KryonWidget* column = kryon_widget_create(registry, KRYON_WIDGET_COLUMN, "layout_column");
    KryonWidget* text1 = kryon_widget_create(registry, KRYON_WIDGET_TEXT, "text1");
    KryonWidget* text2 = kryon_widget_create(registry, KRYON_WIDGET_TEXT, "text2");
    
    // Set text content
    kryon_widget_set_property(text1, "text", "Hello");
    kryon_widget_set_property(text2, "text", "World");
    
    // Add children
    kryon_widget_add_child(column, text1);
    kryon_widget_add_child(column, text2);
    
    // Set column properties
    kryon_widget_set_property(column, "spacing", "10");
    kryon_widget_set_property(column, "main_axis", "start");
    kryon_widget_set_property(column, "cross_axis", "stretch");
    
    // Calculate layout
    KryonSize available = {400, 300};
    kryon_widget_calculate_layout(column, available);
    
    // Check that layout was calculated (positions should be set)
    ASSERT(!column->layout_dirty, "Layout should not be dirty after calculation");
    ASSERT(column->computed_rect.size.width > 0, "Column width should be set");
    ASSERT(column->computed_rect.size.height > 0, "Column height should be set");
    
    kryon_widget_registry_destroy(registry);
    printf("✓ Layout calculation test passed\n");
    return 1;
}

/// Test utility functions
int test_utility_functions() {
    printf("Testing utility functions...\n");
    
    // Test widget type checking
    ASSERT(kryon_widget_is_layout_type(KRYON_WIDGET_COLUMN), "Column should be layout type");
    ASSERT(kryon_widget_is_layout_type(KRYON_WIDGET_ROW), "Row should be layout type");
    ASSERT(!kryon_widget_is_layout_type(KRYON_WIDGET_TEXT), "Text should not be layout type");
    ASSERT(!kryon_widget_is_layout_type(KRYON_WIDGET_BUTTON), "Button should not be layout type");
    
    // Test type string conversion
    ASSERT_STR_EQ(kryon_widget_type_to_string(KRYON_WIDGET_COLUMN), "column", "Column type string");
    ASSERT_STR_EQ(kryon_widget_type_to_string(KRYON_WIDGET_TEXT), "text", "Text type string");
    
    ASSERT_EQ(kryon_widget_type_from_string("column"), KRYON_WIDGET_COLUMN, "String to column type");
    ASSERT_EQ(kryon_widget_type_from_string("text"), KRYON_WIDGET_TEXT, "String to text type");
    
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
    printf("Running Flutter-inspired widget system tests...\n\n");
    
    int tests_passed = 0;
    int total_tests = 0;
    
    #define RUN_TEST(test_func) \
        total_tests++; \
        if (test_func()) tests_passed++; \
        printf("\n");
    
    RUN_TEST(test_widget_registry);
    RUN_TEST(test_widget_creation);
    RUN_TEST(test_widget_hierarchy);
    RUN_TEST(test_widget_transformation);
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