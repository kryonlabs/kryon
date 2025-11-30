// Test auto height calculation with padding
#include "ir/ir_core.h"
#include "ir/ir_builder.h"
#include "backends/desktop/debug_backend.h"
#include <stdio.h>

int main(void) {
    // Create IR context
    IRContext* ctx = ir_context_create();
    if (!ctx) {
        fprintf(stderr, "Failed to create IR context\n");
        return 1;
    }

    // Create root container with auto height and 20px padding
    IRComponent* root = ir_create_component(ctx, IR_COMPONENT_CONTAINER);
    ir_set_width_px(root, 600.0f);
    ir_set_height_auto(root);
    ir_set_padding(root, 20.0f, 20.0f, 20.0f, 20.0f);
    ir_set_background_color(root, 0x1e1b4bff);
    ir_set_layout_direction(root, 0); // Column

    // Add 5 text children with different font sizes
    const float font_sizes[] = {16.0f, 13.0f, 13.0f, 13.0f, 12.0f};
    const char* texts[] = {
        "Available Preset Animations",
        "pulse(duration, iterations) - Scale in/out effect",
        "fadeInOut(duration, iterations) - Opacity 0 to 1 to 0",
        "slideInLeft(duration) - Slide in from left edge",
        "Use -1 for infinite loop, 1 for once, N for N times"
    };

    for (int i = 0; i < 5; i++) {
        IRComponent* text = ir_create_component(ctx, IR_COMPONENT_TEXT);
        ir_set_text_content(text, texts[i]);
        ir_set_font_size(text, font_sizes[i]);
        ir_set_text_color(text, 0xc7d2feff);
        ir_add_child(root, text);
    }

    // Compute layout
    ir_layout_compute(root, 600.0f, 600.0f);

    // Print debug tree
    printf("\n=== DEBUG OUTPUT ===\n");
    DebugRendererConfig config = debug_renderer_config_default();
    DebugRenderer* debug = debug_renderer_create(&config);
    debug_render_tree(debug, root);
    debug_renderer_destroy(debug);

    // Print summary
    printf("\n=== SUMMARY ===\n");
    printf("Container height: %.1f px\n", root->rendered_bounds.height);
    printf("Expected: ~110px (content + 40px padding)\n");
    printf("Padding: top=%.0f, bottom=%.0f (total=%.0f)\n",
        root->style->padding.top, root->style->padding.bottom,
        root->style->padding.top + root->style->padding.bottom);

    float content_height = 0.0f;
    for (uint32_t i = 0; i < root->child_count; i++) {
        content_height += root->children[i]->rendered_bounds.height;
    }
    printf("Total content height: %.1f px\n", content_height);
    printf("Expected container height: %.1f px (content + padding)\n",
        content_height + root->style->padding.top + root->style->padding.bottom);

    // Cleanup
    ir_context_destroy(ctx);
    return 0;
}
