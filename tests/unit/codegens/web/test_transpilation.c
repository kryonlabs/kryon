#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "html_generator.h"
#include "../../ir/include/ir_builder.h"

// Test basic transpilation mode
void test_transpilation_mode() {
    printf("Testing transpilation mode...\n");

    // Create a simple component tree
    IRComponent* root = ir_container("root");
    IRComponent* heading = ir_heading(1, "Hello World");
    IRComponent* button = ir_button("Click me");

    ir_add_child(root, heading);
    ir_add_child(root, button);

    // Test transpilation mode
    HtmlGeneratorOptions opts = {
        .mode = HTML_MODE_TRANSPILE,
        .minify = false,
        .inline_css = true,
        .preserve_ids = true
    };

    HTMLGenerator* generator = html_generator_create_with_options(opts);
    const char* html = html_generator_generate(generator, root);

    printf("Transpilation HTML:\n%s\n", html);

    // Verify data-* attributes are present
    assert(strstr(html, "data-ir-type=\"CONTAINER\"") != NULL);
    assert(strstr(html, "data-ir-type=\"HEADING\"") != NULL);
    assert(strstr(html, "data-ir-type=\"BUTTON\"") != NULL);
    assert(strstr(html, "data-ir-id=\"") != NULL);

    // Verify event handlers are NOT present (transpilation mode)
    assert(strstr(html, "onclick=") == NULL);

    html_generator_destroy(generator);
    ir_destroy_component(root);

    printf("✅ Transpilation mode test passed\n");
}

// Test display mode (default behavior)
void test_display_mode() {
    printf("\nTesting display mode...\n");

    // Create a simple component tree with event
    IRComponent* root = ir_container("root");
    IRComponent* button = ir_button("Click me");
    IREvent* click_event = ir_create_event(IR_EVENT_CLICK, NULL, NULL);
    ir_add_event(button, click_event);
    ir_add_child(root, button);

    // Test display mode
    HtmlGeneratorOptions opts = {
        .mode = HTML_MODE_DISPLAY,
        .minify = false,
        .inline_css = true,
        .preserve_ids = false
    };

    HTMLGenerator* generator = html_generator_create_with_options(opts);
    const char* html = html_generator_generate(generator, root);

    printf("Display HTML:\n%s\n", html);

    // Verify data-ir-type is NOT present (display mode doesn't need it)
    assert(strstr(html, "data-ir-type=") == NULL);

    // Verify event handlers ARE present (display mode)
    assert(strstr(html, "onclick=") != NULL);

    html_generator_destroy(generator);
    ir_destroy_component(root);

    printf("✅ Display mode test passed\n");
}

// Test default options
void test_default_options() {
    printf("\nTesting default options...\n");

    HtmlGeneratorOptions opts = html_generator_default_options();

    assert(opts.mode == HTML_MODE_DISPLAY);
    assert(opts.minify == false);
    assert(opts.inline_css == true);
    assert(opts.preserve_ids == false);

    printf("✅ Default options test passed\n");
}

// Test component type name mapping
void test_component_type_names() {
    printf("\nTesting component type name generation...\n");

    // Create various components
    IRComponent* container = ir_container("test");
    IRComponent* text = ir_text("Hello");
    IRComponent* button = ir_button("Click");
    IRComponent* heading = ir_heading(1, "Title");

    HtmlGeneratorOptions opts = {
        .mode = HTML_MODE_TRANSPILE,
        .minify = false,
        .inline_css = true,
        .preserve_ids = false
    };

    HTMLGenerator* generator = html_generator_create_with_options(opts);

    // Test container
    const char* html1 = html_generator_generate(generator, container);
    assert(strstr(html1, "data-ir-type=\"CONTAINER\"") != NULL);

    // Reset generator for next test
    html_generator_destroy(generator);
    generator = html_generator_create_with_options(opts);

    // Test text
    const char* html2 = html_generator_generate(generator, text);
    assert(strstr(html2, "data-ir-type=\"TEXT\"") != NULL);

    // Reset generator
    html_generator_destroy(generator);
    generator = html_generator_create_with_options(opts);

    // Test button
    const char* html3 = html_generator_generate(generator, button);
    assert(strstr(html3, "data-ir-type=\"BUTTON\"") != NULL);

    // Reset generator
    html_generator_destroy(generator);
    generator = html_generator_create_with_options(opts);

    // Test heading - level is encoded in h1/h2/h3 tag, not data-level attribute
    const char* html4 = html_generator_generate(generator, heading);
    assert(strstr(html4, "data-ir-type=\"HEADING\"") != NULL);
    assert(strstr(html4, "<h1") != NULL);  // Level 1 heading uses h1 tag

    html_generator_destroy(generator);
    ir_destroy_component(container);
    ir_destroy_component(text);
    ir_destroy_component(button);
    ir_destroy_component(heading);

    printf("✅ Component type names test passed\n");
}

int main() {
    printf("Running HTML transpilation tests...\n\n");

    test_default_options();
    test_transpilation_mode();
    test_display_mode();
    test_component_type_names();

    printf("\n✅ All transpilation tests passed!\n");

    return 0;
}
