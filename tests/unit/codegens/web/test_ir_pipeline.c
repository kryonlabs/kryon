#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../../ir/include/ir_core.h"
#include "../../ir/include/ir_builder.h"
#include "ir_web_renderer.h"

// Simple test of IR → Web pipeline
int main() {
    printf("🧪 Testing IR → Web Pipeline\n");
    printf("===================================\n\n");

    // Initialize IR context
    ir_set_context(ir_create_context());

    // 1. Create a simple IR component tree
    printf("📦 Creating IR component tree...\n");

    IRComponent* root = ir_container("app_container");
    if (!root) {
        printf("❌ Failed to create root component\n");
        return 1;
    }

    // Set some basic styling
    IRStyle* root_style = ir_create_style();
    ir_set_width(root_style, IR_DIMENSION_PX, 800);
    ir_set_height(root_style, IR_DIMENSION_PX, 600);
    ir_set_padding(root_style, 16, 16, 16, 16);
    ir_set_style(root, root_style);

    // Add a title
    IRComponent* title = ir_text("Hello Kryon Web!");
    if (title) {
        IRStyle* title_style = ir_get_style(title);
        if (!title_style) {
            title_style = ir_create_style();
            ir_set_style(title, title_style);
        }
        ir_set_font(title_style, 32, "Arial", 0, 0, 0, 255, false, false);
        IRLayout* title_layout = ir_get_layout(title);
        if (!title_layout) {
            title_layout = ir_create_layout();
            ir_set_layout(title, title_layout);
        }
        ir_set_margin(title_style, 0, 0, 16, 0);
        ir_add_child(root, title);
    }

    // Add a button with click handler
    IRComponent* button = ir_button("Click Me!");
    if (button) {
        IRStyle* button_style = ir_get_style(button);
        if (!button_style) {
            button_style = ir_create_style();
            ir_set_style(button, button_style);
        }
        ir_set_width(button_style, IR_DIMENSION_PX, 120);
        ir_set_height(button_style, IR_DIMENSION_PX, 40);

        // Add click event
        IREvent* click_event = ir_create_event(IR_EVENT_CLICK, "handle_button_click", "user_clicked_button");
        ir_add_event(button, click_event);

        ir_add_child(root, button);
    }

    // Add an input field
    IRComponent* input = ir_input("Enter your name...");
    if (input) {
        IRStyle* input_style = ir_get_style(input);
        if (!input_style) {
            input_style = ir_create_style();
            ir_set_style(input, input_style);
        }
        ir_set_width(input_style, IR_DIMENSION_PX, 200);
        ir_set_height(input_style, IR_DIMENSION_PX, 32);
        ir_set_margin(input_style, 16, 0, 0, 0);
        ir_add_child(root, input);
    }

    printf("✅ IR component tree created\n");

    // 2. Validate IR
    printf("\n🔍 Validating IR tree...\n");
    if (!ir_validate_component(root)) {
        printf("❌ IR validation failed\n");
        ir_destroy_component(root);
        return 1;
    }
    printf("✅ IR validation passed\n");

    // 3. Create web renderer
    printf("\n🌐 Creating web renderer...\n");
    WebIRRenderer* renderer = web_ir_renderer_create();
    if (!renderer) {
        printf("❌ Failed to create web renderer\n");
        ir_destroy_component(root);
        return 1;
    }

    // Configure renderer
    web_ir_renderer_set_output_directory(renderer, "test_output");
    web_ir_renderer_set_generate_separate_files(renderer, true);
    web_ir_renderer_set_include_javascript_runtime(renderer, true);
    web_ir_renderer_set_include_wasm_modules(renderer, true);

    printf("✅ Web renderer created\n");

    // 4. Render to web format
    printf("\n🎨 Rendering IR to web format...\n");
    if (!web_ir_renderer_render(renderer, root)) {
        printf("❌ Failed to render IR to web format\n");
        web_ir_renderer_destroy(renderer);
        ir_destroy_component(root);
        return 1;
    }
    printf("✅ Web rendering complete!\n");

    // 5. Print statistics
    printf("\n📊 Web Renderer Statistics:\n");
    web_ir_renderer_print_stats(renderer, root);

    // 6. Test memory-based rendering
    printf("\n💾 Testing memory-based rendering...\n");
    char* html_output = NULL;
    char* css_output = NULL;
    char* js_output = NULL;

    if (web_ir_renderer_render_to_memory(renderer, root, &html_output, &css_output, &js_output)) {
        printf("✅ Memory rendering successful\n");
        printf("   HTML size: %zu bytes\n", html_output ? strlen(html_output) : 0);
        printf("   CSS size: %zu bytes\n", css_output ? strlen(css_output) : 0);
        printf("   JS size: %zu bytes\n", js_output ? strlen(js_output) : 0);

        // Show a preview of generated HTML
        if (html_output) {
            printf("\n📄 HTML Preview (first 500 chars):\n");
            printf("%.500s%s\n", html_output, strlen(html_output) > 500 ? "..." : "");
            free(html_output);
        }

        if (css_output) free(css_output);
        if (js_output) free(js_output);
    } else {
        printf("❌ Memory rendering failed\n");
    }

    // Cleanup
    printf("\n🧹 Cleaning up...\n");
    web_ir_renderer_destroy(renderer);
    ir_destroy_component(root);
    IRContext* context = g_ir_context;
    ir_destroy_context(context);
    g_ir_context = NULL;
    printf("✅ Cleanup complete\n");

    printf("\n🎉 Test completed successfully!\n");
    printf("📁 Check 'test_output/' directory for generated files:\n");
    printf("   - index.html (main page)\n");
    printf("   - kryon.css (styles)\n");
    printf("   - kryon.js (runtime)\n");
    printf("   - kryon_wasm_loader.js (WASM loader)\n");

    return 0;
}